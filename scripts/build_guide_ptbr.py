# -*- coding: utf-8 -*-
"""Guia de montagem DIY - CYBERSHOT  |  PT-BR  |  redesign v2"""
import os
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import inch, mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    Image, HRFlowable, KeepTogether
)
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from PIL import Image as PILImage

# ── paths ──────────────────────────────────────────────────────────────────
IMG_DIR = (r"C:\Users\USURIO~2\AppData\Local\Temp\claude"
           r"\C--Users-Usu-rio-Documents-PlatformIO-Projects-cybershot-cam"
           r"\f7c60bc2-4569-4700-b03b-7182bd87e4de\scratchpad\pdfimgs")
OUT = (r"C:\Users\Usuário\Documents\PlatformIO\Projects\cybershot-cam"
       r"\DIY_CYBERSHOT_Guia_PT-BR.pdf")

# ── palette ─────────────────────────────────────────────────────────────────
BLACK   = colors.HexColor("#111111")
DGRAY   = colors.HexColor("#444444")
GRAY    = colors.HexColor("#777777")
LGRAY   = colors.HexColor("#f4f4f4")
BORDER  = colors.HexColor("#d8d8d8")
ACCENT  = colors.HexColor("#00a86b")
ACCENT2 = colors.HexColor("#e8f7f1")   # accent tint
WHITE   = colors.white

W, H = A4   # 595.27 x 841.89 pt
LM = RM = 0.78 * inch
TM = 0.75 * inch
BM = 0.82 * inch

# ── styles ──────────────────────────────────────────────────────────────────
def S(name, **kw):
    base = dict(fontName="Helvetica", fontSize=10, leading=14, textColor=BLACK,
                spaceAfter=0, spaceBefore=0)
    base.update(kw)
    return ParagraphStyle(name, **base)

sTitle  = S("T",  fontName="Helvetica-Bold", fontSize=27, leading=31, textColor=BLACK)
sAuthor = S("A",  fontSize=10.5, textColor=GRAY, fontName="Helvetica-Oblique")
sDesc   = S("D",  fontSize=9.5,  textColor=DGRAY, leading=14)
sH1     = S("H1", fontName="Helvetica-Bold", fontSize=14.5, leading=18,
             textColor=BLACK, spaceBefore=6, spaceAfter=8)
sH2     = S("H2", fontName="Helvetica-Bold", fontSize=11.5, leading=15,
             textColor=BLACK, spaceBefore=10, spaceAfter=5)
sBody   = S("Bo", fontSize=9.5, leading=13.5, textColor=BLACK)
sSmall  = S("Sm", fontSize=8.8, leading=12.5, textColor=GRAY)
sNote   = S("No", fontSize=8.8, leading=12.5, textColor=GRAY,
             fontName="Helvetica-Oblique")
sTHead  = S("TH", fontName="Helvetica-Bold", fontSize=9, leading=12, textColor=BLACK)
sTCell  = S("TC", fontSize=9,  leading=12, textColor=BLACK)
sNum    = S("Nu", fontName="Helvetica-Bold", fontSize=9, leading=12,
             textColor=ACCENT)
sComp   = S("Co", fontName="Helvetica-Bold", fontSize=10, leading=13, textColor=BLACK)
sStep   = S("St", fontName="Helvetica-Bold", fontSize=9.5, leading=13, textColor=BLACK)
sStepB  = S("SB", fontSize=9.2, leading=13, textColor=DGRAY)
sTip    = S("Ti", fontName="Helvetica-Bold", fontSize=9.5, leading=13, textColor=BLACK)
sTipB   = S("TB", fontSize=9.2, leading=13, textColor=BLACK)
sCode   = S("Cd", fontName="Courier", fontSize=8.5, leading=12, textColor=DGRAY)

def p(text, style=sBody): return Paragraph(text, style)

# ── helpers ─────────────────────────────────────────────────────────────────
def fitted(path, max_w, max_h):
    im = PILImage.open(path)
    w, h = im.size
    r = min(max_w / w, max_h / h)
    return Image(path, width=w * r, height=h * r)

def img(name, max_w, max_h):
    return fitted(os.path.join(IMG_DIR, name), max_w, max_h)

def rule(color=BORDER, thick=0.6, before=4, after=8):
    return HRFlowable(width="100%", thickness=thick, color=color,
                      spaceBefore=before, spaceAfter=after)

def accent_rule(before=2, after=10):
    return HRFlowable(width="100%", thickness=1.5, color=ACCENT,
                      spaceBefore=before, spaceAfter=after)

def h1(text):
    bar = Table([[p("", S("_", fontSize=1)), p(text, sH1)]],
                colWidths=[5, None])
    bar.setStyle(TableStyle([
        ("BACKGROUND",    (0, 0), (0, 0), ACCENT),
        ("VALIGN",        (0, 0), (-1,-1), "MIDDLE"),
        ("TOPPADDING",    (0, 0), (-1,-1), 0),
        ("BOTTOMPADDING", (0, 0), (-1,-1), 0),
        ("LEFTPADDING",   (0, 0), ( 0, 0), 0),
        ("RIGHTPADDING",  (0, 0), ( 0, 0), 0),
        ("LEFTPADDING",   (1, 0), ( 1, 0), 10),
        ("RIGHTPADDING",  (1, 0), ( 1, 0), 0),
    ]))
    return bar

def h2(text):
    return p(f'<font color="#00a86b">▍</font>  {text}', sH2)

def pinout_table(data, widths):
    t = Table(data, colWidths=widths, hAlign="LEFT")
    t.setStyle(TableStyle([
        ("GRID",          (0, 0), (-1, -1), 0.5, BORDER),
        ("BACKGROUND",    (0, 0), (-1,  0), LGRAY),
        ("ROWBACKGROUNDS",(0, 1), (-1, -1), [WHITE, colors.HexColor("#fafafa")]),
        ("VALIGN",        (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING",   (0, 0), (-1, -1), 9),
        ("RIGHTPADDING",  (0, 0), (-1, -1), 9),
        ("TOPPADDING",    (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    return t

# ── canvas callbacks ─────────────────────────────────────────────────────────
def on_page(canvas, doc):
    canvas.saveState()
    # top accent bar
    canvas.setFillColor(ACCENT)
    canvas.rect(0, H - 5, W, 5, fill=1, stroke=0)
    # footer line
    canvas.setStrokeColor(BORDER)
    canvas.setLineWidth(0.6)
    canvas.line(LM, BM * 0.55, W - RM, BM * 0.55)
    canvas.setFont("Helvetica", 7.5)
    canvas.setFillColor(GRAY)
    canvas.drawString(LM, BM * 0.35, "DIY - CYBERSHOT")
    canvas.drawRightString(W - RM, BM * 0.35, f"p. {doc.page}")
    canvas.restoreState()

# ── story ────────────────────────────────────────────────────────────────────
story = []

# ════════════════════════════════════════════════════════
#  PÁGINA 1 — capa + componentes
# ════════════════════════════════════════════════════════
story.append(Spacer(1, 2))
story.append(p("DIY - CYBERSHOT", sTitle))
story.append(Spacer(1, 3))
story.append(p("por @lixofuturista / @cebolander", sAuthor))
story.append(Spacer(1, 8))
story.append(accent_rule(before=0, after=6))
story.append(p(
    "Guia de montagem da CyberShot Cam — câmera digital DIY com ESP32-S3, "
    "viewfinder ao vivo, longa exposição, efeitos de databending e galeria web.", sDesc))
story.append(Spacer(1, 10))

# ── componentes ───────────────────────────────────────────
story.append(h1("O QUE VOCÊ VAI PRECISAR"))
story.append(Spacer(1, 6))

COMPS = [
    ("Esp32-s3 Cam Ov5640 Wifi Bluetooth 5.0 Ble N16r8", "page1_img0.png"),
    ("Display Lcd Tft 1.8\" 128x160 St7735",              "page1_img1.png"),
    ("Modulo Joystick Analógico 3 Eixos 5v",               "page1_img2.png"),
    ("Botão Push (momentâneo)",                            "page1_img3.png"),
    ("Cartão Micro SD / Qualquer modelo",                  "sdcard.png"),
    ("Led 5mm Alto Brilho 100 Vermelho (opcional)",        "page2_img0.png"),
]

MAX_IMG_W = 1.42 * inch
MAX_IMG_H = 0.82 * inch
PAD_V = 7

rows = []
for i, (name, imgf) in enumerate(COMPS):
    num_cell  = p(f"{i+1:02d}", sNum)
    name_cell = p(name, sComp)
    img_cell  = img(imgf, MAX_IMG_W, MAX_IMG_H)
    rows.append([num_cell, name_cell, img_cell])

# col0=number, col1=name, col2=image  — total must fit content width
comp_t = Table(rows, colWidths=[0.42*inch, 2.98*inch, 1.55*inch], hAlign="LEFT")
bg_pairs = [WHITE if i % 2 == 0 else colors.HexColor("#f8f8f8") for i in range(len(COMPS))]
comp_t.setStyle(TableStyle([
    ("ROWBACKGROUNDS", (0, 0), (-1, -1), bg_pairs),
    ("VALIGN",         (0, 0), (-1, -1), "MIDDLE"),
    ("LINEBELOW",      (0, 0), (-1, -2), 0.4, BORDER),
    ("LINEBELOW",      (0,-1), (-1, -1), 0.8, BORDER),
    ("LINETOP",        (0, 0), (-1,  0), 0.8, BORDER),
    ("TOPPADDING",     (0, 0), (-1, -1), PAD_V),
    ("BOTTOMPADDING",  (0, 0), (-1, -1), PAD_V),
    ("LEFTPADDING",    (0, 0), ( 0,-1), 10),
    ("RIGHTPADDING",   (0, 0), ( 0,-1), 6),
    ("LEFTPADDING",    (1, 0), ( 1,-1), 8),
    ("RIGHTPADDING",   (2, 0), ( 2,-1), 8),
    ("ALIGN",          (2, 0), ( 2,-1), "CENTER"),
]))
story.append(comp_t)

story.append(Spacer(1, 7))
story.append(p(
    "<b>Também vai precisar de:</b>  ferro de solda / estanho / "
    "fios para conexões ou placa de circuito.", sSmall))

from reportlab.platypus import PageBreak

# ════════════════════════════════════════════════════════
#  PÁGINA 2 — pinouts
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("PINOUTS"))
story.append(Spacer(1, 8))

# TFT — KeepTogether evita quebra dentro do bloco
story.append(KeepTogether([
    h2("TFT Display (ST7735)"),
    p("Conectado via SPI por hardware (barramento FSPI). "
      "Mantenha os fios curtos para operação confiável em alta velocidade.", sBody),
    Spacer(1, 5),
    pinout_table([
        [p("TFT Pin",    sTHead), p("Função",       sTHead), p("ESP32-S3 GPIO", sTHead)],
        [p("VCC",        sTCell), p("Alimentação",  sTCell), p("3.3 V",         sTCell)],
        [p("GND",        sTCell), p("Terra",        sTCell), p("GND",           sTCell)],
        [p("SCK",        sTCell), p("Clock SPI",    sTCell), p("GPIO 47",       sTCell)],
        [p("SDA (MOSI)", sTCell), p("Dados SPI",    sTCell), p("GPIO 45",       sTCell)],
        [p("CS",         sTCell), p("Chip Select",  sTCell), p("GPIO 43",       sTCell)],
        [p("DC",         sTCell), p("Data/Command", sTCell), p("GPIO 14",       sTCell)],
        [p("RST",        sTCell), p("Reset",        sTCell), p("GPIO 21",       sTCell)],
    ], [1.4*inch, 2.0*inch, 1.5*inch]),
    p("NOTA: o TFT usa o barramento FSPI. Não compartilhe este barramento com outros dispositivos SPI.", sNote),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Joystick Analógico (KY-023)"),
    Spacer(1, 4),
    pinout_table([
        [p("Pino do Joystick", sTHead), p("ESP32-S3 GPIO", sTHead)],
        [p("VRx (eixo X)",     sTCell), p("GPIO 1  (ADC)", sTCell)],
        [p("VRy (eixo Y)",     sTCell), p("GPIO 2  (ADC)", sTCell)],
        [p("SW (botão)",       sTCell), p("GPIO 42",       sTCell)],
        [p("VCC",              sTCell), p("3.3 V",         sTCell)],
        [p("GND",              sTCell), p("GND",           sTCell)],
    ], [2.5*inch, 2.4*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Botão Push (momentâneo)"),
    Spacer(1, 4),
    pinout_table([
        [p("Pino do Botão", sTHead), p("ESP32-S3 GPIO", sTHead)],
        [p("Terminal A",    sTCell), p("GPIO 41",       sTCell)],
        [p("Terminal B",    sTCell), p("GND",           sTCell)],
    ], [2.5*inch, 2.4*inch]),
    p("NOTA: pull-up interno habilitado, ativo em LOW — não precisa de resistor externo.", sNote),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("LED (indicador do sistema)"),
    Spacer(1, 4),
    pinout_table([
        [p("Terminal do LED", sTHead), p("Conexão",                    sTHead)],
        [p("Anodo",           sTCell), p("resistor 100 Ω  →  GPIO 48", sTCell)],
        [p("Catodo",          sTCell), p("GND",                        sTCell)],
    ], [2.5*inch, 2.4*inch]),
]))

# ════════════════════════════════════════════════════════
#  PÁGINA 3 — software
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("CONFIGURAÇÃO DO SOFTWARE"))
story.append(Spacer(1, 8))

STEPS = [
    ("01", "Instale o PlatformIO",
           "Extensão para VS Code ou CLI — https://platformio.org"),
    ("02", "Clone o repositório",
           "git clone https://github.com/seu-usuario/cybershot-cam"),
    ("03", "Abra no VS Code",
           "Abra a pasta do projeto com a extensão do PlatformIO ativa"),
    ("04", "Conecte a placa",
           "Conecte o ESP32-S3 via USB-C; segure BOOT na primeira gravação"),
    ("05", "Grave o firmware",
           'Clique em "Upload" no PlatformIO, ou: pio run --target upload'),
    ("06", "Prepare o cartão SD",
           "Insira um cartão microSD formatado em FAT32 antes do primeiro boot"),
    ("07", "Configure o WiFi",
           "No dispositivo: MENU  →  WIFI  →  CONFIGURE"),
]
step_rows = []
for num, title, body in STEPS:
    step_rows.append([
        p(num, sNum),
        p(f"<b>{title}</b>", sStep),
        p(body, sStepB),
    ])
step_t = Table(step_rows, colWidths=[0.34*inch, 1.65*inch, None], hAlign="LEFT")
step_t.setStyle(TableStyle([
    ("VALIGN",        (0,0), (-1,-1), "MIDDLE"),
    ("TOPPADDING",    (0,0), (-1,-1), 8),
    ("BOTTOMPADDING", (0,0), (-1,-1), 8),
    ("LEFTPADDING",   (0,0), ( 0,-1), 8),
    ("RIGHTPADDING",  (0,0), ( 0,-1), 4),
    ("LEFTPADDING",   (1,0), ( 1,-1), 4),
    ("LINEBELOW",     (0,0), (-1,-2), 0.4, BORDER),
    ("ROWBACKGROUNDS",(0,0), (-1,-1), [WHITE, colors.HexColor("#f8f8f8")]),
]))
story.append(step_t)
story.append(Spacer(1, 12))

story.append(h2("Dependências"))
story.append(p(
    "Instaladas automaticamente pelo PlatformIO via "
    "<font face='Courier'>platformio.ini</font> — sem instalação manual.", sBody))
story.append(Spacer(1, 5))
for dep in ["adafruit/Adafruit ST7735 and ST7789 Library",
            "adafruit/Adafruit GFX Library",
            "bitbank2/JPEGDEC"]:
    story.append(p(f'<font color="#00a86b">+</font>  {dep}', sSmall))
    story.append(Spacer(1, 2))

# ════════════════════════════════════════════════════════
#  PÁGINA 4 — controles
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("CONTROLES E COMO USAR"))
story.append(Spacer(1, 8))

story.append(KeepTogether([
    h2("Controles de entrada"),
    Spacer(1, 4),
    pinout_table([
        [p("Ação",                         sTHead), p("Resultado",                        sTHead)],
        [p("SEGURAR botão  ( > 800 ms )",  sTCell), p("Abre / fecha o menu principal",    sTCell)],
        [p("PRESSIONAR botão  ( curto )",  sTCell), p("Tira foto a partir do viewfinder", sTCell)],
        [p("JOYSTICK  CIMA / BAIXO",       sTCell), p("Navega pelos itens do menu",       sTCell)],
        [p("JOYSTICK  ESQUERDA / DIREITA", sTCell), p("Confirma / ajusta valores",        sTCell)],
        [p("JOYSTICK PRESS  ( SW )",       sTCell), p("Voltar / cancelar",                sTCell)],
    ], [2.55*inch, 2.35*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Menu principal  ( segure o botão no viewfinder )"),
    Spacer(1, 4),
    pinout_table([
        [p("Item",           sTHead), p("Função",                                                                       sTHead)],
        [p("VF COLOR",       sTCell), p("Paleta de cores do viewfinder  ( verde / vermelho / rosa / branco / ciano )",  sTCell)],
        [p("LONG EXP",       sTCell), p("Longa exposição com stacking de quadros — OFF / 3S / 5S / 10S",               sTCell)],
        [p("TIMER",          sTCell), p("Autotimer antes da captura — OFF / 3S / 5S / 10S",                            sTCell)],
        [p("EFFECTS",        sTCell), p("Databending no JPEG: ZIGZAG PERM, DQT EROSION, DHT REMAP, SCAN SWAP, CHROMA AMP", sTCell)],
        [p("WIFI",           sTCell), p("Submenu de configuração de rede ( ver abaixo )",                               sTCell)],
        [p("FORMAT SD CARD", sTCell), p("Formata o cartão SD — pede confirmação pelo joystick",                        sTCell)],
        [p("EXIT",           sTCell), p("Fecha o menu e volta ao viewfinder",                                          sTCell)],
    ], [1.3*inch, 3.6*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Indicadores no viewfinder"),
    Spacer(1, 4),
    pinout_table([
        [p("Indicador",         sTHead), p("Significado",                                           sTHead)],
        [p("L3S / L5S / L10S", sTCell), p("Longa exposição ativa ( stacking de 3s / 5s / 10s )",  sTCell)],
        [p("Barra de EV",       sTCell), p("Compensação de exposição  ( −3 a +3 )",                 sTCell)],
        [p("Paleta de cor",     sTCell), p("Estilo visual do viewfinder — não afeta a foto final",  sTCell)],
    ], [1.5*inch, 3.4*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("WiFi e galeria web"),
    Spacer(1, 4),
    p("1.  MENU → WIFI → CONFIGURE: abre o ponto de acesso <b>CyberShot-Setup</b>", sBody),
    Spacer(1, 3),
    p("2.  Conecte seu celular a essa rede e abra o navegador em <b>192.168.4.1</b>", sBody),
    Spacer(1, 3),
    p("3.  Digite as credenciais do seu WiFi doméstico e salve", sBody),
    Spacer(1, 3),
    p("4.  Acesse a galeria em <b>http://cybershot.local</b>  ( ou pelo IP mostrado na tela )", sBody),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Submenu WIFI"),
    Spacer(1, 4),
    pinout_table([
        [p("Opção",       sTHead), p("Função",                                                sTHead)],
        [p("CONFIGURE",   sTCell), p("Abre portal cativo para configurar uma rede WiFi nova", sTCell)],
        [p("CONNECT STA", sTCell), p("Conecta usando as credenciais já salvas",               sTCell)],
        [p("DIRECT AP",   sTCell), p('Cria ponto de acesso direto "CYBERSHOT" — sem senha',   sTCell)],
        [p("DISCONNECT",  sTCell), p("Desconecta do WiFi atual",                              sTCell)],
        [p("BACK",        sTCell), p("Volta ao menu principal",                               sTCell)],
    ], [1.3*inch, 3.6*inch]),
]))

# ════════════════════════════════════════════════════════
#  PÁGINA 5 — dicas
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("DICAS E NOTAS"))
story.append(Spacer(1, 8))

TIPS = [
    ("PSRAM necessária",
     "O projeto usa PSRAM extensivamente. Garanta que sua placa ESP32-S3 tenha "
     "pelo menos 8 MB de PSRAM — placas sem PSRAM falham ao alocar os frame buffers."),
    ("Longa exposição",
     "Empilha múltiplos quadros JPEG ( 3s / 5s / 10s ) para um resultado bem exposto. "
     "O LED acende durante toda a captura. Funciona melhor com a câmera apoiada."),
    ("Resolução das fotos",
     "XGA 1024 × 768 JPEG para fotos normais e longa exposição. "
     "Salvas como /PHOTO_XXXX.JPG na raiz do cartão SD."),
    ("Galeria web",
     "Em http://cybershot.local: navegue, edite ( brilho, contraste, saturação, vinheta, "
     "aberração cromática, copy fx, pixel sort ) e baixe as fotos. "
     "Roda inteiramente no ESP32 — sem internet."),
    ("Credenciais WiFi",
     "Guardadas na memória NVS do ESP32. Nunca ficam no código-fonte nem no cartão SD."),
    ("Formatar cartão SD",
     "MENU → FORMAT SD CARD. Pede confirmação pelo joystick. Apenas FAT32, máx. 32 GB."),
    ("Alimentação",
     "Funciona via USB-C ( 5 V ). Para uso portátil: bateria LiPo 3.7 V com módulo "
     "de carga/boost ( TP4056 + MT3608 ou similar )."),
    ("Display em branco",
     "Se a tela ficar branca, confira a fiação SCK / SDA / CS / DC / RST. "
     "O RST precisa ser pulsado LOW e depois HIGH na inicialização."),
]

tip_rows = []
for title, body in TIPS:
    tip_rows.append([
        p('<font color="#00a86b">●</font>', sNum),
        p(f"<b>{title}</b>  —  {body}", sBody),
    ])

tip_t = Table(tip_rows, colWidths=[0.24*inch, None], hAlign="LEFT")
tip_t.setStyle(TableStyle([
    ("VALIGN",        (0,0), (-1,-1), "TOP"),
    ("TOPPADDING",    (0,0), (-1,-1), 8),
    ("BOTTOMPADDING", (0,0), (-1,-1), 8),
    ("LEFTPADDING",   (0,0), ( 0,-1), 8),
    ("RIGHTPADDING",  (0,0), ( 0,-1), 2),
    ("LEFTPADDING",   (1,0), ( 1,-1), 4),
    ("LINEBELOW",     (0,0), (-1,-2), 0.4, BORDER),
    ("ROWBACKGROUNDS",(0,0), (-1,-1), [WHITE, colors.HexColor("#f8f8f8")]),
]))
story.append(tip_t)

story.append(Spacer(1, 16))
story.append(rule(ACCENT, 1, 0, 6))
story.append(p(
    "DIY - CYBERSHOT  //  projeto open source  //  "
    "feito com PlatformIO + esp32-camera  //  "
    "github.com/seu-usuario/cybershot-cam", sSmall))

# ── build ────────────────────────────────────────────────────────────────────
doc = SimpleDocTemplate(
    OUT, pagesize=A4,
    leftMargin=LM, rightMargin=RM, topMargin=TM, bottomMargin=BM,
    title="DIY - CYBERSHOT — Guia de Montagem",
    author="@lixofuturista / @cebolander",
)
doc.build(story, onFirstPage=on_page, onLaterPages=on_page)
print("OK ->", OUT)
