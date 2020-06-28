/* Mesh Internal Communication Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
extern "C" {void app_main (void); }

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_http_client.h"

#include "mesh_network.h"
#include "ota_update.h"

#include "ws2812.h"
#define NUM_LEDS 4
#define RED   0xff0000
#define GREEN 0x00ff00
#define BLUE  0x0000ff
#define WHITE 0xaaaaaa
#define OFF 0x000000

#define RX_SIZE          (50)
#define TX_SIZE          (50)

static const char *MESH_TAG = "mesh_main";
static uint8_t tx_buf[TX_SIZE] = { 0, };
static uint8_t rx_buf[RX_SIZE] = { 0, };
static bool is_mesh_connected = false;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;

#define BLINK_GPIO ((gpio_num_t) CONFIG_BLINK_GPIO)
#define DELAY 1000

void start_blink_task(void *pvParameter);
void blink_task_send(void *pvParameter);  
void blink_task_rec(void *pvParameter);    
void test(void *pvParameter);
esp_err_t esp_mesh_blink(void);
void led_control(int position);
void double_blink();
void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data);

uint8_t MESH_ID[6] = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
Mesh_network mesh("vll", "789456123", MESH_ID, &ip_event_handler,
                        &mesh_event_handler, 5, 0, (wifi_auth_mode_t) 4, 6, "MAP_PASSWD", 50);

Ota_update ota("140_112", "Hb6pX9Xswz6kf5b","192.168.100.9", 15, "worker", BIT1);

struct led_state new_state;


void double_blink()
{
    new_state.leds[0] = WHITE;
    new_state.leds[1] = WHITE;
    new_state.leds[2] = WHITE;
    new_state.leds[3] = WHITE;
    ws2812_write_leds(new_state);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    new_state.leds[0] = OFF;
    new_state.leds[1] = OFF;
    new_state.leds[2] = OFF;
    new_state.leds[3] = OFF;
    ws2812_write_leds(new_state);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    new_state.leds[0] = WHITE;
    new_state.leds[1] = WHITE;
    new_state.leds[2] = WHITE;
    new_state.leds[3] = WHITE;
    ws2812_write_leds(new_state);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    new_state.leds[0] = OFF;
    new_state.leds[1] = OFF;
    new_state.leds[2] = OFF;
    new_state.leds[3] = OFF;
    ws2812_write_leds(new_state);
    vTaskDelay(200 / portTICK_PERIOD_MS);
}

void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    mesh_addr_t id = {0,};
    static uint8_t last_layer = 0;

    switch (event_id) {
    case MESH_EVENT_STARTED: {
        mesh.get_mesh_id(&id);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_MESH_STARTED>ID:"MACSTR"", MAC2STR(id.addr));
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_STOPPED: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOPPED>"); 
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED: {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, "MACSTR"",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED: {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, "MACSTR"",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND: {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 no_parent->scan_times);
    }
    /* TODO handler for the failure */
    break;
    case MESH_EVENT_PARENT_CONNECTED: {
        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        esp_mesh_get_id(&id);
        mesh_layer = connected->self_layer;
        memcpy(&mesh_parent_addr.addr, connected->connected.bssid, 6);
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:"MACSTR"%s, ID:"MACSTR"",
                 last_layer, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "", MAC2STR(id.addr));
        last_layer = mesh_layer;
        is_mesh_connected = true;
        if (esp_mesh_is_root()) {
            tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
        }

        double_blink();

        esp_mesh_blink();
    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED: {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
        if(disconnected->reason == 4)
        {
            mesh.mesh_deinit();
            mesh.mesh_initialise();
            mesh.mesh_start();
        }
    }
    break;
    case MESH_EVENT_LAYER_CHANGE: {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        mesh_layer = layer_change->new_layer;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "");
        last_layer = mesh_layer;
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS: {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ADDRESS>root address:"MACSTR"",
                 MAC2STR(root_addr->addr));
    }
    break;
    case MESH_EVENT_VOTE_STARTED: {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:"MACSTR"",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ: {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:"MACSTR"",
                 switch_req->reason,
                 MAC2STR( switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK: {
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:"MACSTR"", mesh_layer, MAC2STR(mesh_parent_addr.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE: {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
    }
    break;
    case MESH_EVENT_ROOT_FIXED: {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD: {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>"MACSTR", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH: {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE: {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE: {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK: {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:"MACSTR"",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH: {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, "MACSTR"",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;
    default:
        ESP_LOGI(MESH_TAG, "unknown id:%d", event_id);
        break;
    }
}

void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(MESH_TAG, "<IP_EVENT_STA_GOT_IP>IP:%s", ip4addr_ntoa(&event->ip_info.ip));
}                          

void led_control(int position)
{
    switch (position)
        {
        case 0:
            new_state.leds[0] = RED;
            new_state.leds[1] = RED;
            new_state.leds[2] = GREEN;
            new_state.leds[3] = GREEN;
            break;
        case 1:
            new_state.leds[0] = BLUE;
            new_state.leds[1] = BLUE;
            new_state.leds[2] = RED;
            new_state.leds[3] = RED;
            break;
        case 2:
            new_state.leds[0] = GREEN;
            new_state.leds[1] = GREEN;
            new_state.leds[2] = BLUE;
            new_state.leds[3] = BLUE;
            break;
        case 3:
            new_state.leds[0] = RED;
            new_state.leds[1] = RED;
            new_state.leds[2] = GREEN;
            new_state.leds[3] = GREEN;
            break;
        
        default:
            new_state.leds[0] = OFF;
            new_state.leds[1] = OFF;
            new_state.leds[2] = OFF;
            new_state.leds[3] = OFF;
            break;
        }

    ws2812_write_leds(new_state);
}

void blink_task_send(void *pvParameter)
{  
    int count = 0;

    mesh_data_t data;
    data.data = tx_buf;
    data.size = sizeof(tx_buf);
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;

    mesh_addr_t route_table[50];
    int route_table_size = 0;

    while(esp_mesh_is_root() && is_mesh_connected)
    {   
        count++;
        ESP_LOGI(MESH_TAG, "count = %d", count);

        mesh.get_route_table(route_table);
        route_table_size = mesh.get_route_table_size();

        for(int i = 0; i < route_table_size; i++)
        {
            ESP_LOGI(MESH_TAG, "root table: "MACSTR"", MAC2STR(route_table[i].addr));
        }

        // blink
        for(int i = 0; i <= 3; i++) {
            data.data[0] = (uint8_t) i;
            ESP_LOGI(MESH_TAG, "root send: %d", (int)data.data[0]);

            for (int j = 1; j < route_table_size; j++) 
                esp_mesh_send(&route_table[j], &data, MESH_DATA_FROMDS, NULL, 0);
            
            led_control(i);
            vTaskDelay(DELAY / portTICK_PERIOD_MS);
        }
        
        if(count == 4) break;        
    }

    led_control(-1);

    data.data[0] = (uint8_t) 4;

    ESP_LOGI(MESH_TAG, "root send: %d", (int)data.data[0]);
    for (int i = 1; i < route_table_size; i++) {
        mesh.mesh_send(&route_table[i], &data, MESH_DATA_FROMDS, NULL, 0);
    }

    mesh.mesh_deinit();
    is_mesh_connected = false;
    xTaskCreate(&start_blink_task, "blink_task", 2048, NULL, 5, NULL);

    if(ota.ota_init() == 1)
    {
        ota.set_auto_restart(false);
        if(ota.ota_update() == 1) 
        {
            ota.set_auto_restart(true);
            double_blink();
            ota.restart();
        }
    }
    ota.ota_deinit();

    mesh.mesh_initialise();
    mesh.mesh_start();

    vTaskDelete(NULL);
}

void blink_task_rec(void *pvParameter)
{
    mesh_data_t data;
    data.data = rx_buf;
    data.size = RX_SIZE;
    mesh_addr_t from;
    int flag = 0;

    while(is_mesh_connected)
    {
        mesh.mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        printf("I'm recive msg %d\n", (int) data.data[0]);
        switch ((int) data.data[0])
        {
        case 0:
            led_control(0);
            continue;
        case 1:
            led_control(1);
            continue;
        case 2:
            led_control(2);
            continue;
        case 3:
            led_control(3);
            continue;
        case 4:
            {
                led_control(-1);

                mesh.mesh_deinit();
                is_mesh_connected = false;
                xTaskCreate(&start_blink_task, "blink_task", 2048, NULL, 5, NULL);

                if(ota.ota_init() == 1)
                {
                    ota.set_auto_restart(false);
                    if(ota.ota_update() == 1) 
                    {
                        ota.set_auto_restart(true);
                        double_blink();
                        ota.restart();
                    }
                }                

                ota.ota_deinit();

                mesh.mesh_initialise();
                mesh.mesh_start();

                vTaskDelete(NULL);
            }
        
        default:
            continue;
        }
    }
    led_control(-1);

    vTaskDelete(NULL);
}

void test(void *pvParameter){
// -----------reinit mesh-----------------------------------

    mesh.mesh_deinit();
    ESP_LOGI(MESH_TAG, "wifi disconect");

// -----------init wifi to download-----------------------------------

    ota.ota_init();
    ESP_LOGI(MESH_TAG, "download");
    ota.ota_update();
    ota.ota_deinit();
    ESP_LOGI(MESH_TAG, "end download");

// -----------init mesh-----------------------------------

    mesh.mesh_initialise();
    mesh.mesh_start();

// - return
    vTaskDelete(NULL);
}

esp_err_t esp_mesh_blink(void)
{
    if(mesh.is_root())
        xTaskCreate(&blink_task_send, "blink_task", 4096, NULL, 5, NULL);
    else
        xTaskCreate(&blink_task_rec, "blink_task", 4096, NULL, 5, NULL);
    return ESP_OK;
}

void start_blink_task(void *pvParameter)
{
    int count = 0;

    while(!is_mesh_connected) {
        count++;

        if(count == 4) count = 0;

        switch (count)
        {
        case 0:
            new_state.leds[0] = RED;
            new_state.leds[1] = GREEN;
            new_state.leds[2] = BLUE;
            new_state.leds[3] = WHITE;
            break;
        case 1:
            new_state.leds[0] = WHITE;
            new_state.leds[1] = RED;
            new_state.leds[2] = GREEN;
            new_state.leds[3] = BLUE;
            break;
        case 2:
            new_state.leds[0] = BLUE;
            new_state.leds[1] = WHITE;
            new_state.leds[2] = RED;
            new_state.leds[3] = GREEN;
            break;
        case 3:
            new_state.leds[0] = GREEN;
            new_state.leds[1] = BLUE;
            new_state.leds[2] = WHITE;
            new_state.leds[3] = RED;
            break;
        
        default:
            break;
        }
        
        ws2812_write_leds(new_state);

        vTaskDelay(300 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void app_main()
{
    nvs_flash_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ws2812_control_init();
    xTaskCreate(&start_blink_task, "blink_task", 2048, NULL, 5, NULL);

    mesh.mesh_initialise();
    mesh.mesh_start();

    return;
}