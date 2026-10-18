// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
// Ignore Space's keyboard Back action only inside a world menu and only
// for the opt-in pointer/KBM path. Native mouse Back is added afterward.
inline uint32_t native_menu_space_filter(uint32_t buttons,uint32_t back,
    bool option,bool kbm,bool world,bool menu,bool focus,bool overlay,
    bool keyboard_device,bool space_bound_and_down){
    return option && kbm && world && menu && focus && !overlay && keyboard_device && space_bound_and_down ? buttons & ~back : buttons;
}
