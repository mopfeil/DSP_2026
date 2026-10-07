/*
 * Sheet 4, Exercise 4.3 -- Quantisation noise and sampling jitter
 * (reference solution)
 *
 * Build and run:   gcc -O2 -o quant quant.c -lm
 *                  ./quant
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 * a) quantise a full-scale sine with N = 4..16 bits, measure the SNR and
 *    compare with 6.02 N + 1.76 dB
 * b) same for a sine 20 dB below full scale
 * c) ideal (unquantised) sampling with Gaussian timing jitter sigma_j:
 *    SNR versus signal frequency, compare with -20 log10(2 pi f sigma_j)
 * d) ADC with 12 bit AND jitter: effective number of bits (ENOB)
 */
#include <stdio.h>
#include <math.h>
#include "asciiplot.h"
#include "csvio.h"

#define FS   50000.0       /* sampling rate, Hz                         */
#define NS   100000        /* number of samples (2 s)                   */
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
    double delta = 2.0 / (double)(1L << nbits);
    long q = (long)floor(x / delta + 0.5);
    long qmax = (1L << (nbits - 1)) - 1, qmin = -(1L << (nbits - 1));
    if (q > qmax) q = qmax;
    if (q < qmin) q = qmin;
    return q * delta;
}

/* SNR in dB of x (reference) and its distorted version y */
static double snr_db(const double *x, const double *y, int n)
{
    double ps = 0.0, pe = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        ps += x[i] * x[i];
        pe += (y[i] - x[i]) * (y[i] - x[i]);
    }
    return 10.0 * log10(ps / pe);
}

static double x[NS], y[NS];

/* quantised sine of amplitude A: returns measured SNR */
static double run_quant(double A, int nbits)
{
    int n;
    for (n = 0; n < NS; n++) {
        x[n] = A * sin(2.0 * M_PI * F0 * n / FS + 0.3);
        y[n] = quantise(x[n], nbits);
    }
    return snr_db(x, y, NS);
}

/* jittered sampling of a sine with frequency f, jitter std sj (s);
   if nbits > 0 the jittered sample is also quantised */
static double run_jitter(double f, double sj, int nbits)
{
    double A = 0.999;
    int n;
    for (n = 0; n < NS; n++) {
        double t = n / FS;
        x[n] = A * sin(2.0 * M_PI * f * t + 0.3);              /* ideal  */
        y[n] = A * sin(2.0 * M_PI * f * (t + sj * grand()) + 0.3);
        if (nbits > 0) y[n] = quantise(y[n], nbits);
    }
    return snr_db(x, y, NS);
}

int main(void)
{
    static double nb[13], snr_fs[13], snr_th[13], dev[13];
    static const double fj[] = {100, 1000, 3000, 6500, 10500, 20000};
    static const double sj[] = {0.1e-6, 1e-6, 4e-6};
    int i, k;

    /* ---------------- a) + b) quantisation ---------------- */
    printf("a/b) quantised sine, f0 = %.1f Hz, fs = %.0f Hz, %d samples\n",
           F0, FS, NS);
    printf("  N   SNR(full scale)  6.02N+1.76   SNR(-20 dBFS)  expected\n");
    for (i = 0; i < 13; i++) {
        int N = 4 + i;
        double delta = 2.0 / (double)(1L << N);
        double s1 = run_quant(1.0 - delta / 2, N);   /* full scale, no clip */
        double s2 = run_quant(0.1, N);
        nb[i] = N; snr_fs[i] = s1; snr_th[i] = 6.02 * N + 1.76;
        dev[i] = s1 - snr_th[i];
        printf(" %2d   %8.2f dB     %8.2f dB    %8.2f dB   %8.2f dB\n",
               N, s1, snr_th[i], s2, snr_th[i] - 20.0);
    }
    ap_size(52, 12);
    ap_stem(dev, 13, "SNR(measured) - (6.02N+1.76) [dB] for N = 4..16");
    ap_size(72, 20);
    csv_write("quant_snr.csv", "N,snr_meas,snr_formula", 13, 3, nb, snr_fs, snr_th);

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
    return 0;
}
