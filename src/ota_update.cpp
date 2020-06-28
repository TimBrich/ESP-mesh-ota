#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "ota_update.h"

// server certificates
extern const char client_cert_pem_start[] asm("_binary_client_cert_pem_start");
extern const char client_cert_pem_end[] asm("_binary_client_cert_pem_end");
extern const char client_key_pem_start[] asm("_binary_client_key_pem_start");
extern const char client_key_pem_end[] asm("_binary_client_key_pem_end");

Ota_update::Ota_update(char* MESH_ROUTER_SSID,
            char* MESH_ROUTER_PASSWD,
            char *server,
            int version,
            char* firmware_name,
            const int _CONNECTED_BIT) : 
            Wifi_initialise(MESH_ROUTER_SSID, MESH_ROUTER_PASSWD), CONNECTED_BIT(_CONNECTED_BIT)
{
    Ota_update::version = version;
    strcpy(Ota_update::firmware_name, firmware_name);

    char buf[255];
    itoa(version, buf, 10);

    strcat(Ota_update::url, "https://");
    strcat(Ota_update::url, server);
    strcat(Ota_update::url, "/getUpdateInfo&version=");
    strcat(Ota_update::url, buf);
    strcat(Ota_update::url, "&name=");
    strcat(Ota_update::url, firmware_name);

    auto_restart = true;
}

Ota_update::~Ota_update() {}

esp_err_t Ota_update::_http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
			printf("error\n");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
				strncpy(rcv_buffer, (char*)evt->data, evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
    }
    return ESP_OK;
}

void Ota_update::ip_event_handler(void *arg, esp_event_base_t event_base,
                    int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(OTA_TAG, "<WIFI_EVENT_STA_START>");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(OTA_TAG, "<WIFI_EVENT_DISCONECT>");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        int bit = (int) arg;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(OTA_TAG, "<IP_EVENT_STA_GOT_IP&&setBIT%d>IP:%s", bit, ip4addr_ntoa(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, bit);
    }
}

void Ota_update::wifi_initialise()
{
    tcpip_adapter_init();
	wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, ROUTER_SSID, sizeof(ROUTER_SSID));
    memcpy(wifi_config.sta.password, ROUTER_PASSWD, sizeof(ROUTER_PASSWD));

    wifi_init_config_t cfg2 = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg2));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, (void *) CONNECTED_BIT));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, (void *) CONNECTED_BIT));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
}

void Ota_update::ota_deinit()
{
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ip_event_handler);
    esp_event_handler_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP, ip_event_handler);
    wifi_deinit();
}

int Ota_update::chek_update(char *_url)
{   
    int client_count = 0;

    esp_http_client_config_t config = {
    .url = url, 
    .port = 443,
    .cert_pem = client_cert_pem_start,
    .event_handler = _http_event_handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    while(1) {
        // downloading the json file
        esp_err_t err;
        err = esp_http_client_perform(client);  
        if(err != ESP_OK) 
        {
            client_count++;
            ESP_LOGI(OTA_TAG, "fail esp_http_client_perform");
            if(client_count == 3)
            {
                return 0;
            }
        }
        else break;
    }

    // parse the json file	
    cJSON *json = cJSON_Parse(rcv_buffer);
    if(json == NULL) return 0;

    cJSON *ans = cJSON_GetObjectItemCaseSensitive(json, "ans");
    cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "url");

    ESP_LOGI(OTA_TAG,"server ans: %s", ans->valuestring);

    const char* chek = "false";
    
    if(file == NULL) {
        esp_http_client_cleanup(client);
        return 0;
    }
    else {
        ESP_LOGI(OTA_TAG,"server url: %s\n", file->valuestring);	
        esp_http_client_cleanup(client);
        strcpy(_url, file->valuestring);
        return 1;
    }

    return 0;
}

int Ota_update::ota_init() {
    wifi_initialise();
    wifi_start();

    for(int i = 0; i <= 3; i++)
        if(wifi_wait_connect()) return 1;

    ESP_LOGI(OTA_TAG, "error: not connect");
    return 0;
}

int Ota_update::update(char* _url)
{
    esp_http_client_config_t ota_client_config = {
        .url = _url,
        .cert_pem = client_cert_pem_start,
    };
    esp_err_t ret = esp_https_ota(&ota_client_config);
    if (ret == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA OK, restarting...");
        this->ota_deinit();
        if(auto_restart) esp_restart();
        else return 1;
    } else {
        ESP_LOGI(OTA_TAG, "OTA failed...");
        return -1;
    }
    return -1;
}

int Ota_update::ota_update()
{
    char url[255] = {0};
    if (chek_update(url))
    {
        ESP_LOGI(OTA_TAG, "download update: %s", url);
        if(update(url) == 1) return 1;
        else
        {
            ESP_LOGI(OTA_TAG, "error update");
            return -1;
        }
    }
    else {
        ESP_LOGI(OTA_TAG, "have't update");
        return -1;
    }
}

int Ota_update::wifi_wait_connect()
{
    EventBits_t chek = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, true, true, 5000 / portTICK_PERIOD_MS);
    ESP_LOGI("ota_main", "wait connection...");
    if (chek == CONNECTED_BIT) return 1;
    return 0;
}

void Ota_update::set_auto_restart(bool arg)
{
    auto_restart = arg;
}

bool Ota_update::get_auto_restart()
{
    return auto_restart;
}

void Ota_update::restart()
{
    esp_restart();
}

