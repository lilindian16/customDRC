#include "Custom_DRC.hpp"

#include <Arduino.h>
#include <ArduinoNvs.h>
#include <esp_ota_ops.h>

#define FW_VERSION "0.0.1"

#define ENCODER_1_WAKEUP_PIN_MASK (((uint64_t)1) << ((uint64_t)ENCODER_1_SW))

// #define TRIAL_TRANSMIT
#define TRIAL_RECEIVE

TaskHandle_t blinky_task_handle;

Audison_AC_Link_Bus Audison_AC_Link;
struct DSP_Settings dsp_settings;

#define NVS_HEADER_KEY       "nvsHead"
#define NVS_DSP_SETTINGS_KEY "dspSet"
const uint8_t NVS_Header[4] = {0xDE, 0xAD, 0xBE, 0xEF};
uint8_t nvs_dsp_settings[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x12};

bool shut_down_mode_enabled = false;

void blinky(void* pvParameters) {
    while (1) {
        if (shut_down_mode_enabled) {
            digitalWrite(LED_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(50));
            digitalWrite(LED_PIN, LOW);
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            digitalWrite(LED_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void on_button_held(void) {
    shut_down_mode_enabled = true;
}

void on_button_released(bool button_was_held) {
    if (button_was_held) {
        shut_down_dsp();
    }
}

void load_dsp_settings_from_nvs(struct DSP_Settings* settings) {
    bool ok = NVS.getBlob(NVS_DSP_SETTINGS_KEY, nvs_dsp_settings, sizeof(nvs_dsp_settings));
    if (ok) {
        settings->memory_select = nvs_dsp_settings[DSP_SETTING_INDEX_MEMORY_SELECT];
        settings->input_select = nvs_dsp_settings[DSP_SETTING_INDEX_INPUT_SELECT];
        settings->mute = (bool)nvs_dsp_settings[DSP_SETTING_INDEX_MUTE];
        settings->master_volume = nvs_dsp_settings[DSP_SETTING_INDEX_MASTER_VOLUME];
        settings->sub_volume = nvs_dsp_settings[DSP_SETTING_INDEX_SUB_VOLUME];
        settings->balance = nvs_dsp_settings[DSP_SETTING_INDEX_BALANCE];
        settings->fader = nvs_dsp_settings[DSP_SETTING_INDEX_FADER];

        log_i("\n*** DSP Settings NVS ***\nMEM: %d\nINP: %d\nMUT: %d\nMV: %d\nSV: %d\nBAL: %d\nFAD: %d\n * **DSP "
              "Settings NVS * **",
              settings->memory_select, settings->input_select, settings->mute, settings->master_volume,
              settings->sub_volume, settings->balance, settings->fader);
    } else {
        log_e("Failed to load DSP settings from NVS");
    }
}

void write_dsp_settings_to_nvs(struct DSP_Settings* settings) {
    nvs_dsp_settings[DSP_SETTING_INDEX_MEMORY_SELECT] = settings->memory_select;
    nvs_dsp_settings[DSP_SETTING_INDEX_INPUT_SELECT] = settings->input_select;
    nvs_dsp_settings[DSP_SETTING_INDEX_MUTE] = settings->mute;
    nvs_dsp_settings[DSP_SETTING_INDEX_MASTER_VOLUME] = settings->master_volume;
    nvs_dsp_settings[DSP_SETTING_INDEX_SUB_VOLUME] = settings->sub_volume;
    nvs_dsp_settings[DSP_SETTING_INDEX_BALANCE] = settings->balance;
    nvs_dsp_settings[DSP_SETTING_INDEX_FADER] = settings->fader;
    bool ok = NVS.setBlob(NVS_DSP_SETTINGS_KEY, nvs_dsp_settings, sizeof(nvs_dsp_settings));
    if (!ok) {
        log_e("Failed to write DSP settings to NVS");
    }
}

void init_custom_drc(void) {
    Serial.begin(115200);

    pinMode(DSP_PWR_EN_PIN, OUTPUT);
    digitalWrite(DSP_PWR_EN_PIN, LOW);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

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

    uint8_t nvs_header_in_flash[4];
    bool ok = NVS.getBlob(NVS_HEADER_KEY, nvs_header_in_flash, sizeof(nvs_header_in_flash));
    if (memcmp(nvs_header_in_flash, NVS_Header, sizeof(nvs_header_in_flash)) != 0) {
        log_i("Failed to find header in NVS. We will reformat the NVS parition");
        ok = NVS.setBlob(NVS_HEADER_KEY, (uint8_t*)NVS_Header, sizeof(NVS_Header));
        ok = NVS.setBlob(NVS_DSP_SETTINGS_KEY, nvs_dsp_settings, sizeof(nvs_dsp_settings));
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
    xTaskCreatePinnedToCore(blinky, "blinky", 8000, NULL, tskIDLE_PRIORITY + 1, &blinky_task_handle, 1);
    init_drc_encoders(&dsp_settings);
    web_server_init(&dsp_settings);
}

void shut_down_dsp(void)
// We need to save the DSP settings to NVS before shutting down
{
    write_dsp_settings_to_nvs(&dsp_settings);
    Audison_AC_Link.turn_off_main_unit();
    digitalWrite(DSP_PWR_EN_PIN, LOW);
    // Enable low power mode now and wake-up on activity from push-button for now
    log_i("Enabling wakeup on ENC1 sw");
    esp_sleep_enable_ext1_wakeup(ENCODER_1_WAKEUP_PIN_MASK, ESP_EXT1_WAKEUP_ALL_LOW);
    delay(1000);
    esp_deep_sleep_start();
}