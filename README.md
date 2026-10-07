# Level Pocket 0.2.0

Broadband vocal compressor with a adaptive spectral detector, derived from Duck Pocket's JUCE UI assets and build infrastructure.

**Only the detector is spectral.** A single gain processes the full signal, with conventional Threshold, Ratio, Attack, Release and Knee. No multiband dynamics, spectral resynthesis or automatic upward gain.

## Controls

- Threshold: −60..0 dBFS, default −24, applied to the virtual RMS-calibrated level.
- Ratio: 1:1..20:1, default 4:1.
- Attack: 0.1..200 ms, default 20; Release: 10..1500 ms, default 200.
- Knee: 0..18 dB, default 6.
- Spectral: 0..100%, default 85%. Zero uses conventional RMS detection.
- Output: −12..+12 dB, default 0. Mix: 0..100%, default 100%, gain interpolation in dB.
- No Learn/profile: asymmetric smoothing estimates the current spectral baseline every 5 ms. Excess energy changes only the detector level.
- Activity Floor, in the foldout: −90..−30 dBFS, default −60. Controls detector validity, not an audio gate.

Dark / Neon / Amber themes, Inter font, designer dial images, settings/power/freeze/foldout icon drawing and Windows VBlank fix come from Duck Pocket. Layout, telemetry and processor are changed for this compressor. Renderer menu is retained on Windows. Optional OpenGL component painting defaults OFF; the original Duck oscilloscope GPU/phosphor code is not used in the new detector graphs. Freeze pauses displayed history and band bars; numeric meters remain live.

Separate plug-in identity: `ru.rainlinemusic.levelpocket`, manufacturer `RnLn`, plug-in code `LvPk`. It does not replace Duck Pocket or import Duck automation. Mono/stereo linked; no external sidechain bus in this prototype. Reported audio latency: 0 samples. The detector still has envelope inertia.

## Build

JUCE 8.0.4, CMake >=3.22, C++17. Put JUCE next to this folder, or pass `JUCE_DIR`.

```sh
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows: add `-A x64`; macOS universal: add `-G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`.

Portable DSP-only tests (no JUCE):

```sh
cmake -S . -B build-dsp -DLEVEL_DSP_ONLY=ON
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

`.github/workflows/build.yml` adapts the original macOS/Windows pipeline: VST3, developer AAX, pluginval, UI screenshot, optional Apple signing/notarization. Four main plugin artifacts. `.github/workflows/dsp.yml` runs DSP sanitizers on Linux.

AAX needs the SDK supplied by JUCE or an explicitly configured AAX SDK, and final PACE signing for distribution/ordinary Pro Tools. Apple codesign is not PACE signing. Reuse the original Apple secrets in a new GitHub repo if desired. No Actions workflow has been remotely triggered by this task.

## Documentation

- `docs/DETECTOR-MATH.md`: exact Russian specification, assumptions, coefficients, compressor curve, limitations and research links.
- `docs/VALIDATION.md`: actual checks performed for this delivery.

See `docs/DETECTOR-MATH.md` for published sources and the exact adaptation. Smooth spectral tilt may escape suppression; normal formants may also be reduced. The prototype is testable, not yet validated perceptually on a real vocal corpus. It can confuse vowel/register changes with changes of vocal effort. It does not measure standard loudness, guarantee unchanged peaks or replace a limiter.
