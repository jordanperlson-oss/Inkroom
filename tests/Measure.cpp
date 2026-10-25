// Measures wet level vs input level for each reverb type with every lane at a fixed value.
#include "PluginProcessor.h"
#include <iostream>
struct Head : juce::AudioPlayHead { double ppq = 0; juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (100); p.setPpqPosition (ppq); p.setIsPlaying (true); return p; } };
static void setP (InkroomProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000; const int bs = 512;
    for (int type = 0; type < 3; ++type)
      for (float sz : { 0.2f, 0.5f, 0.9f })
      {
        InkroomProcessor proc; Head head; proc.setPlayHead (&head);
        proc.setPlayConfigDetails (2, 2, sr, bs);
        setP (proc, "type", (float) type); setP (proc, "mix", 1.0f); setP (proc, "solo", 1.0f); setP (proc, "drift", 0.0f);
        for (int l = 0; l < ink::NumLanes; ++l) proc.addLanes.fill (l, [] (float) { return 0.5f; });
        proc.addLanes.fill (ink::Level, [] (float) { return 1.0f; });
        proc.addLanes.fill (ink::Drive, [] (float) { return 0.0f; });
        proc.addLanes.fill (ink::Size, [sz] (float) { return sz; });
        proc.addLanes.fill (ink::Length, [sz] (float) { return sz; });
        proc.prepareToPlay (sr, bs);
        juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer m; juce::Random r (3);
        for (int i = 0; i < 40; ++i) { buf.clear(); proc.processBlock (buf, m); juce::Thread::sleep (20); }
        double inE = 0, outE = 0; double peakIn = 0, peakOut = 0;
        const double pps = 100.0 / 60.0 / sr;
        for (int b = 0; b < (int) (sr * 8 / bs); ++b)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double q = head.ppq + i * pps, since = (q * 2 - std::floor (q * 2)) / 2 / pps;
                const float v = since < 2400 ? (r.nextFloat() * 2 - 1) * (float) std::exp (-since / 600.0) * 0.5f : 0.0f;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
                if (b > 90) { inE += 2 * v * v; peakIn = std::max (peakIn, (double) std::abs (v)); }
            }
            proc.processBlock (buf, m);
            if (b > 90) for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) { const float o = buf.getSample (c, i); outE += o * o; peakOut = std::max (peakOut, (double) std::abs (o)); }
            head.ppq += bs * pps;
        }
        const char* names[] = { "Room", "Plate", "Spring" };
        std::cout << names[type] << " size/length " << sz << ": tail vs input RMS " << juce::String (10 * std::log10 (outE / inE), 1)
                  << " dB, peak " << juce::String (20 * std::log10 (peakOut / peakIn), 1) << " dB\n";
      }
}
