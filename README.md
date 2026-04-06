# Catalyst Sequencer and Catalyst Controller

Alternate Firmware for the Eurorack modules from 4ms Company.

---

## This Fork

This is a personal fork of [4ms-company/catalyst-firmware](https://github.com/4ms/catalyst-firmware) (upstream v1.3). It adds new sequencer features while keeping the core architecture intact.

### Breaking change: presets are not compatible with upstream v1.3

The Step struct was expanded from 4 to 8 bytes in v1.4.2 to support ratchet/repeat data. This changes the on-flash layout of all sequencer data. **Presets saved under v1.3 cannot be loaded by this firmware and will be discarded on first boot.** A version tag system prevents crashes -- the firmware detects the mismatch and resets to defaults automatically. No manual factory reset is needed.

If you need to preserve v1.3 presets, do not flash this firmware.

---

For a user-friendly guide to all added features, see the [New Features wiki page](https://github.com/voltagecontrolled/catalyst-firmware/wiki/New-Features).

---

# Current Version Overview:

[![v1.4.5 Preview Video](https://img.youtube.com/vi/L5W8HPROsC4/0.jpg)](https://www.youtube.com/watch?v=L5W8HPROsC4)

## Added Features

### Phase Scrub Lock (v1.4.1)
**Combo:** COPY + GLIDE (short press — release either button before 1.5s)

Locks the Phase Scrub slider at its current value. A second press unlocks with pickup behavior -- the slider resumes control only after physically reaching the locked value, preventing jumps. Encoder 8 LED blinks red while locked or during pickup.

### Ratchet expansion (v1.4.2)
**Combo:** Hold Glide (Ratchet), turn step encoder CW

Ratchet (subdivide) count expanded from 4x to 8x maximum. Displayed as a dim salmon to bright red gradient in Glide mode.

### Step Repeats (v1.4.2)
**Combo:** Hold Glide (Ratchet), turn step encoder CCW

A new per-step mode that fires the step N additional times at the current clock division, extending the effective pattern length. An 8-step pattern with one step set to repeat x3 becomes 11 clock ticks long. Displayed as a dim teal to blue gradient in Glide mode.

### Sub-step mask edit mode (v1.4.3)
**Entry:** Hold Glide (Ratchet) + Tap Tempo on a gate channel, then release both buttons

Persistent mode for editing which sub-steps of a ratcheted or repeated step fire or are silenced. Page buttons show and toggle each sub-step's on/off state. All sub-steps including the first can be silenced.

### Linked Tracks: CV follow and gate clock follow (v1.4.4 / v1.4.5)
**Entry:** SHIFT + Chan., then tap Glide (Ratchet) on a CV track. Press FINE to cycle sub-modes.

A CV track can follow another track as a source. Six sub-modes:

- **Blue** — CV add follow: source CV pitch added as a transpose offset each tick
- **Lavender** — CV replace: target outputs source pitch at intersecting steps; 0V acts as pass-through
- **Orange** — Gate clock, ratchets only: CV advances one step per ratchet sub-step on the gate track
- **Yellow** — Gate clock, repeats only: CV advances once per repeat tick on the gate track
- **Salmon** — Gate clock, ratchets + repeats: advances on both
- **Teal** — Gate clock, step only: CV advances once per gate step, ignoring ratchets and repeats

### Phase Scrub Performance Page (v1.4.6)
**Entry:** COPY + GLIDE held 1.5 seconds. **Exit:** COPY + GLIDE or Play/Reset.

A dedicated performance page for Phase Scrub. All settings survive reboots.

| Encoder | Function |
|---------|----------|
| 1 | Quantize (standard/granular: snap to step boundary; beat repeat: snap entry to nearest beat) |
| 2 | Slider mode: off / granular / beat repeat 8-zone / beat repeat 4-zone |
| 3 | Granular width or beat repeat debounce delay (context-sensitive) |
| 4 | Orbit direction (granular only): forward / backward / ping-pong / random |
| 8 | Phase Scrub Lock |

Page buttons toggle per-track scrub participation. Phase Scrub Lock now persists across power cycles.

Sub-step page navigation (v1.4.6): SHIFT + PAGE button while in sub-step mask edit mode changes page without exiting.

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
