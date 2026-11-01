# Haunted Hall: screen assets for KICK (1024 x 600)

Art made in ChatGPT from FlowSphereAI's mockups, cut and fitted for KICK.
Everything here loads once when Haunted Hall opens; nothing loads during playback.

## Files

| Folder | Contents | Drawn how |
|---|---|---|
| `backgrounds/` | `bg_master`, `bg_step`, `bg_browser`, 1024 x 600 | Once per screen change, as one image |
| `cartridges/` | 9 pictures, native 96 x 96 pixel art, 16 colours | Browser window at 2x, hard pixel edges; carousel at 48 x 48 |
| `sprites/` | ghost (8 frames), bat (4), glitch pixels (4), 64 px tall strips | 15-20 fps from one timer, each in its own small rectangle |
| `icons/` | padlock open / closed, star on / off, 20 x 20 | Row locks, FAVORITE |
| `HHLayout.h` | Every control's position on all three screens, plus the palette | Used by the screen code |
| `preview_*.png` | The three screens with their live parts drawn | Reference: what the app should look like |
| `art_tools/` | `layout.py` (the measurements), `preview.py`, `genheader.py`, `repair_step.py` | Change `layout.py`, rerun, and the header and previews follow |

## Drawn in code, not images

Step cells (off, normal, accent, soft), the playhead, knob caps, pointers and
tick rings, fader caps, meter segments, ON / MUTE / BYPASS states, the selected
tab, the selected preset, the decrypting bar, and all changing text (preset
names, bank name, delay time, BAR 01/08). Colours come from your parts sheet
(`ui::colour` in `HHLayout.h`).

## Fixes made to the art

- Step background: the SPOOK knob's doubled ring was replaced with a clean copy
  of the TONE knob; the pasted "SPACE · DELAY MAX 20%" box was replaced with
  "SPACE" rebuilt from the panel's own lettering plus a small "MAX 20%".
- Cartridge pictures: reduced to their true 96 x 96 pixels (no detail lost).
- Browser window is taller than square, so the 96 x 96 art is drawn at 2x with
  3-4 pixels trimmed from each side, and thin dark bands above and below.

## Memory

About 8 MB decoded in total (the three backgrounds are 7.4 MB of that), plus
1.2 MB for the audio engine. Files on disk: about 2.3 MB.

## Text

The previews use a stand-in monospace font. For the app, use a pixel font under
the SIL Open Font License (for example Silkscreen or VT323) so it can ship with
open-source code.
