// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated native appearance-controller lifecycle; no renderer or donor engine.
#include "engine_hooks.h"
#include "menu_memory.h"
#include "native_mirror.h"
#include "guest_layout.h"
#include <windows.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
extern "C" int runtime_file_translate(const char*,char*,std::size_t);
extern "C" int runtime_file_mount(const char*,const char*);
#include "pages/engine_state.h"
#include "pages/option_menu.h"
#include "pages/camera.h"
#include "pages/camera_rotation.h"
#include "pages/rebirth.h"
#include "pages/key_prompts.h"
#include "pages/item_cycle.h"
#include "pages/bbport_settings.h"
namespace BbEngine {
namespace {
constexpr std::uint8_t kStepPrologue[] = {0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x50,0x4d,0x89,0xce,0x48,0x89,0xfb};
constexpr std::uint8_t kFramePrologue[] = {0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x38};
void Log(const char* format, ...) {
    std::fputs("Engine: ", stdout);
    va_list args; va_start(args, format); std::vprintf(format, args); va_end(args);
    std::putchar('\n');
}
// --- the editor's frame ----------------------------------------------------
//
// The list the talk command opens is character creation's Appearance list
// alone. In character creation it sits in the ChrMake_BG window
// (chrmake_bg.gfx: the book, the panels, the model): its controller
// (sub_1f06b10) makes the preview scenes and their render targets, binds the
// menu's placeholder images to them and, in its update (sub_1f07870), copies
// the hunter's face data into them, swaps them in as each loads, turns the
// mouse and the zoom keys into the model's rotation and distance, and picks
// the image the panel shows. Character creation's top controller (sub_1f8dbb0)
// opens that window from a window descriptor (sub_1ed8150), and its menus
// update it every frame as a dialog's child window (sub_1ed5b10, the base
// update, walks them); at the mirror nothing does either, so the list floats
// over the world with an empty panel and the HUD on top.
//
// Here: the list's window step being made (engine/menu_steps.h: type 7,
// "ChrMakeCommandList") while the hunter is in the world - character creation
// runs at the title, with its own window - opens ChrMake_BG with a descriptor
// of ours, filled as character creation fills its own; each frame its
// controller's update runs; and a reference we hold on the list's step tells
// when the list has closed, which closes the window (sub_1ed8e60).
constexpr std::uint64_t kListType = 7;
constexpr char16_t kListName[] = u"ChrMakeCommandList";
constexpr std::uint64_t kOpenWindow = 0x1ed8150;   // (descriptor)
constexpr std::uint64_t kCloseWindow = 0x1ed8e60;  // (descriptor): closes the window, unloads the movie
constexpr std::uint64_t kMenuHeap = 0x5940418;     // the menus' allocator: +0x58 alloc(size, align), +0x70 free(p)
constexpr std::size_t kDescSize = 0xc0;
constexpr std::uint64_t kDescVtable = 0x5736b30, kDescOwnerVtable = 0x5736b70, kDescListVtable = 0x57310d0;
constexpr std::uint64_t kFactoryVtable = 0x573ee10;  // the functor character creation stores ...
constexpr std::uint64_t kFrameFactory = 0x1f08010;   // ... around this factory: sub_1f06b10's controller
constexpr std::uint64_t kFrameName = 0x4d98486;      // L"ChrMake_BG"
constexpr std::uint64_t kFrameType = 6;

std::uint64_t g_slide = 0;
int g_watch = -1;                          // the list's steps (engine/menu_steps.h)
std::uint64_t g_list = 0;                  // the list's step, while we hold a reference
std::uint64_t g_desc = 0;                  // our descriptor, while ChrMake_BG is open
std::uint8_t g_active = 1;                 // the update's argument (a flag byte it reads)
int g_opened = 0;                          // for the log

std::uint64_t slot(std::uint64_t bn) { return g_slide + (bn - kPreferredGuestSlide); }
std::uint64_t rd64(std::uint64_t a) {
    std::uint64_t v;
    v = 0; BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(a), &v, sizeof v);
    return v;
}
void wr64(std::uint64_t a, std::uint64_t v) { std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a)), &v, sizeof v); }

std::uint64_t heap_alloc(std::size_t n) {
    const std::uint64_t heap = rd64(slot(kMenuHeap));
    if (!heap) return 0;
    return static_cast<std::uint64_t>(Call(rd64(rd64(heap) + 0x58), heap, n, 0x10));
}
void heap_free(std::uint64_t p) {
    const std::uint64_t heap = rd64(slot(kMenuHeap));
    if (heap && p) Call(rd64(rd64(heap) + 0x70), heap, p);
}

bool open_frame() {
    const std::uint64_t d = heap_alloc(kDescSize);
    if (!d) return false;
    std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(d)), 0, kDescSize);
    wr64(d + 0x00, slot(kDescVtable));
    wr64(d + 0x08, slot(kDescOwnerVtable));
    wr64(d + 0x18, kFrameType);
    wr64(d + 0x20, slot(kFrameName));
    wr64(d + 0x30, slot(kFactoryVtable));  // the factory functor, in the descriptor's own storage
    wr64(d + 0x38, slot(kFrameFactory));
    wr64(d + 0x50, d + 0x30);
    wr64(d + 0x68, slot(kDescListVtable));
    Call(slot(kOpenWindow), d);
    if (!rd64(d + 0x60)) {  // no window: nothing to keep
        Call(slot(kCloseWindow), d);
        heap_free(d);
        return false;
    }
    g_desc = d;
    return true;
}

void close_frame() {
    Call(slot(kCloseWindow), g_desc);
    heap_free(g_desc);
    g_desc = 0;
}

void release_list() {
    menu_step_release(g_list);
    g_list = 0;
}

bool g_frame_on = false;

}  // namespace

void change_appearance_tick() {
    if (!g_frame_on) return;
    if (const std::uint64_t made = menu_steps_take(g_watch)) {
        std::uint32_t block = 0;
        if (!g_list && !g_desc && world_player_block(&block) && *menu_step_count(made) >= 1 && open_frame()) {
            menu_step_hold(made);  // ours: when it is the last, the list has closed
            g_list = made;
            if (g_opened++ < 4) Log("change appearance: the editor's frame opened (ChrMake_BG, preview of the hunter)");
        }
    }
    if (g_desc) {
        const std::uint64_t ctl = rd64(g_desc + 0x60);
        if (ctl) Call(rd64(rd64(ctl) + 0x18), ctl, &g_active);
    }
    if (g_list && *menu_step_count(g_list) == 1) {
        close_frame();
        release_list();
    }
}


namespace {
std::int64_t __attribute__((sysv_abi)) StepHook(std::uint64_t, const std::uint64_t* saved) {
    menu_steps_notify(saved);
    return 0;
}
std::int64_t __attribute__((sysv_abi)) FrameHook(std::uint64_t, const std::uint64_t*) {
    try {
      change_appearance_tick();
      BbEngine::Options::Tick();
      BbEngine::Camera::Tick();
      BbEngine::Rebirth::Tick();
      NativePages::Tick();
      native_rotation_tick();
    } catch(const std::exception& e) { Log("PC options: %s",e.what()); }
    return 0; // The established FPS++ implementation still runs, byte for byte.
}
} // namespace

} // namespace BbEngine
bool native_mirror_install(std::uint8_t* image,std::size_t size){
 using namespace BbEngine;
 const char* asset=std::getenv("BB_DREAM_MIRROR_ASSET");
 const char* identity=std::getenv("BB_ENGINE_IMAGE_SHA256");
 if(!identity || std::strcmp(identity,"071df19c8880086d97182dbc057bc8cb37badaca57d9112683836b24a0444c0a")){Log("PC startup rejected: verified game identity missing or different");return false;}
 const bool pages=std::getenv("BB_NATIVE_OPTIONS");
 constexpr const char* guest="/app0/dvdroot_ps4/map/mapstudio/m21_00_00_00.msb.dcx";
 const char* original=std::getenv("BB_UI_GAME_VIEW");if(!original)original=std::getenv("BB_GAME_DIR");
 char resolved[2048]{};std::error_code ec;
 if(asset && (!original || runtime_file_translate(guest,resolved,sizeof resolved) || !std::filesystem::equivalent(resolved,std::filesystem::path(original)/"dvdroot_ps4/map/mapstudio/m21_00_00_00.msb.dcx",ec))){Log("PC startup rejected: mirror map does not resolve to the prepared game view");return false;}
 Hook hooks[2];
 auto prepare=[&](Hook& hook,std::size_t offset,auto& expected,Callback callback,const char* name){
  if(Prepare(hook,image,size,offset,expected,callback))return true;
  Log("PC startup rejected: %s hook at image offset 0x%zx (ELF VA 0x%zx)",name,offset,offset+GuestLayout::PreferredBase);
  if(offset<size && sizeof(expected)<=size-offset){
   std::fputs("Engine: expected / actual:",stdout);
   for(auto byte:expected)std::printf(" %02x",unsigned(byte));
   std::fputs(" /",stdout);
   for(std::size_t i=0;i<sizeof(expected);++i)std::printf(" %02x",unsigned(image[offset+i]));
   std::putchar('\n');
  }
  return false;
 };
 Log("PC startup source API 1: ELF-preserving image layout; size=0x%zx; options=%d; mirror=%d",size,int(pages),int(asset!=nullptr));
 const bool step_ok=prepare(hooks[0],GuestLayout::StepOffset,kStepPrologue,StepHook,"menu step");
 const bool frame_ok=prepare(hooks[1],GuestLayout::FrameOffset,kFramePrologue,FrameHook,"frame lifecycle");
 const bool heap_ok=MenuMemory::Compatible(image,size);
 Log("PC startup checks: menu-step=%s frame=%s menu-heaps=%s",step_ok?"OK":"FAIL",frame_ok?"OK":"FAIL",heap_ok?"OK":"FAIL");
 if(!heap_ok){
  Log("PC startup rejected: Scaleform heap layout or budgets differ");
 }
 if(!step_ok || !frame_ok || !heap_ok){
  for(auto& h:hooks)Discard(h);return false;
 }
 if(asset && (!std::filesystem::is_regular_file(asset,ec) || runtime_file_mount(guest,asset))){Log("PC startup rejected: mirror asset mount failed");for(auto& h:hooks)Discard(h);return false;}
 StateInitialize(reinterpret_cast<uint64_t>(image));g_slide=reinterpret_cast<uint64_t>(image);
 if(pages){
  if(!ItemCycle::Install(image,size)){for(auto& h:hooks)Discard(h);return false;}
  native_rotation_install(image,size);
  KeyPrompts::Install(image,size);
  if(!Options::Install(image,size)){Log("PC startup rejected: System options installation failed");for(auto& h:hooks)Discard(h);return false;}
  if(BbSettings::Get().camera_controls && !Camera::Install(image,size)){Log("PC startup rejected: camera installation failed");for(auto& h:hooks)Discard(h);return false;}
  if(std::getenv("BB_REBIRTH_ASSET") && !Rebirth::Install(image,size)){Log("PC startup rejected: Rebirth installation failed");for(auto& h:hooks)Discard(h);return false;}
 }else MenuMemory::Install(image,size);
 if(asset){g_watch=menu_steps_watch(7,u"ChrMakeCommandList");g_frame_on=g_watch>=0;}
 // Mirror text also works when the independent in-game tweak pages are off.
 const char* mirror_messages=std::getenv("BB_MIRROR_MESSAGES");
 if(asset&&!pages&&mirror_messages){
  if(!std::filesystem::is_directory(mirror_messages,ec)||ec||runtime_file_mount("/app0/dvdroot_ps4/msg",mirror_messages)){
   Log("PC startup rejected: mirror message-directory mount failed");for(auto& h:hooks)Discard(h);return false;
  }
 }
 Commit(hooks);
 Log("original engine: native PC options/editor lifecycle installed");return true;
}
