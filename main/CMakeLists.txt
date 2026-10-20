idf_component_register(
    SRCS
        "main.c"
        "board_jc1060p470c.c"
        "idm20_encoder.c"
        "vfd_easydrive.c"
        "ui_winder.c"
        "fonts/lv_font_dejavu_16.c"
        "fonts/lv_font_dejavu_24.c"
        "fonts/lv_font_dejavu_32.c"
        "fonts/lv_font_digits_48.c"
        "fonts/lv_font_digits_72.c"
        "fonts/lv_font_digits_120.c"
    INCLUDE_DIRS "." "fonts"
    REQUIRES
        esp_lcd
        esp_lvgl_port
        lvgl
        nvs_flash
        esp_timer
        esp_hw_support
        esp_driver_ledc
        esp_driver_uart
        esp_driver_pcnt
        esp_driver_i2c
        esp_driver_gpio
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_CONF_INCLUDE_SIMPLE")
