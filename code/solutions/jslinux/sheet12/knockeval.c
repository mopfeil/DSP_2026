/*
 * Sheet 12 -- Exercise 12.2: offline evaluation of knock detectors
 * (reference solution)
 *
 * Build and run:   gcc -O2 -o knockeval knockeval.c -lm
 *                  ./knockeval [knock_amp] [ncomb]   (default 0.2 V, 1000)
 * (engine_signals.h, asciiplot.h, csvio.h, knock_filter.h, ar.h, fft.h in
 *  the same directory; knock_filter.h is the header of Sheet 10)
 *
 * The virtual engine is run at 1500, 3000, 6000 rpm with sensor noise
 * 0.05 V and 0.15 V (6 scenarios, ncomb combustions each, knock
 * probability 0.2, knock amplitude knock_amp * (0.5..1.5), fs = 25 kHz).
 * For every combustion the following features are computed; the ground
 * truth is e.knock_flag[] of the cylinder:
 *   0  WB-seg   wide-band energy sum x^2 over the whole 180 deg segment
 *   1  WB-win   wide-band energy in the window 10..70 deg ATDC
 *   2  BP-seg   band-pass energy (biquad cascade) over the whole segment
 *   3  BP-win   band-pass energy (cascade), window shifted by the filter delay
 *   4  BP2-win  band-pass energy (single biquad B = 400 Hz), shifted window
 *   5  FFT      power 6..7 kHz of the Hann-windowed raw window samples
 *   6  FFT-rect the same without window (rectangular)
 *   7  MEM      power 6..7 kHz of the Burg AR(8) spectrum of the window
 * Every feature is normalised by a per-cylinder background level (EWMA
 * over previous combustions). Output: AUC and detection rate at 1 %
 * false alarms per scenario and pooled, ROC curves -> roc.csv, all
 * features -> features.csv.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"
#include "knock_filter.h"
#include "ar.h"

#define FS       25000.0
#define NFEAT    8
#define NSCEN    6
#define MAXCOMB  2000
#define MAXW     200        /* window samples (60 deg at 1500 rpm = 167)  */
#define WIN0     10.0       /* window start, deg ATDC                      */
#define WIN1     70.0       /* window end                                  */
#define D_SOS    22         /* group delay of the cascade at 6.5 kHz       */
#define D_BP2    20         /* group delay of the single biquad            */
#define LAMBDA   0.05       /* background EWMA weight                      */
#define LAMBDA_W 0.25       /* ... during the warm-up                      */
#define WARMUP   16         /* combustions per cylinder before evaluation  */
#define NFFT_F   256
#define P_MEM    8
#define NGRID    101        /* FPR grid of the ROC output                  */

static const char *fname[NFEAT] = {"WB-seg", "WB-win", "BP-seg", "BP-win",
                                   "BP2-win", "FFT", "FFT-rect", "MEM"};
static const double rpms[3] = {1500.0, 3000.0, 6000.0};
static const double noises[2] = {0.05, 0.15};

/* all evaluated combustions */
typedef struct { float f[NFEAT], raw3; char label, scen, cyl; } comb_t;
static comb_t *cb;
static int ncb = 0;

/* ---------------- filters (double precision, TDF II) ---------------- */
typedef struct { double s1[KF_NSEC], s2[KF_NSEC]; } sos_t;
static double sos_step(sos_t *f, double x)
{
    int k;
    for (k = 0; k < KF_NSEC; k++) {
        const float *c = kf_sos[k];
        double y = c[0] * x + f->s1[k];
        f->s1[k] = c[1] * x - c[3] * y + f->s2[k];
        f->s2[k] = c[2] * x - c[4] * y;
        x = y;
    }
    return x;
}
typedef struct { double s1, s2; } bq_t;
static double bp2_step(bq_t *f, double x)
{
    double y = kf_bp2[0] * x + f->s1;
    f->s1 = kf_bp2[1] * x - kf_bp2[3] * y + f->s2;
    f->s2 = kf_bp2[2] * x - kf_bp2[4] * y;
    return y;
}

/* ---------------- spectral features of the raw window ---------------- */
static double feat_fft(const double *x, int n, int hann)
{
    static double re[NFFT_F], im[NFFT_F];
    double mean = 0.0, u = 0.0, p = 0.0;
    int i, k1 = (int)ceil(6000.0 / FS * NFFT_F), k2 = (int)floor(7000.0 / FS * NFFT_F);
    for (i = 0; i < n; i++) mean += x[i] / n;
    for (i = 0; i < NFFT_F; i++) {
        double w = i >= n ? 0.0 : (hann ? 0.5 - 0.5 * cos(2.0 * M_PI * (i + 0.5) / n) : 1.0);
        re[i] = i < n ? (x[i] - mean) * w : 0.0;
        im[i] = 0.0;
        u += w * w;
    }
    fft(re, im, NFFT_F, 0);
    for (i = k1; i <= k2; i++) p += re[i] * re[i] + im[i] * im[i];
    return p / u;
}

static double feat_mem(const double *x, int n)
{
    static double y[MAXW];
    double a[P_MEM + 1], k[P_MEM + 1], E[P_MEM + 1], mean = 0.0, pb = 0.0, f;
    int i;
    for (i = 0; i < n; i++) mean += x[i] / n;
    for (i = 0; i < n; i++) y[i] = x[i] - mean;
    if (ar_burg(y, n, P_MEM, a, k, E) != 0) return 0.0;
    /* power in the knock band = integral of the MEM spectrum 6..7 kHz
       (bounded by the signal power; the peak value itself is heavy-tailed) */
    for (f = 6012.5; f < 7000.0; f += 25.0) pb += ar_psd(a, P_MEM, E[P_MEM], f, FS) * 25.0;
    return pb;
}

/* ---------------- one scenario --------------------------------------- */
static void run_scenario(int s, double rpm, double noise, double kamp, int ncomb,
                         unsigned seed)
{
    es_engine_t e;
    sos_t sos;
    bq_t bq;
    double xw[MAXW], dh[32];
    double acc[NFEAT], bg[ES_NCYL][NFEAT];
    int nbg[ES_NCYL], cur = -1, first = 1, label = 0, nw = 0, n = 0, done = 0, j;

    memset(&sos, 0, sizeof sos);
    memset(&bq, 0, sizeof bq);
    memset(dh, 0, sizeof dh);
    memset(nbg, 0, sizeof nbg);
    es_init(&e, rpm, seed);
    e.knock_prob = 0.2;
    e.knock_amp = kamp;
    e.noise_amp = noise;
    for (j = 0; j < NFEAT; j++) acc[j] = 0.0;

    while (done < ncomb) {
        double x, y, y2, d, dS, dB;
        int lc;
        es_advance(&e, 1.0 / FS);
        x = es_knock(&e);
        y = sos_step(&sos, x);
        y2 = bp2_step(&bq, x);
        lc = e.last_cyl;
        if (lc != cur) {                       /* new combustion TDC   */
            if (cur >= 0 && !first) {          /* finish the last one  */
                double f[NFEAT];
                f[0] = acc[0]; f[1] = acc[1]; f[2] = acc[2]; f[3] = acc[3]; f[4] = acc[4];
                f[5] = feat_fft(xw, nw, 1);
                f[6] = feat_fft(xw, nw, 0);
                f[7] = feat_mem(xw, nw);
                if (nbg[cur] == 0) for (j = 0; j < NFEAT; j++) bg[cur][j] = f[j];
                if (nbg[cur] >= WARMUP && ncb < NSCEN * MAXCOMB) {
                    comb_t *q = &cb[ncb++];
                    for (j = 0; j < NFEAT; j++) q->f[j] = (float)(f[j] / bg[cur][j]);
                    q->raw3 = (float)f[3];
                    q->label = (char)label; q->scen = (char)s; q->cyl = (char)cur;
                    done++;
                }
                /* background update: knock-robust EWMA (values clipped at
                   3 x background), faster adaptation during the warm-up */
                for (j = 0; j < NFEAT; j++) {
                    double v = f[j] < 3.0 * bg[cur][j] ? f[j] : 3.0 * bg[cur][j];
                    bg[cur][j] += (nbg[cur] < WARMUP ? LAMBDA_W : LAMBDA) * (v - bg[cur][j]);
                }
                nbg[cur]++;
            }
            first = (cur < 0);                 /* the first segment is incomplete */
            cur = lc;
            label = e.knock_flag[cur];
            nw = 0;
            for (j = 0; j < NFEAT; j++) acc[j] = 0.0;
        }
        d = es_angle_diff(e.theta, es_tdc_deg[cur]);     /* deg ATDC */
        dh[n & 31] = d;
        dS = dh[(n - D_SOS) & 31];                        /* angle D samples ago */
        dB = dh[(n - D_BP2) & 31];
        n++;
        acc[0] += x * x;
        acc[2] += y * y;
        if (d >= WIN0 && d < WIN1) {
            acc[1] += x * x;
            if (nw < MAXW) xw[nw++] = x;
        }
        if (dS >= WIN0 && dS < WIN1) acc[3] += y * y;
        if (dB >= WIN0 && dB < WIN1) acc[4] += y2 * y2;
    }
}

/* ---------------- ROC ----------------------------------------------- */
typedef struct { float v; char l; } vl_t;
static int cmp_desc(const void *a, const void *b)
{
    float x = ((const vl_t *)a)->v, y = ((const vl_t *)b)->v;
    return (x < y) - (x > y);
}

/* ROC of feature j over the combustions with scen == s (s < 0: all);
   raw = 1 uses the unnormalised BP-win energy. Returns AUC, the
   detection rate at <= 1 % false alarms and the threshold there;
   tpr_grid[NGRID] = TPR at FPR = 0, 0.01, ..., 1 (optional) */
static double roc(int j, int s, int raw, double *tpr1, double *thr1, double *tpr_grid)
{
    static vl_t v[NSCEN * MAXCOMB];
    int i, m = 0, P = 0, Nn = 0, tp = 0, fp = 0, g;
    double auc = 0.0, last_fpr = 0.0, last_tpr = 0.0;
    for (i = 0; i < ncb; i++)
        if (s < 0 || cb[i].scen == s) {
            v[m].v = raw ? cb[i].raw3 : cb[i].f[j];
            v[m].l = cb[i].label;
            if (v[m].l) P++; else Nn++;
            m++;
        }
    qsort(v, m, sizeof v[0], cmp_desc);
    *tpr1 = 0.0; *thr1 = v[0].v;
    if (tpr_grid) for (g = 0; g < NGRID; g++) tpr_grid[g] = 0.0;
    for (i = 0; i < m; i++) {
        double fpr, tpr;
        if (v[i].l) tp++; else fp++;
        if (i < m - 1 && v[i + 1].v == v[i].v) continue;   /* ties */
        fpr = (double)fp / Nn; tpr = (double)tp / P;
        auc += 0.5 * (tpr + last_tpr) * (fpr - last_fpr);
        if (fpr <= 0.01) { *tpr1 = tpr; *thr1 = v[i].v; }
        if (tpr_grid)
            for (g = 0; g < NGRID; g++) if (fpr <= g / (NGRID - 1.0) && tpr > tpr_grid[g]) tpr_grid[g] = tpr;
        last_fpr = fpr; last_tpr = tpr;
    }
    return auc;
}

int main(int argc, char **argv)
{
    double kamp = argc > 1 ? atof(argv[1]) : 0.2;
    int ncomb = argc > 2 ? atoi(argv[2]) : 1000;
    static double grid[NFEAT + 1][NGRID];
    double tpr1, thr1, auc;
    int s, j, i, nk = 0;

    if (ncomb > MAXCOMB) ncomb = MAXCOMB;
    cb = (comb_t *)malloc(sizeof(comb_t) * NSCEN * MAXCOMB);
    for (s = 0; s < NSCEN; s++)
        run_scenario(s, rpms[s / 2], noises[s % 2], kamp, ncomb, 1000u + 17u * s);
    for (i = 0; i < ncb; i++) nk += cb[i].label;
    printf("knock amplitude %.3f V x (0.5..1.5), knock probability 0.2, fs = %.0f Hz\n", kamp, FS);
    printf("%d combustions evaluated, %d with knock (%.1f %%)\n\n", ncb, nk, 100.0 * nk / ncb);

    for (i = 0; i < 2; i++) {          /* i = 0: AUC, i = 1: detection rate */
        printf(i == 0 ? "area under the ROC curve (AUC)\n"
                      : "\ndetection rate (%%) at <= 1 %% false alarms\n");
        printf("rpm  noise |");
        for (j = 0; j < NFEAT; j++) printf(" %8s", fname[j]);
        printf("\n");
        for (s = 0; s <= NSCEN; s++) {
            if (s < NSCEN) printf("%4.0f %5.2f |", rpms[s / 2], noises[s % 2]);
            else printf("all pooled |");
            for (j = 0; j < NFEAT; j++) {
                auc = roc(j, s < NSCEN ? s : -1, 0, &tpr1, &thr1, s < NSCEN ? NULL : grid[j]);
                if (i == 0) printf(" %8.3f", auc); else printf(" %8.1f", 100.0 * tpr1);
            }
            printf("\n");
        }
    }
    roc(3, -1, 0, &tpr1, &thr1, NULL);
    printf("\npooled, BP-win normalised: threshold at 1 %% false alarms R > %.2f, detection %.1f %%\n",
           thr1, 100.0 * tpr1);
    auc = roc(3, -1, 1, &tpr1, &thr1, grid[NFEAT]);
    printf("pooled, BP-win without normalisation: AUC %.3f, detection at 1 %% false alarms %.1f %%\n",
           auc, 100.0 * tpr1);

    /* thresholds per rpm for the normalised BP-win feature */
    printf("\nBP-win, threshold for 1 %% false alarms per scenario (noise-only quantile):\n");
    for (s = 0; s < NSCEN; s++) {
        roc(3, s, 0, &tpr1, &thr1, NULL);
        printf("  %4.0f rpm, noise %.2f V: R > %.2f  (detection %.1f %%)\n",
               rpms[s / 2], noises[s % 2], thr1, 100.0 * tpr1);
    }

    /* outputs */
    {
        static double fpr[NGRID];
        for (i = 0; i < NGRID; i++) fpr[i] = i / (NGRID - 1.0);
        csv_write("roc.csv", "fpr,wb_seg,wb_win,bp_seg,bp_win,bp2_win,fft,fft_rect,mem,bp_win_raw",
                  NGRID, 10, fpr, grid[0], grid[1], grid[2], grid[3], grid[4], grid[5], grid[6],
                  grid[7], grid[8]);
        ap_plot2(grid[1], grid[3], NGRID,
                 "pooled ROC, TPR over FPR = 0..1: '*' WB-win, 'o' BP-win");
    }
    {
        FILE *f = fopen("features.csv", "w");
        if (f) {
            fprintf(f, "scen,cyl,label,wb_seg,wb_win,bp_seg,bp_win,bp2_win,fft,fft_rect,mem\n");
            for (i = 0; i < ncb; i++) {
                fprintf(f, "%d,%d,%d", cb[i].scen, cb[i].cyl, cb[i].label);
                for (j = 0; j < NFEAT; j++) fprintf(f, ",%.4g", cb[i].f[j]);
                fprintf(f, "\n");
            }
            fclose(f);
        }
    }
    printf("wrote roc.csv, features.csv\n");
    free(cb);
    return 0;
}
