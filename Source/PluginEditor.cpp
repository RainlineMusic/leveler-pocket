#include "PluginEditor.h"
namespace {
juce::Font uiFont(float size) { return pocketFont(size); }
void text(juce::Graphics &g, const juce::String &s, juce::Rectangle<float> r,
          float size, juce::Colour c,
          int align = juce::Justification::centredLeft, float glow = 0.f) {
  g.setFont(uiFont(juce::jmax(13.2f, size)));
  if (glow > 0.f) {
    g.setColour(c.withAlpha(glow));
    const float o[8][2] = {{-1, 0},  {1, 0}, {0, -1}, {0, 1},
                           {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
    for (auto &d : o)
      g.drawText(s, r.translated(d[0], d[1]), align);
  }
  g.setColour(c);
  g.drawText(s, r, align);
}
void stroke(juce::Graphics &g, const juce::Path &p, juce::Colour c,
            float width) {
  g.setColour(c);
  g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded));
}

} // namespace
juce::Colour PocketLook::pick(juce::uint32 neon, juce::uint32 dark,
                              juce::uint32 white) const {
  if (theme == PocketTheme::Neon)
    return juce::Colour(neon);
  if (theme == PocketTheme::SolidDark)
    return juce::Colour(dark);
  if (theme == PocketTheme::SolidWhite)
    return juce::Colour(white);
  // Amber: preserve the semantic brightness/alpha of the dark palette while
  // moving it onto the warm brown/orange ramp from the reference UI.
  const auto source = juce::Colour(dark ? dark : neon);
  const float b = source.getPerceivedBrightness();
  const float sat = juce::jmap(b, 0.f, 1.f, .72f, .20f);
  const float value = juce::jlimit(.025f, 1.f, b * .93f + .018f);
  return juce::Colour::fromHSV(.078f, sat, value, source.getFloatAlpha());
}
juce::Colour PocketLook::ink() const { return tokens().ink; }
juce::Colour PocketLook::muted() const { return tokens().muted; }
juce::Colour PocketLook::accent() const {
  return theme == PocketTheme::Amber ? juce::Colour(0xffff7126)
                                     : pick(0xff35d6dc, 0xffe8e8e8, 0xff2f74d0);
}
juce::Colour PocketLook::accent2() const {
  return theme == PocketTheme::Amber
             ? juce::Colour(0xffffd164)
             : (isNeon() ? juce::Colour(0xff35d6dc) : accent());
}
juce::Colour PocketLook::themedAccent(juce::uint32 neon) const {
  return isAmber()
             ? (juce::Colour(neon).getHue() > .3f ? juce::Colour(0xffffd164)
                                                  : juce::Colour(0xffff7126))
             : juce::Colour(neon);
}
juce::Font PocketLook::getTextButtonFont(juce::TextButton &, int) {
  return uiFont(15);
}
void PocketLook::drawButtonBackground(juce::Graphics &g, juce::Button &button,
                                      const juce::Colour &, bool hover,
                                      bool down) {
  if (button.getButtonText() == "expand")
    return;
  const auto t = tokens();
  auto r = button.getLocalBounds().toFloat().reduced(1);
  if (button.getButtonText() == "freeze")
    r = r.withSizeKeepingCentre(r.getWidth() * 20.2746f / 32.f,
                                r.getHeight() * 20.2746f / 32.f);
  g.setColour(hover ? t.raised.brighter(.12f) : t.raised);
  g.fillRoundedRectangle(r, 2.5f);
  g.setColour(down ? t.ink : t.out.withAlpha(.75f));
  g.drawRoundedRectangle(r, 2.5f, 1.2f);
}
void PocketLook::drawButtonText(juce::Graphics &g, juce::TextButton &b, bool,
                                bool) {
  auto r = b.getLocalBounds().toFloat();
  auto name = b.getButtonText();
  if (name == "freeze")
    r = r.withSizeKeepingCentre(r.getWidth() * 20.2746f / 32.f,
                                r.getHeight() * 20.2746f / 32.f);
  const auto controlInk = isDark() ? tokens().out : ink();
  auto c = r.getCentre();
  float s = juce::jmin(r.getWidth(), r.getHeight()) / 40;
  juce::Path p;
  if (name == "expand") {
    const float direction = b.getToggleState() ? -1.f : 1.f;
    p.startNewSubPath(c.x - 10 * s, c.y - 3 * s * direction);
    p.lineTo(c.x, c.y + 3 * s * direction);
    p.lineTo(c.x + 10 * s, c.y - 3 * s * direction);
    juce::Path outline;
    juce::PathStrokeType(1.7f * s).createStrokedPath(outline, p);
    juce::DropShadow(ink().withAlpha(.30f), 7, {0, 0}).drawForPath(g, outline);
    juce::DropShadow(ink().withAlpha(.55f), 3, {0, 0}).drawForPath(g, outline);
    stroke(g, p, ink().brighter(.2f), 1.7f * s);
    return;
  }
  if (name == "power") {
    p.addCentredArc(0, 1, 8, 8, 0, .65f,
                    juce::MathConstants<float>::twoPi - .65f, true);
    p.startNewSubPath(0, -10);
    p.lineTo(0, -1);
    p.applyTransform(juce::AffineTransform::scale(s).translated(c.x, c.y));
    stroke(g, p, controlInk, 1.7f * s);
    return;
  }

  if (name == "freeze") {
    // small snowflake: three crossed arms with barbs, lit up while frozen
    for (int arm = 0; arm < 3; ++arm) {
      const float a = juce::MathConstants<float>::halfPi +
                      float(arm) * juce::MathConstants<float>::pi / 3.f;
      const float dx = std::cos(a), dy = std::sin(a);
      p.startNewSubPath(-dx * 10, -dy * 10);
      p.lineTo(dx * 10, dy * 10);
      for (float sign : {-1.f, 1.f})
        for (float at : {5.5f, 9.f}) {
          const float bx = dx * at * sign, by = dy * at * sign;
          for (float spread : {.62f, -.62f}) {
            const float ax = std::cos(a + spread), ay = std::sin(a + spread);
            p.startNewSubPath(bx, by);
            p.lineTo(bx + ax * 3.2f * sign, by + ay * 3.2f * sign);
          }
        }
    }
    p.applyTransform(juce::AffineTransform::scale(s).translated(c.x, c.y));
    stroke(g, p,
           (b.getToggleState() ? accent() : controlInk)
               .withAlpha(b.isEnabled() ? 1.f : .35f),
           1.35f * s);
    return;
  }
  if (name == "Learn") {
    g.setColour(ink());
    g.setFont(pocketFont(13));
    g.drawText("Learn", r, juce::Justification::centred);
    return;
  }
  constexpr int teeth = 10;
  for (int i = 0; i < teeth * 4; ++i) {
    float a = float(i) * juce::MathConstants<float>::twoPi / float(teeth * 4) -
              juce::MathConstants<float>::halfPi;
    float radius = (i % 4 == 1 || i % 4 == 2) ? 10.f : 7.7f;
    auto pt = juce::Point<float>(std::cos(a) * radius, std::sin(a) * radius);
    if (i == 0)
      p.startNewSubPath(pt);
    else
      p.lineTo(pt);
  }
  p.closeSubPath();
  p.applyTransform(juce::AffineTransform::scale(s).translated(c.x, c.y));
  stroke(g, p, controlInk, 1.65f * s);
  g.setColour(controlInk);
  g.drawEllipse(c.x - 3.2f * s, c.y - 3.2f * s, 6.4f * s, 6.4f * s, 1.65f * s);
}
void PocketLook::drawLinearSlider(juce::Graphics &g, int x, int y, int w, int h,
                                  float pos, float minPos, float maxPos,
                                  juce::Slider::SliderStyle style,
                                  juce::Slider &slider) {
  auto t = tokens();
  const float scale = float(slider.getWidth()) / 670.f;
  const float cy = float(y) + float(h) * .5f,
              left =
                  style == juce::Slider::TwoValueHorizontal ? minPos : float(x),
              right = style == juce::Slider::TwoValueHorizontal ? maxPos : pos;
  const auto light = theme == PocketTheme::SolidDark
                         ? juce::Colour(0xffe2f5f8)
                         : (slider.getName() == "key" ? t.key : t.out);
  g.setColour(t.glass.darker(.6f));
  g.fillRoundedRectangle(float(x), cy - 4 * scale, float(w), 8 * scale,
                         3 * scale);
  g.setColour(t.border.withAlpha(.6f));
  g.drawLine(float(x), cy + 4 * scale, float(x + w), cy + 4 * scale,
             .6f * scale);
  g.setColour(light.withAlpha(.18f));
  g.fillRoundedRectangle(left, cy - 6 * scale, juce::jmax(.1f, right - left),
                         12 * scale, 4 * scale);
  g.setColour(light);
  g.fillRect(left, cy - 2.5f * scale, juce::jmax(.1f, right - left), 5 * scale);
  auto thumb = [&](float px) {
    auto r =
        juce::Rectangle<float>(13 * scale, 13 * scale).withCentre({px, cy});
    g.setColour(juce::Colours::black.withAlpha(.45f));
    g.fillRoundedRectangle(r.translated(scale, 2 * scale), 2 * scale);
    g.setGradientFill(juce::ColourGradient(
        t.out.interpolatedWith(t.raised, .6f), r.getX(), r.getY(), t.glass,
        r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 2 * scale);
    g.setColour(t.ink.withAlpha(.25f));
    g.drawRoundedRectangle(r, 2 * scale, .65f * scale);
  };
  if (style == juce::Slider::TwoValueHorizontal) {
    thumb(minPos);
    thumb(maxPos);
  } else
    thumb(pos);
}
ModernDial::ModernDial(PocketLook &l, juce::String t, juce::String sub,
                       juce::String u, juce::uint32 a, bool inf, bool infMin,
                       bool compactDial, juce::String infLabel)
    : look(l), title(t), subtitle(sub), unit(u), infinity(inf),
      compact(compactDial) {
  juce::ignoreUnused(a, infMin, infLabel);
  setSliderStyle(juce::Slider::RotaryVerticalDrag);
  setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  setName(t);
  setWantsKeyboardFocus(true);
}
void ModernDial::paint(juce::Graphics &g) {
  const auto t = look.tokens();
  const float designWidth = compact ? 132.f : 240.f,
              designHeight = compact ? 135.f : 250.f;
  const float factor = float(getWidth()) / designWidth;
  const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
  const int pw = juce::jmax(1, juce::roundToInt(getWidth() * scale)),
            ph = juce::jmax(1, juce::roundToInt(getHeight() * scale));
  const juce::Point<float> centre = compact
                                        ? juce::Point<float>{66.26f, 75.40f}
                                        : juce::Point<float>{119.27f, 135.27f};
  const float radius = compact ? 49.68f : 97.5f,
              faceRadius = compact ? 31.34f : 60.f;
  if (!body.isValid() || body.getWidth() != pw || body.getHeight() != ph ||
      bodyTheme != look.theme || std::abs(bodyScale - scale) > .001f) {
    body =
        juce::Image(juce::Image::ARGB, pw, ph, true, juce::SoftwareImageType());
    bodyTheme = look.theme;
    bodyScale = scale;
    ringValue = std::numeric_limits<double>::quiet_NaN();
    juce::Graphics bg(body);
    bg.addTransform(juce::AffineTransform::scale(scale * factor));
    if (look.theme == PocketTheme::SolidDark) {
      static const auto large = juce::ImageFileFormat::loadFrom(
          BinaryData::DialLarge_png, BinaryData::DialLarge_pngSize);
      static const auto small = juce::ImageFileFormat::loadFrom(
          BinaryData::DialSmall_png, BinaryData::DialSmall_pngSize);
      bg.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
      bg.drawImage(compact ? small : large, {0, 0, designWidth, designHeight},
                   juce::RectanglePlacement::stretchToFit);
      // Cool, deeper material grade is cached with the SVG-derived body.
      bg.setGradientFill(juce::ColourGradient(
          juce::Colour(0xff142837).withAlpha(.16f), centre.x - radius,
          centre.y - radius, juce::Colour(0xff020b13).withAlpha(.40f),
          centre.x + radius, centre.y + radius, false));
      bg.fillEllipse(centre.x - radius, centre.y - radius, 2 * radius,
                     2 * radius);
    } else {
      auto rim =
          juce::Rectangle<float>(2 * radius, 2 * radius).withCentre(centre);
      bg.setGradientFill(juce::ColourGradient(
          t.raised.brighter(.2f), centre.x - radius, centre.y - radius,
          t.glass.darker(.15f), centre.x + radius, centre.y + radius, false));
      bg.fillEllipse(rim);
      bg.setColour(t.glass.darker(.5f));
      bg.drawEllipse(rim, compact ? 5.f : 6.f);
      auto face = juce::Rectangle<float>(2 * faceRadius, 2 * faceRadius)
                      .withCentre(centre);
      bg.setGradientFill(juce::ColourGradient(
          t.glass, centre.x - faceRadius, centre.y - faceRadius, t.raised,
          centre.x + faceRadius, centre.y + faceRadius, false));
      bg.fillEllipse(face);
      bg.setColour(t.border);
      bg.drawEllipse(face, 1.5f);
    }
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(pocketFont(compact ? 14.f : 21.f), title, 0, 0);
    const float width = glyphs.getBoundingBox(0, -1, true).getWidth(),
                r = radius + 11.f;
    bg.setColour(t.ink);
    for (int i = 0; i < glyphs.getNumGlyphs(); ++i) {
      const auto &glyph = glyphs.getGlyph(i);
      juce::Path path;
      glyph.createPath(path);
      const float x = glyph.getBounds().getCentreX(),
                  angle = (x - width * .5f) / r;
      bg.fillPath(
          path,
          juce::AffineTransform::translation(-x, 0).rotated(angle).translated(
              centre.x + std::sin(angle) * r, centre.y - std::cos(angle) * r));
    }
  }
  g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
  g.drawImageTransformed(body, juce::AffineTransform::scale(1.f / bodyScale));
  // Cache the blurred ring independently; audio-driven metering never rebuilds
  // it.
  const float proportion = float(valueToProportionOfLength(getValue()));
  if (!ringImage.isValid() || ringValue != double(proportion) ||
      ringImage.getWidth() != pw || ringImage.getHeight() != ph) {
    ringValue = double(proportion);
    ringImage =
        juce::Image(juce::Image::ARGB, pw, ph, true, juce::SoftwareImageType());
    juce::Graphics rg(ringImage);
    rg.addTransform(juce::AffineTransform::scale(scale * factor));
    const float start = juce::MathConstants<float>::pi,
                end = start + juce::MathConstants<float>::twoPi * proportion;
    const auto colour =
        look.theme == PocketTheme::SolidDark
            ? juce::Colour(0xfff5f8ff)
            : (title == "Influence" || title == "Output" ? t.out : t.neutral);
    juce::Path arc;
    if (proportion >= .99999f)
      arc.addEllipse(centre.x - radius, centre.y - radius, 2 * radius,
                     2 * radius);
    else if (proportion > 0)
      arc.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end,
                        true);
    // Rasterised once per value/size/theme, no idle glow animation.
    if (!arc.isEmpty()) {
      juce::Path outline;
      juce::PathStrokeType(compact ? 4.2f : 4.6f, juce::PathStrokeType::curved,
                           juce::PathStrokeType::rounded)
          .createStrokedPath(outline, arc);
      juce::DropShadow(colour.withAlpha(.5f), 14, {0, 0})
          .drawForPath(rg, outline);
      juce::DropShadow(colour.withAlpha(.9f), 5, {0, 0})
          .drawForPath(rg, outline);
      stroke(rg, arc, colour, compact ? 4.2f : 4.6f);
    }
    const float inner = faceRadius + 8.f,
                outer = inner + (compact ? 9.f : 16.f);
    rg.setColour(t.ink.withAlpha(.75f));
    rg.drawLine(centre.x + inner * std::sin(end),
                centre.y - inner * std::cos(end),
                centre.x + outer * std::sin(end),
                centre.y - outer * std::cos(end), 1.2f);
  }
  g.drawImageTransformed(ringImage,
                         juce::AffineTransform::scale(1.f / bodyScale));
  juce::Graphics::ScopedSaveState save(g);
  g.addTransform(juce::AffineTransform::scale(factor));
  if (emphasis > .001f) {
    g.setColour(t.ink.withAlpha(emphasis * .10f));
    g.drawEllipse(centre.x - faceRadius, centre.y - faceRadius, 2 * faceRadius,
                  2 * faceRadius, 1.f);
  }
  const auto value = displayedValue();
  g.setFont(pocketFont(valueTextHeight(value) / factor));
  g.setColour(t.ink);
  g.drawText(value,
             juce::Rectangle<float>{centre.x - faceRadius,
                                    centre.y - (compact ? 15.f : 19.f),
                                    2 * faceRadius, compact ? 25.f : 35.f},
             juce::Justification::centred);
  const auto detail =
      unit == "balance"
          ? balanceLabel()
          : (unit == "dB" ? juce::String("dB")
                          : (isAutoValue() ? juce::String("AUTO") : subtitle));
  g.setFont(pocketFont(compact ? 11.f : 13.f));
  g.setColour(t.muted);
  g.drawText(detail,
             juce::Rectangle<float>{centre.x - faceRadius,
                                    centre.y + (compact ? 8.f : 17.f),
                                    2 * faceRadius, 20.f},
             juce::Justification::centred);
}

LevelPocketAudioProcessorEditor::LevelPocketAudioProcessorEditor(
    LevelPocketAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p) {
  juce::PropertiesFile::Options o;
  o.applicationName = "LevelPocket";
  o.filenameSuffix = "settings";
  o.folderName = "RainlineMusic";
  o.osxLibrarySubFolder = "Application Support";
  preferences = std::make_unique<juce::PropertiesFile>(o);
  setLookAndFeel(&look);
  setOpaque(true);
  setResizable(true, true);
  const auto saved = preferences->getValue("theme", "dark");
  setTheme(saved == "amber"  ? PocketTheme::Amber
           : saved == "neon" ? PocketTheme::Neon
                             : PocketTheme::SolidDark);
  window = level::clamp(preferences->getDoubleValue("graphWindow", 5), 1, 10);
  auto attach = [&](juce::Slider &s, const char *id) {
    addAndMakeVisible(s);
    attachments.push_back(std::make_unique<Attachment>(p.parameters, id, s));
    s.setDoubleClickReturnValue(
        true, p.parameters.getParameter(id)->convertFrom0to1(
                  p.parameters.getParameter(id)->getDefaultValue()));
  };
  attach(target, "threshold");
  attach(amount, "ratio");
  attach(spectral, "spectral");
  attach(range, "knee");
  attach(attack, "attack");
  attach(release, "release");
  attach(gate, "gate");
  attach(output, "outputGain");
  attach(mix, "mix");
  for (auto *b : {&settings, &bypass, &freeze, &expand})
    addAndMakeVisible(b);
  bypassAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.parameters, "bypass", bypass);
  bypass.setClickingTogglesState(true);
  freeze.setClickingTogglesState(true);
  expand.setClickingTogglesState(true);
  settings.onClick = [this] { showMenu(); };
  freeze.onClick = [this] { frozen = freeze.getToggleState(); };
  expand.onClick = [this] { setExpanded(expand.getToggleState()); };
  target.setTooltip("Threshold applied to the virtual detector level in "
                    "RMS-calibrated dBFS.");
  amount.setTooltip(
      "Conventional downward compression ratio. No automatic upward gain.");
  spectral.setTooltip("Reduce spectral protrusions in the detector only. "
                      "0% is conventional level detection.");
  range.setTooltip("Soft knee width in dB. Zero gives a hard knee.");
  gate.setTooltip("Activity floor: below it compression releases normally. "
                  "This is not a noise gate.");
  output.setTooltip("Output trim; double-click resets to 0 dB.");
  mix.setTooltip(
      "Dry/wet gain blend in dB. Output trim remains active at 0% Mix.");
  displayedBands.fill(-140);
  displayedAdjusted.fill(-140);
  setResizeLimits(500, 400, 1400, 1120);
  getConstrainer()->setFixedAspectRatio(800. / 640.);
  setSize(juce::jlimit(500, 1400, p.editorWidth.load()),
          juce::roundToInt(juce::jlimit(500, 1400, p.editorWidth.load()) * .8));
#if LEVEL_ENABLE_OPENGL
  setOpenGL(preferences->getBoolValue("openGL", false));
#endif
  processor.editorOpen.store(true);
  startTimerHz(30);
}
LevelPocketAudioProcessorEditor::~LevelPocketAudioProcessorEditor() {
  stopTimer();
  processor.editorOpen.store(false);
#if LEVEL_ENABLE_OPENGL
  context.detach();
#endif
  setLookAndFeel(nullptr);
}
juce::Rectangle<int> LevelPocketAudioProcessorEditor::scaled(float x, float y,
                                                             float w,
                                                             float h) const {
  const float s = float(getWidth()) / 800;
  return juce::Rectangle<float>(x * s, y * s, w * s, h * s).toNearestInt();
}
void LevelPocketAudioProcessorEditor::resized() {
  processor.editorWidth.store(getWidth());
  chromeValid = false;
  target.setBounds(scaled(34, 77, 240, 250));
  amount.setBounds(scaled(524, 77, 240, 250));
  spectral.setBounds(scaled(268, 78, 132, 135));
  range.setBounds(scaled(402, 78, 132, 135));
  attack.setBounds(scaled(268, 219, 132, 135));
  release.setBounds(scaled(402, 219, 132, 135));
  settings.setBounds(scaled(718, 12, 28, 28));
  bypass.setBounds(scaled(759, 12, 28, 28));
  freeze.setBounds(scaled(735, 352, 32, 32));
  mix.setBounds(scaled(534, 16, 62, 22));
  output.setBounds(scaled(638, 16, 63, 22));
  expand.setBounds(scaled(369, 601, 62, 33));
  gate.setVisible(expanded);
  gate.setBounds(scaled(330, 656, 140, 140));
}
void LevelPocketAudioProcessorEditor::setTheme(PocketTheme t) {
  look.theme = t;
  chromeValid = false;
  preferences->setValue("theme", t == PocketTheme::Amber  ? "amber"
                                 : t == PocketTheme::Neon ? "neon"
                                                          : "dark");
  target.repaint();
  amount.repaint();
  spectral.repaint();
  range.repaint();
  attack.repaint();
  release.repaint();
  gate.repaint();
  repaint();
}
void LevelPocketAudioProcessorEditor::setExpanded(bool e) {
  expanded = e;
  auto *c = getConstrainer();
  c->setSizeLimits(500, juce::roundToInt(designHeight() * .625f), 1400,
                   juce::roundToInt(designHeight() * 1.75f));
  c->setFixedAspectRatio(800. / designHeight());
  setSize(getWidth(), juce::roundToInt(getWidth() * designHeight() / 800));
  resized();
  repaint();
}
#if LEVEL_ENABLE_OPENGL
void LevelPocketAudioProcessorEditor::setOpenGL(bool enabled) {
  if (openGL == enabled)
    return;
  context.detach();
  openGL = enabled;
  if (enabled) {
    context.setComponentPaintingEnabled(true);
    context.setContinuousRepainting(false);
    context.attachTo(*this);
  }
  preferences->setValue("openGL", enabled);
  repaint();
}
#endif
void LevelPocketAudioProcessorEditor::parentHierarchyChanged() {
#if JUCE_WINDOWS
  if (auto *peer = getPeer()) {
    auto names = peer->getAvailableRenderingEngines();
    const auto saved = preferences->getValue("renderer");
    for (int i = 0; i < names.size(); ++i)
      if (names[std::size_t(i)] == saved)
        peer->setCurrentRenderingEngine(i);
  }
#endif
}
void LevelPocketAudioProcessorEditor::showMenu() {
  juce::PopupMenu root, themes, times;
  themes.addItem(1, "Dark", true, look.theme == PocketTheme::SolidDark);
  themes.addItem(2, "Neon", true, look.theme == PocketTheme::Neon);
  themes.addItem(3, "Amber", true, look.theme == PocketTheme::Amber);
  root.addSubMenu("Theme", themes);
  for (int i = 0; i < 3; ++i) {
    const int seconds[]{1, 5, 10};
    times.addItem(10 + i, juce::String(seconds[std::size_t(i)]) + " s", true,
                  window == seconds[std::size_t(i)]);
  }
  root.addSubMenu("Window time", times);
#if LEVEL_ENABLE_OPENGL
  root.addItem(20, "OpenGL", true, openGL);
#endif
#if JUCE_WINDOWS
  juce::StringArray renderers;
  if (auto *peer = getPeer()) {
    renderers = peer->getAvailableRenderingEngines();
    juce::PopupMenu r;
    for (int i = 0; i < renderers.size(); ++i)
      r.addItem(100 + i, renderers[std::size_t(i)], true,
                i == peer->getCurrentRenderingEngine());
    root.addSubMenu("Render type", r);
  }
#else
  juce::StringArray renderers;
#endif
  root.addSeparator();
  auto safe =
      juce::Component::SafePointer<LevelPocketAudioProcessorEditor>(this);
  root.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(settings),
      [safe, renderers](int id) {
        if (!safe)
          return;
        if (id >= 1 && id <= 3)
          safe->setTheme(id == 1   ? PocketTheme::SolidDark
                         : id == 2 ? PocketTheme::Neon
                                   : PocketTheme::Amber);
        if (id >= 10 && id <= 12) {
          const int seconds[]{1, 5, 10};
          safe->window = seconds[id - 10];
          safe->preferences->setValue("graphWindow", safe->window);
          safe->chromeValid = false;
        }
#if LEVEL_ENABLE_OPENGL
        if (id == 20)
          safe->setOpenGL(!safe->openGL);
#endif
#if JUCE_WINDOWS
        if (id >= 100 && id < 100 + renderers.size())
          if (auto *peer = safe->getPeer()) {
            peer->setCurrentRenderingEngine(id - 100);
            safe->preferences->setValue("renderer", renderers[id - 100]);
          }
#endif
        safe->repaint();
      });
}
void LevelPocketAudioProcessorEditor::timerCallback() {
  LevelTrace t;
  while (processor.popTrace(t)) {
    if (frozen)
      continue;
    history[size_t(cursor)] = t;
    cursor = (cursor + 1) % int(history.size());
    count = std::min(count + 1, int(history.size()));
  }
  if (!frozen)
    for (int i = 0; i < level::bands; ++i) {
      displayedBands[i] = processor.bandLevels[i].load();
      displayedAdjusted[i] = processor.adjustedLevels[i].load();
    }
  const bool bypassed =
      processor.displayBypass.load() ||
      processor.parameters.getRawParameterValue("bypass")->load() > .5f;
  if (bypassed != lastBypass) {
    lastBypass = bypassed;
    repaint();
  }
  repaint(scaled(0, 345, 800, 255));
  repaint(scaled(0, 0, 800, 50));
}
void LevelPocketAudioProcessorEditor::paintChrome() {
  chrome = juce::Image(juce::Image::RGB, getWidth(), getHeight(), false);
  juce::Graphics g(chrome);
  g.addTransform(juce::AffineTransform::scale(float(getWidth()) / 800));
  const auto t = look.tokens();
  g.setGradientFill(juce::ColourGradient(
      t.chassis, 400, 220, t.chassis.darker(.2f), 0, designHeight(), true));
  g.fillRect(0.f, 0.f, 800.f, designHeight());
  text(g, "LEVEL POCKET", {24, 8, 264, 35}, 23, t.brand);
  text(g, "mix", {498, 16, 32, 22}, 12, t.ink);
  text(g, "output", {598, 16, 38, 22}, 12, t.ink);
  for (float y : {53.f, 344.f, 392.f, 508.f, 598.f}) {
    g.setColour(juce::Colours::black.withAlpha(.5f));
    g.fillRect(0.f, y, 800.f, 3.f);
    g.setColour(t.ink.withAlpha(.05f));
    g.drawLine(0, y + 3, 800, y + 3, .7f);
  }
  for (auto r : {juce::Rectangle<float>(0, 398, 800, 106),
                 juce::Rectangle<float>(0, 514, 800, 80)}) {
    g.setGradientFill(juce::ColourGradient(
        t.glass, 0, r.getY(), t.glass.darker(.15f), 0, r.getBottom(), false));
    g.fillRect(r);
  }
  text(g, "GAIN REDUCTION", {32, 400, 160, 20}, 12, t.ink);
  text(g, "15-BAND DETECTOR", {32, 516, 240, 18}, 12, t.ink);
  for (int i = 0; i <= 2; ++i) {
    float y = 431 + float(i) * 29;
    g.setColour(t.major.withAlpha(.45f));
    g.drawLine(36, y, 733, y, .7f);
    text(g,
         i == 0   ? "0"
         : i == 1 ? "-18"
                  : "-36",
         {738, y - 8, 40, 16}, 11, t.muted);
  }
  chromeValid = true;
}
void LevelPocketAudioProcessorEditor::paint(juce::Graphics &g) {
  if (!chromeValid)
    paintChrome();
  g.drawImageAt(chrome, 0, 0);
  juce::Graphics::ScopedSaveState s(g);
  g.addTransform(juce::AffineTransform::scale(float(getWidth()) / 800));
  const auto t = look.tokens();
  text(g, "ADAPTIVE DETECTOR", {36, 353, 310, 28}, 12, t.key);
  text(g, "GAIN  " + juce::String(processor.gainMeter.load(), 1) + " dB",
       {367, 353, 210, 28}, 14, t.out);
  text(g, "SC  " + juce::String(processor.correction.load(), 1) + " dB",
       {590, 353, 160, 28}, 12, t.muted);
  text(g,
       "RMS " + juce::String(processor.broadbandLevel.load(), 1) + " / DET " +
           juce::String(processor.detectorLevel.load(), 1),
       {445, 400, 285, 20}, 12, t.muted, juce::Justification::centredRight);
  if (count > 1) {
    const double now = history[size_t((cursor - 1 + int(history.size())) %
                                      int(history.size()))]
                           .time;
    juce::Path path;
    bool first = true;
    for (int i = 0; i < count; ++i) {
      const auto &v = history[size_t(
          (cursor - count + i + int(history.size())) % int(history.size()))];
      const double age = now - v.time;
      if (age > window)
        continue;
      const float x = 36 + float(1 - age / window) * 697,
                  y = 431 - level::clamp(v.gainDb, -36, 0) * 58 / 36;
      if (first) {
        path.startNewSubPath(x, y);
        first = false;
      } else
        path.lineTo(x, y);
    }
    g.setGradientFill(juce::ColourGradient(t.out.withAlpha(.12f), 36, 460,
                                           t.out, 733, 460, false));
    g.strokePath(path, juce::PathStrokeType(1.6f));
  }
  for (int i = 0; i < level::bands; ++i) {
    float x = 36 + float(i) * 46.5f,
          h = float(level::clamp((displayedBands[std::size_t(i)] + 90) / 90, 0,
                                 1)) *
              44;
    g.setColour(t.muted.withAlpha(.4f));
    g.fillRoundedRectangle(x, 582 - h, 28, h, 2);
    const float retained =
        float(level::clamp((displayedAdjusted[i] + 90) / 90, 0, 1)) * 44;
    g.setColour(t.key.withAlpha(.85f));
    g.fillRoundedRectangle(x, 582 - retained, 28, retained, 2);
  }
  text(g, "90 Hz", {36, 580, 90, 16}, 11, t.muted);
  text(g, "950 Hz", {299, 580, 90, 16}, 11, t.muted);
  text(g, "14 kHz", {670, 580, 80, 16}, 11, t.muted,
       juce::Justification::centredRight);
  if (expanded)
    text(g, "Below the activity floor: compression releases normally",
         {36, 638, 728, 22}, 12, t.muted, juce::Justification::centred);
}
void LevelPocketAudioProcessorEditor::paintOverChildren(juce::Graphics &g) {
  if (!processor.displayBypass.load() &&
      processor.parameters.getRawParameterValue("bypass")->load() < .5f)
    return;
  g.setColour(look.tokens().chassis.withAlpha(.76f));
  g.fillRect(scaled(0, 53, 800, 291));
  g.setColour(look.ink());
  g.setFont(pocketFont(23 * float(getWidth()) / 800));
  g.drawText("BYPASSED", scaled(0, 160, 800, 50), juce::Justification::centred);
}
