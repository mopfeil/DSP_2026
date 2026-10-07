/*
 * Sheet 9, Exercise 9.2 -- Pole-zero explorer   (TEMPLATE)
 *
 * Build:  gcc -O2 -o pz_explorer pz_explorer.c -lm
 *         (asciiplot.h and csvio.h in the same directory)
 * Usage:
 *   ./pz_explorer [-fs FS] [-n NIMP] -b b0 b1 ... -a a0 a1 ...
 *   ./pz_explorer [-fs FS] -res F0 R      two-pole resonator at F0 Hz,
 *                                         pole radius R, gain 1 at F0
 *                                         (R = 1: b0 = sin(2 pi F0/FS))
 *   ./pz_explorer                         demo: -fs 40000 -res 6500 0.97
 *
 *            b0 + b1 z^-1 + ... + bM z^-M
 *   H(z) = --------------------------------  ,  a0 != 0
 *            a0 + a1 z^-1 + ... + aN z^-N
 *
 * The program
 *   (a) computes poles and zeros (roots of z^L B(z) and z^L A(z),
 *       L = max(M, N)) with the Durand-Kerner iteration,
 *   (b) prints them in polar form, decides stability and plots the
 *       pole-zero map (ap_pz),
 *   (c) evaluates the frequency response H(e^{jW}) directly AND from the
 *       pole-zero geometry (product of distances), plots magnitude and phase,
 *   (d) computes the impulse response from the difference equation.
 * Output file: pz_resp.csv (f, |H| dB, phase deg).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "asciiplot.h"
#include "csvio.h"

#define MAXC 33              /* max. number of coefficients (order 32) */
#define NF   2001            /* frequency grid points 0 .. fs/2        */

/* ------------------------------------------------ tiny complex library */
typedef struct { double re, im; } cpx;
static cpx cx(double re, double im) { cpx z; z.re = re; z.im = im; return z; }
static cpx cadd(cpx a, cpx b) { return cx(a.re + b.re, a.im + b.im); }
static cpx csub(cpx a, cpx b) { return cx(a.re - b.re, a.im - b.im); }
static cpx cmul(cpx a, cpx b) { return cx(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }
static cpx cdiv(cpx a, cpx b)
{
    double d = b.re * b.re + b.im * b.im;
    return cx((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d);
}

static double cabs_(cpx a) { return hypot(a.re, a.im); }

/* ------------------------------------------------------------- (a) */
/* roots of p[0] z^n + p[1] z^(n-1) + ... + p[n]  (p[0] != 0),
   Durand-Kerner (Weierstrass) iteration; returns number of iterations */
static int poly_roots(const double *p, int n, cpx *r)
{
    /* TODO (a): Durand-Kerner iteration for the monic polynomial
         c(z) = z^n + c1 z^(n-1) + ... + cn,  c_i = p[i] / p[0]
       start values r_i = (0.4 + 0.9j)^(i+1), repeat until max |change| < 1e-14:
         r_i <- r_i - c(r_i) / prod_{j != i} (r_i - r_j)
       Placeholder: all roots at z = 0 */
    int i;
    (void)p;
    for (i = 0; i < n; i++) r[i] = cx(0.0, 0.0);
    return 0;
}

/* roots of z^L * (q0 + q1 z^-1 + ... + qK z^-K): exact zeros at z = 0 for
   vanishing trailing coefficients, Durand-Kerner for the rest.
   Leading zero coefficients (q0 = 0, ...) are roots "at infinity" and are
   skipped. Returns the number of finite roots. */
static int tf_roots(const double *q, int K, int L, cpx *r)
{
    double p[MAXC];
    int i, lead = 0, n, nz = 0;
    for (i = 0; i <= L; i++) p[i] = i <= K ? q[i] : 0.0;
    while (lead <= L && p[lead] == 0.0) lead++;           /* roots at infinity */
    if (lead > L) return 0;
    n = L - lead;                                          /* degree */
    while (n > 0 && p[lead + n] == 0.0) { r[nz++] = cx(0.0, 0.0); n--; }
    poly_roots(p + lead, n, r + nz);
    return nz + n;
}

/* ------------------------------------------------------------- (c) */
/* H(e^{jW}) directly from the coefficients */
static cpx freq_direct(const double *b, int M, const double *a, int N, double W)
{
    cpx nu = cx(0, 0), de = cx(0, 0);
    int k;
    for (k = 0; k <= M; k++) nu = cadd(nu, cx(b[k] * cos(W * k), -b[k] * sin(W * k)));
    for (k = 0; k <= N; k++) de = cadd(de, cx(a[k] * cos(W * k), -a[k] * sin(W * k)));
    return cdiv(nu, de);
}

/* |H(e^{jW})| from the geometry: g * prod |e^{jW} - z_k| / prod |e^{jW} - p_k| */
static double freq_geom(double g, const cpx *z, int nz, const cpx *p, int np, double W)
{
    /* TODO (c): |H| = |g| prod |e^{jW} - z_k| / prod |e^{jW} - p_k| */
    (void)g; (void)z; (void)nz; (void)p; (void)np; (void)W;
    return 1.0;
}
int main(int argc, char **argv)
{
    static double b[MAXC], a[MAXC], fgrid[NF], mag[NF], ph[NF], h[400];
    static double zr[MAXC], zi[MAXC], pr[MAXC], pi_[MAXC];
    cpx z[MAXC], p[MAXC];
    double fs = 40000.0, g, lb, la, rmax = 0.0, maxdiff = 0.0;
    int M = -1, N = -1, nimp = 60, i, k, nz, np, L, mode = 0;

    /* ---------------------------------------------- parse arguments */
    if (argc == 1) {                                      /* demo */
        static char *demo[] = {"pz", "-fs", "40000", "-res", "6500", "0.97"};
        argc = 6; argv = demo;
    }
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-fs") && i + 1 < argc) fs = atof(argv[++i]);
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) nimp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-b")) mode = 1;
        else if (!strcmp(argv[i], "-a")) mode = 2;
        else if (!strcmp(argv[i], "-res") && i + 2 < argc) {
            double f0 = atof(argv[++i]), r = atof(argv[++i]), th = 2 * M_PI * f0 / fs;
            /* unity gain at f0: b0 = (1 - r) |1 - r e^{-2 j th}| */
            b[0] = fabs(1 - r) * sqrt(1 - 2 * r * cos(2 * th) + r * r); M = 0;
            if (fabs(1 - r) < 1e-12) b[0] = sin(th);   /* r = 1: oscillator, h[n] = sin((n+1) th) */
            a[0] = 1.0; a[1] = -2.0 * r * cos(th); a[2] = r * r; N = 2;
        }
        else if (mode == 1 && M < MAXC - 1) b[++M] = atof(argv[i]);
        else if (mode == 2 && N < MAXC - 1) a[++N] = atof(argv[i]);
        else { fprintf(stderr, "bad argument '%s'\n", argv[i]); return 1; }
    }
    if (nimp > 400) nimp = 400;
    if (M < 0) { M = 0; b[0] = 1.0; }
    if (N < 0) { N = 0; a[0] = 1.0; }
    if (a[0] == 0.0) { fprintf(stderr, "a0 must not be 0\n"); return 1; }

    printf("H(z): b =");
    for (k = 0; k <= M; k++) printf(" %.6g", b[k]);
    printf("\n      a =");
    for (k = 0; k <= N; k++) printf(" %.6g", a[k]);
    printf("\n      fs = %.6g Hz\n", fs);

    /* ---------------------------------------------- (a) poles and zeros */
    L = M > N ? M : N;
    nz = tf_roots(b, M, L, z);
    np = tf_roots(a, N, L, p);
    printf("\n%d zero(s):\n", nz);
    for (k = 0; k < nz; k++)
        printf("  z%-2d = %9.5f %+9.5fj   |z| = %.5f   angle = %8.2f deg = %9.1f Hz\n",
               k, z[k].re, z[k].im, cabs_(z[k]), atan2(z[k].im, z[k].re) * 180 / M_PI,
               atan2(z[k].im, z[k].re) / (2 * M_PI) * fs);
    printf("%d pole(s):\n", np);
    for (k = 0; k < np; k++) {
        double r = cabs_(p[k]);
        if (r > rmax) rmax = r;
        printf("  p%-2d = %9.5f %+9.5fj   |p| = %.5f   angle = %8.2f deg = %9.1f Hz\n",
               k, p[k].re, p[k].im, r, atan2(p[k].im, p[k].re) * 180 / M_PI,
               atan2(p[k].im, p[k].re) / (2 * M_PI) * fs);
    }

    /* ---------------------------------------------- (b) stability, map */
    printf("\nmax |p| = %.6f -> causal system is %s\n", rmax,
           rmax < 1.0 - 1e-12 ? "STABLE" : (rmax <= 1.0 + 1e-12 ? "MARGINALLY STABLE (not BIBO stable)" : "UNSTABLE"));
    for (k = 0; k < nz; k++) { zr[k] = z[k].re; zi[k] = z[k].im; }
    for (k = 0; k < np; k++) { pr[k] = p[k].re; pi_[k] = p[k].im; }
    ap_size(61, 25);
    ap_pz(pr, pi_, np, zr, zi, nz, "pole-zero map");
    ap_size(72, 20);

    /* ---------------------------------------------- (c) frequency response */
    /* gain factor of the factorised form: leading nonzero coefficients */
    for (k = 0, lb = 0; k <= M; k++) if (b[k] != 0.0) { lb = b[k]; break; }
    la = a[0];
    g = lb / la;
    {
        int kpk = 0;
        for (k = 0; k < NF; k++) {
            double W = M_PI * k / (NF - 1);
            cpx H = freq_direct(b, M, a, N, W);
            double mg = cabs_(H), mgeo = freq_geom(g, z, nz, p, np, W);
            fgrid[k] = W / (2 * M_PI) * fs;
            mag[k] = 20 * log10(mg + 1e-300);
            if (mag[k] < -120) mag[k] = -120;
            ph[k] = atan2(H.im, H.re) * 180 / M_PI;
            if (mg > 1e-6 && fabs(20 * log10(mgeo / mg)) > maxdiff)
                maxdiff = fabs(20 * log10(mgeo / mg));
            if (mag[k] > mag[kpk]) kpk = k;
        }
        printf("\nmagnitude: geometric vs direct evaluation, max difference %.2e dB\n", maxdiff);
        if (rmax >= 1.0)
            printf("NOTE: unit circle not in the ROC -- H(e^jW) below is only a formal\n"
                   "      evaluation, NOT the frequency response of the causal system!\n");
        printf("|H| at f = 0: %.2f dB, at fs/2: %.2f dB, maximum %.2f dB at %.1f Hz\n",
               mag[0], mag[NF - 1], mag[kpk], fgrid[kpk]);
        if (kpk > 0 && kpk < NF - 1) {             /* -3 dB bandwidth of a peak */
            int lo = kpk, hi = kpk;
            while (lo > 0 && mag[lo] > mag[kpk] - 3.0103) lo--;
            while (hi < NF - 1 && mag[hi] > mag[kpk] - 3.0103) hi++;
            printf("-3 dB band %.1f .. %.1f Hz, B = %.1f Hz (grid %.1f Hz)\n",
                   fgrid[lo], fgrid[hi], fgrid[hi] - fgrid[lo], fs / 2 / (NF - 1));
        }
        ap_plot_xy(fgrid, mag, NF, "|H(e^jW)| in dB over f in Hz");
        ap_plot_xy(fgrid, ph, NF, "phase in degrees over f in Hz");
        csv_write("pz_resp.csv", "f,mag_db,phase_deg", NF, 3, fgrid, mag, ph);
    }

    /* ---------------------------------------------- (d) impulse response */
    for (i = 0; i < nimp; i++) {
        /* TODO (d): difference equation with x = delta:
           a0 h[i] = b_i - a1 h[i-1] - ... - aN h[i-N] */
        h[i] = 0.0;
    }
    printf("\nimpulse response h[0..9]:");
    for (i = 0; i < 10 && i < nimp; i++) printf(" %.4g", h[i]);
    printf("\n|h[%d]| = %.4g\n", nimp - 1, fabs(h[nimp - 1]));
    ap_stem(h, nimp < 72 ? nimp : 72, "impulse response h[n]");
    return 0;
}
