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

// Calculate buffer size for 10 seconds at 8kHz (lower sample rate for better quality)
// 8000 samples/second * 10 seconds = 80000 samples
const uint32_t buffer_size = 80000;  // 10 seconds at 8kHz
int16_t *buffer = (int16_t *)malloc(buffer_size * sizeof(int16_t));

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

void record_and_play_audio(void *arg) {
    const uint32_t SAMPLE_RATE = 17000;  // Lower sample rate for better quality
    const uint32_t RECORD_DURATION_MS = 10000;  // 10 seconds in milliseconds
    
    while (1) {
        // Clear the buffer before each recording
        memset(buffer, 0, buffer_size * sizeof(int16_t));
        
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.println("Ready to record");
        M5.Display.printf("Buffer: %d samples\n", buffer_size);
        M5.Display.printf("Rate: %d Hz\n", SAMPLE_RATE);
        M5.Display.printf("Duration: %d sec\n", RECORD_DURATION_MS / 1000);
        M5.Display.printf("Buffer addr: %p\n", buffer);
        M5.Display.printf("Buffer size: %lu bytes\n", buffer_size * sizeof(int16_t));
        
        // Debug: Print free memory
        M5.Display.printf("Free heap: %lu bytes\n", esp_get_free_heap_size());
        
        // Wait before starting
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // Initialize microphone (make sure speaker is off)
        M5.Speaker.end();
        if (!M5.Mic.begin()) {
            M5.Display.println("Mic init failed!");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        
        // Use microphone configuration for better control
        auto mic_cfg = M5.Mic.config();
        mic_cfg.sample_rate = SAMPLE_RATE;
        mic_cfg.stereo = false;  // mono recording
        M5.Mic.config(mic_cfg);
        
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.println("Starting recording...");
        M5.Display.printf("Sample rate: %d Hz\n", SAMPLE_RATE);
        
        // Start recording
        bool record_success = M5.Mic.record((int16_t*)buffer, buffer_size, SAMPLE_RATE);
        M5.Display.printf("Record start: %s\n", record_success ? "OK" : "FAILED");
        
        if (!record_success) {
            M5.Display.println("Record start failed!");
            M5.Mic.end();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        
        M5.Display.println("Recording in progress");
        
        // Record for exactly 10 seconds
        uint32_t start_time = millis();
        int prev_second = -1;
        bool is_recording = true;
        
        // Continue recording until the full duration has passed
        while (is_recording && millis() - start_time < RECORD_DURATION_MS) {
            int current_second = (millis() - start_time) / 1000;
            
            // Only update the display when the second changes
            if (current_second != prev_second) {
                M5.Display.fillRect(0, 70, 320, 30, BLACK);  // Clear previous text
                M5.Display.setCursor(0, 70);
                M5.Display.printf("Recording: %d sec of %d", 
                    current_second, RECORD_DURATION_MS / 1000);
                
                // Debug: print recording status
                M5.Display.setCursor(0, 100);
                is_recording = M5.Mic.isRecording();
                M5.Display.printf("isRecording: %s", is_recording ? "true" : "false");
                
                prev_second = current_second;
            }
            
            if (!is_recording) {
                M5.Display.println("Recording stopped early!");
                break;
            }
            
            // Process M5 events
            M5.update();
            vTaskDelay(pdMS_TO_TICKS(10)); // Check more frequently
        }
        
        // Ensure we've recorded for the full duration
        uint32_t elapsed = millis() - start_time;
        M5.Display.setCursor(0, 130);
        M5.Display.printf("Record time: %lu ms\n", elapsed);
        
        if (elapsed < RECORD_DURATION_MS) {
            M5.Display.setCursor(0, 160);
            M5.Display.println("Recording time was short!");
            M5.Display.printf("Only recorded %lu ms\n", elapsed);
        }
        
        M5.Display.println("Recording completed");
        
        // Make sure recording is fully complete
        int wait_count = 0;
        while (M5.Mic.isRecording() && wait_count < 100) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
        }
        
        M5.Display.printf("Wait count: %d\n", wait_count);
        
        // Turn off microphone before starting speaker
        M5.Mic.end();
        vTaskDelay(pdMS_TO_TICKS(500)); // Brief pause
        
        // For testing - wait for user to press button to continue
        M5.Display.println("Press button to play back...");
        for (int i = 0; i < 30 && !M5.BtnA.wasPressed(); i++) {
            M5.update();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Debug: Print buffer data
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.println("Buffer Data Analysis:");
        
        // Check for non-zero data in buffer (print first 5 samples)
        M5.Display.println("First 5 samples:");
        for (int i = 0; i < 5 && i < buffer_size; i++) {
            M5.Display.printf("%d: %d\n", i, buffer[i]);
        }
        
        // Check last part of filled buffer
        M5.Display.println("Last samples:");
        int last_idx = 0;
        // Find approximate end of valid data by looking for first long run of zeros
        for (int i = 0; i < buffer_size - 100; i++) {
            if (buffer[i] != 0 && buffer[i+10] == 0 && buffer[i+50] == 0 && buffer[i+100] == 0) {
                last_idx = i;
                break;
            }
        }
        if (last_idx == 0) last_idx = buffer_size - 5; // Default to end if no clear ending found
        
        for (int i = last_idx - 5; i < last_idx && i < buffer_size; i++) {
            M5.Display.printf("%d: %d\n", i, buffer[i]);
        }
        
        // Count non-zero values in buffer
        int non_zero_count = 0;
        int16_t min_value = 32767;
        int16_t max_value = -32768;
        int last_non_zero = 0;
        
        for (int i = 0; i < buffer_size; i++) {
            if (buffer[i] != 0) {
                non_zero_count++;
                if (buffer[i] < min_value) min_value = buffer[i];
                if (buffer[i] > max_value) max_value = buffer[i];
                last_non_zero = i;
            }
        }
        
        M5.Display.printf("Non-zero: %d/%d\n", non_zero_count, buffer_size);
        M5.Display.printf("Min: %d, Max: %d\n", min_value, max_value);
        M5.Display.printf("Last non-zero: %d\n", last_non_zero);
        M5.Display.printf("Est. duration: %.1f sec\n", (float)last_non_zero / SAMPLE_RATE);
        
        // Wait for user to see the data
        for (int i = 0; i < 30 && !M5.BtnA.wasPressed(); i++) {
            M5.update();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Play back the recording
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.println("Playing back...");
        
        if (!M5.Speaker.begin()) {
            M5.Display.println("Speaker init failed!");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        
        M5.Speaker.setVolume(255);  // Set volume to maximum
        
        // Calculate actual recorded samples for playback
        uint32_t actual_samples = last_non_zero > 0 ? last_non_zero + 1 : buffer_size;
        M5.Display.printf("Playing %lu samples\n", actual_samples);
        
        // Make sure to use the exact same sample rate for playback
        M5.Display.println("Starting playback");
        M5.Speaker.playRaw((int16_t*)buffer, actual_samples, SAMPLE_RATE, false, 1, 0);
        
        // Wait for playback to complete
        start_time = millis();
        prev_second = -1;
        while (M5.Speaker.isPlaying()) {
            int current_second = (millis() - start_time) / 1000;
            
            // Only update the display when the second changes
            if (current_second != prev_second) {
                M5.Display.fillRect(0, 40, 320, 30, BLACK);  // Clear previous text
                M5.Display.setCursor(0, 40);
                M5.Display.printf("Playing: %d sec", current_second);
                prev_second = current_second;
            }
            
            M5.update();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        elapsed = millis() - start_time;
        M5.Display.printf("Playback time: %lu ms\n", elapsed);
        
        M5.Speaker.end();
        M5.Display.println("Playback completed");
        M5.Display.println("Wait for next recording...");
        
        vTaskDelay(pdMS_TO_TICKS(3000));  // Longer wait before next cycle
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
    xTaskCreate(record_and_play_audio, "record_and_play_audio", 20480, NULL, 1, NULL);
    //return 0;
}

void loop() {
    // Your main loop code here
    vTaskDelay(pdMS_TO_TICKS(1000));
}
