#pragma once

#include "UiCommon.h"
#include "PluginProcessor.h"

/** The drawing surface: the selected lane over one shape loop, with the others faint behind it.
    Zooms in time, edits points one at a time or as a selection, or paints freehand. */
class ShapeCanvas : public juce::Component
{
public:
    enum class Mode { Points, Brush };

    explicit ShapeCanvas (InkroomProcessor& p);

    void setSpace (int s);
    void setLane (int l);
    void setMode (Mode m);
    void setGrid (int divisionsPerBeat);
    void invalidate();
    int getLane() const  { return lane; }
    int getGrid() const  { return grid; }

    void zoomToFit();
    void zoomToBar();
    void zoomToBeat();

    /** Called before any change to the shape (for undo), and after it (for linked lanes). */
    std::function<void()> onBeforeEdit, onEdited;
    /** Formats a lane value for the drag readout. */
    std::function<juce::String (int lane, float value)> valueText;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed (const juce::KeyPress&) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

private:
    struct Node { float x, y; };
    enum class Drag { None, Point, Group, Scale, Marquee, Overview, Brush };

    ink::Lanes& lanes() const { return proc.lanesFor (space); }
    juce::Rectangle<float> overviewArea() const;
    juce::Rectangle<float> plotArea() const;
    float xOfIndex (float idx) const;
    float indexAtX (float x) const;
    float yOf (float v) const;
    float valueAtY (float y) const;
    int displayDivisions() const;
    float snapIndex (float idx, bool snap) const;
    void setView (double start, double span);
    void zoomAround (float x, double factor);

    void buildNodes();
    void writeNodes();
    int nodeAt (juce::Point<float>) const;
    int selectedCount() const;
    void clearSelection();
    void brushAt (juce::Point<float>);
    void deleteSelection();
    void nudgeSelection (float dxIdx, float dy);
    void edited();
    void updateCursor (const juce::ModifierKeys&);
    juce::String positionText (float idx) const;

    InkroomProcessor& proc;
    int space = ink::AddSpace, lane = 0, grid = 4;
    Mode mode = Mode::Points;

    std::vector<Node> nodes;
    std::vector<char> selected;
    bool nodesValid = false;
    int hover = -1;

    Drag dragKind = Drag::None;
    int anchor = -1;
    juce::Point<float> dragStart;
    std::vector<Node> dragOrig;
    juce::Rectangle<float> marquee;
    int lastIdx = -1;
    float lastV = 0.0f;

    double viewStart = 0.0, viewSpan = 1.0;

    friend struct InkroomEditorTest;
};
