// Shape space check: synthetic drum hits with a room ring, run through the live hit/ring split.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <iostream>

namespace
{
struct Head : juce::AudioPlayHead
{
    double ppq = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setBpm (100.0); p.setPpqPosition (ppq); p.setIsPlaying (true); return p; }
};
void setP (InkroomProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
double db (double x) { return 10.0 * std::log10 (std::max (1e-15, x)); }

struct Result { double hitIn = 0, hitOut = 0, ringIn = 0, ringOut = 0, nullErr = 0, total = 0; std::array<std::array<double, 4>, 4> beat {}; };

/** Quarter notes at 100 bpm: a sharp 4 ms hit plus a 120 ms room ring after it. */
Result run (InkroomProcessor& proc, Head& head)
{
    const double sr = 48000.0; const int bs = 480;
    const int la = proc.getLatencySamples();
    const double pps = 100.0 / 60.0 / sr;
    head.ppq = 0.0;
    juce::Random r (11);
    std::vector<float> history;
    Result res;
    juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer m;
    const int total = (int) (16.0 / pps);
    for (int s0 = 0; s0 < total; s0 += bs)
    {
        for (int i = 0; i < bs; ++i)
        {
            const double q = head.ppq + i * pps;
            const double since = (q - std::floor (q)) / pps;    // samples since the beat
            const float hit = since < 192 ? (r.nextFloat() * 2 - 1) * 0.8f * (float) std::exp (-since / 72.0) : 0.0f;
            const float ring = since > 96 ? (r.nextFloat() * 2 - 1) * 0.12f * (float) std::exp (-(since - 96) / 5760.0) : 0.0f;
            buf.setSample (0, i, hit + ring); buf.setSample (1, i, hit + ring);
            history.push_back (hit + ring);
        }
        proc.processBlock (buf, m);
        for (int i = 0; i < bs; ++i)
        {
            const size_t n = (size_t) (s0 + i);
            if (n < (size_t) la) continue;
            const double qOut = (double) (n - (size_t) la) * pps;      // song position of the input this output came from
            if (qOut < 4.0) continue;
            const float in = history[n - (size_t) la];
            const float out = buf.getSample (0, i);
            const double since = (qOut - std::floor (qOut)) / pps;
            if (since < 160) { res.hitIn += in * in; res.hitOut += out * out; }
            else if (since > 3600 && since < 12000) { res.ringIn += in * in; res.ringOut += out * out; }
            res.nullErr += (out - in) * (out - in); res.total += in * in;
            const int beat = (int) std::floor (qOut) % 4, quarter = (int) std::floor ((qOut - std::floor (qOut)) * 4);
            res.beat[(size_t) beat][(size_t) quarter] += out * out;
        }
        head.ppq += bs * pps;
    }
    return res;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    InkroomProcessor proc; Head head; proc.setPlayHead (&head);
    proc.setPlayConfigDetails (2, 2, 48000.0, 480);
    proc.prepareToPlay (48000.0, 480);
    proc.setSpace (ink::ShapeSpace);
    setP (proc, "mix", 1.0f); setP (proc, "drift", 0.0f);
    std::cout << "Reported latency: " << proc.getLatencySamples() << " samples\n";

    // Neutral lanes: output should match the input (shifted by the latency).
    ink::shapeDefaults (proc.shapeLanes);
    proc.shapeLanes.fill (ink::SLevel, [] (float) { return 1.0f; });
    proc.shapeLanes.fill (ink::SDrive, [] (float) { return 0.0f; });
    setP (proc, "tone", 1.0f);
    auto a = run (proc, head);
    std::cout << "Neutral: output vs input difference " << juce::String (db (a.nullErr / a.total), 1) << " dB (lower is closer)\n";

    // Sustain choked: hits should survive, ring (75-250 ms after each hit) should disappear.
    proc.shapeLanes.fill (ink::Sustain, [] (float) { return 0.0f; });
    auto b = run (proc, head);
    std::cout << "Sustain choked: hits " << juce::String (db (b.hitOut / b.hitIn), 1) << " dB, ring " << juce::String (db (b.ringOut / b.ringIn), 1) << " dB\n";

    // Punch muted, sustain normal: hits should drop, ring should stay.
    proc.shapeLanes.fill (ink::Sustain, [] (float) { return 0.5f; });
    proc.shapeLanes.fill (ink::Punch, [] (float) { return 0.0f; });
    auto c = run (proc, head);
    std::cout << "Punch muted: hits " << juce::String (db (c.hitOut / c.hitIn), 1) << " dB, ring " << juce::String (db (c.ringOut / c.ringIn), 1) << " dB\n";

    // Gated Rooms with Tail solo: the ring should open only on 2 and 4.
    proc.loadPreset (ink::ShapeSpace, 1);
    setP (proc, "solo", 1.0f);
    auto d = run (proc, head);
    double peak = 1e-15; for (auto& row : d.beat) for (double v : row) peak = std::max (peak, v);
    std::cout << "Gated Rooms, ring only (dB per quarter of each beat):\n";
    for (int bt = 0; bt < 4; ++bt)
    {
        std::cout << "  beat " << bt + 1 << ":";
        for (int q = 0; q < 4; ++q) std::cout << "  " << juce::String (db (d.beat[(size_t) bt][(size_t) q] / peak), 1);
        std::cout << "\n";
    }
    setP (proc, "solo", 0.0f);

    // Snapshots of the editor in Shape space, at full size and at 70%.
    for (float scale : { 1.0f, 0.7f })
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize ((int) (1000 * scale), (int) (720 * scale));
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (scale > 0.9f ? "shape_full.png" : "shape_70.png");
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "Snapshot " << out.getFileName() << " " << ed->getWidth() << "x" << ed->getHeight() << "\n";
    }
    return 0;
}
