#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_STANOK_ENABLE_TEST_ENDPOINTS

/**
 * @brief Инициализация подсистемы эндпоинтов тестирования и управления
 */
esp_err_t test_endpoints_init(void);

/**
 * @brief Включение/выключение программного эмулятора частотника Easydrive
 */
void test_endpoints_set_sim_vfd(bool enable);
bool test_endpoints_is_sim_vfd(void);

/**
 * @brief Включение/выключение программного эмулятора энкодера ИДМ-20
 */
void test_endpoints_set_sim_encoder(bool enable);
bool test_endpoints_is_sim_encoder(void);

/**
 * @brief Принудительная установка текущей длины намотки (для тестирования отсечек)
 */
void test_endpoints_simulate_length(float meters);

/**
 * @brief Имитация аварии частотника (код Er01..Er19 или 0 для сброса)
 */
void test_endpoints_inject_fault(uint8_t fault_code);

/**
 * @brief Выполнение команды управления станком (start, stop, jog, rev, reset)
 */
void test_endpoints_execute_cmd(const char *cmd_name);

/**
 * @brief Формирование строки JSON с полной телеметрией станка
 */
char *test_endpoints_get_status_json(char *buf, size_t max_len);

#else

// Если эндпоинты отключены в Kconfig — нулевые статические заглушки
static inline esp_err_t test_endpoints_init(void) { return ESP_OK; }
static inline void test_endpoints_set_sim_vfd(bool enable) { (void)enable; }
static inline bool test_endpoints_is_sim_vfd(void) { return false; }
static inline void test_endpoints_set_sim_encoder(bool enable) { (void)enable; }
static inline bool test_endpoints_is_sim_encoder(void) { return false; }
static inline void test_endpoints_simulate_length(float meters) { (void)meters; }
static inline void test_endpoints_inject_fault(uint8_t fault_code) { (void)fault_code; }
static inline void test_endpoints_execute_cmd(const char *cmd_name) { (void)cmd_name; }
static inline char *test_endpoints_get_status_json(char *buf, size_t max_len) {
    if (buf && max_len > 0) buf[0] = '\0';
    return buf;
}

#endif // CONFIG_STANOK_ENABLE_TEST_ENDPOINTS

#ifdef __cplusplus
}
#endif
