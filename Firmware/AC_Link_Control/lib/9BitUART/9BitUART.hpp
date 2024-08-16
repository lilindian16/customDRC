/**
 * 9-bit UART
 * Software specifically for the Audison Bit 10 DRC using the AC Link Bus (RS485)
 *
 * Audison AC Link bus uses 9-bit serial data on the RS485 bus. The 9th bit of the data
 * indicates whether the data is an address (1) or data byte (0).
 *
 * Microcontroller used: ATMEGA328P - Arduino Uno
 *
 * Connections for ATMEGA328P with RS485 transceiver:
 *      MCU TX - Pin 1 -> RS485_TX
 *      MCU_RX - Pin 0 -> RS485_RX
 *
 * Author: Jaime Sequeira
 */

#pragma once

#include <stdint.h>

class UART_9BIT
{
public:
    void init_uart(unsigned long baudrate);

    /**
     * @param receiver_address
     * @param transmitter_address
     * @param data Buffer for data to be sent in transaction
     * @param data_length Length of the data buffer
     */
    void write_to_audison_bus(uint8_t receiver_address, uint8_t transmitter_address, uint8_t *data, uint8_t data_length);

private:
    void USART_Transmit(unsigned int data);
};