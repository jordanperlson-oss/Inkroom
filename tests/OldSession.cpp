// Loads a session saved by Inkroom 0.1 into the current build, plays it, and opens the editor.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    // Build a 0.1-style state: 0.1 parameters only, five lanes stored as "lanes", "preset".
    juce::ValueTree st ("Inkroom");
    const char* ids[] = { "mix", "drift", "swing", "predelay", "bright", "depth", "tone", "output", "type", "character", "placement", "loop", "solo" };
    const float vals[] = { 0.7f, 0.25f, 0.12f, 12.0f, 0.8f, 0.8f, 0.72f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f };
    for (int i = 0; i < 13; ++i) { juce::ValueTree p ("PARAM"); p.setProperty ("id", ids[i], nullptr); p.setProperty ("value", vals[i], nullptr); st.appendChild (p, nullptr); }
    juce::MemoryBlock lanes; for (int i = 0; i < 5 * 256; ++i) { float v = 0.3f; lanes.append (&v, 4); }
    st.setProperty ("lanes", lanes.toBase64Encoding(), nullptr);
    st.setProperty ("preset", 2, nullptr);
    juce::MemoryBlock blob; { auto xml = st.createXml(); juce::AudioProcessor::copyXmlToBinary (*xml, blob); }

    InkroomProcessor proc;
    proc.setStateInformation (blob.getData(), (int) blob.getSize());
    std::cout << "State loaded. Lane value " << proc.addLanes.get (0, 10) << ", mode " << proc.space() << "\n";
    for (double rate : { 44100.0, 48000.0, 96000.0 })
      for (int bs : { 32, 256, 1024 })
      {
        proc.setPlayConfigDetails (2, 2, rate, bs);
        proc.prepareToPlay (rate, bs);
        juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer m;
        for (int b = 0; b < 50; ++b) { for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) buf.setSample (c, i, std::sin (i * 0.05f) * 0.5f); proc.processBlock (buf, m); }
        proc.setSpace (1); for (int b = 0; b < 50; ++b) proc.processBlock (buf, m); proc.setSpace (0);
      }
    std::cout << "Played at 44.1/48/96 kHz, blocks 32/256/1024, both modes\n";
    for (int sp : { 0, 1 }) { proc.setSpace (sp); std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor()); ed->setSize (800, 576); auto img = ed->createComponentSnapshot (ed->getLocalBounds()); std::cout << "Editor opened in mode " << sp << " at " << ed->getWidth() << "x" << ed->getHeight() << "\n"; }
    // Mono input on a stereo output, as on a mono track.
    InkroomProcessor mono;
    juce::AudioProcessor::BusesLayout lay; lay.inputBuses.add (juce::AudioChannelSet::mono()); lay.outputBuses.add (juce::AudioChannelSet::stereo());
    std::cout << "Mono-to-stereo supported: " << (mono.setBusesLayout (lay) ? "yes" : "NO") << "\n";
    mono.prepareToPlay (48000, 512); juce::AudioBuffer<float> b2 (2, 512); juce::MidiBuffer m2; b2.clear(); mono.processBlock (b2, m2);
    std::cout << "Mono track played\n";
    return 0;
}
