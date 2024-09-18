#include <Arduino.h>
#include <Custom_DRC.hpp>
#include "esp_ota_ops.h"

#define RS485_TX_PIN 18
#define RS485_RX_PIN 21
#define RS485_TX_EN_PIN 19
#define LED_PIN 25
#define DSP_PWR_EN_PIN 5
#define FW_VERSION "0.0.1"

// #define TRIAL_TRANSMIT
#define TRIAL_RECEIVE

TaskHandle_t blinky_task_handle;
uint8_t rx_buffer[255];

Audison_AC_Link_Bus Audison_AC_Link;

void blinky(void *pvParameters)
{
  while (1)
  {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1000));
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup(void)
{
  Serial.begin(115200);

  pinMode(DSP_PWR_EN_PIN, OUTPUT);
  digitalWrite(DSP_PWR_EN_PIN, LOW);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Get the OTA partitions that are running and the next one that it will point to
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *otaPartition = esp_ota_get_next_update_partition(NULL);
  Serial.printf("Running partition type %d subtype %d (offset 0x%08x)\n", running->type, running->subtype, running->address);
  Serial.printf("OTA partition will be type %d subtype %d (offset 0x%x)\n", otaPartition->type, otaPartition->subtype, otaPartition->address);

  Serial.printf("****** CDRC Firmware ******\n");
  Serial.printf("****** FW Version: %s *****\n", FW_VERSION);
  Serial.printf("******  J SEQUEIRA   ******\n");

  RS485_Config_t rs485_config;
  rs485_config.rs485_tx_pin = RS485_TX_PIN;
  rs485_config.rs485_rx_pin = RS485_RX_PIN;
  rs485_config.rs485_tx_en_pin = RS485_TX_EN_PIN;
  Audison_AC_Link.init_ac_link_bus(&rs485_config);
  memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the rx buffer
  xTaskCreatePinnedToCore(blinky, "blinky", 8000, NULL, tskIDLE_PRIORITY + 1, &blinky_task_handle, 1);
  init_drc_encoders();
  web_server_init();
}

void loop(void)
{
  uint8_t bytes_read = Audison_AC_Link.read_rx_message(rx_buffer, sizeof(rx_buffer));
  if (bytes_read > 0)
  {
    // if (rx_buffer[1] == AUDISON_DRC_RS485_ADDRESS) // Filter DRC messages
    // {
    for (uint8_t i = 0; i < bytes_read; i++)
    {
      Serial.print(rx_buffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    // }
    memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the buffer
  }
  vTaskDelay(pdMS_TO_TICKS(50));
}
