// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "JuceHeader.h"

// How the editor looks: its palette and the drawing of JUCE's stock controls.
// Nothing here changes what a control is, says or does, or where the keyboard
// and a screen reader find it.
namespace atlas::ui {

namespace colours {
  inline const Colour window       { 0xff14161a };
  inline const Colour group        { 0xff1b1e24 };   // a group's card
  inline const Colour section      { 0xff23272f };   // a section's card inside it
  inline const Colour field        { 0xff2c313b };   // buttons, menus, text boxes
  inline const Colour fieldHover   { 0xff363c48 };
  inline const Colour track        { 0xff4a5160 };   // the unfilled part of a slider or bar
  inline const Colour outline      { 0xff363c47 };   // around cards
  inline const Colour fieldOutline { 0xff6b7486 };   // around controls: 3:1 against a card
  inline const Colour text         { 0xffe9ecf1 };
  inline const Colour textDim      { 0xffa9b2bf };
  inline const Colour accent       { 0xff58b6d9 };
  inline const Colour focus        { 0xffa5ddf6 };   // the ring around the focused control
  inline const Colour selection    { 0xff2b5c78 };
  inline const Colour warning      { 0xffe4a05e };
}

constexpr float kFieldRadius = 5.0f;
constexpr float kCardRadius = 8.0f;

inline void drawFocusRing(Graphics& g, Rectangle<float> bounds, float radius = kFieldRadius) {
  g.setColour(colours::focus);
  g.drawRoundedRectangle(bounds.reduced(1.0f), radius, 2.0f);
}

// A filled, outlined field; the outline becomes the focus ring.
inline void drawField(Graphics& g, Rectangle<float> bounds, Colour fill, bool focused,
                      float radius = kFieldRadius) {
  g.setColour(fill);
  g.fillRoundedRectangle(bounds, radius);
  if (focused) {
    drawFocusRing(g, bounds, radius);
    return;
  }
  g.setColour(colours::fieldOutline);
  g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

// A bar filled up to `proportion` of its width.
inline void drawBar(Graphics& g, Rectangle<float> bar, float proportion, Colour fill = colours::accent) {
  const float radius = bar.getHeight() * 0.5f;
  g.setColour(colours::track);
  g.fillRoundedRectangle(bar, radius);
  g.setColour(fill);
  g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * jlimit(0.0f, 1.0f, proportion)), radius);
}

class EditorLookAndFeel : public LookAndFeel_V4 {
  public:
    // On a toggle whose row already shows its name: the switch is followed by
    // its state instead of the name again.
    static inline const Identifier kShowsStateOnly { "showsStateOnly" };

    EditorLookAndFeel() :
        LookAndFeel_V4(ColourScheme(colours::window, colours::field, colours::section, colours::fieldOutline,
                                    colours::text, colours::accent, colours::text, colours::selection,
                                    colours::text)) {
      setColour(Slider::textBoxBackgroundColourId, colours::field);
      setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
      // Not opaque, so its corners can be round (fillTextEditorBackground).
      setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
      setColour(TextEditor::highlightColourId, colours::selection);
      setColour(TextEditor::highlightedTextColourId, colours::text);
      setColour(CaretComponent::caretColourId, colours::focus);
      setColour(ListBox::backgroundColourId, colours::window);
      setColour(ListBox::outlineColourId, colours::outline);
      setColour(TableHeaderComponent::textColourId, colours::text);
      setColour(TableHeaderComponent::backgroundColourId, colours::section);
      setColour(TableHeaderComponent::outlineColourId, colours::outline);
      setColour(TableHeaderComponent::highlightColourId, colours::fieldHover);
      setColour(ScrollBar::thumbColourId, colours::track);
      setColour(PopupMenu::headerTextColourId, colours::textDim);
    }

    // Room at both ends of the track for the thumb and its focus ring.
    int getSliderThumbRadius(Slider&) override { return 11; }

    void drawLinearSlider(Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, Slider::SliderStyle style, Slider& slider) override {
      if (style != Slider::LinearHorizontal) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
      }
      const float alpha = slider.isEnabled() ? 1.0f : 0.4f;
      const float centre_y = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
      const Rectangle<float> track(static_cast<float>(x), centre_y - 3.0f, static_cast<float>(width), 6.0f);
      g.setColour(colours::track.withMultipliedAlpha(alpha));
      g.fillRoundedRectangle(track, 3.0f);
      g.setColour(colours::accent.withMultipliedAlpha(alpha));
      g.fillRoundedRectangle(track.withRight(jlimit(track.getX(), track.getRight(), sliderPos)), 3.0f);

      const float radius = 7.0f;
      const Rectangle<float> thumb(sliderPos - radius, centre_y - radius, radius * 2.0f, radius * 2.0f);
      g.setColour(colours::text.withMultipliedAlpha(alpha));
      g.fillEllipse(thumb);
      if (slider.hasKeyboardFocus(true)) {
        g.setColour(colours::focus);
        g.drawEllipse(thumb.expanded(3.0f), 2.0f);
      }
    }

    void drawLabel(Graphics& g, Label& label) override {
      const auto bounds = label.getLocalBounds().toFloat();
      const Colour background = label.findColour(Label::backgroundColourId);
      if (!background.isTransparent()) {
        g.setColour(background);
        g.fillRoundedRectangle(bounds, kFieldRadius);
      }
      const float alpha = label.isEnabled() ? 1.0f : 0.5f;
      if (!label.isBeingEdited()) {
        const Font font(getLabelFont(label));
        g.setColour(label.findColour(Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(font);
        const auto text_area = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), text_area, label.getJustificationType(),
                         jmax(1, static_cast<int>(static_cast<float>(text_area.getHeight()) / font.getHeight())),
                         label.getMinimumHorizontalScale());
      }
      const Colour outline = label.findColour(Label::outlineColourId);
      if (!outline.isTransparent()) {
        g.setColour(outline.withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(bounds.reduced(0.5f), kFieldRadius, 1.0f);
      }
    }

    void drawComboBox(Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box) override {
      const float alpha = box.isEnabled() ? 1.0f : 0.5f;
      const Rectangle<float> bounds(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
      drawField(g, bounds, colours::field.withMultipliedAlpha(alpha), box.hasKeyboardFocus(true));

      const float centre_x = static_cast<float>(width) - 17.0f;
      const float centre_y = static_cast<float>(height) * 0.5f;
      Path chevron;
      chevron.startNewSubPath(centre_x - 5.0f, centre_y - 2.5f);
      chevron.lineTo(centre_x, centre_y + 2.5f);
      chevron.lineTo(centre_x + 5.0f, centre_y - 2.5f);
      g.setColour(colours::textDim.withMultipliedAlpha(alpha));
      g.strokePath(chevron, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void drawButtonBackground(Graphics& g, Button& button, const Colour&, bool highlighted, bool down) override {
      Colour fill = button.getToggleState() ? colours::selection : colours::field;
      if (down)
        fill = fill.brighter(0.2f);
      else if (highlighted)
        fill = button.getToggleState() ? fill.brighter(0.1f) : colours::fieldHover;
      drawField(g, button.getLocalBounds().toFloat(), fill.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f),
                button.hasKeyboardFocus(true));
    }

    // A switch, then the button's text.
    void drawToggleButton(Graphics& g, ToggleButton& button, bool highlighted, bool) override {
      const bool on = button.getToggleState();
      const float alpha = button.isEnabled() ? 1.0f : 0.5f;
      const Rectangle<float> pill(4.0f, (static_cast<float>(button.getHeight()) - 20.0f) * 0.5f, 36.0f, 20.0f);
      Colour fill = on ? colours::accent : colours::track;
      if (highlighted)
        fill = fill.brighter(0.1f);
      g.setColour(fill.withMultipliedAlpha(alpha));
      g.fillRoundedRectangle(pill, 10.0f);
      g.setColour((on ? colours::window : colours::text).withMultipliedAlpha(alpha));
      g.fillEllipse(Rectangle<float>(14.0f, 14.0f).withCentre({ on ? pill.getRight() - 10.0f : pill.getX() + 10.0f,
                                                                 pill.getCentreY() }));
      if (button.hasKeyboardFocus(true))
        drawFocusRing(g, pill.expanded(3.0f), 13.0f);

      String text = button.getButtonText();
      if (button.getProperties().contains(kShowsStateOnly)) {
        // The words a screen reader reads for the state (SpokenToggleButton).
        const String named = button.getProperties()[on ? "accessibleOnText" : "accessibleOffText"].toString();
        text = named.isNotEmpty() ? named : String(on ? "On" : "Off");
      }
      g.setColour(colours::text.withMultipliedAlpha(alpha));
      g.setFont(FontOptions(15.0f));
      g.drawFittedText(text, button.getLocalBounds().withTrimmedLeft(static_cast<int>(pill.getRight()) + 10),
                       Justification::centredLeft, 2);
    }

    void fillTextEditorBackground(Graphics& g, int width, int height, TextEditor& editor) override {
      if (dynamic_cast<AlertWindow*>(editor.getParentComponent()) != nullptr) {
        LookAndFeel_V4::fillTextEditorBackground(g, width, height, editor);
        return;
      }
      const Colour fill = colours::field.withMultipliedAlpha(editor.isEnabled() ? 1.0f : 0.5f);
      // An editor given an opaque background of its own has no see-through corners.
      if (editor.isOpaque())
        g.fillAll(colours::field);
      g.setColour(fill);
      g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), kFieldRadius);
    }

    void drawTextEditorOutline(Graphics& g, int width, int height, TextEditor& editor) override {
      if (dynamic_cast<AlertWindow*>(editor.getParentComponent()) != nullptr) {
        LookAndFeel_V4::drawTextEditorOutline(g, width, height, editor);
        return;
      }
      const Rectangle<float> bounds(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
      if (editor.hasKeyboardFocus(true) && editor.isEnabled()) {
        drawFocusRing(g, bounds);
        return;
      }
      g.setColour(colours::fieldOutline.withMultipliedAlpha(editor.isEnabled() ? 1.0f : 0.5f));
      g.drawRoundedRectangle(bounds.reduced(0.5f), kFieldRadius, 1.0f);
    }

    // JUCE's own, with a sort arrow that shows on a dark header.
    void drawTableHeaderColumn(Graphics& g, TableHeaderComponent& header, const String& columnName, int, int width,
                               int height, bool isMouseOver, bool isMouseDown, int columnFlags) override {
      if (isMouseDown || isMouseOver)
        g.fillAll(colours::fieldHover.withMultipliedAlpha(isMouseDown ? 1.0f : 0.625f));

      auto area = Rectangle<int>(width, height).reduced(4, 0);
      if ((columnFlags & (TableHeaderComponent::sortedForwards | TableHeaderComponent::sortedBackwards)) != 0) {
        const bool forwards = (columnFlags & TableHeaderComponent::sortedForwards) != 0;
        Path arrow;
        arrow.addTriangle(0.0f, 0.0f, 0.5f, forwards ? -0.8f : 0.8f, 1.0f, 0.0f);
        g.setColour(colours::textDim);
        g.fillPath(arrow, arrow.getTransformToScaleToFit(area.removeFromRight(height / 2).reduced(4).toFloat(), true));
      }

      g.setColour(header.findColour(TableHeaderComponent::textColourId));
      g.setFont(FontOptions(static_cast<float>(height) * 0.5f, Font::bold));
      g.drawFittedText(columnName, area, Justification::centredLeft, 1);
    }
};

// One look for every editor in the process. It is JUCE's default while an
// editor is open, so controls built before they have a parent, menus and
// dialog windows use it too. Each editor holds one of these.
class SharedLookAndFeel {
  public:
    SharedLookAndFeel() {
      if (&LookAndFeel::getDefaultLookAndFeel() != &look_.get())
        LookAndFeel::setDefaultLookAndFeel(&look_.get());
    }

  private:
    SharedResourcePointer<EditorLookAndFeel> look_;
};

} // namespace atlas::ui
