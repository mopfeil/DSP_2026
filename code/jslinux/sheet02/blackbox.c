/*
 * Sheet 2, Exercise 2.2 -- black-box system identification
 * (student template)
 *
 * Build and run:   gcc -O2 -o blackbox blackbox.c -lm
 *                  ./blackbox
 * (mystery.h and asciiplot.h in the same directory)
 *
 * Tests the five unknown systems S1..S5 of mystery.h for linearity,
 * time invariance, causality and memory and prints the numerical
 * evidence (largest deviation found by each test).
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mystery.h"
#include "asciiplot.h"

#define N    64
#define TOL  1e-9            /* deviations below TOL count as zero */
#define NTRY 20              /* random repetitions per test        */

typedef void (*system_t)(const double *x, double *y, int n);

static const system_t SYS[5] = { S1, S2, S3, S4, S5 };

/* ---- helpers ------------------------------------------------------- */
static uint32_t rng = 12345u;
static double urand(void)                         /* uniform in [-1, 1) */
{
    rng = rng * 1664525u + 1013904223u;
    return (rng >> 8) / 8388608.0 - 1.0;
}

/* random test signal with amplitude amp; zero in the first and last
   'guard' samples so that shifted versions stay inside 0..N-1          */
static void rand_signal(double *x, double amp, int guard)
{
    int n;
    for (n = 0; n < N; n++)
        x[n] = (n < guard || n >= N - guard) ? 0.0 : amp * urand();
}

static double maxabs(const double *a, int n)
{
    double m = 0.0;
    int i;
    for (i = 0; i < n; i++) if (fabs(a[i]) > m) m = fabs(a[i]);
    return m;
}

/* ---- (a) superposition: S{a x1 + b x2} = a S{x1} + b S{x2} ? ---------
   returns the largest deviation relative to the output amplitude       */
static double test_linear(system_t S, double amp)
{
    (void)S; (void)amp; (void)rand_signal;   /* remove when used */
    /* TODO (a): repeat NTRY times: two random signals x1, x2 of amplitude
       amp (rand_signal), random weights a, b (2*urand()); compare
       S{a x1 + b x2} with a S{x1} + b S{x2}; return the largest
       deviation divided by the output amplitude.                       */
    return 0.0;
}

/* homogeneity tested with scaled unit impulses only (a weak test!) */
static double test_linear_impulse(system_t S)
{
    double x[N] = {0}, y1[N], y2[N], d[N];
    int n;
    x[30] = 1.0;  S(x, y1, N);
    x[30] = 3.0;  S(x, y2, N);
    for (n = 0; n < N; n++) d[n] = y2[n] - 3.0 * y1[n];
    return maxabs(d, N);
}

/* ---- (b) time invariance: S{x[n-k]} = y[n-k] ? ------------------- */
static double test_shift(system_t S)
{
    (void)S;
    /* TODO (b): random x (with zero guard samples), shifted copy
       xs[n] = x[n-k] for k = 1..4; return max |ys[n] - y[n-k]|.        */
    return 0.0;
}

/* ---- (c) causality: inputs equal for n < n0 -> outputs equal for n < n0 ? */
static double test_causal(system_t S)
{
    (void)S;
    /* TODO (c): two inputs that agree for n < n0 and differ afterwards;
       return the largest output difference for n < n0.                */
    return 0.0;
}

/* ---- (d) memory: inputs differ only at n0 -> outputs differ only at n0 ? */
static double test_memory(system_t S)
{
    (void)S;
    /* TODO (d): two inputs that differ only at n0; return the largest
       output difference at n != n0. Use more than one n0 -- why?       */
    return 0.0;
}

static const char *yn(double e) { return e < TOL ? "yes" : "NO "; }

int main(void)
{
    double lin[5], lin_s[5], lin_i[5], ti[5], ca[5], me[5];
    double x[N] = {0}, y[N];
    char title[80];
    int k, n0;

    /* impulse responses for two different impulse positions */
    for (k = 0; k < 5; k++)
        for (n0 = 30; n0 <= 31; n0++) {
            int n;
            for (n = 0; n < N; n++) x[n] = 0.0;
            x[n0] = 1.0;
            SYS[k](x, y, N);
            sprintf(title, "S%d: response to unit impulse at n0 = %d, n = 24..39", k + 1, n0);
            ap_size(48, 7);
            ap_stem(y + 24, 16, title);
        }

    for (k = 0; k < 5; k++) {
        lin[k]   = test_linear(SYS[k], 1.0);
        lin_s[k] = test_linear(SYS[k], 0.01);
        lin_i[k] = test_linear_impulse(SYS[k]);
        ti[k]    = test_shift(SYS[k]);
        ca[k]    = test_causal(SYS[k]);
        me[k]    = test_memory(SYS[k]);
    }
    printf("\nlargest deviations found (relative for the linearity tests):\n");
    printf("sys  linear(A=1) linear(A=0.01) imp-scale   shift      causal     memory\n");
    for (k = 0; k < 5; k++)
        printf("S%d   %9.2e  %9.2e      %9.2e  %9.2e  %9.2e  %9.2e\n", k + 1,
               lin[k], lin_s[k], lin_i[k], ti[k], ca[k], me[k]);
    printf("\nconclusion (deviation < %.0e counts as zero):\n", TOL);
    printf("sys  linear  time-inv  causal  memoryless\n");
    for (k = 0; k < 5; k++)
        printf("S%d   %s     %s       %s     %s\n", k + 1, yn(lin[k]), yn(ti[k]),
               yn(ca[k]), yn(me[k]));
    return 0;
}
