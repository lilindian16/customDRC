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

#define RMT_ITEMS_REQUIRED_FOR_9_BIT_DATA 6

// Macros for the AC Link Bus data packets
#define HEADER_SIZE_BYTES   4
#define CHECKSUM_SIZE_BYTES 1
TaskHandle_t rs485_tx_task_handle = NULL, usb_connected_task_handle = NULL;
SemaphoreHandle_t rs485_tx_semaphore_handle;
EspSoftwareSerial::UART rs485_serial_port;
bool master_mcu_is_on_bus = false; // Flag set to false, set to true when MCU acks on bus
bool dsp_processor_is_on_bus = false;

#define TX_TASK_PRIORITY tskIDLE_PRIORITY + 1

// DSP settings struct ptr
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
                        this->send_fw_version_to_usb();
                        update_web_server_parameter(DSP_SETTING_INDEX_USB_CONNECTED, 1);
                        dsp_settings_rs485->usb_connected = true;
                        disable_encoders();
                        vTaskResume(usb_connected_task_handle); // Enable the USB connected task
                        vTaskSuspend(rs485_tx_task_handle);     // Suspend the TX task last to avoid hanging ourselves
                                                                // without completing
                        break;
                    case AC_LINK_COMMAND_DEVICE_IS_DISCONNECTED:
                        log_i("USB disconnected. RS485 bus active");
                        dsp_settings_rs485->usb_connected = false;
                        enable_encoders();
                        this->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_COMPUTER);
                        update_web_server_parameter(DSP_SETTING_INDEX_USB_CONNECTED, 0);
                        vTaskResume(rs485_tx_task_handle);       // Enable the TX task that pings devices on bus
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
                        dsp_processor_is_on_bus = true; // Flag to allow us to continue the boot process
                        this->dsp_on_bus = true;
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
        if (bytes_to_read > 5) {
            ac_link_bus_ptr->parse_rx_message(received_message, bytes_to_read);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void rs485_tx_task(void* pvParameters) {
    bool boot_up_completed = false;
    Audison_AC_Link_Bus* ac_link_bus_ptr = (Audison_AC_Link_Bus*)pvParameters;
    while (1) {
        if (!boot_up_completed) {
            while (!dsp_processor_is_on_bus) {
                ac_link_bus_ptr->check_dsp_processor_on_bus();
                vTaskDelay(pdMS_TO_TICKS(500));
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
    this->dsp_on_bus = false; // Reset the flag
    uint8_t packet[] = {AC_LINK_COMMAND_CHECK_DEVICE_PRESENT};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
    if (this->dsp_on_bus == false) {
        device_comms_fail_count++;
        if (device_comms_fail_count >= 5) {
            log_i("Failed to communicate with DSP. Shutting down");
            shut_down_dsp();
        }
    } else {
        this->device_comms_fail_count = 0; // Reset the count
    }
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
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
}

void Audison_AC_Link_Bus::change_source(void) {
    // Request the unit to change the source
    // Unit responds with the source that it has changed to -> we need to wait for response
    uint8_t packet[] = {AC_LINK_COMMAND_CHANGE_SOURCE, 0x00};
    this->write_to_audison_bus(AC_LINK_ADDRESS_DSP_PROCESSOR, AC_LINK_ADDRESS_DRC, packet, sizeof(packet), true);
    vTaskDelay(pdMS_TO_TICKS(100));
    this->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_DSP_PROCESSOR);
    vTaskDelay(pdMS_TO_TICKS(100));
    this->update_device_with_latest_settngs(dsp_settings_rs485, AC_LINK_ADDRESS_MASTER_MCU);
}

void Audison_AC_Link_Bus::update_device_with_latest_settngs(struct DSP_Settings* settings,
                                                            uint8_t receiver_address /*AC_LINK_ADDRESS_DSP_MASTER*/) {
    this->set_volume(settings->master_volume, receiver_address);
    vTaskDelay(pdMS_TO_TICKS(100));
    this->set_sub_volume(settings->sub_volume, receiver_address);
    vTaskDelay(pdMS_TO_TICKS(100));
    this->set_balance(settings->balance, receiver_address);
    vTaskDelay(pdMS_TO_TICKS(100));
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

        rmt_item32_t packet_rmt_items[RMT_ITEMS_REQUIRED_FOR_9_BIT_DATA * message_length];
        this->convert_packet_to_rmt_items(message_buffer, message_length, packet_rmt_items);

        while (xSemaphoreTake(rs485_tx_semaphore_handle, TickType_t(10)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        digitalWrite(this->tx_en_pin, HIGH); // TX output enable
        ESP_ERROR_CHECK(rmt_write_items(RMT_CHANNEL_0, packet_rmt_items,
                                        sizeof(packet_rmt_items) / sizeof(packet_rmt_items[0]), true));
        digitalWrite(this->tx_en_pin, LOW); // TX output disable

        vTaskDelay(pdMS_TO_TICKS(50)); // Let the RX interrupt do its thing
        // We can read what we just sent first
        uint8_t transmitted_message[message_length];
        uint8_t bytes_to_read = this->read_rx_message(transmitted_message, sizeof(transmitted_message));
        if (bytes_to_read != sizeof(transmitted_message)) {
            log_e("RS485 ERROR: TX did not send the correct amount of bytes");
            for (uint8_t i = 0; i < bytes_to_read; i++) {
                Serial.print(transmitted_message[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        } else if (memcmp(message_buffer, transmitted_message, sizeof(message_buffer)) != 0) {
            log_e("RS485 ERROR: Bytes sent not matching");
        } else {
            for (uint8_t i = 0; i < sizeof(transmitted_message); i++) {
                Serial.print(transmitted_message[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }

        if (wait_for_response) {
            vTaskDelay(pdMS_TO_TICKS(250));
            uint8_t received_message[250];
            uint8_t bytes_to_read = this->read_rx_message(received_message, sizeof(received_message));
            if (bytes_to_read > 5) {
                this->parse_rx_message(received_message, bytes_to_read);
            }
        }
        xSemaphoreGive(rs485_tx_semaphore_handle);
    }

    else {
        log_e("Can't use the RS485 bus when USB is connected to the DSP!");
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
                } else if (message_length_found && message_index == total_message_length - 1) // Message
                                                                                              // complete
                {
                    if (buffer_length >= total_message_length) {
                        message_completed = true;
                        return total_message_length;
                    }
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

    rs485_tx_semaphore_handle = xSemaphoreCreateMutex();
    if (rs485_tx_semaphore_handle == NULL) {
        log_e("Failed to create the RS485 tx mutex");
        while (1) {
            ;
        }
    }

    this->init_rmt(); // Enable RMT for TX

    rs485_serial_port.begin(RS485_BAUDRATE, SWSERIAL_8S1, this->rx_pin, -1); // Use software serial for RX
    rs485_serial_port.flush();
    xTaskCreatePinnedToCore(rs485_tx_task, "RS485_tx", 8 * 1024, this, TX_TASK_PRIORITY, &rs485_tx_task_handle, 1);
    xTaskCreatePinnedToCore(usb_connected_task, "USBConnRXTask", 8 * 1024, this, tskIDLE_PRIORITY + 1,
                            &usb_connected_task_handle, 1);
    vTaskSuspend(usb_connected_task_handle);
}

void Audison_AC_Link_Bus::convert_byte_to_rmt_item_9bit(uint8_t byte_to_convert, bool is_address,
                                                        rmt_item32_t* item_buffer) {
    // NOTE: UART protocol requires LSB format
    uint8_t item_index = 0;
    uint16_t data = (byte_to_convert << 1); // Add the start bit
    if (is_address) {                       // Add the address flag
        data |= (1 << 9);
    }
    data |= (0b11 << 10); // Add the stop bit and extra idle bit

    for (uint8_t i = 0; i < 11; i += 2) {
        item_buffer[item_index].duration0 = 50;
        item_buffer[item_index].level0 = (data >> i) & 0b1;
        item_buffer[item_index].duration1 = 50;
        item_buffer[item_index].level1 = (data >> (i + 1)) & 0b1;
        item_index++;
    }
}

void Audison_AC_Link_Bus::convert_packet_to_rmt_items(uint8_t* packet, uint8_t packet_length,
                                                      rmt_item32_t* item_buffer) {
    for (uint8_t packet_byte = 0; packet_byte < packet_length; packet_byte++) {
        if (packet_byte == 0) {
            convert_byte_to_rmt_item_9bit(packet[packet_byte], true, item_buffer);
        } else {
            convert_byte_to_rmt_item_9bit(packet[packet_byte], false, item_buffer);
        }
        item_buffer += RMT_ITEMS_REQUIRED_FOR_9_BIT_DATA; // Offset item pointer by size of packet and index
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