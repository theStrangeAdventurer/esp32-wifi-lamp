#include "led_strip.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip_encoder.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
static const char *TAG = "led_strip_component";

/**
 * Global variables
 */
rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;
rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};

void transmit_pixels_data(uint8_t *p_pixels, size_t size) {
  ESP_ERROR_CHECK(
      rmt_transmit(led_chan, led_encoder, p_pixels, size, &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

void reset_pixels_array(uint8_t *p_pixels, size_t size) {
  memset(p_pixels, 0, size);
  ESP_ERROR_CHECK(
      rmt_transmit(led_chan, led_encoder, p_pixels, size, &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

void init_rmt_encoder(int gpio_num) {
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
}

void traverse_matrix(uint8_t *p_pixels, led_callback_t callback,
                     int chase_speed, int led_per_col, int led_per_row) {
  if (!p_pixels || !callback || led_per_row <= 0 || led_per_col <= 0) {
    ESP_LOGE(TAG, "traverse_matrix - incorrect parameters were passed");
    return;
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
