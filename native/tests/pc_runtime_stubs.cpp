// SPDX-License-Identifier: GPL-3.0-or-later
// Renderer-only Windows tests do not create a guest UI module.
#include "../../src/runtime_pc_ui.h"
#ifdef _WIN32
extern "C" {
int runtime_pc_ui_init(){return 1;}
void runtime_pc_ui_image_ready(uint8_t*,size_t){}
void runtime_pc_ui_event(void*){}
bool runtime_pc_ui_window_mode(bool enabled){return enabled;}
bool runtime_pc_ui_take_reload(){return false;}
bool runtime_pc_ui_capture_allowed(){return true;}
void runtime_pc_ui_capture_note(bool){}
void runtime_pc_ui_pad_update(void*){}
void runtime_pc_ui_filter_config(InputConfig*){}
bool runtime_pc_ui_mouse_motion(float*,float*){return true;}
void runtime_pc_ui_render_save(void(*save)()){save();}
}
#endif
