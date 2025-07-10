#pragma once

// lcd.h
#ifndef LCD_H
#define LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "esp_lvgl_port.h"

extern lv_disp_t * disp_handle;

void init_lvgl(void);
void lvgl_ui(void);
void lvgl_ui_label_set_text(const char *text);

#ifdef __cplusplus
}
#endif

#endif // LCD_H