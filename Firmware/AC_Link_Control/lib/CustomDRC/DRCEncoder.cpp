/**
 * Author: Jaime Sequeira
 */

#include "DRCEncoder.hpp"

#include "AudisonACLinkBus.hpp"
#include "CustomDRC.hpp"
#include "CustomDRCWebServer.hpp"

#include <Arduino.h>
#include <ESP32Encoder.h>

constexpr uint8_t ENCODER_1_ID = 0x01;
constexpr uint8_t ENCODER_2_ID = 0x02;

#define BUTTON_HELD_TIME_SECONDS   1
#define ENCODER_TASK_PERIOD_MS     50
#define BUTTON_HELD_TICKS_REQUIRED (BUTTON_HELD_TIME_SECONDS * 1000 / ENCODER_TASK_PERIOD_MS)

ESP32Encoder encoder_1(true, enc_cb);
ESP32Encoder encoder_2(true, enc_cb);
TaskHandle_t encoder_task_handle;
SemaphoreHandle_t encoder_1_semaphore_handle, encoder_2_semaphore_handle;

struct DSP_Settings* dsp_settings_encoders;

volatile bool encoders_enabled = true;

void encoder_task(void* pvParameters); // Forward declaration

static IRAM_ATTR void enc_cb(void* arg) {
    BaseType_t xYieldRequired;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ESP32Encoder* enc = (ESP32Encoder*)arg;
    uint8_t encoder_id = enc->get_encoder_id();

    if (!dsp_settings_encoders->usb_connected) {
        if (encoder_id == ENCODER_1_ID) {
            xSemaphoreGiveFromISR(encoder_1_semaphore_handle, &xHigherPriorityTaskWoken);
        } else if (encoder_id == ENCODER_2_ID) {
            xSemaphoreGiveFromISR(encoder_2_semaphore_handle, &xHigherPriorityTaskWoken);
        }
    }

    // Resume the suspended task.
    xYieldRequired = xTaskResumeFromISR(encoder_task_handle);
    // We should switch context so the ISR returns to a different task.
    // NOTE:  How this is done depends on the port you are using.  Check
    // the documentation and examples for your port.
    portYIELD_FROM_ISR(xYieldRequired);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void encoder_task(void* pvParameters) {
    uint8_t button_held_ticks = 0;
    bool button_held_task_sent = false;
    while (1) {
        if (encoders_enabled) {
            if (xSemaphoreTake(encoder_1_semaphore_handle, TickType_t(10))) {
                int64_t encoder_1_count = encoder_1.getCount(); // Default to using encoder_1 for volume
                if (encoder_1_count > MAX_VOLUME_VALUE) {
                    encoder_1.setCount(MAX_VOLUME_VALUE);
                    encoder_1_count = MAX_VOLUME_VALUE;
                } else if (encoder_1_count < MIN_VOLUME_VALUE) {
                    encoder_1.setCount(MIN_VOLUME_VALUE);
                    encoder_1_count = MIN_VOLUME_VALUE;
                }
                Audison_AC_Link.set_volume(encoder_1_count);
                update_web_server_parameter(DSP_SETTING_INDEX_MASTER_VOLUME, encoder_1_count);
                dsp_settings_encoders->master_volume = encoder_1_count;
            }
            if (xSemaphoreTake(encoder_2_semaphore_handle, TickType_t(10))) {
                int64_t encoder_2_count = encoder_2.getCount();
                if (encoder_2_count > MAX_SUB_VOLUME_VALUE) {
                    encoder_2.setCount(MAX_SUB_VOLUME_VALUE);
                    encoder_2_count = MAX_SUB_VOLUME_VALUE;
                } else if (encoder_2_count < MIN_SUB_VOLUME_VALUE) {
                    encoder_2.setCount(MIN_SUB_VOLUME_VALUE);
                    encoder_2_count = MIN_SUB_VOLUME_VALUE;
                }
                Audison_AC_Link.set_sub_volume(encoder_2_count);
                update_web_server_parameter(DSP_SETTING_INDEX_SUB_VOLUME, encoder_2_count);
                dsp_settings_encoders->sub_volume = encoder_2_count;
            }
            bool current_encoder_1_sw_state = digitalRead(ENCODER_1_SW);
            if (current_encoder_1_sw_state == LOW) {
                // Button has been pressed
                button_held_ticks++;
                if (button_held_ticks >= BUTTON_HELD_TICKS_REQUIRED) {
                    button_held_ticks = BUTTON_HELD_TICKS_REQUIRED;
                    if (!button_held_task_sent) {
                        on_button_held();
                    }
                }
            } else if (current_encoder_1_sw_state == HIGH) {
                // Button released
                if (button_held_ticks >= BUTTON_HELD_TICKS_REQUIRED) {
                    // Call method only if button has been held long enough
                    on_button_released(true);
                    button_held_task_sent = false; // We can reset the flag now
                } else if (button_held_ticks >= 1) {
                    button_held_ticks = 0;
                    on_button_clicked();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ENCODER_TASK_PERIOD_MS));
    }
}

void init_drc_encoders(struct DSP_Settings* settings) {
    dsp_settings_encoders = settings;
    encoder_1_semaphore_handle = xSemaphoreCreateBinary();
    encoder_2_semaphore_handle = xSemaphoreCreateBinary();
    encoder_1.attachSingleEdge(ENCODER_1_A, ENCODER_1_B);
    encoder_1.setCount(dsp_settings_encoders->master_volume);
    encoder_1.setFilter(1023);
    encoder_1.set_encoder_id(ENCODER_1_ID);
    encoder_2.attachSingleEdge(ENCODER_2_A, ENCODER_2_B);
    encoder_2.setCount(dsp_settings_encoders->sub_volume);
    encoder_2.setFilter(1023);
    encoder_2.set_encoder_id(ENCODER_2_ID);

    pinMode(ENCODER_1_SW, INPUT);
    pinMode(ENCODER_2_SW, INPUT);

    xTaskCreatePinnedToCore(encoder_task, "ENCODER", 8 * 1024, NULL, tskIDLE_PRIORITY + 1, &encoder_task_handle, 1);
}

void disable_encoders(void) {
    encoder_1.pauseCount();
    encoder_2.pauseCount();
    encoders_enabled = false;
}

void enable_encoders(void) {

    // Encoders cannot run while USB is plugged in and configuring DSP
    encoder_1.setCount(dsp_settings_encoders->master_volume);
    encoder_2.setCount(dsp_settings_encoders->sub_volume);
    encoders_enabled = true;
    encoder_1.resumeCount();
    encoder_2.resumeCount();
}

bool are_encoders_enabled(void) {
    return encoders_enabled;
}