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

// Gate length step sizes (0..65535 = 0..100% gate)
inline constexpr uint16_t GateStepCoarse = 655;  // ≈ 1%
inline constexpr uint16_t GateStepFine   = 65;   // ≈ 0.1%

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

	// Glide / Ratchet step editor state
	bool    glide_editor_active_   = false;
	bool    ratchet_editor_active_ = false;
	uint8_t glide_editor_ch_       = 0;
	uint8_t ratchet_editor_ch_     = 0;

	// Bitmask: bit i set if an encoder was turned while step i was held (armed Trigger editing).
	// Used to distinguish tap (toggle) from hold+edit (no toggle).
	uint8_t trigger_step_enc_turned_ = 0;

	// Long-press detection for GLIDE + page button entry
	static constexpr uint8_t  kNoLongpressBtn  = 0xFF;
	static constexpr uint32_t kLongpressMs     = 600u;
	Clock::Timer               glide_longpress_timer_{kLongpressMs};
	uint8_t                    glide_longpress_btn_   = kNoLongpressBtn;
	bool                       glide_longpress_shift_ = false;

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
	// Indices kTypeSelGateIdx..kTypeSelMax-1 : CV with custom scales (val gate_idx+1..max)
	// Index kTypeSelMax     : Gate
	// Index kTypeSelMax + 1 : Trigger
	static constexpr uint8_t kTypeSelGateIdx = static_cast<uint8_t>(Quantizer::scale.size());
	static constexpr uint8_t kTypeSelMax     = kTypeSelGateIdx + static_cast<uint8_t>(Model::num_custom_scales);
	static constexpr uint8_t kTypeSelGate    = kTypeSelMax;
	static constexpr uint8_t kTypeSelTrigger = static_cast<uint8_t>(kTypeSelMax + 1u);
	static constexpr uint8_t kTypeSelTotal   = static_cast<uint8_t>(kTypeSelMax + 2u);

	// ---- Direction helper ----
	static Direction CycleDirection(Direction d, int32_t dir) {
		constexpr int32_t n = 4; // Forward, Reverse, PingPong, Random
		const int32_t v = static_cast<int32_t>(d);
		return static_cast<Direction>(((v + dir) % n + n) % n);
	}

	// ---- Type selector helpers ----

	// Map current channel state to flat type selector index.
	uint8_t TypeSelIndex(const ChannelSettings &cs) const {
		if (cs.type == ChannelType::Gate)    return kTypeSelGate;
		if (cs.type == ChannelType::Trigger) return kTypeSelTrigger;
		// CV: map raw scale index, skipping Channel::Mode's gate slot (kTypeSelGateIdx)
		const uint8_t raw = cs.scale.RawIndex();
		return raw < kTypeSelGateIdx ? raw : static_cast<uint8_t>(raw - 1u);
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
		// CV: remap flat index back to raw scale val (skip kTypeSelGateIdx slot)
		cs.type = ChannelType::CV;
		const uint8_t raw = idx < kTypeSelGateIdx ? idx : static_cast<uint8_t>(idx + 1u);
		cs.scale.SetRaw(raw);
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
		std::array<StepValue, 64> flat{};
		for (auto s = 0u; s < 64u; s++)
			flat[s] = p.GetStepValue(ch, static_cast<uint8_t>(s));
		const int32_t rot = ((delta % 64) + 64) % 64;
		std::rotate(flat.begin(), flat.begin() + rot, flat.end());
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
		case ChannelType::CV:      return cs.scale.GetColor();
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

	// Map 0..4095 slider ADC value to StepValue using channel's recording window.
	// StepValue 0..65535 spans -5V..10V (15V total).
	StepValue SliderToStepValue(uint8_t ch, uint16_t adc) const {
		const auto &cs  = p.GetData().channel[ch];
		const float min_v = static_cast<float>(cs.slider_base_v);
		const float max_v = min_v + static_cast<float>(cs.slider_span_v);
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
		const auto old_val = p.GetStepValue(ch, global_step);
		const int32_t step = fine ? GateStepFine : GateStepCoarse;
		const int32_t next = static_cast<int32_t>(old_val) + inc * step;
		p.SetStepValue(ch, global_step, static_cast<StepValue>(std::clamp<int32_t>(next, 0, 65535)));
	}

	void EditTrigStep(uint8_t ch, uint8_t global_step, int32_t inc) {
		const auto old_raw = p.GetStepValue(ch, global_step);
		const auto old_val = static_cast<int8_t>(old_raw & 0xFF);
		const int32_t next = std::clamp<int32_t>(old_val + inc, -8, 8); // ±8 ratchets/repeats max
		p.SetStepValue(ch, global_step, static_cast<StepValue>(static_cast<uint8_t>(static_cast<int8_t>(next))));
	}

	// Toggle gate/trigger step on/off (x0x editing)
	void ToggleGateStep(uint8_t ch, uint8_t global_step) {
		const auto val = p.GetStepValue(ch, global_step);
		// Gate: 0 = off, 32768 (50%) = default gate length on
		p.SetStepValue(ch, global_step, val > 0 ? StepValue{0} : StepValue{32768});
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

	// Color representing a gate step's gate length (0=off, non-zero=dim→bright green).
	Color GateStepColor(uint8_t ch, uint8_t global_step) const {
		const auto sv = p.GetStepValue(ch, global_step);
		if (sv == 0) return Palette::off;
		const float frac = static_cast<float>(sv) / 65535.f;
		return Palette::off.blend(GateColor(), 0.3f + 0.7f * frac);
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
		if (both_held && either_just && !scrub_hold_pending_) {
			scrub_hold_pending_ = true;
			scrub_hold_start_   = Controls::TimeNow();
		}
		if (scrub_hold_pending_) {
			if (Controls::TimeNow() - scrub_hold_start_ >= kScrubHoldTicks) {
				scrub_hold_pending_ = false;
				if (!perf_page_active_) {
					perf_page_active_ = true;
				} else if (!perf_settings_active_) {
					perf_settings_active_ = true;
				}
			} else if (!c.button.fine.is_high() || !c.button.morph.is_high()) {
				scrub_hold_pending_ = false;
				if (perf_page_active_) {
					// Short release from perf page = exit
					perf_page_active_     = false;
					perf_settings_active_ = false;
				}
			}
		}
	}

	void Update() override {
		const bool shift = c.button.shift.is_high();
		const bool fine  = c.button.fine.is_high();
		const bool chan  = c.button.bank.is_high();

		// --- Play / Stop ---
		if (c.button.play.just_went_high()) {
			if (shift)
				p.Reset();
			else
				p.Toggle();
			p.shared.do_save_macro = true;
		}

		// --- Mode switch: if mode was changed externally, yield to sequencer ---
		if (p.shared.mode == Model::Mode::Sequencer) {
			perf_page_active_      = false;
			perf_settings_active_  = false;
			channel_edit_active_   = false;
			glide_editor_active_   = false;
			ratchet_editor_active_ = false;
			SwitchUiMode(main_ui);
			return;
		}

		// --- Performance page ---
		if (perf_page_active_) {
			UpdatePerfPage(fine);
			return;
		}

		// --- Channel Edit entry: Shift + CHAN ---
		if (shift && c.button.bank.just_went_high()) {
			channel_edit_active_ = true;
			return;
		}


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
		if (ratchet_editor_active_) {
			UpdateRatchetEditor(fine);
			return;
		}

		// --- GLIDE modifier ---
		if (c.button.morph.is_high()) {
			UpdateGlideModifier(shift, fine);
			return;
		}

		// --- Shift: page navigation + global settings ---
		if (shift) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high()) {
					NavigatePage(i);
				}
			}
			UpdateGlobalSettings(fine);
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
			// CHAN + encoder N: cycle channel N's output type (unquantized CV → scales → Gate → Trigger)
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				CycleTypeSel(enc, dir);
				focused_ch_ = enc;
				p.shared.do_save_macro = true;
			});
			return;
		}

		// --- Dispatch to arm mode or normal step editing ---
		if (armed_ch_.has_value()) {
			UpdateArmed(fine);
		} else {
			UpdateNormal(fine);
		}
	}

private:
	void UpdateNormal(bool fine) {
		// Disarm if armed channel's page button pressed alone (spec: "press page button N again to disarm")
		// (handled above; armed_ch_ would already be nullopt here)

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
				for (auto i = 0u; i < Model::NumChans; i++) {
					if (!c.button.scene[i].is_high()) continue;
					const uint8_t gs = GlobalStep(i);
					const auto type  = p.GetData().channel[enc].type;
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

	void UpdateArmed(bool fine) {
		const auto ch   = armed_ch_.value();
		const auto type = p.GetData().channel[ch].type;

		// Disarm when the armed channel's page button is pressed alone (not CHAN held)
		if (c.button.scene[ch].just_went_high()) {
			armed_ch_ = std::nullopt;
			trigger_step_enc_turned_ = 0;
			return;
		}

		if (type == ChannelType::CV) {
			UpdateArmedCV(ch, fine);
		} else if (type == ChannelType::Gate) {
			UpdateArmedGate(ch, fine);
		} else {
			UpdateArmedTrigger(ch, fine);
		}
	}



	void UpdateArmedCV(uint8_t ch, bool fine) {
		// Enc 3 (idx 2): slider span; Enc 4 (idx 3): slider base
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t inc) {
			auto &cs = p.GetData().channel[ch];
			if (enc == 2) {
				// Span: 1,2,3,4,5,10,15
				static constexpr std::array<uint8_t, 7> spans = {1, 2, 3, 4, 5, 10, 15};
				uint8_t idx = 0;
				for (auto j = 0u; j < spans.size(); j++) {
					if (spans[j] == cs.slider_span_v) { idx = j; break; }
				}
				idx = static_cast<uint8_t>(std::clamp<int32_t>(idx + inc, 0, spans.size() - 1));
				cs.slider_span_v = spans[idx];
			} else if (enc == 3) {
				// Base: -5,0,1,2,3,4,5,6,7,8,9,10
				static constexpr std::array<int8_t, 12> bases = {-5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
				uint8_t idx = 0;
				for (auto j = 0u; j < bases.size(); j++) {
					if (bases[j] == cs.slider_base_v) { idx = j; break; }
				}
				idx = static_cast<uint8_t>(std::clamp<int32_t>(idx + inc, 0, bases.size() - 1));
				cs.slider_base_v = bases[idx];
			}
			(void)fine;
		});
	}

	void UpdateArmedGate(uint8_t ch, bool fine) {
		// Tap page button → toggle step on/off
		// Hold page button + encoder → set gate length for that step
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (i == ch) continue; // ch's button used for disarm
			if (btn.just_went_high()) {
				last_touched_step_ = i;
				if (!c.button.fine.is_high()) // tap without fine = x0x toggle
					ToggleGateStep(ch, GlobalStep(i));
			}
		}
		// Encoder while a page button is held → set gate length for that step
		ForEachEncoderInc(c, [&](uint8_t /*enc*/, int32_t inc) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (i == ch) continue;
				if (btn.is_high())
					EditGateStep(ch, GlobalStep(i), inc, fine);
			}
		});
	}

	void UpdateArmedTrigger(uint8_t ch, bool fine) {
		// Encoder while page button held → adjust ratchet/repeat count.
		// Mark each held step so release knows not to toggle (hold-to-edit, tap-to-toggle).
		ForEachEncoderInc(c, [&](uint8_t /*enc*/, int32_t inc) {
			(void)fine;
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (static_cast<uint8_t>(i) == ch) continue;
				if (btn.is_high()) {
					EditTrigStep(ch, GlobalStep(i), inc);
					trigger_step_enc_turned_ |= static_cast<uint8_t>(1u << i);
				}
			}
		});

		for (auto [i, btn] : countzip(c.button.scene)) {
			if (static_cast<uint8_t>(i) == ch) continue;
			if (btn.just_went_high()) {
				last_touched_step_ = static_cast<uint8_t>(i);
				trigger_step_enc_turned_ &= ~static_cast<uint8_t>(1u << i); // clear on fresh press
			}
			if (btn.just_went_low()) {
				if (!(trigger_step_enc_turned_ & (1u << i))) {
					ToggleTrigStep(ch, GlobalStep(i)); // tap: toggle on release
				}
				trigger_step_enc_turned_ &= ~static_cast<uint8_t>(1u << i); // clear on release
			}
		}
	}

	// --- Performance Page ---

	void UpdatePerfPage(bool fine) {
		// Exit: Play/Reset, or short Fine+Glide (handled in Common)
		if (c.button.play.just_went_high()) {
			perf_page_active_     = false;
			perf_settings_active_ = false;
			return;
		}

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
		// Exit: Fine+Glide short (handled in Common) or Play
		if (c.button.play.just_went_high()) {
			p.shared.do_save_shared   = true;
			perf_settings_active_     = false;
			return;
		}
		if (c.button.fine.is_high() && c.button.morph.just_went_high()) {
			p.shared.do_save_shared   = true;
			perf_settings_active_     = false;
			return;
		}

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
			}
		}
	}

	// --- Glide / Ratchet helpers ---

	// Offset all active gate steps for channel ch by delta (skips off/zero steps)
	void OffsetAllGateSteps(uint8_t ch, int32_t delta) {
		for (auto s = 0u; s < 64u; s++) {
			const auto val = p.GetStepValue(ch, static_cast<uint8_t>(s));
			if (val == 0) continue;
			const int32_t next = static_cast<int32_t>(val) + delta * GateStepCoarse;
			p.SetStepValue(ch, static_cast<uint8_t>(s),
			               static_cast<StepValue>(std::clamp<int32_t>(next, 1, 65535)));
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

			if (was_shift || type == ChannelType::Trigger) {
				// Shift+GLIDE long-press or Trigger channel: Ratchet Step Editor
				if (type == ChannelType::Trigger) {
					ratchet_editor_active_ = true;
					ratchet_editor_ch_     = ch;
				}
				// CV/Gate with shift+glide long-press: no effect per spec
			} else {
				// Glide Step Editor for CV or Gate
				glide_editor_active_ = true;
				glide_editor_ch_     = ch;
			}
			return;
		}

		// While no page button is pending long-press: encoder adjustments
		if (glide_longpress_btn_ == kNoLongpressBtn) {
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
	}

	// --- Glide Step Editor ---

	void UpdateGlideEditor(bool fine) {
		// Exit: Play, GLIDE press, or re-press the editor's page button
		if (c.button.play.just_went_high() || c.button.morph.just_went_high()) {
			glide_editor_active_   = false;
			p.shared.do_save_macro = true;
			return;
		}
		if (c.button.scene[glide_editor_ch_].just_went_high()) {
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
				// CW = set glide on, CCW = set glide off
				p.SetGlideFlag(ch, gs, dir > 0);
			} else if (type == ChannelType::Gate) {
				EditGateStep(ch, gs, dir, fine);
			} else {
				// Trigger: transition into Ratchet Step Editor
				glide_editor_active_   = false;
				ratchet_editor_active_ = true;
				ratchet_editor_ch_     = ch;
			}
		});
	}

	// --- Ratchet Step Editor ---

	void UpdateRatchetEditor(bool /*fine*/) {
		// Exit: Play, GLIDE press, or re-press the editor's page button
		if (c.button.play.just_went_high() || c.button.morph.just_went_high()) {
			ratchet_editor_active_ = false;
			p.shared.do_save_macro = true;
			return;
		}
		if (c.button.scene[ratchet_editor_ch_].just_went_high()) {
			ratchet_editor_active_ = false;
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

		const auto ch = ratchet_editor_ch_;
		if (p.GetData().channel[ch].type == ChannelType::Trigger) {
			// Enc N edits step N on current page
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				EditTrigStep(ch, GlobalStep(enc), dir);
			});
		}
	}

	// --- Channel Edit (Shift + CHAN) ---
	// Encoder positions aligned with Model::Sequencer::EncoderAlts:
	//   enc 0 = Direction     (StartOffset=0)
	//   enc 1 = Phase rotate  (PlayMode=1)
	//   enc 2 = Length        (SeqLength=2)  ← physical "Length" encoder
	//   enc 3 = Output delay  (PhaseOffset=3)
	//   enc 4 = Range/PW      (Range=4)
	//   enc 5 = Clock div     (ClockDiv=5)
	//   enc 6 = Type selector (Transpose=6)
	//   enc 7 = Random        (Random=7)

	void UpdateChannelEdit(bool fine) {
		// Exit: Play or CHAN press → save channel settings
		if (c.button.play.just_went_high() || c.button.bank.just_went_high()) {
			channel_edit_active_    = false;
			channel_edit_last_enc_  = 0xFF;
			p.shared.do_save_macro  = true;
			return;
		}

		// Shift + page button: page navigation
		if (c.button.shift.is_high()) {
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(i);
			}
			return;
		}

		// Page button press → focus channel
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				edit_ch_              = static_cast<uint8_t>(i);
				channel_edit_last_enc_ = 0xFF;
				return;
			}
		}

		// Encoder editing for focused channel
		auto &cs = p.GetData().channel[edit_ch_];
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			channel_edit_last_enc_ = enc;
			switch (enc) {
			case 0: // Direction (Forward / Reverse / Ping-Pong / Random)
				cs.direction = CycleDirection(cs.direction, dir);
				break;
			case 1: // Phase rotate (destructive)
				RotateChannel(edit_ch_, dir);
				break;
			case 2: // Length (1–64) — auto-navigate to page containing last step
				cs.length = static_cast<uint8_t>(
				    std::clamp<int32_t>(cs.length + dir, 1, 64));
				current_page_ = static_cast<uint8_t>((cs.length - 1u) / Model::NumChans);
				break;
			case 3: // Output delay (0–20ms)
				cs.output_delay_ms = static_cast<uint8_t>(
				    std::clamp<int32_t>(cs.output_delay_ms + dir, 0, 20));
				break;
			case 4: // Range (CV) / Pulse width (Trigger)
				if (cs.type == ChannelType::CV)
					cs.range.Inc(dir);
				else if (cs.type == ChannelType::Trigger)
					cs.pulse_width_ms = static_cast<uint8_t>(
					    std::clamp<int32_t>(cs.pulse_width_ms + dir, 1, 100));
				break;
			case 5: // Clock division
				cs.division.Inc(dir);
				break;
			case 6: // Type selector (unquantized CV → scales → Gate → Trigger, clamped)
				CycleTypeSel(edit_ch_, dir);
				break;
			case 7: // Random amount (0..1 in 0.05 increments)
				cs.random_amount = std::clamp(cs.random_amount + dir * 0.05f, 0.f, 1.f);
				break;
			}
			(void)fine;
		});
	}

	// --- Global Settings (Shift held in normal mode) ---

	void UpdateGlobalSettings(bool fine) {
		auto &d = p.GetData();
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			switch (enc) {
			case 0: // Default direction
				d.default_direction = CycleDirection(d.default_direction, dir);
				break;
			case 2: // Default range
				d.default_range.Inc(dir);
				break;
			case 5: // Default length
				d.default_length = static_cast<uint8_t>(
				    std::clamp<int32_t>(d.default_length + dir, 1, 64));
				break;
			case 6: // Internal BPM
				p.clock.bpm.Inc(dir, fine);
				break;
			default:
				break;
			}
		});
	}

public:
	void PaintLeds(const Model::Output::Buffer & /*outs*/) override {
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

		// --- Ratchet Step Editor ---
		if (ratchet_editor_active_) {
			const auto ch    = ratchet_editor_ch_;
			const bool blink = (Controls::TimeNow() >> 8) & 1u;
			for (auto i = 0u; i < Model::NumChans; i++) {
				c.SetButtonLed(i, i == ch && blink);
				c.SetEncoderLed(i, TrigStepColor(ch, GlobalStep(static_cast<uint8_t>(i))));
			}
			return;
		}

		// --- Channel Edit mode ---
		if (channel_edit_active_) {
			if (channel_edit_last_enc_ == 2) {
				// Length feedback: page buttons = page count, encoder LEDs = step fill on last page
				const auto len          = p.GetData().channel[edit_ch_].length;
				const auto last_page    = static_cast<uint8_t>((len - 1u) / Model::NumChans);
				const auto steps_on_pg  = static_cast<uint8_t>((len - 1u) % Model::NumChans + 1u);
				for (auto i = 0u; i < Model::NumChans; i++) {
					c.SetButtonLed(i, i <= last_page);
					c.SetEncoderLed(i, i < steps_on_pg ? ChannelTypeColor(edit_ch_) : Palette::dim_grey);
				}
			} else {
				for (auto i = 0u; i < Model::NumChans; i++) {
					c.SetButtonLed(i, i == edit_ch_);
					c.SetEncoderLed(i, i == edit_ch_ ? ChannelTypeColor(i) : Palette::dim_grey);
				}
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

		// --- Page button LEDs ---
		for (auto i = 0u; i < Model::NumChans; i++) {
			bool lit = false;
			if (armed && (type == ChannelType::Gate || type == ChannelType::Trigger)) {
				// x0x pattern for armed Gate/Trigger channel
				const auto sv  = p.GetStepValue(ch, GlobalStep(i));
				const auto val = static_cast<int8_t>(sv & 0xFF);
				lit = (type == ChannelType::Gate) ? (sv > 0) : (val != 0);
			} else {
				// Show focused channel's playhead; also light any held buttons for editing feedback
				const auto ph = p.GetPlayhead(focused_ch_);
				lit = (ph / Model::NumChans == current_page_ && ph % Model::NumChans == i);
				lit = lit || c.button.scene[i].is_high();
			}
			c.SetButtonLed(i, lit);
		}

		// --- Encoder LEDs ---
		for (auto i = 0u; i < Model::NumChans; i++) {
			Color col;
			if (armed && i == ch) {
				// Armed channel encoder pulses
				const bool blink = (Controls::TimeNow() >> 8) & 1u;
				col = blink ? Palette::full_white : Palette::off;
			} else {
				// Show each channel's value at the reference step (held or playhead)
				const uint8_t ref_step = p.AnyStepHeld() ? p.GetOutputStep(i) : p.GetPlayhead(i);
				col = StepColorForChannel(i, ref_step);
			}
			c.SetEncoderLed(i, col);
		}
	}
};

} // namespace Catalyst2::Ui::VoltSeq
