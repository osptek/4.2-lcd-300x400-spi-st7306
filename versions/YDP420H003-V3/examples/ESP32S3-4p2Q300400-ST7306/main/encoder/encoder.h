#pragma once

#include "esp_log.h"
#include "driver/gpio.h"

#define GPIO_ENCODER_A  GPIO_NUM_48
#define GPIO_ENCODER_B  GPIO_NUM_21
#define KNOB_NUM        GPIO_NUM_46
#define BTN_ACTIVE_LEVEL 0    // 低电平按下常见

esp_err_t encoder_init_pcnt(void);
esp_err_t encoder_button_init(void);
int       encoder_get_count(void);
void      encoder_clear(void);
uint32_t  encoder_get_clicks(void);