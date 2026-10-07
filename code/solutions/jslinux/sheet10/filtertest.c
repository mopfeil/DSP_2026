/*
 * Sheet 10 -- Exercise 10.2 (e): test the exported knock_filter.h on the
 * virtual engine (reference solution)
 *
 * Build and run:   ./filterdesign             (creates knock_filter.h)
 *                  gcc -O2 -o filtertest filtertest.c -lm
 *                  ./filtertest [knock_amp]    (default 0.3 V)
 * (engine_signals.h, asciiplot.h, csvio.h, knock_filter.h in the same dir)
 *
 * One knocking combustion of cylinder 1 at 3000 rpm, fs = 25 kHz, is
 * filtered with
 *   - the FIR filter        kf_fir[]     (float, direct convolution)
 *   - the biquad cascade    kf_sos[][]   (float, transposed direct form II)
 *   - the biquad cascade    kf_sos_q14   (Q15 data, Q14 coefficients,
 *                                         direct form I, 32-bit accumulator)
 * The program reports the output energy in the 10..70 deg ATDC window with
 * and without knock, the delay of the burst envelope, and the error of the
 * fixed-point implementation; writes filt.csv.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"
#include "knock_filter.h"

#define FS     25000.0
#define NREC   250            /* 180 deg at 3000 rpm = 10 ms     */
#define TH0    90.0           /* record starts 30 deg before TDC1 */
#define VFS    2.5            /* +-2.5 V sensor voltage = Q15 full scale */

/* ---- float FIR, circular buffer ----------------------------------- */
typedef struct { float buf[KF_FIR_N]; int pos; } fir_t;
static float fir_step(fir_t *f, float x)
{
    float acc = 0.0f;
    int k, i = f->pos;
    f->buf[i] = x;
    for (k = 0; k < KF_FIR_N; k++) {
        acc += kf_fir[k] * f->buf[i];
        if (--i < 0) i = KF_FIR_N - 1;
    }
    if (++f->pos == KF_FIR_N) f->pos = 0;
    return acc;
}

/* ---- float biquad cascade, transposed direct form II -------------- */
typedef struct { float s1[KF_NSEC], s2[KF_NSEC]; } sos_t;
static float sos_step(sos_t *f, float x)
{
    int k;
    for (k = 0; k < KF_NSEC; k++) {
        const float *c = kf_sos[k];
        float y = c[0] * x + f->s1[k];
        f->s1[k] = c[1] * x - c[3] * y + f->s2[k];
        f->s2[k] = c[2] * x - c[4] * y;
        x = y;
    }
    return x;
}

/* ---- Q15 biquad cascade, direct form I, saturating ---------------- */
typedef struct { int16_t x1[KF_NSEC], x2[KF_NSEC], y1[KF_NSEC], y2[KF_NSEC]; long nsat; } sosq_t;
static int16_t sat16(int32_t v, long *nsat)
{
    if (v > 32767) { (*nsat)++; return 32767; }
    if (v < -32768) { (*nsat)++; return -32768; }
    return (int16_t)v;
}
static int16_t sosq_step(sosq_t *f, int16_t x)
{
    int k;
    for (k = 0; k < KF_NSEC; k++) {
        const int16_t *c = kf_sos_q14[k];
        int32_t acc = (int32_t)c[0] * x + (int32_t)c[1] * f->x1[k] + (int32_t)c[2] * f->x2[k]
                    - (int32_t)c[3] * f->y1[k] - (int32_t)c[4] * f->y2[k];
        int16_t y = sat16((acc + (1 << 13)) >> 14, &f->nsat);  /* Q29 -> Q15, rounded */
        f->x2[k] = f->x1[k]; f->x1[k] = x;
        f->y2[k] = f->y1[k]; f->y1[k] = y;
        x = y;
    }
    return x;
}

/* run one record; returns window energies (V^2) of fir, iir, q15 */
static void run(double kamp, int with_knock, double *x, double *yf, double *yi, double *yq,
                double *th, double E[3], long *nsat)
{
    es_engine_t e;
    static fir_t fir; static sos_t sos; static sosq_t sq;
    int n;
    double th_prev;
    fir = (fir_t){{0}, 0}; sos = (sos_t){{0}, {0}};
    sq = (sosq_t){{0}, {0}, {0}, {0}, 0};
    es_init(&e, 3000.0, 7u);
    e.knock_amp = kamp;
    e.knock_prob = with_knock ? 1.0 : 0.0;
    E[0] = E[1] = E[2] = 0.0;
    /* run one engine cycle first (filters settle), stop at theta = TH0 */
    do {
        double v;
        th_prev = e.theta;
        es_advance(&e, 1.0 / FS);
        v = es_knock(&e);
        fir_step(&fir, (float)v); sos_step(&sos, (float)v);
        sosq_step(&sq, (int16_t)floor(v / VFS * 32768.0 + 0.5));
    } while (!(e.cycle >= 1 && th_prev < TH0 && e.theta >= TH0));
    /* the knock decision for cylinder 1 was drawn at 30 deg; fix it here */
    e.knock_flag[0]  = with_knock;
    e.knock_onset[0] = 135.0;              /* onset 15 deg ATDC */
    e.knock_ampl[0]  = kamp;
    for (n = 0; n < NREC; n++) {
        double v, d;
        es_advance(&e, 1.0 / FS);
        v = es_knock(&e);
        x[n] = v; th[n] = e.theta;
        yf[n] = fir_step(&fir, (float)v);
        yi[n] = sos_step(&sos, (float)v);
        yq[n] = sosq_step(&sq, (int16_t)floor(v / VFS * 32768.0 + 0.5)) * VFS / 32768.0;
        d = e.theta - 120.0;
        if (d >= 10.0 && d < 70.0) {
            E[0] += yf[n] * yf[n]; E[1] += yi[n] * yi[n]; E[2] += yq[n] * yq[n];
        }
    }
    *nsat = sq.nsat;
}

/* index of the maximum of |y| */
static int argmax_abs(const double *y, int n)
{
    int i, k = 0;
    for (i = 1; i < n; i++) if (fabs(y[i]) > fabs(y[k])) k = i;
    return k;
}

int main(int argc, char **argv)
{
    static double x[NREC], yf[NREC], yi[NREC], yq[NREC], th[NREC], t[NREC];
    double kamp = argc > 1 ? atof(argv[1]) : 0.3;
    double Ek[3], En[3], err = 0.0, ref = 0.0;
    long nsat;
    int n, kx, kf, ki;

    run(kamp, 0, x, yf, yi, yq, th, En, &nsat);
    run(kamp, 1, x, yf, yi, yq, th, Ek, &nsat);
    for (n = 0; n < NREC; n++) {
        t[n] = n / FS * 1e3;
        err += (yq[n] - yi[n]) * (yq[n] - yi[n]);
        ref += yi[n] * yi[n];
    }
    printf("knock amplitude %.2f V (onset 15 deg ATDC), noise 0.05 V, 3000 rpm, fs = 25 kHz\n", kamp);
    printf("window energy 10..70 deg ATDC (V^2):   knock      no knock    ratio\n");
    printf("   FIR (float, N=%d)                 %8.3f   %8.4f   %6.1f\n", KF_FIR_N, Ek[0], En[0], Ek[0] / En[0]);
    printf("   IIR biquads (float, %d sections)    %8.3f   %8.4f   %6.1f\n", KF_NSEC, Ek[1], En[1], Ek[1] / En[1]);
    printf("   IIR biquads (Q15/Q14)              %8.3f   %8.4f   %6.1f\n", Ek[2], En[2], Ek[2] / En[2]);
    printf("Q15 vs float IIR: relative rms error %.2e (%.1f dB), saturations %ld\n",
           sqrt(err / ref), 10.0 * log10(err / ref), nsat);
    for (kx = 0; kx < NREC - 1 && th[kx] < 135.0; kx++) { }   /* knock onset */
    kf = argmax_abs(yf, NREC); ki = argmax_abs(yi, NREC);
    printf("knock onset at %.2f ms (theta %.1f), peak |y_FIR| at %.2f ms (+%.2f), |y_IIR| at %.2f ms (+%.2f)\n",
           t[kx], th[kx], t[kf], t[kf] - t[kx], t[ki], t[ki] - t[kx]);
    ap_plot(x, NREC, "raw knock sensor signal, TDC-30 .. TDC+150 deg (10 ms)");
    ap_plot2(yf, yi, NREC, "band-pass output: '*' FIR, 'o' IIR (note the different delays)");
    csv_write("filt.csv", "t_ms,theta,x,y_fir,y_iir,y_q15", NREC, 6, t, th, x, yf, yi, yq);
    printf("wrote filt.csv\n");
    return 0;
}
