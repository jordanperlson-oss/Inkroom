# Inkroom

A drawn, rhythmic reverb by Imagine Plugins. You draw how the room moves (Level, Size, Length, Pan and Drive) and every lane loops in time with the host's tempo.

This is the first plugin build (MVP 0.1). It covers **Add space** from the browser prototype:

- Room, Plate and Spring reverbs, each made of four generated spaces crossfaded by the Size and Length lanes
- Five drawn lanes, edited as draggable points or painted freehand, with stamps, Smooth, Invert, Undo and Redo
- Locked to the host's tempo and bar position, with Swing and Drift
- Tail-only saturation (Tape, Tube, Fold), into the room or on the tail
- Eight presets, and everything saves with your session

Shape space (room mics), per-hit mode, curves and loading your own impulse responses are next.

## Getting a build

Every push builds the Mac plugin on GitHub. Open the **Actions** tab, click the latest **Build Mac plugin** run, and download **Inkroom-mac** under Artifacts. Then follow [INSTALL.md](INSTALL.md).

## Building it yourself

```
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target Inkroom_AU Inkroom_VST3
```

The build fetches JUCE 8 automatically. JUCE has its own license terms; check them before selling Inkroom.

## Layout

- `Source/PluginProcessor.*`: the audio engine, tempo sync, presets and saved state
- `Source/PluginEditor.*`: the interface, including the drawing canvas
- `Source/IRGen.h`: generated room, plate and spring impulse responses
- `Source/Presets.h`: the factory presets
- `tests/InkroomCheck.cpp`: offline check that renders audio through the engine (build with `-DINKROOM_TESTS=ON`)
