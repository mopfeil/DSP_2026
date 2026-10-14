/*
 * Sheet 9, Exercise 9.3 e) -- Two-pole resonator at the knock frequency
 *                                on the Arduino Uno: float vs. Q15
 *                                (TEMPLATE: complete resonator.h first)
 *
 * Project files: sketch.ino, diagram.json, resonator.h
 * Controls:  potentiometer (A0)  pole radius r = 0.90 ... 1.00
 *            switch SW1 (D3)     HIGH = rounding,   LOW = truncation
 *            switch SW2 (D4)     HIGH = saturation, LOW = wrap-around
 *            button (D2)         start the next test
 * Tests (cyclic):
 *   T0  impulse response, x = 0.5 delta, gain-1 resonator:  n,y_float,y_q15
 *       (both in LSB of Q15, i.e. y * 32768)
 *   T1  zero input, initial state y[-1] = 0.5, 200000 steps (5 s of
 *       signal at fs = 40 kHz): amplitude and period of the limit cycle
 *   T2  overflow: b0 = 1 (no normalisation), knock burst
 *       0.1 e^{-t/0.8 ms} sin(2 pi 6.5 kHz t) as input, 40000 steps
 * The signal is synthetic (fs = 40 kHz is far beyond the Uno ADC); the
 * computation is not real-time -- we study the arithmetic, not the speed.
 */
#include "resonator.h"

#define FS 40000.0f
#define F0 6500.0f

const int PIN_POT = A0, PIN_BTN = 2, PIN_RND = 3, PIN_SAT = 4;
int test = 0;

float read_r(void)
{
    /* 0.9000 ... 1.0000 in steps of about 1e-4 */
    return 0.9f + 0.1f * analogRead(PIN_POT) / 1023.0f;
}

void print_header(const char *name, float r, int norm, int rnd, int sat)
{
    float b0, a1, a2, rq, fq;
    res_q_t q;
    res_design(F0, FS, r, norm, &b0, &a1, &a2);
    res_q_init(&q, b0, a1, a2, rnd, sat);
    res_q_poles(&q, FS, &rq, &fq);
    Serial.print(F("# ")); Serial.print(name);
    Serial.print(F("  r=")); Serial.print(r, 4);
    Serial.print(rnd ? F(" round") : F(" trunc"));
    Serial.println(sat ? F(" saturate") : F(" wrap"));
    Serial.print(F("# float b0,a1,a2 = ")); Serial.print(b0, 6); Serial.print(',');
    Serial.print(a1, 6); Serial.print(','); Serial.println(a2, 6);
    Serial.print(F("# Q14   b0,a1,a2 = ")); Serial.print(q.b0); Serial.print(',');
    Serial.print(q.a1); Serial.print(','); Serial.println(q.a2);
    Serial.print(F("# quantised poles: r_q = ")); Serial.print(rq, 6);
    Serial.print(F(", f_q = ")); Serial.print(fq, 1); Serial.println(F(" Hz"));
}

/* T0: impulse response float vs Q15 */
void test_impulse(float r, int rnd, int sat)
{
    res_f_t f;
    res_q_t q;
    float b0, a1, a2;
    int n;
    print_header("T0 impulse response", r, 1, rnd, sat);
    res_design(F0, FS, r, 1, &b0, &a1, &a2);
    res_f_init(&f, b0, a1, a2);
    res_q_init(&q, b0, a1, a2, rnd, sat);
    Serial.println(F("n,y_float,y_q15"));
    for (n = 0; n < 150; n++) {
        float yf = res_f_step(&f, n == 0 ? 0.5f : 0.0f);
        int16_t yq = res_q_step(&q, n == 0 ? 16384 : 0);
        Serial.print(n); Serial.print(',');
        Serial.print(yf * 32768.0f, 2); Serial.print(',');
        Serial.println(yq);
    }
}

/* T1: zero-input limit cycle */
void test_limit_cycle(float r, int rnd, int sat)
{
    res_q_t q;
    float b0, a1, a2;
    uint32_t n, N = 200000UL;
    int16_t y, yprev = 0, mx = 0;
    int zc = 0;
    print_header("T1 zero-input limit cycle", r, 1, rnd, sat);
    res_design(F0, FS, r, 1, &b0, &a1, &a2);
    res_q_init(&q, b0, a1, a2, rnd, sat);
    q.y1 = 16384;                         /* initial state 0.5, x = 0 */
    for (n = 0; n < N; n++) {
        y = res_q_step(&q, 0);
        if (n >= N - 1000) {             /* observe the last 1000 samples */
            if (y > mx) mx = y;
            if (-y > mx) mx = -y;
            if ((y > 0 && yprev <= 0) || (y < 0 && yprev >= 0)) zc++;
        }
        yprev = y;
    }
    Serial.print(F("# after ")); Serial.print(N);
    Serial.print(F(" steps: amplitude = ")); Serial.print(mx);
    Serial.print(F(" LSB, zero crossings in 1000 samples = ")); Serial.print(zc);
    Serial.print(F(" -> f = ")); Serial.print(zc / 2.0f * FS / 1000.0f, 0);
    Serial.println(F(" Hz"));
    Serial.println(F("n,y_q15"));
    for (n = 0; n < 40; n++) {           /* 40 more samples of the cycle */
        Serial.print(n); Serial.print(',');
        Serial.println(res_q_step(&q, 0));
    }
}

/* T2: overflow with an unnormalised resonator */
void test_overflow(float r, int rnd, int sat)
{
    res_q_t q;
    float b0, a1, a2;
    uint32_t n;
    int16_t mx = 0;
    print_header("T2 overflow, b0 = 1, burst A = 0.1", r, 0, rnd, sat);
    res_design(F0, FS, r, 0, &b0, &a1, &a2);
    res_q_init(&q, b0, a1, a2, rnd, sat);
    Serial.println(F("n,x_q15,y_q15"));
    for (n = 0; n < 40000UL; n++) {
        float t = n / FS;
        float x = n < 400 ? 0.1f * expf(-t / 0.8e-3f) * sinf(2.0f * (float)M_PI * F0 * t) : 0.0f;
        int16_t xq = (int16_t)floorf(x * 32768.0f + 0.5f);
        int16_t y = res_q_step(&q, xq);
        if (n < 200 || (n >= 39960UL)) {
            Serial.print(n); Serial.print(',');
            Serial.print(xq); Serial.print(',');
            Serial.println(y);
        }
        if (n >= 39000UL && (y > mx || -y > mx)) mx = y > 0 ? y : -y;
    }
    Serial.print(F("# overflows = ")); Serial.print(q.novf);
    Serial.print(F(", max |y| in the last 1000 samples = ")); Serial.print(mx);
    Serial.println(F(" LSB"));
}

void run_test(void)
{
    float r = read_r();
    int rnd = digitalRead(PIN_RND), sat = digitalRead(PIN_SAT);
    if (test == 0) test_impulse(r, rnd, sat);
    else if (test == 1) test_limit_cycle(r, rnd, sat);
    else test_overflow(r, rnd, sat);
    Serial.println(F("# done -- press the button for the next test"));
    test = (test + 1) % 3;
}

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_RND, INPUT_PULLUP);
    pinMode(PIN_SAT, INPUT_PULLUP);
    Serial.println(F("# Sheet 9: two-pole resonator at 6.5 kHz, fs = 40 kHz"));
    run_test();
}

void loop()
{
    if (digitalRead(PIN_BTN) == LOW) {   /* button pressed */
        delay(30);                        /* debounce        */
        while (digitalRead(PIN_BTN) == LOW) { }
        run_test();
    }
}
