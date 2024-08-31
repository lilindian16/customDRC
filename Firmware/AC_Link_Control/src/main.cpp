#include <Arduino.h>
#include <AudisonACLink.hpp>

#define RS485_TX_PIN 15
#define RS485_RX_PIN 5
#define RS485_TX_EN_PIN 2

// #define TRIAL_TRANSMIT
#define TRIAL_RECEIVE

AudisonACLink ac_link;

uint8_t rx_buffer[255];

void setup(void)
{
  Serial.begin(115200);
  RS485_Config_t rs485_config;
  rs485_config.rs485_tx_pin = RS485_TX_PIN;
  rs485_config.rs485_rx_pin = RS485_RX_PIN;
  rs485_config.rs485_tx_en_pin = RS485_TX_EN_PIN;
  ac_link.init_ac_link_bus(&rs485_config);

  memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the rx buffer
}

void loop(void)
{
#ifdef TRIAL_TRANSMIT
  /* Trial transmit function. We can send volume adjust packets for now */
  for (uint8_t i = 0; i <= 0x78; i++)
  {
    ac_link.set_volume(i);
    Serial.print("Volume: ");
    Serial.println(i, HEX);
    delay(250);
  }
#endif
#ifdef TRIAL_RECEIVE
  uint8_t bytes_read = ac_link.read_rx_message(rx_buffer, sizeof(rx_buffer));
  if (bytes_read > 0)
  {
    if (rx_buffer[1] == 0x80) // Filter DRC messages
    {
      for (uint8_t i = 0; i < bytes_read; i++)
      {
        Serial.print(rx_buffer[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
    memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the buffer
  }
  delay(50);
#endif
}
