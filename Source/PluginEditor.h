#pragma once

#include "PluginProcessor.h"
#include "UiCommon.h"
#include "ShapeCanvas.h"

class InkLook : public juce::LookAndFeel_V4
{
public:
    InkLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
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
    void lanesChanged();
    void updateHistoryButtons();
    void refreshLaneButtons();
    void rebuildPresetBox();
    void showShapesMenu();
    void applyShape (int id);
    void showLaneMenu();
    void beginSavingPreset();
    void finishSavingPreset (bool save);
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
    juce::TextEditor nameEditor;
    juce::TextButton addSpaceBtn { "Add space" }, shapeSpaceBtn { "Shape space" }, savePresetBtn { "Save" };
    std::array<juce::TextButton, ink::kMaxLanes> laneButtons;
    juce::TextButton pointsBtn { "Points" }, brushBtn { "Brush" }, shapesBtn { "Shapes" }, laneMenuBtn { "Lane" },
                     fitBtn { "Fit" }, barBtn { "Bar" }, beatBtn { "Beat" }, expandBtn { "Expand" },
                     smoothBtn { "Smooth" }, invertBtn { "Invert" }, undoBtn { "Undo" }, redoBtn { "Redo" }, soloBtn { "Tail solo" };
    juce::Slider predelaySl, brightSl, outputSl, mixSl, driftSl, swingSl, depthSl, toneSl, hitLenSl, sensSl, hitsSl;

    std::unique_ptr<ComboAtt> loopAtt, typeAtt, charAtt, placeAtt;
    std::unique_ptr<SliderAtt> predelayAtt, brightAtt, outputAtt, mixAtt, driftAtt, swingAtt, depthAtt, toneAtt,
                               hitLenAtt, sensAtt, hitsAtt;
    std::unique_ptr<ButtonAtt> soloAtt;

    std::vector<std::pair<juce::String, juce::Slider*>> sliderLabels;
    juce::Rectangle<int> readoutArea, hitLightArea, hintArea;

    std::vector<HistoryEntry> undoStack, redoStack;
    std::vector<float> laneClip;
    juce::Array<InkroomProcessor::UserPreset> userList;
    int userPresetId = 0;
    int shapeRepeat = 2, shapeRotate = 0;
    bool expanded = false, savingPreset = false;
    int shownSpace = -1;
    int seenVersion = -1;
    float smoothEnergy = 0.3f, smoothHit = 0.0f;

    friend struct InkroomEditorTest;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InkroomEditor)
};
