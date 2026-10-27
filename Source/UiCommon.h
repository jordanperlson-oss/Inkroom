#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Lanes.h"

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

inline juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font monoFont (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

inline juce::Font displayFont (float size)
{
    static const juce::String face = juce::Font::findAllTypefaceNames().contains ("Georgia")
                                         ? juce::String ("Georgia") : juce::Font::getDefaultSerifFontName();
    return juce::Font (juce::FontOptions (face, size, juce::Font::plain));
}
} // namespace ink::ui
