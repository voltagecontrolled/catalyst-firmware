# Phase Scrub Performance Page — Implementation Notes

Implemented in v1.4.6. For feature documentation see `docs/wiki/NEW_FEATURES.md`. For implementation detail see the v1.4.6 entry in `CHANGELOG.md`.

---

## Post-implementation Notes

### v1.4.6-alpha1 calibration corruption bug

**Symptom:** ~12 cents flat per octave on all CV pitch tracks after upgrading from v1.4.5. Flashing back to v1.4.5 appears to fix it.

**Root cause:** The v1.4.5→v1.4.6 migration in `saved_settings.hh` used a local `V1_4_5_SharedData` overlay struct to read flash. In alpha1, this struct was missing `alignas(4)` on its `Model::Mode saved_mode` member. Because `Mode` is `enum class : uint8_t` (1 byte), without `alignas(4)` the compiler placed `dac_calibration` at offset 6 instead of the correct offset 8 (where 3 bytes of padding exist in the actual struct). The overlay therefore read calibration from the wrong position in flash — picking up 2 bytes of padding (0x0000) as channel[0].offset, and then reading each subsequent calibration field one field too early.

The garbled values happened to pass `Calibration::Dac::Data::Validate()`, so alpha1 wrote the corrupted calibration to flash under tag+2. After that, all subsequent alpha firmware reads tag+2 directly without re-migrating, so the corruption persists across all upgrades.

The specific pitch error arises because each channel's slope was set to the value of the preceding channel's offset. `ApplySlope` multiplies the distance from 0V by `(1 + slope/max_slope * max_adjustment_volts)`. If the corrupted slope is ~10% of max_slope (e.g. an offset value of ~44 DAC counts out of 437 max), the result is ~10% of 120 cents = ~12 cents per octave of systematic pitch drift.

**Why flash-back appears to fix it:** v1.4.5-rc1 uses `CurrentSharedSettingsVersionTag = tag+1`. It does not recognize tag+2, falls through all migration paths to `return false`, and boots with default calibration (offset=0, slope=0). If the hardware tracks acceptably without correction, uncalibrated output appears correct compared to the corrupted-calibrated output.

**Fix (implemented in alpha6/v1.4.6):** `CurrentSharedSettingsVersionTag` bumped to tag+3. A new migration branch for tag+2 (`V1_4_6_Alpha_SharedSettingsVersionTag`) reads all fields except `dac_calibration`, which is left at defaults. Users affected by the alpha1/2 bug must recalibrate from the DAC calibration menu. Users who only ran alpha3+ and had correct calibration in tag+2 also need to recalibrate (there is no way to distinguish correct from corrupted tag+2 data).

**Lesson for future migrations:** When writing a local overlay struct for flash migration, always verify that `alignas` specifiers on members match those in the original struct. Pay particular attention to `enum class : uint8_t` fields declared `alignas(4)` — the underlying type is 1 byte, so without explicit alignment the compiler packs the next member tighter than the original. Always add a `static_assert(sizeof(LocalOverlay) == sizeof(OriginalStruct))` (or a size check against the expected value) before merging any migration that uses a locally-defined overlay type.
