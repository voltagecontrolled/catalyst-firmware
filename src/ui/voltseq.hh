#pragma once

#include "abstract.hh"
#include "conf/palette.hh"
#include "controls.hh"
#include "helper_functions.hh"
#include "quantizer.hh"
#include "util/countzip.hh"
#include "voltseq.hh"

namespace Catalyst2::Ui::VoltSeq
{

using Catalyst2::VoltSeq::ChannelSettings;
using Catalyst2::VoltSeq::ChannelType;
using Catalyst2::VoltSeq::Direction;
using Catalyst2::VoltSeq::StepValue;

// Encoder step sizes for VoltSeq's 0..65535 StepValue space.
// Channel::Cv::type spans 0..4500 (15V), so VoltSeq units are ~14.6× larger.
inline constexpr uint16_t CvStepCoarse = 350; // ≈ 1 semitone (25 × 14)
inline constexpr uint16_t CvStepFine   = 14;  // ≈ 1 sub-semitone unit

// Gate length step sizes (high byte 0..255 = 0..100% gate)
inline constexpr uint16_t GateStepCoarse = 5;  // ≈ 2% per detent (~50 detents for full range)
inline constexpr uint16_t GateStepFine   = 1;  // ≈ 0.4% per detent

// Chaselight color for the currently playing step in armed mode.
// Dimmed from full_white so the indicator is visible without drowning out the step colors.
inline constexpr Color ChaseWhite = Color(110, 110, 110);

class Main : public Abstract {
	Catalyst2::VoltSeq::Interface &p;

	uint8_t                current_page_  = 0;
	std::optional<uint8_t> armed_ch_      = std::nullopt; // which channel (0-7) is armed
	uint8_t                focused_ch_    = 0;            // channel shown on page-button chaselight

	// Last step index (in-page 0-7) held or touched for display reference
	uint8_t last_touched_step_ = 0;

	// Channel Edit mode state
	bool    channel_edit_active_  = false;
	uint8_t edit_ch_              = 0;    // focused channel (0-7) in Channel Edit
	uint8_t channel_edit_last_enc_ = 0xFF; // last encoder turned (drives feedback display)

	// Glide step editor state
	bool    glide_editor_active_   = false;
	uint8_t glide_editor_ch_       = 0;

	// Bitmask: bit i set if an encoder was turned while step i was held (armed Trigger editing).
	// Used to distinguish tap (toggle) from hold+edit (no toggle).
	uint8_t trigger_step_enc_turned_ = 0;

	// Velocity-based encoder acceleration (bypassed in fine mode).
	EncoderAccel enc_accel_{};

	// Long-press detection for GLIDE + page button entry
	static constexpr uint8_t  kNoLongpressBtn  = 0xFF;
	static constexpr uint32_t kLongpressMs     = 600u;
	Clock::Timer               glide_longpress_timer_{kLongpressMs};
	uint8_t                    glide_longpress_btn_   = kNoLongpressBtn;
	bool                       glide_longpress_shift_ = false;

	// Long-press detection in Channel Edit: hold page button to clear that channel's steps
	Clock::Timer channel_edit_clear_timer_{kLongpressMs};
	uint8_t      channel_edit_clear_btn_ = kNoLongpressBtn;
	// Deadline (Controls::TimeNow) until the length-feedback display is shown in Channel Edit.
	// Starts when encoder 2 (Length) is turned; display reverts to normal after ~600 ms.
	uint32_t     length_display_until_ = 0;

	// ---- Slider Performance Page state ----
	bool     perf_page_active_     = false;
	bool     perf_settings_active_ = false;

	// Lock / pickup
	bool     phase_locked_     = false;
	bool     picking_up_       = false;
	uint16_t locked_raw_       = 0;
	uint32_t lock_toggle_time_ = 0;

	// Orbit entry pickup (beat repeat): slider must leave its entry position before taking over
	bool     orbit_pickup_        = false;
	uint16_t orbit_pickup_slider_ = 0;

	// Fine+Glide hold detection for perf page entry / settings entry
	bool     scrub_hold_pending_ = false;
	uint32_t scrub_hold_start_   = 0;
	bool     initialized_from_shared_ = false;

	// Slider movement tracking (for lock indicator)
	uint16_t last_slider_raw_        = 0;
	uint32_t last_slider_move_time_  = 0;

	// Slider-record movement gate: record only while slider is in motion + brief hold-off.
	uint16_t record_slider_prev_   = 0xFFFF;           // invalid sentinel
	uint32_t record_active_until_  = 0;
	static constexpr int32_t  kRecordMoveThreshold  = 8;
	static constexpr uint32_t kRecordActiveTicks    = Clock::MsToTicks(80);

	static constexpr uint32_t kScrubHoldTicks     = Clock::MsToTicks(1500);
	static constexpr int32_t  kPickupThreshold     = 80;
	static constexpr int32_t  kSliderMoveThreshold = 8;
	static constexpr uint32_t kSliderActiveTicks   = Clock::MsToTicks(400);
	static constexpr uint32_t kToggleFeedbackTicks = Clock::MsToTicks(600);
	static constexpr uint32_t kClearHoldTicks         = Clock::MsToTicks(600);
	static constexpr uint32_t kGlobalSettingsHoldTicks = Clock::MsToTicks(2000);

	// Clear mode: SHIFT+PLAY held > kClearHoldTicks → enter; tap page N = clear ch N; PLAY = clear all
	bool     clear_mode_active_   = false;
	bool     shift_play_pending_  = false;   // armed while we wait for long/short distinction
	uint32_t shift_play_press_t_  = 0;

	// Global settings modal: SHIFT+CHAN held ≥2 s → enter; short SHIFT+CHAN → Channel Edit; Play exits.
	bool     global_settings_active_  = false;
	bool     shift_chan_hold_pending_  = false;
	uint32_t shift_chan_hold_start_    = 0;

	// Beat repeat debounce table (matches sequencer)
	static constexpr std::array<uint32_t, 8> kBeatRepeatDebounce = {
	    Clock::MsToTicks(50),  Clock::MsToTicks(100), Clock::MsToTicks(150), Clock::MsToTicks(250),
	    Clock::MsToTicks(400), Clock::MsToTicks(600), Clock::MsToTicks(900), Clock::MsToTicks(1500),
	};

	// ---- Lock helpers ----

	void DoLockToggle() {
		if (!phase_locked_ && !picking_up_) {
			phase_locked_ = true;
			locked_raw_   = c.ReadSlider();
		} else {
			phase_locked_ = false;
			picking_up_   = true;
		}
		lock_toggle_time_              = Controls::TimeNow();
		p.shared.data.phase_locked     = phase_locked_;
		p.shared.data.locked_raw       = locked_raw_;
		p.shared.do_save_shared        = true;
	}

	// ---- Type selector constants ----
	// Flat index mapping for enc 3 (Transpose) in Channel Edit.
	// Indices 0..kTypeSelGateIdx-1 : CV with built-in scales (Channel::Mode val 0..gate_idx-1)
	// Index kTypeSelGateIdx        : Gate
	// Index kTypeSelGateIdx+1      : Trigger
	// Custom scales are intentionally excluded from the selector (duplicate colors, not yet distinct).
	static constexpr uint8_t kTypeSelGateIdx = static_cast<uint8_t>(Quantizer::scale.size());
	static constexpr uint8_t kTypeSelGate    = kTypeSelGateIdx;
	static constexpr uint8_t kTypeSelTrigger = static_cast<uint8_t>(kTypeSelGateIdx + 1u);
	static constexpr uint8_t kTypeSelTotal   = static_cast<uint8_t>(kTypeSelGateIdx + 2u);

	// ---- Direction helper ----
	static Direction CycleDirection(Direction d, int32_t dir) {
		constexpr int32_t n = 4; // Forward, Reverse, PingPong, Random
		const int32_t v = static_cast<int32_t>(d);
		return static_cast<Direction>(std::clamp<int32_t>(v + dir, 0, n - 1));
	}

	// ---- Clear helper ----
	// Resets all 64 steps of a channel to the "zero" value for its type:
	//   CV → 32768 (centre = 0V within the default bipolar range)
	//   Gate / Trigger → 0 (all off / rest)
	void ClearChannel(uint8_t ch) {
		const StepValue zero = (p.GetData().channel[ch].type == ChannelType::CV) ? 32768u : 0u;
		for (uint8_t s = 0; s < 64; s++)
			p.SetStepValue(ch, s, zero);
		p.shared.do_save_macro = true;
		p.shared.blinker.Set(3, 300); // 3-flash confirmation on page buttons
	}

	// ---- Clear mode ----
	// Entered via SHIFT+PLAY held ≥600ms. Page button N = clear channel N; PLAY = clear all;
	// any other button exits. LED: all 8 page buttons + play slow-blink.
	void UpdateClearMode() {
		// Any button other than page buttons and PLAY exits
		if (c.button.shift.just_went_high() || c.button.fine.just_went_high() ||
		    c.button.morph.just_went_high() || c.button.bank.just_went_high() ||
		    c.button.add.just_went_high())
		{
			clear_mode_active_ = false;
			return;
		}

		// Page button N: clear channel N and exit
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				ClearChannel(static_cast<uint8_t>(i));
				clear_mode_active_ = false;
				return;
			}
		}

		// PLAY: clear all channels and exit
		if (c.button.play.just_went_high()) {
			for (uint8_t ch = 0; ch < Model::NumChans; ch++)
				ClearChannel(ch);
			clear_mode_active_ = false;
			return;
		}
	}

	// ---- Type selector helpers ----

	// Map current channel state to flat type selector index.
	uint8_t TypeSelIndex(const ChannelSettings &cs) const {
		if (cs.type == ChannelType::Gate)    return kTypeSelGate;
		if (cs.type == ChannelType::Trigger) return kTypeSelTrigger;
		// CV built-in scales: raw 0..kTypeSelGateIdx-1 map directly to flat index.
		// Custom scales (raw > kTypeSelGateIdx) are not in the selector; show as unquantized (0).
		const uint8_t raw = cs.scale.RawIndex();
		return raw < kTypeSelGateIdx ? raw : 0u;
	}

	// Apply flat type selector index to channel, updating ChannelType and scale.
	void ApplyTypeSel(uint8_t ch, uint8_t idx) {
		auto &cs = p.GetData().channel[ch];
		if (idx == kTypeSelGate) {
			cs.type = ChannelType::Gate;
			return;
		}
		if (idx == kTypeSelTrigger) {
			cs.type = ChannelType::Trigger;
			return;
		}
		// CV built-in scale: flat idx == raw idx for 0..kTypeSelGateIdx-1
		cs.type = ChannelType::CV;
		cs.scale.SetRaw(idx);
	}

	void CycleTypeSel(uint8_t ch, int32_t dir) {
		const auto &cs  = p.GetData().channel[ch];
		const int32_t cur  = TypeSelIndex(cs);
		const int32_t next = std::clamp<int32_t>(cur + dir, 0, static_cast<int32_t>(kTypeSelTotal - 1));
		ApplyTypeSel(ch, static_cast<uint8_t>(next));
	}

	// ---- Phase rotate (destructive) ----

	// Shift all 64 step values for channel ch forward (positive delta) or backward.
	void RotateChannel(uint8_t ch, int32_t delta) {
		const auto len = p.GetData().channel[ch].length;
		std::array<StepValue, 64> flat{};
		for (auto s = 0u; s < 64u; s++)
			flat[s] = p.GetStepValue(ch, static_cast<uint8_t>(s));
		// Only rotate the active steps [0, len-1]; unused steps beyond length are untouched.
		const int32_t rot = ((delta % static_cast<int32_t>(len)) + len) % len;
		std::rotate(flat.begin(), flat.begin() + rot, flat.begin() + len);
		for (auto s = 0u; s < 64u; s++)
			p.SetStepValue(ch, static_cast<uint8_t>(s), flat[s]);
	}

	// ---- Channel type color for display ----
	// Gate = dim green matching Palette::Scales::color.back() (same as Gate in sequencer).
	// Trigger = dimmer green, visually distinct from Gate.
	static Color GateColor()    { return Palette::Scales::color.back(); }
	static Color TriggerColor() { return Palette::off.blend(Palette::green, 0.25f); }

	Color ChannelTypeColor(uint8_t ch) const {
		const auto &cs = p.GetData().channel[ch];
		switch (cs.type) {
		case ChannelType::CV: {
			auto col = cs.scale.GetColor();
			// Unquantized CV (scale index 0) maps to Palette::very_dim_grey which is nearly
			// invisible (Color(3,3,3)).  Use a visible grey so the LED is readable.
			if (col == Palette::very_dim_grey)
				col = Palette::grey;
			return col;
		}
		case ChannelType::Gate:    return GateColor();
		case ChannelType::Trigger: return TriggerColor();
		}
		return Palette::off;
	}

	// ---- Page helpers ----

	uint8_t GlobalStep(uint8_t step_in_page) const {
		return current_page_ * Model::NumChans + step_in_page;
	}

	void NavigatePage(uint8_t page) {
		// Release all step holds on current page before switching
		for (auto i = 0u; i < Model::NumChans; i++) {
			if (c.button.scene[i].is_high())
				p.SetStepHeld(GlobalStep(i), false);
		}
		trigger_step_enc_turned_ = 0;
		current_page_ = page;
	}

	// ---- Slider recording ----

	// Map 0..4095 slider ADC value to StepValue using the channel's Range.
	// Slider travel maps linearly from range.Min() to range.Max().
	StepValue SliderToStepValue(uint8_t ch, uint16_t adc) const {
		const auto &range = p.GetData().channel[ch].range;
		const float min_v = range.Min();
		const float max_v = range.Max();
		const float v     = min_v + (static_cast<float>(adc) / 4095.f) * (max_v - min_v);
		const float norm  = (v - Model::min_output_voltage) / (Model::max_output_voltage - Model::min_output_voltage);
		return static_cast<StepValue>(std::clamp(norm * 65535.f, 0.f, 65535.f));
	}

	void RecordSlider(uint8_t ch, uint8_t global_step) {
		const auto sv = SliderToStepValue(ch, c.ReadSlider());
		p.SetStepValue(ch, global_step, sv);
	}

	// ---- Encoder value editing ----

	void EditCvStep(uint8_t ch, uint8_t global_step, int32_t inc, bool fine) {
		const auto old_val = p.GetStepValue(ch, global_step);
		const int32_t step = fine ? CvStepFine : CvStepCoarse;
		const int32_t next = static_cast<int32_t>(old_val) + inc * step;
		p.SetStepValue(ch, global_step, static_cast<StepValue>(std::clamp<int32_t>(next, 0, 65535)));
	}

	void EditGateStep(uint8_t ch, uint8_t global_step, int32_t inc, bool fine) {
		const auto old_val   = p.GetStepValue(ch, global_step);
		const auto old_len   = static_cast<uint8_t>(old_val >> 8);
		const uint8_t ratchet = static_cast<uint8_t>(old_val & 0xFF);
		const int32_t step   = fine ? GateStepFine : GateStepCoarse;
		const auto new_len   = static_cast<uint8_t>(std::clamp<int32_t>(old_len + inc * step, 0, 255));
		p.SetStepValue(ch, global_step,
		               static_cast<StepValue>((static_cast<uint16_t>(new_len) << 8) | ratchet));
	}

	// Edit the ratchet count for a Gate channel step (low byte).
	// 0 = no ratchet (single gate), 1–8 = ratchet 1–8x per step.
	void EditGateRatchet(uint8_t ch, uint8_t global_step, int32_t inc) {
		const auto old_val   = p.GetStepValue(ch, global_step);
		const auto gate_len  = static_cast<uint8_t>(old_val >> 8);
		const auto old_ratchet = static_cast<uint8_t>(old_val & 0xFF);
		const auto new_ratchet = static_cast<uint8_t>(std::clamp<int32_t>(old_ratchet + inc, 0, 8));
		p.SetStepValue(ch, global_step,
		               static_cast<StepValue>((static_cast<uint16_t>(gate_len) << 8) | new_ratchet));
	}

	void EditTrigStep(uint8_t ch, uint8_t global_step, int32_t inc) {
		const auto old_raw = p.GetStepValue(ch, global_step);
		const auto old_val = static_cast<int8_t>(old_raw & 0xFF);
		const int32_t next = std::clamp<int32_t>(old_val + inc, -8, 8); // ±8 ratchets/repeats max
		p.SetStepValue(ch, global_step, static_cast<StepValue>(static_cast<uint8_t>(static_cast<int8_t>(next))));
	}

	// Toggle gate/trigger step on/off (x0x editing)
	void ToggleGateStep(uint8_t ch, uint8_t global_step) {
		const auto val     = p.GetStepValue(ch, global_step);
		const uint8_t ratchet = static_cast<uint8_t>(val & 0xFF);
		const bool gate_on = (static_cast<uint8_t>(val >> 8) > 0);
		// Toggle gate length: off→50% gate; on→0. Ratchet count preserved either way.
		// Default on-value: high byte 128 = 128/255 ≈ 50%; value = 0x8000 | ratchet.
		const uint16_t new_val = gate_on ? ratchet
		                                 : static_cast<uint16_t>((128u << 8) | ratchet);
		p.SetStepValue(ch, global_step, static_cast<StepValue>(new_val));
	}

	void ToggleTrigStep(uint8_t ch, uint8_t global_step) {
		const auto raw = p.GetStepValue(ch, global_step);
		const auto val = static_cast<int8_t>(raw & 0xFF);
		// Trigger: 0 = rest, 1 = single trigger (default)
		p.SetStepValue(ch, global_step, val == 0 ? StepValue{1} : StepValue{0});
	}

	// ---- LED color helpers ----

	// Color representing a CV channel's value at a given step
	Color CvStepColor(uint8_t ch, uint8_t global_step) const {
		const auto &cs   = p.GetData().channel[ch];
		const auto  sv   = p.GetStepValue(ch, global_step);
		// Map StepValue to Channel::Cv::type for palette color lookup
		const auto  cv_min  = Channel::Cv::RangeToMin(cs.range);
		const auto  cv_max  = Channel::Cv::RangeToMax(cs.range);
		const float t       = static_cast<float>(sv) / 65535.f;
		const auto  cv_val  = static_cast<Channel::Cv::type>(cv_min + t * (cv_max - cv_min));
		return Palette::Cv::fromLevel(p.shared.data.palette[ch], cv_val, cs.range);
	}

	// Color representing a gate step's gate length (high byte: 0=off, 1–255=dim→bright green).
	Color GateStepColor(uint8_t ch, uint8_t global_step) const {
		const auto sv      = p.GetStepValue(ch, global_step);
		const auto gate_len = static_cast<uint8_t>(sv >> 8);
		if (gate_len == 0) return Palette::off;
		const float frac = static_cast<float>(gate_len) / 255.f;
		return Palette::off.blend(GateColor(), 0.3f + 0.7f * frac);
	}

	// Color representing a gate step's ratchet count (low byte: <2 = no ratchet, 2–8 = N pulses).
	Color GateRatchetColor(uint8_t ch, uint8_t global_step) const {
		const auto sv      = p.GetStepValue(ch, global_step);
		const auto ratchet = static_cast<uint8_t>(sv & 0xFF);
		if (ratchet < 2) return Palette::dim_grey;
		const float brightness = std::clamp(static_cast<float>(ratchet) / 8.f, 0.f, 1.f);
		return Palette::off.blend(Palette::green, 0.2f + 0.8f * brightness);
	}

	// Color representing a trigger step — brightness scales with ratchet/repeat count (1–8).
	Color TrigStepColor(uint8_t ch, uint8_t global_step) const {
		const auto raw = p.GetStepValue(ch, global_step);
		const auto val = static_cast<int8_t>(raw & 0xFF);
		if (val == 0) return Palette::off;
		const float brightness = std::clamp(static_cast<float>(std::abs(val)) / 8.f, 0.f, 1.f);
		if (val > 0) return Palette::off.blend(Palette::green, brightness); // ratchet
		return Palette::off.blend(Palette::teal, brightness);               // repeat
	}

	Color StepColorForChannel(uint8_t ch, uint8_t global_step) const {
		switch (p.GetData().channel[ch].type) {
		case ChannelType::CV:      return CvStepColor(ch, global_step);
		case ChannelType::Gate:    return GateStepColor(ch, global_step);
		case ChannelType::Trigger: return TrigStepColor(ch, global_step);
		}
		return Palette::off;
	}

public:
	Main(Catalyst2::VoltSeq::Interface &p, Controls &c, Abstract &other)
		: Abstract{c, other}
		, p{p} {
	}

	void Init() override {
		for (auto &b : c.button.scene)
			b.clear_events();
		c.button.play.clear_events();
		c.button.bank.clear_events();
	}

	// Called every 3kHz tick before App::Update().
	void Common() override {
		p.clock.bpm.external = c.sense.trig.is_high();

		if (c.jack.trig.just_went_high())
			p.clock.ExternalClockTick();
		if (c.jack.reset.just_went_high())
			p.Reset();
		if (c.button.add.just_went_high())
			p.clock.TapTempo();

		const bool ticked = p.UpdateClock();

		// One-time init: restore persisted lock state
		if (!initialized_from_shared_) {
			initialized_from_shared_ = true;
			if (p.shared.data.phase_locked) {
				phase_locked_ = true;
				locked_raw_   = p.shared.data.locked_raw;
				picking_up_   = true;
			}
		}

		// Slider movement gate: track whether slider is in motion.
		{
			const auto slider_now = c.ReadSlider();
			if (record_slider_prev_ == 0xFFFF) record_slider_prev_ = slider_now;
			if (std::abs(static_cast<int32_t>(slider_now) - static_cast<int32_t>(record_slider_prev_)) >= kRecordMoveThreshold) {
				record_slider_prev_  = slider_now;
				record_active_until_ = Controls::TimeNow() + kRecordActiveTicks;
			}
		}
		const bool record_active = Controls::TimeNow() < record_active_until_;

		// Slider recording: on each master tick while a CV channel is armed and slider is moving.
		const bool armed_cv = armed_ch_.has_value()
		                   && p.GetData().channel[armed_ch_.value()].type == ChannelType::CV
		                   && !perf_page_active_
		                   && record_active;
		if (armed_cv) {
			const auto ch = armed_ch_.value();
			if (ticked && p.IsPlaying()) {
				RecordSlider(ch, p.GetShadow(ch));
			} else if (!p.IsPlaying()) {
				RecordSlider(ch, GlobalStep(last_touched_step_));
			}
		}

		// ---- Slider / orbit / beat-repeat machinery (mirrors seq_common.hh) ----
		const auto perf_mode = p.shared.data.slider_perf_mode;
		if (perf_mode > 0) {
			const auto slider_now = c.ReadSlider();

			// Pickup mode: slider must reach locked_raw_ before orbit center updates
			if (picking_up_) {
				if (std::abs(static_cast<int32_t>(slider_now) - static_cast<int32_t>(locked_raw_))
				    <= kPickupThreshold) {
					picking_up_ = false;
				}
			}

			const uint16_t effective_slider = (phase_locked_ || picking_up_) ? locked_raw_ : slider_now;

			// Slider movement tracking for lock indicator
			if (std::abs(static_cast<int32_t>(slider_now) - static_cast<int32_t>(last_slider_raw_))
			    > kSliderMoveThreshold) {
				last_slider_raw_       = slider_now;
				last_slider_move_time_ = Controls::TimeNow();
			}

			if (perf_mode >= 2) {
				// Beat repeat: orbit_pickup entry mode — hold snapped center until slider moves away
				if (orbit_pickup_) {
					if (std::abs(static_cast<int32_t>(slider_now) - static_cast<int32_t>(orbit_pickup_slider_))
					    > kPickupThreshold) {
						orbit_pickup_ = false;
						p.shared.orbit_center = static_cast<float>(effective_slider) / 4095.f;
					}
				} else if (c.button.shift.is_high()) {
					// SHIFT freezes orbit_center in beat repeat
				} else {
					p.shared.orbit_center = static_cast<float>(effective_slider) / 4095.f;
				}

				// Zone tracking
				static constexpr uint16_t off_zone = 32;
				const uint8_t num_zones  = (perf_mode == 3) ? 4u : 8u;
				const uint16_t zone_width = 4096 / num_zones;

				if (effective_slider < off_zone) {
					p.shared.beat_repeat_pending = 0xFF;
					if (!c.button.shift.is_high()) {
						p.shared.beat_repeat_committed       = 0xFF;
						p.shared.beat_repeat_snap_pending    = false;
						orbit_pickup_                        = false;
					}
				} else {
					const auto zone = std::min(
					    static_cast<uint8_t>((effective_slider - 1) / zone_width),
					    static_cast<uint8_t>(num_zones - 1));
					if (zone != p.shared.beat_repeat_pending) {
						p.shared.beat_repeat_pending       = zone;
						p.shared.beat_repeat_pending_since = Controls::TimeNow();
					} else if (!c.button.shift.is_high()) {
						const auto debounce =
						    kBeatRepeatDebounce[p.shared.beat_repeat_debounce_idx];
						if (Controls::TimeNow() - p.shared.beat_repeat_pending_since >= debounce) {
							p.shared.beat_repeat_committed = p.shared.beat_repeat_pending;
						}
					}
				}

				// Shift release: commit pending zone; on first entry from off, arm snap
				if (c.button.shift.just_went_low()) {
					const bool was_off = (p.shared.beat_repeat_committed == 0xFF);
					p.shared.beat_repeat_committed = p.shared.beat_repeat_pending;
					if (p.shared.beat_repeat_committed == 0xFF) {
						orbit_pickup_ = false;
					} else if (was_off) {
						p.shared.beat_repeat_snap_pending = true;
						orbit_pickup_slider_              = slider_now;
						orbit_pickup_                     = true;
					}
				}
			} else {
				// Granular: orbit_center tracks slider directly
				p.shared.orbit_center = static_cast<float>(effective_slider) / 4095.f;
			}
		}

		// Fine+Glide hold detection: entry into perf page (or settings from within perf page)
		const bool both_held  = c.button.fine.is_high() && c.button.morph.is_high();
		const bool either_just = c.button.fine.just_went_high() || c.button.morph.just_went_high();
		// Perf Page entry is allowed from Main Mode (armed or unarmed) and from within Perf Page
		// itself (to re-enter Perf Settings). All other modals block entry.
		const bool perf_entry_blocked = clear_mode_active_ || global_settings_active_ || channel_edit_active_ || glide_editor_active_;
		if (both_held && either_just && !scrub_hold_pending_ && !perf_entry_blocked) {
			scrub_hold_pending_ = true;
			scrub_hold_start_   = Controls::TimeNow();
		}
		if (scrub_hold_pending_) {
			if (Controls::TimeNow() - scrub_hold_start_ >= kScrubHoldTicks) {
				scrub_hold_pending_ = false;
				if (!perf_page_active_) {
					perf_page_active_    = true;
					perf_settings_active_ = true;
					p.shared.blinker.Set(3, 300); // entry confirmation flash
				} else if (!perf_settings_active_) {
					perf_settings_active_ = true;
				}
			} else if (!c.button.fine.is_high() || !c.button.morph.is_high()) {
				scrub_hold_pending_ = false;
				DoLockToggle();
			}
		}
	}

	void Update() override {
		const bool shift    = c.button.shift.is_high();
		const bool fine     = c.button.fine.is_high();
		const bool chan     = c.button.bank.is_high();
		// Pre-read rising-edge events that appear in short-circuit conditions so they are
		// always consumed on the tick they fire (short-circuit eval would otherwise leave
		// got_rising_edge_ set, causing spurious triggers on the next button press).
		const bool bank_jgh = c.button.bank.just_went_high();
		const bool play_jgh = c.button.play.just_went_high();

		// --- SHIFT+CHAN hold: global settings (≥2 s) or channel edit (short tap) ---
		// Timer starts when CHAN goes high while Shift is already held (or simultaneously).
		// Held ≥2 s → Global Settings.  Released before 2 s → toggle Channel Edit as before.
		// Channel Edit / Global Settings entry is allowed only from Main Mode (armed or unarmed).
		// All other modals block entry — modals do not stack, except Armed → Channel Edit which
		// is handled naturally since armed_ch_ is not a modal flag.
		const bool any_modal = clear_mode_active_ || global_settings_active_ || perf_page_active_ || channel_edit_active_ || glide_editor_active_;
		if (shift && bank_jgh && !any_modal) {
			shift_chan_hold_pending_ = true;
			shift_chan_hold_start_   = Controls::TimeNow();
		}
		if (shift_chan_hold_pending_) {
			if (!shift || c.button.bank.just_went_low()) {
				// Released before 2 s: short tap → toggle Channel Edit
				shift_chan_hold_pending_ = false;
				channel_edit_active_ = !channel_edit_active_;
				if (channel_edit_active_) {
					// Show focused channel's length passively on entry
					channel_edit_last_enc_ = 2;
					length_display_until_  = Controls::TimeNow() + Clock::MsToTicks(600);
					// Sync current_page_ to the focused channel's playhead page so the
					// chaselight and step display are immediately on the correct page.
					current_page_ = static_cast<uint8_t>(p.GetPlayhead(edit_ch_) / Model::NumChans);
				} else {
					channel_edit_last_enc_ = 0xFF;
					length_display_until_  = 0;
					if (!p.IsPlaying()) p.shared.do_save_macro = true;
				}
			} else if (Controls::TimeNow() - shift_chan_hold_start_ >= kGlobalSettingsHoldTicks) {
				shift_chan_hold_pending_ = false;
				global_settings_active_  = true;
				for (auto &b : c.button.scene) b.clear_events();
			}
		}

		// --- Play / Stop / Clear-mode entry ---
		// SHIFT+PLAY: short release (<600ms) = Reset; held ≥600ms = enter Clear mode.
		// Plain PLAY while in an edit mode: exit that mode (no playback change).
		// Plain PLAY while armed: disarm (no playback change).
		// Plain PLAY otherwise: Toggle play/stop.
		if (play_jgh) {
			if (global_settings_active_) {
				global_settings_active_ = false;
				p.shared.do_save_macro  = true;
			} else if (shift) {
				shift_play_pending_ = true;
				shift_play_press_t_ = Controls::TimeNow();
			} else if (perf_page_active_) {
				perf_page_active_     = false;
				perf_settings_active_ = false;
				p.shared.do_save_shared = true;
			} else if (channel_edit_active_) {
				channel_edit_active_   = false;
				channel_edit_last_enc_ = 0xFF;
				length_display_until_  = 0;
				if (!p.IsPlaying()) p.shared.do_save_macro = true;
				// else: save deferred to next play-stop toggle
			} else if (glide_editor_active_) {
				glide_editor_active_   = false;
				if (!p.IsPlaying()) p.shared.do_save_macro = true;
			} else if (armed_ch_.has_value()) {
				armed_ch_                = std::nullopt;
				trigger_step_enc_turned_ = 0;
			} else {
				p.Toggle();
				// Play/Stop reset mode: when stopping, also reset all channels to step 1
				if (!p.IsPlaying() && p.GetData().play_stop_reset)
					p.Reset();
				p.shared.do_save_macro = true;
			}
		}
		if (shift_play_pending_) {
			if (!c.button.shift.is_high()) {
				// Shift released before play: cancel (treat as no-op)
				shift_play_pending_ = false;
			} else if (c.button.play.just_went_low()) {
				// Play released while shift still held
				if (Controls::TimeNow() - shift_play_press_t_ < kClearHoldTicks) {
					// Short press: reset
					p.Reset();
					p.shared.do_save_macro = true;
				}
				// Long press released before threshold: nothing (clear_mode_active_ not set yet)
				shift_play_pending_ = false;
			} else if (Controls::TimeNow() - shift_play_press_t_ >= kClearHoldTicks) {
				// Held long enough: enter clear mode
				shift_play_pending_ = false;
				clear_mode_active_  = true;
				p.shared.blinker.Set(3, 300);
				// Consume all button edges so nothing fires when we return
				for (auto &b : c.button.scene)
					b.clear_events();
				c.button.play.clear_events();
			}
		}

		// --- Mode switch: if mode was changed externally, yield to sequencer ---
		if (p.shared.mode == Model::Mode::Sequencer) {
			perf_page_active_        = false;
			perf_settings_active_    = false;
			channel_edit_active_     = false;
			glide_editor_active_     = false;
			clear_mode_active_       = false;
			global_settings_active_   = false;
			shift_chan_hold_pending_  = false;
			shift_play_pending_       = false;
			SwitchUiMode(main_ui);
			return;
		}

		// --- Clear mode ---
		if (clear_mode_active_) {
			UpdateClearMode();
			return;
		}

		// --- Global Settings modal ---
		if (global_settings_active_) {
			UpdateGlobalSettings(fine);
			return;
		}

		// --- Performance page ---
		if (perf_page_active_) {
			UpdatePerfPage(fine);
			return;
		}

		// Channel Edit entry/exit is handled by the SHIFT+CHAN hold timer above.


		// --- Channel Edit mode ---
		if (channel_edit_active_) {
			UpdateChannelEdit(fine);
			return;
		}

		// --- Glide / Ratchet step editors ---
		if (glide_editor_active_) {
			UpdateGlideEditor(fine);
			return;
		}
		// --- GLIDE modifier (unarmed only — armed CV handles Glide internally for per-step flags) ---
		if (c.button.morph.is_high() && !armed_ch_.has_value()) {
			UpdateGlideModifier(shift, fine);
			return;
		}


		// --- Channel arm / type select: CHAN held ---
		if (chan) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high()) {
					if (armed_ch_ == std::optional<uint8_t>{i}) {
						armed_ch_ = std::nullopt;
						trigger_step_enc_turned_ = 0;
					} else {
						armed_ch_   = i;
						focused_ch_ = static_cast<uint8_t>(i);
					}
					return;
				}
			}
			// CHAN + encoder N: cycle channel N's output type (unquantized CV → scales → Gate → Trigger).
			// Accelerated so fast spin jumps quickly through many scales.
			// Save is deferred to CHAN release to avoid a flash write on every encoder detent.
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				CycleTypeSel(enc, dir);
				focused_ch_ = enc;
			});
			return;
		}

		// Save channel type changes when CHAN is released (deferred from CHAN+encoder handler above).
		// Only save immediately when stopped; if playing, save is already deferred to next play-stop toggle.
		if (c.button.bank.just_went_low() && !p.IsPlaying())
			p.shared.do_save_macro = true;

		// --- Dispatch to arm mode or normal step editing ---
		if (armed_ch_.has_value()) {
			UpdateArmed(fine);
		} else {
			UpdateNormal(fine);
		}
	}

private:
	void UpdateNormal(bool fine) {
		// Shift held: page navigation and encoder focus only — no step editing.
		// Page buttons navigate to the selected page; encoder turns update the chaselight channel.
		if (c.button.shift.is_high()) {
			// Release any steps that were held before shift was pressed
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_low())
					p.SetStepHeld(GlobalStep(i), false);
			}
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(static_cast<uint8_t>(i));
			}
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t /*inc*/) { focused_ch_ = enc; });
			return;
		}

		// Step hold tracking and x0x editing
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				last_touched_step_ = i;
				p.SetStepHeld(GlobalStep(i), true);
			}
			if (btn.just_went_low()) {
				p.SetStepHeld(GlobalStep(i), false);
			}
		}

		// While any step is held: encoder N adjusts channel N's value for all held steps
		if (p.AnyStepHeld()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t inc) {
				focused_ch_ = enc; // chaselight follows last-edited channel
				for (auto i = 0u; i < Model::NumChans; i++) {
					if (!c.button.scene[i].is_high()) continue;
					const uint8_t gs = GlobalStep(i);
					const auto type  = p.GetData().channel[enc].type;
					if (type == ChannelType::CV)
						EditCvStep(enc, gs, enc_accel_.Apply(enc, inc, fine), fine);
					else if (type == ChannelType::Gate)
						EditGateStep(enc, gs, inc, fine);
					else
						EditTrigStep(enc, gs, inc); // no accel: ±8 range
				}
			});
		} else {
			// No step held and no shift: update focused channel from encoder wiggle for chaselight tracking
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t /*inc*/) { focused_ch_ = enc; });
			ClearEncoderEvents(c); // drain deltas — prevent accumulated turns from firing on next CHAN press
		}
	}

	void UpdateArmed(bool fine) {
		const auto ch   = armed_ch_.value();
		const auto type = p.GetData().channel[ch].type;

		if (type == ChannelType::CV) {
			UpdateArmedCV(ch, fine);
		} else if (type == ChannelType::Gate) {
			UpdateArmedGate(ch, fine);
		} else {
			UpdateArmedTrigger(ch, fine);
		}
	}



	void UpdateArmedCV(uint8_t ch, bool fine) {
		auto &cs = p.GetData().channel[ch];

		// Glide held: encoder N sets (CW) or clears (CCW) the per-step glide flag for step N.
		// Glide time is set separately via the unarmed Glide modifier (Glide + encoder in normal mode).
		if (c.button.morph.is_high()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				p.SetGlideFlag(ch, GlobalStep(enc), dir > 0);
			});
			return;
		}

		if (c.button.shift.is_high()) {
			// Shift held: page navigation + enc 4 (Range) / enc 6 (Transpose).
			// Slider range IS the recording range, so adjusting Range also changes recording.
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(static_cast<uint8_t>(i));
			}
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				if (enc == 4) cs.range.Inc(dir);
				else if (enc == 6) CycleTypeSel(ch, dir);
			});
			return;
		}

		// Normal: encoder N directly edits step N's CV value on the current page.
		// Slider recording (motion-gated) continues in Common() regardless.
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t inc) {
			EditCvStep(ch, GlobalStep(enc), enc_accel_.Apply(enc, inc, fine), fine);
		});
		(void)cs;
	}

	void UpdateArmedGate(uint8_t ch, bool fine) {
		// Shift held: page navigation only
		if (c.button.shift.is_high()) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(static_cast<uint8_t>(i));
			}
			ClearEncoderEvents(c);
			return;
		}
		// Glide held: encoder N adjusts ratchet count for step N (0 = no ratchet, 2–8 = N pulses)
		if (c.button.morph.is_high()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				EditGateRatchet(ch, GlobalStep(enc), dir);
			});
			return;
		}
		// Tap page button → toggle step on/off
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				last_touched_step_ = i;
				ToggleGateStep(ch, GlobalStep(i));
			}
		}
		// Encoder N → adjust gate length for step N on current page
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t inc) {
			EditGateStep(ch, GlobalStep(enc), inc, fine);
		});
	}

	void UpdateArmedTrigger(uint8_t ch, bool /*fine*/) {
		// Shift held: page navigation only
		if (c.button.shift.is_high()) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(static_cast<uint8_t>(i));
			}
			ClearEncoderEvents(c);
			return;
		}
		// Tap page button → toggle step on/off
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				last_touched_step_ = static_cast<uint8_t>(i);
				ToggleTrigStep(ch, GlobalStep(i));
			}
		}
		// Encoder N → adjust ratchet/repeat count for step N on current page
		// CW = ratchet (positive), CCW = repeat (negative), clamped to ±8
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t inc) {
			EditTrigStep(ch, GlobalStep(enc), inc);
		});
	}

	// --- Performance Page ---

	void UpdatePerfPage(bool fine) {
		// Exit: Play (handled at top of Update), or short Fine+Glide (handled in Common)

		// Perf Settings active: dispatch there
		if (perf_settings_active_) {
			UpdatePerfSettings();
			return;
		}

		// Shift + page button: page navigation
		if (c.button.shift.is_high()) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(static_cast<uint8_t>(i));
			}
			return;
		}

		// Page buttons: toggle per-channel orbit follow mask
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				p.shared.data.scrub_ignore_mask ^= static_cast<uint8_t>(1u << i);
				p.shared.do_save_shared = true;
			}
		}

		// Encoder step editing still works in perf page (same as normal)
		if (p.AnyStepHeld()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t inc) {
				for (auto i = 0u; i < Model::NumChans; i++) {
					if (!c.button.scene[i].is_high()) continue;
					const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
					const auto    type = p.GetData().channel[enc].type;
					if (type == ChannelType::CV)
						EditCvStep(enc, gs, inc, fine);
					else if (type == ChannelType::Gate)
						EditGateStep(enc, gs, inc, fine);
					else
						EditTrigStep(enc, gs, inc);
				}
			});
		}
	}

	void UpdatePerfSettings() {
		// Exit: Play (handled at top of Update — exits Perf Page entirely).
		// Fine+Glide short is handled in Common() as DoLockToggle; it does NOT exit
		// Perf Settings, so the user stays in Settings after locking/unlocking.

		// Encoder settings (mirrors ScrubSettings from sequencer)
		ForEachEncoderInc(c, [this](uint8_t enc, int32_t inc) {
			switch (enc) {
			case 0: // Quantize orbit center (on/off)
				p.shared.data.quantized_scrub ^= 1u;
				break;
			case 1: { // Performance mode: off / granular / beat-repeat 8 / beat-repeat 4
				auto &mode = p.shared.data.slider_perf_mode;
				if (inc > 0 && mode < 3) { mode++; }
				else if (inc < 0 && mode > 0) { mode--; }
				// Clear beat repeat state on any mode switch
				p.shared.beat_repeat_committed = 0xFF;
				p.shared.beat_repeat_pending   = 0xFF;
				break;
			}
			case 2: // Granular width / beat-repeat debounce
				if (p.shared.data.slider_perf_mode >= 2) {
					auto &d = p.shared.beat_repeat_debounce_idx;
					if (inc > 0 && d < 7) d++;
					else if (inc < 0 && d > 0) d--;
				} else {
					auto &w = p.shared.data.orbit_width;
					if (inc > 0 && w < 100) w++;
					else if (inc < 0 && w > 0) w--;
				}
				break;
			case 3: // Orbit direction (granular only)
				if (p.shared.data.slider_perf_mode == 1) {
					auto &d = p.shared.data.orbit_direction;
					if (inc > 0 && d < 3) d++;
					else if (inc < 0 && d > 0) d--;
				}
				break;
			case 7: // Lock toggle
				DoLockToggle();
				break;
			default:
				break;
			}
		});

		// Page buttons: toggle per-channel orbit follow mask
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				p.shared.data.scrub_ignore_mask ^= static_cast<uint8_t>(1u << i);
				p.shared.do_save_shared = true;
			}
		}
	}

	// --- Glide / Ratchet helpers ---

	// Offset all active gate steps for channel ch by delta (skips off steps; preserves ratchet byte)
	void OffsetAllGateSteps(uint8_t ch, int32_t delta) {
		for (auto s = 0u; s < 64u; s++) {
			const auto val     = p.GetStepValue(ch, static_cast<uint8_t>(s));
			const auto gate_len = static_cast<uint8_t>(val >> 8);
			if (gate_len == 0) continue;
			const uint8_t ratchet = static_cast<uint8_t>(val & 0xFF);
			const auto new_len = static_cast<uint8_t>(
			    std::clamp<int32_t>(gate_len + delta * GateStepCoarse, 1, 255));
			p.SetStepValue(ch, static_cast<uint8_t>(s),
			               static_cast<StepValue>((static_cast<uint16_t>(new_len) << 8) | ratchet));
		}
	}

	// Offset all active trigger steps for channel ch by delta (skips rest steps)
	void OffsetAllTrigSteps(uint8_t ch, int32_t delta) {
		for (auto s = 0u; s < 64u; s++) {
			const auto raw = p.GetStepValue(ch, static_cast<uint8_t>(s));
			const auto val = static_cast<int8_t>(raw & 0xFF);
			if (val == 0) continue;
			const int32_t next = std::clamp<int32_t>(val + delta, -8, 8);
			p.SetStepValue(ch, static_cast<uint8_t>(s),
			               static_cast<StepValue>(static_cast<uint8_t>(static_cast<int8_t>(next))));
		}
	}

	// --- GLIDE modifier (called when c.button.morph is held) ---

	void UpdateGlideModifier(bool shift, bool fine) {
		// Track page-button presses for long-press detection
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				glide_longpress_btn_   = static_cast<uint8_t>(i);
				glide_longpress_shift_ = shift;
				glide_longpress_timer_.SetAlarm();
			}
			if (static_cast<uint8_t>(i) == glide_longpress_btn_ && btn.just_went_low()) {
				glide_longpress_btn_ = kNoLongpressBtn; // released before timer fired
			}
		}

		// Check for long-press fire
		if (glide_longpress_btn_ != kNoLongpressBtn && glide_longpress_timer_.Check()) {
			const auto ch   = glide_longpress_btn_;
			const bool was_shift = glide_longpress_shift_;
			glide_longpress_btn_ = kNoLongpressBtn;
			const auto type = p.GetData().channel[ch].type;

			if (!was_shift) {
				// Long-press: Glide Step Editor for CV or Gate channels
				if (type == ChannelType::CV || type == ChannelType::Gate) {
					glide_editor_active_ = true;
					glide_editor_ch_     = ch;
				}
				// Trigger: no glide step editor (ratchet editing is done via armed mode or step+enc)
			}
			return;
		}

		// Step button held + encoder: per-step ratchet edit for Gate channels
		// (consistency with normal step+enc for CV/Trigger editing)
		if (glide_longpress_btn_ != kNoLongpressBtn) {
			const auto step_btn = glide_longpress_btn_;
			bool enc_turned = false;
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				auto &cs = p.GetData().channel[enc];
				if (cs.type == ChannelType::Gate) {
					EditGateRatchet(enc, GlobalStep(step_btn), dir);
					enc_turned = true;
				}
			});
			if (enc_turned)
				glide_longpress_btn_ = kNoLongpressBtn; // cancel long-press once enc turned
			return;
		}

		// No step held: encoder adjustments apply channel-level parameters
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			auto &cs = p.GetData().channel[enc];
			if (shift) {
				// SHIFT+GLIDE + enc: offset all Trigger steps ratchet/repeat
				if (cs.type == ChannelType::Trigger)
					OffsetAllTrigSteps(enc, dir);
			} else {
				// GLIDE + enc: channel-level parameter
				if (cs.type == ChannelType::CV) {
					cs.glide_time = std::clamp(cs.glide_time + dir * 0.1f, 0.f, 10.f);
				} else if (cs.type == ChannelType::Gate) {
					OffsetAllGateSteps(enc, dir);
				} else if (cs.type == ChannelType::Trigger) {
					cs.pulse_width_ms = static_cast<uint8_t>(
					    std::clamp<int32_t>(cs.pulse_width_ms + dir, 1, 100));
				}
			}
			(void)fine;
		});
	}

	// --- Glide Step Editor ---

	void UpdateGlideEditor(bool fine) {
		// Exit: Play (handled at top of Update) or GLIDE press.  Do NOT exit on bare page button
		// press — for channel 0, page button 0 is also step 1, making step 1 unreachable.
		if (c.button.morph.just_went_high()) {
			glide_editor_active_   = false;
			p.shared.do_save_macro = true;
			return;
		}

		// Page navigation while Shift held
		if (c.button.shift.is_high()) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(static_cast<uint8_t>(i));
			}
			return;
		}

		const auto ch   = glide_editor_ch_;
		const auto type = p.GetData().channel[ch].type;

		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			const uint8_t gs = GlobalStep(enc); // enc 0-7 maps to step 0-7 on current page
			if (type == ChannelType::CV) {
				// CW = set glide on, CCW = set glide off (no accel: binary flag)
				p.SetGlideFlag(ch, gs, dir > 0);
			} else if (type == ChannelType::Gate) {
				EditGateStep(ch, gs, dir, fine);
			}
		});
	}

	// --- Channel Edit (Shift + CHAN) ---
	// Encoder positions match physical panel labels (1-indexed on panel = 0-indexed here):
	//   enc 0 = Output delay  (Start)
	//   enc 1 = Direction     (Dir.)
	//   enc 2 = Length        (Length)
	//   enc 3 = Phase rotate  (Phase)  — rotates only active steps [0, length-1]
	//   enc 4 = Range/PW      (Range)
	//   enc 5 = Clock div     (BPM/Clock Div)
	//   enc 6 = Type selector (Transpose)
	//   enc 7 = Random        (Random)

	void UpdateChannelEdit(bool fine) {
		// Exit: Play is handled at the top of Update() before we get here.
		// CHAN (bank) press is also consumed before reaching here via bank_jgh; exit is handled
		// by the SHIFT+CHAN toggle in Update() instead.

		// Shift + page button: page navigation
		if (c.button.shift.is_high()) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(i);
			}
			return;
		}

		// Page button: tap = focus channel, hold (600 ms) = clear that channel's steps
		if (channel_edit_clear_btn_ != kNoLongpressBtn) {
			const uint8_t btn = channel_edit_clear_btn_;
			if (!c.button.scene[btn].is_high()) {
				// Released before long-press: focus channel and show length passively for 600 ms
				edit_ch_                = btn;
				channel_edit_last_enc_  = 2; // pre-select length encoder for passive display
				length_display_until_   = Controls::TimeNow() + Clock::MsToTicks(600);
				channel_edit_clear_btn_ = kNoLongpressBtn;
				return;
			}
			if (channel_edit_clear_timer_.Check()) {
				// Held long enough: clear all steps for this channel
				ClearChannel(btn);
				edit_ch_                = btn;
				channel_edit_last_enc_  = 0xFF;
				length_display_until_   = 0;
				channel_edit_clear_btn_ = kNoLongpressBtn;
				return;
			}
			return; // still waiting for timer or release
		}

		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				channel_edit_clear_btn_ = static_cast<uint8_t>(i);
				channel_edit_clear_timer_.SetAlarm();
				return;
			}
		}

		// Encoder editing for focused channel
		auto &cs = p.GetData().channel[edit_ch_];

		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			channel_edit_last_enc_ = enc;
			switch (enc) {
			case 0: // Output delay (0–20ms) — no accel: narrow range
				cs.output_delay_ms = static_cast<uint8_t>(
				    std::clamp<int32_t>(cs.output_delay_ms + dir, 0, 20));
				break;
			case 1: // Direction (Forward / Reverse / Ping-Pong / Random) — no accel: 4 values
				cs.direction = CycleDirection(cs.direction, dir);
				break;
			case 2: { // Length (1–64)
				const int32_t adelta = dir;
				cs.length = static_cast<uint8_t>(
				    std::clamp<int32_t>(cs.length + adelta, 1, 64));
				current_page_ = static_cast<uint8_t>((cs.length - 1u) / Model::NumChans);
				// Keep length feedback visible for 600 ms after the last turn.
				length_display_until_ = Controls::TimeNow() + Clock::MsToTicks(600);
				break;
			}
			case 3: // Phase rotate — rotates only active steps [0, length-1]
				RotateChannel(edit_ch_, dir);
				break;
			case 4: // Range (CV) / Pulse width (Trigger) — no accel: few options
				if (cs.type == ChannelType::CV)
					cs.range.Inc(dir);
				else if (cs.type == ChannelType::Trigger)
					cs.pulse_width_ms = static_cast<uint8_t>(
					    std::clamp<int32_t>(cs.pulse_width_ms + dir, 1, 100));
				break;
			case 5: // Clock division — no accel: steps are already coarse
				cs.division.Inc(dir);
				break;
			case 6: // Type selector (unquantized CV → scales → Gate → Trigger, clamped) — no accel
				CycleTypeSel(edit_ch_, dir);
				break;
			case 7: // Random amount (0..1 in 0.05 increments) — no accel: narrow range
				cs.random_amount = std::clamp(cs.random_amount + dir * 0.05f, 0.f, 1.f);
				break;
			}
			(void)fine;
		});
	}

	// --- Global Settings modal (entered via Shift held ≥1.5 s; exits on Play) ---
	// Panel assignments:
	//   Panel 1 (Start)       enc 0 — Play/Stop reset mode (clamped off→on; LED red=on, off=off)
	//   Panel 3 (Length)      enc 2 — Master reset steps (0=off, 1–64; red=snap, orange=other, off=0)
	//   Panel 6 (BPM/Clk Div) enc 5 — Internal BPM
	// Page buttons: reset leader channel radio (tap to select, tap selected to deselect)

	void UpdateGlobalSettings(bool fine) {
		auto &d = p.GetData();

		// Page buttons: reset leader radio selector
		auto &leader = d.reset_leader_ch;
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				leader = (leader == static_cast<uint8_t>(i)) ? 0xFF : static_cast<uint8_t>(i);
			}
		}

		// Encoders
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			switch (enc) {
			case 0: { // Panel 1 (Start): Play/Stop reset mode — clamped off/on
				const int32_t v = std::clamp<int32_t>((d.play_stop_reset ? 1 : 0) + dir, 0, 1);
				d.play_stop_reset = (v == 1);
				break;
			}
			case 2: { // Panel 3 (Length): Master reset steps — 0=off, 1–64, clamped
				d.master_reset_steps = static_cast<uint8_t>(
				    std::clamp<int32_t>(d.master_reset_steps + dir, 0, 64));
				break;
			}
			case 5: // Panel 6 (BPM/Clock Div): Internal BPM
				p.clock.bpm.Inc(dir, fine);
				break;
			default:
				break;
			}
		});
	}

public:
	void PaintLeds(const Model::Output::Buffer & /*outs*/) override {
		// --- Clear mode: all page buttons + play LED slow-blink ---
		if (clear_mode_active_) {
			for (auto i = 0u; i < Model::NumChans; i++)
				c.SetEncoderLed(i, Palette::off);
			const bool blink = (Controls::TimeNow() >> 10u) & 0x01u; // ~3 Hz
			for (auto i = 0u; i < Model::NumChans; i++)
				c.SetButtonLed(i, blink);
			c.SetPlayLed(blink);
			return;
		}

		// --- Performance Page Settings ---
		if (perf_page_active_ && perf_settings_active_) {
			// Encoder LEDs mirror ScrubSettings display
			static constexpr std::array<Color, 4> perf_mode_colors = {
			    Palette::off, Palette::green, Palette::blue, Palette::cyan};
			static constexpr std::array<Color, 4> dir_colors = {
			    Palette::green, Palette::blue, Palette::orange, Palette::lavender};

			for (auto i = 0u; i < Model::NumChans; i++)
				c.SetEncoderLed(i, Palette::off);

			c.SetEncoderLed(0, p.shared.data.quantized_scrub ? Palette::orange : Palette::off);
			c.SetEncoderLed(1, perf_mode_colors[p.shared.data.slider_perf_mode]);

			if (p.shared.data.slider_perf_mode >= 2) {
				const auto brightness = p.shared.beat_repeat_debounce_idx / 7.f;
				c.SetEncoderLed(2, Palette::off.blend(Palette::full_white, brightness));
			} else if (p.shared.data.orbit_width == 0) {
				c.SetEncoderLed(2, Palette::off);
			} else {
				c.SetEncoderLed(2, Palette::off.blend(Palette::orange, p.shared.data.orbit_width / 100.f));
			}

			if (p.shared.data.slider_perf_mode == 1)
				c.SetEncoderLed(3, dir_colors[p.shared.data.orbit_direction]);

			c.SetEncoderLed(7, phase_locked_ ? Palette::red : Palette::off);

			// Page buttons: orbit follow mask (lit = follows)
			for (auto i = 0u; i < Model::NumChans; i++)
				c.SetButtonLed(i, static_cast<bool>((p.shared.data.scrub_ignore_mask >> i) & 1u));
			return;
		}

		// --- Performance Page (normal view) ---
		if (perf_page_active_) {
			// Page buttons: orbit follow mask
			for (auto i = 0u; i < Model::NumChans; i++)
				c.SetButtonLed(i, static_cast<bool>((p.shared.data.scrub_ignore_mask >> i) & 1u));

			// Encoder LEDs: show each channel's current output step color
			// plus lock indicator on enc 7 when active
			for (auto i = 0u; i < Model::NumChans; i++) {
				const uint8_t step = p.GetOutputStep(static_cast<uint8_t>(i));
				c.SetEncoderLed(i, StepColorForChannel(static_cast<uint8_t>(i), step));
			}

			// Lock indicator: enc 7 pulses red when locked or picking up
			const auto t = Controls::TimeNow();
			const bool just_toggled = (t - lock_toggle_time_) < kToggleFeedbackTicks;
			const bool slider_active = phase_locked_
			                        && (t - last_slider_move_time_) < kSliderActiveTicks
			                        && last_slider_move_time_ != 0;
			if (just_toggled || slider_active || picking_up_) {
				c.SetEncoderLed(7, ((t >> 7) & 1u) ? Palette::red : Palette::off);
			}
			return;
		}

		// --- Glide Step Editor ---
		if (glide_editor_active_) {
			const auto ch   = glide_editor_ch_;
			const auto type = p.GetData().channel[ch].type;
			const bool blink = (Controls::TimeNow() >> 8) & 1u;
			for (auto i = 0u; i < Model::NumChans; i++) {
				// Page buttons: editor channel pulses
				c.SetButtonLed(i, i == ch && blink);
				// Encoder LEDs: CV = glide flag; Gate = gate length; Trigger = trig color
				const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
				Color col;
				if (type == ChannelType::CV)
					col = p.GlideFlag(ch, gs) ? Palette::full_white : Palette::dim_grey;
				else if (type == ChannelType::Gate)
					col = GateStepColor(ch, gs);
				else
					col = TrigStepColor(ch, gs);
				c.SetEncoderLed(i, col);
			}
			return;
		}

		// --- Channel Edit mode ---
		if (channel_edit_active_) {
			if (Controls::TimeNow() <= length_display_until_) {
				// Length feedback: page buttons = page count, encoder LEDs = step fill on last page.
				// Shown for 600 ms after the last encoder 2 (Length) turn, then reverts.
				const auto len          = p.GetData().channel[edit_ch_].length;
				const auto last_page    = static_cast<uint8_t>((len - 1u) / Model::NumChans);
				const auto steps_on_pg  = static_cast<uint8_t>((len - 1u) % Model::NumChans + 1u);
				for (auto i = 0u; i < Model::NumChans; i++) {
					c.SetButtonLed(i, i <= last_page);
					c.SetEncoderLed(i, i < steps_on_pg ? ChannelTypeColor(edit_ch_) : Palette::dim_grey);
				}
			} else {
				// Encoder LEDs match panel layout:
				//   enc 0 (Start/delay): white brightness = delay amount
				//   enc 1 (Dir):  color = current direction
				//   enc 6 (Transpose): type/scale color
				//   enc 7 (Random): white brightness = random amount
				// Page buttons: focused channel lit solid; current playing step blinks (chaselight).
				static constexpr std::array<Color, 4> dir_col = {
				    Palette::green,  // Forward
				    Palette::orange, // Reverse
				    Palette::yellow, // PingPong
				    Palette::magenta // Random
				};
				const auto &cs       = p.GetData().channel[edit_ch_];
				const auto  ph_edit  = p.GetPlayhead(edit_ch_);
				const bool  on_page  = (ph_edit / Model::NumChans == current_page_);
				const uint8_t chase  = on_page ? static_cast<uint8_t>(ph_edit % Model::NumChans) : 0xFF;
				const bool  blink    = (Controls::TimeNow() >> 8) & 1u;
				for (auto i = 0u; i < Model::NumChans; i++) {
					// Shift held: show current page; otherwise focused-channel + chaselight
					bool page_lit;
					if (c.button.shift.is_high()) {
						page_lit = (i == current_page_);
					} else {
						page_lit = (i == edit_ch_);
						if (i == chase) page_lit = blink;
					}
					c.SetButtonLed(i, page_lit);
					Color col = Palette::dim_grey;
					if (i == 0) col = Palette::off.blend(Palette::full_white, cs.output_delay_ms / 20.f);
					else if (i == 1) col = dir_col[static_cast<uint8_t>(cs.direction)];
					else if (i == 6) col = ChannelTypeColor(edit_ch_);
					else if (i == 7) col = Palette::off.blend(Palette::full_white, cs.random_amount);
					c.SetEncoderLed(i, col);
				}
			}
			return;
		}

		// --- Global Settings modal ---
		// Page buttons: lit = that channel is the reset leader.
		// Encoder LEDs:
		//   enc 0 (Panel 1, Start)        = play/stop reset mode → red=on, off=off
		//   enc 2 (Panel 3, Length)       = master reset steps   → red=snap(8/16/32/64), orange=other, off=0
		//   enc 5 (Panel 6, BPM/Clk Div)  = BPM                 → yellow, pulses with clock phase
		if (global_settings_active_) {
			const auto &d = p.GetData();
			for (auto i = 0u; i < Model::NumChans; i++)
				c.SetButtonLed(i, d.reset_leader_ch == static_cast<uint8_t>(i));
			for (auto i = 0u; i < Model::NumChans; i++) {
				Color col = Palette::dim_grey;
				switch (i) {
				case 0:
					col = d.play_stop_reset ? Palette::red : Palette::off;
					break;
				case 2: {
					if (d.master_reset_steps == 0) {
						col = Palette::off;
					} else {
						const bool snap = (d.master_reset_steps == 8  || d.master_reset_steps == 16
						                || d.master_reset_steps == 32 || d.master_reset_steps == 64);
						col = snap ? Palette::red : Palette::orange;
					}
					break;
				}
				case 5: {
					// Pulses with clock phase (PeekPhase advances even when stopped).
					// Color zone indicates BPM range (ROYGBIV):
					//   <50 red | 50-79 orange | 80-99 yellow | 100-119 green |
					//   120-149 blue | 150-179 teal | 180+ lavender
					const auto bpm = static_cast<uint32_t>(p.clock.bpm.GetBpm() + 0.5f);
					Color zone;
					if      (bpm < 50)  zone = Palette::red;
					else if (bpm < 80)  zone = Palette::orange;
					else if (bpm < 100) zone = Palette::yellow;
					else if (bpm < 120) zone = Palette::green;
					else if (bpm < 150) zone = Palette::blue;
					else if (bpm < 180) zone = Palette::teal;
					else                zone = Palette::lavender;
					col = Palette::off.blend(zone, p.clock.bpm.PeekPhase());
					break;
				}
				default: break;
				}
				c.SetEncoderLed(i, col);
			}
			return;
		}

		// --- CHAN held (no page button pressed): show channel types on encoders ---
		if (c.button.bank.is_high() && !armed_ch_.has_value()) {
			for (auto i = 0u; i < Model::NumChans; i++) {
				c.SetButtonLed(i, false);
				c.SetEncoderLed(i, ChannelTypeColor(i));
			}
			return;
		}

		const bool armed = armed_ch_.has_value();
		const auto ch    = armed ? armed_ch_.value() : 0u;
		const auto type  = armed ? p.GetData().channel[ch].type : ChannelType::CV;

		const bool armed_gate_trig = armed && (type == ChannelType::Gate || type == ChannelType::Trigger);

		// --- Page button LEDs ---
		for (auto i = 0u; i < Model::NumChans; i++) {
			bool lit = false;
			if (armed_gate_trig) {
				// x0x on/off pattern for armed Gate/Trigger
				const auto sv  = p.GetStepValue(ch, GlobalStep(i));
				const auto val = static_cast<int8_t>(sv & 0xFF);
				lit = (type == ChannelType::Gate) ? (sv > 0) : (val != 0);
			} else if (c.button.shift.is_high()) {
				// Shift held: illuminate only the currently selected page
				lit = (i == current_page_);
			} else {
				// CV armed or unarmed: chaselight shows current step of focused channel
				const auto ph = p.GetPlayhead(focused_ch_);
				lit = (ph / Model::NumChans == current_page_ && ph % Model::NumChans == i);
				lit = lit || c.button.scene[i].is_high();
			}
			c.SetButtonLed(i, lit);
		}

		// --- Encoder LEDs ---
		if (armed_gate_trig) {
			// Armed Gate + Glide held: show per-step ratchet counts instead of gate state
			if (type == ChannelType::Gate && c.button.morph.is_high()) {
				for (auto i = 0u; i < Model::NumChans; i++) {
					const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
					c.SetEncoderLed(i, GateRatchetColor(ch, gs));
				}
			} else {
			// Gate/Trigger armed: each encoder shows step state for the armed channel,
			// with the currently playing step highlighted (chaselight).
			// A held page button suppresses the chase blink so the static color is visible while editing.
			const auto ph       = p.GetPlayhead(ch);
			const bool on_page  = (ph / Model::NumChans == current_page_);
			const uint8_t chase = on_page ? static_cast<uint8_t>(ph % Model::NumChans) : 0xFF;
			// Blink faster when the step is repeating (playhead frozen, extra pulses pending).
			const bool repeating = p.IsRepeating(ch);
			const bool blink     = repeating
			    ? ((Controls::TimeNow() >> 6) & 1u)  // ~47 Hz during repeat
			    : ((Controls::TimeNow() >> 8) & 1u); // ~12 Hz normal

			for (auto i = 0u; i < Model::NumChans; i++) {
				const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
				Color col        = StepColorForChannel(ch, gs);
				// Chaselight: current step pulses white, unless that step's button is held (editing)
				if (i == chase && !c.button.scene[i].is_high())
					col = blink ? ChaseWhite : col;
				c.SetEncoderLed(i, col);
			}
			}
		} else if (armed) {
			// CV armed + Glide held: show per-step glide flags (white = glide on, dim grey = off)
			if (c.button.morph.is_high()) {
				for (auto i = 0u; i < Model::NumChans; i++) {
					const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
					c.SetEncoderLed(i, p.GlideFlag(ch, gs) ? Palette::full_white : Palette::dim_grey);
				}
			} else {
				// CV armed: each encoder shows that step's CV value color for the armed channel,
				// with the currently playing step highlighted (chaselight).
				// A held page button suppresses the chase blink so the static color is visible while editing.
				const auto ph       = p.GetPlayhead(ch);
				const bool on_page  = (ph / Model::NumChans == current_page_);
				const uint8_t chase = on_page ? static_cast<uint8_t>(ph % Model::NumChans) : 0xFF;
				const bool blink    = (Controls::TimeNow() >> 8) & 1u;

				for (auto i = 0u; i < Model::NumChans; i++) {
					const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
					Color col        = StepColorForChannel(ch, gs);
					if (i == chase && !c.button.scene[i].is_high())
						col = blink ? ChaseWhite : col;
					c.SetEncoderLed(i, col);
				}
			}
		} else if (p.AnyStepHeld()) {
			// Step held for editing: show each channel's color at the held step so edits are visible live.
			const uint8_t gs = GlobalStep(last_touched_step_);
			for (auto i = 0u; i < Model::NumChans; i++)
				c.SetEncoderLed(i, StepColorForChannel(i, gs));
		} else if (c.button.shift.is_high()) {
			// Shift held (unarmed, no step held): show playhead colors; focused channel blinks white
			// so the user can see which channel's chaselight is tracked before navigating.
			const bool blink = (Controls::TimeNow() >> 8) & 1u;
			for (auto i = 0u; i < Model::NumChans; i++) {
				const uint8_t ref_step = p.GetPlayhead(i);
				Color col              = StepColorForChannel(i, ref_step);
				if (i == focused_ch_)
					col = blink ? ChaseWhite : col;
				c.SetEncoderLed(i, col);
			}
		} else {
			// Unarmed idle: show each channel's color at its current playhead step
			for (auto i = 0u; i < Model::NumChans; i++) {
				const uint8_t ref_step = p.GetPlayhead(i);
				c.SetEncoderLed(i, StepColorForChannel(i, ref_step));
			}
		}

		// --- Phase Scrub Lock indicator: enc 7 (panel 8) ---
		// Shown in all states except Performance Page (which has its own enc 7 display).
		// Matches CatSeq behavior: rapid red flash on toggle, while slider moves when locked,
		// and during pickup. Silent when locked and idle.
		if (!perf_page_active_) {
			const auto t            = Controls::TimeNow();
			const bool just_toggled = (t - lock_toggle_time_) < kToggleFeedbackTicks;
			const bool slider_active = phase_locked_
			                        && (t - last_slider_move_time_) < kSliderActiveTicks
			                        && last_slider_move_time_ != 0;
			if (just_toggled || slider_active || picking_up_) {
				c.SetEncoderLed(7, ((t >> 7) & 1u) ? Palette::red : Palette::off);
			}
		}
	}
};

} // namespace Catalyst2::Ui::VoltSeq
