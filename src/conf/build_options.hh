#pragma once
#include "conf/model.hh"

// CATALYST_SECOND_MODE selects which App class occupies the secondary mode slot
// (the mode reached via the right 3-button cluster: Shift + Tap + Chan).
// Override at build time via cmake: -DCATALYST_SECOND_MODE=CATALYST_MODE_VOLTSEQ
#define CATALYST_MODE_MACRO   0
#define CATALYST_MODE_VOLTSEQ 1

#ifndef CATALYST_SECOND_MODE
#define CATALYST_SECOND_MODE CATALYST_MODE_MACRO
#endif

namespace Catalyst2::BuildOptions
{

inline constexpr auto seq_gate_overrides_prev_step = true;
inline constexpr auto default_mode = Model::Mode::Sequencer;
inline constexpr bool ManualColorMode = false;

#define SKIP_STARTUP_ANIMATION false

//////////////////////////////////////////////////////////////////////

#if SKIP_STARTUP_ANIMATION
#warning "Remember to turn the startup animation on!"
#endif

inline constexpr auto skip_startup_animation = SKIP_STARTUP_ANIMATION;
static_assert(skip_startup_animation == SKIP_STARTUP_ANIMATION);

inline constexpr bool second_mode_is_voltseq = (CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ);
static_assert(second_mode_is_voltseq == (CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ));

} // namespace Catalyst2::BuildOptions
