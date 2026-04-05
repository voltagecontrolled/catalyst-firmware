#pragma once

#include "clock.hh"
#include "seq_common.hh"

namespace Catalyst2::Ui::Sequencer
{
class ScrubSettings : public Usual {
	static constexpr uint8_t quantize_encoder   = 0;
	static constexpr uint8_t perf_mode_encoder  = 1;
	static constexpr uint8_t width_encoder      = 2;
	static constexpr uint8_t direction_encoder  = 3;
	static constexpr uint8_t lock_encoder       = 7;

public:
	using Usual::Usual;

	void Init() override {
		c.button.fine.clear_events();
		c.button.morph.clear_events();
		c.button.play.clear_events();
	}

	void Update() override {
		// Exit: COPY+GLIDE (any duration) or Play/Reset — save on exit, not on every change
		if (c.button.play.just_went_high()) {
			p.shared.do_save_shared = true;
			SwitchUiMode(main_ui);
			return;
		}
		if (c.button.morph.is_high() && c.button.fine.just_went_high()) {
			p.shared.do_save_shared = true;
			SwitchUiMode(main_ui);
			return;
		}

		ForEachEncoderInc(c, [this](uint8_t encoder, int32_t inc) {
			if (encoder == quantize_encoder) {
				p.shared.data.quantized_scrub ^= 1u;
			} else if (encoder == perf_mode_encoder) {
				auto &mode = p.shared.data.slider_perf_mode;
				if (inc > 0 && mode < 3) { mode++; }
				else if (inc < 0 && mode > 0) { mode--; }
				// Clear beat repeat state on any mode switch to avoid stale zone index
				p.shared.beat_repeat_committed = 0xFF;
				p.shared.beat_repeat_pending = 0xFF;
			} else if (encoder == width_encoder) {
				if (p.shared.data.slider_perf_mode >= 2) {
					// In beat repeat modes, enc 3 adjusts debounce delay (transient, not saved)
					auto &d = p.shared.beat_repeat_debounce_idx;
					if (inc > 0 && d < 7) d++;
					else if (inc < 0 && d > 0) d--;
				} else {
					auto &w = p.shared.data.orbit_width;
					if (inc > 0 && w < 100) { w++; }
					else if (inc < 0 && w > 0) { w--; }
				}
			} else if (encoder == direction_encoder) {
				if (p.shared.data.slider_perf_mode == 1) {
					auto &d = p.shared.data.orbit_direction;
					if (inc > 0 && d < 3) { d++; }
					else if (inc < 0 && d > 0) { d--; }
				}
			} else if (encoder == lock_encoder) {
				DoLockToggle();
			}
		});

		// Page buttons: toggle per-track scrub ignore
		ForEachSceneButtonJustPressed(c, [this](uint8_t button) {
			p.shared.data.scrub_ignore_mask ^= (1u << button);
		});
	}

	void PaintLeds(const Model::Output::Buffer & /*outs*/) override {
		ClearEncoderLeds(c);

		// Encoder 1: quantize (orange = on, off = off)
		c.SetEncoderLed(quantize_encoder,
		                p.shared.data.quantized_scrub ? Palette::orange : Palette::off);

		// Encoder 2: slider performance mode (off = standard, green = granular,
		//            blue = beat repeat, cyan = beat repeat + triplets)
		static constexpr std::array<Color, 4> perf_mode_colors = {
		    Palette::off, Palette::green, Palette::blue, Palette::cyan};
		c.SetEncoderLed(perf_mode_encoder, perf_mode_colors[p.shared.data.slider_perf_mode]);

		// Encoder 3: granular width (orange) in granular mode; debounce delay (white) in beat repeat modes
		if (p.shared.data.slider_perf_mode >= 2) {
			const auto brightness = p.shared.beat_repeat_debounce_idx / 7.f;
			c.SetEncoderLed(width_encoder, Palette::off.blend(Palette::full_white, brightness));
		} else if (p.shared.data.orbit_width == 0) {
			c.SetEncoderLed(width_encoder, Palette::off);
		} else {
			const auto brightness = p.shared.data.orbit_width / 100.f;
			c.SetEncoderLed(width_encoder, Palette::off.blend(Palette::orange, brightness));
		}

		// Encoder 4: orbit direction — only active in granular mode
		if (p.shared.data.slider_perf_mode == 1) {
			static constexpr std::array<Color, 4> direction_colors = {
			    Palette::green, Palette::blue, Palette::orange, Palette::lavender};
			c.SetEncoderLed(direction_encoder, direction_colors[p.shared.data.orbit_direction]);
		} else {
			c.SetEncoderLed(direction_encoder, Palette::off);
		}

		// Encoder 8: lock state (red = locked, off = unlocked)
		c.SetEncoderLed(lock_encoder, phase_locked ? Palette::red : Palette::off);

		// Page buttons: scrub active (lit) vs ignored (unlit) per track
		for (auto i = 0u; i < Model::NumChans; i++) {
			c.SetButtonLed(i, static_cast<bool>((p.shared.data.scrub_ignore_mask >> i) & 1u));
		}
	}
};

} // namespace Catalyst2::Ui::Sequencer
