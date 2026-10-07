/*
 * filtdesign.h -- small digital filter design library (Sheet 10, Ex. 10.2)
 *                 STUDENT TEMPLATE: complete the parts marked TODO
 *
 *   FIR band-pass by the window method (rect, Hann, Hamming, Blackman, Kaiser)
 *   IIR Butterworth band-pass: analog prototype -> LP/BP transformation ->
 *       bilinear transform with pre-warping -> cascade of biquads
 *   2nd-order band-pass (single biquad) with given centre and bandwidth
 *   frequency response, group delay, coefficient quantisation, poles
 *
 * Conventions
 *   biquad  H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
 *           stored as b[0..2], a[0..2] with a[0] = 1
 *   cascade of ns biquads: H(z) = prod_k H_k(z)
 *   frequencies in Hz, fs = sampling rate in Hz
 *
 * Plain C99 + libm, no complex.h (own small complex type).
 */
#ifndef FILTDESIGN_H
#define FILTDESIGN_H

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__GNUC__)
#define FD_STATIC static __attribute__((unused))
#else
#define FD_STATIC static
#endif

/* ------------------------------------------------------------------ */
/* complex helpers                                                     */
/* ------------------------------------------------------------------ */
typedef struct { double re, im; } cplx;

FD_STATIC cplx c_make(double re, double im) { cplx z; z.re = re; z.im = im; return z; }
FD_STATIC cplx c_add(cplx a, cplx b) { return c_make(a.re + b.re, a.im + b.im); }
FD_STATIC cplx c_sub(cplx a, cplx b) { return c_make(a.re - b.re, a.im - b.im); }
FD_STATIC cplx c_mul(cplx a, cplx b)
{
    return c_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}
FD_STATIC cplx c_div(cplx a, cplx b)
{
    double d = b.re * b.re + b.im * b.im;
    return c_make((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d);
}
FD_STATIC cplx c_scale(cplx a, double s) { return c_make(a.re * s, a.im * s); }
FD_STATIC double c_abs(cplx a) { return hypot(a.re, a.im); }
FD_STATIC cplx c_sqrt(cplx a)          /* principal square root */
{
    double r = c_abs(a), x = sqrt(0.5 * (r + a.re)), y = sqrt(0.5 * (r - a.re));
    return c_make(x, a.im < 0.0 ? -y : y);
}
FD_STATIC cplx c_expj(double w) { return c_make(cos(w), sin(w)); }

/* ------------------------------------------------------------------ */
/* biquad type                                                         */
/* ------------------------------------------------------------------ */
typedef struct { double b[3], a[3]; } biquad_t;

#define FD_MAXSEC 16

/* ------------------------------------------------------------------ */
/* windows                                                             */
/* ------------------------------------------------------------------ */
enum { WIN_RECT, WIN_HANN, WIN_HAMMING, WIN_BLACKMAN, WIN_KAISER, WIN_COUNT };
static const char *fd_win_name[WIN_COUNT] = {"rect", "Hann", "Hamming", "Blackman", "Kaiser"};

/* modified Bessel function I0 (power series) */
FD_STATIC double fd_bessel_i0(double x)
{
    double s = 1.0, t = 1.0, q = 0.25 * x * x;
    int k;
    for (k = 1; k < 60; k++) {
        t *= q / ((double)k * k);
        s += t;
        if (t < 1e-14 * s) break;
    }
    return s;
}

/* Kaiser's empirical formulas: beta and length N for attenuation As (dB)
   and transition width df (Hz) */
FD_STATIC double fd_kaiser_beta(double As)
{
    if (As > 50.0) return 0.1102 * (As - 8.7);
    if (As >= 21.0) return 0.5842 * pow(As - 21.0, 0.4) + 0.07886 * (As - 21.0);
    return 0.0;
}
FD_STATIC int fd_kaiser_length(double As, double df, double fs)
{
    double dw = 2.0 * M_PI * df / fs;
    return (int)ceil((As - 8.0) / (2.285 * dw)) + 1;
}

/* window value w[n], n = 0..N-1 (symmetric) */
FD_STATIC double fd_window(int type, int n, int N, double beta)
{
    double x = (N > 1) ? (double)n / (N - 1) : 0.5;     /* 0..1 */
    switch (type) {
    /* TODO (a): Hann, Hamming, Blackman and Kaiser windows as functions of
       x = n/(N-1) in [0, 1]. Kaiser: I0(beta sqrt(1 - u^2)) / I0(beta)
       with u = 2x - 1 (use fd_bessel_i0). */
    default:           (void)x; (void)beta; return 1.0;
    }
}

/* ------------------------------------------------------------------ */
/* frequency responses                                                 */
/* ------------------------------------------------------------------ */
FD_STATIC cplx fd_fir_resp(const double *h, int N, double f, double fs)
{
    cplx H = c_make(0.0, 0.0);
    double w = 2.0 * M_PI * f / fs;
    int n;
    for (n = 0; n < N; n++) H = c_add(H, c_scale(c_expj(-w * n), h[n]));
    return H;
}

FD_STATIC cplx fd_biquad_resp(const biquad_t *s, double f, double fs)
{
    double w = 2.0 * M_PI * f / fs;
    cplx z1 = c_expj(-w), z2 = c_expj(-2.0 * w);
    cplx num = c_add(c_make(s->b[0], 0.0), c_add(c_scale(z1, s->b[1]), c_scale(z2, s->b[2])));
    cplx den = c_add(c_make(s->a[0], 0.0), c_add(c_scale(z1, s->a[1]), c_scale(z2, s->a[2])));
    return c_div(num, den);
}

FD_STATIC cplx fd_sos_resp(const biquad_t *s, int ns, double f, double fs)
{
    cplx H = c_make(1.0, 0.0);
    int k;
    for (k = 0; k < ns; k++) H = c_mul(H, fd_biquad_resp(&s[k], f, fs));
    return H;
}

FD_STATIC double fd_db(double mag) { return 20.0 * log10(mag > 1e-12 ? mag : 1e-12); }

/* group delay in samples by numerical differentiation of the phase */
FD_STATIC double fd_sos_grpdelay(const biquad_t *s, int ns, double f, double fs)
{
    double df = fs * 1e-6;
    cplx H1 = fd_sos_resp(s, ns, f - df, fs), H2 = fd_sos_resp(s, ns, f + df, fs);
    double dphi = atan2(H2.im, H2.re) - atan2(H1.im, H1.re);
    while (dphi > M_PI) dphi -= 2.0 * M_PI;
    while (dphi < -M_PI) dphi += 2.0 * M_PI;
    return -dphi / (2.0 * M_PI * 2.0 * df / fs);
}

/* ------------------------------------------------------------------ */
/* FIR band-pass, window method                                        */
/* ------------------------------------------------------------------ */
/* h[0..N-1]: ideal band-pass f1..f2 (cut-off frequencies, Hz), shifted by
   (N-1)/2 samples and multiplied by the window; afterwards normalised to
   unit gain at the band centre (f1+f2)/2. N odd -> linear phase type I. */
FD_STATIC void fd_fir_bandpass(double *h, int N, double f1, double f2, double fs,
                            int win, double beta)
{
    double w1 = 2.0 * M_PI * f1 / fs, w2 = 2.0 * M_PI * f2 / fs, g;
    int n;
    for (n = 0; n < N; n++) {
        double m = n - 0.5 * (N - 1);
        /* TODO (a): ideal band-pass impulse response h_d[m] for the
           cut-off frequencies w1, w2 (rad/sample); careful at m = 0 */
        double hd = (fabs(m) < 1e-9) ? 1.0 : 0.0;
        (void)w1; (void)w2;
        h[n] = hd * fd_window(win, n, N, beta);
    }
    g = c_abs(fd_fir_resp(h, N, 0.5 * (f1 + f2), fs));
    for (n = 0; n < N; n++) h[n] /= g;
}

/* ------------------------------------------------------------------ */
/* IIR band-pass designs                                               */
/* ------------------------------------------------------------------ */
/* pre-warping: analog frequency (rad/s) that the bilinear transform
   s = 2 fs (z-1)/(z+1) maps onto the digital frequency f */
FD_STATIC double fd_prewarp(double f, double fs)
{
    /* TODO (b): pre-warping. Without it (as here) the band edges move. */
    (void)fs;
    return 2.0 * M_PI * f;
}

/* bilinear transform of one analog pole s -> z = (2fs + s)/(2fs - s) */
FD_STATIC cplx fd_bilinear(cplx s, double fs)
{
    return c_div(c_make(2.0 * fs + s.re, s.im), c_make(2.0 * fs - s.re, -s.im));
}

/* peak magnitude of a cascade on a frequency grid (0..fs/2) */
FD_STATIC double fd_sos_peak(const biquad_t *s, int ns, double fs, double *fpk)
{
    double pk = 0.0, f, m;
    int i, M = 4000;
    for (i = 0; i <= M; i++) {
        f = 0.5 * fs * i / M;
        m = c_abs(fd_sos_resp(s, ns, f, fs));
        if (m > pk) { pk = m; if (fpk) *fpk = f; }
    }
    return pk;
}

/* minimum Butterworth prototype order n for a band-pass with pass band
   fp1..fp2 (max. Ap dB loss) and stop-band edges fs1 < fp1, fs2 > fp2
   (min. As dB). Edges are pre-warped. Returns n; *lam_s = prototype
   stop-band frequency (normalised). */
FD_STATIC int fd_butter_bp_order(double fp1, double fp2, double fs1, double fs2,
                              double Ap, double As, double fs, double *lam_s)
{
    double W1 = fd_prewarp(fp1, fs), W2 = fd_prewarp(fp2, fs);
    double W0sq = W1 * W2, B = W2 - W1;
    double Ws1 = fd_prewarp(fs1, fs), Ws2 = fd_prewarp(fs2, fs);
    double l1 = fabs(Ws1 * Ws1 - W0sq) / (B * Ws1);
    double l2 = fabs(Ws2 * Ws2 - W0sq) / (B * Ws2);
    double ls = l1 < l2 ? l1 : l2;
    double num = log10((pow(10.0, As / 10.0) - 1.0) / (pow(10.0, Ap / 10.0) - 1.0));
    if (lam_s) *lam_s = ls;
    return (int)ceil(num / (2.0 * log10(ls)));
}

/* Butterworth band-pass of order 2n as n biquads.
   fp1, fp2 : pass-band edges (Hz) where the loss is exactly Ap dB
   The LP prototype poles  p_k = eps^(-1/n) exp(j pi (2k+n+1)/(2n))  are
   transformed with s -> (s^2 + W0^2)/(B s): every prototype pole p gives
   the two band-pass poles  s = (pB +- sqrt(p^2 B^2 - 4 W0^2))/2 .
   Bilinear transform with pre-warped W1, W2. Each biquad gets the zeros
   z = +1 and z = -1 (numerator 1 - z^-2) and is scaled to peak gain 1
   (L-infinity scaling, important for fixed point); the last section
   carries the remaining gain so that the overall pass-band gain is 1.
   Sections are ordered by increasing pole radius.  Returns n. */
FD_STATIC int fd_butter_bandpass(biquad_t *sec, int n, double fp1, double fp2,
                              double Ap, double fs)
{
    double W1 = fd_prewarp(fp1, fs), W2 = fd_prewarp(fp2, fs);
    double W0 = sqrt(W1 * W2), B = W2 - W1;
    double eps = sqrt(pow(10.0, Ap / 10.0) - 1.0);
    double rp = pow(eps, -1.0 / n);                /* prototype pole radius */
    cplx zp[2 * FD_MAXSEC];
    int k, ns = 0, i, j;
    double g, pk;

    for (k = 0; k < n; k++) {
        cplx p = c_scale(c_expj(M_PI * (2.0 * k + n + 1) / (2.0 * n)), rp);
        if (p.im < -1e-12) continue;               /* use upper half + real */
        /* TODO (b): transform the prototype pole p into the band-pass
           pole(s) s = (pB +- sqrt(p^2 B^2 - 4 W0^2))/2 (use c_mul, c_sqrt,
           ...), map them with fd_bilinear() and store them in zp[ns++]:
           a real p gives ONE conjugate pair (store the root with Im >= 0),
           a complex p gives TWO pairs (store both roots).
           Placeholder: a pole at the origin. */
        (void)B; (void)W0;
        zp[ns++] = c_make(0.0, 0.0);
    }
    /* sort by pole radius (selection sort) */
    for (i = 0; i < ns; i++)
        for (j = i + 1; j < ns; j++)
            if (c_abs(zp[j]) < c_abs(zp[i])) { cplx t = zp[i]; zp[i] = zp[j]; zp[j] = t; }
    for (k = 0; k < ns; k++) {
        sec[k].a[0] = 1.0;
        sec[k].a[1] = -2.0 * zp[k].re;
        sec[k].a[2] = zp[k].re * zp[k].re + zp[k].im * zp[k].im;
        sec[k].b[0] = 1.0; sec[k].b[1] = 0.0; sec[k].b[2] = -1.0;
        pk = fd_sos_peak(&sec[k], 1, fs, NULL);
        for (i = 0; i < 3; i++) sec[k].b[i] /= pk;
    }
    /* overall gain: Butterworth peak = 1 (at the geometric centre) */
    g = fd_sos_peak(sec, ns, fs, NULL);
    for (i = 0; i < 3; i++) sec[ns - 1].b[i] /= g;
    return ns;
}

/* single 2nd-order band-pass (resonator) with centre f0 and -3 dB
   bandwidth Bw (both Hz): analog prototype H(s) = B s / (s^2 + B s + W0^2),
   bilinear transform with pre-warped -3 dB edges. Peak gain 1 at f0. */
FD_STATIC void fd_bp2(biquad_t *s, double f0, double Bw, double fs)
{
    /* The -3 dB edges W1, W2 of the analog band-pass satisfy W1*W2 = W0^2,
       W2 - W1 = B. Choose the digital edges fl, fu = fl + Bw such that
       their pre-warped values have the geometric mean W0 = prewarp(f0)
       (bisection on fl). Then peak and -3 dB edges are exact.          */
    double W0 = fd_prewarp(f0, fs), lo = f0 - Bw, hi = f0, fl = f0, W1, W2;
    double W0sq, B, c = 2.0 * fs, d;
    int it;
    if (lo < 0.0) lo = 0.0;
    for (it = 0; it < 60; it++) {
        fl = 0.5 * (lo + hi);
        if (fd_prewarp(fl, fs) * fd_prewarp(fl + Bw, fs) > W0 * W0) hi = fl; else lo = fl;
    }
    W1 = fd_prewarp(fl, fs); W2 = fd_prewarp(fl + Bw, fs);
    W0sq = W0 * W0; B = W2 - W1;
    d = c * c + B * c + W0sq;
    /* TODO (c): coefficients of the bilinear transform of
       H(s) = B s / (s^2 + B s + W0^2) with s = c (1 - z^-1)/(1 + z^-1),
       normalised to a0 = 1 (d = c^2 + B c + W0^2 is the z^0 term of the
       denominator). Placeholder: H(z) = 1. */
    (void)d;
    s->b[0] = 1.0; s->b[1] = 0.0; s->b[2] = 0.0;
    s->a[0] = 1.0; s->a[1] = 0.0; s->a[2] = 0.0;
}

/* poles of a biquad (complex pair or two real) */
FD_STATIC void fd_biquad_poles(const biquad_t *s, cplx *p1, cplx *p2)
{
    double a1 = s->a[1], a2 = s->a[2], D = a1 * a1 - 4.0 * a2;
    if (D < 0.0) { *p1 = c_make(-0.5 * a1, 0.5 * sqrt(-D)); *p2 = c_make(-0.5 * a1, -0.5 * sqrt(-D)); }
    else { *p1 = c_make(-0.5 * a1 + 0.5 * sqrt(D), 0.0); *p2 = c_make(-0.5 * a1 - 0.5 * sqrt(D), 0.0); }
}

/* ------------------------------------------------------------------ */
/* quantisation helpers                                                */
/* ------------------------------------------------------------------ */
/* round x to a signed fixed-point number with 'frac' fractional bits
   and total word length 'bits' (saturating) */
FD_STATIC double fd_quant(double x, int bits, int frac)
{
    double q = floor(x * pow(2.0, frac) + 0.5);
    double mx = pow(2.0, bits - 1) - 1.0, mn = -pow(2.0, bits - 1);
    if (q > mx) q = mx;
    if (q < mn) q = mn;
    return q / pow(2.0, frac);
}

#endif /* FILTDESIGN_H */
