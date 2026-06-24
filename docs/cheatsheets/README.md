# Cheatsheets — work-in-progress

Printable / browser-viewable cheat sheets for Catalyst VoltSeq and (eventually) Catalyst Sequencer. Goal: fit on one letter-size page, mimic the panel layout enough to jog memory after time away from the module.

## Status

- `voltseq-main.html` — prototype, 4 cards done (Main, Channel Edit, Armed CV, Performance Page). Layout/template is stable; remaining modes still need to be authored.
- Sequencer (classic) cheat sheet — not started.

## Design decisions (locked in — do not re-litigate)

- **Light mode only.** Warm off-white background, white cards, rust-orange accent, navy-blue for Shift.
- **No hardware visual.** Earlier iterations drew jacks/slider/buttons; user found buttons "weren't pulling their weight". Only the **encoder zigzag** is kept as a visual.
- **Encoder zigzag.** Encoders 1–4 alternate low/high/low/high; 5–8 mirror as high/low/high/low. Each encoder + label sits in an equal-sized horizontal box with top/bottom padding so the knob doesn't collide.
- **Knob = small circle, `1.5px dotted` border** to suggest knurled metal. Dotted border is on the knob circle, NOT the surrounding box.
- **Identical card template across all modes** — no per-mode size variance.
- **Encoders / Page-buttons sub-cards = prose** with middot (`·`) separators. Earlier tried a 3-col grid here; user rejected ("modifiers don't line up with the control they are modifying, plus it creates too much whitespace").
- **"Other Combos" sub-card = 3-column grid** `[modifier prefix | trigger | result]`, grouped by modifier (modifier shown once, empty span for continuation rows). Row gaps via `row-gap: 2px`, no border-bottom on rows.
- **All buttons same size** across cards (70×28px) where buttons appear at all.
- Cheat-sheet button label is **"Glide / Ratchet"** (per `docs/PANELMAP.md`), NOT "Glide / Prob".

## Source-of-truth hierarchy

1. **Code** (`src/ui/voltseq.hh`, `src/ui/seq.hh`) — authoritative
2. `docs/wiki/Catalyst-VoltSeq.md` — long-form manual, cross-reference
3. `docs/wiki/VoltSeq-Cheat-Sheet.md` — original combo list, condensed

If wiki and code disagree, code wins. Fix the wiki.

## Known wiki bugs to fix (audited via code, not yet applied)

- `docs/wiki/Catalyst-VoltSeq.md:56` — says Global Settings hold = "2 s"; code (`voltseq.hh:104`, `kGlobalSettingsHoldTicks = MsToTicks(1000)`) says **1 s**. Lines 320 and 449 already say 1 s — line 56 is the outlier.
- `docs/wiki/Catalyst-VoltSeq.md:365` — claims Performance Page supports "Hold Page + turn Encoder → step editing". Code (`voltseq.hh:1085–1109`, `UpdatePerfPage()`) has **no step editing**. Remove that row.
- **Copy is NOT implemented in VoltSeq.** Only in classic Sequencer (`src/ui/seq.hh:94-96`). Add a note in the wiki.
- **Chan + Encoder cycles channel type AND scale**, not just scale (code: `voltseq.hh:879-885`). Tighten the manual language.
- Save gesture hold = **800 ms** (`voltseq.hh:105`, `kSaveHoldTicks = MsToTicks(800)`). Confirmed, no change needed.

## Remaining cheat-sheet work

Cards still to author in `voltseq-main.html` (use the existing template — prose sub-cards for Encoders/Page-buttons, optional 3-col grid for Other Combos):

- Armed Gate (`Chan. + Page N` on a Gate-typed channel)
- Armed Trigger (`Chan. + Page N` on a Trigger-typed channel)
- Glide modifier (unarmed, channel-level)
- Probability / Random (`Shift + Glide` while armed)
- Clear Mode (hold `Fine + Play` ~500 ms)
- Global Settings (hold `Shift + Chan.` 1 s)
- Mode Switch (CatSeq ↔ VoltSeq combos)

Other deferred items:

- **Verify silkscreen left-to-right encoder order.** The current Channel Edit card numbers them: 1=Start, 2=Dir, 3=Length, 4=Phase, 5=Range, 6=BPM/Div, 7=Trans, 8=Random. User has not explicitly confirmed this matches the physical silkscreen order. Worth double-checking against a photo or `docs/PANELMAP.md` before publishing.
- **Mobile swipe nav.** Eventual goal — index page + swipeable cards. Not started.
- **Sequencer (classic) cheat sheet** — separate file, same template.

## How to preview

Open `voltseq-main.html` in a browser. Print preview shows the letter-size layout. CSS print stylesheet uses `@page { size: letter; margin: 0.4in; }`. Mobile responsive via `@media (max-width: 700px)`.
