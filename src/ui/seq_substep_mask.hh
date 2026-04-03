#pragma once

#include "clock.hh"
#include "seq_common.hh"

namespace Catalyst2::Ui::Sequencer
{
class SubStepMask : public Usual {
	Abstract &seq_main;
	uint8_t focused_step = Model::NumChans; // Model::NumChans = no step focused
	uint32_t last_touch_time = 0;

	static constexpr uint32_t count_window_ticks = Clock::MsToTicks(300);

	// Total active sub-steps for a step: ratchet subdivisions or repeat ticks.
	static uint32_t SubStepCount(const Catalyst2::Sequencer::Step &step) {
		if (step.IsRepeat())
			return step.ReadRatchetRepeatCount() + 1u;
		return static_cast<uint32_t>(step.ReadRetrig()) + 1u;
	}

public:
	SubStepMask(Catalyst2::Sequencer::Interface &p, Controls &c, Abstract &morph_ui, Abstract &seq_main)
		: Usual{p, c, morph_ui}
		, seq_main{seq_main} {
	}

	void Init() override {
		focused_step = Model::NumChans;
		last_touch_time = 0;
	}

	void Update() override {
		// Tap Tempo → toggle off (no need to hold Glide; mode persists until explicitly exited)
		if (c.button.add.just_went_high()) {
			SwitchUiMode(seq_main);
			return;
		}

		// Play/Reset → perform play action and exit to Main
		if (c.button.play.just_went_high()) {
			p.TogglePause();
			SwitchUiMode(seq_main);
			return;
		}

		// Encoder turns: focus step and optionally adjust ratchet/repeat count
		ForEachEncoderInc(c, [this](uint8_t encoder, int32_t inc) {
			const auto now = Controls::TimeNow();
			const bool within_window = (focused_step == encoder) &&
			                           (now - last_touch_time) < count_window_ticks;
			focused_step = encoder;
			last_touch_time = now;
			if (within_window) {
				p.IncStepModifier(encoder, inc);
			}
		});

		// Page buttons toggle sub-steps on the focused step
		if (focused_step < Model::NumChans) {
			const auto page = p.IsPageSelected() ? p.GetSelectedPage() : p.GetPlayheadPage();
			const auto step_offset = Catalyst2::Sequencer::SeqPageToStep(page);
			const auto step = p.GetStep(step_offset + focused_step);
			const auto count = SubStepCount(step);

			ForEachSceneButtonJustPressed(c, [this, count](uint8_t button) {
				if (count > 1 && button < count) {
					p.ToggleSubStepMask(focused_step, button);
				}
			});
		}
	}

	void PaintLeds(const Model::Output::Buffer &outs) override {
		const auto page = p.IsPageSelected() ? p.GetSelectedPage() : p.GetPlayheadPage();
		const auto step_offset = Catalyst2::Sequencer::SeqPageToStep(page);

		// Encoder LEDs: focused step blinks yellow (or shows ratchet/repeat feedback
		// during the 300ms count-adjustment window); others show normal ratchet/repeat color
		for (auto i = 0u; i < Model::NumChans; i++) {
			const auto step = p.GetStep(step_offset + i);
			Color color;
			if (i == focused_step) {
				const auto now = Controls::TimeNow();
				const bool in_window = (now - last_touch_time) < count_window_ticks;
				if (in_window) {
					color = Palette::Ratchet::color(step.ReadRatchetRepeatCount(), step.IsRepeat());
				} else {
					color = ((now >> 8) & 1u) ? Palette::yellow : Palette::off;
				}
			} else {
				color = Palette::Ratchet::color(step.ReadRatchetRepeatCount(), step.IsRepeat());
			}

			const auto seqhead_col = p.slot.settings.GetChannelMode(i).IsMuted()
			                           ? Palette::SeqHead::mute
			                           : Palette::SeqHead::active;
			PaintStep(page, i, color, seqhead_col);
		}

		// Page buttons: show sub-step mask for focused step, or all lit if no step focused.
		// During the count-adjustment window, masked-off-but-in-range buttons blink rapidly
		// so the total count is visible even when some sub-steps are silenced.
		if (focused_step < Model::NumChans) {
			const auto now = Controls::TimeNow();
			const auto step = p.GetStep(step_offset + focused_step);
			const auto count = SubStepCount(step);
			const auto mask = step.ReadSubStepMask();
			const bool in_window = (now - last_touch_time) < count_window_ticks;
			const bool dim_blink = (now >> 7) & 1u; // ~23 Hz blink appears dim

			for (auto i = 0u; i < Model::NumChans; i++) {
				bool lit;
				if (count <= 1 || i >= count) {
					lit = false;
				} else if ((mask >> i) & 1u) {
					lit = true;                       // active sub-step: always on
				} else {
					lit = in_window && dim_blink;     // masked off: blink during adjustment
				}
				c.SetButtonLed(i, lit);
			}
		} else {
			for (auto i = 0u; i < Model::NumChans; i++) {
				c.SetButtonLed(i, true);
			}
		}
	}
};

} // namespace Catalyst2::Ui::Sequencer
