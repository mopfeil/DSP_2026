/*
 * Sheet 3, Exercise 3.3 -- smoothing the RPM estimate
 * (student template, Arduino Uno + engine-sim chip)
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
 *     (click the rpm slider at a new position): k,raw,ma,ema,gap
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
    /* TODO (b): moving average over N_MA samples with the running integer
       sum ma_sum and the ring buffer ma_buf/ma_idx (add the newest value,
       subtract the oldest one)                                          */
    float ma = r;
    /* TODO (b): exponential smoother  ema <- ema + ALPHA (r - ema)      */
    ema = r;

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

        /* TODO (b): per-tooth speed in rpm from T (Timer1 ticks of 0.5 us)
           and gap handling according to GAP_MODE:
             0: use the gap period like a normal tooth period
             1: correct the speed for the 18 deg gap, one sample
             2: corrected speed, inserted three times (rep = 3)          */
        float r = 0.0f;
        uint8_t rep = 1;
        (void)T;
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
        /* the FIFO overflowed while printing: drop it and re-arm */
        noInterrupts();
        fifo_rd = fifo_wr;
        interrupts();
        synced = false;
        holdoff = 2 * NREC;
        dump = false;
    }
}
