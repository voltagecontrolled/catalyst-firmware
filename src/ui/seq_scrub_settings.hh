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
		// Exit: COPY+GLIDE (any duration) or Play/Reset
		if (c.button.play.just_went_high()) {
			SwitchUiMode(main_ui);
			return;
		}
		if (c.button.fine.is_high() && c.button.morph.just_went_high()) {
			SwitchUiMode(main_ui);
			return;
		}

		ForEachEncoderInc(c, [this](uint8_t encoder, int32_t inc) {
			if (encoder == quantize_encoder) {
				p.shared.data.quantized_scrub ^= 1u;
				p.shared.do_save_shared = true;
			} else if (encoder == perf_mode_encoder) {
				auto &mode = p.shared.data.slider_perf_mode;
				if (inc > 0 && mode < 3) { mode++; p.shared.do_save_shared = true; }
				else if (inc < 0 && mode > 0) { mode--; p.shared.do_save_shared = true; }
			} else if (encoder == width_encoder) {
				auto &w = p.shared.data.orbit_width;
				if (inc > 0 && w < 100) { w++; p.shared.do_save_shared = true; }
				else if (inc < 0 && w > 0) { w--; p.shared.do_save_shared = true; }
			} else if (encoder == direction_encoder) {
				auto &d = p.shared.data.orbit_direction;
				if (inc > 0) { d = (d + 1) % 4; p.shared.do_save_shared = true; }
				else if (inc < 0) { d = (d + 3) % 4; p.shared.do_save_shared = true; }
			} else if (encoder == lock_encoder) {
				DoLockToggle();
			}
		});

		// Page buttons: toggle per-track scrub ignore
		ForEachSceneButtonJustPressed(c, [this](uint8_t button) {
			p.shared.data.scrub_ignore_mask ^= (1u << button);
			p.shared.do_save_shared = true;
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

		// Encoder 3: granular width (off at 0, dim→bright orange as width increases)
		if (p.shared.data.orbit_width == 0) {
			c.SetEncoderLed(width_encoder, Palette::off);
		} else {
			const auto brightness = p.shared.data.orbit_width / 100.f;
			c.SetEncoderLed(width_encoder, Palette::off.blend(Palette::orange, brightness));
		}

		// Encoder 4: orbit direction (green=fwd, blue=bck, orange=ping-pong, lavender=random)
		static constexpr std::array<Color, 4> direction_colors = {
		    Palette::green, Palette::blue, Palette::orange, Palette::lavender};
		c.SetEncoderLed(direction_encoder, direction_colors[p.shared.data.orbit_direction]);

		// Encoder 8: lock state (red = locked, off = unlocked)
		c.SetEncoderLed(lock_encoder, phase_locked ? Palette::red : Palette::off);

		// Page buttons: scrub active (lit) vs ignored (unlit) per track
		for (auto i = 0u; i < Model::NumChans; i++) {
			c.SetButtonLed(i, static_cast<bool>((p.shared.data.scrub_ignore_mask >> i) & 1u));
		}
	}
};

} // namespace Catalyst2::Ui::Sequencer
