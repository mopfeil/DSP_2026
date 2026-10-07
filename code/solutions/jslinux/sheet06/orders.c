/*
 * Sheet 6, Exercise 6.3 -- Engine orders vs. fixed resonances
 * (reference solution)
 *
 * Build and run:   gcc -O2 -o orders orders.c -lm
 *                  ./orders
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 * The knock-sensor signal of the virtual engine is recorded at constant
 * speed 1500, 3000 and 6000 rpm, always over exactly 8 engine cycles
 * (16 crank revolutions), fs = 50 kHz. Its DTFT is evaluated by direct
 * summation
 *   a) at low frequencies (0..1 kHz): lines of the crank-synchronous
 *      combustion vibration -> engine orders 2, 4, 6, ...
 *   b) at high frequencies (4..12 kHz): knock modes and valve resonance.
 * Amplitudes:  A(f) = 2 |X(f)| / N  (exact for a bin-centred sinusoid).
 */
#include <stdio.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS    50000.0
#define NMAX  32000           /* 8 cycles at 1500 rpm = 0.64 s     */
#define NLO   1001            /* 0 .. 1000 Hz, 1 Hz steps          */
#define NHI   801             /* 4 .. 12 kHz, 10 Hz steps          */

static double x[NMAX], alo[NLO], ahi[NHI], flo[NLO], fhi[NHI], sm[NHI];

/* amplitude spectrum 2|X(f)|/N on a frequency grid, rotating phasor */
static void amp_spectrum(const double *s, int n, const double *f, double *A, int nf)
{
    int i, k;
    for (i = 0; i < nf; i++) {
        double w = 2.0 * M_PI * f[i] / FS, cw = cos(w), sw = sin(w);
        double pr = 1.0, pi = 0.0, re = 0.0, im = 0.0, t;
        for (k = 0; k < n; k++) {
            re += s[k] * pr;
            im -= s[k] * pi;
            t = pr * cw - pi * sw; pi = pr * sw + pi * cw; pr = t;
        }
        A[i] = 2.0 * sqrt(re * re + im * im) / n;
    }
}

/* expected amplitude of order 2m of the half-sine combustion vibration
   (width 60 deg, period 180 deg, amplitude comb_amp) -- Exercise 6.3 b) */
static double halfsine_order_amp(int m, double amp)
{
    double r = 2.0 * m * 60.0 / 180.0;
    return 2.0 * amp * (60.0 / 180.0) * (2.0 / M_PI) * fabs(cos(M_PI * r / 2.0) / (1.0 - r * r));
}

/* largest value of v[] for grid frequencies in [a, b] */
static int argmax_in(const double *v, const double *f, int n, double a, double b)
{
    int i, im = -1;
    for (i = 0; i < n; i++)
        if (f[i] >= a && f[i] <= b && (im < 0 || v[i] > v[im])) im = i;
    return im;
}

int main(void)
{
    static const double rpms[3] = {1500.0, 3000.0, 6000.0};
    es_engine_t e;
    int i, j, n, N, m;
    char title[100];

    for (i = 0; i < NLO; i++) flo[i] = i;
    for (i = 0; i < NHI; i++) fhi[i] = 4000.0 + 10.0 * i;

    printf("expected order amplitudes of the combustion vibration (comb_amp = 0.2 V):\n");
    for (m = 1; m <= 4; m++)
        printf("   order %d: %.4f V\n", 2 * m, halfsine_order_amp(m, 0.2));

    for (j = 0; j < 3; j++) {
        double frot = rpms[j] / 60.0;
        es_init(&e, rpms[j], 2024u);
        e.knock_prob = 0.2;          /* model default */
        N = (int)(16.0 / frot * FS + 0.5);          /* 16 revolutions   */
        for (n = 0; n < N; n++) { es_advance(&e, 1.0 / FS); x[n] = es_knock(&e); }
        {   /* remove the mean (DC leaks into the lowest frequencies) */
            double mean = 0.0;
            for (n = 0; n < N; n++) mean += x[n];
            mean /= N;
            for (n = 0; n < N; n++) x[n] -= mean;
        }

        amp_spectrum(x, N, flo, alo, NLO);
        amp_spectrum(x, N, fhi, ahi, NHI);

        printf("\n=== %.0f rpm: f_rot = %.1f Hz, N = %d samples (%.2f s)\n",
               rpms[j], frot, N, N / FS);
        printf("  a) order   f = order * f_rot   amplitude\n");
        for (m = 1; m <= 4; m++) {
            int k = (int)(2 * m * frot + 0.5);
            if (k < NLO) printf("     %2d      %7.1f Hz         %.4f V\n", 2 * m, flo[k], alo[k]);
        }
        {   /* strongest low-frequency line and its order */
            int k = argmax_in(alo, flo, NLO, 1.0, 1000.0);
            printf("     strongest line below 1 kHz: %.0f Hz = order %.2f\n",
                   flo[k], flo[k] / frot);
        }
        /* b) resonances: smooth |X|^2 over +-150 Hz (31 points) to see the
              envelope instead of the individual order lines             */
        for (i = 0; i < NHI; i++) {
            int k, c = 0;
            sm[i] = 0.0;
            for (k = i - 15; k <= i + 15; k++)
                if (k >= 0 && k < NHI) { sm[i] += ahi[k] * ahi[k]; c++; }
            sm[i] /= c;
        }
        printf("  b) band            peak of smoothed |X|^2   order of that peak\n");
        {
            static const double band[3][2] = {{5500, 7500}, {7500, 8500}, {9500, 11500}};
            for (i = 0; i < 3; i++) {
                int k = argmax_in(sm, fhi, NHI, band[i][0], band[i][1]);
                printf("     %5.0f-%5.0f Hz   %7.0f Hz            %6.1f\n",
                       band[i][0], band[i][1], fhi[k], fhi[k] / frot);
            }
        }
        sprintf(title, "%.0f rpm: amplitude spectrum 0..1000 Hz (1 Hz/point)", rpms[j]);
        ap_size(72, 12);
        ap_plot(alo, NLO, title);
        sprintf(title, "%.0f rpm: amplitude spectrum 4..12 kHz (10 Hz/point)", rpms[j]);
        ap_plot(ahi, NHI, title);
        if (j == 1) {
            csv_write("orders_3000_lo.csv", "f,A", NLO, 2, flo, alo);
            csv_write("orders_3000_hi.csv", "f,A", NHI, 2, fhi, ahi);
        }
    }
    return 0;
}
