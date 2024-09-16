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

#define AUDISON_DSP_RS485_ADDRESS 0x00
#define AUDISON_DRC_RS485_ADDRESS 0x80

typedef struct
{
    int rs485_tx_pin;
    int rs485_rx_pin;
    int rs485_tx_en_pin;
    unsigned long rs485_baudrate = 38400;
} RS485_Config_t;

enum AC_Link_Command
{
    VOLUME_ADJUST = 0x0A,
    BALANCE_ADJUST = 0x0B,
    FADER_ADJUST = 0x0C,
    SUB_VOLUME_ADJUST = 0x0D,
};

class AudisonACLink
{
public: // Public functions
    /**
     * @param rs485_config Config struct pointer
     */
    void init_ac_link_bus(RS485_Config_t *rs485_config);

    /**
     * @param volume Value between mute (0x00) and max volume (0x78)
     */
    void set_volume(uint8_t volume);

    /**
     * @param balance_level Balance between left (0x00) and right (0x24)
     */
    void set_balance(uint8_t balance_level);

    /**
     * @param fade_level Fade between front (0x00) and rear (0x24)
     */
    void set_fader(uint8_t fade_level);

    /**
     * @param sub_volume Value between 0x00 (mute) and 0x18 (-12dB)
     */
    void set_sub_volume(uint8_t sub_volume);

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