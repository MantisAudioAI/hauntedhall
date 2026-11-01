# Hooking Haunted Hall into KICK

What a KICK host needs to do. The engine and the screens are framework-neutral
apart from JUCE for the screens, so this is a thin layer.

| KICK side | Haunted Hall call | Thread |
|---|---|---|
| Program loads | `Engine::prepare(sampleRate)`; build `Assets` (`Assets::load` with a file loader); create `HauntedHallEditor` | message |
| Audio callback | `Engine::process(left, right, n, Transport{playing, bpm, beatPosition})` (adds into the buffers) | audio |
| KICK pads / MIDI in | `Engine::noteOn(layer, note, velocity, sampleOffset)` / `noteOff` (drum notes: 36 38 42 45 39) | audio |
| Touch previews from other screens | `Engine::trigger` / `release` | any |
| The four encoders | `HauntedHallEditor::hardwareKnob(index, delta)` | message |
| Transport display (30 Hz is plenty) | `HauntedHallEditor::setTransport(playing, bpm, beats)` | message |
| KICK's sequencer should own the notes instead | `exportNotes()` gives the pattern as a note list | message |
| Bounce a pattern to audio (for cratedust 12) | `captureState()` then `renderPatternOffline()` on a worker thread | worker |

Open questions for the maintainers:

1. Where Haunted Hall should live (the Analog '83 engine went to
   `Kick/modules/engine/audio_program/synth/drumsynth/`; this would sit next to it as `hauntedhall/`).
2. How a program with its own screens is registered (`MTProgramType::Plugin`?).
3. How KICK hands the four encoders and its transport to a program.
4. Where assets go on the device (the screens load ~2.7 MB of PNGs once, about 8 MB decoded).
5. Whether one program may write audio into another's sample storage (for the cratedust 12 bounce).
