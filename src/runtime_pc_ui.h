/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BB_RUNTIME_PC_UI_H
#define BB_RUNTIME_PC_UI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "runtime_input_config.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifdef _WIN32
int runtime_pc_ui_init(void);
void runtime_pc_ui_image_ready(uint8_t *image, size_t size);
void runtime_pc_ui_event(void *sdl_event);
bool runtime_pc_ui_window_mode(bool enabled);
bool runtime_pc_ui_take_reload(void);
bool runtime_pc_ui_capture_allowed(void);
void runtime_pc_ui_capture_note(bool captured);
void runtime_pc_ui_pad_update(void *pad_data);
void runtime_pc_ui_filter_config(InputConfig *cfg);
bool runtime_pc_ui_mouse_motion(float *x, float *y);
void runtime_pc_ui_render_save(void (*original_save)(void));
#else
/* The existing Linux implementation remains available without the Windows UI module. */
static inline int runtime_pc_ui_init(void) { return 1; }
static inline void runtime_pc_ui_image_ready(uint8_t *p,size_t n) {(void)p;(void)n;}
static inline void runtime_pc_ui_event(void *p) {(void)p;}
static inline bool runtime_pc_ui_window_mode(bool enabled) { return enabled; }
static inline bool runtime_pc_ui_take_reload(void) { return false; }
static inline bool runtime_pc_ui_capture_allowed(void) { return true; }
static inline void runtime_pc_ui_capture_note(bool enabled) {(void)enabled;}
static inline void runtime_pc_ui_pad_update(void *p) {(void)p;}
static inline void runtime_pc_ui_filter_config(InputConfig *p) {(void)p;}
static inline bool runtime_pc_ui_mouse_motion(float *x,float *y) {(void)x;(void)y;return true;}
static inline void runtime_pc_ui_render_save(void (*save)(void)) {save();}
#endif
#ifdef __cplusplus
}
#endif
#endif
