#include "DRC_Encoder.hpp"
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "Audison_AC_Link_Bus.hpp"

#define ENCODER_1_A 35
#define ENCODER_1_B 32
#define ENCODER_1_SW 33

#define ENCODER_2_A GPIO_NUM_36
#define ENCODER_2_B GPIO_NUM_39
#define ENCODER_2_SW GPIO_NUM_34

constexpr uint8_t ENCODER_1_ID = 0x01;
constexpr uint8_t ENCODER_2_ID = 0x02;

ESP32Encoder encoder_1(true, enc_cb);
ESP32Encoder encoder_2(true, enc_cb);
TaskHandle_t encoder_task_handle;
SemaphoreHandle_t encoder_1_semaphore_handle, encoder_2_semaphore_handle;

void encoder_task(void *pvParameters); // Forward declaration

static IRAM_ATTR void enc_cb(void *arg)
{
    ESP32Encoder *enc = (ESP32Encoder *)arg;
    BaseType_t xYieldRequired;
    uint8_t encoder_id = enc->get_encoder_id();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (encoder_id == ENCODER_1_ID)
    {
        xSemaphoreGiveFromISR(encoder_1_semaphore_handle, &xHigherPriorityTaskWoken);
    }
    // Resume the suspended task.
    xYieldRequired = xTaskResumeFromISR(encoder_task_handle);
    // We should switch context so the ISR returns to a different task.
    // NOTE:  How this is done depends on the port you are using.  Check
    // the documentation and examples for your port.
    portYIELD_FROM_ISR(xYieldRequired);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void encoder_task(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(encoder_1_semaphore_handle, TickType_t(10)))
        {
            int64_t encoder_1_count = encoder_1.getCount(); // Default to using encoder_1 for volume
            if (encoder_1_count > MAX_VOLUME_VALUE)
            {
                encoder_1.setCount(MAX_VOLUME_VALUE);
                encoder_1_count = MAX_VOLUME_VALUE;
            }
            else if (encoder_1_count < MIN_VOLUME_VALUE)
            {
                encoder_1.setCount(MIN_VOLUME_VALUE);
                encoder_1_count = MIN_VOLUME_VALUE;
            }
            Audison_AC_Link.set_volume(encoder_1_count);
            Serial.println("Encoder_1 count = " + String(encoder_1_count));
        }
        if (xSemaphoreTake(encoder_2_semaphore_handle, TickType_t(10)))
        {
            int64_t encoder_2_count = encoder_2.getCount();
            if (encoder_2_count > MAX_SUB_VOLUME_VALUE)
            {
                encoder_2.setCount(MAX_SUB_VOLUME_VALUE);
                encoder_2_count = MAX_SUB_VOLUME_VALUE;
            }
            else if (encoder_2_count < MIN_SUB_VOLUME_VALUE)
            {
                encoder_2.setCount(MIN_SUB_VOLUME_VALUE);
                encoder_2_count = MIN_SUB_VOLUME_VALUE;
            }
            Audison_AC_Link.set_sub_volume(encoder_2_count);
            Serial.println("Encoder_2 count = " + String(encoder_2_count));
        }
        vTaskSuspend(NULL);
    }
}

void init_drc_encoders(void)
{
    encoder_1_semaphore_handle = xSemaphoreCreateBinary();
    encoder_2_semaphore_handle = xSemaphoreCreateBinary();
    encoder_1.attachSingleEdge(ENCODER_1_A, ENCODER_1_B);
    encoder_1.clearCount(); // set starting count value after attaching
    encoder_1.setFilter(1023);
    encoder_1.set_encoder_id(ENCODER_1_ID);
    encoder_2.attachSingleEdge(ENCODER_2_A, ENCODER_2_B);
    encoder_2.clearCount();
    encoder_2.setFilter(1023);
    encoder_2.set_encoder_id(ENCODER_2_ID);
    xTaskCreatePinnedToCore(encoder_task, "ENCODER", 8000, NULL, tskIDLE_PRIORITY + 1, &encoder_task_handle, 1);
}