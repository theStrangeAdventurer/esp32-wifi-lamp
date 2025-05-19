#include "led_strip_encoder.h"
#include <stdint.h>

/**
 * Set of handy functions for using with
 * led_strips or led matrices based on ws2812b address leds
 */

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r,
                       uint32_t *g, uint32_t *b);

void set_pixel_color(uint8_t *p_pixels, int offset, int r, int g, int b);

void torch2(uint8_t percent_value);

void increase_brightness(uint8_t *pval, const uint8_t increase,
                         uint8_t *direction);

/*
 * Callback function which receive pixelsArray and led index (starts from 0)
 */
typedef void (*led_callback_t)(uint8_t *p_pixels, int led_index);

void traverse_matrix(uint8_t *p_pixels, led_callback_t callback,
                     int chase_speed);

/**
 * Init matrix or strip function
 */
void init_matrix(int gpio_num, int led_rows, int led_columns);

/**
 * Effects
 */
void crimson_azure_flow(int chase_speed);

void torch(int level, int mode);

void reset_matrix();
