#pragma once

#include "PluginProcessor.h"

namespace ink::ui
{
const juce::Colour paper   { 0xff17151c };
const juce::Colour panel   { 0xff1f1c26 };
const juce::Colour panel2  { 0xff26222f };
const juce::Colour line    { 0xff363140 };
const juce::Colour text    { 0xffece6dc };
const juce::Colour muted   { 0xffa39bab };
const juce::Colour accent  { 0xffe9c27f };
const std::array<juce::Colour, NumLanes> laneColour {
    juce::Colour (0xfff28aa6), juce::Colour (0xff9d8eff), juce::Colour (0xff5ccab8),
    juce::Colour (0xffe8b85e), juce::Colour (0xfff47b4c) };
const std::array<const char*, NumLanes> laneName { "Level", "Size", "Length", "Pan", "Drive" };
}

class InkLook : public juce::LookAndFeel_V4
{
public:
    InkLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

/** The drawing surface: one loop of the selected lane, with the others faint behind it. */
class ShapeCanvas : public juce::Component
{
public:
    enum class Mode { Points, Brush };

    explicit ShapeCanvas (InkroomProcessor& p) : proc (p) {}

    void setLane (int l)    { lane = l; invalidate(); repaint(); }
    void setMode (Mode m)   { mode = m; invalidate(); repaint(); }
    void setGrid (int g)    { grid = g; repaint(); }
    void invalidate()       { nodes.clear(); nodesValid = false; hover = -1; drag = -1; }
    int getLane() const     { return lane; }
    int getGrid() const     { return grid; }

    std::function<void()> onBeforeEdit;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Node { float x, y; };

    juce::Rectangle<float> plotArea() const;
    float phaseAtX (float x) const;
    float valueAtY (float y) const;
    float snapX (float phase, bool snap) const;
    void buildNodes();
    void writeNodes();
    int nodeAt (juce::Point<float>) const;
    void brushAt (juce::Point<float>);

    InkroomProcessor& proc;
    int lane = 0, grid = 4;
    Mode mode = Mode::Points;
    std::vector<Node> nodes;
    bool nodesValid = false;
    int hover = -1, drag = -1, lastIdx = -1;
    float lastV = 0.0f;
};

/** The ink blot: grows with Size, drifts with Pan, glows with Level, warms with Drive. */
class RoomView : public juce::Component
{
public:
    void setState (const std::array<float, ink::NumLanes>& v, float e, int t, float d) { vals = v; energy = e; type = t; depth = d; }
    void paint (juce::Graphics&) override;

private:
    std::array<float, ink::NumLanes> vals {};
    float energy = 0.3f, depth = 0.8f;
    int type = 0;
};

class InkroomEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit InkroomEditor (InkroomProcessor&);
    ~InkroomEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void selectLane (int l);
    void setMode (ShapeCanvas::Mode m);
    void checkpoint();
    void undo();
    void redo();
    void markCustom();
    void stamp (int which);
    void updateHistoryButtons();
    void setupCombo (juce::ComboBox&, const juce::String& paramId, std::unique_ptr<ComboAtt>&);
    void setupSlider (juce::Slider&, const juce::String& paramId, std::unique_ptr<SliderAtt>&, const juce::String& label);
    juce::String formatValue (int lane, float v) const;

    InkroomProcessor& proc;
    InkLook look;
    ShapeCanvas canvas;
    RoomView room;

    juce::ComboBox presetBox, loopBox, typeBox, charBox, placeBox, gridBox;
    std::array<juce::TextButton, ink::NumLanes> laneButtons;
    juce::TextButton pointsBtn { "Points" }, brushBtn { "Brush" }, smoothBtn { "Smooth" }, invertBtn { "Invert" },
                     undoBtn { "Undo" }, redoBtn { "Redo" }, soloBtn { "Tail solo" };
    std::array<juce::TextButton, 5> stampBtns;
    juce::Slider predelaySl, brightSl, outputSl, mixSl, driftSl, swingSl, depthSl, toneSl;

    std::unique_ptr<ComboAtt> loopAtt, typeAtt, charAtt, placeAtt;
    std::unique_ptr<SliderAtt> predelayAtt, brightAtt, outputAtt, mixAtt, driftAtt, swingAtt, depthAtt, toneAtt;
    std::unique_ptr<ButtonAtt> soloAtt;

    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sliderLabels;
    juce::Rectangle<int> readoutArea;

    std::vector<ink::Lanes::Snapshot> undoStack, redoStack;
    int seenVersion = -1;
    float smoothEnergy = 0.3f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InkroomEditor)
};
