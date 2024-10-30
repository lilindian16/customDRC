#include "Custom_DRC.hpp"

#include <Arduino.h>
#include <ArduinoNvs.h>
#include <esp_ota_ops.h>
#include "driver/rtc_io.h"

#define ENCODER_1_WAKEUP_PIN_MASK (((uint64_t)1) << ((uint64_t)ENCODER_1_SW))
#define RS485_RX_PIN_WAKEUP_MASK  (((uint64_t)1) << ((uint64_t)RS485_RX_PIN))

#define DSP_WAKEUP_PIN_MASK (ENCODER_1_WAKEUP_PIN_MASK | ENCODER_1_WAKEUP_PIN_MASK)

// #define TRIAL_TRANSMIT
#define TRIAL_RECEIVE

TaskHandle_t blinky_task_handle;

Audison_AC_Link_Bus Audison_AC_Link;
struct DSP_Settings dsp_settings;

#define NVS_HEADER_KEY          "nvsHead"
#define NVS_DSP_SETTINGS_KEY    "dspSet"
#define NVS_INPUT_SOURCE_KEY    "inputSource"
#define NVS_INPUT_SOURCE_LENGTH 16
const uint8_t NVS_Header[4] = {0xDE, 0xAD, 0xBE, 0xEF};
uint8_t nvs_dsp_settings[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x12};
const uint8_t master_source_name[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x4D, 0x61, 0x73, 0x74,
                                      0x65, 0x72, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00};

LED_Mode_t led_mode = LED_MODE_BOOTUP;

void blinky(void* pvParameters) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    while (1) {
        switch (led_mode) {
            case LED_MODE_DISABLED:
                digitalWrite(LED_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case LED_MODE_BOOTUP:
                digitalWrite(LED_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(100));
                digitalWrite(LED_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(900));
                break;
            case LED_MODE_DEVICE_RUNNING:
                digitalWrite(LED_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case LED_MODE_SHUT_DOWN_MODE:
                digitalWrite(LED_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(50));
                digitalWrite(LED_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(50));
            case LED_MODE_OTA_UPDATE:
                digitalWrite(LED_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(50));
                digitalWrite(LED_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(50));
            default:
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}

void change_led_mode(LED_Mode_t mode) {
    led_mode = mode;
}

void on_button_held(void) {
    ;
}

void on_button_released(bool button_was_held) {
    if (button_was_held) {
        ;
    }
}

void on_button_clicked(void) {
    Audison_AC_Link.change_source();
}

void load_dsp_settings_from_nvs(struct DSP_Settings* settings) {
    bool ok = NVS.getBlob(NVS_DSP_SETTINGS_KEY, nvs_dsp_settings, sizeof(nvs_dsp_settings));
    if (ok) {
        settings->memory_select = nvs_dsp_settings[DSP_SETTING_INDEX_MEMORY_SELECT];
        settings->master_volume = nvs_dsp_settings[DSP_SETTING_INDEX_MASTER_VOLUME];
        settings->sub_volume = nvs_dsp_settings[DSP_SETTING_INDEX_SUB_VOLUME];
        settings->balance = nvs_dsp_settings[DSP_SETTING_INDEX_BALANCE];
        settings->fader = nvs_dsp_settings[DSP_SETTING_INDEX_FADER];
    } else {
        log_e("Failed to load DSP settings from NVS");
    }

    ok = NVS.getBlob(NVS_INPUT_SOURCE_KEY, (uint8_t*)settings->current_source, sizeof(settings->current_source));
    if (!ok) {
        log_e("Failed to get the current source from NVS");
    }

    printf("\n*** DSP Settings NVS ***\n\tMEM: %d\n\tVOL: %d\n\tSUB: %d\n\tBAL: %d\n\tFAD: %d\n\tSRC: %s\n",
           settings->memory_select, settings->master_volume, settings->sub_volume, settings->balance,
           settings->fader, settings->current_source);
}

void write_dsp_settings_to_nvs(struct DSP_Settings* settings) {
    nvs_dsp_settings[DSP_SETTING_INDEX_MEMORY_SELECT] = settings->memory_select;
    nvs_dsp_settings[DSP_SETTING_INDEX_MASTER_VOLUME] = settings->master_volume;
    nvs_dsp_settings[DSP_SETTING_INDEX_SUB_VOLUME] = settings->sub_volume;
    nvs_dsp_settings[DSP_SETTING_INDEX_BALANCE] = settings->balance;
    nvs_dsp_settings[DSP_SETTING_INDEX_FADER] = settings->fader;
    bool ok = NVS.setBlob(NVS_DSP_SETTINGS_KEY, nvs_dsp_settings, sizeof(nvs_dsp_settings));
    if (!ok) {
        log_e("Failed to write DSP settings to NVS");
    }

    ok = NVS.setBlob(NVS_INPUT_SOURCE_KEY, (uint8_t*)dsp_settings.current_source, sizeof(dsp_settings.current_source));
    if (!ok) {
        log_e("Failed to write the current source to NVS");
    }
}

void init_custom_drc(void) {
    Serial.begin(115200);
    rtc_gpio_deinit(RS485_RX_PIN); // De-init the RTC GPIO and re-init to regular GPIO
    pinMode(DSP_PWR_EN_PIN, OUTPUT);
    digitalWrite(DSP_PWR_EN_PIN, LOW); // Make sure the DSP stays powered down on bootup

    xTaskCreatePinnedToCore(blinky, "blinky", 8000, NULL, tskIDLE_PRIORITY + 1, &blinky_task_handle, 1);

    // Get the OTA partitions that are running and the next one that it will point to
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* otaPartition = esp_ota_get_next_update_partition(NULL);
    Serial.printf("Running partition type %d subtype %d (offset 0x%08x)\n", running->type, running->subtype,
                  running->address);
    Serial.printf("OTA partition will be type %d subtype %d (offset 0x%x)\n", otaPartition->type, otaPartition->subtype,
                  otaPartition->address);

    Serial.printf("****** CDRC Firmware ******\n");
    Serial.printf("****** FW Version: %s *****\n", FW_VERSION);
    Serial.printf("******  J SEQUEIRA   ******\n");

    // We will eventually load the DSP settings from NVS
    NVS.begin();

    strcpy(dsp_settings.current_source, "Master"); // Make sure there is something in here

    uint8_t nvs_header_in_flash[4];
    bool ok = NVS.getBlob(NVS_HEADER_KEY, nvs_header_in_flash, sizeof(nvs_header_in_flash));
    if (memcmp(nvs_header_in_flash, NVS_Header, sizeof(nvs_header_in_flash)) != 0) {
        log_i("Failed to find header in NVS. We will reformat the NVS parition");
        ok = NVS.setBlob(NVS_HEADER_KEY, (uint8_t*)NVS_Header, sizeof(NVS_Header));
        ok = NVS.setBlob(NVS_DSP_SETTINGS_KEY, nvs_dsp_settings, sizeof(nvs_dsp_settings));
        ok = NVS.setBlob(NVS_INPUT_SOURCE_KEY, (uint8_t*)master_source_name, sizeof(master_source_name));
    } else {
        load_dsp_settings_from_nvs(&dsp_settings);
    }

    // We can now enable the DSP system
    digitalWrite(DSP_PWR_EN_PIN, HIGH);

    Serial.println("Waiting for DSP to boot ... ");

    // Wait for it to boot up
    delay(5000);

    Serial.println("Starting Custom DRC");

    Audison_AC_Link.init_ac_link_bus(&dsp_settings);

    init_drc_encoders(&dsp_settings);
    web_server_init(&dsp_settings);
}

void shut_down_dsp(void) {
    change_led_mode(LED_MODE_SHUT_DOWN_MODE);
    write_dsp_settings_to_nvs(&dsp_settings); // We need to save the DSP settings to NVS before shutting down
    Audison_AC_Link.turn_off_main_unit();
    digitalWrite(DSP_PWR_EN_PIN, LOW);
    rtc_gpio_pullup_en(RS485_RX_PIN);
    log_i("DSP shut down, now we wait 5 seconds before we put ourselves to sleep");
    delay(5000);
    change_led_mode(LED_MODE_DISABLED);
    esp_sleep_enable_ext1_wakeup(RS485_RX_PIN_WAKEUP_MASK, ESP_EXT1_WAKEUP_ALL_LOW);
    esp_deep_sleep_start();
    delay(1000);
    log_i("This should never print");
}