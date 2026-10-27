#include "PluginEditor.h"
#include "Presets.h"

using namespace ink;
using namespace ink::ui;

namespace
{
double rnd (double n)
{
    const double x = std::sin (n * 12.9898 + 78.233) * 43758.5453;
    return x - std::floor (x);
}
} // namespace

//==============================================================================
InkLook::InkLook()
{
    using namespace ui;
    setColour (juce::ResizableWindow::backgroundColourId, paper);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent.withAlpha (0.85f));
    setColour (juce::Slider::backgroundColourId, line);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, muted);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedTextColourId, accent);
    setColour (juce::TextButton::buttonColourId, panel2);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
}

void InkLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    const auto on = b.findColour (juce::TextButton::buttonOnColourId);

    if (b.getToggleState())
    {
        g.setColour (on.withAlpha (0.16f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (on);
    }
    else
    {
        g.setColour (ui::panel2.brighter (over ? 0.12f : 0.0f).darker (down ? 0.15f : 0.0f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (ui::line.brighter (over ? 0.3f : 0.0f));
    }
    g.drawRoundedRectangle (r, rad, 1.0f);
}

juce::Font InkLook::getTextButtonFont (juce::TextButton&, int)
{
    return uiFont (13.0f);
}

juce::Font InkLook::getComboBoxFont (juce::ComboBox&)
{
    return uiFont (13.5f);
}

juce::Font InkLook::getPopupMenuFont()
{
    return uiFont (14.0f);
}

juce::Label* InkLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (monoFont (11.5f));
    l->setJustificationType (juce::Justification::centredRight);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, ui::text);
    return l;
}

//==============================================================================
void RoomView::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto b = getLocalBounds().toFloat();
    g.setColour (paper);
    g.fillRoundedRectangle (b, 14.0f);

    const float w = b.getWidth(), h = b.getHeight(), m = std::min (w, h);
    const float R = m * (0.1f + 0.34f * st.size);
    const float wx = st.shape ? 0.35f + 1.3f * st.width : 1.0f;
    const float cx = b.getCentreX() + (st.pan - 0.5f) * w * 0.55f, cy = b.getCentreY();
    const auto col = violet.interpolatedWith (ember, juce::jlimit (0.0f, 1.0f, st.drive * st.depth * 1.2f));
    const float e = juce::jlimit (0.0f, 1.0f, st.energy) * (0.25f + 0.75f * st.level);
    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (int k = 0; k < 7; ++k)
    {
        const double a = juce::MathConstants<double>::twoPi * (rnd (k) + t * 0.04 * (1.0 + k * 0.3));
        const float d = R * 0.3f * (0.5f + 0.5f * (float) std::sin (t * 0.7 + k));
        const float x = cx + (float) std::cos (a) * d * wx, y = cy + (float) std::sin (a) * d * 0.8f;
        const float rr = R * (0.35f + 0.65f * (float) rnd (k * 9.7));
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.42f * e), x, y, col.withAlpha (0.0f), x + rr, y, true));
        g.fillEllipse (x - rr * wx, y - rr, rr * 2 * wx, rr * 2);
    }

    g.setColour (text.withAlpha (0.12f + 0.25f * st.level));
    juce::Path outline;
    if (! st.shape && st.type == 1)
        outline.addRectangle (cx - R * 1.05f, cy - R * 0.7f, R * 2.1f, R * 1.4f);
    else if (! st.shape && st.type == 2)
    {
        const float sw = R * 2.2f;
        const int turns = 9 + (int) std::round (st.size * 8);
        for (int k = 0; k <= 160; ++k)
        {
            const float u = (float) k / 160.0f;
            const float x = cx - sw / 2 + u * sw, y = cy + std::sin (u * turns * juce::MathConstants<float>::twoPi) * R * 0.18f;
            if (k == 0) outline.startNewSubPath (x, y); else outline.lineTo (x, y);
        }
    }
    else
        outline.addEllipse (cx - R * 1.05f * wx, cy - R * 0.95f, R * 2.1f * wx, R * 1.9f);
    g.strokePath (outline, juce::PathStrokeType (1.0f));

    g.setColour (text.withAlpha (0.07f));
    g.drawVerticalLine ((int) b.getCentreX(), b.getY() + 8, b.getBottom() - 8);
    g.setColour (line);
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);
}

//==============================================================================
namespace
{
juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Inkroom";
    o.filenameSuffix = ".settings";
    o.folderName = "Imagine Plugins";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}
} // namespace

InkroomEditor::InkroomEditor (InkroomProcessor& p)
    : AudioProcessorEditor (&p), proc (p), canvas (p)
{
    setLookAndFeel (&look);
    settings = std::make_unique<juce::PropertiesFile> (settingsOptions());

    addAndMakeVisible (content);
    content.setSize (designW, designH);
    content.painter = [this] (juce::Graphics& g) { paintContent (g); };

    content.addAndMakeVisible (room);
    content.addAndMakeVisible (canvas);
    canvas.onBeforeEdit = [this] {
        checkpoint();
        markCustom();
        // Editing a lane by hand stops it following another lane.
        if (proc.getLink (shownSpace, canvas.getLane()).source >= 0)
        {
            proc.setLink (shownSpace, canvas.getLane(), -1, false);
            refreshLaneButtons();
        }
    };
    canvas.onEdited = [this] { proc.propagateLinks (shownSpace); };
    canvas.valueText = [this] (int lane, float v) { return formatValue (lane, v); };

    for (auto* b : { &addSpaceBtn, &shapeSpaceBtn })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1003);
        content.addAndMakeVisible (b);
    }
    addSpaceBtn.onClick = [this] { if (addSpaceBtn.getToggleState()) proc.setSpace (AddSpace); };
    shapeSpaceBtn.onClick = [this] { if (shapeSpaceBtn.getToggleState()) proc.setSpace (ShapeSpace); };

    // Presets: factory ones first, then your own.
    presetBox.setTextWhenNothingSelected ("Custom shape");
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        if (id <= 0)
            return;
        if (id >= 1000)
        {
            const int idx = id - 1000;
            if (idx < userList.size() && id != userPresetId)
            {
                checkpoint();
                if (proc.loadUserPreset (userList[idx].file))
                {
                    userPresetId = id;
                    canvas.invalidate();
                    refreshLaneButtons();
                }
            }
            return;
        }
        if (id - 1 == proc.currentPreset[(size_t) shownSpace].load())
            return;
        checkpoint();
        userPresetId = 0;
        proc.loadPreset (shownSpace, id - 1);
        canvas.invalidate();
        refreshLaneButtons();
    };
    content.addAndMakeVisible (presetBox);

    nameEditor.setTextToShowWhenEmpty ("Name your preset", ui::muted);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, ui::panel2);
    nameEditor.setColour (juce::TextEditor::outlineColourId, ui::accent);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, ui::accent);
    nameEditor.setColour (juce::TextEditor::textColourId, ui::text);
    nameEditor.setFont (uiFont (13.5f));
    nameEditor.onReturnKey = [this] { finishSavingPreset (true); };
    nameEditor.onEscapeKey = [this] { finishSavingPreset (false); };
    content.addChildComponent (nameEditor);
    savePresetBtn.onClick = [this] { savingPreset ? finishSavingPreset (true) : beginSavingPreset(); };
    content.addAndMakeVisible (savePresetBtn);

    setupCombo (loopBox, "loop", loopAtt);
    setupCombo (typeBox, "type", typeAtt);
    setupCombo (charBox, "character", charAtt);
    setupCombo (placeBox, "placement", placeAtt);

    const std::pair<const char*, int> grids[] = { { "1/4", 1 }, { "1/8", 2 }, { "1/8T", 3 }, { "1/16", 4 }, { "1/16T", 6 }, { "1/32", 8 }, { "1/64", 16 } };
    for (auto& [name, id] : grids)
        gridBox.addItem (name, id);
    gridBox.setSelectedId (4, juce::dontSendNotification);
    gridBox.onChange = [this] { canvas.setGrid (gridBox.getSelectedId()); };
    content.addAndMakeVisible (gridBox);

    for (int l = 0; l < kMaxLanes; ++l)
    {
        auto& b = laneButtons[(size_t) l];
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1001);
        b.onClick = [this, l] { if (laneButtons[(size_t) l].getToggleState()) canvas.setLane (l); };
        content.addChildComponent (b);
    }

    for (auto* b : { &pointsBtn, &brushBtn })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1002);
        content.addAndMakeVisible (b);
    }
    pointsBtn.onClick = [this] { if (pointsBtn.getToggleState()) canvas.setMode (ShapeCanvas::Mode::Points); };
    brushBtn.onClick = [this] { if (brushBtn.getToggleState()) canvas.setMode (ShapeCanvas::Mode::Brush); };
    pointsBtn.setToggleState (true, juce::dontSendNotification);

    shapesBtn.onClick = [this] { showShapesMenu(); };
    laneMenuBtn.onClick = [this] { showLaneMenu(); };
    fitBtn.onClick = [this] { canvas.zoomToFit(); };
    barBtn.onClick = [this] { canvas.zoomToBar(); };
    beatBtn.onClick = [this] { canvas.zoomToBeat(); };
    expandBtn.onClick = [this] {
        expanded = ! expanded;
        expandBtn.setButtonText (expanded ? "Collapse" : "Expand");
        layoutContent();
        content.repaint();
    };

    smoothBtn.onClick = [this] {
        checkpoint(); markCustom();
        auto& L = shownLanes();
        const int l = canvas.getLane();
        proc.setLink (shownSpace, l, -1, false);
        std::vector<float> c ((size_t) kN);
        for (int pass = 0; pass < 3; ++pass)
        {
            for (int i = 0; i < kN; ++i) c[(size_t) i] = L.get (l, i);
            for (int i = 0; i < kN; ++i)
            {
                float s = 0.0f;
                for (int k = -24; k <= 24; ++k) s += c[(size_t) ((i + k + kN) % kN)];
                L.set (l, i, s / 49.0f);
            }
        }
        lanesChanged();
    };
    invertBtn.onClick = [this] {
        checkpoint(); markCustom();
        auto& L = shownLanes();
        const int l = canvas.getLane();
        proc.setLink (shownSpace, l, -1, false);
        for (int i = 0; i < kN; ++i) L.set (l, i, 1.0f - L.get (l, i));
        lanesChanged();
    };
    undoBtn.onClick = [this] { undo(); };
    redoBtn.onClick = [this] { redo(); };
    for (auto* b : { &shapesBtn, &laneMenuBtn, &fitBtn, &barBtn, &beatBtn, &expandBtn, &smoothBtn, &invertBtn, &undoBtn, &redoBtn })
        content.addAndMakeVisible (b);

    soloBtn.setClickingTogglesState (true);
    content.addAndMakeVisible (soloBtn);
    soloAtt = std::make_unique<ButtonAtt> (proc.apvts, "solo", soloBtn);

    setupSlider (predelaySl, "predelay", predelayAtt, "Pre-delay");
    setupSlider (brightSl, "bright", brightAtt, "Brightness");
    setupSlider (outputSl, "output", outputAtt, "Tail level");
    setupSlider (hitLenSl, "hitlen", hitLenAtt, "Hit length");
    setupSlider (sensSl, "sens", sensAtt, "Sensitivity");
    setupSlider (hitsSl, "hits", hitsAtt, "Hits");
    setupSlider (mixSl, "mix", mixAtt, "Mix");
    setupSlider (driftSl, "drift", driftAtt, "Drift");
    setupSlider (swingSl, "swing", swingAtt, "Swing");
    setupSlider (depthSl, "depth", depthAtt, "Depth");
    setupSlider (toneSl, "tone", toneAtt, "Tone");

    layoutContent();
    applySpace (proc.space());
    updateHistoryButtons();

    // Size: the session's size, else the last size you used, never larger than the screen.
    float scale = proc.editorScale.load();
    if (scale <= 0.0f)
        scale = (float) settings->getDoubleValue ("scale", 1.0);
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        scale = std::min (scale, std::min ((float) area.getWidth() * 0.92f / designW, (float) area.getHeight() * 0.88f / designH));
    }
    scale = juce::jlimit (0.6f, 1.5f, scale);

    setResizable (true, true);
    setResizeLimits ((int) (designW * 0.6f), (int) (designH * 0.6f), (int) (designW * 1.5f), (int) (designH * 1.5f));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designW / designH);
    setSize (juce::roundToInt (designW * scale), juce::roundToInt (designH * scale));
    startTimerHz (60);
}

InkroomEditor::~InkroomEditor()
{
    stopTimer();
    if (settings)
        settings->saveIfNeeded();
    setLookAndFeel (nullptr);
}

void InkroomEditor::resized()
{
    const float scale = (float) getWidth() / (float) designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.editorScale.store (scale);
    if (settings)
        settings->setValue ("scale", scale);
}

void InkroomEditor::setupCombo (juce::ComboBox& box, const juce::String& id, std::unique_ptr<ComboAtt>& att)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        box.addItemList (param->choices, 1);
    content.addAndMakeVisible (box);
    att = std::make_unique<ComboAtt> (proc.apvts, id, box);
}

void InkroomEditor::setupSlider (juce::Slider& s, const juce::String& id, std::unique_ptr<SliderAtt>& att, const juce::String& label)
{
    content.addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, true, 64, 20);
    att = std::make_unique<SliderAtt> (proc.apvts, id, s);
    sliderLabels.push_back ({ label, &s });
}

void InkroomEditor::applySpace (int sp)
{
    shownSpace = sp;
    const bool shape = sp == ShapeSpace;
    addSpaceBtn.setToggleState (! shape, juce::dontSendNotification);
    shapeSpaceBtn.setToggleState (shape, juce::dontSendNotification);

    canvas.setSpace (sp);
    refreshLaneButtons();
    userPresetId = 0;
    rebuildPresetBox();

    for (auto* c : std::initializer_list<juce::Component*> { &typeBox, &predelaySl, &brightSl, &outputSl, &placeBox })
        c->setVisible (! shape);
    for (auto* c : std::initializer_list<juce::Component*> { &hitLenSl, &sensSl, &hitsSl })
        c->setVisible (shape);

    seenVersion = -1;
    content.repaint();
}

void InkroomEditor::refreshLaneButtons()
{
    const int count = ui::laneCount (shownSpace);
    for (int l = 0; l < kMaxLanes; ++l)
    {
        auto& b = laneButtons[(size_t) l];
        b.setVisible (l < count);
        if (l < count)
        {
            const auto& info = ui::laneInfo (shownSpace, l);
            const bool linked = proc.getLink (shownSpace, l).source >= 0;
            b.setButtonText (juce::String (info.name) + (linked ? juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x94")) : juce::String()));
            b.setColour (juce::TextButton::buttonOnColourId, info.colour);
        }
        b.setToggleState (l == canvas.getLane(), juce::dontSendNotification);
    }
}

void InkroomEditor::rebuildPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& list = shownSpace == ShapeSpace ? shapePresets() : presets();
    for (size_t i = 0; i < list.size(); ++i)
        presetBox.addItem (list[i].name, (int) i + 1);

    userList = proc.userPresets (shownSpace);
    if (! userList.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("Your presets");
        for (int i = 0; i < userList.size(); ++i)
            presetBox.addItem (userList[i].name, 1000 + i);
    }
    const int cp = proc.currentPreset[(size_t) shownSpace].load();
    presetBox.setSelectedId (cp >= 0 ? cp + 1 : userPresetId, juce::dontSendNotification);
}

void InkroomEditor::beginSavingPreset()
{
    savingPreset = true;
    presetBox.setVisible (false);
    nameEditor.setText (userPresetId >= 1000 && userPresetId - 1000 < userList.size() ? userList[userPresetId - 1000].name : juce::String(), false);
    nameEditor.setVisible (true);
    nameEditor.grabKeyboardFocus();
    nameEditor.selectAll();
    savePresetBtn.setButtonText ("Done");
}

void InkroomEditor::finishSavingPreset (bool save)
{
    if (! savingPreset)
        return;
    savingPreset = false;
    const auto name = nameEditor.getText().trim();
    nameEditor.setVisible (false);
    presetBox.setVisible (true);
    savePresetBtn.setButtonText ("Save");
    if (save && name.isNotEmpty() && proc.saveUserPreset (name))
    {
        rebuildPresetBox();
        for (int i = 0; i < userList.size(); ++i)
            if (userList[i].name == juce::File::createLegalFileName (name))
            {
                userPresetId = 1000 + i;
                presetBox.setSelectedId (userPresetId, juce::dontSendNotification);
            }
    }
}

void InkroomEditor::checkpoint()
{
    undoStack.push_back ({ shownSpace, shownLanes().snapshot() });
    if (undoStack.size() > 80)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
    updateHistoryButtons();
}

void InkroomEditor::undo()
{
    if (undoStack.empty()) return;
    auto e = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back ({ e.space, proc.lanesFor (e.space).snapshot() });
    proc.lanesFor (e.space).restore (e.snap);
    proc.currentPreset[(size_t) e.space].store (-1);
    if (e.space != shownSpace)
        proc.setSpace (e.space);
    markCustom();
    updateHistoryButtons();
}

void InkroomEditor::redo()
{
    if (redoStack.empty()) return;
    auto e = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back ({ e.space, proc.lanesFor (e.space).snapshot() });
    proc.lanesFor (e.space).restore (e.snap);
    proc.currentPreset[(size_t) e.space].store (-1);
    if (e.space != shownSpace)
        proc.setSpace (e.space);
    markCustom();
    updateHistoryButtons();
}

void InkroomEditor::updateHistoryButtons()
{
    undoBtn.setEnabled (! undoStack.empty());
    redoBtn.setEnabled (! redoStack.empty());
}

void InkroomEditor::markCustom()
{
    proc.currentPreset[(size_t) shownSpace].store (-1);
    userPresetId = 0;
    presetBox.setSelectedId (0, juce::dontSendNotification);
}

void InkroomEditor::lanesChanged()
{
    proc.propagateLinks (shownSpace);
    shownLanes().version.fetch_add (1);
    canvas.invalidate();
    refreshLaneButtons();
}

//==============================================================================
// Shapes: rhythmic stamps, geometric outlines, and two-lane paths that draw in space.

void InkroomEditor::showShapesMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Rhythm");
    m.addItem (1, "Swell");
    m.addItem (2, "Fade");
    m.addItem (3, "Pulse");
    m.addItem (4, "Gate");
    m.addItem (5, "Wave");
    m.addItem (6, "Scatter");
    m.addSectionHeader ("Geometric");
    m.addItem (11, "Triangle");
    m.addItem (12, "Square");
    m.addItem (13, "Hexagon");
    m.addItem (14, "Circle");
    m.addItem (15, "Star");
    const juce::String pair = shownSpace == ShapeSpace ? "Pan + Width" : "Pan + Size";
    m.addSectionHeader ("Draw in space (" + pair + ")");
    m.addItem (21, "Orbit (circle)");
    m.addItem (22, "Triangle path");
    m.addItem (23, "Square path");
    m.addItem (24, "Hexagon path");
    m.addItem (25, "Figure eight");
    m.addSeparator();
    juce::PopupMenu rep, rot;
    const char* reps[] = { "Once per loop", "Every bar", "Every beat", "Every 8th", "Every 16th" };
    for (int i = 0; i < 5; ++i)
        rep.addItem (31 + i, reps[i], true, shapeRepeat == i);
    const char* rots[] = { "No shift", "Shift 1/4", "Shift 1/2", "Shift 3/4" };
    for (int i = 0; i < 4; ++i)
        rot.addItem (41 + i, rots[i], true, shapeRotate == i);
    m.addSubMenu (juce::String ("Repeat: ") + reps[shapeRepeat], rep);
    m.addSubMenu (juce::String ("Rotate: ") + rots[shapeRotate], rot);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapesBtn), [this] (int id) {
        if (id >= 31 && id <= 35) shapeRepeat = id - 31;
        else if (id >= 41 && id <= 44) shapeRotate = id - 41;
        else if (id > 0) applyShape (id);
    });
}

void InkroomEditor::applyShape (int id)
{
    checkpoint();
    markCustom();
    auto& L = shownLanes();
    const int beats = proc.loopBeats();
    const double repsTable[] = { 1.0, juce::jmax (1.0, beats / 4.0), (double) beats, beats * 2.0, beats * 4.0 };
    const double reps = repsTable[shapeRepeat];
    const double shift = shapeRotate * 0.25;
    auto cellPos = [reps, shift] (float p, int& cell) {
        const double x = p * reps - shift;
        cell = (int) std::floor (x);
        return (float) (x - std::floor (x));
    };
    const float lo = 0.05f, hi = 0.95f;
    auto span = [lo, hi] (float v) { return lo + (hi - lo) * juce::jlimit (0.0f, 1.0f, v); };
    const float twoPi = juce::MathConstants<float>::twoPi;

    if (id >= 21)
    {
        // Two lanes together, so the sound traces the shape in the room view.
        const int xLane = shownSpace == ShapeSpace ? (int) SPan : (int) Pan;
        const int yLane = shownSpace == ShapeSpace ? (int) Width : (int) Size;
        const int sides = id == 22 ? 3 : id == 23 ? 4 : id == 24 ? 6 : 0;
        for (int i = 0; i < kN; ++i)
        {
            int cell;
            const float t = cellPos ((float) i / kN, cell);
            float x, y;
            if (id == 21)      { x = std::cos (twoPi * t); y = std::sin (twoPi * t); }
            else if (id == 25) { x = std::sin (twoPi * t); y = std::sin (2 * twoPi * t) * 0.8f; }
            else
            {
                const float s = t * sides;
                const int k = (int) std::floor (s);
                const float f = s - (float) k;
                const float a0 = -juce::MathConstants<float>::halfPi + twoPi * (float) k / sides;
                const float a1 = -juce::MathConstants<float>::halfPi + twoPi * (float) (k + 1) / sides;
                x = std::cos (a0) + (std::cos (a1) - std::cos (a0)) * f;
                y = -(std::sin (a0) + (std::sin (a1) - std::sin (a0)) * f);
            }
            L.set (xLane, i, 0.5f + 0.42f * x);
            L.set (yLane, i, 0.5f + 0.40f * y);
        }
        proc.setLink (shownSpace, xLane, -1, false);
        proc.setLink (shownSpace, yLane, -1, false);
    }
    else
    {
        const int l = canvas.getLane();
        juce::Random r;
        const float seed = r.nextFloat() * 100.0f;
        for (int i = 0; i < kN; ++i)
        {
            int cell;
            const float u = cellPos ((float) i / kN, cell);
            float v = 0.5f;
            switch (id)
            {
                case 1:  v = 0.08f + 0.9f * std::pow (u, 2.2f); break;
                case 2:  v = 0.08f + 0.9f * std::pow (1.0f - u, 2.0f); break;
                case 3:  v = u < 0.5f ? 0.92f : 0.1f; break;
                case 4:  v = u < 0.28f ? 1.0f : 0.02f; break;
                case 5:  v = 0.5f + 0.42f * std::sin (twoPi * u); break;
                case 6:  { const float x = std::sin (((float) cell + seed) * 12.9898f) * 43758.5453f; v = 0.15f + 0.8f * (x - std::floor (x)); break; }
                case 11: v = span (u < 0.5f ? u * 2.0f : 2.0f - u * 2.0f); break;
                case 12: v = u < 0.5f ? hi : lo; break;
                case 13: v = span (u < 0.25f ? u * 4.0f : u < 0.75f ? 1.0f : (1.0f - u) * 4.0f); break;
                case 14: v = span (std::sqrt (std::max (0.0f, 1.0f - (2.0f * u - 1.0f) * (2.0f * u - 1.0f)))); break;
                case 15: { const float s = u * 5.0f; const float f = s - std::floor (s); v = span (std::pow (1.0f - std::abs (2.0f * f - 1.0f), 3.0f)); break; }
                default: break;
            }
            L.set (l, i, v);
        }
        proc.setLink (shownSpace, l, -1, false);
    }
    lanesChanged();
}

//==============================================================================
// Lane menu: copy, paste, paste inverted, and links between lanes.

void InkroomEditor::showLaneMenu()
{
    const int lane = canvas.getLane();
    const auto& info = ui::laneInfo (shownSpace, lane);
    const auto link = proc.getLink (shownSpace, lane);
    juce::PopupMenu m, follow, followInv;
    m.addSectionHeader (juce::String (info.name) + " lane");
    m.addItem (1, "Copy shape");
    m.addItem (2, "Paste", ! laneClip.empty());
    m.addItem (3, "Paste inverted", ! laneClip.empty());
    m.addSeparator();
    for (int l = 0; l < ui::laneCount (shownSpace); ++l)
    {
        if (l == lane) continue;
        const juce::String name = ui::laneInfo (shownSpace, l).name;
        follow.addItem (100 + l, name, true, link.source == l && ! link.inverted);
        followInv.addItem (200 + l, name, true, link.source == l && link.inverted);
    }
    m.addSubMenu ("Link to", follow);
    m.addSubMenu ("Link inverted to", followInv);
    m.addItem (300, link.source >= 0 ? juce::String ("Unlink from ") + ui::laneInfo (shownSpace, link.source).name : juce::String ("Unlink"), link.source >= 0);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&laneMenuBtn), [this, lane] (int id) {
        auto& L = shownLanes();
        if (id == 1)
        {
            laneClip.resize ((size_t) kN);
            for (int i = 0; i < kN; ++i) laneClip[(size_t) i] = L.get (lane, i);
        }
        else if ((id == 2 || id == 3) && laneClip.size() == (size_t) kN)
        {
            checkpoint(); markCustom();
            proc.setLink (shownSpace, lane, -1, false);
            for (int i = 0; i < kN; ++i) L.set (lane, i, id == 3 ? 1.0f - laneClip[(size_t) i] : laneClip[(size_t) i]);
            lanesChanged();
        }
        else if (id >= 100 && id < 300)
        {
            checkpoint(); markCustom();
            proc.setLink (shownSpace, lane, id % 100, id >= 200);
            lanesChanged();
        }
        else if (id == 300)
        {
            proc.setLink (shownSpace, lane, -1, false);
            refreshLaneButtons();
        }
    });
}

//==============================================================================

juce::String InkroomEditor::formatValue (int lane, float v) const
{
    const float depth = proc.apvts.getRawParameterValue ("depth")->load();
    auto pan = [] (float x) {
        const int n = juce::roundToInt ((x - 0.5f) * 200.0f);
        return std::abs (n) < 4 ? juce::String ("C") : juce::String (n < 0 ? "L " : "R ") + juce::String (std::abs (n));
    };
    auto levelDb = [] (float x) { return x < 0.01f ? juce::String ("-inf dB") : juce::String (40.0f * std::log10 (x), 1) + " dB"; };

    if (shownSpace == ShapeSpace)
    {
        switch (lane)
        {
            case Sustain:
            case Punch:
            {
                const float g = v <= 0.5f ? (v / 0.5f) * (v / 0.5f) : 1.0f + (v - 0.5f) * 2.0f;
                if (g < 0.01f) return "-inf dB";
                const float db = 20.0f * std::log10 (g);
                return (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
            }
            case SLevel: return levelDb (v);
            case Width:  return juce::String (juce::roundToInt (v * 200.0f)) + "%";
            case SPan:   return pan (v);
            default:     return juce::String (juce::roundToInt (v * depth * 100.0f)) + "%";
        }
    }

    const int type = (int) proc.apvts.getRawParameterValue ("type")->load();
    switch (lane)
    {
        case Level: return levelDb (v);
        case Size:
            if (type == 1) return v < 0.34f ? "Small" : v < 0.67f ? "Medium" : "Large";
            if (type == 2) return v < 0.34f ? "Short" : v < 0.67f ? "Mid" : "Long";
            return v < 0.15f ? "Closet" : v < 0.35f ? "Booth" : v < 0.55f ? "Studio" : v < 0.78f ? "Hall" : "Cathedral";
        case Length: return juce::String (0.35 * std::pow (4.5 / 0.35, (double) v), 1) + " s";
        case Pan:    return pan (v);
        default:     return juce::String (juce::roundToInt (v * depth * 100.0f)) + "%";
    }
}

void InkroomEditor::timerCallback()
{
    if (proc.space() != shownSpace)
        applySpace (proc.space());

    const int v = shownLanes().version.load();
    if (v != seenVersion)
    {
        seenVersion = v;
        canvas.invalidate();
        refreshLaneButtons();
        const int cp = proc.currentPreset[(size_t) shownSpace].load();
        presetBox.setSelectedId (cp >= 0 ? cp + 1 : userPresetId, juce::dontSendNotification);
    }

    const float target = proc.uiPlaying.load() || proc.uiEnergy.load() > 0.001f ? proc.uiEnergy.load() : 0.3f;
    smoothEnergy += (target - smoothEnergy) * 0.25f;
    smoothHit = std::max (proc.uiHit.load(), smoothHit * 0.85f);

    auto val = [this] (int l) { return proc.uiVals[(size_t) l].load(); };
    RoomView::State st;
    st.energy = smoothEnergy;
    st.depth = proc.apvts.getRawParameterValue ("depth")->load();
    st.type = (int) proc.apvts.getRawParameterValue ("type")->load();
    st.shape = shownSpace == ShapeSpace;
    if (st.shape)
    {
        st.level = val (SLevel); st.size = val (Sustain) * 0.95f; st.pan = val (SPan); st.drive = val (SDrive); st.width = val (Width);
    }
    else
    {
        st.level = val (Level); st.size = val (Size); st.pan = val (Pan); st.drive = val (Drive);
    }
    room.setState (st);
    if (! expanded)
    {
        room.repaint();
        content.repaint (readoutArea);
    }
    canvas.repaint();
    if (st.shape)
        content.repaint (hitLightArea);
}

void InkroomEditor::paintContent (juce::Graphics& g)
{
    const bool shape = shownSpace == ShapeSpace;
    g.fillAll (paper);

    g.setColour (muted);
    g.setFont (monoFont (10.5f));
    g.drawText ("IMAGINE PLUGINS  -  0.3", 20, 14, 300, 14, juce::Justification::centredLeft);
    g.setColour (text);
    g.setFont (displayFont (40.0f));
    g.drawText ("Ink", 20, 28, 70, 46, juce::Justification::centredLeft);
    const int inkW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (displayFont (40.0f), "Ink"));
    g.setColour (accent);
    g.drawText ("room", 20 + inkW, 28, 120, 46, juce::Justification::centredLeft);

    g.setColour (muted);
    g.setFont (monoFont (10.0f));
    g.drawText ("MODE", addSpaceBtn.getX(), addSpaceBtn.getY() - 16, 120, 14, juce::Justification::centredLeft);
    g.drawText ("PRESET", presetBox.getX(), presetBox.getY() - 16, 120, 14, juce::Justification::centredLeft);
    g.drawText ("SHAPE LOOP", loopBox.getX(), loopBox.getY() - 16, 120, 14, juce::Justification::centredLeft);

    const juce::Rectangle<float> unit (20, 84, 960, 404);
    g.setColour (panel);
    g.fillRoundedRectangle (unit, 18.0f);
    g.setColour (line);
    g.drawRoundedRectangle (unit.reduced (0.5f), 18.0f, 1.0f);

    if (! expanded)
    {
        const int count = laneCount (shownSpace);
        const int rowH = count > 5 ? 20 : 24;
        for (int l = 0; l < count; ++l)
        {
            const auto& info = laneInfo (shownSpace, l);
            const float v = proc.uiVals[(size_t) l].load();
            const int y = readoutArea.getY() + l * rowH;
            g.setColour (muted);
            g.setFont (uiFont (12.0f));
            g.drawText (info.name, readoutArea.getX(), y, 64, 18, juce::Justification::centredLeft);
            const juce::Rectangle<float> bar ((float) readoutArea.getX() + 66.0f, (float) y + 7.0f, 104.0f, 4.0f);
            g.setColour (line);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (info.colour);
            const bool isDrive = shape ? l == SDrive : l == Drive;
            const float shown = isDrive ? v * proc.apvts.getRawParameterValue ("depth")->load() : v;
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, shown)), 2.0f);
            g.setColour (text);
            g.setFont (monoFont (11.0f));
            g.drawText (formatValue (l, v), readoutArea.getRight() - 76, y, 76, 18, juce::Justification::centredRight);
        }
    }

    g.setColour (muted);
    g.setFont (uiFont (12.0f));
    g.drawText ("Click to add, drag to move, double-click to remove, Shift snaps. Hold Cmd to select several. Cmd + scroll to zoom.",
                hintArea, juce::Justification::centredLeft);

    const char* trays[] = { shape ? "Split" : "Space", "Motion", "Saturation" };
    for (int i = 0; i < 3; ++i)
    {
        const int x = 20 + i * 330;
        g.setColour (line);
        g.drawHorizontalLine (504, (float) x, (float) x + 300.0f);
        g.setColour (text);
        g.setFont (displayFont (21.0f));
        g.drawText (trays[i], x, 512, 200, 28, juce::Justification::centredLeft);
    }
    if (shape)
    {
        g.setColour (muted);
        g.setFont (uiFont (12.0f));
        g.drawText ("ring only", 680 + 104, 518, 120, 20, juce::Justification::centredLeft);
    }

    g.setColour (muted);
    g.setFont (uiFont (12.0f));
    for (auto& [label, s] : sliderLabels)
        if (s->isVisible())
            g.drawText (label, s->getX() - 84, s->getY(), 84, 26, juce::Justification::centredLeft);

    if (shape)
    {
        g.drawFittedText ("Each hit is split from the ring after it. Sustain, Width, Pan and Drive act on the ring only.",
                          20, 540, 300, 36, juce::Justification::topLeft, 2, 1.0f);
        const auto dot = hitLightArea.toFloat().removeFromLeft (12.0f).withSizeKeepingCentre (10.0f, 10.0f);
        g.setColour (line);
        g.fillEllipse (dot);
        g.setColour (accent.withAlpha (juce::jlimit (0.0f, 1.0f, smoothHit)));
        g.fillEllipse (dot);
        g.setColour (muted);
        g.drawText ("Lights up on each detected hit", hitLightArea.withTrimmedLeft (20), juce::Justification::centredLeft);
        g.drawFittedText ("The Drive lane rides the amount in time. Depth scales the whole lane.", 680, 652, 300, 36, juce::Justification::topLeft, 2, 1.0f);
        g.drawFittedText ("Mutes the hits and the dry signal so you hear only the shaped ring.", 350, 686, 300, 20, juce::Justification::topLeft, 1, 1.0f);
    }
    else
    {
        g.drawFittedText ("The Drive lane rides the amount in time. Depth scales the whole lane.", 680, 652, 300, 36, juce::Justification::topLeft, 2, 1.0f);
        g.drawFittedText ("Mutes the dry signal so you hear only the shaped room.", 350, 686, 300, 20, juce::Justification::topLeft, 1, 1.0f);
    }
}

void InkroomEditor::layoutContent()
{
    addSpaceBtn.setBounds (230, 40, 104, 30);
    shapeSpaceBtn.setBounds (338, 40, 116, 30);
    presetBox.setBounds (600, 40, 190, 30);
    nameEditor.setBounds (600, 40, 190, 30);
    savePresetBtn.setBounds (794, 40, 54, 30);
    loopBox.setBounds (856, 40, 124, 30);

    room.setVisible (! expanded);
    room.setBounds (36, 100, 250, 236);
    readoutArea = { 36, 348, 250, 124 };

    const int x0 = expanded ? 36 : 306;
    const int w = expanded ? 928 : 658;
    for (int l = 0; l < kMaxLanes; ++l)
        laneButtons[(size_t) l].setBounds (x0 + l * 96, 100, 90, 28);
    canvas.setBounds (x0, 136, w, 252);

    pointsBtn.setBounds (x0, 396, 66, 28);
    brushBtn.setBounds (x0 + 70, 396, 60, 28);
    gridBox.setBounds (x0 + 138, 396, 80, 28);
    shapesBtn.setBounds (x0 + 226, 396, 76, 28);
    laneMenuBtn.setBounds (x0 + 306, 396, 62, 28);
    fitBtn.setBounds (x0 + 376, 396, 44, 28);
    barBtn.setBounds (x0 + 424, 396, 44, 28);
    beatBtn.setBounds (x0 + 472, 396, 50, 28);
    expandBtn.setBounds (x0 + 530, 396, 76, 28);
    undoBtn.setBounds (x0, 430, 62, 28);
    redoBtn.setBounds (x0 + 66, 430, 62, 28);
    smoothBtn.setBounds (x0 + 136, 430, 70, 28);
    invertBtn.setBounds (x0 + 210, 430, 64, 28);
    hintArea = { x0, 462, w, 18 };

    auto row = [] (juce::Slider& s, int x, int y) { s.setBounds (x + 84, y, 216, 26); };

    typeBox.setBounds (20, 548, 150, 28);
    row (predelaySl, 20, 588);
    row (brightSl, 20, 622);
    row (outputSl, 20, 656);

    row (hitLenSl, 20, 582);
    row (sensSl, 20, 616);
    row (hitsSl, 20, 650);
    hitLightArea = { 20, 684, 300, 22 };

    row (mixSl, 350, 548);
    row (driftSl, 350, 582);
    row (swingSl, 350, 616);
    soloBtn.setBounds (350, 652, 100, 28);

    charBox.setBounds (680, 548, 120, 28);
    placeBox.setBounds (808, 548, 172, 28);
    row (depthSl, 680, 588);
    row (toneSl, 680, 622);
}
