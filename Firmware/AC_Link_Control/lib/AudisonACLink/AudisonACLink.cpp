/**
 * Software specifically for the Audison Bit 10 DRC using the AC Link Bus (RS485)
 *
 * Audison AC Link bus uses 9-bit serial data on the RS485 bus. The 9th bit of the data
 * indicates whether the data is an address (1) or data byte (0).
 *
 * Microcontroller used: ESP32
 * Module: Wrover-IE
 *
 * Author: Jaime Sequeira
 */

#include "AudisonACLink.hpp"
#include <Arduino.h>
#include <SoftwareSerial.h> // https: //github.com/plerup/espsoftwareserial/tree/main

#define HEADER_SIZE_BYTES 4
#define CHECKSUM_SIZE_BYTES 1

EspSoftwareSerial::UART rs485_serial_port;

void AudisonACLink::init_ac_link_bus(RS485_Config_t *rs485_config)
{
    this->tx_pin = rs485_config->rs485_tx_pin;
    this->rx_pin = rs485_config->rs485_rx_pin;
    this->tx_en_pin = rs485_config->rs485_tx_en_pin;

    rs485_serial_port.begin(rs485_config->rs485_baudrate, SWSERIAL_8S1, this->rx_pin, this->tx_pin);
    digitalWrite(this->tx_en_pin, LOW); // RX EN pin is always low, we control flow with the TX pin going high
}

void AudisonACLink::write_to_audison_bus(uint8_t receiver_address, uint8_t transmitter_address, uint8_t *data, uint8_t data_length)
{
    uint32_t byte_sum = 0; // We use the sum of bytes for the checksum calculation
    uint8_t message_length = HEADER_SIZE_BYTES + data_length + CHECKSUM_SIZE_BYTES;
    uint8_t message_buffer[message_length];
    message_buffer[0] = receiver_address;
    message_buffer[1] = transmitter_address;
    message_buffer[2] = 0x00;
    message_buffer[3] = message_length;
    byte_sum += (receiver_address + transmitter_address + message_length); // Sum the bytes so far
    for (uint8_t i = 0; i < data_length; i++)
    {
        message_buffer[i + 4] = data[i];
        byte_sum += data[i];
    }
    /* Now we append the checksum - CheckSum8 Modulo 256 */
    uint8_t checksum = byte_sum % 256;
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