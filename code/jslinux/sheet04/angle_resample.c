/*
 * Sheet 4, Exercise 4.4 -- Time domain vs. crank-angle domain
 * (student template -- complete the parts marked TODO)
 *
 * Build and run:   gcc -O2 -o angle_resample angle_resample.c -lm
 *                  ./angle_resample
 * (engine_signals.h, asciiplot.h, csvio.h in the same directory)
 *
 * The virtual engine accelerates from 1500 to 4500 rpm within 1 s.
 *   - the ion-current signal is sampled in the time domain at fs = 50 kHz,
 *   - the rising edges of the 60-2 crank signal are time-stamped by an
 *     "input capture" timer with 2 us resolution (as in a real ECU),
 *   - the cam level is latched at every crank edge.
 * The program
 *   a) assigns a crank angle to every crank edge (gap detection + cam),
 *   b) resamples the signal onto an equidistant crank-angle grid
 *      (two linear interpolations: angle -> time via the edges,
 *       time -> signal value via the samples),
 *   c) compares cycle-to-cycle similarity in both domains.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS        50000.0     /* time-domain sampling rate, Hz          */
#define T_REC     1.0         /* record length, s                       */
#define NS        50000       /* = FS * T_REC                           */
#ifndef DT_CAP
#define DT_CAP    2e-6        /* resolution of the edge time stamps, s  */
#endif                        /* (change with gcc -DDT_CAP=20e-6 ...)   */
#define RPM0      1500.0      /* start speed                            */
#define RPM1      4500.0      /* end speed                              */
#define DTH       0.5         /* angle grid step, deg                   */
#define NA_CYC    1440        /* grid points per 720 deg cycle          */
#define MAXEDGE   4000
#define MAXA      40000

static double ts[NS], xs[NS], ths[NS];      /* time samples, signal, true angle */
static double te[MAXEDGE];                  /* edge time stamps                 */
static int    cam_e[MAXEDGE];               /* cam level at the edge            */
static double the[MAXEDGE];                 /* assigned (unwrapped) edge angle  */
static double tha[MAXA], ta[MAXA], xa[MAXA], erra[MAXA];

/* linear interpolation of y(x) at xq, x strictly increasing,
   *k is a search hint (monotone queries) */
static double interp(const double *x, const double *y, int n, double xq, int *k)
{
    while (*k < n - 2 && x[*k + 1] < xq) (*k)++;
    return y[*k] + (y[*k + 1] - y[*k]) * (xq - x[*k]) / (x[*k + 1] - x[*k]);
}

/* correlation coefficient of two sequences of length n */
static double corr(const double *a, const double *b, int n)
{
    double ma = 0, mb = 0, sab = 0, saa = 0, sbb = 0;
    int i;
    for (i = 0; i < n; i++) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    for (i = 0; i < n; i++) {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    return sab / sqrt(saa * sbb);
}

int main(void)
{
    es_engine_t e;
    int n, k, ne = 0, sub, nsub = (int)(1.0 / FS / DT_CAP + 0.5);
    int crank_old, kref = -1, na, c, ncyc, L, j;
    double t = 0.0, unwrap = 0.0, th_old;

    /* ------------- simulation (given) -------------------------------- */
    es_init(&e, RPM0, 7u);
    e.knock_prob = 0.0;                 /* no knock: deterministic shape  */
    crank_old = es_crank(&e);
    th_old = e.theta;
    for (n = 0; n < NS; n++) {
        for (sub = 0; sub < nsub; sub++) {          /* 2 us capture ticks */
            e.rpm = RPM0 + (RPM1 - RPM0) * t / T_REC;   /* speed ramp      */
            es_advance(&e, DT_CAP);
            t += DT_CAP;
            if (e.theta < th_old) unwrap += 720.0;
            th_old = e.theta;
            if (es_crank(&e) && !crank_old && ne < MAXEDGE) {  /* rising edge */
                te[ne] = t;  cam_e[ne] = es_cam(&e);  ne++;
            }
            crank_old = es_crank(&e);
        }
        ts[n]  = t;                     /* sample instant                  */
        xs[n]  = es_ion(&e);            /* time-domain sample              */
        ths[n] = unwrap + e.theta;      /* ground truth (for checking only) */
    }
    printf("simulated %.1f s, %d samples, %d crank edges, %.0f -> %.0f rpm\n",
           T_REC, NS, ne, RPM0, RPM1);

    /* ------------- a) angle of every crank edge ---------------------- */
    /* gap: edge-to-edge time > 2 x previous one (nominal ratio 3).
       The first edge after the gap is tooth 0 (0 or 360 deg, cam decides). */
    /* TODO (a): for k = 2 .. ne-1 compute the edge-to-edge times
         d1 = te[k] - te[k-1], d0 = te[k-1] - te[k-2].
       - before the first gap: wait for d1 > 2*d0; this edge (tooth 0) gets
         the angle 0 deg if cam_e[k] == 1, else 360 deg; store kref = k
       - afterwards: the[k] = the[k-1] + 6 deg (normal tooth) or
         + 18 deg (gap, d1 > 2*d0)                                       */
    (void)k;
    if (kref < 0) {
        printf("a) TODO: no reference edge found -- complete part (a)\n");
        return 0;
    }
    printf("a) first reference edge: #%d at t = %.3f ms, angle %.0f deg, "
           "last edge angle %.0f deg\n", kref, te[kref] * 1e3, the[kref], the[ne - 1]);

    /* ------------- b) resampling onto the angle grid ----------------- */
    {
        int ke = kref, ks = 0;
        double th0 = the[kref];
        na = 0;
        while (na < MAXA) {
            double th = th0 + na * DTH, tq;
            if (th > the[ne - 1]) break;
            /* TODO (b1): angle -> time. Advance ke until the[ke+1] >= th,
               then interpolate linearly between (the[ke], te[ke]) and
               (the[ke+1], te[ke+1]).                                    */
            tq = te[kref];
            (void)ke;
            if (tq > ts[NS - 1]) break;
            /* TODO (b2): time -> value, linear between two signal samples
               (use the helper interp(ts, xs, NS, tq, &ks))              */
            tha[na] = th;
            ta[na]  = tq + na * 1e-5;        /* placeholder, remove        */
            xa[na]  = 0.0;
            (void)ks;
            na++;
        }
    }
    /* angle error of the grid: true angle at the interpolated instants */
    {
        int ks = 0;
        double emax = 0.0, erms = 0.0;
        for (j = 0; j < na; j++) {
            double tr = interp(ts, ths, NS, ta[j], &ks);
            /* compare modulo 720 deg (the cam fixes the phase in the cycle) */
            erra[j] = tr - tha[j];
            erra[j] -= 720.0 * floor(erra[j] / 720.0 + 0.5);
            if (fabs(erra[j]) > emax) emax = fabs(erra[j]);
            erms += erra[j] * erra[j];
        }
        printf("b) %d grid points (%.1f deg step), angle error of the grid: "
               "max %.4f deg, rms %.4f deg\n", na, DTH, emax, sqrt(erms / na));
    }

    /* ------------- c) cycle-to-cycle similarity ---------------------- */
    ncyc = (na - 1) / NA_CYC;          /* complete cycles on the grid */
    /* time domain: segments of fixed length L (= first cycle, in samples)
       that start exactly at the beginning of each cycle (perfect sync!).
       The only difference between the segments is the engine speed.     */
    {
        int nlast = 0;
        L = (int)((ta[NA_CYC] - ta[0]) * FS + 0.5);
        printf("c) %d complete cycles in the angle domain; first cycle lasts %d samples (%.2f ms)\n",
               ncyc, L, L / FS * 1e3);
        printf("   cycle   rpm(mean)  samples/cycle  corr(time domain)  corr(angle domain)\n");
        for (c = 0; c < ncyc; c++) {
            int nc = (int)(ta[c * NA_CYC] * FS + 0.5), n0 = (int)(ta[0] * FS + 0.5);
            double dur = ta[(c + 1) * NA_CYC] - ta[c * NA_CYC];
            double rt, ra = corr(xa, xa + c * NA_CYC, NA_CYC);
            if (nc + L > NS) break;          /* time segment would not fit */
            rt = corr(xs + n0, xs + nc, L);
            nlast = nc;
            if (c < 4 || c % 4 == 0 ||
                (int)(ta[(c + 1) * NA_CYC] * FS + 0.5) + L > NS)
                printf("   %3d     %6.0f      %6.0f         %7.3f            %7.4f\n",
                       c + 1, 120.0 / dur, dur * FS, rt, ra);
        }
        /* plots: first and last complete cycle in both domains */
        ap_plot2(xs + (int)(ta[0] * FS + 0.5), xs + nlast, L,
                 "time domain: cycle 1 (*) and last fitting cycle (o), same no. of samples");
        ap_plot2(xa, xa + (ncyc - 1) * NA_CYC, NA_CYC,
                 "angle domain: cycle 1 (*) and last cycle (o), 0..720 deg");
    }
    csv_write("angle_domain.csv", "theta,t,x,angle_err", na, 4, tha, ta, xa, erra);
    printf("wrote angle_domain.csv (%d rows)\n", na);
    return 0;
}
