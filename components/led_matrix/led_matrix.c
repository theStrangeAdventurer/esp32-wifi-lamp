#include "led_matrix.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip_encoder.h"
#include <stdint.h>
#include <string.h>

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
#define BIT_PER_ONE_ADDRESS_LED 24

static const char *TAG = "led_matrix_component";

/**
 * Global variables
 */
rmt_channel_handle_t led_chan = NULL;
uint8_t *led_strip_pixels = NULL;
rmt_encoder_handle_t led_encoder = NULL;
rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};

uint8_t brightness = 0;
uint8_t brightness_dir = 1;
uint8_t brightness_blue = 0;
uint8_t brightness_blue_dir = 1;
uint8_t increase = 2;

int led_numbers = 0;
int led_per_col = 0;
int led_per_row = 0;
int size = 0;

void increase_brightness(uint8_t *pval, const uint8_t increase,
                         uint8_t *direction) {
  // set_pixel_color(pPixels, 0, 0, brightness, 0);
  if ((*pval + increase) >= 50) {
    *direction = -1;
  }

  if ((*pval - increase) <= 0) {
    *direction = 1;
  }

  if (*direction == 1) {
    *pval += increase;
  } else {
    *pval -= increase;
  }
  return;
}

void set_pixel_color(uint8_t *pPixels, int offset, int r, int g, int b) {
  int _offset = offset * 3; // Три пикселя в каждом светодиоде
  // g, r, b - Реальная последовательность, а не rgb
  pPixels[_offset] = g;
  pPixels[_offset + 1] = r;
  pPixels[_offset + 2] = b;
}

void reset_matrix() {
  memset(led_strip_pixels, 0, size);
  ESP_ERROR_CHECK(
      rmt_transmit(led_chan, led_encoder, led_strip_pixels, size, &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

/**
 * Init function
 */
void init_matrix(int gpio_num, int led_rows, int led_columns) {
  led_per_col = led_columns;
  led_per_row = led_rows;
  led_numbers = led_per_col * led_per_row;
  size = led_numbers * BIT_PER_ONE_ADDRESS_LED;
  led_strip_pixels = (uint8_t *)malloc(size);

  rmt_tx_channel_config_t tx_chan_config = {
      .clk_src = 4, // select source clock ?? I don't know what mean that number
      .gpio_num = gpio_num,
      .mem_block_symbols =
          64, // increase the block size can make the LED less flickering
      .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
      .trans_queue_depth = 4, // set the number of transactions that can be
                              // pending in the background
  };

  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

  ESP_LOGI(TAG, "Install led strip encoder");

  led_strip_encoder_config_t encoder_config = {
      .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
  };

  ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

  ESP_LOGI(TAG, "Enable RMT TX channel");
  ESP_ERROR_CHECK(rmt_enable(led_chan));
  reset_matrix();
}

void traverse_matrix(uint8_t *p_pixels, led_callback_t callback,
                     int chase_speed) {
  if (!p_pixels || !callback || led_per_row <= 0 || led_per_col <= 0) {
        return;  // Защита от некорректных данных
  }
  for (int i = 0; i < led_per_row; i++) {
    int cell = i * led_per_row;
    for (int j = 0; j < led_per_col; j++) {
      int led = cell + j;
      callback(p_pixels, led);
      vTaskDelay(pdMS_TO_TICKS(chase_speed));
    }
  }
}

/* Torch mode starts here ... */
static uint8_t torch_bright = 0;

static void torch_warn(uint8_t *p_pixels, int led_index) {
  uint8_t green = torch_bright / 1.7;
  uint8_t blue = green / 3;
  set_pixel_color(p_pixels, led_index, torch_bright, green, blue);
  ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, p_pixels,
                               led_numbers * BIT_PER_ONE_ADDRESS_LED,
                               &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

static void torch_default(uint8_t *p_pixels, int led_index) {
  set_pixel_color(p_pixels, led_index, torch_bright, torch_bright,
                  torch_bright);
  ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, p_pixels,
                               led_numbers * BIT_PER_ONE_ADDRESS_LED,
                               &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

static uint8_t scale_0_100_to_0_255_fast(uint8_t value) {
    if (value > 100) value = 100;
    return (value * 255 + 50) / 100;
}

void torch2(uint8_t percent_value) {
	if (percent_value < 1) percent_value = 0;
	if (percent_value > 100) percent_value = 100;
	uint8_t val255 = scale_0_100_to_0_255_fast(percent_value);
	torch_bright = val255;
	traverse_matrix(led_strip_pixels, torch_warn, 0);
}

void torch(int level, int mode) {
  uint8_t increase = 5;

  while (1) {
    switch (level) {
    case 0:
      torch_bright = 0;
      break;
    case 1:
      if (torch_bright < 255) {
        torch_bright += increase;
      } else {
        torch_bright -= increase;
      }
      break;
    case 2:
      if (torch_bright < 100) {
        torch_bright += increase;
      } else {
        torch_bright -= increase;
      }
      break;
    case 3:
      if (torch_bright < 20) {
        torch_bright += increase;
      } else {
        torch_bright -= increase;
      }
      break;
    }

    switch (mode) {
    case 0:
      traverse_matrix(led_strip_pixels, torch_default, 0);
      break;
    case 1:
      traverse_matrix(led_strip_pixels, torch_warn, 0);
      break;
    }
  }
}
