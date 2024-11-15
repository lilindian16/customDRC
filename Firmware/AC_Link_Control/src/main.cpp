#include <Arduino.h>
#include <CustomDRC.hpp>

void setup(void) {
    init_custom_drc();
}

void loop(void) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
