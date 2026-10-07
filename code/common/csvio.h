/*
 * csvio.h -- minimal numeric CSV input/output for JSLinux
 *
 *   csv_t d;
 *   if (csv_read("knock.csv", &d) == 0) {
 *       double *t = csv_col(&d, 0);     -- column copy (free() it)
 *       ... d.rows, d.cols, d.name[c], CSV_AT(&d, r, c) ...
 *       csv_free(&d);
 *   }
 *   csv_write("out.csv", "t,x,y", n, 3, t, x, y);   -- columns as doubles
 *
 * Separators ',' ';' tab and blank are accepted. A first line that does
 * not start with a number is taken as header. Lines starting with '#'
 * are ignored. This matches the CSV printed by the Wokwi sketches, so
 * Serial Monitor output can be pasted into a file directly.
 */
#ifndef CSVIO_H
#define CSVIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define CSV_MAXCOLS 16
#define CSV_AT(d, r, c) ((d)->data[(size_t)(r) * (d)->cols + (c)])

typedef struct {
    int rows, cols;
    double *data;                 /* row-major rows x cols */
    char name[CSV_MAXCOLS][32];   /* header names or "c0", "c1", ... */
} csv_t;

static inline int csv_split(char *line, char **tok)
{
    int n = 0;
    char *p = line;
    while (*p && n < CSV_MAXCOLS) {
        while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';') p++;
        if (!*p || *p == '\n' || *p == '\r') break;
        tok[n++] = p;
        while (*p && *p != ',' && *p != ';' && *p != '\t' && *p != ' ' &&
               *p != '\n' && *p != '\r') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

static inline int csv_isnum(const char *s)
{
    char *end;
    strtod(s, &end);
    return end != s;
}

static inline int csv_read(const char *path, csv_t *d)
{
    FILE *f = fopen(path, "r");
    char line[1024], *tok[CSV_MAXCOLS];
    int cap = 1024, n, c, have_cols = 0;
    if (!f) { perror(path); return -1; }
    d->rows = 0; d->cols = 0;
    for (c = 0; c < CSV_MAXCOLS; c++) sprintf(d->name[c], "c%d", c);
    d->data = NULL;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        n = csv_split(line, tok);
        if (n == 0) continue;
        if (!have_cols) {
            d->cols = n; have_cols = 1;
            d->data = (double *)malloc((size_t)cap * n * sizeof(double));
            if (!csv_isnum(tok[0])) {          /* header line */
                for (c = 0; c < n; c++) {
                    strncpy(d->name[c], tok[c], 31);
                    d->name[c][31] = '\0';
                }
                continue;
            }
        }
        if (n < d->cols || !csv_isnum(tok[0])) continue;  /* skip junk */
        if (d->rows == cap) {
            cap *= 2;
            d->data = (double *)realloc(d->data, (size_t)cap * d->cols * sizeof(double));
        }
        for (c = 0; c < d->cols; c++) CSV_AT(d, d->rows, c) = atof(tok[c]);
        d->rows++;
    }
    fclose(f);
    return have_cols ? 0 : -1;
}

static inline double *csv_col(const csv_t *d, int c)
{
    double *x = (double *)malloc((size_t)(d->rows > 0 ? d->rows : 1) * sizeof(double));
    int r;
    for (r = 0; r < d->rows; r++) x[r] = CSV_AT(d, r, c);
    return x;
}

static inline void csv_free(csv_t *d)
{
    free(d->data);
    d->data = NULL;
    d->rows = d->cols = 0;
}

/* write ncols columns of length n, given as double* varargs */
static inline int csv_write(const char *path, const char *header, int n, int ncols, ...)
{
    FILE *f = fopen(path, "w");
    const double *col[CSV_MAXCOLS];
    va_list ap;
    int i, c;
    if (!f) { perror(path); return -1; }
    va_start(ap, ncols);
    for (c = 0; c < ncols && c < CSV_MAXCOLS; c++) col[c] = va_arg(ap, const double *);
    va_end(ap);
    if (header) fprintf(f, "%s\n", header);
    for (i = 0; i < n; i++)
        for (c = 0; c < ncols; c++)
            fprintf(f, "%.9g%c", col[c][i], c == ncols - 1 ? '\n' : ',');
    fclose(f);
    return 0;
}

#endif /* CSVIO_H */
