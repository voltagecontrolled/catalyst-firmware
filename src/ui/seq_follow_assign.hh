#pragma once

#include "conf/model.hh"
#include "conf/palette.hh"
#include "seq_common.hh"

namespace Catalyst2::Ui::Sequencer::Settings
{
// Follow Assign mode: entered from Channel settings (SHIFT+Chan, tap GLIDE).
// Persistent -- stays active after releasing all buttons.
//
// Four sub-modes cycled with FINE:
//   Blue   (0) -- CV transpose follow: source CV track pitch offsets this track
//   Orange (1) -- Gate clock, ratchets only: ratchets advance CV, repeats do not
//   Yellow (2) -- Gate clock, repeats only: repeats advance CV, ratchets do not
//   Salmon (3) -- Gate clock, both: ratchets and repeats advance CV
//
// Encoder LEDs: channel type colors (same as Bank mode), selected channel blinks in sub-mode color.
// Page buttons: lit = assigned source for the active sub-mode. Self is never selectable.
// Tap GLIDE or Play/Reset to exit.
class FollowAssign : public Usual {
	uint8_t mode = 0; // 0=blue/transpose, 1=orange/ratchets, 2=yellow/repeats, 3=salmon/both

	static constexpr std::array<Color, 4> mode_colors = {
		Palette::blue,
		Palette::orange,
		Palette::yellow,
		Palette::salmon,
	};

public:
	FollowAssign(Catalyst2::Sequencer::Interface &p, Controls &c, Abstract &main_ui, Abstract &)
		: Usual{p, c, main_ui} {
	}

	void Init() override {
		mode = 0;
	}

	void Update() override {
		// Tap GLIDE exits
		if (c.button.morph.just_went_high()) {
			SwitchUiMode(main_ui);
			return;
		}
		// Play/Reset exits
		if (c.button.play.just_went_high()) {
			p.TogglePause();
			SwitchUiMode(main_ui);
			return;
		}

		const auto chan = p.GetSelectedChannel();
		if (chan >= Model::NumChans) {
			SwitchUiMode(main_ui);
			return;
		}

		// FINE cycles through sub-modes
		if (c.button.fine.just_went_high()) {
			mode = (mode + 1) % 4;
		}

		ForEachSceneButtonJustReleased(c, [this, chan](uint8_t button) {
			if (button == chan) return; // no self-link
			if (mode == 0) {
				const auto current = p.GetTransposeSource(chan);
				p.SetTransposeSource(chan, current == (button + 1u) ? 0u : button + 1u);
			} else {
				const auto current = p.GetClockSource(chan);
				const auto new_source = current == (button + 1u) ? 0u : button + 1u;
				p.SetClockSource(chan, new_source);
				if (new_source > 0) {
					p.SetClockFollowMode(chan, mode - 1u); // 0=ratchets, 1=repeats, 2=both
				}
			}
		});
	}

	void PaintLeds(const Model::Output::Buffer &outs) override {
		ClearButtonLeds(c);

		const auto chan = p.GetSelectedChannel();
		if (chan >= Model::NumChans) {
			ClearEncoderLeds(c);
			return;
		}

		const auto mode_color = mode_colors[mode];
		const bool blink = (Controls::TimeNow() >> 8) & 1u; // ~12Hz

		// Encoder LEDs: channel type colors; selected channel blinks in sub-mode color
		for (auto i = 0u; i < Model::NumChans; i++) {
			if (i == chan) {
				c.SetEncoderLed(i, blink ? mode_color : Palette::off);
			} else {
				c.SetEncoderLed(i, p.slot.settings.GetChannelMode(i).GetColor());
			}
		}

		// Page buttons: lit = assigned source for active sub-mode
		const auto source = (mode == 0) ? p.GetTransposeSource(chan) : p.GetClockSource(chan);
		if (source > 0 && source <= Model::NumChans) {
			c.SetButtonLed(source - 1, true);
		}
	}
};
} // namespace Catalyst2::Ui::Sequencer::Settings
