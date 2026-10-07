/*
 * Sheet 7, Exercise 7.4 -- From s to z: discretisation of analog systems
 *                          (TEMPLATE, preview of Sheets 9/10)
 *
 * Build and run:   gcc -O2 -o discretise discretise.c -lm
 *                  ./discretise
 * (asciiplot.h and csvio.h in the same directory)
 *
 * Two analog prototypes (at most second order)
 *     H(s) = (b0 s^2 + b1 s + b2) / (a0 s^2 + a1 s + a2)
 *   1) knock resonator  H(s) = w0^2 / (s^2 + 2 sigma s + w0^2)
 *   2) RC low-pass      H(s) = wc / (s + wc),  R = 1 kOhm, C = 10 nF
 * are converted into digital systems
 *     H(z) = (B0 + B1 z^-1 + B2 z^-2) / (1 + A1 z^-1 + A2 z^-2)
 * at fs = 40 kHz by
 *   forward Euler       s = (z - 1) / T
 *   impulse invariance  h[n] = T h(nT)   (poles z = e^{sT})
 *   bilinear transform  s = (2/T) (z - 1) / (z + 1)
 *   bilinear, prewarped s = K (z - 1) / (z + 1),  K = w_c / tan(w_c T / 2)
 * and their frequency responses are compared with the analog one.
 */
#include <stdio.h>
#include <math.h>
#include "asciiplot.h"
#include "csvio.h"

#define FS     40000.0
#define F_D    6500.0
#define TAU    0.8e-3
#define R_OHM  1000.0
#define C_F    10e-9
#define NF     400          /* frequency grid 0 .. fs/2 */

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

static cpx cscale(cpx a, double s) { return cx(a.re * s, a.im * s); }
static cpx cexpo(cpx a) { double m = exp(a.re); return cx(m * cos(a.im), m * sin(a.im)); }
static double cabs_(cpx a) { return sqrt(a.re * a.re + a.im * a.im); }
static cpx csqrt_(cpx a)
{
    double m = cabs_(a), r = sqrt(0.5 * (m + a.re)), i = sqrt(0.5 * (m - a.re));
    return cx(r, a.im < 0 ? -i : i);
}

typedef struct { double b[3], a[3]; } ana_t;   /* analog, descending s  */
typedef struct { double B[3], A[3]; } dig_t;   /* digital, z^0,z^-1,z^-2 */

/* analog frequency response H(j w) */
static cpx ana_resp(const ana_t *h, double w)
{
    cpx s = cx(0.0, w), s2 = cmul(s, s);
    cpx num = cadd(cadd(cscale(s2, h->b[0]), cscale(s, h->b[1])), cx(h->b[2], 0));
    cpx den = cadd(cadd(cscale(s2, h->a[0]), cscale(s, h->a[1])), cx(h->a[2], 0));
    return cdiv(num, den);
}

/* digital frequency response H(e^{j W}), W = 2 pi f / fs */
static cpx dig_resp(const dig_t *h, double W)
{
    cpx z1 = cx(cos(W), -sin(W)), z2 = cmul(z1, z1);
    cpx num = cadd(cadd(cx(h->B[0], 0), cscale(z1, h->B[1])), cscale(z2, h->B[2]));
    cpx den = cadd(cadd(cx(h->A[0], 0), cscale(z1, h->A[1])), cscale(z2, h->A[2]));
    return cdiv(num, den);
}

static void normalise(dig_t *d)
{
    int i;
    double a0 = d->A[0];
    for (i = 0; i < 3; i++) { d->B[i] /= a0; d->A[i] /= a0; }
}

/* ------------------------------------------------------------- (a) */
/* forward Euler: s = (z - 1)/T.  p(s) = c0 s^2 + c1 s + c2 becomes
   z^2: c0/T^2,  z^1: -2c0/T^2 + c1/T,  z^0: c0/T^2 - c1/T + c2 ;
   dividing by z^2 gives the coefficients of z^0, z^-1, z^-2 */
static void map_euler(const double *c, double T, double *C)
{
    /* TODO (a): coefficients of z^0, z^-1, z^-2 after s = (z - 1)/T */
    (void)T;
    C[0] = c[2]; C[1] = 0.0; C[2] = 0.0;
}

static dig_t euler(const ana_t *h, double T)
{
    dig_t d;
    map_euler(h->b, T, d.B);
    map_euler(h->a, T, d.A);
    if (h->a[0] == 0.0) {          /* first order: divide by z, not z^2 */
        d.B[0] = d.B[1]; d.B[1] = d.B[2]; d.B[2] = 0.0;
        d.A[0] = d.A[1]; d.A[1] = d.A[2]; d.A[2] = 0.0;
    }
    normalise(&d);
    return d;
}

/* bilinear: s = K (z - 1)/(z + 1); multiply by (z+1)^2:
   c0 K^2 (z-1)^2 + c1 K (z^2 - 1) + c2 (z+1)^2 */
static void map_bilin(const double *c, double K, double *C)
{
    /* TODO (a): coefficients after s = K (z - 1)/(z + 1), times (z + 1)^2 */
    (void)K;
    C[0] = c[2]; C[1] = 0.0; C[2] = 0.0;
}

static dig_t bilinear(const ana_t *h, double K)
{
    dig_t d;
    map_bilin(h->b, K, d.B);
    map_bilin(h->a, K, d.A);
    if (h->a[0] == 0.0) {          /* first order: multiply by (z+1) only */
        /* TODO (a): first-order case */
    }
    normalise(&d);
    return d;
}

/* impulse invariance: partial fractions H(s) = sum r_i / (s - p_i),
   H(z) = T sum r_i / (1 - e^{p_i T} z^-1)  (strictly proper H only) */
static dig_t impinv(const ana_t *h, double T)
{
    dig_t d;
    /* TODO (b): impulse invariance via partial fractions:
       poles p_i (roots of a0 s^2 + a1 s + a2), residues r_i = N(p_i)/D'(p_i),
       H(z) = T sum r_i / (1 - e^{p_i T} z^-1); first-order case separately.
       Placeholder: H(z) = 1 */
    (void)h; (void)T;
    d.B[0] = 1.0; d.B[1] = 0.0; d.B[2] = 0.0;
    d.A[0] = 1.0; d.A[1] = 0.0; d.A[2] = 0.0;
    return d;
}

/* poles of the digital system (roots of z^2 + A1 z + A2) */
static void dig_poles(const dig_t *d, cpx *p1, cpx *p2, int *np)
{
    if (d->A[2] == 0.0) { *p1 = cx(-d->A[1], 0); *np = 1; return; }
    {
        cpx sq = csqrt_(cx(d->A[1] * d->A[1] - 4 * d->A[2], 0));
        *p1 = cscale(cadd(cx(-d->A[1], 0), sq), 0.5);
        *p2 = cscale(csub(cx(-d->A[1], 0), sq), 0.5);
        *np = 2;
    }
}

static void report(const char *sysname, const ana_t *h, double fref,
                   const double *ftab, int nt, double *mag_out[5])
{
    double T = 1.0 / FS, wref = 2 * M_PI * fref;
    (void)wref;
    dig_t d[4];
    const char *mname[4] = {"forward Euler ", "impulse invar.", "bilinear      ", "bilin. prewarp"};
    int m, i, k;

    d[0] = euler(h, T);
    d[1] = impinv(h, T);
    d[2] = bilinear(h, 2.0 / T);
    d[3] = bilinear(h, 2.0 / T);   /* TODO (c): prewarped K = w_ref / tan(w_ref T / 2) */

    printf("\n=== %s, fs = %.0f Hz ===\n", sysname, FS);
    printf("  method          B0         B1         B2         A1         A2       |p|     arg p [Hz]\n");
    for (m = 0; m < 4; m++) {
        cpx p1, p2;
        int np;
        dig_poles(&d[m], &p1, &p2, &np);
        printf("  %s %10.6f %10.6f %10.6f %10.6f %10.6f  %7.4f  %8.1f%s\n", mname[m],
               d[m].B[0], d[m].B[1], d[m].B[2], d[m].A[1], d[m].A[2],
               cabs_(p1), atan2(fabs(p1.im), p1.re) / (2 * M_PI) * FS,
               cabs_(p1) >= 1.0 ? "  UNSTABLE" : "");
    }
    printf("  magnitude in dB:\n      f [Hz]  analog   Euler  imp.inv. bilin.  prewarp\n");
    for (i = 0; i < nt; i++) {
        double f = ftab[i];
        printf("  %10.0f %7.2f", f, 20 * log10(cabs_(ana_resp(h, 2 * M_PI * f))));
        for (m = 0; m < 4; m++)
            printf(" %7.2f", 20 * log10(cabs_(dig_resp(&d[m], 2 * M_PI * f / FS)) + 1e-300));
        printf("\n");
    }
    /* peak of the magnitude response (resonator only meaningful) */
    {
        double fpk[5], gpk[5], f, g;
        for (m = 0; m < 5; m++) { fpk[m] = 0; gpk[m] = -1e9; }
        for (f = 1.0; f < FS / 2; f += 1.0) {
            g = 20 * log10(cabs_(ana_resp(h, 2 * M_PI * f)));
            if (g > gpk[0]) { gpk[0] = g; fpk[0] = f; }
            for (m = 0; m < 4; m++) {
                g = 20 * log10(cabs_(dig_resp(&d[m], 2 * M_PI * f / FS)));
                if (g > gpk[m + 1]) { gpk[m + 1] = g; fpk[m + 1] = f; }
            }
        }
        printf("  peak:  analog %.0f Hz / %.2f dB", fpk[0], gpk[0]);
        for (m = 0; m < 4; m++)
            printf(";  %s %.0f Hz / %.2f dB", mname[m], fpk[m + 1], gpk[m + 1]);
        printf("\n");
    }
    /* magnitude curves on a linear grid 0 .. fs/2 for plotting */
    for (k = 0; k < NF; k++) {
        double f = (k + 0.5) * FS / 2 / NF;
        mag_out[0][k] = 20 * log10(cabs_(ana_resp(h, 2 * M_PI * f)));
        for (m = 0; m < 4; m++) {
            double g = 20 * log10(cabs_(dig_resp(&d[m], 2 * M_PI * f / FS)) + 1e-12);
            mag_out[m + 1][k] = g < -60 ? -60 : g;
        }
    }
}

int main(void)
{
    static double f[NF], mg[5][NF];
    double *mo[5];
    double sig = 1.0 / TAU, wd = 2 * M_PI * F_D, w0sq = wd * wd + sig * sig;
    double wc = 1.0 / (R_OHM * C_F);
    ana_t res = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}, rc = {{0, 0, 0}, {0, 0, 0}};
    double ftab_res[] = {0, 1000, 5000, 6000, 6500, 7000, 10500, 15000, 19999};
    double ftab_rc[] = {0, 1000, 6500, 10500, 15915, 19999};
    int k, m;

    for (m = 0; m < 5; m++) mo[m] = mg[m];
    for (k = 0; k < NF; k++) f[k] = (k + 0.5) * FS / 2 / NF;

    res.b[2] = w0sq;  res.a[1] = 2 * sig;  res.a[2] = w0sq;
    rc.b[2] = wc;     rc.a[1] = 1.0;       rc.a[2] = wc;

    printf("analog poles: resonator s = %.1f +- j%.1f, RC s = %.1f (fc = %.1f Hz)\n",
           -sig, wd, -wc, wc / (2 * M_PI));
    printf("exact pole map z = e^{sT}: resonator |z| = %.5f at %.1f Hz; RC z = %.5f\n",
           exp(-sig / FS), wd / (2 * M_PI), exp(-wc / FS));

    report("knock resonator (prewarp at 6.5 kHz)", &res, sqrt(w0sq) / (2 * M_PI), ftab_res, 9, mo);
    ap_plot2(mg[0], mg[3], NF, "resonator |H| dB, 0..20 kHz: analog (*) vs bilinear (o)");
    ap_plot2(mg[0], mg[4], NF, "resonator |H| dB, 0..20 kHz: analog (*) vs prewarped bilinear (o)");
    csv_write("disc_resonator.csv", "f,analog,euler,impinv,bilin,prewarp", NF, 6,
              f, mg[0], mg[1], mg[2], mg[3], mg[4]);

    report("RC low-pass (prewarp at fc)", &rc, wc / (2 * M_PI), ftab_rc, 6, mo);
    ap_plot2(mg[0], mg[2], NF, "RC |H| dB, 0..20 kHz: analog (*) vs impulse invariance (o)");
    csv_write("disc_rc.csv", "f,analog,euler,impinv,bilin,prewarp", NF, 6,
              f, mg[0], mg[1], mg[2], mg[3], mg[4]);
    return 0;
}
