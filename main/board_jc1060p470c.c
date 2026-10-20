#include "board_jc1060p470c.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_jd9165.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "fonts/cyrillic_fonts.h"

static const char *TAG = "BOARD_JC1060P470C";

// Статические дескрипторы оборудования
static esp_ldo_channel_handle_t s_dsi_ldo = NULL;
static esp_lcd_dsi_bus_handle_t s_dsi_bus = NULL;
static esp_lcd_panel_io_handle_t s_panel_io = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;
static lv_display_t *s_lv_display = NULL;
static lv_indev_t *s_lv_touch_indev = NULL;

/* =========================================================================
 * 1. ВЕНДОРСКАЯ ТАБЛИЦА ИНИЦИАЛИЗАЦИИ ДИСПЛЕЯ JD9165 (1024x600 IPS)
 * ========================================================================= */
static const jd9165_lcd_init_cmd_t s_jd9165_init_cmds[] = {
    {0x30, (uint8_t[]){0x00}, 1, 0},
    {0xF7, (uint8_t[]){0x49, 0x61, 0x02, 0x00}, 4, 0},
    {0x30, (uint8_t[]){0x01}, 1, 0},
    {0x04, (uint8_t[]){0x0C}, 1, 0},
    {0x05, (uint8_t[]){0x00}, 1, 0},
    {0x06, (uint8_t[]){0x00}, 1, 0},
    {0x0B, (uint8_t[]){0x11}, 1, 0},
    {0x17, (uint8_t[]){0x00}, 1, 0},
    {0x20, (uint8_t[]){0x04}, 1, 0},
    {0x1F, (uint8_t[]){0x05}, 1, 0},
    {0x23, (uint8_t[]){0x00}, 1, 0},
    {0x25, (uint8_t[]){0x19}, 1, 0},
    {0x28, (uint8_t[]){0x18}, 1, 0},
    {0x29, (uint8_t[]){0x04}, 1, 0},
    {0x2A, (uint8_t[]){0x01}, 1, 0},
    {0x2B, (uint8_t[]){0x04}, 1, 0},
    {0x2C, (uint8_t[]){0x01}, 1, 0},
    {0x30, (uint8_t[]){0x02}, 1, 0},
    {0x01, (uint8_t[]){0x22}, 1, 0},
    {0x03, (uint8_t[]){0x12}, 1, 0},
    {0x04, (uint8_t[]){0x00}, 1, 0},
    {0x05, (uint8_t[]){0x64}, 1, 0},
    {0x0A, (uint8_t[]){0x08}, 1, 0},
    {0x0B, (uint8_t[]){0x0A, 0x1A, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x06, 0x08, 0x1F, 0x1D}, 11, 0},
    {0x0C, (uint8_t[]){0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}, 11, 0},
    {0x0D, (uint8_t[]){0x16, 0x1B, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x07, 0x09, 0x1E, 0x1C}, 11, 0},
    {0x0E, (uint8_t[]){0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}, 11, 0},
    {0x0F, (uint8_t[]){0x16, 0x1B, 0x0D, 0x0B, 0x0D, 0x11, 0x10, 0x1C, 0x1E, 0x09, 0x07}, 11, 0},
    {0x10, (uint8_t[]){0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}, 11, 0},
    {0x11, (uint8_t[]){0x0A, 0x1A, 0x0D, 0x0B, 0x0D, 0x11, 0x10, 0x1D, 0x1F, 0x08, 0x06}, 11, 0},
    {0x12, (uint8_t[]){0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D}, 11, 0},
    {0x14, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0x18, (uint8_t[]){0x99}, 1, 0},
    {0x30, (uint8_t[]){0x06}, 1, 0},
    {0x12, (uint8_t[]){0x36, 0x2C, 0x2E, 0x3C, 0x38, 0x35, 0x35, 0x32, 0x2E, 0x1D, 0x2B, 0x21, 0x16, 0x29}, 14, 0},
    {0x13, (uint8_t[]){0x36, 0x2C, 0x2E, 0x3C, 0x38, 0x35, 0x35, 0x32, 0x2E, 0x1D, 0x2B, 0x21, 0x16, 0x29}, 14, 0},
    {0x30, (uint8_t[]){0x0A}, 1, 0},
    {0x02, (uint8_t[]){0x4F}, 1, 0},
    {0x0B, (uint8_t[]){0x40}, 1, 0},
    {0x12, (uint8_t[]){0x3E}, 1, 0},
    {0x13, (uint8_t[]){0x78}, 1, 0},
    {0x30, (uint8_t[]){0x0D}, 1, 0},
    {0x0D, (uint8_t[]){0x04}, 1, 0},
    {0x10, (uint8_t[]){0x0C}, 1, 0},
    {0x11, (uint8_t[]){0x0C}, 1, 0},
    {0x12, (uint8_t[]){0x0C}, 1, 0},
    {0x13, (uint8_t[]){0x0C}, 1, 0},
    {0x30, (uint8_t[]){0x00}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120}, // Выход из Sleep Mode
    {0x29, (uint8_t[]){0x00}, 1, 20},  // Включение дисплея
};

/* =========================================================================
 * 2. ПОДСВЕТКА ДИСПЛЕЯ (LEDC ШИМ)
 * ========================================================================= */
esp_err_t board_backlight_init(void)
{
    ESP_LOGI(TAG, "Инициализация ШИМ подсветки на GPIO %d...", BOARD_LCD_BACKLIGHT_GPIO);
    const ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    const ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BOARD_LCD_BACKLIGHT_GPIO,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    return board_backlight_set_percent(85); // 85% по умолчанию
}

esp_err_t board_backlight_set_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (1023 * percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* =========================================================================
 * 3. ПИТАНИЕ MIPI-DSI PHY (LDO REGULATOR КАНАЛ 3 @ 2500 мВ)
 * ========================================================================= */
static esp_err_t board_enable_dsi_phy_power(void)
{
    esp_ldo_channel_config_t ldo_config = {
        .chan_id = BOARD_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BOARD_MIPI_DSI_PHY_PWR_LDO_MV,
    };
    ESP_LOGI(TAG, "Включение питания MIPI-DSI PHY: LDO ch%d @ %d мВ...",
             BOARD_MIPI_DSI_PHY_PWR_LDO_CHAN, BOARD_MIPI_DSI_PHY_PWR_LDO_MV);
    return esp_ldo_acquire_channel(&ldo_config, &s_dsi_ldo);
}

/* =========================================================================
 * 4. ШИНА I2C И ЕМКОСТНЫЙ ТАЧСКРИН GT911
 * ========================================================================= */
// Диагностический перехватчик чтения координат с сенсора GT911
static esp_err_t (*s_orig_touch_read_data)(esp_lcd_touch_handle_t) = NULL;
static int64_t s_last_touch_log_us = 0;

static esp_err_t touch_read_data_logger(esp_lcd_touch_handle_t tp)
{
    if (!s_orig_touch_read_data) {
        return ESP_OK;
    }
    esp_err_t err = s_orig_touch_read_data(tp);
    if (err == ESP_OK && tp->data.points > 0) {
        int64_t now = esp_timer_get_time();
        if (now - s_last_touch_log_us > 250000) { // Не спамить чаще 4 раз в секунду
            s_last_touch_log_us = now;
            ESP_LOGI("GT911_TOUCH", "Касание: точек=%u, X=%u, Y=%u",
                     tp->data.points, tp->data.coords[0].x, tp->data.coords[0].y);
        }
    }
    return err;
}

static esp_err_t board_i2c_and_touch_init(void)
{
    ESP_LOGI(TAG, "Инициализация I2C Master bus (SDA=%d, SCL=%d)...",
             BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO);

    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus));

    // Проверка адреса тачскрина GT911 (0x5D или 0x14)
    const uint8_t gt_addrs[] = {
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,        // 0x5D
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP  // 0x14
    };
    uint8_t found_addr = 0;
    for (int i = 0; i < 2; i++) {
        if (i2c_master_probe(s_i2c_bus, gt_addrs[i], 100) == ESP_OK) {
            found_addr = gt_addrs[i];
            ESP_LOGI(TAG, "GT911 обнаружен на адресе I2C: 0x%02X", found_addr);
            break;
        }
    }

    if (found_addr == 0) {
        ESP_LOGW(TAG, "GT911 не ответил на probe (будет использован адрес по умолчанию 0x5D)");
        found_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
    }

    // Для прямой ориентации дисплея (0,0 в левом верхнем углу)
    // флаги зеркалирования должны быть 0.
    esp_lcd_touch_config_t touch_config = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    touch_io_config.dev_addr = found_addr;
    touch_io_config.scl_speed_hz = 400000;

    static esp_lcd_touch_io_gt911_config_t gt911_io_cfg = {};
    struct TouchAttempt {
        gpio_num_t rst;
        gpio_num_t int_gpio;
    };
    // 1) Сначала без reset: чип уже проснулся и ответил на probe
    // 2) Если не вышло — полный reset с пинами RST и INT
    const struct TouchAttempt attempts[] = {
        { GPIO_NUM_NC,          GPIO_NUM_NC          },
        { BOARD_TOUCH_RST_GPIO, BOARD_TOUCH_INT_GPIO },
    };

    esp_err_t err = ESP_FAIL;
    esp_lcd_panel_io_handle_t touch_io = NULL;

    for (int i = 0; i < 2; i++) {
        touch_config.rst_gpio_num = attempts[i].rst;
        touch_config.int_gpio_num = attempts[i].int_gpio;

        if (attempts[i].rst != GPIO_NUM_NC) {
            gt911_io_cfg.dev_addr = found_addr;
            touch_config.driver_data = &gt911_io_cfg;
        } else {
            touch_config.driver_data = NULL;
        }

        if (touch_io != NULL) {
            esp_lcd_panel_io_del(touch_io);
            touch_io = NULL;
        }

        err = esp_lcd_new_panel_io_i2c(s_i2c_bus, &touch_io_config, &touch_io);
        if (err != ESP_OK) {
            continue;
        }

        err = esp_lcd_touch_new_i2c_gt911(touch_io, &touch_config, &s_touch_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Драйвер тачскрина GT911 успешно создан (попытка %d)!", i + 1);
            s_orig_touch_read_data = s_touch_handle->read_data;
            s_touch_handle->read_data = touch_read_data_logger;
            break;
        }
        ESP_LOGW(TAG, "GT911 попытка %d не удалась: %s", i + 1, esp_err_to_name(err));
    }

    return err;
}

/* =========================================================================
 * 5. ПОЛНАЯ ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ MIPI-DSI JD9165 И LVGL 9
 * ========================================================================= */
lv_display_t *board_jc1060p470c_init(void)
{
    ESP_LOGI(TAG, ">>> Запуск инициализации платы GUITION JC1060P470C <<<");

    // 1. Подаем питание на MIPI-DSI PHY
    ESP_ERROR_CHECK(board_enable_dsi_phy_power());

    // 2. Создаем шину MIPI-DSI (2 линии данных, 750 Мбит/с)
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .lane_bit_rate_mbps = 750,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &s_dsi_bus));

    // 3. Создаем DBI IO для отправки команд контроллеру панели
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_config, &s_panel_io));

    // 4. Настраиваем тайминги DPI (52 МГц, 1024x600)
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 52,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = BOARD_LCD_H_RES,
            .v_size = BOARD_LCD_V_RES,
            .hsync_pulse_width = 24,
            .hsync_back_porch = 160,
            .hsync_front_porch = 160,
            .vsync_pulse_width = 2,
            .vsync_back_porch = 21,
            .vsync_front_porch = 12,
        },
    };

    // 5. Вендорская конфигурация JD9165
    jd9165_vendor_config_t vendor_config = {
        .init_cmds = s_jd9165_init_cmds,
        .init_cmds_size = sizeof(s_jd9165_init_cmds) / sizeof(jd9165_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = s_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RESET_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Создание панели JD9165 1024x600...");
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9165(s_panel_io, &panel_config, &s_panel_handle));

    // Включаем аппаратное ускорение DMA2D для копирования буфера кадра
    ESP_LOGI(TAG, "Активация аппаратного ускорения DMA2D...");
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_enable_dma2d(s_panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_LOGI(TAG, "Панель JD9165 успешно запущена!");

    // 6. Инициализация подсветки
    board_backlight_init();

    // 7. Инициализация тачскрина GT911
    board_i2c_and_touch_init();

    // 8. Инициализация графического порта LVGL 9 для MIPI-DSI
    ESP_LOGI(TAG, "Инициализация подсистемы esp_lvgl_port (MIPI-DSI)...");
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_stack = 16384;
    lvgl_cfg.task_priority = 4;
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_panel_io,
        .panel_handle = s_panel_handle,
        .buffer_size = BOARD_LCD_H_RES * 50, // Буфер на 50 строк
        .double_buffer = false,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = true,
            .swap_bytes = false, // ВАЖНО: FALSE для правильных цветов RGB565 на MIPI-DSI!
        }
    };

    const lvgl_port_display_dsi_cfg_t dsi_disp_cfg = {
        .flags = {
            .avoid_tearing = false,
        }
    };

    // Для MIPI-DSI дисплея вызывается специализированная функция lvgl_port_add_disp_dsi
    s_lv_display = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_disp_cfg);
    if (!s_lv_display) {
        ESP_LOGE(TAG, "Ошибка: не удалось зарегистрировать DSI дисплей в LVGL!");
        return NULL;
    }

    // Устанавливаем тему интерфейса по умолчанию с кириллическим шрифтом DejaVu 16
    lvgl_port_lock(0);
    lv_theme_t *th = lv_theme_default_init(
        s_lv_display,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_CYAN),
        true,
        &lv_font_dejavu_16
    );
    lv_display_set_theme(s_lv_display, th);
    lvgl_port_unlock();
    ESP_LOGI(TAG, "Тема LVGL 9 с кириллическим шрифтом DejaVu Sans 16 установлена!");

    // 9. Привязка тачскрина к LVGL
    if (s_touch_handle) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = s_lv_display,
            .handle = s_touch_handle,
        };
        s_lv_touch_indev = lvgl_port_add_touch(&touch_cfg);
        if (s_lv_touch_indev) {
            ESP_LOGI(TAG, "Сенсорный экран GT911 успешно привязан к LVGL 9!");
        } else {
            ESP_LOGW(TAG, "Не удалось добавить устройство ввода тачскрина в LVGL");
        }
    }

    ESP_LOGI(TAG, ">>> Плата JC1060P470C и LVGL 9 полностью готовы к работе! <<<");
    return s_lv_display;
}
