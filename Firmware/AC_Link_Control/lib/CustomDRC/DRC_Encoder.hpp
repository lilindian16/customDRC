#pragma once
#include <Arduino.h>

void init_drc_encoders(void);
static IRAM_ATTR void enc_cb(void *arg);