/*
 * Sheet 7, Exercise 7.3 -- Lambda sensor: first-order lag + transport delay
 *                          (REFERENCE SOLUTION)
 *
 * Build and run:   gcc -O2 -o lambda_pade lambda_pade.c -lm
 *                  ./lambda_pade
 * (asciiplot.h and csvio.h in the same directory)
 *
 * Model (same values as the Wokwi chip lambda-probe, wall film neglected):
 *
 *            e^{-s Td}
 *   G(s) = ----------- ,   Td = 200 ms (gas transport), T = 30 ms (sensor)
 *           1 + s T
 *
 * The delay is not a rational function of s. It is replaced by
 *   lag     e^{-sTd} ~ 1 / (1 + s Td)
 *   Pade 1  e^{-sTd} ~ (1 - s Td/2) / (1 + s Td/2)
 *   Pade 2  e^{-sTd} ~ (1 - s Td/2 + s^2 Td^2/12) / (1 + s Td/2 + s^2 Td^2/12)
 * Every approximation times the sensor lag is a strictly proper rational
 * transfer function that is simulated in controllable canonical form with
 * RK4. The exact system is simulated with a delay line (ring buffer).
 */
#include <stdio.h>
#include <math.h>
#include "asciiplot.h"
#include "csvio.h"

#define TD    0.200        /* transport delay, s       */
#define TS    0.030        /* sensor time constant, s  */
#define H     1e-4         /* integration step, s      */
#define TEND  1.0          /* simulated time, s        */
#define NS    10001        /* TEND / H + 1 samples     */
#define MAXN  4            /* max. system order        */

/* rational transfer function, polynomials in DESCENDING powers of s:
   G(s) = (b[0] s^m + ... + b[m]) / (a[0] s^n + ... + a[n]),  m < n */
typedef struct { int n, m; double a[MAXN + 1], b[MAXN + 1]; } tf_t;

/* polynomial product c = p * q (descending powers), returns degree */
static int polymul(const double *p, int np, const double *q, int nq, double *c)
{
    int i, j;
    for (i = 0; i <= np + nq; i++) c[i] = 0.0;
    for (i = 0; i <= np; i++)
        for (j = 0; j <= nq; j++) c[i + j] += p[i] * q[j];
    return np + nq;
}

/* ---------------------------------------------------------------- (a) */
/* derivative of the controllable canonical state space form
   x1' = x2, ..., xn' = -(a_n x1 + ... + a_1 xn)/a0 + u/a0  (monic: /a0) */
static void ccf_rhs(const tf_t *g, const double *x, double u, double *dx)
{
    int i, n = g->n;
    double s = u;
    for (i = 0; i < n - 1; i++) dx[i] = x[i + 1];
    for (i = 0; i < n; i++) s -= g->a[n - i] * x[i];
    dx[n - 1] = s / g->a[0];
}

static double ccf_out(const tf_t *g, const double *x)
{
    int i, m = g->m;
    double y = 0.0;
    for (i = 0; i <= m; i++) y += g->b[m - i] * x[i];   /* b_m x1 + ... */
    return y;
}

static void rk4(const tf_t *g, double *x, double u, double h)
{
    double k1[MAXN], k2[MAXN], k3[MAXN], k4[MAXN], xt[MAXN];
    int i, n = g->n;
    ccf_rhs(g, x, u, k1);
    for (i = 0; i < n; i++) xt[i] = x[i] + 0.5 * h * k1[i];
    ccf_rhs(g, xt, u, k2);
    for (i = 0; i < n; i++) xt[i] = x[i] + 0.5 * h * k2[i];
    ccf_rhs(g, xt, u, k3);
    for (i = 0; i < n; i++) xt[i] = x[i] + h * k3[i];
    ccf_rhs(g, xt, u, k4);
    for (i = 0; i < n; i++) x[i] += h / 6.0 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]);
}

/* unit step response of g, NS samples */
static void step_tf(const tf_t *g, double *y)
{
    double x[MAXN] = {0, 0, 0, 0};
    int k;
    for (k = 0; k < NS; k++) {
        y[k] = ccf_out(g, x);
        rk4(g, x, 1.0, H);
    }
}

/* exact system: delay line + first-order lag, exact discretisation of
   the lag for piecewise constant input (u is a step -> exact) */
static void step_exact_sim(double *y)
{
    static double buf[4096];
    int D = (int)(TD / H + 0.5), k, w = 0;
    double ys = 0.0, a = exp(-H / TS);
    for (k = 0; k < D; k++) buf[k] = 0.0;
    for (k = 0; k < NS; k++) {
        double ud = buf[w];          /* input delayed by D samples */
        buf[w] = 1.0;                /* current input (unit step)  */
        w = (w + 1) % D;
        y[k] = ys;
        ys = a * ys + (1.0 - a) * ud;
    }
}

/* analytic step response of the exact system */
static double step_exact(double t)
{
    return t < TD ? 0.0 : 1.0 - exp(-(t - TD) / TS);
}

/* ---------------------------------------------------------------- (c) */
/* frequency response of a tf_t at angular frequency w (phase unwrapped
   by the caller) */
static void tf_freq(const tf_t *g, double w, double *mag, double *ph)
{
    double nr = 0, ni = 0, dr = 0, di = 0, pr, pi;
    int i;
    /* Horner with complex s = j w */
    for (i = 0; i <= g->m; i++) { pr = -ni * w; pi = nr * w; nr = pr + g->b[i]; ni = pi; }
    for (i = 0; i <= g->n; i++) { pr = -di * w; pi = dr * w; dr = pr + g->a[i]; di = pi; }
    *mag = sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
    *ph = atan2(ni, nr) - atan2(di, dr);
}

int main(void)
{
    static double t[NS], ye[NS], ys[NS], y0[NS], y1[NS], y2[NS];
    tf_t g[3];
    const char *name[3] = {"lag 1/(1+sTd)", "Pade(1,1)    ", "Pade(2,2)    "};
    double lagT[2] = {TS, 1.0};
    double *yy[3] = {y0, y1, y2};
    int i, k;

    /* build the three approximations G_i(s) = P_i(s) / ((T s + 1) Q_i(s)) */
    {
        double q0[2] = {TD, 1.0}, p0[1] = {1.0};
        double p1[2] = {-TD / 2, 1.0}, q1[2] = {TD / 2, 1.0};
        double p2[3] = {TD * TD / 12, -TD / 2, 1.0}, q2[3] = {TD * TD / 12, TD / 2, 1.0};
        g[0].m = 0; g[0].b[0] = p0[0];
        g[0].n = polymul(q0, 1, lagT, 1, g[0].a);
        g[1].m = 1; g[1].b[0] = p1[0]; g[1].b[1] = p1[1];
        g[1].n = polymul(q1, 1, lagT, 1, g[1].a);
        g[2].m = 2; for (i = 0; i < 3; i++) g[2].b[i] = p2[i];
        g[2].n = polymul(q2, 2, lagT, 1, g[2].a);
    }

    /* poles and zeros (closed form) */
    printf("Lambda path G(s) = exp(-s*%.3f)/(1 + s*%.3f)\n", TD, TS);
    printf("  sensor pole            s = %.2f 1/s\n", -1.0 / TS);
    printf("  lag:     extra pole    s = %.2f\n", -1.0 / TD);
    printf("  Pade(1): pole s = %.2f, zero s = +%.2f (right half plane!)\n",
           -2.0 / TD, 2.0 / TD);
    {
        double re = -3.0 / TD, im = sqrt(3.0) / TD;
        printf("  Pade(2): poles s = %.2f +- j%.2f, zeros s = +%.2f +- j%.2f\n",
               re, im, -re, im);
    }

    /* ------------------------------------------------------------ (a),(b) */
    for (k = 0; k < NS; k++) { t[k] = k * H; ye[k] = step_exact(t[k]); }
    step_exact_sim(ys);
    {
        double e = 0.0;
        for (k = 0; k < NS; k++) if (fabs(ys[k] - ye[k]) > e) e = fabs(ys[k] - ye[k]);
        printf("\ndelay line + exact lag discretisation: max error %.2e\n", e);
    }
    printf("\nstep responses (0..%.1f s):\n", TEND);
    printf("  model          min(y)   t_min[ms]  t50[ms]  rms err  max err\n");
    for (i = 0; i < 3; i++) {
        double mn = 0, tmn = 0, t50 = -1, se = 0, me = 0, e;
        step_tf(&g[i], yy[i]);
        for (k = 0; k < NS; k++) {
            if (yy[i][k] < mn) { mn = yy[i][k]; tmn = t[k]; }
            if (t50 < 0 && yy[i][k] >= 0.5) t50 = t[k];
            e = yy[i][k] - ye[k];
            se += e * e;
            if (fabs(e) > me) me = fabs(e);
        }
        printf("  %s  %7.3f   %7.1f   %7.1f   %7.4f  %7.4f\n", name[i], mn,
               tmn * 1e3, t50 * 1e3, sqrt(se / NS), me);
    }
    {
        double t50 = TD + TS * log(2.0);
        printf("  exact          %7.3f   %7s   %7.1f\n", 0.0, "-", t50 * 1e3);
    }
    ap_plot2(ye, y1, NS, "step response: exact (*) vs Pade(1,1) (o), 0..1 s");
    ap_plot2(ye, y2, NS, "step response: exact (*) vs Pade(2,2) (o), 0..1 s");
    csv_write("lambda_step.csv", "t,exact,lag,pade1,pade2", NS, 5, t, ye, y0, y1, y2);

    /* ------------------------------------------------------------ (c) */
    printf("\nphase of the delay part: exact -w*Td vs approximations [deg]\n");
    printf("   f[Hz]   exact     lag     Pade1    Pade2\n");
    {
        double fz[] = {0.1, 0.5, 1.0, 2.0, 5.0};
        for (k = 0; k < 5; k++) {
            double w = 2 * M_PI * fz[k], m, p[3];
            for (i = 0; i < 3; i++) {
                tf_freq(&g[i], w, &m, &p[i]);
                p[i] += atan(w * TS);          /* remove sensor lag phase */
                while (p[i] > 0.1) p[i] -= 2 * M_PI;   /* unwrap (phase <= 0) */
            }
            printf("  %6.2f  %7.1f %7.1f  %7.1f  %7.1f\n", fz[k], -w * TD * 180 / M_PI,
                   p[0] * 180 / M_PI, p[1] * 180 / M_PI, p[2] * 180 / M_PI);
        }
    }
    /* frequency up to which the phase error stays below 5 degrees */
    for (i = 1; i < 3; i++) {
        double w, m, p, err;
        for (w = 0.01; w < 200.0; w += 0.001) {
            tf_freq(&g[i], w, &m, &p);
            p += atan(w * TS);
            while (p > 0.1) p -= 2 * M_PI;
            err = fabs(p + w * TD) * 180 / M_PI;
            if (err > 5.0) break;
        }
        printf("  %s: phase error > 5 deg above f = %.3f Hz (w*Td = %.3f)\n",
               name[i], w / (2 * M_PI), w * TD);
    }
    return 0;
}
