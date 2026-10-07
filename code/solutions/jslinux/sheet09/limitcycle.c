/*
 * Sheet 9, Exercise 9.3 b)-d) -- Digital resonator: float vs. Q15
 *                                (REFERENCE SOLUTION, JSLinux part)
 *
 * Build and run:   gcc -O2 -o limitcycle limitcycle.c -lm
 *                  ./limitcycle
 * (resonator.h -- the same file as in the Wokwi project -- and asciiplot.h,
 *  csvio.h in the same directory)
 *
 *   (a) coefficient quantisation: effective pole radius / frequency
 *   (b) impulse response float vs. Q15
 *   (c) zero-input limit cycles for r -> 1, rounding vs. truncation
 *   (d) overflow: unnormalised resonator (b0 = 1), wrap-around vs. saturation
 */
#include <stdio.h>
#include <math.h>
#include "resonator.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS   40000.0f
#define F0   6500.0f
#define NLC  200000          /* length of the limit-cycle test (5 s) */

/* zero-input test: initial state y[-1] = 0.5 (16384 LSB), x = 0; returns
   the amplitude (LSB) in the last 1000 samples and the oscillation frequency (zero crossings) */
static void limit_cycle(float r, int rnd, int *amp, float *freq, int16_t *ylog)
{
    res_q_t q;
    float b0, a1, a2;
    int n, mx = 0, zc = 0;
    int16_t y, yprev = 0;
    res_design(F0, FS, r, 1, &b0, &a1, &a2);
    res_q_init(&q, b0, a1, a2, rnd, 1);
    q.y1 = 16384;                          /* initial state, zero input */
    for (n = 0; n < NLC; n++) {
        y = res_q_step(&q, 0);
        if (ylog && n >= NLC - 400) ylog[n - (NLC - 400)] = y;
        if (n >= NLC - 1000) {
            if (y > mx) mx = y;
            if (-y > mx) mx = -y;
            if ((y > 0 && yprev <= 0) || (y < 0 && yprev >= 0)) zc++;
        }
        yprev = y;
    }
    *amp = mx;
    *freq = zc / 2.0f / (1000.0f / FS);
}

int main(void)
{
    float rs[] = {0.9f, 0.97f, 0.99f, 0.995f, 0.999f, 0.9999f, 1.0f};
    int nr = 7, i, n;

    /* ------------------------------------------------------------ (a) */
    printf("(a) coefficients, f0 = %.0f Hz, fs = %.0f Hz (Q14: 1 = 16384)\n", F0, FS);
    printf("     r        b0        a1        a2      b0_q  a1_q   a2_q   r_q        f_q [Hz]  B~(1-r)fs/pi\n");
    for (i = 0; i < nr; i++) {
        float b0, a1, a2, rq, fq;
        res_q_t q;
        res_design(F0, FS, rs[i], 1, &b0, &a1, &a2);
        res_q_init(&q, b0, a1, a2, 1, 1);
        res_q_poles(&q, FS, &rq, &fq);
        printf("  %7.4f  %8.6f %9.6f %8.6f  %5d %6d %6d  %.6f  %8.1f  %7.1f\n", rs[i], b0,
               a1, a2, q.b0, q.a1, q.a2, rq, fq, (1 - rs[i]) * FS / M_PI);
    }

    /* ------------------------------------------------------------ (b) */
    {
        static double yf[400], yq[400];
        res_f_t f;
        res_q_t q;
        float b0, a1, a2, emax = 0;
        res_design(F0, FS, 0.97f, 1, &b0, &a1, &a2);
        res_f_init(&f, b0, a1, a2);
        res_q_init(&q, b0, a1, a2, 1, 1);
        for (n = 0; n < 400; n++) {
            yf[n] = res_f_step(&f, n == 0 ? 0.5f : 0.0f) * 32768.0;   /* in LSB */
            yq[n] = res_q_step(&q, n == 0 ? 16384 : 0);
            if (fabs(yf[n] - yq[n]) > emax) emax = (float)fabs(yf[n] - yq[n]);
        }
        printf("\n(b) impulse 0.5 delta, r = 0.97, rounding: max |y_float - y_Q15| = %.1f LSB, "
               "y_float[399] = %.3f LSB, y_Q15[399] = %.0f LSB\n", emax, yf[399], yq[399]);
        ap_plot2(yf, yq, 120, "(b) impulse response in LSB, n = 0..119: float (*), Q15 (o), r = 0.97");
    }

    /* ------------------------------------------------------------ (c) */
    printf("\n(c) zero-input limit cycles (last 1000 of %d samples)\n", NLC);
    printf("     r       a2_q   bound 0.5/(1-a2q)  round: amp[LSB] f[Hz]   trunc: amp[LSB] f[Hz]\n");
    for (i = 0; i < nr; i++) {
        int ar, at;
        float fr, ft, b0, a1, a2;
        res_q_t q;
        res_design(F0, FS, rs[i], 1, &b0, &a1, &a2);
        res_q_init(&q, b0, a1, a2, 1, 1);
        limit_cycle(rs[i], 1, &ar, &fr, NULL);
        limit_cycle(rs[i], 0, &at, &ft, NULL);
        printf("  %7.4f  %6d  %10.1f        %10d %8.0f     %10d %8.0f\n", rs[i], q.a2,
               q.a2 < 16384 ? 0.5 / (1.0 - q.a2 / 16384.0) : 1.0 / 0.0, ar, fr, at, ft);
    }
    {
        static int16_t yl[400];
        static double yd[400];
        int a;
        float f;
        limit_cycle(0.99f, 1, &a, &f, yl);
        for (n = 0; n < 400; n++) yd[n] = yl[n];
        ap_plot(yd, 400, "(c) r = 0.99, rounding: last 400 samples (LSB)");
        csv_write("limitcycle.csv", "y_q15", 400, 1, yd);
    }

    /* ------------------------------------------------------------ (d) */
    printf("\n(d) b0 = 1 (not normalised), r = 0.99, rounding, 1 s of data;\n"
           "    input: knock burst A e^{-t/0.8ms} sin(2 pi 6.5k t), 0..10 ms\n");
    {
        float amp[2] = {0.1f, 0.5f};
        int sat, j;
        for (j = 0; j < 2; j++)
            for (sat = 0; sat <= 1; sat++) {
                res_f_t f;
                res_q_t q;
                float b0, a1, a2, pk = 0;
                int mx_end = 0;
                res_design(F0, FS, 0.99f, 0, &b0, &a1, &a2);
                res_f_init(&f, b0, a1, a2);
                res_q_init(&q, b0, a1, a2, 1, sat);
                for (n = 0; n < 40000; n++) {
                    float t = n / FS;
                    float x = n < 400 ? amp[j] * expf(-t / 0.8e-3f) * sinf(2 * (float)M_PI * F0 * t) : 0.0f;
                    float yf = res_f_step(&f, x);
                    int16_t yq = res_q_step(&q, (int16_t)floorf(x * 32768.0f + 0.5f));
                    if (fabsf(yf) > pk) pk = fabsf(yf);
                    if (n >= 39000 && (yq > mx_end || -yq > mx_end)) mx_end = yq > 0 ? yq : -yq;
                }
                printf("    A = %.1f, %-11s: float peak |y| = %.2f (full scale 1), Q15 overflows = %5u, "
                       "max|y| in last 1000 samples = %5d LSB\n",
                       amp[j], sat ? "saturation" : "wrap-around", pk, q.novf, mx_end);
            }
    }
    return 0;
}
