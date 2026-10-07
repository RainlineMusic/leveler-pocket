#include "PluginProcessor.h"
#include "PluginEditor.h"
LevelPocketAudioProcessor::LevelPocketAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "LEVEL_POCKET_STATE", layout()) {
  const char *ids[]{"threshold", "ratio", "spectral",   "knee", "attack",
                    "release",   "gate",  "outputGain", "mix",  "bypass"};
  for (int i = 0; i < 10; ++i)
    params[std::size_t(i)] =
        parameters.getRawParameterValue(ids[std::size_t(i)]);
  for (auto &l : bandLevels)
    l.store(-140);
  for (auto &r : adjustedLevels)
    r.store(-140);
}
juce::AudioProcessorValueTreeState::ParameterLayout
LevelPocketAudioProcessor::layout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
  auto add = [&](const char *id, const char *name, float lo, float hi,
                 float def, float skew = 1) {
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        id, name, juce::NormalisableRange<float>(lo, hi, .01f, skew), def));
  };
  add("threshold", "Threshold", -60, 0, -24);
  add("ratio", "Ratio", 1, 20, 4, .5f);
  add("spectral", "Spectral", 0, 100, 85);
  add("knee", "Knee", 0, 18, 6);
  add("attack", "Attack", .1f, 200, 20, .4f);
  add("release", "Release", 10, 1500, 200, .4f);
  add("gate", "Activity Floor", -90, -30, -60);
  add("outputGain", "Output", -12, 12, 0);
  add("mix", "Mix", 0, 100, 100);
  p.push_back(
      std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
  return {p.begin(), p.end()};
}
void LevelPocketAudioProcessor::prepareToPlay(double sr, int) {
  engine.prepare(sr);
  hop = juce::jmax(1, int(std::round(engine.rate * .005)));
  counter = 0;
  setLatencySamples(0);
}
bool LevelPocketAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &l) const {
  return (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
          l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()) &&
         l.getMainOutputChannelSet() == l.getMainInputChannelSet();
}
void LevelPocketAudioProcessor::processBlock(juce::AudioBuffer<float> &b,
                                             juce::MidiBuffer &) {
  process(b, false);
}
void LevelPocketAudioProcessor::process(juce::AudioBuffer<float> &b,
                                        bool hostBypass) {
  juce::ScopedNoDenormals denormals;
  if (resetRequested.exchange(false))
    engine.reset();
  level::Config c;
  c.threshold = params[0]->load();
  c.ratio = params[1]->load();
  c.spectral = params[2]->load() * .01;
  c.knee = params[3]->load();
  c.range = 60;
  c.attackMs = params[4]->load();
  c.releaseMs = params[5]->load();
  c.gate = params[6]->load();
  c.output = params[7]->load();
  c.mix = params[8]->load() * .01;
  c.bypass = hostBypass || params[9]->load() > .5f;
  engine.configure(c);
  displayBypass.store(c.bypass);
  if (b.getNumChannels() == 0)
    return;
  auto *l = b.getWritePointer(0);
  auto *r = b.getNumChannels() > 1 ? b.getWritePointer(1) : nullptr;
  for (int n = 0; n < b.getNumSamples(); ++n) {
    auto out = engine.process(l[n], r ? r[n] : l[n]);
    l[n] = out[0];
    if (r)
      r[n] = out[1];
    time += 1 / engine.rate;
    if (++counter >= hop) {
      counter = 0;
      for (int i = 0; i < level::bands; ++i) {
        bandLevels[std::size_t(i)].store(float(engine.levels[std::size_t(i)]),
                                         std::memory_order_relaxed);
        adjustedLevels[std::size_t(i)].store(
            float(engine.levels[i] - engine.attenuation[i] * c.spectral),
            std::memory_order_relaxed);
      }
      correction.store(float(engine.correction));
      detectorLevel.store(float(engine.detector));
      broadbandLevel.store(float(engine.broadband));
      gainMeter.store(float(engine.actualGainDb()));
      if (editorOpen.load(std::memory_order_relaxed)) {
        int a, s, z, t;
        fifo.prepareToWrite(1, a, s, z, t);
        if (s) {
          traces[size_t(a)] = {float(engine.actualGainDb()),
                               float(engine.broadband), float(engine.detector),
                               time};
          fifo.finishedWrite(1);
        }
      }
    }
  }
  for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels();
       ++ch)
    b.clear(ch, 0, b.getNumSamples());
}
bool LevelPocketAudioProcessor::popTrace(LevelTrace &t) {
  int a, s, b, n;
  fifo.prepareToRead(1, a, s, b, n);
  if (!s)
    return false;
  t = traces[size_t(a)];
  fifo.finishedRead(1);
  return true;
}
void LevelPocketAudioProcessor::getStateInformation(juce::MemoryBlock &b) {
  auto state = parameters.copyState();
  state.setProperty("uiWidth", editorWidth.load(), nullptr);
  state.setProperty("schema", 2, nullptr);
  state.removeProperty("profileReady", nullptr);
  for (int i = 0; i < level::bands; ++i)
    state.removeProperty("reference" + juce::String(i), nullptr);
  if (auto x = state.createXml())
    copyXmlToBinary(*x, b);
}
void LevelPocketAudioProcessor::setStateInformation(const void *data,
                                                    int size) {
  if (auto x = getXmlFromBinary(data, size))
    if (x->hasTagName(parameters.state.getType())) {
      auto state = juce::ValueTree::fromXml(*x);
      for (auto *p : getParameters())
        if (auto *ranged = dynamic_cast<juce::RangedAudioParameter *>(p)) {
          auto node =
              state.getChildWithProperty("id", ranged->getParameterID());
          if (node.isValid()) {
            const float v = float(node.getProperty("value"));
            const auto &range = ranged->getNormalisableRange();
            node.setProperty(
                "value",
                std::isfinite(v)
                    ? range.snapToLegalValue(
                          juce::jlimit(range.start, range.end, v))
                    : ranged->convertFrom0to1(ranged->getDefaultValue()),
                nullptr);
          }
        }
      editorWidth.store(
          juce::jlimit(500, 1400, int(state.getProperty("uiWidth", 800))));
      state.removeProperty("profileReady", nullptr);
      for (int i = 0; i < level::bands; ++i)
        state.removeProperty("reference" + juce::String(i), nullptr);
      resetRequested.store(true, std::memory_order_release);
      parameters.replaceState(state);
    }
}
juce::AudioProcessorEditor *LevelPocketAudioProcessor::createEditor() {
  return new LevelPocketAudioProcessorEditor(*this);
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new LevelPocketAudioProcessor();
}
