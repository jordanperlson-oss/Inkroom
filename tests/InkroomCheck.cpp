// Offline check for Inkroom: renders drum-like clicks through the processor with a fake host
// transport, reports how the drawn Level lane shapes the tail, and saves an editor snapshot.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

namespace
{
struct FakeHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 100.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (true);
        return p;
    }
};

void setParam (InkroomProcessor& p, const char* id, float plain)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

/** Renders 4 bars of 8th-note clicks; returns wet energy per (beat 1-4, quarter of beat) over bars 2-4. */
std::array<std::array<double, 4>, 4> render (InkroomProcessor& proc, FakeHead& head, double sr, int bs, bool& finite)
{
    std::array<std::array<double, 4>, 4> e {};
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    const double pps = head.bpm / 60.0 / sr;
    head.ppq = 0.0;
    juce::Random r (7);
    const int total = (int) (16.0 / pps);

    for (int s0 = 0; s0 < total; s0 += bs)
    {
        buf.clear();
        for (int i = 0; i < bs; ++i)
        {
            const double q = head.ppq + i * pps;
            const double since = (q * 2.0 - std::floor (q * 2.0)) / 2.0 / pps;   // samples since last 8th
            if (since < 240.0)
            {
                const float v = (r.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-since / 60.0) * 0.8f;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
        }
        proc.processBlock (buf, midi);
        for (int i = 0; i < bs; ++i)
        {
            const double q = head.ppq + i * pps;
            const float l = buf.getSample (0, i), rr = buf.getSample (1, i);
            if (! std::isfinite (l) || ! std::isfinite (rr)) finite = false;
            if (q < 4.0) continue;
            const int beat = (int) std::floor (q) % 4;
            const int quarter = (int) std::floor ((q - std::floor (q)) * 4.0);
            e[(size_t) beat][(size_t) quarter] += (double) (l * l + rr * rr);
        }
        head.ppq += bs * pps;
    }
    return e;
}

void report (const char* name, const std::array<std::array<double, 4>, 4>& e)
{
    double peak = 1e-12;
    for (auto& b : e) for (double v : b) peak = std::max (peak, v);
    std::cout << name << "  (relative tail energy, dB, per quarter of each beat)\n";
    for (int b = 0; b < 4; ++b)
    {
        std::cout << "  beat " << (b + 1) << ":";
        for (int q = 0; q < 4; ++q)
            std::cout << "  " << juce::String (10.0 * std::log10 (std::max (1e-12, e[(size_t) b][(size_t) q]) / peak), 1);
        std::cout << "\n";
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int bs = 512;

    InkroomProcessor proc;
    FakeHead head;
    proc.setPlayHead (&head);
    proc.setPlayConfigDetails (2, 2, sr, bs);
    proc.prepareToPlay (sr, bs);
    setParam (proc, "solo", 1.0f);   // tail only, so the shaping is easy to read
    setParam (proc, "drift", 0.0f);

    // The convolution engines load impulse responses in the background; let them land.
    juce::AudioBuffer<float> silent (2, bs);
    juce::MidiBuffer midi;
    for (int i = 0; i < 40; ++i)
    {
        silent.clear();
        proc.processBlock (silent, midi);
        juce::Thread::sleep (25);
    }

    bool finite = true;
    for (int idx : { 2, 3 })
    {
        proc.loadPreset (idx);
        for (int i = 0; i < 20; ++i) { silent.clear(); proc.processBlock (silent, midi); juce::Thread::sleep (20); }
        report (idx == 2 ? "Gated '84" : "Reverse Snare", render (proc, head, sr, bs, finite));
    }

    // Spring and plate impulses should load and render too.
    for (int type : { 1, 2 })
    {
        setParam (proc, "type", (float) type);
        proc.prepareToPlay (sr, bs);   // rebuilds the impulses for the new type
        for (int i = 0; i < 20; ++i) { silent.clear(); proc.processBlock (silent, midi); juce::Thread::sleep (20); }
        auto e = render (proc, head, sr, bs, finite);
        double total = 0; for (auto& b : e) for (double v : b) total += v;
        std::cout << (type == 1 ? "Plate" : "Spring") << " total tail energy: " << total << "\n";
    }
    std::cout << "All samples finite: " << (finite ? "yes" : "NO") << "\n";

    // State round trip.
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    InkroomProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    bool same = true;
    for (int l = 0; l < ink::NumLanes; ++l)
        for (int i = 0; i < ink::kN; ++i)
            same = same && std::abs (other.lanes.get (l, i) - proc.lanes.get (l, i)) < 1e-6f;
    std::cout << "State round trip keeps lanes: " << (same ? "yes" : "NO") << "\n";

    if (argc > 1)
    {
        setParam (proc, "type", 0.0f);
        proc.loadPreset (3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "Editor snapshot written to " << out.getFullPathName() << "\n";
    }
    return finite && same ? 0 : 1;
}
