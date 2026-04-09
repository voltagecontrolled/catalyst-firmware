# VoltSeq — Presets / Scene Save-Load

No current implementation. Users need a way to save and recall entire VoltSeq states (all 8 channels, steps, settings) as named presets — analogous to the sequencer's slot/scene system.

---

## Open Questions

### Capacity
- How many presets? 8 (matching page buttons)? 16?
- Flash budget: `sizeof(VoltSeq::Data)` ≈ 1.1 KB × 8 slots = ~9 KB. Needs flash layout review against `docs/PERFORMANCE.md`.

### UX
- **Save vs. load gestures:** Long-press page button = save to slot, short press = load? Or Shift+page = save, page = load? Or a dedicated mode?
- **Confirmation:** Should saving to an occupied slot require a confirm gesture (e.g., press twice) to prevent accidental overwrites?
- **Slot state display:** How to show which slots are occupied vs. empty on the page button LEDs? Dim = occupied, bright on hover?
- **Scene-switch timing:** Does switching happen instantaneously or at the next step/bar boundary? Instantaneous is simpler; boundary-aligned avoids rhythmic glitches during live performance.

### Partial recall
- Should loading a preset restore all 8 channels, or allow selective recall (e.g., hold page button = load only that channel's data from the slot)?
- Should BPM and Global Settings be included in the preset, or be global/independent of presets?

---

## Implementation Notes

- Needs new flash sector(s) or expansion of the existing VoltSeq flash layout.
- `VoltSeq::Data::current_tag` bump not strictly required if presets are stored in a separate region, but the preset region needs its own version tag.
- Consider whether presets should survive a `current_tag` reset (firmware update wiping main data). Probably not — simpler to treat them as ephemeral across firmware updates.
