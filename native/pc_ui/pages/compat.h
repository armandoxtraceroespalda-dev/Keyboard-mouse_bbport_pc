// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../compat.h"
int native_scancode_from_name(const char*);
const char* native_scancode_name(int);

bool debug_menu_active();
