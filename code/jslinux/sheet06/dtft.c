/*
 * Sheet 6, Exercise 6.2 c) -- DTFT of a knock burst by direct summation
 * (student template -- complete the parts marked TODO)
 *
 * Build and run:   gcc -O2 -o dtft dtft.c -lm
 *                  ./dtft
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 *   X(f) = sum_n x[n] exp(-j 2 pi f n / fs)     (f in Hz, period fs)
 * evaluated on a fine frequency grid (no FFT!).
 *   1. damped sinusoid exp(-t/tau) sin(2 pi f0 t), f0 = 6.5 kHz,
 *      tau = 0.4/0.8/1.6 ms: peak, -3 dB bandwidth, comparison with the
 *      closed-form DTFT and with the Lorentzian approximation
 *   2. periodicity and symmetry of the DTFT
 *   3. a real knock burst of the virtual engine (both modes + noise)
 */
#include <stdio.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS   50000.0          /* sampling rate, Hz        */
#define N    500              /* 10 ms record             */
#define NF   2501             /* grid 0 .. fs/2, 10 Hz    */
#define F0   6500.0

typedef struct { double re, im; } cpx;

static double x[N], fgrid[NF], mag[NF];

/* (a) DTFT of x[0..n-1] at frequency f (Hz) */
static cpx dtft(const double *s, int n, double f)
{
    /* TODO (a): X = sum_k s[k] exp(-j 2 pi f k / FS)                   */
    cpx X = {0.0, 0.0};
    (void)s; (void)n; (void)f;
    return X;
}

static double cabs_(cpx z) { return sqrt(z.re * z.re + z.im * z.im); }

/* closed form of the DTFT of a^n sin(W0 n) u[n] (infinite length):
   X = a sin(W0) e^{-jW} / (1 - 2 a cos(W0) e^{-jW} + a^2 e^{-2jW})     */
static double dtft_closed(double a, double W0, double W)
{
    double nr = a * sin(W0);
    double dr = 1.0 - 2.0 * a * cos(W0) * cos(W) + a * a * cos(2.0 * W);
    double di = 2.0 * a * cos(W0) * sin(W) - a * a * sin(2.0 * W);
    return fabs(nr) / sqrt(dr * dr + di * di);
}

/* spectrum on the grid, peak index, -3 dB bandwidth (linear interp.) */
static double analyse(const double *s, int n, double flo, double fhi,
                      double *fpk, double *pk)
{
    int i, ip = 0, il, ir;
    double half, fl, fr;
    for (i = 0; i < NF; i++) mag[i] = cabs_(dtft(s, n, fgrid[i]));
    while (ip < NF - 1 && fgrid[ip] < flo) ip++;     /* start inside range */
    for (i = ip; i < NF; i++)
        if (fgrid[i] >= flo && fgrid[i] <= fhi && mag[i] > mag[ip]) ip = i;
    half = mag[ip] / sqrt(2.0);
    for (il = ip; il > 0 && mag[il] > half; il--) { }
    for (ir = ip; ir < NF - 1 && mag[ir] > half; ir++) { }
    fl = fgrid[il] + (half - mag[il]) / (mag[il + 1] - mag[il]) * (fgrid[il + 1] - fgrid[il]);
    fr = fgrid[ir - 1] + (half - mag[ir - 1]) / (mag[ir] - mag[ir - 1]) * (fgrid[ir] - fgrid[ir - 1]);
    *fpk = fgrid[ip]; *pk = mag[ip];
    return fr - fl;
}

int main(void)
{
    static const double taus[3] = {0.4e-3, 0.8e-3, 1.6e-3};
    es_engine_t e;
    double fpk, pk, bw, a, maxdev = 0.0;
    int i, n, j;

    for (i = 0; i < NF; i++) fgrid[i] = 0.5 * FS * i / (NF - 1);

    /* ---- 1. damped sinusoid ------------------------------------------ */
    printf("1. damped sinusoid f0 = %.0f Hz, fs = %.0f Hz, N = %d (%.0f ms)\n",
           F0, FS, N, N / FS * 1e3);
    printf("   tau [ms]  peak [Hz]  |X| peak  fs*tau/2   BW_-3dB [Hz]  1/(pi tau) [Hz]   Q\n");
    for (j = 0; j < 3; j++) {
        for (n = 0; n < N; n++) x[n] = exp(-n / FS / taus[j]) * sin(2 * M_PI * F0 * n / FS);
        bw = analyse(x, N, 5000.0, 8000.0, &fpk, &pk);
        printf("   %5.1f     %7.0f    %7.2f   %7.2f    %7.0f        %7.0f      %5.1f\n",
               taus[j] * 1e3, fpk, pk, FS * taus[j] / 2, bw, 1.0 / (M_PI * taus[j]),
               fpk / bw);
        if (j == 1) {
            ap_size(72, 16);
            ap_plot(mag + 500, 401, "|X(f)|, tau = 0.8 ms, f = 5 .. 9 kHz (10 Hz/col)");
            csv_write("dtft_damped.csv", "f,absX", NF, 2, fgrid, mag);
        }
    }
    /* closed form vs. direct sum (tau = 0.8 ms) */
    a = exp(-1.0 / (FS * 0.8e-3));
    for (n = 0; n < N; n++) x[n] = pow(a, n) * sin(2 * M_PI * F0 * n / FS);
    for (i = 0; i < NF; i++) {
        double d = fabs(cabs_(dtft(x, N, fgrid[i])) - dtft_closed(a, 2 * M_PI * F0 / FS, 2 * M_PI * fgrid[i] / FS));
        if (d > maxdev) maxdev = d;
    }
    printf("   max | direct sum (N = %d) - closed form (N = inf) | = %.2e  (a^N = %.1e)\n",
           N, maxdev, pow(a, N));

    /* ---- 2. periodicity and symmetry --------------------------------- */
    printf("\n2. |X(f)| at f = 6000, 6000 + fs, -6000, fs - 6000 Hz: "
           "%.4f %.4f %.4f %.4f\n",
           cabs_(dtft(x, N, 6000.0)), cabs_(dtft(x, N, 6000.0 + FS)),
           cabs_(dtft(x, N, -6000.0)), cabs_(dtft(x, N, FS - 6000.0)));
    {
        cpx p = dtft(x, N, 6000.0), m = dtft(x, N, -6000.0);
        printf("   X(6000) = %.4f %+.4fj,  X(-6000) = %.4f %+.4fj  (conjugate symmetric)\n",
               p.re, p.im, m.re, m.im);
    }

    /* ---- 3. real knock burst from the virtual engine ----------------- */
    es_init(&e, 3000.0, 11u);
    e.knock_prob = 1.0;
    while (e.knock_t0 < 0.0) es_advance(&e, 1.0 / FS);     /* wait for onset */
    for (n = 0; n < N; n++) { x[n] = es_knock(&e); es_advance(&e, 1.0 / FS); }
    printf("\n3. knock burst of the virtual engine (amplitude %.2f V, %d samples):\n",
           e.knock_a, N);
    bw = analyse(x, N, 5000.0, 8000.0, &fpk, &pk);
    printf("   mode 1: peak %.0f Hz, |X| = %.2f, -3 dB bandwidth %.0f Hz\n", fpk, pk, bw);
    bw = analyse(x, N, 9500.0, 11500.0, &fpk, &pk);
    printf("   mode 2: peak %.0f Hz, |X| = %.2f, -3 dB bandwidth %.0f Hz\n", fpk, pk, bw);
    ap_plot(mag, NF, "|X(f)| of the knock burst, f = 0 .. 25 kHz");
    csv_write("dtft_knock.csv", "f,absX", NF, 2, fgrid, mag);
    return 0;
}
