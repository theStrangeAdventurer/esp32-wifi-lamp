idf_component_register(SRCS "led_strip.c" "led_strip_encoder.c"
                    REQUIRES driver esp_common
                    INCLUDE_DIRS "include" ".")
