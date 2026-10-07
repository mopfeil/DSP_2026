/*
 * Sheet 5, Exercise 5.2 -- Aliasing made visible (ESP32)
 * (student template -- complete the parts marked TODO)
 *
 * Same measurement as on the Uno, but with fs = 25 kHz: the ESP32 samples
 * the KNOCK output of the engine-sim chip (GPIO 34, ADC1) in a deadline
 * loop with micros(). KNK (GPIO 27) and TDC (GPIO 26, interrupt) are stored
 * with every sample. TS_US is a multiple of the chip update period 20 us.
 *   TS_US = 40 -> 25 kHz   (default, Nyquist 12.5 kHz > 10.5 kHz)
 *   TS_US = 60 -> 16.7 kHz (10.5 kHz aliases to 6.17 kHz!)
 * Output: blocks of NBUF samples as CSV "n,adc,knk,tdc"; a summary line
 * reports missed deadlines (must be 0, otherwise increase TS_US).
 */
#define TS_US     40UL
#define NBUF      2500          /* 100 ms at 25 kHz                  */
#define PIN_KNOCK 34
#define PIN_KNK   27
#define PIN_TDC   26
#define PIN_MARK  2             /* toggles at every sample (logic D0) */

static uint16_t buf[NBUF];      /* bits 0..11 ADC, 14 KNK, 15 TDC     */
volatile uint8_t tdc_seen = 0;

void IRAM_ATTR on_tdc() { tdc_seen = 1; }

void setup()
{
    Serial.begin(115200);
    analogReadResolution(12);
    pinMode(PIN_KNK, INPUT);
    pinMode(PIN_TDC, INPUT);
    pinMode(PIN_MARK, OUTPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_TDC), on_tdc, RISING);
    Serial.print("# Sheet 5.2 ESP32: fs = ");
    Serial.print(1000000.0 / TS_US, 1);
    Serial.println(" Hz");
}

void loop()
{
    uint32_t late = 0, t_next;
    int i, mark = 0;

    /* ---- acquisition: deadline loop, no printing inside ---- */
    t_next = micros() + TS_US;
    for (i = 0; i < NBUF; i++) {
        /* TODO (a): wait until the deadline t_next is reached, count the
           sample as "late" if we are more than TS_US/2 behind, and move
           the deadline by TS_US (no drift: never use t_next = micros()) */
        (void)t_next;
        digitalWrite(PIN_MARK, mark ^= 1);
        uint16_t v = analogRead(PIN_KNOCK);
        if (digitalRead(PIN_KNK)) v |= 0x4000;
        if (tdc_seen) { v |= 0x8000; tdc_seen = 0; }
        buf[i] = v;
    }

    /* ---- print the block ---- */
    Serial.println("n,adc,knk,tdc");
    for (i = 0; i < NBUF; i++) {
        Serial.print(i); Serial.print(',');
        Serial.print(buf[i] & 0x0FFF); Serial.print(',');
        Serial.print((buf[i] >> 14) & 1); Serial.print(',');
        Serial.println(buf[i] >> 15);
    }
    Serial.print("# late samples in this block: ");
    Serial.println(late);
    delay(100);
}
