**Note: this is a pre-release firmware, use at your own risk. This is not supported or endorsed by 4ms Company and flashing third party firmware could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware.**

## Phase Scrub lock persistence (v1.4.5)

Phase Scrub lock state now survives reboot. If the slider is locked when the module powers down, it will be locked on next boot at the same position. The slider enters pickup mode automatically on boot since its physical position may have drifted.

## Sub-step page navigation (v1.4.5)

Sequences longer than 8 steps can now be edited in sub-step mask edit mode without exiting. Press **SHIFT + PAGE** to change pages while the mode is active. Previously the only way to exit was Tap Tempo or Play/Reset, losing the edit context.

## Firmware upgrade preset preservation (v1.4.5)

Presets are no longer wiped on every firmware upgrade -- only on upgrades that change the underlying data layout. Upgrading within the 1.4.x series will preserve saved patterns.

See the [New Features wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki/New-Features) for full usage.

## Linked Tracks: new follow modes and quirk fixes (v1.4.5)

Two new follow modes have been added:

- **Teal -- Gate clock, step only:** the CV track advances once per gate step, ignoring ratchets and repeats. Useful when a ratcheted gate track drives rhythm but you want steady one-pitch-per-step CV movement.
- **Lavender -- CV replace:** the source CV track's current value replaces this track's step value entirely, rather than offsetting it. Use when you want hard pitch control from another sequence rather than transposition.

The page button quirk when switching between gate clock follow modes has also been fixed. Previously the button for an assigned track would remain lit when switching modes, and pressing it would not correctly deactivate the old assignment. Page buttons now only light in the mode where the assignment was made, and pressing a lit button correctly unlinks.

## Bugfixes and small changes (v1.4.5)

- Fixed an issue where entering Channel Settings (SHIFT + Chan.) would sometimes immediately jump to Follow Assign mode without requiring a Glide tap. Caused by Glide button bounce or overlap during the entry combo.
- Sub-step 1 can now be silenced in the sub-step mask editor. Previously it was forced on. Silencing all sub-steps on a step produces no output for that step's duration, enabling microtiming effects (e.g. a 2x ratchet with only sub-step 2 active fires halfway through the step).
