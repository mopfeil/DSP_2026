/*
 * ar.h -- autoregressive (AR) modelling and maximum entropy spectra
 *         (Sheet 11, reference solution)
 *
 * AR(p) model   x[n] = -a[1] x[n-1] - ... - a[p] x[n-p] + e[n],
 *               A(z) = 1 + a[1] z^-1 + ... + a[p] z^-p,  a[0] = 1
 * MEM spectrum  P(f) = E_p T_s / |A(e^{j 2 pi f/fs})|^2
 *
 *   ar_autocorr(x, N, r, p)        biased autocorrelation r[0..p]
 *   ar_levinson(r, p, a, k, Ep)    Yule-Walker via Levinson-Durbin
 *   ar_burg(x, N, p, a, k, Ep)     Burg's method
 *   ar_psd(a, p, E, f, fs)         MEM power spectral density at f
 *   ar_crit(E, N, p, type)         order criteria FPE, AIC, MDL
 *   per_psd(x, N, nfft, win, P)    (windowed) periodogram via fft.h
 *   peak_in_band(P, nf, df, f1, f2) frequency of the maximum in [f1,f2]
 *
 * All functions return 0 on success, -1 on error (e.g. |k| >= 1).
 * E/Ep arrays have p+1 entries: E[m] = prediction error power of order m.
 */
#ifndef AR_H
#define AR_H

#include <math.h>
#include <stdlib.h>
#include "fft.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__GNUC__)
#define AR_STATIC static __attribute__((unused))
#else
#define AR_STATIC static
#endif

#define AR_MAXP 64

/* biased autocorrelation estimate r[m] = 1/N sum_n x[n] x[n+m] */
AR_STATIC void ar_autocorr(const double *x, int N, double *r, int p)
{
    int m, n;
    for (m = 0; m <= p; m++) {
        double s = 0.0;
        for (n = 0; n + m < N; n++) s += x[n] * x[n + m];
        r[m] = s / N;
    }
}

/* Levinson-Durbin recursion: solves the Yule-Walker equations
   sum_i a[i] r[|m-i|] = -r[m], m = 1..p, in O(p^2).
   a[0..p] (a[0] = 1), k[1..p] reflection coefficients, E[0..p] errors */
AR_STATIC int ar_levinson(const double *r, int p, double *a, double *k, double *E)
{
    double tmp[AR_MAXP + 1];
    int m, i;
    if (p > AR_MAXP || r[0] <= 0.0) return -1;
    a[0] = 1.0;
    E[0] = r[0];
    for (m = 1; m <= p; m++) {
        double acc = r[m];
        for (i = 1; i < m; i++) acc += a[i] * r[m - i];
        k[m] = -acc / E[m - 1];
        for (i = 1; i < m; i++) tmp[i] = a[i] + k[m] * a[m - i];
        for (i = 1; i < m; i++) a[i] = tmp[i];
        a[m] = k[m];
        E[m] = (1.0 - k[m] * k[m]) * E[m - 1];
        if (fabs(k[m]) >= 1.0) return -1;
    }
    return 0;
}

/* Burg's method: minimises forward + backward prediction error power
   directly on the data (no autocorrelation estimate, no windowing).
   k[m] = -2 sum f_{m-1}[n] b_{m-1}[n-1] / sum (f_{m-1}[n]^2 + b_{m-1}[n-1]^2) */
AR_STATIC int ar_burg(const double *x, int N, int p, double *a, double *k, double *E)
{
    double *f, *b, tmp[AR_MAXP + 1];
    int m, n, i;
    if (p > AR_MAXP || p >= N) return -1;
    f = (double *)malloc(N * sizeof(double));
    b = (double *)malloc(N * sizeof(double));
    a[0] = 1.0;
    E[0] = 0.0;
    for (n = 0; n < N; n++) { f[n] = b[n] = x[n]; E[0] += x[n] * x[n]; }
    E[0] /= N;
    for (m = 1; m <= p; m++) {
        double num = 0.0, den = 0.0;
        for (n = m; n < N; n++) {
            num += f[n] * b[n - 1];
            den += f[n] * f[n] + b[n - 1] * b[n - 1];
        }
        k[m] = den > 0.0 ? -2.0 * num / den : 0.0;
        /* update errors: go backwards in n so b[n-1] is still the old value */
        for (n = N - 1; n >= m; n--) {
            double fo = f[n];
            f[n] = fo + k[m] * b[n - 1];
            b[n] = b[n - 1] + k[m] * fo;
        }
        /* Levinson update of the polynomial */
        for (i = 1; i < m; i++) tmp[i] = a[i] + k[m] * a[m - i];
        for (i = 1; i < m; i++) a[i] = tmp[i];
        a[m] = k[m];
        E[m] = (1.0 - k[m] * k[m]) * E[m - 1];
    }
    free(f); free(b);
    return 0;
}

/* MEM / AR power spectral density (one-sided, V^2/Hz) */
AR_STATIC double ar_psd(const double *a, int p, double E, double f, double fs)
{
    double w = 2.0 * M_PI * f / fs, re = 0.0, im = 0.0;
    int i;
    for (i = 0; i <= p; i++) { re += a[i] * cos(w * i); im -= a[i] * sin(w * i); }
    return 2.0 * E / fs / (re * re + im * im);
}

/* order selection criteria */
enum { CRIT_FPE, CRIT_AIC, CRIT_MDL };
AR_STATIC double ar_crit(double Ep, int N, int p, int type)
{
    switch (type) {
    case CRIT_FPE: return Ep * (N + p + 1.0) / (N - p - 1.0);
    case CRIT_AIC: return N * log(Ep) + 2.0 * p;
    default:       return N * log(Ep) + p * log((double)N);
    }
}

/* window w[n], n = 0..N-1: 0 = rectangular, 1 = Hann */
AR_STATIC double ar_win(int type, int n, int N)
{
    return type == 1 ? 0.5 - 0.5 * cos(2.0 * M_PI * (n + 0.5) / N) : 1.0;
}

/* periodogram of the windowed record, zero padded to nfft (power of 2):
   P[k], k = 0..nfft/2, one-sided PSD in V^2/Hz at f = k fs/nfft */
AR_STATIC void per_psd(const double *x, int N, int nfft, int win, double fs, double *P)
{
    double *re = (double *)calloc(nfft, sizeof(double));
    double *im = (double *)calloc(nfft, sizeof(double));
    double u = 0.0, mean = 0.0;
    int n, kk;
    for (n = 0; n < N; n++) mean += x[n] / N;
    for (n = 0; n < N; n++) {
        double w = ar_win(win, n, N);
        re[n] = (x[n] - mean) * w;
        u += w * w;
    }
    fft(re, im, nfft, 0);
    for (kk = 0; kk <= nfft / 2; kk++)
        P[kk] = 2.0 * (re[kk] * re[kk] + im[kk] * im[kk]) / (fs * u);
    free(re); free(im);
}

/* frequency of the largest value of P (sampled at 0, df, 2 df, ...)
   inside [f1, f2], refined by a parabola through the three points
   around the maximum (in dB) */
AR_STATIC double peak_in_band(const double *P, int nf, double df, double f1, double f2)
{
    int i, i1 = (int)ceil(f1 / df), i2 = (int)floor(f2 / df), im = -1;
    double d;
    if (i1 < 1) i1 = 1;
    if (i2 > nf - 2) i2 = nf - 2;
    for (i = i1; i <= i2; i++) if (im < 0 || P[i] > P[im]) im = i;
    if (im < 0) return -1.0;
    {
        double l = log(P[im - 1]), c = log(P[im]), r = log(P[im + 1]);
        double den = l - 2.0 * c + r;
        d = den < 0.0 ? 0.5 * (l - r) / den : 0.0;
    }
    return (im + d) * df;
}

#endif /* AR_H */
