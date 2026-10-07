/*
 * Sheet 10 -- Exercise 10.3 b): Q15 biquad on the Arduino Uno
 * (reference solution)
 *
 * Wiring (diagram.json): engine-sim (vmax 5 V, offset 2.5 V) KNOCK -> A0,
 * TDC -> D2 (INT0), CAM -> D4; D8 is HIGH while the sampling ISR runs
 * (logic analyzer channel D0 -> ISR load).
 *
 * Timer1 (CTC, no prescaler) interrupts at fs = 16 MHz / 640 = 25 kHz.
 * The ISR reads the ADC result of the conversion started in the previous
 * ISR and starts the next one (ADC clock 1 MHz -> 13 us per conversion).
 *
 * Filter: single 2nd-order band-pass kf_bp2_q14 (f0 = 6.5 kHz, B = 400 Hz)
 * from knock_filter.h, direct form I, Q15 data, Q2.14 coefficients,
 * 32-bit accumulator, rounding, saturation (SATURATE 1) or wrap-around
 * (SATURATE 0) of the output.  Input scaling: x = (adc - 512) << SHIFT.
 *
 * MODE 0 (block test): 200 raw samples after the TDC of cylinder 1 are
 *   stored; loop() filters them with float and with Q15 arithmetic and
 *   prints n,adc,x_q15,y_float,y_q15 (y in Q15 units).
 * MODE 1 (real time): the ISR filters every sample and integrates y^2 over
 *   the window 10..70 deg ATDC; one CSV line per combustion:
 *   cyl,energy,n_ovf,isr_cycles  (energy = sum (y*y >> 8)).
 */
#include "knock_filter.h"

#define MODE      1        /* 0 = block test, 1 = real time            */
#define SHIFT     6        /* input scaling: 6 = full ADC range -> Q15 */
#define SATURATE  1        /* 1 = saturate, 0 = wrap around            */

const int PIN_TDC = 2;     /* INT0 */
const int PIN_CAM = 4;
const int PIN_BUSY = 8;    /* PORTB0 */

static const uint8_t cyl_no[4] = {1, 3, 4, 2};

/* ---------------- Q15 biquad, direct form I ------------------------- */
static int16_t bx1, bx2, by1, by2;          /* filter state            */
static volatile uint16_t n_ovf = 0;         /* overflow events         */

static inline int16_t biquad_q15(int16_t x)
{
    /* kf_bp2_q14 = {b0, b1, b2, a1, a2}, b1 = 0 for the band-pass     */
    int32_t acc = (int32_t)kf_bp2_q14[0] * x
                + (int32_t)kf_bp2_q14[2] * bx2
                - (int32_t)kf_bp2_q14[3] * by1
                - (int32_t)kf_bp2_q14[4] * by2;      /* Q29 */
    acc += 1L << 13;                                  /* rounding     */
    /* Q29 -> Q15: bits 14..29 of acc. (acc << 2) >> 16 needs only byte
       moves on the AVR, a loop ">> 14" would cost ~100 cycles.          */
    int16_t y = (int16_t)(((uint32_t)acc << 2) >> 16);  /* wraps around */
    if (acc >= (1L << 29) || acc < -(1L << 29)) {    /* |y| > 1: overflow */
        n_ovf++;
#if SATURATE
        y = acc > 0 ? 32767 : -32768;
#endif
    }
    bx2 = bx1; bx1 = x;
    by2 = by1; by1 = y;
    return y;
}

static inline int16_t scale_input(int16_t adc)
{
#if SHIFT <= 6
    return (int16_t)((adc - 512) << SHIFT);            /* always fits */
#else
    int32_t v = (int32_t)(adc - 512) << SHIFT;
    if (v > 32767) v = 32767;                         /* input saturation */
    if (v < -32768) v = -32768;
    return (int16_t)v;
#endif
}

/* ---------------- shared state ---------------------------------------- */
volatile uint16_t n_since_tdc = 0;          /* samples since last TDC    */
volatile uint16_t w0 = 0, w1 = 0;           /* window in samples (0: none) */
volatile uint8_t cyl_idx = 3;
volatile uint32_t energy = 0;
volatile uint32_t result_e;                 /* published per combustion  */
volatile uint8_t result_cyl, result_ready = 0;
volatile uint16_t isr_cycles_max = 0;

#if MODE == 0
#define NB 200
static int16_t blk[NB];
volatile int16_t nb = -1;                   /* -1: idle, NB: full        */
#endif

void on_tdc()
{
    uint16_t P = n_since_tdc;               /* samples per 180 deg       */
    n_since_tdc = 0;
    w0 = P / 18;                            /* 10 deg                    */
    w1 = (uint16_t)(7UL * P / 18);          /* 70 deg                    */
    cyl_idx = digitalRead(PIN_CAM) ? 0 : (uint8_t)((cyl_idx + 1) & 3);
    energy = 0;
#if MODE == 0
    if (nb < 0 && cyl_idx == 0) nb = 0;     /* start a block at cyl 1    */
#endif
}

ISR(TIMER1_COMPA_vect)
{
    PORTB |= 1;                             /* D8 HIGH: busy             */
    int16_t adc = ADC;                      /* result of last conversion */
    ADCSRA |= _BV(ADSC);                    /* start next conversion     */
    uint16_t n = n_since_tdc;
    if (n < 65535) n_since_tdc = n + 1;
#if MODE == 0
    if (nb >= 0 && nb < NB) blk[nb++] = adc;
#else
    int16_t y = biquad_q15(scale_input(adc));
    if (n >= w0 && n < w1) {
        energy += (uint32_t)((int32_t)y * y) >> 8;   /* byte shift */
    } else if (n == w1 && w1 != 0) {
        result_e = energy;
        result_cyl = cyl_idx;
        result_ready = 1;
    }
#endif
    uint16_t c = TCNT1;                     /* cycles since compare match */
    if (c > isr_cycles_max) isr_cycles_max = c;
    PORTB &= ~1;
}

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_CAM, INPUT);
    pinMode(PIN_BUSY, OUTPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_TDC), on_tdc, RISING);
    /* ADC: AVcc reference, channel A0, prescaler 16 -> 1 MHz ADC clock */
    ADMUX = _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2);
    DIDR0 = _BV(ADC0D);
    ADCSRA |= _BV(ADSC);
    /* Timer1 CTC, no prescaler, 25 kHz */
    noInterrupts();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS10);
    OCR1A = 639;
    TIMSK1 = _BV(OCIE1A);
    interrupts();
#if MODE == 0
    Serial.println(F("n,adc,x_q15,y_float,y_q15"));
#else
    Serial.println(F("cyl,energy,n_ovf,isr_cycles"));
#endif
}

void loop()
{
#if MODE == 0
    if (nb == NB) {
        /* float reference and Q15 on the same block */
        float fx1 = 0, fx2 = 0, fy1 = 0, fy2 = 0;
        bx1 = bx2 = by1 = by2 = 0;
        for (int n = 0; n < NB; n++) {
            int16_t xq = scale_input(blk[n]);
            float x = xq, yf;
            yf = kf_bp2[0] * x + kf_bp2[2] * fx2 - kf_bp2[3] * fy1 - kf_bp2[4] * fy2;
            fx2 = fx1; fx1 = x; fy2 = fy1; fy1 = yf;
            int16_t yq = biquad_q15(xq);
            Serial.print(n); Serial.print(',');
            Serial.print(blk[n]); Serial.print(',');
            Serial.print(xq); Serial.print(',');
            Serial.print(yf, 1); Serial.print(',');
            Serial.println(yq);
        }
        Serial.print(F("# overflows: ")); Serial.println(n_ovf);
        delay(2000);
        nb = -1;                             /* arm for the next block */
    }
#else
    if (result_ready) {
        noInterrupts();
        uint32_t e = result_e;
        uint8_t c = result_cyl;
        uint16_t ov = n_ovf, cy = isr_cycles_max;
        result_ready = 0;
        interrupts();
        Serial.print(cyl_no[c]); Serial.print(',');
        Serial.print(e); Serial.print(',');
        Serial.print(ov); Serial.print(',');
        Serial.println(cy);
    }
#endif
}
