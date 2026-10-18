// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
namespace BbEngine::GuestLayout {
// link_modules.py copies each segment to image[p_vaddr], and probe.c loads
// that image unchanged. Binary Ninja's preferred base applies to BN addresses,
// NOT these ELF virtual addresses verified by integrated_mirror_assets.py.
inline constexpr std::size_t PreferredBase=0;
inline constexpr std::size_t StepVA=0x1c1cce0, FrameVA=0x2034770;
constexpr std::size_t ImageOffset(std::size_t va) { return va; }
inline constexpr auto StepOffset=ImageOffset(StepVA), FrameOffset=ImageOffset(FrameVA);
}
