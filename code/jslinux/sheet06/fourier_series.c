/*
 * Sheet 6, Exercise 6.2 a,b) -- Fourier series of the 60-2 crank signal,
 * Gibbs phenomenon  (student template -- complete the parts marked TODO)
 *
 * Build and run:   gcc -O2 -o fourier_series fourier_series.c -lm
 *                  ./fourier_series
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 * The crank signal is a function of the crank angle with period 360 deg.
 * It is sampled on a fine angle grid (M points per revolution) and the
 * Fourier coefficients
 *        c_k = 1/360 int_0^360 x(theta) exp(-j 2 pi k theta / 360) dtheta
 * are approximated by the direct sum  c_k ~ 1/M sum_n x[n] exp(-j 2 pi k n / M).
 * k is the ENGINE ORDER (cycles per crank revolution).
 */
#include <stdio.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define M     36000         /* angle samples per revolution (0.01 deg) */
#define KMAX  1200          /* highest order computed                  */

typedef struct { double re, im; } cpx;

static double x[M];
static cpx c[KMAX + 1];

/* analytic |c_k| of the 60-2 wheel (Exercise 6.1) */
static double ck_analytic(int k)
{
    double s, g;
    if (k == 0) return 58.0 * 3.0 / 360.0;
    s = sin(M_PI * k / 120.0) / (M_PI * k / 120.0);         /* sinc(k/120) */
    if (k % 60 == 0) g = 58.0;
    else g = sin(29.0 * M_PI * k / 30.0) / sin(M_PI * k / 60.0);
    return fabs(3.0 / 360.0 * s * g);
}

/* partial sum S_K(theta) = sum_{|k|<=K} c_k exp(j 2 pi k theta/360) */
static double partial_sum(double theta, int K)
{
    /* TODO (b): c_0 + sum_{k=1..K} 2 Re{ c_k exp(j 2 pi k theta / 360) } */
    (void)theta; (void)K;
    return c[0].re;
}

int main(void)
{
    static const int kshow[] = {0, 1, 2, 3, 30, 31, 57, 58, 59, 60, 61, 62, 119, 120, 121, 180};
    static const int Ks[] = {60, 180, 600, 1200};
    static double mag[KMAX + 1], kk[KMAX + 1];
    static double win[1201], ps[1201];
    es_engine_t e;
    int n, k, i;

    /* ---- sample one revolution of the crank signal on the angle grid ---- */
    es_init(&e, 3000.0, 1u);
    for (n = 0; n < M; n++) {
        e.theta = 360.0 * n / M;           /* set the angle directly      */
        x[n] = es_crank(&e);
    }

    /* ---- a) Fourier coefficients by direct summation ------------------- */
    for (k = 0; k <= KMAX; k++) {
        /* rotating phasor instead of cos/sin per term (saves time)        */
        /* TODO (a): c_k = 1/M sum_n x[n] exp(-j 2 pi k n / M)
           (tip: a rotating phasor p <- p * exp(j 2 pi k / M) avoids
            calling cos/sin M times for every k)                          */
        c[k].re = 0.0; c[k].im = 0.0;
        mag[k] = sqrt(c[k].re * c[k].re + c[k].im * c[k].im);
        kk[k] = k;
    }
    printf("a) Fourier coefficients of the 60-2 signal (M = %d points/rev)\n", M);
    printf("   order k   |c_k| numeric   |c_k| analytic   ratio\n");
    for (i = 0; i < (int)(sizeof kshow / sizeof kshow[0]); i++) {
        k = kshow[i];
        if (ck_analytic(k) > 1e-9)
            printf("   %5d      %10.6f      %10.6f    %7.4f\n", k, mag[k],
                   ck_analytic(k), mag[k] / ck_analytic(k));
        else
            printf("   %5d      %10.6f      %10.6f       -\n", k, mag[k], 0.0);
    }
    printf("   full 60-tooth wheel for comparison: |c_60| = %.6f\n",
           0.5 * sin(M_PI / 2) / (M_PI / 2));
    ap_plot(mag + 1, 130, "|c_k| for orders k = 1..130 (lines at 60, 180 and sidebands)");
    csv_write("crank_coeffs.csv", "k,abs_ck", KMAX + 1, 2, kk, mag);

    /* ---- b) partial sums and Gibbs phenomenon around the gap ------------- */
    printf("\nb) partial sums S_K on 336..372 deg (gap and teeth 0..1)\n");
    printf("      K    max S_K    overshoot   min S_K    rms error\n");
    for (i = 0; i < 4; i++) {
        double mx = -1e9, mn = 1e9, se = 0.0;
        int K = Ks[i];
        for (n = 0; n <= 1200; n++) {          /* 0.03 deg steps over 36 deg */
            double th = 336.0 + 0.03 * n, ref;
            win[n] = th;
            ps[n] = partial_sum(th, K);
            e.theta = fmod(th, 360.0);
            ref = es_crank(&e);
            if (ps[n] > mx) mx = ps[n];
            if (ps[n] < mn) mn = ps[n];
            se += (ps[n] - ref) * (ps[n] - ref);
        }
        printf("   %5d    %7.4f    %6.2f %%   %7.4f    %7.4f\n", K, mx,
               100.0 * (mx - 1.0), mn, sqrt(se / 1201));
        if (K == 180 || K == 1200) {
            char t[80];
            sprintf(t, "partial sum K = %d, theta = 336..372 deg", K);
            ap_plot(ps, 1201, t);
        }
    }
    csv_write("partial_sum_K1200.csv", "theta,S", 1201, 2, win, ps);
    return 0;
}
