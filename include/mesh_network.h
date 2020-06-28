#ifndef _MESH_NETWORK_H
#define _MESH_NETWORK_H

#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "wifi_initialise.h"

class Mesh_network: private Wifi_initialise {
private:
    const int table_size;

    uint8_t MESH_ID[6];
    int MESH_MAX_LAYER;
    int MESH_CHANNEL;
    wifi_auth_mode_t MESH_AP_AUTHMODE;
    int MESH_AP_CONNECTIONS;
    char MESH_AP_PASSWD[11];    

    void (*ip_event_handler)(void *arg, esp_event_base_t event_base,
            int32_t event_id, void *event_data);
    void (*mesh_event_handler)(void *arg, esp_event_base_t event_base,
            int32_t event_id, void *event_data);

    void wifi_initialise();
public:    
    Mesh_network(
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
                );                
    ~Mesh_network();

    void mesh_deinit();
    void mesh_initialise();

    void mesh_start();
    void mesh_stop();
    void mesh_set_self_organized(bool, bool);

    void mesh_send(const mesh_addr_t *to, const mesh_data_t *data, int flag, const mesh_opt_t *opt, int opt_count);
    void mesh_recv(mesh_addr_t *from, mesh_data_t *data, int timeout_ms, int *flag, mesh_opt_t *opt, int opt_count);

    bool is_root();
    void get_route_table(mesh_addr_t *table);   
    int get_route_table_size(); 
    void get_mesh_id(mesh_addr_t* id);
};

#endif