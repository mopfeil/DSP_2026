/*
 * Sheet 4, Exercise 4.2 -- Timer-driven ADC sampling (Arduino Uno)
 * (reference solution)
 *
 * Timer1 in CTC mode triggers an interrupt every 1/FS_HZ seconds. The ISR
 * samples the KNOCK output of the engine-sim chip (A1) into a RAM buffer.
 *   pin 8  toggles at every sample        -> logic analyzer D0
 *   pin 9  HIGH while the ISR is running  -> logic analyzer D1
 *   pin 2  (INT0) TDC pulses of the chip; the next sample is flagged
 * After NBUF samples the timer is stopped and the block is printed as CSV
 * "n,adc,tdc", followed by summary lines starting with '#':
 *   - potentiometer (A0) reading (a slow signal, read once per block)
 *   - standard deviation of the knock signal in a "quiet window"
 *     140..175 deg after each TDC (only sensor noise there)
 */
#define FS_HZ    5000UL       /* sampling rate, Hz                         */
#define NBUF     400          /* samples per block (800 bytes of 2 KB RAM)  */
#define RPM_NOM  3000.0       /* engine speed set with the chip slider      */
#define QW_DEG0  140.0        /* quiet window after TDC, degrees            */
#define QW_DEG1  175.0

volatile uint16_t buf[NBUF];  /* bit 15 = TDC flag, bits 0..9 = ADC code    */
volatile uint16_t nbuf = 0;
volatile bool done = false;
volatile uint8_t tdc_seen = 0;

void on_tdc() { tdc_seen = 1; }

/* ---------------- Timer1 compare-match interrupt: one sample ---------- */
ISR(TIMER1_COMPA_vect)
{
    PORTB |= _BV(PB1);                  /* pin 9 HIGH: ISR busy            */
    PINB = _BV(PB0);                    /* writing 1 to PINx toggles pin 8 */
    uint16_t v = analogRead(A1);        /* ~112 us on the Uno              */
    if (tdc_seen) { v |= 0x8000; tdc_seen = 0; }
    buf[nbuf++] = v;
    if (nbuf >= NBUF) { TIMSK1 = 0; done = true; }   /* block complete     */
    PORTB &= ~_BV(PB1);                 /* pin 9 LOW                       */
}

/* ---------------- (a) Timer1: CTC mode, prescaler 8 ------------------- */
void timer1_start()
{
    noInterrupts();
    TCCR1A = 0;                         /* no PWM outputs                  */
    TCCR1B = _BV(WGM12) | _BV(CS11);    /* CTC (TOP = OCR1A), clk/8 = 2 MHz */
    OCR1A  = F_CPU / 8 / FS_HZ - 1;     /* 16e6/8/5000 - 1 = 399           */
    TCNT1  = 0;
    TIFR1  = _BV(OCF1A);                /* clear a pending flag            */
    TIMSK1 = _BV(OCIE1A);               /* enable compare-match interrupt  */
    interrupts();
}

void setup()
{
    Serial.begin(115200);
    pinMode(8, OUTPUT);
    pinMode(9, OUTPUT);
    pinMode(2, INPUT);
    attachInterrupt(digitalPinToInterrupt(2), on_tdc, RISING);
    Serial.println(F("# Sheet 4.2: Timer1 sampling of the knock signal"));
}

void loop()
{
    uint16_t i, k;

    /* ---- record one block (the CPU only waits here) ---- */
    nbuf = 0; done = false; tdc_seen = 0;
    timer1_start();
    uint32_t guard = 0;                 /* safety: timer not running?     */
    while (!done && ++guard < 3000000UL) { }
    if (!done) { Serial.println(F("# timeout: Timer1 not running")); delay(1000); return; }

    /* ---- slow signal: potentiometer, read while the timer is stopped ---- */
    int pot = analogRead(A0);

    /* ---- print the block ---- */
    Serial.println(F("n,adc,tdc"));
    for (i = 0; i < NBUF; i++) {
        Serial.print(i); Serial.print(',');
        Serial.print(buf[i] & 0x03FF); Serial.print(',');
        Serial.println(buf[i] >> 15);
    }

    /* ---- (c) noise in the quiet window after every TDC ---- */
    float spd = FS_HZ / (6.0 * RPM_NOM);          /* samples per degree */
    uint16_t i0 = (uint16_t)ceil(QW_DEG0 * spd), i1 = (uint16_t)floor(QW_DEG1 * spd);
    long s1 = 0; float s2 = 0; uint16_t cnt = 0;
    for (k = 0; k < NBUF; k++) {
        if (!(buf[k] & 0x8000)) continue;          /* TDC at sample k     */
        for (i = k + i0; i <= k + i1 && i < NBUF; i++) {
            int v = buf[i] & 0x03FF;
            s1 += v; s2 += (float)v * v; cnt++;
        }
    }
    Serial.print(F("# pot: code ")); Serial.print(pot);
    Serial.print(F(" = ")); Serial.print(pot * 5.0 / 1024.0, 3); Serial.println(F(" V"));
    if (cnt > 1) {
        float m = (float)s1 / cnt;
        float sd = sqrt((s2 - cnt * m * m) / (cnt - 1));
        Serial.print(F("# quiet window samples ")); Serial.print(i0);
        Serial.print(F("..")); Serial.print(i1);
        Serial.print(F(" after TDC, n = ")); Serial.print(cnt);
        Serial.print(F(", mean = ")); Serial.print(m, 2);
        Serial.print(F(" LSB, std = ")); Serial.print(sd, 2);
        Serial.print(F(" LSB = ")); Serial.print(sd * 5000.0 / 1024.0, 1);
        Serial.println(F(" mV"));
    }
    delay(200);
}
