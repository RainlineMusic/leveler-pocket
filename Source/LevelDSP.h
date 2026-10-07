#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace level {
constexpr int bands = 15;
constexpr std::array<double, bands> centres{{90, 135, 200, 300, 450, 650, 950,
                                             1400, 2050, 3000, 4300, 6100, 8000,
                                             10500, 14000}};
inline double clamp(double x, double a, double b) {
  return std::max(a, std::min(b, x));
}
inline double db(double p) { return 10 * std::log10(std::max(1.e-14, p)); }
inline double smooth(double time, double rate) {
  return std::exp(-1 / (time * rate));
}
inline double finite(double x, double fallback = 0) {
  return std::isfinite(x) ? x : fallback;
}
struct Config {
  double threshold = -24, ratio = 4, spectral = .85, range = 60, attackMs = 20,
         releaseMs = 200, gate = -60, output = 0, mix = 1, knee = 6;
  bool bypass = false;
};
// RBJ constant-0dB-peak bandpass, Q=.9. Analysis only: never re-synthesise
// audio.
struct Bandpass {
  double b0 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void prepare(double f, double sr) {
    z1 = z2 = 0;
    const double w = 2 * 3.14159265358979323846 * f / sr,
                 alpha = std::sin(w) / (2 * .9), norm = 1 / (1 + alpha);
    b0 = alpha * norm;
    b2 = -b0;
    a1 = -2 * std::cos(w) * norm;
    a2 = (1 - alpha) * norm;
  }
  double tick(double x) {
    const double y = b0 * x + z1;
    z1 = -a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};
// Eilers & Boelens (2005), asymmetric penalised least squares.
// Our audio adaptation: current log power density on a log-frequency grid.
// No learned profile or long-term state. Fixed-size pentadiagonal Cholesky.
struct Estimate {
  double level = -140, correction = 0;
  std::array<double, bands> envelope{}, attenuation{};
};
inline Estimate estimate(const std::array<double, bands> &levels,
                         double broadband, double strength, double sr) {
  Estimate out;
  out.level = broadband;
  out.envelope = levels;
  int n = 0;
  while (n < bands && centres[n] < sr * .45)
    ++n;
  if (n < 3 || broadband < -100)
    return out;
  std::array<double, bands> y{}, z{}, w{}, rhs{}, solution{};
  std::array<std::array<double, bands>, bands> penalty{}, factor{};
  for (int i = 0; i < n; ++i) {
    y[i] = std::max(levels[i], broadband - 60) - 10 * std::log10(centres[i]);
    z[i] = y[i];
    w[i] = 1;
  }
  // Nonuniform divided differences; affine spectral tilt has zero penalty.
  const double spacing = std::log(centres[n - 1] / centres[0]) / (n - 1);
  for (int i = 0; i < n - 2; ++i) {
    const double h0 = std::log(centres[i + 1] / centres[i]) / spacing;
    const double h1 = std::log(centres[i + 2] / centres[i + 1]) / spacing;
    const double d[3] = {2 / (h0 * (h0 + h1)), -2 / (h0 * h1),
                         2 / (h1 * (h0 + h1))};
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k)
        penalty[i + j][i + k] += 120 * d[j] * d[k];
  }
  for (int iteration = 0; iteration < 8; ++iteration) {
    for (auto &row : factor)
      row.fill(0);
    for (int i = 0; i < n; ++i) {
      rhs[i] = w[i] * y[i];
      for (int j = std::max(0, i - 2); j <= i; ++j) {
        double v = penalty[i][j] + (i == j ? w[i] : 0);
        for (int k = std::max(0, i - 2); k < j; ++k)
          v -= factor[i][k] * factor[j][k];
        factor[i][j] =
            i == j ? std::sqrt(std::max(1e-12, v)) : v / factor[j][j];
      }
      double v = rhs[i];
      for (int j = std::max(0, i - 2); j < i; ++j)
        v -= factor[i][j] * solution[j];
      solution[i] = v / factor[i][i];
    }
    for (int i = n - 1; i >= 0; --i) {
      double v = solution[i];
      for (int j = i + 1; j < std::min(n, i + 3); ++j)
        v -= factor[j][i] * z[j];
      z[i] = v / factor[i][i];
    }
    for (int i = 0; i < n; ++i)
      w[i] = y[i] > z[i] ? .05 : .95;
  }
  double total = 0, retained = 0;
  for (int i = 0; i < n; ++i) {
    out.envelope[i] = z[i] + 10 * std::log10(centres[i]);
    out.attenuation[i] = clamp(levels[i] - out.envelope[i] - 3, 0, 18);
    // A ratio, not an assertion that overlapping filter energies are additive.
    const double energy = std::pow(10., (levels[i] - broadband) / 10);
    total += energy;
    retained += energy * std::pow(10., -out.attenuation[i] / 10);
  }
  out.correction = clamp(strength, 0, 1) *
                   clamp(db(retained / std::max(1e-20, total)), -18, 0);
  out.level = broadband + out.correction;
  return out;
}
// Standard soft-knee downward gain computer, output is always <= 0 dB.
inline double compressorGain(double input, double threshold, double ratio,
                             double knee, double range) {
  const double x = input - threshold, slope = 1 - 1 / ratio;
  double reduction = 0;
  if (knee <= 0)
    reduction = slope * std::max(0., x);
  else if (x >= knee * .5)
    reduction = slope * x;
  else if (x > -knee * .5)
    reduction = slope * (x + knee * .5) * (x + knee * .5) / (2 * knee);
  return -clamp(reduction, 0, range);
}
class Engine {
public:
  Config config;
  std::array<double, bands> levels{}, envelope{}, attenuation{};
  double broadband = -140, detector = -140, correction = 0, gainDb = 0,
         sibilance = 0;
  bool active = false;
  double rate = 48000;
  void prepare(double sr) {
    rate = clamp(finite(sr, 48000), 8000, 384000);
    for (int i = 0; i < bands; ++i)
      for (int ch = 0; ch < 2; ++ch)
        for (auto &section : filters[std::size_t(ch)][i])
          section.prepare(std::min(centres[std::size_t(i)], rate * .45), rate);
    reset();
  }
  // Clears only short filter/envelope and gain state.
  void reset() {
    for (auto &channel : filters)
      for (auto &band : channel)
        for (auto &f : band)
          f.z1 = f.z2 = 0;
    power.fill(0);
    broadPower = 0;
    gainDb = appliedDb = 0;
    frame = 0;
    quietTime = 0;
    desired = 0;
    active = false;
    levels.fill(-140);
    broadband = detector = -140;
    correction = 0;
    envelope.fill(-140);
    attenuation.fill(0);
    updateCoefficients();
  }
  void configure(Config c) {
    config = c;
    config.threshold = clamp(finite(c.threshold, -24), -60, 0);
    config.ratio = clamp(finite(c.ratio, 4), 1, 20);
    config.knee = clamp(finite(c.knee, 6), 0, 18);
    config.spectral = clamp(finite(c.spectral, .85), 0, 1);
    config.range = clamp(finite(c.range, 60), 0, 60);
    config.attackMs = clamp(finite(c.attackMs, 20), .1, 200);
    config.releaseMs = clamp(finite(c.releaseMs, 200), 10, 1500);
    config.gate = clamp(finite(c.gate, -60), -90, -30);
    config.output = clamp(finite(c.output), -12, 12);
    config.mix = clamp(finite(c.mix, 1), 0, 1);
    updateCoefficients();
  }
  std::array<float, 2> process(float left, float right) {
    const double l = finite(left), r = finite(right), p = (l * l + r * r) * .5;
    broadPower = env * broadPower + (1 - env) * p;
    for (int i = 0; i < bands; ++i) {
      const double a = filters[0][i][1].tick(filters[0][i][0].tick(l)),
                   b = filters[1][i][1].tick(filters[1][i][0].tick(r));
      power[std::size_t(i)] =
          env * power[std::size_t(i)] + (1 - env) * .5 * (a * a + b * b);
    }
    if (++frame >= hop) {
      frame = 0;
      analyse();
    }
    // Conventional downward compression: attack entering reduction, release
    // leaving it.
    const double a = desired < gainDb ? down : up;
    gainDb = a * gainDb + (1 - a) * desired;
    const double targetDb =
        config.bypass ? 0 : config.mix * gainDb + config.output;
    appliedDb = bypassSmooth * appliedDb + (1 - bypassSmooth) * targetDb;
    const double gain = std::pow(10., appliedDb / 20);
    return {float(l * gain), float(r * gain)};
  }
  double actualGainDb() const { return appliedDb; }

private:
  std::array<std::array<std::array<Bandpass, 2>, bands>, 2> filters{};
  std::array<double, bands> power{};
  double broadPower = 0, env = 0, down = 0, up = 0, bypassSmooth = 0,
         appliedDb = 0, quietTime = 0, desired = 0;
  int frame = 0, hop = 240;
  void updateCoefficients() {
    env = smooth(.025, rate);
    down = smooth(config.attackMs * .001, rate);
    up = smooth(config.releaseMs * .001, rate);
    bypassSmooth = smooth(.005, rate);
    hop = std::max(1, int(std::round(rate * .005)));
  }
  void analyse() {
    const double dt = double(hop) / rate;
    broadband = db(broadPower);
    for (int i = 0; i < bands; ++i)
      levels[std::size_t(i)] = db(power[std::size_t(i)]);
    double body = 0, high = 0;
    for (int i = 3; i <= 10; ++i)
      body += power[std::size_t(i)];
    for (int i = 11; i < bands; ++i)
      if (centres[std::size_t(i)] < rate * .45)
        high += power[std::size_t(i)];
    sibilance = clamp((db(high) - db(body) + 6) / 12, 0, 1);
    // Heuristic activity, deliberately not advertised as voiced/unvoiced
    // classification.
    active = broadband > config.gate;
    const auto e = estimate(levels, broadband, config.spectral, rate);
    envelope = e.envelope;
    attenuation = e.attenuation;
    correction = e.correction;
    detector = e.level;
    if (active) {
      quietTime = 0;
      desired = compressorGain(detector, config.threshold, config.ratio,
                               config.knee, config.range);
    } else {
      quietTime += dt;
      desired = 0; // release normally in silence; no upward compression
    }
  }
};
} // namespace level
