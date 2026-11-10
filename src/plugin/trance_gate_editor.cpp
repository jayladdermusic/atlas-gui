// SPDX-License-Identifier: GPL-3.0-or-later
#include "trance_gate_editor.h"
#include "operators.h"
#include "synth_strings.h"

namespace {
// A step's level: 0 is its full level, -1 silent (muted), and presets may
// hold others, up to +1 for double.
constexpr double kMutedValue = -1.0;
bool isMuted(double value) { return value <= kMutedValue + 0.005; }
String levelText(double value) { return String(roundToInt((value + 1.0) * 100.0)) + "%"; }
}

TranceGateEditor::TranceGateEditor(State& state) : state_(state) {
  setTitle("Trance gate Editor");
  setDescription("Keyboard step sequencer with position announcements and concise edit feedback.");
  setHelpText(helpText());
  getStatusText = [this] { return statusText(); };
  onKeyPressed = [this](const KeyPress& key) { return handleShortcut(key); };
  paintShape = [this](Graphics& g, Rectangle<float> area) { paintSteps(g, area); };
  // The grid and the range have rows of their own, which change the picture too.
  shapeStamp = [this] {
    atlas::ui::Stamp stamp;
    if (read)
      stamp.add(read("grid")).add(read("start")).add(read("stop"));
    return stamp.add(state_.cursor).value();
  };
}

void TranceGateEditor::paintSteps(Graphics& g, Rectangle<float> area) const {
  using namespace atlas::ui;
  if (!read)
    return;
  const int start = jlimit(1, vital::trance_gate::kSteps, roundToInt(read("start")));
  const int stop = jlimit(1, vital::trance_gate::kSteps, roundToInt(read("stop")));
  Step steps[vital::trance_gate::kSteps];
  double peak = 1.0;
  for (int i = 0; i < vital::trance_gate::kSteps; ++i) {
    steps[i] = step(i);
    peak = jmax(peak, steps[i].value + 1.0);
  }

  // Each step's gain through its length, with no attack or decay.
  const vital::trance_gate::Envelope flat { 0.0, 0.0, 1.0 };
  constexpr int kSamples = 16;
  const float width = area.getWidth() / static_cast<float>(vital::trance_gate::kSteps);
  for (int i = 0; i < vital::trance_gate::kSteps; ++i) {
    const Rectangle<float> slot(area.getX() + static_cast<float>(i) * width, area.getY(), width, area.getHeight());
    const auto bar = slot.reduced(1.0f, 0.0f);
    // A gated step holds its level and then cuts off; a ramp slopes.
    const bool ramp = steps[i].mode >= 3;
    Path shape;
    shape.startNewSubPath(bar.getX(), bar.getBottom());
    for (int sample = 0; sample < kSamples; ++sample) {
      const double phase = (sample + (ramp ? 0.0 : 0.5)) / kSamples;
      const double gain = vital::trance_gate::targetGain(steps[i], flat, phase, 1.0) / peak;
      const float top = bar.getBottom() - static_cast<float>(gain) * bar.getHeight();
      shape.lineTo(bar.getX() + bar.getWidth() * static_cast<float>(sample) / kSamples, top);
      if (!ramp)
        shape.lineTo(bar.getX() + bar.getWidth() * static_cast<float>(sample + 1) / kSamples, top);
    }
    if (ramp) {
      const double gain = vital::trance_gate::targetGain(steps[i], flat, 1.0, 1.0) / peak;
      shape.lineTo(bar.getRight(), bar.getBottom() - static_cast<float>(gain) * bar.getHeight());
    }
    shape.lineTo(bar.getRight(), bar.getBottom());
    shape.closeSubPath();
    const bool plays = i + 1 >= jmin(start, stop) && i + 1 <= jmax(start, stop);
    g.setColour(colours::accent.withAlpha(plays ? 0.9f : 0.3f));
    g.fillPath(shape);
    g.setColour(colours::track.withAlpha(plays ? 1.0f : 0.5f));
    g.fillRect(bar.withTop(bar.getBottom() - 1.0f));
    if (i == state_.cursor) {
      g.setColour(colours::focus);
      g.drawRect(slot, 1.5f);
    }
  }
}

String TranceGateEditor::helpText() {
  return "Left/Right move between steps. G/H change the step's shape: Whole, Medium, Short, Ramp down or Ramp up. "
         "M mutes or unmutes the step. Minus/Equals make the grid coarser/finer. "
         "Semicolon/Apostrophe shorten/lengthen the pattern. Backspace resets the pattern to its default. "
         "Navigation announces position first. Edits announce only what changed.";
}

TranceGateEditor::Step TranceGateEditor::step(int index) const {
  const auto prefix = "step_" + String(index + 1) + "_";
  return { read(prefix + "value"), jlimit(0, vital::trance_gate::kModes - 1, roundToInt(read(prefix + "mode"))), read(prefix + "curve") };
}

String TranceGateEditor::statusText() const {
  if (!read) return "Trance gate editor unavailable";
  const auto value = step(state_.cursor);
  const int grid = jlimit(0, 7, roundToInt(read("grid")));
  const int time = roundToInt(state_.cursor * vital::trance_gate::beatsPerStep(grid) * 100.0);
  // The level only when a preset set one other than full or silent.
  String level;
  if (isMuted(value.value))
    level = ", muted";
  else if (std::abs(value.value) >= 0.005)
    level = ", level " + levelText(value.value);
  return "Bar " + String(time / 400 + 1) + " beat " + String((time % 400) / 100 + 1) + " " +
      String(time % 100) + "%, Step " + String(state_.cursor + 1) + ", " +
      String(strings::kTranceGateModeNames[value.mode]) + level;
}

void TranceGateEditor::refresh() {
  // The explicit announcement supplies edit feedback. A valueChanged event on
  // this group would also make screen readers repeat its full description.
  repaint();
}
void TranceGateEditor::say(const String& message) {
  refresh();
  if (announce) announce(message);
}
void TranceGateEditor::apply(Changes changes, const String& message) {
  if (write && !changes.empty()) write(changes);
  say(message);
}
void TranceGateEditor::move(int direction) {
  state_.cursor = jlimit(0, 31, state_.cursor + direction);
  say(statusText());
}
void TranceGateEditor::changeMode(int direction) {
  const int mode = (step(state_.cursor).mode + direction + vital::trance_gate::kModes) % vital::trance_gate::kModes;
  apply({ { "step_" + String(state_.cursor + 1) + "_mode", static_cast<float>(mode) } }, strings::kTranceGateModeNames[mode]);
}
void TranceGateEditor::toggleMute() {
  // Unmuting brings the step back at its full level.
  const bool mute = !isMuted(step(state_.cursor).value);
  apply({ { "step_" + String(state_.cursor + 1) + "_value", static_cast<float>(mute ? kMutedValue : 0.0) } },
        mute ? "Muted" : "Unmuted");
}

void TranceGateEditor::resetPattern() {
  // Every step, the grid and the pattern's range as a new Trance gate has
  // them (trance_gate_parameters.inc); its other controls stay.
  Changes changes;
  for (int i = 1; i <= vital::trance_gate::kSteps; ++i)
    for (auto* field : { "_value", "_mode", "_curve" })
      changes.emplace_back("step_" + String(i) + field, 0.0f);
  changes.emplace_back("grid", 2.0f);
  changes.emplace_back("start", 1.0f);
  changes.emplace_back("stop", static_cast<float>(vital::trance_gate::kSteps));
  state_.cursor = 0;
  apply(std::move(changes), "Reset pattern to default");
}

bool TranceGateEditor::handleShortcut(const KeyPress& key) {
  if (!read || !write) return false;
  const auto modifiers = key.getModifiers();
  // Leave host and assistive-technology modifier chords untouched.
  if (modifiers.isCommandDown() || modifiers.isCtrlDown() || modifiers.isAltDown()) return false;
  const auto character = CharacterFunctions::toLowerCase(key.getTextCharacter());
  const bool shift = modifiers.isShiftDown();
  const int code = key.getKeyCode();
  if (!shift && code == KeyPress::leftKey) { move(-1); return true; }
  if (!shift && code == KeyPress::rightKey) { move(1); return true; }
  if (!shift && (character == 'g' || character == 'h')) { changeMode(character == 'h' ? 1 : -1); return true; }
  if (!shift && character == 'm') { toggleMute(); return true; }
  if (!shift && code == KeyPress::backspaceKey) { resetPattern(); return true; }
  if (character == '-' || character == '_' || character == '=' || character == '+') {
    const int delta = character == '-' || character == '_' ? -1 : 1;
    const int grid = jlimit(0, 7, roundToInt(read("grid")) + delta);
    apply({ { "grid", static_cast<float>(grid) } }, strings::kTranceGateGridNames[grid]); return true;
  }
  if (character == ';' || character == '\'') {
    const int start = jlimit(1, 32, roundToInt(read("start")));
    const int stop = jlimit(1, 32, roundToInt(read("stop")));
    const int direction = start <= stop ? 1 : -1;
    const int length = jmax(1, std::abs(stop - start) + 1 + (character == ';' ? -1 : 1));
    const int next_stop = jlimit(1, 32, start + direction * (length - 1));
    apply({ { "stop", static_cast<float>(next_stop) } }, String(start) + " to " + String(next_stop)); return true;
  }
  return false;
}
