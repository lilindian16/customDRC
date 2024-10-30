#pragma once

#include <stdint-gcc.h>

void web_server_init(struct DSP_Settings* dsp_settings);

/**
 * void update_web_server_parameter(uint8_t parameter, uint8_t value)
 *
 * Update a web server parameter with the given value
 */
void update_web_server_parameter(uint8_t parameter, uint8_t value);

/**
 * void update_web_server_parameter_string(uint8_t parameter, char* value_string)
 *
 * Update a web server parameter with a string. Note, the string must be null terminated!!
 */
void update_web_server_parameter_string(uint8_t parameter, char* value_string);