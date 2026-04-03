# Catalyst Sequencer and Catalyst Controller

Firmware for the Eurorack modules from 4ms Company.

---

## This Fork

This is a personal fork of [4ms-company/catalyst-firmware](https://github.com/4ms/catalyst-firmware) (upstream v1.3). It adds new sequencer features while keeping the core architecture intact.

### Breaking change: presets are not compatible with upstream v1.3

The Step struct was expanded from 4 to 8 bytes in v1.4.2 to support ratchet/repeat data. This changes the on-flash layout of all sequencer data. **Presets saved under v1.3 cannot be loaded by this firmware and will be discarded on first boot.** A version tag system prevents crashes -- the firmware detects the mismatch and resets to defaults automatically. No manual factory reset is needed.

If you need to preserve v1.3 presets, do not flash this firmware.

---

For a user-friendly guide to all added features, see the [New Features wiki page](https://github.com/voltagecontrolled/catalyst-firmware/wiki/New-Features).

---

## Added Features

### Phase Scrub Lock (v1.4.1)
**Combo:** Fine + Glide (Ratchet)

Locks the Phase Scrub slider at its current value. A second press unlocks with pickup behavior -- the slider resumes control only after physically reaching the locked value, preventing jumps. Encoder 8 LED blinks red while locked.

### Ratchet expansion (v1.4.2)
**Combo:** Hold Glide (Ratchet), turn step encoder CW

Ratchet (subdivide) count expanded from 4x to 8x maximum. Displayed as a dim salmon to bright red gradient in Glide mode.

### Repeat mode (v1.4.2)
**Combo:** Hold Glide (Ratchet), turn step encoder CCW

A new per-step mode that fires the step N additional times at the current clock division, extending the effective pattern length. An 8-step pattern with one step set to repeat x3 becomes 11 clock ticks long. Displayed as a dim teal to blue gradient in Glide mode.

### Sub-step mask edit mode (v1.4.3)
**Entry:** Hold Glide (Ratchet) + Tap Tempo on a gate channel, then release both buttons

Persistent mode for editing which sub-steps of a ratcheted or repeated step fire or are silenced. Page buttons show and toggle each sub-step's on/off state. Sub-step 0 cannot be silenced.

### CV track transpose follow (v1.4.4)
**Entry:** SHIFT + Chan., then tap Glide (Ratchet) on a CV track

A CV track can follow another CV track and use its output as a transpose offset. The source track's current pitch (relative to 0V) is added to the target track each tick.

### Gate track clock follow (v1.4.4)
**Entry:** SHIFT + Chan., then tap Glide (Ratchet) -- press FINE to switch to clock sub-mode

A CV track can designate a gate track as its clock source. The CV track advances one step per firing event on the gate track instead of following the master clock. Masked-off sub-steps (silenced via sub-step mask) do not trigger an advance.

---

## Planned Features

See [TODO.md](TODO.md) for the full backlog with implementation notes.

---

## Build

Requirements: arm-none-eabi-gcc v12.3, ninja

```
git clone https://github.com/voltagecontrolled/catalyst-firmware.git
cd catalyst-firmware
git submodule update --init --recursive
make all
```

To create a .wav file for the audio cable bootloader:

```
make wav
```

To run unit tests:

```
make test
```

The .wav file path is `build/f401/f401.wav`. Flash it via the audio bootloader or with `make jflash-app` if you have a JLink.

---

## Original README

Upstream project: [4ms-company/catalyst-firmware](https://github.com/4ms/catalyst-firmware)

These files are produced by the build:

- `f401.hex`: the main application firmware
- `f401-bootloader.hex`: the bootloader (only needed if you modified bootloader code)
- `f401-combo.hex`: bootloader plus application, used for production
