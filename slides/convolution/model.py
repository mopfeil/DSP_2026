"""Signals and systems of the convolution deck.

Everything the slides show is computed here. Faltung.bas implements the
same formulas in VBA for the interactive slides, so change both together.
"""
import numpy as np

# ---------------------------------------------------------------- discrete
X = np.array([1.0, 2.0, 0.0, -1.0])          # x[k], k = 0..3 (sheet 3.1 a)
H_SETS = [
    ("h = {1; −1; 0,5}  (Blatt 3.1 a)", np.array([1.0, -1.0, 0.5])),
    ("MA4: h = ¼·{1; 1; 1; 1}", np.full(4, 0.25)),
    ("EMA a = 0,5: h[n] = 0,5·0,5ⁿ (bis n = 7)", 0.5 * 0.5 ** np.arange(8)),
]
K_MIN, K_MAX = -3, 12                         # axis range of the discrete plots


def conv(x, h):
    return np.convolve(x, h)


def yrange(v, lo=-0.5, pad=0.4):
    return min(lo, v.min()) - pad, max(0.5, v.max()) + pad


# ------------------------------------------------------- continuous system
# 2nd order low pass, f0 = 1 Hz, zeta = 0.15, DC gain 1:
#   h(t) = w0/sqrt(1-z^2) * exp(-z w0 t) * sin(wd t)        (t >= 0)
#   s(t) = 1 - exp(-z w0 t) * (cos(wd t) + z/sqrt(1-z^2) sin(wd t))
ZETA = 0.15
W0 = 2 * np.pi
WD = W0 * np.sqrt(1 - ZETA ** 2)


def h2(t):
    t = np.asarray(t, float)
    v = W0 / np.sqrt(1 - ZETA ** 2) * np.exp(-ZETA * W0 * t) * np.sin(WD * t)
    return np.where(t >= 0, v, 0.0)


def s2(t):
    t = np.asarray(t, float)
    v = 1 - np.exp(-ZETA * W0 * t) * (np.cos(WD * t)
                                       + ZETA / np.sqrt(1 - ZETA ** 2) * np.sin(WD * t))
    return np.where(t >= 0, v, 0.0)


def g_pulse(t, T):
    """Response to the real impulse p_T (height 1/T on [0, T))."""
    return (s2(t) - s2(t - T)) / T


T_LIST = [0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0]


def x_cont(t):
    """Test input of slide 'beliebiger Eingang' (0 <= t <= 4)."""
    t = np.asarray(t, float)
    v = 0.5 * (1 - np.cos(2 * np.pi * t / 4)) + 0.3 * np.sin(2 * np.pi * 0.75 * t)
    return np.where((t >= 0) & (t <= 4), v, 0.0)


def y_stair(t, T):
    """Superposition of pulse responses: sum_k x((k+1/2)T) * T * g_T(t-kT)."""
    t = np.asarray(t, float)
    y = np.zeros_like(t)
    for k in range(int(round(4 / T))):
        y += x_cont((k + 0.5) * T) * (s2(t - k * T) - s2(t - (k + 1) * T))
    return y


def x_stair(t, T):
    t = np.asarray(t, float)
    k = np.floor(t / T)
    return np.where((t >= 0) & (t < 4), x_cont((k + 0.5) * T), 0.0)


# ----------------------------------------------- continuous flip and slide
TAU = 0.5                                    # RC time constant


def h_rc(t):
    t = np.asarray(t, float)
    return np.where(t >= 0, np.exp(-np.clip(t, 0, None) / TAU) / TAU, 0.0)


def x_rect(t):
    t = np.asarray(t, float)
    return np.where((t >= 0) & (t <= 2), 1.0, 0.0)


def y_rc(t):
    t = np.asarray(t, float)
    a = 1 - np.exp(-np.clip(t, 0, None) / TAU)
    b = (1 - np.exp(-2 / TAU)) * np.exp(-(t - 2) / TAU)
    return np.where(t < 0, 0.0, np.where(t <= 2, a, b))


# ---------------------------------------------------- switched-on cosine
OMEGAS = [(0, "0"), (np.pi / 32, "π/32"), (np.pi / 16, "π/16"), (np.pi / 8, "π/8"),
          (np.pi / 4, "π/4"), (2 * np.pi / 5, "2π/5"), (np.pi / 2, "π/2"), (np.pi, "π")]
A_EMA = 0.8
N_MAX = 48


def filt(x, kind):
    if kind == 0:                                 # EMA
        y = np.zeros_like(x)
        prev = 0.0
        for n, v in enumerate(x):
            prev = A_EMA * prev + (1 - A_EMA) * v
            y[n] = prev
        return y
    return np.convolve(x, np.full(5, 0.2))[:len(x)]


def H(om, kind):
    if kind == 0:
        return (1 - A_EMA) / (1 - A_EMA * np.exp(-1j * om))
    return np.sum(0.2 * np.exp(-1j * om * np.arange(5)))
