"""Builds Faltung_LTI.pptx (python-pptx). Run figs.py first.

Interactive slides carry
  pa_<ID>      invisible rectangle = plot area for the macros
  static_<ID>  PNG of the default state (hidden by the macros)
  txt_<ID>     readout text box (rewritten by the macros)
  btn_<Macro>  buttons; Setup in Faltung.bas links them to <Macro>
"""
import os
import numpy as np
from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from model import *

HERE = os.path.dirname(os.path.abspath(__file__))
B = os.path.join(HERE, "build")

INK, ORANGE, TEAL, VIOLET, RED = "1B2A41", "E07A1F", "2A9D8F", "7B5EA7", "C8553D"
GRAY, MUTED, PALE, WHITE, ICE = "8A94A6", "5B6575", "F1F4F8", "FFFFFF", "C9D6EA"
HEAD, BODY, MATH = "Cambria", "Calibri", "Cambria Math"

prs = Presentation()
prs.slide_width, prs.slide_height = Inches(13.333), Inches(7.5)
BLANK = prs.slide_layouts[6]
PA = (4.85, 1.3, 7.9, 4.85)                   # plot area of interactive slides


def rgb(h):
    return RGBColor.from_string(h)


def name(shape, n):
    shape._element.xpath("./*[1]/p:cNvPr")[0].set("name", n)


def bg(slide, color):
    f = slide.background.fill
    f.solid()
    f.fore_color.rgb = rgb(color)


def text(slide, x, y, w, h, paras, size=15, color=INK, font=BODY, bold=False,
         align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP, space=6, nm=None, margin=0.0):
    """paras: list of str or list of (text, opts) runs; opts: b, c, f, s, i."""
    tb = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = tb.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    for side in ("left", "right", "top", "bottom"):
        setattr(tf, f"margin_{side}", Inches(margin))
    for i, p in enumerate(paras):
        para = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        para.alignment = align
        para.space_after = Pt(space)
        runs = [(p, {})] if isinstance(p, str) else p
        for t, o in runs:
            r = para.add_run()
            r.text = t
            fo = r.font
            fo.name = o.get("f", font)
            fo.size = Pt(o.get("s", size))
            fo.bold = o.get("b", bold)
            fo.italic = o.get("i", False)
            fo.color.rgb = rgb(o.get("c", color))
    if nm:
        name(tb, nm)
    return tb


def box(slide, x, y, w, h, fill, line=None, shape=MSO_SHAPE.ROUNDED_RECTANGLE, nm=None, adj=0.08):
    s = slide.shapes.add_shape(shape, Inches(x), Inches(y), Inches(w), Inches(h))
    if shape == MSO_SHAPE.ROUNDED_RECTANGLE:
        s.adjustments[0] = adj
    if fill is None:
        s.fill.background()
    else:
        s.fill.solid()
        s.fill.fore_color.rgb = rgb(fill)
    if line is None:
        s.line.fill.background()
    else:
        s.line.color.rgb = rgb(line)
        s.line.width = Pt(1)
    s.shadow.inherit = False
    if nm:
        name(s, nm)
    return s


def title(slide, t, color=INK):
    text(slide, 0.6, 0.38, 12.1, 0.8, [t], size=30, color=color, font=HEAD, bold=True,
         anchor=MSO_ANCHOR.MIDDLE)


def pic(slide, f, x, y, w=None, h=None, nm=None):
    kw = {}
    if w:
        kw["width"] = Inches(w)
    if h:
        kw["height"] = Inches(h)
    p = slide.shapes.add_picture(os.path.join(B, f), Inches(x), Inches(y), **kw)
    if nm:
        name(p, nm)
    return p


def F(t, **o):                                   # formula run
    o.setdefault("f", MATH)
    return (t, o)


def R(t, **o):
    return (t, o)


def notes(slide, t):
    slide.notes_slide.notes_text_frame.text = t


def circle_num(slide, x, y, d, label, fill=ORANGE, size=16):
    c = box(slide, x, y, d, d, fill, shape=MSO_SHAPE.OVAL)
    tf = c.text_frame
    for side in ("left", "right", "top", "bottom"):
        setattr(tf, f"margin_{side}", 0)
    tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    p = tf.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    r = p.add_run()
    r.text = label
    r.font.size = Pt(size)
    r.font.bold = True
    r.font.name = BODY
    r.font.color.rgb = rgb(WHITE)
    return c


def interactive(slide, sid, buttons, readout):
    x, y, w, h = PA
    pic(slide, f"fig_{sid}.png", x, y, w=w, h=h, nm=f"static_{sid}")
    box(slide, x, y, w, h, None, shape=MSO_SHAPE.RECTANGLE, nm=f"pa_{sid}")
    bw, gap = 1.45, 0.14
    for i, (label, macro) in enumerate(buttons):
        b = box(slide, x + i * (bw + gap), y + h + 0.22, bw, 0.48, INK, nm=f"btn_{macro}", adj=0.3)
        tf = b.text_frame
        tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        for side in ("left", "right", "top", "bottom"):
            setattr(tf, f"margin_{side}", Inches(0.03))
        p = tf.paragraphs[0]
        p.alignment = PP_ALIGN.CENTER
        r = p.add_run()
        r.text = label
        r.font.size = Pt(14)
        r.font.bold = True
        r.font.name = BODY
        r.font.color.rgb = rgb(WHITE)
    box(slide, 0.6, 5.0, 3.95, 1.95, PALE)
    text(slide, 0.78, 5.08, 3.6, 0.3, ["LIVE-WERTE"], size=11, color=ORANGE, bold=True)
    text(slide, 0.78, 5.38, 3.65, 1.5, readout.split("\n"), size=13, color=INK,
         space=3, nm=f"txt_{sid}")


def left(slide, paras, y=1.35, h=3.5, size=15):
    text(slide, 0.6, y, 3.95, h, paras, size=size, space=8)


def de(v, d=3):
    s = f"{v:.{d}f}".rstrip("0").rstrip(".").replace(".", ",")
    return s.replace("-", "−")


# ======================================================================
# 1 title
s = prs.slides.add_slide(BLANK)
bg(s, INK)
pic(s, "fig_hero.png", 7.0, 1.3, w=5.9)
text(s, 0.7, 1.7, 6.4, 0.4, ["DSP · ERGÄNZUNG ZU BLATT 3"], size=14, color=ORANGE, bold=True)
text(s, 0.7, 2.15, 6.4, 2.2, ["Von der Impulsantwort zur Faltung"], size=40, color=WHITE,
     font=HEAD, bold=True)
text(s, 0.7, 4.6, 6.0, 1.2, ["Warum y = x * h gilt – diskret und kontinuierlich, mit "
                              "realen Impulsen und unendlich langen Signalen"],
     size=18, color=ICE)
text(s, 0.7, 6.3, 6.0, 0.5, ["Interaktive Folien: Buttons in der Bildschirmpräsentation "
                             "(Makros, siehe letzte Folie)"], size=12, color=GRAY)
notes(s, "Bild rechts: Eingang (hellblau), Antworten auf die einzelnen Pulse (orange) "
         "und ihre Summe (weiß). Genau das ist die Faltung.")

# 2 roadmap
s = prs.slides.add_slide(BLANK)
title(s, "Sechs Fragen, eine Idee: zerlegen – einzeln antworten – addieren")
Q = [
    ("Wie kommt man von h zur Antwort auf beliebiges x?", "y[n] = Σ x[k]·h[n−k]", "Folien 4–6"),
    ("Warum ist das so?", "Linearität + Zeitinvarianz", "Folien 3–4"),
    ("Wie sieht die Antwort auf einen realen Impuls aus?", "geglättetes h: g_T = p_T * h", "Folie 7"),
    ("Kann man damit beliebige Antworten berechnen?", "ja – als Summe von Pulsantworten", "Folien 8–9"),
    ("Wo sind die Grenzen?", "Nichtlinearität, Zeitvarianz, Bandbreite, Rauschen", "Folie 10"),
    ("Diskret, kontinuierlich, unendlich lang?", "Σ → ∫,  Konvergenz,  Rekursion", "Folien 11–15"),
]
for i, (q, a, f) in enumerate(Q):
    cx, cy = 0.6 + (i % 3) * 4.1, 1.55 + (i // 3) * 2.75
    box(s, cx, cy, 3.85, 2.45, PALE)
    circle_num(s, cx + 0.25, cy + 0.25, 0.55, str(i + 1))
    text(s, cx + 0.95, cy + 0.22, 2.75, 1.0, [q], size=16, bold=True, anchor=MSO_ANCHOR.MIDDLE)
    text(s, cx + 0.25, cy + 1.2, 3.4, 0.75, [[F(a, c=TEAL, s=15)]])
    text(s, cx + 0.25, cy + 2.0, 3.4, 0.3, [f], size=12, color=MUTED)
notes(s, "Die Antworten auf alle sechs Fragen folgen aus derselben Idee: "
         "Eingang in Impulse zerlegen, Antwort auf einen Impuls kennen, Antworten addieren.")

# 3 LTI
s = prs.slides.add_slide(BLANK)
title(s, "Zwei Annahmen genügen: linear und zeitinvariant")
cards = [
    ("Impulsantwort", [F("h[n] = S{δ[n]}")], "Die Antwort auf den Einheitsimpuls δ[n] (1 bei n = 0, sonst 0)."),
    ("Zeitinvarianz", [F("S{x[n−k]} = y[n−k]")], "Später anregen → dieselbe Antwort, nur später."),
    ("Linearität", [F("S{a·x₁ + b·x₂} = a·y₁ + b·y₂")], "Skalieren und Addieren gehen durch das System hindurch."),
]
for i, (hd, fm, ex) in enumerate(cards):
    cy = 1.45 + i * 1.85
    box(s, 0.6, cy, 4.6, 1.65, PALE)
    text(s, 0.85, cy + 0.15, 4.2, 0.35, [hd], size=16, bold=True, color=ORANGE)
    text(s, 0.85, cy + 0.52, 4.2, 0.45, [fm], size=18)
    text(s, 0.85, cy + 1.0, 4.2, 0.6, [ex], size=14, color=MUTED)
pic(s, "fig_lti.png", 5.55, 1.4, w=7.2)
notes(s, "Rechts dieselben drei Aussagen als Bild mit h = {1; −1; 0,5} aus Blatt 3.1 a. "
         "Mehr wird für die Faltung nicht gebraucht – keine Differentialgleichung, kein Modell.")

# 4 derivation
s = prs.slides.add_slide(BLANK)
title(s, "Warum? Jedes Signal ist eine Summe von Impulsen")
steps = [
    ("Zerlegen", "x[n] = Σ x[k]·δ[n−k]"),
    ("Zeitinvarianz", "δ[n−k]  →  h[n−k]"),
    ("Homogenität", "x[k]·δ[n−k]  →  x[k]·h[n−k]"),
    ("Additivität", "Σ …  →  Σ …"),
]
for i, (hd, fm) in enumerate(steps):
    cy = 1.45 + i * 0.95
    circle_num(s, 0.6, cy + 0.08, 0.5, str(i + 1), fill=TEAL, size=15)
    text(s, 1.3, cy, 3.9, 0.35, [hd], size=14, bold=True, color=TEAL)
    text(s, 1.3, cy + 0.33, 4.2, 0.45, [[F(fm, s=17)]])
box(s, 0.6, 5.3, 4.75, 1.65, INK)
text(s, 0.85, 5.42, 4.3, 0.5, [[F("y[n] = Σ x[k]·h[n−k] = (x * h)[n]", s=17, c=WHITE)]])
text(s, 0.85, 5.98, 4.3, 0.85, ["Das System „kennt“ nur h. Alles andere ist Buchhaltung: "
                               "verschieben, skalieren, addieren."], size=14, color=ICE)
pic(s, "fig_deriv.png", 5.7, 1.35, w=7.05)
notes(s, "Links Zeile für Zeile die Abtastwerte als Impulse, rechts deren Antworten. "
         "Die Summe der linken Spalte ist x, die der rechten ist y. "
         "Das ist zugleich die Produkttabelle aus Blatt 3.1 a: Zeile k = x[k]·h[n−k].")

# 5 interactive B
s = prs.slides.add_slide(BLANK)
title(s, "Eingangssicht: jeder Abtastwert erzeugt ein Echo")
left(s, ["Jeder Wert x[k] startet bei n = k eine Kopie der Impulsantwort, skaliert mit x[k].",
         [F("y[n] = Σ x[k]·h[n−k]", s=17, c=TEAL)],
         "Die Ausgabe ist die Summe aller Echos. Klicken Sie die Echos einzeln dazu.",
         [R("Zeilen der Tabelle aus Blatt 3.1 a", c=MUTED, s=13)]])
interactive(s, "B", [("← Echo", "B_Prev"), ("Echo →", "B_Next"), ("Alle", "B_All"),
                     ("Neu", "B_Reset"), ("h wechseln", "B_H")],
            "y[n] = x[0]·h[n] + x[1]·h[n−1] + x[2]·h[n−2] + x[3]·h[n−3]\n"
            "4 von 4 Echos addiert → fertig: y = x * h\n"
            "Impulsantwort: h = {1; −1; 0,5}  (Blatt 3.1 a)")
notes(s, "Mit 'Neu' starten, dann Echo für Echo. Fragen: Wo beginnt das Echo von x[2]? "
         "(gar keins, x[2] = 0). Wie lang ist y? (4 + 3 − 1 = 6). "
         "'h wechseln': MA4 und EMA aus Blatt 3.1 b.")

# 6 interactive A
s = prs.slides.add_slide(BLANK)
title(s, "Ausgangssicht: spiegeln, schieben, multiplizieren, summieren")
left(s, ["Für ein festes n ist h[n−k] als Funktion von k die gespiegelte und nach n "
         "geschobene Impulsantwort.",
         [F("y[n] = Σ x[k]·h[n−k]", s=16, c=TEAL)],
         [F("       = Σ h[m]·x[n−m]", s=16, c=TEAL)],
         "Gleiche Summe wie links – nur spaltenweise statt zeilenweise gelesen.",
         [R("So rechnet ein Filter in Echtzeit: ein Ausgangswert pro Takt.", c=MUTED, s=13)]])
interactive(s, "A", [("← n", "A_Prev"), ("n →", "A_Next"), ("Abspielen", "A_Play"),
                     ("h wechseln", "A_H")],
            "y[2] = x[0]h[2] + x[1]h[1] + x[2]h[0]\n"
            "= 1·0,5 + 2·(−1) + 0·1 = −1,5\n"
            "Impulsantwort: h = {1; −1; 0,5}  (Blatt 3.1 a)")
notes(s, "Warum spiegeln? h[n−k]: das Echo, das x[k] zum Zeitpunkt n beiträgt. "
         "Der jüngste Eingang x[n] wird mit h[0] gewichtet, der älteste mit dem Ende von h. "
         "Substitution m = n−k zeigt die Kommutativität x*h = h*x.")

# 7 interactive C
s = prs.slides.add_slide(BLANK)
title(s, "Der reale Impuls: kurz, aber nicht unendlich kurz")
Tc = 0.2
tt = -0.5 + 0.01 * np.arange(551)
eC = np.max(abs(g_pulse(tt, Tc) - h2(tt))) / np.max(abs(h2(tt)))
left(s, ["Ein echter Testimpuls hat Breite T und Fläche 1. Die gemessene Antwort ist selbst "
         "eine Faltung:",
         [F("g_T = p_T * h", s=16, c=TEAL)],
         [F("g_T(t) = (1/T)·∫ h(τ) dτ", s=16, c=TEAL), R("  über [t−T, t]", s=13, c=MUTED)],
         "also h, über die Pulsbreite gemittelt: glatter und um ≈ T/2 verzögert.",
         [R("g_T → h für ", b=True), F("T ≪ 1/f_max", b=True), R("  (hier f₀ = 1 Hz)", b=True)]], size=15)
interactive(s, "C", [("← T kürzer", "C_Shorter"), ("T länger →", "C_Longer")],
            f"T = 0,2 s,   T·f₀ = 0,2\n"
            f"max|g_T − h| = {100 * eC:.0f} % von max|h|\n"
            f"g_T ist h, über T gemittelt: Verzögerung ≈ T/2")
notes(s, "System: Tiefpass 2. Ordnung, f0 = 1 Hz, Dämpfung 0,15 – klingt deutlich nach. "
         "T von 2 s bis 0,02 s durchklicken. Bei T = 1/f0 ist von der Schwingung fast nichts "
         "mehr zu sehen, bei T = 0,02 s liegen g_T und h praktisch aufeinander. "
         "Ein Hammerschlag, ein kurzer Spannungspuls, ein Klatschen im Raum: alles p_T.")

# 8 interactive D
s = prs.slides.add_slide(BLANK)
title(s, "Beliebige Antworten aus der realen Pulsantwort")
TD = 0.5
td = -0.5 + 0.01 * np.arange(751)
yE, yT = y_stair(td, 0.01), y_stair(td, TD)
eD = np.max(abs(yE - yT)) / np.max(abs(yE))
left(s, ["Zerlege x in Pulse der Breite T:",
         [F("x(t) ≈ Σ x(kT)·T·p_T(t−kT)", s=15, c=TEAL)],
         "Linear + zeitinvariant →",
         [F("y(t) ≈ Σ x(kT)·T·g_T(t−kT)", s=15, c=TEAL)],
         "Für die Treppe ist das exakt. Der Fehler kommt nur daher, dass x keine Treppe ist – "
         "er hängt vom Eingang ab, nicht nur von h."], size=14)
interactive(s, "D", [("← T kürzer", "D_Shorter"), ("T länger →", "D_Longer")],
            f"T = 0,5 s  →  8 Pulsantworten addiert\n"
            f"max. Fehler = {100 * eD:.0f} % von max|y|\n"
            f"(exakt für die Treppe, Fehler = Treppe − x)")
notes(s, "Der Eingang enthält einen Anteil bei 0,75 Hz, nahe der Resonanz bei 1 Hz. "
         "Bei T = 1 s wird er von der Treppe fast nicht erfasst – die Ausgangsschwingung fehlt. "
         "Antwort auf Frage 4: ja, man kann mit der realen Pulsantwort rechnen, solange T klein "
         "gegen die schnellsten Änderungen von x UND von h ist.")

# 9 step response
s = prs.slides.add_slide(BLANK)
title(s, "In der Praxis oft besser: die Sprungantwort messen")
cards = [
    ("Zusammenhang", [F("s(t) = ∫ h(τ) dτ  (0 … t)   ⇔   h = ds/dt", s=14)]),
    ("Der reale Puls sind zwei Sprünge", [F("T·g_T(t) = s(t) − s(t−T)", s=16)]),
    ("Beliebiger Eingang (Duhamel)", [F("y = Σ Δx[k]·s(t−kT) → ∫ x′(τ)·s(t−τ) dτ", s=14)]),
]
for i, (hd, fm) in enumerate(cards):
    cx = 0.6 + i * 4.1
    box(s, cx, 1.4, 3.85, 1.3, PALE)
    text(s, cx + 0.22, 1.5, 3.5, 0.35, [hd], size=15, bold=True, color=ORANGE)
    text(s, cx + 0.22, 1.95, 3.5, 0.65, [fm])
pic(s, "fig_step.png", 0.6, 2.95, w=8.2)
text(s, 9.15, 3.05, 3.6, 3.8,
     ["Ein Sprung hat viel Energie bei kleiner Amplitude und ist leicht zu erzeugen "
      "(Schalter, Sollwertsprung).",
      "Blatt 3.3: Drehzahlsprung am Drehregler → Sprungantwort von MA und EMA.",
      [R("Nachteil: h = ds/dt verstärkt das Messrauschen.", c=MUTED)]], size=15, space=10)
notes(s, "Antwort auf Frage 3/4 aus Sicht der Praxis: Eine lineare Abbildung ist durch die "
         "Antwort auf irgendein bekanntes Testsignal festgelegt, solange dessen Spektrum dort, "
         "wo es interessiert, nicht null ist. Sprung: Spektrum 1/(j2πf), nie null.")

# 10 limits
s = prs.slides.add_slide(BLANK)
title(s, "Wo sind die Grenzen?")
pic(s, "fig_limits.png", 0.6, 1.3, w=12.1)
lim = [
    (RED, "Linearität", "Ein kurzer Puls braucht die Höhe 1/T. Sättigung, Totzone und "
                        "Quantisierung verletzen die Superposition."),
    (ORANGE, "Bandbreite", "P_T(f) = sinc(fT) ist bei f = k/T null. Diese Frequenzen sind in g_T "
                           "verloren; H = G/P_T zurückrechnen ist dort instabil."),
    (VIOLET, "Rauschen", "Die Pulsfläche ist durch die Maximalamplitude begrenzt: SNR wächst mit T. "
                         "h muss bis zum Abklingen gemessen werden."),
    (TEAL, "Zeitinvarianz", "Ändert sich das System (Temperatur, Drehzahl), gilt "
                            "y[n] = Σ x[k]·h[n, k]. Blatt 3.3 d: Zahn- vs. Zeitbereich."),
]
for i, (c, hd, tx) in enumerate(lim):
    cx = 0.6 + i * 3.06
    box(s, cx, 4.55, 2.9, 2.4, PALE)
    text(s, cx + 0.2, 4.65, 2.55, 0.35, [hd], size=16, bold=True, color=c)
    text(s, cx + 0.2, 5.05, 2.55, 1.85, [tx], size=13)
notes(s, "Links: Ausgang auf ±2,5 begrenzt, gemessen mit T = 0,05 s. Mitte: Spektrum des "
         "Rechteckpulses gegen |H(f)|. Rechts: gleicher Rauschpegel, kurzer Puls mit "
         "Amplitudengrenze 10 hat nur Fläche 0,2 – nach Normierung fünffaches Rauschen. "
         "Weitere Grenzen: Anfangszustand muss null sein, h muss abgeklungen sein (Länge).")

# 11 continuous derivation
s = prs.slides.add_slide(BLANK)
title(s, "Kontinuierlich: aus der Summe wird ein Integral")
chain = [
    ("Pulse der Breite Δ", "x(t) ≈ Σ x(kΔ)·p_Δ(t−kΔ)·Δ"),
    ("Antwort (LTI)", "y(t) ≈ Σ x(kΔ)·g_Δ(t−kΔ)·Δ"),
    ("Grenzwert Δ → 0", "p_Δ → δ,   g_Δ → h,   Σ·Δ → ∫"),
]
for i, (hd, fm) in enumerate(chain):
    cy = 1.45 + i * 1.0
    circle_num(s, 0.6, cy + 0.08, 0.5, str(i + 1), fill=TEAL, size=15)
    text(s, 1.3, cy, 4.6, 0.35, [hd], size=14, bold=True, color=TEAL)
    text(s, 1.3, cy + 0.33, 4.6, 0.45, [[F(fm, s=17)]])
box(s, 0.6, 4.6, 5.3, 1.0, INK)
text(s, 0.85, 4.72, 4.9, 0.8, [[F("y(t) = ∫ x(τ)·h(t−τ) dτ = (x * h)(t)", s=20, c=WHITE)]],
     anchor=MSO_ANCHOR.MIDDLE)
text(s, 0.6, 5.85, 5.3, 1.1, ["Der Dirac-Impuls δ(t) – Fläche 1, Breite 0 – ist der Grenzwert "
                              "des realen Pulses. Messen lässt er sich nicht, nur annähern "
                              "(Folie 7)."], size=14, color=MUTED)
pic(s, "fig_riemann.png", 6.25, 1.5, w=6.5)
text(s, 6.25, 4.2, 6.5, 2.4,
     ["Jeder orange Streifen ist ein Puls x(kΔ)·Δ·p_Δ(t−kΔ). Seine Antwort ist "
      "x(kΔ)·Δ·g_Δ(t−kΔ) – genau die Summe aus Folie 8.",
      "Mit Δ → 0 wird aus der Riemann-Summe das Faltungsintegral."], size=15, space=10)
notes(s, "Die Herleitung ist dieselbe wie diskret. Neu ist nur der Grenzwert. "
         "Deshalb hat δ(t) die Einheit 1/s und h(t) die Einheit (Ausgang/Eingang)/s.")

# 12 interactive E
s = prs.slides.add_slide(BLANK)
title(s, "Kontinuierlich: spiegeln, schieben, Fläche bilden")
left(s, ["Eingang: Rechteck von 0 bis 2 s. System: RC-Glied",
         [F("h(t) = (1/τ)·e^(−t/τ),  τ = 0,5 s", s=15, c=TEAL)],
         "Für jedes t ist y(t) die Fläche unter x(τ)·h(t−τ).",
         [R("t < 0: ", b=True), R("keine Überlappung")],
         [R("0 ≤ t ≤ 2: ", b=True), R("Laden, 1 − e^(−t/τ)")],
         [R("t > 2: ", b=True), R("Entladen, Abklingen mit τ")]], size=15)
interactive(s, "E", [("← t", "E_Prev"), ("t →", "E_Next"), ("Abspielen", "E_Play"),
                     ("Neu", "E_Reset")],
            f"t = 1,25 s\n"
            f"y(t) = ∫ x(τ)·h(t−τ) dτ = {de(float(y_rc(1.25)))}\n"
            f"Laden: 1 − e^(−t/τc)")
notes(s, "Gleiche Bewegung wie auf Folie 6, nur mit Flächen statt Summen. "
         "Die Fallunterscheidung der Integrationsgrenzen ist genau die Frage: "
         "wo überlappen x(τ) und h(t−τ)?")

# 13 discrete vs continuous
s = prs.slides.add_slide(BLANK)
title(s, "Diskret und kontinuierlich im Vergleich")
rows = [
    ("", "diskret", "kontinuierlich"),
    ("Impuls", "δ[n]: Wert 1 bei n = 0", "δ(t): Fläche 1, Breite 0"),
    ("Faltung", "Σ x[k]·h[n−k]", "∫ x(τ)·h(t−τ) dτ"),
    ("Impuls messen", "exakt möglich (eine Zahl)", "nur angenähert (p_T)"),
    ("Rechnen", "N Multiplikationen pro Wert (FIR)", "analytisch oder numerisch"),
    ("Verbindung", "h[n] = T_s·h(nT_s)", "y(nT_s) ≈ Σ x[k]·h[n−k]"),
]
tbl = s.shapes.add_table(len(rows), 3, Inches(0.6), Inches(1.45), Inches(6.6), Inches(4.2)).table
tbl.columns[0].width, tbl.columns[1].width, tbl.columns[2].width = Inches(1.6), Inches(2.5), Inches(2.5)
for r, row in enumerate(rows):
    for c, v in enumerate(row):
        cell = tbl.cell(r, c)
        cell.fill.solid()
        cell.fill.fore_color.rgb = rgb(INK if r == 0 else (PALE if r % 2 else WHITE))
        cell.margin_left = cell.margin_right = Inches(0.1)
        cell.vertical_anchor = MSO_ANCHOR.MIDDLE
        tf = cell.text_frame
        tf.word_wrap = True
        p = tf.paragraphs[0]
        run = p.add_run()
        run.text = v
        run.font.size = Pt(14)
        run.font.bold = r == 0 or c == 0
        run.font.name = MATH if (r in (2, 5) and c > 0) else BODY
        run.font.color.rgb = rgb(WHITE if r == 0 else INK)
pic(s, "fig_sampling.png", 7.55, 1.45, w=5.2)
text(s, 7.55, 4.35, 5.2, 2.5,
     ["Abtasten mit T_s: Das Integral wird zur Riemann-Summe mit Schrittweite T_s, "
      "daher der Faktor T_s in h[n].",
      [R("Gut, wenn x und h zwischen den Abtastwerten glatt sind – "
         "dieselbe Bedingung wie beim realen Impuls.", c=MUTED)]], size=15, space=10)
notes(s, "Diskret ist der Impuls ein ganz normales Signal – deshalb ist h[n] im Rechner "
         "exakt bestimmbar (Blatt 3.1 b: Impuls ins Filter, h ablesen).")

# 14 infinite inputs
s = prs.slides.add_slide(BLANK)
title(s, "Unendlich lange Eingänge")
items = [
    ("Kausal: nur die Vergangenheit zählt", "y[n] = Σ x[k]·h[n−k],   k = −∞ … n"),
    ("Existiert, wenn h absolut summierbar ist (BIBO)", "Σ|h[n]| < ∞  ⇒  |y[n]| ≤ max|x|·Σ|h|"),
    ("FIR: endliches Gedächtnis", "y[n] = Σ h[m]·x[n−m],   m = 0 … N−1"),
    ("IIR: unendlich viele Terme, aber rekursiv", "y[n] = a·y[n−1] + (1−a)·x[n]"),
]
for i, (hd, fm) in enumerate(items):
    cy = 1.4 + i * 1.12
    text(s, 0.6, cy, 6.6, 0.35, [hd], size=15, bold=True, color=ORANGE)
    text(s, 0.6, cy + 0.38, 6.6, 0.5, [[F(fm, s=17)]])
box(s, 0.6, 5.95, 6.4, 1.0, PALE)
text(s, 0.8, 6.02, 6.1, 0.9,
     ["Man braucht nie den ganzen Eingang: Jeder Ausgangswert kommt aus den letzten N "
      "Eingängen bzw. dem letzten Ausgang. Streaming statt Speichern."], size=14)
pic(s, "fig_infinite.png", 7.4, 1.4, w=5.35)
text(s, 7.4, 4.4, 5.35, 2.5,
     ["Abschneiden nach M Termen kostet höchstens max|x| · (Summe der |h[n]| ab n = M), beim EMA also a^M.",
      "Kontinuierlich gilt dasselbe mit ∫|h(t)| dt < ∞."], size=15, space=10)
notes(s, "Instabiles Gegenbeispiel: a > 1 – dann wächst h, die Summe divergiert schon für "
         "x = Sprung. Die EMA-Rekursion berechnet die unendliche Faltungssumme exakt mit zwei "
         "Multiplikationen (Blatt 3.1 b).")

# 15 interactive F
s = prs.slides.add_slide(BLANK)
title(s, "Eingeschaltete Schwingung: Einschwingen und Eigenfunktion")
om = np.pi / 8
Hv = H(om, 0)
left(s, ["Läuft der Eingang schon ewig, kommt eine Schwingung gleicher Frequenz heraus:",
         [F("e^(jΩn) → H(Ω)·e^(jΩn)", s=15, c=TEAL)],
         [F("H(Ω) = Σ h[k]·e^(−jΩk)", s=15, c=TEAL)],
         "Bei n = 0 eingeschaltet: Einschwingen, solange h in den „leeren“ Bereich n < 0 "
         "reicht – FIR nach N−1 Werten vorbei, IIR klingt ab."], size=14)
interactive(s, "F", [("← Ω kleiner", "F_Down"), ("Ω größer →", "F_Up"),
                     ("Filter wechseln", "F_Filter")],
            f"Ω = π/8:   |H(Ω)| = {de(abs(Hv))},   φ = {np.degrees(np.angle(Hv)):.0f}°".replace("-", "−") + "\n"
            "EMA: Einschwingen klingt mit 0,8ⁿ ab\n"
            "Ab −∞ eingeschaltet: nur die gestrichelte Kurve")
notes(s, "Ω = 0 ist der Sprung (Blatt 3.3). Bei MA5 und Ω = 2π/5 ist |H| = 0: "
         "nach dem Einschwingen ist der Ausgang exakt null – vgl. Blatt 3.1 d, "
         "Nullstellen von MA30 bei geraden Motorordnungen.")

# 16 summary
s = prs.slides.add_slide(BLANK)
bg(s, INK)
title(s, "Auf einen Blick", color=WHITE)
summ = [
    ("Idee", "zerlegen in Impulse – je eine Antwort h – addieren"),
    ("Diskret", "y[n] = Σ x[k]·h[n−k]"),
    ("Kontinuierlich", "y(t) = ∫ x(τ)·h(t−τ) dτ"),
    ("Realer Impuls", "g_T = p_T * h  ≈  h  für T ≪ 1/f_max"),
    ("Sprungantwort", "h = ds/dt,   T·g_T(t) = s(t) − s(t−T)"),
    ("Grenzen", "nur LTI · Bandbreite des Testsignals · Rauschen"),
    ("Unendlich lang", "Σ|h| < ∞,   Rekursion statt Summe,   e^(jΩn) → H(Ω)·e^(jΩn)"),
]
for i, (k, v) in enumerate(summ):
    cy = 1.5 + i * 0.75
    text(s, 0.8, cy, 2.6, 0.5, [k], size=17, bold=True, color=ORANGE)
    text(s, 3.5, cy, 9.2, 0.5, [[F(v, s=19, c=WHITE)]])
notes(s, "Zum Mitnehmen.")

# 17 macros
s = prs.slides.add_slide(BLANK)
title(s, "Interaktive Folien einrichten (einmalig)")
st = [
    ("Modul importieren", "Alt+F11 → Datei → Datei importieren… → Faltung.bas"),
    ("Setup ausführen", "Cursor in „Sub Setup“ setzen → F5. Verknüpft alle Buttons und "
                        "zeichnet die Folien 5, 6, 7, 8, 12 und 15."),
    ("Als .pptm speichern", "Datei → Speichern unter → PowerPoint-Präsentation mit Makros"),
    ("Präsentieren", "In der Bildschirmpräsentation die dunklen Buttons klicken. "
                     "Ohne Makros zeigen die Folien ein festes Standardbild."),
]
for i, (hd, tx) in enumerate(st):
    cy = 1.45 + i * 1.3
    circle_num(s, 0.6, cy + 0.05, 0.6, str(i + 1))
    text(s, 1.45, cy, 6.3, 0.4, [hd], size=17, bold=True)
    text(s, 1.45, cy + 0.42, 6.3, 0.8, [tx], size=15, color=MUTED)
box(s, 8.2, 1.45, 4.55, 5.2, PALE)
text(s, 8.45, 1.6, 4.1, 5.0,
     [[R("Hinweise", b=True, c=ORANGE, s=16)],
      "Makros müssen erlaubt sein (Datei → Optionen → Trust Center).",
      "Aktualisiert sich das Bild in der Präsentation nicht, im Modul "
      "REFRESH_IN_SHOW = True setzen.",
      "ResetStatic löscht die gezeichneten Formen und zeigt wieder die Standardbilder.",
      "Die Formeln stehen auch in model.py (Python), das die Standardbilder erzeugt."],
     size=14, space=10)
notes(s, "PowerPoint für Mac: Extras → Makro → Visual Basic-Editor, dann Datei → Importieren.")

out = os.path.join(HERE, "Faltung_LTI.pptx")
prs.save(out)
print("saved", out)
