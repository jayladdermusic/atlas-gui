// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "JuceHeader.h"
#include "editor_look_and_feel.h"

namespace atlas::ui {

// A parameter row: its name above its control.
constexpr int kRowPadX = 8;
constexpr int kRowPadY = 4;
constexpr int kRowLabelHeight = 20;
constexpr int kControlHeight = 30;
constexpr int kRowHeight = 2 * kRowPadY + kRowLabelHeight + kControlHeight;
constexpr int kButtonHeight = 38;
constexpr int kButtonGap = 6;
// A keyboard editor with its shape drawn in it, and a picture with no control.
constexpr int kShapeEditorHeight = 150;
constexpr int kShapePictureHeight = 110;

// A control's name shown above it, for the eye only. A screen reader reads the
// control's own title, and neither it nor the keyboard ever lands here.
class Caption : public Component {
  public:
    explicit Caption(const String& text) : text_(text) {
      setAccessible(false);
      setInterceptsMouseClicks(false, false);
    }

    void paint(Graphics& g) override {
      g.setColour(colours::textDim);
      g.setFont(FontOptions(13.5f));
      g.drawText(text_, getLocalBounds().withTrimmedLeft(2), Justification::centredLeft, true);
    }

  private:
    String text_;
};

// Lays controls out in a grid of equal columns, left to right and then down,
// in the order they are asked for. That is the order the editor adds them to
// its focus lists, so the keyboard and a screen reader move through the grid
// as it reads.
//
// The editor's layout code keeps a running y for each group it fills. A cell
// goes on the line of the cell before it while nothing else was placed in
// between: the same parent, x and width, and y still at the bottom of that
// line. Anything placed by moving y (a heading, an editor, a nested group)
// starts a new line, with nothing to tell this class. So does a button after
// rows or a row after buttons, since they differ in height.
class RowFlow {
  public:
    static constexpr int kMinCellWidth = 250;
    static constexpr int kMaxColumns = 4;
    static constexpr int kGap = 8;

    // The next cell's bounds in `parent`. Moves `y` to the bottom of its line.
    Rectangle<int> next(const Component& parent, int x, int& y, int width, int height) {
      const int columns = jlimit(1, kMaxColumns, (width + kGap) / (kMinCellWidth + kGap));
      if (&parent != parent_ || x != x_ || width != width_ || y != line_top_ + line_height_ ||
          (line_height_ != 0 && height != line_height_) || column_ >= columns) {
        parent_ = &parent;
        x_ = x;
        width_ = width;
        line_top_ = y;
        line_height_ = 0;
        column_ = 0;
      }
      line_height_ = height;
      y = line_top_ + line_height_;
      const int cell_width = (width - (columns - 1) * kGap) / columns;
      return { x + column_++ * (cell_width + kGap), line_top_, cell_width, height };
    }

    // A button's bounds: a cell with room below it before the next line.
    Rectangle<int> nextButton(const Component& parent, int x, int& y, int width) {
      return next(parent, x, y, width, kButtonHeight + kButtonGap).withTrimmedBottom(kButtonGap);
    }

    // A button among parameter rows of `rowHeight`: a cell of their height,
    // the button level with their controls.
    Rectangle<int> nextBesideRows(const Component& parent, int x, int& y, int width, int rowHeight) {
      return next(parent, x, y, width, rowHeight).reduced(kRowPadX, kRowPadY).removeFromBottom(kControlHeight);
    }

    // Forgets the line it was on, once the components it was laid out in are gone.
    void reset() { parent_ = nullptr; }

  private:
    const Component* parent_ = nullptr;
    int x_ = 0;
    int width_ = 0;
    int line_top_ = 0;
    int line_height_ = 0;
    int column_ = 0;
};

} // namespace atlas::ui
