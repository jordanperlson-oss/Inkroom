#pragma once

#include "Lanes.h"
#include <vector>

namespace ink
{
struct Preset
{
    const char* name;
    const char* note;
    int loop;       // 0 = 2 beats, 1 = 1 bar, 2 = 2 bars, 3 = 4 bars
    int type;       // 0 = Room, 1 = Plate, 2 = Spring
    int character;  // 0 = Tape, 1 = Tube, 2 = Fold
    int placement;  // 0 = Into room, 1 = On tail
    std::function<void (Lanes&)> apply;
};

namespace detail
{
    inline float frac (float x) { return x - std::floor (x); }
    inline float rnd (float n) { return frac (std::sin (n * 12.9898f + 78.233f) * 43758.5453f); }
    constexpr float twoPi = 6.28318530718f;
    /** Which beat (of 4) we are in, and how far through it. */
    inline void beatOf (float p, int& c, float& u) { const float x = p * 4.0f; c = (int) std::floor (x); u = x - (float) c; }
}

inline const std::vector<Preset>& presets()
{
    using namespace detail;
    static const std::vector<Preset> list {
        { "Downbeat Bloom", "Rooms open into every beat", 1, 0, 0, 1, [] (Lanes& L) {
            L.fill (Size,   [] (float p) { return 0.15f + 0.75f * std::pow (frac (p * 4), 1.6f); });
            L.fill (Length, [] (float p) { return 0.55f + 0.12f * std::sin (twoPi * p); });
            L.fill (Pan,    [] (float p) { return 0.5f + 0.35f * std::sin (twoPi * p); });
            L.fill (Level,  [] (float p) { return 0.12f + 0.88f * std::pow (frac (p * 2), 2.2f); });
            L.fill (Drive,  [] (float p) { return frac (p * 8) < 0.5f ? 0.12f : 0.55f; });
        } },
        { "Breathing Hall", "Slow fade in and out over two bars", 2, 0, 0, 1, [] (Lanes& L) {
            L.fill (Size,   [] (float p) { return 0.8f + 0.12f * std::sin (twoPi * p); });
            L.fill (Length, [] (float) { return 0.86f; });
            L.fill (Pan,    [] (float p) { return 0.5f + 0.18f * std::sin (2 * twoPi * p); });
            L.fill (Level,  [] (float p) { return 0.5f - 0.48f * std::cos (twoPi * p); });
            L.fill (Drive,  [] (float) { return 0.08f; });
        } },
        { "Gated '84", "Big room slammed shut on 2 and 4", 1, 0, 1, 0, [] (Lanes& L) {
            L.fill (Size,   [] (float) { return 0.78f; });
            L.fill (Length, [] (float) { return 0.72f; });
            L.fill (Pan,    [] (float) { return 0.5f; });
            L.fill (Level,  [] (float p) { int c; float u; beatOf (p, c, u); return (c % 2 == 1 && u < 0.3f) ? 1.0f : 0.02f; });
            L.fill (Drive,  [] (float) { return 0.45f; });
        } },
        { "Reverse Snare", "Swells into 2 and 4, cut dead on the hit", 1, 0, 0, 1, [] (Lanes& L) {
            L.fill (Level,  [] (float p) { int c; float u; beatOf (p, c, u); return c % 2 == 0 ? std::pow (u, 3.0f) : 0.0f; });
            L.fill (Size,   [] (float p) { int c; float u; beatOf (p, c, u); return c % 2 == 0 ? 0.6f + 0.35f * u : 0.6f; });
            L.fill (Length, [] (float) { return 0.92f; });
            L.fill (Pan,    [] (float p) { int c; float u; beatOf (p, c, u); return c % 2 == 0 ? 0.5f + (c == 0 ? -0.3f : 0.3f) * (1 - u) : 0.5f; });
            L.fill (Drive,  [] (float p) { int c; float u; beatOf (p, c, u); return c % 2 == 0 ? 0.1f + 0.5f * u * u : 0.1f; });
        } },
        { "Tape Ghost", "Short, warm tails that decay per beat", 1, 0, 0, 1, [] (Lanes& L) {
            L.fill (Size,   [] (float p) { return 0.4f + 0.1f * std::sin (twoPi * p * 2); });
            L.fill (Length, [] (float) { return 0.7f; });
            L.fill (Pan,    [] (float p) { return 0.25f + 0.5f * rnd (std::floor (p * 8) * 3.3f); });
            L.fill (Level,  [] (float p) { return 0.1f + 0.85f * std::pow (1 - frac (p * 4), 1.5f); });
            L.fill (Drive,  [] (float p) { return frac (p * 4) < 0.25f ? 0.9f : 0.6f; });
        } },
        { "Ping-Pong Cathedral", "Huge space bouncing on 8ths", 1, 0, 2, 1, [] (Lanes& L) {
            L.fill (Size,   [] (float) { return 0.95f; });
            L.fill (Length, [] (float) { return 0.9f; });
            L.fill (Pan,    [] (float p) { return ((int) std::floor (p * 8)) % 2 ? 0.08f : 0.92f; });
            L.fill (Level,  [] (float p) { return 0.6f + 0.3f * std::sin (twoPi * p); });
            L.fill (Drive,  [] (float p) { return 0.1f + 0.25f * frac (p * 2); });
        } },
        { "Surf Snare", "Spring tank drip on 2 and 4", 1, 2, 1, 0, [] (Lanes& L) {
            L.fill (Size,   [] (float) { return 0.62f; });
            L.fill (Length, [] (float) { return 0.66f; });
            L.fill (Pan,    [] (float p) { return 0.5f + 0.12f * std::sin (twoPi * p); });
            L.fill (Level,  [] (float p) { int c; float u; beatOf (p, c, u); return c % 2 == 1 ? 0.15f + 0.85f * std::pow (1 - u, 0.6f) : 0.12f; });
            L.fill (Drive,  [] (float) { return 0.7f; });
        } },
        { "Plate Swells", "Bright plate that blooms into each beat", 1, 1, 0, 1, [] (Lanes& L) {
            L.fill (Size,   [] (float p) { return 0.45f + 0.2f * std::sin (twoPi * p); });
            L.fill (Length, [] (float) { return 0.78f; });
            L.fill (Pan,    [] (float p) { return 0.5f + 0.3f * std::sin (2 * twoPi * p); });
            L.fill (Level,  [] (float p) { return 0.06f + 0.94f * std::pow (frac (p * 4), 2.6f); });
            L.fill (Drive,  [] (float) { return 0.15f; });
        } },
    };
    return list;
}
} // namespace ink
