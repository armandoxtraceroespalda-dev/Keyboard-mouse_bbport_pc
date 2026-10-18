// SPDX-License-Identifier: GPL-3.0-or-later
#include "physical_input.h"
#include <cassert>
#include <cstdio>
int main(){
 InputConfig c{};c.mouse.camera_direct=1;c.mouse.camera_sensitivity=1.25;c.toggle_scancode=64;c.reload_scancode=65;
 c.table.binding_count[OUT_CROSS]=2;c.table.bindings[OUT_CROSS][0]={IN_KEY,44,0};c.table.bindings[OUT_CROSS][1]={IN_CBUTTON,0,0};
 c.table.binding_count[OUT_SQUARE]=2;c.table.bindings[OUT_SQUARE][0]={IN_KEY,8,0};c.table.bindings[OUT_SQUARE][1]={IN_CBUTTON,2,0};
 c.table.binding_count[OUT_R1]=2;c.table.bindings[OUT_R1][0]={IN_MOUSE_BUTTON,1,0};c.table.bindings[OUT_R1][1]={IN_CBUTTON,10,0};
 c.table.binding_count[OUT_PAD_UP]=1;c.table.bindings[OUT_PAD_UP][0]={IN_MOUSE_WHEEL,0,0};
 c.table.binding_count[OUT_L2]=1;c.table.bindings[OUT_L2][0]={IN_AXIS,4,0};
 c.table.binding_count[OUT_AXIS_LEFT_X_MINUS]=1;c.table.bindings[OUT_AXIS_LEFT_X_MINUS][0]={IN_AXIS_HALF,0,-1};
 KeepPhysicalControllerBindings(c);
 assert(c.table.binding_count[OUT_CROSS]==1&&c.table.bindings[OUT_CROSS][0].kind==IN_CBUTTON);
 assert(c.table.binding_count[OUT_SQUARE]==1&&c.table.bindings[OUT_SQUARE][0].value==2);
 assert(c.table.binding_count[OUT_R1]==1&&c.table.bindings[OUT_R1][0].kind==IN_CBUTTON);
 assert(c.table.binding_count[OUT_PAD_UP]==0);
 assert(c.table.bindings[OUT_L2][0].kind==IN_AXIS&&c.table.bindings[OUT_AXIS_LEFT_X_MINUS][0].half_sign==-1);
 assert(c.mouse.camera_direct==1&&c.mouse.camera_sensitivity==1.25&&c.toggle_scancode==64&&c.reload_scancode==65);
 KeepPhysicalControllerBindings(c);assert(c.table.binding_count[OUT_CROSS]==1);
 puts("PASS: stock Space->Cross and E->Square removed; controller buttons/axes, camera and hotkeys preserved on load/reload.");
}
