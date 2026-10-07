/*
 * lambda-probe.chip.c -- Wokwi custom chip "lambda-probe"
 *
 * Fuel path + switching lambda sensor.
 *   INJ  digital input: PWM from the microcontroller = fuel command.
 *        duty 50 % = stoichiometric fuel quantity (q = 2 * duty).
 *   O2   analog output: lambda sensor voltage (0.1 V lean ... 0.9 V rich)
 *   LAM  analog output: true lambda / 2 in volts (ground truth, 0.5 V = 1.0)
 * Model (1 ms time step):
 *   q_f    = first-order lag of q (wall film, tau 20 ms)
 *   lambda = (1 + air/100) / q_f          (air = unmetered air, %)
 *   lambda delayed by the transport delay T_d (gas travel to the sensor)
 *   sensor = first-order lag (tau 30 ms), then switching characteristic
 * Controls: air (%, -10..10, slider value - 10), delay (ms).
 */
#include "wokwi-api.h"
#include <stdlib.h>
#include <stdbool.h>
#include "engine_signals.h"

#define MAXDELAY 1000

typedef struct {
    pin_t inj, o2, lam;
    uint32_t a_air, a_delay;
    timer_t tick;
    uint64_t t_rise, t_prev_rise, t_edge;
    double duty, qf, ls;
    double buf[MAXDELAY];
    int widx;
} chip_t;

static void on_inj(void *user, pin_t pin, uint32_t value)
{
    chip_t *c = (chip_t *)user;
    uint64_t now = get_sim_nanos();
    (void)pin;
    if (value == HIGH) {
        if (c->t_prev_rise && c->t_edge > c->t_prev_rise && now > c->t_prev_rise)
            c->duty = (double)(c->t_edge - c->t_prev_rise) / (double)(now - c->t_prev_rise);
        c->t_prev_rise = now;
    }
    c->t_edge = now;
}

static void on_tick(void *user)
{
    chip_t *c = (chip_t *)user;
    uint64_t now = get_sim_nanos();
    double q, lam, lam_d, air = ((double)attr_read(c->a_air) - 10.0) / 100.0;
    int d = (int)attr_read(c->a_delay), ridx;
    if (d < 1) d = 1;
    if (d >= MAXDELAY) d = MAXDELAY - 1;
    /* no PWM edges for 5 ms: constant level */
    if (now - c->t_edge > 5000000ull) c->duty = pin_read(c->inj) ? 1.0 : 0.0;
    q = 2.0 * c->duty;
    c->qf += (q - c->qf) * (1.0 / 20.0);                /* tau 20 ms */
    lam = (1.0 + air) / (c->qf > 0.05 ? c->qf : 0.05);
    if (lam > 2.0) lam = 2.0;
    if (lam < 0.5) lam = 0.5;
    c->buf[c->widx] = lam;
    ridx = (c->widx - d + MAXDELAY) % MAXDELAY;
    lam_d = c->buf[ridx];
    c->widx = (c->widx + 1) % MAXDELAY;
    c->ls += (lam_d - c->ls) * (1.0 / 30.0);             /* tau 30 ms */
    pin_dac_write(c->o2, (float)es_lambda_voltage(c->ls));
    pin_dac_write(c->lam, (float)(lam / 2.0));
}

void chip_init(void)
{
    chip_t *c = (chip_t *)calloc(1, sizeof(chip_t));
    timer_config_t tc = { .callback = on_tick, .user_data = c };
    pin_watch_config_t wc = { .edge = BOTH, .pin_change = on_inj, .user_data = c };
    int i;
    c->inj = pin_init("INJ", INPUT);
    c->o2  = pin_init("O2", ANALOG);
    c->lam = pin_init("LAM", ANALOG);
    c->a_air   = attr_init("air", 15);       /* slider 0..20 -> -10..+10 % */
    c->a_delay = attr_init("delay", 200);
    c->duty = 0.5; c->qf = 1.0; c->ls = 1.0;
    for (i = 0; i < MAXDELAY; i++) c->buf[i] = 1.0;
    pin_watch(c->inj, &wc);
    c->tick = timer_init(&tc);
    timer_start(c->tick, 1000, true);
}
