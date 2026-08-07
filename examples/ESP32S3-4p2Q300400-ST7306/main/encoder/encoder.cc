#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"     // IDF v5+
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"            // for ESP_RETURN_ON_ERROR
#include <atomic>

#include "encoder.h"

static const char *TAG = "ENC";
// 计数上下限（避免溢出）；可按需改大
#define PCNT_HIGH_LIMIT  32767
#define PCNT_LOW_LIMIT   -32768

static pcnt_unit_handle_t s_pcnt_unit = NULL;
static pcnt_channel_handle_t s_chan_a = NULL;
static pcnt_channel_handle_t s_chan_b = NULL;

static volatile int64_t s_last_btn_irq_us = 0;   // 去抖
static volatile int s_btn_state = 1;             // 当前按键电平
// 原来：static volatile uint32_t s_btn_clicks = 0;
static std::atomic<uint32_t> s_btn_clicks{0};

static void IRAM_ATTR btn_isr(void *arg)
{
    int level = gpio_get_level(KNOB_NUM);
    int64_t now = esp_timer_get_time();
    if (level == BTN_ACTIVE_LEVEL && (now - s_last_btn_irq_us) > 10000) {
        s_btn_clicks.fetch_add(1, std::memory_order_relaxed);  // 替代 ++
        s_last_btn_irq_us = now;
    }
    s_btn_state = level;
}

// --- 初始化正交编码器到 PCNT ---
esp_err_t encoder_init_pcnt(void)
{
    // 1) 创建计数单元
    pcnt_unit_config_t unit_cfg{};  // 全零
    unit_cfg.low_limit  = PCNT_LOW_LIMIT;   // 先 low 再 high
    unit_cfg.high_limit = PCNT_HIGH_LIMIT;

    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, &s_pcnt_unit), TAG, "pcnt_new_unit failed");

    // 2) 设置毛刺滤波（去抖），例如 1us ~ 5us 视硬件噪声而定
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 1000, // 1us
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_cfg), TAG, "filter failed");

    // 3) 创建 A 通道（边沿=A，相位参考= B）
    pcnt_chan_config_t ch_a_cfg{};              // 全零，包含 flags
    ch_a_cfg.edge_gpio_num  = GPIO_ENCODER_A;
    ch_a_cfg.level_gpio_num = GPIO_ENCODER_B;

    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &ch_a_cfg, &s_chan_a), TAG, "chan A failed");
    //   A 上升加、下降减（常见配置）；电平翻转用于正交
    // --- A 通道：产生计数 ---
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(
        s_chan_a,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // A 上升 +1
        PCNT_CHANNEL_EDGE_ACTION_DECREASE),  // A 下降 -1
        TAG, "edge A failed");

    // A 的方向参考 = B 的电平：B=0 保持，B=1 反向（你也可以反过来，见“修正方向”）
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(
        s_chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,      // 当 B=0 时，按上面的 +1/-1 执行
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE),  // 当 B=1 时，方向取反
        TAG, "level A failed");

    // 4) 创建 B 通道（边沿=B，相位参考= A）
    pcnt_chan_config_t ch_b_cfg{};              // 同样全零
    ch_b_cfg.edge_gpio_num  = GPIO_ENCODER_B;
    ch_b_cfg.level_gpio_num = GPIO_ENCODER_A;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &ch_b_cfg, &s_chan_b), TAG, "chan B failed");
    // --- B 通道：不参与计数（防止抵消），但把引脚连上保证能读到 B 电平 ---
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(
        s_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_HOLD,       // 上升不加不减
        PCNT_CHANNEL_EDGE_ACTION_HOLD),      // 下降不加不减
        TAG, "edge B failed");

    // level 动作对最终计数没有影响（因为 B 不再改计数），随便 KEEP 即可
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(
        s_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP),
        TAG, "level B failed");

    // 5) 使能/清零/启动
    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_pcnt_unit), TAG, "enable failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt_unit), TAG, "clear failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(s_pcnt_unit), TAG, "start failed");

    return ESP_OK;
}

// --- 初始化按键（GPIO 中断） ---
esp_err_t encoder_button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << KNOB_NUM,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = (BTN_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (BTN_ACTIVE_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = (BTN_ACTIVE_LEVEL == 0) ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;  // 真错误才返回
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(KNOB_NUM, btn_isr, NULL));

    s_btn_state = gpio_get_level(KNOB_NUM);
    return ESP_OK;
}

// --- 对外提供的读数函数 ---
int encoder_get_count(void)
{
    int count = 0;
    if (s_pcnt_unit) {
        pcnt_unit_get_count(s_pcnt_unit, &count);
    }
    return count;
}

void encoder_clear(void)
{
    if (s_pcnt_unit) {
        pcnt_unit_clear_count(s_pcnt_unit);
    }
}

// 对外读取
uint32_t encoder_get_clicks(void)
{
    return s_btn_clicks.load(std::memory_order_relaxed);
}