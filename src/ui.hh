#pragma once

#include <new>

#include "conf/board_conf.hh"
#include "conf/model.hh"
#include "controls.hh"
#include "conf/build_options.hh"
#include "params.hh"
#include "saved_settings.hh"
#include "ui/abstract.hh"
#include "ui/dac_calibration.hh"
#include "ui/seq.hh"

#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
#include "ui/voltseq.hh"
#else
#include "ui/macro.hh"
#endif
#include "util/countzip.hh"

namespace Catalyst2::Ui
{

#ifndef __NOP
volatile uint8_t dummy;
#define __NOP() (void)dummy
#endif

class Interface {
	Params &params;
	Controls controls;

	Abstract *ui;

#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
	Ui::VoltSeq::Main macro{params.macro, controls, sequencer};
#else
	Macro::Main macro{params.macro, controls, sequencer};
#endif
	Sequencer::Main sequencer{params.sequencer, controls, macro};

	// 3-button cluster mode-switch timers (1 s hold)
	static constexpr uint32_t kClusterHoldMs = 1000u;
	Clock::Timer left_cluster_timer_{kClusterHoldMs};   // Fine + Play + Glide → Sequencer
	Clock::Timer right_cluster_timer_{kClusterHoldMs};  // Shift + Tap  + Chan  → Macro/VoltSeq
	bool left_cluster_pending_  = false;
	bool right_cluster_pending_ = false;

#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
	SavedSettings<Catalyst2::Sequencer::Data, Catalyst2::VoltSeq::Data, Catalyst2::Shared::Data> settings;
#else
	SavedSettings<Catalyst2::Sequencer::Data, Catalyst2::Macro::Data, Catalyst2::Shared::Data> settings;
#endif

public:
	Interface(Params &params)
		: params{params} {
	}

	void Start() {
		controls.Start();
		std::srand(controls.ReadSlider() + controls.ReadCv());
		Load();
		if (controls.button.shift.is_high() && controls.button.morph.is_high()) {
			if (Calibration::Dac::Calibrate(params.data.shared.dac_calibration, controls)) {
				SaveShared();
			}
		}
		StartupAnimation(controls);
		params.sequencer.Stop();
		ui = MainUI();
		ui->Init();
	}

	void Update() {
		controls.Update();
		params.shared.blinker.Update();
		params.shared.youngest_scene_button.Update(controls);

		// --- 3-button cluster mode switch (works from any UI state) ---
		{
			const auto &b = controls.button;
			const bool left_held  = b.fine.is_high() && b.play.is_high() && b.morph.is_high();
			const bool right_held = b.shift.is_high() && b.add.is_high() && b.bank.is_high();

			if (left_held && !left_cluster_pending_) {
				left_cluster_pending_ = true;
				left_cluster_timer_.SetAlarm();
			}
			if (!left_held) left_cluster_pending_ = false;

			if (right_held && !right_cluster_pending_) {
				right_cluster_pending_ = true;
				right_cluster_timer_.SetAlarm();
			}
			if (!right_held) right_cluster_pending_ = false;

			if (left_cluster_pending_ && left_cluster_timer_.Check()) {
				left_cluster_pending_ = false;
				if (params.shared.mode != Model::Mode::Sequencer) {
					params.shared.mode = Model::Mode::Sequencer;
					params.shared.do_save_shared = true;
					for (auto i = 0u; i < Model::NumChans; i++)
						params.shared.blinker.Set(i, 1, 200, 100 * i + 250);
				}
			}
			if (right_cluster_pending_ && right_cluster_timer_.Check()) {
				right_cluster_pending_ = false;
				if (params.shared.mode != Model::Mode::Macro) {
					params.shared.mode = Model::Mode::Macro;
					params.shared.do_save_shared = true;
					for (auto i = 0u; i < Model::NumChans; i++)
						params.shared.blinker.Set(i, 1, 200, 100 * i + 250);
				}
			}
		}

		ui->Common();
		ui->Update();
		if (auto new_ui = ui->NextUi()) {
			ui = new_ui.value();
			ClearEncoderEvents(controls);
			ui->Init();
		}

		if (params.shared.do_save_macro) {
			params.shared.do_save_macro = false;
			SaveMacro();
		}
		if (params.shared.do_save_seq) {
			params.shared.do_save_seq = false;
			SaveSeq();
		}
		if (params.shared.do_save_shared) {
			params.shared.do_save_shared = false;
			SaveShared();
		}
	}

	void SetOutputs(Model::Output::Buffer &outs) {
		controls.Write(outs);

		if (controls.LedsReady()) {
			ui->PaintLeds(outs);
			if (params.shared.blinker.IsSet()) {
				LedBlinker(controls, params.shared.blinker);
			}
			controls.WriteButtonLeds();
		}
	}

private:
	Abstract *MainUI() {
		if (params.shared.mode == Model::Mode::Macro) {
			return &macro;
		} else {
			return &sequencer;
		}
	}
	void SaveShared() {
		if (!settings.write(params.data.shared))
			SaveError();
	}
	void SaveMacro() {
		params.data.macro.PreSave();
		if (!settings.write(params.data.macro))
			SaveError();
		params.data.macro.PostLoad();
	}
	void SaveSeq() {
		params.data.sequencer.PreSave();
		if (!settings.write(params.data.sequencer))
			SaveError();
		params.data.sequencer.PostLoad();
	}
	void SaveError() {
		params.shared.blinker.Set(48, 3000);
	}

	void Load() {
		if (!settings.read(params.data.shared)) {
			new (&params.data.shared) Catalyst2::Shared::Data{};
		}
		if (!settings.read(params.data.sequencer)) {
			new (&params.data.sequencer) Catalyst2::Sequencer::Data{};
		}
		if (!settings.read(params.data.macro)) {
#if CATALYST_SECOND_MODE == CATALYST_MODE_VOLTSEQ
			new (&params.data.macro) Catalyst2::VoltSeq::Data{};
#else
			new (&params.data.macro) Catalyst2::Macro::Data{};
#endif
		}
		params.data.sequencer.PostLoad();
		params.data.macro.PostLoad();

		params.sequencer.Load();
#if CATALYST_SECOND_MODE != CATALYST_MODE_VOLTSEQ
		params.macro.bank.SelectBank(0);
#endif

		const auto saved_mode = params.shared.data.saved_mode;

		auto &b = controls.button;
		bool wait = false;
		if (b.play.is_high() && b.morph.is_high() && b.fine.is_high()) {
			params.shared.data.saved_mode = Model::Mode::Sequencer;
			wait = true;
		} else if (b.bank.is_high() && b.add.is_high() && b.shift.is_high()) {
			params.shared.data.saved_mode = Model::Mode::Macro;
			wait = true;
		}

		if (saved_mode != params.shared.data.saved_mode) {
			SaveShared();
		}

		while (wait && (b.play.is_high() || b.morph.is_high() || b.fine.is_high() || b.bank.is_high() ||
						b.add.is_high() || b.shift.is_high()))
		{
			controls.SetPlayLed(true);
			controls.Delay(64);
			controls.SetPlayLed(false);
			controls.Delay(64);
		}
		params.shared.mode = params.shared.data.saved_mode;
	}
};

} // namespace Catalyst2::Ui
