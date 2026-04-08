# VoltSeq TODO

Backlog for VoltSeq firmware features, known bugs, and porting work.

---

## Active / Next Up

### Live Encoder Recording in Normal View
**Desired:** In normal view (unarmed, not in any editor), turning an encoder while NOT holding a page button live-records a value into the current playing step for that channel. Encoder movement should be fast — coarse increments relative to the channel's configured Range — unless Fine is held, which switches to fine sub-semitone steps.

**Why a TODO:** The debounce interaction needs careful tuning. At 3kHz, an encoder turn produces a delta of 1 per detent. Applying a large increment per detent (e.g., ÷8 of the full range per step = 8192 units) may overshoot. Also need to decide: record on every tick the encoder moves, or only on positive edges? Probably coarse = range/32 per detent, fine = range/256 per detent. Gate length and Trigger values have narrower ranges so need separate scaling.

---

### Global Channel Length / Clock Div Sync
**Desired:** A combo in Global Settings (Shift + encoder) that sets ALL channels to the same length and/or the same clock division simultaneously. Useful for "syncing up" channels after independent editing, especially when setting up a fresh sequence.

**Proposed UX:** Shift held, enc 3 (Length) sets a "default length" as today but also immediately applies it to ALL channels with one extra confirmation press (e.g., press encoder knob or hold for 600ms). Similarly for enc 6 (Clock Div). Alternatively, a dedicated "apply global to all" page-button press while in Global Settings.

---

### Channel Length — View Without Editing
**Desired:** When navigating to Channel Edit and pressing enc 2 (Length) to see the current length, the display should appear without immediately consuming the first encoder detent as an edit. Consider a brief "view mode" window after Channel Edit is entered (or after channel focus) where the length feedback is shown passively before the first encoder turn registers as a change. Similar to a "hold to preview, turn to edit" interaction.

**Note:** Related to the existing 600ms display timeout added in alpha4. May be solvable simply by requiring a small minimum delta (e.g., 2 detents) before the first length edit registers.

---

### Clock Division Redesign — multipliers + triplets
**Current state:** `Clock::Divider::type` is a `uint8_t` 0–255; `Read()` = v+1 (pure integer division by 1–256). No support for multipliers or fractional ratios.

**Desired:** CCW from unity adds clock multipliers (×2, ×3, ×4); divisions include triplet-friendly values (÷3, ÷6, ÷12, ÷24) so channels can run at ⅓, ⅙, etc. of the master clock.

**Proposed design:**
Replace `Divider::type` storage with a **table index** into a fixed list of ratios:

```
Index  Ratio   Label
  0    ×4      ×4
  1    ×3      ×3
  2    ×2      ×2
  3    ×1      ×1   ← default (v=3)
  4    ÷2      /2
  5    ÷3      /3
  6    ÷4      /4
  7    ÷6      /6
  8    ÷8      /8
  9    ÷12     /12
 10    ÷16     /16
 11    ÷24     /24
 12    ÷32     /32
```

For multipliers (ratio < 1), `Divider::Update()` must fire more than once per master tick. Replace bool return with int8 fire-count. Caller (`OnChannelFired`) loops on the count. Internally use a **phase accumulator** (float 0..1) for clean sub-integer handling:

```cpp
int Update(float ratio):
    phase_acc += 1.0f / ratio
    fires = 0
    while (phase_acc >= 1.0f) { fires++; phase_acc -= 1.0f; }
    return fires
```

**Persistent layout change:** bump `VoltSeq::Data::current_tag` when the stored index replaces the raw uint8.

---

### Global BPM — 1 BPM increments + reference snap points
**Current state:** BPM is stored as `bpm_in_ticks`; adjusting by enc delta moves in tick units (non-linear, hard to set a deliberate tempo).

**Desired:**
- Encoder 6 (BPM/Clock Div) in Global Settings moves in **1 BPM steps** (convert current BPM to int, ±1, convert back to ticks).
- Add **snap points** at common tempos (e.g., 60, 80, 90, 100, 120, 130, 140, 150, 160, 180, 200 BPM). The snap should act like a slight magnetic pull — encoder must pass through +2 steps within 400 ms to skip past a snap point.
- Display note: wiki should list the snap BPM values so users know where to aim.

---

### Presets / Scene Save-Load
**No current implementation.** Users need a way to save and recall entire VoltSeq states (all 8 channels, steps, settings) as named presets — analogous to the sequencer's slot/scene system.

**Open questions:**
- How many presets? 8 (matching scene buttons)? 16?
- Flash budget: `sizeof(VoltSeq::Data)` ≈ 1.1 KB × 8 = ~9 KB. Needs flash-layout review.
- UX: long-press page button = save to that slot, short press = load? Or Shift+page = save, page = load?
- Does scene-switch happen instantaneously or on next step boundary?

---

## Main Sequencer Ports (future)

### Universal "Play/Reset exits any mode"
VoltSeq exits Channel Edit, Glide Step Editor, Ratchet Step Editor, and Performance Page on Play press. The main sequencer has modal states that don't share this pattern. Port the single-button-exits-everything UX to the main sequencer UI for consistency.

---

## Known Quirks / Low Priority

- In the Glide Step Editor and Ratchet Step Editor, page navigation requires Shift+Page. There is no way to navigate without Shift while in the editor. Consider allowing bare page presses to navigate (since exit is now Glide/Play only, there is no conflict).
- Phase rotate (Channel Edit enc 3) is destructive with no undo. Add a brief "rotate pending" state where the user must confirm (second turn = commit), or document clearly.
- Trigger pulse width (Channel Edit enc 4) has no display feedback. Consider a brightness ramp on enc 4 LED (dim = short pulse, bright = long pulse).
