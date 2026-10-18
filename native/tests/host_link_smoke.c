/* SPDX-License-Identifier: GPL-3.0-or-later
 * Windows: run with BB_NATIVE_MENUS=1 and bb-native-ui.dll beside this executable
 * to exercise source API loading/dispatch without SDL, a Vulkan device or a game.
 */
#include "../../src/runtime_pc_ui.h"
#include <assert.h>
#include <string.h>
int bbgpu_overlay_captures_input(void){return 0;}
int input_config_load(InputConfig *cfg,const char *path){(void)cfg;(void)path;return 0;}
int input_config_path(char *out,size_t size){if(size)out[0]=0;return 0;}
int runtime_file_translate(const char *guest,char *out,size_t size){(void)guest;if(size)out[0]=0;return -1;}
int runtime_file_mount(const char *guest,const char *host){(void)guest;(void)host;return 0;}
void runtime_file_unmount(const char *guest){(void)guest;}
static int saves;
static void save(void){++saves;}
int main(void){
    assert(runtime_pc_ui_init());
    assert(!runtime_pc_ui_window_mode(false));
    assert(runtime_pc_ui_capture_allowed());
    assert(!runtime_pc_ui_take_reload());
    runtime_pc_ui_event(NULL);
    runtime_pc_ui_pad_update(NULL);
    float x=1,y=2;assert(runtime_pc_ui_mouse_motion(&x,&y));
    assert(x==1&&y==2);
    runtime_pc_ui_render_save(save);assert(saves==1);
    return 0;
}
