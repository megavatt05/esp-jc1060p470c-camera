# Пульт EASYDRIVE: разметку генерирует EEZ Studio
# (C:\Users\megav\Downloads\eez-winder-hmi\WinderHMI.eez-project, сборка
# "EEZ Studio.exe --build-project"), прошивка реализует только
# get_var_*/set_var_* и action_* в eez_ui/eez_glue.c.
# Предыдущая версия на SquareLine Studio осталась в main/ui и main/ui_easydrive.c,
# но в сборку не входит: оба интерфейса дают одинаковые символы ui_init()/ui_tick().
set(EEZ_UI_SRCS
    eez_ui/ui.c
    eez_ui/screens.c
    eez_ui/styles.c
    eez_ui/images.c
    eez_ui/eez_glue.c
    eez_ui/fonts/ui_font_body16.c
    eez_ui/fonts/ui_font_label20.c
    eez_ui/fonts/ui_font_head26.c
    eez_ui/fonts/ui_font_big56.c
)

idf_component_register(
    SRCS
        "main.c"
        "board_jc1060p470c.c"
        "idm20_encoder.c"
        "vfd_easydrive.c"
        ${EEZ_UI_SRCS}
        "fonts/lv_font_dejavu_16.c"
        "fonts/lv_font_dejavu_24.c"
        "fonts/lv_font_dejavu_32.c"
        "fonts/lv_font_digits_48.c"
        "fonts/lv_font_digits_72.c"
        "fonts/lv_font_digits_120.c"
    INCLUDE_DIRS "." "fonts" "eez_ui"
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
