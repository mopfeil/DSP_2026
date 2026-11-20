"""Static figures of the convolution deck (PNG, 200 dpi) -> build/.

The fig_<id>.png of an interactive slide shows its default state; the macro
replaces it with live drawings.
"""
import os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker
from model import *

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build")
os.makedirs(OUT, exist_ok=True)

INK, ORANGE, TEAL, VIOLET, RED = "#1B2A41", "#E07A1F", "#2A9D8F", "#7B5EA7", "#C8553D"
GRAY, LIGHT, MUTED = "#8A94A6", "#D5DAE3", "#5B6575"
ECHO = [TEAL, ORANGE, VIOLET, RED]
PA = (7.9, 4.85)                       # plot area of the interactive slides (in)

plt.rcParams.update({
    "font.family": "Carlito", "font.size": 12, "axes.edgecolor": GRAY,
    "axes.labelcolor": MUTED, "xtick.color": MUTED, "ytick.color": MUTED,
    "axes.spines.top": False, "axes.spines.right": False,
    "mathtext.fontset": "cm", "savefig.dpi": 200,
})


def save(fig, name):
    fig.savefig(os.path.join(OUT, name), facecolor="white")
    plt.close(fig)


def _de(v, pos=None):
    return f"{v:g}".replace(".", ",").replace("-", "−")


def style(ax, title, color=INK, zero=True):
    ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(_de))
    ax.xaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(_de))
    ax.set_title(title, loc="left", color=color, fontsize=13, fontweight="bold", pad=4)
    if zero:
        ax.axhline(0, color=GRAY, lw=0.8)
    ax.spines["bottom"].set_visible(False)


def stem(ax, k, v, color, ms=6, lw=2.0, hollow=False):
    k = np.atleast_1d(k)
    v = np.atleast_1d(v)
    ax.vlines(k, 0, v, color=color, lw=lw)
    ax.plot(k, v, "o", ms=ms, color=color, mfc="white" if hollow else color, mew=1.6)


def panels(n, ratios=None):
    fig, axs = plt.subplots(n, 1, figsize=PA, sharex=True,
                            gridspec_kw={"height_ratios": ratios or [1] * n})
    fig.subplots_adjust(left=0.07, right=0.98, top=0.95, bottom=0.09, hspace=0.42)
    return fig, axs


# ------------------------------------------------------------ hero (title)
def fig_hero():
    fig, ax = plt.subplots(figsize=(6.2, 4.6))
    fig.patch.set_alpha(0)
    ax.set_facecolor("none")
    t = np.linspace(-0.5, 7, 1500)
    ax.plot(t, x_cont(t) + 2.4, color="#9FB3D1", lw=2.5)
    for T, a in [(0.5, 1.0)]:
        for k in range(8):
            g = x_cont((k + .5) * T) * (s2(t - k * T) - s2(t - (k + 1) * T))
            ax.plot(t, g, color=ORANGE, lw=1.4, alpha=0.55)
    ax.plot(t, y_stair(t, 0.01), color="white", lw=3)
    ax.axis("off")
    fig.subplots_adjust(0, 0, 1, 1)
    fig.savefig(os.path.join(OUT, "fig_hero.png"), transparent=True)
    plt.close(fig)


# ---------------------------------------------------------- LTI properties
def fig_lti():
    h = H_SETS[0][1]
    fig, axs = plt.subplots(3, 2, figsize=(7.6, 4.7), sharex=True, sharey=True)
    fig.subplots_adjust(left=0.06, right=0.99, top=0.92, bottom=0.07, hspace=0.55, wspace=0.12)
    n = np.arange(-1, 8)
    rows = [
        ("δ[n]", [(0, 1, INK)], "h[n]", [(0, h, ORANGE)]),
        ("δ[n−3]  (verschoben)", [(3, 1, INK)], "h[n−3]  (gleich verschoben)", [(3, h, ORANGE)]),
        ("2·δ[n] − δ[n−3]", [(0, 2, INK), (3, -1, INK)],
         "2·h[n] − h[n−3]  (gleich kombiniert)", [(0, 2 * h, ORANGE), (3, -h, ORANGE)]),
    ]
    for r, (lt, ins, rt, outs) in enumerate(rows):
        a, b = axs[r]
        for k, v, c in ins:
            stem(a, k, v, c)
        y = np.zeros(12)
        for k, v, c in outs:
            y[k:k + len(np.atleast_1d(v))] += v
        stem(b, np.arange(12), y, ORANGE)
        style(a, lt)
        style(b, rt, ORANGE)
        for ax in (a, b):
            ax.set_xlim(-1.5, 7.5)
            ax.set_ylim(-2.6, 2.6)
            ax.set_yticks([-2, 0, 2])
            ax.set_xticks(range(0, 8))
    save(fig, "fig_lti.png")


# ---------------------------------------------- derivation: Σ δ -> Σ h
def fig_deriv():
    h = H_SETS[0][1]
    fig, axs = plt.subplots(5, 2, figsize=(7.6, 5.6), sharex=True)
    fig.subplots_adjust(left=0.07, right=0.99, top=0.95, bottom=0.05, hspace=0.75, wspace=0.14)
    for k in range(4):
        a, b = axs[k]
        stem(a, k, X[k], ECHO[k])
        stem(b, np.arange(k, k + 3), X[k] * h, ECHO[k])
        style(a, f"x[{k}]·δ[n−{k}]", ECHO[k])
        style(b, f"x[{k}]·h[n−{k}]", ECHO[k])
        for ax in (a, b):
            ax.set_ylim(-2.6, 2.6)
            ax.set_yticks([-2, 0, 2])
    a, b = axs[4]
    stem(a, np.arange(4), X, INK)
    y = conv(X, H_SETS[0][1])
    stem(b, np.arange(len(y)), y, INK)
    style(a, "Σ = x[n]")
    style(b, "Σ = y[n] = Σ x[k]·h[n−k]")
    for ax in (a, b):
        ax.set_ylim(-2.6, 2.6)
        ax.set_yticks([-2, 0, 2])
        ax.set_xlim(-1.5, 8.5)
        ax.set_xticks(range(0, 9))
    save(fig, "fig_deriv.png")


# -------------------------------------------- interactive defaults (static)
def fig_A(n=2, hs=0):
    """Output view: flip and slide."""
    h = H_SETS[hs][1]
    y = conv(X, h)
    fig, axs = panels(3)
    k = np.arange(K_MIN, K_MAX + 1)
    stem(axs[0], np.arange(4), X, INK)
    kk = np.arange(n - len(h) + 1, n + 1)
    stem(axs[0], kk + 0.15, h[n - kk], ORANGE)
    style(axs[0], f"x[k]   und   h[{n}−k]  (gespiegelt, nach n = {n} geschoben)")
    prod = np.array([X[j] * h[n - j] if 0 <= j < 4 and 0 <= n - j < len(h) else 0 for j in k])
    m = prod != 0
    stem(axs[1], k[m], prod[m], TEAL)
    style(axs[1], f"Produkte x[k]·h[{n}−k]   →   Summe = y[{n}] = {_de(round(y[n], 2))}", TEAL)
    stem(axs[2], np.arange(len(y)), y, LIGHT, hollow=True)
    stem(axs[2], np.arange(n + 1), y[:n + 1], INK)
    stem(axs[2], [n], [y[n]], ORANGE, ms=9)
    style(axs[2], "y[n]")
    lo, hi = yrange(y)
    axs[0].set_ylim(-1.5, 2.5)
    axs[1].set_ylim(-2.5, 2.5)
    axs[2].set_ylim(lo, hi)
    axs[2].set_xlim(K_MIN - 0.5, K_MAX + 0.5)
    axs[2].set_xticks(range(K_MIN, K_MAX + 1))
    save(fig, "fig_A.png")


def fig_B(K=4, hs=0):
    """Input view: add echoes."""
    h = H_SETS[hs][1]
    y = conv(X, h)
    fig, axs = panels(3)
    for k in range(4):
        stem(axs[0], k, X[k], ECHO[k] if k < K else LIGHT, hollow=k >= K)
    style(axs[0], "x[k] — jeder Abtastwert ist ein skalierter, verschobener Impuls")
    yK = np.zeros(len(y))
    for k in range(K):
        e = np.zeros(len(y))
        e[k:k + len(h)] = X[k] * h
        nz = np.nonzero(e)[0]
        stem(axs[1], nz + (k - 1.5) * 0.13, e[nz], ECHO[k], ms=5)
        yK += e
    style(axs[1], "Echos x[k]·h[n−k]  (jeweils eine Impulsantwort)")
    stem(axs[2], np.arange(len(y)), y, LIGHT, hollow=True)
    stem(axs[2], np.arange(len(y)), yK, INK)
    style(axs[2], f"Summe der ersten {K} Echos = y[n]")
    lo, hi = yrange(y)
    axs[0].set_ylim(-1.5, 2.5)
    axs[1].set_ylim(-2.5, 2.5)
    axs[2].set_ylim(lo, hi)
    axs[2].set_xlim(K_MIN - 0.5, K_MAX + 0.5)
    axs[2].set_xticks(range(K_MIN, K_MAX + 1))
    save(fig, "fig_B.png")


def fig_C(T=0.2):
    t = np.linspace(-0.5, 5, 1101)
    fig, axs = panels(2, [1, 2.3])
    p = np.where((t >= 0) & (t < T), 1 / T, 0)
    axs[0].fill_between(t, 0, np.minimum(p, 6), color=ORANGE, alpha=0.35, step="post")
    axs[0].plot(t, np.minimum(p, 6), color=ORANGE, lw=2)
    style(axs[0], f"realer Impuls p_T(t): Breite T = {T:g} s, Höhe 1/T = {1/T:g}, Fläche 1".replace(".", ","), ORANGE)
    axs[0].set_ylim(-0.5, 6.5)
    axs[0].set_yticks([0, 3, 6])
    axs[1].plot(t, h2(t), color=INK, lw=2, ls="--", label="h(t)  (idealer Dirac-Impuls)")
    axs[1].plot(t, g_pulse(t, T), color=ORANGE, lw=2.6, label="g_T(t) = (p_T * h)(t)")
    style(axs[1], "Antworten")
    axs[1].legend(loc="upper right", frameon=False, fontsize=12)
    axs[1].set_ylim(-4, 6)
    axs[1].set_xlim(-0.5, 5)
    axs[1].set_xlabel("t / s", loc="right")
    save(fig, "fig_C.png")


def fig_D(T=0.5):
    t = np.linspace(-0.5, 7, 1501)
    fig, axs = panels(2)
    axs[0].plot(t, x_cont(t), color=INK, lw=2, label="x(t)")
    axs[0].plot(t, x_stair(t, T), color=ORANGE, lw=2, label="Treppe aus Pulsen der Breite T")
    style(axs[0], "Eingang")
    axs[0].legend(loc="upper right", frameon=False, fontsize=12)
    axs[0].set_ylim(-0.6, 1.6)
    axs[1].plot(t, y_stair(t, 0.01), color=INK, lw=2, label="y = x * h  (exakt)")
    axs[1].plot(t, y_stair(t, T), color=ORANGE, lw=2.4, label="Σ x(kT)·T·g_T(t−kT)")
    style(axs[1], "Ausgang")
    axs[1].legend(loc="upper right", frameon=False, fontsize=12)
    axs[1].set_ylim(-0.8, 1.8)
    axs[1].set_xlim(-0.5, 7)
    axs[1].set_xlabel("t / s", loc="right")
    save(fig, "fig_D.png")


def fig_E(t0=1.25):
    tau = np.linspace(-2, 6, 1601)
    fig, axs = panels(3)
    axs[0].plot(tau, x_rect(tau), color=INK, lw=2.2, label="x(τ)")
    axs[0].plot(tau, h_rc(t0 - tau), color=ORANGE, lw=2.2, label=f"h(t−τ), t = {t0:g}".replace(".", ","))
    axs[0].axvline(t0, color=ORANGE, lw=0.8, ls=":")
    style(axs[0], "x(τ)   und   h(t−τ)  (gespiegelt, nach t geschoben)")
    axs[0].legend(loc="upper right", frameon=False, fontsize=12)
    pr = x_rect(tau) * h_rc(t0 - tau)
    axs[1].fill_between(tau, 0, pr, color=TEAL, alpha=0.4)
    axs[1].plot(tau, pr, color=TEAL, lw=2)
    style(axs[1], f"Produkt x(τ)·h(t−τ)  →  Fläche = y(t) = {float(y_rc(t0)):.3f}".replace(".", ","), TEAL)
    axs[2].plot(tau, y_rc(tau), color=LIGHT, lw=2)
    m = tau <= t0
    axs[2].plot(tau[m], y_rc(tau[m]), color=INK, lw=2.4)
    axs[2].plot([t0], [y_rc(t0)], "o", color=ORANGE, ms=9)
    style(axs[2], "y(t)")
    axs[0].set_ylim(-0.3, 2.4)
    axs[1].set_ylim(-0.3, 2.4)
    axs[2].set_ylim(-0.2, 1.2)
    axs[2].set_xlim(-2, 6)
    axs[2].set_xlabel("τ  bzw.  t", loc="right")
    save(fig, "fig_E.png")


def fig_F(io=3, kind=0):
    om, lab = OMEGAS[io]
    n = np.arange(0, N_MAX + 1)
    x = np.cos(om * n)
    y = filt(x, kind)
    Hv = H(om, kind)
    fig, axs = panels(2)
    stem(axs[0], n, x, GRAY, ms=3.5, lw=1.2)
    style(axs[0], f"x[n] = cos(Ω n)·u[n],  Ω = {lab}   (ab n = 0 eingeschaltet)", MUTED)
    stem(axs[1], n, y, INK, ms=3.5, lw=1.2)
    nn = np.linspace(-4, N_MAX, 800)
    axs[1].plot(nn, abs(Hv) * np.cos(om * nn + np.angle(Hv)), color=ORANGE, lw=1.8, ls="--")
    name = "EMA a = 0,8" if kind == 0 else "MA5"
    style(axs[1], f"y[n] ({name})   - - -  |H(Ω)|·cos(Ω n + φ)  (eingeschwungen)")
    for ax in axs:
        ax.set_ylim(-1.3, 1.3)
    axs[1].set_xlim(-4, N_MAX + 1)
    save(fig, "fig_F.png")


# ---------------------------------------------------------- other statics
def fig_step():
    t = np.linspace(-0.5, 7, 1501)
    fig, axs = plt.subplots(1, 2, figsize=(7.8, 3.3))
    fig.subplots_adjust(left=0.06, right=0.99, top=0.88, bottom=0.14, wspace=0.18)
    a, b = axs
    a.plot(t, np.where(t >= 0, 1, 0), color=GRAY, lw=1.5, label="u(t)")
    a.plot(t, s2(t), color=INK, lw=2.4, label="s(t)")
    a.plot(t, h2(t) / 6, color=ORANGE, lw=1.6, ls="--", label="h(t) = ds/dt  (÷ 6)")
    style(a, "Sprungantwort s(t) und ihre Ableitung")
    a.legend(frameon=False, fontsize=11, loc="upper right", ncol=1)
    a.set_ylim(-0.7, 2.6)
    a.set_xlim(-0.5, 4)
    T = 0.5
    xs = x_stair(t, T)
    b.plot(t, xs, color=GRAY, lw=1.4)
    prev = 0.0
    for k in range(9):
        v = x_cont((k + .5) * T) if k < 8 else 0.0
        d = v - prev
        b.plot(t, d * s2(t - k * T), color=ECHO[k % 4], lw=1.0, alpha=0.55)
        prev = v
    b.plot(t, y_stair(t, T), color=INK, lw=2.6)
    style(b, r"Treppe aus Sprüngen:  $y = \Sigma_k\, \Delta x_k\, s(t-kT)$")
    b.set_ylim(-0.8, 1.8)
    b.set_xlim(-0.5, 7)
    save(fig, "fig_step.png")


def fig_limits():
    fig, axs = plt.subplots(1, 3, figsize=(11.6, 3.0))
    fig.subplots_adjust(left=0.04, right=0.99, top=0.86, bottom=0.16, wspace=0.2)
    t = np.linspace(-0.3, 4, 1000)
    # (1) saturation
    T = 0.05
    g = g_pulse(t, T)
    a = axs[0]
    a.plot(t, h2(t), color=INK, lw=1.5, ls="--", label="h(t)")
    gs = np.clip(g, -2.5, 2.5)
    a.plot(t, gs, color=RED, lw=2.2, label="gemessen, Ausgang begrenzt ±2,5")
    style(a, "Linearität verletzt (Sättigung)", RED)
    a.legend(frameon=False, fontsize=10, loc="upper right")
    a.set_ylim(-4, 6.5)
    # (2) bandwidth
    f = np.linspace(0, 6, 1200)
    b = axs[1]
    Hf = 1 / (1 - (f / 1) ** 2 + 2j * ZETA * f)
    b.plot(f, abs(Hf), color=INK, lw=2, label="|H(f)|")
    for T, c in [(0.2, ORANGE), (0.5, VIOLET)]:
        b.plot(f, abs(np.sinc(f * T)), color=c, lw=2, label=f"|P_T(f)|, T = {T:g} s".replace(".", ","))
    style(b, "Bandbreite: P_T(f) hat Nullstellen bei 1/T", ORANGE)
    b.legend(frameon=False, fontsize=10, loc="upper right")
    b.set_ylim(0, 3.6)
    b.set_xlabel("f / Hz", loc="right")
    # (3) noise
    c = axs[2]
    rng = np.random.default_rng(3)
    for T, col in [(0.5, VIOLET), (0.02, ORANGE)]:
        area = min(1.0, 10 * T)            # amplitude limit 10 -> area 10 T
        meas = area * g_pulse(t, T) + 0.08 * rng.standard_normal(t.size)
        c.plot(t, meas / area, color=col, lw=1.0 if T < 0.1 else 1.8,
               label=f"T = {T:g} s, Fläche {area:g}, ÷ Fläche".replace(".", ","))
    style(c, "Rauschen: kurzer Puls = wenig Energie", VIOLET)
    c.legend(frameon=False, fontsize=10, loc="upper right")
    c.set_ylim(-5, 8)
    for ax in (axs[0], axs[2]):
        ax.set_xlabel("t / s", loc="right")
    save(fig, "fig_limits.png")


def fig_riemann():
    t = np.linspace(-0.3, 4.3, 2000)
    fig, axs = plt.subplots(1, 2, figsize=(7.8, 3.0), sharey=True)
    fig.subplots_adjust(left=0.06, right=0.99, top=0.86, bottom=0.14, wspace=0.08)
    for ax, D in zip(axs, [0.5, 0.1]):
        ax.plot(t, x_cont(t), color=INK, lw=2)
        ax.plot(t, x_stair(t, D), color=ORANGE, lw=1.6)
        k = int(1.6 / D)
        a0, a1 = k * D, (k + 1) * D
        ax.fill_between([a0, a1], 0, x_cont((k + .5) * D), color=ORANGE, alpha=0.45)
        style(ax, f"Δ = {D:g}: x(t) ≈ Σ x(kΔ)·Δ·p_Δ(t−kΔ)".replace(".", ","))
        ax.set_xlim(-0.3, 4.3)
        ax.set_ylim(-0.6, 1.5)
        ax.set_xlabel("t / s", loc="right")
    save(fig, "fig_riemann.png")


def fig_sampling():
    t = np.linspace(-0.3, 3, 1500)
    Ts = 0.1
    n = np.arange(0, 31)
    fig, ax = plt.subplots(figsize=(5.6, 2.9))
    fig.subplots_adjust(left=0.08, right=0.99, top=0.86, bottom=0.16)
    ax.plot(t, h2(t) * Ts, color=GRAY, lw=1.8, label="T_s · h(t)")
    stem(ax, n * Ts, h2(n * Ts) * Ts, ORANGE, ms=4, lw=1.4)
    style(ax, "h[n] = T_s·h(nT_s),  T_s = 0,1 s")
    ax.set_xlabel("t / s", loc="right")
    save(fig, "fig_sampling.png")


def fig_infinite():
    a = A_EMA
    n = np.arange(0, 41)
    hh = (1 - a) * a ** n
    M = 15
    fig, ax = plt.subplots(figsize=(5.6, 3.0))
    fig.subplots_adjust(left=0.09, right=0.99, top=0.86, bottom=0.14)
    stem(ax, n[:M], hh[:M], INK, ms=4, lw=1.4)
    stem(ax, n[M:], hh[M:], ORANGE, ms=4, lw=1.4)
    style(ax, f"EMA a = 0,8: h[n] = 0,2·0,8ⁿ — Rest ab n = {M}: Σ = $0{{,}}8^{{{M}}}$ = {a**M:.3f}".replace(".", ",").replace("{,}", "{,}"))
    ax.set_xlabel("n", loc="right")
    save(fig, "fig_infinite.png")


if __name__ == "__main__":
    for f in [fig_hero, fig_lti, fig_deriv, fig_A, fig_B, fig_C, fig_D, fig_E, fig_F,
              fig_step, fig_limits, fig_riemann, fig_sampling, fig_infinite]:
        f()
    print("figures ->", OUT)
