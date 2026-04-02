#pragma once

#include "controls.hh"

#include "seq_common.hh"

namespace Catalyst2::Ui::Sequencer
{
class Morph : public Usual {
public:
	using Usual::Usual;

	void Init() override {
		if (!p.IsChannelSelected()) {
			p.SelectChannel();
		}
	}

	void Update() override {
		ForEachEncoderInc(c, [this](uint8_t encoder, int32_t inc) { OnEncoderInc(encoder, inc); });
		ForEachSceneButtonJustReleased(c, [this](uint8_t button) { OnSceneButtonRelease(button); });

		if (!c.button.morph.is_high() || c.button.shift.is_high() || c.button.fine.is_high()) {
			SwitchUiMode(main_ui);
			return;
		}
	}

	void OnEncoderInc(uint8_t encoder, int32_t inc) {
		p.IncStepModifier(encoder, inc);
	}

	void OnSceneButtonRelease(uint8_t button) {
		p.SelectPage(button);
	}

	void PaintLeds(const Model::Output::Buffer &outs) override {
		ClearButtonLeds(c);

		const auto playheadpage = p.GetPlayheadPage();
		uint8_t page;
		if (p.IsPageSelected()) {
			page = p.GetSelectedPage();
			BlinkSelectedPage(page);
		} else {
			page = playheadpage;
			c.SetButtonLed(playheadpage, true);
		}
		const uint8_t step_offset = Catalyst2::Sequencer::SeqPageToStep(page);
		const auto chan = p.GetSelectedChannel();
		const auto is_gate = p.slot.settings.GetChannelMode(chan).IsGate();

		for (auto i = 0u; i < Model::NumChans; i++) {
			const auto &step = p.GetStep(step_offset + i);
			Color color;
			if (is_gate) {
				color = Palette::Ratchet::color(step.ReadRatchetRepeatCount(), step.IsRepeat());
			} else {
				color = Palette::Morph::color(step.ReadMorph());
			}

			Color seqhead_col = Palette::SeqHead::active;
			if (p.slot.settings.GetChannelMode(i).IsMuted())
				seqhead_col = Palette::SeqHead::mute;

			PaintStep(page, i, color, seqhead_col);
		}
	}
};

} // namespace Catalyst2::Ui::Sequencer
