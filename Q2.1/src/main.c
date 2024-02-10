// Including the required libraries

#include <stdio.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

//WiFi
#include "esp_wifi.h"

// Logging
#include "esp_log.h"

// Non-volatile storage
#include "nvs_flash.h"

// Network interface
#include "esp_netif.h"
#include "my_data.h"

// HTTP client
#include "esp_http_client.h"


// Handling WiFi events
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

// Connecting to WiFi
void wifi_connection()
{
    esp_netif_init();                    
    esp_event_loop_create_default();     
    esp_netif_create_default_wifi_sta(); 
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); 

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = SSID,
            .password = PASS}};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);

    esp_wifi_start();

    esp_wifi_connect();
}

// Handling HTTP client events during a GET request
esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    // Data received from the server
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    // Ignore other events
    default:
        break;
    }

    return ESP_OK;
}

// Executing a GET request to a REST API endpoint
static void rest_get()
{
    // Initializing HTTP client configuration
    esp_http_client_config_t config_get = {
        // Specifying the REST API endpoint URL
        .url = "http://192.168.1.111:80/test",
        // Setting the HTTP method to GET
        .method = HTTP_METHOD_GET,
        // Defining the event handler to handle GET request events
        .event_handler = client_event_get_handler
        };
    // Creating an HTTP client handle using the configuration
    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    // Performing the HTTP GET request
    esp_http_client_perform(client);
    // Cleaning up and releasing resources associated with the HTTP client
    esp_http_client_cleanup(client);
}

void app_main(void)
{
    // Initializing the NVS flash storage
    nvs_flash_init();

    // Starting the WiFi connection
    wifi_connection();

    // Waiting for 2 seconds to ensure WiFi connection is established
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("WIFI was initiated ...........\n\n");

    // Executing a GET request to the REST API endpoint
    rest_get();
}