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

#define HEADER_SIZE_BYTES   4
#define CHECKSUM_SIZE_BYTES 1

EspSoftwareSerial::UART rs485_serial_port;

TaskHandle_t rs485_rx_task_handle, rs485_tx_task_handle;
SemaphoreHandle_t rs485_tx_semaphore_handle;

struct DSP_Settings* dsp_settings_rs485;

bool master_mcu_is_on_bus = false; // Flag set to false, set to true when MCU acks on bus
bool dsp_processor_is_on_bus = false;

void rs485_rx_task(void* pvParameters) {
    Audison_AC_Link_Bus* ac_link_bus_ptr = (Audison_AC_Link_Bus*)pvParameters;
    uint8_t rx_buffer[255];
    while (1) {
        memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the rx buffer
        uint8_t bytes_read = ac_link_bus_ptr->read_rx_message(rx_buffer, sizeof(rx_buffer));
        if (bytes_read >= 5) // Minimum valid packet count
        {
            uint8_t receiver = rx_buffer[0];
            uint8_t transmitter = rx_buffer[1];
            uint8_t message_length = rx_buffer[3];
            uint8_t command = rx_buffer[4];

            if (receiver == AC_LINK_ADDRESS_DRC) // Filter only DRC addressed messages
            {
                if (transmitter == AC_LINK_ADDRESS_COMPUTER) {
                    switch (command) {
                        case AC_LINK_COMMAND_DEVICE_IS_PRESENT:
                            if (message_length == 7 && rx_buffer[5] == 0x0A) {
                                ac_link_bus_ptr->send_fw_version_to_usb();
                                update_web_server_parameter(DSP_SETTING_INDEX_USB_CONNECTED, 1);
                                dsp_settings_rs485->usb_connected = true;
                                disable_encoders();
                                log_i("USB connected. RS485 bus inactive");
                            }
                            break;
                        case AC_LINK_COMMAND_DEVICE_IS_DISCONNECTED:
                            dsp_settings_rs485->usb_connected = false;
                            enable_encoders();
                            ac_link_bus_ptr->update_device_with_latest_settngs(dsp_settings_rs485,
                                                                               AC_LINK_ADDRESS_COMPUTER);
                            update_web_server_parameter(DSP_SETTING_INDEX_USB_CONNECTED, 0);
                            log_i("USB disconnected. RS485 bus active");
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
                            if (!dsp_processor_is_on_bus) {
                                log_i("DSP Processor has joined the bus");
                                dsp_processor_is_on_bus = true;
                                break;
                            }
                            break;
                        case AC_LINK_COMMAND_INPUT_SOURCE_NAME:
                            if (message_length == 0x16) {
                                // Copy the source name to the internal buffer
                                memcpy(dsp_settings_rs485->current_source, &rx_buffer[5], 16);
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
                            dsp_settings_rs485->memory_select = rx_buffer[5] - 1; // Offset for DSP index 1
                            update_web_server_parameter(DSP_SETTING_INDEX_MEMORY_SELECT, rx_buffer[5] - 1);
                            break;
                        case AC_LINK_COMMAND_MASTER_VOLUME:
                            dsp_settings_rs485->master_volume = rx_buffer[5];
                            update_web_server_parameter(DSP_SETTING_INDEX_MASTER_VOLUME, rx_buffer[5]);
                            break;
                        case AC_LINK_COMMAND_SUB_VOLUME_ADJUST:
                            dsp_settings_rs485->sub_volume = rx_buffer[5];
                            update_web_server_parameter(DSP_SETTING_INDEX_SUB_VOLUME, rx_buffer[5]);
                            break;
                        default:
                            log_e("RS485->Master->USB: Unknown command received");
                            break;
                    }
                }
            }

            // Print all messages on serial
            for (uint8_t i = 0; i < bytes_read; i++) {
                Serial.print(rx_buffer[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void rs485_tx_task(void* pvParameters) {
    bool boot_up_completed = false;
    Audison_AC_Link_Bus* ac_link_bus_ptr = (Audison_AC_Link_Bus*)pvParameters;
    while (1) {
        if (!boot_up_completed) {
            while (!master_mcu_is_on_bus && !dsp_processor_is_on_bus) {
                ac_link_bus_ptr->check_usb_on_bus();
                vTaskDelay(pdMS_TO_TICKS(1000));
                ac_link_bus_ptr->check_master_mcu_on_bus();
                vTaskDelay(pdMS_TO_TICKS(1000));
                ac_link_bus_ptr->check_dsp_processor_on_bus();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            ac_link_bus_ptr->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_MASTER_MCU);
            vTaskDelay(pdMS_TO_TICKS(1000));
            ac_link_bus_ptr->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_COMPUTER);
            update_web_server_parameter_string(DSP_SETTINGS_CURRENT_INPUT_SOURCE, dsp_settings_rs485->current_source);
            boot_up_completed = true;
        }
        if (!dsp_settings_rs485->usb_connected) {
            ac_link_bus_ptr->check_usb_on_bus();
            vTaskDelay(pdMS_TO_TICKS(1000));
            ac_link_bus_ptr->check_master_mcu_on_bus();
            vTaskDelay(pdMS_TO_TICKS(1000));
            ac_link_bus_ptr->check_dsp_processor_on_bus();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void Audison_AC_Link_Bus::init_ac_link_bus(struct DSP_Settings* settings) {
    dsp_settings_rs485 = settings;
    this->tx_pin = RS485_TX_PIN;
    this->rx_pin = RS485_RX_PIN;
    this->tx_en_pin = RS485_TX_EN_PIN;

    pinMode(this->tx_en_pin, OUTPUT);

    rs485_tx_semaphore_handle = xSemaphoreCreateMutex();
    if (rs485_tx_semaphore_handle == NULL) {
        log_e("Failed to create the RS485 tx mutex");
        while (1) {
            ;
        }
    }

    rs485_serial_port.begin(RS485_BAUDRATE, SWSERIAL_8S1, this->rx_pin, this->tx_pin);
    // xTaskCreatePinnedToCore(rs485_rx_task, "RS485_rx", 16000, this, tskIDLE_PRIORITY + 1, &rs485_rx_task_handle, 1);
    xTaskCreatePinnedToCore(rs485_tx_task, "RS485_tx", 8000, this, tskIDLE_PRIORITY + 1, &rs485_tx_task_handle, 1);
    digitalWrite(this->tx_en_pin, LOW); // RX EN pin is always low, we control flow with the TX pin going high
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
    this->write_to_audison_bus(AC_LINK_ADDRESS_COMPUTER, AC_LINK_ADDRESS_DRC, packet, sizeof(packet));
}

void Audison_AC_Link_Bus::check_master_mcu_on_bus(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_CHECK_DEVICE_PRESENT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_MASTER_MCU, AC_LINK_ADDRESS_DRC, packet, sizeof(packet));
}

void Audison_AC_Link_Bus::check_dsp_processor_on_bus(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_CHECK_DEVICE_PRESENT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet));
}

void Audison_AC_Link_Bus::send_fw_version_to_usb(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_DRC_FW_VERSION, DRC_FIRMWARE_VERSION[0], DRC_FIRMWARE_VERSION[1]};
    this->write_to_audison_bus(AC_LINK_ADDRESS_COMPUTER, AC_LINK_ADDRESS_DRC, packet, sizeof(packet));
}

void Audison_AC_Link_Bus::set_dsp_memory(uint8_t memory) {
    // We index the DSP memory at 0 but Audison have it indexed at 1. Apply the
    // offset here
    uint8_t memory_corrected = memory + 1;
    uint8_t packet[] = {AC_LINK_COMMAND_CHANGE_DSP_MEMORY, memory_corrected};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet));
}

void Audison_AC_Link_Bus::get_current_input_source(void) {
    uint8_t packet[] = {AC_LINK_COMMAND_GET_CURRENT_SOURCE_NAME};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet));
}

void Audison_AC_Link_Bus::change_source(void) {
    // Request the unit to change the source
    uint8_t packet[] = {AC_LINK_COMMAND_CHANGE_SOURCE, 0x00};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet));
}

void Audison_AC_Link_Bus::update_device_with_latest_settngs(struct DSP_Settings* settings,
                                                            uint8_t receiver_address /*AC_LINK_ADDRESS_DSP_MASTER*/) {
    this->set_volume(settings->master_volume, receiver_address);
    this->set_sub_volume(settings->sub_volume, receiver_address);
    this->set_balance(settings->balance, receiver_address);
    this->set_fader(settings->fader, receiver_address);
}

void Audison_AC_Link_Bus::write_to_audison_bus(uint8_t receiver_address, uint8_t transmitter_address, uint8_t* data,
                                               uint8_t data_length) {
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

        if (xSemaphoreTake(rs485_tx_semaphore_handle, (TickType_t)10) == pdTRUE) {
            digitalWrite(this->tx_en_pin, HIGH); // TX output enable

            for (uint8_t i = 0; i < message_length; i++) {
                if (i == 0) {
                    rs485_serial_port.write(message_buffer[i],
                                            EspSoftwareSerial::PARITY_MARK); // Append 1 to data to
                                                                             // show address
                } else {
                    rs485_serial_port.write(message_buffer[i], EspSoftwareSerial::PARITY_SPACE);
                }
            }

            digitalWrite(this->tx_en_pin, LOW); // TX output disable
            xSemaphoreGive(rs485_tx_semaphore_handle);
        } else {
            log_e("Failed to obtain RS485 TX mutex");
        }
    } else {
        log_e("Can't use the RS485 bus when USB is connected to the DSP!");
    }
}

void Audison_AC_Link_Bus::turn_off_main_unit(void) {
    uint8_t data_packet[] = {AC_LINK_COMMAND_TURN_OFF_MAIN_UNIT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_MASTER_MCU, AC_LINK_ADDRESS_DRC, data_packet, sizeof(data_packet));
    vTaskSuspend(rs485_tx_task_handle);
}

uint8_t Audison_AC_Link_Bus::calculate_checksum(uint8_t* data_buffer, uint8_t data_length_bytes) {
    uint32_t byte_sum = 0;
    for (uint8_t i = 0; i < data_length_bytes; i++) {
        byte_sum += data_buffer[i];
    }
    uint8_t checksum = byte_sum % 256;
    return checksum;
}

uint8_t Audison_AC_Link_Bus::read_rx_message(uint8_t* data_buffer, uint8_t buffer_length) {
    uint8_t bytes_to_read = rs485_serial_port.available();
    if (bytes_to_read) {
        uint8_t message_index = 0;
        bool message_completed = false;
        bool transmitter_address_found = false;
        bool message_length_found = false;
        uint8_t total_message_length = 0;
        uint8_t rx_data_buffer[255];

        while (!message_completed) {
            if (rs485_serial_port.available()) {
                uint8_t data = rs485_serial_port.read();
                if (!transmitter_address_found && rs485_serial_port.readParity() == true) {
                    rx_data_buffer[message_index] = data; // We have the transmitter address
                    transmitter_address_found = true;
                } else if (transmitter_address_found) // We are receiving a message
                {
                    message_index++;
                    rx_data_buffer[message_index] = data;
                    if (!message_length_found && message_index == 3) // The total length of message
                    {
                        message_length_found = true;
                        total_message_length = data;
                    } else if (message_length_found && message_index == total_message_length - 1) // Message complete
                    {
                        if (buffer_length >= total_message_length) {
                            memcpy(data_buffer, rx_data_buffer, total_message_length);
                            message_completed = true;
                        }
                    }
                }
            }

            else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        return total_message_length;
    }

    else {
        return 0;
    }
}