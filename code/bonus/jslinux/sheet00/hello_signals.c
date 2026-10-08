/*
 * Sheet 0, Bonus Exercise 0.B1 -- Getting started with JSLinux
 *
 * Build and run:   gcc -O2 -o hello_signals hello_signals.c -lm
 *                  ./hello_signals
 * (copy engine_signals.h, asciiplot.h and csvio.h into the same directory)
 *
 * Generates a sampled sine and 5 ms of the virtual knock sensor signal,
 * plots both in the terminal and writes them to hello.csv.
 */
#include <stdio.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

#define FS 50000.0          /* sampling rate in Hz */
#define N  250              /* 5 ms                */

int main(void)
{
    static double t[N], x[N], k[N];
    es_engine_t eng;
    int n;

    es_init(&eng, 3000.0, 42u);
    eng.knock_prob = 1.0;            /* every combustion knocks */

    /* skip to shortly before the first knock event (TDC cyl 1 at 120 deg) */
    while (eng.theta < 125.0) es_advance(&eng, 1.0 / FS);

    for (n = 0; n < N; n++) {
        t[n] = n / FS;
        x[n] = sin(2.0 * M_PI * 1000.0 * t[n]);     /* 1 kHz test tone */
        es_advance(&eng, 1.0 / FS);
        k[n] = es_knock(&eng);
    }
    ap_plot(x, N, "1 kHz sine, fs = 50 kHz, 5 ms");
    ap_plot(k, N, "knock sensor signal after TDC of cylinder 1, 5 ms");
    csv_write("hello.csv", "t,sine,knock", N, 3, t, x, k);
    printf("wrote hello.csv (%d rows)\n", N);
    return 0;
}
