/*
 * Sheet 3, Exercise 3.3 -- smoothing the RPM estimate
 * (REFERENCE SOLUTION, Arduino Uno + engine-sim chip)
 *
 *   eng:CRANK -> D2 (INT0)
 *
 * The ISR time-stamps every rising crank edge with Timer1 (0.5 us) and
 * pushes the tooth period and a gap flag into a FIFO. loop() forms the
 * per-tooth speed r[k] (gap handling selectable), and smooths it with
 *   - a moving average over N_MA samples (running integer sum)
 *   - an exponential smoother with ALPHA = 2/(N_MA+1)
 * Output:
 *   - one CSV line per revolution: rev,t_ms,rpm_raw,rpm_ma,rpm_ema
 *   - a capture block of NREC samples (NPRE before the trigger) whenever
 *     the raw speed jumps by more than TRIG_RPM between two teeth
 *     (click the rpm slider at a new position): k,raw,ma,ema,gap,
 *     followed by the step-response evaluation of both filters
 *     (50 % / 90 % points and ramp lag D = sum (1 - s[k]))
 */
#define N_MA      16                /* moving-average length (<= 64)        */
#define GAP_MODE  2                 /* 0 naive, 1 one sample, 2 three samples */
#define TRIG_RPM  150.0f          /* trigger: jump between two teeth  */
#define NREC      128
#define NPRE      32

const float ALPHA = 2.0f / (N_MA + 1);
const byte CRANK_PIN = 2;

/* ---- ISR -> loop FIFO ------------------------------------------------- */
#define FIFO_N 32                   /* power of two */
volatile uint16_t fifo_T[FIFO_N];
volatile uint8_t  fifo_gap[FIFO_N];
volatile uint8_t  fifo_wr = 0;
uint8_t fifo_rd = 0;
volatile uint16_t t_last, T_prev = 0;
volatile bool have_edge = false;

void crank_isr(void)
{
    uint16_t now = TCNT1;
    uint16_t T = now - t_last;
    t_last = now;
    if (!have_edge) { have_edge = true; return; }
    bool gap = (T_prev != 0) && (T > 2 * T_prev);
    T_prev = T;
    fifo_T[fifo_wr] = T;
    fifo_gap[fifo_wr] = gap;
    fifo_wr = (fifo_wr + 1) & (FIFO_N - 1);
}

/* ---- filters ------------------------------------------------------------ */
int16_t ma_buf[N_MA];
uint8_t ma_idx = 0;
int32_t ma_sum = 0;
float   ema = 0.0f;
bool    filt_init = false;

/* ---- capture buffer (ring, NREC records) ---------------------------------- */
int16_t rec_raw[NREC], rec_ma[NREC], rec_ema[NREC];
uint8_t rec_gap[NREC / 8];
uint8_t rec_w = 0;
int16_t post = -1;                  /* samples still to record after trigger */
uint16_t holdoff = 0;
bool    dump = false;

float last_ma;
float r_prev = 0.0f;               /* last non-gap raw value (trigger)  */

void process(float r, bool gap)
{
    int16_t ri = (int16_t)(r + 0.5f);
    if (!filt_init) {                          /* pre-load with first value */
        for (uint8_t i = 0; i < N_MA; i++) ma_buf[i] = ri;
        ma_sum = (int32_t)ri * N_MA;
        ema = r;
        filt_init = true;
    }
    /* moving average: running sum, add newest, subtract oldest */
    ma_sum += ri - ma_buf[ma_idx];
    ma_buf[ma_idx] = ri;
    if (++ma_idx >= N_MA) ma_idx = 0;
    float ma = (float)ma_sum / N_MA;
    /* exponential smoother */
    ema += ALPHA * (r - ema);

    last_ma = ma;

    /* capture ring buffer */
    if (!dump) {
        rec_raw[rec_w] = ri;
        rec_ma[rec_w]  = (int16_t)(ma + 0.5f);
        rec_ema[rec_w] = (int16_t)(ema + 0.5f);
        if (gap) rec_gap[rec_w >> 3] |= (1 << (rec_w & 7));
        else     rec_gap[rec_w >> 3] &= ~(1 << (rec_w & 7));
        rec_w = (rec_w + 1) % NREC;
        if (holdoff) holdoff--;
        if (!gap) {                          /* trigger on a speed jump */
            if (post < 0 && holdoff == 0 && r_prev > 0.0f &&
                fabs(r - r_prev) > TRIG_RPM)
                post = NREC - NPRE;
            r_prev = r;
        }
        if (post > 0 && --post == 0) { dump = true; post = -1; }
    }
}

/* ---- step-response evaluation of a capture ------------------------------- */
/* rec[] is the ring buffer, record i (oldest = 0) is rec[(rec_w + i) % NREC],
   the trigger is at i = NPRE. Normalised step response
     s[k] = (y[NPRE + k] - base) / (final - base),  k = 0 .. NREC-NPRE-1
   base = mean of the NPRE pre-trigger samples, final = mean of the last 36. */
#define NFIN 36
void step_metrics(const __FlashStringHelper *name, const int16_t *rec)
{
    float base = 0.0f, fin = 0.0f, D = 0.0f;
    int16_t k50 = -1, k90 = -1, i;
    for (i = 0; i < NPRE; i++) base += rec[(rec_w + i) % NREC];
    base /= NPRE;
    for (i = NREC - NFIN; i < NREC; i++) fin += rec[(rec_w + i) % NREC];
    fin /= NFIN;
    for (i = NPRE; i < NREC; i++) {
        float s = (rec[(rec_w + i) % NREC] - base) / (fin - base);
        if (k50 < 0 && s >= 0.5f) k50 = i - NPRE;
        if (k90 < 0 && s >= 0.9f) k90 = i - NPRE;
        D += 1.0f - s;
    }
    Serial.print(F("# "));        Serial.print(name);
    Serial.print(F(": base "));   Serial.print(base, 1);
    Serial.print(F(", final "));  Serial.print(fin, 1);
    Serial.print(F(", 50% at k = ")); Serial.print(k50);
    Serial.print(F(", 90% at k = ")); Serial.print(k90);
    Serial.print(F(", D = sum(1-s) = ")); Serial.print(D, 2);
    Serial.println(F(" samples"));
}

void setup()
{
    Serial.begin(115200);
    pinMode(CRANK_PIN, INPUT);
    TCCR1A = 0;                 /* Timer1 free running, prescaler 8: 0.5 us */
    TCCR1B = _BV(CS11);
    TIMSK1 = 0;
    attachInterrupt(digitalPinToInterrupt(CRANK_PIN), crank_isr, RISING);
    Serial.println("rev,t_ms,rpm_raw,rpm_ma,rpm_ema");
    holdoff = 2 * NREC;
}

void loop()
{
    static uint32_t rev = 0;
    static bool synced = false;

    while (fifo_rd != fifo_wr) {
        uint16_t T = fifo_T[fifo_rd];
        bool gap = fifo_gap[fifo_rd];
        fifo_rd = (fifo_rd + 1) & (FIFO_N - 1);
        if (gap) synced = true;
        if (!synced) continue;

        /* per-tooth speed: rpm = 1 / T[s] = 2e6 / T[ticks] */
        float r = 2.0e6f / T;
        uint8_t rep = 1;
#if GAP_MODE >= 1
        if (gap) r *= 3.0f;                    /* gap spans 18 deg */
#endif
#if GAP_MODE == 2
        if (gap) rep = 3;                      /* one sample per 6 deg */
#endif
        for (uint8_t i = 0; i < rep; i++) process(r, gap);

        if (gap && !dump) {                    /* once per revolution */
            Serial.print(rev++);       Serial.print(',');
            Serial.print(millis());    Serial.print(',');
            Serial.print(r_prev, 1);   Serial.print(',');
            Serial.print(last_ma, 1);  Serial.print(',');
            Serial.println(ema, 1);
        }
    }

    if (dump) {
        Serial.println("# capture: k,raw,ma,ema,gap (k = 0: trigger)");
        for (int16_t i = 0; i < NREC; i++) {
            uint8_t j = (rec_w + i) % NREC;    /* oldest first */
            Serial.print(i - NPRE);       Serial.print(',');
            Serial.print(rec_raw[j]);     Serial.print(',');
            Serial.print(rec_ma[j]);      Serial.print(',');
            Serial.print(rec_ema[j]);     Serial.print(',');
            Serial.println((rec_gap[j >> 3] >> (j & 7)) & 1);
        }
        Serial.println("# end of capture");
        step_metrics(F("MA "), rec_ma);
        step_metrics(F("EMA"), rec_ema);
        /* the FIFO overflowed while printing: drop it and re-arm */
        noInterrupts();
        fifo_rd = fifo_wr;
        interrupts();
        synced = false;
        holdoff = 2 * NREC;
        dump = false;
    }
}
