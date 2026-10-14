/*
 * Sheet 1, Exercises 1.2 and 1.3 -- 60-2 crank wheel decoding
 * (student template, Arduino Uno + engine-sim chip)
 *
 *   eng:CRANK -> D2 (INT0)   eng:CAM -> D4   button -> D7 (to GND)
 *   D12 = TDC LED (our estimate), D11 = cylinder-1 LED (needs cam sync)
 *
 * The crank ISR time-stamps every rising edge, computes the tooth period,
 * detects the missing-tooth gap (period ratio > 2), counts the teeth and
 * derives crank angle and engine speed. Once per revolution one CSV line
 * is printed. Pressing the button records the next DUMP_N tooth periods
 * and prints them as a second CSV block.
 *
 * Time base: TIMEBASE_TIMER1 = 1 uses Timer1 (prescaler 8, 0.5 us/tick),
 * 0 uses micros() (4 us resolution on the Uno).
 */
#define TIMEBASE_TIMER1 1

const byte CRANK_PIN = 2, CAM_PIN = 4, BTN_PIN = 7, TDC_LED = 12, CYL1_LED = 11;
const byte TDC_TOOTH_A = 0, TDC_TOOTH_B = 0;     /* TODO (b): TDC teeth   */
const byte LED_TEETH = 5;                         /* LED on for 30 deg     */
const byte CAM_TOOTH = 0;                         /* TODO (Ex 1.4)         */
#define DUMP_N 120

#if TIMEBASE_TIMER1
typedef uint16_t tick_t;
const float TICKS_PER_S = 2.0e6f;
static inline tick_t now_ticks(void) { return TCNT1; }
#else
typedef uint32_t tick_t;
const float TICKS_PER_S = 1.0e6f;
static inline tick_t now_ticks(void) { return micros(); }
#endif

/* ---- state shared between ISR and loop ---------------------------- */
volatile tick_t   t_last;
volatile tick_t   T_prev = 0;        /* last tooth period (ticks)         */
volatile uint8_t  tooth = 0;         /* 0..57, 0 = first tooth after gap   */
volatile bool     synced = false;    /* gap seen at least once             */
volatile bool     have_edge = false;
volatile int16_t  phase = -1;        /* 0 or 360 deg (cam), -1 = unknown   */
volatile uint32_t rev_ticks = 0, rev_acc = 0;
volatile tick_t   Tmin_acc, Tmax_acc, Tmin_rev, Tmax_rev, T_tooth_rev;
volatile uint8_t  teeth_acc = 0, teeth_rev = 0;
volatile bool     rev_ready = false;
volatile uint16_t sync_errors = 0, cam_errors = 0;
volatile uint16_t dump_T[DUMP_N];
volatile uint8_t  dump_k[DUMP_N];
volatile uint8_t  dump_n = 0;
volatile bool     dump_active = false;

void crank_isr(void)
{
    tick_t now = now_ticks();
    tick_t T = now - t_last;              /* unsigned: wrap-around safe */
    t_last = now;
    if (!have_edge) { have_edge = true; return; }

    bool gap = false;
    /* TODO (b): gap detection. Across the gap the period is about three
       times the previous tooth period: gap = (T > 2 * T_prev).
       Remember T in T_prev for the next edge.                          */
    (void)T;

    if (gap) {
        /* TODO (b): one revolution is complete:
           - consistency check: if (synced && tooth != 57) sync_errors++;
           - hand over to loop(): rev_ticks (sum of ALL periods of the
             revolution incl. the gap), Tmin_rev, Tmax_rev, teeth_rev;
             set rev_ready; reset the accumulators (Tmin_acc = 0xFFFF)
           - synced = true; tooth = 0 (this edge is the reference mark)  */
    } else {
        if (!synced) return;
        /* TODO (b): tooth++; accumulate rev_acc, teeth_acc, Tmin_acc,
           Tmax_acc; T_tooth_rev = T                                     */
    }

    /* TODO (b): crank angle of this edge = 6 * tooth (+ phase).
       Switch TDC_LED on at the TDC teeth (TDC_TOOTH_A/B -- check the
       values above!) and off LED_TEETH teeth later.                          */

    /* TODO (Ex 1.4, challenge): read CAM_PIN at tooth CAM_TOOTH and set
       phase = 0 (cam HIGH) or 360 (cam LOW); toggle phase at every gap;
       count cam_errors; light CYL1_LED only at the TDC of cylinder 1.  */

    if (dump_active) {
        dump_T[dump_n] = (uint16_t)T;
        dump_k[dump_n] = gap ? 255 : tooth;   /* 255 marks the gap */
        if (++dump_n >= DUMP_N) dump_active = false;
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(CRANK_PIN, INPUT);
    pinMode(CAM_PIN, INPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);
    pinMode(TDC_LED, OUTPUT);
    pinMode(CYL1_LED, OUTPUT);
#if TIMEBASE_TIMER1
    TCCR1A = 0;                 /* normal mode, free running 16 bit */
    TCCR1B = _BV(CS11);         /* prescaler 8 -> 2 MHz = 0.5 us     */
    TIMSK1 = 0;
#endif
    Tmin_acc = 0xFFFF;
    attachInterrupt(digitalPinToInterrupt(CRANK_PIN), crank_isr, RISING);
    Serial.println("rev,t_ms,rpm_rev,rpm_tooth,rpm_min,rpm_max,teeth,phase,sync_err,cam_err");
}

void loop()
{
    static uint32_t rev = 0;
    static bool btn_old = true;

    /* button pressed -> start recording tooth periods */
    bool b = digitalRead(BTN_PIN);
    if (!b && btn_old && !dump_active) { dump_n = 0; dump_active = true; }
    btn_old = b;

    if (rev_ready) {
        uint32_t rt; tick_t tmin, tmax, tl; uint8_t nt; int16_t ph;
        uint16_t se, ce;
        noInterrupts();
        rt = rev_ticks; tmin = Tmin_rev; tmax = Tmax_rev; tl = T_tooth_rev;
        nt = teeth_rev; ph = phase; se = sync_errors; ce = cam_errors;
        rev_ready = false;
        /* TODO (b): engine speed in rpm
             rpm_rev   from the duration of the whole revolution (rt ticks)
             rpm_tooth from the last tooth period (tl), rpm_min/rpm_max
             from the longest/shortest tooth period of the revolution.
           TICKS_PER_S converts ticks to seconds.                        */
        float rpm_rev = 0, rpm_tooth = 0, rpm_min = 0, rpm_max = 0;
        (void)rt; (void)tl; (void)tmin; (void)tmax;
        Serial.print(rev++);              Serial.print(',');
        Serial.print(millis());           Serial.print(',');
        Serial.print(rpm_rev, 1);         Serial.print(',');
        Serial.print(rpm_tooth, 1);       Serial.print(',');
        Serial.print(rpm_min, 1);         Serial.print(',');
        Serial.print(rpm_max, 1);         Serial.print(',');
        Serial.print(nt);                 Serial.print(',');
        Serial.print(ph);                 Serial.print(',');
        Serial.print(se);                 Serial.print(',');
        Serial.println(ce);
    }

    if (!dump_active && dump_n == DUMP_N) {
        Serial.println("# tooth period dump (tooth 255 = gap)");
        Serial.println("n,tooth,T_ticks,rpm");
        for (uint8_t i = 0; i < DUMP_N; i++) {
            float r = TICKS_PER_S / dump_T[i];
            if (dump_k[i] == 255) r *= 3.0f;     /* gap spans 3 teeth */
            Serial.print(i);           Serial.print(',');
            Serial.print(dump_k[i]);   Serial.print(',');
            Serial.print(dump_T[i]);   Serial.print(',');
            Serial.println(r, 1);
        }
        Serial.println("# end of dump");
        dump_n = 0;
    }
}
