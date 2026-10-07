/*
 * Sheet 5, Exercise 5.2 -- Aliasing made visible (Arduino Uno)
 * (student template -- complete the parts marked TODO)
 *
 * The Uno samples the KNOCK output of the engine-sim chip (A0) with a
 * Timer1 interrupt. Together with every sample the ground-truth outputs of
 * the chip are stored:  KNK (pin 3, HIGH during a knock burst) and TDC
 * (pin 2, INT0, flags the first sample after a combustion TDC).
 *
 * Choice of fs: the chip updates its analog output every 20 us (attribute
 * fs_khz = 50, zero-order hold). The sampling period is therefore chosen as
 * an integer multiple DIV of 20 us, so every ADC sample sees exactly one
 * chip update -- no additional "sampling-phase jitter" (see Ex. 5.2 d).
 *   DIV = 10 -> 200 us, fs = 5 kHz       (default)
 *   DIV =  8 -> 160 us, fs = 6.25 kHz
 *   DIV =  6 -> 120 us, fs = 8.33 kHz    (close to the Uno limit: one
 *                                          analogRead takes ~112 us)
 * Output: blocks of NBUF samples as CSV  "n,adc,knk,tdc"  -> analyse with
 * jslinux/sheet05/alias_scan.c
 */
#define DIV     10                       /* sampling period = DIV * 20 us */
#define TS_US   (20UL * DIV)
#define NBUF    512                      /* 1 KB of the 2 KB RAM          */

volatile uint16_t buf[NBUF];   /* bits 0..9 ADC, bit 14 KNK, bit 15 TDC   */
volatile uint16_t nbuf = 0;
volatile bool done = false;
volatile uint8_t tdc_seen = 0;

void on_tdc() { tdc_seen = 1; }

ISR(TIMER1_COMPA_vect)
{
    PINB = _BV(PB0);                         /* toggle pin 8: sample marker */
    uint16_t v = analogRead(A0);
    /* TODO (b): set bit 14 of v if KNK (pin 3, PIND bit PD3) is HIGH, set
       bit 15 if a TDC was seen since the last sample (clear tdc_seen)    */
    buf[nbuf++] = v;
    if (nbuf >= NBUF) { TIMSK1 = 0; done = true; }
}

void timer1_start()
{
    noInterrupts();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11);         /* CTC, clk/8 = 2 MHz          */
    OCR1A  = 0;    /* TODO (a): compare value for the period TS_US        */
    TCNT1  = 0;
    TIFR1  = _BV(OCF1A);
    TIMSK1 = _BV(OCIE1A);
    TIMSK0 = 0;           /* stop the millis() interrupt: no extra jitter  */
    interrupts();
}

void timer1_stop()
{
    TIMSK1 = 0;
    TIMSK0 = _BV(TOIE0);                     /* millis() again              */
}

void setup()
{
    Serial.begin(115200);
    pinMode(8, OUTPUT);
    pinMode(3, INPUT);
    pinMode(2, INPUT);
    attachInterrupt(digitalPinToInterrupt(2), on_tdc, RISING);
    Serial.print(F("# Sheet 5.2 Uno: fs = "));
    Serial.print(1000000.0 / TS_US, 1);
    Serial.println(F(" Hz"));
}

void loop()
{
    uint16_t i;
    nbuf = 0; done = false; tdc_seen = 0;
    timer1_start();
    uint32_t guard = 0;                 /* safety: timer not running?     */
    while (!done && ++guard < 3000000UL) { }
    timer1_stop();
    if (!done) { Serial.println(F("# timeout: Timer1 not running")); delay(1000); return; }

    Serial.println(F("n,adc,knk,tdc"));
    for (i = 0; i < NBUF; i++) {
        Serial.print(i); Serial.print(',');
        Serial.print(buf[i] & 0x03FF); Serial.print(',');
        Serial.print((buf[i] >> 14) & 1); Serial.print(',');
        Serial.println(buf[i] >> 15);
    }
    delay(100);
}
