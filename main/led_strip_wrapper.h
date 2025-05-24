#ifndef __SMART_LAMP_LED_STRIP_WRAPPER_H__
#define __SMART_LAMP_LED_STRIP_WRAPPER_H__
#include <stdint.h>
uint8_t scale_0_255_to_0_100_fast(uint8_t value);
void set_brightness_value(uint8_t percent_value);
void init_led();
#endif
