/*
 * Sheet 8, Exercise 8.3 -- Windows, leakage, zero padding, knock spectrum
 *                          (REFERENCE SOLUTION)
 *
 * Build and run:   gcc -O2 -o windows windows.c -lm
 *                  ./windows
 * (fft.h from Exercise 8.2, engine_signals.h, asciiplot.h, csvio.h in the
 *  same directory)
 *
 *   (a) window functions and their figures of merit
 *   (b) leakage: strong 6.5 kHz tone (not bin-centred) + weak 8 kHz tone
 *   (c) zero padding vs. true resolution (two close tones)
 *   (d) spectrum of the virtual knock sensor in a window after TDC
 */
#include <stdio.h>
#include <math.h>
#include "fft.h"
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS    40000.0
#define N     256
#define NPAD  4096

enum { RECT, HANN, HAMMING, BLACKMAN, NWIN };
static const char *wname[NWIN] = {"rectangular", "Hann", "Hamming", "Blackman"};

/* ------------------------------------------------------------- (a) */
/* periodic ("DFT-even") windows of length n */
static void make_window(int type, double *w, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        double c1 = cos(2.0 * M_PI * i / n), c2 = cos(4.0 * M_PI * i / n);
        switch (type) {
        case HANN:     w[i] = 0.5 - 0.5 * c1; break;
        case HAMMING:  w[i] = 0.54 - 0.46 * c1; break;
        case BLACKMAN: w[i] = 0.42 - 0.5 * c1 + 0.08 * c2; break;
        default:       w[i] = 1.0;
        }
    }
}

/* amplitude spectrum in dB of the windowed signal, zero padded to nfft.
   Scaled such that a sinusoid of amplitude A at a bin centre gives
   20 log10(A): |X[k]| * 2 / sum(w). Returns bins 0..nfft/2 in db[] */
static void spectrum_db(const double *x, const double *w, int n, int nfft, double *db)
{
    static double re[NPAD], im[NPAD];
    double sw = 0.0;
    int i;
    for (i = 0; i < nfft; i++) {
        re[i] = i < n ? x[i] * w[i] : 0.0;
        im[i] = 0.0;
        if (i < n) sw += w[i];
    }
    fft(re, im, nfft, 0);
    for (i = 0; i <= nfft / 2; i++) {
        double a = 2.0 * sqrt(re[i] * re[i] + im[i] * im[i]) / sw;
        db[i] = 20.0 * log10(a + 1e-12);
    }
}

/* local maxima at most rel dB below the highest one, refined by parabolic interpolation of
   the dB values; prints frequency and level */
static void print_peaks(const double *db, int nb, int nfft, double rel)
{
    int k;
    double thr = db[0];
    for (k = 1; k < nb; k++) if (db[k] > thr) thr = db[k];
    thr -= rel;                                  /* relative threshold */
    for (k = 2; k < nb - 2; k++) {
        if (db[k] > thr && db[k] >= db[k - 1] && db[k] > db[k + 1] &&
            db[k] >= db[k - 2] && db[k] > db[k + 2]) {
            double a = db[k - 1], b = db[k], c = db[k + 1];
            double d = 0.5 * (a - c) / (a - 2 * b + c);   /* offset in bins */
            printf("    peak at %8.1f Hz  (bin %3d%+.2f)  %6.1f dB\n",
                   (k + d) * FS / nfft, k, d, b - 0.25 * (a - c) * d);
        }
    }
}

int main(void)
{
    static double x[N], w[N], db[NPAD / 2 + 1], fax[NPAD / 2 + 1];
    static double dbw[NWIN][N / 2 + 1];
    int t, n, k;

    /* ---------------------------------------------- (a) figures of merit */
    printf("(a) window      coh.gain  ENBW[bins]  scalloping[dB]  highest sidelobe[dB]\n");
    for (t = 0; t < NWIN; t++) {
        double s1 = 0, s2 = 0, sl, side = -200, main0;
        make_window(t, w, N);
        for (n = 0; n < N; n++) { s1 += w[n]; s2 += w[n] * w[n]; }
        /* scalloping loss: tone exactly between two bins */
        for (n = 0; n < N; n++) x[n] = cos(2 * M_PI * 40.5 * n / N);
        spectrum_db(x, w, N, N, db);
        sl = db[40] > db[41] ? db[40] : db[41];
        /* sidelobes: window spectrum zero padded 16x, beyond the main lobe */
        {   /* spectrum of the window itself: signal = w, "window" = 1 */
            static double one[N];
            for (n = 0; n < N; n++) one[n] = 1.0;
            spectrum_db(w, one, N, NPAD, db);
        }
        main0 = db[0];
        for (k = 1; k < NPAD / 2 && db[k] < db[k - 1]; k++) ;  /* first null */
        for (; k < NPAD / 2; k++) if (db[k] - main0 > side) side = db[k] - main0;
        printf("    %-11s  %7.3f   %8.3f   %10.2f      %10.1f\n", wname[t], s1 / N,
               N * s2 / (s1 * s1), sl, side);
    }

    /* ---------------------------------------------- (b) leakage */
    printf("\n(b) x = sin(2 pi 6500 t) + 0.001 sin(2 pi 8000 t), fs = %.0f Hz, N = %d\n",
           FS, N);
    printf("    df = %.2f Hz, 6500 Hz = bin %.2f, 8000 Hz = bin %.2f\n", FS / N,
           6500 * N / FS, 8000 * N / FS);
    for (n = 0; n < N; n++)
        x[n] = sin(2 * M_PI * 6500 * n / FS) + 1e-3 * sin(2 * M_PI * 8000 * n / FS);
    printf("    window       peak[dB]  level@8kHz[dB]  level@9.5kHz[dB]  level@15kHz[dB]\n");
    for (t = 0; t < NWIN; t++) {
        make_window(t, w, N);
        spectrum_db(x, w, N, N, dbw[t]);
        {
            int k8 = (int)floor(8000 * N / FS + 0.5), k95 = (int)floor(9500 * N / FS + 0.5);
            int k15 = (int)floor(15000 * N / FS + 0.5), kp = 0;
            for (k = 0; k <= N / 2; k++) if (dbw[t][k] > dbw[t][kp]) kp = k;
            printf("    %-11s  %7.2f      %7.1f          %7.1f          %7.1f\n", wname[t],
                   dbw[t][kp], dbw[t][k8], dbw[t][k95], dbw[t][k15]);
        }
    }
    ap_plot(dbw[RECT], N / 2 + 1, "(b) rectangular window, dB over bin 0..128");
    ap_plot(dbw[BLACKMAN], N / 2 + 1, "(b) Blackman window, dB over bin 0..128");
    for (k = 0; k <= N / 2; k++) fax[k] = k * FS / N;
    csv_write("leakage.csv", "f,rect,hann,hamming,blackman", N / 2 + 1, 5, fax,
              dbw[0], dbw[1], dbw[2], dbw[3]);

    /* ---------------------------------------------- (c) zero padding */
    printf("\n(c) two tones 6500 Hz and 6900 Hz (equal amplitude), Hann window\n");
    {
        int len[3] = {64, 128, 256};
        int i;
        for (i = 0; i < 3; i++) {
            for (n = 0; n < len[i]; n++)
                x[n] = sin(2 * M_PI * 6500 * n / FS) + sin(2 * M_PI * 6900 * n / FS + 1.0);
            make_window(HANN, w, len[i]);
            spectrum_db(x, w, len[i], NPAD, db);
            printf("  record N = %3d (T = %.1f ms, fs/N = %.1f Hz), zero padded to %d:\n",
                   len[i], len[i] / FS * 1e3, FS / len[i], NPAD);
            print_peaks(db, NPAD / 2, NPAD, 20.0);
            if (len[i] == 64) {
                int k0 = (int)(5000 * NPAD / FS), k1 = (int)(8500 * NPAD / FS);
                ap_plot(db + k0, k1 - k0, "(c) N = 64 zero padded to 4096, 5..8.5 kHz");
            }
        }
    }

    /* ---------------------------------------------- (d) knock spectrum */
    printf("\n(d) virtual knock sensor, 3000 rpm, knock_prob = 1, Hann window\n");
    {
        es_engine_t e;
        int cyc, len, i;
        es_init(&e, 3000.0, 7u);
        e.knock_prob = 1.0;
        for (cyc = 0; cyc < 2; cyc++) {
            /* run to the TDC of the next firing cylinder */
            int c0 = e.last_cyl;
            while (e.last_cyl == c0) es_advance(&e, 1.0 / FS);
            printf("  TDC of cylinder %d, knock onset %.1f deg ATDC\n",
                   es_cyl_id[e.last_cyl],
                   es_angle_diff(e.knock_onset[e.last_cyl], es_tdc_deg[e.last_cyl]));
            for (n = 0; n < N; n++) { x[n] = es_knock(&e); es_advance(&e, 1.0 / FS); }
            /* (d1) full 256-sample window: 0 .. 115 deg ATDC */
            {
                double m = 0;
                for (n = 0; n < N; n++) m += x[n] / N;
                for (n = 0; n < N; n++) x[n] -= m;
            }
            make_window(HANN, w, N);
            spectrum_db(x, w, N, N, db);
            printf("   window 0..%.0f deg ATDC (N = %d):\n", N / FS * 18000.0, N);
            print_peaks(db, N / 2, N, 15.0);
            if (cyc == 0) {
                for (k = 0; k <= N / 2; k++) fax[k] = k * FS / N;
                ap_plot_xy(fax, db, N / 2 + 1, "(d) knock window 0..115 deg ATDC, dB over f/Hz");
                csv_write("knock_spectrum.csv", "f,db", N / 2 + 1, 2, fax, db);
            }
            /* (d2) measurement window 10..70 deg ATDC: 128 samples from 10 deg */
            i = (int)(10.0 / 18000.0 * FS + 0.5);
            len = 128;
            make_window(HANN, w, len);
            spectrum_db(x + i, w, len, len, db);
            printf("   window 10..%.0f deg ATDC (N = %d):\n", 10.0 + len / FS * 18000.0, len);
            print_peaks(db, len / 2, len, 15.0);
        }
    }
    return 0;
}
