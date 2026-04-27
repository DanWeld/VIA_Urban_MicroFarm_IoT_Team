// Windows stub for LED driver
#include "led.h"

void led_init(void) {}
led_status_t led_on(int8_t led_no) { return LED_OK; }
led_status_t led_off(int8_t led_no) { return LED_OK; }
led_status_t led_toggle(int8_t led_no) { return LED_OK; }
led_status_t led_blink(int8_t led_no, uint16_t period_ms) { return LED_OK; }
