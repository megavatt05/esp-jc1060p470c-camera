#include "wifi_manager.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_lvgl_port.h"

#include "ui_stopwatch.h"
#include "ui_test_patterns.h"
#include "board_jc1060p470c.h"

static const char *TAG = "WIFI_MANAGER";

#define WIFI_AP_SSID      "JC1060-CAMERA-STAND"
#define WIFI_AP_CHANNEL   1
#define WIFI_MAX_STA      4

static httpd_handle_t s_server = NULL;
static char s_ap_ip_str[32] = "192.168.4.1";

// =========================================================================
// HTML / JS ДАШБОРД УПРАВЛЕНИЯ ПЛАТОЙ (Адаптив под мобильные телефоны)
// =========================================================================
static const char *s_index_html = 
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>JC1060P470C Wi-Fi 6 Панель управления</title>"
"<style>"
"body{background:#0b0f19;color:#e2e8f0;font-family:sans-serif;margin:0;padding:16px;text-align:center}"
"h2{color:#00ff41;margin:10px 0}"
".card{background:#151d30;border-radius:12px;padding:16px;margin-bottom:16px;box-shadow:0 4px 12px rgba(0,0,0,0.4)}"
".time{font-family:monospace;font-size:32px;color:#ffe600;margin:10px 0}"
"button{background:#1e3a5f;color:#fff;border:none;border-radius:8px;padding:12px 18px;font-size:16px;margin:6px;cursor:pointer;transition:0.2s}"
"button:hover{background:#2a4f80}"
"button.active{background:#008272;font-weight:bold}"
"button.stop{background:#d83b01}"
"button.start{background:#107c41}"
"button.blue{background:#0078d4}"
".slider-box{margin:15px 0}"
"input[type=range]{width:80%;max-width:320px}"
".badge{display:inline-block;padding:4px 8px;border-radius:4px;background:#00ff41;color:#000;font-weight:bold;font-size:12px}"
"</style></head><body>"
"<h2>🛠️ JC1060P470C Стенд Камеры</h2>"
"<div class='card'>"
"  <div>Время эталона:</div>"
"  <div class='time' id='time_val'>00.000 000</div>"
"  <div>Статус: <span id='status_badge' class='badge'>ПОДКЛЮЧЕНО</span> | Режим: <b id='mode_name'>СЕКУНДОМЕР</b></div>"
"  <div style='margin-top:12px'>"
"    <button class='start' onclick='sendCmd(\"start\")'>СТАРТ</button>"
"    <button class='stop' onclick='sendCmd(\"stop\")'>ПАУЗА</button>"
"    <button class='blue' onclick='sendCmd(\"reset\")'>СБРОС</button>"
"    <button style='background:#8764b8' onclick='sendCmd(\"mark\")'>MARK</button>"
"  </div>"
"</div>"
"<div class='card'>"
"  <h3>📊 Тестовые калибровочные таблицы</h3>"
"  <button onclick='sendCmd(\"pattern=0\")'>⏱️ Секундомер</button>"
"  <button onclick='sendCmd(\"pattern=1\")'>🎯 Фокус (Siemens)</button>"
"  <button onclick='sendCmd(\"pattern=2\")'>🌈 Цвет (Color Bars)</button>"
"  <button onclick='sendCmd(\"pattern=3\")'>📐 Сетка (Distortion)</button>"
"  <button onclick='sendCmd(\"pattern=4\")'>🌓 Серый клин (WDR)</button>"
"</div>"
"<div class='card'>"
"  <h3>💡 Яркость подсветки экрана: <span id='br_val'>20</span>%</h3>"
"  <div class='slider-box'>"
"    <input type='range' min='2' max='95' value='20' id='br_slider' oninput='updateBright(this.value)'>"
"  </div>"
"  <div style='margin-top:8px'>"
"    <button onclick='setBright(10)'>10% (Ночь)</button>"
"    <button onclick='setBright(20)'>20% (Норма)</button>"
"    <button onclick='setBright(50)'>50%</button>"
"    <button onclick='setBright(85)'>85% (День)</button>"
"  </div>"
"</div>"
"<script>"
"function sendCmd(param){fetch('/api/cmd?' + param);updateStatus();}"
"function updateBright(val){document.getElementById('br_val').innerText = val;fetch('/api/cmd?bright=' + val);}"
"function setBright(val){document.getElementById('br_slider').value = val;updateBright(val);}"
"function updateStatus(){"
"  fetch('/api/status').then(r=>r.json()).then(d=>{"
"    let s = Math.floor(d.time_us / 1000000); let us = d.time_us % 1000000;"
"    let ms = Math.floor(us / 1000); let rem_us = us % 1000;"
"    document.getElementById('time_val').innerText = (s<10?'0':'')+s+'.'+(ms<100?'0':'')+(ms<10?'0':'')+ms+' '+(rem_us<100?'0':'')+(rem_us<10?'0':'')+rem_us;"
"    document.getElementById('mode_name').innerText = d.pattern_name;"
"    document.getElementById('br_val').innerText = d.brightness;"
"    document.getElementById('br_slider').value = d.brightness;"
"  }).catch(e=>{});"
"}"
"setInterval(updateStatus, 500);"
"</script></body></html>";

// =========================================================================
// ОБРАБОТЧИКИ HTTP МАРШРУТОВ
// =========================================================================

// GET / - Главная страница
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, s_index_html, HTTPD_RESP_USE_STRLEN);
}

// GET /api/status - JSON статус
static esp_err_t status_get_handler(httpd_req_t *req)
{
    char resp[256];
    int64_t t = ui_stopwatch_get_time_us();
    pattern_mode_t pat = ui_test_patterns_get_mode();
    snprintf(resp, sizeof(resp),
             "{\"time_us\":%lld,\"running\":%s,\"pattern\":%d,\"pattern_name\":\"%s\",\"brightness\":20,\"ip\":\"%s\"}",
             (long long)t, ui_stopwatch_is_running() ? "true" : "false",
             pat, ui_test_patterns_get_name(pat),
             s_ap_ip_str);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

// GET /api/cmd - Выполнение команды
static esp_err_t cmd_get_handler(httpd_req_t *req)
{
    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(query, "pattern", param, sizeof(param)) == ESP_OK) {
            if (lvgl_port_lock(0)) {
                ui_test_patterns_cmd(param);
                lvgl_port_unlock();
            }
        } else if (httpd_query_key_value(query, "bright", param, sizeof(param)) == ESP_OK) {
            int br = 20;
            if (sscanf(param, "%d", &br) == 1) {
                if (br < 2) br = 2;
                if (br > 95) br = 95;
                board_backlight_set_percent((uint8_t)br);
            }
        } else if (strstr(query, "start")) {
            ui_stopwatch_start();
        } else if (strstr(query, "stop")) {
            ui_stopwatch_stop();
        } else if (strstr(query, "reset")) {
            ui_stopwatch_reset();
        } else if (strstr(query, "mark")) {
            ui_stopwatch_trigger_mark();
        }
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t captive_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uris[] = {
            { .uri = "/",                          .method = HTTP_GET, .handler = root_get_handler },
            { .uri = "/api/status",                .method = HTTP_GET, .handler = status_get_handler },
            { .uri = "/api/cmd",                   .method = HTTP_GET, .handler = cmd_get_handler },
            { .uri = "/generate_204",              .method = HTTP_GET, .handler = captive_handler },
            { .uri = "/gen_204",                   .method = HTTP_GET, .handler = captive_handler },
            { .uri = "/hotspot-detect.html",       .method = HTTP_GET, .handler = captive_handler },
            { .uri = "/library/test/success.html", .method = HTTP_GET, .handler = captive_handler },
            { .uri = "/ncsi.txt",                  .method = HTTP_GET, .handler = captive_handler },
            { .uri = "/connecttest.txt",           .method = HTTP_GET, .handler = captive_handler },
            { .uri = "/canonical.html",            .method = HTTP_GET, .handler = captive_handler },
            { .uri = "/favicon.ico",               .method = HTTP_GET, .handler = root_get_handler },
        };
        for (size_t i = 0; i < sizeof(uris)/sizeof(uris[0]); i++) {
            httpd_register_uri_handler(server, &uris[i]);
        }

        ESP_LOGI(TAG, "HTTP Веб-сервер управления успешно запущен на порту 80 (с Captive Portal)");
        return server;
    }

    ESP_LOGE(TAG, "Ошибка запуска HTTP веб-сервера");
    return NULL;
}

// =========================================================================
// ИНИЦИАЛИЗАЦИЯ WI-FI 6 (ЧИСТЫЙ АВТОНОМНЫЙ SOFTAP НА КАНАЛЕ 1)
// =========================================================================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Клиент ПОДКЛЮЧИЛСЯ к SoftAP: %02x:%02x:%02x:%02x:%02x:%02x (AID=%d)",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Клиент ОТКЛЮЧИЛСЯ от SoftAP: %02x:%02x:%02x:%02x:%02x:%02x (AID=%d)",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5], event->aid);
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Инициализация аппаратного радиомодуля ESP32-C6 (GPIO54 RST / SDIO)...");

    // Аппаратный сброс и пробуждение чипа ESP32-C6 через GPIO54
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 54),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(54, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(54, 1);
    vTaskDelay(pdMS_TO_TICKS(200)); // Даем чипу C6 инициализироваться

    ESP_LOGI(TAG, "Инициализация сетевого стека LwIP и Wi-Fi 6 (SoftAP)...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Создаем интерфейс SoftAP
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "Ошибка создания сетевого интерфейса SoftAP");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_MAX_STA,
            .authmode = WIFI_AUTH_OPEN, // Открытая сеть: мгновенный коннект без WPA-зависаний
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  ТОЧКА ДОСТУПА WI-FI 6 СТЕНДА УСПЕШНО ЗАПУЩЕНА!");
    ESP_LOGI(TAG, "  Имя сети (SSID):  %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "  Пароль:           (БЕЗ ПАРОЛЯ, открытая сеть)");
    ESP_LOGI(TAG, "  Канал:            %d (2.4 GHz)", WIFI_AP_CHANNEL);
    ESP_LOGI(TAG, "  IP адрес стенда:  http://%s/", s_ap_ip_str);
    ESP_LOGI(TAG, "============================================================");

    // Запускаем HTTP сервер управления на порту 80
    s_server = start_webserver();

    return ESP_OK;
}

const char* wifi_manager_get_ip(void)
{
    return s_ap_ip_str;
}

const char* wifi_manager_get_sta_ip(void)
{
    return s_ap_ip_str;
}

bool wifi_manager_is_sta_connected(void)
{
    return false;
}
