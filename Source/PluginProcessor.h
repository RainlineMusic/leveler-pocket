#pragma once
#include "LevelDSP.h"
#include <JuceHeader.h>
struct LevelTrace {
  float gainDb = 0, broadband = -140, detector = -140;
  double time = 0;
};
class LevelPocketAudioProcessor final : public juce::AudioProcessor {
public:
  LevelPocketAudioProcessor();
  juce::AudioProcessorValueTreeState parameters;
  std::atomic<bool> editorOpen{false}, displayBypass{false},
      resetRequested{false};
  std::atomic<int> editorWidth{800};
  std::array<std::atomic<float>, level::bands> bandLevels{}, adjustedLevels{};
  std::atomic<float> correction{0}, detectorLevel{-140}, broadbandLevel{-140},
      gainMeter{0};
  bool popTrace(LevelTrace &);
  void prepareToPlay(double, int) override;
  void reset() override { engine.reset(); }
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout &) const override;
  using AudioProcessor::processBlock;
  using AudioProcessor::processBlockBypassed;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
  void processBlockBypassed(juce::AudioBuffer<float> &b,
                            juce::MidiBuffer &) override {
    process(b, true);
  }
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override { return true; }
  juce::AudioProcessorParameter *getBypassParameter() const override {
    return parameters.getParameter("bypass");
  }
  const juce::String getName() const override { return "Level Pocket"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String &) override {}
  void getStateInformation(juce::MemoryBlock &) override;
  void setStateInformation(const void *, int) override;

private:
  static juce::AudioProcessorValueTreeState::ParameterLayout layout();
  level::Engine engine;
  std::array<std::atomic<float> *, 10> params{};
  juce::AbstractFifo fifo{4096};
  std::array<LevelTrace, 4096> traces{};
  int counter = 0, hop = 240;
  double time = 0;
  void process(juce::AudioBuffer<float> &, bool);
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelPocketAudioProcessor)
};
