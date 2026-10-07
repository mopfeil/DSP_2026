/*
 * fft.h -- iterative radix-2 FFT (in place)
 *
 *   void fft(double *re, double *im, int n, int inverse);
 *
 * n must be a power of two. inverse = 0: X[k] = sum_n x[n] e^{-j2pi kn/n}
 * inverse = 1: x[n] = (1/n) sum_k X[k] e^{+j2pi kn/n}  (includes 1/n).
 * Same API as the FFT of Sheet 8 -- you may use your own fft.h from
 * Sheet 8 instead (this course only uses the forward transform here).
 */
#ifndef FFT_H
#define FFT_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__GNUC__)
__attribute__((unused))
#endif
static void fft(double *re, double *im, int n, int inverse)
{
    int i, j, k, m, len;
    double sgn = inverse ? 1.0 : -1.0;
    /* bit-reversal permutation */
    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    /* butterflies */
    for (len = 2; len <= n; len <<= 1) {
        double ang = sgn * 2.0 * M_PI / len;
        double wr = cos(ang), wi = sin(ang);
        m = len >> 1;
        for (i = 0; i < n; i += len) {
            double cr = 1.0, ci = 0.0;
            for (k = 0; k < m; k++) {
                int a = i + k, b = i + k + m;
                double tr = re[b] * cr - im[b] * ci;
                double ti = re[b] * ci + im[b] * cr;
                double t;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr;        im[a] += ti;
                t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
    if (inverse)
        for (i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
}

#endif /* FFT_H */
