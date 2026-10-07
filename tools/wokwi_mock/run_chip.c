#define _POSIX_C_SOURCE 0
/* Tiny discrete-event runner for the mock Wokwi API.
   Build: gcc -std=c99 -I. run_chip.c ../../code/wokwi/chips/engine-sim.chip.c -lm
   Prints "t_ns,pin,value" for digital pin changes and every 10th DAC write. */
#include "wokwi-api.h"
#include <string.h>
#include <stdlib.h>
#define MAXT 8
static struct { timer_config_t cfg; uint64_t due, period; int active; } T[MAXT];
static int nt = 0, np = 0, na = 0;
static const char *pname[16]; static uint32_t pval[16];
static struct { const char *name; float v; } A[32];
static uint64_t now = 0; static long ndac = 0;
pin_t pin_init(const char *name, uint32_t mode) { pname[np] = name; pval[np] = (mode == OUTPUT_HIGH); return np++; }
void pin_write(pin_t p, uint32_t v) { if (pval[p] != v) printf("%llu,%s,%u\n", (unsigned long long)now, pname[p], v); pval[p] = v; }
uint32_t pin_read(pin_t p) { return pval[p]; }
void pin_dac_write(pin_t p, float v) { if (getenv("DAC") && (ndac++ % 10) == 0) printf("%llu,%s,%.4f\n", (unsigned long long)now, pname[p], v); }
bool pin_watch(pin_t p, const pin_watch_config_t *c) { (void)p; (void)c; return true; }
static float attr_env(const char *n, float d) { char k[64]; const char *s; snprintf(k, 64, "ATTR_%s", n); s = getenv(k); return s ? (float)atof(s) : d; }
uint32_t attr_init(const char *n, uint32_t d) { A[na].name = n; A[na].v = attr_env(n, (float)d); return na++; }
uint32_t attr_init_float(const char *n, float d) { A[na].name = n; A[na].v = attr_env(n, d); return na++; }
uint32_t attr_read(uint32_t a) { return (uint32_t)A[a].v; }
float attr_read_float(uint32_t a) { return A[a].v; }
timer_t timer_init(const timer_config_t *c) { T[nt].cfg = *c; T[nt].active = 0; return nt++; }
void timer_start_ns(timer_t t, uint64_t ns, bool rep) { T[t].due = now + ns; T[t].period = rep ? ns : 0; T[t].active = 1; }
void timer_start(timer_t t, uint32_t us, bool rep) { timer_start_ns(t, (uint64_t)us * 1000, rep); }
void timer_stop(timer_t t) { T[t].active = 0; }
uint64_t get_sim_nanos(void) { return now; }
void chip_init(void);
int main(int argc, char **argv)
{
    uint64_t end = (uint64_t)((argc > 1 ? atof(argv[1]) : 0.05) * 1e9);
    chip_init();
    for (;;) {
        int i, k = -1;
        for (i = 0; i < nt; i++) if (T[i].active && (k < 0 || T[i].due < T[k].due)) k = i;
        if (k < 0 || T[k].due > end) break;
        now = T[k].due;
        if (T[k].period) T[k].due += T[k].period; else T[k].active = 0;
        T[k].cfg.callback(T[k].cfg.user_data);
    }
    return 0;
}
