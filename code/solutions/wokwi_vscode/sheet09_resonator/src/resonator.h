/*
 * resonator.h -- two-pole digital resonator in float and in Q15 fixed point
 *                Sheet 9, Exercise 9.3 (REFERENCE SOLUTION)
 *
 * Used unchanged by the Uno sketch (Wokwi) and by limitcycle.c (JSLinux).
 * Only fixed-width integer types are used (int is 16 bit on the AVR!).
 *
 *   H(z) = b0 / (1 + a1 z^-1 + a2 z^-2),  a1 = -2 r cos(th), a2 = r^2,
 *   th = 2 pi f0 / fs,  b0 = (1 - r) |1 - r e^{-2j th}|  (gain 1 at f0)
 *
 * Fixed point: signals x, y in Q15 (int16, 1 LSB = 2^-15),
 *              coefficients in Q14 (int16, range -2 .. 2 - 2^-14) because
 *              |a1| may be close to 2,
 *              accumulator int32 in Q29 (Q14 * Q15),
 *              y = acc >> 14 with truncation or rounding, then saturation
 *              or two's complement wrap-around to int16.
 */
#ifndef RESONATOR_H
#define RESONATOR_H

#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float b0, a1, a2;     /* coefficients            */
    float y1, y2;         /* y[n-1], y[n-2]          */
} res_f_t;

typedef struct {
    int16_t b0, a1, a2;   /* Q14 coefficients        */
    int16_t y1, y2;       /* Q15 state               */
    uint8_t rnd;          /* 1 = round, 0 = truncate */
    uint8_t sat;          /* 1 = saturate, 0 = wrap  */
    uint16_t novf;        /* number of overflows     */
} res_q_t;

/* design: coefficients as float (normalise = 0 -> b0 = 1) */
static void res_design(float f0, float fs, float r, int normalise,
                       float *b0, float *a1, float *a2)
{
    float th = 2.0f * (float)M_PI * f0 / fs;
    *a1 = -2.0f * r * cosf(th);
    *a2 = r * r;
    *b0 = normalise ? fabsf(1.0f - r) * sqrtf(1.0f - 2.0f * r * cosf(2.0f * th) + r * r)
                    : 1.0f;
    if (normalise && *b0 < 1e-6f) *b0 = sinf(th);    /* r = 1: oscillator */
}

static void res_f_init(res_f_t *s, float b0, float a1, float a2)
{
    s->b0 = b0; s->a1 = a1; s->a2 = a2;
    s->y1 = s->y2 = 0.0f;
}

static float res_f_step(res_f_t *s, float x)
{
    float y = s->b0 * x - s->a1 * s->y1 - s->a2 * s->y2;
    s->y2 = s->y1;
    s->y1 = y;
    return y;
}

/* float -> Q14 with rounding and range check */
static int16_t res_q14(float c)
{
    float v = c * 16384.0f;
    v = v < 0 ? v - 0.5f : v + 0.5f;
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    return (int16_t)v;
}

static void res_q_init(res_q_t *s, float b0, float a1, float a2, int rnd, int sat)
{
    s->b0 = res_q14(b0); s->a1 = res_q14(a1); s->a2 = res_q14(a2);
    s->y1 = s->y2 = 0;
    s->rnd = (uint8_t)rnd; s->sat = (uint8_t)sat; s->novf = 0;
}

static int16_t res_q_step(res_q_t *s, int16_t x)
{
    int32_t acc, y;
    acc = (int32_t)s->b0 * x - (int32_t)s->a1 * s->y1 - (int32_t)s->a2 * s->y2; /* Q29 */
    if (s->rnd) acc += (int32_t)1 << 13;      /* round to nearest          */
    y = acc >> 14;                            /* arithmetic shift: floor   */
    if (y > 32767 || y < -32768) {
        s->novf++;
        if (s->sat) y = y > 0 ? 32767 : -32768;
        else y = (int16_t)(uint16_t)(y & 0xFFFF);  /* two's complement wrap */
    }
    s->y2 = s->y1;
    s->y1 = (int16_t)y;
    return (int16_t)y;
}

/* effective pole radius and pole frequency of the quantised coefficients */
static void res_q_poles(const res_q_t *s, float fs, float *r, float *f)
{
    float a1 = s->a1 / 16384.0f, a2 = s->a2 / 16384.0f, c;
    *r = sqrtf(a2 > 0 ? a2 : 0);
    c = *r > 0 ? -a1 / (2.0f * *r) : 0;
    if (c > 1) c = 1;
    if (c < -1) c = -1;
    *f = acosf(c) / (2.0f * (float)M_PI) * fs;
}

#endif /* RESONATOR_H */
