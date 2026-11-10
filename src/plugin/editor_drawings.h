// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "JuceHeader.h"
#include "editor_look_and_feel.h"

#include <cmath>
#include <cstring>

// The editor's pictures: an LFO's shape, a remap curve, a wavetable frame, an
// envelope. Each shows what a screen reader already says in words, read from
// the same data. None of it is a control.
namespace atlas::ui {

// Folds what a drawing shows into one number, to tell when it has changed.
class Stamp {
  public:
    Stamp& add(uint32 bits) {
      hash_ = (hash_ ^ bits) * 16777619u;
      return *this;
    }
    Stamp& add(int value) { return add(static_cast<uint32>(value)); }
    Stamp& add(float value) {
      uint32 bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      return add(bits);
    }

    uint32 value() const { return hash_; }

  private:
    uint32 hash_ = 2166136261u;
};

// A component that draws a shape other controls or the engine can change.
// Nothing here animates: the editor's timer calls refreshShape on the ones in
// view, and one repaints only when shapeStamp says what it draws has changed.
class ShapeView : public Component {
  public:
    std::function<void(Graphics&, Rectangle<float>)> paintShape;
    std::function<uint32()> shapeStamp;

    void refreshShape() {
      if (!shapeStamp)
        return;
      const uint32 stamp = shapeStamp();
      if (stamp == stamp_)
        return;
      stamp_ = stamp;
      repaint();
    }

  private:
    uint32 stamp_ = 0;
};

// A shape with no control of its own, such as an envelope's: a picture for
// the eye, which no key or screen reader reaches.
class ShapeDisplay : public ShapeView {
  public:
    ShapeDisplay() {
      setAccessible(false);
      setInterceptsMouseClicks(false, false);
    }

    void paint(Graphics& g) override {
      const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
      g.setColour(colours::window);
      g.fillRoundedRectangle(bounds, 6.0f);
      g.setColour(colours::outline);
      g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
      if (paintShape)
        paintShape(g, bounds.reduced(10.0f, 8.0f));
    }
};

// The box of a keyboard editor (LFO, Trance gate, wavetable, remap): its
// shape, when it draws one, above the line a screen reader reads for it.
inline void drawKeyboardEditorBox(Graphics& g, const Component& editor, const String& status,
                                  const std::function<void(Graphics&, Rectangle<float>)>& paintShape = {}) {
  drawField(g, editor.getLocalBounds().toFloat().reduced(1.0f), colours::window, editor.hasKeyboardFocus(false), 6.0f);
  auto bounds = editor.getLocalBounds().reduced(12, 6);
  if (paintShape) {
    const auto text = bounds.removeFromBottom(22);
    paintShape(g, bounds.withTrimmedBottom(4).toFloat());
    g.setColour(colours::textDim);
    g.setFont(FontOptions(13.5f));
    g.drawText(status, text, Justification::centredLeft, true);
    return;
  }
  g.setColour(colours::text);
  g.setFont(FontOptions(15.0f));
  g.drawText(status, bounds, Justification::centredLeft, true);
}

inline float plotX(Rectangle<float> area, float phase) {
  return area.getX() + jlimit(0.0f, 1.0f, phase) * area.getWidth();
}

inline float plotY(Rectangle<float> area, float value) {
  return area.getBottom() - jlimit(0.0f, 1.0f, value) * area.getHeight();
}

// A faint vertical line at `phase`.
inline void drawPlotLine(Graphics& g, Rectangle<float> area, float phase) {
  g.setColour(colours::outline);
  g.fillRect(plotX(area, phase) - 0.5f, area.getY(), 1.0f, area.getHeight());
}

// The curve valueAt(phase), both from 0 to 1, across `area`, tinted between
// the curve and `baseline`.
template <typename ValueAt>
void drawCurve(Graphics& g, Rectangle<float> area, ValueAt&& valueAt, float baseline = 0.0f) {
  const int steps = jmax(2, roundToInt(area.getWidth()));
  Path line;
  for (int i = 0; i <= steps; ++i) {
    const float phase = static_cast<float>(i) / static_cast<float>(steps);
    const float x = plotX(area, phase);
    const float y = plotY(area, valueAt(phase));
    if (i == 0)
      line.startNewSubPath(x, y);
    else
      line.lineTo(x, y);
  }
  Path fill(line);
  fill.lineTo(area.getRight(), plotY(area, baseline));
  fill.lineTo(area.getX(), plotY(area, baseline));
  fill.closeSubPath();
  g.setColour(colours::accent.withAlpha(0.18f));
  g.fillPath(fill);
  g.setColour(colours::accent);
  g.strokePath(line, PathStrokeType(1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

// A point of a curve; the selected one is larger and in the focus colour.
inline void drawCurvePoint(Graphics& g, Rectangle<float> area, float phase, float value, bool selected) {
  const float radius = selected ? 4.5f : 3.0f;
  g.setColour(selected ? colours::focus : colours::text);
  g.fillEllipse(Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre({ plotX(area, phase), plotY(area, value) }));
}

// Where a keyboard editor's cursor stands.
inline void drawPlotCursor(Graphics& g, Rectangle<float> area, float phase) {
  g.setColour(colours::focus.withAlpha(0.8f));
  g.fillRect(plotX(area, phase) - 0.75f, area.getY(), 1.5f, area.getHeight());
}

// An envelope's stages: times in seconds, sustain from 0 to 1, and each
// stage's curve as the engine's power.
struct EnvelopeShape {
  float delay = 0.0f, attack = 0.0f, hold = 0.0f, decay = 0.0f, sustain = 1.0f, release = 0.0f;
  float attack_power = 0.0f, decay_power = 0.0f, release_power = 0.0f;
};

// The engine's curve for a stage (futils::powerScale).
inline float powerScale(float value, float power) {
  if (std::abs(power) < 0.01f)
    return value;
  return (std::exp(power * value) - 1.0f) / (std::exp(power) - 1.0f);
}

// Delay, attack, hold, decay, a held stretch at the sustain level, release.
// A held note has no length of its own, so the stretch takes a fifth of the rest.
inline void drawEnvelope(Graphics& g, Rectangle<float> area, const EnvelopeShape& shape) {
  const float timed = shape.delay + shape.attack + shape.hold + shape.decay + shape.release;
  const float held = jmax(0.05f, timed * 0.2f);
  const float total = timed + held;
  const float attack_start = shape.delay;
  const float hold_start = attack_start + shape.attack;
  const float decay_start = hold_start + shape.hold;
  const float sustain_start = decay_start + shape.decay;
  const float release_start = sustain_start + held;

  for (const float time : { attack_start, hold_start, decay_start, sustain_start, release_start })
    drawPlotLine(g, area, time / total);
  drawCurve(g, area, [&](float phase) {
    const float time = phase * total;
    if (time < attack_start)
      return 0.0f;
    if (time < hold_start)
      return powerScale((time - attack_start) / shape.attack, -shape.attack_power);
    if (time < decay_start)
      return 1.0f;
    if (time < sustain_start)
      return 1.0f + (shape.sustain - 1.0f) * powerScale((time - decay_start) / shape.decay, shape.decay_power);
    if (time < release_start || shape.release <= 0.0f)
      return time < release_start ? shape.sustain : 0.0f;
    return shape.sustain * (1.0f - powerScale((time - release_start) / shape.release, shape.release_power));
  });
}

// A wave's cycle, `count` samples from -1 to 1, around a centre line.
inline void drawWaveform(Graphics& g, Rectangle<float> area, const float* samples, int count) {
  g.setColour(colours::outline);
  g.fillRect(area.getX(), area.getCentreY() - 0.5f, area.getWidth(), 1.0f);
  drawCurve(g, area, [=](float phase) {
    return 0.5f + 0.5f * samples[jmin(count - 1, static_cast<int>(phase * static_cast<float>(count)))];
  }, 0.5f);
}

// Bars for `count` levels against the largest of them; `selected` is in the focus colour.
inline void drawBars(Graphics& g, Rectangle<float> area, const float* levels, int count, int selected) {
  float peak = 0.0001f;
  for (int i = 0; i < count; ++i)
    peak = jmax(peak, levels[i]);
  const float width = area.getWidth() / static_cast<float>(count);
  for (int i = 0; i < count; ++i) {
    const float height = jmax(1.0f, area.getHeight() * levels[i] / peak);
    g.setColour(i == selected ? colours::focus : colours::accent);
    g.fillRect(area.getX() + static_cast<float>(i) * width + 0.5f, area.getBottom() - height,
               jmax(1.0f, width - 1.0f), height);
  }
}

} // namespace atlas::ui
