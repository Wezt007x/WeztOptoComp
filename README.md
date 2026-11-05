# Wezt Opto Comp (VST3)

Original optical-style compressor. JUCE 8 + CMake, VST3 only.

## Build

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE is fetched automatically (needs git and internet on first configure). With `COPY_PLUGIN_AFTER_BUILD` on, the `.vst3` is copied to your system VST3 folder.
Requirements: CMake 3.22+, a C++17 compiler (MSVC 2022, Xcode, or GCC/Clang with the JUCE Linux dependencies).

## Controls

| Control | Function |
|---|---|
| Peak Reduction | Moves the threshold from 0 to -45 dB (more reduction) |
| Gain | Make-up gain, -12 to +24 dB |
| Limit | Switches ratio from 3:1 (Compress) to 12:1 (Limit) |
| Warmth | Biased tanh tube-style stage, unity small-signal gain |
| Mix | Parallel compression blend |
| Bypass | 10 ms click-free crossfade |

## DSP notes

- Detector is stereo-linked (loudest channel), with a 60 Hz sidechain high-pass and 5 ms RMS window.
- Program-dependent release: fast (10 ms / 60 ms) and slow (250 ms / 2.5 s) trackers in parallel; applied reduction is `max(fast, 0.7 * slow)`.
- 10 dB soft knee. Parameter IDs are stable (version hint 1).

## Known next steps

- Oversampling for the tube stage (aliasing at high drive)
- A/B compare and preset browser (the shared framework)
- Input meter and an output/GR meter selector
- Calibrate ballistics and thresholds by ear and against measurements

## No-install build (GitHub Actions)

1. Create a free GitHub account and a new repository.
2. Upload everything in this folder, including the hidden `.github` folder, keeping the folder structure.
3. Open the **Actions** tab. The "Build Wezt Opto Comp" workflow runs automatically (or press "Run workflow").
4. When it finishes (about 10 minutes), open the run and download the artifact for your OS under **Artifacts**.
5. Unzip it and copy the `Wezt Opto Comp.vst3` into your VST3 folder:
   - Windows: `C:\Program Files\Common Files\VST3`
   - macOS: `~/Library/Audio/Plug-Ins/VST3`
   - Linux: `~/.vst3`
