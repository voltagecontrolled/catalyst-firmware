#pragma once

#include "clock.hh"
#include "conf/build_options.hh"
#include "conf/model.hh"
#include "pathway.hh"
#include "quantizer.hh"
#include "recorder.hh"
#include "sequencer.hh"
#include "shared.hh"
#include "util/countzip.hh"
#include <array>
#include <optional>
#include <type_traits>

#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
#include "voltseq.hh"
#else
#include "macro.hh"
#endif

namespace Catalyst2
{

// Non-volatile data shared between the two modes. The secondary mode slot is
// occupied by Macro::Data (stock) or VoltSeq::Data (VoltSeq build) — both fit
// within the MacroModeData flash allocation.

struct Data {
	Shared::Data shared{};
#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
	VoltSeq::Data macro{};
#else
	Macro::Data macro{};
#endif
	Sequencer::Data sequencer{};
};

#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
inline constexpr auto macro_data_size = sizeof(VoltSeq::Data);
#else
inline constexpr auto macro_data_size = sizeof(Macro::Data); // 7820
#endif
inline constexpr auto seq_data_size = sizeof(Sequencer::Data);
inline constexpr auto data_size = sizeof(Data);

struct Params {
	Data data{};
	Shared::Interface shared{data.shared};
	Sequencer::Interface sequencer{data.sequencer, shared};
#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
	VoltSeq::Interface macro{data.macro, shared};
#else
	Macro::Interface macro{data.macro, shared};
#endif
};

inline constexpr auto params_size = sizeof(Params);

} // namespace Catalyst2
