# Level Pocket 0.2.0

- Replaced frozen-profile Huber consensus with current-frame AsLS spectral baseline.
- Removed Learn/Profile Lock, long-term singer reference and persisted profile.
- Added 3 dB protrusion margin, 18 dB band cut cap and normalized retained-energy detector.
- Preserved conventional broadband downward gain, parameter IDs, themes, bypass, menu and workflows.
- SC meter shows detector correction; gray band bars show original energy, coloured bars retained analysis energy.
- Added amplitude scaling, smooth tilt, peak suppression and start-order/reset reproducibility checks.
- Updated primary-source research, exact equations and measured limitations.

# Level Pocket 0.1.0

- Separate JUCE product and unique VST3/AAX identity derived from Duck Pocket.
- New 15-band analysis-only detector, fixed normalized reference, Huber consensus, reliability and confidence weighting.
- Two cascaded analysis filters per band to reduce leakage of strong fundamentals into the body estimate.
- Standard broadband downward compressor with Threshold, Ratio, Knee, Attack, Release, Output and Mix.
- Spectral=0 conventional RMS detector, mono/stereo linked processing, zero sample audio latency.
- No auto makeup or upward ride.
- Voice profile learning, persistence, explicit relearn; heuristic sibilance protection and activity floor.
- Duck Pocket Inter fonts, knob artwork, theme palettes, icon drawing and Windows VBlank patch reused.
- New layout and gain/detector display; optional OpenGL component painting defaults OFF.
- Original macOS/Windows build, packaging, Apple signing/notary and pluginval infrastructure adapted; separate portable DSP sanitizer job.
- Portable deterministic DSP tests and JUCE state/bypass/UI smoke tests included.
