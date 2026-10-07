/*
 * Sheet 7, Exercise 7.2 -- Knock resonator: ODE solvers vs. Laplace
 *                          (REFERENCE SOLUTION)
 *
 * Build and run:   gcc -O2 -o resonator_ode resonator_ode.c -lm
 *                  ./resonator_ode
 * (asciiplot.h and csvio.h in the same directory)
 *
 * Second-order model of the knock path (engine block + sensor):
 *
 *            Y(s)          w0^2
 *   H(s) = ------ = -------------------- ,  sigma = 1/tau, wd = 2 pi f_d,
 *            U(s)    s^2 + 2 sigma s + w0^2         w0^2 = wd^2 + sigma^2
 *
 *   ODE:  y'' + 2 sigma y' + w0^2 y = w0^2 u
 *   state x = (y, y'):  x1' = x2,  x2' = -w0^2 x1 - 2 sigma x2 + w0^2 u
 *
 * The program
 *   (a) integrates the impulse and step response with forward Euler and
 *       classical Runge-Kutta (RK4),
 *   (b) compares with the analytic responses (inverse Laplace transform)
 *       for several step sizes h and estimates the order of convergence,
 *   (c) evaluates |H(j w)| and arg H(j w) on a logarithmic grid (Bode),
 *       finds the resonance peak and the -3 dB bandwidth.
 */
#include <stdio.h>
#include <math.h>
#include "asciiplot.h"
#include "csvio.h"

#define F_D    6500.0      /* damped resonance frequency, Hz (knock mode 1) */
#define TAU    0.8e-3      /* decay time constant, s                        */
#define T_END  4.0e-3      /* simulated duration, s (5 tau)                 */
#define NMAX   70000       /* max. number of steps                          */

static double sig, wd, w0;

/* ------------------------------------------------------------ (a) */
/* right-hand side of the state equation x' = f(x, u) */
static void rhs(const double *x, double u, double *dx)
{
    dx[0] = x[1];
    dx[1] = -w0 * w0 * x[0] - 2.0 * sig * x[1] + w0 * w0 * u;
}

static void euler_step(double *x, double u, double h)
{
    double k[2];
    rhs(x, u, k);
    x[0] += h * k[0];
    x[1] += h * k[1];
}

static void rk4_step(double *x, double u, double h)
{
    double k1[2], k2[2], k3[2], k4[2], xt[2];
    int i;
    rhs(x, u, k1);
    for (i = 0; i < 2; i++) xt[i] = x[i] + 0.5 * h * k1[i];
    rhs(xt, u, k2);
    for (i = 0; i < 2; i++) xt[i] = x[i] + 0.5 * h * k2[i];
    rhs(xt, u, k3);
    for (i = 0; i < 2; i++) xt[i] = x[i] + h * k3[i];
    rhs(xt, u, k4);
    for (i = 0; i < 2; i++)
        x[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
}

/* ------------------------------------------------------------ (b) */
/* analytic impulse response h(t) = w0^2/wd e^{-sigma t} sin(wd t) */
static double h_exact(double t)
{
    return w0 * w0 / wd * exp(-sig * t) * sin(wd * t);
}

/* analytic step response */
static double step_exact(double t)
{
    return 1.0 - exp(-sig * t) * (cos(wd * t) + sig / wd * sin(wd * t));
}

/* simulate impulse (impulse=1) or step response with step size h,
   method 0 = Euler, 1 = RK4; returns max abs error relative to the peak
   of the exact response; stores the response in y (if y != NULL) */
static double simulate(int method, int impulse, double h, double *y, int *n_out)
{
    double x[2], err = 0.0, peak = 0.0, u, ye;
    int n, N = (int)floor(T_END / h + 0.5);
    if (N > NMAX) N = NMAX;
    /* impulse w0^2 delta(t) at the input sets y'(0+) = w0^2 */
    x[0] = 0.0;
    x[1] = impulse ? w0 * w0 : 0.0;
    u = impulse ? 0.0 : 1.0;
    for (n = 0; n <= N; n++) {
        double t = n * h;
        ye = impulse ? h_exact(t) : step_exact(t);
        if (fabs(ye) > peak) peak = fabs(ye);
        if (fabs(x[0] - ye) > err) err = fabs(x[0] - ye);
        if (y && n < NMAX) y[n] = x[0];
        if (method == 0) euler_step(x, u, h); else rk4_step(x, u, h);
    }
    if (n_out) *n_out = N + 1;
    return err / peak;
}

/* ------------------------------------------------------------ (c) */
/* frequency response H(j w): magnitude in dB and phase in degrees */
static void freq_resp(double f, double *mag_db, double *ph_deg)
{
    double w = 2.0 * M_PI * f;
    double re = w0 * w0 - w * w, im = 2.0 * sig * w;   /* denominator */
    *mag_db = 20.0 * log10(w0 * w0 / sqrt(re * re + im * im));
    *ph_deg = -atan2(im, re) * 180.0 / M_PI;
}

int main(void)
{
    static double y1[NMAX], y2[NMAX], ye[NMAX];
    static double fl[400], mdb[400], ph[400], lf[400];
    double hs[] = {40e-6, 20e-6, 10e-6, 5e-6, 2.5e-6, 1.25e-6, 0.625e-6};
    int nh = 7, i, n, N;
    double eE_prev = 0, eR_prev = 0;

    sig = 1.0 / TAU;
    wd = 2.0 * M_PI * F_D;
    w0 = sqrt(wd * wd + sig * sig);
    printf("Knock resonator: sigma = %.1f 1/s, wd = %.1f rad/s, w0 = %.1f rad/s\n",
           sig, wd, w0);
    printf("  zeta = %.5f, Q = %.2f, f0 = %.1f Hz, poles -%.1f +- j%.1f\n",
           sig / w0, w0 / (2 * sig), w0 / (2 * M_PI), sig, wd);
    printf("  Euler stability limit h < 2 sigma / w0^2 = %.3f us\n",
           2.0 * sig / (w0 * w0) * 1e6);

    /* (b) error table */
    printf("\nmax. error relative to peak (impulse response, 0..%.0f ms)\n",
           T_END * 1e3);
    printf("   h [us]     Euler        order   RK4          order\n");
    for (i = 0; i < nh; i++) {
        double eE = simulate(0, 1, hs[i], NULL, NULL);
        double eR = simulate(1, 1, hs[i], NULL, NULL);
        printf("  %7.3f   %11.3e", hs[i] * 1e6, eE);
        if (i > 0 && eE < 1.0 && eE_prev < 1.0) printf("  %5.2f", log(eE_prev / eE) / log(2.0));
        else printf("     - ");
        printf("   %11.3e", eR);
        if (i > 0 && eR_prev > 1e-13) printf("  %5.2f", log(eR_prev / eR) / log(2.0));
        printf("\n");
        eE_prev = eE; eR_prev = eR;
    }
    printf("\nmax. error relative to final value (step response)\n");
    printf("   h [us]     Euler        RK4\n");
    for (i = 0; i < nh; i++)
        printf("  %7.3f   %11.3e  %11.3e\n", hs[i] * 1e6,
               simulate(0, 0, hs[i], NULL, NULL), simulate(1, 0, hs[i], NULL, NULL));

    /* plots: RK4 with h = 20 us (fs = 50 kHz) vs exact; Euler with 1.25 us */
    simulate(1, 1, 20e-6, y1, &N);
    for (n = 0; n < N; n++) ye[n] = h_exact(n * 20e-6);
    ap_plot2(ye, y1, N, "impulse response: exact (*) vs RK4 h = 20 us (o), 0..4 ms");
    simulate(1, 0, 20e-6, y2, &N);
    ap_plot(y2, N, "step response (RK4, h = 20 us), 0..4 ms");
    {
        static double t[NMAX];
        for (n = 0; n < N; n++) t[n] = n * 20e-6;
        csv_write("resonator_ode.csv", "t,h_exact,h_rk4,step_rk4", N, 4, t, ye, y1, y2);
    }
    simulate(0, 1, 1.25e-6, y1, &N);
    ap_plot(y1, N, "impulse response, forward Euler, h = 1.25 us (stable but wrong)");

    /* (c) Bode diagram on a logarithmic grid 100 Hz .. 100 kHz */
    {
        int K = 361, kpk = 0;
        double flo = -1.0, fhi = -1.0;
        for (i = 0; i < K; i++) {
            fl[i] = 100.0 * pow(10.0, 3.0 * i / (K - 1));
            lf[i] = log10(fl[i]);
            freq_resp(fl[i], &mdb[i], &ph[i]);
            if (mdb[i] > mdb[kpk]) kpk = i;
        }
        /* refine peak and -3 dB points by bisection-free fine search */
        {
            double f, m, p, best = -1e9, fpk = 0;
            for (f = 6000.0; f < 7000.0; f += 0.1) {
                freq_resp(f, &m, &p);
                if (m > best) { best = m; fpk = f; }
            }
            for (f = 6000.0; f < 7000.0; f += 0.1) {
                freq_resp(f, &m, &p);
                if (flo < 0 && m >= best - 3.0103) flo = f;
                if (flo > 0 && fhi < 0 && f > fpk && m < best - 3.0103) fhi = f;
            }
            printf("\nBode: peak %.2f dB at %.1f Hz, -3 dB band %.1f..%.1f Hz, B = %.1f Hz\n",
                   best, fpk, flo, fhi, fhi - flo);
            printf("      (sigma/pi = %.1f Hz)\n", sig / M_PI);
        }
        printf("   f [Hz]    |H| [dB]   phase [deg]\n");
        {
            double fs_[] = {100, 1000, 3000, 6000, 6500, 7000, 10500, 20000, 100000};
            for (i = 0; i < 9; i++) {
                double m, p;
                freq_resp(fs_[i], &m, &p);
                printf("  %8.0f   %8.2f   %8.1f\n", fs_[i], m, p);
            }
        }
        ap_plot_xy(lf, mdb, K, "|H(jw)| in dB over log10(f/Hz)");
        ap_plot_xy(lf, ph, K, "phase in degrees over log10(f/Hz)");
        csv_write("resonator_bode.csv", "f,mag_db,phase_deg", K, 3, fl, mdb, ph);
    }
    return 0;
}
