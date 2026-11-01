# Haunted Hall: art to request from ChatGPT

The app draws the screens from these pieces. Everything that changes (lit
steps, knob pointers, fader caps, meters, preset names, numbers) is drawn by
the app on top, so the backgrounds must leave those spots **empty**.

Attach your Haunted Hall mockup to every request so the style matches.

## Style line (paste at the top of every prompt)

> Match the attached Haunted Hall mockup's style exactly: dark purple-black
> panels, lavender bevelled hardware, mint-green neon accents, pixel-art
> graveyard trim with crosses and iron fences, haunted castle silhouettes, a
> faint CRT scanline texture. Flat front view, no perspective, no device
> frame, no logos, no brand names.

## 1. Three screen backgrounds (same size as your mockups, 1536 x 900)

**MASTER background.** Same layout as the "Fractured Cartridge" master
mockup, with these changes: the LEVEL meters are empty dark slots, the ON and
MUTE buttons are blank unlit pads, the four mixer faders are empty tracks with
no caps, the delay knobs are empty dark wells with tick marks but no knob
caps, the time display is an empty dark screen, the BYPASS light is off, and
the MASTER / DRUM tabs at the bottom are both plain (unselected). Keep the
static labels: DRUM, SYNTH A, SYNTH B, SYNTH C, LINE MIXER (4-CH), MASTER
DELAY (GLOBAL), TIME, FEEDBACK, MIX, LEVEL. Remove "DRUM MAX 20%" and "MAX
20%".

**DRUM / STEP background.** Same as the "Decrypted Cartridge" step-mode
mockup, with these changes: every grid cell is dark (no lit steps); remove
PLAY, STOP and LOOP; the bottom row is five blank buttons labelled RANDOM,
CLEAR, COPY, PASTE, UNDO; above the grid add a strip of 16 small blank bar
buttons, a two-way switch labelled 8 BARS / 16 BARS and a two-way switch
labelled A / B; under the grid add a thin empty strip labelled MELODY; the
four knobs are empty wells labelled DECAY, TONE, SPOOK and "SPACE · DELAY MAX
20%"; small padlock outlines at the left end of each row name (KICK, SNARE,
HAT, GHOST, FX).

**CARTRIDGE BROWSER background.** Same as the lavender cartridge browser
mockup, with these changes: the cartridge's picture window is empty (dark);
the preset list is 10 empty name slots in two columns; the theme carousel at
the bottom is 6 empty square slots with arrows; the four category tabs at the
top (drum, bass guitar, wave, keyboard icons for Drum, Bass, Lead, Pad) are
all plain; add a slim strip with four small empty knob wells labelled DECAY,
TONE, SPOOK, SPACE; keep the RANDOM die, FAVORITE, LOAD PRESET and BACK
buttons blank and unlit.

## 2. Nine cartridge pictures (one prompt each)

> Pixel art, chunky pixels as if drawn on a 96 x 96 grid, 16-colour palette
> in the Haunted Hall colours, square, no border, no text: **[subject]**.

Subjects: a haunted castle under a full moon; a skeleton drummer; a haunted
pipe organ in a gothic chapel with a ghostly organist; an alien saucer
beaming up a figure over pine trees; a sports car on a neon night highway; a
boss dragon roaring over a castle; a glowing crystal sword in a stone; a
floating platform block level with coins; a flock of flying ghosts over a
graveyard.

## 3. Sprite sheets (transparent background)

> Pixel-art sprite sheet on a transparent background, frames in one row,
> evenly spaced, same size each, no text:

- the floating ghost from the mockup, **8 frames** of a gentle bob and wave
- a bat flapping, **4 frames**
- the glitch pixels that fly off the cartridge, **4 frames**
- the "DECRYPTING..." progress bar as separate pieces: empty bar, one lit
  segment, the label

## 4. Small parts sheet (transparent background, laid out on a grid)

A grid step cell in four states (off, on, accent, soft); a knob cap with **no
pointer**; a fader cap; ON lit and unlit; MUTE lit and unlit; BYPASS light on
and off; a bottom tab selected and unselected; a blank button up and pressed;
a small padlock open and closed; a FAVORITE star on and off.

## After you send them back

I cut, scale and name the pieces (pixel art scaled with hard pixel edges), lay
out the screens to match, and check the memory and redraw budgets in
`SCREENS.md`.

## Fonts

For the text the app draws (preset names, numbers), use a free pixel font
under the SIL Open Font License so it can ship with open-source code, for
example Silkscreen or VT323. The big "HAUNTED HALL" title stays baked into
the art.
