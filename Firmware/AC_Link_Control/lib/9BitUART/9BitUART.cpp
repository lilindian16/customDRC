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

#include "9BitUART.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>

#define HEADER_SIZE_BYTES 4
#define CHECKSUM_SIZE_BYTES 1

void UART_9BIT::init_uart(unsigned long baudrate)
{
    /*Set baud rate */
    unsigned long myubr = F_CPU / 16 / baudrate - 1;
    UBRR0H = (unsigned char)(myubr >> 8);
    UBRR0L = (unsigned char)myubr;
    /* Enable receiver and transmitter. Also set UCSZ02 to allow for 9-bit data */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | 1 << UCSZ02;
    /* Set frame format: 9 data, 1 stop bit, no parity */
    UCSR0C = (0b11 << UCSZ00) | (0 << USBS0);
}

void UART_9BIT::write_to_audison_bus(uint8_t receiver_address, uint8_t transmitter_address, uint8_t *data, uint8_t data_length)
{
    uint32_t byte_sum = 0; // We use the sum of bytes for the checksum calculation
    uint8_t message_length = HEADER_SIZE_BYTES + data_length + CHECKSUM_SIZE_BYTES;
    USART_Transmit(receiver_address | 0x0100);
    USART_Transmit(transmitter_address);
    USART_Transmit(0x00);
    USART_Transmit(message_length);
    byte_sum += (receiver_address + transmitter_address + message_length);
    for (uint8_t i = 0; i < data_length; i++)
    {
        USART_Transmit(data[i]);
        byte_sum += data[i];
    }
    /* Now we append ther checksum - CheckSum8 Modulo 256 */
    uint8_t checksum = byte_sum % 256;
    USART_Transmit(checksum);
}

void UART_9BIT::USART_Transmit(unsigned int data)
{
    /* Wait for empty transmit buffer */
    while (!(UCSR0A & (1 << UDRE0)))
    {
        ;
    }
    /* Copy 9th bit to TXB80 */
    UCSR0B &= ~(1 << TXB80);
    if (data & 0x0100)
    {
        UCSR0B |= (1 << TXB80);
    }
    /* Put data into buffer, sends the data */
    UDR0 = data;
}