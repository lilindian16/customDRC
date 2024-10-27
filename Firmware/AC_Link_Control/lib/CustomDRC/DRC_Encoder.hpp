#pragma once
#include <Arduino.h>

void init_drc_encoders(struct DSP_Settings* dsp_settings);
static IRAM_ATTR void enc_cb(void* arg);

void disable_encoders(void);
void enable_encoders(void);
bool are_encoders_enabled(void);

/* Declare these functions - they must be implemented in main */
extern void on_button_held();
extern void on_button_released(bool was_held);