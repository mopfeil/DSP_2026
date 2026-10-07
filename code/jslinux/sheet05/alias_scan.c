/*
 * Sheet 5, Exercise 5.2 -- Aliasing made visible: analysis of the
 * Wokwi recordings (student template -- complete the parts marked TODO)
 *
 * Build:   gcc -O2 -o alias_scan alias_scan.c -lm
 * Usage:   ./alias_scan uno.csv   5000  10     Wokwi CSV, fs = 5 kHz, 10 bit
 *          ./alias_scan esp.csv  25000  12     Wokwi CSV, fs = 25 kHz, 12 bit
 *          ./alias_scan sim      5000  10     virtual Wokwi: simulates the
 *                                              engine-sim chip (50 kHz ZOH
 *                                              output) and an ADC that samples
 *                                              it at fs (fs must divide 50 kHz)
 * The CSV must have the columns n,adc,knk,tdc (as printed by the sketches).
 *
 * For every knock burst (rising edge of the ground-truth KNK pin) and every
 * valve impact (90 deg after each TDC) a short segment is cut out, its mean
 * is removed and the "amplitude spectrum" |sum x[n] exp(-j 2 pi f n/fs)| is
 * evaluated on a frequency grid 0..fs/2 (this is the DTFT -- Sheet 6; here
 * it is used as a black-box frequency analyser). The averaged power spectra
 * of all bursts are searched for peaks and compared with the alias
 * frequencies predicted in Exercise 5.1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define MAXN   20000
#define NF     501               /* frequency grid points 0..fs/2       */
#define CHIP_FS 50000.0          /* update rate of the engine-sim chip  */

static double x[MAXN];
static int knk[MAXN], tdc[MAXN], blk[MAXN];   /* blk: block number */
static double pk[NF], pv[NF], fgrid[NF];

/* ---- (a) apparent (alias) frequency of a tone f sampled at fs --------- */
static double alias_freq(double f, double fs)
{
    /* TODO (a): fold f into [0, fs) and mirror into [0, fs/2]          */
    (void)fs;
    return f;
}

/* virtual Wokwi: engine-sim chip (ZOH at 50 kHz) + ADC sampling at fs */
static int simulate(double fs, int bits, double rpm, double dur)
{
    es_engine_t e;
    int D = (int)(CHIP_FS / fs + 0.5), n = 0, k, last_cyl;
    double vmax = bits == 10 ? 5.0 : 3.3, off = vmax / 2.0, v = off;
    double lsb_div = bits == 10 ? 1024.0 : 4095.0;
    int tdc_pending = 0, ktot = (int)(dur * CHIP_FS);
    if (fabs(D * fs - CHIP_FS) > 1e-6 * CHIP_FS)
        printf("warning: fs = %.0f Hz does not divide the chip rate 50 kHz\n", fs);
    es_init(&e, rpm, 1234u);             /* chip defaults: seed 1234     */
    e.knock_prob = 1.0;                  /* slider "knock prob" = 100 %  */
    last_cyl = e.last_cyl;
    for (k = 0; k < ktot && n < MAXN; k++) {
        es_advance(&e, 1.0 / CHIP_FS);   /* chip update every 20 us      */
        v = off + 0.6 * es_knock(&e);    /* KNOCK pin voltage (held)     */
        (void)es_ion(&e);                /* the chip also computes ION   */
        if (v < 0) v = 0;
        if (v > vmax) v = vmax;
        if (e.last_cyl != last_cyl) { last_cyl = e.last_cyl; tdc_pending = 1; }
        if (k % D == D - 1) {            /* ADC samples the held value   */
            double code = bits == 10 ? floor(v / vmax * lsb_div)
                                     : floor(v / vmax * lsb_div + 0.5);
            if (code > (1 << bits) - 1) code = (1 << bits) - 1;
            x[n] = code;
            blk[n] = 0;
            knk[n] = e.knock_t0 >= 0.0 && e.t - e.knock_t0 < 3.0 * ES_KNOCK_TAU;
            tdc[n] = tdc_pending; tdc_pending = 0;
            n++;
        }
    }
    return n;
}

static int load_csv(const char *path)
{
    csv_t d;
    int r, n = 0;
    if (csv_read(path, &d) != 0 || d.cols < 4) {
        fprintf(stderr, "cannot read %s (need columns n,adc,knk,tdc)\n", path);
        exit(1);
    }
    for (r = 0; r < d.rows && n < MAXN; r++, n++) {
        /* several pasted blocks: the sample counter restarts at 0      */
        blk[n] = n == 0 ? 0 : blk[n - 1] + (CSV_AT(&d, r, 0) != CSV_AT(&d, r - 1, 0) + 1);
        x[n] = CSV_AT(&d, r, 1);
        knk[n] = CSV_AT(&d, r, 2) > 0.5;
        tdc[n] = CSV_AT(&d, r, 3) > 0.5;
    }
    csv_free(&d);
    return n;
}

/* add |DTFT|^2 of the mean-free segment s[0..L-1] to acc[] */
static void add_spectrum(const double *s, int L, double fs, double *acc)
{
    double m = 0.0;
    int i, n;
    for (n = 0; n < L; n++) m += s[n];
    m /= L;
    for (i = 0; i < NF; i++) {
        double w = 2.0 * M_PI * fgrid[i] / fs, re = 0.0, im = 0.0;
        for (n = 0; n < L; n++) {
            re += (s[n] - m) * cos(w * n);
            im -= (s[n] - m) * sin(w * n);
        }
        acc[i] += re * re + im * im;
    }
}

/* print the (up to) 3 largest local maxima above 20 % of the maximum */
static void peaks(const double *p, const char *what)
{
    int i, j, best[3] = {-1, -1, -1};
    double mx = 0.0;
    for (i = 0; i < NF; i++) if (p[i] > mx) mx = p[i];
    for (i = 0; i < NF; i++) {
        int ismax = (i == 0 || p[i] >= p[i - 1]) && (i == NF - 1 || p[i] > p[i + 1]);
        if (!ismax || p[i] < 0.2 * mx) continue;
        for (j = 0; j < 3; j++)
            if (best[j] < 0 || p[i] > p[best[j]]) {
                int m;
                for (m = 2; m > j; m--) best[m] = best[m - 1];
                best[j] = i;
                break;
            }
    }
    printf("   %s spectral peaks:", what);
    for (j = 0; j < 3; j++)
        if (best[j] >= 0)
            printf("  %6.0f Hz (%3.0f %%)", fgrid[best[j]], 100.0 * p[best[j]] / mx);
    printf("\n");
}

int main(int argc, char **argv)
{
    double fs, rpm = 3000.0;
    int bits, N, n, i, nk = 0, nv = 0, Lk, Lv, last_tdc = -1;
    static const double ftrue[3] = {ES_KNOCK_F1, ES_VALVE_F, ES_KNOCK_F2};
    static const char *fname[3] = {"knock mode 1", "valve impact", "knock mode 2"};

    if (argc < 4) {
        printf("usage: %s <file.csv|sim> <fs_Hz> <adc_bits> [rpm (sim only)]\n", argv[0]);
        return 1;
    }
    fs = atof(argv[2]);
    bits = atoi(argv[3]);
    if (argc > 4) rpm = atof(argv[4]);
    if (strcmp(argv[1], "sim") == 0) {
        FILE *f = fopen("alias_sim.csv", "w");     /* same format as Wokwi */
        N = simulate(fs, bits, rpm, 0.4);
        if (f) {
            for (n = 0; n < N; n++) {
                if (n % 512 == 0) fprintf(f, "n,adc,knk,tdc\n");
                fprintf(f, "%d,%.0f,%d,%d\n", n % 512, x[n], knk[n], tdc[n]);
            }
            fclose(f);
            printf("simulated data also written to alias_sim.csv (blocks of 512)\n");
        }
    } else N = load_csv(argv[1]);
    printf("%d blocks, %d samples at fs = %.0f Hz (%.1f ms), Nyquist frequency %.0f Hz\n",
           blk[N - 1] + 1, N, fs, N / fs * 1e3, fs / 2);

    /* (a) predictions */
    printf("a) predicted apparent frequencies:\n");
    for (i = 0; i < 3; i++)
        printf("   %-13s %6.0f Hz  ->  %6.0f Hz\n", fname[i], ftrue[i],
               alias_freq(ftrue[i], fs));

    /* (b) measured spectra of knock and valve segments */
    for (i = 0; i < NF; i++) { fgrid[i] = fs / 2.0 * i / (NF - 1); pk[i] = pv[i] = 0.0; }
    Lk = (int)ceil(4.0 * ES_KNOCK_TAU * fs);          /* 4 tau = 3.2 ms */
    Lv = (int)ceil(5.0 * ES_VALVE_TAU * fs);          /* 5 tau = 2.0 ms */
    for (n = 1; n < N; n++) {
        if (blk[n] != blk[n - 1]) { last_tdc = -1; continue; }   /* new block */
        if (knk[n] && !knk[n - 1] && n + Lk <= N && blk[n + Lk - 1] == blk[n]) {
            add_spectrum(x + n, Lk, fs, pk); nk++;
        }
        if (tdc[n]) {
            if (last_tdc >= 0) {
                /* TODO (c): first sample of the valve burst = TDC + 90 deg;
                   use the number of samples between two TDCs (180 deg) */
                int nv0 = n;
                if (nv0 + Lv <= N && blk[nv0 + Lv - 1] == blk[n]) {
                    add_spectrum(x + nv0, Lv, fs, pv); nv++;
                }
            }
            last_tdc = n;
        }
    }
    printf("b) %d knock bursts (%d samples each), %d valve bursts (%d samples each)\n",
           nk, Lk, nv, Lv);
    if (nk) peaks(pk, "knock bursts:");
    if (nv) peaks(pv, "valve bursts:");
    if (nk) { ap_size(72, 14); ap_plot(pk, NF, "knock bursts: averaged |X(f)|^2 over index i, f = i*fs/1000"); }
    if (nv) ap_plot(pv, NF, "valve bursts: averaged |X(f)|^2 over index i, f = i*fs/1000");
    if (nk) {    /* time plot of the first knock burst, 4 x the window */
        for (n = 1; n < N && !(knk[n] && !knk[n - 1]); n++) { }
        if (n + 4 * Lk < N) ap_plot(x + n - Lk / 2, 4 * Lk, "first knock burst (ADC codes)");
    }
    csv_write("alias_spectra.csv", "f,P_knock,P_valve", NF, 3, fgrid, pk, pv);
    return 0;
}
