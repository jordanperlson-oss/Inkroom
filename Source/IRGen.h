#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/** Generated impulse responses, ported from the browser prototype. */
namespace ink::ir
{
inline float noise (juce::Random& r) { return r.nextFloat() * 2.0f - 1.0f; }

inline juce::AudioBuffer<float> room (double sr, double size, double decay, juce::Random& r)
{
    const int len = (int) (sr * std::min (6.0, decay * 1.15 + 0.15));
    juce::AudioBuffer<float> buf (2, len);
    buf.clear();
    const int pre = (int) (sr * (0.002 + size * 0.032));
    const double attack = 0.002 + size * 0.03;
    const double erWin = 0.012 + size * 0.08;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        double y = 0.0;
        const double a0 = 0.85 - size * 0.35, damp = 1.4 - size * 0.6;

        for (int i = pre; i < len; ++i)
        {
            const double tt = (i - pre) / sr;
            const double env = std::pow (10.0, -3.0 * tt / decay) * (1.0 - std::exp (-tt / attack));
            const double a = 0.04 + a0 * std::exp (-tt * damp);
            y += a * (noise (r) - y);
            d[i] = (float) (y * env * 2.2);
        }

        const int taps = 10 + (int) std::round ((1.0 - size) * 14.0);
        for (int k = 0; k < taps; ++k)
        {
            const int at = pre + (int) (r.nextFloat() * erWin * sr);
            if (at < len)
                d[at] += (float) ((r.nextBool() ? -1.0 : 1.0) * (0.35 + r.nextFloat() * 0.5) * (1.0 - (double) k / taps * 0.6));
        }
    }
    return buf;
}

inline juce::AudioBuffer<float> plate (double sr, double size, double decay, juce::Random& r)
{
    const int len = (int) (sr * std::min (6.0, decay * 1.15 + 0.1));
    juce::AudioBuffer<float> buf (2, len);
    buf.clear();
    const double attack = 0.0006 + size * 0.004, damp = 0.45 + (1.0 - size) * 0.7;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        double y = 0.0, y2 = 0.0;
        for (int i = 0; i < len; ++i)
        {
            const double tt = i / sr;
            const double env = std::pow (10.0, -3.0 * tt / decay) * (1.0 - std::exp (-tt / attack));
            const double a = 0.12 + 0.82 * std::exp (-tt * damp);
            y += a * (noise (r) - y);
            d[i] = (float) ((y - 0.35 * y2) * env * 2.0);
            y2 = y;
        }
    }
    return buf;
}

inline juce::AudioBuffer<float> spring (double sr, double size, double decay, juce::Random& r)
{
    const int len = (int) (sr * std::min (6.0, decay * 1.1 + 0.2));
    juce::AudioBuffer<float> buf (2, len);
    buf.clear();
    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        const double T = (0.026 + size * 0.034) * (ch ? 1.07 : 0.94);
        const double g = std::pow (10.0, -3.0 * T / decay);
        double amp = 1.0;

        // One chirp per trip down the spring and back; each trip smears and darkens.
        for (int n = 0; amp > 0.0008; ++n)
        {
            const int start = (int) ((0.004 + n * T + ch * 0.003) * sr);
            if (start >= len)
                break;

            const double dur = 0.006 + 0.0045 * std::pow (n + 1.0, 0.85) * (0.7 + size * 0.6);
            const int Ln = (int) (dur * sr);
            const double f0 = 180.0, f1 = 600.0 + 3600.0 * std::pow (0.93, n);
            const double norm = amp / std::sqrt (dur * 200.0);
            double ph = 0.0;

            for (int j = 0; j < Ln && start + j < len; ++j)
            {
                const double u = (double) j / Ln;
                const double f = f0 * std::pow (f1 / f0, u);
                ph += twoPi * f / sr;
                const double w = std::sin (juce::MathConstants<double>::pi * u);
                d[start + j] += (float) (norm * w * w * std::sin (ph));
            }
            amp *= g * (0.92 + 0.16 * r.nextFloat());
        }

        double y = 0.0;
        for (int i = 0; i < len; ++i)
        {
            const double tt = i / sr;
            const double env = std::pow (10.0, -3.0 * tt / decay) * (1.0 - std::exp (-tt / 0.02));
            y += 0.25 * (noise (r) - y);
            d[i] += (float) (y * env * 0.25);
        }

        const double al = 1.0 - std::exp (-twoPi * 5000.0 / sr), ah = std::exp (-twoPi * 120.0 / sr);
        double lp = 0.0, hp = 0.0, prev = 0.0;
        for (int i = 0; i < len; ++i)
        {
            lp += al * (d[i] - lp);
            hp = ah * (hp + lp - prev);
            prev = lp;
            d[i] = (float) hp;
        }
    }
    return buf;
}

/** The four corners crossfaded by the Size and Length lanes: {size, decay seconds}. */
constexpr double kCorners[4][2] = { { 0.0, 0.35 }, { 0.0, 4.5 }, { 1.0, 0.35 }, { 1.0, 4.5 } };

/** Scales an impulse response to unit energy, so every type and corner plays back at the same loudness. */
inline void normaliseEnergy (juce::AudioBuffer<float>& buf)
{
    double e = 0.0;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const auto* d = buf.getReadPointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            e += (double) d[i] * d[i];
    }
    e /= juce::jmax (1, buf.getNumChannels());
    if (e > 1.0e-12)
        buf.applyGain ((float) (1.0 / std::sqrt (e)));
}

inline juce::AudioBuffer<float> make (int type, double sr, int corner, juce::Random& r)
{
    const double s = kCorners[corner][0], dcy = kCorners[corner][1];
    juce::AudioBuffer<float> buf;
    switch (type)
    {
        case 1:  buf = plate (sr, s, dcy, r); break;
        case 2:  buf = spring (sr, s, dcy, r); break;
        default: buf = room (sr, s, dcy, r); break;
    }
    normaliseEnergy (buf);
    return buf;
}
} // namespace ink::ir
