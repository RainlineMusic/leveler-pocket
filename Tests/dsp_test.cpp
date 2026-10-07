#include "../Source/LevelDSP.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
void check(bool value, const char *name) {
  if (!value) {
    std::cerr << "FAIL: " << name << '\n';
    std::exit(1);
  }
}
void near(double value, double expected, double eps, const char *name) {
  check(std::abs(value - expected) <= eps, name);
}
int main() {
  std::array<double, level::bands> l{};
  for (int i = 0; i < level::bands; ++i)
    l[i] = -40 - 10 * std::log10(level::centres[i] / 90.);
  auto e = level::estimate(l, -24, 1, 48000);
  near(e.level, -24, 1e-6, "smooth spectral tilt untouched");
  l[6] += 24;
  auto peak = level::estimate(l, -24, 1, 48000);
  check(peak.level < -27, "protruding peak reduces detector");
  for (auto &v : l)
    v += 6;
  auto shifted = level::estimate(l, -18, 1, 48000);
  near(shifted.level, peak.level + 6, 1e-7, "amplitude scaling equivariance");
  near(level::estimate(l, -18, 0, 48000).level, -18, 1e-9, "Spectral zero RMS");
  near(level::compressorGain(-12, -24, 4, 0, 60), -9, 1.e-9,
       "standard ratio 4:1");
  near(level::compressorGain(-24, -24, 4, 6, 60), -.5625, 1.e-9,
       "soft knee at threshold");
  near(level::compressorGain(0, -60, 1, 6, 60), 0, 1.e-9, "1:1 no compression");
  l.fill(-140);
  near(level::estimate(l, -140, 1, 48000).level, -140, 1e-9,
       "silence fallback");
  for (double sr : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    level::Engine engine;
    engine.prepare(sr);
    level::Config c;
    c.threshold = -30;
    c.ratio = 4;
    c.knee = 0;
    c.spectral = 0;
    c.range = 60;
    c.attackMs = 20;
    c.releaseMs = 150;
    engine.configure(c);
    for (int n = 0; n < int(sr * 2); ++n) {
      float x = float(.2 * std::sin(2 * 3.141592653589793 * 440 * n / sr));
      auto out = engine.process(x, -x);
      check(std::isfinite(out[0]) && std::isfinite(out[1]), "finite audio");
      near(out[0], -out[1], 1.e-6, "stereo linked no phase cancellation");
    }
    near(engine.gainDb, -.75 * (level::db(.02) + 30), .15,
         "steady conventional RMS compression");
    for (int n = 0; n < int(sr * 3); ++n) {
      engine.process(0, 0);
    }
    check(std::abs(engine.gainDb) < .001, "silence releases to unity");
    c.bypass = true;
    c.output = 6;
    engine.configure(c);
    for (int n = 0; n < int(sr * .2); ++n)
      engine.process(.1f, .1f);
    auto out = engine.process(.1f, .1f);
    near(out[0], .1, 1.e-6, "bypass excludes makeup");
  }
  // End-to-end harmonic voice model. Subtone has identical broadband RMS,
  // stronger first two harmonics and attenuated remaining harmonics.
  std::array<double, 384> normal{}, sub{};
  double pn = 0, ps = 0;
  for (int i = 0; i < 384; ++i) {
    for (int h = 1; h <= 60; ++h) {
      const double a = 1 / std::pow(h, 1.15),
                   phase = 2 * 3.141592653589793 * h * i / 384;
      normal[i] += a * std::sin(phase);
      sub[i] += a * (h <= 2 ? 2. : .3) * std::sin(phase);
    }
    pn += normal[i] * normal[i] / 384;
    ps += sub[i] * sub[i] / 384;
  }
  for (int i = 0; i < 384; ++i) {
    normal[i] *= .1 / std::sqrt(pn);
    sub[i] *= .1 / std::sqrt(ps);
  }
  level::Engine vocal;
  vocal.prepare(48000);
  level::Config vocalConfig;
  vocalConfig.threshold = -24;
  vocalConfig.spectral = 1;
  vocal.configure(vocalConfig);
  for (int i = 0; i < 48000 * 4; ++i) {
    vocal.process(float(normal[i % 384]), float(normal[i % 384]));
  }
  const double denseRms = vocal.broadband, denseDetector = vocal.detector,
               denseGain = vocal.gainDb;
  for (int i = 0; i < 48000 * 8; ++i) {
    vocal.process(float(sub[i % 384]), float(sub[i % 384]));
  }
  near(vocal.broadband, denseRms, .1, "end-to-end equal RMS");
  check(vocal.detector < denseDetector - .8, "end-to-end subtone detection");
  check(vocal.gainDb > denseGain + .5, "end-to-end less false compression");
  std::cout << "harmonic fixture: dense detector " << denseDetector
            << " / subtone " << vocal.detector << " / dense gain " << denseGain
            << " / subtone gain " << vocal.gainDb << '\n';
  level::Engine direct;
  direct.prepare(48000);
  direct.configure(vocalConfig);
  for (int i = 0; i < 48000 * 2; ++i)
    direct.process(float(sub[i % 384]), float(sub[i % 384]));
  near(direct.detector, vocal.detector, 1e-5,
       "start with subtone equals after dense");
  direct.reset();
  for (int i = 0; i < 48000 * 2; ++i)
    direct.process(float(normal[i % 384]), float(normal[i % 384]));
  near(direct.detector, denseDetector, 1e-5,
       "reset/middle playback reproducible");
  level::Engine engine;
  engine.prepare(48000);
  auto out = engine.process(std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity());
  check(std::isfinite(out[0]) && std::isfinite(out[1]), "bad audio sanitised");
  std::cout << "PASS: adaptive envelope, compressor curve, stereo, silence, "
               "transport, "
               "bypass, six rates\n";
}
