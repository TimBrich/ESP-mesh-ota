#ifndef _OTA_UPDATE_H
#define _OTA_UPDATE_H

#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "esp_http_client.h"

#include "wifi_initialise.h"

#define BLINK_GPIO CONFIG_BLINK_GPIO
static const char *OTA_TAG = "ota_main";
static EventGroupHandle_t wifi_event_group = xEventGroupCreate();
static char rcv_buffer[200];

class Ota_update: private Wifi_initialise {
public:
    const int CONNECTED_BIT;
    char url[200];
    char firmware_name[100];
    int version;
    bool auto_restart;

    Ota_update(char* MESH_ROUTER_SSID,
                char* MESH_ROUTER_PASSWD,
                char *server,
                int version,
                char* firmware_name,
                const int CONNECTED_BIT);
    ~Ota_update();

    static esp_err_t _http_event_handler(esp_http_client_event_t *evt);
    static void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data);

    void wifi_initialise();
    int wifi_wait_connect();
    int ota_init();
    void ota_deinit();
    int ota_update();

    void set_auto_restart(bool arg);
    bool get_auto_restart();
    void restart();

    int chek_update(char *_url);
    int update(char* url);
};

#endif