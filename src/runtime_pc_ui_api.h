/* SPDX-License-Identifier: GPL-3.0-or-later
 * Versioned source interface between bb-probe and the native PC UI module.
 * Host function addresses are supplied by the linker/runtime, never PE offsets.
 */
#ifndef BB_RUNTIME_PC_UI_API_H
#define BB_RUNTIME_PC_UI_API_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "runtime_input_config.h"
#define BB_PC_UI_API_VERSION 1u
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BbPcHostApi {
    uint32_t size, version;
    int (*overlay_open)(void);
    const bool *(*keyboard_state)(int *count);
    int (*input_load)(InputConfig *cfg, const char *path);
    int (*input_path)(char *out, size_t size);
    int (*file_translate)(const char *guest, char *out, size_t size);
    int (*file_mount)(const char *guest, const char *host);
    void (*file_unmount)(const char *guest);
} BbPcHostApi;
typedef struct BbPcUiApi {
    uint32_t size, version;
    void (*image_ready)(uint8_t *image, size_t size);
    void (*event)(void *sdl_event);
    bool (*window_mode)(bool enabled);
    bool (*take_reload)(void);
    bool (*capture_allowed)(void);
    void (*capture_note)(bool captured);
    void (*pad_update)(void *pad_data);
    void (*filter_config)(InputConfig *cfg);
    bool (*mouse_motion)(float *x, float *y);
    void (*render_save)(void (*original_save)(void));
} BbPcUiApi;
typedef const BbPcUiApi *(*BbPcGetApiFn)(const BbPcHostApi *host);
#ifdef __cplusplus
}
#endif
#endif
