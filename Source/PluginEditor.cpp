#include "PluginEditor.h"
#include "Presets.h"

using namespace ink;

namespace
{
juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
juce::Font monoFont (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}
juce::Font displayFont (float size)
{
    static const juce::String face = juce::Font::findAllTypefaceNames().contains ("Georgia")
                                         ? juce::String ("Georgia") : juce::Font::getDefaultSerifFontName();
    return juce::Font (juce::FontOptions (face, size, juce::Font::plain));
}
double rnd (double n)
{
    const double x = std::sin (n * 12.9898 + 78.233) * 43758.5453;
    return x - std::floor (x);
}
const char* sizeTop[] = { "Hall", "Large plate", "Long tank" };
const char* sizeBot[] = { "Closet", "Small plate", "Short tank" };
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
juce::Rectangle<float> ShapeCanvas::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (18.0f).withTrimmedBottom (4.0f);
}

float ShapeCanvas::phaseAtX (float x) const
{
    const auto p = plotArea();
    return juce::jlimit (0.0f, 0.9999f, (x - p.getX()) / p.getWidth());
}

float ShapeCanvas::valueAtY (float y) const
{
    const auto p = plotArea();
    return juce::jlimit (0.0f, 1.0f, 1.0f - (y - p.getY()) / p.getHeight());
}

float ShapeCanvas::snapX (float phase, bool snap) const
{
    if (! snap)
        return phase * kN;
    const float cells = (float) (proc.loopBeats() * grid);
    return juce::jmin ((float) kN - 0.25f, std::round (phase * cells) / cells * (float) kN);
}

void ShapeCanvas::buildNodes()
{
    // Simplify the drawn shape to as few points as keep it within a small error.
    std::array<float, kN> pts {};
    for (int i = 0; i < kN; ++i)
        pts[(size_t) i] = lanes().get (lane, i);

    const float xs = 3.0f, eps = 0.022f;
    std::vector<char> keep ((size_t) kN, 0);
    keep[0] = keep[(size_t) kN - 1] = 1;

    std::function<void (int, int)> seg = [&] (int a, int b) {
        const float ax = (float) a / kN * xs, ay = pts[(size_t) a], bx = (float) b / kN * xs, by = pts[(size_t) b];
        const float dx = bx - ax, dy = by - ay, len = std::max (1.0e-9f, std::hypot (dx, dy));
        int far = -1;
        float dmax = 0.0f;
        for (int i = a + 1; i < b; ++i)
        {
            const float d = std::abs (dy * ((float) i / kN * xs - ax) - dx * (pts[(size_t) i] - ay)) / len;
            if (d > dmax) { dmax = d; far = i; }
        }
        if (dmax > eps && far > 0)
        {
            keep[(size_t) far] = 1;
            seg (a, far);
            seg (far, b);
        }
    };
    seg (0, kN - 1);

    nodes.clear();
    for (int i = 0; i < kN; ++i)
        if (keep[(size_t) i])
            nodes.push_back ({ (float) i, pts[(size_t) i] });
    nodesValid = true;
}

void ShapeCanvas::writeNodes()
{
    if (nodes.empty())
        return;
    std::vector<Node> ext;
    ext.reserve (nodes.size() + 2);
    ext.push_back ({ nodes.back().x - (float) kN, nodes.back().y });
    for (auto& n : nodes)
        ext.push_back (n);
    ext.push_back ({ nodes.front().x + (float) kN, nodes.front().y });

    size_t j = 0;
    for (int i = 0; i < kN; ++i)
    {
        while (j + 2 < ext.size() && ext[j + 1].x < (float) i)
            ++j;
        const auto& a = ext[j];
        const auto& b = ext[j + 1];
        const float f = std::abs (b.x - a.x) < 1.0e-6f ? 1.0f : ((float) i - a.x) / (b.x - a.x);
        lanes().set (lane, i, a.y + (b.y - a.y) * f);
    }
}

int ShapeCanvas::nodeAt (juce::Point<float> pos) const
{
    const auto p = plotArea();
    int best = -1;
    float bd = 11.0f;
    for (size_t k = 0; k < nodes.size(); ++k)
    {
        const float x = p.getX() + nodes[k].x / kN * p.getWidth();
        const float y = p.getY() + (1.0f - nodes[k].y) * p.getHeight();
        const float d = pos.getDistanceFrom ({ x, y });
        if (d < bd) { bd = d; best = (int) k; }
    }
    return best;
}

void ShapeCanvas::brushAt (juce::Point<float> pos)
{
    const int idx = juce::jlimit (0, kN - 1, (int) (phaseAtX (pos.x) * kN));
    const float v = valueAtY (pos.y);
    if (lastIdx >= 0)
    {
        const int n = std::abs (idx - lastIdx), dir = idx > lastIdx ? 1 : -1;
        for (int k = 0; k <= n; ++k)
            lanes().set (lane, lastIdx + k * dir, lastV + (v - lastV) * (n ? (float) k / n : 1.0f));
    }
    else
        lanes().set (lane, idx, v);
    lastIdx = idx;
    lastV = v;
    nodesValid = false;
}

void ShapeCanvas::mouseDown (const juce::MouseEvent& e)
{
    if (onBeforeEdit)
        onBeforeEdit();

    if (mode == Mode::Brush)
    {
        lastIdx = -1;
        brushAt (e.position);
        repaint();
        return;
    }

    if (! nodesValid)
        buildNodes();
    int k = nodeAt (e.position);
    if (k < 0)
    {
        const float x = snapX (phaseAtX (e.position.x), e.mods.isShiftDown());
        const float v = valueAtY (e.position.y);
        auto it = std::find_if (nodes.begin(), nodes.end(), [x] (const Node& n) { return n.x > x; });
        k = (int) std::distance (nodes.begin(), it);
        nodes.insert (it, { x, v });
        writeNodes();
    }
    drag = hover = k;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void ShapeCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (mode == Mode::Brush)
    {
        brushAt (e.position);
        repaint();
        return;
    }
    if (drag < 0 || drag >= (int) nodes.size())
        return;

    const size_t k = (size_t) drag;
    const float lo = k > 0 ? nodes[k - 1].x + 0.25f : 0.0f;
    const float hi = k + 1 < nodes.size() ? nodes[k + 1].x - 0.25f : (float) kN - 0.25f;
    nodes[k].x = juce::jlimit (lo, juce::jmax (lo, hi), snapX (phaseAtX (e.position.x), e.mods.isShiftDown()));
    nodes[k].y = valueAtY (e.position.y);
    writeNodes();
    repaint();
}

void ShapeCanvas::mouseUp (const juce::MouseEvent&)
{
    drag = -1;
    lastIdx = -1;
}

void ShapeCanvas::mouseMove (const juce::MouseEvent& e)
{
    if (mode != Mode::Points)
    {
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        return;
    }
    if (! nodesValid)
        buildNodes();
    const int h = nodeAt (e.position);
    if (h != hover)
    {
        hover = h;
        repaint();
    }
    setMouseCursor (hover >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void ShapeCanvas::mouseExit (const juce::MouseEvent&)
{
    if (drag < 0)
        hover = -1;
}

void ShapeCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (mode != Mode::Points || ! nodesValid)
        return;
    const int k = nodeAt (e.position);
    if (k < 0 || nodes.size() <= 2)
        return;
    if (onBeforeEdit)
        onBeforeEdit();
    nodes.erase (nodes.begin() + k);
    writeNodes();
    hover = drag = -1;
    repaint();
}

void ShapeCanvas::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (paper);
    g.fillRoundedRectangle (bounds, 12.0f);

    const auto p = plotArea();
    const int beats = proc.loopBeats();
    const int cells = beats * grid;

    g.setColour (text.withAlpha (0.05f));
    for (int c = 0; c < cells; ++c)
        g.drawVerticalLine ((int) (p.getX() + (float) c / cells * p.getWidth()), p.getY(), p.getBottom());

    g.setFont (monoFont (10.5f));
    for (int b = 0; b < beats; ++b)
    {
        const bool bar = b % 4 == 0;
        const float x = p.getX() + (float) b / beats * p.getWidth();
        g.setColour (text.withAlpha (bar ? 0.22f : 0.11f));
        g.drawVerticalLine ((int) x, p.getY() - 4.0f, p.getBottom());
        g.setColour (text.withAlpha (bar ? 0.7f : 0.38f));
        const juce::String label = bar ? juce::String (b / 4 + 1) : juce::String (b / 4 + 1) + "." + juce::String (b % 4 + 1);
        g.drawText (label, juce::Rectangle<float> (x + 4.0f, 2.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    }
    g.setColour (text.withAlpha (0.06f));
    g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());

    auto yOf = [&p] (float v) { return p.getY() + (1.0f - v) * p.getHeight(); };
    auto lanePath = [&] (int l) {
        juce::Path path;
        for (int i = 0; i <= kN; ++i)
        {
            const float x = p.getX() + (float) i / kN * p.getWidth();
            const float y = yOf (lanes().get (l, i % kN));
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        return path;
    };

    for (int l = 0; l < ui::laneCount (space); ++l)
    {
        if (l == lane) continue;
        g.setColour (ui::laneInfo (space, l).colour.withAlpha (0.28f));
        g.strokePath (lanePath (l), juce::PathStrokeType (1.2f));
    }

    const auto col = ui::laneInfo (space, lane).colour;
    auto path = lanePath (lane);
    auto fill = path;
    fill.lineTo (p.getRight(), p.getBottom());
    fill.lineTo (p.getX(), p.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.32f), 0.0f, p.getY(), col.withAlpha (0.02f), 0.0f, p.getBottom(), false));
    g.fillPath (fill);

    const juce::PathStrokeType::JointStyle js = juce::PathStrokeType::curved;
    const juce::PathStrokeType::EndCapStyle ec = juce::PathStrokeType::rounded;
    g.setColour (col.withAlpha (0.16f)); g.strokePath (path, juce::PathStrokeType (9.0f, js, ec));
    g.setColour (col.withAlpha (0.35f)); g.strokePath (path, juce::PathStrokeType (4.5f, js, ec));
    g.setColour (col);                   g.strokePath (path, juce::PathStrokeType (2.0f, js, ec));

    if (mode == Mode::Points)
    {
        if (! nodesValid)
            buildNodes();
        for (size_t k = 0; k < nodes.size(); ++k)
        {
            const bool on = (int) k == hover;
            const float r = on ? 6.5f : 4.5f;
            const float x = p.getX() + nodes[k].x / kN * p.getWidth(), y = yOf (nodes[k].y);
            g.setColour (on ? col : paper);
            g.fillEllipse (x - r, y - r, r * 2, r * 2);
            g.setColour (col);
            g.drawEllipse (x - r, y - r, r * 2, r * 2, 1.8f);
        }
    }

    if (proc.uiPlaying.load())
    {
        const float ph = proc.uiPhase.load();
        const float x = p.getX() + ph * p.getWidth();
        g.setColour (text.withAlpha (0.75f));
        g.drawVerticalLine ((int) x, p.getY() - 4.0f, p.getBottom());
        const float y = yOf (lanes().sample (lane, ph));
        g.setColour (col);
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
    }

    // Axis labels for the selected lane.
    const int type = juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("type")->load());
    const auto& info = ui::laneInfo (space, lane);
    const bool sizeLane = space == AddSpace && lane == Size;
    const juce::String top = sizeLane ? sizeTop[type] : info.top, bot = sizeLane ? sizeBot[type] : info.bot;
    g.setFont (monoFont (10.0f));
    g.setColour (muted);
    g.drawText (top.toUpperCase(), juce::Rectangle<float> (p.getX() + 8, p.getY() + 4, 160, 14), juce::Justification::centredLeft);
    g.drawText (bot.toUpperCase(), juce::Rectangle<float> (p.getX() + 8, p.getBottom() - 18, 160, 14), juce::Justification::centredLeft);

    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);
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
    canvas.onBeforeEdit = [this] { checkpoint(); markCustom(); };

    for (auto* b : { &addSpaceBtn, &shapeSpaceBtn })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1003);
        content.addAndMakeVisible (b);
    }
    addSpaceBtn.onClick = [this] { if (addSpaceBtn.getToggleState()) proc.setSpace (AddSpace); };
    shapeSpaceBtn.onClick = [this] { if (shapeSpaceBtn.getToggleState()) proc.setSpace (ShapeSpace); };

    presetBox.setTextWhenNothingSelected ("Custom shape");
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        if (id <= 0 || id - 1 == proc.currentPreset[(size_t) shownSpace].load())
            return;
        checkpoint();
        proc.loadPreset (shownSpace, id - 1);
        canvas.invalidate();
    };
    content.addAndMakeVisible (presetBox);

    setupCombo (loopBox, "loop", loopAtt);
    setupCombo (typeBox, "type", typeAtt);
    setupCombo (charBox, "character", charAtt);
    setupCombo (placeBox, "placement", placeAtt);

    gridBox.addItem ("1/4", 1);
    gridBox.addItem ("1/8", 2);
    gridBox.addItem ("1/16", 4);
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

    const char* stampNames[] = { "Swell", "Fade", "Pulse", "Gate", "Wave" };
    for (int i = 0; i < 5; ++i)
    {
        stampBtns[(size_t) i].setButtonText (stampNames[i]);
        stampBtns[(size_t) i].onClick = [this, i] { stamp (i); };
        content.addAndMakeVisible (stampBtns[(size_t) i]);
    }

    smoothBtn.onClick = [this] {
        checkpoint(); markCustom();
        auto& L = shownLanes();
        const int l = canvas.getLane();
        for (int pass = 0; pass < 3; ++pass)
        {
            std::array<float, kN> c {};
            for (int i = 0; i < kN; ++i) c[(size_t) i] = L.get (l, i);
            for (int i = 0; i < kN; ++i)
            {
                float s = 0.0f;
                for (int k = -3; k <= 3; ++k) s += c[(size_t) ((i + k + kN) % kN)];
                L.set (l, i, s / 7.0f);
            }
        }
        canvas.invalidate();
    };
    invertBtn.onClick = [this] {
        checkpoint(); markCustom();
        auto& L = shownLanes();
        const int l = canvas.getLane();
        for (int i = 0; i < kN; ++i) L.set (l, i, 1.0f - L.get (l, i));
        canvas.invalidate();
    };
    undoBtn.onClick = [this] { undo(); };
    redoBtn.onClick = [this] { redo(); };
    for (auto* b : { &smoothBtn, &invertBtn, &undoBtn, &redoBtn })
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

    const int count = ui::laneCount (sp);
    for (int l = 0; l < kMaxLanes; ++l)
    {
        auto& b = laneButtons[(size_t) l];
        b.setVisible (l < count);
        if (l < count)
        {
            const auto& info = ui::laneInfo (sp, l);
            b.setButtonText (info.name);
            b.setColour (juce::TextButton::buttonOnColourId, info.colour);
        }
        b.setToggleState (l == 0, juce::dontSendNotification);
    }
    canvas.setSpace (sp);

    presetBox.clear (juce::dontSendNotification);
    const auto& list = shape ? shapePresets() : presets();
    for (size_t i = 0; i < list.size(); ++i)
        presetBox.addItem (list[i].name, (int) i + 1);
    const int cp = proc.currentPreset[(size_t) sp].load();
    presetBox.setSelectedId (cp >= 0 ? cp + 1 : 0, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &typeBox, &predelaySl, &brightSl, &outputSl, &placeBox })
        c->setVisible (! shape);
    for (auto* c : std::initializer_list<juce::Component*> { &hitLenSl, &sensSl, &hitsSl })
        c->setVisible (shape);

    seenVersion = -1;
    content.repaint();
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
    presetBox.setSelectedId (0, juce::dontSendNotification);
}

void InkroomEditor::stamp (int which)
{
    checkpoint();
    markCustom();
    const int l = canvas.getLane();
    const int cells = proc.loopBeats() * canvas.getGrid();
    shownLanes().fill (l, [which, cells] (float p) {
        if (which == 4)
            return 0.5f + 0.42f * std::sin (juce::MathConstants<float>::twoPi * p);
        const float x = p * (float) cells, u = x - std::floor (x);
        switch (which)
        {
            case 0:  return 0.08f + 0.9f * std::pow (u, 2.2f);
            case 1:  return 0.08f + 0.9f * std::pow (1.0f - u, 2.0f);
            case 2:  return u < 0.5f ? 0.92f : 0.1f;
            default: return u < 0.28f ? 1.0f : 0.02f;
        }
    });
    canvas.invalidate();
}

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
        const int cp = proc.currentPreset[(size_t) shownSpace].load();
        presetBox.setSelectedId (cp >= 0 ? cp + 1 : 0, juce::dontSendNotification);
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
    room.repaint();
    canvas.repaint();
    content.repaint (readoutArea);
    if (st.shape)
        content.repaint (hitLightArea);
}

void InkroomEditor::paintContent (juce::Graphics& g)
{
    using namespace ui;
    const bool shape = shownSpace == ShapeSpace;
    g.fillAll (paper);

    g.setColour (muted);
    g.setFont (monoFont (10.5f));
    g.drawText ("IMAGINE PLUGINS  -  0.2", 20, 14, 300, 14, juce::Justification::centredLeft);
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

    // Readouts.
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

    g.setColour (muted);
    g.setFont (uiFont (12.0f));
    g.drawText ("Points: drag to move, click to add, double-click to remove, Shift snaps. Brush: paint freehand.",
                306, 460, 660, 18, juce::Justification::centredLeft);

    // Trays.
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
    presetBox.setBounds (620, 40, 220, 30);
    loopBox.setBounds (856, 40, 124, 30);

    room.setBounds (36, 100, 250, 236);
    readoutArea = { 36, 348, 250, 124 };

    for (int l = 0; l < kMaxLanes; ++l)
        laneButtons[(size_t) l].setBounds (306 + l * 96, 100, 90, 28);
    canvas.setBounds (306, 136, 658, 250);

    pointsBtn.setBounds (306, 394, 70, 28);
    brushBtn.setBounds (380, 394, 62, 28);
    gridBox.setBounds (452, 394, 74, 28);
    for (int i = 0; i < 5; ++i)
        stampBtns[(size_t) i].setBounds (540 + i * 62, 394, 58, 28);
    undoBtn.setBounds (306, 428, 62, 28);
    redoBtn.setBounds (372, 428, 62, 28);
    smoothBtn.setBounds (442, 428, 70, 28);
    invertBtn.setBounds (516, 428, 64, 28);

    auto row = [] (juce::Slider& s, int x, int y) { s.setBounds (x + 84, y, 216, 26); };

    // Space tray (Add space).
    typeBox.setBounds (20, 548, 150, 28);
    row (predelaySl, 20, 588);
    row (brightSl, 20, 622);
    row (outputSl, 20, 656);

    // Split tray (Shape space).
    row (hitLenSl, 20, 582);
    row (sensSl, 20, 616);
    row (hitsSl, 20, 650);
    hitLightArea = { 20, 684, 300, 22 };

    // Motion tray.
    row (mixSl, 350, 548);
    row (driftSl, 350, 582);
    row (swingSl, 350, 616);
    soloBtn.setBounds (350, 652, 100, 28);

    // Saturation tray.
    charBox.setBounds (680, 548, 120, 28);
    placeBox.setBounds (808, 548, 172, 28);
    row (depthSl, 680, 588);
    row (toneSl, 680, 622);
}
