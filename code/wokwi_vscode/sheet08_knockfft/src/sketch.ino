/*
 * Sheet 8, Exercise 8.4 -- FFT of the knock signal on the ESP32
 *                          (TEMPLATE)
 *
 * Wiring (diagram.json): engine-sim chip  KNOCK -> D34 (ADC1),
 *                        TDC -> D18, KNK (ground truth) -> D19.
 * Project files: sketch.ino, diagram.json, fft.h (your FFT of Ex. 8.2),
 *                engine-sim.chip.c, engine-sim.chip.json
 *
 * After every rising edge of the TDC pulse the sketch samples N = 256
 * values of the knock signal at fs = 40 kHz (deadline loop with micros()),
 * i.e. a crank-angle window of N/fs * 6 * rpm degrees (115 deg at
 * 3000 rpm). It removes the mean, applies a Hann window, computes the FFT
 * and prints (peak = largest bin above F_MIN = 3 kHz)
 *   # win=<i> knk=<0/1> peak=<Hz> <dB> e_knock=<dB> late=<n> tcap=<us> tfft=<us>
 *   win,f_hz,db       (N/2+1 lines: amplitude spectrum, sensor volts)
 * Lines starting with '#' are ignored by csv_read() in JSLinux.
 *
 * Note: Wokwi is not cycle-accurate. tcap and tfft are simulated times and
 * only indicative; "late" counts samples whose deadline had already passed.
 */
#include "fft.h"

#define N        256            /* FFT length (power of two)          */
#define TS_US    25             /* sampling period -> fs = 40 kHz      */
#define FS       (1e6 / TS_US)
#define F_MIN    3000.0         /* peak search above the combustion hump */

const int PIN_KNOCK = 34;       /* ADC1 channel                        */
const int PIN_TDC   = 18;
const int PIN_KNK   = 19;
const double VREF = 3.3, OFFSET_GAIN = 0.6;   /* chip: offset + 0.6 * x  */

static uint16_t raw[N];
static double re[N], im[N], db[N / 2 + 1];
static unsigned long win = 0;

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_TDC, INPUT);
    pinMode(PIN_KNK, INPUT);
    analogReadResolution(12);
    Serial.println("# Sheet 8: knock FFT, N = 256, fs = 40 kHz, Hann window");
    Serial.println("win,f_hz,db");
}

void loop()
{
    int n, k, late = 0, knk = 0, kp;
    unsigned long t0, tn, tcap, tfft;
    double mean = 0.0, sw = 0.0, e_knock = 0.0;

    /* 1) wait for the rising edge of the TDC pulse */
    while (digitalRead(PIN_TDC) == HIGH) { }
    while (digitalRead(PIN_TDC) == LOW) { }

    /* 2) capture N samples with a deadline loop */
    t0 = tn = micros();
    for (n = 0; n < N; n++) {
        /* TODO (b): count missed deadlines in 'late', wait for the deadline
           tn, advance tn by TS_US, read the ADC into raw[n] and remember
           in 'knk' whether the KNK pin was HIGH during the window */
        raw[n] = 2048;
    }
    (void)tn;
    tcap = micros() - t0;

    /* 3) sensor volts, remove mean, Hann window */
    for (n = 0; n < N; n++) mean += raw[n];
    mean /= N;
    for (n = 0; n < N; n++) {
        double w = 1.0;   /* TODO (c): Hann window */
        re[n] = (raw[n] - mean) * (VREF / 4095.0) / OFFSET_GAIN * w;
        im[n] = 0.0;
        sw += w;
    }

    /* 4) FFT and amplitude spectrum in dB (sinusoid of amplitude A -> 20 log A) */
    t0 = micros();
    fft(re, im, N, 0);
    tfft = micros() - t0;
    kp = (int)(F_MIN * N / FS);
    for (k = 0; k <= N / 2; k++) {
        double a = 2.0 * sqrt(re[k] * re[k] + im[k] * im[k]) / sw;
        db[k] = 20.0 * log10(a + 1e-9);
        /* TODO (c): kp = index of the largest bin above F_MIN */
        /* energy in the knock band 6.0 .. 7.0 kHz */
        if (k * FS / N >= 6000.0 && k * FS / N <= 7000.0) e_knock += a * a;
    }

    /* 5) peak frequency with parabolic interpolation */
    double d = 0.0;
    if (kp < N / 2) {
        double a = db[kp - 1], b = db[kp], c = db[kp + 1];
        d = 0.0 * (a - b + c);   /* TODO (c): parabolic interpolation */
    }

    Serial.print("# win="); Serial.print(win);
    Serial.print(" knk="); Serial.print(knk);
    Serial.print(" peak="); Serial.print((kp + d) * FS / N, 0);
    Serial.print(" Hz "); Serial.print(db[kp], 1);
    Serial.print(" dB e_knock="); Serial.print(10.0 * log10(e_knock + 1e-12), 1);
    Serial.print(" dB late="); Serial.print(late);
    Serial.print(" tcap="); Serial.print(tcap);
    Serial.print(" tfft="); Serial.println(tfft);
    for (k = 0; k <= N / 2; k++) {
        Serial.print(win); Serial.print(',');
        Serial.print(k * FS / N, 1); Serial.print(',');
        Serial.println(db[k], 2);
    }
    win++;
}
