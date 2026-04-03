**Note: This firmware is not supported or endorsed by 4ms Company and flashing third party could void your warranty or render your module inoperable. The author assumes no liability for any damage resulting from use of this firmware**

_WARNING: Old presets from previous releases will NOT work with this firmware._

 ## Linked Tracks (v1.4.4)

  CV transpose follow and gate track clock follow. Four sub-modes selectable per CV track via SHIFT + Chan. + Glide (tap to enter persistent
  mode), then COPY to cycle:

  - **Blue** - CV transpose follow: source CV track pitch offsets this track
  - **Orange** - Gate clock, ratchets only: each ratchet sub-step advances the CV track
  - **Yellow** - Gate clock, repeats only: each repeat tick advances the CV track
  - **Salmon** - Gate clock, ratchets + repeats: both advance the CV track

  Ratchet sub-step detection runs at 3kHz, so each sub-step produces a distinct pitch from the CV track.

  **Settings auto-reset on first boot** (version tag 5 - incompatible with prior saves).

  See the [New Features wiki](https://github.com/voltagecontrolled/catalyst-firmware/wiki/New-Features) for full usage.

  > **Known quirk:** switching between Orange/Yellow/Salmon while a gate track is assigned keeps the page button lit but does not re-apply the
   mode. Unlink and re-link to switch modes on an already-assigned track.

 ## Bugfixes (v1.4.4)
- Fixed a crash caused when attempting to save a preset.
