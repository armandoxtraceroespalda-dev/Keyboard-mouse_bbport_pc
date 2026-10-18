// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
void native_input_event(void*);
void native_apply_input(void*,const bool*,int,bool,bool,bool);
void native_note_context(bool mouse_mode);
bool native_rebinding();
