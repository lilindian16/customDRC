/**
 * Software specifically for integrating into the Audison AC Link Bus (RS485)
 *
 * Audison AC Link bus uses 9-bit serial data on the RS485 bus. The 9th bit of
 * the data indicates whether the data is an address (1) or data byte (0)
 *
 * MCU used: ESP32 WROVER-IE Module (ESP32-D0WD-V3)
 *
 * Author: Jaime Sequeira
 */

#include "Audison_AC_Link_Bus.hpp"
#include <Arduino.h>
#include <SoftwareSerial.h> // https: //github.com/plerup/espsoftwareserial/tree/main

#include <driver/rmt.h>
#include "esp_check.h"
#include "esp_log.h"
static const char* TAG = "rmt-uart";

/**
 * RMT payload uses "items". Each item can portray 2 bits on serial. We require a
 * START bit, 9 bits of data and STOP bit. We require 11 bits but use 12
 * (1 extra idle bit) which means we require 6 items to send 1 byte on the bus
 */
#define RMT_ITEMS_REQUIRED_FOR_9_BIT_DATA 6

/**
 * Our ring buffer uses 16-bit data to pack more packet info
 * We want the ring buffer to hold ~4kB of packet data so we allocate 4kB
 * of memory sized at 16-bits
 */
#define RING_BUFFER_LENGTH_BYTES (sizeof(uint16_t)) * 1024 * 4

/* Macros for the AC Link Bus data packets */
#define HEADER_SIZE_BYTES   4
#define CHECKSUM_SIZE_BYTES 1

/* Software Serial object handle */
EspSoftwareSerial::UART rs485_serial_port;

/* FreeRTOS task handles */
TaskHandle_t rs485_bus_device_polling_task_handle, usb_connected_task_handle, rs485_bus_task_handle;

/* Task prioritites */
#define TX_TASK_PRIORITY            tskIDLE_PRIORITY + 1
#define USB_CONNECTED_TASK_PRIORITY tskIDLE_PRIORITY + 1

bool master_mcu_is_on_bus = false; // Flag set to false by default, set to true when MCU acks on bus

/* DSP settings struct ptr */
struct DSP_Settings* dsp_settings_rs485;

void Audison_AC_Link_Bus::parse_rx_message(uint8_t* message, uint8_t message_len) {
    if (message_len > 5) {
        uint8_t receiver = message[0];
        uint8_t transmitter = message[1];
        uint8_t message_length = message[3];
        uint8_t command = message[4];

        // Print all messages on serial
        for (uint8_t i = 0; i < message_len; i++) {
            Serial.print(message[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        if (receiver == AC_LINK_ADDRESS_DRC) // Filter only DRC addressed messages
        {
            if (transmitter == AC_LINK_ADDRESS_COMPUTER) {
                switch (command) {
                    case AC_LINK_COMMAND_DEVICE_IS_PRESENT:
                        log_i("USB connected. RS485 bus inactive");
                        change_led_mode(LED_MODE_USB_CONNECTED);
                        this->send_fw_version_to_usb();
                        update_web_server_parameter(DSP_SETTING_INDEX_USB_CONNECTED, 1);
                        disable_encoders();
                        vTaskResume(usb_connected_task_handle);   // Enable the USB connected task
                        dsp_settings_rs485->usb_connected = true; // We set this flag last to avoid being unable to
                                                                  // write to bus
                        vTaskSuspend(rs485_bus_device_polling_task_handle); // Suspend the TX task last to avoid hanging
                                                                            // ourselves without completing
                        break;
                    case AC_LINK_COMMAND_DEVICE_IS_DISCONNECTED:
                        log_i("USB disconnected. RS485 bus active");
                        change_led_mode(LED_MODE_DEVICE_RUNNING);
                        dsp_settings_rs485->usb_connected = false;
                        enable_encoders();
                        this->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_MASTER_MCU);
                        vTaskDelay(pdMS_TO_TICKS(500));
                        this->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_DSP_PROCESSOR);
                        update_web_server_parameter(DSP_SETTING_INDEX_USB_CONNECTED, 0);
                        vTaskResume(rs485_bus_device_polling_task_handle); // Enable the TX task that pings devices on
                                                                           // bus
                        vTaskSuspend(usb_connected_task_handle); // Disable the USB connected task last to avoid hanging
                                                                 // ourselves without completing
                        break;
                    default:
                        log_i("RS485->USB->DRC, unknown command received: %02x", command);
                        break;
                }
            } else if (transmitter == AC_LINK_ADDRESS_MASTER_MCU) {
                switch (command) {
                    case AC_LINK_COMMAND_DEVICE_IS_PRESENT:
                        if (!master_mcu_is_on_bus) {
                            log_i("Master MCU has joined the bus");
                            master_mcu_is_on_bus = true;
                        }
                        break;
                    default:
                        log_i("RS485->MASTER_MCU->DRC, unknown command received from Master MCU "
                              "processor: %02x",
                              command);
                        break;
                }
            } else if (transmitter == AC_LINK_ADDRESS_DSP_PROCESSOR) {
                switch (command) {
                    case AC_LINK_COMMAND_DEVICE_IS_PRESENT:
                        this->dsp_on_bus = true;
                        this->dsp_ping_count = 0;
                        break;
                    case AC_LINK_COMMAND_INPUT_SOURCE_NAME:
                        if (message_length == 0x16) {
                            // Copy the source name to the internal buffer
                            memcpy(dsp_settings_rs485->current_source, &message[5], 16);
                            update_web_server_parameter_string(DSP_SETTINGS_CURRENT_INPUT_SOURCE,
                                                               dsp_settings_rs485->current_source);
                            log_i("Current input source: %s", dsp_settings_rs485->current_source);
                        }
                        break;
                    default:
                        log_i("RS485->DSP_Pros->DRC, unknown command received by DSP processor");
                        break;
                }
            } else {
                log_e("RS485 unknown sender: %02x", transmitter);
            }
        } else if (receiver == AC_LINK_ADDRESS_MASTER_MCU) {
            if (transmitter == AC_LINK_ADDRESS_COMPUTER) {
                // Computer is communuicating to master MCU via Bit Tune software
                switch (command) {
                    case AC_LINK_COMMAND_CHANGE_DSP_MEMORY:
                        dsp_settings_rs485->memory_select = message[5] - 1; // Offset for DSP index 1
                        update_web_server_parameter(DSP_SETTING_INDEX_MEMORY_SELECT, message[5] - 1);
                        break;
                    case AC_LINK_COMMAND_MASTER_VOLUME:
                        dsp_settings_rs485->master_volume = message[5];
                        update_web_server_parameter(DSP_SETTING_INDEX_MASTER_VOLUME, message[5]);
                        break;
                    case AC_LINK_COMMAND_SUB_VOLUME_ADJUST:
                        dsp_settings_rs485->sub_volume = message[5];
                        update_web_server_parameter(DSP_SETTING_INDEX_SUB_VOLUME, message[5]);
                        break;
                    default:
                        log_e("RS485->Master->USB: Unknown command received");
                        break;
                }
            }
        }
    }
}

/**
 * void usb_connected_task(void* pvParameters)
 *
 * Task to monitor the RS485 bus when a PC is connected to the DSP via USB.
 * The PC acts as the master, we are now the slave. We listen to messages between the
 * PC and DSP / MCU. The PC also tells us when it is done via a disconnected packet
 */
void usb_connected_task(void* pvParameters) {
    Audison_AC_Link_Bus* ac_link_bus_ptr = (Audison_AC_Link_Bus*)pvParameters;
    while (1) {
        uint8_t received_message[50];
        uint8_t bytes_to_read = ac_link_bus_ptr->read_rx_message(received_message, sizeof(received_message));
        while (bytes_to_read > 5) {
            ac_link_bus_ptr->parse_rx_message(received_message, bytes_to_read);
            memset(received_message, 0x00, sizeof(bytes_to_read));
            bytes_to_read = ac_link_bus_ptr->read_rx_message(received_message, sizeof(received_message));
            vTaskDelay(pdMS_TO_TICKS(10)); // Let other stuff happen
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // We can idle for a while since this is not too important
    }
}

void rs485_bus_device_polling_task(void* pvParameters) {
    bool boot_up_completed = false;
    Audison_AC_Link_Bus* ac_link_bus_ptr = (Audison_AC_Link_Bus*)pvParameters;
    while (1) {
        if (!boot_up_completed) {
            while (!ac_link_bus_ptr->is_dsp_on_bus()) {
                ac_link_bus_ptr->check_dsp_processor_on_bus();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            ac_link_bus_ptr->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_MASTER_MCU);
            vTaskDelay(pdMS_TO_TICKS(500));
            ac_link_bus_ptr->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_DSP_PROCESSOR);
            update_web_server_parameter_string(DSP_SETTINGS_CURRENT_INPUT_SOURCE, dsp_settings_rs485->current_source);
            change_led_mode(LED_MODE_DEVICE_RUNNING);
            boot_up_completed = true;
        }
        if (!dsp_settings_rs485->usb_connected) {
            ac_link_bus_ptr->check_usb_on_bus();
            vTaskDelay(pdMS_TO_TICKS(500));
            ac_link_bus_ptr->check_dsp_processor_on_bus();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void Audison_AC_Link_Bus::set_volume(uint8_t volume, uint8_t receiver_address /*AC_LINK_ADDRESS_DSP_MASTER*/) {
    if (volume <= MAX_VOLUME_VALUE) {
        uint8_t volume_adjust_packet[2] = {AC_LINK_COMMAND_MASTER_VOLUME, volume};
        this->write_to_audison_bus(receiver_address, AC_LINK_ADDRESS_DRC, (uint8_t*)volume_adjust_packet,
                                   sizeof(volume_adjust_packet));
    }
}

void Audison_AC_Link_Bus::set_balance(uint8_t balance_level, uint8_t receiver_address /*AC_LINK_ADDRESS_DSP_MASTER*/) {
    if (balance_level <= MAX_BALANCE_VALUE) {
        uint8_t balance_adjust_packet[2] = {AC_LINK_COMMAND_BALANCE_ADJUST, balance_level};
        this->write_to_audison_bus(receiver_address, AC_LINK_ADDRESS_DRC, (uint8_t*)balance_adjust_packet,
                                   sizeof(balance_adjust_packet));
    }
}

void Audison_AC_Link_Bus::set_fader(uint8_t fade_level, uint8_t receiver_address /*AC_LINK_ADDRESS_DSP_MASTER*/) {
    if (fade_level <= MAX_FADER_VALUE) {
        uint8_t fader_adjust_packet[2] = {AC_LINK_COMMAND_FADER_ADJUST, fade_level};
        this->write_to_audison_bus(receiver_address, AC_LINK_ADDRESS_DRC, (uint8_t*)fader_adjust_packet,
                                   sizeof(fader_adjust_packet));
    }
}

void Audison_AC_Link_Bus::set_sub_volume(uint8_t sub_volume, uint8_t receiver_address /*AC_LINK_ADDRESS_DSP_MASTER*/) {
    if (sub_volume <= MAX_SUB_VOLUME_VALUE) {
        uint8_t sub_volume_adjust_packet[2] = {AC_LINK_COMMAND_SUB_VOLUME_ADJUST, sub_volume};
        this->write_to_audison_bus(receiver_address, AC_LINK_ADDRESS_DRC, (uint8_t*)sub_volume_adjust_packet,
                                   sizeof(sub_volume_adjust_packet));
    }
}

void Audison_AC_Link_Bus::check_usb_on_bus(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_CHECK_DEVICE_PRESENT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_COMPUTER, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
}

void Audison_AC_Link_Bus::check_master_mcu_on_bus(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_CHECK_DEVICE_PRESENT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_MASTER_MCU, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
}

void Audison_AC_Link_Bus::check_dsp_processor_on_bus(void) {
    this->dsp_ping_count++;
    uint8_t packet[] = {AC_LINK_COMMAND_CHECK_DEVICE_PRESENT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
    if (this->dsp_ping_count >= 5) {
        log_e("Tried to ping the DSP too many times. Shutting down now");
        shut_down_dsp();
    }
}

bool Audison_AC_Link_Bus::is_dsp_on_bus(void) {
    return this->dsp_on_bus;
}

void Audison_AC_Link_Bus::send_fw_version_to_usb(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_DRC_FW_VERSION, DRC_FIRMWARE_VERSION[0], DRC_FIRMWARE_VERSION[1]};
    this->write_to_audison_bus(AC_LINK_ADDRESS_COMPUTER, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), false);
}

void Audison_AC_Link_Bus::set_dsp_memory(uint8_t memory) {
    /* We index the DSP memory at 0 but Audison have it indexed at 1. Apply the offset here */
    uint8_t memory_corrected = memory + 1;
    uint8_t packet[] = {AC_LINK_COMMAND_CHANGE_DSP_MEMORY, memory_corrected};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), false);
}

void Audison_AC_Link_Bus::get_current_input_source(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_GET_CURRENT_SOURCE_NAME};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
}

void Audison_AC_Link_Bus::change_source(void) {
    /**Request the unit to change the source. Unit responds with the source that it has
     * changed to -> we need to wait for response
     * */
    uint8_t packet[] = {AC_LINK_COMMAND_CHANGE_SOURCE, 0x00};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
    vTaskDelay(pdMS_TO_TICKS(500));
    this->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_DSP_PROCESSOR);
    vTaskDelay(pdMS_TO_TICKS(100));
    this->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_MASTER_MCU);
}

void Audison_AC_Link_Bus::update_device_with_latest_settngs(struct DSP_Settings* settings,
                                                            uint8_t receiver_address /*AC_LINK_ADDRESS_DSP_MASTER*/) {
    this->set_volume(settings->master_volume, receiver_address);
    this->set_sub_volume(settings->sub_volume, receiver_address);
    this->set_balance(settings->balance, receiver_address);
    this->set_fader(settings->fader, receiver_address);
}

void Audison_AC_Link_Bus::turn_off_main_unit(void) {
    uint8_t data_packet[] = {AC_LINK_COMMAND_TURN_OFF_MAIN_UNIT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_MASTER_MCU, AC_LINK_ADDRESS_DRC, data_packet, sizeof(data_packet));
}

uint8_t Audison_AC_Link_Bus::calculate_checksum(uint8_t* data_buffer, uint8_t data_length_bytes) {
    uint32_t byte_sum = 0;
    for (uint8_t i = 0; i < data_length_bytes; i++) {
        byte_sum += data_buffer[i];
    }
    uint8_t checksum = byte_sum % 256;
    return checksum;
}

void Audison_AC_Link_Bus::enable_transmission(void) {
    digitalWrite(this->tx_en_pin, HIGH);
}

void Audison_AC_Link_Bus::disable_transmission(void) {
    digitalWrite(this->tx_en_pin, LOW);
}

bool Audison_AC_Link_Bus::lock_ring_buffer(void) {
    return xSemaphoreTake(this->ring_buffer_semaphore, (TickType_t)10);
}

void Audison_AC_Link_Bus::release_ring_buffer(void) {
    xSemaphoreGive(this->ring_buffer_semaphore);
}

void Audison_AC_Link_Bus::write_to_audison_bus(uint8_t receiver_address, uint8_t transmitter_address, uint8_t* data,
                                               uint8_t data_length, bool wait_for_response /*default=false*/) {
    if (!dsp_settings_rs485->usb_connected) {
        uint8_t message_length = HEADER_SIZE_BYTES + data_length + CHECKSUM_SIZE_BYTES;
        uint8_t message_buffer[message_length];
        message_buffer[0] = receiver_address;
        message_buffer[1] = transmitter_address;
        message_buffer[2] = 0x00;
        message_buffer[3] = message_length;
        for (uint8_t i = 0; i < data_length; i++) {
            message_buffer[i + 4] = data[i];
        }
        /* Now we append the checksum - CheckSum8 Modulo 256 */
        uint8_t checksum = this->calculate_checksum(message_buffer,
                                                    sizeof(message_buffer) - 1); // Last byte is the checksum
        message_buffer[message_length - 1] = checksum;

        // Add packet to the ring buffer
        uint16_t ring_buffer_packet[message_length + 1];
        for (uint8_t i = 0; i < message_length; i++) {
            ring_buffer_packet[i] = (uint16_t)message_buffer[i];
        }
        if (wait_for_response) {
            ring_buffer_packet[message_length] = (uint16_t)PACKET_WAIT_FOR_RESPONSE;
        } else {
            ring_buffer_packet[message_length] = (uint16_t)PACKET_NO_RESPONSE;
        }

        while (!this->lock_ring_buffer()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        UBaseType_t res = pdFALSE;
        while (res == pdFALSE) {
            res = xRingbufferSend(this->tx_ring_buffer_handle, ring_buffer_packet, sizeof(ring_buffer_packet),
                                  (TickType_t)100);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        this->release_ring_buffer();

    } else {
        log_e("Can't use the RS485 bus when USB is connected to the DSP!");
    }
}

void rs485_bus_task(void* pvParameters) {
    Audison_AC_Link_Bus* ac_link_bus_ptr = (Audison_AC_Link_Bus*)pvParameters;
    while (1) {
        // Receive the items from the ring buffer
        size_t item_size;
        while (!ac_link_bus_ptr->lock_ring_buffer()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        uint16_t* item =
            (uint16_t*)xRingbufferReceive(ac_link_bus_ptr->tx_ring_buffer_handle, &item_size, (TickType_t)10);

        if (item != NULL) {
            item_size /= sizeof(uint16_t); /* xRingbufferReceive updates item_size with number of bytes obtained. Each
                                              item is 16-bit */
            uint16_t ring_buffer_packet[item_size];
            memcpy(ring_buffer_packet, item, sizeof(ring_buffer_packet)); // memcpy uses number of BYTES as param!
            vRingbufferReturnItem(ac_link_bus_ptr->tx_ring_buffer_handle, (void*)item); // Return Item
            ac_link_bus_ptr->release_ring_buffer();
            uint8_t packet_index = 0;
            // Parse each packet
            while (packet_index < item_size) { // There may be multiple messages retrieved from the buffer
                bool wait_for_response = false;
                uint8_t temp_buf[item_size];
                uint8_t temp_buf_length = 0;
                for (size_t i = 0; i < item_size; i++) {
                    if (ring_buffer_packet[packet_index] == (uint16_t)PACKET_WAIT_FOR_RESPONSE) {
                        wait_for_response = true;
                        packet_index++;
                        break;
                    } else if (ring_buffer_packet[packet_index] == (uint16_t)PACKET_NO_RESPONSE) {
                        packet_index++;
                        break;
                    } else {
                        temp_buf[i] = (uint8_t)ring_buffer_packet[packet_index];
                        temp_buf_length++;
                        packet_index++;
                    }
                }

                if (temp_buf_length > 5) {
                    rmt_item32_t packet_rmt_items[RMT_ITEMS_REQUIRED_FOR_9_BIT_DATA * temp_buf_length];
                    ac_link_bus_ptr->convert_packet_to_rmt_items(temp_buf, temp_buf_length, packet_rmt_items);

                    // Now we write it to the bus
                    ac_link_bus_ptr->enable_transmission(); // TX output enable
                    ESP_ERROR_CHECK(rmt_write_items(RMT_CHANNEL_0, packet_rmt_items,
                                                    sizeof(packet_rmt_items) / sizeof(packet_rmt_items[0]), true));
                    ac_link_bus_ptr->disable_transmission(); // TX output disable

                    // We can read what we just sent first
                    uint8_t transmitted_message[temp_buf_length];
                    uint8_t bytes_to_read =
                        ac_link_bus_ptr->read_rx_message(transmitted_message, sizeof(transmitted_message));
                    if (bytes_to_read != temp_buf_length) {
                        log_e("RS485 ERROR: TX did not send the correct amount of bytes, sent %d bytes but expected to "
                              "send %d bytes",
                              bytes_to_read, temp_buf_length);
                        for (uint8_t i = 0; i < bytes_to_read; i++) {
                            Serial.print(transmitted_message[i], HEX);
                            Serial.print(" ");
                        }
                        Serial.println();
                    } else if (memcmp(temp_buf, transmitted_message, temp_buf_length) != 0) {
                        log_e("RS485 ERROR: Bytes sent not matching");
                    } else {
                        for (uint8_t i = 0; i < sizeof(transmitted_message); i++) {
                            Serial.print(transmitted_message[i], HEX);
                            Serial.print(" ");
                        }
                        Serial.println();
                    }
                    if (wait_for_response) {
                        bytes_to_read = 0xFF;
                        while (bytes_to_read) {
                            vTaskDelay(pdMS_TO_TICKS(250));
                            uint8_t received_message[50];
                            bytes_to_read =
                                ac_link_bus_ptr->read_rx_message(received_message, sizeof(received_message));
                            if (bytes_to_read > 5) {
                                ac_link_bus_ptr->parse_rx_message(received_message, bytes_to_read);
                            }
                        }
                    }
                }
            }
        } else {
            ac_link_bus_ptr->release_ring_buffer();
            uint8_t bytes_to_read = 0xFF;
            while (bytes_to_read > 0) {
                vTaskDelay(pdMS_TO_TICKS(200));
                uint8_t received_message[50];
                bytes_to_read = ac_link_bus_ptr->read_rx_message(received_message, sizeof(received_message));
                if (bytes_to_read > 5) {
                    ac_link_bus_ptr->parse_rx_message(received_message, bytes_to_read);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

uint8_t Audison_AC_Link_Bus::read_rx_message(uint8_t* data_buffer, uint8_t buffer_length) {
    uint8_t bytes_to_read = rs485_serial_port.available();
    if (bytes_to_read) {
        uint8_t message_index = 0;
        bool message_completed = false;
        bool transmitter_address_found = false;
        bool message_length_found = false;
        uint8_t total_message_length = 0;

        while (rs485_serial_port.available() && !message_completed) {
            uint8_t data = rs485_serial_port.read();
            if (!transmitter_address_found && rs485_serial_port.readParity() == true) {
                data_buffer[message_index] = data; // We have the transmitter address
                transmitter_address_found = true;
            } else if (transmitter_address_found) // We are receiving a message
            {
                message_index++;
                data_buffer[message_index] = data;
                if (!message_length_found && message_index == 3) // The total length of message
                {
                    message_length_found = true;
                    total_message_length = data;
                } else if (message_length_found && message_index == (total_message_length - 1)) // Message
                                                                                                // complete
                {
                    message_completed = true;
                    return total_message_length;
                }
            }
        }
        return total_message_length;
    } else {
        return 0;
    }
}

void Audison_AC_Link_Bus::init_ac_link_bus(struct DSP_Settings* settings) {
    dsp_settings_rs485 = settings;
    this->tx_pin = RS485_TX_PIN;
    this->rx_pin = RS485_RX_PIN;
    this->tx_en_pin = RS485_TX_EN_PIN;

    pinMode(this->tx_en_pin, OUTPUT);
    this->disable_transmission();

    this->ring_buffer_semaphore = xSemaphoreCreateMutex();
    xSemaphoreGive(this->ring_buffer_semaphore);

    // Create the TX ring buffer
    this->tx_ring_buffer_handle = xRingbufferCreate(RING_BUFFER_LENGTH_BYTES, RINGBUF_TYPE_BYTEBUF);
    if (this->tx_ring_buffer_handle == NULL) {
        log_e("Failed to create ring buffer!");
    }

    this->init_rmt(); // Enable RMT for TX

    rs485_serial_port.begin(RS485_BAUDRATE, SWSERIAL_8S1, this->rx_pin, -1); // Use software serial only for RX
    xTaskCreatePinnedToCore(rs485_bus_task, "RS485BusTask", 8 * 1024, this, tskIDLE_PRIORITY + 1,
                            &rs485_bus_task_handle, 1);
    xTaskCreatePinnedToCore(rs485_bus_device_polling_task, "RS485_tx", 8 * 1024, this, TX_TASK_PRIORITY,
                            &rs485_bus_device_polling_task_handle, 1);
    xTaskCreatePinnedToCore(usb_connected_task, "USBConnRXTask", 8 * 1024, this, USB_CONNECTED_TASK_PRIORITY,
                            &usb_connected_task_handle, 1);
    vTaskSuspend(usb_connected_task_handle);
}

void Audison_AC_Link_Bus::convert_byte_to_rmt_item_9bit(uint8_t byte_to_convert, bool is_address,
                                                        rmt_item32_t* item_buffer) {
    /** NOTE: UART protocol requires LSB format. We start by adding the extra idle
     * bit and STOP bit to the data packet. We add the address flag (9th bit), the 8 bits
     * of data and then the STOP bit (LOW for UART). The STOP bit is achieved by left shifting
     * everything - you are left with 0 as the STOP bit
     */
    uint16_t data = (0b11 << 10) | (is_address << 9) | (byte_to_convert << 1);

    uint8_t item_index = 0;
    for (uint8_t i = 0; i < 11; i += 2) {
        item_buffer[item_index].duration0 = 50;
        item_buffer[item_index].level0 = (data >> i) & 1;
        item_buffer[item_index].duration1 = 50;
        item_buffer[item_index].level1 = (data >> (i + 1)) & 1;
        item_index++;
    }
}

void Audison_AC_Link_Bus::convert_packet_to_rmt_items(uint8_t* packet, uint8_t packet_length,
                                                      rmt_item32_t* item_buffer_ptr) {
    uint8_t packet_byte = 0;
    convert_byte_to_rmt_item_9bit(packet[packet_byte], true, item_buffer_ptr); // Convert the address first
    item_buffer_ptr += RMT_ITEMS_REQUIRED_FOR_9_BIT_DATA; // Offset item pointer by size of packet and index
    for (packet_byte = 1; packet_byte < packet_length; packet_byte++) {
        convert_byte_to_rmt_item_9bit(packet[packet_byte], false, item_buffer_ptr);
        item_buffer_ptr += RMT_ITEMS_REQUIRED_FOR_9_BIT_DATA; // Offset item pointer by size of packet and index
    }
}

int Audison_AC_Link_Bus::init_rmt(void) {
    const int RMT_DIV = APB_CLK_FREQ / 50 / 38400;
    const int RMT_TICK = APB_CLK_FREQ / RMT_DIV;
    uint16_t bit_len = RMT_TICK / 38400;
    ESP_RETURN_ON_FALSE(((10UL * RMT_TICK / 38400) < 0xFFFF), ESP_FAIL, TAG,
                        "rmt tick too long, reconfigure 'RMT_DIV'");
    ESP_RETURN_ON_FALSE(((RMT_TICK / 38400) > 49), ESP_FAIL, TAG, "rmt tick too long, reconfigure 'RMT_DIV'");
    ESP_RETURN_ON_FALSE(((RMT_TICK / 38400 / 2) < 0xFF), ESP_FAIL, TAG, "baud rate too slow, reconfigure 'RMT_DIV'");
    log_i("baud=%d rmt_div=%d rmt_tick=%d, bit_len=%i", 38400, RMT_DIV, RMT_TICK, bit_len);
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(RS485_TX_PIN, RMT_CHANNEL_0);
    config.tx_config.carrier_en = false;               // Disable the carrier frequency
    config.tx_config.idle_output_en = true;            // Enable idle control
    config.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH; // Enable high idle (UART spec)
    config.clk_div = RMT_DIV;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    return ESP_OK;
}