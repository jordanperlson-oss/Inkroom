#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Lanes.h"

class InkroomProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    InkroomProcessor();
    ~InkroomProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Inkroom"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Message thread only. Fills the lanes and sets the reverb settings for a preset. */
    void loadPreset (int index);

    /** Beats in one shape loop, from the Shape loop setting. */
    int loopBeats() const;

    juce::AudioProcessorValueTreeState apvts;
    ink::Lanes lanes;
    std::atomic<int> currentPreset { 0 };

    // Read by the editor for the playhead, readouts and room view.
    std::atomic<float> uiPhase { 0.0f }, uiEnergy { 0.0f };
    std::atomic<bool> uiPlaying { false };
    std::array<std::atomic<float>, ink::NumLanes> uiVals {};

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;
    void rebuildImpulses();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, double ppq0, double ppqPerSample);
    float laneValue (int lane, double p, long long k, float drift) const noexcept;
    float saturate (int ch, float x, float pre, float post, int type) noexcept;

    std::array<std::unique_ptr<juce::dsp::Convolution>, 4> convs;
    juce::AudioBuffer<float> sendBuf, convTmp, wetBuf;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay { 48000 };
    juce::dsp::StateVariableTPTFilter<float> brightLP, toneLP, satHP;

    std::array<std::vector<float>, 4> pXf;
    std::vector<float> pLev, pPan, pPre, pPost;
    std::array<float, 4> sXf {};
    float sLev = 0.0f, sPan = 0.5f, sPre = 0.1f, sPost = 1.0f;

    double sr = 44100.0, freePpq = 0.0, lastBpm = 120.0;
    int maxBlock = 512;
    int builtType = -1;
    double builtRate = 0.0;

    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pDrift = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pPredelay = nullptr;
    std::atomic<float>* pBright = nullptr;
    std::atomic<float>* pDepth = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pType = nullptr;
    std::atomic<float>* pChar = nullptr;
    std::atomic<float>* pPlace = nullptr;
    std::atomic<float>* pLoop = nullptr;
    std::atomic<float>* pSolo = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InkroomProcessor)
};
