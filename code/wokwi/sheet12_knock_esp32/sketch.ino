/*
 * Sheet 12 -- Exercise 12.3: real-time knock detector on the ESP32
 * (student template)
 *
 * Wiring (diagram.json): engine-sim KNOCK -> GPIO34 (ADC1), CRANK -> GPIO18,
 * CAM -> GPIO19, TDC -> GPIO21 (only to verify the crank sync),
 * KNK -> GPIO22 (ground truth, only for the evaluation);
 * knock LEDs cylinder 1..4 -> GPIO25, 26, 27, 14; ignition retard
 * (PWM, duty ~ mean retard) -> GPIO13.
 *
 * Signal chain (one sample every 40 us, fs = 25 kHz):
 *   crank sync   60-2 decoder in the CRANK interrupt (gap detection, phase
 *                from CAM), crank angle by interpolation between teeth
 *   band-pass    10th-order Butterworth biquad cascade (knock_filter.h)
 *   window       10..70 deg ATDC, shifted by the filter group delay
 *   integration  E = sum y^2 over the window
 *   normalisation R = E / B[c], B[c] = background of cylinder c (EWMA)
 *   decision     knock if R > K_THR
 *   action       retard ignition of that cylinder, knock LED
 * One CSV line per combustion: cyl,rpm,R,knock,truth,retard
 * Every 200 combustions a summary line "# ..." with the confusion counts.
 */
#include "knock_filter.h"

const int PIN_KNOCK = 34;
const int PIN_CRANK = 18;
const int PIN_CAM = 19;
const int PIN_TDC = 21;
const int PIN_KNK = 22;
const int PIN_LED[4] = {25, 26, 27, 14};     /* cylinder 1, 2, 3, 4  */
const int PIN_RETARD = 13;

const unsigned long TS_US = 40;              /* fs = 25 kHz          */
const float V_PER_LSB = 3.3f / 4095.0f;
const float CHIP_GAIN = 0.6f;
const int ADC_OFFSET = 2048;

#define WIN0      10.0f    /* window start, deg ATDC                     */
#define WIN1      70.0f    /* window end                                 */
#define KF_DELAY  22       /* group delay of the cascade at 6.5 kHz      */
#define K_THR     2.0f     /* threshold on E/B (12.2: ~1 % false alarms) */
#define LAMBDA    0.05f    /* background EWMA weight                     */
#define LAMBDA_W  0.25f    /* ... during the warm-up                     */
#define WARMUP    16       /* combustions per cylinder for the start     */
#define RET_STEP  1.5f     /* deg retard per detected knock              */
#define RET_REC   0.05f    /* deg advance per knock-free combustion      */
#define RET_MAX   9.0f

static const float tdc_deg[4] = {120.0f, 300.0f, 480.0f, 660.0f};
static const int cyl_no[4] = {1, 3, 4, 2};   /* firing order 1-3-4-2   */

/* ---------------- crank decoder (interrupt) --------------------------- */
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t t_edge = 0;      /* time of the last rising edge, us   */
volatile uint32_t t_tooth = 0;     /* duration of one 6 deg tooth, us    */
volatile int tooth = -1;           /* 0..57, -1: no gap seen yet         */
volatile int rev = -1;             /* 0: theta 0..360, 1: 360..720       */

void IRAM_ATTR on_crank()
{
    uint32_t t = micros();
    portENTER_CRITICAL_ISR(&mux);
    uint32_t dt = t - t_edge;
    t_edge = t;
    /* TODO (a): 60-2 decoder. If dt is more than twice the last tooth
       period t_tooth, this edge follows the gap: tooth = 0 and toggle rev
       (if rev >= 0). Otherwise count the tooth (if synchronised, i.e.
       tooth >= 0; more than 57 teeth = lost sync -> tooth = -1) and store
       dt in t_tooth. Placeholder: only the tooth period is measured. */
    t_tooth = dt;
    if (tooth == 10) rev = digitalRead(PIN_CAM) ? 0 : 1;   /* phase from CAM
                                         (HIGH for 0..180 deg), checked every rev. */
    portEXIT_CRITICAL_ISR(&mux);
}

/* crank angle at time t (deg, 0..720) or -1 if not synchronised */
static float crank_angle(uint32_t t)
{
    portENTER_CRITICAL(&mux);
    uint32_t te = t_edge, tt = t_tooth;
    int k = tooth, r = rev;
    portEXIT_CRITICAL(&mux);
    if (k < 0 || r < 0 || tt == 0) return -1.0f;
    /* TODO (a): interpolate between the teeth: frac = (t - te)/tt teeth
       since the last edge (signed!), limited to 1 (3 after tooth 57, the
       gap follows); angle = 360 r + 6 (k + frac). Placeholder: -1. */
    (void)te;
    return -1.0f;
}

/* TDC pin: time of the pulse; the decoded angle at this time is compared
   with the nominal TDC angles in loop() (verification of the crank sync) */
volatile uint32_t t_tdc = 0;
volatile bool tdc_flag = false;
void IRAM_ATTR on_tdc() { t_tdc = micros(); tdc_flag = true; }
static float tdc_err_sum = 0.0f;
static int tdc_n = 0;

/* ---------------- band-pass ------------------------------------------- */
static float s1[KF_NSEC], s2[KF_NSEC];
static float bandpass(float x)
{
    for (int k = 0; k < KF_NSEC; k++) {
        const float *c = kf_sos[k];
        float y = c[0] * x + s1[k];
        s1[k] = c[1] * x - c[3] * y + s2[k];
        s2[k] = c[2] * x - c[4] * y;
        x = y;
    }
    return x;
}

/* ---------------- detector state -------------------------------------- */
static float th_hist[32];                    /* angle of the last 32 samples */
static uint32_t n_samp = 0;
static int win_cyl = -1;                     /* cylinder of the open window  */
static float E = 0.0f;
static float B[4];
static int nB[4];
static float retard[4];
static bool truth[4];                        /* KNK seen after TDC          */
static unsigned long led_off[4];
static long cnt_tp = 0, cnt_fp = 0, cnt_fn = 0, cnt_tn = 0, ncomb = 0;
static unsigned long t_next;
static int last_c_now = -1;

/* which cylinder's 0..180 deg segment contains theta, and the angle ATDC */
static int segment(float th, float *atdc)
{
    for (int c = 0; c < 4; c++) {
        float d = th - tdc_deg[c];
        if (d < 0.0f) d += 720.0f;
        if (d < 180.0f) { *atdc = d; return c; }
    }
    *atdc = th - 660.0f + 720.0f;            /* not reached */
    return 3;
}

static void finish_combustion(int c)
{
    /* TODO (c): normalisation R = E / B[c], decision knock = R > K_THR
       (only after WARMUP combustions of this cylinder), update of the
       background B[c] (clipped EWMA, see 12.1) and of the ignition retard
       (+RET_STEP on knock, max RET_MAX; -RET_REC otherwise, min 0).
       Placeholder: no detection, B = E. */
    float R = 1.0f;
    bool knock = false;
    B[c] = E;
    nB[c]++;
    float rmean = 0.25f * (retard[0] + retard[1] + retard[2] + retard[3]);
    ledcWrite(PIN_RETARD, (int)(255.0f * rmean / RET_MAX));
    int led = PIN_LED[cyl_no[c] - 1];
    if (knock) { digitalWrite(led, HIGH); led_off[c] = millis() + 100; }
    /* statistics against the ground truth */
    if (nB[c] > WARMUP) {
        ncomb++;
        if (knock && truth[c]) cnt_tp++;
        else if (knock) cnt_fp++;
        else if (truth[c]) cnt_fn++;
        else cnt_tn++;
    }
    float tt = t_tooth;
    Serial.printf("%d,%.0f,%.2f,%d,%d,%.2f\n", cyl_no[c], tt > 0 ? 1.0e6f / tt : 0.0f,
                  R, knock ? 1 : 0, truth[c] ? 1 : 0, retard[c]);
    if (ncomb > 0 && ncomb % 200 == 0 && nB[c] > WARMUP)
        Serial.printf("# %ld combustions: TP %ld FP %ld FN %ld TN %ld, detection %.1f %%, "
                      "false alarms %.1f %%, TDC sync error %.2f deg\n", ncomb, cnt_tp, cnt_fp,
                      cnt_fn, cnt_tn, 100.0f * cnt_tp / fmaxf(1.0f, (float)(cnt_tp + cnt_fn)),
                      100.0f * cnt_fp / fmaxf(1.0f, (float)(cnt_fp + cnt_tn)),
                      tdc_n > 0 ? tdc_err_sum / tdc_n : 0.0f);
    E = 0.0f;
}

void setup()
{
    Serial.setTxBufferSize(4096);
    Serial.begin(115200);
    analogReadResolution(12);
    pinMode(PIN_CRANK, INPUT);
    pinMode(PIN_CAM, INPUT);
    pinMode(PIN_TDC, INPUT);
    pinMode(PIN_KNK, INPUT);
    for (int i = 0; i < 4; i++) { pinMode(PIN_LED[i], OUTPUT); digitalWrite(PIN_LED[i], LOW); }
    ledcAttach(PIN_RETARD, 5000, 8);
    attachInterrupt(digitalPinToInterrupt(PIN_CRANK), on_crank, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_TDC), on_tdc, RISING);
    Serial.println("# Sheet 12 knock detector, fs = 25 kHz, window 10..70 deg ATDC");
    Serial.println("cyl,rpm,R,knock,truth,retard");
    t_next = micros();
}

void loop()
{
    while ((long)(micros() - t_next) < 0) { }
    uint32_t t = t_next;                     /* nominal sampling instant */
    t_next += TS_US;

    int adc = analogRead(PIN_KNOCK);
    float x = (adc - ADC_OFFSET) * V_PER_LSB / CHIP_GAIN;
    float y = bandpass(x);

    float th = crank_angle(t);
    th_hist[n_samp & 31] = th;
    float thd = th_hist[(n_samp - KF_DELAY) & 31];   /* angle of the sample the
                                                        filter output belongs to */
    n_samp++;
    if (th < 0.0f || thd < 0.0f || n_samp < 32) {          /* not synchronised */
        tdc_flag = false;
        return;
    }

    /* ground truth: KNK HIGH in 0..90 deg after TDC of the current cylinder */
    float a_now;
    int c_now = segment(th, &a_now);
    if (c_now != last_c_now) { truth[c_now] = false; last_c_now = c_now; }
    if (a_now < 90.0f && digitalRead(PIN_KNK)) truth[c_now] = true;

    /* crank sync check: decoded angle at the TDC pulse */
    if (tdc_flag) {
        float at, thc = crank_angle(t_tdc);
        tdc_flag = false;
        if (thc >= 0.0f) {
            segment(thc, &at);
            tdc_err_sum += fabsf(at < 90.0f ? at : at - 180.0f);
            tdc_n++;
        }
    }

    /* window on the delayed angle */
    float a;
    int c = segment(thd, &a);
    /* TODO (b): integrate E += y^2 while the delayed angle a is in
       WIN0..WIN1 (start with E = 0 when the window of a new cylinder c
       opens: win_cyl != c); when a reaches WIN1, call
       finish_combustion(c) once and set win_cyl = -1. */
    (void)a; (void)c; (void)y;

    for (int i = 0; i < 4; i++)
        if (led_off[i] && (long)(millis() - led_off[i]) > 0) {
            digitalWrite(PIN_LED[cyl_no[i] - 1], LOW);
            led_off[i] = 0;
        }
}
