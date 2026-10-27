# Inkroom

A drawn, rhythmic reverb by Imagine Plugins. You draw how the room moves (Level, Size, Length, Pan and Drive) and every lane loops in time with the host's tempo.

Version 0.3 has two modes.

**Add space** generates a new reverb and shapes its tail:

- Room, Plate and Spring reverbs, each made of four generated spaces crossfaded by the Size and Length lanes
- Five drawn lanes, edited as draggable points or painted freehand, with stamps, Smooth, Invert, Undo and Redo
- Locked to the host's tempo and bar position, with Swing and Drift
- Tail-only saturation (Tape, Tube, Fold), into the room or on the tail
- Eight presets, and everything saves with your session

**Shape space** reshapes the room that's already in your audio (room mics, a reverb bus):

- Each hit is split live from the ring after it, using 3 ms of lookahead that Logic compensates for automatically
- Six drawn lanes: Sustain and Punch (the ring and the hits, from silent to +6 dB), Level, Width, Pan and Drive on the ring
- Split controls (Hit length, Sensitivity, Hits) and a light that shows each detected hit
- Seven presets

**Editing (both modes)**

- Zoom in time with Cmd + scroll or a trackpad pinch, or the Fit, Bar and Beat buttons; an overview strip shows where you are, and Expand gives the canvas the full width
- The grid gets finer as you zoom (down to 1/64, plus triplets in their own tint); Shift snaps to it
- Lanes are stored at 2048 steps per loop, so points land where you put them
- Hold Cmd to select: drag a box or click points, then drag to move them together, Option-drag to deepen or flatten them, arrows to nudge, Delete to remove
- A readout shows each point's exact position (bar.beat.16th + ms) and value while you drag
- Shapes menu: rhythmic shapes, geometric outlines, and two-lane paths that draw in space, with Repeat and Rotate
- Lane menu: copy, paste, paste inverted, and link a lane to follow another (matched or inverted)
- Save your own presets from the Save button; they're listed under "Your presets" in each mode
- Works on mono and stereo tracks

The window resizes from the bottom-right corner and remembers its size.

Per-hit mode, the waveform view, Added space, curves and loading your own impulse responses are next.

## Getting a build

Every push builds the Mac plugin on GitHub and publishes it as the latest release. Download **Inkroom-mac.dmg** from https://github.com/jordanperlson-oss/Inkroom/releases/latest and follow [INSTALL.md](INSTALL.md).

## Building it yourself

```
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target Inkroom_AU Inkroom_VST3
```

The build fetches JUCE 8 automatically. JUCE has its own license terms; check them before selling Inkroom.

## Layout

- `Source/PluginProcessor.*`: the audio engine, tempo sync, presets and saved state
- `Source/PluginEditor.*`: the interface
- `Source/ShapeCanvas.*`: the drawing canvas (zoom, selection, readout)
- `Source/IRGen.h`: generated room, plate and spring impulse responses
- `Source/Presets.h`: the factory presets
- `tests/`: offline checks that render audio through both modes and measure levels (build with `-DINKROOM_TESTS=ON`)
