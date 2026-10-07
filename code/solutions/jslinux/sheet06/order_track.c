/*
 * Sheet 6, Exercise 6.4 (challenge) -- Order tracking during a run-up
 * (reference solution)
 *
 * Build and run:   gcc -O2 -o order_track order_track.c -lm
 *                  ./order_track
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 * The engine accelerates from 1500 to 6000 rpm within 1 s. The knock-sensor
 * signal (no knock) is sampled at fs = 50 kHz, crank edges are time-stamped
 * with 2 us resolution (as in Exercise 4.4).
 *   a) DTFT of the time signal, 0..500 Hz: the engine orders are smeared
 *   b) resampling onto a 0.5 deg crank-angle grid (Exercise 4.4) and
 *      "angle-domain DTFT" over the ORDER axis 0..10: sharp order lines
 */
#include <stdio.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS      50000.0
#define NS      50000          /* 1 s                          */
#define DT_CAP  2e-6
#define RPM0    1500.0
#define RPM1    6000.0
#define DTH     0.5            /* deg                          */
#define MAXEDGE 4000
#define MAXA    50000
#define NFT     501            /* 0..500 Hz, 1 Hz              */
#define NORD    1001           /* order 0..10, step 0.01       */

static double ts[NS], xs[NS], te[MAXEDGE], the[MAXEDGE], xa[MAXA];
static double ft[NFT], at[NFT], ord[NORD], ao[NORD];

/* amplitude spectrum 2|sum s[k] exp(-j w k)|/n for w = 2 pi f[i] * scale */
static void amp_spec(const double *s, int n, const double *f, double scale,
                     double *A, int nf)
{
    int i, k;
    for (i = 0; i < nf; i++) {
        double w = 2.0 * M_PI * f[i] * scale, cw = cos(w), sw = sin(w);
        double pr = 1.0, pi = 0.0, re = 0.0, im = 0.0, t;
        for (k = 0; k < n; k++) {
            re += s[k] * pr; im -= s[k] * pi;
            t = pr * cw - pi * sw; pi = pr * sw + pi * cw; pr = t;
        }
        A[i] = 2.0 * sqrt(re * re + im * im) / n;
    }
}

static void remove_mean(double *s, int n)
{
    double m = 0.0;
    int k;
    for (k = 0; k < n; k++) m += s[k];
    for (k = 0; k < n; k++) s[k] -= m / n;
}

int main(void)
{
    es_engine_t e;
    int n, k, sub, ne = 0, kref = -1, na = 0, crank_old, nrev, i;
    int nsub = (int)(1.0 / FS / DT_CAP + 0.5);
    double t = 0.0, mx;

    /* ---- simulation: run-up, time samples and crank edges (given) ---- */
    es_init(&e, RPM0, 99u);
    e.knock_prob = 0.0;
    crank_old = es_crank(&e);
    for (n = 0; n < NS; n++) {
        for (sub = 0; sub < nsub; sub++) {
            e.rpm = RPM0 + (RPM1 - RPM0) * t;
            es_advance(&e, DT_CAP);
            t += DT_CAP;
            if (es_crank(&e) && !crank_old && ne < MAXEDGE) te[ne++] = t;
            crank_old = es_crank(&e);
        }
        ts[n] = t;
        xs[n] = es_knock(&e);
    }

    /* ---- a) time-domain spectrum ------------------------------------- */
    remove_mean(xs, NS);
    for (i = 0; i < NFT; i++) ft[i] = i;
    amp_spec(xs, NS, ft, 1.0 / FS, at, NFT);
    for (mx = 0.0, i = 1; i < NFT; i++) if (at[i] > mx) mx = at[i];
    printf("a) time domain, run-up %.0f -> %.0f rpm: largest amplitude in 1..500 Hz: %.4f V\n",
           RPM0, RPM1, mx);
    ap_size(72, 12);
    ap_plot(at, NFT, "a) |X(f)| amplitude, 0..500 Hz (time domain, run-up)");

    /* ---- b) angle domain: edge angles (gap detection), resampling ----- */
    for (k = 2; k < ne; k++) {
        double d1 = te[k] - te[k - 1], d0 = te[k - 1] - te[k - 2];
        if (kref < 0) { if (d1 > 2.0 * d0) { kref = k; the[k] = 0.0; } }
        else the[k] = the[k - 1] + (d1 > 2.0 * d0 ? 18.0 : 6.0);
    }
    {
        int ke = kref, ks = 0;
        while (na < MAXA) {
            double th = na * DTH, tq;
            if (th > the[ne - 1]) break;
            while (ke < ne - 2 && the[ke + 1] < th) ke++;
            tq = te[ke] + (te[ke + 1] - te[ke]) * (th - the[ke]) / (the[ke + 1] - the[ke]);
            if (tq >= ts[NS - 1]) break;
            while (ks < NS - 2 && ts[ks + 1] < tq) ks++;
            xa[na++] = xs[ks] + (xs[ks + 1] - xs[ks]) * (tq - ts[ks]) / (ts[ks + 1] - ts[ks]);
        }
    }
    nrev = (int)(na * DTH / 360.0);              /* whole revolutions only */
    na = (int)(nrev * 360.0 / DTH);
    remove_mean(xa, na);
    for (i = 0; i < NORD; i++) ord[i] = 0.01 * i;
    /* order O = cycles per revolution; angle step DTH -> w = 2 pi O DTH/360 */
    amp_spec(xa, na, ord, DTH / 360.0, ao, NORD);
    printf("b) angle domain: %d revolutions, %d samples (%.1f deg)\n", nrev, na, DTH);
    printf("   order   amplitude (angle domain)\n");
    for (k = 1; k <= 4; k++) printf("   %4d     %.4f V\n", 2 * k, ao[200 * k]);
    ap_plot(ao, NORD, "b) order spectrum, orders 0..10 (angle domain, run-up)");
    csv_write("order_spectrum.csv", "order,A", NORD, 2, ord, ao);
    csv_write("time_spectrum.csv", "f,A", NFT, 2, ft, at);
    return 0;
}
