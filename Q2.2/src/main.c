// Including the required libraries

// String manipulation
#include <string.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Getting mac address of the ESP32
#include "esp_mac.h"

// WIFi
#include "esp_wifi.h"

// Event system
#include "esp_event.h"

// Logging
#include "esp_log.h"

// Non-volatile storage
#include "nvs_flash.h"

// Error codes for the LwIP TCP/IP stack
#include "lwip/err.h"
// Low-level system operations
#include "lwip/sys.h"

// Constants and macros for system parameters
#include <sys/param.h>

// Network interface
#include "esp_netif.h"

// HTTP servers
#include <esp_http_server.h>

// JSON
#include <cJSON.h>

// LED
#include "driver/gpio.h"


// Defining the required parameters

// Using the sdkconfig.esp32dev file
#define ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

// Defining the LED pin
#define LED GPIO_NUM_2

static const char *TAG = "LEDServer";


// Handling WiFi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // If a station has connected to the Wi-Fi access point ...
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        // Logging
        // (MAC2STR converts the MAC address from binary format to a human-readable format)
        // (AID is the Access Point ID, which is a unique identifier for each connected station)
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    } 
    // A station has disconnected from the Wi-Fi access point ...
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) 
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

// Initializing the ESP32's network interface
void wifi_init_softap(void)
{
    // Initializing
    ESP_ERROR_CHECK(esp_netif_init());

    // Creating an event loop for handling Wi-Fi events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Creating the default Wi-Fi AP network interface
    esp_netif_create_default_wifi_ap();

    // Initializing Wi-Fi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registering the Wi-Fi event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Configuring the Wi-Fi AP settings
    wifi_config_t wifi_config = {
        .ap = {
            // Setting the access point (AP) SSID
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            // Setting the Wi-Fi channel
            .channel = ESP_WIFI_CHANNEL,
            // Setting the AP password (if blank: open AP)
            .password = ESP_WIFI_PASS,
            // Setting the maximum number of connected stations
            .max_connection = MAX_STA_CONN,

            // Enabling WPA3-PSK authentication with AES encryption
            #ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
                .authmode = WIFI_AUTH_WPA3_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            #endif
            // Enabling PMF (Protected Management Frames) for stronger security
            .pmf_cfg = {
                    .required = true,
            },
        },
    };

    // If the password is empty, set the authentication mode to WIFI_AUTH_OPEN (no password)
    if (strlen(ESP_WIFI_PASS) == 0) 
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Setting the Wi-Fi mode to AP mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    // Setting the Wi-Fi AP configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    // Starting the Wi-Fi AP
    ESP_ERROR_CHECK(esp_wifi_start());
    // Printing a message indicating the Wi-Fi AP initialization is complete
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}

// Handling the HTTP request (POST)
static esp_err_t led_handler(httpd_req_t *req)
{
    // Receiving and Storing the request's body
    char content[100];
    httpd_req_recv(req, content, sizeof(content));

    // Sparsing the objects in the request which is in the json format    
    cJSON *request = cJSON_Parse(content);
    // Taking the command part which is needed for deciding
    char *command = cJSON_GetStringValue(cJSON_GetObjectItem(request, "command"));

    // Creating a json object to response in the json format
    cJSON *response = cJSON_CreateObject();

    // Deciding based on the command
    if (strcmp(command, "ON") == 0) 
    {
        // Turning the LED on
        gpio_set_level(LED, 1);
        // Sending the appropriate message in json format
        cJSON_AddStringToObject(response, "message", "LED is turned on");
        char *jsonStr = cJSON_Print(response);
        httpd_resp_send(req, jsonStr, strlen(jsonStr));
    } 
    else if (strcmp(command, "OFF") == 0) 
    {
        // Turning the LED off
        gpio_set_level(LED, 0);
        cJSON_AddStringToObject(response, "message", "LED is turned off");
        char *jsonStr = cJSON_Print(response);
        httpd_resp_send(req, jsonStr, strlen(jsonStr));
    } 
    // Other commands
    else 
    {
        httpd_resp_send(req, "Invalid command", HTTPD_RESP_USE_STRLEN);
    }

    // Deleting the json objects
    cJSON_Delete(request);
    cJSON_Delete(response);

    return ESP_OK;
}

// Defining the request URI
static const httpd_uri_t led = {
    // URI for the LED control endpoint
    .uri       = "/led",
    // HTTP method for this endpoint (POST)
    .method    = HTTP_POST,
    // Handler function for this endpoint
    .handler   = led_handler,
    // User context for this endpoint (can be used to pass data to the handler)
    .user_ctx  = NULL
};

// Starting the webserver
static httpd_handle_t start_webserver(void)
{
    // Declaring a variable to store the server handle
    httpd_handle_t server = NULL;

    // Initializing the default HTTP server configuration
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Enabling LRU (Least Recently Used) cache purge to free up memory for frequently accessed resources
    config.lru_purge_enable = true;

    // Logging that the HTTP server starts on port 80 (default)
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    // Starting the HTTP server and checking for errors
    if (httpd_start(&server, &config) == ESP_OK) 
    {
        // Registering the URI handler for the '/led' endpoint
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &led);
        // Returning the server handle if it was started successfully
        return server;
    }
    
    // Printing an error message if the server failed to start
    ESP_LOGI(TAG, "Error starting server!");
    // Returning NULL if the server could not be started
    return NULL;
}

// Setting and Starting the webserver 
static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    // Starting the web server if it hasn't been started yet
    if (*server == NULL) 
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

// Configuring the LED pin
static void configure_led(void)
{
    // Resetting the pin
    gpio_reset_pin(LED);
    // Setting the pin as output
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
}

// The main application
void app_main(void)
{
    static httpd_handle_t server = NULL;

    // Configuring the LED
    configure_led();

    //Initializing NVS
    esp_err_t ret = nvs_flash_init();
    // If there is no NVS flash available or the NVS version is newer than the one supported by this ESP32, erase the NVS flash and try again
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initializing the Wi-Fi in AP (Access Point) mode
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    // Initializing the network interface
    ESP_ERROR_CHECK(esp_netif_init());

    // Registering a handler for the IP_EVENT_AP_STAIPASSIGNED event, which is triggered when a new client connects to the ESP32's Wi-Fi AP
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_handler, &server));
}