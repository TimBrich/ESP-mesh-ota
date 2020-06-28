#include <cstring>
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "wifi_initialise.h"

Wifi_initialise::Wifi_initialise(char* ROUTER_SSID, char* ROUTER_PASSWD)
{
    strcpy(Wifi_initialise::ROUTER_SSID, ROUTER_SSID);
    strcpy(Wifi_initialise::ROUTER_PASSWD, ROUTER_PASSWD);
}

Wifi_initialise::~Wifi_initialise(void)
{
}

void Wifi_initialise::wifi_start()
{
    ESP_ERROR_CHECK(esp_wifi_start());
}

void Wifi_initialise::wifi_stop()
{
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
}

void Wifi_initialise::wifi_restore()
{
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_restore());
}

void Wifi_initialise::wifi_deinit()
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}