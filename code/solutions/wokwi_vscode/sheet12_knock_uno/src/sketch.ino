/*
 * Sheet 12 -- Challenge: knock detector on the Arduino Uno in fixed point
 * (reference solution)
 *
 * Wiring (diagram.json): engine-sim (vmax 5 V, offset 2.5 V) KNOCK -> A0,
 * TDC -> D2 (INT0), CAM -> D4, KNK (ground truth) -> D5;
 * knock LEDs cylinder 1..4 -> D9..D12; ignition retard (PWM) -> D6;
 * D8 HIGH while the sampling ISR runs.
 *
 * Simplifications compared with the ESP32 version (Exercise 12.3):
 *   - crank sync from the TDC pulse (+ CAM for cylinder 1) instead of the
 *     60-2 decoder; window positions from the previous TDC period
 *   - single biquad band-pass kf_bp2_q14 (f0 = 6.5 kHz, B = 400 Hz), Q15
 *   - all arithmetic in integers: energy sum (y*y >> 8) in 32 bit,
 *     background EWMA with shifts (lambda = 1/16), threshold E > 2 B
 * Output per combustion: cyl,E,B,knock,truth ; every 200 combustions a
 * summary line "# ..." with the confusion counts.
 */
#include "knock_filter.h"

const int PIN_TDC = 2;     /* INT0 */
const int PIN_CAM = 4;
const int PIN_KNK = 5;     /* PORTD5 */
const int PIN_RETARD = 6;
const int PIN_BUSY = 8;    /* PORTB0 */
const int PIN_LED[4] = {9, 10, 11, 12};      /* cylinder 1..4 */

#define D_BP2   20         /* group delay of the biquad, samples           */
#define WARMUP  16
static const uint8_t cyl_no[4] = {1, 3, 4, 2};

/* ---------------- Q15 biquad (as in Sheet 10), saturating ------------ */
static int16_t bx1, bx2, by1, by2;
static inline int16_t biquad_q15(int16_t x)
{
    int32_t acc = (int32_t)kf_bp2_q14[0] * x + (int32_t)kf_bp2_q14[2] * bx2
                - (int32_t)kf_bp2_q14[3] * by1 - (int32_t)kf_bp2_q14[4] * by2;
    acc += 1L << 13;
    int16_t y = (int16_t)(((uint32_t)acc << 2) >> 16);
    if (acc >= (1L << 29)) y = 32767;
    else if (acc < -(1L << 29)) y = -32768;
    bx2 = bx1; bx1 = x;
    by2 = by1; by1 = y;
    return y;
}

/* ---------------- shared state (ISR <-> loop) ------------------------- */
volatile uint16_t n_since_tdc = 0;
volatile uint16_t w0 = 0, w1 = 0, wk = 0;   /* window, KNK-check limit    */
volatile uint8_t cyl_idx = 3, cyl_win = 3;
volatile uint32_t energy = 0;
volatile uint8_t knk_seen = 0, knk_win = 0;
volatile uint32_t result_e;
volatile uint8_t result_cyl, result_truth, result_ready = 0;

void on_tdc()
{
    uint16_t P = n_since_tdc;               /* samples per 180 deg        */
    n_since_tdc = 0;
    w0 = P / 18 + D_BP2;                    /* 10 deg + filter delay      */
    w1 = (uint16_t)(7UL * P / 18) + D_BP2;  /* 70 deg + filter delay      */
    wk = P / 2;                             /* 90 deg                     */
    cyl_idx = digitalRead(PIN_CAM) ? 0 : (uint8_t)((cyl_idx + 1) & 3);
    knk_seen = 0;
}

ISR(TIMER1_COMPA_vect)
{
    PORTB |= 1;
    int16_t adc = ADC;
    ADCSRA |= _BV(ADSC);
    uint16_t n = n_since_tdc;
    if (n < 65535) n_since_tdc = n + 1;
    int16_t y = biquad_q15((int16_t)((adc - 512) << 6));
    if (n < wk && (PIND & _BV(5))) knk_seen = 1;     /* ground truth  */
    if (n == w0) { energy = 0; cyl_win = cyl_idx; knk_win = 0; }
    if (n >= w0 && n < w1) {
        energy += (uint32_t)((int32_t)y * y) >> 8;
        knk_win |= knk_seen;
    } else if (n == w1 && w1 != 0) {
        result_e = energy;
        result_cyl = cyl_win;
        result_truth = knk_win | knk_seen;
        result_ready = 1;
    }
    PORTB &= ~1;
}

/* ---------------- detector in loop() ---------------------------------- */
static uint32_t B[4];
static uint8_t nB[4];
static uint8_t retard[4];                   /* in 0.5 deg steps           */
static unsigned long led_off[4];
static uint16_t tp = 0, fp = 0, fn = 0, tn = 0, ncomb = 0;

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_CAM, INPUT);
    pinMode(PIN_KNK, INPUT);
    pinMode(PIN_BUSY, OUTPUT);
    for (int i = 0; i < 4; i++) pinMode(PIN_LED[i], OUTPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_TDC), on_tdc, RISING);
    ADMUX = _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2);
    DIDR0 = _BV(ADC0D);
    ADCSRA |= _BV(ADSC);
    noInterrupts();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS10);
    OCR1A = 639;                             /* 25 kHz */
    TIMSK1 = _BV(OCIE1A);
    interrupts();
    Serial.println(F("cyl,E,B,knock,truth"));
}

void loop()
{
    if (!result_ready) {
        for (uint8_t i = 0; i < 4; i++)
            if (led_off[i] && (long)(millis() - led_off[i]) > 0) {
                digitalWrite(PIN_LED[cyl_no[i] - 1], LOW);
                led_off[i] = 0;
            }
        return;
    }
    noInterrupts();
    uint32_t E = result_e;
    uint8_t c = result_cyl, truth = result_truth;
    result_ready = 0;
    interrupts();

    uint8_t knock = (nB[c] >= WARMUP) && (E > (B[c] << 1));      /* E/B > 2 */
    /* background: clipped EWMA with shifts (1/4 during warm-up, then 1/16) */
    if (nB[c] == 0) {
        B[c] = E;
    } else {
        uint32_t v = E < 3 * B[c] ? E : 3 * B[c];
        int32_t d = (int32_t)(v - B[c]);
        B[c] += d >> (nB[c] < WARMUP ? 2 : 4);
    }
    if (nB[c] < 255) nB[c]++;
    if (knock) {
        if (retard[c] < 18) retard[c] += 3;  /* +1.5 deg, max 9 deg      */
        digitalWrite(PIN_LED[cyl_no[c] - 1], HIGH);
        led_off[c] = millis() + 100;
    } else if (retard[c] > 0 && (ncomb & 7) == 0) {
        retard[c]--;                         /* slow recovery            */
    }
    analogWrite(PIN_RETARD, (retard[0] + retard[1] + retard[2] + retard[3]) * 255 / 72);
    if (nB[c] > WARMUP) {
        ncomb++;
        if (knock && truth) tp++; else if (knock) fp++; else if (truth) fn++; else tn++;
    }
    Serial.print(cyl_no[c]); Serial.print(',');
    Serial.print(E); Serial.print(',');
    Serial.print(B[c]); Serial.print(',');
    Serial.print(knock); Serial.print(',');
    Serial.println(truth);
    if (ncomb > 0 && ncomb % 200 == 0 && nB[c] > WARMUP) {
        Serial.print(F("# ")); Serial.print(ncomb);
        Serial.print(F(" combustions: TP ")); Serial.print(tp);
        Serial.print(F(" FP ")); Serial.print(fp);
        Serial.print(F(" FN ")); Serial.print(fn);
        Serial.print(F(" TN ")); Serial.println(tn);
    }
}
