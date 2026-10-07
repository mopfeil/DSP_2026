/*
 * Sheet 2, Exercise 2.2 -- generator for mystery.h (INSTRUCTOR ONLY)
 *
 * Build and run:   gcc -O2 -o mystery_gen mystery_gen.c && ./mystery_gen > mystery.h
 *
 * All five systems are instances of one generic kernel
 *     m      = (H > 1) ? n - n % H : n                (hold, time-variant)
 *     acc    = sum_{i<M} b_i x[m + L - i] + q x[m] x[m-1]
 *     acc    = (s > 0) ? s tanh(acc / s) : acc        (static saturation)
 *     y[n]   = acc + a y[n-1]
 * (x[k] = 0 outside 0..N-1). The parameters are stored as XOR-scrambled
 * fixed-point words, and S1..S5 are mapped to the kernels by a scrambled
 * permutation, so that the header is not readable at a glance.
 *
 * TRUTH TABLE (keep secret):
 *   S1: y[n] = x[n - n mod 3]                    linear, TIME-VARIANT, causal, memory
 *   S2: y[n] = 0.75 y[n-1] + 0.25 x[n-1]         LTI, causal, memory (IIR)
 *   S3: y[n] = tanh(x[n])                        NONLINEAR, MEMORYLESS, TI, causal
 *   S4: y[n] = 0.25 x[n-1] + 0.5 x[n] + 0.25 x[n+1]   LTI, NON-CAUSAL, memory
 *   S5: y[n] = 0.625 x[n] + 0.375 x[n-1] + 0.25 x[n] x[n-1]
 *                                                NONLINEAR, TI, causal, memory
 */
#include <stdio.h>
#include <stdint.h>

#define NP 9
#define SC 1048576.0                    /* 2^20 fixed point */
#define KEY 0xC3A5E1F7u

/* params: L, M, b0, b1, b2, a, H, q, s   (kernel index = order below) */
static const double P[5][NP] = {
    /* k0 = S4 */ { 1, 3, 0.25, 0.5, 0.25, 0.0, 1, 0.0, 0.0 },
    /* k1 = S1 */ { 0, 1, 1.0, 0.0, 0.0, 0.0, 3, 0.0, 0.0 },
    /* k2 = S5 */ { 0, 2, 0.625, 0.375, 0.0, 0.0, 1, 0.25, 0.0 },
    /* k3 = S2 */ { -1, 1, 0.25, 0.0, 0.0, 0.75, 1, 0.0, 0.0 },
    /* k4 = S3 */ { 0, 1, 1.0, 0.0, 0.0, 0.0, 1, 0.0, 1.0 },
};
/* S1..S5 -> kernel index */
static const int PERM[5] = { 1, 3, 4, 0, 2 };

static uint32_t rotl(uint32_t v, int r) { return (v << r) | (v >> (32 - r)); }

int main(void)
{
    int k, i;
    printf("/*\n * mystery.h -- five unknown systems S1..S5 for Exercise 2.2\n"
           " *\n *   void Sk(const double *x, double *y, int N);   k = 1..5\n"
           " *   x: input x[0..N-1] (x[n] = 0 outside), y: output y[0..N-1]\n"
           " *\n * Treat the systems as black boxes: do NOT try to read this file,\n"
           " * find their properties by experiments only!\n */\n");
    printf("#ifndef MYSTERY_H\n#define MYSTERY_H\n#include <math.h>\n#include <stdint.h>\n\n");
    printf("static const uint32_t mq_t[%d] = {", 5 * NP + 5);
    for (k = 0; k < 5; k++)
        for (i = 0; i < NP; i++) {
            int32_t v = (int32_t)(P[k][i] * SC);
            uint32_t w = rotl((uint32_t)v ^ KEY ^ (uint32_t)((k * NP + i) * 0x9E3779B9u),
                              (k * NP + i) % 29 + 1);
            printf("%s0x%08Xu,", (k * NP + i) % 6 == 0 ? "\n    " : " ", w);
        }
    for (k = 0; k < 5; k++)
        printf("%s0x%08Xu%s", k == 0 ? "\n    " : " ",
               (uint32_t)(PERM[k] * 0x01000193u) ^ (KEY >> (k + 1)), k < 4 ? "," : "");
    printf("\n};\n\n");
    printf(
"static double mq_d(int j)\n{\n"
"    uint32_t w = mq_t[j], r = (uint32_t)(j %% 29 + 1);\n"
"    w = (w >> r) | (w << (32u - r));\n"
"    w ^= 0x%08Xu ^ (uint32_t)((uint32_t)j * 0x9E3779B9u);\n"
"    return (double)(int32_t)w / %.1f;\n}\n\n", KEY, SC);
    printf(
"static double mq_x(const double *x, int N, int k)\n"
"{\n    return (k >= 0 && k < N) ? x[k] : 0.0;\n}\n\n"
"static void mq_run(int s, const double *x, double *y, int N)\n{\n"
"    double p[%d], acc;\n"
"    int j, n, m, c, M, L, H;\n"
"    c = (int)((mq_t[%d + s] ^ (0x%08Xu >> (s + 1))) / 0x01000193u);\n"
"    for (j = 0; j < %d; j++) p[j] = mq_d(c * %d + j);\n"
"    L = (int)floor(p[0] + 0.5); M = (int)floor(p[1] + 0.5); H = (int)floor(p[6] + 0.5);\n"
"    for (n = 0; n < N; n++) {\n"
"        m = H > 1 ? n - n %% H : n;\n"
"        for (acc = 0.0, j = 0; j < M; j++) acc += p[2 + j] * mq_x(x, N, m + L - j);\n"
"        acc += p[7] * mq_x(x, N, m) * mq_x(x, N, m - 1);\n"
"        if (p[8] > 0.0) acc = p[8] * tanh(acc / p[8]);\n"
"        y[n] = acc + p[5] * (n > 0 ? y[n - 1] : 0.0);\n"
"    }\n}\n\n", NP, 5 * NP, KEY, NP, NP);
    for (k = 0; k < 5; k++)
        printf("static void S%d(const double *x, double *y, int N) { mq_run(%d, x, y, N); }\n",
               k + 1, k);
    printf("\n#endif /* MYSTERY_H */\n");
    return 0;
}
