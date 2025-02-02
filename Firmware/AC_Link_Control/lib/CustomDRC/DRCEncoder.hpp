/**
 * Author: Jaime Sequeira
 */

#pragma once

#include <Arduino.h>

void init_drc_encoders(struct DSP_Settings* dsp_settings);
static IRAM_ATTR void enc_cb(void* arg);

void disable_encoders(void);
void enable_encoders(void);
bool are_encoders_enabled(void);

void set_encoder_value(uint8_t encoder_index, uint8_t value);

/* Declare these functions - they must be implemented in main */
extern void on_button_held();
extern void on_button_released(bool was_held);
extern void on_button_clicked(void);