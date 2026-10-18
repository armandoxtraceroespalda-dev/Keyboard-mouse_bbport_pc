// SPDX-License-Identifier: GPL-3.0-or-later
// DLL-local PC settings only. Never aliases the original renderer's struct.
#pragma once
#include <atomic>
#include <string>
namespace BbSettings {
constexpr int OutputCount=10,OutputDefault=1;
struct Values {
 std::atomic<bool> keyboard_mouse{true},key_prompts{true};
 std::atomic<bool> pc_controls{true},mouse_camera{true},mouse_menu{true},mouse_invert_x{false},mouse_invert_y{false},mouse_auto_rotation{false};
 std::atomic<int> mouse_sensitivity{5};
 std::atomic<bool> change_appearance{false},rebirth{false},camera_controls{true};
 std::atomic<bool> graphics_controls{false},graphics_vignette{true},effects[8]{};
 std::atomic<float> camera_fov_scale{1},camera_distance_scale{1},camera_height_scale{1};
 std::atomic<float> graphics_ao_strength{1},graphics_shadow_scale{1},graphics_bloom{1},graphics_saturation{1};
 std::atomic<int> output_res{1};
};
Values& Get();void Load();void Save();
void ApplyDs3Profile();void RestoreControllerDefaults();
void SelectKeyboardMouse(bool);
}
namespace NativePages {
void LoadBindings();void Tick();
std::string ControllerBindingLabel(int);
bool Install(unsigned char*,std::size_t);
void RequestReload();
void PreserveRenderSave(void(*)());
int Scancode(const char*);const char* KeyName(int);
}
