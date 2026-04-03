# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Firmware for the Catalyst Sequencer and Catalyst Controller Eurorack modules (4ms Company). Target: STM32F401CE (ARM Cortex-M4), C++20/23.

## Build Commands

```bash
# First-time setup
git submodule update --init --recursive

# Build firmware (outputs to build/f401/)
make all

# Run unit tests
make test

# Create .wav for audio-cable bootloader update
make wav

# Flash via JLink
make jflash-app

# Clean
make clean
```

The top-level `make all` runs `cmake -B build -GNinja` if needed, then `cmake --build build`. Requires `arm-none-eabi-gcc` v12.3 and `ninja`.

## Tests

Unit tests use [doctest](https://github.com/doctest/doctest). Test files live in `tests/` and match `*_test.cc`, `*_tests.cc`, or `test_*.cc`.

```bash
# Run all tests
make test

# Run tests directly (after building)
tests/build/runtests

# Run a specific test case by name
tests/build/runtests -tc="<test case name>"

# List all test cases
tests/build/runtests --list-test-cases
```

Library-level tests exist separately:
```bash
make -f lib/cpputil/tests/Makefile
make -f lib/mdrivlib/tests/Makefile
```

## Code Style

Formatting is enforced via `.clang-format` (LLVM-based, tabs, 120-col limit, C++20):

```bash
clang-format -i src/file.hh
```

Identifier naming: CamelCase (enforced by `.clangd` clang-tidy config).

## Architecture

The main loop runs at 3kHz (`Timekeeper cv_stream`) and calls:
1. `ui.Update()` — reads hardware inputs (buttons, encoders, ADC)
2. `macroseq.Update()` — generates CV/Gate output values
3. `ui.SetOutputs(out)` — drives LEDs based on current output

### Dual-mode design (`src/app.hh`)

`MacroSeq` owns both `Sequencer::App` and `Macro::App` and delegates `Update()` to whichever mode is active (`params.shared.mode`). Mode is persisted in flash.

**The two modes correspond to two distinct physical products with different front panels.** The panel must be physically removed and flipped to switch between them. See `PANELMAP.md` for the full hardware reference. Button identifiers in code use internal names; both panels label the same physical controls differently:

| Code name (`c.button.*`) | Controller (Macro) panel | Sequencer panel |
|---|---|---|
| `shift` | Shift | Shift |
| `morph` | Morph / Range | Glide (Ratchet) |
| `bank` | Bank / Quantize (Gate) | Chan. / Quantize (Gate) |
| `fine` | Fine / Copy | Fine / Copy |
| `add` | Add / Delete | Tap Tempo |
| `play` | Play / Rec | Play / Reset |
| `scene[0–7]` | Scenes | Pages |
| slider (ADC) | Pathway | Phase Scrub |

When reasoning about UX or button combos, always clarify which mode is in scope and refer to controls by their panel label for that mode.

### Data model (`src/params.hh`, `src/conf/model.hh`)

`Params` is the top-level container, split into:
- `params.shared` — state shared between modes (mode selection, DAC calibration, scene buttons)
- `params.sequencer` — sequencer state (slots, steps, clock, player)
- `params.macro` — macro/CV automation state (banks, scenes, pathway, slew)

Constants: 8 channels (`NumChans`), 8 sequencer slots, 8 pages/slot, up to 64 steps/page, 8 macro scenes.

### Hardware layer (`src/f401-drivers/`)

All hardware I/O is encapsulated here. Key files:
- `controls.hh` — ADC, buttons, encoders, LEDs (via LP5024 I2C driver and direct GPIO)
- `outputs.hh` — DAC and gate outputs
- `conf/board_conf.hh` — pin assignments, timing constants
- `conf/model.hh` — user-facing constants (voltages, counts)
- `conf/build_options.hh` — compile-time feature flags

### UI (`src/ui/`, `src/ui.hh`)

`Ui::Interface` manages mode-specific UI classes. Macro and Sequencer each have their own UI implementation under `src/ui/macro*.hh` and `src/ui/seq*.hh`.

### Libraries (`lib/`)

- `cpputil/` — header-only C++ utilities (no stdlib dependency); used heavily throughout
- `mdrivlib/` — STM32 peripheral driver library; target-specific code under `target/stm32f401/` and `target/stm32f4xx/`
- `CMSIS/` + `STM32F4xx_HAL_Driver/` — vendor headers/HAL

### Non-volatile storage

Settings (sequencer state, macro banks, DAC calibration) are saved to internal flash. See `src/f401-drivers/saved_settings.hh` and `conf/flash_layout.hh` for memory layout.

---

## Fork changes (relative to upstream v1.3)

This is a personal fork. See `CHANGELOG.md` for the full feature log.

### Implemented features (v1.4.2 — hardware verified)

**Phase Scrub Lock** (`src/ui/seq_common.hh`, `src/ui/seq.hh`) — hardware verified v1.4.1
Fine + Glide (Ratchet) toggles a phase lock on the Phase Scrub slider. Pickup behavior on unlock. Encoder 8 LED blinks red as indicator.

**Ratchet/Repeat** (Sequencer mode, gate channels)
- `src/sequencer_step.hh` — `Step` struct expanded to 8 bytes (was 4). Separate `ratchet_repeat_count` (3 bits), `is_repeat` (1 bit), `morph` (4 bits) fields. `IncRatchetRepeat()`: CW = ratchet (subdivide), CCW = repeat (extend).
- `src/app.hh` — `Gate()` subdivides step into ratchet sub-steps using gate-width comparison per sub-step.
- `src/sequencer_player.hh` — `Update()` takes `const std::array<bool, NumChans>&` per-channel step array.
- `src/sequencer.hh` — `repeat_ticks_remaining` array; `Reset()` clears repeat counters. `Sequencer::Data` has `SettingsVersionTag` + `current_tag = 1`; `validate()` rejects tag mismatch. `startup_slot` default is now explicit `= 0`.
- `src/conf/palette.hh` — `Palette::Ratchet`: dim salmon→bright red for ratchets, dim teal→blue for repeats.
- `src/ui/seq_morph.hh` — Gate channel display: ratchet/repeat color by count.
- `src/f401-drivers/saved_settings.hh` — `write(SeqModeData &data)` stamps `SettingsVersionTag` before writing.
- `src/ui.hh` — `Load()` uses placement new (`new (&x) T{}`) instead of assignment to avoid stack temporaries.
- `tests/seq_test.cc` — Unit tests for `IncRatchetRepeat`, `Step::Validate`, version tag.

### Next session

**Hardware status:** v1.4.2 stable. Phase scrub lock, ratchet, repeat: fully verified.

**Next feature (v1.4.3):** User-editable sub-step masks via Page buttons — see SUBSTEP.md for the full spec.

**Future — mask table redesign:** Replace Mute (all off, redundant with gate width=0) with Last on (`ooox`). Proposed table: All on, All tied, First on, Last on, Alternating, Alternating tied. Needs `current_tag` bump — stored index 5 (Mute) becomes Alternating tied after the change.

**Future — gate probability iterative mode:** CW = random % (current), CCW = deterministic cycle (1/2, 2/2, 1/3... up to 5/5). Same signed-position model as ratchet/repeat. Needs 1 extra bit in the probability field — candidate: repurpose gate-channel morph bits (currently unused on gate channels). Player needs a per-channel cycle counter similar to `repeat_ticks_remaining`.

**Future — quantized glide / implicit arp (CV channels):** CW = smooth glide (current), CCW = quantized glide — CV steps through scale notes between prev and current step pitch. When paired with ratchets on the gate channel, produces an arp: ratchet count = arp speed, step pitches = arp range, active scale = valid notes. Self-contained implementation possible (divide step phase into N segments, snap to scale note per segment) without cross-channel state. Morph field would need to go signed — same 1-extra-bit constraint as probability iterative mode.

See the test procedure at the bottom of this file.

---

## Known pitfalls

### Changing `sizeof(Step)` or `Sequencer::Data` layout
**What broke:** Doubling `sizeof(Step)` from 4→8 bytes changed the entire `Sequencer::Data` struct layout. `startup_slot` landed at a different offset, so old flash data read as garbage → out-of-bounds `data.slot[garbage]` → hard fault on startup.

**Fix applied:** `Sequencer::Data` now has a `SettingsVersionTag` field (first field, so flash peeking works). `validate()` rejects any data where the tag doesn't match `current_tag`. `ui.hh` resets seq data to defaults on validation failure. `SavedSettings::write()` stamps the tag on every save.

**Rule for future changes:** Any time you change `sizeof(Step)`, add/remove fields from `Sequencer::Data`, or change the layout of any persistent struct, **increment `Sequencer::Data::current_tag`**. This forces a clean reset on first boot rather than a crash. The equivalent for `SharedData` and `MacroData` is already handled by `CurrentSharedSettingsVersionTag` in `saved_settings.hh`.

---

### `GatePattern::bits` is wider than the valid range
`GatePattern::max = 5`, but `GatePattern::bits = std::bit_width(5) = 3`, which means the `gate_pattern` bitfield can store 0–7. Values 6 and 7 are invalid and would previously walk off the end of `GatePattern::table[6]`.

**Fix applied:** `GatePattern::Get()` now clamps to `Off` for any `pattern > max`. `Step::Validate()` now rejects steps with `gate_pattern > GatePattern::max`.

**Rule:** Whenever a bitfield's width exceeds the valid value range, either add a `Get()`-style bounds guard at the use site or validate in `Step::Validate()`.

---

### `IncRatchetRepeat()` CCW direction — neutral state
The CCW branch used `!is_repeat && ratchet_repeat_count > 0` as the condition to enter repeat mode. This failed when the step was at neutral (count = 0 and is_repeat = 0), causing CCW turns to increment the ratchet count instead of entering repeat mode.

**Fix applied:** Changed condition to `!is_repeat` (no count guard). CCW from any non-repeat state — including neutral — now correctly enters repeat ×1.

**Rule:** When writing mode-crossing logic on a bidirectional encoder, test the neutral (zero) state explicitly — it's the most common starting point and easy to miss.

---

### Stack overflow from large struct temporaries in `Load()`
**What broke:** `Load()` used `= T{}` assignment to reset flash-load failures (e.g. `params.data.sequencer = Catalyst2::Sequencer::Data{}`). The compiler allocates stack space for the temporary at function entry. `sizeof(Sequencer::Data)` is ~34KB. With main()'s ~60KB stack frame already consuming most of the ~89KB stack, `Load()`'s frame overflowed the stack — hard fault before the first line of `Load()` executed.

**Fix applied:** Use placement new instead: `new (&params.data.sequencer) Catalyst2::Sequencer::Data{}`. This constructs directly into existing storage (already allocated in main()'s frame) with no temporary. Added `#include <new>` to `ui.hh`.

**Rule:** On embedded, never use `x = T{}` to reset a large struct — use `new (&x) T{}` instead. The structs involved (`Shared::Data`, `Sequencer::Data`, `Macro::Data`) are all trivially destructible, so skipping the explicit destructor call before placement new is safe.

---

## Hardware test procedure

Flash the firmware (`make wav`, then audio bootloader, or `make jflash-app` with JLink). No factory reset needed — v1.4.2 auto-resets seq data on first boot.

### 1. Phase Scrub Lock
- Set a gate channel, run the sequencer.
- Hold **Fine + Glide (Ratchet)** — encoder 8 LED should blink red briefly, then stay red while locked.
- Move the Phase Scrub slider — phase should not change while locked.
- Hold **Fine + Glide (Ratchet)** again to unlock — slider should not snap; it should resume control only after physically reaching the locked position (pickup). Encoder 8 LED blinks while picking up.

### 2. Ratchet (CW) and display ✓
- Set a channel to gate mode. Hold **Glide (Ratchet)** and turn a step encoder CW — encoder LED should go from off → dim salmon → bright red (up to 8x). **Verified.**
- Verify the step fires more rapidly when the sequencer plays.

### 3. Repeat (CCW) and display ✓
- Hold **Glide (Ratchet)** and turn a step encoder CCW from neutral — LED should go from off → dim teal → blue. Further CCW increases repeat count. **Verified.**
- CW from repeat passes back through neutral (off) before entering ratchet — no direct jump between modes.
- Play the sequencer — the repeated step should hold the playhead for N extra clock ticks, making the pattern longer.
- Press Play/Reset — repeat counters should clear and the pattern should restart cleanly.

### 4. Gate pattern mask (partial)
- Set a step to 4x ratchet. Hold **Glide (Ratchet) + Tap Tempo** — should enter mask edit mode.
- Turn a step encoder — LED color should cycle through: red → dim orange → dim pink → yellow → teal → off (patterns 0–5).
- Release Glide (Ratchet) — should exit mask mode.
- **Needs retest with a gate-duration-sensitive voice** (BIA is trigger-only — tied sub-steps and gate-width patterns are not audible). Expected behavior:
  - **All on (0):** 4 equal triggers — baseline ratchet behavior
  - **All tied (1):** one long sustained gate across all 4 sub-steps
  - **First on (2):** only first sub-step fires, other 3 silent
  - **Alternating (3):** sub-steps 1 and 3 fire, 2 and 4 silent
  - **Alternating tied (4):** sub-steps 1 and 3 fire with gate held through 2 and 4
  - **Mute (5):** silence

### 5. Mask on repeat steps (not working — v1.4.3)
- Repeat steps currently ignore mask patterns. Every repeat tick evaluates as sub-step 0 (always On), so only Mute has any effect.
- Do not test until v1.4.3 is implemented.

### 6. CV channels in Glide (Ratchet) mode
- Switch to a CV channel. Hold **Glide (Ratchet)** — encoder LEDs should show morph (grey→red gradient) as before.
- Hold **Glide (Ratchet) + Tap Tempo** — should NOT enter mask mode (gate channels only).

### 7. Save/load
- Set up a pattern with ratchets, repeats, and masks. Save to a slot. Load it back. Verify all settings are preserved.
