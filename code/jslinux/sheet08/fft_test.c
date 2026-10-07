/*
 * Sheet 8, Exercise 8.2 -- Direct DFT vs. own radix-2 FFT
 *                          (TEMPLATE)
 *
 * Build and run:   gcc -O2 -o fft_test fft_test.c -lm
 *                  ./fft_test
 * (fft.h, asciiplot.h, csvio.h in the same directory)
 *
 *   (a) direct DFT with a precomputed twiddle table
 *   (b) check of the FFT (fft.h) against the DFT, inverse transform,
 *       Parseval and conjugate symmetry
 *   (c) runtime vs. N, fit of t = c N^p for the DFT and of
 *       t = c N log2 N for the FFT
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "fft.h"
#include "asciiplot.h"
#include "csvio.h"

#define NMAX 32768

/* ------------------------------------------------------------- (a) */
/* direct DFT X[k] = sum_n x[n] W^{kn}, W = e^{-j 2 pi / N} (forward only).
   The twiddle factors are taken from a table of N values: index (k n) mod N */
static void dft(const double *xr, const double *xi, double *Xr, double *Xi, int N)
{
    /* TODO (a): direct DFT, O(N^2); precompute a table of the N twiddle
       factors cos/sin(2 pi m / N) and use the index m = (k n) mod N */
    int k;
    (void)xr; (void)xi;
    for (k = 0; k < N; k++) { Xr[k] = 0.0; Xi[k] = 0.0; }
}

static double urand(void) { return 2.0 * rand() / (double)RAND_MAX - 1.0; }

/* ------------------------------------------------------------- (c) */
/* average runtime of one transform in seconds (repeat until >= 10 ms) */
static double timeit(int use_fft, int N, double *xr, double *xi, double *Xr, double *Xi)
{
    int rep = 1, r;
    double dt;
    for (;;) {                       /* double the repetitions until the */
        clock_t t0 = clock();        /* measured interval is long enough */
        for (r = 0; r < rep; r++) {
            if (use_fft) fft(xr, xi, N, 0);
            else dft(xr, xi, Xr, Xi, N);
        }
        dt = (double)(clock() - t0) / CLOCKS_PER_SEC;
        if (dt >= 0.01 || rep >= (1 << 22)) return dt / rep;
        rep *= 2;
    }
}

/* least-squares fit of log y = log c + p log x, returns p */
static double fit_exponent(const double *x, const double *y, int n)
{
    /* TODO (c): least-squares straight line through (log x, log y) -> slope */
    (void)x; (void)y; (void)n;
    return 0.0;
}

int main(void)
{
    static double xr[NMAX], xi[NMAX], Xr[NMAX], Xi[NMAX], yr[NMAX], yi[NMAX];
    int N, n, k, i;

    /* small examples of Exercise 8.1 */
    {
        double a[4] = {1, 2, 0, -1}, b[8] = {1, 1, 1, 1, 0, 0, 0, 0}, z[8] = {0};
        double ar[4], ai[4], br[8], bi[8];
        for (n = 0; n < 4; n++) { ar[n] = a[n]; ai[n] = 0; }
        for (n = 0; n < 8; n++) { br[n] = b[n]; bi[n] = z[n]; }
        fft(ar, ai, 4, 0);
        fft(br, bi, 8, 0);
        printf("4-point DFT of {1,2,0,-1}:\n");
        for (k = 0; k < 4; k++) printf("  X[%d] = %8.4f %+8.4f j\n", k, ar[k], ai[k]);
        printf("8-point DFT of {1,1,1,1,0,0,0,0}:\n");
        for (k = 0; k < 8; k++)
            printf("  X[%d] = %8.4f %+8.4f j   |X| = %.4f\n", k, br[k], bi[k],
                   sqrt(br[k] * br[k] + bi[k] * bi[k]));
    }

    /* ------------------------------------------------------------- (b) */
    printf("\nFFT vs DFT (random complex input), inverse FFT round trip:\n");
    printf("      N    max|FFT-DFT|   max|IFFT(FFT x)-x|   Parseval rel.err\n");
    srand(1);
    for (N = 8; N <= 4096; N *= 4) {
        double e1 = 0, e2 = 0, ex = 0, eX = 0;
        for (n = 0; n < N; n++) { xr[n] = yr[n] = urand(); xi[n] = yi[n] = urand(); }
        dft(xr, xi, Xr, Xi, N);
        fft(yr, yi, N, 0);
        for (k = 0; k < N; k++) {
            double d = hypot(yr[k] - Xr[k], yi[k] - Xi[k]);
            if (d > e1) e1 = d;
            eX += yr[k] * yr[k] + yi[k] * yi[k];
            ex += xr[k] * xr[k] + xi[k] * xi[k];
        }
        fft(yr, yi, N, 1);
        for (n = 0; n < N; n++) {
            double d = hypot(yr[n] - xr[n], yi[n] - xi[n]);
            if (d > e2) e2 = d;
        }
        printf("  %5d    %10.2e      %10.2e          %10.2e\n", N, e1, e2,
               fabs(eX / N - ex) / ex);
    }
    /* conjugate symmetry for real input */
    {
        double es = 0;
        N = 64;
        for (n = 0; n < N; n++) { xr[n] = urand(); xi[n] = 0; }
        fft(xr, xi, N, 0);
        for (k = 1; k < N; k++)
            es = fmax(es, hypot(xr[k] - xr[N - k], xi[k] + xi[N - k]));
        printf("real input, N = 64: max |X[k] - X*[N-k]| = %.2e, Im X[0] = %.1e, Im X[N/2] = %.1e\n",
               es, xi[0], xi[N / 2]);
    }

    /* ------------------------------------------------------------- (c) */
    {
        double Nd[16], td[16], Nf[16], tf[16], cf[16];
        int nd = 0, nf = 0;
        printf("\nruntime per transform:\n");
        printf("      N      DFT [ms]     FFT [ms]   speed-up  FFT/(N log2 N) [ns]\n");
        for (N = 16; N <= NMAX; N *= 2) {
            double t_d = -1, t_f;
            for (n = 0; n < N; n++) { xr[n] = urand(); xi[n] = urand(); }
            if (N <= 2048) { t_d = timeit(0, N, xr, xi, Xr, Xi); Nd[nd] = N; td[nd++] = t_d; }
            t_f = timeit(1, N, xr, xi, Xr, Xi);
            Nf[nf] = N; tf[nf] = t_f; cf[nf++] = t_f / (N * log2((double)N)) * 1e9;
            if (t_d > 0)
                printf("  %6d   %10.4f   %10.5f   %8.1f   %8.2f\n", N, t_d * 1e3,
                       t_f * 1e3, t_d / t_f, cf[nf - 1]);
            else
                printf("  %6d          -     %10.5f          -   %8.2f\n", N,
                       t_f * 1e3, cf[nf - 1]);
        }
        printf("fitted exponent: DFT t ~ N^%.2f, FFT t ~ N^%.2f\n",
               fit_exponent(Nd + 2, td + 2, nd - 2), fit_exponent(Nf + 2, tf + 2, nf - 2));
        printf("real multiplications for N = 1024: DFT 4N^2 = %.0f, FFT 2N log2 N = %.0f\n",
               4.0 * 1024 * 1024, 2.0 * 1024 * 10);
        for (i = 0; i < nf; i++) Nf[i] = log2(Nf[i]);
        ap_plot_xy(Nf, cf, nf, "FFT runtime / (N log2 N) in ns over log2 N");
    }
    return 0;
}
