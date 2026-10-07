/* Minimal stand-in for Wokwi's wokwi-api.h: lets the custom chips be
   compiled and run on a PC (tools/wokwi_mock/run_chip.c). Not the
   official header -- only the subset used by the course chips. */
#ifndef WOKWI_API_MOCK_H
#define WOKWI_API_MOCK_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
enum pin_value { LOW = 0, HIGH = 1 };
enum pin_mode { INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2, INPUT_PULLDOWN = 3, ANALOG = 4, OUTPUT_LOW = 16, OUTPUT_HIGH = 17 };
enum edge { RISING = 1, FALLING = 2, BOTH = 3 };
typedef int32_t pin_t;
typedef uint32_t timer_t;
typedef struct { void *user_data; void (*callback)(void *user_data); } timer_config_t;
typedef struct { void *user_data; uint32_t edge; void (*pin_change)(void *user_data, pin_t pin, uint32_t value); } pin_watch_config_t;
pin_t pin_init(const char *name, uint32_t mode);
void pin_write(pin_t pin, uint32_t value);
uint32_t pin_read(pin_t pin);
void pin_dac_write(pin_t pin, float voltage);
bool pin_watch(pin_t pin, const pin_watch_config_t *config);
uint32_t attr_init(const char *name, uint32_t default_value);
uint32_t attr_init_float(const char *name, float default_value);
uint32_t attr_read(uint32_t attr_id);
float attr_read_float(uint32_t attr_id);
timer_t timer_init(const timer_config_t *config);
void timer_start(timer_t timer_id, uint32_t micros, bool repeat);
void timer_start_ns(timer_t timer_id, uint64_t nanos, bool repeat);
void timer_stop(timer_t timer_id);
uint64_t get_sim_nanos(void);
#endif
