/*
 * Sheet 4, Exercise 4.4 -- Time domain vs. crank-angle domain
 * (REFERENCE SOLUTION)
 *
 * ESP32 used as a plain C computer: no circuit, the program runs once in
 * setup() and prints its results (and ASCII plots) to the Serial Monitor.
 * (engine_signals.h and asciiplot.h in the same folder)
 *
 * The virtual engine accelerates from 1500 to 4500 rpm within 0.5 s.
 *   - the ion-current signal is sampled in the time domain at fs = 50 kHz,
 *   - the rising edges of the 60-2 crank signal are time-stamped by an
 *     "input capture" timer with DT_CAP = 2 us resolution (as in a real
 *     ECU): the stamp is the first timer tick at or after the edge,
 *   - the cam level is latched at every crank edge.
 * The program
 *   a) assigns a crank angle to every crank edge (gap detection + cam),
 *   b) resamples the signal onto an equidistant crank-angle grid
 *      (two linear interpolations: angle -> time via the edges,
 *       time -> signal value via the samples),
 *   c) compares cycle-to-cycle similarity in both domains.
 *
 * RAM: the ESP32 has about 300 KB of heap. The samples are therefore
 * stored as 16-bit integers (0.1 mV per LSB), the resampled signal as
 * float, and the true angle (only used to check the grid) at every 5th
 * sample. The run takes a few seconds in the simulator.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"

#define FS        50000.0     /* time-domain sampling rate, Hz          */
#define T_REC     0.5         /* record length, s                       */
#define NS        25000       /* = FS * T_REC                           */
#define DT_CAP    2e-6        /* resolution of the edge time stamps, s  */
                              /* (part b: change to 20e-6 and rerun)    */
#define RPM0      1500.0      /* start speed                            */
#define RPM1      4500.0      /* end speed                              */
#define DTH       0.5         /* angle grid step, deg                   */
#define NA_CYC    1440        /* grid points per 720 deg cycle          */
#define MAXEDGE   2000
#define MAXA      18000       /* max. grid points                       */
#define MAXCYC    (MAXA / NA_CYC + 1)
#define X_LSB     1e-4        /* V per LSB of the stored samples        */
#define TDEC      5           /* true angle stored every TDEC samples   */

static int16_t *xs;           /* time samples x(n/FS), units of X_LSB   */
static double  *thr;          /* true unwrapped angle at n = 0, TDEC, ... */
static double  *te;           /* edge time stamps                       */
static char    *cam_e;        /* cam level at the edge                  */
static double  *the;          /* assigned (unwrapped) edge angle        */
static float   *xa;           /* signal on the angle grid               */
static double   ta_cyc[MAXCYC + 1];  /* time of every 720 deg grid point */

/* signal value (V) at time tq, linear between the two neighbouring
   samples at n/FS and (n+1)/FS                                        */
static double x_at(double tq)
{
    double u = tq * FS;
    int n = (int)floor(u);
    if (n < 0) n = 0;
    if (n > NS - 2) n = NS - 2;
    return X_LSB * (xs[n] + (xs[n + 1] - xs[n]) * (u - n));
}

/* true angle at time tq (ground truth, for checking the grid only) */
static double true_angle(double tq)
{
    double u = tq * FS / TDEC;
    int k = (int)floor(u);
    if (k > (NS - 1) / TDEC - 1) k = (NS - 1) / TDEC - 1;
    return thr[k] + (thr[k + 1] - thr[k]) * (u - k);
}

/* correlation coefficient of two sequences of length n */
static double corr_f(const float *a, const float *b, int n)
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
static double corr_s(const int16_t *a, const int16_t *b, int n)
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

/* min and max of every plot column (2 values per column), so that the
   plot of a long sequence needs only a short double array             */
#define PW 72
static void minmax_f(double *d, const float *s, int n)
{
    int c, i;
    for (c = 0; c < PW; c++) {
        int i0 = (int)((long)c * n / PW), i1 = (int)((long)(c + 1) * n / PW);
        double mn = s[i0], mx = s[i0];
        for (i = i0 + 1; i < i1; i++) { if (s[i] < mn) mn = s[i]; if (s[i] > mx) mx = s[i]; }
        d[2 * c] = mn; d[2 * c + 1] = mx;
    }
}
static void minmax_s(double *d, const int16_t *s, int n)
{
    int c, i;
    for (c = 0; c < PW; c++) {
        int i0 = (int)((long)c * n / PW), i1 = (int)((long)(c + 1) * n / PW);
        double mn = s[i0], mx = s[i0];
        for (i = i0 + 1; i < i1; i++) { if (s[i] < mn) mn = s[i]; if (s[i] > mx) mx = s[i]; }
        d[2 * c] = mn * X_LSB; d[2 * c + 1] = mx * X_LSB;
    }
}

/* two curves of 2*PW min/max values, x axis labelled x0 .. x1 */
static void plot2(const double *y1, const double *y2, double x0, double x1,
                  const char *title)
{
    static char cv[200 * 80];
    double lo1, hi1, lo2, hi2;
    ap_size(PW, 20);
    ap_range(y1, 2 * PW, &lo1, &hi1);
    ap_range(y2, 2 * PW, &lo2, &hi2);
    if (lo2 < lo1) lo1 = lo2;
    if (hi2 > hi1) hi1 = hi2;
    ap_clear(cv, lo1, hi1);
    ap_curve(cv, y1, 2 * PW, lo1, hi1, '*');
    ap_curve(cv, y2, 2 * PW, lo1, hi1, 'o');
    ap_print(cv, lo1, hi1, x0, x1, title);
}

static void run(void)
{
    es_engine_t e;
    int n, k, ne = 0, kref = -1, na, c, ncyc, L;
    double unwrap = 0.0, dt = 1.0 / FS;

    xs    = (int16_t *)malloc(NS * sizeof(int16_t));
    thr   = (double *)malloc(((NS - 1) / TDEC + 1) * sizeof(double));
    te    = (double *)malloc(MAXEDGE * sizeof(double));
    the   = (double *)malloc(MAXEDGE * sizeof(double));
    cam_e = (char *)malloc(MAXEDGE);
    xa    = (float *)malloc(MAXA * sizeof(float));
    if (!xs || !thr || !te || !the || !cam_e || !xa) {
        printf("out of memory\n");
        return;
    }

    /* ------------- simulation (given) -------------------------------- */
    es_init(&e, RPM0, 7u);
    e.knock_prob = 0.0;                 /* no knock: deterministic shape  */
    for (n = 0; n < NS; n++) {
        double t = n * dt, th0, th1;
        long m;
        xs[n] = (int16_t)lround(es_ion(&e) / X_LSB);   /* sample at n/FS */
        if (n % TDEC == 0) thr[n / TDEC] = unwrap + e.theta;  /* truth   */
        /* advance to the next sample instant; the speed is constant
           within one step, so the angle grows linearly                  */
        e.rpm = RPM0 + (RPM1 - RPM0) * t / T_REC;           /* speed ramp */
        th0 = unwrap + e.theta;
        es_advance(&e, dt);
        if (unwrap + e.theta < th0) unwrap += 720.0;
        th1 = unwrap + e.theta;
        /* rising crank edges at 6 deg * m (teeth 0..57 of every turn)
           in (th0, th1]: exact time, then the input-capture time stamp  */
        for (m = (long)floor(th0 / 6.0) + 1; 6.0 * m <= th1; m++) {
            double tedge = t + dt * (6.0 * m - th0) / (th1 - th0);
            if (m % 60 >= 58 || ne >= MAXEDGE) continue;
            te[ne]    = ceil(tedge / DT_CAP - 1e-9) * DT_CAP;
            cam_e[ne] = fmod(6.0 * m, 720.0) < 180.0;   /* es_cam() level */
            ne++;
        }
    }
    printf("simulated %.1f s, %d samples, %d crank edges, %.0f -> %.0f rpm, "
           "DT_CAP = %.0f us\n", T_REC, NS, ne, RPM0, RPM1, DT_CAP * 1e6);

    /* ------------- a) angle of every crank edge ---------------------- */
    /* gap: edge-to-edge time > 2 x previous one (nominal ratio 3).
       The first edge after the gap is tooth 0 (0 or 360 deg, cam decides). */
    for (k = 2; k < ne; k++) {
        double d1 = te[k] - te[k - 1], d0 = te[k - 1] - te[k - 2];
        if (kref < 0) {
            if (d1 > 2.0 * d0) {                       /* first reference */
                kref = k;
                the[k] = cam_e[k] ? 0.0 : 360.0;
            }
        } else {
            the[k] = the[k - 1] + (d1 > 2.0 * d0 ? 18.0 : 6.0);
            if (d1 > 2.0 * d0 && fmod(the[k], 360.0) != 0.0)
                printf("warning: gap at unexpected angle %.0f\n", the[k]);
        }
    }
    printf("a) first reference edge: #%d at t = %.3f ms, angle %.0f deg, "
           "last edge angle %.0f deg\n", kref, te[kref] * 1e3, the[kref], the[ne - 1]);

    /* ------------- b) resampling onto the angle grid ----------------- */
    {
        int ke = kref;
        double th0 = the[kref], emax = 0.0, erms = 0.0;
        na = 0;
        while (na < MAXA) {
            double th = th0 + na * DTH, tq, err;
            if (th > the[ne - 1]) break;
            /* angle -> time: linear between the two neighbouring edges */
            while (ke < ne - 2 && the[ke + 1] < th) ke++;
            tq = te[ke] + (te[ke + 1] - te[ke]) * (th - the[ke]) / (the[ke + 1] - the[ke]);
            if (tq * FS > NS - 1) break;
            /* time -> value: linear between two signal samples */
            xa[na] = (float)x_at(tq);
            if (na % NA_CYC == 0) ta_cyc[na / NA_CYC] = tq;
            /* angle error of the grid point: true angle at tq - th,
               modulo 720 deg (the cam fixes the phase in the cycle)     */
            err = true_angle(tq) - th;
            err -= 720.0 * floor(err / 720.0 + 0.5);
            if (fabs(err) > emax) emax = fabs(err);
            erms += err * err;
            na++;
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
        static double p1[2 * PW], p2[2 * PW];
        int nlast = 0, n0 = (int)(ta_cyc[0] * FS + 0.5);
        L = (int)((ta_cyc[1] - ta_cyc[0]) * FS + 0.5);
        printf("c) %d complete cycles in the angle domain; first cycle lasts %d samples (%.2f ms)\n",
               ncyc, L, L / FS * 1e3);
        printf("   cycle   rpm(mean)  samples/cycle  corr(time domain)  corr(angle domain)\n");
        for (c = 0; c < ncyc; c++) {
            int nc = (int)(ta_cyc[c] * FS + 0.5);
            double dur = ta_cyc[c + 1] - ta_cyc[c];
            double rt, ra = corr_f(xa, xa + c * NA_CYC, NA_CYC);
            if (nc + L > NS) break;          /* time segment would not fit */
            rt = corr_s(xs + n0, xs + nc, L);
            nlast = nc;
            printf("   %3d     %6.0f      %6.0f         %7.3f            %7.4f\n",
                   c + 1, 120.0 / dur, dur * FS, rt, ra);
        }
        /* plots: first and last complete cycle in both domains */
        minmax_s(p1, xs + n0, L);
        minmax_s(p2, xs + nlast, L);
        plot2(p1, p2, 0, L - 1,
              "time domain: cycle 1 (*) and last fitting cycle (o), samples");
        minmax_f(p1, xa, NA_CYC);
        minmax_f(p2, xa + (ncyc - 1) * NA_CYC, NA_CYC);
        plot2(p1, p2, 0, 720,
              "angle domain: cycle 1 (*) and last cycle (o), deg");
    }
    free(xs); free(thr); free(te); free(the); free(cam_e); free(xa);
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    run();
    printf("\ndone.\n");
}

void loop() {}
