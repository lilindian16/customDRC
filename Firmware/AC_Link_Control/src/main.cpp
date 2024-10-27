#include <Custom_DRC.hpp>
#include <Arduino.h>

void setup(void)
{
  init_custom_drc();
}

void loop(void)
{
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
