#pragma once
#include <cstdint>
#include <cstring>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <algorithm>

#define DC_PIN      GPIO_NUM_12
#define RES_PIN     GPIO_NUM_13
#define CS_PIN      GPIO_NUM_11
#define SCLK_PIN    GPIO_NUM_10
#define SDIN_PIN    GPIO_NUM_9
#define TE_PIN      GPIO_NUM_14

#define LCD_H   400  
#define LCD_W   300

class ST7306_LCD_Color4 {
public:
    ST7306_LCD_Color4(
        gpio_num_t dc, 
        gpio_num_t rst, 
        gpio_num_t cs,
        gpio_num_t sclk, 
        gpio_num_t mosi,
        spi_host_device_t 
        host = SPI2_HOST,
        int spi_clock_hz = 40 * 1000 * 1000
    );
    ~ST7306_LCD_Color4();
    void initialize();
    void fill(uint8_t data);
    void clearDisplay();
    void writePoint(uint16_t x, uint16_t y, uint16_t data);
    void DisplayFullScreen(const uint8_t* image);
    void ShowImageAt(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data);
    void display();

    void Low_Power_Mode();
    void High_Power_Mode();
    void display_on(bool enabled);
    void display_sleep(bool enabled);
    void display_Inversion(bool enabled);

private:
    esp_err_t write_cmd(uint8_t cmd);
    esp_err_t write_data(const uint8_t* data, size_t len);
    esp_err_t write_param(uint8_t p);
    void address();
    void Initial_ST7306();

private:
    const gpio_num_t DC_PIN, RES_PIN, CS_PIN, SCLK_PIN, SDIN_PIN;
    const int LCD_WIDTH = LCD_W;
    const int LCD_HIGH  = LCD_H;
    const int LCD_DATA_WIDTH  = LCD_W/2;        // 每行字节数 = 屏幕宽度 = 300
    const int LCD_DATA_HIGH   = LCD_H/2;  
    const int DISPLAY_BUFFER_LENGTH = LCD_DATA_WIDTH * LCD_DATA_HIGH;

    bool HPM_MODE = true;
    bool LPM_MODE = false;

    spi_host_device_t spi_host_;
    int spi_clock_hz_;
    spi_device_handle_t spi_dev_ = nullptr;

    uint8_t* display_buffer = nullptr;
};
