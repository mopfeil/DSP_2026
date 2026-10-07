/*
 * Sheet 3, Exercise 3.2 -- convolution engine (student template)
 *
 * Build and run:   gcc -O2 -o conv conv.c -lm
 *                  ./conv
 * (asciiplot.h in the same directory)
 *
 * (a) linear convolution y = x * h, check with the example of Ex. 3.1
 * (b) commutativity, associativity, distributivity, numerically
 * (c) exponential smoother: recursion vs. convolution with truncated h
 * (d) cascade of two moving averages = triangular impulse response
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "asciiplot.h"

#define MAXN 4096

/* ---- (a) y[n] = sum_k x[k] h[n-k],  n = 0 .. nx+nh-2 ------------------ */
static int conv(const double *x, int nx, const double *h, int nh, double *y)
{
    int n, ny = nx + nh - 1;
    (void)x; (void)h;
    /* TODO (a): y[n] = sum_k x[k] h[n-k] for n = 0..ny-1; let k run only
       over the indices where both x[k] and h[n-k] exist.               */
    for (n = 0; n < ny; n++) y[n] = 0.0;
    return ny;
}

/* ---- helpers --------------------------------------------------------- */
static unsigned rng = 4711u;
static double urand(void)
{
    rng = rng * 1103515245u + 12345u;
    return (rng >> 1) / 1073741824.0 - 1.0;       /* 31 random bits */
}

static double maxdiff(const double *a, const double *b, int n)
{
    double m = 0.0;
    int i;
    for (i = 0; i < n; i++) if (fabs(a[i] - b[i]) > m) m = fabs(a[i] - b[i]);
    return m;
}

static void print_seq(const char *name, const double *v, int n)
{
    int i;
    printf("%s =", name);
    for (i = 0; i < n; i++) printf(" %g", v[i]);
    printf("\n");
}

/* ---- (c) recursive exponential smoother y[n] = a y[n-1] + (1-a) x[n] -- */
static void exp_smooth(const double *x, int n, double a, double *y)
{
    int i;
    (void)a;
    /* TODO (c): recursion y[n] = a y[n-1] + (1-a) x[n], initially at rest */
    for (i = 0; i < n; i++) y[i] = x[i];
}

int main(void)
{
    static double x[MAXN], h[MAXN], g[MAXN], y1[MAXN], y2[MAXN], t1[MAXN], t2[MAXN];
    static double xh[MAXN];
    int i, n, L;

    /* (a) example of Exercise 3.1 a) */
    {
        double xe[4] = { 1, 2, 0, -1 }, he[3] = { 1, -1, 0.5 }, ye[6];
        n = conv(xe, 4, he, 3, ye);
        printf("(a) x * h for the example of Ex. 3.1:\n");
        print_seq("    x", xe, 4); print_seq("    h", he, 3); print_seq("    y", ye, n);
    }

    /* (b) algebraic properties with random sequences */
    for (i = 0; i < 200; i++) x[i] = urand();
    for (i = 0; i < 50; i++)  h[i] = urand();
    for (i = 0; i < 30; i++)  g[i] = urand();
    conv(x, 200, h, 50, y1);  conv(h, 50, x, 200, y2);
    printf("\n(b) commutativity   max|x*h - h*x|             = %.2e\n",
           maxdiff(y1, y2, 249));
    conv(x, 200, h, 50, t1);  conv(t1, 249, g, 30, y1);        /* (x*h)*g */
    conv(h, 50, g, 30, t2);   conv(x, 200, t2, 79, y2);        /* x*(h*g) */
    printf("    associativity   max|(x*h)*g - x*(h*g)|     = %.2e\n",
           maxdiff(y1, y2, 278));
    for (i = 0; i < 50; i++) xh[i] = h[i] + (i < 30 ? g[i] : 0.0);  /* h+g */
    conv(x, 200, xh, 50, y1);
    conv(x, 200, h, 50, t1);  conv(x, 200, g, 30, t2);
    for (i = 0; i < 249; i++) y2[i] = t1[i] + (i < 229 ? t2[i] : 0.0);
    printf("    distributivity  max|x*(h+g) - (x*h + x*g)| = %.2e\n",
           maxdiff(y1, y2, 249));

    /* (c) exponential smoother: recursion vs. truncated convolution */
    {
        double a = 0.9;
        int nx = 1000;
        for (i = 0; i < nx; i++) x[i] = (i >= 100 ? 1.0 : 0.0) + 0.2 * urand();
        exp_smooth(x, nx, a, y1);
        printf("\n(c) exponential smoother a = %.2f, %d samples\n", a, nx);
        printf("      L   max|recursive - conv|   a^L      mult./sample\n");
        for (L = 10; L <= 160; L *= 2) {
            /* TODO (c): h[i] = impulse response of the smoother, i < L */
            for (i = 0; i < L; i++) h[i] = (i == 0);
            conv(x, nx, h, L, y2);
            printf("    %3d   %.3e           %.3e   %d (recursion: 2)\n",
                   L, maxdiff(y1, y2, nx), pow(a, L), L);
        }
        /* step response check: s[n] = 1 - a^(n+1) */
        for (i = 0; i < 60; i++) x[i] = 1.0;
        exp_smooth(x, 60, a, y1);
        for (i = 0; i < 60; i++) y2[i] = 1.0 - pow(a, i + 1);
        printf("    step response vs. 1 - a^(n+1): max deviation %.2e\n",
               maxdiff(y1, y2, 60));
    }

    /* (d) cascade of two 8-point moving averages */
    {
        int N = 8;
        for (i = 0; i < N; i++) h[i] = 1.0 / N;
        n = conv(h, N, h, N, g);                     /* h * h */
        printf("\n(d) h_MA8 * h_MA8 (length %d), times 64:", n);
        for (i = 0; i < n; i++) printf(" %g", 64.0 * g[i]);
        printf("\n");
        ap_size(60, 10);
        ap_stem(g, n, "(d) impulse response of two cascaded 8-point MAs");
        /* step responses: one MA vs. cascade */
        for (i = 0; i < 40; i++) x[i] = i >= 5 ? 1.0 : 0.0;
        conv(x, 40, h, N, y1);
        conv(x, 40, g, n, y2);
        ap_size(60, 12);
        ap_plot2(y1, y2, 40, "(d) step response: one MA8 (*), cascade (o)");
        {
            double s = 0.0, c = 0.0;
            for (i = 0; i < n; i++) { s += g[i]; c += i * g[i]; }
            printf("    sum of h*h = %.6f, centroid = %.2f samples "
                   "(one MA8: %.2f)\n", s, c / s, (N - 1) / 2.0);
        }
    }
    return 0;
}
