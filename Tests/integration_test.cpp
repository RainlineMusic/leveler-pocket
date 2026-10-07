#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <iostream>
void require(bool v, const char *msg) {
  if (!v) {
    std::cerr << msg << '\n';
    std::exit(1);
  }
}
struct LevelUiTestAccess {
  static void refresh(LevelPocketAudioProcessorEditor &e) { e.timerCallback(); }
  static void theme(LevelPocketAudioProcessorEditor &e, PocketTheme t) {
    e.setTheme(t);
  }
};
int main(int argc, char **argv) {
  juce::ScopedJuceInitialiser_GUI gui;
  LevelPocketAudioProcessor p;
  require(p.getLatencySamples() == 0, "zero latency");
  for (int channels : {1, 2}) {
    auto buses = p.getBusesLayout();
    buses.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono()
                                          : juce::AudioChannelSet::stereo());
    buses.outputBuses.set(0, buses.inputBuses[0]);
    require(p.setBusesLayout(buses), "mono/stereo layout");
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> b(channels, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 500; ++block) {
      for (int ch = 0; ch < channels; ++ch)
        for (int n = 0; n < 256; ++n)
          b.setSample(ch, n,
                      .2f * std::sin(float(2 * juce::MathConstants<double>::pi *
                                           440 * (block * 256 + n) / 48000)));
      p.processBlock(b, midi);
    }
    require(std::isfinite(p.detectorLevel.load()), "adaptive detector active");
    juce::MemoryBlock state;
    p.getStateInformation(state);
    LevelPocketAudioProcessor q;
    q.prepareToPlay(48000, 256);
    q.setStateInformation(state.getData(), int(state.getSize()));
    b.clear();
    q.processBlock(b, midi);
    require(q.parameters.getRawParameterValue("spectral")->load() ==
                p.parameters.getRawParameterValue("spectral")->load(),
            "parameter state restoration");
    for (int block = 0; block < 100; ++block) {
      for (int ch = 0; ch < channels; ++ch)
        for (int n = 0; n < 256; ++n)
          b.setSample(ch, n, .1f);
      q.processBlockBypassed(b, midi);
    }
    require(std::abs(b.getSample(0, 255) - .1f) < 1.e-5, "host bypass unity");
  }
  if (argc > 1) {
    auto folder =
        juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    folder.createDirectory();
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    auto &editor = static_cast<LevelPocketAudioProcessorEditor &>(*e);
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 800; ++block) {
      for (int n = 0; n < 256; ++n) {
        float x = .2f *
                  std::sin(float(2 * juce::MathConstants<double>::pi * 440 *
                                 (block * 256 + n) / 48000)) *
                  (.4f + .6f * std::pow(std::sin(block * .01f), 2.f));
        audio.setSample(0, n, x);
        audio.setSample(1, n, x);
      }
      p.processBlock(audio, midi);
      LevelUiTestAccess::refresh(editor);
    }
    for (auto t :
         {PocketTheme::SolidDark, PocketTheme::Neon, PocketTheme::Amber}) {
      LevelUiTestAccess::theme(editor, t);
      for (int width : {500, 800, 1200}) {
        e->setSize(width, int(width * .8));
        auto image = e->createComponentSnapshot(e->getLocalBounds(), true, 1.);
        const auto name = t == PocketTheme::SolidDark ? "Dark"
                          : t == PocketTheme::Neon    ? "Neon"
                                                      : "Amber";
        juce::FileOutputStream file(
            folder.getChildFile("LevelPocket-" + juce::String(name) + "-" +
                                juce::String(width) + ".png"));
        file.setPosition(0);
        file.truncate();
        juce::PNGImageFormat png;
        require(png.writeImageToStream(image, file), "UI PNG export");
      }
    }
    LevelUiTestAccess::theme(editor, PocketTheme::SolidDark);
  }
  std::cout
      << "PASS: mono/stereo, state roundtrip, host bypass, zero latency\n";
}
