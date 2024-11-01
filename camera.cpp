// Dodawanie bibliotek do tworzenia serwera
#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "img_converters.h"
#include "Arduino.h"

// Zamiana danych do sieci WiFi
const char* ssid = "network";
const char* password = "password";

// Definiowanie modelu uzywanej plytki, w projekcie został wykorzystany AI_THINKER
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

String websocket_server_host;
const uint16_t websocket_server_port1 = 8000;

using namespace websockets;
WebsocketsClient client;

bool find_ip(int start, int end) {
    for (int i = start; i <= end; i++) {
        String ip = "192.168.55." + String(i);
        Serial.print("Sprawdzam: ");
        Serial.println(ip);

        if (client.connect(ip.c_str(), websocket_server_port1, "/check-ip")) {
            Serial.println("Połączenie nawiązane z: " + ip); 
            websocket_server_host = ip; 
             client.close(); 
            return true; 
        } else {
            Serial.println("Brak odpowiedzi od: " + ip); 
        }
        delay(10); 
    }
    return false; 
}

void onEventsCallback(WebsocketsEvent event, String data) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            Serial.println("Connection Opened");
            break;
        case WebsocketsEvent::ConnectionClosed:
            Serial.println("Connection Closed");
            ESP.restart(); // Restart the ESP on connection close
            break;
        case WebsocketsEvent::GotPing:
            Serial.println("Got a Ping!");
            break;
        case WebsocketsEvent::GotPong:
            Serial.println("Got a Pong!");
            break;
    }
}

void onMessageCallback(WebsocketsMessage message) {
   
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(false);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000; // Set XCLK frequency
    config.pixel_format = PIXFORMAT_JPEG; // Set pixel format to JPEG

  
    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10; // JPEG quality set to 10
        config.fb_count = 2; // Buffer count
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12; // JPEG quality set to 12
        config.fb_count = 1; // Buffer count
    }

  
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Wystąpił błąd 0x%x\n", err);
        return; // Return on error
    }


    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi połączone");



     if (!find_ip(100, 254)) {
        // If not found, search from 0 to 99
        if (!find_ip(0, 99)) {
            // If still not found, search from 200 to 255
            find_ip(200, 255);
        }
    }

    
  
    Serial.print("Streaming Kamery jest gotowy!");

    // Rozpoczęcie WebSocket
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    while (!client.connect(websocket_server_host, websocket_server_port1, "/ws")) {
        delay(500);
    }
}

void loop() {
    delay(1);
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Uchwycenie obrazu się nie powiodło");
        return; 
    }

    size_t _jpg_buf_len = 0;
    uint8_t* _jpg_buf = NULL;


    if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb); 
        if (!jpeg_converted) {
            Serial.println("JPEG compression failed");
            return; 
        }
    } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf; 
    }

    
    if (_jpg_buf) {
        client.sendBinary((const char*)_jpg_buf, _jpg_buf_len);
        Serial.print("Wysylam, buf_len: ");
Serial.println(_jpg_buf_len);
      
        if (fb->format != PIXFORMAT_JPEG) {
            free(_jpg_buf); 
        }
    }

    esp_camera_fb_return(fb); 
    delay(1); 
}

