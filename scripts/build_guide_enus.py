# -*- coding: utf-8 -*-
"""Assembly guide DIY - CYBERSHOT  |  EN-US"""
import os
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import inch, mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    Image, HRFlowable, KeepTogether, PageBreak
)
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from PIL import Image as PILImage

# ── paths ──────────────────────────────────────────────────────────────────
IMG_DIR = (r"C:\Users\USURIO~2\AppData\Local\Temp\claude"
           r"\C--Users-Usu-rio-Documents-PlatformIO-Projects-cybershot-cam"
           r"\f7c60bc2-4569-4700-b03b-7182bd87e4de\scratchpad\pdfimgs")
OUT = (r"C:\Users\Usuário\Documents\PlatformIO\Projects\cybershot-cam"
       r"\DIY_CYBERSHOT_Guide_EN-US.pdf")

# ── palette ─────────────────────────────────────────────────────────────────
BLACK   = colors.HexColor("#111111")
DGRAY   = colors.HexColor("#444444")
GRAY    = colors.HexColor("#777777")
LGRAY   = colors.HexColor("#f4f4f4")
BORDER  = colors.HexColor("#d8d8d8")
ACCENT  = colors.HexColor("#00a86b")
WHITE   = colors.white

W, H = A4
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
    return p(f'<font color="#00a86b">&#9616;</font>  {text}', sH2)

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
    canvas.setFillColor(ACCENT)
    canvas.rect(0, H - 5, W, 5, fill=1, stroke=0)
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
#  PAGE 1 — cover + components
# ════════════════════════════════════════════════════════
story.append(Spacer(1, 2))
story.append(p("DIY - CYBERSHOT", sTitle))
story.append(Spacer(1, 3))
story.append(p("by @lixofuturista / @cebolander", sAuthor))
story.append(Spacer(1, 8))
story.append(accent_rule(before=0, after=6))
story.append(p(
    "Assembly guide for the CyberShot Cam — DIY digital camera built with ESP32-S3, "
    "live viewfinder, long exposure, databending effects and web gallery.", sDesc))
story.append(Spacer(1, 10))

# ── components ────────────────────────────────────────────
story.append(h1("WHAT YOU'LL NEED"))
story.append(Spacer(1, 6))

COMPS = [
    ("Esp32-s3 Cam Ov5640 Wifi Bluetooth 5.0 Ble N16r8", "page1_img0.png"),
    ("TFT LCD Display 1.8\" 128x160 ST7735",              "page1_img1.png"),
    ("Analog Joystick Module KY-023",                      "page1_img2.png"),
    ("Push Button (momentary)",                            "page1_img3.png"),
    ("Micro SD Card / Any model",                          "sdcard.png"),
    ("5mm High-Brightness LED, Red (optional)",            "page2_img0.png"),
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
    "<b>You'll also need:</b>  soldering iron / solder / "
    "wires for connections or a circuit board.", sSmall))

# ════════════════════════════════════════════════════════
#  PAGE 2 — pinouts
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("PINOUTS"))
story.append(Spacer(1, 8))

story.append(KeepTogether([
    h2("TFT Display (ST7735)"),
    p("Connected via hardware SPI (FSPI bus). "
      "Keep wires short for reliable high-speed operation.", sBody),
    Spacer(1, 5),
    pinout_table([
        [p("TFT Pin",    sTHead), p("Function",     sTHead), p("ESP32-S3 GPIO", sTHead)],
        [p("VCC",        sTCell), p("Power",        sTCell), p("3.3 V",         sTCell)],
        [p("GND",        sTCell), p("Ground",       sTCell), p("GND",           sTCell)],
        [p("SCK",        sTCell), p("SPI Clock",    sTCell), p("GPIO 47",       sTCell)],
        [p("SDA (MOSI)", sTCell), p("SPI Data",     sTCell), p("GPIO 45",       sTCell)],
        [p("CS",         sTCell), p("Chip Select",  sTCell), p("GPIO 43",       sTCell)],
        [p("DC",         sTCell), p("Data/Command", sTCell), p("GPIO 14",       sTCell)],
        [p("RST",        sTCell), p("Reset",        sTCell), p("GPIO 21",       sTCell)],
    ], [1.4*inch, 2.0*inch, 1.5*inch]),
    p("NOTE: the TFT uses the FSPI bus. Do not share this bus with other SPI devices.", sNote),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Analog Joystick (KY-023)"),
    Spacer(1, 4),
    pinout_table([
        [p("Joystick Pin",  sTHead), p("ESP32-S3 GPIO", sTHead)],
        [p("VRx (X axis)",  sTCell), p("GPIO 1  (ADC)", sTCell)],
        [p("VRy (Y axis)",  sTCell), p("GPIO 2  (ADC)", sTCell)],
        [p("SW (button)",   sTCell), p("GPIO 42",       sTCell)],
        [p("VCC",           sTCell), p("3.3 V",         sTCell)],
        [p("GND",           sTCell), p("GND",           sTCell)],
    ], [2.5*inch, 2.4*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Push Button (momentary)"),
    Spacer(1, 4),
    pinout_table([
        [p("Button Pin",  sTHead), p("ESP32-S3 GPIO", sTHead)],
        [p("Terminal A",  sTCell), p("GPIO 41",       sTCell)],
        [p("Terminal B",  sTCell), p("GND",           sTCell)],
    ], [2.5*inch, 2.4*inch]),
    p("NOTE: internal pull-up enabled, active LOW — no external resistor needed.", sNote),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("LED (system indicator)"),
    Spacer(1, 4),
    pinout_table([
        [p("LED Terminal", sTHead), p("Connection",                  sTHead)],
        [p("Anode",        sTCell), p("100 Ohm resistor -> GPIO 48", sTCell)],
        [p("Cathode",      sTCell), p("GND",                         sTCell)],
    ], [2.5*inch, 2.4*inch]),
]))

# ════════════════════════════════════════════════════════
#  PAGE 3 — software setup
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("SOFTWARE SETUP"))
story.append(Spacer(1, 8))

STEPS = [
    ("01", "Install PlatformIO",
           "VS Code extension or CLI — https://platformio.org"),
    ("02", "Clone the repository",
           "git clone https://github.com/seu-usuario/cybershot-cam"),
    ("03", "Open in VS Code",
           "Open the project folder with the PlatformIO extension active"),
    ("04", "Connect the board",
           "Connect the ESP32-S3 via USB-C; hold BOOT on the first flash"),
    ("05", "Flash the firmware",
           'Click "Upload" in PlatformIO, or run: pio run --target upload'),
    ("06", "Prepare the SD card",
           "Insert a FAT32-formatted microSD card before the first boot"),
    ("07", "Configure WiFi",
           "On the device: MENU  ->  WIFI  ->  CONFIGURE"),
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

story.append(h2("Dependencies"))
story.append(p(
    "Automatically installed by PlatformIO via "
    "<font face='Courier'>platformio.ini</font> — no manual setup needed.", sBody))
story.append(Spacer(1, 5))
for dep in ["adafruit/Adafruit ST7735 and ST7789 Library",
            "adafruit/Adafruit GFX Library",
            "bitbank2/JPEGDEC"]:
    story.append(p(f'<font color="#00a86b">+</font>  {dep}', sSmall))
    story.append(Spacer(1, 2))

# ════════════════════════════════════════════════════════
#  PAGE 4 — controls
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("CONTROLS & HOW TO USE"))
story.append(Spacer(1, 8))

story.append(KeepTogether([
    h2("Input controls"),
    Spacer(1, 4),
    pinout_table([
        [p("Action",                          sTHead), p("Result",                          sTHead)],
        [p("HOLD button  ( > 800 ms )",       sTCell), p("Open / close the main menu",      sTCell)],
        [p("PRESS button  ( short )",         sTCell), p("Take a photo from the viewfinder", sTCell)],
        [p("JOYSTICK  UP / DOWN",             sTCell), p("Navigate menu items",              sTCell)],
        [p("JOYSTICK  LEFT / RIGHT",          sTCell), p("Confirm / adjust values",          sTCell)],
        [p("JOYSTICK PRESS  ( SW )",          sTCell), p("Back / cancel",                   sTCell)],
    ], [2.55*inch, 2.35*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Main menu  ( hold the button in viewfinder )"),
    Spacer(1, 4),
    pinout_table([
        [p("Item",           sTHead), p("Function",                                                                         sTHead)],
        [p("VF COLOR",       sTCell), p("Viewfinder color palette  ( green / red / pink / white / cyan )",                  sTCell)],
        [p("LONG EXP",       sTCell), p("Long exposure with frame stacking — OFF / 3S / 5S / 10S",                          sTCell)],
        [p("TIMER",          sTCell), p("Self-timer before capture — OFF / 3S / 5S / 10S",                                  sTCell)],
        [p("EFFECTS",        sTCell), p("JPEG databending: ZIGZAG PERM, DQT EROSION, DHT REMAP, SCAN SWAP, CHROMA AMP",    sTCell)],
        [p("WIFI",           sTCell), p("Network configuration submenu  ( see below )",                                     sTCell)],
        [p("FORMAT SD CARD", sTCell), p("Format the SD card — confirm with joystick",                                       sTCell)],
        [p("EXIT",           sTCell), p("Close the menu and return to viewfinder",                                          sTCell)],
    ], [1.3*inch, 3.6*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("Viewfinder indicators"),
    Spacer(1, 4),
    pinout_table([
        [p("Indicator",         sTHead), p("Meaning",                                              sTHead)],
        [p("L3S / L5S / L10S", sTCell), p("Long exposure active  ( 3s / 5s / 10s stacking )",     sTCell)],
        [p("EV bar",            sTCell), p("Exposure compensation  ( -3 to +3 )",                  sTCell)],
        [p("Color palette",     sTCell), p("Viewfinder visual style — does not affect saved photo", sTCell)],
    ], [1.5*inch, 3.4*inch]),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("WiFi & web gallery"),
    Spacer(1, 4),
    p("1.  MENU -> WIFI -> CONFIGURE: opens the <b>CyberShot-Setup</b> access point", sBody),
    Spacer(1, 3),
    p("2.  Connect your phone to that network and open a browser at <b>192.168.4.1</b>", sBody),
    Spacer(1, 3),
    p("3.  Enter your home WiFi credentials and save", sBody),
    Spacer(1, 3),
    p("4.  Browse the gallery at <b>http://cybershot.local</b>  ( or the IP shown on screen )", sBody),
    Spacer(1, 14),
]))

story.append(KeepTogether([
    h2("WIFI submenu"),
    Spacer(1, 4),
    pinout_table([
        [p("Option",      sTHead), p("Function",                                              sTHead)],
        [p("CONFIGURE",   sTCell), p("Opens a captive portal to set up a new WiFi network",   sTCell)],
        [p("CONNECT STA", sTCell), p("Connect using already saved credentials",               sTCell)],
        [p("DIRECT AP",   sTCell), p('Create open access point "CYBERSHOT" — no password',    sTCell)],
        [p("DISCONNECT",  sTCell), p("Disconnect from current WiFi",                          sTCell)],
        [p("BACK",        sTCell), p("Return to main menu",                                   sTCell)],
    ], [1.3*inch, 3.6*inch]),
]))

# ════════════════════════════════════════════════════════
#  PAGE 5 — tips
# ════════════════════════════════════════════════════════
story.append(PageBreak())
story.append(h1("TIPS & NOTES"))
story.append(Spacer(1, 8))

TIPS = [
    ("PSRAM required",
     "The project uses PSRAM extensively. Make sure your ESP32-S3 board has at least "
     "8 MB of PSRAM — boards without PSRAM will crash when allocating frame buffers."),
    ("Long exposure",
     "Stacks multiple JPEG frames ( 3s / 5s / 10s ) for a well-exposed result. "
     "The LED stays on throughout the capture. Works best with the camera on a stable surface."),
    ("Photo resolution",
     "XGA 1024 x 768 JPEG for both normal and long exposure shots. "
     "Saved as /PHOTO_XXXX.JPG at the root of the SD card."),
    ("Web gallery",
     "At http://cybershot.local: browse, edit ( brightness, contrast, saturation, vignette, "
     "chromatic aberration, copy fx, pixel sort ) and download photos. "
     "Runs entirely on the ESP32 — no internet required."),
    ("WiFi credentials",
     "Stored in ESP32 NVS flash memory. Never written to source code or the SD card."),
    ("Format SD card",
     "MENU -> FORMAT SD CARD. Confirm with the joystick. FAT32 only, max. 32 GB."),
    ("Power",
     "Runs via USB-C ( 5 V ). For portable use: 3.7 V LiPo battery with a "
     "charge/boost module ( TP4056 + MT3608 or similar )."),
    ("White screen",
     "If the display stays white, check the SCK / SDA / CS / DC / RST wiring. "
     "RST must be pulsed LOW then HIGH on startup."),
]

tip_rows = []
for title, body in TIPS:
    tip_rows.append([
        p('<font color="#00a86b">&#9679;</font>', sNum),
        p(f"<b>{title}</b>  -  {body}", sBody),
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
    "DIY - CYBERSHOT  //  open source project  //  "
    "built with PlatformIO + esp32-camera  //  "
    "github.com/seu-usuario/cybershot-cam", sSmall))

# ── build ────────────────────────────────────────────────────────────────────
doc = SimpleDocTemplate(
    OUT, pagesize=A4,
    leftMargin=LM, rightMargin=RM, topMargin=TM, bottomMargin=BM,
    title="DIY - CYBERSHOT — Assembly Guide",
    author="@lixofuturista / @cebolander",
)
doc.build(story, onFirstPage=on_page, onLaterPages=on_page)
print("OK ->", OUT)
