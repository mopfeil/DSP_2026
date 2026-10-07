/*
 * Sheet 10 -- Exercise 10.2 (d): coefficient quantisation (reference solution)
 *
 * Build and run:   gcc -O2 -o coefquant coefquant.c -lm
 *                  ./coefquant            (fs = 25 kHz)
 *                  ./coefquant 200000     (same analog spec at fs = 200 kHz)
 * (filtdesign.h, asciiplot.h in the same directory)
 *
 * The Butterworth knock band-pass of filterdesign.c is realised
 *   (1) as cascade of biquads, every coefficient in Q2.(B-2)
 *   (2) as ONE direct-form polynomial of order 2n (expanded cascade),
 *       every coefficient vector with its own best binary point
 * for word lengths B = 8..24 bits. Reported: largest pole displacement,
 * largest pole radius (stability), worst pass-band deviation from the
 * unquantised response and worst stop-band gain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "filtdesign.h"
#include "asciiplot.h"

#define FP1 6000.0
#define FP2 7000.0
#define FST1 5000.0
#define FST2 8000.0
#define AP 1.0
#define AS 40.0
#define MAXORD (2 * FD_MAXSEC)

/* polynomial in z^-1: p[0] + p[1] z^-1 + ... + p[m] z^-m */
static void poly_mul(const double *a, int na, const double *b, int nb, double *c)
{
    int i, j;
    for (i = 0; i <= na + nb; i++) c[i] = 0.0;
    for (i = 0; i <= na; i++)
        for (j = 0; j <= nb; j++) c[i + j] += a[i] * b[j];
}

static cplx poly_eval(const double *p, int m, double w)  /* at z = e^{jw} */
{
    cplx s = c_make(0.0, 0.0);
    int k;
    for (k = 0; k <= m; k++) s = c_add(s, c_scale(c_expj(-w * k), p[k]));
    return s;
}

/* roots of z^m + p1 z^(m-1) + ... + pm (p[0] = 1), Durand-Kerner */
static void poly_roots(const double *p, int m, cplx *r)
{
    int i, j, it;
    for (i = 0; i < m; i++) r[i] = c_scale(c_expj(2.0 * M_PI * i / m + 0.4), 0.9);
    for (it = 0; it < 2000; it++) {
        double change = 0.0;
        for (i = 0; i < m; i++) {
            cplx num = c_make(1.0, 0.0), den = c_make(1.0, 0.0), d;
            for (j = 1; j <= m; j++) num = c_add(c_mul(num, r[i]), c_make(p[j], 0.0));
            for (j = 0; j < m; j++) if (j != i) den = c_mul(den, c_sub(r[i], r[j]));
            d = c_div(num, den);
            r[i] = c_sub(r[i], d);
            change += c_abs(d);
        }
        if (change < 1e-14) break;
    }
}

/* largest distance of any root in q from the nearest root in r */
static double max_root_shift(const cplx *r, const cplx *q, int m)
{
    double worst = 0.0;
    int i, j;
    for (i = 0; i < m; i++) {
        double best = 1e9;
        for (j = 0; j < m; j++) { double d = c_abs(c_sub(q[i], r[j])); if (d < best) best = d; }
        if (best > worst) worst = best;
    }
    return worst;
}

static double max_radius(const cplx *r, int m)
{
    double mx = 0.0;
    int i;
    for (i = 0; i < m; i++) if (c_abs(r[i]) > mx) mx = c_abs(r[i]);
    return mx;
}

/* quantise a vector with its own binary point (B bits incl. sign) */
static void quant_vec(const double *x, double *y, int m, int bits)
{
    double mx = 0.0;
    int k, ib;
    for (k = 0; k <= m; k++) if (fabs(x[k]) > mx) mx = fabs(x[k]);
    ib = (int)ceil(log2(mx + 1e-300) + 1e-12);   /* integer bits needed */
    if (ib < 0) ib = 0;
    for (k = 0; k <= m; k++) y[k] = fd_quant(x[k], bits, bits - 1 - ib);
}

/* worst pass-band deviation (dB) between H and Href and max stop-band gain */
static void compare(const biquad_t *s, int ns, const double *B, const double *A, int m,
                    const biquad_t *sref, int nsref, double fs, double *pdev, double *smax)
{
    int i;
    *pdev = 0.0; *smax = -300.0;
    for (i = 0; i <= 1000; i++) {
        double f = 0.5 * fs * i / 1000.0, w = 2.0 * M_PI * f / fs, mq, mr;
        if (s) mq = fd_db(c_abs(fd_sos_resp(s, ns, f, fs)));
        else mq = fd_db(c_abs(c_div(poly_eval(B, m, w), poly_eval(A, m, w))));
        mr = fd_db(c_abs(fd_sos_resp(sref, nsref, f, fs)));
        if (f >= FP1 && f <= FP2 && fabs(mq - mr) > *pdev) *pdev = fabs(mq - mr);
        if ((f <= FST1 || f >= FST2) && mq > *smax) *smax = mq;
    }
}

int main(int argc, char **argv)
{
    double fs = argc > 1 ? atof(argv[1]) : 25000.0;
    biquad_t sos[FD_MAXSEC], sq[FD_MAXSEC];
    double A[MAXORD + 1], Bn[MAXORD + 1], Aq[MAXORD + 1], Bq[MAXORD + 1], t[MAXORD + 1];
    cplx r_ref[MAXORD], r_q[MAXORD], p1, p2;
    double lam, amax = 0.0;
    int n, ns, m, k, i, bits;
    static const int bl[] = {8, 10, 12, 14, 16, 20, 24, 32};

    n = fd_butter_bp_order(FP1, FP2, FST1, FST2, AP, AS, fs, &lam);
    ns = fd_butter_bandpass(sos, n, FP1, FP2, AP, fs);
    m = 2 * ns;
    printf("fs = %.0f Hz: Butterworth band-pass, %d biquads (order %d)\n", fs, ns, m);
    fd_biquad_poles(&sos[ns - 1], &p1, &p2);
    printf("pole angle of last section %.1f deg, radius %.5f\n",
           atan2(p1.im, p1.re) * 180.0 / M_PI, c_abs(p1));

    /* expanded direct form */
    A[0] = 1.0; Bn[0] = 1.0;
    for (k = 0; k < ns; k++) {
        int deg = 2 * k;
        poly_mul(A, deg, sos[k].a, 2, t); for (i = 0; i <= deg + 2; i++) A[i] = t[i];
        poly_mul(Bn, deg, sos[k].b, 2, t); for (i = 0; i <= deg + 2; i++) Bn[i] = t[i];
    }
    for (k = 0; k <= m; k++) if (fabs(A[k]) > amax) amax = fabs(A[k]);
    printf("direct form denominator: max |a_k| = %.3f (needs %d integer bits)\n",
           amax, (int)ceil(log2(amax)));
    for (k = 0; k < ns; k++) fd_biquad_poles(&sos[k], &r_ref[2 * k], &r_ref[2 * k + 1]);

    printf("\n         |------- biquad cascade (Q2.B-2) ------|"
           "  |------- direct form, order %2d -------|\n", m);
    printf(" B bits  | max|dp|   max|p|   pass dev  stop max |"
           "  max|dp|   max|p|   pass dev  stop max\n");
    for (i = 0; i < (int)(sizeof bl / sizeof bl[0]); i++) {
        double dp1, r1, pd1, sm1, dp2, r2, pd2, sm2;
        bits = bl[i];
        /* (1) cascade */
        for (k = 0; k < ns; k++) {
            int j;
            sq[k].a[0] = 1.0;
            quant_vec(sos[k].b, sq[k].b, 2, bits);   /* numerator: own binary point */
            for (j = 1; j < 3; j++) sq[k].a[j] = fd_quant(sos[k].a[j], bits, bits - 2);
            fd_biquad_poles(&sq[k], &r_q[2 * k], &r_q[2 * k + 1]);
        }
        dp1 = max_root_shift(r_ref, r_q, m);
        r1 = max_radius(r_q, m);
        compare(sq, ns, NULL, NULL, 0, sos, ns, fs, &pd1, &sm1);
        /* (2) direct form */
        quant_vec(A, Aq, m, bits);
        quant_vec(Bn, Bq, m, bits);
        Aq[0] = 1.0;            /* a0 = 1 is exactly representable as a shift */
        poly_roots(Aq, m, r_q);
        dp2 = max_root_shift(r_ref, r_q, m);
        r2 = max_radius(r_q, m);
        compare(NULL, 0, Bq, Aq, m, sos, ns, fs, &pd2, &sm2);
        printf("  %2d     | %8.2e  %7.5f  %7.2f   %7.1f  |  %8.2e  %7.5f  ",
               bits, dp1, r1, pd1, sm1, dp2, r2);
        if (r2 >= 1.0) printf("  UNSTABLE\n");
        else printf("%8.2f  %7.1f\n", pd2, sm2);
    }

    /* single 2nd-order band-pass: pole radius vs word length */
    {
        biquad_t b2, b2q;
        double r0, fp0;
        fd_bp2(&b2, 6500.0, 400.0, fs);
        fd_biquad_poles(&b2, &p1, &p2);
        r0 = c_abs(p1); fp0 = atan2(p1.im, p1.re) / (2 * M_PI) * fs;
        printf("\nsingle biquad B=400 Hz: r = %.6f, f_p = %.1f Hz, -3 dB bandwidth ~ (1-r) fs/pi = %.0f Hz\n",
               r0, fp0, (1.0 - r0) * fs / M_PI);
        for (bits = 6; bits <= 16; bits += 2) {
            int j;
            b2q.a[0] = 1.0;
            quant_vec(b2.b, b2q.b, 2, bits);
            for (j = 1; j < 3; j++) b2q.a[j] = fd_quant(b2.a[j], bits, bits - 2);
            fd_biquad_poles(&b2q, &p1, &p2);
            printf("  %2d bits: r = %.6f  f_p = %7.1f Hz  bandwidth ~ %5.0f Hz  peak gain %+.2f dB\n",
                   bits, c_abs(p1), atan2(p1.im, p1.re) / (2 * M_PI) * fs,
                   (1.0 - c_abs(p1)) * fs / M_PI, fd_db(fd_sos_peak(&b2q, 1, fs, NULL)));
        }
    }
    return 0;
}
