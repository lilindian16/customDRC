/**
 * Software specifically for integrating into the Audison AC Link Bus (RS485)
 *
 * Audison AC Link bus uses 9-bit serial data on the RS485 bus. The 9th bit of the data
 * indicates whether the data is an address (1) or data byte (0)
 *
 * MCU used: ESP32 WROVER-IE Module (ESP32-D0WD-V3)
 *
 * Author: Jaime Sequeira
 */

#pragma once

#include <stdint.h>
#include "Custom_DRC.hpp"

enum AC_Link_Address
{
    AC_LINK_ADDRESS_DSP_MASTER = 0x00,
    AC_LINK_ADDRESS_DSP_PROCESSOR = 0x46,
    AC_LINK_ADDRESS_USB_CONTROLLER = 0x5A,
    AC_LINK_ADDRESS_DRC = 0x80,
};

constexpr uint8_t MIN_VOLUME_VALUE = 0x00;
constexpr uint8_t MAX_VOLUME_VALUE = 0x78;
constexpr uint8_t MIN_SUB_VOLUME_VALUE = 0x00;
constexpr uint8_t MAX_SUB_VOLUME_VALUE = 0x18;
constexpr uint8_t MIN_FADER_VALUE = 0x00;
constexpr uint8_t MAX_FADER_VALUE = 0x24;
constexpr uint8_t MIN_BALANCE_VALUE = 0x00;
constexpr uint8_t MAX_BALANCE_VALUE = 0x24;

enum AC_Link_Command
{
    AC_LINK_COMMAND_INPUT_SOURCE_NAME = 0x09,
    AC_LINK_COMMAND_MASTER_VOLUME = 0x0A,
    AC_LINK_COMMAND_BALANCE_ADJUST = 0x0B,
    AC_LINK_COMMAND_FADER_ADJUST = 0x0C,
    AC_LINK_COMMAND_SUB_VOLUME_ADJUST = 0x0D,
    AC_LINK_COMMAND_CHANGE_DSP_MEMORY = 0x0F,
    AC_LINK_COMMAND_CHECK_DEVICE_PRESENT = 0x11,
    AC_LINK_COMMAND_DEVICE_IS_PRESENT = 0x12,
    AC_LINK_COMMAND_USB_CONNECTED = 0x13,
    AC_LINK_COMMAND_DEVICE_IS_DISCONNECTED = 0x35,
    AC_LINK_COMMAND_TURN_OFF_MAIN_UNIT = 0x6E, // Unknown yet

};

class Audison_AC_Link_Bus
{
public: // Public functions
    /**
     * @param rs485_config Config struct pointer
     */
    void init_ac_link_bus(struct DSP_Settings *settings);

    /**
     * @param volume Value between mute (0x00) and max volume (0x78)
     */
    void set_volume(uint8_t volume, uint8_t receiver_address = AC_LINK_ADDRESS_DSP_MASTER);

    /**
     * @param balance_level Balance between left (0x00) and right (0x24)
     */
    void set_balance(uint8_t balance_level, uint8_t receiver_address = AC_LINK_ADDRESS_DSP_MASTER);

    /**
     * @param fade_level Fade between front (0x00) and rear (0x24)
     */
    void set_fader(uint8_t fade_level, uint8_t receiver_address = AC_LINK_ADDRESS_DSP_MASTER);
    /**
     * @param sub_volume Value between 0x00 (mute) and 0x18 (-12dB)
     */
    void set_sub_volume(uint8_t sub_volume, uint8_t receiver_address = AC_LINK_ADDRESS_DSP_MASTER);

    /**
     * @param memory Value between 0x01 (A) and 0x02 (B)
     */
    void set_dsp_memory(uint8_t memory);

    void turn_off_main_unit(void);

    /**
     * Checks to see if USB device is plugged in and bus is inaccessible
     */
    void check_usb_on_bus(void);

    /**
     * Updates a device with the latest DRC settings
     */
    void update_device_with_latest_settngs(struct DSP_Settings *settings, uint8_t receiver_address = AC_LINK_ADDRESS_DSP_MASTER);

    /**
     * @param data_buffer Empty buffer for data to be populated into
     * @param buffer_length Length of buffer provided (bytes)
     * @returns Amount of bytes read and put into buffer
     */
    uint8_t read_rx_message(uint8_t *data_buffer, uint8_t buffer_length);

public:  // Public Variables
private: // Private functions
    /**
     * @param receiver_address Used for transmission. Parity bit will be marked to indicate address
     * @param transmitter_address
     * @param data Buffer for data to be sent in transaction
     * @param data_length Length of the data buffer
     */
    void write_to_audison_bus(uint8_t receiver_address, uint8_t transmitter_address, uint8_t *data, uint8_t data_length);

    /**
     * @param data_buffer   Buffer of data to be checksummed
     * @param data_length_bytes
     *
     * @returns Checksum byte (8-bit module 256)
     */
    uint8_t calculate_checksum(uint8_t *data_buffer, uint8_t data_length_bytes);

private: // Private Variables
    int tx_pin;
    int rx_pin;
    int tx_en_pin;
};

extern Audison_AC_Link_Bus Audison_AC_Link;