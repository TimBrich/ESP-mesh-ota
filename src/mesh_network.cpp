#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "mesh_network.h"

Mesh_network::Mesh_network
            (
                char* ROUTER_SSID,
                char* ROUTER_PASSWD,
                uint8_t* MESH_ID, 
                void (*ip_event_handler)(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data), 
                void (*mesh_event_handler)(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data),
                int MESH_MAX_LAYER,
                int MESH_CHANNEL,
                wifi_auth_mode_t MESH_AP_AUTHMODE,
                int MESH_AP_CONNECTIONS,
                char* MESH_AP_PASSWD,
                const int mesh_table_size             
            ) : Wifi_initialise(ROUTER_SSID, ROUTER_PASSWD), table_size(mesh_table_size)
{
    Mesh_network::MESH_MAX_LAYER = MESH_MAX_LAYER;
    Mesh_network::MESH_CHANNEL = MESH_CHANNEL;
    Mesh_network::MESH_AP_AUTHMODE = MESH_AP_AUTHMODE;
    Mesh_network::MESH_AP_CONNECTIONS = MESH_AP_CONNECTIONS;

    strcpy(Mesh_network::MESH_AP_PASSWD, MESH_AP_PASSWD);

    for(int i = 0; i < 6; i++) Mesh_network::MESH_ID[i] = MESH_ID[i];

    Mesh_network::ip_event_handler = ip_event_handler;
    Mesh_network::mesh_event_handler = mesh_event_handler;
}

Mesh_network::~Mesh_network(){}

void Mesh_network::wifi_initialise()
{
    tcpip_adapter_init();
    tcpip_adapter_dhcp_status_t status;
    tcpip_adapter_dhcps_get_status(TCPIP_ADAPTER_IF_AP, &status);
    if(status != TCPIP_ADAPTER_DHCP_STOPPED){
        ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    }
    tcpip_adapter_dhcps_get_status(TCPIP_ADAPTER_IF_STA, &status);
    if(status != TCPIP_ADAPTER_DHCP_STOPPED){
        ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_STA));
    }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL));
}

void Mesh_network::mesh_initialise()
{
    wifi_initialise();
    wifi_start();
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, mesh_event_handler, NULL));
    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *) &cfg.mesh_id, MESH_ID, 6);
    /* router */
    cfg.channel = MESH_CHANNEL;
    cfg.router.ssid_len = strlen(ROUTER_SSID);
    memcpy((uint8_t *) &cfg.router.ssid, ROUTER_SSID, cfg.router.ssid_len);
    memcpy((uint8_t *) &cfg.router.password, ROUTER_PASSWD,
           strlen(ROUTER_PASSWD));
    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = MESH_AP_CONNECTIONS;
    memcpy((uint8_t *) &cfg.mesh_ap.password, MESH_AP_PASSWD,
           strlen(MESH_AP_PASSWD));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    esp_mesh_set_xon_qsize(64);
    /* mesh start */
    esp_mesh_set_self_organized ( true , true ); 
}

void Mesh_network::mesh_start()
{
    ESP_ERROR_CHECK(esp_mesh_start());
}

void Mesh_network::mesh_stop()
{
    esp_mesh_disconnect();
    esp_mesh_stop();
}

void Mesh_network::mesh_deinit()
{
    esp_event_handler_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP, ip_event_handler);
    esp_event_handler_unregister(MESH_EVENT, ESP_EVENT_ANY_ID, mesh_event_handler);
    esp_mesh_stop();
    esp_mesh_deinit();

    wifi_deinit();
}

void Mesh_network::mesh_send(const mesh_addr_t *to, const mesh_data_t *data, int flag, const mesh_opt_t *opt, int opt_count)
{
    esp_mesh_send(to, data, flag, opt, opt_count);
}

void Mesh_network::mesh_recv(mesh_addr_t *from, mesh_data_t *data, int timeout_ms, int *flag, mesh_opt_t *opt, int opt_count)
{
    esp_mesh_recv(from, data, timeout_ms, flag, opt, opt_count);
}

void Mesh_network::mesh_set_self_organized(bool enable, bool select_parent)
{
    esp_mesh_set_self_organized(enable, select_parent);
}

void Mesh_network::get_route_table(mesh_addr_t* table)
{
    mesh_addr_t route_table[table_size];
    int route_table_size = 0;
    esp_mesh_get_routing_table((mesh_addr_t *) &route_table,
                                esp_mesh_get_routing_table_size() * 6,
                                &route_table_size);

    memcpy(table, route_table, sizeof(route_table));
}

int Mesh_network::get_route_table_size()
{
    return esp_mesh_get_routing_table_size();
}

bool Mesh_network::is_root()
{
    return esp_mesh_is_root();
}

void Mesh_network::get_mesh_id(mesh_addr_t* _id)
{
    esp_mesh_get_id(_id);
}