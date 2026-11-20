#!/usr/bin/env python3

import math
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
ART = os.path.join(HERE, "garden-art.svg")
HEAD, TAIL_AT, TAIL = 371, 43989, 7

HUE = ["#AEB1DC", "#35D2BA", "#FF8380", "#65FF00", "#698EFF", "#F15A29",
       "#00ED95", "#837CC7", "#FF3A5D", "#FFB7B3", "#CC6699", "#747F8A"]
OCHRE, BARK, UMBER, DGRAY, GUIDE = "#A18463", "#634F3B", "#3F3933", "#454A42", "#9CA596"
WHITE, BLACK = "#FFFFFF", "#000000"
BLUE, TAN, GRAY, ORANGE, PERI = "#3C6087", "#8A653C", "#CDD0C2", "#E52817", "#698EFF"
TEXT_BLUE, TEXT_TAN = "#4C7DA8", "#AD8959"
KNOB = [BLUE, TAN, GRAY, ORANGE]
TEXT_KNOB = [TEXT_BLUE, TEXT_TAN, GRAY, ORANGE]

PETALS = [6, 6, 4, 5, 5, 3, 6, 6, 2, 4, 4, 5]
STYLE = [0, 1, 2, 3, 4, 5, 6, 7, 3, 5, 0, 7]
PETAL = [
    [(0, (0, 0)), (1, (-100, 30, -105, 100, 0, 102)), (1, (105, 100, 100, 30, 0, 0))],
    [(0, (0, 0)), (1, (-70, 20, -115, 90, -20, 100)), (1, (115, 90, 70, 20, 0, 0))],
    [(0, (0, 0)), (1, (-100, 30, -100, 90, -45, 100)), (1, (-20, 100, -5, 82, 0, 82)), (1, (5, 82, 20, 100, 45, 100)), (1, (100, 90, 100, 30, 0, 0))],
    [(0, (0, 0)), (1, (-95, 35, -35, 85, 0, 115)), (1, (35, 85, 95, 35, 0, 0))],
    [(0, (0, 0)), (1, (-115, 45, -40, 80, 0, 115)), (1, (40, 80, 115, 45, 0, 0))],
    [(0, (0, 0)), (1, (-100, 40, -100, 40, -90, 100)), (2, (-35, 82)), (2, (0, 108)), (2, (35, 82)), (2, (90, 100)), (1, (100, 40, 100, 40, 0, 0))],
    [(0, (0, 0)), (1, (-120, 30, -120, 90, -45, 100)), (1, (-20, 100, -5, 68, 0, 68)), (1, (5, 68, 20, 100, 45, 100)), (1, (120, 90, 120, 30, 0, 0))],
    [(0, (0, 0)), (1, (-110, 20, -20, 70, -25, 106)), (1, (95, 95, 80, 35, 0, 0))],
]
SIN = [round(16384 * math.sin(2 * math.pi * i / 256)) for i in range(256)]

THIN = [("ochre", OCHRE, 1.3, 12), ("orange", ORANGE, 1.3, 24), ("white", WHITE, 2.0, 40), ("dgray", DGRAY, 2.0, 4)] + \
       [("hue%d" % i, HUE[i], 1.3, 12) for i in range(12)] + \
       [("knob%d" % k, KNOB[k], 2.0, 1) for k in range(4)]
THICK = [("ochre", OCHRE, 3.0, 16), ("orange", ORANGE, 3.0, 16)] + [("hue%d" % i, HUE[i], 3.0, 16) for i in range(12)]
ICON_COPIES = 8
SMALL_COPIES = 3

def sn(a):
    return SIN[a & 255]

def cs(a):
    return SIN[(a + 64) & 255]

def f(v):
    s = ("%.2f" % v).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s

def P(d, stroke, width, fill="none", cap="round"):
    return '<path fill="%s" stroke="%s" stroke-width="%s" stroke-linecap="%s" d="%s"/>' % (fill, stroke, f(width), cap, d)

def L(x, y, xx, yy, w, c, cap="round"):
    return P("M%s,%sL%s,%s" % (f(x), f(y), f(xx), f(yy)), c, w, cap=cap)

def poly(points, w, c, closed=False):
    return P("M" + "L".join("%s,%s" % (f(x), f(y)) for x, y in points) + ("z" if closed else ""), c, w)

def ring(x, y, r, w, c):
    return poly([(x + r * cs(i * 256 // 12) / 16384.0, y + r * sn(i * 256 // 12) / 16384.0) for i in range(12)], w, c, True)

def box(x, y, w, h, lw, c):
    return poly([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], lw, c, True)

def filled_rect(x, y, w, h, c):
    return P("M%s,%sL%s,%sL%s,%sL%s,%sz" % (f(x), f(y), f(x + w), f(y), f(x + w), f(y + h), f(x), f(y + h)), c, 1, fill=c)

def ellipse(cx, cy, rx, ry, c):
    k = 0.5523
    d = ("M%s,%s" % (f(cx + rx), f(cy)) +
         "C%s,%s %s,%s %s,%s" % (f(cx + rx), f(cy + k * ry), f(cx + k * rx), f(cy + ry), f(cx), f(cy + ry)) +
         "C%s,%s %s,%s %s,%s" % (f(cx - k * rx), f(cy + ry), f(cx - rx), f(cy + k * ry), f(cx - rx), f(cy)) +
         "C%s,%s %s,%s %s,%s" % (f(cx - rx), f(cy - k * ry), f(cx - k * rx), f(cy - ry), f(cx), f(cy - ry)) +
         "C%s,%s %s,%s %s,%s" % (f(cx + k * rx), f(cy - ry), f(cx + rx), f(cy - k * ry), f(cx + rx), f(cy)) + "z")
    return P(d, c, 1, fill=c)

def frame_point(cx, cy, a, base, Ln, W, px, py):
    lx, ly = px * W / 100.0, base + py * Ln / 100.0
    c, s = math.cos(2 * math.pi * a / 256), math.sin(2 * math.pi * a / 256)
    return cx + lx * c + ly * s, cy + lx * s - ly * c

def petal_d(style, cx, cy, a, base, Ln, W):
    out = []
    for op, p in PETAL[style]:
        pts = [frame_point(cx, cy, a, base, Ln, W, p[i], p[i + 1]) for i in range(0, len(p), 2)]
        out.append(("M" if op == 0 else "L" if op == 2 else "C") + " ".join("%s,%s" % (f(x), f(y)) for x, y in pts))
    return "".join(out)

def petal_angle(j, n):
    return 128 + (2 * j + 1) * 128 // n

class Picture:
    def __init__(self):
        self.body = []
        self.n = 0
        self.texts = []

    def part(self, inner):
        i = self.n
        self.n += 1
        self.body.append('<g id="n%d">%s</g>\n' % (i, "".join(inner)))
        return i

    def line_part(self, colour, width):
        i = self.n
        self.n += 1
        self.body.append('<line id="n%d" fill="none" stroke="%s" stroke-width="%s" stroke-linecap="butt" x1="0" y1="0" x2="1" y2="0"/>\n'
                         % (i, colour, f(width)))
        return i

    def text(self, colour, align, height=14):

        i = len(self.texts)
        self.texts.append(align)
        self.body.append('<path id="t%d" fill="none" stroke="%s" stroke-width="1" d="M0,-%dL1,-%dL1,0L0,0z"/>\n'
                         % (i, colour, height, height))
        return i

def art_groups():
    return dict(re.findall(r'<g id="(\w+)">(.*?)</g>', open(ART).read(), re.S))

def bloom_petal(kind, rx, j, state):

    n, cx, cy, ry = PETALS[kind], 240, 78, 15
    a = petal_angle(j, n)
    base = ry + (rx - ry) * abs(sn(a)) // 16384 + 1
    return P(petal_d(STYLE[kind], cx, cy, a, base, 28 if state == 0 else 30, 15 if n <= 3 else 12),
             WHITE if state == 2 else HUE[kind], 2.0 if state == 0 else 2.5)

def flower_icon(kind, r=10, w=1.2):
    n, out = PETALS[kind], []
    for j in range(n):
        out.append(P(petal_d(STYLE[kind], 0, 0, petal_angle(j, n), 2.5, r, r * (50 if n <= 3 else 40) / 100.0), HUE[kind], w))
    out.append(L(-1, 0, 1, 0, 2.5, OCHRE))
    return out

GROUND_PERIOD = [0, 48, 26, 144, 32, 16, 0, 0]
HARP_X = list(range(20, 466, 24))

def ground(style):
    Y = 122
    if style == 6:
        return [L(x, Y, min(x + 10, 466), Y, 1.6, UMBER) for x in range(14, 466, 14)]
    end = 466 + GROUND_PERIOD[style]
    pts = []
    for x in list(range(14, end, 8)) + [end]:
        u, y = x, Y
        if style == 1:
            y -= 2 * sn(u * 256 // 48) / 16384.0
        if style == 2:
            y -= 3 * sn(u * 256 // 26) / 16384.0
        if style == 3:
            y -= (((u // 9) % 16) * 2654435761 & 0xffffffff) >> 30
        if style == 4:
            y -= 3 if (u // 16) % 2 else 0
        if style == 5:
            y -= 3 if (u // 8) % 2 else 0
        pts.append((x, y))
    return [poly(pts, 1.4, UMBER)]

def build():
    art = art_groups()
    pic = Picture()
    H = []
    single = {}

    def one(name, inner):
        single[name] = pic.part(inner)

    def family(name, items):
        first = None
        for inner in items:
            i = pic.part(inner)
            first = i if first is None else first
        H.append("#define %s_AT %d" % (name, first))

    for page, inner in enumerate([
        [L(240, 93, 240, 122, 2.0, GUIDE)],
        [L(104 + 272 * b / 4.0, 22, 104 + 272 * b / 4.0, 166, 1.0 if b % 4 else 1.4, DGRAY if b % 4 else GUIDE) for b in range(5)] +
        [L(104 + 272 * q / 16.0, 163, 104 + 272 * q / 16.0, 166, 1.0, DGRAY) for q in range(1, 16) if q % 4],
        [L(0, 0, 1, 0, 0.5, BLACK)],
        [L(14 + 116 * (i % 4) + 6 + 24 * k, (34 if i < 4 else 124) + 60, 14 + 116 * (i % 4) + 26 + 24 * k,
           (34 if i < 4 else 124) + 60, 1.0, DGRAY) for i in range(8) for k in range(4)],
    ]):
        one("PAGE%d" % (page + 1), inner)

    s0 = 60 - 18
    one("STAIR", [L(s0, 24, s0 + 4, 24, 1.5, BLUE), L(s0 + 4, 24, s0 + 4, 20, 1.5, BLUE), L(s0 + 4, 20, s0 + 8, 20, 1.5, BLUE),
                  L(s0 + 8, 20, s0 + 8, 16, 1.5, BLUE), L(s0 + 8, 16, s0 + 12, 16, 1.5, BLUE)])
    chips = []
    for k in range(4):
        c = 60 + 120 * k
        x = s0 - 18 if k == 0 else c - 24
        for b in range(2):
            bx = x + 7 * b
            chips.append([box(bx, 15, 5, 7, 1.0, KNOB[k])])
            chips.append([filled_rect(bx, 15, 5, 7, KNOB[k])])
    family("CHIP", chips)
    dots = []
    for k in range(4):
        x = 231 + 6 * k
        dots += [[L(x - 1, 6, x + 1, 6, 1.5, DGRAY)], [L(x - 1, 6, x + 1, 6, 3.0, WHITE)], [L(x - 1, 6, x + 1, 6, 3.0, ORANGE)]]
    family("DOT", dots)

    one("SUN", [ring(0, 0, 6, 1.5, BLUE)])
    half = [4, 5, 5, 5, 4]
    one("SUNFILL", [L(-half[k], -4 + 2 * k, half[k], -4 + 2 * k, 2.2, BLUE) for k in range(5)])
    one("HORIZON", [L(430, 115, 462, 115, 1.0, BLUE), L(436, 111, 456, 111, 1.0, BLUE)])
    rays, lead = [], []
    for fv in range(17):
        n, ln = 3 + fv * 5 // 16, 3 + fv * 6 // 16
        rays.append([L(9 * cs(192 + (2 * k - (n - 1)) * 12) / 16384.0, 9 * sn(192 + (2 * k - (n - 1)) * 12) / 16384.0,
                       (9 + ln) * cs(192 + (2 * k - (n - 1)) * 12) / 16384.0, (9 + ln) * sn(192 + (2 * k - (n - 1)) * 12) / 16384.0,
                       1.2, BLUE) for k in range(n)])
        lead.append([L(10 * cs(192 + (2 * k - (n - 2)) * 12) / 16384.0, 10 * sn(192 + (2 * k - (n - 2)) * 12) / 16384.0,
                       (14 + ln) * cs(192 + (2 * k - (n - 2)) * 12) / 16384.0, (14 + ln) * sn(192 + (2 * k - (n - 2)) * 12) / 16384.0,
                       1.8, PERI) for k in range(n - 1)])
    family("RAYS", rays)
    family("LEAD", lead)
    family("GROUND", [ground(s) for s in range(8)])
    H.append("static const unsigned char GROUND_PERIOD[8] = { %s };" % ", ".join(str(p) for p in GROUND_PERIOD))
    one("GMASK", [filled_rect(0, 104, 13.3, 32, BLACK), filled_rect(466.7, 104, 13.3, 32, BLACK)])
    family("HARP", [[L(0, 0, 0, -6, 1.0, UMBER)] for x in HARP_X])
    H.append("static const unsigned short HARP_X[%d] = { %s };" % (len(HARP_X), ", ".join(str(x) for x in HARP_X)))
    H.append("#define HARP_N %d" % len(HARP_X))
    family("MOLE", [[art["m%d" % i]] for i in range(5)])

    petal_at, items = [], []
    for k in range(12):
        petal_at.append(len(items))
        for w, rx in ((0, 16), (1, 20)):
            for j in range(PETALS[k]):
                for state in (0, 1, 2):
                    items.append([bloom_petal(k, rx, j, state)])
    family("PETAL", items)
    H.append("static const unsigned short PETAL_OF[12] = { %s };" % ", ".join(str(x) for x in petal_at))
    H.append("static const unsigned char PETALS[12] = { %s };" % ", ".join(str(x) for x in PETALS))
    one("DISC1", [ellipse(240, 78, 15, 14, OCHRE)])
    one("DISC2", [ellipse(240, 78, 19, 14, OCHRE)])

    family("KRING", [[ring(30 + 60 * i, 179, 6, 1.0, DGRAY)] for i in range(8)])
    family("KRINGW", [[ring(30 + 60 * i, 179, 6, 1.0, WHITE)] for i in range(8)])
    family("HELD", [[L(30 + 60 * i - 14, 196, 30 + 60 * i + 14, 196, 1.5, GRAY)] for i in range(8)])
    family("HELDW", [[L(30 + 60 * i - 14, 196, 30 + 60 * i + 14, 196, 1.5, WHITE)] for i in range(8)])
    family("MUTE", [[L(30 + 60 * (4 + i) - 11, 189, 30 + 60 * (4 + i) + 11, 169, 1.2, DGRAY)] for i in range(4)])
    family("CRITTER", [[art["c%d" % e]] for e in range(12)])
    family("SMALL", [[art["c%d" % e]] for e in range(12) for c in range(SMALL_COPIES)])
    H.append("#define SMALL_COPIES %d" % SMALL_COPIES)
    family("ICON", [flower_icon(k) for k in range(12) for c in range(ICON_COPIES)])
    H.append("#define ICON_COPIES %d" % ICON_COPIES)
    family("ICONON", [[L(-1, 0, 1, 0, 4.0, WHITE)] for c in range(9)])

    X1 = 376
    one("LOOPMARK", [L(X1 + 4, 48, X1 + 4, 140, 2.0, BLUE), L(X1 + 8, 48, X1 + 8, 140, 1.0, BLUE),
                     L(X1 - 4, 86, X1 - 2, 86, 3.0, BLUE), L(X1 - 4, 102, X1 - 2, 102, 3.0, BLUE)])
    one("PLAYHEAD", [L(0, 22, 0, 166, 1.5, WHITE)])

    frames = []
    for q in range(4):
        ext, row = q >= 2, q & 1
        x0, y0 = (244 if ext else 12), (114 if row else 16)
        frames += [[box(x0, y0, 224, 90, 1.0, DGRAY)], [box(x0, y0, 224, 90, 1.8, WHITE)]]
    family("FRAME", frames)
    family("CURSOR", [[box(-26, -22, 52, 66, 2.0, KNOB[k])] for k in range(4)])

    boxes = []
    for i in range(8):
        x0, y0 = 14 + 116 * (i % 4), (34 if i < 4 else 124)
        pts = [(x0, y0 + 6), (x0 + 52, y0 + 12), (x0 + 104, y0 + 6), (x0 + 104, y0 + 84), (x0, y0 + 84)]
        boxes += [[poly(pts, 1.3, DGRAY, True)], [poly(pts, 1.3, BARK, True)], [poly(pts, 2.0, WHITE, True)]]
    family("GBOX", boxes)
    one("GSEL", [poly([(3, 10), (52, 15), (101, 10), (101, 81), (3, 81)], 1.5, BLUE, True)])
    one("GSQ", [L(0, 0, 20, 0, 1.5, ORANGE), L(0, 0, 0, 18, 1.5, ORANGE), L(20, 0, 20, 18, 1.5, ORANGE), L(0, 18, 20, 18, 2.0, ORANGE)])

    def mark(i, x, y, c):
        out = []
        if i == 2:
            l, r, t, b = x - 6, x + 6, y - 10, y - 2
            out += [poly([(l + 2, t), (l, t + 2), (l, b - 2), (l + 2, b), (r - 2, b), (r, b - 2), (r, t + 2), (r - 2, t), (x + 1, t)], 1.2, c)]
            out += [L(x - 2 + k, t - 2 + k, x - 2 + k, t + 2 - k, 1.0, c) for k in range(3)]
        if i == 3:
            out += [L(x - 6 + 2 * k, y - 10 + k, x - 6 + 2 * k, y - k, 1.2, c) for k in range(5)] + [L(x + 5, y - 10, x + 5, y, 1.5, c)]
        if i == 4:
            out += [L(x - 1 - 2 * k, y - 10 + k, x - 1 - 2 * k, y - k, 1.2, c) for k in range(5)]
            out += [poly([(x + 2, y), (x + 2, y - 10), (x + 7, y - 10), (x + 7, y - 6), (x + 2, y - 6)], 1.2, c), L(x + 4, y - 6, x + 7, y, 1.2, c)]
        if i == 5:
            out += [L(x - 6 + 4 * k, y - 2, x - 5 + 4 * k, y - 2, 2.0, c) for k in range(4)]
        return out
    marks = []
    for i in range(2, 6):
        x0, y0 = 14 + 116 * (i % 4), (34 if i < 4 else 124)
        for c in (GUIDE, BLUE):
            marks.append(mark(i, x0 + 59, y0 + 80, c))
    family("MARK", marks)
    one("WALKON", [filled_rect(14, 10, 66, 23, BLUE)])
    one("WALKBOX", [box(14, 10, 66, 23, 1.5, BLUE)])

    one("HELP", [ellipse(20, 58 + 30 * k, 5, 5, KNOB[k]) for k in range(4)] + [L(8, 172, 472, 172, 1.0, DGRAY)])

    for name, pools in (("THIN", THIN), ("THICK", THICK)):
        at = []
        for pname, colour, width, count in pools:
            at.append(pic.n)
            for _ in range(count):
                pic.line_part(colour, width)
        H.append("static const unsigned short %s_AT[%d] = { %s };" % (name, len(pools), ", ".join(str(x) for x in at)))
        H.append("static const unsigned char %s_N[%d] = { %s };" % (name, len(pools), ", ".join(str(p[3]) for p in pools)))
        H.append("enum { %s };" % ", ".join("%s_%s = %d" % (name, p[0].upper(), i) for i, p in enumerate(pools)))

    T = {}
    for k in range(4):
        T["ST%d" % k] = pic.text(TEXT_KNOB[k], 1)
    T["ROOT"] = pic.text(BLACK, 1)
    for i in range(8):
        T["KLW%d" % i] = pic.text(WHITE, 1)
    for i in range(8):
        T["KLG%d" % i] = pic.text(GUIDE, 1)
    for i in range(9):
        T["NOTE%d" % i] = pic.text(WHITE, 1)
    T["T1_LAST"] = T["NOTE8"]
    for k in range(4):
        T["T2L%d" % k] = pic.text(TEXT_KNOB[k], 0)
        T["T2V%d" % k] = pic.text(TEXT_KNOB[k], 0)
    T["T2_LAST"] = T["T2V3"]
    for i in range(16):
        T["T3L%d" % i] = pic.text(WHITE, 1)
    T["T3_LAST"] = T["T3L15"]
    for i in range(8):
        T["T4N%d" % i] = pic.text(WHITE, 1)
    for i in range(8):
        T["T4K%d" % i] = pic.text(WHITE, 1)
    T["T4TOP"] = pic.text(WHITE, 1)
    T["T4WALK"] = pic.text(WHITE, 1)
    T["T4S0"] = pic.text(TEXT_BLUE, 0)
    T["T4S1"] = pic.text(TEXT_TAN, 1)
    T["T4S2"] = pic.text(GUIDE, 1)
    T["T4_LAST"] = T["T4S2"]
    for k in range(4):
        T["HTW%d" % k] = pic.text(WHITE, 1)
    for k in range(4):
        T["HTG%d" % k] = pic.text(GUIDE, 1)
    for k in range(4):
        T["HR%d" % k] = pic.text(TEXT_KNOB[k], 0)
    for k in range(4):
        T["HP%d" % k] = pic.text(GUIDE, 2)
    T["HK0"] = pic.text(WHITE, 0)
    T["HK1"] = pic.text(GUIDE, 0)
    H.append("static const unsigned char TEXT_ALIGN[%d] = { %s };" % (len(pic.texts), ", ".join(str(a) for a in pic.texts)))

    for name, i in single.items():
        H.append("#define %s %d" % (name, i))
    for name, i in T.items():
        H.append("#define TX_%s %d" % (name, i))
    H.append("#define PART_COUNT %d" % pic.n)
    H.append("#define TEXT_COUNT %d" % len(pic.texts))
    return "".join(pic.body).encode(), H

def main():
    ours, header = build()
    size = HEAD + len(ours) + TAIL
    text = (b"op1f picture 2\nsize %d\nfrom tempo.svg 43996 eb050950\nc 0 0 %d\na %d\n" % (size, HEAD, len(ours))
            + ours + b"\nc 0 %d %d\n" % (TAIL_AT, TAIL))
    open(os.path.join(ROOT, "garden.svg.make"), "wb").write(text)
    with open(os.path.join(ROOT, "garden_parts.h"), "w") as out:
        out.write("// Written by art/make_picture.py with garden.svg.make: the parts of the picture by index. Do not edit.\n")
        out.write("#ifndef GARDEN_PARTS_H\n#define GARDEN_PARTS_H\n")
        out.write("\n".join(header) + "\n#endif\n")
    print("wrote garden.svg.make (%d bytes of picture, %d of ours) and garden_parts.h" % (size, len(ours)))

if __name__ == "__main__":
    main()
