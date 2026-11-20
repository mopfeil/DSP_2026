/*
 * Sheet 3, Exercise 3.1 -- convolution and smoothers in numbers
 * (student template)
 *
 * ESP32 used as a plain C computer: no circuit, the program runs once in
 * setup() and prints its results (and ASCII plots) to the Serial Monitor.
 * (asciiplot.h in the same folder)
 *
 * (a) convolution as a table of products x[k] h[n-k], sum check
 * (b) impulse and step response of MA and EMA by iterating the
 *     difference equations; closed forms; 50 % / 90 % points
 * (c) noise gain, mean delay, ramp lag, EMA with the noise gain of an MA
 * (d) complex exponential as eigenfunction, |H| at the 2nd engine order
 *
 * The filters are written as "one sample in, one sample out" functions
 * (ma_step, ema_step) -- exactly as they would run in a real-time loop.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "asciiplot.h"

#define MAXMA  64            /* max. moving-average length            */
#define LH     1000          /* length of the computed impulse responses */

/* ---- streaming filters ------------------------------------------------ */
typedef struct {             /* moving average over N samples, at rest = 0 */
    int N, idx;
    double buf[MAXMA], sum;
} ma_t;

typedef struct {             /* exponential smoother, at rest = 0          */
    double a, y;
} ema_t;

typedef struct { double re, im; } cplx;   /* complex number for (d) */

static void ma_init(ma_t *f, int N)
{
    int i;
    f->N = N; f->idx = 0; f->sum = 0.0;
    for (i = 0; i < MAXMA; i++) f->buf[i] = 0.0;
}

/* (b) y[n] = y[n-1] + (x[n] - x[n-N]) / N with a ring buffer of the
   last N inputs; returns y[n]                                          */
static double ma_step(ma_t *f, double x)
{
    /* TODO (b): running sum: add x, subtract the input of N samples ago
       (f->buf[f->idx]), store x there, advance f->idx (modulo f->N),
       return sum / N.                                                  */
    (void)f;
    return x;
}

static void ema_init(ema_t *f, double a) { f->a = a; f->y = 0.0; }

/* (b) y[n] = a y[n-1] + (1-a) x[n]; returns y[n] */
static double ema_step(ema_t *f, double x)
{
    /* TODO (b): y[n] = a y[n-1] + (1-a) x[n], state in f->y */
    (void)f;
    return x;
}

/* ---- (b) closed forms ------------------------------------------------- */
/* TODO (b): read the pattern off the printed table (and derive it from the
   difference equation), then return the closed forms of h[n] and s[n]
   for n >= 0. The program prints the max. deviation from the iteration. */
static double h_ma_formula(int n, int N)     { (void)n; (void)N; return 0.0; }
static double s_ma_formula(int n, int N)     { (void)n; (void)N; return 0.0; }
static double h_ema_formula(int n, double a) { (void)n; (void)a; return 0.0; }
static double s_ema_formula(int n, double a) { (void)n; (void)a; return 0.0; }

/* ---- helpers ------------------------------------------------------------ */
static unsigned rng = 4711u;
static double urand(void)                  /* uniform in [-1, 1) */
{
    rng = rng * 1103515245u + 12345u;
    return (rng >> 1) / 1073741824.0 - 1.0;
}

/* rounding residue (e.g. -1e-17) and -0 printed as 0 */
static double nz(double v) { return fabs(v) < 5e-9 ? 0.0 : v; }

/* first index n with s[n] >= level (-1 if never) */
static int first_reach(const double *s, int n, double level)
{
    int i;
    for (i = 0; i < n; i++) if (s[i] >= level - 1e-12) return i;
    return -1;
}

/* impulse (kind 0) or step (kind 1) response of an MA (N > 0) or an
   EMA (N == 0, coefficient a), length n, computed by iteration         */
static void response(int kind, int N, double a, double *out, int n)
{
    static ma_t ma;
    ema_t ema;
    int i;
    if (N > 0) ma_init(&ma, N); else ema_init(&ema, a);
    for (i = 0; i < n; i++) {
        double x = kind == 0 ? (i == 0 ? 1.0 : 0.0) : 1.0;
        out[i] = N > 0 ? ma_step(&ma, x) : ema_step(&ema, x);
    }
}

/* ---- (c) characteristics of an impulse response h[0..n-1] ------------- */
static void characteristics(const double *h, int n, double *sum, double *ng,
                            double *D, double *Ds)
{
    double s = 0.0, q = 0.0, d = 0.0, ds = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        s += h[i];                    /* s = step response s[i]          */
        /* TODO (c): accumulate the noise gain q = sum h^2, the mean delay
           d = sum n h[n] and ds = sum (1 - s[n])                         */
    }
    *sum = s; *ng = q; *D = d; *Ds = ds;
}

/* white-noise test: output variance / input variance (streaming) */
static double noise_test(int N, double a, int n)
{
    static ma_t ma;
    ema_t ema;
    double vx = 0.0, vy = 0.0;
    int i;
    if (N > 0) ma_init(&ma, N); else ema_init(&ema, a);
    for (i = 0; i < n; i++) {
        double x = urand(), y = N > 0 ? ma_step(&ma, x) : ema_step(&ema, x);
        if (i >= 500) { vx += x * x; vy += y * y; }   /* skip start-up */
    }
    return vy / vx;
}

/* ramp test x[n] = n: returns y[n] - n at n = nend (= -lag) */
static double ramp_test(int N, double a, int nend)
{
    static ma_t ma;
    ema_t ema;
    double y = 0.0;
    int i;
    if (N > 0) ma_init(&ma, N); else ema_init(&ema, a);
    for (i = 0; i <= nend; i++) y = N > 0 ? ma_step(&ma, (double)i) : ema_step(&ema, (double)i);
    return y - nend;
}

/* noise gain of the EMA from its computed impulse response */
static double ema_noise_gain(double a)
{
    static double h[LH];
    double s, q, d, ds;
    response(0, 0, a, h, LH);
    characteristics(h, LH, &s, &q, &d, &ds);
    return q;
}

/* ---- (d) frequency response ------------------------------------------------ */

static cplx cdiv(cplx a, cplx b)
{
    double d = b.re * b.re + b.im * b.im;
    cplx r = { (a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d };
    return r;
}

/* H(Omega) = sum_k h[k] e^{-j Omega k} */
static cplx freq_resp(const double *h, int n, double Om)
{
    cplx H = { 0.0, 0.0 };
    int k;
    /* TODO (d): H = sum_k h[k] (cos(Om k) - j sin(Om k)) */
    (void)h; (void)n; (void)Om; (void)k;
    return H;
}

/* feed x[n] = e^{j Omega n} (as cos and sin through two copies of the
   filter -- linearity) and print y[n]/x[n] for n = n0..n0+4             */
static void eigen_test(const char *name, int N, double a, double Om, int n0)
{
    static ma_t mc, ms;
    ema_t ec, es;
    int n;
    if (N > 0) { ma_init(&mc, N); ma_init(&ms, N); }
    else       { ema_init(&ec, a); ema_init(&es, a); }
    printf("  %-10s y[n]/x[n] =", name);
    for (n = 0; n < n0 + 5; n++) {
        double xc = cos(Om * n), xs = sin(Om * n);
        cplx x = { xc, xs }, y;
        y.re = N > 0 ? ma_step(&mc, xc) : ema_step(&ec, xc);
        y.im = N > 0 ? ma_step(&ms, xs) : ema_step(&es, xs);
        if (n >= n0) {
            cplx r = cdiv(y, x);
            printf(" %.4f%+.4fj", nz(r.re), nz(r.im));
        }
    }
    printf("\n");
}

static void run(void)
{
    static double h[LH], s[LH], h2[LH];
    const double a16 = 15.0 / 17.0;
    int i, n;

    /* ---- (a) product table ------------------------------------------ */
    {
        double x[4] = { 1, 2, 0, -1 }, hh[3] = { 1, -1, 0.5 }, y[6] = { 0 };
        int nx = 4, nh = 3, ny = nx + nh - 1, k;
        double sx = 0, sh = 0, sy = 0;
        printf("(a) table of products x[k] h[n-k]\n      k  x[k] |");
        for (n = 0; n < ny; n++) printf("  n=%d  ", n);
        printf("\n");
        for (k = 0; k < nx; k++) {
            printf("    %3d %5g |", k, x[k]);
            for (n = 0; n < ny; n++) {
                /* TODO (a): if h[n-k] exists (0 <= n-k < nh): product
                   p = x[k] h[n-k], add it to y[n] and print it with
                   printf(" %6.2f", nz(p)); otherwise print "      ."   */
                (void)hh; (void)nh;
                printf("      .");
            }
            printf("\n");
        }
        printf("    y[n]       |");
        for (n = 0; n < ny; n++) printf(" %6.2f", nz(y[n]));
        printf("\n");
        for (i = 0; i < nx; i++) sx += x[i];
        for (i = 0; i < nh; i++) sh += hh[i];
        for (i = 0; i < ny; i++) sy += y[i];
        printf("    sum y = %g,  sum x * sum h = %g * %g = %g\n", sy, sx, sh, sx * sh);
    }

    /* ---- (b) impulse and step responses by iteration ------------------ */
    {
        const int N = 16, NP = 40;
        double hm[40], sm[40], he[40], se[40];
        double dev = 0.0;
        response(0, N, 0, hm, NP);  response(1, N, 0, sm, NP);
        response(0, 0, a16, he, NP); response(1, 0, a16, se, NP);
        printf("\n(b) responses by iteration, MA16 and EMA a = 15/17\n");
        printf("     n    h_MA     s_MA     h_EMA    s_EMA\n");
        for (n = 0; n < 24; n++)
            printf("    %2d  %.5f  %.5f  %.5f  %.5f\n", n, hm[n], sm[n], he[n], se[n]);
        for (n = 0; n < NP; n++) {
            double d[4] = { hm[n] - h_ma_formula(n, N), sm[n] - s_ma_formula(n, N),
                            he[n] - h_ema_formula(n, a16), se[n] - s_ema_formula(n, a16) };
            for (i = 0; i < 4; i++) if (fabs(d[i]) > dev) dev = fabs(d[i]);
        }
        printf("    max |iteration - closed form| over n = 0..%d: %.2e\n", NP - 1, dev);
        printf("    MA16      : 50%% at n = %2d, 90%% at n = %2d\n",
               first_reach(sm, NP, 0.5), first_reach(sm, NP, 0.9));
        printf("    EMA 15/17 : 50%% at n = %2d, 90%% at n = %2d\n",
               first_reach(se, NP, 0.5), first_reach(se, NP, 0.9));
        ap_size(60, 12);
        ap_plot2(sm, se, NP, "(b) step responses: MA16 (*), EMA 15/17 (o)");
    }

    /* ---- (c) noise gain, mean delay, ramp lag ------------------------- */
    {
        double sum, ng, D, Ds;
        int N;
        printf("\n(c) characteristics (from the computed impulse responses)\n");
        printf("    filter      sum h   noise gain  meas.(noise)  D=sum n h  sum(1-s)  ramp lag\n");
        response(0, 16, 0, h, LH);
        characteristics(h, LH, &sum, &ng, &D, &Ds);
        printf("    MA16       %.4f    %.5f     %.5f      %7.3f   %7.3f   %7.3f\n",
               sum, ng, noise_test(16, 0, 20000), D, Ds, -ramp_test(16, 0, 400));
        response(0, 0, a16, h, LH);
        characteristics(h, LH, &sum, &ng, &D, &Ds);
        printf("    EMA 15/17  %.4f    %.5f     %.5f      %7.3f   %7.3f   %7.3f\n",
               sum, ng, noise_test(0, a16, 20000), D, Ds, -ramp_test(0, a16, 400));

        /* EMA with the noise gain of MA_N: bisection on a */
        printf("\n    EMA with the noise gain of MA_N (bisection on a)\n");
        printf("      N   NG(MA)    D(MA)   a (bisection)  (N-1)/(N+1)   D(EMA)\n");
        for (N = 4; N <= 32; N *= 2) {
            double lo = 0.0, hi = 0.999, am = 0.5, dm;
            int it;
            response(0, N, 0, h, LH);
            characteristics(h, LH, &sum, &ng, &D, &Ds);
            for (it = 0; it < 40; it++) {         /* NG(a) decreases with a */
                am = 0.5 * (lo + hi);
                /* TODO (c): bisection -- compare ema_noise_gain(am) with the
                   noise gain ng of the MA and move lo or hi to am          */
                (void)lo; (void)hi; (void)ema_noise_gain;
            }
            response(0, 0, am, h2, LH);
            characteristics(h2, LH, &sum, &ng, &dm, &Ds);
            printf("     %2d   %.5f  %6.3f   %.8f     %.8f    %6.3f\n", N, 1.0 / N, D,
                   am, (N - 1.0) / (N + 1.0), dm);
        }
    }

    /* ---- (d) eigenfunction and |H| ------------------------------------ */
    {
        double Om = 2.0 * M_PI / 30.0;
        int ord;
        printf("\n(d) x[n] = exp(j Omega n), Omega = 2 pi/30 (2nd engine order)\n");
        eigen_test("MA16", 16, 0, Om, 300);
        eigen_test("MA30", 30, 0, Om, 300);
        eigen_test("EMA 15/17", 0, a16, Om, 300);
        printf("    H(Omega) = sum h[k] e^{-j Omega k}:\n");
        response(0, 16, 0, h, LH);
        {
            cplx H = freq_resp(h, LH, Om);
            printf("  MA16      H = %.4f%+.4fj  |H| = %.4f\n", nz(H.re), nz(H.im), hypot(H.re, H.im));
        }
        response(0, 30, 0, h2, LH);
        {
            cplx H = freq_resp(h2, LH, Om);
            printf("  MA30      H = %.4f%+.4fj  |H| = %.2e\n", nz(H.re), nz(H.im), hypot(H.re, H.im));
        }
        response(0, 0, a16, s, LH);
        {
            cplx H = freq_resp(s, LH, Om);
            printf("  EMA 15/17 H = %.4f%+.4fj  |H| = %.4f\n", nz(H.re), nz(H.im), hypot(H.re, H.im));
        }
        printf("\n    |H| at the engine orders (60 samples per revolution)\n");
        printf("    order  Omega    MA16    MA30    EMA 15/17\n");
        for (ord = 1; ord <= 6; ord++) {
            double O = 2.0 * M_PI * ord / 60.0;
            cplx A = freq_resp(h, LH, O), B = freq_resp(h2, LH, O), C = freq_resp(s, LH, O);
            printf("     %d    %.4f   %.4f  %.4f  %.4f\n", ord, O, hypot(A.re, A.im),
                   hypot(B.re, B.im), hypot(C.re, C.im));
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    run();
    printf("\ndone.\n");
}

void loop() {}
