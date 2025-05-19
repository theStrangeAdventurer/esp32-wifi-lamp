#include "esp_log.h"
#include "led_strip.h"

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
#define RMT_LED_STRIP_GPIO_NUM 19

#define LED_COLS 1
#define LED_ROWS 1
#define EXAMPLE_CHASE_SPEED_MS 100

const char *TAG = "led.c";

void set_light_value(uint8_t percent_value) {
  ESP_LOGI(TAG, "Setting light value: %d", percent_value);
  torch2(percent_value);
}

void init_led() {
  ESP_LOGI(TAG, "init");
  init_matrix(RMT_LED_STRIP_GPIO_NUM, LED_ROWS, LED_COLS);
}
