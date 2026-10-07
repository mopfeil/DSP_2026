/*
 * Sheet 5, Exercise 5.3 b) -- Anti-aliasing filter before the ADC
 * (reference solution)
 *
 * Build and run:   gcc -O2 -o antialias antialias.c -lm
 *                  ./antialias
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 * The analog world is represented on a fine grid with 1 MHz. The analog
 * anti-aliasing (AA) filter is simulated on this grid, then the ADC takes
 * every (1 MHz / fs)-th value. Three input signals are used (the filter is
 * linear, so each effect can be measured separately):
 *   1. knock tones 6.5 kHz (1 V) and 10.5 kHz (0.5 V)  -> passband loss
 *   2. a disturbance tone at f_d = fs - 6.5 kHz (0.5 V), e.g. ignition or
 *      injector driver interference; it aliases exactly onto 6.5 kHz
 *   3. white noise, sigma = 0.1 V, bandwidth 500 kHz  -> aliased noise in
 *      the knock band 5..11 kHz
 * Filters: none, RC (1st order), Butterworth 2nd and 4th order, all with
 * cut-off frequency FC; sampling rates 25 kHz and 50 kHz.
 */
#include <stdio.h>
#include <math.h>
#include "asciiplot.h"
#include "csvio.h"

#define FF    1.0e6          /* fine-grid rate ("analog" time), Hz    */
#define TREC  0.2            /* record length, s                      */
#define NFG   200000         /* = FF * TREC                           */
#define TSKIP 0.01           /* skip filter transient, s              */
#define FC    12000.0        /* AA cut-off frequency, Hz              */

static double xin[NFG], yout[NFG];

/* ---------------- random numbers for the white noise ---------------- */
static unsigned int rng = 88172645u;
static double grand(void)
{
    double u1, u2;
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    u1 = ((rng >> 8) + 0.5) / 16777216.0;
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    u2 = ((rng >> 8) + 0.5) / 16777216.0;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ---------------- analog filter sections -------------------------- */
/* (a) RC low-pass  y' = wc (x - y), input x constant during dt:
   exact solution of the ODE over one step                            */
static void rc_filter(double *y, const double *x, int n, double fc, double dt)
{
    double a = exp(-2.0 * M_PI * fc * dt), s = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        s = a * s + (1.0 - a) * x[i];
        y[i] = s;
    }
}

/* 2nd-order low-pass section  y'' + (w0/Q) y' + w0^2 y = w0^2 x,
   integrated with RK4 (input held constant during dt) -- given        */
static void biquad_filter(double *y, const double *x, int n, double f0,
                          double Q, double dt)
{
    double w0 = 2.0 * M_PI * f0, p = 0.0, v = 0.0;   /* state: y, y'   */
    int i;
    for (i = 0; i < n; i++) {
        double u = x[i], k1p, k1v, k2p, k2v, k3p, k3v, k4p, k4v;
        k1p = v;                 k1v = w0 * w0 * (u - p) - w0 / Q * v;
        k2p = v + 0.5 * dt * k1v;
        k2v = w0 * w0 * (u - (p + 0.5 * dt * k1p)) - w0 / Q * (v + 0.5 * dt * k1v);
        k3p = v + 0.5 * dt * k2v;
        k3v = w0 * w0 * (u - (p + 0.5 * dt * k2p)) - w0 / Q * (v + 0.5 * dt * k2v);
        k4p = v + dt * k3v;
        k4v = w0 * w0 * (u - (p + dt * k3p)) - w0 / Q * (v + dt * k3v);
        p += dt / 6.0 * (k1p + 2 * k2p + 2 * k3p + k4p);
        v += dt / 6.0 * (k1v + 2 * k2v + 2 * k3v + k4v);
        y[i] = p;
    }
}

/* (b) AA filter of a given order: 0 = none, 1 = RC, 2/4 = Butterworth  */
static void aa_filter(double *y, const double *x, int n, int order)
{
    static double tmp[NFG];
    double dt = 1.0 / FF;
    int i;
    switch (order) {
    case 0: for (i = 0; i < n; i++) y[i] = x[i]; break;
    case 1: rc_filter(y, x, n, FC, dt); break;
    case 2: biquad_filter(y, x, n, FC, 1.0 / sqrt(2.0), dt); break;
    case 4: /* Q_k = 1 / (2 sin((2k-1) pi / (2n))), k = 1, 2           */
        biquad_filter(tmp, x, n, FC, 1.0 / (2.0 * sin(M_PI / 8.0)), dt);
        biquad_filter(y, tmp, n, FC, 1.0 / (2.0 * sin(3.0 * M_PI / 8.0)), dt);
        break;
    }
}

/* (c) amplitude of the frequency component f in the SAMPLED signal
   (ADC takes every D-th fine-grid value, after the transient)        */
static double sampled_amp(const double *y, int D, double f)
{
    double fs = FF / D, re = 0.0, im = 0.0;
    int n, n0 = (int)(TSKIP * fs), N = (int)(TREC * fs) - n0;
    for (n = 0; n < N; n++) {
        double v = y[(n0 + n) * D], w = 2.0 * M_PI * f * n / fs;
        re += v * cos(w);
        im -= v * sin(w);
    }
    return 2.0 * sqrt(re * re + im * im) / N;
}

/* (d) mean noise PSD in the band f1..f2 of the SAMPLED signal, relative
   to the PSD the analog noise would have without filter and without
   aliasing (sigma^2/FF two-sided): averaged periodogram |X(f)|^2 / N   */
static double band_noise_ratio(const double *y, int D, double f1, double f2,
                               double sigma)
{
    double fs = FF / D, acc = 0.0, f;
    int n, n0 = (int)(TSKIP * fs), N = (int)(TREC * fs) - n0, nf = 0;
    for (f = f1; f <= f2 + 1e-9; f += 25.0, nf++) {
        double re = 0.0, im = 0.0;
        for (n = 0; n < N; n++) {
            double v = y[(n0 + n) * D], w = 2.0 * M_PI * f * n / fs;
            re += v * cos(w);
            im -= v * sin(w);
        }
        acc += (re * re + im * im) / N;
    }
    return acc / nf / (sigma * sigma * fs / FF);
}

/* analytic magnitude response of the filters */
static double H_mag(int order, double f)
{
    double r = f / FC;
    return order == 0 ? 1.0 : 1.0 / sqrt(1.0 + pow(r, 2.0 * order));
}

int main(void)
{
    static const int orders[4] = {0, 1, 2, 4};
    static const double fsv[2] = {25000.0, 50000.0};
    double sigma = 0.1;
    int io, is, i;

    printf("AA filter cut-off fc = %.0f Hz, analog grid %.0f MHz, record %.1f s\n",
           FC, FF / 1e6, TREC);
    for (is = 0; is < 2; is++) {
        double fs = fsv[is], fd = fs - 6500.0;
        int D = (int)(FF / fs + 0.5);
        printf("\nfs = %.0f Hz: disturbance at f_d = %.0f Hz aliases onto 6.5 kHz;"
               " first frequency folding into 0..10.5 kHz: %.1f kHz\n",
               fs, fd, (fs - 10500.0) / 1e3);
        printf("  order | gain 6.5k  gain 10.5k | alias of f_d (A_out/A_in) | "
               "theory |H(f_d)| | noise PSD 5-11 kHz\n");
        for (io = 0; io < 4; io++) {
            int ord = orders[io];
            double g1, g2, ga, nv;
            /* 1. passband: knock tones */
            for (i = 0; i < NFG; i++)
                xin[i] = sin(2 * M_PI * 6500.0 * i / FF) + 0.5 * sin(2 * M_PI * 10500.0 * i / FF + 1.0);
            aa_filter(yout, xin, NFG, ord);
            g1 = sampled_amp(yout, D, 6500.0) / 1.0;
            g2 = sampled_amp(yout, D, 10500.0) / 0.5;
            /* 2. disturbance at fs - 6.5 kHz: measure what appears at 6.5 kHz */
            for (i = 0; i < NFG; i++) xin[i] = 0.5 * sin(2 * M_PI * fd * i / FF);
            aa_filter(yout, xin, NFG, ord);
            ga = sampled_amp(yout, D, 6500.0) / 0.5;
            /* 3. white noise: in-band PSD relative to the unaliased one */
            for (i = 0; i < NFG; i++) xin[i] = sigma * grand();
            aa_filter(yout, xin, NFG, ord);
            nv = band_noise_ratio(yout, D, 5000.0, 11000.0, sigma);
            printf("    %d   | %6.2f dB  %6.2f dB  |   %7.4f  = %6.1f dB     | %6.1f dB   |  %+5.1f dB\n",
                   ord, 20 * log10(g1), 20 * log10(g2), ga, 20 * log10(ga),
                   20 * log10(H_mag(ord, fd)), 10 * log10(nv));
        }
    }
    return 0;
}
