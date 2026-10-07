/*
 * fft.h -- iterative radix-2 FFT (header only, C99, no complex.h)
 *          Sheet 8, Exercise 8.2 (REFERENCE SOLUTION); reused on later sheets
 *
 *   void fft(double *re, double *im, int n, int inverse);
 *
 *   In-place transform of the complex sequence re[k] + j im[k], k = 0..n-1.
 *   n must be a power of two (otherwise the data are left unchanged).
 *     inverse = 0:  X[k] = sum_n x[n] e^{-j 2 pi k n / N}        (forward)
 *     inverse = 1:  x[n] = 1/N sum_k X[k] e^{+j 2 pi k n / N}    (inverse,
 *                   including the factor 1/N)
 *
 *   Algorithm: decimation in time. 1) reorder the input in bit-reversed
 *   order, 2) log2(n) stages of butterflies
 *        a' = a + W b,   b' = a - W b,   W = e^{-+j 2 pi m / L}
 *   with L = 2, 4, ..., n. (n/2) log2(n) butterflies, each one complex
 *   multiplication (4 real mul + 2 add) and two complex additions (4 add).
 *
 * Works unchanged on Arduino (AVR: double = 32-bit float) and ESP32.
 */
#ifndef FFT_H
#define FFT_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 1 if n is a power of two (n >= 1) */
static int fft_ispow2(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

static void fft(double *re, double *im, int n, int inverse)
{
    int i, j, k, L, half, bit;
    double sgn = inverse ? 1.0 : -1.0;

    if (!fft_ispow2(n) || n < 2) return;

    /* 1) bit-reversal permutation */
    for (i = 1, j = 0; i < n; i++) {
        bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j |= bit;
        if (i < j) {
            double t;
            t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }

    /* 2) butterflies, stage by stage: L = length of the sub-DFTs */
    for (L = 2; L <= n; L <<= 1) {
        double ang = sgn * 2.0 * M_PI / L;
        double cr = cos(ang), ci = sin(ang);     /* W_L = e^{-+j 2 pi / L} */
        half = L >> 1;
        for (i = 0; i < n; i += L) {             /* every sub-DFT block    */
            double wr = 1.0, wi = 0.0, t;        /* W_L^0                  */
            for (k = 0; k < half; k++) {
                int a = i + k, b = a + half;
                double tr = wr * re[b] - wi * im[b];   /* W b */
                double ti = wr * im[b] + wi * re[b];
                re[b] = re[a] - tr;  im[b] = im[a] - ti;
                re[a] += tr;         im[a] += ti;
                t  = wr * cr - wi * ci;          /* W_L^{k+1} = W_L^k W_L  */
                wi = wr * ci + wi * cr;
                wr = t;
            }
        }
    }

    /* 3) scaling of the inverse transform */
    if (inverse) {
        double s = 1.0 / n;
        for (i = 0; i < n; i++) { re[i] *= s; im[i] *= s; }
    }
}

#endif /* FFT_H */
