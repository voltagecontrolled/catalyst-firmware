#pragma once

#include "abstract.hh"
#include "clock.hh"
#include "params.hh"

namespace Catalyst2::Ui::Sequencer
{
inline void PlayModeLedAnimation(Controls &c, Catalyst2::Sequencer::Settings::PlayMode::Mode pm) {
	using namespace Palette::Setting;
	static constexpr auto animation_duration = static_cast<float>(Clock::MsToTicks(1000));
	const auto time_now = Controls::TimeNow();
	auto phase = (time_now / animation_duration);
	phase -= static_cast<uint32_t>(phase);
	Color col;

	using enum Catalyst2::Sequencer::Settings::PlayMode::Mode;

	if (pm == Forward) {
		col = playmode_fwd.blend(Palette::off, 1.f - phase);
	} else if (pm == Backward) {
		col = playmode_bck.blend(Palette::off, phase);
	} else if (pm == PingPong) {
		if (phase < .5f) {
			phase *= 2.f;
			col = playmode_bck.blend(Palette::off, phase);
		} else {
			phase -= .5f;
			phase *= 2.f;
			col = playmode_fwd.blend(Palette::off, 1.f - phase);
		}
	} else if (pm == Random0) {
		col = Palette::green;
	} else {
		uint8_t cphase = phase * 6;
		col = Palette::Random::color(cphase << cphase);
	}
	c.SetEncoderLed(Model::Sequencer::EncoderAlts::PlayMode, col);
}

class Usual : public Abstract {
	bool initialized_from_shared = false;

public:
	Catalyst2::Sequencer::Interface &p;

	Usual(Catalyst2::Sequencer::Interface &p, Controls &c, Abstract &main_ui)
		: Abstract{c, main_ui}
		, p{p} {
	}
	void Common() final {
		p.seqclock.external = c.sense.trig.is_high();

		if (c.jack.reset.just_went_high()) {
			if (p.slot.clock_sync_mode.mode == Clock::Sync::Mode::SYNCED) {
				p.Reset();
			} else {
				p.Play();
			}
		} else if (c.jack.reset.just_went_low()) {
			if (p.slot.clock_sync_mode.mode == Clock::Sync::Mode::DIN_SYNC) {
				p.Stop(true);
			}
		}

		if (c.jack.trig.just_went_high()) {
			p.Trig();
		}

		// One-time init: restore persisted lock state from Shared::Data on first tick
		if (!initialized_from_shared) {
			initialized_from_shared = true;
			if (p.shared.data.phase_locked) {
				phase_locked = true;
				locked_raw = p.shared.data.locked_raw;
				locked_phase = (locked_raw + c.ReadCv()) / 4096.f;
				picking_up = true;
			}
		}

		// COPY+GLIDE: short release = lock toggle; 3s hold = scrub settings entry.
		// Trigger on whichever button is pressed second so both press orders work.
		// Timer is in p.shared so it survives Morph→Main mode switches (GLIDE-first path).
		const bool both_held = c.button.fine.is_high() && c.button.morph.is_high();
		const bool either_just_pressed = c.button.fine.just_went_high() || c.button.morph.just_went_high();
		if (both_held && either_just_pressed && !p.shared.scrub_hold_pending) {
			p.shared.scrub_hold_pending = true;
			p.shared.scrub_hold_start = Controls::TimeNow();
		}
		if (p.shared.scrub_hold_pending) {
			if (!c.button.fine.is_high()) {
				p.shared.scrub_hold_pending = false; // COPY released first — abort, no toggle
			} else if (c.button.morph.just_went_low()) {
				p.shared.scrub_hold_pending = false;
				DoLockToggle(); // GLIDE released with COPY still held — short press
			} else if (Controls::TimeNow() - p.shared.scrub_hold_start >= scrub_settings_hold_ticks) {
				p.shared.scrub_hold_pending = false;
				p.shared.scrub_settings_entry_requested = true;
			}
		}

		if (picking_up) {
			if (std::abs((int32_t)c.ReadSlider() - (int32_t)locked_raw) <= pickup_threshold) {
				picking_up = false;
			}
		}

		// Slider movement tracking for indicator (active-movement-only flash)
		const auto slider_now = c.ReadSlider();
		if (std::abs((int32_t)slider_now - (int32_t)last_slider_raw) > slider_move_threshold) {
			last_slider_raw = slider_now;
			last_slider_move_time = Controls::TimeNow();
		}

		// Phase computation
		if (p.shared.data.slider_perf_mode > 0) {
			// When locked/picking-up, treat locked_raw as the effective slider position
			const uint16_t effective_slider = (phase_locked || picking_up) ? locked_raw : slider_now;

			// Orbit center: in beat repeat entry pickup mode, hold at the playhead-snapped value
			// until the slider moves away from where it was on shift release. This lets the user
			// time the loop start step rhythmically (release shift on the beat) rather than
			// having to nail the slider position within a zone. Slider takes over once it moves.
			if (orbit_pickup && p.shared.data.slider_perf_mode >= 2) {
				if (std::abs((int32_t)slider_now - (int32_t)orbit_pickup_slider) > pickup_threshold) {
					orbit_pickup = false;
					p.shared.orbit_center = static_cast<float>(effective_slider) / 4095.f;
				} else {
					p.shared.orbit_center = orbit_pickup_center;
				}
			} else {
				p.shared.orbit_center = static_cast<float>(effective_slider) / 4095.f;
			}

			if (p.shared.data.slider_perf_mode >= 2) {
				// Beat repeat: blue (mode 2) = 8 zones with triplets, cyan (mode 3) = 4 wide zones no triplets
				const bool is_cyan = (p.shared.data.slider_perf_mode == 3);
				const uint8_t num_zones = is_cyan ? 4 : 8;
				const uint16_t zone_width = 4096 / num_zones; // 1024 or 512
				// Small dead zone at slider minimum; ADC jitter near 0 would otherwise briefly
				// land on zone 0 and corrupt beat_repeat_pending right as shift releases.
				static constexpr uint16_t off_zone = 32;

				if (effective_slider < off_zone) {
					p.shared.beat_repeat_pending = 0xFF;
					if (!c.button.shift.is_high()) {
						p.shared.beat_repeat_committed = 0xFF;
						orbit_pickup = false;
					}
				} else {
					const uint8_t zone = std::min(
						static_cast<uint8_t>((effective_slider - 1) / zone_width),
						static_cast<uint8_t>(num_zones - 1));
					if (zone != p.shared.beat_repeat_pending) {
						p.shared.beat_repeat_pending = zone;
						p.shared.beat_repeat_pending_since = Controls::TimeNow();
					} else if (!c.button.shift.is_high()) {
						const auto debounce = beat_repeat_debounce_table[p.shared.beat_repeat_debounce_idx];
						if (Controls::TimeNow() - p.shared.beat_repeat_pending_since >= debounce) {
							p.shared.beat_repeat_committed = p.shared.beat_repeat_pending;
						}
					}
				}

				// Shift release: commit pending zone immediately.
				// On first entry from off: snap orbit_center to the current playhead step so the
				// loop starts on the step the user timed with their ear, not the slider position.
				if (c.button.shift.just_went_low()) {
					const bool was_off = (p.shared.beat_repeat_committed == 0xFF);
					p.shared.beat_repeat_committed = p.shared.beat_repeat_pending;
					if (p.shared.beat_repeat_committed == 0xFF) {
						orbit_pickup = false; // returning to off, reset for next entry
					} else if (was_off) {
						const auto len = p.slot.settings.GetLength();
						const auto global_step =
							static_cast<uint32_t>(p.GetPlayheadPage()) * Model::Sequencer::Steps::PerPage
							+ p.GetPlayheadStepOnPage();
						orbit_pickup_center = static_cast<float>(global_step)
						                    / static_cast<float>(len > 0u ? len : 1u);
						orbit_pickup_slider = slider_now;
						orbit_pickup = true;
					}
				}
			}

			// Pass phase=0; orbit overrides phase scrub entirely
			p.Update(0.f, p.shared.data.scrub_ignore_mask);
		} else if (phase_locked || picking_up) {
			p.Update(locked_phase, p.shared.data.scrub_ignore_mask);
		} else {
			const auto live_phase = (slider_now + c.ReadCv()) / 4096.f;
			float apply_phase = live_phase;
			if (p.shared.data.quantized_scrub) {
				const auto length = static_cast<float>(p.slot.settings.GetLength());
				if (length > 1.f) {
					const float step_size = 1.f / length;
					apply_phase = std::round(apply_phase / step_size) * step_size;
				}
			}
			p.Update(apply_phase, p.shared.data.scrub_ignore_mask);
		}
	}

protected:
	bool phase_locked = false;
	bool picking_up = false;
	float locked_phase = 0.f;
	uint32_t lock_toggle_time = 0;
	uint16_t locked_raw = 0;

	static constexpr uint32_t scrub_settings_hold_ticks = Clock::MsToTicks(1500);

	// Slider movement tracking for indicator
	uint16_t last_slider_raw = 0;
	uint32_t last_slider_move_time = 0;
	static constexpr uint32_t slider_active_ticks = Clock::MsToTicks(400);
	static constexpr int32_t slider_move_threshold = 8;

	static constexpr uint32_t toggle_feedback_ticks = Clock::MsToTicks(600);
	static constexpr int32_t pickup_threshold = 80;
	static constexpr uint8_t phase_lock_encoder = 7;

	// Beat repeat entry: orbit_center snapped to playhead on shift release, slider takes
	// over once it moves away from its position at the moment of entry (pickup mode).
	bool orbit_pickup = false;
	uint16_t orbit_pickup_slider = 0;
	float orbit_pickup_center = 0.f;
	// Debounce durations indexed by beat_repeat_debounce_idx (0=fastest, 7=slowest). Default = 2 (150ms).
	static constexpr std::array<uint32_t, 8> beat_repeat_debounce_table = {
		Clock::MsToTicks(50),  Clock::MsToTicks(100), Clock::MsToTicks(150), Clock::MsToTicks(250),
		Clock::MsToTicks(400), Clock::MsToTicks(600), Clock::MsToTicks(900), Clock::MsToTicks(1500),
	};

	void DoLockToggle() {
		if (!phase_locked && !picking_up) {
			phase_locked = true;
			locked_raw = c.ReadSlider();
			locked_phase = (locked_raw + c.ReadCv()) / 4096.f;
		} else {
			phase_locked = false;
			picking_up = true;
		}
		p.shared.data.phase_locked = phase_locked;
		p.shared.data.locked_raw = locked_raw;
		p.shared.do_save_shared = true;
		lock_toggle_time = Controls::TimeNow();
	}

	void PaintPhaseLockIndicator() {
		const auto t = Controls::TimeNow();
		const bool just_toggled = (t - lock_toggle_time) < toggle_feedback_ticks;
		const bool slider_moving = phase_locked &&
		                           (t - last_slider_move_time) < slider_active_ticks &&
		                           last_slider_move_time != 0;

		if (just_toggled || slider_moving || picking_up) {
			c.SetEncoderLed(phase_lock_encoder, ((t >> 7) & 1) ? Palette::red : Palette::off);
		}
	}

	void PaintStepValues(uint8_t page) {
		const auto chan = p.GetSelectedChannel();
		const auto step_offset = Catalyst2::Sequencer::SeqPageToStep(page);
		const auto is_cv = !p.slot.settings.GetChannelMode(chan).IsGate();
		const auto fine_pressed = c.button.fine.is_high() && !c.button.morph.is_high();
		const auto seqhead_col =
			p.slot.settings.GetChannelMode(chan).IsMuted() ? Palette::SeqHead::mute : Palette::SeqHead::active;

		if (is_cv) {
			const auto range = p.slot.settings.GetRange(chan);
			for (auto step_i = 0u; step_i < Model::Sequencer::Steps::PerPage; step_i++) {
				const auto step = p.GetStep(step_offset + step_i);
				const auto color = Palette::Cv::fromLevel(p.shared.data.palette[chan], step.ReadCv(), range);
				PaintStep(page, step_i, color, seqhead_col);
			}
		} else {
			if (!fine_pressed) {
				for (auto step_i = 0u; step_i < Model::Sequencer::Steps::PerPage; step_i++) {
					const auto step = p.GetStep(step_offset + step_i);
					const auto color = Palette::Gate::fromLevelSequencer(step.ReadGate());
					PaintStep(page, step_i, color, seqhead_col);
				}
			} else {
				for (auto step_i = 0u; step_i < Model::Sequencer::Steps::PerPage; step_i++) {
					const auto step = p.GetStep(step_offset + step_i);
					const auto color = Palette::Gate::fromTrigDelay(step.ReadTrigDelay());
					PaintStep(page, step_i, color, seqhead_col);
				}
			}
		}
	}
	void PaintStep(uint8_t page, uint8_t step, Color base_color, Color seq_head) {
		const auto playhead_page = p.GetPlayheadPage();
		const auto playhead_pos = p.GetPlayheadStepOnPage();
		if (page == playhead_page && step == playhead_pos)
			SetPlayheadStepLed(step, base_color, seq_head);
		else
			c.SetEncoderLed(step, base_color);
	}
	void SetPlayheadStepLed(uint8_t playhead_pos, Color base_color, Color seq_head) {
		const auto color = p.ShowPlayhead() ?
							   base_color.blend(seq_head, std::clamp(1.f - 2.f * p.seqclock.PeekPhase(), 0.f, 1.f)) :
							   base_color;
		c.SetEncoderLed(playhead_pos, color);
	}
	void BlinkSelectedPage(uint8_t page) {
		c.SetButtonLed(page, ((Controls::TimeNow() >> 8) & 1) > 0);
	}
};
} // namespace Catalyst2::Ui::Sequencer
