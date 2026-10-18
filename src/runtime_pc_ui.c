/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native UI module loader. No inspection or modification of the host executable.
 */
#ifdef _WIN32
#include "runtime_pc_ui.h"
#include "runtime_pc_ui_api.h"
#include "runtime.h"
#include "../gpu/bbgpu.h"
#include <windows.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <wchar.h>
static HMODULE module;
static _Atomic(const BbPcUiApi *) ui;
static const bool *keyboard_state(int *count) {
    typedef const bool *(*Fn)(int *);
    HMODULE sdl=GetModuleHandleW(L"SDL3.dll");
    Fn fn=sdl?(Fn)(uintptr_t)GetProcAddress(sdl,"SDL_GetKeyboardState"):NULL;
    if (!fn) {if(count)*count=0;return NULL;}
    return fn(count);
}
int runtime_pc_ui_init(void) {
    if(atomic_load_explicit(&ui,memory_order_acquire))return 1;
    if(!getenv("BB_NATIVE_MENUS")&&!getenv("BB_NATIVE_OPTIONS")&&!getenv("BB_NATIVE_MIRROR"))return 1;
    wchar_t path[32768];
    DWORD n=GetModuleFileNameW(NULL,path,(DWORD)(sizeof(path)/sizeof(path[0])));
    if(!n||n>=sizeof(path)/sizeof(path[0]))return 0;
    wchar_t *slash=wcsrchr(path,L'\\');
    if(!slash)return 0;
    const wchar_t name[]=L"bb-native-ui.dll";
    size_t left=(sizeof(path)/sizeof(path[0]))-(size_t)(slash+1-path);
    if(left<sizeof(name)/sizeof(name[0]))return 0;
    wcscpy(slash+1,name);
    module=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module){fprintf(stderr,"Native UI: cannot load bb-native-ui.dll (Windows error %lu)\n",(unsigned long)GetLastError());return 0;}
    BbPcGetApiFn get=(BbPcGetApiFn)(uintptr_t)GetProcAddress(module,"BbPcGetApi");
    const BbPcHostApi host={sizeof(BbPcHostApi),BB_PC_UI_API_VERSION,
        bbgpu_overlay_captures_input,keyboard_state,input_config_load,input_config_path,
        runtime_file_translate,runtime_file_mount,runtime_file_unmount};
    const BbPcUiApi *candidate=get?get(&host):NULL;
    if(!candidate||candidate->version!=BB_PC_UI_API_VERSION||candidate->size!=sizeof(BbPcUiApi)||
       !candidate->image_ready||!candidate->event||!candidate->window_mode||!candidate->take_reload||
       !candidate->capture_allowed||!candidate->capture_note||!candidate->pad_update||
       !candidate->filter_config||!candidate->mouse_motion||!candidate->render_save){
        fputs("Native UI: incompatible module interface; rebuild/package the matching module\n",stderr);
        FreeLibrary(module);module=NULL;return 0;
    }
    atomic_store_explicit(&ui,candidate,memory_order_release);
    printf("Native UI: source API %u loaded\n",BB_PC_UI_API_VERSION);
    return 1;
}
#define PC_API const BbPcUiApi *api=atomic_load_explicit(&ui,memory_order_acquire)
void runtime_pc_ui_image_ready(uint8_t *p,size_t n){PC_API;if(api)api->image_ready(p,n);}
void runtime_pc_ui_event(void *p){PC_API;if(api)api->event(p);}
bool runtime_pc_ui_window_mode(bool enabled){PC_API;return api?api->window_mode(enabled):enabled;}
bool runtime_pc_ui_take_reload(void){PC_API;return api&&api->take_reload();}
bool runtime_pc_ui_capture_allowed(void){PC_API;return !api||api->capture_allowed();}
void runtime_pc_ui_capture_note(bool captured){PC_API;if(api)api->capture_note(captured);}
void runtime_pc_ui_pad_update(void *p){PC_API;if(api)api->pad_update(p);}
void runtime_pc_ui_filter_config(InputConfig *p){PC_API;if(api)api->filter_config(p);}
bool runtime_pc_ui_mouse_motion(float *x,float *y){PC_API;return !api||api->mouse_motion(x,y);}
void runtime_pc_ui_render_save(void (*save)(void)){PC_API;if(api)api->render_save(save);else save();}
#undef PC_API
#endif
