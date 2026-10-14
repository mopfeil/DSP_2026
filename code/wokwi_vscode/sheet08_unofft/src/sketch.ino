/*
 * Sheet 8, Exercise 8.4 d) -- How large can an FFT be on the Arduino Uno?
 *                             (TEMPLATE)
 *
 * Project files: sketch.ino, diagram.json, fft.h (same file as on JSLinux;
 * on the AVR "double" is a 32-bit float, so re[] and im[] need 8 bytes
 * per complex sample).
 *
 * The sketch synthesises a test signal (6.5 kHz + 0.5 * 10.5 kHz at a
 * nominal fs = 40 kHz -- the Uno ADC cannot sample that fast), computes the
 * N-point FFT, prints the strongest bins, the free RAM and the operation
 * count. Change N to 64, 128, 256 and compile: what happens?
 *
 * micros() is printed for orientation only -- Wokwi is not cycle-accurate
 * for every peripheral; compare operation counts instead.
 */
#include "fft.h"

#define N   128                 /* try 64, 128, 256                */
#define FS  40000.0

double re[N], im[N];            /* 2 * 4 * N bytes of RAM          */

/* free RAM between heap and stack (classic AVR trick) */
extern int __heap_start, *__brkval;
static int free_ram(void)
{
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup()
{
    int n, k, stages = 0;
    unsigned long t0, t1;
    Serial.begin(115200);

    for (n = 0; n < N; n++) {
        re[n] = sin(2.0 * M_PI * 6500.0 * n / FS) + 0.5 * sin(2.0 * M_PI * 10500.0 * n / FS);
        im[n] = 0.0;
    }
    for (k = N; k > 1; k >>= 1) stages++;

    t0 = micros();
    fft(re, im, N, 0);
    t1 = micros();

    Serial.print(F("N = ")); Serial.print(N);
    Serial.print(F(", sizeof(double) = ")); Serial.print(sizeof(double));
    Serial.print(F(", arrays = ")); Serial.print(2 * N * sizeof(double));
    Serial.print(F(" bytes, free RAM = ")); Serial.println(free_ram());
    /* TODO (d): print the number of butterflies and of real multiplications
       and additions of the radix-2 FFT (use 'stages' = log2 N) */
    Serial.print(F("micros() for the FFT (indicative only): ")); Serial.println(t1 - t0);

    Serial.println(F("k,f_hz,mag"));
    for (k = 0; k <= N / 2; k++) {
        double m = 2.0 * sqrt(re[k] * re[k] + im[k] * im[k]) / N;
        if (m > 0.1) {                       /* print strong bins only */
            Serial.print(k); Serial.print(',');
            Serial.print(k * FS / N, 1); Serial.print(',');
            Serial.println(m, 3);
        }
    }
}

void loop() { }
