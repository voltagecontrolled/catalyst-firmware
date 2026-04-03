#pragma once

#include "channel.hh"
#include <algorithm>
#include <bit>
#include <cstdint>

namespace Catalyst2::Sequencer
{
struct TrigDelay {
	using type = int8_t;
	static constexpr type min = -31;
	static constexpr type max = 31;
	static constexpr type bits = std::bit_width(static_cast<uint32_t>(max - min));
};
struct Probability {
	using type = uint8_t;
	static constexpr type min = 0u;
	static constexpr type max = 15u;
	static constexpr type bits = std::bit_width(max);
	static constexpr float toFloat(type p) {
		return p / static_cast<float>(max);
	}
};

struct Step {
	uint32_t cv : Channel::Cv::bits = Channel::Cv::zero;
	int32_t trig_delay : TrigDelay::bits = 0;
	uint32_t gate : Channel::Gate::bits = 0;
	uint32_t morph : 4 = 0;                         // CV channels: shape 0-15
	uint32_t ratchet_repeat_count : 3 = 0;          // Gate channels: 0-7 = 1x-8x (ratchets or repeats)
	uint32_t is_repeat : 1 = 0;                     // Gate channels: 0 = ratchet, 1 = repeat
	uint32_t probability : Probability::bits = 0;

public:
	Channel::Cv::type ReadCv(float random = 0.f) const {
		return Channel::Cv::Read(cv, random);
	}
	void IncCv(int32_t inc, bool fine, Channel::Cv::Range range) {
		cv = Channel::Cv::Inc(cv, inc, fine, range);
	}
	float ReadGate(float adjustment = 0.f) const {
		static constexpr std::array<float, Channel::Gate::max + 1> gate_widths = {0.f,
																				  0.004f,
																				  0.032f,
																				  0.064f,
																				  0.128f,
																				  0.256f,
																				  7.f / 16.f,
																				  8.f / 16.f,
																				  9.f / 16.f,
																				  10.f / 16.f,
																				  11.f / 16.f,
																				  12.f / 16.f,
																				  13.f / 16.f,
																				  14.f / 16.f,
																				  15.f / 16.f,
																				  1.f};
		auto random_gate = gate_widths[gate] + adjustment;
		// Fold back across 0
		random_gate = std::abs(random_gate);
		// Fold back across 1
		if (random_gate > 1.f)
			random_gate = 2.f - random_gate;

		return std::clamp(random_gate, 0.f, 1.f);
	}
	void IncGate(int32_t inc) {
		gate = Channel::Gate::Inc(gate, inc);
	}
	float ReadTrigDelay() const {
		return trig_delay / static_cast<float>(TrigDelay::max + 1);
	}
	void IncTrigDelay(int32_t inc) {
		trig_delay = std::clamp<int32_t>(trig_delay + inc, TrigDelay::min, TrigDelay::max);
	}
	float ReadMorph() const {
		return morph / static_cast<float>(15u);
	}
	void IncMorph(int32_t inc) {
		morph = std::clamp<int32_t>(morph + inc, 0, 15);
	}
	uint8_t ReadRatchetRepeatCount() const {
		return ratchet_repeat_count;
	}
	bool IsRepeat() const {
		return is_repeat;
	}
	// Legacy accessor for playback — returns ratchet count when in ratchet mode, 0 when in repeat mode
	uint8_t ReadRetrig() const {
		return is_repeat ? 0u : ratchet_repeat_count;
	}
	// Called by UI: CW increments ratchet count, CCW increments repeat count.
	// State modelled as a signed position: positive = ratchet, negative = repeat, zero = neutral.
	// Crossing zero passes through neutral rather than jumping directly between modes.
	void IncRatchetRepeat(int32_t inc) {
		int32_t pos = is_repeat ? -(int32_t)ratchet_repeat_count : (int32_t)ratchet_repeat_count;
		pos = std::clamp<int32_t>(pos + inc, -7, 7);
		if (pos > 0) {
			is_repeat = 0;
			ratchet_repeat_count = pos;
		} else if (pos < 0) {
			is_repeat = 1;
			ratchet_repeat_count = -pos;
		} else {
			is_repeat = 0;
			ratchet_repeat_count = 0;
		}
	}
	Probability::type ReadProbability() const {
		return probability;
	}
	void IncProbability(int32_t inc) {
		probability = std::clamp<int32_t>(probability + inc, Probability::min, Probability::max);
	}
	bool Validate() const {
		auto ret = true;
		ret &= Channel::Cv::Validate(cv);
		ret &= Channel::Gate::Validate(gate);
		return ret;
	}
};

static_assert(sizeof(Step) <= 8);

} // namespace Catalyst2::Sequencer
