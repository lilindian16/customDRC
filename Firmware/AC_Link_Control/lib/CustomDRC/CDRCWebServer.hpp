#pragma once

#include <stdint-gcc.h>

void web_server_init(void);

void update_web_server_master_volume_value(uint8_t value);
void update_web_server_fader_value(uint8_t value);
void update_web_server_balance_value(uint8_t value);
void update_web_server_sub_volume_value(uint8_t value);