#pragma once

#include <stdint-gcc.h>

void web_server_init(struct DSP_Settings *dsp_settings);
void update_web_server_parameter(uint8_t parameter, uint8_t value);