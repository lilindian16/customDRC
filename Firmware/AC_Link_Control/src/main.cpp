#include <Arduino.h>
#include <Custom_DRC.hpp>

void setup(void) {
    init_custom_drc();
}

void loop(void) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
