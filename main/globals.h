#ifndef __SMART_LAMP_GLOBALS_H__
#define __SMART_LAMP_GLOBALS_H__

#include "freertos/idf_additions.h"
#include "led_strip.h"
#include <stdint.h>

extern SemaphoreHandle_t lamp_state_mutex;
extern led_strip_state_t lamp_state;

#endif
