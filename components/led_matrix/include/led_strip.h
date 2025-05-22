#ifndef __LED_STRIP_H__
#define __LED_STRIP_H__

#include <stddef.h>
#include <stdint.h>
typedef struct {
  int gpio_num;
  int is_initiated;
  uint8_t cols;
  uint8_t rows;
  uint8_t brightness;
  uint8_t *p_pixels;
  int pixels_size;
} led_strip_state_t;
/*
 * Callback function which receive pixelsArray and led index (starts from 0)
 */
typedef void (*led_callback_t)(uint8_t *p_pixels, int led_index);

void transmit_pixels_data(uint8_t *p_pixels, size_t size);
void traverse_matrix(uint8_t *p_pixels, led_callback_t callback,
                     int chase_speed, int led_per_col, int led_per_row);
void init_rmt_encoder(int gpio_num);
void reset_pixels_array(uint8_t *p_pixels, size_t size);
#endif
