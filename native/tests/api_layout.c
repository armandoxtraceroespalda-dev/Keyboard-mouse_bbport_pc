/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../src/runtime_pc_ui_api.h"
_Static_assert(sizeof(void*)==8,"Windows x64 host required");
_Static_assert(sizeof(bool)==1,"SDL3 bool ABI");
_Static_assert(sizeof(BbPcHostApi)==64,"C host table ABI");
_Static_assert(sizeof(BbPcUiApi)==88,"C UI table ABI");
_Static_assert(offsetof(BbPcUiApi,image_ready)==8,"UI table alignment");
