#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/uart.h"
#include "M5Unified.h"
//#include "M5GFX.h"

#define EXAMPLE_ESP_WIFI_SSID      "myssid"
#define EXAMPLE_ESP_WIFI_PASS      "mypassword"
#define EXAMPLE_MAX_STA_CONN       4

static esp_err_t hello_get_handler(httpd_req_t *req) {
    const char* resp_str = "Hello, world!";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello);
    }
    return server;
}

void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_ap_config_t ap_config;
    
    memset(&ap_config, 0, sizeof(ap_config));
    
    strncpy((char *)ap_config.ssid, EXAMPLE_ESP_WIFI_SSID, sizeof(ap_config.ssid));
    strncpy((char *)ap_config.password, EXAMPLE_ESP_WIFI_PASS, sizeof(ap_config.password));
    ap_config.ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID);
    ap_config.channel = 0;
    ap_config.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.max_connection = EXAMPLE_MAX_STA_CONN;
    ap_config.beacon_interval = 100;



    wifi_config_t wifi_config = {
        .ap = ap_config
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_AP, &wifi_config);
    esp_wifi_start();
}

void app_main(void * arg) {
    // Initialize the serial port
    //const uart_port_t uart_num = UART_NUM_1;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config);
    //uart_set_pin(uart0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //uart_driver_install(uart0, 1024 * 2, 0, 0, NULL, 0);
    
    nvs_flash_init();
    wifi_init_softap();
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    printf("IP Address: " IPSTR "\n", IP2STR(&ip_info.ip));
    start_webserver();
    printf("Web server started\n");
    printf("ESP32 is running...\n");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("Hello from FreeRTOS task!\n");
    }
}

//int main(int argc, char const *argv[])
void setup() {
    M5.begin();
    M5.Power.begin();
    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Hello, M5Stack!");

    xTaskCreate(app_main, "app_main_task", 20480, NULL, 1, NULL);
    //vTaskStartScheduler();
    //return 0;
}

void loop() {
    // Your main loop code here
    vTaskDelay(pdMS_TO_TICKS(1000));
}
