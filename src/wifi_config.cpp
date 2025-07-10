#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "esp_http_server.h"
#include "wifi_config.h"
#include "esp_lcd_panel_io.h"
#include "lcd.h"
#include "esp_lvgl_port.h"

static const char *TAG = "wifi_config";

/* NVS Namespace */
#define NVS_NAMESPACE "wifi_config"

wifi_config_data_t web_wifi_config_data;
static httpd_handle_t wifi_config_server = NULL;
static bool web_is_configured = false;
static bool sta_is_connected = false;
static esp_ip4_addr_t sta_ip = {0};

/**
 * @brief Callback triggered when the STA obtains an IP address.
 */
static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    static int s_retry_num = 0;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        sta_ip = event->ip_info.ip;
        sta_is_connected = true;
    }
}

/**
 * @brief Get the IP address in Station (STA) mode.
 * @return The current IPv4 address of the STA interface.
 */
esp_ip4_addr_t get_sta_ip_address(void) {
    return sta_ip;
}

/**
 * @brief Start SoftAP (Access Point) mode with given SSID and password.
 * @param ssid The SSID for the SoftAP.
 * @param password The password for the SoftAP.
 * @return Pointer to the created esp_netif object.
 */
esp_netif_t* start_wifi_ap(const char *ssid, const char *password) {
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    wifi_config_t ap_config = {0};
    ap_config.ap.ssid_len = strlen(ssid);
    if(strlen(password)==0)
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    else
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    strncpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));
    if(strlen(password)!=0)
    {
        strncpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password));
    }
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    return ap_netif;
}

/**
 * @brief Check if the device is connected to a WiFi network in STA mode.
 * @return true if connected, false otherwise.
 */
bool wifi_is_sta_connected(void)
{
    return sta_is_connected;
}

/**
 * @brief Start Station (STA) mode and connect to the specified WiFi network.
 * @param ssid The SSID of the target WiFi network.
 * @param password The password of the target WiFi network.
 * @return Pointer to the created esp_netif object.
 */
esp_netif_t* start_wifi_sta(const char *ssid, const char *password) {
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    /*
    wifi_config_t sta_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = 5,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    */
    wifi_config_t sta_config = {0};
    sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_config.sta.failure_retry_cnt = 5;
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_start();
    esp_wifi_connect();

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_got_ip, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);
    uint8_t connect_timeout = 0;
    while (!sta_is_connected && connect_timeout<10) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        connect_timeout++;
    }
    if(connect_timeout>=10)
    {
        sta_is_connected = false;
        ESP_LOGI(TAG, "Connect timeout");
    }
    return sta_netif;
}

/**
 * @brief Get the WiFi configuration submitted via the web interface.
 * @return Pointer to the stored WiFi configuration data.
 */
const wifi_config_data_t* get_web_wifi_config_data(void) {
    while (!web_is_configured) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return &web_wifi_config_data;
}

/**
 * @brief HTTP GET handler for the root ("/") endpoint.
 *        Returns the WiFi configuration form HTML page.
 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* WIFI_CONFIG_HTML = 
    "<!DOCTYPE html>\n" \
    "<html lang=\"en\">\n" \
    "<head>\n" \
    "    <meta charset=\"UTF-8\">\n" \
    "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n" \
    "    <title>WiFi Configuration</title>\n" \
    "    <style>\n" \
    "        body {\n" \
    "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n" \
    "            background: linear-gradient(to right, #f8f9fa, #e9ecef);\n" \
    "            margin: 0; padding: 0;\n" \
    "            display: flex;\n" \
    "            justify-content: center;\n" \
    "            align-items: center;\n" \
    "            height: 100vh;\n" \
    "            color: #333;\n" \
    "        }\n" \
    "        .container {\n" \
    "            background-color: white;\n" \
    "            border-radius: 12px;\n" \
    "            box-shadow: 0 8px 16px rgba(0,0,0,0.15);\n" \
    "            padding: 40px;\n" \
    "            width: 90%; max-width: 400px;\n" \
    "        }\n" \
    "        h2 { text-align: center; margin-bottom: 20px; font-size: 1.5em; color: #4a4a4a; }\n" \
    "        label { display: block; margin-bottom: 6px; font-weight: bold; font-size: 1em; }\n" \
    "        input[type=\"text\"], input[type=\"password\"] {\n" \
    "            width: 100%;\n" \
    "            padding: 10px 12px;\n" \
    "            margin-bottom: 16px;\n" \
    "            border: 1px solid #ccc;\n" \
    "            border-radius: 6px;\n" \
    "            font-size: 1em;\n" \
    "            transition: border-color 0.3s ease;\n" \
    "        }\n" \
    "        input[type=\"text\"]:focus, input[type=\"password\"]:focus {\n" \
    "            border-color: #007bff;\n" \
    "            outline: none;\n" \
    "            box-shadow: 0 0 0 2px rgba(0,123,255,0.25);\n" \
    "        }\n" \
    "        input[type=\"submit\"] {\n" \
    "            width: 100%;\n" \
    "            background-color: #007bff;\n" \
    "            color: white;\n" \
    "            padding: 12px;\n" \
    "            font-size: 1.1em;\n" \
    "            border: none;\n" \
    "            border-radius: 6px;\n" \
    "            cursor: pointer;\n" \
    "            transition: background-color 0.3s ease;\n" \
    "        }\n" \
    "        input[type=\"submit\"]:hover {\n" \
    "            background-color: #0056b3;\n" \
    "        }\n" \
    "        @media (max-width: 480px) {\n" \
    "            .container {\n" \
    "                padding: 20px 15px;\n" \
    "                margin: 10px;\n" \
    "            }\n" \
    "            h2 {\n" \
    "                font-size: 1.2em;\n" \
    "                margin-bottom: 16px;\n" \
    "            }\n" \
    "            input[type=\"text\"], input[type=\"password\"] {\n" \
    "                font-size: 1em;\n" \
    "                padding: 8px 10px;\n" \
    "            }\n" \
    "            input[type=\"submit\"] {\n" \
    "                font-size: 1em;\n" \
    "                padding: 10px;\n" \
    "            }\n" \
    "        }\n" \
    "    </style>\n" \
    "</head>\n" \
    "<body>\n" \
    "    <div class=\"container\">\n" \
    "        <h2>WiFi Configuration</h2>\n" \
    "        <form action=\"/configure\" method=\"POST\">\n" \
    "            <label for=\"ssid\">WiFi Name (SSID):</label>\n" \
    "            <input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"Enter your SSID\">\n" \
    "\n" \
    "            <label for=\"password\">WiFi Password:</label>\n" \
    "            <input type=\"password\" id=\"password\" name=\"password\" placeholder=\"Enter your password\">\n" \
    "\n" \
    "            <label for=\"openai_key\">OpenAI API Key:</label>\n" \
    "            <input type=\"text\" id=\"openai_key\" name=\"openai_key\" placeholder=\"sk-xxxxx...\">\n" \
    "\n" \
    "            <input type=\"submit\" value=\"Submit\">\n" \
    "        </form>\n" \
    "    </div>\n" \
    "</body>\n" \
    "</html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WIFI_CONFIG_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

/**
 * @brief Handle the POST request from the web configuration form.
 *        Parses the submitted SSID, password, and OpenAI key.
 */
static esp_err_t configure_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int remaining = req->content_len;
    int received = 0;

    if (remaining <= 0 || remaining >= sizeof(buf)) {
        ESP_LOGE(TAG, "Invalid content length: %d", remaining);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)-1));
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    char *ssid_start = strstr(buf, "ssid=");
    char *password_start = strstr(buf, "password=");
    char *key_start = strstr(buf, "openai_key=");

    if (!ssid_start || !password_start || !key_start) {
        ESP_LOGE(TAG, "Missing form fields");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing form fields");
        return ESP_FAIL;
    }

    sscanf(ssid_start, "ssid=%[^&]", web_wifi_config_data.ssid);
    sscanf(password_start, "password=%[^&]", web_wifi_config_data.password);
    sscanf(key_start, "openai_key=%[^&]", web_wifi_config_data.openai_key);

    const char *success_html = 
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <title>Success</title>\n"
        "    <style>\n"
        "        body { font-family: sans-serif; text-align: center; margin-top: 10%; background-color: #f0fff0; }\n"
        "        h1 { color: green; }\n"
        "        p { font-size: 1.2em; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1> Configuration successful!</h1>\n"
        "    <p>The device will connect to your WiFi network within a few seconds.</p>\n"
        "    <p>You can close this webpage.</p>\n"
        "</body>\n"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, success_html, HTTPD_RESP_USE_STRLEN);

    web_is_configured = true;

    return ESP_OK;
}
static const httpd_uri_t configure = {
    .uri       = "/configure",
    .method    = HTTP_POST,
    .handler   = configure_post_handler,
    .user_ctx  = NULL
};

/**
 * @brief Handle favicon.ico requests.
 */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    const char *resp = "";
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, resp, 0);
    return ESP_OK;
}
static const httpd_uri_t favicon = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler,
    .user_ctx  = NULL
};

/**
 * @brief Start the web server.
 * @return Handle to the started web server instance.
 */
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server_handle = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server_handle, &root);
        httpd_register_uri_handler(server_handle, &configure);
        httpd_register_uri_handler(server_handle, &favicon);
        return server_handle;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

/**
 * @brief Stop the running web server.
 */
static void stop_webserver(void)
{
    if (wifi_config_server) {
        httpd_stop(wifi_config_server);
        wifi_config_server = NULL;
    }
}

/**
 * @brief Event handler for station IP connection.
 */
static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (wifi_config_server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        wifi_config_server = start_webserver();
    }
}

/**
 * @brief Event handler for station disconnection.
 */
static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (wifi_config_server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver();
    }
}

/**
 * @brief Start listening for web configuration events.
 */
void start_wifi_config_webserver(void)
{
    static bool initialized = false;
    if (!initialized) {
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, NULL));
        initialized = true;
    }
    wifi_config_server = start_webserver();
}

/**
 * @brief Stop the web server and release resources.
 */
void stop_wifi_config_webserver(void)
{
    stop_webserver();
    wifi_config_server = NULL;
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler);
}

/**
 * @brief Read WiFi configuration from NVS.
 * @param config Pointer to the structure to store the configuration.
 * @return true if successfully read, false otherwise.
 */
bool read_wifi_config_from_nvs(wifi_config_data_t *config) {
    nvs_handle_t my_nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }
    size_t len;

    len = sizeof(config->ssid);
    err = nvs_get_str(my_nvs_handle, "ssid", config->ssid, &len);
    if (err != ESP_OK) {
        nvs_close(my_nvs_handle);
        return false;
    }

    len = sizeof(config->password);
    err = nvs_get_str(my_nvs_handle, "password", config->password, &len);
    if (err != ESP_OK) {
        nvs_close(my_nvs_handle);
        return false;
    }

    len = sizeof(config->openai_key);
    err = nvs_get_str(my_nvs_handle, "openai_key", config->openai_key, &len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read openai_key from NVS");
    }

    nvs_close(my_nvs_handle);
    return true;
}

/**
 * @brief Write WiFi configuration to NVS.
 * @param config Pointer to the configuration to write.
 */
void write_wifi_config_to_nvs(const wifi_config_data_t *config) {
    nvs_handle_t my_nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(my_nvs_handle, "ssid", config->ssid);
    err |= nvs_set_str(my_nvs_handle, "password", config->password);
    if (strlen(config->openai_key) > 0) {
        err |= nvs_set_str(my_nvs_handle, "openai_key", config->openai_key);
    }

    err |= nvs_commit(my_nvs_handle);
    nvs_close(my_nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write failed: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Clear all saved WiFi configuration data from NVS.
 */
void clear_nvs_config(void) {
    nvs_handle_t my_nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_all(my_nvs_handle);
        nvs_commit(my_nvs_handle);
        nvs_close(my_nvs_handle);
    }
    ESP_LOGI(TAG, "NVS cleared");
}

void wifi_config_init(void) { 
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_data_t nvs_config = {0};
    bool has_saved_config = read_wifi_config_from_nvs(&nvs_config);
    esp_netif_t *esp_netif_sta;
    esp_netif_t *esp_netif_ap;
    if (has_saved_config && strlen(nvs_config.ssid) > 0) {
        ESP_LOGI(TAG, "Found saved config: SSID=%s, PSD=%s", nvs_config.ssid, nvs_config.password);
        ESP_LOGI(TAG, "Found saved config: Open Ai Key=%s", nvs_config.openai_key);
        ESP_LOGI(TAG, "Connecting to WiFi...");
        char buf[256];
        lv_snprintf(buf, sizeof(buf), "SSID=%s", nvs_config.ssid);
        lvgl_ui_label_set_text(buf);
        lv_snprintf(buf, sizeof(buf), "PSD=%s", nvs_config.password);
        lvgl_ui_label_set_text(buf);
        esp_netif_sta = start_wifi_sta(nvs_config.ssid, nvs_config.password);
        bool is_connected = wifi_is_sta_connected();
        if (is_connected) {
            ESP_LOGI(TAG, "WiFi connection successful.");
            lvgl_ui_label_set_text("WiFi connection successful.");
            vTaskDelay(500);
            lvgl_ui_label_set_text("You can chat now.");
        } else {
            ESP_LOGI(TAG, "WiFi connection failed.");
            lvgl_ui_label_set_text("WiFi connection failed.");
            clear_nvs_config();
            esp_restart();
        }
    }
    else { 
        esp_netif_ap = start_wifi_ap(EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD);
        ESP_LOGI(TAG, "NVS has not configuration. Starting SoftAP...");
        ESP_LOGI(TAG, "Use your browser to open http://192.168.4.1");
        lvgl_ui_label_set_text("Connect to the router \"OpenAi\" and access \"192.168.4.1\" using a browser.");

        start_wifi_config_webserver();
        const wifi_config_data_t *web_config = get_web_wifi_config_data();
        ESP_LOGI(TAG, "Web Configuration: SSID=%s, PSD=%s", web_config->ssid, web_config->password);
        ESP_LOGI(TAG, "OPENAI_KEY=%s", web_config->openai_key);
        ESP_ERROR_CHECK(esp_wifi_stop());
        esp_netif_destroy(esp_netif_ap);
        write_wifi_config_to_nvs(web_config);
        esp_restart();
    }
}