#pragma once

#include "conf/model.hh"
#include "conf/palette.hh"
#include "seq_common.hh"

namespace Catalyst2::Ui::Sequencer::Settings
{
// Follow Assign mode: entered from Channel settings (SHIFT+Chan, tap GLIDE).
// Persistent -- stays active after releasing all buttons.
//
// Six sub-modes cycled with FINE:
//   Blue     (0) -- CV add follow: source CV track pitch offsets this track (adds to step value)
//   Lavender (1) -- CV replace follow: source CV track pitch replaces this track's step value
//   Orange   (2) -- Gate clock, ratchets only: ratchets advance CV, repeats do not  (clock_follow_mode=0)
//   Yellow   (3) -- Gate clock, repeats only: repeats advance CV, ratchets do not   (clock_follow_mode=1)
//   Salmon   (4) -- Gate clock, both: ratchets and repeats advance CV               (clock_follow_mode=2)
//   Teal     (5) -- Gate clock, step only: advances once per gate step              (clock_follow_mode=3)
//
// Encoder LEDs: channel type colors (same as Bank mode), selected channel blinks in sub-mode color.
// Page buttons: lit = assigned source for the active sub-mode only. Self is never selectable.
// Tap GLIDE or Play/Reset to exit.
class FollowAssign : public Usual {
	uint8_t mode = 0; // 0=blue/add, 1=lavender/replace, 2=orange/ratchets, 3=yellow/repeats, 4=salmon/both, 5=teal/step

	static constexpr std::array<Color, 6> mode_colors = {
		Palette::blue,
		Palette::lavender,
		Palette::orange,
		Palette::yellow,
		Palette::salmon,
		Palette::teal,
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
			mode = (mode + 1) % 6;
		}

		ForEachSceneButtonJustReleased(c, [this, chan](uint8_t button) {
			if (button == chan) return; // no self-link
			if (mode == 0) {
				// Blue: CV add -- radio toggle, store as 1..8
				const auto current = p.GetTransposeSource(chan);
				const bool already_set = (current == button + 1u) && !p.IsTransposeReplace(chan);
				p.SetTransposeSource(chan, already_set ? 0u : button + 1u);
			} else if (mode == 1) {
				// Lavender: CV replace -- radio toggle, store as 9..16 (encoded)
				const auto current = p.GetTransposeSource(chan);
				const bool already_set = (current == button + 1u) && p.IsTransposeReplace(chan);
				p.SetTransposeSourceReplace(chan, already_set ? 0u : button + 1u);
			} else {
				// Gate clock modes: orange/yellow/salmon/teal
				const auto current_source = p.GetClockSource(chan);
				const auto current_mode = p.GetClockFollowMode(chan);
				const bool already_set = (current_source == button + 1u) && (current_mode == mode - 2u);
				const auto new_source = already_set ? 0u : button + 1u;
				p.SetClockSource(chan, new_source);
				if (new_source > 0)
					p.SetClockFollowMode(chan, mode - 2u); // 0=ratchets, 1=repeats, 2=both, 3=step only
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

		// Page buttons: lit = assigned source for active sub-mode only
		if (mode == 0) {
			// Blue: lit only when in add mode
			const auto source = p.GetTransposeSource(chan);
			if (source > 0 && source <= Model::NumChans && !p.IsTransposeReplace(chan))
				c.SetButtonLed(source - 1, true);
		} else if (mode == 1) {
			// Lavender: lit only when in replace mode
			const auto source = p.GetTransposeSource(chan);
			if (source > 0 && source <= Model::NumChans && p.IsTransposeReplace(chan))
				c.SetButtonLed(source - 1, true);
		} else {
			// Gate clock modes: lit only when source and mode both match
			const auto source = p.GetClockSource(chan);
			if (source > 0 && source <= Model::NumChans && p.GetClockFollowMode(chan) == mode - 2u)
				c.SetButtonLed(source - 1, true);
		}
	}
};
} // namespace Catalyst2::Ui::Sequencer::Settings
