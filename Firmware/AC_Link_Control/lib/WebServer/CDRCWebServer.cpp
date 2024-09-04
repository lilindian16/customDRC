#include "CDRCWebServer.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "CustomDRChtml.h"
#include "CustomDRCcss.h"
#include "CustomDRCjs.h"

#ifdef TEMPLATE_PLACEHOLDER // if the macro TEMPLATE_PLACEHOLDER is defined
#undef TEMPLATE_PLACEHOLDER // un-define it
#endif
#define TEMPLATE_PLACEHOLDER '$' // define it with the new value

/* Put your SSID & Password */
const char *ssid = "ESP32";        // Enter SSID here
const char *password = "12345678"; // Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

uint8_t LED1pin = 4;
bool LED1status = LOW;

uint8_t LED2pin = 5;
bool LED2status = LOW;

TaskHandle_t web_server_task_handle;

void handle_OnConnect()
{
    LED1status = LOW;
    LED2status = LOW;
    Serial.println("GPIO4 Status: OFF | GPIO5 Status: OFF");
    server.send_P(200, "text/html", custom_html, sizeof(custom_html));
}

void handle_css_request(void)
{
    Serial.println("Handle css request");
    server.send_P(200, "text/css", custom_css, sizeof(custom_css));
    Serial.println("Sent css");
}

void handle_js_request(void)
{
    Serial.println("Sending JS");
    server.send_P(200, "text/javascript", custom_js, sizeof(custom_js));
}

void handle_NotFound()
{
    server.send(404, "text/plain", "Not found");
}

void web_server_task(void *pvParameters)
{
    while (1)
    {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void web_server_init()
{
    WiFi.softAP(ssid, password);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);

    server.on("/", handle_OnConnect);
    server.on("/pico.min.css", handle_css_request);
    server.on("/index.js", handle_js_request);
    server.onNotFound(handle_NotFound);

    server.begin();
    Serial.println("HTTP server started");

    xTaskCreatePinnedToCore(web_server_task, "WS", 8000, NULL, tskIDLE_PRIORITY + 1, &web_server_task_handle, 1);
}