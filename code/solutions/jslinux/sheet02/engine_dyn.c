/*
 * Sheet 2, Exercise 2.3 -- the engine as a dynamic system
 * (REFERENCE SOLUTION)
 *
 * Build and run:   gcc -O2 -o engine_dyn engine_dyn.c -lm
 *                  ./engine_dyn
 * (engine_signals.h, asciiplot.h and csvio.h in the same directory)
 *
 * (a) crankshaft speed as a first-order system driven by the fuel quantity
 * (b) lambda path: fuel -> wall film -> lambda -> transport delay ->
 *     sensor lag -> switching characteristic (same model as the Wokwi
 *     chip "lambda-probe", 1 ms time step)
 * (c) closed loop with a two-point controller with integrator
 *     (prediction for Exercise 2.4)
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine_signals.h"
#include "asciiplot.h"
#include "csvio.h"

/* ---------------------------------------------------------------- (a) */
#define TA     0.001         /* s, time step                              */
#define NA     3000          /* 3 s                                       */
#define TAU_N  0.4           /* s, time constant of the crankshaft speed  */
#define K_N    2000.0        /* rpm per unit fuel quantity                */

static double ta[NA], q_a[NA], n_eu[NA], n_ex[NA];

static void part_a(void)
{
    int k, k63 = -1;
    double a = exp(-TA / TAU_N);
    n_eu[0] = n_ex[0] = K_N * 1.0;                  /* steady state q = 1 */
    for (k = 0; k < NA; k++) {
        ta[k] = k * TA;
        q_a[k] = ta[k] < 0.5 ? 1.0 : 1.5;          /* fuel step at 0.5 s  */
        if (k == 0) continue;
        /* Euler:  n' = (K q - n) / tau                                     */
        n_eu[k] = n_eu[k - 1] + TA / TAU_N * (K_N * q_a[k - 1] - n_eu[k - 1]);
        /* exact discretisation (input constant over one step)              */
        n_ex[k] = a * n_ex[k - 1] + (1.0 - a) * K_N * q_a[k - 1];
    }
    for (k = 0; k < NA; k++)
        if (ta[k] >= 0.5 && n_ex[k] >= 2000.0 + 0.632 * 1000.0) { k63 = k; break; }
    printf("(a) fuel step 1.0 -> 1.5 at t = 0.5 s: n = %.0f -> %.1f rpm (t = 3 s)\n",
           n_ex[0], n_ex[NA - 1]);
    if (k63 >= 0)
        printf("    63%% point reached at t = %.3f s -> tau = %.3f s\n",
               ta[k63], ta[k63] - 0.5);
    printf("    n(0.6 s): Euler %.2f, exact %.2f, analytic %.2f rpm\n",
           n_eu[600], n_ex[600], 2000.0 + 1000.0 * (1.0 - exp(-0.1 / TAU_N)));
    printf("    difference equation: n[k] = %.6f n[k-1] + %.6f * K q[k-1]\n",
           a, 1.0 - a);
    ap_plot(n_ex, NA, "(a) engine speed [rpm], fuel step at t = 0.5 s, 0..3 s");
}

/* ---------------------------------------------------------- (b), (c) */
/* lambda path, identical to the Wokwi chip (1 ms step)                */
#define MAXD 1000
typedef struct {
    double air;              /* unmetered air, e.g. 0.05 = +5 %          */
    int    delay;            /* transport delay in ms                    */
    double qf, ls;           /* wall-film fuel, sensor state              */
    double buf[MAXD];
    int    w;
} lam_path_t;

static void lp_init(lam_path_t *p, double air, int delay_ms, double q0)
{
    int i;
    double lam0 = (1.0 + air) / q0;
    p->air = air; p->delay = delay_ms; p->qf = q0; p->ls = lam0; p->w = 0;
    for (i = 0; i < MAXD; i++) p->buf[i] = lam0;
}

/* one 1-ms step: input fuel quantity q (1 = stoichiometric without
   unmetered air), outputs true lambda and sensor voltage               */
static double lp_step(lam_path_t *p, double q, double *lam_true)
{
    double lam, lam_d;
    p->qf += (q - p->qf) / 20.0;                       /* wall film, 20 ms  */
    lam = (1.0 + p->air) / (p->qf > 0.05 ? p->qf : 0.05);   /* static, NL */
    if (lam > 2.0) lam = 2.0;
    if (lam < 0.5) lam = 0.5;
    p->buf[p->w] = lam;                                 /* transport delay */
    lam_d = p->buf[(p->w - p->delay + MAXD) % MAXD];
    p->w = (p->w + 1) % MAXD;
    p->ls += (lam_d - p->ls) / 30.0;                    /* sensor lag, 30 ms */
    *lam_true = lam;
    return es_lambda_voltage(p->ls);                    /* static, NL     */
}

#define NB 1000
static double ub[NB], lb[NB], ub2[NB];

static void part_b(void)
{
    lam_path_t p, p2;
    double lam, lam2;
    int k, kc = -1, kl = -1;
    lp_init(&p, 0.05, 200, 1.00);
    lp_init(&p2, 0.05, 200, 1.00);
    for (k = 0; k < NB; k++) {
        double q = k < 100 ? 1.00 : 1.10;               /* step at 0.1 s  */
        ub[k] = lp_step(&p, q, &lam);
        lb[k] = lam;
        ub2[k] = lp_step(&p2, k < 100 ? 1.00 : 1.20, &lam2);
        if (kl < 0 && lam < 1.0) kl = k;
        if (kc < 0 && ub[k] > 0.45) kc = k;
    }
    printf("\n(b) fuel step 1.00 -> 1.10 at t = 100 ms (air +5 %%, Td = 200 ms)\n");
    printf("    lambda %.4f -> %.4f, crosses 1 after %d ms (wall film)\n",
           lb[0], lb[NB - 1], kl - 100);
    printf("    sensor crosses 0.45 V after %d ms\n", kc - 100);
    for (k = 0; k < NB; k++) if (ub2[k] > 0.45) break;
    printf("    with step 1.00 -> 1.20 (double size): crosses after %d ms, "
           "final U = %.3f V (step 1.10: %.3f V)\n", k - 100, ub2[NB - 1], ub[NB - 1]);
    ap_plot2(lb, ub, NB, "(b) true lambda (*) and sensor voltage (o), 0..1 s");
}

/* closed loop: two-point controller with integrator (+ optional P jump),
   controller period 10 ms, 8-bit PWM like analogWrite on the Uno       */
static void closed_loop(int delay_ms, double rate, double pjump, double air,
                        int plot)
{
    static double tq[3000], tl[3000], tu[3000], tt[3000];
    lam_path_t p;
    double q = 1.0, qpwm = 1.0, u = 0.5, lam = 1.0, lmin = 9, lmax = 0,
           qsum = 0.0, t_first = -1, t_last = -1;
    int k, ncross = 0, rich_old = 0, nq = 0;
    lp_init(&p, air, delay_ms, 1.0);
    for (k = 0; k < 30000; k++) {                       /* 30 s */
        if (k % 10 == 0) {                              /* controller */
            int rich = u > 0.45;
            if (rich != rich_old) q += rich ? -pjump : pjump;
            q += (rich ? -rate : rate) * 0.010;
            if (q < 0.5) q = 0.5;
            if (q > 1.5) q = 1.5;
            qpwm = 2.0 * floor(255.0 * q / 2.0 + 0.5) / 255.0;
            if (k >= 10000 && rich && !rich_old) {     /* lean->rich switch */
                if (t_first < 0) t_first = k;
                t_last = k; ncross++;
            }
            rich_old = rich;
        }
        u = lp_step(&p, qpwm, &lam);
        if (k >= 10000) {                               /* after 10 s */
            if (lam < lmin) lmin = lam;
            if (lam > lmax) lmax = lam;
            qsum += qpwm; nq++;
        }
        if (plot && k >= 10000 && k < 13000) {
            int i = k - 10000;
            tt[i] = k * 1e-3; tq[i] = qpwm; tl[i] = lam; tu[i] = u;
        }
    }
    printf("    Td = %3d ms, r = %.2f/s, P = %.3f, air %+3.0f%%: period %.3f s, "
           "lambda %.3f..%.3f, mean q %.4f\n", delay_ms, rate, pjump, air * 100,
           ncross > 1 ? (t_last - t_first) * 1e-3 / (ncross - 1) : 0.0,
           lmin, lmax, qsum / nq);
    if (plot) {
        ap_plot2(tl, tu, 3000, "(c) lambda (*) and sensor voltage (o), t = 10..13 s");
        csv_write("lambda_loop.csv", "t,q,lambda,u", 3000, 4, tt, tq, tl, tu);
        printf("    wrote lambda_loop.csv\n");
    }
}

/* air step +5 % -> new value at t = 5 s: time until the sensor reports
   lean again (end of the transient) and new mean q (solution only)    */
static void air_step(double air_new)
{
    lam_path_t p;
    double q = 1.0, qpwm = 1.0, u = 0.5, lam, qsum = 0.0;
    int k, rich_old = 0, nq = 0, k_lean = -1, k_rich = -1;
    lp_init(&p, 0.05, 200, 1.0);
    for (k = 0; k < 20000; k++) {
        if (k == 5000) p.air = air_new;
        if (k % 10 == 0) {
            int rich = u > 0.45;
            q += (rich ? -0.2 : 0.2) * 0.010;
            qpwm = 2.0 * floor(255.0 * q / 2.0 + 0.5) / 255.0;
            if (k > 5000 && k_rich < 0 && rich) k_rich = k;
            if (k > 5000 && k_rich > 0 && k_lean < 0 && !rich) k_lean = k;
            rich_old = rich;
        }
        u = lp_step(&p, qpwm, &lam);
        if (k >= 10000) { qsum += qpwm; nq++; }
    }
    (void)rich_old;
    printf("    air +5%% -> %+.0f%% at t = 5 s: first rich report after %d ms, "
           "back to lean after %d ms, mean q (t > 10 s) = %.4f\n",
           air_new * 100, k_rich - 5000, k_lean - 5000, qsum / nq);
}

int main(void)
{
    int d;
    static const int delays[] = {50, 100, 200, 400, 800};
    part_a();
    part_b();
    printf("\n(c) closed loop, two-point controller with integrator\n");
    closed_loop(200, 0.2, 0.0, 0.05, 1);
    for (d = 0; d < 5; d++) {
        closed_loop(delays[d], 0.2, 0.0, 0.05, 0);
        printf("        prediction 4 (Td + 50 ms) = %.3f s, lambda amplitude ~ "
               "r (Td + 50 ms) = %.3f\n", 4e-3 * (delays[d] + 50),
               0.2 * 1e-3 * (delays[d] + 50));
    }
    printf("  unmetered air 0 %% / +10 %% (Td = 200 ms):\n");
    closed_loop(200, 0.2, 0.0, 0.00, 0);
    closed_loop(200, 0.2, 0.0, 0.10, 0);
    printf("  with P jump (Td = 200 ms):\n");
    closed_loop(200, 0.2, 0.03, 0.05, 0);
    closed_loop(200, 0.1, 0.025, 0.05, 0);
    printf("  air steps (Td = 200 ms, r = 0.2/s):\n");
    air_step(-0.10);
    air_step(0.10);
    return 0;
}
