#include "CDRCWebServer.hpp"
#include "CustomDRChtml.h"
#include "CustomDRCcss.h"
#include "CustomDRCjs.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

/* Put your SSID & Password */
const char *ssid = "Custom-DRC"; // Enter SSID here
const char *password = "admin";  // Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Create the web server
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket web_socket_handle("/ws");

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        data[len] = '\0';
        Serial.printf("WS Message Rec: %s\n", data);
    }
}

// Websocket on event callback
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket()
{
    web_socket_handle.onEvent(onEvent);
    server.addHandler(&web_socket_handle);
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void web_server_init()
{
    WiFi.softAP(ssid, password);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);

    initWebSocket();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", (const uint8_t *)custom_html, sizeof(custom_html), nullptr); });

    server.on("/pico.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/css", (const uint8_t *)custom_css, sizeof(custom_css), nullptr); });

    server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/js", (const uint8_t *)custom_js, sizeof(custom_js), nullptr); });

    server.begin();
    Serial.println("HTTP server started");
    server.onNotFound(notFound);
    server.begin();
}