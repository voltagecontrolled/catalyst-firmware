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
using Catalyst2::VoltSeq::StepFlags;
using Catalyst2::VoltSeq::StepValue;
using Catalyst2::VoltSeq::VoltSeqRange;

// Encoder step sizes for VoltSeq's 0..65535 StepValue space.
// Channel::Cv::type spans 0..4500 (15V), so VoltSeq units are ~14.6× larger.
inline constexpr uint16_t CvStepCoarse = 350; // ≈ 1 semitone (25 × 14)
inline constexpr uint16_t CvStepFine   = 14;  // ≈ 1 sub-semitone unit

// Gate length step sizes (high byte 0..255 = 0..100% gate)
inline constexpr uint16_t GateStepCoarse = 5;  // ≈ 2% per detent (~50 detents for full range)
inline constexpr uint16_t GateStepFine   = 1;  // ≈ 0.4% per detent

// Chaselight color for the currently playing step in armed mode.
// Dimmed from full_white so the indicator is visible without drowning out the step colors.
inline constexpr Color ChaseWhite = Color(60, 60, 60);

// Max brightness for probability/deviation ramps — bright enough to read, not blinding.
inline constexpr Color ProbMaxWhite = Color(120, 120, 120);

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

	// Bitmask: bit i set if an encoder was turned while step i was held (armed Trigger editing).
	// Used to distinguish tap (toggle) from hold+edit (no toggle).
	uint8_t trigger_step_enc_turned_ = 0;

	// Velocity-based encoder acceleration (bypassed in fine mode).
	EncoderAccel enc_accel_{};

	// Long-press detection constants (shared by Channel Edit clear and any future long-press)


	// Deadline (Controls::TimeNow) until the length-feedback display is shown in Channel Edit.
	// Starts when encoder 2 (Length) is turned; display reverts to normal after ~600 ms.
	uint32_t     length_display_until_   = 0;
	// Deadline until the clock-division feedback display is shown in Channel Edit.
	// Starts when encoder 5 (Clock Div) is turned; display reverts to normal after ~600 ms.
	uint32_t     division_display_until_ = 0;

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
	static constexpr uint32_t kGlobalSettingsHoldTicks = Clock::MsToTicks(1000);
	static constexpr uint32_t kSaveHoldTicks           = Clock::MsToTicks(800);

	// Clear mode: FINE+PLAY held ≥kClearHoldTicks → enter; Page N = clear ch N; PLAY = exit
	bool     clear_mode_active_          = false;
	bool     clear_mode_fine_suppressed_ = false; // true while Fine remains held from the entry combo
	bool     clear_mode_play_suppressed_ = false; // true while Play remains held from the entry combo
	bool     fine_play_pending_  = false;    // armed while counting toward clear-mode threshold
	uint32_t fine_play_press_t_  = 0;
	bool     save_hold_pending_   = false;   // armed when CHAN+GLIDE both held (save gesture)
	uint32_t save_hold_start_     = 0;

	// Global settings modal: SHIFT+CHAN held ≥1 s → enter; short SHIFT+CHAN → toggle Channel Edit; Play exits.
	bool     global_settings_active_  = false;
	bool     shift_chan_hold_pending_  = false;
	bool     shift_chan_from_edit_     = false; // snapshot: were we in channel edit when hold started?
	uint32_t shift_chan_hold_start_    = 0;

	// Beat repeat debounce table (matches sequencer)
	static constexpr std::array<uint32_t, 8> kBeatRepeatDebounce = {
	    Clock::MsToTicks(50),  Clock::MsToTicks(100), Clock::MsToTicks(150), Clock::MsToTicks(250),
	    Clock::MsToTicks(400), Clock::MsToTicks(600), Clock::MsToTicks(900), Clock::MsToTicks(1500),
	};

	// ---- Lock helpers ----

	void DoLockToggle() {
		if (!phase_locked_ && !picking_up_) {
			// Unlocked: lock at current slider position
			phase_locked_ = true;
			locked_raw_   = c.ReadSlider();
		} else if (phase_locked_) {
			// Locked: unlock and enter pickup mode
			phase_locked_ = false;
			picking_up_   = true;
		} else {
			// In pickup mode: second tap cancels pickup and fully unlocks
			picking_up_ = false;
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

	// ---- Clear helpers ----
	// Page button (no Shift): clears step values and per-step flags; preserves channel settings.
	//   CV → 32768 (centre = 0V); Gate / Trigger → 0 (all off / rest); glide + probability → 0.
	void ClearChannelSteps(uint8_t ch) {
		const StepValue zero = (p.GetData().channel[ch].type == ChannelType::CV) ? 32768u : 0u;
		for (uint8_t s = 0; s < 64; s++)
			p.SetStepValue(ch, s, zero);
		p.GetData().flags[ch] = StepFlags{};   // zero per-step glide and probability
		p.shared.blinker.Set(3, 300);
	}

	// Shift + Page button: full factory reset — steps, flags, and all channel settings.
	//   ChannelSettings{} resets type to CV, so step values are always set to 32768.
	void FullResetChannel(uint8_t ch) {
		p.GetData().channel[ch] = ChannelSettings{};
		for (uint8_t s = 0; s < 64; s++)
			p.SetStepValue(ch, s, 32768u);
		p.GetData().flags[ch] = StepFlags{};
		p.shared.blinker.Set(3, 300);
	}

	// ---- Clear mode ----
	// Clear mode overlay: FINE+PLAY held ≥kClearHoldTicks enters; PLAY exits.
	// While active:
	//   Page button N          → clear steps + flags for channel N (preserve settings)
	//   Shift + Page button N  → full factory reset for channel N (steps + flags + settings)
	//   Play/Reset             → exit clear mode (no playback change)
	// All other inputs fall through to normal Update() handling.
	void UpdateClearMode() {
		// Release suppression once entry-combo buttons go low
		if (clear_mode_fine_suppressed_ && !c.button.fine.is_high())
			clear_mode_fine_suppressed_ = false;
		if (clear_mode_play_suppressed_ && !c.button.play.is_high())
			clear_mode_play_suppressed_ = false;

		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				if (c.button.shift.is_high())
					FullResetChannel(static_cast<uint8_t>(i));
				else
					ClearChannelSteps(static_cast<uint8_t>(i));
				return; // consume the press so it doesn't also trigger step editing
			}
		}
		// All other inputs (Play, encoders, Chan, etc.) fall through to normal Update() handling
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

	// Map 0..4095 slider ADC value to StepValue using the channel window [transpose, transpose+span].
	// Full slider travel = full channel window.
	StepValue SliderToStepValue(uint8_t ch, uint16_t adc) const {
		const auto &cs    = p.GetData().channel[ch];
		const float min_v = static_cast<float>(cs.transpose);
		const float max_v = min_v + cs.range.Span();
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
		const auto &cs    = p.GetData().channel[ch];
		const auto  sv    = p.GetStepValue(ch, global_step);
		const float min_v = static_cast<float>(cs.transpose);
		const float max_v = min_v + cs.range.Span();
		const float t     = static_cast<float>(sv) / 65535.f;
		const float v     = min_v + t * (max_v - min_v);
		const auto  out   = Channel::Output::from_volts(v);
		return Palette::Cv::fromOutput(p.shared.data.palette[ch], out);
	}

	// LED color for per-step randomness amount (0–100; 0 = no randomness).
	// Ramp: off → violet → grey → white.
	static Color StepProbColor(uint8_t prob) {
		if (prob == 0) return Palette::off;
		const float t = static_cast<float>(prob) / 100.f;
		if (t < 0.5f)
			return Palette::lavender.blend(Palette::grey, t * 2.f);
		return Palette::grey.blend(ProbMaxWhite, (t - 0.5f) * 2.f);
	}

	// LED color for deviation amount (int8_t signed volts; 0 = off).
	// Unipolar (+N): grey → blue → white. Bipolar (-N): salmon → red → white.
	static Color RandomAmountColor(int8_t amount_v) {
		if (amount_v == 0) return Palette::off;
		const float t = static_cast<float>(std::abs(static_cast<int>(amount_v))) / 15.f;
		if (amount_v > 0) {
			if (t < 0.5f) return Palette::grey.blend(Palette::blue, t * 2.f);
			return Palette::blue.blend(ProbMaxWhite, (t - 0.5f) * 2.f);
		}
		if (t < 0.5f) return Palette::salmon.blend(Palette::bright_red, t * 2.f);
		return Palette::bright_red.blend(ProbMaxWhite, (t - 0.5f) * 2.f);
	}

	// LED color for the Range encoder (enc4) in Channel Edit and Armed (+Shift).
	// 7 options indexed 0–6 for spans {1,2,3,4,5,10,15}V.
	static Color RangeColor(const VoltSeqRange &r) {
		static constexpr std::array<Color, 7> colors = {
			Palette::Range::Positive[0], // 1V  — blue
			Palette::Range::Positive[1], // 2V  — cyan
			Palette::Range::Positive[2], // 3V  — green
			Palette::Range::Positive[3], // 4V  — yellow
			Palette::Range::Positive[4], // 5V  — orange
			Palette::Range::Positive[5], // 10V — magenta
			Palette::full_white,         // 15V — white (full hardware range)
		};
		return colors[r.Index()];
	}

	// LED color for the Transpose encoder (enc6) in Channel Edit and Armed (+Shift).
	// Negative = warm (red/orange/pink), zero = grey, positive = cool (blue/violet/white).
	static Color TransposeColor(int8_t t) {
		if (t < 0) {
			static constexpr std::array<Color, 5> neg = {
				Palette::red,        // −5V
				Palette::bright_red, // −4V
				Palette::orange,     // −3V
				Palette::salmon,     // −2V
				Palette::pink,       // −1V
			};
			return neg[std::clamp<int>(t + 5, 0, 4)];
		}
		if (t == 0)
			return Palette::grey;
		static constexpr std::array<Color, 6> pos = {
			Palette::blue,      // +1V
			Palette::cyan,      // +2V
			Palette::teal,      // +3V
			Palette::lavender,  // +4V
			Palette::magenta,   // +5V
			Palette::full_white // +6V and above
		};
		return pos[std::clamp<int>(t - 1, 0, 5)];
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

	// Returns true when a step is at its default/empty value for its channel type.
	// Used to show dim_grey (editable but empty) vs a color (has a value) during step hold.
	bool IsStepEmpty(uint8_t ch, uint8_t global_step) const {
		const auto sv = p.GetStepValue(ch, global_step);
		switch (p.GetData().channel[ch].type) {
		case ChannelType::CV:      return sv == 32768;            // center = 0V default
		case ChannelType::Gate:    return (sv >> 8) == 0;         // gate length = 0
		case ChannelType::Trigger: return (sv & 0xFF) == 0;       // rest
		}
		return true;
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

		if (c.jack.reset.just_went_high()) {
			p.ResetExternal();
			p.clock.SyncStepClock(); // fire step 0 within <0.3ms — no audible gap on reset jack
		}
		if (c.jack.trig.just_went_high())
			p.clock.ExternalClockTick();
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

		// Read slider once for the entire Common() tick — reused by movement gate, pickup,
		// orbit machinery, and RecordSlider below.
		const uint16_t slider_raw = c.ReadSlider();

		// Slider movement gate: track whether slider is in motion.
		if (record_slider_prev_ == 0xFFFF) record_slider_prev_ = slider_raw;
		if (std::abs(static_cast<int32_t>(slider_raw) - static_cast<int32_t>(record_slider_prev_)) >= kRecordMoveThreshold) {
			record_slider_prev_  = slider_raw;
			record_active_until_ = Controls::TimeNow() + kRecordActiveTicks;
		}
		const bool record_active = Controls::TimeNow() < record_active_until_;

		// Armed-on-CV: slider belongs to recording; orbit machinery yields.
		const bool armed_on_cv = armed_ch_.has_value()
		                      && p.GetData().channel[armed_ch_.value()].type == ChannelType::CV;

		// Slider recording: on each step tick while a CV channel is armed and slider is moving.
		const bool armed_cv = armed_on_cv && !perf_page_active_ && record_active;
		if (armed_cv) {
			const auto ch = armed_ch_.value();
			if (p.clock.StepTicked() && p.IsPlaying()) {
				// Use cached slider_raw; same value RecordSlider would read from hardware.
				const auto sv = SliderToStepValue(ch, slider_raw);
				p.SetStepValue(ch, p.GetShadow(ch), sv);
			} else if (!p.IsPlaying()) {
				const auto sv = SliderToStepValue(ch, slider_raw);
				p.SetStepValue(ch, GlobalStep(last_touched_step_), sv);
			}
		}

		// ---- Slider / orbit / beat-repeat machinery (mirrors seq_common.hh) ----
		// Yielded to recording when armed on a CV channel — orbit center is frozen while recording.
		const auto perf_mode = p.shared.data.slider_perf_mode;

		// Pickup resolution runs regardless of perf_mode so lock state never gets stuck.
		if (picking_up_) {
			if (std::abs(static_cast<int32_t>(slider_raw) - static_cast<int32_t>(locked_raw_))
			    <= kPickupThreshold) {
				picking_up_ = false;
			}
		}

		if (perf_mode > 0 && !armed_on_cv) {
			const uint16_t effective_slider = (phase_locked_ || picking_up_) ? locked_raw_ : slider_raw;

			// Slider movement tracking for lock indicator
			if (std::abs(static_cast<int32_t>(slider_raw) - static_cast<int32_t>(last_slider_raw_))
			    > kSliderMoveThreshold) {
				last_slider_raw_       = slider_raw;
				last_slider_move_time_ = Controls::TimeNow();
			}

			if (perf_mode >= 2) {
				// Beat repeat: orbit_pickup entry mode — hold snapped center until slider moves away
				if (orbit_pickup_) {
					if (std::abs(static_cast<int32_t>(slider_raw) - static_cast<int32_t>(orbit_pickup_slider_))
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
						orbit_pickup_slider_              = slider_raw;
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
		// Perf Page entry is allowed from Main Mode (unarmed) and from within Perf Page itself.
		// Blocked while armed: Fine+Glide has a different meaning (glide flag editing on armed CV).
		const bool perf_entry_blocked = clear_mode_active_ || global_settings_active_ || channel_edit_active_ || armed_ch_.has_value();
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
		const bool bank_jgh  = c.button.bank.just_went_high();
		const bool play_jgh  = c.button.play.just_went_high();
		const bool shift_jgh = c.button.shift.just_went_high();
		// Note: c.button.morph.just_went_high() is consumed in Common() (perf page entry check)
		// and always reads false here — do not pre-read it.

		// --- SHIFT+CHAN: channel edit toggle or global settings ---
		// Short tap  (<1 s): toggle channel edit (exit if active, enter if not).
		// Long hold  (≥1 s): enter global settings (exits channel edit first if needed).
		// Blocked entirely while clear mode, global settings, or perf page is active.
		// The hold timer starts on press regardless of whether channel edit is active —
		// this allows a single continuous hold to go from channel edit straight to global settings.
		{
			const bool block_entry = clear_mode_active_ || global_settings_active_ || perf_page_active_;
			if (shift && bank_jgh && !block_entry && !shift_chan_hold_pending_) {
				shift_chan_hold_pending_ = true;
				shift_chan_hold_start_   = Controls::TimeNow();
				shift_chan_from_edit_    = channel_edit_active_; // snapshot entry state
			}
			if (shift_chan_hold_pending_) {
				c.button.bank.just_went_low(); // drain CHAN release events while timer is pending
				const uint32_t elapsed = Controls::TimeNow() - shift_chan_hold_start_;
				if (!shift) {
					// SHIFT released before timer: toggle channel edit
					shift_chan_hold_pending_ = false;
					if (shift_chan_from_edit_) {
						// Was in channel edit: exit it
						channel_edit_active_   = false;
						channel_edit_last_enc_ = 0xFF;
						length_display_until_  = 0;
					} else {
						// Was not in channel edit: enter it
						channel_edit_active_     = true;
						channel_edit_last_enc_   = 2;
						length_display_until_    = Controls::TimeNow() + Clock::MsToTicks(600);
						// Sync current_page_ to the focused channel's playhead page.
						current_page_ = static_cast<uint8_t>(p.GetPlayhead(edit_ch_) / Model::NumChans);
					}
				} else if (elapsed >= kGlobalSettingsHoldTicks) {
					// Long hold: enter global settings (exit channel edit if we came from there)
					shift_chan_hold_pending_ = false;
					channel_edit_active_     = false;
					channel_edit_last_enc_   = 0xFF;
					length_display_until_    = 0;
					global_settings_active_  = true;
					for (auto &b : c.button.scene) b.clear_events();
				}
			}
		}

		// --- Play / Stop / Clear-mode entry ---
		// SHIFT+PLAY:  immediate reset (any press order).
		// FINE+PLAY:   hold ≥kClearHoldTicks → enter Clear mode.
		// Plain PLAY while in an edit/modal mode: exit that mode (no playback change).
		// Plain PLAY while armed: disarm (no playback change).
		// Plain PLAY otherwise: Toggle play/stop.
		if (play_jgh) {
			if (clear_mode_active_) {
				clear_mode_active_ = false;
			} else if (global_settings_active_) {
				global_settings_active_ = false;
			} else if (shift) {
				// Shift+Play: immediate reset regardless of press order
				p.Reset();
			} else if (perf_page_active_) {
				perf_page_active_     = false;
				perf_settings_active_ = false;
				p.shared.do_save_shared = true;
			} else if (channel_edit_active_) {
				channel_edit_active_   = false;
				channel_edit_last_enc_ = 0xFF;
				length_display_until_  = 0;
			} else if (armed_ch_.has_value()) {
				armed_ch_                = std::nullopt;
				trigger_step_enc_turned_ = 0;
				picking_up_              = true;
				locked_raw_              = 0;
			} else if (fine && !fine_play_pending_ && !clear_mode_active_) {
				// Fine+Play: arm to enter clear mode
				fine_play_pending_ = true;
				fine_play_press_t_ = Controls::TimeNow();
			} else {
				p.Toggle();
				if (!p.IsPlaying() && p.GetData().play_stop_reset)
					p.Reset();
			}
		}
		// Reverse-order Shift+Play reset: Play already held when Shift goes high.
		if (shift_jgh && c.button.play.is_high() && !global_settings_active_ && !clear_mode_active_) {
			p.Reset();
		}
		// Fine+Play clear-mode poll: cancel if either button released; enter on threshold.
		// Uses level checks (!is_high) to cancel rather than edge checks, avoiding any
		// just_went_low() timing sensitivity.
		if (fine_play_pending_) {
			if (!c.button.fine.is_high() || !c.button.play.is_high()) {
				fine_play_pending_ = false;
			} else if (Controls::TimeNow() - fine_play_press_t_ >= kClearHoldTicks) {
				fine_play_pending_          = false;
				clear_mode_active_          = true;
				clear_mode_fine_suppressed_ = true;
				clear_mode_play_suppressed_ = true;
				for (auto &b : c.button.scene)
					b.clear_events();
				c.button.play.clear_events();
			}
		}

		// --- CHAN+GLIDE hold-to-save gesture ---
		{
			const bool chan_high  = c.button.bank.is_high();
			const bool glide_high = c.button.morph.is_high();
			if (!shift && (bank_jgh && glide_high) && !save_hold_pending_) {
				save_hold_pending_ = true;
				save_hold_start_   = Controls::TimeNow();
			}
			if (save_hold_pending_) {
				if (!chan_high || !glide_high) {
					save_hold_pending_ = false;
				} else if (Controls::TimeNow() - save_hold_start_ >= kSaveHoldTicks) {
					save_hold_pending_      = false;
					p.shared.do_save_macro  = true;
					p.shared.blinker.Set(6, 600);
					c.button.bank.clear_events();
					c.button.morph.clear_events();
				}
			}
		}

		// --- Mode switch: if mode was changed externally, yield to sequencer ---
		if (p.shared.mode == Model::Mode::Sequencer) {
			perf_page_active_        = false;
			perf_settings_active_    = false;
			channel_edit_active_     = false;
			clear_mode_active_       = false;
			global_settings_active_   = false;
			shift_chan_hold_pending_  = false;
			fine_play_pending_        = false;
			SwitchUiMode(main_ui);
			return;
		}

		// --- Clear mode overlay: page buttons clear channels; everything else falls through normally ---
		if (clear_mode_active_)
			UpdateClearMode();

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
						armed_ch_                = std::nullopt;
						trigger_step_enc_turned_ = 0;
						picking_up_              = true;
						locked_raw_              = 0;
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

		// SHIFT+Glide: enc 0–7 adjust per-step randomness amount for steps 0–7.
		if (c.button.shift.is_high() && c.button.morph.is_high()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				p.AdjustStepProbability(ch, GlobalStep(enc), dir);
			});
			return;
		}

		// Glide held + encoder N: adjust per-step glide amount for step N on this channel.
		// Fully CCW = 0 = no glide. LED brightness tracks amount.
		if (c.button.morph.is_high()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				p.AdjustGlideAmount(ch, GlobalStep(enc), dir);
			});
			return;
		}

		if (c.button.shift.is_high()) {
			// Shift held: page navigation + enc 4 (Range) / enc 6 (Transpose) for CV channels.
			// Adjusting Range also changes the slider recording window immediately.
			for (auto [i, btn] : countzip(c.button.scene)) {
				if (btn.just_went_high())
					NavigatePage(static_cast<uint8_t>(i));
			}
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				if (enc == 4) {
					cs.range.Inc(dir);
					const auto max_xpose = static_cast<int8_t>(10 - static_cast<int>(cs.range.Span()));
					cs.transpose = std::clamp<int8_t>(cs.transpose, -5, max_xpose);
				} else if (enc == 6) {
					const auto max_xpose = static_cast<int8_t>(10 - static_cast<int>(cs.range.Span()));
					cs.transpose = std::clamp<int8_t>(
					    static_cast<int8_t>(cs.transpose + static_cast<int8_t>(dir)), -5, max_xpose);
				}
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
		// SHIFT+Glide: enc 0–7 adjust per-step randomness amount for steps 0–7.
		if (c.button.shift.is_high() && c.button.morph.is_high()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				p.AdjustStepProbability(ch, GlobalStep(enc), dir);
			});
			return;
		}

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
		// SHIFT+Glide: enc 0–7 adjust per-step randomness amount for steps 0–7.
		if (c.button.shift.is_high() && c.button.morph.is_high()) {
			ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
				p.AdjustStepProbability(ch, GlobalStep(enc), dir);
			});
			return;
		}

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

	void UpdatePerfPage(bool /*fine*/) {
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

		// Page buttons: toggle per-channel orbit follow mask — save deferred to Play/Reset exit
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high())
				p.shared.data.scrub_ignore_mask ^= static_cast<uint8_t>(1u << i);
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
			case 7: { // Lock toggle — state synced immediately; flash write deferred to Play/Reset exit
				// Directional: CW = lock, CCW = unlock. Idempotent — no surprise multi-toggles
				// from fast spinning. Flash write deferred to Play/Reset exit.
				if (inc > 0 && !phase_locked_ && !picking_up_) {
					phase_locked_              = true;
					locked_raw_                = c.ReadSlider();
					p.shared.data.phase_locked = true;
					p.shared.data.locked_raw   = locked_raw_;
					lock_toggle_time_          = Controls::TimeNow();
				} else if (inc < 0 && (phase_locked_ || picking_up_)) {
					phase_locked_              = false;
					picking_up_                = false;
					p.shared.data.phase_locked = false;
					lock_toggle_time_          = Controls::TimeNow();
				}
				break;
			}
			default:
				break;
			}
		});

		// Page buttons: toggle per-channel orbit follow mask — save deferred to Play/Reset exit
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high())
				p.shared.data.scrub_ignore_mask ^= static_cast<uint8_t>(1u << i);
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
		// Page button held + encoder: Gate = per-step ratchet edit; CV = adjust glide time
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.is_high()) {
				const auto step_btn = static_cast<uint8_t>(i);
				ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
					if (p.GetData().channel[enc].type == ChannelType::Gate)
						EditGateRatchet(enc, GlobalStep(step_btn), dir);
					else if (p.GetData().channel[enc].type == ChannelType::CV)
						p.AdjustGlideAmount(enc, GlobalStep(step_btn), dir);
				});
				return;
			}
		}

		// No step held: encoder adjustments apply channel-level parameters
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			auto &cs = p.GetData().channel[enc];
			if (shift) {
				// SHIFT+GLIDE + enc: offset all Trigger steps ratchet/repeat
				if (cs.type == ChannelType::Trigger)
					OffsetAllTrigSteps(enc, dir);
			} else {
				// GLIDE + enc: offset all steps' glide amounts within channel length
				if (cs.type == ChannelType::CV) {
					p.OffsetAllGlideAmounts(enc, dir);
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

	// --- Channel Edit (Shift + CHAN) ---
	// Encoder positions match physical panel labels (1-indexed on panel = 0-indexed here):
	//   enc 0 = Output delay  (Start)
	//   enc 1 = Direction     (Dir.)
	//   enc 2 = Length        (Length)
	//   enc 3 = Phase rotate  (Phase)  — rotates only active steps [0, length-1]
	//   enc 4 = Range         (Range)      — CV only; inactive for Gate/Trigger
	//   enc 5 = Clock div    (BPM/Clock Div)
	//   enc 6 = Transpose    (Transpose)  — CV only; inactive for Gate/Trigger
	//   enc 7 = Random       (Random)     — CV only

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

		// Page button: tap = focus channel and show length passively for 600 ms.
		// Channel clearing is done via SHIFT+PLAY → Clear Mode (not here).
		for (auto [i, btn] : countzip(c.button.scene)) {
			if (btn.just_went_high()) {
				edit_ch_               = static_cast<uint8_t>(i);
				channel_edit_last_enc_ = 2; // pre-select length encoder for passive display
				length_display_until_  = Controls::TimeNow() + Clock::MsToTicks(600);
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
				length_display_until_   = Controls::TimeNow() + Clock::MsToTicks(600);
				division_display_until_ = 0;
				break;
			}
			case 3: // Phase rotate — rotates only active steps [0, length-1]
				RotateChannel(edit_ch_, dir);
				break;
			case 4: // Range — CV only; inactive for Gate/Trigger
				if (cs.type == ChannelType::CV) {
					cs.range.Inc(dir);
					// Clamp transpose so window stays within hardware limits
					const auto max_xpose = static_cast<int8_t>(10 - static_cast<int>(cs.range.Span()));
					cs.transpose = std::clamp<int8_t>(cs.transpose, -5, max_xpose);
				}
				break;
			case 5: // Clock division — steps through shared musical table (see Clock::DivisionTables)
				Clock::Divider::Step(cs.division, Clock::DivisionTables::VoltSeq, dir);
				division_display_until_ = Controls::TimeNow() + Clock::MsToTicks(600);
				length_display_until_   = 0;
				break;
			case 6: // Transpose (±1V per detent, whole volts) — CV only; inactive for Gate/Trigger
				if (cs.type == ChannelType::CV) {
					const auto max_xpose = static_cast<int8_t>(10 - static_cast<int>(cs.range.Span()));
					cs.transpose = std::clamp<int8_t>(
					    static_cast<int8_t>(cs.transpose + static_cast<int8_t>(dir)), -5, max_xpose);
				}
				break;
			case 7: // Deviation amount (±1V per detent, −15..+15) — CV only; inactive for Gate/Trigger
				if (cs.type == ChannelType::CV)
					cs.random_amount_v = static_cast<int8_t>(
					    std::clamp<int32_t>(cs.random_amount_v + dir, -15, 15));
				break;
			}
			(void)fine;
		});
	}

	// --- Global Settings modal (SHIFT+CHAN held ≥2 s; exits on Play) ---
	// Panel assignments:
	//   Panel 1 (Start)       enc 0 — Play/Stop reset mode (clamped off→on; LED red=on, off=off)
	//   Panel 3 (Length)      enc 2 — Master loop length (0=off, 1–64 steps; LED off=0, orange=other, red=8/16/32/48/64)
	//   Panel 6 (BPM/Clk Div) enc 5 — Internal BPM

	void UpdateGlobalSettings(bool fine) {
		auto &d = p.GetData();

		// Encoders
		ForEachEncoderInc(c, [&](uint8_t enc, int32_t dir) {
			switch (enc) {
			case 0: { // Panel 1 (Start): Play/Stop reset mode — clamped off/on
				const int32_t v = std::clamp<int32_t>((d.play_stop_reset ? 1 : 0) + dir, 0, 1);
				d.play_stop_reset = (v == 1);
				break;
			}
			case 2: // Panel 3 (Length): Master loop length — 0=off, 1–64 16th-note steps
				d.master_loop_length =
				    static_cast<uint8_t>(std::clamp<int32_t>(d.master_loop_length + dir, 0, 64));
				break;
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
		// Play LED: lit while playing. Modal modes override with a slow blink so
		// the user knows Play will exit the mode rather than play/stop.
		// Clear mode overrides further below with its own blink pattern.
		c.SetPlayLed(p.IsPlaying());
		const bool in_modal = channel_edit_active_ || armed_ch_.has_value() || global_settings_active_ || perf_page_active_;
		if (in_modal)
			c.SetPlayLed((Controls::TimeNow() >> 11u) & 0x01u); // ~1.5 Hz slow blink

		// Clear mode page-button blink is handled inline in the page LED loop below.
		// Play LED and encoder LEDs render normally so the user can see playback state.

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
			} else if (Controls::TimeNow() <= division_display_until_) {
				// Clock-div feedback: same LED position as CatSeq for the same musical division.
				// Index maps enc 0 = quarter note … enc 7 = quadruple whole (see Clock::DivisionTables).
				// Shown for 600 ms after the last encoder 5 (Clock Div) turn, then reverts.
				ClearEncoderLeds(c);
				ClearButtonLeds(c);
				const auto idx = Clock::Divider::IndexOf(Clock::DivisionTables::VoltSeq,
				                                         p.GetData().channel[edit_ch_].division.Read());
				SetLedsClockDiv(c, idx + 1);
			} else {
				// Encoder LEDs match panel layout:
				//   enc 0 (Start/delay): white brightness = delay amount
				//   enc 1 (Dir):  color = current direction
				//   enc 4 (Range): range color — CV only; off for Gate/Trigger
				//   enc 5 (Clock Div): active color = divided, dim_grey = 1/1
				//   enc 6 (Transpose): transpose color — CV only; off for Gate/Trigger
				//   enc 7 (Random): white brightness = random amount
				// Page buttons: focused channel lit solid; current playing step blinks (chaselight).
				static constexpr std::array<Color, 4> dir_col = {
				    Palette::green,  // Forward
				    Palette::orange, // Reverse
				    Palette::yellow, // PingPong
				    Palette::magenta // Random
				};
				const auto &cs       = p.GetData().channel[edit_ch_];
				const bool  cv_ch    = (cs.type == ChannelType::CV);
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
					if      (i == 0) col = Palette::off.blend(Palette::full_white, cs.output_delay_ms / 20.f);
					else if (i == 1) col = dir_col[static_cast<uint8_t>(cs.direction)];
					else if (i == 4) col = cv_ch ? RangeColor(cs.range)          : Palette::off;
					else if (i == 5) col = Palette::Setting::active;
					else if (i == 6) col = cv_ch ? TransposeColor(cs.transpose)  : Palette::off;
					else if (i == 7) col = cv_ch ? RandomAmountColor(cs.random_amount_v) : Palette::off;
					c.SetEncoderLed(i, col);
				}
			}
			return;
		}

		// --- Global Settings modal ---
		// Page buttons: lit = that channel is the reset leader.
		// Encoder LEDs:
		//   enc 0 (Panel 1, Start)       = play/stop reset mode → red=on, off=off
		//   enc 2 (Panel 3, Length)      = master loop length   → off=0, orange=1–64, red=8/16/32/48/64
		//   enc 5 (Panel 6, BPM/Clk Div) = BPM                 → color zone, pulses with clock phase
		//   all others: off (unassigned)
		if (global_settings_active_) {
			const auto &d       = p.GetData();
			const auto  ph_foc  = p.GetPlayhead(focused_ch_);
			const bool  on_pg   = (ph_foc / Model::NumChans == current_page_);
			const uint8_t chase = on_pg ? static_cast<uint8_t>(ph_foc % Model::NumChans) : 0xFF;
			const bool  blink   = (Controls::TimeNow() >> 8) & 1u;
			for (auto i = 0u; i < Model::NumChans; i++) {
				const bool lit = (i == chase) ? blink : false;
				c.SetButtonLed(i, lit);
			}
			for (auto i = 0u; i < Model::NumChans; i++) {
				Color col = Palette::off;
				switch (i) {
				case 0:
					col = d.play_stop_reset ? Palette::red : Palette::off;
					break;
				case 2: {
					const auto mll = d.master_loop_length;
					if (mll == 0)
						col = Palette::off;
					else if (mll % 8 == 0)
						col = Palette::red;    // bar-aligned: 8, 16, 32, 48, 64
					else
						col = Palette::orange; // non-bar value
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

		// --- CHAN+GLIDE save gesture in progress: fast pulse on all page buttons ---
		if (save_hold_pending_) {
			const bool blink = (Controls::TimeNow() >> 7u) & 0x01u; // ~12 Hz
			for (auto i = 0u; i < Model::NumChans; i++) {
				c.SetButtonLed(i, blink);
				c.SetEncoderLed(i, Palette::off);
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
		const auto ph_focus = armed ? p.GetPlayhead(ch) : p.GetPlayhead(focused_ch_);
		const bool on_focus_page = (ph_focus / Model::NumChans == current_page_);
		const uint8_t chase_btn = on_focus_page ? static_cast<uint8_t>(ph_focus % Model::NumChans) : 0xFF;
		const bool chase_blink = (Controls::TimeNow() >> 8) & 1u;

		for (auto i = 0u; i < Model::NumChans; i++) {
			bool lit = false;
			if (clear_mode_active_) {
				// Clear mode: all page buttons blink to signal the mode is active
				lit = (Controls::TimeNow() >> 10u) & 0x01u; // ~3 Hz
			} else if (armed_gate_trig) {
				// x0x on/off pattern for armed Gate/Trigger, with chaselight
				const auto sv = p.GetStepValue(ch, GlobalStep(i));
				lit = (type == ChannelType::Gate) ? (static_cast<uint8_t>(sv >> 8) > 0)
				                                  : (static_cast<int8_t>(sv & 0xFF) != 0);
				if (i == chase_btn)
					lit = chase_blink;
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
			// SHIFT+Glide: show per-step randomness amount ramp (violet→grey→white).
			if (c.button.shift.is_high() && c.button.morph.is_high()) {
				for (auto i = 0u; i < Model::NumChans; i++) {
					const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
					c.SetEncoderLed(i, StepProbColor(p.GetData().flags[ch].step_prob[gs]));
				}
			} else if (type == ChannelType::Gate && c.button.morph.is_high()) {
				// Armed Gate + Glide held: show per-step ratchet counts instead of gate state
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
			// CV armed + SHIFT+Glide: show per-step randomness amount ramp (violet→grey→white).
			if (c.button.shift.is_high() && c.button.morph.is_high()) {
				for (auto i = 0u; i < Model::NumChans; i++) {
					const uint8_t gs = GlobalStep(static_cast<uint8_t>(i));
					c.SetEncoderLed(i, StepProbColor(p.GetData().flags[ch].step_prob[gs]));
				}
			} else if (c.button.shift.is_high()) {
				// CV armed + Shift held: show only enc4 (Range) and enc6 (Transpose); all others dark.
				const auto &cs = p.GetData().channel[ch];
				for (auto i = 0u; i < Model::NumChans; i++)
					c.SetEncoderLed(i, Palette::off);
				c.SetEncoderLed(4, RangeColor(cs.range));
				c.SetEncoderLed(6, TransposeColor(cs.transpose));
			} else if (c.button.morph.is_high()) {
				// CV armed + Glide held: each encoder shows its step's glide amount as brightness
				for (auto i = 0u; i < Model::NumChans; i++) {
					const uint8_t gs   = GlobalStep(static_cast<uint8_t>(i));
					const float    amt = p.GetData().flags[ch].glide[gs] / 255.f;
					c.SetEncoderLed(i, Palette::off.blend(Palette::full_white, amt));
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
			const uint8_t gs = GlobalStep(last_touched_step_);
			if (c.button.morph.is_high()) {
				// Step held + Glide: show per-step glide amount as brightness for each CV channel;
				// Gate/Trigger show off (glide not applicable).
				for (auto i = 0u; i < Model::NumChans; i++) {
					if (p.GetData().channel[i].type == ChannelType::CV) {
						const float amt = p.GetData().flags[i].glide[gs] / 255.f;
						c.SetEncoderLed(i, Palette::off.blend(Palette::full_white, amt));
					} else {
						c.SetEncoderLed(i, Palette::off);
					}
				}
			} else {
				// Step held for editing:
				//   - in-range, non-empty value: channel step color
				//   - in-range, empty value:     dim_grey (editable but at default)
				//   - out of range:              off (not editable)
				for (auto i = 0u; i < Model::NumChans; i++) {
					if (gs >= p.GetData().channel[i].length) {
						c.SetEncoderLed(i, Palette::off);
					} else if (IsStepEmpty(i, gs)) {
						c.SetEncoderLed(i, Palette::very_dim_grey);
					} else {
						c.SetEncoderLed(i, StepColorForChannel(i, gs));
					}
				}
			}
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
