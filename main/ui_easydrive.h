#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Построение панели EASYDRIVE (код сгенерирован SquareLine Studio в main/ui)
 * и подключение её к движку Modbus из vfd_easydrive.c.
 * Вызывать из задачи app_main под lvgl_port_lock().
 */
void easydrive_ui_init(void);

#ifdef __cplusplus
}
#endif
