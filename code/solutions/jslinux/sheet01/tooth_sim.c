/*
 * Sheet 1, Exercise 1.3 -- offline model of the Wokwi tooth-timing
 * measurement (instructor tool, used to compute the expected values
 * quoted in the solution).
 *
 * Build and run:   gcc -O2 -o tooth_sim tooth_sim.c -lm
 *                  ./tooth_sim [rpm] [tick_us]      e.g. ./tooth_sim 3000 4
 *
 * The rising crank edges of the virtual engine (incl. the 1 % speed
 * ripple) are located with sub-microsecond accuracy, time-stamped with a
 * timer of resolution tick_us (4 us = Arduino micros(), 0.5 us = Timer1
 * with prescaler 8), and processed exactly as in the sketch: tooth
 * period, gap detection (ratio > 2), tooth counter, per-tooth rpm.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "engine_signals.h"

#define NREV 200

int main(int argc, char **argv)
{
    double rpm = argc > 1 ? atof(argv[1]) : 3000.0;
    double tick = (argc > 2 ? atof(argv[2]) : 4.0) * 1e-6;
    double dt = 1e-6, th_prev, t_prev;
    es_engine_t e;
    long stamp, last = -1, T, Tprev = 0;
    int tooth = -1, synced = 0, revs = 0, nper = 0, nbad = 0;
    double s = 0, s2 = 0, mn = 1e9, mx = 0, rmin_gap = 1e9, rmax_gap = 0,
           rmax_norm = 0, rpm_k;
    long cnt = 0;
    long rev_start = -1;
    double rr, rr_min = 1e9, rr_max = 0;

    es_init(&e, rpm, 99u);
    while (revs < NREV) {
        th_prev = e.theta; t_prev = e.t;
        es_advance(&e, dt);
        {   /* rising edge at 6k deg (k = 0..57) inside (th_prev, theta] ? */
            double a = fmod(th_prev, 360.0), b = a + (e.theta - th_prev);
            double edge;
            int k;
            if (b < a) b += 720.0;                    /* wrap at 720 */
            k = (int)floor(a / 6.0) + 1;
            edge = 6.0 * k;
            if (edge <= b && (k % 60) < 58) {
                double tedge = t_prev + dt * (edge - a) / (b - a);
                stamp = (long)floor(tedge / tick);    /* timer value */
                if (last >= 0) {
                    T = stamp - last;
                    if (Tprev > 0 && T > 2 * Tprev) {          /* gap */
                        double ratio = (double)T / Tprev;
                        if (synced) {
                            if (tooth != 57) nbad++;
                            revs++;
                            if (rev_start >= 0 && revs > 2) {
                                rr = 60.0 / ((stamp - rev_start) * tick);
                                if (rr < rr_min) rr_min = rr;
                                if (rr > rr_max) rr_max = rr;
                            }
                            if (ratio < rmin_gap) rmin_gap = ratio;
                            if (ratio > rmax_gap) rmax_gap = ratio;
                        }
                        synced = 1;
                        rev_start = stamp;
                        tooth = 0;
                    } else if (synced) {
                        double ratio = (double)T / Tprev;
                        tooth++;
                        if (ratio > rmax_norm) rmax_norm = ratio;
                        rpm_k = 1.0 / (T * tick);     /* rpm = 1/T[s] */
                        if (revs >= 2) {              /* settle */
                            s += rpm_k; s2 += rpm_k * rpm_k; cnt++;
                            if (rpm_k < mn) mn = rpm_k;
                            if (rpm_k > mx) mx = rpm_k;
                        }
                        nper++;
                    }
                    Tprev = T;
                }
                last = stamp;
            }
        }
    }
    s /= cnt;
    printf("rpm %.0f, tick %.2f us: per-tooth rpm mean %.1f std %.1f "
           "min %.1f max %.1f (%ld teeth)\n", rpm, tick * 1e6, s,
           sqrt(s2 / cnt - s * s), mn, mx, cnt);
    printf("  per-revolution rpm %.2f..%.2f\n", rr_min, rr_max);
    printf("  gap ratio %.3f..%.3f, max normal ratio %.4f, "
           "revolutions with tooth count != 58: %d\n",
           rmin_gap, rmax_gap, rmax_norm, nbad);
    printf("  rpm quantisation step n^2*tick = %.1f rpm, ripple amplitude %.1f rpm\n",
           rpm * rpm * tick, 0.01 * rpm);
    return 0;
}
