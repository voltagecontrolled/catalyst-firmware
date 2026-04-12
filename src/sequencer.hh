#pragma once

#include "channel.hh"
#include "clock.hh"
#include "conf/model.hh"
#include "random.hh"
#include "random_shuffle.hh"
#include "range.hh"
#include "sequence_phaser.hh"
#include "sequencer_player.hh"
#include "sequencer_settings.hh"
#include "sequencer_step.hh"
#include "shared.hh"
#include "song_mode.hh"
#include "util/countzip.hh"
#include "util/fixed_vector.hh"
#include <algorithm>
#include <array>
#include <vector>

namespace Catalyst2::Sequencer
{

struct ChannelData : public std::array<Step, Model::Sequencer::Steps::Max> {
	bool Validate() const {
		auto ret = true;
		for (auto &s : *this) {
			ret &= s.Validate();
		}
		return ret;
	}
};

struct Slot {
	std::array<Sequencer::ChannelData, Model::NumChans> channel;
	Sequencer::Settings::Data settings;
	Player::Data player;
	SongMode::Data songmode;
	Clock::Divider::type clockdiv{};
	Clock::Sync clock_sync_mode;
	Clock::Bpm::Data bpm{};

	void PreSave() {
		bpm.PreSave();
	}
	void PostLoad() {
		bpm.PostLoad();
	}

	bool validate() const {
		auto ret = true;
		for (auto &c : channel) {
			ret &= c.Validate();
		}
		ret &= clockdiv.Validate();
		ret &= settings.Validate();
		ret &= player.Validate();
		ret &= songmode.Validate();
		ret &= bpm.Validate();
		ret &= clock_sync_mode.Validate();
		return ret;
	}
};

struct Data {
	// Increment current_tag whenever sizeof(Step) or any other persistent field layout changes.
	// validate() checks this tag so WearLevel rejects incompatible flash data gracefully.
	static constexpr uint32_t current_tag = 6u;
	uint32_t SettingsVersionTag = 0;

	std::array<Slot, Model::Sequencer::NumSlots> slot;
	uint8_t startup_slot = 0;

	void PreSave() {
		for (auto &s : slot) {
			s.PreSave();
		}
	}
	void PostLoad() {
		for (auto &s : slot) {
			s.PostLoad();
		}
	}

	bool validate() const {
		if (SettingsVersionTag != current_tag)
			return false;
		auto ret = true;
		for (auto &s : slot) {
			ret &= s.validate();
		}
		ret &= startup_slot < slot.size();
		return ret;
	}
};

inline uint8_t SeqPageToStep(uint8_t page) {
	return page * Model::Sequencer::Steps::PerPage;
}

class Interface {
	Data &data;
	uint8_t cur_channel = 0;
	uint8_t prev_channel = 0;
	uint8_t cur_page = Model::Sequencer::NumPages;
	struct Clipboard {
		ChannelData cd;
		Settings::Channel cs;
		std::array<Step, Model::Sequencer::Steps::PerPage> page;
	} clipboard;
	uint32_t time_last_reset;
	uint8_t playhead_pos;
	uint8_t playhead_page;
	uint8_t last_playhead_pos = Model::Sequencer::NumPages;
	bool show_playhead = true;
	bool gates_blocked = true;
	std::array<uint8_t, Model::NumChans> repeat_ticks_remaining{};
	std::array<bool, Model::NumChans> gate_repeat_fired{};    // set after repeat tick fires; consumed next 3kHz tick
	std::array<uint8_t, Model::NumChans> gate_substep_idx{}; // last seen ratchet sub-step index per gate channel
	std::array<bool, Model::NumChans> step_fired{};           // true on the tick a channel's step advances

	// Orbit / beat repeat state
	bool orbit_active = false;
	uint8_t orbit_pos = 0;
	bool orbit_ping_dir = true;
	std::array<uint8_t, Model::NumChans> orbit_step{};
	std::array<uint8_t, Model::NumChans> orbit_step_prev{};
	uint32_t beat_repeat_countdown = 0;
	float beat_repeat_phase = 0.f;

public:
	Slot slot;
	Clock::Bpm::Interface seqclock{slot.bpm};
	Shared::Interface &shared;
	Player::Interface player{slot.player, slot.settings, slot.songmode};

	Interface(Data &data, Shared::Interface &shared)
		: data{data}
		, shared{shared} {
	}

	Quantizer::Scale toScale() const {
		const auto chan = cur_channel == Model::NumChans ? prev_channel : cur_channel;
		FixedVector<Channel::Cv::type, Quantizer::Scale::MaxScaleNotes> notes;
		const auto seq_length = slot.settings.GetLengthOrGlobal(chan);
		const unsigned size =
			seq_length > Quantizer::Scale::MaxScaleNotes ? Quantizer::Scale::MaxScaleNotes : seq_length;
		const auto &cur_scale = GetScale(chan);
		for (auto i = 0u; i < size; i++) {
			notes.push_back(Quantizer::Process(cur_scale, slot.channel[chan][i].ReadCv()));
		}
		return Quantizer::Scale{notes};
	}

	void Load() {
		Load(data.startup_slot);
	}
	void Load(uint8_t slot) {
		this->slot = data.slot[slot];
	}
	void Save(uint8_t slot) {
		data.startup_slot = slot;
		data.slot[slot] = this->slot;
	}
	uint8_t GetStartupSlot() {
		return data.startup_slot;
	}
	bool OrbitActive() const { return orbit_active; }
	bool OrbitActiveForChannel(uint8_t chan) const {
		return orbit_active && ((shared.data.scrub_ignore_mask >> chan) & 1u);
	}
	uint8_t GetOrbitStep(uint8_t chan) const { return orbit_step[chan]; }
	uint8_t GetOrbitStepPrev(uint8_t chan) const { return orbit_step_prev[chan]; }
	float GetEffectiveStepPhase(uint8_t chan) const {
		if (OrbitActiveForChannel(chan) && shared.data.slider_perf_mode >= 2) {
			return beat_repeat_phase;
		}
		return player.GetStepPhase(chan);
	}
	float GetEffectiveMovementDir(uint8_t chan, int8_t relative) const {
		if (OrbitActiveForChannel(chan)) {
			const auto dir = shared.data.orbit_direction;
			if (dir == 1) return -1.f;
			if (dir == 2) return orbit_ping_dir ? 1.f : -1.f;
			return 1.f;
		}
		return player.RelativeStepMovementDir(chan, relative);
	}
	std::array<Step, 3> GetOrbitStepCluster(uint8_t chan) {
		std::array<Step, 3> out;
		out[0] = slot.channel[chan][orbit_step_prev[chan]];
		out[1] = slot.channel[chan][orbit_step[chan]];
		out[2] = slot.channel[chan][orbit_step[chan]]; // next = current (window loops)
		return out;
	}

	void Update(float phase, uint8_t scrub_ignore_mask = 0xFFu) {
		const auto clock_ticked = seqclock.Update();

		std::array<uint8_t, Model::NumChans> per_chan_step{};
		for (auto chan = 0u; chan < Model::NumChans; chan++) {
			if (!clock_ticked) {
				per_chan_step[chan] = 0;
			} else if (repeat_ticks_remaining[chan] > 0) {
				repeat_ticks_remaining[chan]--;
				per_chan_step[chan] = 0;
			} else if (!slot.settings.GetChannelMode(chan).IsGate() &&
			           slot.settings.GetClockSource(chan) > 0) {
				// CV track with gate clock source: master clock does not advance it
				per_chan_step[chan] = 0;
			} else {
				per_chan_step[chan] = 1;
			}
		}

		// Gate Track Follow: advance linked CV tracks.
		// Runs every 3kHz iteration (not just on clock ticks) to catch ratchet sub-step crossings.
		// Each gate channel produces three separate advance signals; each CV channel selects
		// which to consume based on its clock_follow_mode (0=ratchets, 1=repeats, 2=both).
		{
			// Phase 1: compute per-gate-channel advance signals for this iteration
			std::array<uint8_t, Model::NumChans> gate_step_adv{};    // step boundary: always
			std::array<uint8_t, Model::NumChans> gate_ratchet_adv{}; // ratchet sub-step crossings
			std::array<uint8_t, Model::NumChans> gate_repeat_adv{};  // repeat tick fires

			for (auto gate_chan = 0u; gate_chan < Model::NumChans; gate_chan++) {
				const auto cm = slot.settings.GetChannelMode(gate_chan);
				if (!cm.IsGate() || cm.IsMuted()) {
					gate_substep_idx[gate_chan] = 0;
					continue;
				}
				if (per_chan_step[gate_chan] > 0) {
					// Gate step just advanced (first tick of any step, including repeat and ratchet)
					gate_step_adv[gate_chan] = 1;
					gate_substep_idx[gate_chan] = 0;
				} else {
					if (gate_repeat_fired[gate_chan]) {
						gate_repeat_adv[gate_chan] = 1;
						gate_repeat_fired[gate_chan] = false;
					}
					// Ratchet sub-step boundary detection
					const auto cur_step = player.GetRelativeStep(gate_chan, 0);
					const auto &gs = slot.channel[gate_chan][cur_step];
					if (!gs.IsRepeat()) {
						const auto ratchet_count = gs.ReadRatchetRepeatCount() + 1u;
						if (ratchet_count > 1) {
							const auto step_phase = player.GetStepPhase(gate_chan);
							const auto cur_substep = static_cast<uint8_t>(ratchet_count * step_phase);
							if (cur_substep > gate_substep_idx[gate_chan]) {
								gate_ratchet_adv[gate_chan] = cur_substep - gate_substep_idx[gate_chan];
								gate_substep_idx[gate_chan] = cur_substep;
							}
						}
					}
				}
			}

			// Phase 2: apply selected advances to each linked CV channel
			for (auto cv_chan = 0u; cv_chan < Model::NumChans; cv_chan++) {
				if (slot.settings.GetChannelMode(cv_chan).IsGate()) continue;
				const auto clock_src = slot.settings.GetClockSource(cv_chan);
				if (clock_src == 0 || clock_src > Model::NumChans) continue;
				const auto gate_chan = static_cast<uint8_t>(clock_src - 1u);
				if (!slot.settings.GetChannelMode(gate_chan).IsGate()) continue;

				const auto follow_mode = slot.settings.GetClockFollowMode(cv_chan);
				const bool use_ratchets = (follow_mode == 0 || follow_mode == 2); // ratchets or both
				const bool use_repeats  = (follow_mode == 1 || follow_mode == 2); // repeats or both
				// follow_mode 3 = step only: neither ratchets nor repeats (gate_step_adv always applied)

				per_chan_step[cv_chan] += gate_step_adv[gate_chan]; // always included
				if (use_ratchets) per_chan_step[cv_chan] += gate_ratchet_adv[gate_chan];
				if (use_repeats)  per_chan_step[cv_chan] += gate_repeat_adv[gate_chan];
			}
		}

		player.Update(phase, seqclock.GetPhase(), per_chan_step, scrub_ignore_mask);

		// Record which channels advanced a step this tick (used by lavender CV replace intersection logic)
		for (auto chan = 0u; chan < Model::NumChans; chan++) {
			step_fired[chan] = per_chan_step[chan] > 0;
		}

		// Orbit / Beat Repeat advance
		{
			const auto perf_mode = shared.data.slider_perf_mode;
			if (perf_mode == 0) {
				orbit_active = false;
				beat_repeat_countdown = 0;
				beat_repeat_phase = 0.f;
			} else {
				const auto width_pct = shared.data.orbit_width;
				const auto dir = shared.data.orbit_direction;
				const auto bpm = GetGlobalDividedBpm();
				const uint32_t beat_period = (bpm > 0.f) ? static_cast<uint32_t>(3000.f * 60.f / bpm) : 3000u;
				bool orbit_should_advance = false;

				if (perf_mode == 1) {
					// Granular: active only when width > 0 and slider is off zero (zero = bypass)
					orbit_active = (width_pct > 0) && (shared.orbit_center > 0.f);
					orbit_should_advance = clock_ticked && orbit_active;
					beat_repeat_phase = 0.f;
				} else {
					// Beat repeat: mode 2 = 8 zones with triplets, mode 3 = 4 wide zones no triplets
					orbit_active = (shared.beat_repeat_committed != 0xFF);
					if (orbit_active) {
						static constexpr std::array<uint32_t, 8> div_num_8   = {2, 1, 2, 1, 1, 1, 1, 1};
						static constexpr std::array<uint32_t, 8> div_denom_8 = {1, 1, 3, 2, 3, 4, 6, 8};
						static constexpr std::array<uint32_t, 4> div_num_4   = {2, 1, 1, 1};
						static constexpr std::array<uint32_t, 4> div_denom_4 = {1, 1, 2, 4};
						const bool is_cyan = (perf_mode == 3);
						const auto zone = std::min(shared.beat_repeat_committed,
						                           static_cast<uint8_t>(is_cyan ? 3 : 7));
						const uint32_t safe_period = is_cyan
							? std::max<uint32_t>(1u, beat_period * div_num_4[zone] / div_denom_4[zone])
							: std::max<uint32_t>(1u, beat_period * div_num_8[zone] / div_denom_8[zone]);
						if (beat_repeat_countdown == 0) {
							// On first entry (snap_pending), quantize mode snaps to the nearest step
							// boundary: fire immediately if in the first half of the step (current
							// boundary was closer), or wait for the next clock tick (next boundary is
							// closer). Non-quantized and normal cycle completions always fire immediately.
							const bool first_entry = shared.beat_repeat_snap_pending;
							const bool should_fire = !first_entry
							                      || !shared.data.quantized_scrub
							                      || clock_ticked
							                      || seqclock.PeekPhase() < 0.5f;
							if (should_fire) {
								beat_repeat_phase = 0.f;
								if (first_entry && cur_channel < Model::NumChans) {
									const auto len = slot.settings.GetLengthOrGlobal(cur_channel);
									if (len > 0) {
										shared.orbit_center =
											static_cast<float>(player.GetRelativeStep(cur_channel, 0))
											/ static_cast<float>(len);
									}
									shared.beat_repeat_snap_pending = false;
								}
								orbit_should_advance = true;
								beat_repeat_countdown = safe_period;
							}
						} else {
							beat_repeat_phase =
								1.f - static_cast<float>(beat_repeat_countdown) / static_cast<float>(safe_period);
							beat_repeat_countdown--;
						}
					} else {
						beat_repeat_countdown = 0;
						beat_repeat_phase = 0.f;
						shared.beat_repeat_snap_pending = false;
					}
				}

				// Advance orbit_pos based on direction
				if (orbit_should_advance) {
					const auto ref_len = slot.settings.GetLength();
					const uint32_t window_ref =
						(perf_mode == 1)
							? std::max<uint32_t>(1u, static_cast<uint32_t>(width_pct / 100.f * ref_len + 0.5f))
							: 1u;
					if (dir == 0) {
						orbit_pos = static_cast<uint8_t>((orbit_pos + 1) % window_ref);
					} else if (dir == 1) {
						orbit_pos = static_cast<uint8_t>((orbit_pos + window_ref - 1) % window_ref);
					} else if (dir == 2) {
						if (window_ref <= 1) {
							orbit_pos = 0;
						} else if (orbit_ping_dir) {
							if (orbit_pos + 1u >= window_ref) {
								orbit_ping_dir = false;
								orbit_pos = static_cast<uint8_t>(window_ref - 2);
							} else {
								orbit_pos++;
							}
						} else {
							if (orbit_pos == 0) {
								orbit_ping_dir = true;
								orbit_pos = 1;
							} else {
								orbit_pos--;
							}
						}
					} else {
						orbit_pos = static_cast<uint8_t>(static_cast<uint32_t>(std::rand()) % window_ref);
					}
				}

				// Compute per-channel orbit_step
				if (orbit_active) {
					for (auto chan = 0u; chan < Model::NumChans; chan++) {
						orbit_step_prev[chan] = orbit_step[chan];
						const auto len = slot.settings.GetLengthOrGlobal(chan);
						const uint32_t window_chan =
							(perf_mode == 1)
								? std::max<uint32_t>(1u, static_cast<uint32_t>(width_pct / 100.f * len + 0.5f))
								: 1u;
						float center_f = shared.orbit_center * static_cast<float>(len);
						if (shared.data.quantized_scrub && len > 1) {
							center_f = std::round(center_f);
						}
						const auto center_step = static_cast<uint8_t>(
							std::clamp<uint32_t>(static_cast<uint32_t>(center_f), 0u, static_cast<uint32_t>(len - 1)));
						const auto pos_in_window = orbit_pos % window_chan;
						orbit_step[chan] = static_cast<uint8_t>((center_step + pos_in_window) % len);
					}
				}
			}
		}

		if (clock_ticked) {
			for (auto chan = 0u; chan < Model::NumChans; chan++) {
				if (per_chan_step[chan] && slot.settings.GetChannelMode(chan).IsGate()) {
					const auto cur_step = player.GetRelativeStep(chan, 0);
					const auto &s = slot.channel[chan][cur_step];
					repeat_ticks_remaining[chan] = s.IsRepeat() ? s.ReadRatchetRepeatCount() : 0u;
				}
			}
		}

		// Record repeat tick signals for Gate Track Follow.
		// Only repeat steps need this — step boundary and ratchet sub-steps are detected directly.
		// gate_repeat_fired persists until consumed by the Gate Track Follow block.
		if (clock_ticked) {
			for (auto chan = 0u; chan < Model::NumChans; chan++) {
				const auto cm = slot.settings.GetChannelMode(chan);
				if (!cm.IsGate() || cm.IsMuted() || per_chan_step[chan] > 0) continue;
				const auto cur_step = player.GetRelativeStep(chan, 0);
				const auto &s = slot.channel[chan][cur_step];
				if (!s.IsRepeat()) continue;
				const uint32_t sub_step_idx = s.ReadRatchetRepeatCount() - repeat_ticks_remaining[chan];
				const uint32_t count = s.ReadRatchetRepeatCount() + 1u;
				gate_repeat_fired[chan] = !(count > 1 && !(s.ReadSubStepMask() & (1u << sub_step_idx)));
			}
		}
		playhead_page = player.GetPlayheadPage(cur_channel);
		playhead_pos = player.GetPlayheadStepOnPage(cur_channel);

		if (last_playhead_pos != playhead_pos) {
			last_playhead_pos = playhead_pos;
			seqclock.ResetPeek();
			show_playhead = true;
			if (gates_blocked && (Controls::TimeNow() - time_last_reset >= Clock::BpmToTicks(Clock::Bpm::max_bpm))) {
				gates_blocked = false;
			}
		}
	}
	void IncChannelMode(uint8_t chan, int32_t inc) {
		do {
			slot.settings.IncChannelMode(chan, inc);
		} while (!slot.settings.GetChannelMode(chan).IsGate() && GetScale(chan).size() == 0 &&
				 slot.settings.GetChannelMode(chan).GetScaleIdx() != 0);
	}
	void UpdateChannelMode() {
		for (auto i = 0u; i < Model::NumChans; i++) {
			const auto mode = slot.settings.GetChannelMode(i);
			if (mode.IsGate()) {
				continue;
			}
			if (GetScale(i).size() == 0) {
				IncChannelMode(i, -1);
			}
		}
	}
	const Quantizer::Scale &GetScale(uint8_t chan) const {
		const auto mode = slot.settings.GetChannelMode(chan);
		if (mode.IsGate()) {
			return Quantizer::scale[0];
		}
		if (mode.IsCustomScale()) {
			return shared.data.custom_scale[mode.GetScaleIdx() - Quantizer::scale.size()];
		}
		return Quantizer::scale[mode.GetScaleIdx()];
	}

	bool ShowPlayhead() const {
		return show_playhead;
	}
	uint8_t GetPlayheadStepOnPage() const {
		return playhead_pos;
	}
	uint8_t GetPlayheadPage() const {
		return playhead_page;
	}
	bool IsPaused() const {
		return seqclock.pause;
	}
	void Play() {
		seqclock.pause = false;
		gates_blocked = false;
	}
	void Pause() {
		seqclock.pause = true;
		gates_blocked = false;
	}
	void TogglePause() {
		seqclock.pause ^= 1;
		gates_blocked = false;
	}
	void Reset() {
		shared.clockdivider.Reset();
		player.Reset();
		seqclock.Reset();
		gates_blocked = false;
		repeat_ticks_remaining.fill(0);
		gate_repeat_fired.fill(false);
		gate_substep_idx.fill(0);
		step_fired.fill(false);
		orbit_active = false;
		orbit_pos = 0;
		orbit_ping_dir = true;
		orbit_step.fill(0);
		orbit_step_prev.fill(0);
		beat_repeat_countdown = 0;
		beat_repeat_phase = 0.f;

		// blocks next trig for a short period of time after reset
		time_last_reset = Controls::TimeNow();
	}
	void Stop(bool kill_gates = false) {
		seqclock.pause = true;
		Reset();
		gates_blocked = kill_gates;
	}
	bool IsGatesBlocked() const {
		return gates_blocked;
	}
	float GetGlobalDividedBpm() {
		return seqclock.GetBpm() / slot.clockdiv.Read();
	}
	float GetChannelDividedBpm(uint32_t chan) {
		return GetGlobalDividedBpm() / slot.settings.GetClockDiv(chan).Read();
	}
	void Trig() {
		if (Controls::TimeNow() - time_last_reset >= Clock::BpmToTicks(Clock::Bpm::max_bpm)) {
			if (shared.clockdivider.Update(slot.clockdiv)) {
				seqclock.Trig();
			}
		}
	}

	void SelectChannel() {
		cur_channel = prev_channel;
	}
	void SelectChannel(uint8_t chan) {
		if (chan == cur_channel) {
			prev_channel = chan;
			cur_channel = Model::NumChans;
		} else {
			cur_channel = chan;
		}
	}
	uint8_t GetSelectedChannel() {
		return cur_channel;
	}
	uint8_t GetPrevSelectedChannel() {
		return prev_channel;
	}
	bool IsChannelSelected() {
		return cur_channel < Model::NumChans;
	}
	void SelectPage(uint8_t page) {
		if (page == cur_page) {
			cur_page = Model::Sequencer::NumPages;
		} else {
			cur_page = page;
		}
	}
	uint8_t GetSelectedPage() {
		return cur_page;
	}
	bool IsPageSelected() {
		return cur_page < Model::Sequencer::NumPages;
	}
	void IncStep(uint8_t step, int32_t inc, bool fine) {
		show_playhead = false;
		IncStepInSequence(StepOnPageToStep(step), inc, fine);
	}
	void IncStepInSequence(uint8_t step, int32_t inc, bool fine) {
		if (step >= Model::Sequencer::Steps::Max)
			return;
		auto &c = slot.channel[cur_channel][step];
		if (slot.settings.GetChannelMode(cur_channel).IsGate()) {
			if (fine) {
				c.IncTrigDelay(inc);
			} else {
				c.IncGate(inc);
			}
		} else {
			c.IncCv(inc, fine, slot.settings.GetRange(cur_channel));
		}
	}
	void RotateStepsLeft(uint8_t first_step, uint8_t last_step) {
		auto &all_steps = slot.channel[cur_channel];
		auto steps = std::span<Step>{&all_steps[first_step], &all_steps[last_step + 1]};
		std::rotate(steps.begin(), steps.begin() + 1, steps.end());
	}
	void RotateStepsRight(uint8_t first_step, uint8_t last_step) {
		auto &all_steps = slot.channel[cur_channel];
		auto steps = std::span<Step>{&all_steps[first_step], &all_steps[last_step + 1]};
		std::rotate(steps.rbegin(), steps.rbegin() + 1, steps.rend());
	}
	void RandomShuffleStepOrder(uint8_t first_step, uint8_t last_step) {
		auto &all_steps = slot.channel[cur_channel];
		auto steps = std::span<Step>{&all_steps[first_step], &all_steps[last_step + 1]};
		Catalyst2::random_shuffle(steps.begin(), steps.end());
	}
	void ReverseStepOrder(uint8_t first_step, uint8_t last_step) {
		auto &all_steps = slot.channel[cur_channel];
		auto steps = std::span<Step>{&all_steps[first_step], &all_steps[last_step + 1]};
		std::reverse(steps.begin(), steps.end());
	}
	void IncStepModifier(uint8_t step, int32_t inc) {
		show_playhead = false;
		step = StepOnPageToStep(step);
		if (slot.settings.GetChannelMode(cur_channel).IsGate()) {
			slot.channel[cur_channel][step].IncRatchetRepeat(inc);
		} else {
			slot.channel[cur_channel][step].IncMorph(inc);
		}
	}
	void IncStepProbability(uint8_t step, int32_t inc) {
		show_playhead = false;
		step = StepOnPageToStep(step);
		slot.channel[cur_channel][step].IncProbability(inc);
	}
	uint8_t GetRepeatTicksRemaining(uint8_t chan) const {
		return repeat_ticks_remaining[chan];
	}
	uint8_t GetTransposeSource(uint8_t chan) const {
		return slot.settings.GetTransposeSource(chan);
	}
	void SetTransposeSource(uint8_t chan, uint8_t source) {
		slot.settings.SetTransposeSource(chan, source);
	}
	bool IsTransposeReplace(uint8_t chan) const {
		return slot.settings.IsTransposeReplace(chan);
	}
	bool StepFired(uint8_t chan) const {
		return step_fired[chan];
	}
	void SetTransposeSourceReplace(uint8_t chan, uint8_t source) {
		slot.settings.SetTransposeSourceReplace(chan, source);
	}
	uint8_t GetClockSource(uint8_t chan) const {
		return slot.settings.GetClockSource(chan);
	}
	void SetClockSource(uint8_t chan, uint8_t source) {
		slot.settings.SetClockSource(chan, source);
	}
	uint8_t GetClockFollowMode(uint8_t chan) const {
		return slot.settings.GetClockFollowMode(chan);
	}
	void SetClockFollowMode(uint8_t chan, uint8_t mode) {
		slot.settings.SetClockFollowMode(chan, mode);
	}
	void ToggleSubStepMask(uint8_t step_on_page, uint8_t sub_step) {
		show_playhead = false;
		slot.channel[cur_channel][StepOnPageToStep(step_on_page)].ToggleSubStepMask(sub_step);
	}

	Step GetRelativeStep(uint8_t chan, int8_t relative_pos) {
		const auto step = player.GetRelativeStep(chan, relative_pos);
		return slot.channel[chan][step];
	}
	Step GetStep(uint8_t step) {
		return slot.channel[cur_channel][step];
	}
	void CopySequence() {
		clipboard.cd = slot.channel[cur_channel];
		clipboard.cs = slot.settings.Copy(cur_channel);
	}
	void PasteSequence() {
		slot.channel[cur_channel] = clipboard.cd;
		slot.settings.Paste(cur_channel, clipboard.cs);
	}
	void CopyPage(uint8_t page) {
		for (auto i = 0u; i < Model::Sequencer::Steps::PerPage; i++) {
			clipboard.page[i] = slot.channel[cur_channel][SeqPageToStep(page) + i];
		}
	}
	void PastePage(uint8_t page) {
		for (auto i = 0u; i < Model::Sequencer::Steps::PerPage; i++) {
			slot.channel[cur_channel][SeqPageToStep(page) + i] = clipboard.page[i];
		}
	}

	std::array<Step, 3> GetStepCluster_TEST(uint8_t chan) {
		std::array<Step, 3> out;
		out[0] = GetRelativeStep(chan, -1);
		out[1] = GetRelativeStep(chan, 0);
		out[2] = GetRelativeStep(chan, 1);
		return out;
	}

private:
	uint8_t StepOnPageToStep(uint8_t step_on_page) {
		const auto page = IsPageSelected() ? GetSelectedPage() : GetPlayheadPage();
		return step_on_page + (page * Model::Sequencer::Steps::PerPage);
	}
};

} // namespace Catalyst2::Sequencer
