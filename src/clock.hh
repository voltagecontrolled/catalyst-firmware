#pragma once

#include "conf/model.hh"
#include "controls.hh"
#include "legacy/v1_0/conf/model.hh"
#include <algorithm>
#include <array>
#include <utility>

namespace Catalyst2::Clock
{

constexpr uint32_t BpmToTicks(uint32_t bpm) {
	return (60.f * Model::sample_rate_hz) / bpm;
}
constexpr float TicksToBpm(uint32_t tick) {
	return (60.f * Model::sample_rate_hz) / tick;
}
constexpr uint32_t MsToTicks(uint32_t ms) {
	return (Model::sample_rate_hz / 1000.f) * ms;
}
constexpr uint32_t TicksToMs(uint32_t tick) {
	return ((tick + 0.f) / Model::sample_rate_hz) * 1000;
}

class Timer {
	const uint32_t duration;
	uint32_t set_time;

public:
	Timer(uint32_t duration_ms)
		: duration{MsToTicks(duration_ms)} {
	}
	void SetAlarm() {
		set_time = Controls::TimeNow();
	}
	bool Check() {
		return Controls::TimeNow() - set_time >= duration;
	}
};

struct Sync {
	enum class Mode : uint8_t {
		SYNCED,
		DIN_SYNC,
	};

	static constexpr auto ModeMax = std::underlying_type_t<Mode>{2};

	Mode mode = Mode::SYNCED;

	void Inc(int32_t inc) {
		const auto temp = std::to_underlying(mode);
		mode = static_cast<Mode>(std::clamp<int32_t>(temp + inc, 0, ModeMax - 1));
	}
	bool Validate() const {
		if (std::to_underlying(mode) >= ModeMax)
			return false;
		return true;
	}
};

namespace Bpm
{
inline constexpr auto min_bpm = 10u;
inline constexpr auto max_bpm = 1200u;

inline constexpr auto max_ticks = BpmToTicks(min_bpm);
inline constexpr auto absolute_max_ticks = INT16_MAX; // ~7.32 bpm
inline constexpr auto min_ticks = BpmToTicks(max_bpm);
inline constexpr auto absolute_min_ticks = 1;

struct Data {
	static constexpr auto original_sample_rate_hz = Legacy::V1_0::Model::sample_rate_hz;

	int16_t bpm_in_ticks = static_cast<int16_t>((60.f * original_sample_rate_hz) / 120.f); // legacy 4kHz format; PostLoad converts to current 3kHz ticks

	void PostLoad() {
		const auto bpm = (60.f * original_sample_rate_hz) / bpm_in_ticks;
		const auto temp = BpmToTicks(bpm);
		bpm_in_ticks = std::clamp<uint32_t>(temp, absolute_min_ticks, absolute_max_ticks);
	}

	void PreSave() {
		const auto bpm = TicksToBpm(bpm_in_ticks);
		const auto temp = (60.f * original_sample_rate_hz) / bpm;
		bpm_in_ticks = std::clamp<uint32_t>(temp, absolute_min_ticks, absolute_max_ticks);
	}

	bool Validate() const {
		return bpm_in_ticks >= absolute_min_ticks;
	}
};

class Interface {
	int16_t &bpm_in_ticks;
	uint32_t prev_tap_time;

	uint32_t cnt;
	uint32_t peek_cnt;

	bool     did_trig  = false;
	uint8_t  tap_count = 0; // consecutive taps; BPM only updates on 3rd+ tap

public:
	bool external;
	bool pause;

	Interface(Data &data)
		: bpm_in_ticks{data.bpm_in_ticks} {
	}

	void Trig() {
		// this wont ever be true unless there is a hardware problem..
		// leaving commented so I don't re-write it and then re-realize it wont be necessary
		// if (!external) {
		//       return;
		// }

		did_trig = !pause;
		cnt = 0;
		peek_cnt = 0;

		UpdateBpm();
	}

	float GetBpm() const {
		return Clock::TicksToBpm(bpm_in_ticks);
	}
	uint32_t GetBpmInTicks() const {
		return static_cast<uint32_t>(bpm_in_ticks);
	}
	void SetBpmInTicks(uint32_t ticks) {
		bpm_in_ticks = static_cast<int16_t>(ticks);
	}

	bool Update() {
		const uint32_t bpm = bpm_in_ticks;

		peek_cnt++;
		if (peek_cnt >= bpm) {
			peek_cnt = 0;
		}

		cnt += !pause;

		if (external) {
			if (did_trig) {
				did_trig = false;
				return true;
			}
		} else {
			if (cnt >= bpm) {
				cnt = 0;
				peek_cnt = 0;
				return true;
			}
		}

		return false;
	}

	void Inc(int32_t inc, bool fine) {
		auto temp = static_cast<uint32_t>(TicksToBpm(bpm_in_ticks));
		inc = fine ? inc : inc * 10;
		temp = std::clamp<int32_t>(temp + inc, min_bpm, max_bpm);
		bpm_in_ticks = BpmToTicks(temp);
	}

	void Tap() {
		if (external) {
			return;
		}
		const auto now = Controls::TimeNow();
		// Reset streak if gap since last tap exceeds the slowest valid BPM period
		if (now - prev_tap_time > static_cast<uint32_t>(absolute_max_ticks))
			tap_count = 0;
		tap_count = static_cast<uint8_t>(std::min<uint32_t>(tap_count + 1u, 255u));
		if (tap_count >= 3)
			UpdateBpm();
		else
			prev_tap_time = now; // keep prev_tap_time fresh so 3rd tap measures correctly
	}

	float GetPhase() const {
		const auto tc = static_cast<float>(std::clamp<int32_t>(cnt, 0, bpm_in_ticks - 1));
		return tc / bpm_in_ticks;
	}
	float PeekPhase() const {
		return static_cast<float>(peek_cnt) / bpm_in_ticks;
	}
	void Reset() {
		cnt = 0;
	}
	void ResetPeek() {
		peek_cnt = 0;
	}

private:
	void UpdateBpm() {
		const auto time_now = Controls::TimeNow();
		bpm_in_ticks = std::clamp<uint32_t>(time_now - prev_tap_time, absolute_min_ticks, absolute_max_ticks);
		prev_tap_time = time_now;
	}
};

} // namespace Bpm

// Musical division tables for per-channel clock division.
// Both tables use the same raw values so the same encoder position = the same division ratio
// relative to each mode's base clock.  The base clocks differ (CatSeq = quarter note,
// VoltSeq = 16th note), so absolute musical speed differs between modes at the same position —
// but the ratio behaviour (enc 0 = full speed, enc 1 = half speed, …) is identical.
//
// Index  Value  Ratio
//   0      1    ÷1  (full speed)
//   1      2    ÷2
//   2      3    ÷3
//   3      4    ÷4
//   4      6    ÷6
//   5      8    ÷8
//   6     12    ÷12
//   7     16    ÷16
namespace DivisionTables
{
inline constexpr std::array<uint8_t, 8> CatSeq  = {1,  2,  3,  4,  6,  8,  12, 16};
inline constexpr std::array<uint8_t, 8> VoltSeq = {1,  2,  3,  4,  6,  8,  12, 16};
} // namespace DivisionTables

class Divider {
	uint32_t counter = 0;

public:
	class type {
		static constexpr auto min = 0u;
		static constexpr auto max = 255u;
		uint8_t v = 0;

	public:
		void Inc(int32_t inc) {
			v = std::clamp<int32_t>(v + inc, min, max);
		}
		// Set to a specific Read() value (e.g. from a division table entry).
		void Set(uint32_t read_value) {
			v = static_cast<uint8_t>(std::clamp<uint32_t>(read_value - 1, min, max));
		}
		uint32_t Read() const {
			return v + 1;
		}
		bool Validate() const {
			// uses entire integer range
			return true;
		}
	};

	// Return the index of the first table entry >= read_value (snaps up to valid entry).
	// Clamps to the last index if read_value exceeds the entire table.
	template<size_t N>
	static uint8_t IndexOf(const std::array<uint8_t, N> &table, uint32_t read_value) {
		for (auto i = 0u; i < N; i++)
			if (table[i] >= read_value) return static_cast<uint8_t>(i);
		return static_cast<uint8_t>(N - 1);
	}

	// Step a divider through a named division table by inc (+1 or -1), clamped at both ends.
	template<size_t N>
	static void Step(type &div, const std::array<uint8_t, N> &table, int32_t inc) {
		const auto idx     = IndexOf(table, div.Read());
		const auto new_idx = std::clamp<int32_t>(static_cast<int32_t>(idx) + inc, 0, static_cast<int32_t>(N) - 1);
		div.Set(table[static_cast<size_t>(new_idx)]);
	}
	bool Update(type div) {
		counter += 1;
		if (counter >= div.Read()) {
			counter = 0;
			return true;
		}
		return false;
	}
	float GetPhase(type div) const {
		return static_cast<float>(counter) / div.Read();
	}
	float GetPhase(type div, float phase) const {
		return (phase / div.Read()) + GetPhase(div);
	}
	void Reset() {
		counter = 0;
	}
	// Prime the counter so the next Update() fires immediately regardless of division.
	// Used when resetting dividers at play/reset so all channels fire on the first sub-clock.
	void ResetToReady(uint32_t div_read) {
		counter = div_read > 0 ? div_read - 1 : 0;
	}
};

} // namespace Catalyst2::Clock
