**Note: this is a pre-release firmware, use at your own risk. This is not supported or endorsed by 4ms Company and flashing third party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

## Upgrading from v1.4.5 — recalibration may be required

If you ran any v1.4.6 alpha release (alpha1–alpha5) before installing this release, your DAC calibration data was corrupted by a bug in the alpha1 migration. You may notice pitch tracks running approximately 12 cents flat per octave. If so, recalibrate: hold **Shift + Glide** while powering the module to enter the calibration screen. See `docs/CALIBRATION.md` for the full procedure.

If you are upgrading directly from v1.4.5, your calibration is migrated correctly and recalibration should not be needed.

---

## Phase Scrub Performance Page (v1.4.6)

**Entry:** hold Copy + Glide for 3 seconds. **Exit:** Copy + Glide (any duration) or Play/Reset.

A dedicated performance page replacing the basic lock toggle combo. All settings persist across reboots.

| Encoder | Function | Colors |
|---------|----------|--------|
| 1 | Quantize | orange = on, off = off |
| 2 | Slider Performance Mode | off / green / blue |
| 3 | Granular Width | off at 0%, dim→bright orange |
| 4 | Orbit Direction | green / blue / orange / lavender |
| 8 | Phase Scrub Lock | red = locked, off = unlocked |

**Page buttons:** toggle per-track scrub participation. Lit = track follows scrub, unlit = track ignores scrub and plays normally.

**Quantize (Enc 1):** snaps the scrub phase to the nearest step boundary.

**Slider Performance Mode (Enc 2):**
- **Off:** standard phase scrub
- **Green — Granular:** slider positions an orbit window within the sequence. The sequencer loops steps within that window at the normal clock rate. Enc 3 sets window width (0–100% of pattern length); Enc 4 sets playback direction within the window.
- **Blue — Beat Repeat:** slider selects a subdivision rate from 8 zones (1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32). A 150ms debounce prevents accidental division changes while swiping.

## Phase Scrub lock persistence (v1.4.6)

Phase Scrub lock state and locked slider position now survive power cycles. On boot with lock restored, the slider enters pickup mode automatically.

## Sub-step page navigation (v1.4.6)

**Combo:** Shift + Page button while in sub-step mask edit mode.

Changes page without exiting sub-step edit mode. The focused step clears so you can focus a step on the new page.
