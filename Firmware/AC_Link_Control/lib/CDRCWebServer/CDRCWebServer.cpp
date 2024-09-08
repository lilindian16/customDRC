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

void handle_json_key_value(JsonPair key_value)
{
    if (strcmp(key_value.key().c_str(), "getRemoteSettings") == 0)
    {
        Serial.println("*WS* Webpage loaded. Get settings");
    }
    else if (strcmp(key_value.key().c_str(), "password") == 0)
    {
        String password = key_value.value();
        Serial.printf("*WS* password: %s\n", password.c_str());
    }
    else if (strcmp(key_value.key().c_str(), "dspMemory") == 0)
    {
        uint8_t dspMemoryValue = key_value.value();
        Serial.printf("*WS* dspMemory: %d\n", dspMemoryValue);
    }
    else if (strcmp(key_value.key().c_str(), "inputSelect") == 0)
    {
        uint8_t inputSelectValue = key_value.value();
        Serial.printf("*WS* inputSelect: %d\n", inputSelectValue);
    }
    else if (strcmp(key_value.key().c_str(), "mute") == 0)
    {
        uint8_t muteValue = key_value.value();
        Serial.printf("*WS* mute: %d\n", muteValue);
    }
    else if (strcmp(key_value.key().c_str(), "masterVolume") == 0)
    {
        uint8_t master_volume_value = key_value.value();
        Serial.printf("*WS* masterVolume: %d\n", master_volume_value);
    }
    else if (strcmp(key_value.key().c_str(), "subVolume") == 0)
    {
        uint8_t sub_volume_value = key_value.value();
        Serial.printf("*WS* subVolume: %d\n", sub_volume_value);
    }
    else if (strcmp(key_value.key().c_str(), "balance") == 0)
    {
        uint8_t balance_value = key_value.value();
        Serial.printf("*WS* balance: %d\n", balance_value);
    }
    else if (strcmp(key_value.key().c_str(), "fader") == 0)
    {
        uint8_t fader_value = key_value.value();
        Serial.printf("*WS* fader: %d\n", fader_value);
    }
    else
    {
        Serial.println("Unknown JSON format key value pair");
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        data[len] = '\0';
        // Serial.printf("WS Message Rec: %s\n", data);
        JsonDocument doc;                                        // Allocate the JSON document
        DeserializationError error = deserializeJson(doc, data); // Parse JSON object
        if (error == DeserializationError::Ok)
        {
            for (JsonPair kv : doc.as<JsonObject>())
            {
                handle_json_key_value(kv);
            }
        }
        else
        {
            Serial.println("Error parsing JSON");
        }
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