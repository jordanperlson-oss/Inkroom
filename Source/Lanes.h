#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <vector>

namespace ink
{
/** Resolution of each drawn lane across one shape loop (about a 1/128 note on a 4-bar loop). */
constexpr int kN = 2048;
/** Resolution used by Inkroom 0.1 and 0.2; older sessions are resampled on load. */
constexpr int kLegacyN = 256;
constexpr int kMaxLanes = 8;

/** Add space: generates a new reverb and shapes its tail. */
enum Lane { Level = 0, Size, Length, Pan, Drive, NumLanes };

/** Shape space: reshapes the room already in the audio. */
enum ShapeLane { Sustain = 0, Punch, SLevel, Width, SPan, SDrive, NumShapeLanes };

enum Space { AddSpace = 0, ShapeSpace = 1 };

/** A set of drawn shapes. Written by the editor (message thread), read by the audio thread. */
struct Lanes
{
    /** count * kN values, lane after lane. */
    using Snapshot = std::vector<float>;

    explicit Lanes (int numLanes) : count (numLanes)
    {
        for (auto& lane : pts)
            for (auto& p : lane)
                p.store (0.5f, std::memory_order_relaxed);
    }

    float get (int lane, int i) const noexcept
    {
        return pts[(size_t) lane][(size_t) i].load (std::memory_order_relaxed);
    }

    void set (int lane, int i, float v) noexcept
    {
        pts[(size_t) lane][(size_t) i].store (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed);
    }

    /** Linear read at loop phase p in [0, 1). */
    float sample (int lane, double p) const noexcept
    {
        const double x = p * kN;
        const int a = ((int) std::floor (x) % kN + kN) % kN;
        const int b = (a + 1) % kN;
        const float f = (float) (x - std::floor (x));
        return get (lane, a) * (1.0f - f) + get (lane, b) * f;
    }

    void fill (int lane, const std::function<float (float)>& fn)
    {
        for (int i = 0; i < kN; ++i)
            set (lane, i, fn ((float) i / (float) kN));
    }

    void copyLane (int from, int to, bool inverted)
    {
        for (int i = 0; i < kN; ++i)
            set (to, i, inverted ? 1.0f - get (from, i) : get (from, i));
    }

    Snapshot snapshot() const
    {
        Snapshot s ((size_t) (count * kN));
        for (int l = 0; l < count; ++l)
            for (int i = 0; i < kN; ++i)
                s[(size_t) (l * kN + i)] = get (l, i);
        return s;
    }

    void restore (const Snapshot& s)
    {
        if (s.size() != (size_t) (count * kN))
            return;
        for (int l = 0; l < count; ++l)
            for (int i = 0; i < kN; ++i)
                set (l, i, s[(size_t) (l * kN + i)]);
        version.fetch_add (1);
    }

    juce::String toBase64() const
    {
        juce::MemoryBlock mb;
        for (int l = 0; l < count; ++l)
            for (int i = 0; i < kN; ++i)
            {
                const float v = get (l, i);
                mb.append (&v, sizeof (float));
            }
        return mb.toBase64Encoding();
    }

    /** Accepts the current resolution and the 256-step lanes saved by earlier versions. */
    bool fromBase64 (const juce::String& text)
    {
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (text))
            return false;

        const auto* f = static_cast<const float*> (mb.getData());
        const size_t floats = mb.getSize() / sizeof (float);
        int n = 0;
        if (floats == (size_t) (count * kN))             n = kN;
        else if (floats == (size_t) (count * kLegacyN))  n = kLegacyN;
        else return false;

        for (int l = 0; l < count; ++l)
            for (int i = 0; i < kN; ++i)
            {
                const double x = (double) i * n / kN;
                const int a = (int) x, b = (a + 1) % n;
                const float t = (float) (x - a);
                set (l, i, f[l * n + a] * (1.0f - t) + f[l * n + b] * t);
            }
        version.fetch_add (1);
        return true;
    }

    const int count;
    std::array<std::array<std::atomic<float>, kN>, kMaxLanes> pts;

    /** Bumped whenever lanes change from outside the canvas (presets, undo, session load). */
    std::atomic<int> version { 0 };
};
} // namespace ink
