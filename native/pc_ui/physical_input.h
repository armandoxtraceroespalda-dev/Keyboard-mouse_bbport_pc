// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "runtime_input_config.h"
// input_config_load repairs a controller-only file by inserting stock keyboard
// bindings. Remove those after load, before the original sampler sees them.
inline void KeepPhysicalControllerBindings(InputConfig& cfg){
 for(int action=0;action<OUT_COUNT;++action){
  auto& count=cfg.table.binding_count[action];int write=0;
  for(int i=0;i<count;++i){const auto binding=cfg.table.bindings[action][i];
   if(binding.kind==IN_CBUTTON||binding.kind==IN_AXIS||binding.kind==IN_AXIS_HALF)
    cfg.table.bindings[action][write++]=binding;
  }
  for(int i=write;i<count;++i)cfg.table.bindings[action][i]={};
  count=static_cast<uint8_t>(write);
 }
}
