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

const juce::Colour pink    { 0xfff28aa6 };
const juce::Colour violet  { 0xff9d8eff };
const juce::Colour teal    { 0xff5ccab8 };
const juce::Colour amber   { 0xffe8b85e };
const juce::Colour ember   { 0xfff47b4c };
const juce::Colour blue    { 0xff7db4ff };

struct LaneInfo { const char* name; juce::Colour colour; const char* top; const char* bot; };

inline int laneCount (int space) { return space == ShapeSpace ? (int) NumShapeLanes : (int) NumLanes; }

inline const LaneInfo& laneInfo (int space, int lane)
{
    static const LaneInfo add[NumLanes] = {
        { "Level", pink, "Full", "Silent" }, { "Size", violet, "Hall", "Closet" }, { "Length", teal, "4.5 s", "0.3 s" },
        { "Pan", amber, "Right", "Left" }, { "Drive", ember, "Hot", "Clean" } };
    static const LaneInfo shape[NumShapeLanes] = {
        { "Sustain", violet, "+6 dB", "Choked" }, { "Punch", blue, "+6 dB", "Muted" }, { "Level", pink, "Full", "Silent" },
        { "Width", teal, "Wide", "Mono" }, { "Pan", amber, "Right", "Left" }, { "Drive", ember, "Hot", "Clean" } };
    return space == ShapeSpace ? shape[lane] : add[lane];
}
} // namespace ink::ui

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

    void setSpace (int s)   { space = s; lane = 0; invalidate(); repaint(); }
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

    ink::Lanes& lanes() const { return proc.lanesFor (space); }
    juce::Rectangle<float> plotArea() const;
    float phaseAtX (float x) const;
    float valueAtY (float y) const;
    float snapX (float phase, bool snap) const;
    void buildNodes();
    void writeNodes();
    int nodeAt (juce::Point<float>) const;
    void brushAt (juce::Point<float>);

    InkroomProcessor& proc;
    int space = ink::AddSpace, lane = 0, grid = 4;
    Mode mode = Mode::Points;
    std::vector<Node> nodes;
    bool nodesValid = false;
    int hover = -1, drag = -1, lastIdx = -1;
    float lastV = 0.0f;
};

/** The ink blot: grows with Size (or Sustain), drifts with Pan, glows with Level, warms with Drive. */
class RoomView : public juce::Component
{
public:
    struct State { float level = 0.5f, size = 0.5f, pan = 0.5f, drive = 0.1f, width = 0.5f, energy = 0.3f, depth = 0.8f; int type = 0; bool shape = false; };
    void setState (const State& s) { st = s; }
    void paint (juce::Graphics&) override;

private:
    State st;
};

/** Holds the whole interface at its design size; the editor scales it to the window. */
class ScaledContent : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    void paint (juce::Graphics& g) override { if (painter) painter (g); }
};

class InkroomEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit InkroomEditor (InkroomProcessor&);
    ~InkroomEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int designW = 1000, designH = 720;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct HistoryEntry { int space; ink::Lanes::Snapshot snap; };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void applySpace (int sp);
    void checkpoint();
    void undo();
    void redo();
    void markCustom();
    void stamp (int which);
    void updateHistoryButtons();
    void setupCombo (juce::ComboBox&, const juce::String& paramId, std::unique_ptr<ComboAtt>&);
    void setupSlider (juce::Slider&, const juce::String& paramId, std::unique_ptr<SliderAtt>&, const juce::String& label);
    juce::String formatValue (int lane, float v) const;
    ink::Lanes& shownLanes() { return proc.lanesFor (shownSpace); }

    InkroomProcessor& proc;
    InkLook look;
    ScaledContent content;
    ShapeCanvas canvas;
    RoomView room;
    std::unique_ptr<juce::PropertiesFile> settings;

    juce::ComboBox presetBox, loopBox, typeBox, charBox, placeBox, gridBox;
    juce::TextButton addSpaceBtn { "Add space" }, shapeSpaceBtn { "Shape space" };
    std::array<juce::TextButton, ink::kMaxLanes> laneButtons;
    juce::TextButton pointsBtn { "Points" }, brushBtn { "Brush" }, smoothBtn { "Smooth" }, invertBtn { "Invert" },
                     undoBtn { "Undo" }, redoBtn { "Redo" }, soloBtn { "Tail solo" };
    std::array<juce::TextButton, 5> stampBtns;
    juce::Slider predelaySl, brightSl, outputSl, mixSl, driftSl, swingSl, depthSl, toneSl, hitLenSl, sensSl, hitsSl;

    std::unique_ptr<ComboAtt> loopAtt, typeAtt, charAtt, placeAtt;
    std::unique_ptr<SliderAtt> predelayAtt, brightAtt, outputAtt, mixAtt, driftAtt, swingAtt, depthAtt, toneAtt,
                               hitLenAtt, sensAtt, hitsAtt;
    std::unique_ptr<ButtonAtt> soloAtt;

    std::vector<std::pair<juce::String, juce::Slider*>> sliderLabels;
    juce::Rectangle<int> readoutArea, hitLightArea;

    std::vector<HistoryEntry> undoStack, redoStack;
    int shownSpace = -1;
    int seenVersion = -1;
    float smoothEnergy = 0.3f, smoothHit = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InkroomEditor)
};
