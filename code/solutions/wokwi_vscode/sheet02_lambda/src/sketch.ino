/*
 * Sheet 2, Exercise 2.4 -- closed lambda control loop
 * (REFERENCE SOLUTION, Arduino Uno + lambda-probe chip)
 *
 *   D9  -> lam:INJ   fuel command as PWM (analogWrite, 490 Hz),
 *                    duty 50 % = stoichiometric quantity q = 1
 *   A0  <- lam:O2    switching lambda sensor, 0.1 V lean ... 0.9 V rich
 *   A1  <- lam:LAM   true lambda / 2 (ground truth, not used by the controller)
 *
 * Two-point controller with integrator (+ optional P jump), period 10 ms.
 * CSV output every 10 ms: t_ms,u_o2,lambda,q,period_ms
 */
const unsigned long TS_US = 10000;     /* controller period 10 ms           */
const float U_THR  = 0.45f;            /* rich/lean threshold in V          */
const float RATE   = 0.2f;             /* integrator slope, q per second    */
const float PJUMP  = 0.0f;             /* jump at each sensor switch        */
const int   INJ_PIN = 9;

unsigned long t_next, t_switch = 0;
float q = 1.0f;                        /* fuel quantity, 1 = 50 % duty      */
bool  rich_old = false;
unsigned int period_ms = 0;

void setup()
{
    Serial.begin(115200);
    pinMode(INJ_PIN, OUTPUT);
    analogWrite(INJ_PIN, 128);
    Serial.println("t_ms,u_o2,lambda,q,period_ms");
    t_next = micros();
}

void loop()
{
    while ((long)(micros() - t_next) < 0) { }
    t_next += TS_US;

    float u   = analogRead(A0) * (5.0f / 1023.0f);       /* sensor voltage */
    float lam = 2.0f * analogRead(A1) * (5.0f / 1023.0f); /* ground truth   */

    /* ---- two-point controller with integrator ---------------------- */
    bool rich = u > U_THR;
    if (rich != rich_old) {
        q += rich ? -PJUMP : PJUMP;                       /* P jump */
        if (rich) {                                       /* lean -> rich */
            unsigned long now = millis();
            if (t_switch) period_ms = now - t_switch;
            t_switch = now;
        }
    }
    q += (rich ? -RATE : RATE) * (TS_US * 1e-6f);         /* I ramp */
    if (q < 0.5f) q = 0.5f;
    if (q > 1.5f) q = 1.5f;
    rich_old = rich;

    /* duty = q / 2, 8-bit PWM */
    analogWrite(INJ_PIN, (int)(127.5f * q + 0.5f));

    Serial.print(millis());   Serial.print(',');
    Serial.print(u, 3);       Serial.print(',');
    Serial.print(lam, 3);     Serial.print(',');
    Serial.print(q, 4);       Serial.print(',');
    Serial.println(period_ms);
}
