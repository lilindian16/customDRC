#include <Arduino.h>
#include <AudisonACLink.hpp>

#define RS485_TX_PIN 15
#define RS485_RX_PIN 6
#define RS485_TX_EN_PIN 2

AudisonACLink ac_link;

void setup(void)
{
  Serial.begin(115200);
  RS485_Config_t rs485_config;
  rs485_config.rs485_tx_pin = RS485_TX_PIN;
  rs485_config.rs485_rx_pin = RS485_RX_PIN;
  rs485_config.rs485_tx_en_pin = RS485_TX_EN_PIN;
  ac_link.init_ac_link_bus(&rs485_config);
}

void loop(void)
{
  /* Trial transmit function. We can send volume adjust packets for now */
  for (uint8_t i = 0; i <= 0x78; i++)
  {
    ac_link.set_volume(i);
    Serial.print("Volume: ");
    Serial.println(i, HEX);
    delay(250);
  }
}
