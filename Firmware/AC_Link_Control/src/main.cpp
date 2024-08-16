#include <Arduino.h>
#include <9BitUART.hpp>

#define AUDISON_BIT10_RS485_ADDRESS 0x00
#define AUDISON_DRC_RS485_ADDRESS 0x80

UART_9BIT Audison_AC_Link_Bus;

void setup(void)
{
  Audison_AC_Link_Bus.init_uart(38400);
}

void loop(void)
{
  /* Trial trnasmit function. We can send volume adjust packets for now */
  for (uint8_t i = 0; i < 0x78; i++)
  {
    uint8_t volume_adjust_packet[2] = {0x0A, i};
    Audison_AC_Link_Bus.write_to_audison_bus(AUDISON_BIT10_RS485_ADDRESS,
                                             AUDISON_DRC_RS485_ADDRESS,
                                             (uint8_t *)volume_adjust_packet,
                                             sizeof(volume_adjust_packet));
    delay(1000);
  }
}
