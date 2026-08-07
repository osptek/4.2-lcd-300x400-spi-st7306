#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "st7306_color4.h"
#include "encoder.h"

#include "image2.h"

#define ENCODER_COUNTS_PER_STEP 1

static const char *TAG = "APP";
static constexpr int kTotalPages = 2;   // 预留页数，后续自行填充内容

static int current_page = 0;

ST7306_LCD_Color4 lcd(DC_PIN, RES_PIN, CS_PIN, SCLK_PIN, SDIN_PIN, SPI2_HOST);

void AppInit() {
    lcd.initialize();
    //lcd.Low_Power_Mode();
    lcd.High_Power_Mode();
    lcd.display_on(true);
    lcd.display_Inversion(false);

    lcd.clearDisplay();
}

void drawEightColorBars() {
    uint16_t colors[4] = {
        0x00,
        0x01,
        0x02,
        0x03
    };

    int barHeight = LCD_H / 4;   // 每条色带高度
    int screenWidth = LCD_W;
    int startY = 0;

    for (int i = 0; i < 4; i++)
    {
        uint16_t color = colors[i];

        // 画一条水平色带
        for (int y = startY; y < startY + barHeight; y++)
        {
            for (int x = 0; x < screenWidth; x++)
            {
                lcd.writePoint(x, y, color);
            }
        }
        startY += barHeight;
    }
}

static void render_page(int page) {
    lcd.clearDisplay();

    switch (page) {
        case 0:
            drawEightColorBars();
            break;
        
        case 1:
            lcd.ShowImageAt(0, 0, image2_w, image2_h, image2);
            break;

        default:
            ESP_LOGI(TAG, "Page %d placeholder (待实现内容)", page);
            break;
    }

    lcd.display();
}

static void apply_page_delta(int steps) {
    if (steps == 0) {
        return;
    }

    int next = current_page + steps;
    if (kTotalPages > 0) {
        next %= kTotalPages;
        if (next < 0) {
            next += kTotalPages;
        }
    }

    if (next != current_page) {
        current_page = next;
        render_page(current_page);
    }
}

static void encoder_task(void *arg) {
    int last_count = encoder_get_count();

    while (true) {
        int now_count = encoder_get_count();
        int delta_counts = now_count - last_count;
        int steps = 0;

        if (ENCODER_COUNTS_PER_STEP > 1) {
            steps = delta_counts / ENCODER_COUNTS_PER_STEP;
            if (steps != 0) {
                last_count += steps * ENCODER_COUNTS_PER_STEP;
            }
        } else if (delta_counts != 0) {
            steps = delta_counts;
            last_count = now_count;
        }

        steps = -steps;  // 方向修正，保持与原有习惯一致

        if (steps != 0) {
            apply_page_delta(steps);
        }

        vTaskDelay(pdMS_TO_TICKS(10));   // 100 Hz 轮询足够顺滑
    }
}

extern "C" void app_main(void) {
    AppInit();
/*
    lcd.ShowImageAt(0, 0, image2_w, image2_h, image2);
    lcd.display();
*/


    ESP_ERROR_CHECK(encoder_init_pcnt());
    ESP_ERROR_CHECK(encoder_button_init());

    render_page(current_page);

    BaseType_t ok = xTaskCreate(encoder_task, "encoder_task", 4096, nullptr, 5, nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create encoder task");
    }
}
