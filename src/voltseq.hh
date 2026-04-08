#pragma once

#include "channel.hh"
#include "channelmode.hh"
#include "clock.hh"
#include "conf/model.hh"
#include "range.hh"
#include "recorder.hh"
#include "shared.hh"
#include <algorithm>
#include <array>
#include <cstdint>

namespace Catalyst2::VoltSeq
{

using StepValue = uint16_t;

// 8 pages × 8 steps × 8 channels = 512 × uint16_t = 1024 bytes
using StepGrid = std::array<                                       // [page 0..7]
                   std::array<                                     // [step 0..7 within page]
                     std::array<StepValue, Model::NumChans>,       // [channel 0..7]
                   Model::NumChans>,
                 Model::NumChans>;

enum class Direction : uint8_t { Forward, Reverse, PingPong, Random };

// Channel output type.  Stored separately from Channel::Mode (the quantizer scale) so
// extending Channel::Mode's index space is not required for basic output routing.
enum class ChannelType : uint8_t { CV, Gate, Trigger };

struct ChannelSettings {
	ChannelType          type            = ChannelType::CV;          // CV, Gate, or Trigger
	uint8_t              length          = 8;                        // 1..64 steps
	Clock::Divider::type division        = {};                       // clock division (1–256)
	Channel::Cv::Range   range           = [] { Channel::Cv::Range r; r.Inc(2); return r; }(); // default: −5V to +5V
	Channel::Mode        scale           = {};                       // quantizer scale (CV only)
	Direction            direction       = Direction::Forward;
	uint8_t              pulse_width_ms  = 10;                       // trigger pulse width ms (Trigger only)
	uint8_t              output_delay_ms = 0;                        // output delay ms (Gate/Trigger)
	float                random_amount   = 0.f;                      // 0 = deterministic, 1 = fully random
	float                glide_time      = 0.f;                      // seconds; 0 = disabled (CV only)
};

struct StepFlags {
	uint64_t glide = 0; // bit N = glide flag for step N (steps 0..63); CV channels only
};

struct Data {
	// Increment current_tag whenever the struct layout changes (fields added/removed/reordered).
	// validate() checks this tag so WearLevel rejects stale or incompatible flash data gracefully.
	static constexpr uint32_t current_tag     = 3u;
	uint32_t                  SettingsVersionTag = current_tag;

	static StepGrid DefaultSteps() {
		StepGrid g{};
		for (auto &page : g)
			for (auto &row : page)
				row.fill(32768); // center = 0V for default bipolar −5V to +5V range
		return g;
	}

	StepGrid                                       steps = DefaultSteps();
	std::array<ChannelSettings, Model::NumChans>   channel{};
	std::array<StepFlags, Model::NumChans>         flags{};
	Direction                                      default_direction = Direction::Forward;
	Channel::Cv::Range                             default_range     = [] { Channel::Cv::Range r; r.Inc(2); return r; }();
	uint8_t                                        default_length    = 8;
	uint8_t                                        _reserved         = 0;
	Clock::Bpm::Data                               bpm{};             // internal BPM
	Macro::Recorder::Data                          recorder{};        // slider sample buffer (reserved)

	void PreSave() {
		bpm.PreSave();
	}
	void PostLoad() {
		bpm.PostLoad();
	}

	bool validate() const {
		if (SettingsVersionTag != current_tag)
			return false;
		if (!bpm.Validate())
			return false;
		if (default_length < 1)
			return false;
		for (const auto &ch : channel) {
			if (!ch.range.Validate())
				return false;
			if (!ch.scale.Validate())
				return false;
			if (ch.length < 1)
				return false;
		}
		if (!default_range.Validate())
			return false;
		return true;
	}
};

// Clock engine — drives master tick, per-channel dividers, and inter-tick interval tracking.
class ClockEngine {
	static constexpr uint32_t NumIntervalSamples = 4;

	std::array<uint32_t, NumIntervalSamples> tick_intervals{};
	uint32_t                                 interval_idx   = 0;
	uint32_t                                 last_tick_time = 0;

	std::array<Clock::Divider, Model::NumChans> dividers{};
	std::array<bool, Model::NumChans>           channel_fired_{};

public:
	Clock::Bpm::Interface bpm;

	ClockEngine(Clock::Bpm::Data &data)
		: bpm{data} {
	}

	// Call on rising edge of Clock In jack
	void ExternalClockTick() {
		const auto now       = Controls::TimeNow();
		tick_intervals[interval_idx % NumIntervalSamples] = now - last_tick_time;
		last_tick_time                                     = now;
		interval_idx++;
		bpm.Trig();
	}

	// Call on rising edge of Tap Tempo button
	void TapTempo() {
		bpm.Tap();
	}

	// Average inter-tick interval in main-loop ticks (3kHz). Meaningful under external clock
	// after NumIntervalSamples ticks have been recorded. Used by beat repeat for period calculation.
	float AverageIntervalTicks() const {
		if (interval_idx < NumIntervalSamples)
			return static_cast<float>(bpm.GetBpm());
		uint32_t sum = 0;
		for (auto v : tick_intervals)
			sum += v;
		return static_cast<float>(sum) / NumIntervalSamples;
	}

	// Advance clock and per-channel dividers.  Returns true if the master clock ticked.
	bool Update(const std::array<ChannelSettings, Model::NumChans> &channels) {
		const bool master_ticked = bpm.Update();
		channel_fired_.fill(false);
		if (master_ticked) {
			for (auto ch = 0u; ch < Model::NumChans; ch++) {
				channel_fired_[ch] = dividers[ch].Update(channels[ch].division);
			}
		}
		return master_ticked;
	}

	bool ChannelFired(uint8_t ch) const {
		return channel_fired_[ch];
	}

	void ResetDividers() {
		for (auto &d : dividers)
			d.Reset();
		channel_fired_.fill(false);
	}
};

// Per-channel gate output state (Gate channel type)
struct GateState {
	bool     high      = false;
	bool     pending   = false;  // awaiting output_delay_ms before firing
	uint32_t fire_tick = 0;      // Controls::TimeNow() when to turn on
	uint32_t off_tick  = 0;      // Controls::TimeNow() when to turn off
};

// Per-channel ratchet/repeat state (Trigger channel type)
struct RatchetState {
	uint8_t  total_count      = 0;   // ratchet pulses to fire this step (0 = rest)
	uint8_t  fired_count      = 0;   // pulses fired so far
	uint8_t  repeat_remaining = 0;   // additional step repeats remaining
	uint32_t next_fire_tick   = 0;   // when to fire the next pulse
	uint32_t interval_ticks   = 1;   // ticks between ratchet pulses
	bool     pulse_high       = false;
	uint32_t pulse_off_tick   = 0;
};

class Interface {
	Data &data;

	// Playback runtime state (not persisted)
	bool                                      playing = false;
	std::array<uint8_t, Model::NumChans>      playhead{};
	std::array<uint8_t, Model::NumChans>      shadow{};
	std::array<int8_t,  Model::NumChans>      pingpong_dir{};

	// Step-lock + arpeggiation — global step indices (0..63)
	uint8_t                                   held_count_ = 0;
	std::array<uint8_t, Model::NumChans>      held_steps_{};   // sorted ascending global step indices
	uint8_t                                   arp_index_  = 0;

	std::array<GateState,    Model::NumChans> gate_state{};
	std::array<RatchetState, Model::NumChans> ratchet_state{};

	// Orbit / beat-repeat state (driven by slider performance page)
	bool                                      orbit_active_          = false;
	uint8_t                                   orbit_pos_             = 0;
	bool                                      orbit_ping_dir_        = true;
	std::array<uint8_t, Model::NumChans>      orbit_step_{};
	std::array<uint8_t, Model::NumChans>      orbit_step_prev_{};
	uint32_t                                  beat_repeat_countdown_ = 0;
	float                                     beat_repeat_phase_     = 0.f;

	// ---- Orbit engine ----

	// Advance orbit_pos and compute per-channel orbit_step[] each tick.
	// Called from UpdateClock() after the normal clock advance.
	void UpdateOrbit(bool master_ticked) {
		const auto perf_mode = shared.data.slider_perf_mode;
		if (perf_mode == 0) {
			orbit_active_          = false;
			beat_repeat_countdown_ = 0;
			beat_repeat_phase_     = 0.f;
			return;
		}

		// Beat period in main-loop ticks (3kHz).  Use averaged external interval when clocked
		// externally; fall back to internal BPM otherwise.
		const float bpm = clock.bpm.external
		                      ? (clock.AverageIntervalTicks() > 0.f
		                             ? (60.f * static_cast<float>(Model::sample_rate_hz)) / clock.AverageIntervalTicks()
		                             : clock.bpm.GetBpm())
		                      : clock.bpm.GetBpm();
		const uint32_t beat_period =
		    (bpm > 0.f) ? static_cast<uint32_t>(static_cast<float>(Model::sample_rate_hz) * 60.f / bpm) : 3000u;

		const auto width_pct        = shared.data.orbit_width;
		const auto dir              = shared.data.orbit_direction;
		bool       orbit_should_adv = false;

		if (perf_mode == 1) {
			// Granular: active when orbit width > 0 and slider is off zero
			orbit_active_      = (width_pct > 0) && (shared.orbit_center > 0.f);
			orbit_should_adv   = master_ticked && orbit_active_;
			beat_repeat_phase_ = 0.f;
		} else {
			// Beat repeat (mode 2 = 8 zones with triplets, mode 3 = 4 zones no triplets)
			orbit_active_ = (shared.beat_repeat_committed != 0xFF);
			if (orbit_active_) {
				static constexpr std::array<uint32_t, 8> div_num_8   = {2, 1, 2, 1, 1, 1, 1, 1};
				static constexpr std::array<uint32_t, 8> div_denom_8 = {1, 1, 3, 2, 3, 4, 6, 8};
				static constexpr std::array<uint32_t, 4> div_num_4   = {2, 1, 1, 1};
				static constexpr std::array<uint32_t, 4> div_denom_4 = {1, 1, 2, 4};
				const bool is_cyan = (perf_mode == 3);
				const auto zone    = std::min(shared.beat_repeat_committed,
                                          static_cast<uint8_t>(is_cyan ? 3u : 7u));
				const uint32_t safe_period =
				    is_cyan ? std::max<uint32_t>(1u, beat_period * div_num_4[zone] / div_denom_4[zone])
				            : std::max<uint32_t>(1u, beat_period * div_num_8[zone] / div_denom_8[zone]);

				if (beat_repeat_countdown_ == 0) {
					const bool first_entry = shared.beat_repeat_snap_pending;
					const bool should_fire = !first_entry || !shared.data.quantized_scrub || master_ticked;
					if (should_fire) {
						beat_repeat_phase_ = 0.f;
						if (first_entry) {
							// Snap orbit_center to the current playhead of the longest channel
							uint8_t ref_len = data.default_length > 0 ? data.default_length : 1u;
							uint8_t ref_ph  = playhead[0];
							for (auto ch = 0u; ch < Model::NumChans; ch++) {
								if (data.channel[ch].length > ref_len) {
									ref_len = data.channel[ch].length;
									ref_ph  = playhead[ch];
								}
							}
							shared.orbit_center =
							    static_cast<float>(ref_ph) / static_cast<float>(ref_len);
							shared.beat_repeat_snap_pending = false;
						}
						orbit_should_adv       = true;
						beat_repeat_countdown_ = safe_period;
					}
				} else {
					beat_repeat_phase_ =
					    1.f - static_cast<float>(beat_repeat_countdown_) / static_cast<float>(safe_period);
					beat_repeat_countdown_--;
				}
			} else {
				beat_repeat_countdown_          = 0;
				beat_repeat_phase_              = 0.f;
				shared.beat_repeat_snap_pending = false;
			}
		}

		// Advance orbit_pos based on direction setting
		if (orbit_should_adv) {
			const uint8_t  ref_len    = data.default_length > 0 ? data.default_length : 1u;
			const uint32_t window_ref = (perf_mode == 1)
			                                ? std::max<uint32_t>(1u, static_cast<uint32_t>(width_pct / 100.f * ref_len + 0.5f))
			                                : 1u;
			if (dir == 0) {
				orbit_pos_ = static_cast<uint8_t>((orbit_pos_ + 1) % window_ref);
			} else if (dir == 1) {
				orbit_pos_ = static_cast<uint8_t>((orbit_pos_ + window_ref - 1) % window_ref);
			} else if (dir == 2) {
				// Ping-pong
				if (window_ref <= 1) {
					orbit_pos_ = 0;
				} else if (orbit_ping_dir_) {
					if (orbit_pos_ + 1u >= window_ref) {
						orbit_ping_dir_ = false;
						orbit_pos_      = static_cast<uint8_t>(window_ref - 2);
					} else {
						orbit_pos_++;
					}
				} else {
					if (orbit_pos_ == 0) {
						orbit_ping_dir_ = true;
						orbit_pos_      = 1;
					} else {
						orbit_pos_--;
					}
				}
			} else {
				orbit_pos_ = static_cast<uint8_t>(static_cast<uint32_t>(std::rand()) % window_ref);
			}
		}

		// Compute per-channel orbit_step[] from orbit_pos and orbit_center
		if (orbit_active_) {
			for (auto ch = 0u; ch < Model::NumChans; ch++) {
				orbit_step_prev_[ch] = orbit_step_[ch];
				const auto     len         = data.channel[ch].length;
				const uint32_t window_chan = (perf_mode == 1)
				                                ? std::max<uint32_t>(1u, static_cast<uint32_t>(width_pct / 100.f * len + 0.5f))
				                                : 1u;
				float center_f = shared.orbit_center * static_cast<float>(len);
				if (shared.data.quantized_scrub && len > 1)
					center_f = std::round(center_f);
				const auto center_step = static_cast<uint8_t>(
				    std::clamp<uint32_t>(static_cast<uint32_t>(center_f), 0u, static_cast<uint32_t>(len - 1)));
				const auto pos_in_window = orbit_pos_ % window_chan;
				orbit_step_[ch] = static_cast<uint8_t>((center_step + pos_in_window) % len);
			}
		}
	}

	// ---- internal helpers ----

	// Global step index 0..63 → page and step-within-page
	static constexpr uint8_t PageOf(uint8_t step)    { return step / Model::NumChans; }
	static constexpr uint8_t StepInPage(uint8_t step) { return step % Model::NumChans; }

	// Advance shadow playhead for channel ch per its direction setting
	void AdvanceShadow(uint8_t ch) {
		const uint8_t len = data.channel[ch].length;
		switch (data.channel[ch].direction) {
		case Direction::Forward:
			shadow[ch] = (shadow[ch] + 1) % len;
			break;
		case Direction::Reverse:
			shadow[ch] = shadow[ch] == 0 ? len - 1 : shadow[ch] - 1;
			break;
		case Direction::PingPong: {
			const int8_t dir = pingpong_dir[ch];
			const int16_t next = shadow[ch] + dir;
			if (next < 0) {
				shadow[ch] = 1 < len ? 1 : 0;
				pingpong_dir[ch] = 1;
			} else if (next >= len) {
				shadow[ch] = len > 1 ? len - 2 : 0;
				pingpong_dir[ch] = -1;
			} else {
				shadow[ch] = static_cast<uint8_t>(next);
			}
			break;
		}
		case Direction::Random: {
			// Uniform random, never same step twice in a row
			if (len > 1) {
				uint8_t next;
				do {
					next = static_cast<uint8_t>(std::rand() % len);
				} while (next == shadow[ch]);
				shadow[ch] = next;
			}
			break;
		}
		}
	}

	// Compute step period for channel ch in main-loop ticks
	uint32_t StepPeriodTicks(uint8_t ch) const {
		return data.bpm.bpm_in_ticks * data.channel[ch].division.Read();
	}

	// Set up gate output for channel ch when its step fires
	void FireGate(uint8_t ch, uint8_t step_idx) {
		const auto &cs    = data.channel[ch];
		const auto  raw   = data.steps[PageOf(step_idx)][StepInPage(step_idx)][ch];
		const float frac  = static_cast<float>(raw) / 65535.f;
		if (frac <= 0.f) {
			gate_state[ch] = {};
			return;
		}
		const uint32_t delay = Clock::MsToTicks(cs.output_delay_ms);
		const uint32_t dur   = static_cast<uint32_t>(frac * StepPeriodTicks(ch));
		const uint32_t now   = Controls::TimeNow();
		gate_state[ch].pending   = delay > 0;
		gate_state[ch].high      = delay == 0;
		gate_state[ch].fire_tick = now + delay;
		gate_state[ch].off_tick  = now + delay + (dur > 0 ? dur : 1u);
	}

	// Fire a single trigger pulse on channel ch without modifying ratchet/repeat counters.
	// Used by OnChannelFired when consuming repeat_remaining ticks.
	void FirePulse(uint8_t ch) {
		auto          &rs          = ratchet_state[ch];
		const uint32_t pulse_ticks = Clock::MsToTicks(
		    data.channel[ch].pulse_width_ms > 0 ? data.channel[ch].pulse_width_ms : 1u);
		rs.pulse_high     = true;
		rs.pulse_off_tick = Controls::TimeNow() + pulse_ticks;
	}

	// Set up ratchet/repeat output for channel ch when its step fires
	void FireTrigger(uint8_t ch, uint8_t step_idx) {
		const auto &cs  = data.channel[ch];
		const auto  raw = data.steps[PageOf(step_idx)][StepInPage(step_idx)][ch];
		const auto  val = static_cast<int8_t>(raw & 0xFF);
		if (val == 0) {
			ratchet_state[ch] = {};
			return;
		}
		const uint32_t now         = Controls::TimeNow();
		const uint32_t delay       = Clock::MsToTicks(cs.output_delay_ms);
		const uint32_t pulse_ticks = Clock::MsToTicks(cs.pulse_width_ms > 0 ? cs.pulse_width_ms : 1u);
		auto &rs = ratchet_state[ch];

		if (val > 0) {
			// Ratchet: val pulses subdivided within step period
			const auto count         = static_cast<uint8_t>(val);
			const uint32_t period    = StepPeriodTicks(ch);
			const uint32_t interval  = period > 0 ? period / count : 1u;
			rs.total_count      = count;
			rs.fired_count      = 0;
			rs.repeat_remaining = 0;
			rs.next_fire_tick   = now + delay;
			rs.interval_ticks   = interval;
			rs.pulse_high       = false;
			rs.pulse_off_tick   = 0;
		} else {
			// Repeat: step fires once now, then |val| more times on subsequent channel ticks
			rs.total_count      = 1;
			rs.fired_count      = 0;
			rs.repeat_remaining = static_cast<uint8_t>(-val);
			rs.next_fire_tick   = now + delay;
			rs.interval_ticks   = StepPeriodTicks(ch);
			rs.pulse_high       = false;
			rs.pulse_off_tick   = 0;
		}
		(void)pulse_ticks; // used in UpdateTriggers()
	}

	// Called when channel ch's divider fires and we are playing
	void OnChannelFired(uint8_t ch) {
		if (ratchet_state[ch].repeat_remaining > 0) {
			// Repeat: hold playhead, decrement counter, fire pulse without re-arming repeat.
			// (FireTrigger must NOT be called here — it would reset repeat_remaining, looping forever.)
			ratchet_state[ch].repeat_remaining--;
			const auto step = GetOutputStep(ch);
			if (data.channel[ch].type == ChannelType::Gate)
				FireGate(ch, step);
			else if (data.channel[ch].type == ChannelType::Trigger)
				FirePulse(ch);
			return;
		}
		AdvanceShadow(ch);
		if (!AnyStepHeld())
			playhead[ch] = shadow[ch];
		const auto step = GetOutputStep(ch);
		if (data.channel[ch].type == ChannelType::Gate)
			FireGate(ch, step);
		else if (data.channel[ch].type == ChannelType::Trigger)
			FireTrigger(ch, step);
	}

public:
	Shared::Interface &shared;
	ClockEngine        clock;

	Interface(Data &data, Shared::Interface &shared)
		: data{data}
		, shared{shared}
		, clock{data.bpm} {
		pingpong_dir.fill(1);
	}

	Data       &GetData()       { return data; }
	const Data &GetData() const { return data; }

	// Returns true if orbit is active and channel ch follows it (bit set in scrub_ignore_mask).
	bool OrbitActiveForChannel(uint8_t ch) const {
		return orbit_active_ && ((shared.data.scrub_ignore_mask >> ch) & 1u);
	}
	uint8_t GetOrbitStep(uint8_t ch)     const { return orbit_step_[ch]; }
	uint8_t GetOrbitStepPrev(uint8_t ch) const { return orbit_step_prev_[ch]; }

	void Play() {
		playing = true;
	}
	void Stop() {
		playing = false;
	}
	void Toggle() {
		playing ? Stop() : Play();
	}
	bool IsPlaying() const {
		return playing;
	}

	void Reset() {
		playhead.fill(0);
		shadow.fill(0);
		pingpong_dir.fill(1);
		held_count_ = 0;
		arp_index_  = 0;
		gate_state    = {};
		ratchet_state = {};
		clock.ResetDividers();
	}

	// ---- Step-lock / arpeggiation API (called from UI) ----
	// global_step is the full step index 0..63 (page * 8 + step_in_page)

	void SetStepHeld(uint8_t global_step, bool held) {
		if (held) {
			// Insert into sorted list (dedup)
			for (auto i = 0u; i < held_count_; i++)
				if (held_steps_[i] == global_step) return;
			if (held_count_ < Model::NumChans) {
				held_steps_[held_count_++] = global_step;
				std::sort(held_steps_.begin(), held_steps_.begin() + held_count_);
			}
		} else {
			// Remove from list
			for (auto i = 0u; i < held_count_; i++) {
				if (held_steps_[i] == global_step) {
					for (auto j = i; j + 1 < held_count_; j++)
						held_steps_[j] = held_steps_[j + 1];
					--held_count_;
					break;
				}
			}
			if (held_count_ == 0) {
				// All released: snap real playheads to shadows
				for (auto ch = 0u; ch < Model::NumChans; ch++)
					playhead[ch] = shadow[ch];
				arp_index_ = 0;
			}
		}
	}

	bool AnyStepHeld() const { return held_count_ > 0; }

	// Global step index to use for output this tick (step-lock > orbit > playhead).
	uint8_t GetOutputStep(uint8_t ch) const {
		if (held_count_ > 0) return held_steps_[arp_index_ % held_count_];
		if (OrbitActiveForChannel(ch)) return orbit_step_[ch];
		return playhead[ch];
	}

	// Advance arp index.  Call once per master clock tick from UpdateClock().
	void TickArp() {
		if (held_count_ > 1)
			arp_index_ = (arp_index_ + 1) % held_count_;
	}

	// ---- Step data read/write (called from App and UI) ----

	StepValue GetStepValue(uint8_t ch, uint8_t global_step) const {
		return data.steps[PageOf(global_step)][StepInPage(global_step)][ch];
	}

	void SetStepValue(uint8_t ch, uint8_t global_step, StepValue val) {
		data.steps[PageOf(global_step)][StepInPage(global_step)][ch] = val;
	}

	bool GlideFlag(uint8_t ch, uint8_t global_step) const {
		return (data.flags[ch].glide >> global_step) & 1u;
	}

	void SetGlideFlag(uint8_t ch, uint8_t global_step, bool on) {
		if (on)
			data.flags[ch].glide |=  (uint64_t{1} << global_step);
		else
			data.flags[ch].glide &= ~(uint64_t{1} << global_step);
	}

	// ---- Output state accessors (called from App::Update()) ----

	bool IsGateHigh(uint8_t ch) const { return gate_state[ch].high; }
	bool IsTrigHigh(uint8_t ch) const { return ratchet_state[ch].pulse_high; }

	// ---- Playhead accessors (called from UI for display and recording) ----

	uint8_t GetShadow(uint8_t ch)   const { return shadow[ch]; }
	uint8_t GetPlayhead(uint8_t ch) const { return playhead[ch]; }

	// ---- Tick-level updates (called from VoltSeq::Main::Common()) ----

	// Update gate output timers each main-loop tick
	void UpdateGates() {
		const auto now = Controls::TimeNow();
		for (auto ch = 0u; ch < Model::NumChans; ch++) {
			if (data.channel[ch].type != ChannelType::Gate) continue;
			auto &gs = gate_state[ch];
			if (gs.pending && now >= gs.fire_tick) {
				gs.pending = false;
				gs.high    = true;
			}
			if (gs.high && now >= gs.off_tick) {
				gs.high = false;
			}
		}
	}

	// Update trigger/ratchet pulse timers each main-loop tick
	void UpdateTriggers() {
		const auto now = Controls::TimeNow();
		for (auto ch = 0u; ch < Model::NumChans; ch++) {
			if (data.channel[ch].type != ChannelType::Trigger) continue;
			auto &rs = ratchet_state[ch];
			// Turn off current pulse
			if (rs.pulse_high && now >= rs.pulse_off_tick) {
				rs.pulse_high = false;
			}
			// Fire next ratchet pulse
			if (!rs.pulse_high && rs.fired_count < rs.total_count && now >= rs.next_fire_tick) {
				const uint32_t pulse_ticks = Clock::MsToTicks(data.channel[ch].pulse_width_ms > 0
				                                                   ? data.channel[ch].pulse_width_ms
				                                                   : 1u);
				rs.pulse_high     = true;
				rs.pulse_off_tick = now + pulse_ticks;
				rs.next_fire_tick += rs.interval_ticks;
				rs.fired_count++;
			}
		}
	}

	// Advance clock and per-channel dividers; fire channel step advances when appropriate.
	// Called once per main-loop tick from VoltSeq::Main::Common().
	bool UpdateClock() {
		const bool master_ticked = clock.Update(data.channel);
		if (master_ticked && AnyStepHeld())
			TickArp();
		if (playing) {
			for (auto ch = 0u; ch < Model::NumChans; ch++) {
				if (clock.ChannelFired(ch))
					OnChannelFired(ch);
			}
		}
		UpdateGates();
		UpdateTriggers();
		UpdateOrbit(master_ticked);
		return master_ticked;
	}

	bool ChannelFired(uint8_t ch) const {
		return clock.ChannelFired(ch);
	}
};

} // namespace Catalyst2::VoltSeq
