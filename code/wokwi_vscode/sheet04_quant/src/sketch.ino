/*
 * Sheet 4, Exercise 4.3 -- Quantisation noise and sampling jitter
 * (student template -- complete the parts marked TODO)
 *
 * ESP32 used as a plain C computer: no circuit, the program runs once in
 * setup() and prints its results (and ASCII plots) to the Serial Monitor.
 * (asciiplot.h in the same folder)
 *
 * a) quantise a full-scale sine with N = 4..16 bits, measure the SNR and
 *    compare with 6.02 N + 1.76 dB
 * b) same for a sine 20 dB below full scale
 * c) ideal (unquantised) sampling with Gaussian timing jitter sigma_j:
 *    SNR versus signal frequency, compare with -20 log10(2 pi f sigma_j)
 * d) ADC with 12 bit AND jitter: effective number of bits (ENOB)
 *
 * The sample arrays are float (on the heap) to fit into the RAM of the
 * ESP32; all computations and sums are done in double. float is exact
 * enough here: its rounding error (~1e-7 relative) is far below the
 * smallest error measured (16 bit: ~1e-5).
 * The ESP32 computes double in software: the run takes a while.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "asciiplot.h"

#define FS   50000.0       /* sampling rate, Hz                         */
#define NS   10000         /* number of samples (0.2 s)                 */
#define F0   1234.5        /* test frequency, not commensurate with FS  */

/* ---------------- small Gaussian random generator ------------------ */
static unsigned int rng = 2463534242u;
static double urand(void)                   /* uniform in (0,1) */
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return ((rng >> 8) + 0.5) / 16777216.0;
}
static double grand(void)                   /* N(0,1), Box-Muller */
{
    double u1 = urand(), u2 = urand();
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ---------------- (a) uniform mid-tread quantiser ------------------ */
/* full-scale range [-1, 1), N bits: step delta = 2 / 2^N,
   output codes -2^(N-1) .. 2^(N-1)-1 (clipped), returns the value */
static double quantise(double x, int nbits)
{
    /* TODO (a): step delta = 2 / 2^N; round x/delta to the nearest
       integer q, clip q to -2^(N-1) .. 2^(N-1)-1, return q * delta     */
    (void)nbits;
    return x;
}

/* SNR in dB of x (reference) and its distorted version y */
static double snr_db(const float *x, const float *y, int n)
{
    /* TODO (a): signal power sum x^2, error power sum (y-x)^2, ratio in dB
       (accumulate in double)                                            */
    (void)x; (void)y; (void)n;
    return 0.0;
}

static float *x, *y;                         /* NS samples each (heap) */

/* quantised sine of amplitude A: returns measured SNR */
static double run_quant(double A, int nbits)
{
    int n;
    for (n = 0; n < NS; n++) {
        x[n] = (float)(A * sin(2.0 * M_PI * F0 * n / FS + 0.3));
        y[n] = (float)quantise(x[n], nbits);
    }
    return snr_db(x, y, NS);
}

/* jittered sampling of a sine with frequency f, jitter std sj (s);
   if nbits > 0 the jittered sample is also quantised */
static double run_jitter(double f, double sj, int nbits)
{
    double A = 0.999, v;
    int n;
    for (n = 0; n < NS; n++) {
        double t = n / FS;
        x[n] = (float)(A * sin(2.0 * M_PI * f * t + 0.3));     /* ideal  */
        /* TODO (c): sample at the jittered instant t + sj * grand()     */
        v = x[n]; (void)sj; (void)grand;
        if (nbits > 0) v = quantise(v, nbits);
        y[n] = (float)v;
    }
    return snr_db(x, y, NS);
}

static void run(void)
{
    static double dev[13];
    static const double fj[] = {100, 1000, 3000, 6500, 10500, 20000};
    static const double sj[] = {0.1e-6, 1e-6, 4e-6};
    int i, k;

    x = (float *)malloc(NS * sizeof(float));
    y = (float *)malloc(NS * sizeof(float));
    if (!x || !y) { printf("out of memory\n"); return; }

    /* ---------------- a) + b) quantisation ---------------- */
    printf("a/b) quantised sine, f0 = %.1f Hz, fs = %.0f Hz, %d samples\n",
           F0, FS, NS);
    printf("  N   SNR(full scale)  6.02N+1.76   SNR(-20 dBFS)  expected\n");
    for (i = 0; i < 13; i++) {
        int N = 4 + i;
        double delta = 2.0 / (double)(1L << N);
        double s1 = run_quant(1.0 - delta / 2, N);   /* full scale, no clip */
        double s2 = run_quant(0.1, N);
        double th = 6.02 * N + 1.76;
        dev[i] = s1 - th;
        printf(" %2d   %8.2f dB     %8.2f dB    %8.2f dB   %8.2f dB\n",
               N, s1, th, s2, th - 20.0);
    }
    ap_size(52, 12);
    ap_stem(dev, 13, "SNR(measured) - (6.02N+1.76) [dB] for N = 4..16");
    ap_size(72, 20);

    /* ---------------- c) jitter ---------------- */
    printf("\nc) ideal sampling with Gaussian timing jitter\n");
    printf("  f [Hz]   sigma_j [us]   SNR meas [dB]   -20log10(2 pi f sj) [dB]\n");
    for (k = 0; k < 3; k++)
        for (i = 0; i < 6; i++) {
            double s = run_jitter(fj[i], sj[k], 0);
            double th = -20.0 * log10(2.0 * M_PI * fj[i] * sj[k]);
            printf("  %7.0f   %6.1f        %8.2f        %8.2f\n",
                   fj[i], sj[k] * 1e6, s, th);
        }

    /* ---------------- d) 12-bit ADC + jitter: ENOB ---------------- */
    printf("\nd) 12-bit quantiser plus jitter: SNR and ENOB = (SNR-1.76)/6.02\n");
    printf("  f [Hz]   sigma_j [us]   SNR [dB]   ENOB [bit]\n");
    for (k = 0; k < 3; k++)
        for (i = 0; i < 6; i += (i == 0 ? 1 : 2)) {
            double s = run_jitter(fj[i], sj[k], 12);
            printf("  %7.0f   %6.1f      %7.2f     %5.2f\n",
                   fj[i], sj[k] * 1e6, s, (s - 1.76) / 6.02);
        }
    printf("  frequency where jitter noise = 12-bit quantisation noise, "
           "sigma_j = 1 us: %.1f Hz\n",
           pow(10.0, -(6.02 * 12 + 1.76) / 20.0) / (2.0 * M_PI * 1e-6));
    free(x); free(y);
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    run();
    printf("\ndone.\n");
}

void loop() {}
