/*
 * fft.h -- iterative radix-2 FFT (header only, C99, no complex.h)
 *          Sheet 8, Exercise 8.2 (TEMPLATE); reused on later sheets
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

    /* TODO 1) bit-reversal permutation: swap re/im[i] and re/im[j] for
               j = bit-reversed i (only if i < j) */

    /* TODO 2) log2(n) stages, L = 2, 4, ..., n: for every block of length L
               and k = 0 .. L/2-1 the butterfly
                   t = W_L^k * x[i+k+L/2],  x[i+k+L/2] = x[i+k] - t,  x[i+k] += t
               with W_L = e^{sgn j 2 pi / L} */
    (void)re; (void)im; (void)sgn; (void)i; (void)j; (void)k; (void)L; (void)half; (void)bit;

    /* TODO 3) inverse transform: scale by 1/n */
}

#endif /* FFT_H */
