/*
 * Take from https://github.com/longpth/ESP32CamAI/blob/master/ESP32CamAI_arduino/camera_wrap.h
*/

//#pragma once // not recommended to use, just use include

#include "esp_camera.h"
#include <string.h>
#include <Arduino.h>

extern int initCamera();
// extern esp_err_t grabImage( size_t& jpg_buf_len, uint8_t *jpg_buf);
