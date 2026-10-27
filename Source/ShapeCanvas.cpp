#include "ShapeCanvas.h"

using namespace ink;

namespace
{
const char* sizeTop[] = { "Hall", "Large plate", "Long tank" };
const char* sizeBot[] = { "Closet", "Small plate", "Short tank" };
}

ShapeCanvas::ShapeCanvas (InkroomProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void ShapeCanvas::setSpace (int s) { space = s; lane = 0; invalidate(); repaint(); }
void ShapeCanvas::setLane (int l)  { lane = l; invalidate(); repaint(); }
void ShapeCanvas::setMode (Mode m) { mode = m; invalidate(); repaint(); }
void ShapeCanvas::setGrid (int g)  { grid = juce::jmax (1, g); repaint(); }

void ShapeCanvas::invalidate()
{
    nodes.clear();
    selected.clear();
    nodesValid = false;
    hover = anchor = -1;
    dragKind = Drag::None;
}

//==============================================================================
// Geometry. Node positions are in lane steps (0 .. kN); the view is a slice of the loop.

juce::Rectangle<float> ShapeCanvas::overviewArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 0.0f).withTrimmedTop (5.0f).withHeight (8.0f);
}

juce::Rectangle<float> ShapeCanvas::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (32.0f).withTrimmedBottom (4.0f);
}

float ShapeCanvas::xOfIndex (float idx) const
{
    const auto p = plotArea();
    return p.getX() + (float) (((double) idx / kN - viewStart) / viewSpan) * p.getWidth();
}

float ShapeCanvas::indexAtX (float x) const
{
    const auto p = plotArea();
    const double ph = viewStart + (double) (x - p.getX()) / p.getWidth() * viewSpan;
    return (float) juce::jlimit (0.0, (double) kN - 0.001, ph * kN);
}

float ShapeCanvas::yOf (float v) const
{
    const auto p = plotArea();
    return p.getY() + (1.0f - v) * p.getHeight();
}

float ShapeCanvas::valueAtY (float y) const
{
    const auto p = plotArea();
    return juce::jlimit (0.0f, 1.0f, 1.0f - (y - p.getY()) / p.getHeight());
}

int ShapeCanvas::displayDivisions() const
{
    // The grid gets finer as you zoom in, keeping lines at least ~24 px apart.
    const double pxPerBeat = plotArea().getWidth() / (proc.loopBeats() * viewSpan);
    int div = grid;
    while (pxPerBeat / (div * 2) >= 24.0 && div < 128)
        div *= 2;
    return div;
}

float ShapeCanvas::snapIndex (float idx, bool snap) const
{
    if (! snap)
        return idx;
    const double cells = (double) proc.loopBeats() * displayDivisions();
    return (float) juce::jmin ((double) kN - 0.25, std::round (idx / kN * cells) / cells * kN);
}

void ShapeCanvas::setView (double start, double span)
{
    const double minSpan = 1.0 / (proc.loopBeats() * 8.0);
    viewSpan = juce::jlimit (minSpan, 1.0, span);
    viewStart = juce::jlimit (0.0, 1.0 - viewSpan, start);
    repaint();
}

void ShapeCanvas::zoomAround (float x, double factor)
{
    const auto p = plotArea();
    const double rel = juce::jlimit (0.0, 1.0, (double) (x - p.getX()) / p.getWidth());
    const double at = viewStart + rel * viewSpan;
    const double newSpan = juce::jlimit (1.0 / (proc.loopBeats() * 8.0), 1.0, viewSpan * factor);
    setView (at - rel * newSpan, newSpan);
}

void ShapeCanvas::zoomToFit() { setView (0.0, 1.0); }

void ShapeCanvas::zoomToBar()
{
    const double beats = proc.loopBeats(), span = juce::jmin (1.0, 4.0 / beats);
    const double centre = viewStart + viewSpan * 0.5;
    setView (std::floor (centre / span) * span, span);
}

void ShapeCanvas::zoomToBeat()
{
    const double span = 1.0 / proc.loopBeats();
    const double centre = viewStart + viewSpan * 0.5;
    setView (std::floor (centre / span) * span, span);
}

//==============================================================================
// Points

void ShapeCanvas::buildNodes()
{
    // Simplify the drawn shape to as few points as keep it within a small error.
    std::vector<float> pts ((size_t) kN);
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
    selected.assign (nodes.size(), 0);
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
    int best = -1;
    float bd = 11.0f;
    for (size_t k = 0; k < nodes.size(); ++k)
    {
        const float d = pos.getDistanceFrom ({ xOfIndex (nodes[k].x), yOf (nodes[k].y) });
        if (d < bd) { bd = d; best = (int) k; }
    }
    return best;
}

int ShapeCanvas::selectedCount() const
{
    return (int) std::count (selected.begin(), selected.end(), (char) 1);
}

void ShapeCanvas::clearSelection()
{
    std::fill (selected.begin(), selected.end(), (char) 0);
}

void ShapeCanvas::brushAt (juce::Point<float> pos)
{
    const int idx = juce::jlimit (0, kN - 1, (int) indexAtX (pos.x));
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

void ShapeCanvas::deleteSelection()
{
    if (selectedCount() == 0 || (int) nodes.size() - selectedCount() < 2)
        return;
    if (onBeforeEdit) onBeforeEdit();
    std::vector<Node> keep;
    for (size_t k = 0; k < nodes.size(); ++k)
        if (! selected[k])
            keep.push_back (nodes[k]);
    nodes = keep;
    selected.assign (nodes.size(), 0);
    hover = -1;
    writeNodes();
    edited();
}

void ShapeCanvas::nudgeSelection (float dxIdx, float dy)
{
    if (selectedCount() == 0)
        return;
    if (onBeforeEdit) onBeforeEdit();
    for (size_t k = 0; k < nodes.size(); ++k)
    {
        if (! selected[k]) continue;
        const float lo = k > 0 ? nodes[k - 1].x + 0.25f : 0.0f;
        const float hi = k + 1 < nodes.size() ? nodes[k + 1].x - 0.25f : (float) kN - 0.25f;
        nodes[k].x = juce::jlimit (lo, juce::jmax (lo, hi), nodes[k].x + dxIdx);
        nodes[k].y = juce::jlimit (0.0f, 1.0f, nodes[k].y + dy);
    }
    writeNodes();
    edited();
}

void ShapeCanvas::edited()
{
    if (onEdited) onEdited();
    repaint();
}

juce::String ShapeCanvas::positionText (float idx) const
{
    const double beats = (double) idx / kN * proc.loopBeats();
    const int bar = (int) std::floor (beats / 4.0) + 1;
    const int beat = (int) std::floor (std::fmod (beats, 4.0)) + 1;
    const double sixteenths = (beats - std::floor (beats)) * 4.0;
    const int six = (int) std::floor (sixteenths) + 1;
    const double ms = (sixteenths - std::floor (sixteenths)) * 0.25 * 60000.0 / juce::jmax (20.0f, proc.uiBpm.load());
    juce::String s = juce::String (bar) + "." + juce::String (beat) + "." + juce::String (six);
    if (ms >= 0.5)
        s << " +" << juce::roundToInt (ms) << " ms";
    return s;
}

//==============================================================================
// Mouse and keys

void ShapeCanvas::updateCursor (const juce::ModifierKeys& mods)
{
    if (mods.isCommandDown())
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else if (mode == Mode::Points && hover >= 0)
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void ShapeCanvas::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto pos = e.position;
    dragStart = pos;

    if (pos.y < plotArea().getY() - 14.0f)
    {
        dragKind = Drag::Overview;
        const auto o = overviewArea();
        setView ((pos.x - o.getX()) / o.getWidth() - viewSpan * 0.5, viewSpan);
        return;
    }

    if (mode == Mode::Brush && ! e.mods.isCommandDown())
    {
        if (onBeforeEdit) onBeforeEdit();
        dragKind = Drag::Brush;
        lastIdx = -1;
        brushAt (pos);
        edited();
        return;
    }

    if (! nodesValid)
        buildNodes();
    const int k = nodeAt (pos);

    if (e.mods.isCommandDown())
    {
        // Selection mode: click toggles a point, drag draws a box.
        if (k >= 0)
        {
            selected[(size_t) k] = selected[(size_t) k] ? 0 : 1;
            dragKind = Drag::None;
        }
        else
        {
            dragKind = Drag::Marquee;
            marquee = { pos, pos };
        }
        repaint();
        return;
    }

    if (k >= 0 && selected[(size_t) k] && selectedCount() > 1)
    {
        if (onBeforeEdit) onBeforeEdit();
        dragKind = e.mods.isAltDown() ? Drag::Scale : Drag::Group;
        anchor = k;
        dragOrig = nodes;
        return;
    }

    clearSelection();
    if (onBeforeEdit) onBeforeEdit();
    if (k >= 0)
        anchor = k;
    else
    {
        const float x = snapIndex (indexAtX (pos.x), e.mods.isShiftDown());
        const float v = valueAtY (pos.y);
        auto it = std::find_if (nodes.begin(), nodes.end(), [x] (const Node& n) { return n.x > x; });
        anchor = (int) std::distance (nodes.begin(), it);
        nodes.insert (it, { x, v });
        selected.insert (selected.begin() + anchor, (char) 0);
        writeNodes();
        edited();
    }
    selected[(size_t) anchor] = 1;
    hover = anchor;
    dragKind = Drag::Point;
    repaint();
}

void ShapeCanvas::mouseDrag (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    const auto p = plotArea();

    switch (dragKind)
    {
        case Drag::Overview:
        {
            const auto o = overviewArea();
            setView ((pos.x - o.getX()) / o.getWidth() - viewSpan * 0.5, viewSpan);
            break;
        }
        case Drag::Brush:
            brushAt (pos);
            edited();
            break;

        case Drag::Marquee:
            marquee = juce::Rectangle<float> (dragStart, pos);
            repaint();
            break;

        case Drag::Point:
        {
            if (anchor < 0 || anchor >= (int) nodes.size()) break;
            const size_t k = (size_t) anchor;
            const float lo = k > 0 ? nodes[k - 1].x + 0.25f : 0.0f;
            const float hi = k + 1 < nodes.size() ? nodes[k + 1].x - 0.25f : (float) kN - 0.25f;
            nodes[k].x = juce::jlimit (lo, juce::jmax (lo, hi), snapIndex (indexAtX (pos.x), e.mods.isShiftDown()));
            nodes[k].y = valueAtY (pos.y);
            writeNodes();
            edited();
            break;
        }

        case Drag::Group:
        {
            float dx = (pos.x - dragStart.x) / p.getWidth() * (float) viewSpan * kN;
            float dy = -(pos.y - dragStart.y) / p.getHeight();
            if (e.mods.isShiftDown() && anchor >= 0)
                dx = snapIndex (dragOrig[(size_t) anchor].x + dx, true) - dragOrig[(size_t) anchor].x;

            // Keep the group in order with the points around it, and inside the canvas.
            float dxMin = -1.0e9f, dxMax = 1.0e9f, dyMin = -1.0f, dyMax = 1.0f;
            for (size_t k = 0; k < dragOrig.size(); ++k)
            {
                if (! selected[k]) continue;
                const float x = dragOrig[k].x, y = dragOrig[k].y;
                const float lo = k == 0 ? 0.0f : (selected[k - 1] ? -1.0e9f : dragOrig[k - 1].x + 0.25f);
                const float hi = k + 1 == dragOrig.size() ? (float) kN - 0.25f : (selected[k + 1] ? 1.0e9f : dragOrig[k + 1].x - 0.25f);
                dxMin = std::max (dxMin, lo - x);
                dxMax = std::min (dxMax, hi - x);
                dyMin = std::max (dyMin, -y);
                dyMax = std::min (dyMax, 1.0f - y);
            }
            dx = juce::jlimit (dxMin, juce::jmax (dxMin, dxMax), dx);
            dy = juce::jlimit (dyMin, juce::jmax (dyMin, dyMax), dy);
            for (size_t k = 0; k < nodes.size(); ++k)
                if (selected[k])
                    nodes[k] = { dragOrig[k].x + dx, dragOrig[k].y + dy };
            writeNodes();
            edited();
            break;
        }

        case Drag::Scale:
        {
            // Option-drag: deepen or flatten the selection around its middle.
            float centre = 0.0f;
            int n = 0;
            for (size_t k = 0; k < dragOrig.size(); ++k)
                if (selected[k]) { centre += dragOrig[k].y; ++n; }
            centre /= (float) juce::jmax (1, n);
            const float factor = juce::jlimit (0.0f, 3.0f, 1.0f - (pos.y - dragStart.y) / p.getHeight() * 2.0f);
            for (size_t k = 0; k < nodes.size(); ++k)
                if (selected[k])
                    nodes[k].y = juce::jlimit (0.0f, 1.0f, centre + (dragOrig[k].y - centre) * factor);
            writeNodes();
            edited();
            break;
        }

        case Drag::None:
            break;
    }
}

void ShapeCanvas::mouseUp (const juce::MouseEvent& e)
{
    if (dragKind == Drag::Marquee)
    {
        const auto box = marquee;
        for (size_t k = 0; k < nodes.size(); ++k)
            if (box.contains (xOfIndex (nodes[k].x), yOf (nodes[k].y)))
                selected[k] = 1;
        marquee = {};
    }
    if (dragKind == Drag::Point)
        clearSelection();
    dragKind = Drag::None;
    lastIdx = -1;
    updateCursor (e.mods);
    repaint();
}

void ShapeCanvas::mouseMove (const juce::MouseEvent& e)
{
    if (mode == Mode::Points)
    {
        if (! nodesValid)
            buildNodes();
        const int h = nodeAt (e.position);
        if (h != hover)
        {
            hover = h;
            repaint();
        }
    }
    updateCursor (e.mods);
}

void ShapeCanvas::mouseExit (const juce::MouseEvent&)
{
    if (dragKind == Drag::None && hover >= 0)
    {
        hover = -1;
        repaint();
    }
}

void ShapeCanvas::modifierKeysChanged (const juce::ModifierKeys& mods)
{
    updateCursor (mods);
}

void ShapeCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (mode != Mode::Points || e.mods.isCommandDown() || ! nodesValid)
        return;
    const int k = nodeAt (e.position);
    if (k < 0 || nodes.size() <= 2)
        return;
    if (onBeforeEdit) onBeforeEdit();
    nodes.erase (nodes.begin() + k);
    selected.erase (selected.begin() + k);
    hover = anchor = -1;
    writeNodes();
    edited();
}

void ShapeCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomAround (e.position.x, std::exp (-w.deltaY * 2.0));
        return;
    }
    if (viewSpan < 1.0)
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : -w.deltaY;
        setView (viewStart + d * viewSpan * 0.6, viewSpan);
        return;
    }
    Component::mouseWheelMove (e, w);
}

void ShapeCanvas::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    if (scaleFactor > 0.0f)
        zoomAround (e.position.x, 1.0 / scaleFactor);
}

bool ShapeCanvas::keyPressed (const juce::KeyPress& key)
{
    if (mode != Mode::Points)
        return false;
    if (! nodesValid)
        buildNodes();

    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        std::fill (selected.begin(), selected.end(), (char) 1);
        repaint();
        return true;
    }
    if (selectedCount() == 0)
        return false;

    const bool fine = key.getModifiers().isShiftDown();
    const float step = (float) kN / ((float) proc.loopBeats() * displayDivisions());
    if (key.isKeyCode (juce::KeyPress::deleteKey) || key.isKeyCode (juce::KeyPress::backspaceKey)) { deleteSelection(); return true; }
    if (key.isKeyCode (juce::KeyPress::leftKey))  { nudgeSelection (-(fine ? step / 4 : step), 0.0f); return true; }
    if (key.isKeyCode (juce::KeyPress::rightKey)) { nudgeSelection (fine ? step / 4 : step, 0.0f); return true; }
    if (key.isKeyCode (juce::KeyPress::upKey))    { nudgeSelection (0.0f, fine ? 0.005f : 0.02f); return true; }
    if (key.isKeyCode (juce::KeyPress::downKey))  { nudgeSelection (0.0f, fine ? -0.005f : -0.02f); return true; }
    if (key.isKeyCode (juce::KeyPress::escapeKey)) { clearSelection(); repaint(); return true; }
    return false;
}

//==============================================================================
// Drawing

void ShapeCanvas::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (paper);
    g.fillRoundedRectangle (bounds, 12.0f);

    const auto p = plotArea();
    const int beats = proc.loopBeats();
    const auto& info = laneInfo (space, lane);
    const auto col = info.colour;

    // Overview of the whole loop, with the visible slice outlined.
    {
        const auto o = overviewArea();
        g.setColour (panel2);
        g.fillRoundedRectangle (o, 3.0f);
        juce::Path mini;
        for (int i = 0; i <= 120; ++i)
        {
            const float x = o.getX() + o.getWidth() * (float) i / 120.0f;
            const float y = o.getBottom() - lanes().sample (lane, (double) i / 120.0 * 0.9999) * o.getHeight();
            if (i == 0) mini.startNewSubPath (x, y); else mini.lineTo (x, y);
        }
        g.setColour (col.withAlpha (0.55f));
        g.strokePath (mini, juce::PathStrokeType (1.0f));
        if (viewSpan < 0.999)
        {
            const auto win = juce::Rectangle<float> (o.getX() + (float) viewStart * o.getWidth(), o.getY() - 1.0f,
                                                     (float) viewSpan * o.getWidth(), o.getHeight() + 2.0f);
            g.setColour (accent.withAlpha (0.18f));
            g.fillRoundedRectangle (win, 2.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.drawRoundedRectangle (win, 2.0f, 1.0f);
        }
    }

    g.saveState();
    g.reduceClipRegion (p.toNearestInt().withTop ((int) p.getY() - 16));

    // Grid: subdivisions, then beats, then bars, each stronger than the last. Triplets are tinted.
    const int div = displayDivisions();
    const bool triplet = div % 3 == 0;
    const double cells = (double) beats * div;
    const int c0 = (int) std::floor (viewStart * cells), c1 = (int) std::ceil ((viewStart + viewSpan) * cells);
    for (int c = c0; c <= c1; ++c)
    {
        if (c % div == 0) continue;
        const float x = xOfIndex ((float) (c / cells * kN));
        g.setColour (triplet ? amber.withAlpha (0.16f) : text.withAlpha (0.10f));
        g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
    }
    g.setFont (monoFont (10.5f));
    const int b0 = (int) std::floor (viewStart * beats), b1 = (int) std::ceil ((viewStart + viewSpan) * beats);
    for (int b = b0; b <= b1 && b <= beats; ++b)
    {
        const bool bar = b % 4 == 0;
        const float x = xOfIndex ((float) b / beats * kN);
        g.setColour (text.withAlpha (bar ? 0.38f : 0.22f));
        g.drawVerticalLine ((int) x, p.getY() - 4.0f, p.getBottom());
        if (b < beats)
        {
            g.setColour (text.withAlpha (bar ? 0.75f : 0.45f));
            const juce::String label = bar ? juce::String (b / 4 + 1) : juce::String (b / 4 + 1) + "." + juce::String (b % 4 + 1);
            g.drawText (label, juce::Rectangle<float> (x + 4.0f, p.getY() - 16.0f, 50.0f, 14.0f), juce::Justification::centredLeft);
        }
    }
    g.setColour (text.withAlpha (0.07f));
    g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());

    // Lanes, sampled per pixel so zooming shows full detail.
    auto lanePath = [&] (int l) {
        juce::Path path;
        const int w = (int) p.getWidth();
        for (int px = 0; px <= w; px += 2)
        {
            const float x = p.getX() + (float) px;
            const float y = yOf (lanes().sample (l, indexAtX (x) / kN));
            if (px == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        return path;
    };
    for (int l = 0; l < laneCount (space); ++l)
    {
        if (l == lane) continue;
        g.setColour (laneInfo (space, l).colour.withAlpha (0.28f));
        g.strokePath (lanePath (l), juce::PathStrokeType (1.2f));
    }

    auto path = lanePath (lane);
    auto fill = path;
    fill.lineTo (p.getRight(), p.getBottom());
    fill.lineTo (p.getX(), p.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.32f), 0.0f, p.getY(), col.withAlpha (0.02f), 0.0f, p.getBottom(), false));
    g.fillPath (fill);
    const auto js = juce::PathStrokeType::curved;
    const auto ec = juce::PathStrokeType::rounded;
    g.setColour (col.withAlpha (0.16f)); g.strokePath (path, juce::PathStrokeType (9.0f, js, ec));
    g.setColour (col.withAlpha (0.35f)); g.strokePath (path, juce::PathStrokeType (4.5f, js, ec));
    g.setColour (col);                   g.strokePath (path, juce::PathStrokeType (2.0f, js, ec));

    if (mode == Mode::Points)
    {
        if (! nodesValid)
            buildNodes();
        for (size_t k = 0; k < nodes.size(); ++k)
        {
            const float x = xOfIndex (nodes[k].x);
            if (x < p.getX() - 8 || x > p.getRight() + 8) continue;
            const float y = yOf (nodes[k].y);
            const bool on = (int) k == hover, sel = selected.size() > k && selected[k];
            const float r = on ? 6.5f : sel ? 5.5f : 4.5f;
            g.setColour (sel ? col : (on ? col : paper));
            g.fillEllipse (x - r, y - r, r * 2, r * 2);
            g.setColour (sel ? text : col);
            g.drawEllipse (x - r, y - r, r * 2, r * 2, sel ? 2.0f : 1.8f);
        }
    }

    if (! marquee.isEmpty())
    {
        g.setColour (accent.withAlpha (0.10f));
        g.fillRect (marquee);
        g.setColour (accent.withAlpha (0.7f));
        g.drawRect (marquee, 1.0f);
    }

    if (proc.uiPlaying.load())
    {
        const float ph = proc.uiPhase.load();
        const float x = xOfIndex (ph * kN);
        if (x >= p.getX() && x <= p.getRight())
        {
            g.setColour (text.withAlpha (0.75f));
            g.drawVerticalLine ((int) x, p.getY() - 4.0f, p.getBottom());
            g.setColour (col);
            const float y = yOf (lanes().sample (lane, ph));
            g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        }
    }
    g.restoreState();

    // Axis labels for the selected lane.
    const int type = juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("type")->load());
    const bool sizeLane = space == AddSpace && lane == Size;
    const juce::String top = sizeLane ? sizeTop[type] : info.top, bot = sizeLane ? sizeBot[type] : info.bot;
    g.setFont (monoFont (10.0f));
    g.setColour (muted);
    g.drawText (top.toUpperCase(), juce::Rectangle<float> (p.getX() + 8, p.getY() + 4, 160, 14), juce::Justification::centredLeft);
    g.drawText (bot.toUpperCase(), juce::Rectangle<float> (p.getX() + 8, p.getBottom() - 18, 160, 14), juce::Justification::centredLeft);

    // Readout while dragging: exact position and value.
    if ((dragKind == Drag::Point || dragKind == Drag::Group || dragKind == Drag::Scale) && anchor >= 0 && anchor < (int) nodes.size())
    {
        const auto& n = nodes[(size_t) anchor];
        juce::String label = positionText (n.x);
        if (valueText)
            label << "   " << valueText (lane, n.y);
        const auto f = monoFont (11.0f);
        const float w = juce::GlyphArrangement::getStringWidth (f, label) + 16.0f;
        auto box = juce::Rectangle<float> (xOfIndex (n.x) + 12.0f, yOf (n.y) - 30.0f, w, 20.0f);
        box = box.constrainedWithin (p.reduced (2.0f));
        g.setColour (panel2.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (col);
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (text);
        g.setFont (f);
        g.drawText (label, box, juce::Justification::centred);
    }

    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);
}
