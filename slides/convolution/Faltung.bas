Attribute VB_Name = "Faltung"
' ===========================================================================
'  Faltung.bas - interactive slides of Faltung_LTI.pptx
'
'  Install (once):  Alt+F11 -> File -> Import File... -> Faltung.bas
'                   put the cursor into "Setup" -> F5
'                   save the deck as .pptm (PowerPoint macro-enabled)
'
'  Setup links every shape named "btn_<Macro>" to the macro <Macro> and
'  draws all interactive slides once. The interactive slides are found by
'  their invisible plot area "pa_<ID>"; the PNG "static_<ID>" (default
'  state, for use without macros) is hidden as soon as a slide is drawn.
'  All drawn shapes are named "dyn_..." and are deleted before a redraw.
'  ResetStatic deletes them again and shows the PNGs.
'
'  The math is the same as in model.py (Python, generates the PNGs).
'  Source is pure ASCII; special characters come from U() via ChrW.
' ===========================================================================
Option Explicit

' If the slide show does not repaint after a click, set this to True.
Private Const REFRESH_IN_SHOW As Boolean = False

Private Const PI As Double = 3.14159265358979
Private Const ZETA As Double = 0.15          ' 2nd order system, f0 = 1 Hz
Private Const TAU_RC As Double = 0.5         ' RC time constant (slide E)
Private Const A_EMA As Double = 0.8          ' EMA of slide F
Private Const K_MIN As Long = -3             ' axis of the discrete slides
Private Const K_MAX As Long = 12

Private Type TFrame
    L As Single
    T As Single
    W As Single
    H As Single
    x0 As Double
    x1 As Double
    y0 As Double
    y1 As Double
End Type

Private gSeq As Long

' ============================================================ public: setup
Public Sub Setup()
    Dim s As Slide, sh As Shape, n As Long
    For Each s In ActivePresentation.Slides
        For Each sh In s.Shapes
            If Left$(sh.Name, 4) = "btn_" Then
                With sh.ActionSettings(ppMouseClick)
                    .Action = ppActionRunMacro
                    .Run = Mid$(sh.Name, 5)
                End With
                n = n + 1
            End If
        Next sh
    Next s
    DrawAll
    MsgBox n & " Buttons verkn" & ChrW(252) & "pft, interaktive Folien gezeichnet." & vbCrLf & _
           "Jetzt als .pptm speichern (PowerPoint-Pr" & ChrW(228) & "sentation mit Makros).", _
           vbInformation, "Faltung"
End Sub

Public Sub DrawAll()
    B_Draw
    A_Draw
    C_Draw
    D_Draw
    E_Draw
    F_Draw
End Sub

Public Sub ResetStatic()
    Dim s As Slide, i As Long
    For Each s In ActivePresentation.Slides
        For i = s.Shapes.Count To 1 Step -1
            If Left$(s.Shapes(i).Name, 4) = "dyn_" Then
                s.Shapes(i).Delete
            ElseIf Left$(s.Shapes(i).Name, 7) = "static_" Then
                s.Shapes(i).Visible = msoTrue
            End If
        Next i
    Next s
End Sub

' ===================================================== slide B: echo view
Public Sub B_Next()
    StepTag "B", "K", 1, 0, 4, 0
    B_Draw
End Sub
Public Sub B_Prev()
    StepTag "B", "K", -1, 0, 4, 0
    B_Draw
End Sub
Public Sub B_All()
    SetTag "B", "K", 4
    B_Draw
End Sub
Public Sub B_Reset()
    SetTag "B", "K", 0
    B_Draw
End Sub
Public Sub B_H()
    StepTag "B", "HS", 1, 0, 2, 0, True
    B_Draw
End Sub

Public Sub B_Draw()
    Dim sld As Slide, L As Single, T As Single, W As Single, H As Single
    Dim f As TFrame, x() As Double, hh() As Double, y() As Double, yK() As Double
    Dim nh As Long, ny As Long, K As Long, hs As Long, i As Long, j As Long
    Dim lab As String, lo As Double, hi As Double, s As String
    Set sld = FindSlide("B")
    If sld Is Nothing Then Exit Sub
    K = GetTag(sld, "K", 4)
    hs = GetTag(sld, "HS", 0)
    GetX x
    GetH hs, hh, nh, lab
    ny = 4 + nh - 1
    Conv x, hh, nh, y
    ReDim yK(0 To ny - 1)
    For i = 0 To K - 1
        For j = 0 To nh - 1
            yK(i + j) = yK(i + j) + x(i) * hh(j)
        Next j
    Next i
    Prepare sld, "B", L, T, W, H

    PanelFrame f, L, T, W, H, 0, 1 / 3, K_MIN - 0.5, K_MAX + 0.5, -1.5, 2.5
    DrawAxes sld, f, U("x[k] {-} jeder Abtastwert ist ein skalierter, verschobener Impuls"), cInk, Array(-1, 0, 1, 2)
    For i = 0 To 3
        If i < K Then
            Stem sld, f, i, x(i), EchoCol(i), False, 8
        Else
            Stem sld, f, i, x(i), cLight, True, 8
        End If
    Next i

    PanelFrame f, L, T, W, H, 1 / 3, 2 / 3, K_MIN - 0.5, K_MAX + 0.5, -2.5, 2.5
    DrawAxes sld, f, U("Echos x[k]{.}h[n{-}k]  (jeweils eine Impulsantwort)"), cInk, Array(-2, 0, 2)
    For i = 0 To K - 1
        For j = 0 To nh - 1
            If Abs(x(i) * hh(j)) > 0.000000001 Then
                Stem sld, f, i + j + (i - 1.5) * 0.13, x(i) * hh(j), EchoCol(i), False, 6
            End If
        Next j
    Next i

    YRange y, ny, lo, hi
    PanelFrame f, L, T, W, H, 2 / 3, 1, K_MIN - 0.5, K_MAX + 0.5, lo, hi
    DrawAxes sld, f, "Summe der ersten " & K & " Echos = y[n]", cInk, Array(-1, 0, 1)
    For i = 0 To ny - 1
        Stem sld, f, i, y(i), cLight, True, 8
    Next i
    For i = 0 To ny - 1
        If i < K - 1 + nh And K > 0 Then Stem sld, f, i, yK(i), cInk, False, 8
    Next i
    XTicks sld, f, K_MIN, K_MAX, 1, "n"

    If K = 0 Then
        s = "Noch kein Echo addiert: y[n] = 0"
    Else
        s = "y[n] = "
        For i = 0 To K - 1
            If i > 0 Then s = s & " + "
            s = s & "x[" & i & "]{.}h[n" & IIf(i = 0, "", "{-}" & i) & "]"
        Next i
        s = s & vbCr & K & " von 4 Echos addiert"
        If K = 4 Then s = s & " {->} fertig: y = x * h"
    End If
    s = s & vbCr & lab
    Readout sld, "B", U(s)
    Repaint sld
End Sub

' =================================================== slide A: flip & slide
Public Sub A_Next()
    StepTag "A", "N", 1, -2, 11, 2
    A_Draw
End Sub
Public Sub A_Prev()
    StepTag "A", "N", -1, -2, 11, 2
    A_Draw
End Sub
Public Sub A_H()
    StepTag "A", "HS", 1, 0, 2, 0, True
    A_Draw
End Sub
Public Sub A_Play()
    Dim sld As Slide, n As Long
    Set sld = FindSlide("A")
    If sld Is Nothing Then Exit Sub
    For n = -2 To 11
        sld.Tags.Add "N", CStr(n)
        A_Draw
        Pause 0.6
    Next n
End Sub

Public Sub A_Draw()
    Dim sld As Slide, L As Single, T As Single, W As Single, H As Single
    Dim f As TFrame, x() As Double, hh() As Double, y() As Double
    Dim nh As Long, ny As Long, n As Long, hs As Long, k As Long, j As Long
    Dim kA As Long, kB As Long, lab As String, lo As Double, hi As Double
    Dim s1 As String, s2 As String, yn As Double
    Set sld = FindSlide("A")
    If sld Is Nothing Then Exit Sub
    n = GetTag(sld, "N", 2)
    hs = GetTag(sld, "HS", 0)
    GetX x
    GetH hs, hh, nh, lab
    ny = 4 + nh - 1
    Conv x, hh, nh, y
    kA = n - nh + 1
    If kA < 0 Then kA = 0
    kB = n
    If kB > 3 Then kB = 3
    Prepare sld, "A", L, T, W, H

    PanelFrame f, L, T, W, H, 0, 1 / 3, K_MIN - 0.5, K_MAX + 0.5, -1.5, 2.5
    DrawAxes sld, f, U("x[k]   und   h[n{-}k]  (gespiegelt, nach n = " & Fm(n) & " geschoben)"), cInk, Array(-1, 0, 1, 2)
    For k = 0 To 3
        Stem sld, f, k, x(k), cInk, False, 8
    Next k
    For j = 0 To nh - 1
        Stem sld, f, n - j + 0.15, hh(j), cOrange, False, 8
    Next j
    VMark sld, f, n, cOrange

    yn = 0
    For k = kA To kB
        yn = yn + x(k) * hh(n - k)
    Next k
    PanelFrame f, L, T, W, H, 1 / 3, 2 / 3, K_MIN - 0.5, K_MAX + 0.5, -2.5, 2.5
    DrawAxes sld, f, U("Produkte x[k]{.}h[n{-}k]   {->}   Summe = y[" & Fm(n) & "] = " & Fm(yn)), cTeal, Array(-2, 0, 2)
    For k = kA To kB
        Stem sld, f, k, x(k) * hh(n - k), cTeal, False, 8
    Next k

    YRange y, ny, lo, hi
    PanelFrame f, L, T, W, H, 2 / 3, 1, K_MIN - 0.5, K_MAX + 0.5, lo, hi
    DrawAxes sld, f, "y[n]", cInk, Array(-1, 0, 1)
    For k = 0 To ny - 1
        If k <= n Then
            Stem sld, f, k, y(k), cInk, False, 8
        Else
            Stem sld, f, k, y(k), cLight, True, 8
        End If
    Next k
    If n >= 0 And n < ny Then Stem sld, f, n, y(n), cOrange, False, 11
    XTicks sld, f, K_MIN, K_MAX, 1, "k  bzw.  n"

    If kA > kB Then
        s1 = "n = " & Fm(n) & ": keine " & "{Ue}berlappung"
        s2 = "y[" & Fm(n) & "] = 0"
    Else
        s1 = "y[" & Fm(n) & "] = "
        s2 = "= "
        For k = kA To kB
            If k > kA Then s1 = s1 & " + ": s2 = s2 & " + "
            s1 = s1 & "x[" & k & "]h[" & (n - k) & "]"
            s2 = s2 & Par(x(k)) & "{.}" & Par(hh(n - k))
        Next k
        s2 = s2 & " = " & Fm(yn)
    End If
    Readout sld, "A", U(s1 & vbCr & s2 & vbCr & lab)
    Repaint sld
End Sub

' ================================================ slide C: real impulse
Public Sub C_Shorter()
    StepTag "C", "IT", -1, 0, 6, 3
    C_Draw
End Sub
Public Sub C_Longer()
    StepTag "C", "IT", 1, 0, 6, 3
    C_Draw
End Sub

Public Sub C_Draw()
    Dim sld As Slide, L As Single, T As Single, W As Single, H As Single
    Dim f As TFrame, it As Long, Tp As Double, hp As Double
    Dim tx() As Double, hv() As Double, gv() As Double, i As Long, np As Long
    Dim eMax As Double, hMax As Double, shp As Shape
    Set sld = FindSlide("C")
    If sld Is Nothing Then Exit Sub
    it = GetTag(sld, "IT", 3)
    Tp = TList(it)
    Prepare sld, "C", L, T, W, H

    hp = 1 / Tp
    PanelFrame f, L, T, W, H, 0, 0.3, -0.5, 5, -0.5, 6.5
    DrawAxes sld, f, U("realer Impuls p_T(t): Breite T = " & Fm(Tp) & " s, H{oe}he 1/T = " & Fm(hp) & ", Fl{ae}che 1"), cOrange, Array(0, 3, 6)
    Set shp = sld.Shapes.AddShape(msoShapeRectangle, PX(f, 0), PY(f, hp), _
                                  Max1(PX(f, Tp) - PX(f, 0), 1.5), PY(f, 0) - PY(f, hp))
    shp.Fill.ForeColor.RGB = cOrange
    shp.Fill.Transparency = 0.55
    shp.Line.ForeColor.RGB = cOrange
    shp.Line.Weight = 2
    shp.Shadow.Visible = msoFalse
    TagShape shp
    If hp > 6.5 Then
        Txt sld, PX(f, Tp) + 6, PY(f, 6.2), U("{^}  abgeschnitten, H{oe}he " & Fm(hp)), 12, cOrange, False
    End If

    np = 551
    ReDim tx(1 To np): ReDim hv(1 To np): ReDim gv(1 To np)
    For i = 1 To np
        tx(i) = -0.5 + (i - 1) * 0.01
        hv(i) = H2(tx(i))
        gv(i) = (S2(tx(i)) - S2(tx(i) - Tp)) / Tp
        If Abs(hv(i)) > hMax Then hMax = Abs(hv(i))
        If Abs(gv(i) - hv(i)) > eMax Then eMax = Abs(gv(i) - hv(i))
    Next i
    PanelFrame f, L, T, W, H, 0.3, 1, -0.5, 5, -4, 6
    DrawAxes sld, f, "Antworten", cInk, Array(-4, -2, 0, 2, 4, 6)
    Poly sld, f, tx, hv, np, cInk, 2, True
    Poly sld, f, tx, gv, np, cOrange, 2.6, False
    DrawLegend sld, f, U("- - -  h(t)  (idealer Dirac-Impuls)"), cInk, 0
    DrawLegend sld, f, U("___  g_T(t) = (p_T * h)(t)"), cOrange, 1
    XTicks sld, f, 0, 5, 1, "t / s"

    Readout sld, "C", U("T = " & Fm(Tp) & " s,   T{.}f{_0} = " & Fm(Tp) & vbCr & _
        "max|g_T {-} h| = " & Format$(100 * eMax / hMax, "0") & " % von max|h|" & vbCr & _
        "g_T ist h, {ue}ber T gemittelt: Verz{oe}gerung {~} T/2")
    Repaint sld
End Sub

' ======================================= slide D: arbitrary input from g_T
Public Sub D_Shorter()
    StepTag "D", "IT", -1, 0, 6, 4
    D_Draw
End Sub
Public Sub D_Longer()
    StepTag "D", "IT", 1, 0, 6, 4
    D_Draw
End Sub

Public Sub D_Draw()
    Const NP As Long = 751, OFS As Long = 50     ' t = -0.5 .. 7, dt = 0.01
    Dim sld As Slide, L As Single, T As Single, W As Single, H As Single
    Dim f As TFrame, it As Long, Tp As Double, r As Long, nk As Long
    Dim tx() As Double, xv() As Double, yT() As Double, yE() As Double, Sg() As Double
    Dim sx() As Double, sy() As Double, i As Long, k As Long, m As Long
    Dim eMax As Double, yMax As Double, v As Double, xE() As Double, xT() As Double
    Set sld = FindSlide("D")
    If sld Is Nothing Then Exit Sub
    it = GetTag(sld, "IT", 4)
    Tp = TList(it)
    r = CLng(Tp / 0.01)
    nk = CLng(4 / Tp)

    ReDim xE(0 To 399): ReDim xT(0 To nk - 1)
    For k = 0 To 399
        xE(k) = XC((k + 0.5) * 0.01)
    Next k
    For k = 0 To nk - 1
        xT(k) = XC((k + 0.5) * Tp)
    Next k
    ReDim Sg(0 To NP)
    For m = 0 To NP
        Sg(m) = S2(m * 0.01)
    Next m
    ReDim tx(1 To NP): ReDim xv(1 To NP): ReDim yT(1 To NP): ReDim yE(1 To NP)
    For i = 1 To NP
        tx(i) = -0.5 + (i - 1) * 0.01
        xv(i) = XC(tx(i))
        yE(i) = StairSum(Sg, xE, i - 1 - OFS, 1, 400)       ' exact (dt = 0.01)
        yT(i) = StairSum(Sg, xT, i - 1 - OFS, r, nk)
        If Abs(yE(i)) > yMax Then yMax = Abs(yE(i))
        If Abs(yE(i) - yT(i)) > eMax Then eMax = Abs(yE(i) - yT(i))
    Next i
    Prepare sld, "D", L, T, W, H

    ReDim sx(1 To 2 * nk + 4): ReDim sy(1 To 2 * nk + 4)
    sx(1) = -0.5: sy(1) = 0
    sx(2) = 0: sy(2) = 0
    For k = 0 To nk - 1
        v = XC((k + 0.5) * Tp)
        sx(3 + 2 * k) = k * Tp: sy(3 + 2 * k) = v
        sx(4 + 2 * k) = (k + 1) * Tp: sy(4 + 2 * k) = v
    Next k
    sx(2 * nk + 3) = 4: sy(2 * nk + 3) = 0
    sx(2 * nk + 4) = 7: sy(2 * nk + 4) = 0
    PanelFrame f, L, T, W, H, 0, 0.5, -0.5, 7, -0.6, 1.6
    DrawAxes sld, f, "Eingang", cInk, Array(0, 1)
    Poly sld, f, tx, xv, NP, cInk, 2, False
    Poly sld, f, sx, sy, 2 * nk + 4, cOrange, 2, False
    DrawLegend sld, f, "___  x(t)", cInk, 0
    DrawLegend sld, f, "___  Treppe aus " & nk & " Pulsen der Breite T", cOrange, 1

    PanelFrame f, L, T, W, H, 0.5, 1, -0.5, 7, -0.8, 1.8
    DrawAxes sld, f, "Ausgang", cInk, Array(0, 1)
    Poly sld, f, tx, yE, NP, cInk, 2, False
    Poly sld, f, tx, yT, NP, cOrange, 2.4, False
    DrawLegend sld, f, "___  y = x * h  (exakt)", cInk, 0
    DrawLegend sld, f, U("___  {Sigma} x(kT){.}T{.}g_T(t{-}kT)"), cOrange, 1
    XTicks sld, f, 0, 7, 1, "t / s"

    Readout sld, "D", U("T = " & Fm(Tp) & " s  {->}  " & nk & " Pulsantworten addiert" & vbCr & _
        "max. Fehler = " & Format$(100 * eMax / yMax, "0") & " % von max|y|" & vbCr & _
        "(exakt f{ue}r die Treppe, Fehler = Treppe {-} x)")
    Repaint sld
End Sub

' sum_k x((k+1/2) T) * (s(t - kT) - s(t - (k+1)T)),  t = i0 * 0.01, T = r * 0.01,
' xk(k) = x((k+1/2) T)
Private Function StairSum(Sg() As Double, xk() As Double, ByVal i0 As Long, ByVal r As Long, ByVal nk As Long) As Double
    Dim k As Long, a As Long, b As Long, acc As Double, sa As Double, sb As Double
    For k = 0 To nk - 1
        a = i0 - k * r
        If a < 0 Then Exit For
        b = a - r
        sa = Sg(a)
        If b >= 0 Then sb = Sg(b) Else sb = 0
        acc = acc + xk(k) * (sa - sb)
    Next k
    StairSum = acc
End Function

' ===================================== slide E: continuous flip and slide
Public Sub E_Next()
    StepTag "E", "I", 1, 0, 24, 9
    E_Draw
End Sub
Public Sub E_Prev()
    StepTag "E", "I", -1, 0, 24, 9
    E_Draw
End Sub
Public Sub E_Reset()
    SetTag "E", "I", 0
    E_Draw
End Sub
Public Sub E_Play()
    Dim sld As Slide, i As Long
    Set sld = FindSlide("E")
    If sld Is Nothing Then Exit Sub
    For i = 0 To 24
        sld.Tags.Add "I", CStr(i)
        E_Draw
        Pause 0.25
    Next i
End Sub

Public Sub E_Draw()
    Const NP As Long = 801                       ' tau = -2 .. 6, dt = 0.01
    Dim sld As Slide, L As Single, T As Single, W As Single, H As Single
    Dim f As TFrame, t0 As Double, i As Long, n As Long, ta As Double
    Dim ax() As Double, ay() As Double, gx() As Double, gy() As Double
    Dim rx(1 To 6) As Double, ry(1 To 6) As Double, tb As Double
    Set sld = FindSlide("E")
    If sld Is Nothing Then Exit Sub
    t0 = -1 + 0.25 * GetTag(sld, "I", 9)
    Prepare sld, "E", L, T, W, H

    PanelFrame f, L, T, W, H, 0, 1 / 3, -2, 6, -0.3, 2.4
    DrawAxes sld, f, U("x({tau})   und   h(t{-}{tau})  (gespiegelt, nach t geschoben)"), cInk, Array(0, 1, 2)
    rx(1) = -2: ry(1) = 0: rx(2) = 0: ry(2) = 0: rx(3) = 0: ry(3) = 1
    rx(4) = 2: ry(4) = 1: rx(5) = 2: ry(5) = 0: rx(6) = 6: ry(6) = 0
    Poly sld, f, rx, ry, 6, cInk, 2.2, False
    n = 0
    ReDim gx(1 To NP + 3): ReDim gy(1 To NP + 3)
    For i = 1 To NP
        ta = -2 + (i - 1) * 0.01
        If ta > t0 Then Exit For
        n = n + 1: gx(n) = ta: gy(n) = HRC(t0 - ta)
    Next i
    n = n + 1: gx(n) = t0: gy(n) = 1 / TAU_RC
    n = n + 1: gx(n) = t0: gy(n) = 0
    n = n + 1: gx(n) = 6: gy(n) = 0
    Poly sld, f, gx, gy, n, cOrange, 2.2, False
    VMark sld, f, t0, cOrange
    DrawLegend sld, f, U("___  x({tau}) = Rechteck 0 ... 2 s"), cInk, 0
    DrawLegend sld, f, U("___  h(t{-}{tau}),  t = " & Fm(t0)), cOrange, 1

    PanelFrame f, L, T, W, H, 1 / 3, 2 / 3, -2, 6, -0.3, 2.4
    DrawAxes sld, f, U("Produkt x({tau}){.}h(t{-}{tau})   {->}   Fl{ae}che = y(t) = " & Fm(YRC(t0), 3)), cTeal, Array(0, 1, 2)
    tb = t0
    If tb > 2 Then tb = 2
    If t0 > 0 Then
        n = 0
        ReDim ax(1 To NP): ReDim ay(1 To NP)
        For i = 1 To NP
            ta = (i - 1) * 0.01
            If ta > tb Then Exit For
            n = n + 1: ax(n) = ta: ay(n) = HRC(t0 - ta)
        Next i
        If n >= 2 Then Area sld, f, ax, ay, n, cTeal
    End If
    Ln sld, PX(f, -2), PY(f, 0), PX(f, 6), PY(f, 0), cTeal, 1.5

    PanelFrame f, L, T, W, H, 2 / 3, 1, -2, 6, -0.2, 1.2
    DrawAxes sld, f, "y(t)", cInk, Array(0, 1)
    ReDim ax(1 To NP): ReDim ay(1 To NP)
    n = 0
    For i = 1 To NP
        ax(i) = -2 + (i - 1) * 0.01: ay(i) = YRC(ax(i))
        If ax(i) <= t0 Then n = i
    Next i
    Poly sld, f, ax, ay, NP, cLight, 2, False
    If n >= 2 Then Poly sld, f, ax, ay, n, cInk, 2.4, False
    Dot sld, PX(f, t0), PY(f, YRC(t0)), 11, cOrange, False
    XTicks sld, f, -2, 6, 1, U("{tau}  bzw.  t")

    Readout sld, "E", U("t = " & Fm(t0) & " s" & vbCr & _
        "y(t) = {int} x({tau}){.}h(t{-}{tau}) d{tau} = " & Fm(YRC(t0), 3) & vbCr & _
        IIf(t0 <= 0, "keine {Ue}berlappung {->} y = 0", _
            IIf(t0 <= 2, "Laden: 1 {-} e^({-}t/{tau}c)", "Entladen: Rechteck ist vorbei")))
    Repaint sld
End Sub

' ======================================= slide F: switched-on cosine
Public Sub F_Up()
    StepTag "F", "IO", 1, 0, 7, 3
    F_Draw
End Sub
Public Sub F_Down()
    StepTag "F", "IO", -1, 0, 7, 3
    F_Draw
End Sub
Public Sub F_Filter()
    StepTag "F", "KIND", 1, 0, 1, 0, True
    F_Draw
End Sub

Public Sub F_Draw()
    Const NN As Long = 48
    Dim sld As Slide, L As Single, T As Single, W As Single, H As Single
    Dim f As TFrame, io As Long, kind As Long, om As Double, lab As String
    Dim x(0 To NN) As Double, y(0 To NN) As Double, n As Long, k As Long
    Dim hre As Double, him As Double, mag As Double, ph As Double, den As Double
    Dim sx() As Double, sy() As Double, i As Long, prev As Double, s As String
    Set sld = FindSlide("F")
    If sld Is Nothing Then Exit Sub
    io = GetTag(sld, "IO", 3)
    kind = GetTag(sld, "KIND", 0)
    OmegaOf io, om, lab

    For n = 0 To NN
        x(n) = Cos(om * n)
    Next n
    If kind = 0 Then
        prev = 0
        For n = 0 To NN
            prev = A_EMA * prev + (1 - A_EMA) * x(n)
            y(n) = prev
        Next n
        den = (1 - A_EMA * Cos(om)) ^ 2 + (A_EMA * Sin(om)) ^ 2
        hre = (1 - A_EMA) * (1 - A_EMA * Cos(om)) / den
        him = -(1 - A_EMA) * A_EMA * Sin(om) / den
    Else
        For n = 0 To NN
            For k = 0 To 4
                If n - k >= 0 Then y(n) = y(n) + 0.2 * x(n - k)
            Next k
        Next n
        For k = 0 To 4
            hre = hre + 0.2 * Cos(om * k)
            him = him - 0.2 * Sin(om * k)
        Next k
    End If
    mag = Sqr(hre * hre + him * him)
    ph = Atan2(him, hre)
    Prepare sld, "F", L, T, W, H

    PanelFrame f, L, T, W, H, 0, 0.5, -4, NN + 1, -1.3, 1.3
    DrawAxes sld, f, U("x[n] = cos({Omega}n){.}u[n],  {Omega} = " & lab & "   (ab n = 0 eingeschaltet)"), cMuted, Array(-1, 0, 1)
    For n = 0 To NN
        Stem sld, f, n, x(n), cGray, False, 5, 1.2
    Next n

    PanelFrame f, L, T, W, H, 0.5, 1, -4, NN + 1, -1.3, 1.3
    DrawAxes sld, f, U("y[n] (" & IIf(kind = 0, "EMA a = 0,8", "MA5") & _
        ")   - - -  |H({Omega})|{.}cos({Omega}n + {phi})  (eingeschwungen)"), cInk, Array(-1, 0, 1)
    For n = 0 To NN
        Stem sld, f, n, y(n), cInk, False, 5, 1.2
    Next n
    ReDim sx(1 To 521): ReDim sy(1 To 521)
    For i = 1 To 521
        sx(i) = -4 + (i - 1) * 0.1
        sy(i) = mag * Cos(om * sx(i) + ph)
    Next i
    Poly sld, f, sx, sy, 521, cOrange, 1.8, True
    XTicks sld, f, 0, 48, 8, "n"

    s = "{Omega} = " & lab & ":   |H({Omega})| = " & Fm(mag, 3)
    If mag > 0.0001 Then s = s & ",   {phi} = " & Replace(Format$(ph * 180 / PI, "0"), "-", ChrW(8722)) & "{deg}"
    s = s & vbCr & IIf(kind = 0, "EMA: Einschwingen klingt mit 0,8{^n} ab", _
                       "MA5: ab n = 4 exakt eingeschwungen")
    s = s & vbCr & "Ab {-}{inf} eingeschaltet: nur die gestrichelte Kurve"
    Readout sld, "F", U(s)
    Repaint sld
End Sub

' =============================================================== models
Private Sub GetX(x() As Double)
    ReDim x(0 To 3)
    x(0) = 1: x(1) = 2: x(2) = 0: x(3) = -1
End Sub

Private Sub GetH(ByVal hs As Long, hh() As Double, nh As Long, lab As String)
    Dim j As Long
    Select Case hs
    Case 0
        nh = 3: ReDim hh(0 To 2)
        hh(0) = 1: hh(1) = -1: hh(2) = 0.5
        lab = "h = {1; {-}1; 0,5}  (Blatt 3.1 a)"
    Case 1
        nh = 4: ReDim hh(0 To 3)
        For j = 0 To 3: hh(j) = 0.25: Next j
        lab = "MA4: h = {1/4}{.}{1; 1; 1; 1}"
    Case Else
        nh = 8: ReDim hh(0 To 7)
        For j = 0 To 7: hh(j) = 0.5 * 0.5 ^ j: Next j
        lab = "EMA a = 0,5: h[n] = 0,5{.}0,5{^n} (bis n = 7)"
    End Select
    lab = "Impulsantwort: " & lab
End Sub

Private Sub Conv(x() As Double, hh() As Double, ByVal nh As Long, y() As Double)
    Dim i As Long, j As Long
    ReDim y(0 To 4 + nh - 2)
    For i = 0 To 3
        For j = 0 To nh - 1
            y(i + j) = y(i + j) + x(i) * hh(j)
        Next j
    Next i
End Sub

Private Sub YRange(y() As Double, ByVal ny As Long, lo As Double, hi As Double)
    Dim i As Long
    lo = -0.5: hi = 0.5
    For i = 0 To ny - 1
        If y(i) < lo Then lo = y(i)
        If y(i) > hi Then hi = y(i)
    Next i
    lo = lo - 0.4: hi = hi + 0.4
End Sub

Private Function TList(ByVal i As Long) As Double
    TList = Choose(i + 1, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2)
End Function

Private Sub OmegaOf(ByVal i As Long, om As Double, lab As String)
    Select Case i
    Case 0: om = 0: lab = "0"
    Case 1: om = PI / 32: lab = "{pi}/32"
    Case 2: om = PI / 16: lab = "{pi}/16"
    Case 3: om = PI / 8: lab = "{pi}/8"
    Case 4: om = PI / 4: lab = "{pi}/4"
    Case 5: om = 2 * PI / 5: lab = "2{pi}/5"
    Case 6: om = PI / 2: lab = "{pi}/2"
    Case Else: om = PI: lab = "{pi}"
    End Select
End Sub

' 2nd order low pass f0 = 1 Hz, zeta = 0.15, DC gain 1
Private Function H2(ByVal t As Double) As Double
    Dim w0 As Double, wd As Double
    If t < 0 Then H2 = 0: Exit Function
    w0 = 2 * PI: wd = w0 * Sqr(1 - ZETA * ZETA)
    H2 = w0 / Sqr(1 - ZETA * ZETA) * Exp(-ZETA * w0 * t) * Sin(wd * t)
End Function

Private Function S2(ByVal t As Double) As Double
    Dim w0 As Double, wd As Double
    If t < 0 Then S2 = 0: Exit Function
    w0 = 2 * PI: wd = w0 * Sqr(1 - ZETA * ZETA)
    S2 = 1 - Exp(-ZETA * w0 * t) * (Cos(wd * t) + ZETA / Sqr(1 - ZETA * ZETA) * Sin(wd * t))
End Function

Private Function XC(ByVal t As Double) As Double
    If t < 0 Or t > 4 Then XC = 0: Exit Function
    XC = 0.5 * (1 - Cos(2 * PI * t / 4)) + 0.3 * Sin(2 * PI * 0.75 * t)
End Function

Private Function HRC(ByVal t As Double) As Double
    If t < 0 Then HRC = 0 Else HRC = Exp(-t / TAU_RC) / TAU_RC
End Function

Private Function YRC(ByVal t As Double) As Double
    If t < 0 Then
        YRC = 0
    ElseIf t <= 2 Then
        YRC = 1 - Exp(-t / TAU_RC)
    Else
        YRC = (1 - Exp(-2 / TAU_RC)) * Exp(-(t - 2) / TAU_RC)
    End If
End Function

Private Function Atan2(ByVal y As Double, ByVal x As Double) As Double
    If x > 0 Then
        Atan2 = Atn(y / x)
    ElseIf x < 0 Then
        If y >= 0 Then Atan2 = Atn(y / x) + PI Else Atan2 = Atn(y / x) - PI
    Else
        If y > 0 Then
            Atan2 = PI / 2
        ElseIf y < 0 Then
            Atan2 = -PI / 2
        Else
            Atan2 = 0
        End If
    End If
End Function

' ============================================================ state (tags)
Private Function FindSlide(ByVal id As String) As Slide
    Dim s As Slide, sh As Shape
    For Each s In ActivePresentation.Slides
        For Each sh In s.Shapes
            If sh.Name = "pa_" & id Then
                Set FindSlide = s
                Exit Function
            End If
        Next sh
    Next s
End Function

Private Function GetTag(sld As Slide, ByVal key As String, ByVal def As Long) As Long
    Dim v As String
    v = sld.Tags(key)
    If v = "" Then GetTag = def Else GetTag = CLng(v)
End Function

Private Sub SetTag(ByVal id As String, ByVal key As String, ByVal v As Long)
    Dim sld As Slide
    Set sld = FindSlide(id)
    If Not sld Is Nothing Then sld.Tags.Add key, CStr(v)
End Sub

Private Sub StepTag(ByVal id As String, ByVal key As String, ByVal d As Long, _
                    ByVal lo As Long, ByVal hi As Long, ByVal def As Long, _
                    Optional ByVal wrap As Boolean = False)
    Dim sld As Slide, v As Long
    Set sld = FindSlide(id)
    If sld Is Nothing Then Exit Sub
    v = GetTag(sld, key, def) + d
    If v > hi Then v = IIf(wrap, lo, hi)
    If v < lo Then v = IIf(wrap, hi, lo)
    sld.Tags.Add key, CStr(v)
End Sub

' ================================================================ drawing
Private Sub Prepare(sld As Slide, ByVal id As String, L As Single, T As Single, W As Single, H As Single)
    Dim i As Long
    For i = sld.Shapes.Count To 1 Step -1
        If Left$(sld.Shapes(i).Name, 4) = "dyn_" Then sld.Shapes(i).Delete
    Next i
    On Error Resume Next
    sld.Shapes("static_" & id).Visible = msoFalse
    On Error GoTo 0
    With sld.Shapes("pa_" & id)
        L = .Left: T = .Top: W = .Width: H = .Height
    End With
End Sub

' panel between the fractions a..b of the plot area (top to bottom)
Private Sub PanelFrame(f As TFrame, ByVal L As Single, ByVal T As Single, ByVal W As Single, _
                       ByVal H As Single, ByVal a As Double, ByVal b As Double, _
                       ByVal x0 As Double, ByVal x1 As Double, ByVal y0 As Double, ByVal y1 As Double)
    Const LM As Single = 34, RM As Single = 6, TT As Single = 20, GAP As Single = 10, BOT As Single = 22
    Dim avail As Single
    avail = H - BOT
    f.L = L + LM
    f.W = W - LM - RM
    f.T = T + a * avail + TT
    f.H = (b - a) * avail - TT - GAP
    f.x0 = x0: f.x1 = x1: f.y0 = y0: f.y1 = y1
End Sub

Private Function PX(f As TFrame, ByVal x As Double) As Single
    PX = f.L + (x - f.x0) / (f.x1 - f.x0) * f.W
End Function

Private Function PY(f As TFrame, ByVal y As Double) As Single
    If y < f.y0 Then y = f.y0
    If y > f.y1 Then y = f.y1
    PY = f.T + f.H - (y - f.y0) / (f.y1 - f.y0) * f.H
End Function

Private Sub TagShape(shp As Shape)
    gSeq = gSeq + 1
    shp.Name = "dyn_" & gSeq
End Sub

Private Sub Ln(sld As Slide, ByVal x1 As Single, ByVal y1 As Single, ByVal x2 As Single, _
               ByVal y2 As Single, ByVal col As Long, ByVal wt As Single)
    Dim s As Shape
    If wt <= 0 Then Exit Sub
    Set s = sld.Shapes.AddLine(x1, y1, x2, y2)
    s.Line.ForeColor.RGB = col
    s.Line.Weight = wt
    s.Shadow.Visible = msoFalse
    TagShape s
End Sub

Private Sub Dot(sld As Slide, ByVal cx As Single, ByVal cy As Single, ByVal d As Single, _
                ByVal col As Long, ByVal hollow As Boolean)
    Dim s As Shape
    Set s = sld.Shapes.AddShape(msoShapeOval, cx - d / 2, cy - d / 2, d, d)
    s.Line.Visible = msoTrue
    s.Line.ForeColor.RGB = col
    s.Line.Weight = 1.5
    s.Fill.Visible = msoTrue
    s.Fill.Solid
    If hollow Then s.Fill.ForeColor.RGB = RGB(255, 255, 255) Else s.Fill.ForeColor.RGB = col
    s.Shadow.Visible = msoFalse
    TagShape s
End Sub

Private Sub Stem(sld As Slide, f As TFrame, ByVal x As Double, ByVal y As Double, _
                 ByVal col As Long, ByVal hollow As Boolean, ByVal d As Single, _
                 Optional ByVal wt As Single = 2)
    If x < f.x0 Or x > f.x1 Then Exit Sub
    If Abs(PY(f, y) - PY(f, 0)) > 0.5 Then Ln sld, PX(f, x), PY(f, 0), PX(f, x), PY(f, y), col, wt
    Dot sld, PX(f, x), PY(f, y), d, col, hollow
End Sub

' dotted vertical marker at x over the whole panel
Private Sub VMark(sld As Slide, f As TFrame, ByVal x As Double, ByVal col As Long)
    Dim s As Shape
    If x < f.x0 Or x > f.x1 Then Exit Sub
    Set s = sld.Shapes.AddLine(PX(f, x), f.T, PX(f, x), f.T + f.H)
    s.Line.ForeColor.RGB = col
    s.Line.Weight = 0.75
    s.Line.DashStyle = msoLineRoundDot
    TagShape s
End Sub

Private Sub Poly(sld As Slide, f As TFrame, xs() As Double, ys() As Double, ByVal n As Long, _
                 ByVal col As Long, ByVal wt As Single, ByVal dash As Boolean)
    Dim pts() As Single, i As Long, s As Shape
    If n < 2 Then Exit Sub
    ReDim pts(1 To n, 1 To 2)
    For i = 1 To n
        pts(i, 1) = PX(f, xs(i))
        pts(i, 2) = PY(f, ys(i))
    Next i
    Set s = sld.Shapes.AddPolyline(pts)
    s.Fill.Visible = msoFalse
    s.Line.ForeColor.RGB = col
    s.Line.Weight = wt
    If dash Then s.Line.DashStyle = msoLineDash
    s.Shadow.Visible = msoFalse
    TagShape s
End Sub

' filled area between the curve and y = 0
Private Sub Area(sld As Slide, f As TFrame, xs() As Double, ys() As Double, ByVal n As Long, ByVal col As Long)
    Dim pts() As Single, i As Long, s As Shape
    ReDim pts(1 To n + 3, 1 To 2)
    pts(1, 1) = PX(f, xs(1)): pts(1, 2) = PY(f, 0)
    For i = 1 To n
        pts(i + 1, 1) = PX(f, xs(i))
        pts(i + 1, 2) = PY(f, ys(i))
    Next i
    pts(n + 2, 1) = PX(f, xs(n)): pts(n + 2, 2) = PY(f, 0)
    pts(n + 3, 1) = pts(1, 1): pts(n + 3, 2) = pts(1, 2)
    Set s = sld.Shapes.AddPolyline(pts)
    s.Fill.Visible = msoTrue
    s.Fill.Solid
    s.Fill.ForeColor.RGB = col
    s.Fill.Transparency = 0.55
    s.Line.ForeColor.RGB = col
    s.Line.Weight = 2
    s.Shadow.Visible = msoFalse
    TagShape s
End Sub

' align: 1 = left edge at x, 2 = centred on x, 3 = right edge at x
Private Sub Txt(sld As Slide, ByVal x As Single, ByVal y As Single, ByVal s As String, _
                ByVal sz As Single, ByVal col As Long, ByVal bold As Boolean, _
                Optional ByVal align As Long = 1)
    Dim shp As Shape, w As Single
    w = 60
    If align = 1 Then w = 500
    Select Case align
    Case 2: x = x - w / 2
    Case 3: x = x - w
    End Select
    Set shp = sld.Shapes.AddTextbox(msoTextOrientationHorizontal, x, y, w, sz + 6)
    With shp.TextFrame
        .WordWrap = msoFalse
        .AutoSize = ppAutoSizeNone
        .MarginLeft = 0: .MarginRight = 0: .MarginTop = 0: .MarginBottom = 0
        With .TextRange
            .Text = s
            .Font.Name = "Calibri"
            .Font.Size = sz
            .Font.Bold = IIf(bold, msoTrue, msoFalse)
            .Font.Color.RGB = col
            .ParagraphFormat.Alignment = Choose(align, ppAlignLeft, ppAlignCenter, ppAlignRight)
        End With
    End With
    TagShape shp
End Sub

Private Sub DrawAxes(sld As Slide, f As TFrame, ByVal title As String, ByVal tcol As Long, yt As Variant)
    Dim i As Long, v As Double
    Ln sld, f.L, f.T, f.L, f.T + f.H, cGray, 0.75
    If f.y0 < 0 And f.y1 > 0 Then Ln sld, f.L, PY(f, 0), f.L + f.W, PY(f, 0), cGray, 0.75
    For i = LBound(yt) To UBound(yt)
        v = yt(i)
        If v >= f.y0 And v <= f.y1 Then
            Ln sld, f.L - 4, PY(f, v), f.L, PY(f, v), cGray, 0.75
            Txt sld, f.L - 6, PY(f, v) - 8, Fm(v), 11, cMuted, False, 3
        End If
    Next i
    Txt sld, f.L, f.T - 19, title, 13, tcol, True
End Sub

Private Sub XTicks(sld As Slide, f As TFrame, ByVal a As Double, ByVal b As Double, _
                   ByVal stp As Double, ByVal lbl As String)
    Dim v As Double
    v = a
    Do While v <= b + 0.000001
        Ln sld, PX(f, v), f.T + f.H, PX(f, v), f.T + f.H + 4, cGray, 0.75
        Txt sld, PX(f, v), f.T + f.H + 5, Fm(v), 11, cMuted, False, 2
        v = v + stp
    Loop
    Txt sld, f.L + f.W, f.T + f.H + 19, lbl, 11, cMuted, False, 3
End Sub

' legend line i (0, 1, ...) at the top right of a panel
Private Sub DrawLegend(sld As Slide, f As TFrame, ByVal s As String, ByVal col As Long, ByVal i As Long)
    Txt sld, f.L + f.W - 250, f.T + 2 + 16 * i, s, 12, col, False, 1, 248
End Sub

Private Sub Readout(sld As Slide, ByVal id As String, ByVal s As String)
    On Error Resume Next
    sld.Shapes("txt_" & id).TextFrame.TextRange.Text = s
End Sub

Private Sub Repaint(sld As Slide)
    If Not REFRESH_IN_SHOW Then Exit Sub
    On Error Resume Next
    If SlideShowWindows.Count > 0 Then
        If SlideShowWindows(1).View.Slide.SlideID = sld.SlideID Then
            SlideShowWindows(1).View.GotoSlide sld.SlideIndex, msoFalse
        End If
    End If
End Sub

Private Sub Pause(ByVal sec As Double)
    Dim t0 As Double
    t0 = Timer
    Do While Timer < t0 + sec And Timer >= t0
        DoEvents
    Loop
End Sub

' ================================================================ helpers
Private Function Max1(ByVal a As Single, ByVal b As Single) As Single
    If a > b Then Max1 = a Else Max1 = b
End Function

' number with decimal comma and a real minus sign
Private Function Fm(ByVal v As Double, Optional ByVal dec As Long = 2) As String
    Dim s As String
    If Abs(v - Round(v)) < 0.0000001 Then
        s = CStr(CLng(Round(v)))
    Else
        s = Format$(v, "0." & String$(dec, "#"))
        s = Replace(s, ".", ",")
        If Right$(s, 1) = "," Then s = Left$(s, Len(s) - 1)
    End If
    If s = "-0" Then s = "0"
    Fm = Replace(s, "-", ChrW(8722))
End Function

' number in parentheses if negative (for products)
Private Function Par(ByVal v As Double) As String
    If v < 0 Then Par = "(" & Fm(v) & ")" Else Par = Fm(v)
End Function

Private Function U(ByVal s As String) As String
    s = Replace(s, "{-}", ChrW(8722))
    s = Replace(s, "{.}", ChrW(183))
    s = Replace(s, "{Sigma}", ChrW(931))
    s = Replace(s, "{tau}", ChrW(964))
    s = Replace(s, "{Omega}", ChrW(937))
    s = Replace(s, "{pi}", ChrW(960))
    s = Replace(s, "{phi}", ChrW(966))
    s = Replace(s, "{->}", ChrW(8594))
    s = Replace(s, "{int}", ChrW(8747))
    s = Replace(s, "{inf}", ChrW(8734))
    s = Replace(s, "{~}", ChrW(8776))
    s = Replace(s, "{^}", ChrW(8593))
    s = Replace(s, "{^n}", ChrW(8319))
    s = Replace(s, "{_0}", ChrW(8320))
    s = Replace(s, "{1/4}", ChrW(188))
    s = Replace(s, "{deg}", ChrW(176))
    s = Replace(s, "{ae}", ChrW(228))
    s = Replace(s, "{oe}", ChrW(246))
    s = Replace(s, "{ue}", ChrW(252))
    s = Replace(s, "{Ue}", ChrW(220))
    U = s
End Function

Private Function EchoCol(ByVal k As Long) As Long
    EchoCol = Choose((k Mod 4) + 1, cTeal, cOrange, cViolet, cRed)
End Function

Private Function cInk() As Long
    cInk = RGB(27, 42, 65)
End Function
Private Function cOrange() As Long
    cOrange = RGB(224, 122, 31)
End Function
Private Function cTeal() As Long
    cTeal = RGB(42, 157, 143)
End Function
Private Function cViolet() As Long
    cViolet = RGB(123, 94, 167)
End Function
Private Function cRed() As Long
    cRed = RGB(200, 85, 61)
End Function
Private Function cGray() As Long
    cGray = RGB(138, 148, 166)
End Function
Private Function cLight() As Long
    cLight = RGB(213, 218, 227)
End Function
Private Function cMuted() As Long
    cMuted = RGB(91, 101, 117)
End Function
