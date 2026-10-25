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

    /** Message thread only. Fills a lane set and sets the related settings for a preset. */
    void loadPreset (int space, int index);

    /** Beats in one shape loop, from the Shape loop setting. */
    int loopBeats() const;

    /** Which mode is active: ink::AddSpace or ink::ShapeSpace. */
    int space() const;
    void setSpace (int s);

    ink::Lanes& lanesFor (int s) { return s == ink::ShapeSpace ? shapeLanes : addLanes; }

    juce::AudioProcessorValueTreeState apvts;
    ink::Lanes addLanes { ink::NumLanes };
    ink::Lanes shapeLanes { ink::NumShapeLanes };
    std::array<std::atomic<int>, 2> currentPreset {};

    /** Editor size as a scale of the design size; saved with the session. */
    std::atomic<float> editorScale { -1.0f };

    // Read by the editor for the playhead, readouts and room view.
    std::atomic<float> uiPhase { 0.0f }, uiEnergy { 0.0f }, uiHit { 0.0f };
    std::atomic<bool> uiPlaying { false };
    std::array<std::atomic<float>, ink::kMaxLanes> uiVals {};

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;
    void rebuildImpulses();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, double ppq0, double ppqPerSample);
    void processAdd (int len, double ppq0, double pps, bool publishUi);
    void processShape (int len, double ppq0, double pps, bool publishUi);
    void loopPhase (double ppq, double& p, long long& k) const noexcept;
    float laneValue (const ink::Lanes& lanes, int lane, double p, long long k, float drift) const noexcept;
    float saturate (int ch, float x, float pre, float post, int type) noexcept;

    // Add space
    std::array<std::unique_ptr<juce::dsp::Convolution>, 4> convs;
    juce::AudioBuffer<float> sendBuf, convTmp, wetBuf;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay { 48000 };
    juce::dsp::StateVariableTPTFilter<float> brightLP, toneLP, satHP;
    std::array<std::vector<float>, 4> pXf;
    std::vector<float> pLev, pPan, pPre, pPost;
    std::array<float, 4> sXf {};
    float sLev = 0.0f, sPan = 0.5f, sPre = 0.1f, sPost = 1.0f;

    // Shape space
    std::vector<float> qSus, qPunch, qLev, qWidth, qPan, qPre, qPost;
    float hSus = 1.0f, hPunch = 1.0f, hLev = 1.0f, hWidth = 1.0f, hPan = 0.5f, hPre = 0.1f, hPost = 1.0f;
    float detFast = 0.0f, detSlow = 0.0f, mask = 0.0f;
    int holdLeft = 0;

    // Shared: 3 ms lookahead so the split can catch each hit before it lands.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lookahead { 4096 };
    juce::AudioBuffer<float> undelayed, delayed, addOut, shapeOut;
    float modeFade = 0.0f;
    int lookaheadSamples = 0;

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
    std::atomic<float>* pSpace = nullptr;
    std::atomic<float>* pHitLen = nullptr;
    std::atomic<float>* pSens = nullptr;
    std::atomic<float>* pHits = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InkroomProcessor)
};
