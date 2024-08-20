#include <Arduino.h>
#include <AudisonACLink.hpp>

#define AUDISON_BIT10_RS485_ADDRESS 0x00
#define AUDISON_DRC_RS485_ADDRESS 0x80

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
  /* Trial trnasmit function. We can send volume adjust packets for now */
  for (uint8_t i = 0; i <= 0x78; i++)
  {
    uint8_t volume_adjust_packet[2] = {0x0A, i};
    ac_link.write_to_audison_bus(AUDISON_BIT10_RS485_ADDRESS,
                                 AUDISON_DRC_RS485_ADDRESS,
                                 (uint8_t *)volume_adjust_packet,
                                 sizeof(volume_adjust_packet));
    Serial.print("Volume: ");
    Serial.println(i, HEX);
    delay(1000);
  }
}
