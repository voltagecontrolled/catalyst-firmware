#pragma once
#include "conf/flash_layout.hh"
#include "conf/model.hh"
#include "drivers/flash_block.hh"
#include "drivers/flash_sectors.hh"
#include "legacy/v1_0/legacy_shared_data.hh"
#include "legacy/v1_1-v1_2/shared.hh"
#include "util/wear_level.hh"

namespace Catalyst2
{

template<typename SeqModeData, typename MacroModeData, typename SharedData>
class SavedSettings {

	WearLevel<mdrivlib::FlashBlock<SeqModeData, SeqSettingsFlashAddr, SeqSettingsSectorSize>> seq_settings_flash;
	WearLevel<mdrivlib::FlashBlock<MacroModeData, MacroSettingsFlashAddr, MacroSettingsSectorSize>>
		macro_settings_flash;
	WearLevel<mdrivlib::FlashBlock<SharedData, SharedSettingsFlashAddr, SharedSettingsSectorSize>>
		shared_settings_flash;

	static constexpr uint32_t CurrentSharedSettingsVersionTag = Legacy::V1_1__V1_2::Shared::tag + 3;
	// v1.4.5 tag (tag+1): SharedData without phase lock persistence or scrub settings fields
	static constexpr uint32_t V1_4_5_SharedSettingsVersionTag = Legacy::V1_1__V1_2::Shared::tag + 1;
	// v1.4.6-alpha tag (tag+2): SharedData with scrub settings fields; dac_calibration may be
	// corrupted by the alpha1 migration bug (missing alignas caused a 2-byte layout shift in the
	// V1_4_5_SharedData overlay, corrupting calibration on any device that ran alpha1 or alpha2).
	// Cannot distinguish good from bad tag+2 data, so calibration is always reset to defaults.
	static constexpr uint32_t V1_4_6_Alpha_SharedSettingsVersionTag = Legacy::V1_1__V1_2::Shared::tag + 2;

public:
	bool read(SeqModeData &data) {
		return seq_settings_flash.read(data);
	}

	bool write(SeqModeData &data) {
		data.SettingsVersionTag = SeqModeData::current_tag;
		return seq_settings_flash.write(data);
	}

	bool read(MacroModeData &data) {
		return macro_settings_flash.read(data);
	}

	bool write(MacroModeData const &data) {
		return macro_settings_flash.write(data);
	}

	bool read(SharedData &data) {
		// we know the incoming struct has been constructed using the current version's default values
		const auto magic_number = *reinterpret_cast<uint32_t *>(SharedSettingsFlashAddr);
		if (magic_number == CurrentSharedSettingsVersionTag) {
			return shared_settings_flash.read(data);
		} else if (magic_number == V1_4_5_SharedSettingsVersionTag) {
			// v1.4.5 → v1.4.6: added phase lock persistence and scrub settings fields.
			// Old struct is identical except it lacks the 8 bytes at the end.
			// Read only the known-good fields by overlaying the old struct size.
			struct V1_4_5_SharedData {
				uint32_t SettingsVersionTag alignas(4);
				Model::Mode saved_mode alignas(4);
				Calibration::Dac::Data dac_calibration alignas(4);
				Quantizer::CustomScales custom_scale;
				std::array<uint8_t, Model::NumChans> palette;
				bool validate() const { return SettingsVersionTag == V1_4_5_SharedSettingsVersionTag; }
			};
			WearLevel<mdrivlib::FlashBlock<V1_4_5_SharedData, SharedSettingsFlashAddr, SharedSettingsSectorSize>>
				v1_4_5_flash;
			V1_4_5_SharedData old{};
			if (v1_4_5_flash.read(old)) {
				data.saved_mode = old.saved_mode;
				data.dac_calibration = old.dac_calibration;
				data.custom_scale = old.custom_scale;
				data.palette = old.palette;
				// new fields retain their zero-initialized defaults (unlocked, no quantization, all tracks active)
				shared_settings_flash.erase();
				data.SettingsVersionTag = CurrentSharedSettingsVersionTag;
				shared_settings_flash.write(data);
				return shared_settings_flash.read(data);
			}
		} else if (magic_number == V1_4_6_Alpha_SharedSettingsVersionTag) {
			// v1.4.6-alpha1/2 may have corrupted dac_calibration via a struct overlay bug.
			// Preserve everything else; reset calibration to defaults (recalibration required).
			struct V1_4_6_Alpha_SharedData {
				uint32_t SettingsVersionTag alignas(4);
				Model::Mode saved_mode alignas(4);
				Calibration::Dac::Data dac_calibration alignas(4);
				Quantizer::CustomScales custom_scale;
				std::array<uint8_t, Model::NumChans> palette;
				uint8_t phase_locked;
				uint8_t quantized_scrub;
				uint8_t scrub_ignore_mask;
				uint8_t slider_perf_mode;
				uint16_t locked_raw;
				uint8_t orbit_width;
				uint8_t orbit_direction;
				bool validate() const { return SettingsVersionTag == V1_4_6_Alpha_SharedSettingsVersionTag; }
			};
			WearLevel<mdrivlib::FlashBlock<V1_4_6_Alpha_SharedData, SharedSettingsFlashAddr, SharedSettingsSectorSize>>
				alpha_flash;
			V1_4_6_Alpha_SharedData old{};
			if (alpha_flash.read(old)) {
				data.saved_mode = old.saved_mode;
				// dac_calibration intentionally NOT copied -- may be corrupted by alpha1/2 bug
				data.custom_scale = old.custom_scale;
				data.palette = old.palette;
				data.phase_locked = old.phase_locked;
				data.quantized_scrub = old.quantized_scrub;
				data.scrub_ignore_mask = old.scrub_ignore_mask;
				data.slider_perf_mode = old.slider_perf_mode;
				data.locked_raw = old.locked_raw;
				data.orbit_width = old.orbit_width;
				data.orbit_direction = old.orbit_direction;
				shared_settings_flash.erase();
				data.SettingsVersionTag = CurrentSharedSettingsVersionTag;
				shared_settings_flash.write(data);
				return shared_settings_flash.read(data);
			}
		} else if (magic_number == Legacy::V1_1__V1_2::Shared::tag) {
			WearLevel<mdrivlib::FlashBlock<Legacy::V1_1__V1_2::Shared::Data,
										   SharedSettingsFlashAddr,
										   SharedSettingsSectorSize>>
				v1_2Data;
			if (v1_2Data.read(*reinterpret_cast<Legacy::V1_1__V1_2::Shared::Data *>(&data))) {
				// we have the old data in the new struct.
				shared_settings_flash.erase();
				data.SettingsVersionTag = CurrentSharedSettingsVersionTag;
				shared_settings_flash.write(data);
				return shared_settings_flash.read(data);
			}
		} else {
			auto legacy_settings_slot0 = *reinterpret_cast<Legacy::V1_0::Shared::Data *>(Legacy::V1_0::FirstSlot);
			auto legacy_settings_slot1 = *reinterpret_cast<Legacy::V1_0::Shared::Data *>(Legacy::V1_0::SecondSlot);
			const Legacy::V1_0::Shared::Data *valid_slot = nullptr;

			if (legacy_settings_slot1.Validate()) {
				valid_slot = &legacy_settings_slot1;
			} else if (legacy_settings_slot0.Validate()) {
				valid_slot = &legacy_settings_slot0;
			}

			if (valid_slot) {
				if (valid_slot->saved_mode == Legacy::V1_0::Shared::Data::Mode::Macro)
					data.saved_mode = Catalyst2::Model::Mode::Macro;
				else
					data.saved_mode = Catalyst2::Model::Mode::Sequencer;

				for (auto [oldcal, newcal] : zip(valid_slot->dac_calibration.channel, data.dac_calibration.channel)) {
					newcal.offset = oldcal.offset;
					newcal.slope = oldcal.slope;
				}

				// wipe legacy sectors, which is v1.1 Settings and Macro sectors
				Legacy::V1_0::eraseFlashSectors();

				// Try to write the extracted data, regardless if the above erasing failed
				// Reset the settings WearLeveling sector since we just wiped it
				auto reset_settings = decltype(shared_settings_flash){};
				data.SettingsVersionTag = CurrentSharedSettingsVersionTag;
				reset_settings.write(data);

				return true;
			}
		}

		return false;
	}

	bool write(SharedData &data) {
		data.SettingsVersionTag = CurrentSharedSettingsVersionTag;
		return shared_settings_flash.write(data);
	}
};

} // namespace Catalyst2
