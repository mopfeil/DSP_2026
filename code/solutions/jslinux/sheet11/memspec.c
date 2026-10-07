/*
 * Sheet 11 -- Exercise 11.2: MEM (AR) spectra vs. periodogram on short
 * knock windows (reference solution)
 *
 * Build and run:   gcc -O2 -o memspec memspec.c -lm
 *                  ./memspec [N] [p]      (default N = 64, p = 8)
 * (ar.h, fft.h, engine_signals.h, asciiplot.h, csvio.h in the same dir)
 *
 *  (a) one knock window of N samples (fs = 25 kHz, start 10 deg ATDC):
 *      Hann periodogram, Yule-Walker and Burg AR(p) spectra -> mem.csv
 *  (b) statistics of the estimated knock frequency (6.5 kHz mode) over
 *      M windows for N = 32, 64, 128
 *  (c) model order selection with FPE, AIC, MDL (Burg)
 *  (d) spurious peaks / line splitting for too high orders
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"
#include "ar.h"

#define FS     25000.0
#define NFFT   1024
#define NF     (NFFT / 2 + 1)      /* frequency grid 0..fs/2, df = 24.4 Hz */
#define M      300                 /* windows for the statistics          */
#define PMAX   40

/* collect one knock window: N samples starting at 10 deg ATDC of the next
   knocking combustion; mean removed. Returns the true onset angle (deg
   ATDC) of the burst. */
static double knock_window(es_engine_t *e, double *x, int N)
{
    int c, n;
    double th_prev, mean = 0.0, onset;
    for (;;) {                               /* find next TDC + 10 deg */
        th_prev = e->theta;
        es_advance(e, 1.0 / FS);
        (void)es_knock(e);
        for (c = 0; c < ES_NCYL; c++)
            if (es_crossed(th_prev, e->theta, es_wrap720(es_tdc_deg[c] + 10.0)))
                break;
        if (c < ES_NCYL && e->knock_flag[c]) break;
    }
    onset = es_angle_diff(e->knock_onset[c], es_tdc_deg[c]);
    for (n = 0; n < N; n++) {
        es_advance(e, 1.0 / FS);
        x[n] = es_knock(e);
        mean += x[n] / N;
    }
    for (n = 0; n < N; n++) x[n] -= mean;
    return onset;
}

static void ar_spectrum(const double *a, int p, double E, double *P)
{
    int i;
    for (i = 0; i < NF; i++) P[i] = ar_psd(a, p, E, i * FS / NFFT, FS);
}

/* number of local maxima of P within 20 dB of the global maximum */
static int count_peaks(const double *P, int nf)
{
    int i, n = 0;
    double mx = 0.0;
    for (i = 0; i < nf; i++) if (P[i] > mx) mx = P[i];
    for (i = 1; i < nf - 1; i++)
        if (P[i] > P[i - 1] && P[i] >= P[i + 1] && P[i] > 0.01 * mx) n++;
    return n;
}

/* number of local maxima of P within 10 dB of the largest value in [f1,f2] */
static int peaks_in_band(const double *P, double df, double f1, double f2)
{
    int i, n = 0, i1 = (int)(f1 / df), i2 = (int)(f2 / df);
    double mx = 0.0;
    for (i = i1; i <= i2; i++) if (P[i] > mx) mx = P[i];
    for (i = i1; i <= i2; i++)
        if (P[i] > P[i - 1] && P[i] >= P[i + 1] && P[i] > 0.1 * mx) n++;
    return n;
}

/* -3 dB width (Hz) of the peak of P at frequency fpk */
static double peak_width(const double *P, int nf, double df, double fpk)
{
    int i0 = (int)floor(fpk / df + 0.5), lo = i0, hi = i0;
    double half = 0.5 * P[i0];
    while (lo > 0 && P[lo] > half) lo--;
    while (hi < nf - 1 && P[hi] > half) hi++;
    return (hi - lo) * df;
}

static void to_db(const double *P, double *D, int nf)
{
    int i;
    double mx = 0.0;
    for (i = 0; i < nf; i++) if (P[i] > mx) mx = P[i];
    for (i = 0; i < nf; i++) {
        D[i] = 10.0 * log10(P[i] / mx + 1e-12);
        if (D[i] < -50.0) D[i] = -50.0;
    }
}

int main(int argc, char **argv)
{
    static double x[512], Pp[NF], Py[NF], Pb[NF], D1[NF], D2[NF], fr[NF];
    double a[PMAX + 1], k[PMAX + 1], E[PMAX + 1], r[PMAX + 1];
    int N0 = argc > 1 ? atoi(argv[1]) : 64, p0 = argc > 2 ? atoi(argv[2]) : 8;
    static const int Ns[3] = {32, 64, 128};
    es_engine_t e;
    int i, j, m, n;

    es_init(&e, 3000.0, 2024u);
    e.knock_prob = 1.0;

    /* ---------------- (a) one window -------------------------------- */
    {
        double on = knock_window(&e, x, N0);
        per_psd(x, N0, NFFT, 1, FS, Pp);
        ar_autocorr(x, N0, r, p0);
        ar_levinson(r, p0, a, k, E);
        ar_spectrum(a, p0, E[p0], Py);
        ar_burg(x, N0, p0, a, k, E);
        ar_spectrum(a, p0, E[p0], Pb);
        for (i = 0; i < NF; i++) fr[i] = i * FS / NFFT;
        printf("(a) knock window N = %d (%.2f ms) from 10 deg ATDC, onset %.1f deg ATDC, p = %d\n",
               N0, N0 / FS * 1e3, on, p0);
        printf("    peak 5.5..7.5 kHz: periodogram (Hann) %.0f Hz, Yule-Walker %.0f Hz, Burg %.0f Hz\n",
               peak_in_band(Pp, NF, FS / NFFT, 5500, 7500), peak_in_band(Py, NF, FS / NFFT, 5500, 7500),
               peak_in_band(Pb, NF, FS / NFFT, 5500, 7500));
        printf("    peak 9.5..11.5 kHz: periodogram %.0f Hz, Yule-Walker %.0f Hz, Burg %.0f Hz\n",
               peak_in_band(Pp, NF, FS / NFFT, 9500, 11500), peak_in_band(Py, NF, FS / NFFT, 9500, 11500),
               peak_in_band(Pb, NF, FS / NFFT, 9500, 11500));
        printf("    Burg reflection coefficients:");
        for (i = 1; i <= p0; i++) printf(" %.3f", k[i]);
        printf("\n");
        to_db(Pp, D1, NF); to_db(Pb, D2, NF);
        ap_plot2(D1, D2, NF, "dB, 0..12.5 kHz: '*' Hann periodogram, 'o' Burg MEM");
        csv_write("mem.csv", "f,per_hann,yule_walker,burg", NF, 4, fr, Pp, Py, Pb);
        printf("    wrote mem.csv\n\n");
    }

    /* ---------------- (b) statistics -------------------------------- */
    printf("(b) estimated knock frequency (true 6500 Hz), %d windows, 3000 rpm, noise 0.05 V\n", M);
    printf("      N   method              mean (Hz) std (Hz) rms err (Hz) -3dB width (Hz) 10.5k found\n");
    for (j = 0; j < 3; j++) {
        int N = Ns[j], meth;
        static double fest[4][M];
        double wid[4] = {0, 0, 0, 0};
        int found[4] = {0, 0, 0, 0};
        static const char *mn[4] = {"periodogram rect", "periodogram Hann", "Yule-Walker AR", "Burg AR"};
        es_init(&e, 3000.0, 1000u + j);
        e.knock_prob = 1.0;
        for (m = 0; m < M; m++) {
            knock_window(&e, x, N);
            per_psd(x, N, NFFT, 0, FS, Pp);
            fest[0][m] = peak_in_band(Pp, NF, FS / NFFT, 5500, 7500);
            wid[0] += peak_width(Pp, NF, FS / NFFT, fest[0][m]) / M;
            found[0] += fabs(peak_in_band(Pp, NF, FS / NFFT, 9000, 12000) - 10500) < 300;
            per_psd(x, N, NFFT, 1, FS, Pp);
            fest[1][m] = peak_in_band(Pp, NF, FS / NFFT, 5500, 7500);
            wid[1] += peak_width(Pp, NF, FS / NFFT, fest[1][m]) / M;
            found[1] += fabs(peak_in_band(Pp, NF, FS / NFFT, 9000, 12000) - 10500) < 300;
            ar_autocorr(x, N, r, p0);
            ar_levinson(r, p0, a, k, E);
            ar_spectrum(a, p0, E[p0], Py);
            fest[2][m] = peak_in_band(Py, NF, FS / NFFT, 5500, 7500);
            wid[2] += peak_width(Py, NF, FS / NFFT, fest[2][m]) / M;
            found[2] += fabs(peak_in_band(Py, NF, FS / NFFT, 9000, 12000) - 10500) < 300;
            ar_burg(x, N, p0, a, k, E);
            ar_spectrum(a, p0, E[p0], Pb);
            fest[3][m] = peak_in_band(Pb, NF, FS / NFFT, 5500, 7500);
            wid[3] += peak_width(Pb, NF, FS / NFFT, fest[3][m]) / M;
            found[3] += fabs(peak_in_band(Pb, NF, FS / NFFT, 9000, 12000) - 10500) < 300;
        }
        for (meth = 0; meth < 4; meth++) {
            double s = 0.0, s2 = 0.0, se = 0.0;
            for (m = 0; m < M; m++) {
                s += fest[meth][m];
                s2 += fest[meth][m] * fest[meth][m];
                se += (fest[meth][m] - 6500.0) * (fest[meth][m] - 6500.0);
            }
            s /= M;
            printf("    %4d   %-18s  %7.0f  %7.0f  %8.0f     %8.0f       %5.0f %%\n", N, mn[meth], s,
                   sqrt(s2 / M - s * s), sqrt(se / M), wid[meth], 100.0 * found[meth] / M);
        }
    }

    /* ---------------- (c) order selection --------------------------- */
    printf("\n(c) order chosen by FPE / AIC / MDL (Burg, p = 1..%d), %d windows\n", PMAX / 2, M);
    for (j = 0; j < 3; j++) {
        int N = Ns[j], hist[3][PMAX + 1], c, pm = N / 2 < PMAX / 2 ? N / 2 : PMAX / 2;
        double mean[3] = {0, 0, 0};
        for (c = 0; c < 3; c++) for (i = 0; i <= PMAX; i++) hist[c][i] = 0;
        es_init(&e, 3000.0, 3000u + j);
        e.knock_prob = 1.0;
        for (m = 0; m < M; m++) {
            knock_window(&e, x, N);
            ar_burg(x, N, pm, a, k, E);
            for (c = 0; c < 3; c++) {
                int best = 1;
                for (n = 1; n <= pm; n++)
                    if (ar_crit(E[n], N, n, c) < ar_crit(E[best], N, best, c)) best = n;
                hist[c][best]++;
                mean[c] += (double)best / M;
            }
        }
        printf("    N = %3d: mean order FPE %.1f, AIC %.1f, MDL %.1f; most frequent: ", N,
               mean[0], mean[1], mean[2]);
        for (c = 0; c < 3; c++) {
            int mode = 1;
            for (i = 1; i <= PMAX; i++) if (hist[c][i] > hist[c][mode]) mode = i;
            printf("%s %d%s", c == 0 ? "FPE" : (c == 1 ? "AIC" : "MDL"), mode, c < 2 ? ", " : "\n");
        }
    }

    /* ---------------- (d) too high orders ---------------------------- */
    printf("\n(d) number of spectral peaks (within 20 dB of the maximum), Burg, N = 64, mean of %d windows\n", M);
    {
        static const int ps[6] = {2, 4, 8, 16, 24, 32};
        double npk[6] = {0, 0, 0, 0, 0, 0}, ferr[6] = {0, 0, 0, 0, 0, 0};
        int split[6] = {0, 0, 0, 0, 0, 0};
        es_init(&e, 3000.0, 4000u);
        e.knock_prob = 1.0;
        for (m = 0; m < M; m++) {
            knock_window(&e, x, 64);
            for (j = 0; j < 6; j++) {
                double f;
                ar_burg(x, 64, ps[j], a, k, E);
                ar_spectrum(a, ps[j], E[ps[j]], Pb);
                npk[j] += (double)count_peaks(Pb, NF) / M;
                split[j] += peaks_in_band(Pb, FS / NFFT, 5500.0, 7500.0) >= 2;
                f = peak_in_band(Pb, NF, FS / NFFT, 5500, 7500);
                ferr[j] += (f - 6500.0) * (f - 6500.0) / M;
            }
        }
        for (j = 0; j < 6; j++)
            printf("    p = %2d: %.2f peaks, 6.5 kHz peak split in %3.0f %% of the windows, "
                   "rms error %4.0f Hz\n", ps[j], npk[j], 100.0 * split[j] / M, sqrt(ferr[j]));
        /* example of a high-order spectrum */
        ar_burg(x, 64, 32, a, k, E);
        ar_spectrum(a, 32, E[32], Pb);
        ar_burg(x, 64, 8, a, k, E);
        ar_spectrum(a, 8, E[8], Py);
        to_db(Py, D1, NF); to_db(Pb, D2, NF);
        ap_plot2(D1, D2, NF, "dB, 0..12.5 kHz, N = 64: '*' Burg p = 8, 'o' Burg p = 32");
    }
    return 0;
}
