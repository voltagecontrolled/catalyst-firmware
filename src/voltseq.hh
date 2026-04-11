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
	// Per-step glide amount: 0 = no glide, 1–255 maps linearly to 0–2 s portamento time.
	// CV channels only; Gate/Trigger ignore this field.
	std::array<uint8_t, 64> glide{};
};

struct Data {
	// Increment current_tag whenever the struct layout changes (fields added/removed/reordered).
	// validate() checks this tag so WearLevel rejects stale or incompatible flash data gracefully.
	static constexpr uint32_t current_tag     = 7u;
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
	bool                                           play_stop_reset    = false; // true = Stop also resets all channels to step 1
	uint8_t                                        reset_leader_ch    = 0xFF; // 0xFF=off; reset all when this channel wraps
	uint8_t                                        _reserved          = 0;
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
		for (const auto &ch : channel) {
			if (!ch.range.Validate())
				return false;
			if (!ch.scale.Validate())
				return false;
			if (ch.length < 1)
				return false;
		}
		if (reset_leader_ch != 0xFF && reset_leader_ch >= Model::NumChans)
			return false;
		return true;
	}
};

// Clock engine — drives master tick, per-channel dividers, and inter-tick interval tracking.
//
// Step resolution: channels fire at 16th-note rate (bpm_in_ticks / 4 per step at div=1).
// The master BPM clock (bpm.Update()) fires at quarter-note rate and is used for:
//   - BPM display and tap tempo calibration
//   - Beat-repeat period calculation (UpdateOrbit)
//   - Master-reset counter and arp tick
// Channel step advances are driven by a 16th-note sub-clock: sub_counter_ counts up and
// fires channel dividers every (bpm_in_ticks / 4) ticks regardless of the master tick.
// Channel divisions then further sub-divide (div=1 → 16th, div=2 → 8th, div=4 → quarter, …).
// StepPeriodTicks() = bpm_in_ticks × div / 4, which matches the channel fire period exactly.
class ClockEngine {
	static constexpr uint32_t NumIntervalSamples = 4;

	std::array<uint32_t, NumIntervalSamples> tick_intervals{};
	uint32_t                                 interval_idx   = 0;
	uint32_t                                 last_tick_time = 0;

	std::array<Clock::Divider, Model::NumChans> dividers{};
	std::array<bool, Model::NumChans>           channel_fired_{};

	// 16th-note sub-clock: counts up; resets and fires channel dividers every sub_period ticks.
	uint32_t sub_counter_  = 0;
	bool     step_ticked_  = false; // true the tick the 16th-note sub-clock (or ext pulse) fires

public:
	Clock::Bpm::Interface bpm;

	ClockEngine(Clock::Bpm::Data &data)
		: bpm{data} {
	}

	// Call on rising edge of Clock In jack.
	// External clock runs at 16 PPQN (1 pulse per 16th note). bpm.Trig() sets
	// bpm_in_ticks to the inter-pulse interval (a 16th note period). Scale by 4
	// so bpm_in_ticks represents a quarter note — matching the internal clock's
	// expectation — and the BPM display / tap tempo handoff are correct.
	void ExternalClockTick() {
		const auto now       = Controls::TimeNow();
		tick_intervals[interval_idx % NumIntervalSamples] = now - last_tick_time;
		last_tick_time                                     = now;
		interval_idx++;
		bpm.Trig();
		// Re-scale: bpm.Trig() stored the 16th-note interval; multiply by 4 for quarter-note.
		const auto sixteenth = bpm.GetBpmInTicks();
		bpm.SetBpmInTicks(std::clamp<uint32_t>(sixteenth * 4u,
		    Clock::Bpm::absolute_min_ticks, Clock::Bpm::absolute_max_ticks));
	}

	// Returns the Controls::TimeNow() timestamp of the last ExternalClockTick() call.
	// Used by ResetExternal() to decide whether a clock fired recently enough to have
	// already advanced to step 0 (i.e., primed should stay true).
	uint32_t GetLastExternalClockTime() const {
		return last_tick_time;
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

	// Advance clock and per-channel dividers.  Returns true if the master (quarter-note) clock ticked.
	//
	// Internal clock: channels fire at 16th-note rate via sub_counter_ (bpm_in_ticks/4 per step).
	// External clock: sub_counter_ is bypassed; channels fire directly on each incoming pulse.
	//   bpm_in_ticks under external clock is the inter-pulse interval, NOT a quarter note, so
	//   dividing it further by 4 would fire steps at 1/64th note rate — bypassing is essential.
	bool Update(const std::array<ChannelSettings, Model::NumChans> &channels) {
		const bool master_ticked = bpm.Update();
		channel_fired_.fill(false);
		step_ticked_ = false;

		if (bpm.external) {
			// External clock: each incoming pulse fires channel dividers once.
			// sub_counter_ is held at 0 so the internal sub-clock is clean on return to internal.
			sub_counter_ = 0;
			if (master_ticked) {
				step_ticked_ = true;
				for (auto ch = 0u; ch < Model::NumChans; ch++)
					channel_fired_[ch] = dividers[ch].Update(channels[ch].division);
			}
		} else {
			// Internal clock: 16th-note sub-clock drives step advances.
			// sub_counter_ runs freely — no re-sync to the master beat.
			// Re-syncing caused audible hiccups when the quarter-note master fired just as a step
			// was about to advance, delaying it by up to a full sub_period.  Integer rounding of
			// bpm_in_ticks/4 causes at most 1 tick of drift per beat (< 0.1% at any practical BPM),
			// which is inaudible and negligible compared to tap-tempo imprecision.
			const uint32_t sub_period = std::max<uint32_t>(1u, bpm.GetBpmInTicks() / 4u);
			sub_counter_++;
			if (sub_counter_ >= sub_period) {
				sub_counter_ = 0;
				step_ticked_ = true;
				for (auto ch = 0u; ch < Model::NumChans; ch++)
					channel_fired_[ch] = dividers[ch].Update(channels[ch].division);
			}
		}

		return master_ticked;
	}

	bool StepTicked() const { return step_ticked_; }

	bool ChannelFired(uint8_t ch) const {
		return channel_fired_[ch];
	}

	void ResetDividers() {
		for (auto &d : dividers)
			d.Reset();
		channel_fired_.fill(false);
		sub_counter_ = 0;
	}

	// Prime sub_counter_ so the next Update() tick fires channels immediately.
	// Safe to call in external mode — sub_counter_ is overwritten each tick anyway.
	void SyncStepClock() {
		const uint32_t sub_period = std::max<uint32_t>(1u, bpm.GetBpmInTicks() / 4u);
		sub_counter_ = sub_period;
	}
};

// Per-channel gate output state (Gate channel type)
struct GateState {
	bool     high             = false;
	bool     pending          = false;  // awaiting fire_tick before turning on
	uint32_t fire_tick        = 0;      // tick when gate turns on (start of current pulse)
	uint32_t off_tick         = 0;      // tick when gate turns off (end of current pulse)
	// Gate ratchet: when ratchet_rem > 0, additional pulses fire after the current one
	uint8_t  ratchet_rem      = 0;      // additional pulses remaining
	uint32_t ratchet_interval = 0;      // ticks between ratchet pulse starts
	uint32_t ratchet_dur      = 0;      // duration of each ratchet pulse in ticks
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
	// primed[ch]: false until the first OnChannelFired after Reset().  When false, the advance
	// is skipped so the first step output after Reset is step 0, not step 1.
	std::array<bool,    Model::NumChans>      primed{};

	// Step-lock + arpeggiation — global step indices (0..63)
	uint8_t                                   held_count_ = 0;
	std::array<uint8_t, Model::NumChans>      held_steps_{};   // sorted ascending global step indices
	uint8_t                                   arp_index_  = 0;

	std::array<GateState,    Model::NumChans> gate_state{};
	std::array<RatchetState, Model::NumChans> ratchet_state{};


	// Per-channel wrap flag: set true by AdvanceShadow when a channel's sequence wraps to step 0.
	// Used by the reset-leader check in OnChannelFired.  Cleared at the start of each AdvanceShadow call.
	std::array<bool, Model::NumChans>         just_wrapped_{};

	// Orbit / beat-repeat state (driven by slider performance page)
	bool                                      orbit_active_          = false;
	uint8_t                                   orbit_pos_             = 0;
	bool                                      orbit_ping_dir_        = true;
	std::array<uint8_t, Model::NumChans>      orbit_step_{};
	std::array<uint8_t, Model::NumChans>      orbit_step_prev_{};
	uint32_t                                  beat_repeat_countdown_   = 0;
	float                                     beat_repeat_phase_       = 0.f;
	uint32_t                                  beat_repeat_safe_period_ = 0u;

	// ---- Orbit engine ----

	// Advance orbit_pos and compute per-channel orbit_step[] each tick.
	// Returns true when the orbit position advanced this tick (used by UpdateClock to drive
	// gate/trigger fires at the subdivision rate during beat repeat).
	// Called from UpdateClock() after the normal clock advance.
	bool UpdateOrbit(bool master_ticked) {
		const auto perf_mode = shared.data.slider_perf_mode;
		if (perf_mode == 0) {
			orbit_active_          = false;
			beat_repeat_countdown_ = 0;
			beat_repeat_phase_     = 0.f;
			return false;
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
				beat_repeat_safe_period_ = safe_period;

				if (beat_repeat_countdown_ == 0) {
					const bool first_entry = shared.beat_repeat_snap_pending;
					const bool should_fire = !first_entry || !shared.data.quantized_scrub || master_ticked;
					if (should_fire) {
						beat_repeat_phase_ = 0.f;
						if (first_entry) {
							// Snap orbit_center to the current playhead of the longest channel
							uint8_t ref_len = 1u;
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
			// Reference length = longest active channel (used for granular window scaling)
			uint8_t ref_len = 1u;
			for (auto ch = 0u; ch < Model::NumChans; ch++)
				ref_len = std::max(ref_len, data.channel[ch].length);
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
		return orbit_should_adv;
	}

	// ---- internal helpers ----

	// Global step index 0..63 → page and step-within-page
	static constexpr uint8_t PageOf(uint8_t step)    { return step / Model::NumChans; }
	static constexpr uint8_t StepInPage(uint8_t step) { return step % Model::NumChans; }

	// Advance shadow playhead for channel ch per its direction setting.
	// Sets just_wrapped_[ch] = true when the sequence crosses its boundary (Forward: step → 0;
	// Reverse: step → len-1).  PingPong and Random do not set just_wrapped_ (no clear boundary).
	void AdvanceShadow(uint8_t ch) {
		just_wrapped_[ch] = false;
		const uint8_t len = data.channel[ch].length;
		switch (data.channel[ch].direction) {
		case Direction::Forward:
			just_wrapped_[ch] = (shadow[ch] + 1u >= len);
			shadow[ch] = (shadow[ch] + 1) % len;
			break;
		case Direction::Reverse:
			just_wrapped_[ch] = (shadow[ch] == 0);
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

	// Compute step period for channel ch in main-loop ticks.
	// bpm_in_ticks = one quarter note at the current BPM (set by tap or encoder).
	// Dividing by 4 makes each step a 16th note: tap the beat, steps run at 16th-note resolution.
	uint32_t StepPeriodTicks(uint8_t ch) const {
		return (data.bpm.bpm_in_ticks * data.channel[ch].division.Read()) / 4;
	}

	// Set up gate output for channel ch when its step fires.
	// Gate step value encoding: high byte = gate length (0=off, 1–255 ≈ 0.4%–100%);
	//                           low byte  = ratchet count (0 or 1 = single gate, 2–8 = N pulses per step).
	// period_override: if non-zero, use this as the step period instead of StepPeriodTicks(ch).
	//                  Used by FireOrbitStep during beat repeat to match the subdivision rate.
	void FireGate(uint8_t ch, uint8_t step_idx, uint32_t period_override = 0u) {
		const auto &cs    = data.channel[ch];
		const auto  raw   = data.steps[PageOf(step_idx)][StepInPage(step_idx)][ch];
		const auto gate_len_byte = static_cast<uint8_t>(raw >> 8);
		if (gate_len_byte == 0) {
			gate_state[ch] = {};
			return;
		}
		const float frac = static_cast<float>(gate_len_byte) / 255.f;
		const auto  ratchet_byte  = static_cast<uint8_t>(raw & 0xFF);
		const uint8_t count       = (ratchet_byte >= 2u) ? ratchet_byte : 1u;
		const uint32_t delay      = Clock::MsToTicks(cs.output_delay_ms);
		const uint32_t now        = Controls::TimeNow();
		const uint32_t period     = period_override > 0u ? period_override : StepPeriodTicks(ch);
		const uint32_t interval   = (count > 1u && period > 0u) ? (period / count) : period;
		const uint32_t dur        = static_cast<uint32_t>(frac * interval);
		auto &gs = gate_state[ch];
		gs.pending          = delay > 0;
		gs.high             = delay == 0;
		gs.fire_tick        = now + delay;
		gs.off_tick         = now + delay + (dur > 0u ? dur : 1u);
		gs.ratchet_rem      = count > 1u ? (count - 1u) : 0u;
		gs.ratchet_interval = interval;
		gs.ratchet_dur      = dur > 0u ? dur : 1u;
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

	// Set up ratchet/repeat output for channel ch when its step fires.
	// period_override: if non-zero, use this as the step period instead of StepPeriodTicks(ch).
	//                  Used by FireOrbitStep during beat repeat to match the subdivision rate.
	void FireTrigger(uint8_t ch, uint8_t step_idx, uint32_t period_override = 0u) {
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
			const uint32_t period    = period_override > 0u ? period_override : StepPeriodTicks(ch);
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
			rs.interval_ticks   = period_override > 0u ? period_override : StepPeriodTicks(ch);
			rs.pulse_high       = false;
			rs.pulse_off_tick   = 0;
		}
		(void)pulse_ticks; // used in UpdateTriggers()
	}

	// Fire the gate/trigger for channel ch at orbit_step_[ch], using beat_repeat_safe_period_
	// for gate duration and ratchet interval timing instead of StepPeriodTicks(ch).
	// Only called by UpdateClock when the orbit advances during beat repeat.
	void FireOrbitStep(uint8_t ch) {
		const auto step = orbit_step_[ch];
		if (data.channel[ch].type == ChannelType::Gate)
			FireGate(ch, step, beat_repeat_safe_period_);
		else if (data.channel[ch].type == ChannelType::Trigger)
			FireTrigger(ch, step, beat_repeat_safe_period_);
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
		// Skip the advance on the very first fire after Reset() so that step 0 plays first.
		// On all subsequent fires, advance normally.
		if (primed[ch]) {
			AdvanceShadow(ch);
		} else {
			primed[ch] = true;
		}
		if (!AnyStepHeld())
			playhead[ch] = shadow[ch];

		// Reset-leader: when the designated channel wraps its sequence, reset all channels.
		// Uses ResetExternal() so step 0 fires immediately and primed stays true — no priming pause.
		if (just_wrapped_[ch] && data.reset_leader_ch == ch) {
			ResetExternal();
			return;
		}

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
		// If starting from a reset state (primed=false), fire step 0 immediately so the first
		// clock advances to step 1 rather than wasting a clock on the priming pause.
		if (!primed[0]) {
			primed.fill(true);
			for (auto ch = 0u; ch < Model::NumChans; ch++) {
				const auto step = GetOutputStep(ch);
				if (data.channel[ch].type == ChannelType::Gate)
					FireGate(ch, step);
				else if (data.channel[ch].type == ChannelType::Trigger)
					FireTrigger(ch, step);
			}
		}
		clock.SyncStepClock();
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
		primed.fill(false);
		just_wrapped_.fill(false);
		held_count_           = 0;
		arp_index_            = 0;

		gate_state    = {};
		ratchet_state = {};
		clock.ResetDividers();
		clock.SyncStepClock();
	}

	// External reset: rewind all playheads to step 0 and fire step 0 immediately.
	// Unlike Reset() (used for manual play/stop), this sets primed=true so the next
	// clock tick advances to step 1 — matching CatSeq's behavior where the phaser
	// always advances on each clock with no "priming" pause.
	void ResetExternal() {
		playhead.fill(0);
		shadow.fill(0);
		pingpong_dir.fill(1);
		primed.fill(true);
		just_wrapped_.fill(false);
		held_count_           = 0;
		arp_index_            = 0;

		gate_state    = {};
		ratchet_state = {};
		clock.ResetDividers();
		clock.SyncStepClock();

		// Fire step 0 immediately so it produces output from this tick.
		for (auto ch = 0u; ch < Model::NumChans; ch++) {
			const auto step = GetOutputStep(ch);
			if (data.channel[ch].type == ChannelType::Gate)
				FireGate(ch, step);
			else if (data.channel[ch].type == ChannelType::Trigger)
				FireTrigger(ch, step);
		}
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
	// When a step is held, channels shorter than the held position wrap via modulo so they
	// output their equivalent position rather than reading out-of-range (zeroed) grid data.
	uint8_t GetOutputStep(uint8_t ch) const {
		if (held_count_ > 0) {
			const auto raw = held_steps_[arp_index_ % held_count_];
			return static_cast<uint8_t>(raw % data.channel[ch].length);
		}
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
		return data.flags[ch].glide[global_step] > 0;
	}

	// Returns glide time in seconds for the given step (0 = no glide, max = 2 s)
	float GlideAmount(uint8_t ch, uint8_t global_step) const {
		return (data.flags[ch].glide[global_step] / 255.f) * 2.0f;
	}

	// Adjust per-step glide amount by dir, clamped to 0–255
	void AdjustGlideAmount(uint8_t ch, uint8_t global_step, int32_t dir) {
		auto &val = data.flags[ch].glide[global_step];
		val = static_cast<uint8_t>(std::clamp<int32_t>(val + dir, 0, 255));
	}

	// Adjust glide amount for all steps within channel length simultaneously
	void OffsetAllGlideAmounts(uint8_t ch, int32_t dir) {
		const uint8_t len = data.channel[ch].length;
		for (auto s = 0u; s < len; s++)
			AdjustGlideAmount(ch, static_cast<uint8_t>(s), dir);
	}

	// ---- Output state accessors (called from App::Update()) ----

	bool IsGateHigh(uint8_t ch)  const { return gate_state[ch].high; }
	// True while a trigger channel is holding a step repeat (playhead frozen, extra pulses pending).
	bool IsRepeating(uint8_t ch) const { return ratchet_state[ch].repeat_remaining > 0; }
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
				// Fire next ratchet pulse if any remain
				if (gs.ratchet_rem > 0) {
					gs.ratchet_rem--;
					const uint32_t next_start = gs.fire_tick + gs.ratchet_interval;
					gs.fire_tick = next_start;
					gs.off_tick  = next_start + gs.ratchet_dur;
					if (now >= next_start) {
						gs.high    = true;
						gs.pending = false;
					} else {
						gs.pending = true;
					}
				}
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

		if (clock.StepTicked() && AnyStepHeld())
			TickArp();

		// During beat repeat (perf_mode 2 or 3), orbit-following channels are driven by the
		// orbit advance timer rather than their own clock dividers.  Suppress OnChannelFired for
		// those channels here; FireOrbitStep fires them below when the orbit advances.
		const bool beat_repeat_active = (shared.data.slider_perf_mode >= 2);
		if (playing) {
			for (auto ch = 0u; ch < Model::NumChans; ch++) {
				if (clock.ChannelFired(ch)) {
					if (beat_repeat_active && OrbitActiveForChannel(ch)) {
						// Keep shadow advancing so playhead re-syncs correctly when beat repeat releases.
						if (primed[ch])
							AdvanceShadow(ch);
						else
							primed[ch] = true;
						continue;
					}
					OnChannelFired(ch);
				}
			}
		}
		UpdateGates();
		UpdateTriggers();
		const bool orbit_advanced = UpdateOrbit(master_ticked);
		if (playing && orbit_advanced && beat_repeat_active) {
			for (auto ch = 0u; ch < Model::NumChans; ch++) {
				if (OrbitActiveForChannel(ch))
					FireOrbitStep(ch);
			}
		}
		return master_ticked;
	}

	bool ChannelFired(uint8_t ch) const {
		return clock.ChannelFired(ch);
	}
};

} // namespace Catalyst2::VoltSeq
