/*
 *   ██████╗ ██╗██╗   ██╗
 *   ██╔══██╗██║╚██╗ ██╔╝
 *   ██║  ██║██║ ╚████╔╝
 *   ██║  ██║██║  ╚██╔╝
 *   ██████╔╝██║   ██║
 *   ╚═════╝ ╚═╝   ╚═╝
 *
 *    ██████╗██╗   ██╗██████╗ ███████╗██████╗ ███████╗██╗  ██╗ ██████╗ ████████╗
 *   ██╔════╝╚██╗ ██╔╝██╔══██╗██╔════╝██╔══██╗██╔════╝██║  ██║██╔═══██╗╚══██╔══╝
 *   ██║      ╚████╔╝ ██████╔╝█████╗  ██████╔╝███████╗███████║██║   ██║   ██║
 *   ██║       ╚██╔╝  ██╔══██╗██╔══╝  ██╔══██╗╚════██║██╔══██║██║   ██║   ██║
 *    ██████╗   ██║   ██████╔╝███████╗██║  ██║███████║██║  ██║╚██████╔╝   ██║
 *    ╚═════╝   ╚═╝   ╚═════╝ ╚══════╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝    ╚═╝
 *
 *   CyberShot Cam — ESP32-S3 DIY Camera
 *   @lixofuturista / @cebolander
 */

#include <Arduino.h>
#include <algorithm>
#include <vector>
#include <JPEGDEC.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "esp_camera.h"
#include "img_converters.h"
#include "SD_MMC.h"

DNSServer    dnsServer;
Preferences  wifiPrefs;
bool         wifiSetup = false;   // true = captive portal ativo

// TFT
#define TFT_SCK  47
#define TFT_SDA  45
#define TFT_CS   43
#define TFT_DC   14
#define TFT_RST  21

// Controles
#define BTN_PIN  41
#define JOY_X     1
#define JOY_Y     2
#define JOY_SW   42

// Flash LED
#define LED_FLASH   48

// SD (hardwired GOOUUU V1.3)
#define SD_CLK_PIN  39
#define SD_CMD_PIN  38
#define SD_D0_PIN   40

// Camera
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5
#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM     11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM   13

// Resolução de captura: XGA 1024×768
#define FRAME_W  1024
#define FRAME_H  768

uint8_t* photoBuf   = nullptr;
size_t   photoLen   = 0;
bool     photoReady = false;
int      photoCount = 0;

bool wifiOK = false;
bool wifiAP = false;  // true = rodando como Access Point
bool sdOK   = false;

enum AppState { STATE_VF, STATE_MENU, STATE_CONFIRM, STATE_VF_COLOR, STATE_EFFECTS, STATE_WIFI };

static bool fxDQT    = false;
static bool fxScan   = false;
static bool fxChroma = false;
static bool fxZigzag = false;
static bool fxDHT    = false;
static int  effectsSel  = 0;
static int  wifiMenuSel = 0;
static int  timerSecs  = 0;   // 0=off, 3, 5, 10
static AppState appState     = STATE_VF;
static int      menuSel      = 0;
static int      confirmSel   = 1;
static bool     vfNeedsClear = true;

// paletas do viewfinder: 4 tons escuro→brilhante por cor
const uint16_t vfPalettes[5][4] = {
    { 0x00C0, 0x0260, 0x0480, 0x07C0 },  // 0 verde (default)
    { 0x2000, 0x5000, 0x9000, 0xF800 },  // 1 vermelho
    { 0x2804, 0x6009, 0xA050, 0xF8DC },  // 2 rosa
    { 0x18C3, 0x4208, 0x8410, 0xFFFF },  // 3 branco
    { 0x0106, 0x028C, 0x04D4, 0x07FF },  // 4 ciano
};
const char* VF_COLOR_NAMES[] = { "VERDE", "VERMELHO", "ROSA", "BRANCO", "CIANO" };
static int  vfColorIdx   = 0;
static int  leSeconds    = 0;   // 0=OFF, 3, 5, 10
static int  evComp       = 0;    // compensação de exposição: -3 a +3 stops

// ─── Auto-exposição ───────────────────────────────────────────────────────────
// Valores calibrados pelo viewfinder, reaproveitados na captura.
// VF roda em 10 MHz XCLK; captura normal em 20 MHz → aec dobrado p/ compensar.
// Captura longa usa 5 MHz + valores máximos; VF long-preview usa max sem mudar XCLK.
static int  vfAecValue  = 600;   // 0–1200 (linhas de exposição manual)
static int  vfAgcGain   = 15;    // 0–30   (ganho manual)
static int  vfFrameCnt  = 0;     // contador interno para ajuste periódico

static const int LUMA_TARGET = 5000;  // alvo de luma médio (~36 % do max 13698)
static const int LUMA_HYST   = 700;   // faixa de tolerância (evita oscilação)

// Mede luma média do frame raw do viewfinder (big-endian RGB565, OV2640 output).
// Amostra 1 pixel a cada 32 para não custar tempo no loop principal.
int measureLuma(const uint8_t* buf, int w, int h) {
    uint32_t sum   = 0;
    int      count = 0;
    for (int i = 0; i < w * h * 2; i += 64) {
        uint8_t hi = buf[i], lo = buf[i + 1];
        uint8_t r = hi >> 3;
        uint8_t g = ((hi & 0x07) << 3) | (lo >> 5);
        uint8_t b = lo & 0x1F;
        sum += (uint32_t)r * 54 + (uint32_t)g * 182 + (uint32_t)b * 18;
        count++;
    }
    return count ? (int)(sum / count) : 0;
}

// Ajusta vfAecValue/vfAgcGain para atingir LUMA_TARGET e aplica imediatamente ao sensor.
// Estratégia em dois estágios: primeiro esgota aec_value, depois toca gain (e vice-versa).
void autoExposure(int avgLuma) {
    int target = LUMA_TARGET;
    if      (evComp > 0) target = min(13000, LUMA_TARGET << evComp);
    else if (evComp < 0) target = max(200,   LUMA_TARGET >> (-evComp));

    int diff = avgLuma - target;
    if (abs(diff) < LUMA_HYST) return;

    // Step proporcional: quanto mais longe do alvo, mais rápido converge
    int aecStep = constrain(abs(diff) / 8, 80, 350);
    bool changed = false;

    if (diff < 0) {  // muito escuro → aumenta exposição
        if (vfAecValue < 1200) {
            vfAecValue = min(1200, vfAecValue + aecStep);
            changed = true;
        } else if (vfAgcGain < 30) {
            vfAgcGain = min(30, vfAgcGain + 2);
            changed = true;
        }
    } else {  // muito brilhante → reduz exposição
        if (vfAgcGain > 0) {
            vfAgcGain = max(0, vfAgcGain - 2);
            changed = true;
        } else if (vfAecValue > 50) {
            vfAecValue = max(50, vfAecValue - aecStep);
            changed = true;
        }
    }
    if (changed) {
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_aec_value(s, vfAecValue);
            s->set_agc_gain(s, vfAgcGain);
        }
    }
}

void applyVfExposure() {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return;
    s->set_aec_value(s, vfAecValue);
    s->set_agc_gain(s, vfAgcGain);
}

const uint16_t gbPalette[4] = {0x00C0, 0x0260, 0x0480, 0x07C0};

SPIClass        tftSPI(FSPI);
Adafruit_ST7735 tft(&tftSPI, TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

// ─── Camera ───────────────────────────────────────────────────────────────────

bool initCamera(pixformat_t fmt, framesize_t size, uint8_t quality, uint8_t fbCount) {
    esp_camera_deinit();
    camera_config_t cfg = {};
    cfg.ledc_channel  = LEDC_CHANNEL_0;
    cfg.ledc_timer    = LEDC_TIMER_0;
    cfg.pin_d0        = Y2_GPIO_NUM;
    cfg.pin_d1        = Y3_GPIO_NUM;
    cfg.pin_d2        = Y4_GPIO_NUM;
    cfg.pin_d3        = Y5_GPIO_NUM;
    cfg.pin_d4        = Y6_GPIO_NUM;
    cfg.pin_d5        = Y7_GPIO_NUM;
    cfg.pin_d6        = Y8_GPIO_NUM;
    cfg.pin_d7        = Y9_GPIO_NUM;
    cfg.pin_xclk      = XCLK_GPIO_NUM;
    cfg.pin_pclk      = PCLK_GPIO_NUM;
    cfg.pin_vsync     = VSYNC_GPIO_NUM;
    cfg.pin_href      = HREF_GPIO_NUM;
    cfg.pin_sccb_sda  = SIOD_GPIO_NUM;
    cfg.pin_sccb_scl  = SIOC_GPIO_NUM;
    cfg.pin_pwdn      = PWDN_GPIO_NUM;
    cfg.pin_reset     = RESET_GPIO_NUM;

    if (fmt == PIXFORMAT_RGB565) cfg.xclk_freq_hz = 10000000;
    else                         cfg.xclk_freq_hz = 20000000;

    cfg.pixel_format = fmt;
    cfg.frame_size   = size;
    cfg.jpeg_quality = quality;
    cfg.fb_count     = fbCount;
    cfg.grab_mode    = CAMERA_GRAB_LATEST;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    if (esp_camera_init(&cfg) != ESP_OK) return false;
    delay(200);

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_hmirror(s, 1);
        s->set_brightness(s, 1);
        s->set_gainceiling(s, GAINCEILING_128X);
        s->set_exposure_ctrl(s, 0);  // sempre manual — valores controlados pelo código
        s->set_gain_ctrl(s, 0);

        if (fmt == PIXFORMAT_RGB565) {
            s->set_whitebal(s, 0);
            s->set_aec_value(s, vfAecValue);
            s->set_agc_gain(s, vfAgcGain);
        } else {
            // Captura normal: XCLK=20 MHz é 2× mais rápido que VF (10 MHz).
            // Dobrar aec_value mantém o mesmo tempo de exposição absoluto.
            s->set_whitebal(s, 1);    // AWB ligado na captura — corrige dominância de cor
            s->set_awb_gain(s, 1);
            s->set_wb_mode(s, 0);     // modo auto
            s->set_aec_value(s, min(1200, vfAecValue * 2));
            s->set_agc_gain(s, vfAgcGain);
        }
    }
    return true;
}

// ─── Viewfinder ───────────────────────────────────────────────────────────────

void toGreenTones(uint8_t* buf, int w, int h) {
    const uint16_t* pal = vfPalettes[vfColorIdx];
    for (int i = 0, total = w * h * 2; i < total; i += 2) {
        uint8_t hi = buf[i], lo = buf[i + 1];
        uint8_t r  = hi >> 3;
        uint8_t g  = ((hi & 0x07) << 3) | (lo >> 5);
        uint8_t b  = lo & 0x1F;
        uint16_t luma = (uint16_t)r * 54 + (uint16_t)g * 182 + (uint16_t)b * 18;
        uint8_t  tone = (uint8_t)(luma >> 12);
        if (tone > 3) tone = 3;
        uint16_t px = pal[tone];
        buf[i]     = px >> 8;
        buf[i + 1] = px & 0xFF;
    }
}

// Barra de compensação de exposição: 7 posições (-3 a +3) no rodapé do VF.
// Desenhada dentro da área do viewfinder → sobrescrita pelo próximo frame.
void drawEvBar() {
    const int Y    = 109;
    const int STEP = 21;   // px entre posições
    const int X0   = 17;   // x da posição ev=-3

    uint16_t dim = vfPalettes[vfColorIdx][1];
    uint16_t mid = vfPalettes[vfColorIdx][2];
    uint16_t bri = vfPalettes[vfColorIdx][3];

    tft.fillRect(0, Y - 1, 160, 14, ST77XX_BLACK);

    for (int e = -3; e <= 3; e++) {
        int x = X0 + (e + 3) * STEP;
        if (e == evComp) {
            tft.fillRect(x - 4, Y, 9, 11, evComp > 0 ? bri : dim);
        } else if (e == 0) {
            tft.drawRect(x - 3, Y + 1, 7, 9, mid);
        } else {
            tft.drawFastVLine(x, Y + 2, 7, dim);
        }
    }

    // valor numérico no canto direito
    char txt[5];
    snprintf(txt, sizeof(txt), evComp > 0 ? "+%d" : "%d", evComp);
    tft.setTextSize(1);
    tft.setTextColor(evComp > 0 ? bri : dim);
    tft.setCursor(149, Y + 2);
    tft.print(txt);
}

void drawViewfinderOverlay() {
    uint16_t c = vfPalettes[vfColorIdx][3];
    int m = 8;
    tft.drawFastHLine(0,       0,       m, c);
    tft.drawFastVLine(0,       0,       m, c);
    tft.drawFastHLine(160 - m, 0,       m, c);
    tft.drawFastVLine(159,     0,       m, c);
    tft.drawFastHLine(0,       127,     m, c);
    tft.drawFastVLine(0,       127 - m, m, c);
    tft.drawFastHLine(160 - m, 127,     m, c);
    tft.drawFastVLine(159,     127 - m, m, c);
    tft.drawFastHLine(76, 64, 8, c);
    tft.drawFastVLine(79, 61, 6, c);

    tft.setTextSize(1);

    // indicadores de modo (canto superior)
    if (leSeconds > 0) {
        tft.setTextColor(vfPalettes[vfColorIdx][2]);
        tft.setCursor(2, 2);
        char leLabel[6];
        snprintf(leLabel, sizeof(leLabel), "L%dS", leSeconds);
        tft.print(leLabel);
    }
    // barra de EV — desenhada aqui para garantir que fica por cima do frame
    if (evComp != 0) drawEvBar();

    static bool _wifi = false;
    static bool _ap   = false;
    if (wifiOK == _wifi && wifiAP == _ap) return;
    _wifi = wifiOK;
    _ap   = wifiAP;
    tft.setTextSize(1);
    tft.setCursor(118, 120);
    if (!wifiOK) {
        tft.setTextColor(ST77XX_RED);   tft.print("----");
    } else if (wifiAP) {
        tft.setTextColor(ST77XX_CYAN);  tft.print(" AP ");
    } else {
        tft.setTextColor(ST77XX_GREEN); tft.print("WiFi");
    }
}

// ─── Forward declarations ─────────────────────────────────────────────────────

void handleRoot();
void handleFoto();
void handleGallery();
void handleEditor();
void handleDelete();
void handleSDFile();

void setupWebServer();   // forward declaration

// ─── NVS helpers ─────────────────────────────────────────────────────────────

void saveWiFiCreds(const String& ssid, const String& pass) {
    wifiPrefs.begin("wifi", false);
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("pass", pass);
    wifiPrefs.end();
}

bool loadWiFiCreds(String& ssid, String& pass) {
    wifiPrefs.begin("wifi", true);
    ssid = wifiPrefs.getString("ssid", "");
    pass = wifiPrefs.getString("pass", "");
    wifiPrefs.end();
    return ssid.length() > 0;
}

void clearWiFiCreds() {
    wifiPrefs.begin("wifi", false);
    wifiPrefs.clear();
    wifiPrefs.end();
}

// ─── Captive portal ──────────────────────────────────────────────────────────

void handleSetupPage() {
    // Escaneia redes disponíveis
    int n = WiFi.scanNetworks();
    String nets = "";
    for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        int    r = WiFi.RSSI(i);
        String bars = r > -60 ? "▊▊▊" : r > -75 ? "▊▊░" : "▊░░";
        s.replace("\"", "&quot;");
        nets += "<div class='n' onclick='pick(this)' data-s='" + s + "'>"
              + s + "<span>" + bars + " " + String(r) + "dBm</span></div>";
    }
    WiFi.scanDelete();

    String html = R"(<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<meta charset='utf-8'>
<title>CyberShot WiFi</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;background:#0a0a0a;color:#ddd;max-width:420px;margin:0 auto;padding:16px}
h1{color:#00ff88;font-size:1.1em;margin:0 0 4px}
p{color:#666;font-size:.85em;margin:0 0 12px}
.n{padding:12px;margin:4px 0;background:#1a1a1a;border-radius:8px;cursor:pointer;
   border:2px solid transparent;display:flex;justify-content:space-between;align-items:center}
.n span{font-size:.75em;color:#666}
.n.sel{border-color:#00ff88;background:#0d1f14}
input{width:100%;padding:12px;margin:8px 0 16px;background:#1a1a1a;color:#ddd;
      border:1px solid #333;border-radius:8px;font-size:1em}
button{width:100%;padding:14px;background:#00ff88;color:#000;font-weight:700;
       border:none;border-radius:8px;font-size:1em;cursor:pointer}
#st{margin-top:12px;text-align:center;min-height:20px;font-size:.9em}
.ok{color:#00ff88}.err{color:#ff4444}
</style></head><body>
<h1>CyberShot WiFi Setup</h1>
<p>Select your network and enter the password:</p>
)" + nets + R"(
<input type='password' id='pw' placeholder='Password' autocomplete='current-password'>
<button onclick='go()'>Connect</button>
<div id='st'></div>
<script>
var sel='';
function pick(el){
  document.querySelectorAll('.n').forEach(function(e){e.classList.remove('sel')});
  el.classList.add('sel'); sel=el.dataset.s;
}
function go(){
  if(!sel){document.getElementById('st').innerHTML='<span class=err>Select a network first</span>';return}
  var pw=document.getElementById('pw').value;
  document.getElementById('st').innerHTML='Connecting to <b>'+sel+'</b>...';
  fetch('/configure',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(sel)+'&pass='+encodeURIComponent(pw)
  }).then(function(r){return r.text()}).then(function(t){
    if(t.startsWith('OK:')){
      var ip=t.slice(3);
      document.getElementById('st').innerHTML='<span class=ok>Connected! Open <a href="http://'+ip+'" style="color:#00ff88">http://'+ip+'</a> to use the camera.</span>';
    } else {
      document.getElementById('st').innerHTML='<span class=err>Failed — wrong password?</span>';
    }
  }).catch(function(){
    document.getElementById('st').innerHTML='<span class=ok>Connected! Check your camera IP.</span>';
  });
}
</script></body></html>)";

    server.send(200, "text/html", html);
}

void handleConfigure() {
    if (!server.hasArg("ssid")) { server.send(400, "text/plain", "missing ssid"); return; }
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");

    WiFi.mode(WIFI_STA);
    WiFi.begin(newSsid.c_str(), newPass.c_str());

    unsigned long t = millis();
    while (millis() - t < 12000 && WiFi.status() != WL_CONNECTED) {
        dnsServer.processNextRequest();
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        server.send(200, "text/plain", "OK:" + ip);
        delay(200);

        saveWiFiCreds(newSsid, newPass);
        dnsServer.stop();
        wifiSetup = false;
        wifiOK    = true;
        wifiAP    = false;
        server.stop();
        delay(200);
        setupWebServer();

        // Atualiza TFT com IP
        tft.fillRect(0, 88, 160, 8, ST77XX_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(8, 88);
        tft.printf("IP: %s", ip.c_str());
        vfNeedsClear = true;
    } else {
        // Falhou — volta ao AP de setup
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("CyberShot-Setup");
        server.send(200, "text/plain", "FAIL");
    }
}

void startWiFiPortal() {
    server.stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CyberShot-Setup");

    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/",          handleSetupPage);
    server.on("/configure", HTTP_POST, handleConfigure);
    server.onNotFound([](){ server.sendHeader("Location","http://192.168.4.1/",true); server.send(302,"text/plain",""); });
    server.begin();

    wifiSetup = true;
    wifiOK    = false;
    wifiAP    = false;

    // TFT: instrução
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(0x07E0);
    tft.setCursor(8, 30); tft.print("WiFi Setup:");
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(8, 45); tft.print("Connect to:");
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(8, 55); tft.print("CyberShot-Setup");
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(8, 70); tft.print("then open browser");
    tft.setTextColor(0x7BEF);
    tft.setCursor(8, 85); tft.print("192.168.4.1");
    tft.setTextColor(0x2965);
    tft.setCursor(4, 120); tft.print("[HOLD] cancel");
}

// ─── Web server (modo câmera normal) ─────────────────────────────────────────

void setupWebServer() {
    MDNS.begin("cybershot");
    server.on("/",        handleRoot);
    server.on("/foto",    handleFoto);
    server.on("/galeria", handleGallery);
    server.on("/editor",  handleEditor);
    server.on("/delete",  handleDelete);
    server.onNotFound(handleSDFile);
    server.begin();
    IPAddress ip = wifiAP ? WiFi.softAPIP() : WiFi.localIP();
    Serial.printf("http://%s\n", ip.toString().c_str());
}

// Tenta STA com credenciais salvas na NVS. Se não houver ou falhar, fica offline.
// Exibe status no TFT (chamada dentro de setup(), linha y=50).
void setupWiFi() {
    tft.setTextSize(1);
    tft.setTextColor(0x7BEF);
    tft.setCursor(8, 50); tft.print("WIFI ...");

    String ssid, pass;
    if (!loadWiFiCreds(ssid, pass)) {
        tft.fillRect(0, 50, 160, 8, ST77XX_BLACK);
        tft.setTextColor(ST77XX_YELLOW);
        tft.setCursor(8, 50);
        tft.print("WIFI not configured");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long t = millis();
    while (millis() - t < 10000 && WiFi.status() != WL_CONNECTED) delay(200);

    tft.fillRect(0, 50, 160, 8, ST77XX_BLACK);
    tft.setCursor(8, 50);
    if (WiFi.status() == WL_CONNECTED) {
        wifiOK = true;
        wifiAP = false;
        setupWebServer();
        tft.setTextColor(ST77XX_GREEN);
        tft.printf("WIFI %s", WiFi.localIP().toString().c_str());
    } else {
        WiFi.disconnect(true);
        tft.setTextColor(ST77XX_YELLOW);
        tft.print("WIFI failed");
    }
}

// ─── SD ──────────────────────────────────────────────────────────────────────

bool saveToSD(const uint8_t* buf, size_t len, char* nameOut) {
    if (!sdOK) return false;
    do {
        photoCount++;
        sprintf(nameOut, "/PHOTO_%04d.JPG", photoCount);
    } while (SD_MMC.exists(nameOut) && photoCount < 9999);
    File f = SD_MMC.open(nameOut, FILE_WRITE);
    if (!f) return false;
    f.write(buf, len);
    f.close();
    return true;
}

// ─── Glitch: DQT frequency erosion ───────────────────────────────────────────
//
// Derive a 0.0–1.0 "darkness factor" from the exposure state at capture time.
// Dark scenes (high AEC/gain) → higher factor → more aggressive frequency erosion.
// Bright scenes → subtle erosion of only the highest frequencies.
static float glitchFactor() {
    float aec  = (float)vfAecValue / 1200.0f;
    float gain = (float)vfAgcGain  / 30.0f;
    return constrain(aec * 0.35f + gain * 0.65f, 0.05f, 0.95f);
}

// Modifies DQT quantization tables in-place inside the JPEG buffer.
//
// Works with absolute values instead of relative multiplication — necessary
// because quality=12 JPEG already has high quantization values, making
// multiplication ineffective. Instead, we SET coefficients directly:
//
//   k=0  (DC):        untouched — preserves overall brightness per block
//   k=1..cutLow:      set to 1 (minimum) → decoder amplifies these components
//                     enormously → strong blocking, posterization, tonal jumps
//   k=cutLow..cutHi:  untouched — transition band
//   k=cutHi..63:      set to 255 (maximum) → decoder zeroes these out → no texture
//
// The two cut points are driven by gf:
//   gf low  → narrow low-freq band set to 1, most frequencies untouched (subtle)
//   gf high → wide low-freq band set to 1, wide high-freq band set to 255 (aggressive)
//
// Setting low-freq coefs to 1 is the key insight: the encoder wrote coefficients
// already scaled by the original (high) quant value. The decoder divides by the
// quant value in the file — if we write 1 here, the decoder multiplies by 1/original
// of what it expects, producing DC-level jumps between 8x8 blocks: posterization.
void applyGlitchDQT(uint8_t* buf, size_t len) {
    float gf = glitchFactor();

    // low-freq cut: k indices 1..cutLow get set to 1
    // gf=0.05 → cutLow=1 (barely anything); gf=0.95 → cutLow=18
    int cutLow = (int)(gf * 19.0f);

    // high-freq cut: k indices cutHi..63 get set to 255
    // gf=0.05 → cutHi=62 (almost nothing); gf=0.95 → cutHi=32
    int cutHi = 63 - (int)(gf * 31.0f);

    int i = 0;
    while (i < (int)len - 4) {
        if (buf[i] != 0xFF || buf[i + 1] != 0xDB) { i++; continue; }

        int segEnd = i + 2 + ((buf[i + 2] << 8) | buf[i + 3]);
        if (segEnd > (int)len) break;

        int pos = i + 4;
        while (pos < segEnd - 1) {
            int precision  = (buf[pos] >> 4) & 0x0F;
            int tableBytes = precision ? 128 : 64;
            pos++;
            if (pos + tableBytes > segEnd) break;

            for (int k = 0; k < 64; k++) {
                uint8_t newVal = 0;
                if      (k == 0)              newVal = buf[pos];   // DC: keep
                else if (k <= cutLow)         newVal = 1;          // low-freq: min quant → max decoder amplification
                else if (k >= cutHi)          newVal = 255;        // high-freq: max quant → zeroed out
                else                          newVal = buf[pos];   // transition band: keep

                if (precision) {
                    buf[pos]     = 0;
                    buf[pos + 1] = newVal;
                    pos += 2;
                } else {
                    buf[pos] = newVal;
                    pos++;
                }
            }
        }

        i = segEnd;
    }
}

// ─── Glitch: scan data transplant ────────────────────────────────────────────
//
// The OV2640 does not emit restart markers, so segmentation is done
// proportionally: the scan bitstream is treated as N equal-sized virtual
// "bands" mapped to image height. Overwriting one band's bytes with another's
// de-syncs the Huffman decoder at that point — it tries to parse the wrong
// bitstream and produces cascading color drift, brightness smear, and block
// echo artifacts that propagate until it finds a valid code pattern again.
// This cascade is the VHS head-misalignment look.
//
// Two transplants are performed at positions derived from the image's own
// exposure values (vfAecValue → upper transplant, vfAgcGain → lower).
// The donor region is offset by ~1/4 of scan length from the recipient,
// so the "echo" comes from a meaningfully different part of the scene.
void applyGlitchScan(uint8_t* buf, size_t len) {
    // ── 1. Locate scan data (SOS marker) ──────────────────────────────────
    int scanStart = -1;
    for (int i = 0; i < (int)len - 3; i++) {
        if (buf[i] == 0xFF && buf[i + 1] == 0xDA) {
            scanStart = i + 2 + ((buf[i + 2] << 8) | buf[i + 3]);
            break;
        }
    }
    if (scanStart < 0) return;

    // ── 2. Locate EOI to bound scan data ──────────────────────────────────
    int scanEnd = (int)len - 2;
    for (int i = (int)len - 2; i >= scanStart; i--) {
        if (buf[i] == 0xFF && buf[i + 1] == 0xD9) { scanEnd = i; break; }
    }
    int scanLen = scanEnd - scanStart;
    if (scanLen < 512) return;

    // ── 3. Randomized glitch parameters (controlled variation per shot) ───
    //
    //   nTransplants : 2–6   number of cut points
    //   divisor      : 8–24  band width = scanLen / divisor
    //   donorPct     : 25–66 donor offset as % of scanLen
    //
    // esp_random() uses the ESP32 hardware RNG — no seed needed.
    uint32_t rng = esp_random();
    int nTransplants = 2   + (int)((rng & 0xFF)         % 3);       // 2..4
    int divisor      = 8   + (int)(((rng >> 8)  & 0xFF) % 11);      // 8..18
    int donorPct     = 25  + (int)(((rng >> 16) & 0xFF) % 26);      // 25..50

    float aecNorm  = (float)vfAecValue / 1200.0f;
    float gainNorm = (float)vfAgcGain  / 30.0f;

    int tLen      = constrain(scanLen / divisor, 64, 4096);
    int donorOff  = (int)((float)scanLen * donorPct / 100.0f);

    Serial.printf("[glitch] transplants=%d  divisor=/%d  donor=%d%%\n",
                  nTransplants, divisor, donorPct);

    // ── 4. Perform transplants ─────────────────────────────────────────────
    for (int t = 0; t < nTransplants; t++) {
        float base = (float)t / nTransplants
                   + (t % 2 == 0 ? aecNorm : gainNorm) * (1.0f / nTransplants * 0.6f);
        int r = scanStart + (int)(base * (scanLen - tLen));
        int d = scanStart + ((r - scanStart + donorOff) % scanLen);
        int l = tLen;
        if (r + l > scanEnd) l = scanEnd - r;
        if (l > 0 && d + l <= scanEnd)
            memcpy(buf + r, buf + d, l);
    }
}

// ─── Glitch: chroma DQT amplification ────────────────────────────────────────
//
// JPEG stores color in YCbCr with separate DQT tables: table ID 0 = luma (Y),
// table ID 1 = chroma (Cb/Cr). Leaving luma untouched preserves all structure,
// edges, and brightness. Manipulating only the chroma table produces purely
// chromatic distortion — the image stays sharp and recognizable but colors
// bleed, oversaturate, and misregister like offset printing with wrong ink density.
//
// Approach A — chroma DC + low-freq amplification:
//   Set DC (k=0) and low-frequency chroma coefficients (k=1..cutLow) to very
//   small values (1..ampFloor). The decoder divides stored coefficients by these
//   values — dividing by 1 instead of the original ~30 amplifies color deltas
//   enormously → saturation explodes, color bleeds across block boundaries.
//
// The amplification depth is derived from the scene's exposure:
//   darkness → cutLow   (how many low-freq coefficients get amplified)
//   darkness → ampFloor (minimum quantization value — lower = more amplification)
void applyGlitchChroma(uint8_t* buf, size_t len) {
    float aecNorm  = (float)vfAecValue / 1200.0f;
    float gainNorm = (float)vfAgcGain  / 30.0f;
    float darkness = aecNorm * 0.4f + gainNorm * 0.6f;  // 0=bright, 1=dark

    // Target mid-frequency chroma AC band (k=kMidLo..kMidHi).
    // With quality=12 the encoder preserves more AC chroma values in this band,
    // giving amplification real material → color variation within each block
    // → smaller apparent patches than DC-only amplification.
    // DC (k=0) also amplified at moderate level to keep bold color character
    // without creating large solid-color 16×16 patches.
    int kMidLo = 4;
    int kMidHi = 18 + (int)(darkness * 10.0f);  // 18..28
    int dcVal  = 162 + (int)(darkness * 18.0f);  // 162..180  (-10%)
    int acVal  = 198 + (int)(darkness * 32.0f);  // 198..230  (-10%)

    Serial.printf("[glitch] chroma dc=%d ac=%d kMid=%d..%d darkness=%.2f\n",
                  dcVal, acVal, kMidLo, kMidHi, darkness);

    // ── Scan all DQT tables and apply amplification ───────────────────────
    // Strategy: prefer chroma table (ID=1) if it exists; otherwise use all
    // tables but skip only k=0 (DC) to preserve global brightness.
    // Count tables first, then decide.
    bool hasChromaTable = false;
    int  tablesFound    = 0;
    int  i = 0;
    while (i < (int)len - 4) {
        if (buf[i] != 0xFF || buf[i + 1] != 0xDB) { i++; continue; }
        int segEnd = i + 2 + ((buf[i + 2] << 8) | buf[i + 3]);
        if (segEnd > (int)len) break;
        int pos = i + 4;
        while (pos < segEnd - 1) {
            int precision  = (buf[pos] >> 4) & 0x0F;
            int tableId    =  buf[pos] & 0x0F;
            int tableBytes = precision ? 128 : 64;
            Serial.printf("[glitch] DQT tableId=%d precision=%d\n", tableId, precision);
            if (tableId == 1) hasChromaTable = true;
            tablesFound++;
            pos += 1 + tableBytes;
        }
        i = segEnd;
    }
    Serial.printf("[glitch] tables=%d hasChroma=%d\n", tablesFound, hasChromaTable);

    // Amplify: if chroma table exists touch only ID=1; else touch everything
    // (single-table OV2640 case) skipping only k=0 to keep global brightness.
    i = 0;
    while (i < (int)len - 4) {
        if (buf[i] != 0xFF || buf[i + 1] != 0xDB) { i++; continue; }
        int segEnd = i + 2 + ((buf[i + 2] << 8) | buf[i + 3]);
        if (segEnd > (int)len) break;

        int pos = i + 4;
        while (pos < segEnd - 1) {
            int precision  = (buf[pos] >> 4) & 0x0F;
            int tableId    =  buf[pos] & 0x0F;
            int tableBytes = precision ? 128 : 64;
            pos++;
            if (pos + tableBytes > segEnd) break;

            bool modify = hasChromaTable ? (tableId == 1) : true;

            if (modify) {
                for (int k = 0; k < 64; k++) {
                    uint8_t newVal = 0;
                    if (k == 0)                        newVal = (uint8_t)dcVal;   // DC: moderate boost
                    else if (k >= kMidLo && k <= kMidHi) newVal = (uint8_t)acVal; // mid-freq AC: strong
                    // all other k: leave original value (keeps them unchanged)

                    if (newVal > 0) {
                        if (precision) {
                            buf[pos]     = 0;
                            buf[pos + 1] = newVal;
                        } else {
                            buf[pos] = newVal;
                        }
                    }
                    pos += precision ? 2 : 1;
                }
            } else {
                pos += tableBytes;
            }
        }
        i = segEnd;
    }
}

// ─── Glitch: DQT zigzag permutation ──────────────────────────────────────────
//
// JPEG DQT tables store 64 quantization values in zigzag scan order:
//   k=0       → DC coefficient (block average brightness)
//   k=1..10   → low spatial frequencies (broad shapes, slow gradients)
//   k=11..30  → mid frequencies (textures, contours)
//   k=31..63  → high frequencies (fine edges, noise)
//
// At quality=12, values rise steeply from DC (~2) to high freq (~80+).
// DC gets near-lossless precision; high freq is coarsely quantized.
//
// Effect: rotate the 63 AC values (k=1..63) by N positions (circular).
//   - Values intended for low-freq slots go to mid/high-freq slots →
//     low-freq DCT coefficients are now coarsely quantized → posterized
//     blocks, stepped tonal gradients, loss of smooth areas.
//   - Values intended for high-freq slots go to low-freq slots →
//     high-freq DCT coefficients are finely quantized → edges and
//     textures are over-preserved, amplified, ring/emboss-like.
//
// DC (k=0) is intentionally kept untouched — this preserves overall
// block brightness and keeps the image recognizable no matter the rotation.
//
// Rotation amount driven by glitchFactor(): darker/higher-gain scenes
// rotate further (more scrambling); bright scenes rotate less (subtle).
//   gf ≈ 0.05 → rotation = 3   (mild frequency swap)
//   gf ≈ 0.95 → rotation = 31  (half the array, maximum inversion)
void applyGlitchZigzag(uint8_t* buf, size_t len) {
    float gf     = glitchFactor();
    int rotation = 4 + (int)(gf * 37.0f);  // 4..41  (-15% vs 5..49)

    int i = 0;
    while (i < (int)len - 4) {
        if (buf[i] != 0xFF || buf[i + 1] != 0xDB) { i++; continue; }

        int segEnd = i + 2 + ((buf[i + 2] << 8) | buf[i + 3]);
        if (segEnd > (int)len) break;

        int pos = i + 4;
        while (pos < segEnd - 1) {
            int precision  = (buf[pos] >> 4) & 0x0F;
            int tableBytes = precision ? 128 : 64;
            pos++;
            if (pos + tableBytes > segEnd) break;

            // Snapshot AC values k=1..63 before modifying
            uint8_t tmp[63];
            for (int k = 0; k < 63; k++)
                tmp[k] = precision ? buf[pos + (k + 1) * 2 + 1] : buf[pos + k + 1];

            // Write back with circular rotation: slot k gets value from slot (k+rotation)%63
            for (int k = 0; k < 63; k++) {
                int src = (k + rotation) % 63;
                if (precision) {
                    buf[pos + (k + 1) * 2]     = 0;
                    buf[pos + (k + 1) * 2 + 1] = tmp[src];
                } else {
                    buf[pos + k + 1] = tmp[src];
                }
            }

            pos += tableBytes;
        }
        i = segEnd;
    }
}

// ─── Glitch: DHT run-length rotation ─────────────────────────────────────────
//
// AC Huffman symbols encode (run << 4) | size:
//   run  = zero-run-length before this coefficient (high nibble, 0..15)
//   size = magnitude category = extra bits to read from scan data (low nibble, 0..10)
//
// Rotating the full symbol array changes 'size' values → decoder reads wrong
// number of extra bits → bit-offset shifts → Huffman de-sync → gray image.
//
// Safe strategy: rotate only WITHIN each size group (same low nibble).
// Symbols with size=N consume exactly N extra bits regardless of run value,
// so swapping run values inside the group never shifts the bit position.
// The scan data stays perfectly in sync; only the zero-run-length before each
// coefficient is interpreted wrongly → coefficients land in wrong positions
// within the 8×8 DCT block → spatial frequency mis-registration:
//   - Texture coefficients appear in smooth-area positions → noise in flat zones
//   - Edge coefficients displaced → ringing artifacts in wrong places
//   - Overall: structured distortion that varies by block content
//
// DC tables are skipped: DC symbols ARE the size category, so any rotation
// would change bit consumption and de-sync the stream.
//
// Refinement: SIZE-SCALED ROTATION
//   rotation for group sz = (base * sz) / 5
//   sz=1 (small magnitude, common) → less rotation → flat areas preserved
//   sz=10 (large magnitude, strong edges) → more rotation → distortion on high-contrast zones
//
// Alternating direction (even/odd sz) was attempted but creates near-reversals
// in small groups (gn=3, backward=2 → 2/3 rotation → chroma explosion). Dropped.
//
// Two base rotations driven by glitchFactor():
//   rotLuma   (tableId=0): 1..6
//   rotChroma (tableId=1): rotLuma-1, capped 1..3 — chroma table smaller, more sensitive
void applyGlitchDHT(uint8_t* buf, size_t len) {
    float gf        = glitchFactor();
    int   rotLuma   = constrain(1 + (int)(gf * 5.0f), 1, 6);
    int   rotChroma = constrain(rotLuma - 1, 1, 3);

    int i = 0;
    while (i < (int)len - 4) {
        if (buf[i] != 0xFF || buf[i + 1] != 0xC4) { i++; continue; }

        int segEnd = i + 2 + ((buf[i + 2] << 8) | buf[i + 3]);
        if (segEnd > (int)len) break;

        int pos = i + 4;
        while (pos < segEnd) {
            int tableClass = (buf[pos] >> 4) & 0x0F;
            int tableId    =  buf[pos] & 0x0F;
            pos++;

            int nSymbols = 0;
            for (int k = 0; k < 16 && pos + k < segEnd; k++)
                nSymbols += buf[pos + k];
            pos += 16;

            // AC tables only — DC symbol values equal size, rotation would de-sync
            if (tableClass == 1 && nSymbols > 1 && pos + nSymbols <= segEnd) {
                int      rotation = (tableId == 0) ? rotLuma : rotChroma;
                uint8_t* syms     = buf + pos;
                for (int sz = 1; sz <= 10; sz++) {
                    uint8_t grpVal[16];
                    int     grpIdx[16];
                    int     gn = 0;
                    for (int k = 0; k < nSymbols && gn < 16; k++) {
                        if ((syms[k] & 0x0F) == sz) {
                            grpVal[gn] = syms[k];
                            grpIdx[gn] = k;
                            gn++;
                        }
                    }
                    if (gn < 2) continue;

                    // Scale rotation by size: high-freq bands (sz grande) giram mais
                    int scaledRot = constrain((rotation * sz) / 5, 1, gn - 1);

                    for (int k = 0; k < gn; k++)
                        syms[grpIdx[k]] = grpVal[(k + scaledRot) % gn];
                }
            }
            pos += nSymbols;
        }
        i = segEnd;
    }
}

// ─── Captura ─────────────────────────────────────────────────────────────────

void drawCaptureStatus(const char* label, int pct) {
    tft.fillScreen(ST77XX_BLACK);
    uint16_t bright = vfPalettes[vfColorIdx][3];
    uint16_t mid    = vfPalettes[vfColorIdx][2];
    uint16_t dim    = vfPalettes[vfColorIdx][1];
    tft.setTextSize(1);
    tft.setTextColor(dim);
    tft.setCursor(52, 14);
    tft.print("CYBER SHOT");
    tft.setTextColor(bright);
    tft.setCursor(8, 44);
    tft.print(label);
    tft.drawRect(8, 60, 144, 10, mid);
    if (pct > 0)
        tft.fillRect(9, 61, 142 * pct / 100, 8, bright);
    tft.setTextColor(mid);
    tft.setCursor(8, 76);
    tft.printf("%d%%", pct);
}

// ─── Preview JPEG no TFT ─────────────────────────────────────────────────────

static JPEGDEC  _jpeg;
static int16_t  _pvX = 0, _pvY = 0;

int previewCallback(JPEGDRAW* pDraw) {
    tft.drawRGBBitmap(pDraw->x + _pvX, pDraw->y + _pvY,
                      pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    return 1;
}

void showPreview(uint8_t* buf, size_t len, const char* filename) {
    if (!_jpeg.openRAM(buf, (int)len, previewCallback)) return;

    _jpeg.setPixelType(RGB565_LITTLE_ENDIAN);

    int pw = _jpeg.getWidth()  / 8;
    int ph = _jpeg.getHeight() / 8;
    _pvX = (160 - pw) / 2;
    _pvY = (128 - ph) / 2;

    tft.fillScreen(ST77XX_BLACK);
    _jpeg.decode(0, 0, JPEG_SCALE_EIGHTH);
    _jpeg.close();

    // overlay: nome do arquivo + instrução
    tft.setTextSize(1);
    tft.setTextColor(0x7BEF);
    tft.setCursor(2, 2);
    tft.print(filename);

    tft.setTextColor(0x2965);
    tft.setCursor(2, 120);
    tft.print("[BTN] close");

    // aguarda botão ou timeout de 6 s
    unsigned long t0 = millis();
    while (millis() - t0 < 6000) {
        if (digitalRead(BTN_PIN) == LOW) { delay(180); break; }
        delay(10);
    }
    vfNeedsClear = true;
}

// ─── Long exposure: frame stacking ──────────────────────────────────────────
// Captura JPEG QVGA e usa JPEGDEC para decodificar cada frame.
// JPEGDEC já funciona corretamente no preview TFT (RGB565_LITTLE_ENDIAN
// confirmado), então os canais R/G/B têm ordem garantida sem ambiguidade.

static uint16_t* g_leR = nullptr;
static uint16_t* g_leG = nullptr;
static uint16_t* g_leB = nullptr;
static int        g_leW = 1024;

int leStackCallback(JPEGDRAW* pDraw) {
    for (int y = 0; y < pDraw->iHeight; y++) {
        for (int x = 0; x < pDraw->iWidth; x++) {
            int idx = (pDraw->y + y) * g_leW + (pDraw->x + x);
            uint16_t p = pDraw->pPixels[y * pDraw->iWidth + x];
            // Acumula bits raw — mais rápido e usa metade da RAM vs float
            g_leR[idx] += (p >> 11) & 0x1F;   // 0-31 por frame
            g_leG[idx] += (p >>  5) & 0x3F;   // 0-63 por frame
            g_leB[idx] +=  p        & 0x1F;   // 0-31 por frame
        }
    }
    return 1;
}

void takeLongExposureStacked() {
    const int W = 1024, H = 768, N = W * H;

    tft.fillScreen(gbPalette[3]); delay(15);
    tft.fillScreen(gbPalette[0]);
    drawCaptureStatus("SETTLING...", 3);

    // JPEG XGA: uint16_t acumuladores = 4.5MB (cabe nos 8MB PSRAM)
    if (!initCamera(PIXFORMAT_JPEG, FRAMESIZE_XGA, 12, 2)) {
        initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
        vfNeedsClear = true; return;
    }

    // Começa com exposição máxima, habilita AE auto — converge de cima pra baixo (rápido)
    {
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_aec_value(s, 1200);
            s->set_agc_gain(s, 30);
            s->set_exposure_ctrl(s, 1);
            s->set_gain_ctrl(s, 1);
            s->set_whitebal(s, 1);
            s->set_awb_gain(s, 1);
        }
    }

    // 15 frames: AE desce do máximo até valor correto
    for (int i = 0; i < 15; i++) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
        delay(1);
    }
    // AE permanece em auto durante toda a captura

    uint16_t* accR = (uint16_t*)ps_malloc(N * sizeof(uint16_t));
    uint16_t* accG = (uint16_t*)ps_malloc(N * sizeof(uint16_t));
    uint16_t* accB = (uint16_t*)ps_malloc(N * sizeof(uint16_t));

    if (!accR || !accG || !accB) {
        free(accR); free(accG); free(accB);
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_RED); tft.setTextSize(1);
        tft.setCursor(8, 55); tft.print("PSRAM error");
        delay(2000);
        initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
        vfNeedsClear = true; return;
    }
    for (int i = 0; i < N; i++) { accR[i] = 0; accG[i] = 0; accB[i] = 0; }

    g_leR = accR; g_leG = accG; g_leB = accB; g_leW = W;

    // Usa _jpeg (static, em BSS) — JPEGDEC local causaria stack overflow
    int frameCount = 0;
    unsigned long t0      = millis();
    unsigned long totalMs = (unsigned long)leSeconds * 1000UL;
    int lastPct = 5;

    digitalWrite(LED_FLASH, HIGH);
    while (millis() - t0 < totalMs) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { delay(5); continue; }

        if (fb->format == PIXFORMAT_JPEG && fb->len > 0) {
            if (_jpeg.openRAM(fb->buf, (int)fb->len, leStackCallback)) {
                _jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                _jpeg.decode(0, 0, 0);   // resolução completa, sem escala
                _jpeg.close();
                frameCount++;
            }
        }
        esp_camera_fb_return(fb);
        delay(1);   // yield RTOS / watchdog

        int pct = 5 + (int)((millis() - t0) * 85UL / totalMs);
        if (pct > lastPct) {
            int sLeft = leSeconds - (int)((millis() - t0) / 1000);
            char lbl[22]; snprintf(lbl, sizeof(lbl), "EXPOSING... %ds", sLeft);
            drawCaptureStatus(lbl, pct);
            lastPct = pct;
        }
    }

    digitalWrite(LED_FLASH, LOW);
    g_leR = g_leG = g_leB = nullptr;
    drawCaptureStatus("PROCESSING...", 92);

    if (frameCount == 0) {
        free(accR); free(accG); free(accB);
        initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
        vfNeedsClear = true; return;
    }

    // Mede luma média (bits raw: R/B em 0-31, G em 0-63) → escala para 0.0-1.0
    float sumLuma = 0.0f;
    {
        const float kR = 0.299f / (31.0f * frameCount);
        const float kG = 0.587f / (63.0f * frameCount);
        const float kB = 0.114f / (31.0f * frameCount);
        for (int i = 0; i < N; i++)
            sumLuma += accR[i] * kR + accG[i] * kG + accB[i] * kB;
    }
    float avgLuma = sumLuma / (float)N;
    float boost   = (avgLuma > 0.005f) ? fminf(6.0f, 0.35f / avgLuma) : 1.0f;

    uint8_t* rgb = (uint8_t*)ps_malloc(N * 3);
    if (rgb) {
        const float invR = 255.0f * boost / (31.0f * frameCount);
        const float invG = 255.0f * boost / (63.0f * frameCount);
        for (int i = 0; i < N; i++) {
            float r = accR[i] * invR;
            float g = accG[i] * invG;
            float b = accB[i] * invR;
            // fmt2jpg(PIXFORMAT_RGB888) interpreta o buffer como BGR888
            rgb[i * 3    ] = b > 255.0f ? 255 : (uint8_t)b;
            rgb[i * 3 + 1] = g > 255.0f ? 255 : (uint8_t)g;
            rgb[i * 3 + 2] = r > 255.0f ? 255 : (uint8_t)r;
        }
    }
    free(accR); free(accG); free(accB);

    if (!rgb) {
        initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
        vfNeedsClear = true; return;
    }

    uint8_t* jpg    = nullptr;
    size_t   jpgLen = 0;
    fmt2jpg(rgb, N * 3, W, H, PIXFORMAT_RGB888, 90, &jpg, &jpgLen);
    free(rgb);

    if (!jpg || jpgLen == 0) {
        free(jpg);
        initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
        vfNeedsClear = true; return;
    }

    if (photoBuf) { free(photoBuf); photoBuf = nullptr; }
    photoBuf = (uint8_t*)ps_malloc(jpgLen);
    if (photoBuf) {
        memcpy(photoBuf, jpg, jpgLen);
        photoLen   = jpgLen;
        photoReady = true;
    }
    free(jpg);

    // Efeitos de glitch (JPEG databending)
    if (photoBuf && (fxDQT || fxScan || fxChroma || fxZigzag || fxDHT)) {
        drawCaptureStatus("GLITCHING...", 96);
        if (fxZigzag) applyGlitchZigzag(photoBuf, photoLen);
        if (fxDQT)    applyGlitchDQT(photoBuf, photoLen);
        if (fxDHT)    applyGlitchDHT(photoBuf, photoLen);
        if (fxScan)   applyGlitchScan(photoBuf, photoLen);
        if (fxChroma) applyGlitchChroma(photoBuf, photoLen);
    }

    // WiFi
    if (!wifiAP && !wifiOK && WiFi.status() == WL_CONNECTED) {
        wifiOK = true; setupWebServer();
    }

    char filename[32] = "";
    bool savedSD = saveToSD(photoBuf, photoLen, filename);

    initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
    drawCaptureStatus("DONE!", 100);

    if (photoBuf) {
        char label[36];
        if (savedSD) snprintf(label, sizeof(label), "%s", filename + 1);
        else         snprintf(label, sizeof(label), "RAM (no SD)");
        showPreview(photoBuf, photoLen, label);
    } else {
        vfNeedsClear = true;
    }
}

void takePhoto() {
    if (leSeconds > 0) { takeLongExposureStacked(); return; }

    // shutter flash
    tft.fillScreen(gbPalette[3]); delay(25);
    tft.fillScreen(gbPalette[1]); delay(20);
    tft.fillScreen(gbPalette[0]);

    const char* captureLabel = "CAPTURING...";
    drawCaptureStatus(captureLabel, 5);

    // ── Captura JPEG (pipeline único para todos os efeitos) ─────────────────
    if (!initCamera(PIXFORMAT_JPEG, FRAMESIZE_XGA, 12, 1)) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_RED); tft.setTextSize(1);
        tft.setCursor(4, 55); tft.print("JPEG camera error");
        delay(2000);
        initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
        vfNeedsClear = true; return;
    }

    drawCaptureStatus(captureLabel, 25);
    digitalWrite(LED_FLASH, HIGH);
    for (int i = 0; i < 2; i++) {
        camera_fb_t* w = esp_camera_fb_get();
        if (w) esp_camera_fb_return(w);
    }

    drawCaptureStatus(captureLabel, 50);
    camera_fb_t* fb = esp_camera_fb_get();
    digitalWrite(LED_FLASH, LOW);

    if (!fb) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_RED); tft.setTextSize(1);
        tft.setCursor(4, 55); tft.print("Capture error");
        delay(2000);
        initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);
        vfNeedsClear = true; return;
    }

    if (photoBuf) { free(photoBuf); photoBuf = nullptr; }
    photoBuf = (uint8_t*)ps_malloc(fb->len);
    if (photoBuf) {
        memcpy(photoBuf, fb->buf, fb->len);
        photoLen   = fb->len;
        photoReady = true;
    }
    esp_camera_fb_return(fb); fb = nullptr;

    // ── JPEG FX: databending em photoBuf ───────────────────────────────────
    if (photoBuf && (fxDQT || fxScan || fxChroma || fxZigzag || fxDHT)) {
        drawCaptureStatus("GLITCHING...", 90);
        if (fxZigzag) applyGlitchZigzag(photoBuf, photoLen);
        if (fxDQT)    applyGlitchDQT(photoBuf, photoLen);
        if (fxDHT)    applyGlitchDHT(photoBuf, photoLen);
        if (fxScan)   applyGlitchScan(photoBuf, photoLen);
        if (fxChroma) applyGlitchChroma(photoBuf, photoLen);
    }

    // reconnect STA se necessário (não aplica em AP mode)
    if (!wifiAP) {
        if (!wifiOK && WiFi.status() == WL_CONNECTED) { wifiOK = true; setupWebServer(); }
        if (!wifiOK) {
            unsigned long t = millis();
            while (millis() - t < 4000 && WiFi.status() != WL_CONNECTED) delay(100);
            if (WiFi.status() == WL_CONNECTED) { wifiOK = true; setupWebServer(); }
        }
    }

    char filename[32] = "";
    bool savedSD = saveToSD(photoBuf, photoLen, filename);

    // reinicia câmera para viewfinder
    initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2);

    drawCaptureStatus("DONE!", 100);

    // preview da foto no TFT
    if (photoBuf) {
        char label[36];
        if (savedSD) snprintf(label, sizeof(label), "%s", filename + 1);
        else         snprintf(label, sizeof(label), "RAM (no SD)");
        showPreview(photoBuf, photoLen, label);
    } else {
        vfNeedsClear = true;
    }
}

// ─── Web handlers ─────────────────────────────────────────────────────────────

void handleEditor() {
    String file = server.arg("file");
    String ram  = server.arg("ram");
    String src  = ram.length() ? "/foto" : ("/sd/" + file);
    String dlname = file.length() ? (file.substring(0, file.lastIndexOf('.')) + "_edit.jpg") : "cybershot_edit.jpg";

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // ── HEAD + CSS ──────────────────────────────────────────────────────────
    server.sendContent(
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta charset='utf-8'><title>editor</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{background:#0a0a0a;color:#ccc;font-family:monospace;margin:0;overflow:hidden}"
        ".topbar{display:flex;align-items:center;justify-content:space-between;padding:6px 8px;background:#0d0d0d;border-bottom:1px solid #1a1a1a}"
        ".topbar a{color:#0f0;font-size:12px;text-decoration:none}"
        ".topbar h2{color:#0f0;font-size:14px}"
        ".fn{font-size:10px;color:#555;padding:2px 8px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;background:#0d0d0d;border-bottom:1px solid #111}"
        ".editor-layout{display:flex;flex-direction:column;height:calc(100vh - 58px)}"
        ".canvas-panel{display:flex;flex-direction:column;align-items:center;justify-content:center;background:#111;flex-shrink:0;max-height:42vh;overflow:hidden}"
        ".wrap{width:100%}"
        "canvas{max-width:100%;max-height:38vh;display:block;margin:0 auto}"
        "#status{font-size:10px;color:#444;padding:2px 8px;text-align:center}"
        ".ctrl-panel{flex:1;overflow-y:auto;padding:8px}"
        "@media(min-width:600px){"
          ".editor-layout{flex-direction:row;height:calc(100vh - 58px)}"
          ".canvas-panel{flex:1;max-height:none;height:100%;align-self:stretch}"
          "canvas{max-height:calc(100vh - 90px);max-width:100%}"
          ".ctrl-panel{width:300px;min-width:280px;height:100%;border-left:1px solid #1a1a1a;padding:8px}}"
        ".sec{font-size:9px;color:#888;text-transform:uppercase;letter-spacing:2px;margin:8px 0 4px;border-bottom:1px solid #1a1a1a;padding-bottom:2px}"
        ".row{display:flex;align-items:center;gap:6px;margin:4px 0}"
        "label{width:72px;font-size:10px;color:#777;flex-shrink:0}"
        "input[type=range]{flex:1;accent-color:#0f0;height:20px}"
        ".val{width:28px;text-align:right;font-size:10px;color:#aaa}"
        ".btns{display:flex;flex-wrap:wrap;gap:5px;margin:6px 0}"
        "button{background:#141414;border:1px solid #2a2a2a;color:#0f0;"
        "padding:8px 12px;font-family:monospace;font-size:11px;"
        "cursor:pointer;border-radius:3px;touch-action:manipulation;flex:1;min-width:60px}"
        "button:active{opacity:.7}"
        "button.on{background:#0f0;color:#000;border-color:#0f0}"
        ".rot button{color:#08f;border-color:#08f}"
        ".rot button.on{background:#08f;color:#000}"
        ".pxs button{color:#f0f;border-color:#f0f}"
        ".pxs button.on{background:#f0f;color:#000}"
        ".ebt button{color:#0ff;border-color:#0ff}"
        ".ebt button.on{background:#0ff;color:#000}"
        ".crp button{color:#fa0;border-color:#fa0;min-width:44px}"
        ".crp button.on{background:#fa0;color:#000}"
        "#bSave{border-color:#ff0;color:#ff0;flex:2}"
        "#bSave.on{background:#ff0;color:#000}"
        ".divider{height:1px;background:#161616;margin:8px 0}"
        "</style></head><body>"
    );

    // ── TOP BAR ─────────────────────────────────────────────────────────────
    server.sendContent(
        "<div class='topbar'>"
        "<a href='/galeria'>&#8592; gallery</a>"
        "<h2>EDITOR</h2><span></span>"
        "</div>"
    );
    server.sendContent(String("<div class='fn'>") + (file.length() ? file : "last photo (RAM)") + "</div>");

    // ── CANVAS + CONTROLES ──────────────────────────────────────────────────
    server.sendContent(
        "<div class='editor-layout'>"
        "<div class='canvas-panel'>"
        "<div class='wrap'><canvas id='c'></canvas></div>"
        "<div id='status'>loading...</div>"
        "</div>"
        "<div class='ctrl-panel'>"

        "<div class='sec'>adjust</div>"
        "<div class='row'><label>brightness</label><input type='range' id='sBri' min='-100' max='100' value='0'><span class='val' id='vBri'>0</span></div>"
        "<div class='row'><label>contrast</label><input type='range' id='sCon' min='-100' max='100' value='0'><span class='val' id='vCon'>0</span></div>"
        "<div class='row'><label>saturation</label><input type='range' id='sSat' min='-100' max='100' value='0'><span class='val' id='vSat'>0</span></div>"
        "<div class='row'><label>shadows</label><input type='range' id='sSha' min='-100' max='100' value='0'><span class='val' id='vSha'>0</span></div>"
        "<div class='row'><label>highlights</label><input type='range' id='sHil' min='-100' max='100' value='0'><span class='val' id='vHil'>0</span></div>"
        "<div class='row'><label>temp</label><input type='range' id='sTemp' min='-50' max='50' value='0'><span class='val' id='vTemp'>0</span></div>"
        "<div class='row'><label>fade</label><input type='range' id='sFade' min='0' max='100' value='0'><span class='val' id='vFade'>0</span></div>"
        "<div class='row'><label>vignette</label><input type='range' id='sVig' min='0' max='100' value='0'><span class='val' id='vVig'>0</span></div>"
        "<div class='row'><label>chroma ab.</label><input type='range' id='sCA' min='0' max='20' value='0'><span class='val' id='vCA'>0</span></div>"

        "<div class='sec'>crop</div>"
        "<div class='btns crp'>"
        "<button id='bCrOrig' class='on'>orig</button>"
        "<button id='bCr11'>1:1</button>"
        "<button id='bCr45'>4:5</button>"
        "<button id='bCr169'>16:9</button>"
        "<button id='bCr916'>9:16</button>"
        "<button id='bCr32'>3:2</button>"
        "</div>"
        "<div class='row'><label>scale</label><input type='range' id='sCrSc' min='25' max='100' value='100'><span class='val' id='vCrSc'>100</span></div>"
        "<div class='row'><label>pan X</label><input type='range' id='sCrX' min='0' max='100' value='50'><span class='val' id='vCrX'>50</span></div>"
        "<div class='row'><label>pan Y</label><input type='range' id='sCrY' min='0' max='100' value='50'><span class='val' id='vCrY'>50</span></div>"
        "<div class='btns crp'><button id='bCrApply'>&#9986; apply crop</button></div>"

        "<div class='sec'>filter</div>"
        "<div class='btns'>"
        "<button id='bGray'>grayscale</button>"
        "<button id='bSepia'>sepia</button>"
        "<button id='bInvert'>invert</button>"
        "<button id='bNoir'>noir</button>"
        "</div>"

        "<div class='sec'>rotation</div>"
        "<div class='btns rot'>"
        "<button id='bRotL'>&#8634; 90°</button>"
        "<button id='bRotR'>&#8635; 90°</button>"
        "<button id='bFlip'>&#8596; flip</button>"
        "</div>"

        "<div class='sec'>pixel sort</div>"
        "<div class='row'><label>thr min</label>"
        "<input type='range' id='sPsMin' min='0' max='100' value='20'>"
        "<span class='val' id='vPsMin'>20</span></div>"
        "<div class='row'><label>thr max</label>"
        "<input type='range' id='sPsMax' min='0' max='100' value='80'>"
        "<span class='val' id='vPsMax'>80</span></div>"
        "<div class='btns'>"
        "<button id='bPsH' class='on'>&#8596; H</button>"
        "<button id='bPsV'>&#8597; V</button>"
        "<button id='bPsLuma' class='on'>luma</button>"
        "<button id='bPsHue'>hue</button>"
        "<button id='bPsSat'>sat</button>"
        "</div>"
        "<div class='btns pxs'><button id='bPxSort'>&#8801; sort: off</button></div>"

        "<div class='sec'>8 bit</div>"
        "<div class='row'><label>pixels</label>"
        "<input type='range' id='sEBsz' min='4' max='24' value='8' step='2'>"
        "<span class='val' id='vEBsz'>8</span></div>"
        "<div class='row'><label>colors</label>"
        "<input type='range' id='sELvl' min='2' max='8' value='4' step='1'>"
        "<span class='val' id='vELvl'>4</span></div>"
        "<div class='btns ebt'><button id='bEight'>&#9632; 8bit: off</button></div>"

        "<div class='divider'></div>"
        "<div class='btns'>"
        "<button id='bReset'>reset</button>"
        "<button id='bSave'>&#8595; save</button>"
        "</div></div></div>"
    );

    // ── JS: carregamento da imagem ───────────────────────────────────────────
    server.sendContent(String(
        "<script>"
        "const c=document.getElementById('c'),ctx=c.getContext('2d');"
        "let orig=null,origW=0,origH=0,rot=0,flipH=false,filt=null,eightOn=false,cropRatio=null;"
        "const img=new Image();"
        "img.crossOrigin='anonymous';"
        "img.onload=()=>{"
          "origW=img.width;origH=img.height;"
          "const t=document.createElement('canvas');"
          "t.width=origW;t.height=origH;"
          "t.getContext('2d').drawImage(img,0,0);"
          "orig=t.getContext('2d').getImageData(0,0,origW,origH);"
          "document.getElementById('status').textContent=origW+'x'+origH+' — pronto';"
          "render();"
        "};"
        "img.onerror=()=>document.getElementById('status').textContent='erro ao carregar';"
        "img.src='") + src + "';"
    );

    // ── JS: render() ────────────────────────────────────────────────────────
    server.sendContent(
        "function clamp(v){return Math.max(0,Math.min(255,v))}"
        "function render(){"
          "if(!orig)return;"
          // pixel ops (bri/con/sat/filter)
          "const d=new ImageData(new Uint8ClampedArray(orig.data),origW,origH);"
          "const px=d.data;"
          "const br=+document.getElementById('sBri').value;"
          "const co=+document.getElementById('sCon').value;"
          "const sa=+document.getElementById('sSat').value;"
          "const te=+document.getElementById('sTemp').value;"
          "const sh=+document.getElementById('sSha').value;"
          "const hi=+document.getElementById('sHil').value;"
          "const fd=+document.getElementById('sFade').value/100;"
          "const cf=259*(co+255)/(255*(259-co)),sm=1+sa/100;"
          "for(let i=0;i<px.length;i+=4){"
            "let r=px[i],g=px[i+1],b=px[i+2];"
            "r+=br;g+=br;b+=br;"
            "r=cf*(r-128)+128;g=cf*(g-128)+128;b=cf*(b-128)+128;"
            "const gr=0.299*r+0.587*g+0.114*b;"
            "r=gr+(r-gr)*sm;g=gr+(g-gr)*sm;b=gr+(b-gr)*sm;"
            "if(te!==0){r+=te*0.8;b-=te*0.8;}"
            "if(sh!==0){const sl=Math.max(0,1-(0.299*r+0.587*g+0.114*b)/128);const a=sh*0.6*sl;r+=a;g+=a;b+=a;}"
            "if(hi!==0){const hl=Math.max(0,(0.299*r+0.587*g+0.114*b-128)/128);const a=hi*0.6*hl;r+=a;g+=a;b+=a;}"
            "if(fd>0){r=r*(1-fd*0.35)+fd*55;g=g*(1-fd*0.35)+fd*52;b=b*(1-fd*0.35)+fd*48;}"
            "if(filt==='gray'){const fl=0.299*r+0.587*g+0.114*b;r=g=b=fl;}"
            "else if(filt==='sepia'){"
              "const sr=r*0.393+g*0.769+b*0.189,sg=r*0.349+g*0.686+b*0.168,sb=r*0.272+g*0.534+b*0.131;"
              "r=sr;g=sg;b=sb;"
            "}else if(filt==='invert'){r=255-r;g=255-g;b=255-b;}"
            "else if(filt==='noir'){const nl=0.299*r+0.587*g+0.114*b;r=g=b=(nl-128)*1.5+128;}"
            "px[i]=clamp(r);px[i+1]=clamp(g);px[i+2]=clamp(b);"
          "}"
          // rotation + flip
          "const ptmp=document.createElement('canvas');"
          "ptmp.width=origW;ptmp.height=origH;"
          "ptmp.getContext('2d').putImageData(d,0,0);"
          "const sw=rot%2!==0;"
          "c.width=sw?origH:origW;c.height=sw?origW:origH;"
          "ctx.save();ctx.translate(c.width/2,c.height/2);ctx.rotate(rot*Math.PI/2);"
          "if(flipH)ctx.scale(-1,1);"
          "ctx.drawImage(ptmp,-origW/2,-origH/2);ctx.restore();"
          // vignette
          "const vig=+document.getElementById('sVig').value;"
          "if(vig>0){"
            "const vg=ctx.createRadialGradient(c.width/2,c.height/2,Math.min(c.width,c.height)*0.25,c.width/2,c.height/2,Math.max(c.width,c.height)*0.75);"
            "vg.addColorStop(0,'rgba(0,0,0,0)');vg.addColorStop(1,'rgba(0,0,0,'+vig/100+')');"
            "ctx.fillStyle=vg;ctx.fillRect(0,0,c.width,c.height);"
          "}"
          // chromatic aberration
          "const ca=+document.getElementById('sCA').value;"
          "if(ca>0){"
            "const caid=ctx.getImageData(0,0,c.width,c.height);"
            "const casrc=new Uint8ClampedArray(caid.data);"
            "const CW=c.width,CH=c.height;"
            "for(let y=0;y<CH;y++){for(let x=0;x<CW;x++){"
              "const i=(y*CW+x)*4;"
              "const rx=Math.min(CW-1,x+ca),bx=Math.max(0,x-ca);"
              "caid.data[i]=casrc[(y*CW+rx)*4];"
              "caid.data[i+2]=casrc[(y*CW+bx)*4+2];"
            "}}"
            "ctx.putImageData(caid,0,0);"
          "}"
          // pixel sort
          "if(psOn){"
            "const psid=ctx.getImageData(0,0,c.width,c.height);"
            "const pspx=psid.data;"
            "const PW=c.width,PH=c.height;"
            "const pt0=+document.getElementById('sPsMin').value/100;"
            "const pt1=+document.getElementById('sPsMax').value/100;"
            "function psK(i){"
              "const r=pspx[i]/255,g=pspx[i+1]/255,b=pspx[i+2]/255;"
              "if(psKey==='luma')return 0.299*r+0.587*g+0.114*b;"
              "const mx=Math.max(r,g,b),mn=Math.min(r,g,b),d=mx-mn;"
              "if(psKey==='sat')return mx===0?0:d/mx;"
              "if(d===0)return 0;"
              "let h=mx===r?(g-b)/d:mx===g?(b-r)/d+2:(r-g)/d+4;"
              "return((h%6)+6)%6/6;}"
            "const pLines=psDir==='v'?PW:PH,pLen=psDir==='v'?PH:PW;"
            "for(let li=0;li<pLines;li++){"
              "const gi=psDir==='v'?(p)=>(p*PW+li)*4:(p)=>(li*PW+p)*4;"
              "let ss=-1;"
              "const fl=(end)=>{"
                "if(end-ss<2){ss=-1;return;}"
                "const seg=[];"
                "for(let p=ss;p<end;p++){const i=gi(p);seg.push({k:psK(i),r:pspx[i],g:pspx[i+1],b:pspx[i+2]});}"
                "seg.sort((a,b)=>a.k-b.k);"
                "for(let p=ss;p<end;p++){const i=gi(p),s=seg[p-ss];pspx[i]=s.r;pspx[i+1]=s.g;pspx[i+2]=s.b;}"
                "ss=-1;};"
              "for(let pos=0;pos<=pLen;pos++){"
                "if(pos<pLen){const i=gi(pos),k=psK(i);"
                  "if(k>=pt0&&k<=pt1){if(ss===-1)ss=pos;}else if(ss!==-1)fl(pos);}"
                "else if(ss!==-1)fl(pos);}}"
            "ctx.putImageData(psid,0,0);"
          "}"
          // 8-bit: pixelation + color quantization
          "if(eightOn){"
            "const EW=c.width,EH=c.height;"
            "const bsz=+document.getElementById('sEBsz').value;"
            "const lvl=+document.getElementById('sELvl').value;"
            "const eid=ctx.getImageData(0,0,EW,EH);"
            "const epx=eid.data;"
            "const step=255/(lvl-1);"
            "for(let by=0;by<EH;by+=bsz){for(let bx=0;bx<EW;bx+=bsz){"
              "let sr=0,sg=0,sb=0,cnt=0;"
              "const bx2=Math.min(bx+bsz,EW),by2=Math.min(by+bsz,EH);"
              "for(let y=by;y<by2;y++){for(let x=bx;x<bx2;x++){"
                "const i=(y*EW+x)*4;sr+=epx[i];sg+=epx[i+1];sb+=epx[i+2];cnt++;}}"
              "const qr=Math.round(Math.round(sr/cnt/step)*step);"
              "const qg=Math.round(Math.round(sg/cnt/step)*step);"
              "const qb=Math.round(Math.round(sb/cnt/step)*step);"
              "for(let y=by;y<by2;y++){for(let x=bx;x<bx2;x++){"
                "const i=(y*EW+x)*4;epx[i]=qr;epx[i+1]=qg;epx[i+2]=qb;}}}}"
            "ctx.putImageData(eid,0,0);"
          "}"
          // crop overlay com grade de terços
          "if(cropRatio){"
            "const CW=c.width,CH=c.height,rw=cropRatio.w,rh=cropRatio.h;"
            "let cw,ch;"
            "if(rw/rh>CW/CH){cw=CW;ch=Math.round(CW*rh/rw);}else{ch=CH;cw=Math.round(CH*rw/rh);}"
            "const sc=+document.getElementById('sCrSc').value/100;"
            "cw=Math.max(4,Math.round(cw*sc));ch=Math.max(4,Math.round(ch*sc));"
            "const px2=+document.getElementById('sCrX').value/100;"
            "const py2=+document.getElementById('sCrY').value/100;"
            "const cx=Math.round(px2*(CW-cw)),cy=Math.round(py2*(CH-ch));"
            "const saved=ctx.getImageData(cx,cy,Math.max(1,cw),Math.max(1,ch));"
            "ctx.fillStyle='rgba(0,0,0,0.65)';ctx.fillRect(0,0,CW,CH);"
            "ctx.putImageData(saved,cx,cy);"
            "ctx.strokeStyle='rgba(255,255,255,0.9)';ctx.lineWidth=1;"
            "ctx.strokeRect(cx+0.5,cy+0.5,cw-1,ch-1);"
            "ctx.strokeStyle='rgba(255,255,255,0.25)';ctx.lineWidth=0.5;"
            "for(let ti=1;ti<3;ti++){"
              "ctx.beginPath();ctx.moveTo(cx+cw*ti/3,cy);ctx.lineTo(cx+cw*ti/3,cy+ch);ctx.stroke();"
              "ctx.beginPath();ctx.moveTo(cx,cy+ch*ti/3);ctx.lineTo(cx+cw,cy+ch*ti/3);ctx.stroke();}"
          "}"
        "}"
    );

    // ── JS: wiring e handlers ────────────────────────────────────────────────
    server.sendContent(
        "function wire(s,v){const e=document.getElementById(s);"
          "e.addEventListener('input',()=>{document.getElementById(v).textContent=e.value;render();});}"
        "wire('sBri','vBri');wire('sCon','vCon');wire('sSat','vSat');"
        "wire('sSha','vSha');wire('sHil','vHil');wire('sTemp','vTemp');wire('sFade','vFade');"
        "wire('sVig','vVig');wire('sCA','vCA');"
        "wire('sPsMin','vPsMin');wire('sPsMax','vPsMax');"
        "wire('sEBsz','vEBsz');wire('sELvl','vELvl');"
        "wire('sCrSc','vCrSc');wire('sCrX','vCrX');wire('sCrY','vCrY');"

        // pixel sort state + handlers
        "let psOn=false,psDir='h',psKey='luma';"
        "function setPsDir(d){"
          "psDir=d;"
          "document.getElementById('bPsH').classList.toggle('on',d==='h');"
          "document.getElementById('bPsV').classList.toggle('on',d==='v');"
          "if(psOn)render();}"
        "function setPsKey(k){"
          "psKey=k;"
          "['bPsLuma','bPsHue','bPsSat'].forEach(id=>document.getElementById(id).classList.remove('on'));"
          "const m={luma:'bPsLuma',hue:'bPsHue',sat:'bPsSat'};"
          "document.getElementById(m[k]).classList.add('on');"
          "if(psOn)render();}"
        "document.getElementById('bPsH').onclick=()=>setPsDir('h');"
        "document.getElementById('bPsV').onclick=()=>setPsDir('v');"
        "document.getElementById('bPsLuma').onclick=()=>setPsKey('luma');"
        "document.getElementById('bPsHue').onclick=()=>setPsKey('hue');"
        "document.getElementById('bPsSat').onclick=()=>setPsKey('sat');"
        "document.getElementById('bPxSort').onclick=()=>{"
          "psOn=!psOn;"
          "const b=document.getElementById('bPxSort');"
          "b.classList.toggle('on',psOn);"
          "b.innerHTML=psOn?'&#8801; sort: on':'&#8801; sort: off';"
          "render();};"

        "function setFilt(f){"
          "filt=(filt===f)?null:f;"
          "['bGray','bSepia','bInvert','bNoir'].forEach(id=>document.getElementById(id).classList.remove('on'));"
          "const m={gray:'bGray',sepia:'bSepia',invert:'bInvert',noir:'bNoir'};"
          "if(filt)document.getElementById(m[filt]).classList.add('on');render();}"
        "document.getElementById('bGray').onclick=()=>setFilt('gray');"
        "document.getElementById('bSepia').onclick=()=>setFilt('sepia');"
        "document.getElementById('bInvert').onclick=()=>setFilt('invert');"
        "document.getElementById('bNoir').onclick=()=>setFilt('noir');"

        "document.getElementById('bRotL').onclick=()=>{rot=(rot+3)%4;render();};"
        "document.getElementById('bRotR').onclick=()=>{rot=(rot+1)%4;render();};"
        "document.getElementById('bFlip').onclick=()=>{"
          "flipH=!flipH;document.getElementById('bFlip').classList.toggle('on',flipH);render();};"

        "document.getElementById('bEight').onclick=()=>{"
          "eightOn=!eightOn;"
          "const b=document.getElementById('bEight');"
          "b.classList.toggle('on',eightOn);"
          "b.innerHTML=eightOn?'&#9632; 8bit: on':'&#9632; 8bit: off';"
          "render();};"

        "function setRatio(btn,r){"
          "cropRatio=r;"
          "['bCrOrig','bCr11','bCr45','bCr169','bCr916','bCr32'].forEach(id=>document.getElementById(id).classList.remove('on'));"
          "document.getElementById(btn).classList.add('on');"
          "render();}"
        "document.getElementById('bCrOrig').onclick=()=>setRatio('bCrOrig',null);"
        "document.getElementById('bCr11').onclick=()=>setRatio('bCr11',{w:1,h:1});"
        "document.getElementById('bCr45').onclick=()=>setRatio('bCr45',{w:4,h:5});"
        "document.getElementById('bCr169').onclick=()=>setRatio('bCr169',{w:16,h:9});"
        "document.getElementById('bCr916').onclick=()=>setRatio('bCr916',{w:9,h:16});"
        "document.getElementById('bCr32').onclick=()=>setRatio('bCr32',{w:3,h:2});"
        "document.getElementById('bCrApply').onclick=()=>{"
          "if(!cropRatio)return;"
          "const CW=c.width,CH=c.height,rw=cropRatio.w,rh=cropRatio.h;"
          "let cw,ch;"
          "if(rw/rh>CW/CH){cw=CW;ch=Math.round(CW*rh/rw);}else{ch=CH;cw=Math.round(CH*rw/rh);}"
          "const sc=+document.getElementById('sCrSc').value/100;"
          "cw=Math.max(4,Math.round(cw*sc));ch=Math.max(4,Math.round(ch*sc));"
          "const px2=+document.getElementById('sCrX').value/100;"
          "const py2=+document.getElementById('sCrY').value/100;"
          "const cx=Math.round(px2*(CW-cw)),cy=Math.round(py2*(CH-ch));"
          "cropRatio=null;render();"
          "const cd=ctx.getImageData(cx,cy,Math.max(1,cw),Math.max(1,ch));"
          "c.width=cw;c.height=ch;ctx.putImageData(cd,0,0);"
          "const t=document.createElement('canvas');t.width=cw;t.height=ch;"
          "t.getContext('2d').drawImage(c,0,0);"
          "orig=t.getContext('2d').getImageData(0,0,cw,ch);"
          "origW=cw;origH=ch;rot=0;flipH=false;"
          "document.getElementById('status').textContent=cw+'x'+ch+' — cropped';"
          "document.getElementById('sCrSc').value=100;document.getElementById('vCrSc').textContent=100;"
          "document.getElementById('sCrX').value=50;document.getElementById('vCrX').textContent=50;"
          "document.getElementById('sCrY').value=50;document.getElementById('vCrY').textContent=50;"
          "setRatio('bCrOrig',null);};"

        "document.getElementById('bReset').onclick=()=>{"
          "['sBri','sCon','sSat','sSha','sHil','sTemp','sFade','sVig','sCA'].forEach(s=>document.getElementById(s).value=0);"
          "['vBri','vCon','vSat','vSha','vHil','vTemp','vFade','vVig','vCA'].forEach(v=>document.getElementById(v).textContent=0);"
          "['sPsMin','sPsMax'].forEach(s=>document.getElementById(s).value=s==='sPsMin'?20:80);"
          "document.getElementById('vPsMin').textContent=20;document.getElementById('vPsMax').textContent=80;"
          "filt=null;rot=0;flipH=false;psOn=false;psDir='h';psKey='luma';eightOn=false;cropRatio=null;"
          "document.getElementById('bFlip').classList.remove('on');"
          "['bGray','bSepia','bInvert','bNoir'].forEach(id=>document.getElementById(id).classList.remove('on'));"
          "setRatio('bCrOrig',null);"
          "document.getElementById('sCrSc').value=100;document.getElementById('vCrSc').textContent=100;"
          "document.getElementById('sCrX').value=50;document.getElementById('vCrX').textContent=50;"
          "document.getElementById('sCrY').value=50;document.getElementById('vCrY').textContent=50;"
          "document.getElementById('bPxSort').classList.remove('on');"
          "document.getElementById('bPxSort').innerHTML='&#8801; sort: off';"
          "document.getElementById('bEight').classList.remove('on');"
          "document.getElementById('bEight').innerHTML='&#9632; 8bit: off';"
          "document.getElementById('sEBsz').value=8;document.getElementById('vEBsz').textContent=8;"
          "document.getElementById('sELvl').value=4;document.getElementById('vELvl').textContent=4;"
          "document.getElementById('bPsH').classList.add('on');"
          "document.getElementById('bPsV').classList.remove('on');"
          "document.getElementById('bPsLuma').classList.add('on');"
          "['bPsHue','bPsSat'].forEach(id=>document.getElementById(id).classList.remove('on'));"
          "render();};"
    );

    server.sendContent(String(
        "document.getElementById('bSave').onclick=()=>{"
          "const b=document.getElementById('bSave');"
          "b.classList.add('on');b.textContent='saving...';"
          "setTimeout(()=>{"
            "const a=document.createElement('a');"
            "a.download='") + dlname + "';"
            "a.href=c.toDataURL('image/jpeg',0.92);a.click();"
            "b.classList.remove('on');b.textContent='\\u2195 save';"
          "},50);};"
        "</script></body></html>"
    );
}

void handleRoot() {
    String html =
        "<html><head><meta name='viewport' content='width=device-width'>"
        "<style>body{background:#000;color:#0f0;font-family:monospace;text-align:center}"
        "a{color:#0f0}</style></head><body>"
        "<h2>CYBER SHOT</h2>"
        "<p><a href='/galeria'>SD gallery</a></p>";
    if (photoReady)
        html += "<p><a href='/foto'>last photo (RAM)</a>"
                " &nbsp; <a href='/editor?ram=1'>[edit]</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleFoto() {
    if (!photoReady) { server.send(404, "text/plain", "no photo"); return; }
    server.sendHeader("Content-Disposition", "inline; filename=cybershot.jpg");
    server.send_P(200, "image/jpeg", (const char*)photoBuf, photoLen);
}

void handleGallery() {
    // Coleta e ordena arquivos mais recente primeiro
    std::vector<String> files;
    if (sdOK) {
        File root = SD_MMC.open("/");
        File f = root.openNextFile();
        while (f) {
            String name = f.name();
            if (name.endsWith(".JPG") || name.endsWith(".jpg"))
                files.push_back(name);
            f = root.openNextFile();
        }
        std::sort(files.begin(), files.end(), [](const String& a, const String& b){ return a > b; });
    }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    server.sendContent(
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta charset='utf-8'><title>gallery</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{background:#0a0a0a;color:#ccc;font-family:monospace;padding:10px}"
        "h2{text-align:center;color:#0f0;font-size:15px;margin-bottom:4px}"
        ".info{text-align:center;font-size:10px;color:#444;margin-bottom:10px}"
        ".nav a{color:#0f0;font-size:12px;text-decoration:none}"
        ".nav{margin-bottom:10px}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}"
        ".card{background:#161616;border-radius:5px;overflow:hidden;border:1px solid #222}"
        ".card.ram{grid-column:1/-1}"
        ".thumb{width:100%;height:110px;object-fit:cover;display:block;cursor:pointer}"
        ".thumb:active{opacity:.7}"
        ".card.ram .thumb{height:160px}"
        ".name{font-size:9px;color:#555;padding:3px 6px 2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
        ".acts{display:flex;gap:3px;padding:4px 6px 6px}"
        ".acts a{flex:1;text-align:center;padding:5px 2px;font-size:10px;border-radius:3px;"
                "text-decoration:none;border:1px solid;font-family:monospace;}"
        ".ae{color:#0f0;border-color:#0a3a0a;background:#0d1a0d}"
        ".ab{color:#08f;border-color:#0a1a3a;background:#0d0f1a}"
        ".ad{color:#f44;border-color:#3a0a0a;background:#1a0d0d;cursor:pointer}"
        ".empty{grid-column:1/-1;text-align:center;color:#333;font-size:12px;padding:30px 0}"
        "</style></head><body>"
    );

    // header
    char hdr[60];
    snprintf(hdr, sizeof(hdr), "<h2>GALLERY</h2><div class='info'>%d photos on SD</div>", (int)files.size());
    server.sendContent(hdr);
    server.sendContent("<div class='nav'><a href='/'>&#8592; home</a></div><div class='grid'>");

    // última foto em RAM — card full-width no topo
    if (photoReady) {
        server.sendContent(
            "<div class='card ram'>"
            "<img class='thumb' src='/foto' loading='lazy' onclick=\"window.open('/foto')\">"
            "<div class='name'>&#9679; last photo (RAM)</div>"
            "<div class='acts'>"
            "<a class='ae' href='/editor?ram=1'>edit</a>"
            "<a class='ab' href='/foto' download>save</a>"
            "</div></div>"
        );
    }

    if (files.empty()) {
        server.sendContent("<div class='empty'>SD empty</div>");
    } else {
        for (auto& name : files) {
            server.sendContent(
                "<div class='card' id='c_" + name + "'>"
                "<img class='thumb' src='/sd/" + name + "' loading='lazy'"
                " onclick=\"window.open('/sd/" + name + "')\">"
                "<div class='name'>" + name + "</div>"
                "<div class='acts'>"
                "<a class='ae' href='/editor?file=" + name + "'>edit</a>"
                "<a class='ab' href='/sd/" + name + "' download>save</a>"
                "<a class='ad' onclick=\"delFoto('" + name + "');return false\">&#10005;</a>"
                "</div></div>"
            );
        }
    }

    server.sendContent(
        "</div>"
        "<script>"
        "function delFoto(n){"
          "if(!confirm('Delete '+n+'?'))return;"
          "fetch('/delete?file='+n).then(r=>{"
            "if(r.ok){const c=document.getElementById('c_'+n);"
              "if(c)c.remove();}"
            "else alert('delete error');"
          "});}"
        "</script></body></html>"
    );
}

void handleDelete() {
    String file = server.arg("file");
    if (!file.length()) { server.send(400, "text/plain", "no file"); return; }
    if (!sdOK) { server.send(503, "text/plain", "no sd"); return; }
    String path = "/" + file;
    if (!SD_MMC.exists(path)) { server.send(404, "text/plain", "not found"); return; }
    SD_MMC.remove(path);
    server.send(200, "text/plain", "ok");
}

void handleSDFile() {
    String uri = server.uri();
    if (!uri.startsWith("/sd/")) { server.send(404, "text/plain", "not found"); return; }
    String path = "/" + uri.substring(4);
    if (!sdOK || !SD_MMC.exists(path)) { server.send(404, "text/plain", "file not found"); return; }
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) { server.send(500, "text/plain", "open error"); return; }
    server.sendHeader("Cache-Control", "max-age=86400, public");
    server.streamFile(f, "image/jpeg");
    f.close();
}

// ─── Boot intro ──────────────────────────────────────────────────────────────

static void typePrint(const char* s, int x, int y, uint8_t sz, uint16_t col, int ms) {
    tft.setTextSize(sz);
    tft.setTextColor(col);
    for (int i = 0; s[i]; i++) {
        tft.setCursor(x + i * 6 * sz, y);
        tft.print(s[i]);
        if (ms > 0) delay(ms);
    }
}

// Layout CBLNDR: textSize 6 (36×48px/char), 3 chars por linha, centrado
//   Linha 1 — CBL: x=26, y=4   (3×36=108px, margem 26px cada lado)
//   Linha 2 — NDR: x=26, y=56  (gap 4px após y=52)
//   Handles :      y=111 / y=119  (após separador em y=106)
//
// Scramble por coluna: C+N travam juntos, depois B+D, depois L+R
//   → 3 pulsos de LED, colunas "solidificando" da esquerda pra direita

static void bootIntro() {
    const uint16_t gc = 0x07E0;   // verde brilhante (scramble / cantos)
    const uint16_t gd = 0x03C0;   // verde escuro    (separador / handles)

    tft.fillScreen(ST77XX_BLACK);

    // ── cantos viewfinder crescem antes do scramble ───────────────────────────
    for (int m = 1; m <= 10; m++) {
        tft.drawPixel(m - 1,   0,       gc);
        tft.drawPixel(0,       m - 1,   gc);
        tft.drawPixel(160 - m, 0,       gc);
        tft.drawPixel(159,     m - 1,   gc);
        tft.drawPixel(m - 1,   127,     gc);
        tft.drawPixel(0,       128 - m, gc);
        tft.drawPixel(160 - m, 127,     gc);
        tft.drawPixel(159,     128 - m, gc);
        delay(14);
    }
    delay(80);

    // ── scramble CBLNDR ───────────────────────────────────────────────────────
    // Linha 1 — CBL (y=4)  |  Linha 2 — NDR (y=56)
    // Colunas travam em par: C+N (step 0), B+D (step 4), L+R (step 8)
    const char*  target = "CBLNDR";
    const int8_t px[6]  = { 26, 62, 98, 26, 62, 98 };
    const int8_t py[6]  = {  4,  4,  4, 56, 56, 56 };

    tft.setTextSize(6);
    for (int step = 0; step < 15; step++) {
        bool flash = false;
        // ~25% de chance de re-scramble em char já travado → instabilidade
        int reglitch = (step >= 4 && random(4) == 0) ? random(6) : -1;

        for (int i = 0; i < 6; i++) {
            tft.fillRect(px[i], py[i], 36, 48, ST77XX_BLACK);
            char c;
            uint16_t col;
            bool settled = (step >= (i % 3) * 4) && (i != reglitch);
            if (settled) {
                c = target[i]; col = ST77XX_WHITE;
                if (step == (i % 3) * 4) flash = true;
            } else {
                c = (char)('A' + random(26)); col = gc;
            }
            tft.setTextColor(col);
            tft.setCursor(px[i], py[i]);
            tft.print(c);
        }
        if (flash) digitalWrite(LED_FLASH, HIGH);
        delay(62);
        if (flash) digitalWrite(LED_FLASH, LOW);
    }

    // ── glitch agressivo: 9 bursts de 1-3 chars simultâneos ──────────────────
    for (int g = 0; g < 9; g++) {
        int n = 1 + random(3);
        int pos[3] = { random(6), random(6), random(6) };
        if (n > 1 && pos[1] == pos[0]) pos[1] = (pos[0] + 1) % 6;
        if (n > 2 && (pos[2] == pos[0] || pos[2] == pos[1])) pos[2] = (pos[1] + 1) % 6;

        bool ledOn = (random(3) == 0);
        tft.setTextSize(6);
        for (int k = 0; k < n; k++) {
            tft.fillRect(px[pos[k]], py[pos[k]], 36, 48, ST77XX_BLACK);
            tft.setTextColor(gc);
            tft.setCursor(px[pos[k]], py[pos[k]]);
            tft.print((char)('A' + random(26)));
        }
        if (ledOn) digitalWrite(LED_FLASH, HIGH);
        delay(14 + random(20));
        if (ledOn) digitalWrite(LED_FLASH, LOW);

        for (int k = 0; k < n; k++) {
            tft.fillRect(px[pos[k]], py[pos[k]], 36, 48, ST77XX_BLACK);
            tft.setTextColor(ST77XX_WHITE);
            tft.setCursor(px[pos[k]], py[pos[k]]);
            tft.print(target[pos[k]]);
        }
        delay(28 + random(38));
    }

    // finale: tudo scramble de uma vez → trava coluna por coluna com LED
    tft.setTextSize(6);
    for (int i = 0; i < 6; i++) {
        tft.fillRect(px[i], py[i], 36, 48, ST77XX_BLACK);
        tft.setTextColor(gc);
        tft.setCursor(px[i], py[i]);
        tft.print((char)('A' + random(26)));
    }
    digitalWrite(LED_FLASH, HIGH);
    delay(100);
    digitalWrite(LED_FLASH, LOW);
    for (int i = 0; i < 6; i++) {
        tft.fillRect(px[i], py[i], 36, 48, ST77XX_BLACK);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(px[i], py[i]);
        tft.print(target[i]);
        delay(20);
    }
    delay(140);

    // ── separador e handles ───────────────────────────────────────────────────
    for (int x = 26; x <= 134; x += 4) {
        tft.drawFastHLine(26, 106, x - 26, gd);
        delay(6);
    }

    // @cebolander: 11 chars × 6px = 66px → x=(160-66)/2 = 47
    typePrint("@cebolander",    47, 111, 1, ST77XX_WHITE, 32);
    // @lixofuturista: 14 chars × 6px = 84px → x=(160-84)/2 = 38
    typePrint("@lixofuturista", 38, 119, 1, ST77XX_WHITE, 22);

    delay(2000);
}

// ─── Setup ───────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    pinMode(BTN_PIN,   INPUT_PULLUP);
    pinMode(LED_FLASH, OUTPUT);
    digitalWrite(LED_FLASH, LOW);
    pinMode(JOY_SW,  INPUT_PULLUP);

    tftSPI.begin(TFT_SCK, -1, TFT_SDA, -1);
    tft.initR(INITR_BLACKTAB);
    tft.setSPISpeed(40000000);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    photoBuf = nullptr;

    // ── Fases 1 e 2 ──────────────────────────────────────────────────────────
    bootIntro();

    // ── Fase 3 — CYBERSHOT DIY + system checks ───────────────────────────────
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(0x07E0);
    tft.setCursor(8, 6);
    tft.print("CYBERSHOT DIY");
    tft.drawFastHLine(0, 17, 160, 0x03E0);

    // SD
    tft.setTextColor(0x7BEF);
    tft.setCursor(8, 26); tft.print("SD   ...");
    SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
    sdOK = SD_MMC.begin("/sdcard", true);
    tft.fillRect(0, 26, 160, 8, ST77XX_BLACK);
    tft.setCursor(8, 26);
    if (sdOK) {
        tft.setTextColor(ST77XX_GREEN);
        tft.printf("SD   OK  %lluMB", SD_MMC.totalBytes() / (1024 * 1024));
        File root = SD_MMC.open("/");
        File f = root.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                const char* name = f.name();
                int n = 0;
                if (sscanf(name, "/PHOTO_%d.JPG", &n) == 1 || sscanf(name, "PHOTO_%d.JPG", &n) == 1)
                    if (n > photoCount) photoCount = n;
            }
            f = root.openNextFile();
        }
        root.close();
    } else {
        tft.setTextColor(ST77XX_YELLOW);
        tft.print("SD   no card");
    }

    // Camera
    tft.setTextColor(0x7BEF);
    tft.setCursor(8, 38); tft.print("CAM  ...");
    if (!initCamera(PIXFORMAT_RGB565, FRAMESIZE_QQVGA, 12, 2)) {
        tft.fillRect(0, 38, 160, 8, ST77XX_BLACK);
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(8, 38); tft.print("CAM  FAIL");
        while (true) delay(1000);
    }
    tft.fillRect(0, 38, 160, 8, ST77XX_BLACK);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(8, 38); tft.print("CAM  OK");

    // WiFi (desenha em y=50)
    setupWiFi();
    delay(800);
    tft.fillScreen(ST77XX_BLACK);
}

// ─── Menu ────────────────────────────────────────────────────────────────────

// Countdown do timer: pisca LED em ritmo crescente, exibe contagem no TFT.
void runCountdown() {
    uint16_t bright  = vfPalettes[vfColorIdx][3];
    uint16_t accent  = vfPalettes[vfColorIdx][2];
    unsigned long total   = (unsigned long)timerSecs * 1000;
    unsigned long endTime = millis() + total;
    int lastSec = -1;

    while (millis() < endTime) {
        unsigned long remaining = endTime - millis();

        // Atualiza display a cada segundo
        int sec = (int)((remaining + 999) / 1000);
        if (sec != lastSec) {
            lastSec = sec;
            tft.fillScreen(ST77XX_BLACK);
            tft.setTextColor(accent);
            tft.setTextSize(1);
            tft.setCursor(60, 8);
            tft.print("TIMER");
            tft.setTextColor(bright);
            tft.setTextSize(6);
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", sec);
            int tw = (int)strlen(buf) * 36;
            tft.setCursor((160 - tw) / 2, 38);
            tft.print(buf);
        }

        // Período de pisca: 800ms no início → 100ms no final
        unsigned long period = 100 + (700UL * remaining / total);
        unsigned long half   = period / 2;

        digitalWrite(LED_FLASH, HIGH);
        delay(half);
        if (millis() >= endTime) break;
        digitalWrite(LED_FLASH, LOW);
        delay(half);
    }

    digitalWrite(LED_FLASH, LOW);
}

void wifiDisconnect() {
    dnsServer.stop();
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    wifiOK    = false;
    wifiAP    = false;
    wifiSetup = false;
}

void switchToDirectAP() {
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CYBERSHOT");   // sem senha — acesso direto
    wifiOK    = true;
    wifiAP    = true;
    wifiSetup = false;
    delay(200);
    setupWebServer();
}

// ─── Submenu WiFi ─────────────────────────────────────────────────────────────

void drawWiFiMenu() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);

    tft.setTextColor(0x07C0);
    tft.setCursor(44, 4);
    tft.print("-- WIFI --");
    tft.drawFastHLine(0, 14, 160, 0x0260);

    // Status
    tft.setCursor(8, 17);
    if (wifiSetup) {
        tft.setTextColor(ST77XX_YELLOW);
        tft.print("Setup portal active");
        tft.setCursor(8, 26); tft.setTextColor(0x7BEF);
        tft.print("  192.168.4.1");
    } else if (wifiOK && !wifiAP) {
        tft.setTextColor(ST77XX_GREEN);
        tft.print("Connected (STA)");
        tft.setCursor(8, 26); tft.setTextColor(0x07E0);
        tft.printf("  %s", WiFi.localIP().toString().c_str());
    } else if (wifiOK && wifiAP) {
        tft.setTextColor(ST77XX_CYAN);
        tft.print("AP mode (CYBERSHOT)");
        tft.setCursor(8, 26); tft.setTextColor(0x07FF);
        tft.printf("  %s", WiFi.softAPIP().toString().c_str());
    } else {
        tft.setTextColor(0x7BEF);
        tft.print("Offline");
        String ssid, pass;
        if (loadWiFiCreds(ssid, pass)) {
            tft.setCursor(8, 26); tft.setTextColor(0x4208);
            tft.printf("  saved: %s", ssid.c_str());
        }
    }

    tft.drawFastHLine(0, 36, 160, 0x0260);

    const char* items[] = { "CONFIGURE", "CONNECT STA", "DIRECT AP", "DISCONNECT", "BACK" };
    for (int i = 0; i < 5; i++) {
        int iy = 39 + i * 16;
        bool sel = (i == wifiMenuSel);
        tft.fillRect(0, iy, 160, 14, sel ? 0x0260 : ST77XX_BLACK);
        tft.setTextColor(sel ? ST77XX_BLACK : ST77XX_WHITE);
        tft.setCursor(8, iy + 3);
        tft.print(items[i]);
    }

    tft.setTextColor(0x2965);
    tft.setCursor(4, 120);
    tft.print("[BTN]=OK  [HOLD]=back");
}

void connectSTA() {
    String ssid, pass;
    if (!loadWiFiCreds(ssid, pass)) {
        tft.fillRect(0, 17, 160, 18, ST77XX_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_YELLOW);
        tft.setCursor(8, 17); tft.print("No saved network.");
        tft.setCursor(8, 26); tft.print("Use CONFIGURE first.");
        delay(2500);
        drawWiFiMenu();
        return;
    }
    wifiDisconnect();
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(8, 50); tft.print("Connecting to:");
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(8, 62); tft.print(ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long t = millis();
    while (millis() - t < 10000 && WiFi.status() != WL_CONNECTED) delay(200);

    if (WiFi.status() == WL_CONNECTED) {
        wifiOK = true; wifiAP = false; wifiSetup = false;
        setupWebServer();
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(8, 76); tft.printf("IP: %s", WiFi.localIP().toString().c_str());
        delay(1500);
    } else {
        WiFi.disconnect(true);
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(8, 76); tft.print("Failed to connect.");
        delay(2000);
    }
    drawWiFiMenu();
}

const char* MENU_ITEMS[] = { "VF COLOR", "LONG EXP", "TIMER", "EFFECTS", "WIFI", "FORMAT SD CARD", "EXIT" };
const int   MENU_N       = 7;

void drawMenu() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(0x07C0);
    tft.setCursor(50, 5);
    tft.print("-- MENU --");
    tft.drawFastHLine(0, 15, 160, 0x0260);

    // 7 itens: y = 17 + i*15, height=12 → último termina em 122, hint em 120 (sobrepõe ok)
    for (int i = 0; i < MENU_N; i++) {
        int iy = 17 + i * 15;
        if (i == menuSel) {
            tft.fillRect(0, iy, 160, 12, 0x0260);
            tft.setTextColor(ST77XX_BLACK);
        } else {
            tft.fillRect(0, iy, 160, 12, ST77XX_BLACK);
            tft.setTextColor(ST77XX_WHITE);
        }
        tft.setCursor(8, iy + 2);
        if (i == 1) {
            char lb[22];
            if (leSeconds == 0) snprintf(lb, sizeof(lb), "LONG EXP: OFF");
            else                snprintf(lb, sizeof(lb), "LONG EXP: %dS", leSeconds);
            tft.print(lb);
        } else if (i == 2) {
            const char* ts = timerSecs == 0 ? "OFF" :
                             timerSecs == 3 ? "3S"  :
                             timerSecs == 5 ? "5S"  : "10S";
            char lb[22];
            snprintf(lb, sizeof(lb), "TIMER: %s", ts);
            tft.print(lb);
        } else if (i == 3) {
            int n = (int)fxDQT + (int)fxScan + (int)fxChroma + (int)fxZigzag + (int)fxDHT;
            char lb[22];
            if (n > 0) snprintf(lb, sizeof(lb), "EFFECTS [%d]", n);
            else       snprintf(lb, sizeof(lb), "EFFECTS");
            tft.print(lb);
        } else if (i == 4) {
            char lb[22];
            if (wifiSetup || !wifiOK) snprintf(lb, sizeof(lb), "WIFI: setup");
            else if (wifiAP)          snprintf(lb, sizeof(lb), "WIFI: AP");
            else                      snprintf(lb, sizeof(lb), "WIFI: %s", WiFi.localIP().toString().c_str());
            tft.print(lb);
        } else {
            tft.print(MENU_ITEMS[i]);
        }
    }

    tft.setTextColor(0x02A0);
    tft.setCursor(4, 120);
    tft.print("[BTN]=OK  [HOLD]=exit");
}

void drawConfirmFormat() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(1);
    tft.setCursor(8, 8);
    tft.print("FORMAT SD CARD");
    tft.drawFastHLine(0, 18, 160, ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(8, 26);
    tft.print("Erases ALL on SD.");
    tft.setCursor(8, 38);
    tft.print("Are you sure?");

    if (confirmSel == 0) {
        tft.fillRoundRect(8,  62, 60, 20, 4, ST77XX_RED);
        tft.setTextColor(ST77XX_WHITE);
    } else {
        tft.drawRoundRect(8,  62, 60, 20, 4, 0x0260);
        tft.setTextColor(0x0260);
    }
    tft.setCursor(26, 68); tft.print("YES");

    if (confirmSel == 1) {
        tft.fillRoundRect(92, 62, 60, 20, 4, 0x0260);
        tft.setTextColor(ST77XX_BLACK);
    } else {
        tft.drawRoundRect(92, 62, 60, 20, 4, 0x0260);
        tft.setTextColor(0x0260);
    }
    tft.setCursor(110, 68); tft.print("NO");

    tft.setTextColor(0x02A0);
    tft.setCursor(4, 120);
    tft.print("[JOY]=sel  [BTN]=OK");
}

bool formatSDCard() {
    if (!sdOK) return false;
    File root = SD_MMC.open("/");
    if (!root || !root.isDirectory()) return false;
    File f = root.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String path = f.path();
            f.close();
            SD_MMC.remove(path.c_str());
        } else {
            f.close();
        }
        f = root.openNextFile();
    }
    root.close();
    photoCount = 0;
    photoReady = false;
    return true;
}

// ─── Seletor de efeitos ──────────────────────────────────────────────────────

const char* FX_NAMES[] = { "DQT EROSION", "SCAN SWAP", "CHROMA AMP", "ZIGZAG PERM", "DHT REMAP" };
const int   FX_N       = 5;

void drawEffectsMenu() {
    bool* flags[FX_N] = { &fxDQT, &fxScan, &fxChroma, &fxZigzag, &fxDHT };
    uint16_t accent = vfPalettes[vfColorIdx][2];
    uint16_t bright = vfPalettes[vfColorIdx][3];

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);

    tft.setTextColor(bright);
    tft.setCursor(30, 4);
    tft.print("-- EFFECTS --");
    tft.drawFastHLine(0, 14, 160, accent);

    const int ITEM_Y[5] = { 18, 36, 54, 72, 90 };

    for (int i = 0; i < FX_N; i++) {
        bool active = *flags[i];
        bool sel    = (i == effectsSel);
        int  y      = ITEM_Y[i];
        tft.fillRect(0, y, 160, 16, sel ? accent : ST77XX_BLACK);
        tft.setTextColor(sel ? ST77XX_BLACK : (active ? bright : 0x7BEF));
        tft.drawRect(2, y + 4, 8, 8, sel ? ST77XX_BLACK : accent);
        if (active) tft.fillRect(4, y + 6, 4, 4, sel ? ST77XX_BLACK : bright);
        tft.setCursor(14, y + 4);
        tft.print(FX_NAMES[i]);
    }

    tft.setTextColor(0x02A0);
    tft.setCursor(4, 120);
    tft.print("[BTN]=toggle [SW]=exit");
}

// ─── Seletor de cor do viewfinder ────────────────────────────────────────────

void drawVfColorSelect() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(0x07C0);
    tft.setCursor(22, 4);
    tft.print("VF COLOR");
    tft.drawFastHLine(0, 14, 160, 0x0260);

    for (int i = 0; i < 5; i++) {
        bool sel = (i == vfColorIdx);
        uint16_t bright = vfPalettes[i][3];
        uint16_t mid    = vfPalettes[i][2];
        int y = 18 + i * 20;
        if (sel) {
            tft.fillRect(0, y, 160, 18, mid);
            tft.setTextColor(ST77XX_BLACK);
        } else {
            tft.fillRect(0, y, 160, 18, ST77XX_BLACK);
            tft.fillRect(2, y + 3, 12, 12, bright);
            tft.setTextColor(ST77XX_WHITE);
        }
        tft.setCursor(20, y + 5);
        tft.print(VF_COLOR_NAMES[i]);
    }

    tft.setTextColor(0x02A0);
    tft.setCursor(4, 122);
    tft.print("[JOY]=sel  [BTN]=OK");
}

// ─── Botão ───────────────────────────────────────────────────────────────────

void handleButton() {
    static bool          lastBtn     = HIGH;
    static unsigned long pressTime   = 0;
    static bool          longHandled = false;

    bool btn = digitalRead(BTN_PIN);

    if (btn == LOW && lastBtn == HIGH) {
        pressTime   = millis();
        longHandled = false;
    }

    if (btn == LOW && !longHandled && (millis() - pressTime > 800)) {
        longHandled = true;
        if (wifiSetup) {
            wifiDisconnect();
            appState = STATE_VF; vfNeedsClear = true;
        } else if (appState == STATE_WIFI) {
            appState = STATE_MENU; drawMenu();
        } else if (appState == STATE_VF) {
            appState = STATE_MENU;
            menuSel  = 0;
            drawMenu();
        } else {
            appState = STATE_VF; vfNeedsClear = true;
        }
    }

    if (btn == HIGH && lastBtn == LOW && !longHandled && (millis() - pressTime > 30)) {
        if (appState == STATE_VF) {
            if (timerSecs > 0) runCountdown();
            takePhoto();
        } else if (appState == STATE_MENU) {
            if (menuSel == 0) {
                appState = STATE_VF_COLOR;
                drawVfColorSelect();
            } else if (menuSel == 1) {
                if      (leSeconds == 0)  leSeconds = 3;
                else if (leSeconds == 3)  leSeconds = 5;
                else if (leSeconds == 5)  leSeconds = 10;
                else                      leSeconds = 0;
                applyVfExposure();
                drawMenu();
            } else if (menuSel == 2) {
                // cicla timer: 0 → 3 → 5 → 10 → 0
                const int opts[] = {0, 3, 5, 10};
                int ti = 0;
                for (int j = 0; j < 4; j++) if (opts[j] == timerSecs) { ti = j; break; }
                timerSecs = opts[(ti + 1) % 4];
                drawMenu();
            } else if (menuSel == 3) {
                appState = STATE_EFFECTS;
                effectsSel = 0;
                drawEffectsMenu();
            } else if (menuSel == 4) {
                appState = STATE_WIFI;
                wifiMenuSel = 0;
                drawWiFiMenu();
            } else if (menuSel == 5) {
                confirmSel = 1;
                appState   = STATE_CONFIRM;
                drawConfirmFormat();
            } else {
                appState = STATE_VF; vfNeedsClear = true;
            }
        } else if (appState == STATE_WIFI) {
            switch (wifiMenuSel) {
                case 0:  // CONFIGURE → captive portal
                    wifiDisconnect();
                    startWiFiPortal();
                    appState = STATE_VF;
                    break;
                case 1:  // CONNECT STA
                    connectSTA();
                    break;
                case 2:  // DIRECT AP
                    wifiDisconnect();
                    switchToDirectAP();
                    drawWiFiMenu();
                    break;
                case 3:  // DISCONNECT
                    wifiDisconnect();
                    drawWiFiMenu();
                    break;
                case 4:  // BACK
                    appState = STATE_MENU;
                    drawMenu();
                    break;
            }
        } else if (appState == STATE_EFFECTS) {
            bool* flags[FX_N] = { &fxDQT, &fxScan, &fxChroma, &fxZigzag, &fxDHT };
            *flags[effectsSel] = !*flags[effectsSel];
            drawEffectsMenu();
        } else if (appState == STATE_VF_COLOR) {
            appState = STATE_VF; vfNeedsClear = true;
        } else if (appState == STATE_CONFIRM) {
            if (confirmSel == 0) {
                tft.fillScreen(ST77XX_BLACK);
                tft.setTextColor(ST77XX_WHITE);
                tft.setTextSize(1);
                tft.setCursor(30, 58);
                tft.print("Formatting...");
                bool ok = formatSDCard();
                tft.fillScreen(ST77XX_BLACK);
                tft.setCursor(ok ? 45 : 8, 58);
                tft.setTextColor(ok ? ST77XX_GREEN : ST77XX_RED);
                tft.print(ok ? "SD cleared!" : "Format error");
                delay(2000);
                appState = STATE_VF; vfNeedsClear = true;
            } else {
                appState = STATE_MENU;
                drawMenu();
            }
        }
    }

    lastBtn = btn;
}

void handleMenuInput() {
    static unsigned long lastMove = 0;
    if (millis() - lastMove < 220) return;

    if (appState == STATE_MENU) {
        int y = analogRead(JOY_Y);
        if (y < 1000) {
            menuSel = (menuSel - 1 + MENU_N) % MENU_N;
            drawMenu(); lastMove = millis();
        } else if (y > 3000) {
            menuSel = (menuSel + 1) % MENU_N;
            drawMenu(); lastMove = millis();
        }
        if (digitalRead(JOY_SW) == LOW) {
            appState = STATE_VF; vfNeedsClear = true; lastMove = millis();
        }
    } else if (appState == STATE_CONFIRM) {
        int x = analogRead(JOY_X);
        if (x < 1000 && confirmSel != 1) {
            confirmSel = 1; drawConfirmFormat(); lastMove = millis();
        } else if (x > 3000 && confirmSel != 0) {
            confirmSel = 0; drawConfirmFormat(); lastMove = millis();
        }
        if (digitalRead(JOY_SW) == LOW) {
            appState = STATE_MENU; drawMenu(); lastMove = millis();
        }
    } else if (appState == STATE_VF_COLOR) {
        int y = analogRead(JOY_Y);
        if (y < 1000) {
            vfColorIdx = (vfColorIdx - 1 + 5) % 5;
            drawVfColorSelect(); lastMove = millis();
        } else if (y > 3000) {
            vfColorIdx = (vfColorIdx + 1) % 5;
            drawVfColorSelect(); lastMove = millis();
        }
        if (digitalRead(JOY_SW) == LOW) {
            appState = STATE_MENU; drawMenu(); lastMove = millis();
        }
    } else if (appState == STATE_EFFECTS) {
        int y = analogRead(JOY_Y);
        if (y < 1000) {
            effectsSel = (effectsSel - 1 + FX_N) % FX_N;
            drawEffectsMenu(); lastMove = millis();
        } else if (y > 3000) {
            effectsSel = (effectsSel + 1) % FX_N;
            drawEffectsMenu(); lastMove = millis();
        }
        if (digitalRead(JOY_SW) == LOW) {
            appState = STATE_MENU; drawMenu(); lastMove = millis();
        }
    } else if (appState == STATE_WIFI) {
        int y = analogRead(JOY_Y);
        if (y < 1000) {
            wifiMenuSel = (wifiMenuSel - 1 + 5) % 5;
            drawWiFiMenu(); lastMove = millis();
        } else if (y > 3000) {
            wifiMenuSel = (wifiMenuSel + 1) % 5;
            drawWiFiMenu(); lastMove = millis();
        }
        if (digitalRead(JOY_SW) == LOW) {
            appState = STATE_MENU; drawMenu(); lastMove = millis();
        }
    }
}

void handleVfInput() {
    static unsigned long lastMove = 0;
    if (appState != STATE_VF) return;
    if (millis() - lastMove < 300) return;

    int y = analogRead(JOY_Y);
    int newEv = evComp;
    if      (y < 1000 && evComp < 3)  newEv++;
    else if (y > 3000 && evComp > -3) newEv--;
    if (newEv == evComp) return;

    int delta = newEv - evComp;  // +1 = mais brilhante, -1 = mais escuro
    evComp = newEv;
    lastMove = millis();

    // Aplica imediatamente ao sensor usando a DIREÇÃO (delta), não o valor absoluto
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return;
    if (delta > 0) {
        vfAecValue = min(1200, vfAecValue * 2);
        if (vfAecValue >= 1200) vfAgcGain = min(30, vfAgcGain + 5);  // ~1 stop de ganho
    } else {
        if (vfAgcGain >= 5) vfAgcGain = max(0,  vfAgcGain - 5);
        else { vfAgcGain = 0; vfAecValue = max(50, vfAecValue / 2); }
    }
    s->set_aec_value(s, vfAecValue);
    s->set_agc_gain(s, vfAgcGain);
}

// ─── Loop ────────────────────────────────────────────────────────────────────

void loop() {
    if (!wifiAP && !wifiOK && WiFi.status() == WL_CONNECTED) {
        wifiOK = true;
        setupWebServer();
    }
    if (wifiSetup) dnsServer.processNextRequest();
    if (wifiOK || wifiSetup) server.handleClient();

    handleButton();
    handleVfInput();

    if (appState != STATE_VF) {
        handleMenuInput();
        return;
    }

    if (wifiSetup) return;   // portal ativo: não roda VF, mantém tela de instrução

    if (vfNeedsClear) {
        tft.fillScreen(ST77XX_BLACK);
        vfNeedsClear = false;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return;

    if (++vfFrameCnt % 2 == 0) {
        autoExposure(measureLuma(fb->buf, fb->width, fb->height));
    }

    toGreenTones(fb->buf, fb->width, fb->height);

    int vfW = min((int)fb->width,  160);
    int vfH = min((int)fb->height, 128);
    int vfX = (160 - vfW) / 2;
    int vfY = (128 - vfH) / 2;

    tft.startWrite();
    tft.setAddrWindow(vfX, vfY, vfW, vfH);
    tft.writePixels((uint16_t*)fb->buf, vfW * vfH, true, true);
    tft.endWrite();

    esp_camera_fb_return(fb);
    drawViewfinderOverlay();
}
