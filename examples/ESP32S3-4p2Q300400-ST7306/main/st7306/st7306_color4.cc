#include "st7306_color4.h"

static const char* TAG = "ST7306";

ST7306_LCD_Color4::ST7306_LCD_Color4(gpio_num_t dc, gpio_num_t rst, gpio_num_t cs,
    gpio_num_t sclk, gpio_num_t mosi,
    spi_host_device_t host, int spi_clock_hz)
    : DC_PIN(dc), RES_PIN(rst), CS_PIN(cs), SCLK_PIN(sclk), SDIN_PIN(mosi),
      spi_host_(host), spi_clock_hz_(spi_clock_hz) 
{
    // 显存使用 DMA 可用内存，便于一次性刷屏
    display_buffer = (uint8_t*)heap_caps_malloc(DISPLAY_BUFFER_LENGTH, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (display_buffer) {
        memset(display_buffer, 0x00, DISPLAY_BUFFER_LENGTH);
    }
}

ST7306_LCD_Color4::~ST7306_LCD_Color4() {
    if (spi_dev_) {
        spi_bus_remove_device(spi_dev_);
        spi_dev_ = nullptr;
    }
    spi_bus_free(spi_host_);
    if (display_buffer) { free(display_buffer); display_buffer = nullptr; }
}

void ST7306_LCD_Color4::initialize() {
    gpio_config_t io = {};
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << DC_PIN) | (1ULL << RES_PIN);
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.pull_up_en   = GPIO_PULLUP_DISABLE;
    io.intr_type    = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io));

    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = SCLK_PIN;
    buscfg.mosi_io_num = SDIN_PIN;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    // 一次整屏写入需要较大 TX buffer
    buscfg.max_transfer_sz = DISPLAY_BUFFER_LENGTH + 16;
    ESP_ERROR_CHECK(spi_bus_initialize(spi_host_, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev{};
    dev.clock_speed_hz = spi_clock_hz_;
    dev.mode = 0;
    dev.spics_io_num = CS_PIN;
    dev.queue_size = 4;
    dev.flags = SPI_DEVICE_NO_DUMMY;
    ESP_ERROR_CHECK(spi_bus_add_device(spi_host_, &dev, &spi_dev_));

    gpio_set_level(RES_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RES_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RES_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    Initial_ST7306();
    fill(0x00);
}

void ST7306_LCD_Color4::Initial_ST7306() {
    write_cmd(0xD6); write_param(0x17); write_param(0x02);
    write_cmd(0xD1); write_param(0x01);

    write_cmd(0xC0); //Gate Voltage Setting 
    write_param(0X12); //VGH 00:8V  04:10V  08:12V   0E:15V   12:17V
    write_param(0X0a); //VGL 00:-5V   04:-7V   0A:-10V

    // VLC=3.6V (12/-5)(delta Vp=0.6V)		
    write_cmd(0xC1); //VSHP Setting (4.8V)	
    write_param(115); //VSHP1 	
    write_param(0X3E); //VSHP2 	
    write_param(0X3C); //VSHP3 	
    write_param(0X3C); //VSHP4	

    write_cmd(0xC2); //VSLP Setting (0.98V)	
    write_param(0); //VSLP1 	
    write_param(0X21); //VSLP2 	
    write_param(0X23); //VSLP3 	
    write_param(0X23); //VSLP4 	

    write_cmd(0xC4); //VSHN Setting (-3.6V)	
    write_param(50); //VSHN1	
    write_param(0X5C); //VSHN2 	
    write_param(0X5A); //VSHN3 	
    write_param(0X5A); //VSHN4 	

    write_cmd(0xC5); //VSLN Setting (0.22V)	
    write_param(50); //VSLN1 	
    write_param(0X35); //VSLN2 	
    write_param(0X37); //VSLN3 	
    write_param(0X37); //VSLN4

    write_cmd(0XD8);   //OSC Setting
    write_param(0XA6);  
    write_param(0XE9);
    /*-- HPM=32hz ; LPM=> 0x15=8Hz 0x14=4Hz 0x13=2Hz 0x12=1Hz 0x11=0.5Hz 0x10=0.25Hz---*/
    write_cmd(0xB2); //Frame Rate Control 
    write_param(0X12); //HPM=32hz ; LPM=1hz 

    // write_cmd(0XD8);   //OSC Setting
    // write_param(0XA6);  
    // write_param(0XE9);
    // /*-- HPM=32hz ; LPM=> 0x15=8Hz 0x14=4Hz 0x13=2Hz 0x12=1Hz 0x11=0.5Hz 0x10=0.25Hz---*/
    // write_cmd(0xB2); //Frame Rate Control 
    // write_param(0X02); //HPM=16hz ; LPM=1hz 

    write_cmd(0xB3); //Update Period Gate EQ Control in HPM 
    write_param(0XE5); 
    write_param(0XF6); 
    write_param(0X17);
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X71); 

    write_cmd(0xB4); //Update Period Gate EQ Control in LPM 
    write_param(0X05); //LPM EQ Control 
    write_param(0X46); 
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X77); 
    write_param(0X76); 
    write_param(0X45); 

    write_cmd(0x62); //Gate Timing Control
    write_param(0X32);
    write_param(0X03);
    write_param(0X1F);

    // write_cmd(0XC7);   //Ultra Low Power Mode  
    // write_param(0XC1);  
    // write_param(0X41);
    // write_param(0X26);

    write_cmd(0xB7); //Source EQ Enable 
    write_param(0X13); 

    write_cmd(0XB0);   //Gate Line Setting
    write_param(0X64);  //400 line = 100*4

    write_cmd(0x11); //Sleep out 
    vTaskDelay(pdMS_TO_TICKS(120));

    write_cmd(0xC9); //Source Voltage Select  
    write_param(0X00); //VSHP1; VSLP1 ; VSHN1 ; VSLN1

    write_cmd(0x36); //Memory Data Access Control
    // write_param(0X00); //Memory Data Access Control: MX=0 ; DO=0 
    write_param(0X48); //MX=1 ; DO=1 
    // write_param(0X4c); //MX=1 ; DO=1 GS=1

    write_cmd(0x3A); //Data Format Select 
    write_param(0X11); //10:4write for 24bit ; 11: 3write for 24bit

    write_cmd(0xB9); //Gamma Mode Setting 
    write_param(0X20); //20: Mono 00:4GS  

    write_cmd(0xB8); //Panel Setting 
    write_param(0x29); // Panel Setting: 0x29: 1-Dot inversion, Frame inversion, One Line Interlace

    //WRITE RAM 300X400
    write_cmd(0X2A);   //Column Address Setting
    write_param(0X05);
    write_param(0X36);
    write_cmd(0X2B);   //Row Address Setting
    write_param(0X00);
    write_param(0XC7);

    write_cmd(0x35); //TE
    write_param(0X00); 

    write_cmd(0xD0); //Auto power dowb OFF
    // write_param(0X7F); //Auto power dowb OFF
    write_param(0XFF); //Auto power dowb ON
    // write_cmd(0x39); //LPM:Low Power Mode ON
    write_cmd(0x38); //HPM:high Power Mode ON

    HPM_MODE = true;
    LPM_MODE = false;

    write_cmd(0x29); //DISPLAY ON  
    // write_cmd(0x28); //DISPLAY OFF  

    // write_cmd(0x21); //Display Inversion On 
    write_cmd(0x20); //Display Inversion Off 

    write_cmd(0xBB); // Enable Clear RAM
    write_param(0x4F); // CLR=0 ; Enable Clear RAM,clear RAM to 0
}

void ST7306_LCD_Color4::Low_Power_Mode(){
    if(LPM_MODE){
        HPM_MODE = false;
        LPM_MODE = true;
    }
    else{
        HPM_MODE = false;
        LPM_MODE = true;

        // VLC=3.6V (12/-5)(delta Vp=0.6V)		
        write_cmd(0xC1); //VSHP Setting (4.8V)	
        write_param(115); //VSHP1 	
        write_param(0X3E); //VSHP2 	
        write_param(0X3C); //VSHP3 	
        write_param(0X3C); //VSHP4	

        write_cmd(0xC2); //VSLP Setting (0.98V)	
        write_param(0); //VSLP1 	
        write_param(0X21); //VSLP2 	
        write_param(0X23); //VSLP3 	
        write_param(0X23); //VSLP4 	

        write_cmd(0xC4); //VSHN Setting (-3.6V)	
        write_param(50); //VSHN1	
        write_param(0X5C); //VSHN2 	
        write_param(0X5A); //VSHN3 	
        write_param(0X5A); //VSHN4 	

        write_cmd(0xC5); //VSLN Setting (0.22V)	
        write_param(50); //VSLN1 	
        write_param(0X35); //VSLN2 	
        write_param(0X37); //VSLN3 	
        write_param(0X37); //VSLN4

        write_cmd(0xC9); //Source Voltage Select  
        write_param(0X00); //VSHP1; VSLP1 ; VSHN1 ; VSLN1

        vTaskDelay(pdMS_TO_TICKS(20));

        write_cmd(0x39); //LPM:Low Power Mode ON
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ST7306_LCD_Color4::High_Power_Mode(){
    if(HPM_MODE){
        HPM_MODE = true;
        LPM_MODE = false;
    }
    else{
        HPM_MODE = true;
        LPM_MODE = false;

        write_cmd(0x38); //HPM:high Power Mode ON
        vTaskDelay(pdMS_TO_TICKS(300));

        // VLC=3.6V (12/-5)(delta Vp=0.6V)		
        write_cmd(0xC1); //VSHP Setting (4.8V)	
        write_param(115); //VSHP1 	
        write_param(0X3E); //VSHP2 	
        write_param(0X3C); //VSHP3 	
        write_param(0X3C); //VSHP4	

        write_cmd(0xC2); //VSLP Setting (0.98V)	
        write_param(0); //VSLP1 	
        write_param(0X21); //VSLP2 	
        write_param(0X23); //VSLP3 	
        write_param(0X23); //VSLP4 	

        write_cmd(0xC4); //VSHN Setting (-3.6V)	
        write_param(50); //VSHN1	
        write_param(0X5C); //VSHN2 	
        write_param(0X5A); //VSHN3 	
        write_param(0X5A); //VSHN4 	

        write_cmd(0xC5); //VSLN Setting (0.22V)	
        write_param(50); //VSLN1 	
        write_param(0X35); //VSLN2 	
        write_param(0X37); //VSLN3 	
        write_param(0X37); //VSLN4

        write_cmd(0xC9); //Source Voltage Select  
        write_param(0X00); //VSHP1; VSLP1 ; VSHN1 ; VSLN1

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void ST7306_LCD_Color4::display_on(bool enabled) {
    write_cmd(enabled ? 0x29 : 0x28);
}

void ST7306_LCD_Color4::display_sleep(bool enabled){
    if (enabled){
        if (LPM_MODE){
            write_cmd(0x38); // back to HPM before sleep
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        write_cmd(0x10);
        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        write_cmd(0x11);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ST7306_LCD_Color4::display_Inversion(bool enabled){
    write_cmd(enabled ? 0x21 : 0x20);
}

void ST7306_LCD_Color4::address() {
    write_cmd(0x2A);          // Column
    write_param(0x05);
    write_param(0x36);

    write_cmd(0x2B);          // Row
    write_param(0x00);
    write_param(0xC7);

    write_cmd(0x2C);          // Memory Write
}

void ST7306_LCD_Color4::display() {
    address();
    // 先拉 DC=1 表示后续是数据流
    gpio_set_level(DC_PIN, 1);
    spi_transaction_t t = {};
    t.flags = 0;
    t.length = DISPLAY_BUFFER_LENGTH * 8;
    t.tx_buffer = display_buffer;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_dev_, &t));
}

void ST7306_LCD_Color4::DisplayFullScreen(const uint8_t* image) {
    if (!image) {
        return;
    }
    memcpy(display_buffer, image, DISPLAY_BUFFER_LENGTH); // 8064 字节
}

void ST7306_LCD_Color4::ShowImageAt(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data) {
    if (!data || w == 0 || h == 0) {
        return;
    }
    for (uint16_t row = 0; row < h; ++row) {
        uint32_t dst_y = static_cast<uint32_t>(y) + row;
        if (dst_y >= LCD_HIGH) {
            break;
        }
        uint32_t row_base = static_cast<uint32_t>(row) * w;
        for (uint16_t col = 0; col < w; ++col) {
            uint32_t dst_x = static_cast<uint32_t>(x) + col;
            if (dst_x >= LCD_WIDTH) {
                break;
            }
            uint32_t pixel_index = row_base + col;
            uint32_t byte_index = pixel_index / 4;
            uint32_t shift = (3 - (pixel_index % 4)) * 2;
            uint16_t pixel = (data[byte_index] >> shift) & 0x3;
            writePoint(static_cast<uint16_t>(dst_x), static_cast<uint16_t>(dst_y), pixel);
        }
    }
}

void ST7306_LCD_Color4::writePoint(uint16_t x, uint16_t y, uint16_t data) {
    if(x>=LCD_WIDTH || y>=LCD_HIGH){
        return;
    }
    else{
        // 找到是哪一行的数据
        uint16_t real_x = x/2; // 0->0, 3->0, 4->1, 7->1
        uint16_t real_y = y/2; // 0->0, 1->0, 2->1, 3->1
        uint16_t write_byte_index = real_y*LCD_DATA_WIDTH+real_x;
        uint16_t one_two = (y % 2 == 0)?0:1; // 0 1
        uint16_t line_bit_1 = (x % 2)*4; // 0 4
        uint16_t line_bit_0 = (x % 2)*4 + 2; // 2 6
        uint8_t write_bit_1 = 7-(line_bit_1+one_two);
        uint8_t write_bit_0 = 7-(line_bit_0+one_two);

        bool data_bit0 = (data & 0x0001) > 0? true : false;
        bool data_bit1 = (data & 0x0002) > 0? true : false;

        if (data_bit1) {
            // 将指定位置的 bit 置为 1
            display_buffer[write_byte_index] |= (1 << write_bit_1);
        } else {
            // 将指定位置的 bit 置为 0
            display_buffer[write_byte_index] &= ~(1 << write_bit_1);
        }

        if (data_bit0) {
            // 将指定位置的 bit 置为 1
            display_buffer[write_byte_index] |= (1 << write_bit_0);
        } else {
            // 将指定位置的 bit 置为 0
            display_buffer[write_byte_index] &= ~(1 << write_bit_0);
        }
    }
}

esp_err_t ST7306_LCD_Color4::write_cmd(uint8_t cmd) {
    gpio_set_level(DC_PIN, 0); // 命令
    spi_transaction_t t{};
    t.length = 8;
    t.tx_buffer = &cmd;
    return spi_device_polling_transmit(spi_dev_, &t);
}

esp_err_t ST7306_LCD_Color4::write_param(uint8_t p) {
    gpio_set_level(DC_PIN, 1); // 数据/参数
    spi_transaction_t t{};
    t.length = 8;
    t.tx_buffer = &p;
    return spi_device_polling_transmit(spi_dev_, &t);
}

esp_err_t ST7306_LCD_Color4::write_data(const uint8_t* data, size_t len) {
    gpio_set_level(DC_PIN, 1);
    spi_transaction_t t{};
    t.length = len * 8;
    t.tx_buffer = data;
    return spi_device_polling_transmit(spi_dev_, &t);
}

void ST7306_LCD_Color4::fill(uint8_t data) {
    memset(display_buffer, data, DISPLAY_BUFFER_LENGTH);
    ESP_LOGI(TAG, "fill data = 0x%02x", data);
}

void ST7306_LCD_Color4::clearDisplay() {
    memset(display_buffer, 0x00, DISPLAY_BUFFER_LENGTH);
}
