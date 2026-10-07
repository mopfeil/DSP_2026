/*
 * Sheet 3, Exercise 3.3 a) -- smoothing the per-tooth engine speed,
 * offline prediction (REFERENCE SOLUTION)
 *
 * Build and run:   gcc -O2 -o rpm_offline rpm_offline.c -lm
 *                  ./rpm_offline
 * (engine_signals.h, asciiplot.h and csvio.h in the same directory)
 *
 * The rising crank edges of the virtual engine are time-stamped with
 * 0.5 us resolution (Timer1 of the Uno, as in Exercise 1.3). From the
 * tooth periods a per-tooth speed sequence r[k] is formed (three ways
 * of handling the gap) and smoothed with
 *   - a moving average over N samples        (FIR, LTI)
 *   - an exponential smoother, alpha         (IIR, LTI)
 * Scenarios: constant speed, speed step, speed ramp.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define TICK   0.5e-6        /* s, timer resolution                         */
#define MAXS   12000         /* max. number of speed samples                */
#define GAP_NAIVE  0         /* gap period used like a normal tooth period  */
#define GAP_ONE    1         /* one sample 3/T_gap                          */
#define GAP_THREE  2         /* three samples 3/T_gap (one per 6 deg)       */

static double r[MAXS], rt[MAXS], yma[MAXS], yema[MAXS];
static int    isgap[MAXS];

/* ---- speed profiles ------------------------------------------------ */
static double profile(int scen, double t)
{
    switch (scen) {
    case 1:  return t < 0.505 ? 3000.0 : 5000.0;  /* step at 90 deg */
    case 2:  if (t < 0.2) return 2000.0;                        /* ramp */
             if (t < 1.2) return 2000.0 + 3000.0 * (t - 0.2);
             return 5000.0;
    default: return 3000.0;                                     /* const */
    }
}

/* simulate the crank edges and build the per-tooth speed sequence r[]
   (rpm), the true mean speed rt[] and the gap marker isgap[];
   returns the number of samples                                        */
static int make_speed(int scen, double tend, int gap_mode)
{
    es_engine_t e;
    double dt = 1e-6, th_prev, t_prev;
    long stamp, last = -1, T, Tprev = 0;
    int ns = 0, synced = 0;
    es_init(&e, profile(scen, 0.0), 99u);
    while (e.t < tend && ns < MAXS - 3) {
        double a, b, edge;
        int k;
        e.rpm = profile(scen, e.t);
        th_prev = e.theta; t_prev = e.t;
        es_advance(&e, dt);
        a = fmod(th_prev, 360.0); b = a + (e.theta - th_prev);
        if (b < a) b += 720.0;
        k = (int)floor(a / 6.0) + 1;
        edge = 6.0 * k;
        if (edge > b || (k % 60) >= 58) continue;          /* no rising edge */
        stamp = (long)floor((t_prev + dt * (edge - a) / (b - a)) / TICK);
        if (last >= 0) {
            int gap, rep = 1, i;
            double v;
            T = stamp - last;
            gap = Tprev > 0 && T > 2 * Tprev;
            Tprev = T;
            if (gap) synced = 1;
            if (synced) {
                /* ---- (a) per-tooth speed, gap handling ---------------- */
                v = 1.0 / (T * TICK);                      /* rpm = 1/T[s] */
                if (gap && gap_mode != GAP_NAIVE) v *= 3.0;
                if (gap && gap_mode == GAP_THREE) rep = 3;
                for (i = 0; i < rep; i++) {
                    r[ns] = v; rt[ns] = e.rpm; isgap[ns] = gap; ns++;
                }
            }
        }
        last = stamp;
    }
    return ns;
}

/* ---- (b) filters ------------------------------------------------------ */
/* moving average over N samples, running sum; the first N-1 outputs
   use the first sample as "history" (filter pre-loaded, no start-up)  */
static void moving_average(const double *x, int n, int N, double *y)
{
    double s = N * x[0];
    int k;
    for (k = 0; k < n; k++) {
        s += x[k] - (k >= N ? x[k - N] : x[0]);
        y[k] = s / N;
    }
}

/* exponential smoother y[k] = y[k-1] + alpha (x[k] - y[k-1]),
   pre-loaded with x[0]                                                 */
static void exp_smoother(const double *x, int n, double alpha, double *y)
{
    double s = x[0];
    int k;
    for (k = 0; k < n; k++) {
        s += alpha * (x[k] - s);
        y[k] = s;
    }
}

/* ---- evaluation ------------------------------------------------------ */
static void stat_const(const char *name, const double *y, int n)
{
    double m = 0, s2 = 0, mn = 1e9, mx = -1e9;
    int k, cnt = 0;
    for (k = 200; k < n; k++) {                     /* skip settling */
        m += y[k]; s2 += y[k] * y[k]; cnt++;
        if (y[k] < mn) mn = y[k];
        if (y[k] > mx) mx = y[k];
    }
    m /= cnt;
    printf("  %-10s mean %7.1f  std %6.2f  min %7.1f  max %7.1f\n", name, m,
           sqrt(s2 / cnt - m * m), mn, mx);
}

static void stat_step(const char *name, const double *y, int n)
{
    int k0, k, k50 = -1, k90 = -1;
    for (k0 = 0; k0 < n && rt[k0] < 4000.0; k0++) {}   /* first sample at 5000 */
    for (k = k0; k < n; k++) {
        if (k50 < 0 && y[k] >= 4000.0) k50 = k;
        if (k90 < 0 && y[k] >= 4800.0) { k90 = k; break; }
    }
    printf("  %-10s 50%% after %3d samples, 90%% after %3d samples\n", name,
           k50 - k0, k90 - k0);
}

static void stat_ramp(const char *name, const double *y, int n)
{
    double err = 0.0, slope = 0.0, rpm = 0.0;
    int k, cnt = 0, k0 = -1, k1 = -1;
    for (k = 0; k < n; k++) {                         /* middle of the ramp */
        if (k0 < 0 && rt[k] > 3000.0) k0 = k;
        if (k1 < 0 && rt[k] > 4000.0) k1 = k;
    }
    for (k = k0; k < k1; k++) { err += rt[k] - y[k]; rpm += rt[k]; cnt++; }
    err /= cnt; rpm /= cnt;
    slope = (rt[k1] - rt[k0]) / (k1 - k0);            /* rpm per sample */
    printf("  %-10s lag error %6.1f rpm = %5.2f samples = %5.2f ms at %4.0f rpm\n",
           name, err, err / slope, err / slope * 1e3 / rpm,   /* 1 sample = 1/rpm s */
           rpm);
}

int main(void)
{
    static const char *gname[3] = {"naive gap", "gap once", "gap x3"};
    int n, g, N;
    double alpha16 = 2.0 / 17.0;

    printf("(b) constant 3000 rpm, 1 s\n");
    for (g = 0; g < 3; g++) {
        n = make_speed(0, 1.0, g);
        printf(" gap handling: %s (%d samples)\n", gname[g], n);
        stat_const("raw", r, n);
        moving_average(r, n, 16, yma);  stat_const("MA16", yma, n);
        moving_average(r, n, 30, yma);  stat_const("MA30", yma, n);
        exp_smoother(r, n, alpha16, yema); stat_const("EMA 2/17", yema, n);
        if (g == GAP_NAIVE) {
            moving_average(r, n, 16, yma);
            exp_smoother(r, n, alpha16, yema);
            ap_size(72, 14);
            ap_plot2(yma + 400, yema + 400, 180,
                     "naive gap: MA16 (*) and EMA 2/17 (o), 180 samples");
        }
        if (g == GAP_THREE) {
            moving_average(r, n, 30, yma);
            ap_plot2(r + 400, yma + 400, 180,
                     "gap x3: raw (*) and MA30 (o), 180 samples = 3 revolutions");
        }
    }

    printf("\n(c) step 3000 -> 5000 rpm, gap x3\n");
    n = make_speed(1, 1.0, GAP_THREE);
    for (N = 8; N <= 32; N *= 2) {
        char nm[32];
        sprintf(nm, "MA%d", N);
        moving_average(r, n, N, yma); stat_step(nm, yma, n);
        sprintf(nm, "EMA 2/%d", N + 1);
        exp_smoother(r, n, 2.0 / (N + 1), yema); stat_step(nm, yema, n);
    }
    {
        int k0;
        for (k0 = 0; k0 < n && rt[k0] < 4000.0; k0++) {}
        moving_average(r, n, 16, yma);
        exp_smoother(r, n, alpha16, yema);
        ap_plot2(yma + k0 - 10, yema + k0 - 10, 60,
                 "step: MA16 (*) and EMA 2/17 (o), samples -10..49");
    }

    printf("\n(d) ramp 2000 -> 5000 rpm in 1 s (3000 rpm/s), gap x3\n");
    n = make_speed(2, 1.4, GAP_THREE);
    for (N = 8; N <= 32; N *= 2) {
        char nm[32];
        sprintf(nm, "MA%d", N);
        moving_average(r, n, N, yma); stat_ramp(nm, yma, n);
        sprintf(nm, "EMA 2/%d", N + 1);
        exp_smoother(r, n, 2.0 / (N + 1), yema); stat_ramp(nm, yema, n);
    }
    exp_smoother(r, n, alpha16, yema);
    csv_write("rpm_ramp.csv", "true,raw,ema16", n, 3, rt, r, yema);
    printf("wrote rpm_ramp.csv\n");
    return 0;
}
