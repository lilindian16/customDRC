/**
 * Author: Jaime Sequeira
 */

#pragma once

#include "AudisonACLinkBus.hpp"
#include "CustomDRCWebServer.hpp"
#include "DRCEncoder.hpp"

constexpr uint8_t DRC_FIRMWARE_VERSION[2] = {0x03, 0x00};

/* RS485 Pinouts */
#define RS485_TX_PIN    GPIO_NUM_18
#define RS485_RX_PIN    GPIO_NUM_27
#define RS485_TX_EN_PIN GPIO_NUM_19
#define RS485_BAUDRATE  38400

/* Encoder Pinouts */
#define ENCODER_1_A  GPIO_NUM_35
#define ENCODER_1_B  GPIO_NUM_32
#define ENCODER_1_SW GPIO_NUM_33
#define ENCODER_2_A  GPIO_NUM_36
#define ENCODER_2_B  GPIO_NUM_39
#define ENCODER_2_SW GPIO_NUM_34

/* Status LED Pinout */
#define LED_PIN 25

/* DSP Power Enable Output Pinout */
#define DSP_PWR_EN_PIN 5

enum DSP_Memory_Select {
    DSP_MEMORY_A,
    DSP_MEMORY_B,
};

enum DSP_Input_Select {
    DSP_INPUT_MASTER,
    DSP_INPUT_AUX,
    DSP_INPUT_DIGITAL_OPTICAL,
};

struct DSP_Settings {
    uint8_t memory_select = (uint8_t)DSP_MEMORY_A;
    char current_source[17];
    uint8_t master_volume = 0x00;
    uint8_t sub_volume = 0x00;
    uint8_t balance = 18;
    uint8_t fader = 18;
    bool usb_connected = false;
};

enum DSP_Settings_Indexes {
    DSP_SETTING_INDEX_MEMORY_SELECT,
    DSP_SETTINGS_CURRENT_INPUT_SOURCE,
    DSP_SETTING_INDEX_MASTER_VOLUME,
    DSP_SETTING_INDEX_SUB_VOLUME,
    DSP_SETTING_INDEX_BALANCE,
    DSP_SETTING_INDEX_FADER,
    DSP_SETTING_INDEX_USB_CONNECTED,
    FIRMWARE_VERSION_NUMBER_STRING_PARAMETER,
};

typedef enum {
    LED_MODE_DISABLED = 0,
    LED_MODE_BOOTUP,
    LED_MODE_DEVICE_RUNNING,
    LED_MODE_USB_CONNECTED,
    LED_MODE_OTA_UPDATE,
    LED_MODE_SHUT_DOWN_MODE,
} LED_Mode_t;

/**
 * Initialise the Custom Digital Remote Control (DRC). This loads all settings and starts up the webserver
 */
void init_custom_drc(void);

/**
 * Shut down the main DSP unit. Also enter hibernation mode - wake on acitivity on the bus
 */
void shut_down_dsp(void);

/**
 * Change the LED mode
 *
 * @param led_mode Mode to change to
 */
void change_led_mode(LED_Mode_t led_mode);