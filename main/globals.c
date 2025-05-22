#include "globals.h"
#include <unistd.h>
// Определение глобальной переменной (память выделяется здесь)
led_strip_state_t lamp_state = {.is_initiated = 0,
                                .gpio_num = -1,
                                .cols = 0,
                                .rows = 0,
                                .brightness = 10, // 0-255
                                .p_pixels = NULL, // Пока нет массива
                                .pixels_size = 0};
