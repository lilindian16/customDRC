#include "CDRCWebServer.hpp"
#include "CustomDRChtml.h"
#include "CustomDRCcss.h"
#include "CustomDRCjs.h"
#include "Audison_AC_Link_Bus.hpp"
#include "DRC_Encoder.hpp"
#include "Custom_DRC.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Update.h>

/* Put your SSID & Password */
const char *ssid = "Custom-DRC";   // Enter SSID here
const char *password = "12345678"; // Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Create the web server
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket web_socket_handle("/ws");

struct DSP_Settings *dsp_settings_web_server;

bool client_connected_to_websocket = false;

void handle_json_key_value(JsonPair key_value)
{
    if (strcmp(key_value.key().c_str(), "getRemoteSettings") == 0)
    {
        Serial.println("*WS* Webpage loaded. Get settings");
        // Get the latest remote settings
        update_web_server_parameter(DSP_SETTING_INDEX_MEMORY_SELECT, dsp_settings_web_server->memory_select);
        update_web_server_parameter(DSP_SETTING_INDEX_INPUT_SELECT, dsp_settings_web_server->input_select);
        update_web_server_parameter(DSP_SETTING_INDEX_MUTE, (uint8_t)dsp_settings_web_server->mute);
        update_web_server_parameter(DSP_SETTING_INDEX_MASTER_VOLUME, dsp_settings_web_server->master_volume);
        update_web_server_parameter(DSP_SETTING_INDEX_SUB_VOLUME, dsp_settings_web_server->sub_volume);
        update_web_server_parameter(DSP_SETTING_INDEX_BALANCE, dsp_settings_web_server->balance);
        update_web_server_parameter(DSP_SETTING_INDEX_FADER, dsp_settings_web_server->fader);
        update_web_server_parameter(DSP_SETTING_INDEX_USB_CONNECTED, (uint8_t)dsp_settings_web_server->usb_connected);
    }
    else if (strcmp(key_value.key().c_str(), "password") == 0)
    {
        String password = key_value.value();
        Serial.printf("*WS* password: %s\n", password.c_str());
    }
    else if (strcmp(key_value.key().c_str(), "dspMemory") == 0)
    {
        uint8_t dspMemoryValue = key_value.value();
        dsp_settings_web_server->memory_select = dspMemoryValue;
        Serial.printf("*WS* dspMemory: %d\n", dspMemoryValue);
    }
    else if (strcmp(key_value.key().c_str(), "inputSelect") == 0)
    {
        uint8_t inputSelectValue = key_value.value();
        dsp_settings_web_server->input_select = inputSelectValue;
        Serial.printf("*WS* inputSelect: %d\n", inputSelectValue);
    }
    else if (strcmp(key_value.key().c_str(), "mute") == 0)
    {
        bool muteValue = key_value.value();
        Serial.printf("*WS* mute: %d\n", muteValue);
        dsp_settings_web_server->mute = muteValue;
        dsp_settings_web_server->master_volume = 0x00;
        Audison_AC_Link.set_volume(0); // Still need to find a way to mute the system while remembering the old volume
        update_web_server_parameter(DSP_SETTING_INDEX_MASTER_VOLUME, dsp_settings_web_server->master_volume);
    }
    else if (strcmp(key_value.key().c_str(), "masterVolume") == 0)
    {
        uint8_t master_volume_value = key_value.value();
        dsp_settings_web_server->master_volume = master_volume_value;
        Serial.printf("*WS* masterVolume: %d\n", master_volume_value);
        Audison_AC_Link.set_volume(master_volume_value);
    }
    else if (strcmp(key_value.key().c_str(), "subVolume") == 0)
    {
        uint8_t sub_volume_value = key_value.value();
        dsp_settings_web_server->sub_volume = sub_volume_value;
        Audison_AC_Link.set_sub_volume(sub_volume_value);
        Serial.printf("*WS* subVolume: %d\n", sub_volume_value);
    }
    else if (strcmp(key_value.key().c_str(), "balance") == 0)
    {
        uint8_t balance_value = key_value.value();
        dsp_settings_web_server->balance = balance_value;
        Audison_AC_Link.set_balance(balance_value);
        Serial.printf("*WS* balance: %d\n", balance_value);
    }
    else if (strcmp(key_value.key().c_str(), "fader") == 0)
    {
        uint8_t fader_value = key_value.value();
        dsp_settings_web_server->fader = fader_value;
        Audison_AC_Link.set_fader(fader_value);
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
        client_connected_to_websocket = true;
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        client_connected_to_websocket = false;
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

// handles uploads
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);

    if (!index)
    {
        logmessage = "Upload Start: " + String(filename);
        // open the file on first call and store the file handle in the request object
        // request->_tempFile = SPIFFS.open("/" + filename, "w");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            Update.printError(Serial);
        }
        Serial.println(logmessage);
    }

    if (len)
    {
        // stream the incoming chunk to the opened file
        // request->_tempFile.write(data, len);
        logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
        Serial.println(logmessage);
        if (Update.write(data, len) != len)
        {
            Update.printError(Serial);
            Serial.printf("Progress: %d%%\n", (Update.progress() * 100) / Update.size());
        }
    }

    if (final)
    {
        logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
        // close the file handle as the upload is now done
        // request->_tempFile.close();
        Serial.println(logmessage);
        if (!Update.end(true))
        {
            Update.printError(Serial);
        }
        else
        {
            Serial.println("Update complete - rebooting in 5 seconds");
            vTaskDelay(pdMS_TO_TICKS(5000));
            Serial.flush();
            ESP.restart();
        }
    }
}

// WARNING: This function is called from a separate FreeRTOS task (thread)!
void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event)
    {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Serial.println("Client connected");
        break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Serial.println("Client disconnected");
        break;
    default:
        break;
    }
}

void web_server_init(struct DSP_Settings *settings)
{
    dsp_settings_web_server = settings;
    WiFi.softAP(ssid, password, 1, 0, 1, false);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.onEvent(WiFiEvent);

    initWebSocket();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", (const uint8_t *)custom_html, sizeof(custom_html), nullptr); });

    server.on("/pico.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/css", (const uint8_t *)custom_css, sizeof(custom_css), nullptr); });

    server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/js", (const uint8_t *)custom_js, sizeof(custom_js), nullptr); });

    // run handleUpload function when any file is uploaded
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(200); }, handleUpload);

    server.begin();
    Serial.println("HTTP server started");
    server.onNotFound(notFound);
    server.begin();
}

void update_web_server_parameter(uint8_t parameter, uint8_t value)
{

    if (client_connected_to_websocket)
    {
        switch (parameter)
        {
        case DSP_SETTING_INDEX_MEMORY_SELECT:
            web_socket_handle.printfAll("{\"dspMemory\": %d}", value);
            break;

        case DSP_SETTING_INDEX_INPUT_SELECT:
            web_socket_handle.printfAll("{\"inputSelect\": %d}", value);
            break;

        case DSP_SETTING_INDEX_MUTE:
            web_socket_handle.printfAll("{\"mute\": %d}", value);
            break;

        case DSP_SETTING_INDEX_MASTER_VOLUME:
            web_socket_handle.printfAll("{\"masterVolume\": %d}", value);
            break;

        case DSP_SETTING_INDEX_SUB_VOLUME:
            web_socket_handle.printfAll("{\"subVolume\": %d}", value);
            break;

        case DSP_SETTING_INDEX_BALANCE:
            web_socket_handle.printfAll("{\"balance\": %d}", value);
            break;

        case DSP_SETTING_INDEX_FADER:
            web_socket_handle.printfAll("{\"fader\": %d}", value);
            break;

        case DSP_SETTING_INDEX_USB_CONNECTED:
            web_socket_handle.printfAll("{\"usbConnected\": %d}", value);
            break;

        default:
            log_e("Unknown web server parameter update request");
            break;
        }
    }
}