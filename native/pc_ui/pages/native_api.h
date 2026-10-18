// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../runtime_input_config.h"
#include <cstdint>
bool native_read_original_config(InputConfig&);
void native_request_reload();
bool native_options_enabled();
bool native_mouse_mode();

bool native_reserved_key(int);

void native_request_mouse_enable();
