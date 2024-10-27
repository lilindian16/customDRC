#pragma once
#include "Audison_AC_Link_Bus.hpp"
#include "CDRCWebServer.hpp"
#include "DRC_Encoder.hpp"

/* RS485 Pinouts */
#define RS485_TX_PIN GPIO_NUM_18
#define RS485_RX_PIN GPIO_NUM_21
#define RS485_TX_EN_PIN GPIO_NUM_19
#define RS485_BAUDRATE 38400

/* Encoder Pinouts */
#define ENCODER_1_A GPIO_NUM_35
#define ENCODER_1_B GPIO_NUM_32
#define ENCODER_1_SW GPIO_NUM_33
#define ENCODER_2_A GPIO_NUM_36
#define ENCODER_2_B GPIO_NUM_39
#define ENCODER_2_SW GPIO_NUM_34

/* Status LED Pinout */
#define LED_PIN 25

/* DSP Power Enable Output Pinout */
#define DSP_PWR_EN_PIN 5

enum DSP_Memory_Select
{
    DSP_MEMORY_A,
    DSP_MEMORY_B,
};

enum DSP_Input_Select
{
    DSP_INPUT_MASTER,
    DSP_INPUT_AUX,
    DSP_INPUT_PHONE,
};

struct DSP_Settings
{
    uint8_t memory_select = (uint8_t)DSP_MEMORY_A;
    uint8_t input_select = (uint8_t)DSP_INPUT_MASTER;
    bool mute = false;
    uint8_t master_volume = 0x00;
    uint8_t sub_volume = 0x00;
    uint8_t balance = 18;
    uint8_t fader = 18;
    bool usb_connected = false;
};

enum DSP_Settings_Indexes
{
    DSP_SETTING_INDEX_MEMORY_SELECT,
    DSP_SETTING_INDEX_INPUT_SELECT,
    DSP_SETTING_INDEX_MUTE,
    DSP_SETTING_INDEX_MASTER_VOLUME,
    DSP_SETTING_INDEX_SUB_VOLUME,
    DSP_SETTING_INDEX_BALANCE,
    DSP_SETTING_INDEX_FADER,
    DSP_SETTING_INDEX_USB_CONNECTED,
};

/* Functions */
void init_custom_drc(void);
void shut_down_dsp(void);