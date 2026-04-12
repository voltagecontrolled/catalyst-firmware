# Catalyst VoltSeq — Clock Division Redesign

Replaces the current integer-only clock divider with a table-indexed system that adds clock multipliers and triplet-friendly divisions.

---

## Current State

`Clock::Divider::type` is a `uint8_t` (0–255). `Read()` returns `type + 1`, giving pure integer divisions from ÷1 to ÷256. No multipliers, no fractional ratios.

---

## Proposed Table

Replace `Divider::type` storage with an index into this fixed table:

| Index | Ratio | Label | Notes |
|-------|-------|-------|-------|
| 0 | ×4 | ×4 | |
| 1 | ×3 | ×3 | |
| 2 | ×2 | ×2 | |
| 3 | ×1 | ×1 | **default** |
| 4 | ÷2 | /2 | |
| 5 | ÷3 | /3 | Triplet-friendly |
| 6 | ÷4 | /4 | |
| 7 | ÷6 | /6 | Triplet-friendly |
| 8 | ÷8 | /8 | |
| 9 | ÷12 | /12 | Triplet-friendly |
| 10 | ÷16 | /16 | |
| 11 | ÷24 | /24 | Triplet-friendly |
| 12 | ÷32 | /32 | |

---

## Multiplier Engine

For multipliers (index 0–2, ratio > 1), `Divider::Update()` must fire more than once per master tick. Replace the current bool return with an int8 fire-count. The caller (`OnChannelFired`) loops on the count.

Use a phase accumulator for clean sub-integer handling:

```cpp
int8_t Update(float ratio) {
    phase_acc += 1.0f / ratio;  // ratio < 1 for multipliers (e.g. ×4 → ratio=0.25)
    int8_t fires = 0;
    while (phase_acc >= 1.0f) { fires++; phase_acc -= 1.0f; }
    return fires;
}
```

At ×4, `1/ratio = 4.0`, so `phase_acc` increments by 4 per master tick and fires 4 times.

---

## Persistent Layout Change

`Divider::type` currently stores a raw division value (0–255). After this change it stores a table index (0–12). These are incompatible. **Bump `VoltSeq::Data::current_tag`** when implementing so stored presets reset cleanly rather than mapping old division values to wrong table entries.

---

## Open Questions

- Should the table be extended? Current range is ÷32 to ×4. ÷64 and ×8 could be added without much cost.
- LED display in Channel Edit (enc 6): how to visually represent the ratio? Could use color (green = multiplier, white = unity, orange = division) with brightness = magnitude, or a numeric display on the page buttons.
- How do multipliers interact with gate length? A ×4 multiplier firing 4 steps per master clock tick would need gate durations scaled down accordingly. Gate ratchets on those steps may also need care.
