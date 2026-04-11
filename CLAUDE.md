# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Firmware for the Catalyst Sequencer and Catalyst Controller Eurorack modules (4ms Company). Target: STM32F401CE (ARM Cortex-M4), C++20/23.

## Project Documents

| File | Purpose |
|------|---------|
| `CHANGELOG.md` | Feature history by version, implementation notes for each addition |
| `TODO.md` | Backlog and v1.4.6 work items with area pointers |
| `docs/PANELMAP.md` | Full hardware reference: all buttons, jacks, LEDs, and their code names |
| `docs/CALIBRATION.md` | DAC calibration procedure (boot-only entry, offset/slope per channel) |
| `docs/PERFORMANCE.md` | Flash layout, SRAM budget, cycle cost estimates per feature |
| `docs/implemented/CATSEQ-PHASESCRUB-SETTINGS.md` | Post-implementation notes for Phase Scrub Performance Page (v1.4.6); includes the alpha1 calibration corruption bug root cause and fix |
| `docs/planned/CATSEQ-GATE-ENVELOPE.md` | Gate envelope (A/D function generator) feature spec |
| `docs/planned/CATSEQ-STEP-ARP.md` | Step arpeggio feature spec |
| `docs/wiki/NEW_FEATURES.md` | End-user-facing feature documentation (mirrored to GitHub wiki) |
| `docs/wiki/VOLTSEQ.md` | VoltSeq user-facing wiki (also mirrored to GitHub wiki) |
| `docs/VOLTSEQ-MODES.md` | VoltSeq UI state machine reference: every mode, state variable, entry/exit path, LED display, and known collision points |
| `VOLTSEQ-TODO.md` | VoltSeq feature backlog, known bugs, and hardware verification checklist |
| `docs/release-notes/` | Per-release changelogs for GitHub releases |

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

## GitHub Actions

Two workflows run on the `voltagecontrolled/catalyst-firmware` fork:

- **Wiki sync** (`.github/workflows/wiki.yml`): triggers on any push to `main` that touches `docs/wiki/NEW_FEATURES.md`. Copies it to the GitHub wiki as `New-Features.md`.
- **Release** (`.github/workflows/release.yml`): triggers on a tag matching `catalyst-v*`. Builds firmware and WAV, resolves release notes from `docs/release-notes/<tag-without-prerelease-suffix>.md`, and creates a GitHub release with the WAV attached.

To publish a release: `git tag catalyst-v1.x.y && git push origin catalyst-v1.x.y`. Strip any `-alpha*` or `-rc*` suffix — the workflow does that automatically when looking up the release notes path.

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

**The two modes correspond to two distinct physical products with different front panels.** The panel must be physically removed and flipped to switch between them. See `docs/PANELMAP.md` for the full hardware reference. Button identifiers in code use internal names; both panels label the same physical controls differently:

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

Settings (sequencer state, macro banks, DAC calibration) are saved to internal flash. See `src/f401-drivers/saved_settings.hh` and `conf/flash_layout.hh` for memory layout. Feature history and current version status: see `CHANGELOG.md`. Planned work: see `TODO.md`.

---

## Known pitfalls

### Changing `sizeof(Step)` or persistent struct layout

Any time you change `sizeof(Step)`, add/remove fields from `Sequencer::Data`, or change the layout of any persistent struct, **increment `Sequencer::Data::current_tag`**. This forces a clean reset on first boot rather than a crash. The equivalent for `Shared::Data` is `CurrentSharedSettingsVersionTag` in `saved_settings.hh` — bump it and add a migration case when adding fields (see the v1.4.5→v1.4.6 migration as a template).

---

### `GatePattern::bits` is wider than the valid range

`GatePattern::max = 5`, but `GatePattern::bits = std::bit_width(5) = 3`, so the bitfield can store 0–7. Values 6–7 are invalid. `GatePattern::Get()` clamps to `Off` for out-of-range values; `Step::Validate()` rejects them. Apply the same pattern whenever a bitfield width exceeds its valid range.

---

### Mode-crossing encoder logic — always test the neutral state

When writing bidirectional encoder logic that crosses modes (e.g. ratchet ↔ neutral ↔ repeat), test the neutral (zero) state explicitly. It's the most common starting point and the easiest to miss in conditional guards.

---

### Migration overlay structs must match `alignas` exactly

When writing a local struct to overlay flash data during migration (see `SavedSettings::read()` in `saved_settings.hh`), every `alignas(N)` specifier from the original struct must be reproduced exactly. The most dangerous case is `enum class : uint8_t` members declared `alignas(4)` — the underlying type is 1 byte, so without the explicit specifier the compiler packs the following member 2 bytes earlier than in the real struct, silently corrupting everything after it.

Add a `static_assert(sizeof(OverlayStruct) == ExpectedSize)` (or compare against `sizeof(RealStruct)` if available in scope) in any migration branch that uses a locally-defined overlay type. See `docs/implemented/CATSEQ-PHASESCRUB-SETTINGS.md` (Post-implementation Notes) for the full account of how this bug corrupted DAC calibration in v1.4.6-alpha1 and what the 12-cent/octave drift symptoms looked like.

---

### Stack overflow from large struct temporaries in `Load()`

Never use `x = T{}` to reset a large struct — use `new (&x) T{}` instead. `= T{}` causes the compiler to allocate stack space for the temporary at function entry; on this target `sizeof(Sequencer::Data)` is ~34KB, which overflows the stack before `Load()` even executes. Placement new constructs directly into existing storage with no temporary.

---

## Commit message format

One sentence, no Co-Authored-By or attribution lines. Use the formula:

```
v<version>-<label>: <what changed>
```

Examples:
- `v1.4.6-alpha1: substep page nav, phase scrub persistence and settings menu`
- `v1.4.6-alpha2: scrub settings encoder 1 unlit when quantization is off`
- `v1.4.5: fix sub-step 0 silencing not working`

For commits not tied to a specific version (docs, planning, spec files):

```
<short imperative description>
```

Examples:
- `planning for step clock divisions`
- `performance audit results & docs cleanup`
