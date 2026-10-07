/*
 * Sheet 11 -- Exercise 11.3: resolving two close resonances
 * (student template)
 *
 * Build and run:   gcc -O2 -o resolve resolve.c -lm
 *                  ./resolve [df]          (frequency spacing, default 500 Hz)
 * (ar.h, fft.h, engine_signals.h, asciiplot.h in the same directory)
 *
 * Two components at f1 = 6500 Hz and f2 = f1 + df in white noise, fs = 25 kHz.
 *   (a) sinusoids (random phases), SNR = 40 / 20 / 10 dB per component
 *   (b) damped modes with tau = 0.8 ms (like knock bursts), SNR 40 dB
 * For record lengths N = 16 ... 256 the program counts in how many of
 * NTRIAL trials the two components are resolved by
 *   - the periodogram with rectangular window
 *   - the periodogram with Hann window
 *   - Burg's method with order p = N/3 (at most 32)
 * Resolved: two local maxima, one within df/2 of f1 and one within df/2
 * of f2, and a dip of at least 3 dB between them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "ar.h"

#define FS     25000.0
#define NFFT   2048
#define NF     (NFFT / 2 + 1)
#define NTRIAL 200
#define F1     6500.0

static double df_g = 500.0;

/* resolution test on a spectrum P sampled at k*dfr, k = 0..nf-1 */
static int resolved(const double *P, int nf, double dfr)
{
    /* TODO (e): search the local maxima of P in [f1 - df, f2 + df];
       return 1 if there is a maximum within df/2 of f1 AND one within
       df/2 of f2 (take the largest in each case) and the minimum of P
       between them is at least 3 dB below the smaller maximum.
       Placeholder: never resolved. */
    (void)P; (void)nf; (void)dfr;
    return 0;
}

static void make_signal(es_engine_t *e, double *x, int N, double snr_db, int damped)
{
    double sigma = sqrt(0.5 / pow(10.0, snr_db / 10.0));   /* A = 1 */
    double p1 = 2.0 * M_PI * es_rand_uniform(e), p2 = 2.0 * M_PI * es_rand_uniform(e);
    double mean = 0.0;
    int n;
    for (n = 0; n < N; n++) {
        double t = n / FS, g = damped ? exp(-t / ES_KNOCK_TAU) : 1.0;
        x[n] = g * (sin(2.0 * M_PI * F1 * t + p1) + sin(2.0 * M_PI * (F1 + df_g) * t + p2))
               + sigma * es_rand_gauss(e);
        mean += x[n] / N;
    }
    for (n = 0; n < N; n++) x[n] -= mean;
}

int main(int argc, char **argv)
{
    static const int Ns[8] = {16, 24, 32, 48, 64, 96, 128, 256};
    static const double snr[3] = {40.0, 20.0, 10.0};
    static double x[256], P[NF], D1[NF], D2[NF];
    double a[AR_MAXP + 1], k[AR_MAXP + 1], E[AR_MAXP + 1];
    es_engine_t e;
    int s, j, t, i, cnt[3];

    if (argc > 1) df_g = atof(argv[1]);
    es_init(&e, 3000.0, 99u);
    printf("f1 = %.0f Hz, f2 = %.0f Hz (df = %.0f Hz), fs = %.0f Hz, %d trials\n",
           F1, F1 + df_g, df_g, FS, NTRIAL);
    printf("Rayleigh limit fs/df = %.0f samples (rect), ~2 fs/df = %.0f (Hann)\n\n",
           FS / df_g, 2.0 * FS / df_g);
    for (s = 0; s < 4; s++) {
        int damped = (s == 3);
        double sn = damped ? 40.0 : snr[s];
        printf("%s, SNR %2.0f dB: resolved in %% of trials\n",
               damped ? "(b) damped modes, tau = 0.8 ms" : "(a) sinusoids", sn);
        printf("      N   p   rect   Hann   Burg\n");
        for (j = 0; j < 8; j++) {
            int N = Ns[j], p = N / 3 > 32 ? 32 : N / 3;
            cnt[0] = cnt[1] = cnt[2] = 0;
            for (t = 0; t < NTRIAL; t++) {
                make_signal(&e, x, N, sn, damped);
                per_psd(x, N, NFFT, 0, FS, P);
                cnt[0] += resolved(P, NF, FS / NFFT);
                per_psd(x, N, NFFT, 1, FS, P);
                cnt[1] += resolved(P, NF, FS / NFFT);
                ar_burg(x, N, p, a, k, E);
                /* only the band around f1, f2 is needed by resolved() */
                for (i = (int)((F1 - df_g) / (FS / NFFT)) - 1;
                     i <= (int)((F1 + 2.0 * df_g) / (FS / NFFT)) + 1 && i < NF; i++)
                    P[i] = ar_psd(a, p, E[p], i * FS / NFFT, FS);
                cnt[2] += resolved(P, NF, FS / NFFT);
            }
            printf("    %4d  %2d  %4.0f   %4.0f   %4.0f\n", N, p, 100.0 * cnt[0] / NTRIAL,
                   100.0 * cnt[1] / NTRIAL, 100.0 * cnt[2] / NTRIAL);
        }
        printf("\n");
    }

    /* example: N = 32, SNR 40 dB sinusoids */
    make_signal(&e, x, 32, 40.0, 0);
    per_psd(x, 32, NFFT, 0, FS, P);
    for (i = 0; i < NF; i++) D1[i] = 10.0 * log10(P[i] + 1e-30);
    ar_burg(x, 32, 10, a, k, E);
    for (i = 0; i < NF; i++) D2[i] = 10.0 * log10(ar_psd(a, 10, E[10], i * FS / NFFT, FS) + 1e-30);
    {
        /* normalise both to 0 dB peak and show 4..10 kHz */
        double m1 = -1e9, m2 = -1e9;
        int i0 = (int)(4000.0 / (FS / NFFT)), i1 = (int)(10000.0 / (FS / NFFT));
        for (i = i0; i < i1; i++) { if (D1[i] > m1) m1 = D1[i]; if (D2[i] > m2) m2 = D2[i]; }
        for (i = i0; i < i1; i++) {
            D1[i - i0] = D1[i] - m1 < -40 ? -40 : D1[i] - m1;
            D2[i - i0] = D2[i] - m2 < -40 ? -40 : D2[i] - m2;
        }
        ap_plot2(D1, D2, i1 - i0, "N = 32, 4..10 kHz, dB: '*' periodogram (rect), 'o' Burg p = 10");
    }
    return 0;
}
