# DAC Calibration

Fine trims for each CV output channel. Corrects two types of hardware DAC error:

- **Offset** — a fixed voltage shift that affects all outputs equally (e.g. 0V actually measures −20mV on every note)
- **Slope** — a scaling error proportional to distance from 0V (e.g. each octave up is a few cents flat or sharp)

Each of the 8 output channels has an independent offset and slope correction, adjustable up to ±100mV.

---

## What you need

A way to measure the CV output of each channel. Either:
- A multimeter set to DC volts, or
- A VCO patched to a tuner

---

## Entering calibration

Hold **Shift + Glide** (Sequencer panel) or **Shift + Morph** (Controller panel) before powering the module, and keep both held while it boots. The calibration screen appears instead of the normal startup animation.

The lit page button shows which test voltage is currently being sent to all CV outputs.

---

## Page buttons — test voltages

| Page | Voltage |
|------|---------|
| 1 | −4.75V |
| 2 | −3V |
| 3 | 0V |
| 4 | +2V |
| 5 | +4V |
| 6 | +5V |
| 7 | +8V |
| 8 | +9.75V |

Press a page button to switch to that reference voltage. Only one is active at a time.

---

## Encoder LEDs

While adjusting, each encoder's LED shows the current correction value for that channel:

- **Blue, brighter** = larger positive correction
- **Red, brighter** = larger negative correction
- **Off** = no correction (default)

---

## Adjustment

**Shift + turn encoder N** — adjusts **offset** for channel N (shifts output voltage up or down)

**Copy + turn encoder N** — adjusts **slope** for channel N (corrects per-octave tracking error)

Encoders do nothing unless Shift or Copy is held.

---

## Recommended procedure

### 1. Set offset at 0V

Press page button **3** (0V). Patch a channel into your meter or VCO. Hold **Shift** and turn that channel's encoder until the output reads exactly 0.000V (or the VCO is in tune at your reference pitch). Repeat for each channel.

Slope has no effect at 0V — the slope correction multiplies distance from 0V, so 0V is always a fixed point regardless of slope setting. Set offset first.

### 2. Correct slope at a high voltage

Press page button **7** or **8** (+8V or +9.75V). Hold **Copy** and turn each encoder until the high-voltage output is correct relative to the 0V point you just set. Error at 8V is 8× the per-volt error, so this reference makes small slope differences easy to detect.

Spot-check by switching back to page 3 — 0V should still be correct. If it drifted, the slope adjustment interacted with an uncorrected offset; re-do step 1 first.

### 3. Save and exit

**Chan. + Glide** (Sequencer) / **Bank + Morph** (Controller) — saves calibration and exits.

---

## Other controls

**Play** (alone) — discards all changes made since entering calibration and exits. Restores whatever was saved previously.

**Chan. + Play** / **Bank + Play** — resets all offset and slope values to zero (no correction). Does not exit — press Chan. + Glide afterward to save the reset, or Play to abandon it.

---

## Notes

- Calibration is only accessible at boot. There is no way to enter it from running firmware.
- Corrections are stored in `Shared::Data::dac_calibration` and persist across power cycles.
- If calibration data is lost or reset (e.g. after a firmware migration that cannot recover it), all corrections default to zero. Zero is a valid state — it means no correction is applied and the raw DAC output is used.
- Maximum correction range is ±100mV for both offset and slope. This is a fine trim, not a coarse tuning tool. If an output is more than 100mV off, check hardware (DAC chip, output stage, power supply).
