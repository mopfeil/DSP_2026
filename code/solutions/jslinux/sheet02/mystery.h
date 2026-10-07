/*
 * mystery.h -- five unknown systems S1..S5 for Exercise 2.2
 *
 *   void Sk(const double *x, double *y, int N);   k = 1..5
 *   x: input x[0..N-1] (x[n] = 0 outside), y: output y[0..N-1]
 *
 * Treat the systems as black boxes: do NOT try to read this file,
 * find their properties by experiments only!
 */
#ifndef MYSTERY_H
#define MYSTERY_H
#include <math.h>
#include <stdint.h>

static const uint32_t mq_t[50] = {
    0x876BC3EFu, 0x768A6139u, 0xFE78942Fu, 0x90B8CDC1u, 0x6F80E277u, 0x2C205AB5u,
    0x7C9DD0BBu, 0x21B5F890u, 0x3C587E64u, 0x5A99D931u, 0xF9066F6Cu, 0x7DB040FDu,
    0x8AAB7527u, 0x33E4B2DDu, 0xA4F4B256u, 0xC02086D5u, 0xF4CE41A4u, 0xD2F9082Bu,
    0x77AEE21Bu, 0x94C7D9BEu, 0x7073FF4Cu, 0x768E4BC7u, 0x08ADB0CAu, 0x68F55E0Eu,
    0x5E2D0D11u, 0x9AC32C0Bu, 0xEE903DE9u, 0x4938C377u, 0x71B49639u, 0x5FD25004u,
    0x24968965u, 0x5892E487u, 0x546D6D70u, 0xD26A25D4u, 0x3EF2D970u, 0x1822DE31u,
    0x68FFF3FCu, 0x62EC943Bu, 0x27C206FEu, 0xB356C6CEu, 0xEE51F7B0u, 0xF3EAD2A8u,
    0x066B4DABu, 0x49F2287Au, 0x0A3BF23Du,
    0x60D2F168u, 0x33E97CC4u, 0x1C74BA72u, 0x0C3A5E1Fu, 0x041D2C29u
};

static double mq_d(int j)
{
    uint32_t w = mq_t[j], r = (uint32_t)(j % 29 + 1);
    w = (w >> r) | (w << (32u - r));
    w ^= 0xC3A5E1F7u ^ (uint32_t)((uint32_t)j * 0x9E3779B9u);
    return (double)(int32_t)w / 1048576.0;
}

static double mq_x(const double *x, int N, int k)
{
    return (k >= 0 && k < N) ? x[k] : 0.0;
}

static void mq_run(int s, const double *x, double *y, int N)
{
    double p[9], acc;
    int j, n, m, c, M, L, H;
    c = (int)((mq_t[45 + s] ^ (0xC3A5E1F7u >> (s + 1))) / 0x01000193u);
    for (j = 0; j < 9; j++) p[j] = mq_d(c * 9 + j);
    L = (int)floor(p[0] + 0.5); M = (int)floor(p[1] + 0.5); H = (int)floor(p[6] + 0.5);
    for (n = 0; n < N; n++) {
        m = H > 1 ? n - n % H : n;
        for (acc = 0.0, j = 0; j < M; j++) acc += p[2 + j] * mq_x(x, N, m + L - j);
        acc += p[7] * mq_x(x, N, m) * mq_x(x, N, m - 1);
        if (p[8] > 0.0) acc = p[8] * tanh(acc / p[8]);
        y[n] = acc + p[5] * (n > 0 ? y[n - 1] : 0.0);
    }
}

static void S1(const double *x, double *y, int N) { mq_run(0, x, y, N); }
static void S2(const double *x, double *y, int N) { mq_run(1, x, y, N); }
static void S3(const double *x, double *y, int N) { mq_run(2, x, y, N); }
static void S4(const double *x, double *y, int N) { mq_run(3, x, y, N); }
static void S5(const double *x, double *y, int N) { mq_run(4, x, y, N); }

#endif /* MYSTERY_H */
