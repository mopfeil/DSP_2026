/*
 * Sheet 0 -- Getting started with Wokwi (Arduino Uno)
 *
 * Samples the potentiometer on A0 every 10 ms (fs = 100 Hz) and prints
 * CSV lines "n,t_us,adc,volt" to the Serial Monitor. The LED on pin 13
 * toggles at every sample so the sampling rate can be checked with the
 * logic analyzer (channel D0).
 */
const unsigned long TS_US = 10000;   /* sampling period in microseconds */
const int LED_PIN = 13;

unsigned long t_next;
unsigned long n = 0;

void setup()
{
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    Serial.println("n,t_us,adc,volt");
    t_next = micros();
}

void loop()
{
    /* wait for the next sampling instant (deadline based, no drift) */
    while ((long)(micros() - t_next) < 0) { }
    t_next += TS_US;

    int adc = analogRead(A0);                 /* 10 bit: 0..1023 */
    float volt = adc * (5.0f / 1023.0f);
    digitalWrite(LED_PIN, n & 1);

    Serial.print(n); Serial.print(',');
    Serial.print(micros()); Serial.print(',');
    Serial.print(adc); Serial.print(',');
    Serial.println(volt, 3);
    n++;
}
