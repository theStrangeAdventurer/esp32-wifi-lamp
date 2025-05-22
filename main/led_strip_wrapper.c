#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "globals.h"
#include "led_strip.h"
#include "mdns.h"
#include <stdint.h>
#include <stdlib.h>

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
#define RMT_LED_STRIP_GPIO_NUM 19

#define LED_COLS 1
#define LED_ROWS 1
#define EXAMPLE_CHASE_SPEED_MS 100
#define BIT_PER_ONE_ADDRESS_LED 24

const char *TAG = "led_strip_wrapper.c";

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} color_t;

color_t get_warm_light(uint8_t brightness) {
  // color_t res = {.g = brightness / 1.7, .b = brightness / 3, .r =
  // brightness}; return res;
  return (color_t){.r = brightness,
                   .g = brightness * 0.588f, // ~1/1.7
                   .b = brightness / 3};
}

static void set_pixel_color(uint8_t *p_pixels, int offset, int r, int g,
                            int b) {
  int _offset = offset * 3; // Три пикселя в каждом светодиоде
  // g, r, b - Реальная последовательность, а не rgb
  p_pixels[_offset] = g;
  p_pixels[_offset + 1] = r;
  p_pixels[_offset + 2] = b;
}

static uint8_t scale_0_100_to_0_255_fast(uint8_t value) {
  if (value > 100)
    value = 100;
  return (value * 255 + 50) / 100;
}

static void set_brightness_cb(uint8_t *p_pixels, int index) {
  color_t color = get_warm_light(lamp_state.brightness);
  set_pixel_color(lamp_state.p_pixels, index, color.r, color.g, color.b);
  transmit_pixels_data(lamp_state.p_pixels, lamp_state.pixels_size);
}

static void update_led_strip_brightness() {
  traverse_matrix(lamp_state.p_pixels, set_brightness_cb,
                  EXAMPLE_CHASE_SPEED_MS, lamp_state.cols, lamp_state.rows);
}

void set_brightness_value(uint8_t percent_value) {
  uint8_t result_value = scale_0_100_to_0_255_fast(percent_value);
  ESP_LOGI(TAG, "Setting light value from: %d to %d", percent_value,
           result_value);
  lamp_state.brightness = result_value;
  update_led_strip_brightness();
}

void init_led() {
  if (lamp_state.is_initiated) {
    ESP_LOGE(TAG, "Lamp state is already initiated");
    return;
  }

  lamp_state.is_initiated = 1;
  lamp_state.cols = LED_COLS;
  lamp_state.rows = LED_ROWS;
  lamp_state.gpio_num = RMT_LED_STRIP_GPIO_NUM;
  lamp_state.pixels_size = LED_ROWS * LED_ROWS * BIT_PER_ONE_ADDRESS_LED;
  lamp_state.p_pixels = malloc(lamp_state.pixels_size);

  if (!lamp_state.p_pixels) {
    ESP_LOGE(TAG, "Pixels memory allocation error");
    return;
  }

  ESP_LOGI(TAG, "init_led with brightness: %d", lamp_state.brightness);
  init_rmt_encoder(RMT_LED_STRIP_GPIO_NUM);
  reset_pixels_array(lamp_state.p_pixels, lamp_state.pixels_size);
}
