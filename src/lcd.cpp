// lcd.c
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_common.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include <esp_log.h>

#include "lcd.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#define TAG "MediaKit"

/**********************
 *     定义引脚和参数
 **********************/
#define LCD_SPI_HOST SPI3_HOST
#define DISPLAY_MOSI_PIN GPIO_NUM_21
#define DISPLAY_CLK_PIN GPIO_NUM_47
#define DISPLAY_DC_PIN GPIO_NUM_45
#define DISPLAY_RST_PIN GPIO_NUM_20
#define DISPLAY_CS_PIN GPIO_NUM_NC

#define LCD_RST_PIN GPIO_NUM_20
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_2
#define BACKLIGHT_PWM_TIMER LEDC_TIMER_0
#define BACKLIGHT_PWM_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_PWM_FREQ 1000                // PWM 频率：5kHz
#define BACKLIGHT_RESOLUTION LEDC_TIMER_13_BIT // 13-bit 分辨率 (0 ~ 8191)

// 屏幕分辨率和偏移
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 135
#define DISPLAY_OFFSET_X 40
#define DISPLAY_OFFSET_Y 53

esp_lcd_panel_handle_t panel = NULL;
lv_disp_t * disp_handle;

//定义一个结构体
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *container;
} lvgl_screen_t;

lvgl_screen_t lvgl_screen;

/**********************
 * @brief 初始化背光 PWM 控制
 **********************/
void backlight_init(void)
{
    ledc_timer_config_t timer={
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BACKLIGHT_RESOLUTION,
        .timer_num = BACKLIGHT_PWM_TIMER,
        .freq_hz = BACKLIGHT_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel={
        .gpio_num = DISPLAY_BACKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BACKLIGHT_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE,
        .flags = {.output_invert = false}
    };
    ledc_channel_config(&channel);
}

/**********************
 * @brief 设置背光亮度
 * @param brightness - 百分比 (0 ~ 100)
 **********************/
void set_backlight_brightness(int brightness)
{
    int duty_max = (1 << BACKLIGHT_RESOLUTION) - 1;
    int duty = (brightness * duty_max) / 100;

    if (duty > duty_max)
        duty = duty_max;
    else if (duty < 0)
        duty = 0;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_PWM_CHANNEL);
}

  void reset_lcd(void) {
    // 配置引脚为输出模式，并设置默认电平为低
    gpio_config_t io_20_conf = {};
    io_20_conf.intr_type = GPIO_INTR_DISABLE;
    io_20_conf.mode = GPIO_MODE_OUTPUT;
    io_20_conf.pin_bit_mask = (1ULL << LCD_RST_PIN);
    io_20_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_20_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_20_conf);
    gpio_set_level(LCD_RST_PIN, 0); // 设置引脚为低电平
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LCD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    io_20_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    gpio_config(&io_20_conf);
}

void init_lvgl(void)
{
    ESP_LOGD(TAG, "Install BL IO");
    backlight_init();
    set_backlight_brightness(100);

    ESP_LOGD(TAG, "Install Spi IO");
    spi_bus_config_t spi_bus_io = {};
    spi_bus_io.mosi_io_num = DISPLAY_MOSI_PIN;
    spi_bus_io.miso_io_num = GPIO_NUM_NC;
    spi_bus_io.sclk_io_num = DISPLAY_CLK_PIN;
    spi_bus_io.quadwp_io_num = GPIO_NUM_NC;
    spi_bus_io.quadhd_io_num = GPIO_NUM_NC;
    spi_bus_io.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &spi_bus_io, SPI_DMA_CH_AUTO));

    ESP_LOGD(TAG, "Install Panel IO");
    esp_lcd_panel_io_handle_t io_handle  = NULL;
    esp_lcd_panel_io_spi_config_t io_config={};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = 3;
    io_config.pclk_hz = 80 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &io_handle));

    ESP_LOGD(TAG, "Install Panel Config");
    esp_lcd_panel_dev_config_t panel_config={};
    panel_config.reset_gpio_num = LCD_RST_PIN;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel));
    esp_lcd_panel_reset(panel);               // 液晶屏复位

    reset_lcd();
    esp_lcd_panel_init(panel);                // 初始化配置寄存器
    esp_lcd_panel_invert_color(panel, true);  // 颜色反转
    esp_lcd_panel_swap_xy(panel, true);       // 显示翻转 
    esp_lcd_panel_mirror(panel, true, false); // 镜像

    uint16_t *buffer = (uint16_t *)malloc(DISPLAY_WIDTH * sizeof(uint16_t));
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for display buffer");
        return;
    }
    for (int i = 0; i < DISPLAY_WIDTH; i++) {
        buffer[i] = 0xFFFF; // 白色填充 (RGB565 格式)
    }
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, DISPLAY_WIDTH, y + 1, buffer);
    }
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    free(buffer);

    lv_init();

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t err = lvgl_port_init(&lvgl_cfg);
    ESP_LOGI(TAG, "lvgl_port_init: %s", err == ESP_OK ? "OK" : "Failed");

    lvgl_port_display_cfg_t disp_cfg;
    disp_cfg.io_handle = io_handle;
    disp_cfg.panel_handle = panel;
    disp_cfg.control_handle = NULL;
    disp_cfg.buffer_size = DISPLAY_WIDTH * 20;
    disp_cfg.double_buffer = 0;
    disp_cfg.trans_size = 0;
    disp_cfg.hres = DISPLAY_WIDTH;
    disp_cfg.vres = DISPLAY_HEIGHT;
    disp_cfg.monochrome = false;
    disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;

    disp_cfg.rotation.swap_xy = true;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = false;

    disp_cfg.flags.buff_dma = 1;
    disp_cfg.flags.buff_spiram = 0;
    disp_cfg.flags.sw_rotate = 0;
    disp_cfg.flags.swap_bytes = 1;
    disp_cfg.flags.full_refresh = 0;
    disp_cfg.flags.direct_mode = 0;

    disp_handle = lvgl_port_add_disp(&disp_cfg);
    lv_display_set_offset(disp_handle, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
}

void lvgl_ui(void)
{
    lvgl_port_lock(0);  //锁住lvgl
    // 创建主屏幕对象
    lvgl_screen.screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(lvgl_screen.screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // 创建容器
    lvgl_screen.container = lv_obj_create(lvgl_screen.screen);
    lv_obj_set_size(lvgl_screen.container, DISPLAY_WIDTH-2, DISPLAY_HEIGHT-2);
    lv_obj_center(lvgl_screen.container);                                                   // 居中
    //隐藏容器的右滑条
    lv_obj_set_style_pad_all(lvgl_screen.container, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(lvgl_screen.container, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // 白色背景
    lv_obj_set_flex_flow(lvgl_screen.container, LV_FLEX_FLOW_COLUMN);                       // 垂直排列
    lv_obj_set_scroll_dir(lvgl_screen.container, LV_DIR_VER);                               // 垂直滚动
    lv_obj_set_scrollbar_mode(lvgl_screen.container, LV_SCROLLBAR_MODE_AUTO);               // 自动滚动
    lvgl_port_unlock(); //解锁lvgl
}

//每次调用这个函数，就在容器中创建一个标签，标签的内容就是text，标签一行不够可以换行，标签背景色为绿色
void lvgl_ui_label_set_text(const char *text)
{
    lvgl_port_lock(0); 
    lv_obj_t *btn = lv_btn_create(lvgl_screen.container);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), LV_STATE_DEFAULT);
    lv_obj_set_width(btn, lv_pct(100));                                          // 占满宽度
    lv_obj_set_height(btn, LV_SIZE_CONTENT);                                     // 高度自适应内容
    lv_obj_set_style_radius(btn, 5, LV_STATE_DEFAULT);                           // 圆角

    lv_obj_t *label = lv_label_create(btn);                                      // 创建标签
    lv_label_set_text(label, text);                                              // 设置标签内容
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_STATE_DEFAULT);// 设置标签字体为黑色
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);     // 设置字体大小为20
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);        // 文字靠左
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);                           // 自动换行
    lv_obj_set_width(label, lv_pct(100));                                        // 占满宽度
    lv_obj_set_height(label, LV_SIZE_CONTENT);                                   // 高度自适应内容

    lv_obj_set_style_pad_column(lvgl_screen.container, 10, LV_PART_MAIN);

    const uint8_t max_labels = 5;
    if (lv_obj_get_child_cnt(lvgl_screen.container) > max_labels) {
        lv_obj_t *first_child = lv_obj_get_child(lvgl_screen.container, 0);
        if (first_child) {
            lv_obj_del(first_child);
        }
    }
    lv_obj_update_layout(lvgl_screen.container);

    int index = lv_obj_get_child_cnt(lvgl_screen.container);
    if(index > 0)
    {
        lv_obj_t *last_child = lv_obj_get_child(lvgl_screen.container, index - 1);
        lv_coord_t visible_height = lv_obj_get_content_height(lvgl_screen.container); 
        lv_coord_t y_aligned = lv_obj_get_y(last_child) - (visible_height / 2) + (lv_obj_get_height(last_child) / 2);
        lv_obj_scroll_to_y(lvgl_screen.container, y_aligned, LV_ANIM_OFF);
    }
    lvgl_port_unlock(); 
}