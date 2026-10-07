/*
 * Sheet 1, Exercise 1.2 -- Signal zoo (REFERENCE SOLUTION)
 *
 * Build and run:   gcc -O2 -o signal_zoo signal_zoo.c -lm
 *                  ./signal_zoo
 * (engine_signals.h, asciiplot.h and csvio.h in the same directory)
 *
 * Generates the five sensor signals of the virtual engine, plots them,
 * prints simple statistics and estimates the period of each signal from
 * the peak of its normalised autocorrelation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS      25000.0     /* sampling rate of the engine signals, Hz   */
#define N       4000        /* 160 ms = 4 engine cycles at 3000 rpm      */
#define MAXLAG  1100        /* autocorrelation lags 0..MAXLAG            */
#define FS_LAM  200.0       /* lambda sensor: slow signal, 200 Hz        */
#define N_LAM   4000        /* 20 s                                      */

static double crank[N], cam[N], knock[N], ion[N], crank_acc[N], tt[N];
static double lam_v[N_LAM];
static double r[MAXLAG + 1];

/* ---- (a) statistics -------------------------------------------------- */
static void stats(const char *name, const double *x, int n)
{
    double mean = 0.0, ms = 0.0, mn = x[0], mx = x[0];
    int i;
    for (i = 0; i < n; i++) {
        mean += x[i];
        ms += x[i] * x[i];
        if (x[i] < mn) mn = x[i];
        if (x[i] > mx) mx = x[i];
    }
    mean /= n;
    ms /= n;
    printf("%-14s %9.4f %9.4f %9.4f %9.4f\n", name, mean, sqrt(ms), mn, mx);
}

/* ---- (b) normalised autocorrelation (mean removed, unbiased: each lag
         is divided by its number of products N-m, so that a periodic
         signal gives r = 1 at its period) ------------------------------ */
static void autocorr(const double *x, int n, double *rr, int maxlag)
{
    double mean = 0.0, r0;
    int i, m;
    for (i = 0; i < n; i++) mean += x[i];
    mean /= n;
    for (m = 0; m <= maxlag; m++) {
        double s = 0.0;
        for (i = 0; i + m < n; i++) s += (x[i] - mean) * (x[i + m] - mean);
        rr[m] = s / (n - m);
    }
    r0 = rr[0] > 0.0 ? rr[0] : 1.0;
    for (m = 0; m <= maxlag; m++) rr[m] /= r0;
}

/* period estimate: after the first zero crossing, find the largest
   autocorrelation value rmax; the period is the FIRST local maximum that
   reaches 0.9 rmax (avoids picking a multiple of the period).
   Returns the lag (0 if none), *peak = r[lag].                        */
static int period_lag(const double *rr, int maxlag, double *peak)
{
    int m0 = 1, m;
    double rmax = -1.0;
    while (m0 <= maxlag && rr[m0] > 0.0) m0++;        /* first zero crossing */
    for (m = m0; m <= maxlag; m++) if (rr[m] > rmax) rmax = rr[m];
    for (m = m0 + 1; m < maxlag; m++)
        if (rr[m] >= rr[m - 1] && rr[m] >= rr[m + 1] && rr[m] >= 0.9 * rmax) {
            *peak = rr[m];
            return m;
        }
    *peak = 0.0;
    return 0;
}

static void analyse(const char *name, const double *x, int n, double fs)
{
    double pk;
    int lag, m, mbest = 0;
    autocorr(x, n, r, MAXLAG);
    lag = period_lag(r, MAXLAG, &pk);
    for (m = lag; m <= MAXLAG; m++) if (r[m] > r[mbest] || mbest == 0) mbest = m;
    if (lag)
        printf("%-12s first peak: lag %4d = %7.2f ms r = %.3f | "
               "max: lag %4d r = %.3f\n", name, lag, 1e3 * lag / fs, pk,
               mbest, r[mbest]);
    else
        printf("%-12s no peak found\n", name);
}

int main(void)
{
    es_engine_t e, ea;
    int n;

    es_init(&e, 3000.0, 1234u);
    es_init(&ea, 2000.0, 1234u);
    for (n = 0; n < N; n++) {
        es_advance(&e, 1.0 / FS);
        tt[n] = n / FS;
        crank[n] = es_crank(&e);
        cam[n]   = es_cam(&e);
        knock[n] = es_knock(&e);
        ion[n]   = es_ion(&e);
        /* crank signal during a ramp 2000 -> 4000 rpm */
        ea.rpm = 2000.0 + 2000.0 * n / N;
        es_advance(&ea, 1.0 / FS);
        crank_acc[n] = es_crank(&ea);
    }
    /* lambda: slow oscillation around 1 (as in a closed loop) + noise */
    for (n = 0; n < N_LAM; n++) {
        double lam = 1.0 + 0.02 * sin(2.0 * M_PI * 0.8 * n / FS_LAM)
                         + 0.002 * es_rand_gauss(&e);
        lam_v[n] = es_lambda_voltage(lam);
    }

    ap_plot(crank, 1000, "crank (60-2), first 40 ms = 1 engine cycle");
    ap_plot(cam, N, "cam, 160 ms");
    ap_plot(knock, 1000, "knock sensor [V], 40 ms");
    ap_plot(ion, 1000, "ion current [V], 40 ms");
    ap_plot(lam_v, N_LAM, "lambda sensor [V], 20 s");

    printf("\n(a) statistics\n%-14s %9s %9s %9s %9s\n", "signal", "mean", "rms",
           "min", "max");
    stats("crank", crank, N);
    stats("cam", cam, N);
    stats("knock", knock, N);
    stats("ion", ion, N);
    stats("lambda", lam_v, N_LAM);

    printf("\n(b) period from autocorrelation (fs = %.0f kHz)\n", FS / 1000);
    analyse("crank 3000", crank, N, FS);
    analyse("crank ramp", crank_acc, N, FS);
    analyse("cam", cam, N, FS);
    analyse("knock", knock, N, FS);
    analyse("ion", ion, N, FS);
    analyse("lambda", lam_v, N_LAM, FS_LAM);

    autocorr(crank, N, r, MAXLAG);
    ap_plot(r, MAXLAG + 1, "autocorrelation crank, lag 0..1100 samples");
    autocorr(knock, N, r, MAXLAG);
    ap_plot(r, MAXLAG + 1, "autocorrelation knock, lag 0..1100 samples");

    csv_write("zoo.csv", "t,crank,cam,knock,ion", N, 5, tt, crank, cam, knock, ion);
    printf("wrote zoo.csv\n");
    return 0;
}
