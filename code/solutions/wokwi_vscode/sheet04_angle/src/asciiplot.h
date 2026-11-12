/*
 * asciiplot.h -- dependency-free terminal plots for JSLinux
 *
 *   ap_plot(y, n, "title")               y[n] over sample index
 *   ap_plot_xy(x, y, n, "title")          y over x (x need not be uniform)
 *   ap_plot2(y1, y2, n, "title")          two curves ('*' and 'o')
 *   ap_stem(y, n, "title")                stem plot for short sequences
 *   ap_pz(pr, pi, np, zr, zi, nz, "t")    pole-zero map with unit circle
 *
 * The plot size can be changed with ap_size(width, height).
 * Long signals are decimated by taking min/max per column, so peaks
 * are never lost.
 */
#ifndef ASCIIPLOT_H
#define ASCIIPLOT_H

#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int ap_w = 72, ap_h = 20;

static inline void ap_size(int w, int h)
{
    ap_w = w < 20 ? 20 : (w > 200 ? 200 : w);
    ap_h = h < 5 ? 5 : (h > 80 ? 80 : h);
}

static inline void ap_range(const double *y, int n, double *lo, double *hi)
{
    int i;
    *lo = *hi = y[0];
    for (i = 1; i < n; i++) {
        if (y[i] < *lo) *lo = y[i];
        if (y[i] > *hi) *hi = y[i];
    }
    if (*hi - *lo < 1e-12) { *lo -= 1.0; *hi += 1.0; }
}

/* internal: draw canvas with given y range and x labels */
static inline void ap_print(char *cv, double lo, double hi, double x0, double x1,
                     const char *title)
{
    int r, c;
    if (title) printf("  %s\n", title);
    for (r = 0; r < ap_h; r++) {
        double yv = hi - (hi - lo) * r / (ap_h - 1);
        if (r == 0 || r == ap_h - 1 || r == ap_h / 2) printf("%10.3g |", yv);
        else printf("           |");
        for (c = 0; c < ap_w; c++) putchar(cv[r * ap_w + c]);
        putchar('\n');
    }
    printf("           +");
    for (c = 0; c < ap_w; c++) putchar('-');
    printf("\n           %-10.4g%*s%10.4g\n", x0, ap_w - 20, "", x1);
}

static inline int ap_row(double v, double lo, double hi)
{
    int r = (int)floor((hi - v) / (hi - lo) * (ap_h - 1) + 0.5);
    return r < 0 ? 0 : (r >= ap_h ? ap_h - 1 : r);
}

/* mark column c from value a to b (vertical segment) */
static inline void ap_vseg(char *cv, int c, double a, double b, double lo, double hi,
                    char ch)
{
    int r0 = ap_row(a, lo, hi), r1 = ap_row(b, lo, hi), r;
    if (r0 > r1) { r = r0; r0 = r1; r1 = r; }
    for (r = r0; r <= r1; r++) cv[r * ap_w + c] = ch;
}

static inline void ap_clear(char *cv, double lo, double hi)
{
    int i, rz;
    for (i = 0; i < ap_w * ap_h; i++) cv[i] = ' ';
    if (lo < 0.0 && hi > 0.0) {             /* zero line */
        rz = ap_row(0.0, lo, hi);
        for (i = 0; i < ap_w; i++) cv[rz * ap_w + i] = '.';
    }
}

/* curve into canvas: min/max per column */
static inline void ap_curve(char *cv, const double *y, int n, double lo, double hi,
                     char ch)
{
    int c;
    for (c = 0; c < ap_w; c++) {
        int i0 = (int)((long)c * n / ap_w), i1 = (int)((long)(c + 1) * n / ap_w);
        double mn, mx;
        int i;
        if (i1 <= i0) i1 = i0 + 1;
        if (i0 >= n) break;
        if (i1 > n) i1 = n;
        mn = mx = y[i0];
        for (i = i0 + 1; i < i1; i++) {
            if (y[i] < mn) mn = y[i];
            if (y[i] > mx) mx = y[i];
        }
        ap_vseg(cv, c, mn, mx, lo, hi, ch);
    }
}

static inline void ap_plot(const double *y, int n, const char *title)
{
    static char cv[200 * 80];
    double lo, hi;
    if (n <= 0) return;
    ap_range(y, n, &lo, &hi);
    ap_clear(cv, lo, hi);
    ap_curve(cv, y, n, lo, hi, '*');
    ap_print(cv, lo, hi, 0, n - 1, title);
}

static inline void ap_plot2(const double *y1, const double *y2, int n,
                     const char *title)
{
    static char cv[200 * 80];
    double lo1, hi1, lo2, hi2;
    if (n <= 0) return;
    ap_range(y1, n, &lo1, &hi1);
    ap_range(y2, n, &lo2, &hi2);
    if (lo2 < lo1) lo1 = lo2;
    if (hi2 > hi1) hi1 = hi2;
    ap_clear(cv, lo1, hi1);
    ap_curve(cv, y1, n, lo1, hi1, '*');
    ap_curve(cv, y2, n, lo1, hi1, 'o');
    ap_print(cv, lo1, hi1, 0, n - 1, title);
    printf("           '*' = curve 1, 'o' = curve 2\n");
}

static inline void ap_plot_xy(const double *x, const double *y, int n,
                       const char *title)
{
    static char cv[200 * 80];
    double lo, hi, x0, x1;
    int i;
    if (n <= 0) return;
    ap_range(y, n, &lo, &hi);
    ap_range(x, n, &x0, &x1);
    ap_clear(cv, lo, hi);
    for (i = 0; i < n; i++) {
        int c = (int)floor((x[i] - x0) / (x1 - x0) * (ap_w - 1) + 0.5);
        cv[ap_row(y[i], lo, hi) * ap_w + c] = '*';
    }
    ap_print(cv, lo, hi, x0, x1, title);
}

static inline void ap_stem(const double *y, int n, const char *title)
{
    static char cv[200 * 80];
    double lo, hi;
    int i;
    if (n <= 0) return;
    ap_range(y, n, &lo, &hi);
    if (lo > 0.0) lo = 0.0;
    if (hi < 0.0) hi = 0.0;
    ap_clear(cv, lo - 1e-9, hi);
    for (i = 0; i < n && i < ap_w; i++) {
        int c = n > 1 ? (int)((long)i * (ap_w - 1) / (n - 1)) : 0;
        ap_vseg(cv, c, 0.0, y[i], lo - 1e-9, hi, '|');
        cv[ap_row(y[i], lo - 1e-9, hi) * ap_w + c] = 'o';
    }
    ap_print(cv, lo, hi, 0, n - 1, title);
}

/* pole-zero map: poles 'x', zeros 'o', unit circle '.' */
static inline void ap_pz(const double *pr, const double *pi, int np,
                  const double *zr, const double *zi, int nz,
                  const char *title)
{
    static char cv[200 * 80];
    double R = 1.2;
    int i, k, w = ap_w, h = ap_h;
    for (i = 0; i < np; i++) if (fabs(pr[i]) > R || fabs(pi[i]) > R) R = 1.1 * fmax(fabs(pr[i]), fabs(pi[i]));
    for (i = 0; i < nz; i++) if (fabs(zr[i]) > R || fabs(zi[i]) > R) R = 1.1 * fmax(fabs(zr[i]), fabs(zi[i]));
    for (i = 0; i < w * h; i++) cv[i] = ' ';
#define AP_PZ_PUT(re, im, ch) do { \
        int c_ = (int)floor(((re) + R) / (2 * R) * (w - 1) + 0.5); \
        int r_ = (int)floor((R - (im)) / (2 * R) * (h - 1) + 0.5); \
        if (c_ >= 0 && c_ < w && r_ >= 0 && r_ < h) cv[r_ * w + c_] = (ch); } while (0)
    for (k = 0; k < w; k++) AP_PZ_PUT(-R + 2 * R * k / (w - 1), 0.0, '-');
    for (k = 0; k < h; k++) AP_PZ_PUT(0.0, R - 2 * R * k / (h - 1), '|');
    for (k = 0; k < 360; k++)
        AP_PZ_PUT(cos(k * M_PI / 180), sin(k * M_PI / 180), '.');
    for (i = 0; i < nz; i++) AP_PZ_PUT(zr[i], zi[i], 'o');
    for (i = 0; i < np; i++) AP_PZ_PUT(pr[i], pi[i], 'x');
#undef AP_PZ_PUT
    if (title) printf("  %s\n", title);
    for (i = 0; i < h; i++) {
        printf("  ");
        for (k = 0; k < w; k++) putchar(cv[i * w + k]);
        putchar('\n');
    }
    printf("  Re/Im range +-%.2f   x = pole, o = zero, . = unit circle\n", R);
}

#endif /* ASCIIPLOT_H */
