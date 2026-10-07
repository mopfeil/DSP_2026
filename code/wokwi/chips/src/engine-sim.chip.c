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
#include "engine_signals.h"

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
