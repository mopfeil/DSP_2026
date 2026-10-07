/*
 * engine-sim.chip.c -- Wokwi custom chip "engine-sim"
 *
 * Virtual 4-cylinder engine. Outputs
 *   CRANK  digital, 60-2 trigger wheel (exact edge timing)
 *   CAM    digital, HIGH for crank angle 0..180 deg
 *   KNOCK  analog,  offset + gain * knock sensor signal  (clamped 0..VMAX)
 *   ION    analog,  gain * ion current signal            (clamped 0..VMAX)
 *   TDC    digital, 50 us pulse at every combustion TDC (ground truth)
 *   KNK    digital, HIGH while a knock burst is active    (ground truth)
 * Controls (sliders in the simulator, ids = attributes):
 *   rpm, knock_prob (%), knock_amp (x0.1 V), noise (mV)
 * Attributes (diagram.json "attrs"): offset (V, default 1.65),
 *   gain (default 0.6), vmax (V, default 3.3; use 5 for Arduino Uno),
 *   fs_khz (analog update rate, default 50), seed.
 *
 * The signal model is engine_signals.h (inlined below by make_chips.sh).
 */
#include "wokwi-api.h"
#include <stdlib.h>
#include <stdbool.h>
/* ---- begin inlined engine_signals.h ---- */
/*
 * engine_signals.h -- virtual engine for the DSP exercises
 *
 * Header-only C99 model of the sensor signals of a 4-cylinder, 4-stroke
 * spark-ignition engine. The same file is used
 *   - on JSLinux   (gcc/tcc, link with -lm),
 *   - in Wokwi Arduino/ESP32 sketches (add as extra file to the project),
 *   - inside the Wokwi custom chip "engine-sim" (inlined by tools/make_chips.sh).
 *
 * Conventions (used consistently in all exercise sheets)
 * -----------------------------------------------------
 *   crank angle theta in degrees, 0 <= theta < 720 (one engine cycle)
 *   60-2 trigger wheel: 60 tooth positions per revolution, 6 deg each.
 *     Tooth k (k = 0..57) is HIGH for [6k, 6k+3) deg, LOW for [6k+3, 6k+6).
 *     Positions 58 and 59 are missing -> signal LOW from 345 to 360 deg.
 *     The rising edge of tooth 0 (theta = 0 or 360) is the reference mark.
 *     Rising edge to rising edge: 6 deg normally, 18 deg across the gap.
 *   Cam signal: HIGH for 0 <= theta < 180, LOW otherwise (once per cycle).
 *   Firing order 1-3-4-2, combustion TDC at
 *     cyl 1: 120 deg, cyl 3: 300 deg, cyl 4: 480 deg, cyl 2: 660 deg.
 *   Spark at TDC - spark_adv.
 *   Knock: damped oscillation, modes ES_KNOCK_F1 (6.5 kHz) and
 *     ES_KNOCK_F2 (10.5 kHz), decay time ES_KNOCK_TAU, onset 5..25 deg ATDC.
 *   Valve-closing impacts: damped 8 kHz burst at TDC + 90 deg (disturbance).
 *
 * All signal functions return volts (knock and ion are AC/positive values
 * without ADC offset; the caller adds an offset for a unipolar ADC).
 *
 * Usage:
 *   es_engine_t e; es_init(&e, 3000.0, 1234u);
 *   for (n = 0; n < N; n++) { es_advance(&e, 1.0/fs); x[n] = es_knock(&e); }
 */
#ifndef ENGINE_SIGNALS_H
#define ENGINE_SIGNALS_H

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ES_NCYL        4
#define ES_TEETH       60
#define ES_KNOCK_F1    6500.0   /* Hz, first circumferential mode  */
#define ES_KNOCK_F2    10500.0  /* Hz, second mode                 */
#define ES_KNOCK_TAU   0.8e-3   /* s, decay time of knock burst    */
#define ES_VALVE_F     8000.0   /* Hz, valve impact resonance      */
#define ES_VALVE_TAU   0.4e-3   /* s                                */

static const double es_tdc_deg[ES_NCYL] = {120.0, 300.0, 480.0, 660.0};
static const int    es_cyl_id[ES_NCYL]  = {1, 3, 4, 2};   /* firing order */

typedef struct {
    /* ---- parameters (may be changed at any time) ---- */
    double rpm;           /* mean engine speed                         */
    double speed_ripple;  /* relative 2nd-order speed fluctuation      */
    double spark_adv;     /* ignition advance, deg before TDC          */
    double knock_prob;    /* probability that a combustion knocks      */
    double knock_amp;     /* mean knock amplitude, V                   */
    double noise_amp;     /* std of broadband sensor noise, V          */
    double valve_amp;     /* amplitude of valve impact bursts, V       */
    double comb_amp;      /* low-frequency combustion vibration, V     */
    double ion_amp;       /* amplitude of ion current peak, V          */
    /* ---- state (read only for the user) ---- */
    double t;             /* simulated time, s                         */
    double theta;         /* crank angle, deg, [0, 720)                */
    uint32_t rng;         /* xorshift32 state                          */
    double knock_t0, knock_a, knock_ph; /* active knock burst           */
    double valve_t0;                    /* active valve burst           */
    int    knock_flag[ES_NCYL];  /* ground truth of the current cycle   */
    double knock_onset[ES_NCYL]; /* onset angle (deg) per cylinder      */
    double knock_ampl[ES_NCYL];  /* amplitude per cylinder              */
    int    last_cyl;             /* index (0..3) of last fired cylinder */
    uint32_t cycle;              /* number of completed 720 deg cycles  */
} es_engine_t;

/* ------------------------------------------------------------------ */
/* random numbers (deterministic on every platform)                    */
/* ------------------------------------------------------------------ */
static inline uint32_t es_rand_u32(es_engine_t *e)
{
    uint32_t x = e->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    e->rng = x;
    return x;
}

static inline double es_rand_uniform(es_engine_t *e)      /* (0, 1) */
{
    return ((double)(es_rand_u32(e) >> 8) + 0.5) / 16777216.0;
}

static inline double es_rand_gauss(es_engine_t *e)        /* N(0, 1) */
{
    double u1 = es_rand_uniform(e), u2 = es_rand_uniform(e);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */
static inline double es_wrap720(double th)
{
    while (th >= 720.0) th -= 720.0;
    while (th < 0.0)    th += 720.0;
    return th;
}

/* signed angle difference a - b mapped to [-360, 360) */
static inline double es_angle_diff(double a, double b)
{
    double d = a - b;
    while (d >= 360.0) d -= 720.0;
    while (d < -360.0) d += 720.0;
    return d;
}

/* draw knock decision for one cylinder */
static inline void es_draw_knock(es_engine_t *e, int c)
{
    e->knock_flag[c]  = es_rand_uniform(e) < e->knock_prob;
    e->knock_onset[c] = es_wrap720(es_tdc_deg[c] + 5.0 + 20.0 * es_rand_uniform(e));
    e->knock_ampl[c]  = e->knock_amp * (0.5 + es_rand_uniform(e));
}

static inline void es_init(es_engine_t *e, double rpm, uint32_t seed)
{
    int c;
    e->rpm = rpm;
    e->speed_ripple = 0.01;
    e->spark_adv = 15.0;
    e->knock_prob = 0.2;
    e->knock_amp = 1.0;
    e->noise_amp = 0.05;
    e->valve_amp = 0.3;
    e->comb_amp = 0.2;
    e->ion_amp = 1.0;
    e->t = 0.0;
    e->theta = 0.0;
    e->rng = seed ? seed : 0x12345678u;
    e->knock_t0 = -1.0; e->knock_a = 0.0; e->knock_ph = 0.0;
    e->valve_t0 = -1.0;
    e->last_cyl = ES_NCYL - 1;
    e->cycle = 0;
    for (c = 0; c < ES_NCYL; c++) es_draw_knock(e, c);
}

/* instantaneous angular speed in deg/s (incl. 2nd-order ripple) */
static inline double es_omega(const es_engine_t *e)
{
    double th = e->theta * M_PI / 180.0;
    return 6.0 * e->rpm * (1.0 + e->speed_ripple * sin(2.0 * th));
}

/* true if angle 'a' lies in [th0, th1) when moving from th0 to th1
   (th1 may wrap at 720); half-open, so every event is counted once */
static inline int es_crossed(double th0, double th1, double a)
{
    if (th1 >= th0) return a >= th0 && a < th1;
    return a >= th0 || a < th1;               /* wrapped at 720 */
}

/* advance the engine by dt seconds (dt should be < 1 ms) */
static inline void es_advance(es_engine_t *e, double dt)
{
    double th0 = e->theta, th1;
    int c;
    th1 = th0 + es_omega(e) * dt;
    if (th1 >= 720.0) { th1 -= 720.0; e->cycle++; }
    e->t += dt;
    for (c = 0; c < ES_NCYL; c++) {
        /* combustion TDC: remember fired cylinder                   */
        if (es_crossed(th0, th1, es_tdc_deg[c])) e->last_cyl = c;
        /* knock onset                                              */
        if (e->knock_flag[c] && es_crossed(th0, th1, e->knock_onset[c])) {
            e->knock_t0 = e->t;
            e->knock_a  = e->knock_ampl[c];
            e->knock_ph = 2.0 * M_PI * es_rand_uniform(e);
        }
        /* valve impact                                             */
        if (es_crossed(th0, th1, es_wrap720(es_tdc_deg[c] + 90.0)))
            e->valve_t0 = e->t;
        /* new random knock decision 90 deg before TDC (next cycle)  */
        if (es_crossed(th0, th1, es_wrap720(es_tdc_deg[c] - 90.0)))
            es_draw_knock(e, c);
    }
    e->theta = th1;
}

/* ------------------------------------------------------------------ */
/* sensor signals                                                      */
/* ------------------------------------------------------------------ */

/* tooth position 0..59 within the current revolution */
static inline int es_tooth_pos(const es_engine_t *e)
{
    double th = fmod(e->theta, 360.0);
    return (int)(th / 6.0);
}

/* 60-2 crankshaft signal (Hall sensor, logic level 0/1) */
static inline int es_crank(const es_engine_t *e)
{
    double th = fmod(e->theta, 360.0);
    int k = (int)(th / 6.0);
    if (k >= 58) return 0;                     /* missing teeth   */
    return (th - 6.0 * k) < 3.0 ? 1 : 0;
}

/* camshaft signal (logic level 0/1), HIGH for 0 <= theta < 180 */
static inline int es_cam(const es_engine_t *e)
{
    return e->theta < 180.0 ? 1 : 0;
}

/* knock sensor signal in volts (zero mean) */
static inline double es_knock(es_engine_t *e)
{
    double v = e->noise_amp * es_rand_gauss(e);
    double tk, d;
    int c;
    /* knock burst                                                   */
    if (e->knock_t0 >= 0.0) {
        tk = e->t - e->knock_t0;
        if (tk < 10.0 * ES_KNOCK_TAU)
            v += e->knock_a * exp(-tk / ES_KNOCK_TAU) *
                 (sin(2.0 * M_PI * ES_KNOCK_F1 * tk + e->knock_ph) +
                  0.5 * sin(2.0 * M_PI * ES_KNOCK_F2 * tk + 1.3 * e->knock_ph));
    }
    /* valve impact burst                                            */
    if (e->valve_t0 >= 0.0) {
        tk = e->t - e->valve_t0;
        if (tk < 10.0 * ES_VALVE_TAU)
            v += e->valve_amp * exp(-tk / ES_VALVE_TAU) *
                 sin(2.0 * M_PI * ES_VALVE_F * tk);
    }
    /* low-frequency combustion vibration: half sine 0..60 deg ATDC   */
    for (c = 0; c < ES_NCYL; c++) {
        d = es_angle_diff(e->theta, es_tdc_deg[c]);
        if (d >= 0.0 && d < 60.0) v += e->comb_amp * sin(M_PI * d / 60.0);
    }
    return v;
}

/* ion current signal (measuring circuit output, V, mostly >= 0) of the
   cylinder that fired last: chemical peak near TDC, thermal peak later */
static inline double es_ion(es_engine_t *e)
{
    double v = 0.0, d;
    int c;
    for (c = 0; c < ES_NCYL; c++) {
        d = es_angle_diff(e->theta, es_tdc_deg[c]);
        if (d > -e->spark_adv && d < 90.0) {
            double dc = (d + 2.0) / 4.0, dt = (d - 15.0) / 10.0;
            v += e->ion_amp * (exp(-dc * dc) + 0.6 * exp(-dt * dt));
            if (e->knock_flag[c] && d > es_angle_diff(e->knock_onset[c], es_tdc_deg[c]))
                v += 0.1 * e->ion_amp *
                     sin(2.0 * M_PI * ES_KNOCK_F1 * (e->t - e->knock_t0));
        }
    }
    return v + 0.01 * es_rand_gauss(e);
}

/* switching (binary) lambda sensor: ~0.9 V rich, ~0.1 V lean */
static inline double es_lambda_voltage(double lambda)
{
    return 0.5 + 0.4 * tanh((1.0 - lambda) / 0.005);
}

#endif /* ENGINE_SIGNALS_H */
/* ---- end inlined engine_signals.h ---- */

typedef struct {
    pin_t crank, cam, knock, ion, tdc, knk;
    uint32_t a_rpm, a_kprob, a_kamp, a_noise;
    float offset, gain, vmax;
    timer_t edge_timer, sample_timer, tdc_timer;
    es_engine_t e;
    uint64_t last_ns;
    int half_idx;          /* index of next 3-deg boundary, 0..239 */
    uint32_t nsamp;
} chip_t;

static float clampv(float v, float vmax)
{
    return v < 0.0f ? 0.0f : (v > vmax ? vmax : v);
}

static void read_controls(chip_t *c)
{
    c->e.rpm        = (double)attr_read(c->a_rpm);
    c->e.knock_prob = attr_read(c->a_kprob) / 100.0;
    c->e.knock_amp  = attr_read(c->a_kamp) / 10.0;
    c->e.noise_amp  = attr_read(c->a_noise) / 1000.0;
    if (c->e.rpm < 100.0) c->e.rpm = 100.0;
}

/* bring the engine model to the current simulation time */
static void sync_engine(chip_t *c)
{
    uint64_t now = get_sim_nanos();
    double dt = (double)(now - c->last_ns) * 1e-9;
    c->last_ns = now;
    while (dt > 20e-6) { es_advance(&c->e, 20e-6); dt -= 20e-6; }
    if (dt > 0.0) es_advance(&c->e, dt);
}

static void on_tdc_end(void *user)
{
    chip_t *c = (chip_t *)user;
    pin_write(c->tdc, LOW);
}

static void on_edge(void *user)
{
    chip_t *c = (chip_t *)user;
    double next;
    sync_engine(c);
    /* snap the model angle exactly to the boundary that is due now */
    c->e.theta = 3.0 * c->half_idx;
    pin_write(c->crank, es_crank(&c->e) ? HIGH : LOW);
    pin_write(c->cam, es_cam(&c->e) ? HIGH : LOW);
    /* TDC pulse exactly at the boundary 120/300/480/660 deg
       (all TDC angles are multiples of 3 deg) */
    if (c->half_idx % 60 == 40) {
        pin_write(c->tdc, HIGH);
        timer_start(c->tdc_timer, 50, false);
    }
    c->half_idx = (c->half_idx + 1) % 240;
    next = 3.0 * c->half_idx;
    if (next == 0.0) next = 720.0;
    timer_start_ns(c->edge_timer,
                   (uint64_t)((next - c->e.theta) / es_omega(&c->e) * 1e9), false);
}

static void on_sample(void *user)
{
    chip_t *c = (chip_t *)user;
    double kt;
    if ((c->nsamp++ & 63u) == 0) read_controls(c);
    sync_engine(c);
    pin_dac_write(c->knock, clampv(c->offset + c->gain * (float)es_knock(&c->e), c->vmax));
    pin_dac_write(c->ion, clampv(c->gain * (float)es_ion(&c->e), c->vmax));
    kt = c->e.t - c->e.knock_t0;
    pin_write(c->knk, (c->e.knock_t0 >= 0.0 && kt < 3.0 * ES_KNOCK_TAU) ? HIGH : LOW);
}

void chip_init(void)
{
    chip_t *c = (chip_t *)malloc(sizeof(chip_t));
    uint32_t a_off, a_gain, a_vmax, a_fs, a_seed;
    timer_config_t te = { .callback = on_edge, .user_data = c };
    timer_config_t ts = { .callback = on_sample, .user_data = c };
    timer_config_t tt = { .callback = on_tdc_end, .user_data = c };

    c->crank = pin_init("CRANK", OUTPUT_LOW);
    c->cam   = pin_init("CAM", OUTPUT_LOW);
    c->tdc   = pin_init("TDC", OUTPUT_LOW);
    c->knk   = pin_init("KNK", OUTPUT_LOW);
    c->knock = pin_init("KNOCK", ANALOG);
    c->ion   = pin_init("ION", ANALOG);

    c->a_rpm   = attr_init("rpm", 3000);
    c->a_kprob = attr_init("knock_prob", 20);
    c->a_kamp  = attr_init("knock_amp", 10);
    c->a_noise = attr_init("noise", 50);
    a_off  = attr_init_float("offset", 1.65f);
    a_gain = attr_init_float("gain", 0.6f);
    a_vmax = attr_init_float("vmax", 3.3f);
    a_fs   = attr_init("fs_khz", 50);
    a_seed = attr_init("seed", 1234);
    c->offset = attr_read_float(a_off);
    c->gain   = attr_read_float(a_gain);
    c->vmax   = attr_read_float(a_vmax);

    es_init(&c->e, 3000.0, attr_read(a_seed));
    read_controls(c);
    c->last_ns = get_sim_nanos();
    c->half_idx = 1;
    c->nsamp = 0;
    pin_write(c->crank, HIGH);
    pin_write(c->cam, HIGH);

    c->edge_timer   = timer_init(&te);
    c->sample_timer = timer_init(&ts);
    c->tdc_timer    = timer_init(&tt);
    timer_start_ns(c->edge_timer, (uint64_t)(3.0 / es_omega(&c->e) * 1e9), false);
    timer_start_ns(c->sample_timer, 1000000ull / attr_read(a_fs), true);
    printf("engine-sim: 60-2 wheel, 4 cyl, knock %.1f/%.1f kHz\n",
           ES_KNOCK_F1 / 1000, ES_KNOCK_F2 / 1000);
}
