/**
 * Author: Jaime Sequeira
 */

#pragma once

#include <stdint-gcc.h>

void web_server_init(struct DSP_Settings* dsp_settings);

/**
 * Update a web server parameter with the given value
 * @param parameter Use DSP_Settings_Indexes enum to invoke the parameter to be changed
 * @param value
 */
void update_web_server_parameter(uint8_t parameter, uint8_t value);

/**
 * Update a web server parameter with a string. Note, the string must be null terminated!!
 */
void update_web_server_parameter_string(uint8_t parameter, char* value_string);

/**
 * Task to handle updating the DRC with the latest settings / inputs from the webserver
 */
void update_drc_settings_task(void* pvParameters);