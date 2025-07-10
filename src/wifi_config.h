#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_netif.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AP Configuration */
#define EXAMPLE_ESP_WIFI_AP_SSID    "OpenAI"
#define EXAMPLE_ESP_WIFI_AP_PASSWD  ""

typedef struct {
    char ssid[128];       // WiFi network name (SSID)
    char password[128];   // WiFi password
    char openai_key[256]; // Optional OpenAI API key
} wifi_config_data_t;

esp_ip4_addr_t get_sta_ip_address(void); // Get IP address in Station (STA) mode
esp_netif_t* start_wifi_ap(const char *ssid, const char *password); // Start SoftAP (Access Point) mode
esp_netif_t* start_wifi_sta(const char *ssid, const char *password); // Start Station (STA) mode
bool wifi_is_sta_connected(void); // Check if connected to WiFi in STA mode
const wifi_config_data_t* get_web_wifi_config_data(void); // Get WiFi configuration submitted via web

void start_wifi_config_webserver(void); // Start the WiFi configuration web server
void stop_wifi_config_webserver(void);  // Stop the web server and release resources

bool read_wifi_config_from_nvs(wifi_config_data_t *config); // Read WiFi config from NVS
void write_wifi_config_to_nvs(const wifi_config_data_t *config); // Write WiFi config to NVS
void clear_nvs_config(void); // Clear stored WiFi configuration in NVS

void wifi_config_init(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CONFIG_H