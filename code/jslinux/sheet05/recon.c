/*
 * Sheet 5, Exercise 5.3 a) -- Reconstruction: zero-order hold, linear
 * interpolation and (windowed) sinc interpolation
 * (student template -- complete the parts marked TODO)
 *
 * Build and run:   gcc -O2 -o recon recon.c -lm
 *                  ./recon
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 * A "continuous" signal is represented on a fine grid (M = 40 points per
 * sampling interval, i.e. 1 MHz for fs = 25 kHz). It is sampled at fs and
 * reconstructed on the fine grid with
 *   ZOH    x_r(t) = x[n]                      for nT <= t < (n+1)T
 *   LIN    straight line between x[n] and x[n+1]
 *   SINC   x_r(t) = sum_{|k-n|<=K} x[k] sinc((t - kT)/T)       (truncated)
 *   WSINC  as SINC, but each term weighted with a Hann window of width 2K+1
 * The normalised mean squared error  NMSE = sum e^2 / sum x^2  is computed
 * over the central part of the record (away from the edges).
 */
#include <stdio.h>
#include <math.h>
#include "asciiplot.h"
#include "csvio.h"

#define FS   25000.0       /* sampling rate, Hz                  */
#define M    40            /* fine-grid points per sample period */
#define NSMP 600           /* number of samples (24 ms)          */
#define NF   (NSMP * M)    /* fine-grid length                   */
#define K    16            /* half length of the sinc kernel     */

static double xc[NF], xr[NF], xs[NSMP];

static double sinc(double u)
{
    return fabs(u) < 1e-12 ? 1.0 : sin(M_PI * u) / (M_PI * u);
}

/* ---- reconstruction methods; result on the fine grid in xr[] -------- */
static void rec_zoh(void)
{
    /* TODO (a): x_r = x[n] for the fine-grid points i of interval n = i/M */
    int i;
    for (i = 0; i < NF; i++) xr[i] = 0.0;
}

static void rec_lin(void)
{
    /* TODO (a): straight line between xs[n] and xs[n+1], u = (i % M) / M */
    int i;
    for (i = 0; i < NF; i++) xr[i] = 0.0;
}

static void rec_sinc(int windowed)
{
    int i, k;
    for (i = 0; i < NF; i++) {
        double tn = (double)i / M;              /* time in sample periods */
        int n = i / M;
        double s = 0.0;
        /* TODO (a): sum over k = n-K .. n+K+1 (inside 0..NSMP-1) of
           xs[k] * sinc(tn - k) * w, with w = 1 (plain) or, if windowed,
           w = 0.5 + 0.5 cos(pi u / (K+1)) for |u| < K+1 (else 0)        */
        (void)k; (void)tn; (void)n; (void)windowed; (void)sinc;
        xr[i] = s;
    }
}

/* NMSE in dB over the central part (edges of 2K samples excluded) */
static double nmse_db(void)
{
    double se = 0.0, sx = 0.0;
    int i;
    for (i = 2 * K * M; i < NF - 2 * K * M; i++) {
        se += (xr[i] - xc[i]) * (xr[i] - xc[i]);
        sx += xc[i] * xc[i];
    }
    return 10.0 * log10(se / sx);
}

/* fill xc (fine grid) with a sine of frequency f, and xs with its samples */
static void make_sine(double f)
{
    int i;
    for (i = 0; i < NF; i++) xc[i] = sin(2.0 * M_PI * f * i / (M * FS) + 0.7);
    for (i = 0; i < NSMP; i++) xs[i] = xc[i * M];
}

/* dominant frequency of xr from its zero crossings (central part) */
static double zc_freq(void)
{
    int i, nz = 0, i0 = -1, i1 = -1;
    for (i = 2 * K * M + 1; i < NF - 2 * K * M; i++)
        if ((xr[i - 1] < 0.0) != (xr[i] < 0.0)) {
            if (i0 < 0) i0 = i;
            i1 = i; nz++;
        }
    return nz > 1 ? (nz - 1) / 2.0 / ((double)(i1 - i0) / (M * FS)) : 0.0;
}

int main(void)
{
    static const double ftest[] = {500, 1000, 2500, 5000, 8000, 10500, 12000};
    static double fcol[7], ezoh[7], ezth[7], elin[7], esinc[7], ewsinc[7];
    int j, nt = 7, i;

    printf("a) fs = %.0f Hz, fine grid %d x oversampled, sinc kernel +-%d samples\n",
           FS, M, K);
    printf("   f [Hz]   f/fs    ZOH     ZOH theory   LIN      SINC     WSINC   (NMSE in dB)\n");
    for (j = 0; j < nt; j++) {
        double wT = 2.0 * M_PI * ftest[j] / FS;
        make_sine(ftest[j]);
        rec_zoh();      ezoh[j]  = nmse_db();
        rec_lin();      elin[j]  = nmse_db();
        rec_sinc(0);    esinc[j] = nmse_db();
        rec_sinc(1);    ewsinc[j] = nmse_db();
        ezth[j] = 10.0 * log10(2.0 * (1.0 - sin(wT) / wT));
        fcol[j] = ftest[j];
        printf("  %6.0f   %5.3f  %7.2f  %7.2f    %7.2f  %7.2f  %7.2f\n", ftest[j],
               ftest[j] / FS, ezoh[j], ezth[j], elin[j], esinc[j], ewsinc[j]);
    }
    csv_write("recon_nmse.csv", "f,zoh,zoh_theory,lin,sinc,wsinc", nt, 6,
              fcol, ezoh, ezth, elin, esinc, ewsinc);

    /* plot: 8 kHz sine, 0.4 ms: original vs. ZOH and vs. windowed sinc */
    make_sine(8000.0);
    rec_zoh();
    ap_plot2(xc + 400 * M, xr + 400 * M, 10 * M,
             "8 kHz, fs = 25 kHz, 0.4 ms: original (*) and ZOH (o)");
    rec_lin();
    ap_plot2(xc + 400 * M, xr + 400 * M, 10 * M,
             "8 kHz: original (*) and linear interpolation (o)");
    rec_sinc(1);
    ap_plot2(xc + 400 * M, xr + 400 * M, 10 * M,
             "8 kHz: original (*) and windowed-sinc reconstruction (o)");

    /* b) a tone above fs/2 */
    printf("\nb) 15 kHz tone sampled at %.0f Hz and sinc-reconstructed:\n", FS);
    make_sine(15000.0);
    rec_sinc(1);
    printf("   NMSE = %.2f dB, zero-crossing frequency of the original %.0f Hz,"
           " of the reconstruction %.0f Hz\n", nmse_db(),
           (double)15000.0, zc_freq());
    /* sin(2 pi 15000 n/fs + 0.7) = sin(2 pi (15000 - 25000) n/fs + 0.7)       */
    for (i = 0; i < NF; i++) xc[i] = sin(-2.0 * M_PI * 10000.0 * i / (M * FS) + 0.7);
    printf("   NMSE against the alias sin(-2 pi 10 kHz t + 0.7): %.2f dB\n", nmse_db());
    return 0;
}
