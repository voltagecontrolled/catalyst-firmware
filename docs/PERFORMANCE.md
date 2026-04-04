# Performance and Memory Audit

Reference document for hardware constraints, current resource utilization, and the
estimated cost of each firmware addition. Update whenever a feature is added or the
build is profiled.

---

## Hardware

| Resource | Spec |
|----------|------|
| MCU | STM32F401CE (Cortex-M4, 84 MHz) |
| Flash | 512 KB internal |
| SRAM | 96 KB |
| Main loop rate | 3 kHz timer ISR |
| Cycles per ISR tick | ~28,000 (84 MHz ÷ 3 kHz) |

---

## Flash Layout

| Sector | Address | Size | Purpose |
|--------|---------|------|---------|
| 0–1 | 0x08000000 | 32 KB | Bootloader |
| 2 | 0x08008000 | 16 KB | Empty (reserved) |
| 3 | 0x0800C000 | 16 KB | Shared settings (WearLevel) |
| 4 | 0x08010000 | 64 KB | Macro settings (WearLevel) |
| 5 | 0x08020000 | 128 KB | Sequencer settings (WearLevel) |
| 6 | 0x08040000 | 128 KB | **Application firmware** |
| 7 | 0x08060000 | 128 KB | Bootloader receive buffer |

`SeqSettingsSectorSize` is currently calculated as 96 KB (sectors 2–4 range, legacy
v1.0 value). The comment in `flash_layout.hh` notes the desired value is 128 KB (sector
5 alone). Leave this alone unless reworking the storage layout.

---

## Flash Write Lifespan

STM32F401 flash endurance: **10,000 erase cycles per sector**.

All three settings pools use `WearLevel`, which appends writes within the sector and only
erases when the sector is full. Effective write lifespan per pool:

| Pool | Sector size | Data size | Effective writes before erase |
|------|------------|-----------|-------------------------------|
| Shared settings | 16 KB | ~small | Many thousands |
| Macro settings | 64 KB | 7,820 B | ~8 writes/erase × 10,000 = ~80,000 saves |
| Sequencer settings | 96 KB | `sizeof(Sequencer::Data)` | Similar order |

**Bottom line:** normal use (saving presets manually) will not approach the wear limit in
any realistic product lifetime. Features that write to flash on frequent events (e.g. each
encoder turn) should be avoided; writing on deliberate user actions (save, lock toggle)
is fine.

---

## Current Build Utilization

Last measured: v1.4.5-rc1 (`c8f0213`)

```
text (code in flash):   89,612 B   /  131,072 B app sector  =  68%
data (initialized RAM):    508 B
bss  (zero-init RAM):    6,272 B
```

`Params` (contains all sequencer + macro runtime state) is stack-allocated in `main()`.
Stack minimum reserved in linker script: 4 KB. Dominant RAM consumers:

| Object | Size | Notes |
|--------|------|-------|
| `Macro::Data` | 7,820 B | Stored even in sequencer mode |
| `Sequencer::Data` | TBD | Multiple slots × 8 channels × max steps × 8 B/step |
| Everything else | small | Interfaces, shared state, stack frame overhead |

Total RAM is 96 KB. No RAM pressure observed; the firmware runs comfortably within
budget. Additions to `Params` should be noted here.

---

## Per-Feature Impact Log

### v1.4.x additions (this fork)

| Feature | Flash (code) | RAM (runtime) | Flash writes | ISR cost |
|---------|-------------|---------------|--------------|----------|
| Phase Scrub Lock | ~tiny | 3 bools + 2 uint16 in UI object | None (not persisted yet) | Negligible |
| Ratchet expansion | ~tiny | None | None | Negligible |
| Repeat mode | ~small | `repeat_ticks_remaining[8]` (8 B) | On preset save | Negligible |
| Sub-step mask editor | ~small | `gate_repeat_fired[8]` + `gate_substep_idx[8]` (16 B) | On preset save | Negligible |
| Linked Tracks (gate clock follow) | ~1–2 KB | `step_fired[8]` (8 B) | On preset save | O(8) loop, ~100 cycles |
| Lavender CV replace | ~500 B | `replace_latch[8]` (16 B) | None | Per-step boundary: quantizer call × N linked chans |
| Teal gate-clock step-only | ~tiny | None (reuses clock follow state) | None | Negligible |

### Planned features (estimated)

| Feature | Est. flash | Est. RAM | Flash writes | Notes |
|---------|-----------|----------|--------------|-------|
| Step Arp | ~1–2 KB | `arp_pos[8]` (8 B) + interval table (~60 B) | On preset save | Per-advance: table lookup + semitone add |
| Phase Scrub persistence | ~tiny | `phase_locked` + `locked_raw` in `Shared::Data` | On lock toggle | Shared settings WearLevel write |
| Sub-step page nav | ~tiny | None | None | UI only |
| Conditional auto-reset | ~small | None | None | Validation logic only |
| Gate Envelope | ~3–5 KB | Envelope state per gate channel in `App` | On preset save | Per-tick: A/D compute × N envelope chans |
| Per-track reset settings | ~1 KB | `reset_mode[8]` in `Settings::Channel` | On preset save | Player check per reset event |

---

## Macro Mode Footprint

Macro mode code totals approximately **15 KB** in flash (from `nm` analysis):

| Symbol | Size |
|--------|------|
| `Macro::App::Update()` | 4,152 B |
| `Ui::Macro::Main::Update()` | 2,076 B |
| `Ui::Macro::Settings::PaintLeds()` + `OnEncoderInc()` | ~2,100 B |
| `Ui::Macro::Main::PaintLeds()` | 1,640 B |
| `Macro::Interface::Update()` | 652 B |
| Remaining Macro UI methods | ~4,000 B |

`Macro::Data` contributes **7,820 B** of RAM regardless of active mode (both
`Sequencer::Interface` and `Macro::Interface` are instantiated in `Params` simultaneously).

Removing Macro mode would recover ~15 KB code flash and ~8 KB+ RAM, and bring flash
utilization to approximately 57%. Not required for stability, but meaningful if headroom
ever becomes a concern or if Macro mode is simply unused.

---

## Notes on ISR Safety

- All additions run in the 3 kHz ISR (`cv_stream` timekeeper callback in `main.cc`).
  `ui.Update()` runs first, then `macroseq.Update()`.
- No dynamic allocation anywhere in the hot path.
- The M4's hardware floating-point unit handles the morph/phase math at minimal cycle
  cost. Simple integer additions (arp intervals, repeat counters) are effectively free.
- Features to avoid: anything that scans all steps (O(max_steps)) every tick, or that
  triggers flash reads (blocking on STM32) in the ISR.
