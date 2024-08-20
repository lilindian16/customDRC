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

#include "AudisonACLink.hpp"
#include <Arduino.h>
#include <SoftwareSerial.h> // https: //github.com/plerup/espsoftwareserial/tree/main

#define HEADER_SIZE_BYTES 4
#define CHECKSUM_SIZE_BYTES 1
#define AUDISON_DSP_RS485_ADDRESS 0x00
#define AUDISON_DRC_RS485_ADDRESS 0x80

EspSoftwareSerial::UART rs485_serial_port;

void AudisonACLink::init_ac_link_bus(RS485_Config_t *rs485_config)
{
    this->tx_pin = rs485_config->rs485_tx_pin;
    this->rx_pin = rs485_config->rs485_rx_pin;
    this->tx_en_pin = rs485_config->rs485_tx_en_pin;

    rs485_serial_port.begin(rs485_config->rs485_baudrate, SWSERIAL_8S1, this->rx_pin, this->tx_pin);
    digitalWrite(this->tx_en_pin, LOW); // RX EN pin is always low, we control flow with the TX pin going high
}

void AudisonACLink::set_volume(uint8_t volume)
{
    if (volume <= 0x78)
    {
        uint8_t volume_adjust_packet[2] = {VOLUME_ADJUST, volume};
        this->write_to_audison_bus(AUDISON_DSP_RS485_ADDRESS,
                                   AUDISON_DRC_RS485_ADDRESS,
                                   (uint8_t *)volume_adjust_packet,
                                   sizeof(volume_adjust_packet));
    }
}

void AudisonACLink::write_to_audison_bus(uint8_t receiver_address, uint8_t transmitter_address, uint8_t *data, uint8_t data_length)
{
    uint8_t message_length = HEADER_SIZE_BYTES + data_length + CHECKSUM_SIZE_BYTES;
    uint8_t message_buffer[message_length];
    message_buffer[0] = receiver_address;
    message_buffer[1] = transmitter_address;
    message_buffer[2] = 0x00;
    message_buffer[3] = message_length;
    for (uint8_t i = 0; i < data_length; i++)
    {
        message_buffer[i + 4] = data[i];
    }
    /* Now we append the checksum - CheckSum8 Modulo 256 */
    uint8_t checksum = this->calculate_checksum(message_buffer, sizeof(message_buffer) - 1); // Last byte is the checksum
    message_buffer[message_length - 1] = checksum;

    digitalWrite(this->tx_en_pin, HIGH); // TX output enable

    for (uint8_t i = 0; i < message_length; i++)
    {
        if (i == 0)
        {
            rs485_serial_port.write(message_buffer[i], EspSoftwareSerial::PARITY_MARK); // Append 1 to data to show address
        }
        else
        {
            rs485_serial_port.write(message_buffer[i], EspSoftwareSerial::PARITY_SPACE);
        }
    }

    digitalWrite(this->tx_en_pin, LOW); // TX output disable
}

uint8_t AudisonACLink::calculate_checksum(uint8_t *data_buffer, uint8_t data_length_bytes)
{
    uint32_t byte_sum = 0;
    for (uint8_t i = 0; i < data_length_bytes; i++)
    {
        byte_sum += data_buffer[i];
    }
    uint8_t checksum = byte_sum % 256;
    return checksum;
}