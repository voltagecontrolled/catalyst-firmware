**Note: this is a pre-release firmware, use at your own risk. This is not supported or endorsed by 4ms Company and flashing third party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

## Phase Scrub lock persistence (v1.4.5)

Phase Scrub lock state now survives reboot. If the slider is locked when the module powers down, it will be locked on next boot at the same position. The slider enters pickup mode automatically on boot since its physical position may have drifted.

## Sub-step page navigation (v1.4.5)

Sequences longer than 8 steps can now be edited in sub-step mask edit mode without exiting. Press **SHIFT + PAGE** to change pages while the mode is active. Previously the only way to exit was Tap Tempo or Play/Reset, losing the edit context.

## Firmware upgrade preset preservation (v1.4.5)

Presets are no longer wiped on every firmware upgrade -- only on upgrades that change the underlying data layout. Upgrading within the 1.4.x series will preserve saved patterns.

See the [New Features wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki/New-Features) for full usage.
