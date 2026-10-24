#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>

namespace ink
{
/** Resolution of each drawn lane across one shape loop. */
constexpr int kN = 256;

enum Lane { Level = 0, Size, Length, Pan, Drive, NumLanes };

/** The five drawn shapes. Written by the editor (message thread), read by the audio thread. */
struct Lanes
{
    using Snapshot = std::array<std::array<float, kN>, NumLanes>;

    Lanes()
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

    Snapshot snapshot() const
    {
        Snapshot s {};
        for (int l = 0; l < NumLanes; ++l)
            for (int i = 0; i < kN; ++i)
                s[(size_t) l][(size_t) i] = get (l, i);
        return s;
    }

    void restore (const Snapshot& s)
    {
        for (int l = 0; l < NumLanes; ++l)
            for (int i = 0; i < kN; ++i)
                set (l, i, s[(size_t) l][(size_t) i]);
        version.fetch_add (1);
    }

    juce::String toBase64() const
    {
        juce::MemoryBlock mb;
        for (int l = 0; l < NumLanes; ++l)
            for (int i = 0; i < kN; ++i)
            {
                const float v = get (l, i);
                mb.append (&v, sizeof (float));
            }
        return mb.toBase64Encoding();
    }

    bool fromBase64 (const juce::String& text)
    {
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (text) || mb.getSize() != sizeof (float) * (size_t) (kN * NumLanes))
            return false;

        const auto* f = static_cast<const float*> (mb.getData());
        for (int l = 0; l < NumLanes; ++l)
            for (int i = 0; i < kN; ++i)
                set (l, i, f[l * kN + i]);
        version.fetch_add (1);
        return true;
    }

    std::array<std::array<std::atomic<float>, kN>, NumLanes> pts;

    /** Bumped whenever lanes change from outside the canvas (presets, undo, session load). */
    std::atomic<int> version { 0 };
};
} // namespace ink
