#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "IRGen.h"
#include "Presets.h"

namespace
{
constexpr int kControlStep = 16;   // samples between lane reads
constexpr int kLoopBeats[] = { 2, 4, 8, 16 };

inline double fracd (double x) { return x - std::floor (x); }
inline double rndd (double n) { return fracd (std::sin (n * 12.9898 + 78.233) * 43758.5453); }

inline float shapeCurve (int type, float x) noexcept
{
    x = juce::jlimit (-1.0f, 1.0f, x);
    static const float t6 = std::tanh (6.0f);
    switch (type)
    {
        case 1:  return x >= 0.0f ? std::tanh (6.0f * x) / t6 : 0.8f * std::tanh (7.5f * x);  // Tube
        case 2:  return std::sin (6.0f * x);                                                // Fold
        default: return std::tanh (6.0f * x) / t6;                                          // Tape
    }
}

inline void satGains (float drive, float depth, float& pre, float& post) noexcept
{
    const float d = drive * depth;
    pre = 0.04f + d * 1.4f;
    post = std::min (4.2f, (1.0f / (6.0f * pre)) * (1.0f + 1.6f * d));
}

juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String kHzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz)); }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout InkroomProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<F> (juce::ParameterID { "mix", 1 }, "Mix", R (0.0f, 1.0f, 0.01f), 0.7f, A().withStringFromValueFunction (pctText)));
    layout.add (std::make_unique<F> (juce::ParameterID { "drift", 1 }, "Drift", R (0.0f, 1.0f, 0.01f), 0.25f, A().withStringFromValueFunction (pctText)));
    layout.add (std::make_unique<F> (juce::ParameterID { "swing", 1 }, "Swing", R (0.0f, 0.7f, 0.01f), 0.12f,
        A().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (50.0f + v * 25.0f)) + "%"; })));
    layout.add (std::make_unique<F> (juce::ParameterID { "predelay", 1 }, "Pre-delay", R (0.0f, 150.0f, 1.0f), 12.0f,
        A().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })));
    layout.add (std::make_unique<F> (juce::ParameterID { "bright", 1 }, "Brightness", R (0.0f, 1.0f, 0.01f), 0.8f,
        A().withStringFromValueFunction ([] (float v, int) { return kHzText (1500.0f * std::pow (12.0f, v)); })));
    layout.add (std::make_unique<F> (juce::ParameterID { "depth", 1 }, "Drive depth", R (0.0f, 1.0f, 0.01f), 0.8f, A().withStringFromValueFunction (pctText)));
    layout.add (std::make_unique<F> (juce::ParameterID { "tone", 1 }, "Drive tone", R (0.0f, 1.0f, 0.01f), 0.72f,
        A().withStringFromValueFunction ([] (float v, int) { return kHzText (600.0f * std::pow (30.0f, v)); })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "type", 1 }, "Reverb type", juce::StringArray { "Room", "Plate", "Spring" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "character", 1 }, "Drive character", juce::StringArray { "Tape", "Tube", "Fold" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "placement", 1 }, "Drive placement", juce::StringArray { "Into room", "On tail" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "loop", 1 }, "Shape loop", juce::StringArray { "2 beats", "1 bar", "2 bars", "4 bars" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "solo", 1 }, "Tail solo", false));
    return layout;
}

InkroomProcessor::InkroomProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Inkroom", createLayout())
{
    for (auto& c : convs)
        c = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::NonUniform { 512 });

    pMix = apvts.getRawParameterValue ("mix");
    pDrift = apvts.getRawParameterValue ("drift");
    pSwing = apvts.getRawParameterValue ("swing");
    pPredelay = apvts.getRawParameterValue ("predelay");
    pBright = apvts.getRawParameterValue ("bright");
    pDepth = apvts.getRawParameterValue ("depth");
    pTone = apvts.getRawParameterValue ("tone");
    pType = apvts.getRawParameterValue ("type");
    pChar = apvts.getRawParameterValue ("character");
    pPlace = apvts.getRawParameterValue ("placement");
    pLoop = apvts.getRawParameterValue ("loop");
    pSolo = apvts.getRawParameterValue ("solo");

    for (auto& v : uiVals)
        v.store (0.5f);

    loadPreset (0);
    startTimerHz (10);
}

InkroomProcessor::~InkroomProcessor()
{
    stopTimer();
}

int InkroomProcessor::loopBeats() const
{
    return kLoopBeats[juce::jlimit (0, 3, (int) pLoop->load())];
}

void InkroomProcessor::loadPreset (int index)
{
    const auto& list = ink::presets();
    if (index < 0 || index >= (int) list.size())
        return;

    const auto& p = list[(size_t) index];
    p.apply (lanes);
    lanes.version.fetch_add (1);

    auto setChoice = [this] (const char* id, int idx) {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
    };
    setChoice ("loop", p.loop);
    setChoice ("type", p.type);
    setChoice ("character", p.character);
    setChoice ("placement", p.placement);
    currentPreset.store (index);
}

bool InkroomProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void InkroomProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (32, samplesPerBlock);
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

    for (auto& c : convs)
        c->prepare (spec);

    predelay.setMaximumDelayInSamples ((int) (0.2 * sr) + 2);
    predelay.prepare (spec);

    brightLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    satHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    for (auto* f : { &brightLP, &toneLP, &satHP })
    {
        f->prepare (spec);
        f->setResonance (0.5f);
    }
    satHP.setCutoffFrequency (30.0f);

    sendBuf.setSize (2, maxBlock);
    convTmp.setSize (2, maxBlock);
    wetBuf.setSize (2, maxBlock);
    for (auto& v : pXf)
        v.assign ((size_t) maxBlock, 0.0f);
    for (auto* v : { &pLev, &pPan, &pPre, &pPost })
        v->assign ((size_t) maxBlock, 0.0f);

    if (builtType != (int) pType->load() || std::abs (builtRate - sr) > 0.5)
        rebuildImpulses();
}

void InkroomProcessor::rebuildImpulses()
{
    const int type = juce::jlimit (0, 2, (int) pType->load());
    juce::Random r (1234 + type);
    for (int i = 0; i < 4; ++i)
        convs[(size_t) i]->loadImpulseResponse (ink::ir::make (type, sr, i, r), sr,
                                                juce::dsp::Convolution::Stereo::yes,
                                                juce::dsp::Convolution::Trim::no,
                                                juce::dsp::Convolution::Normalise::yes);
    builtType = type;
    builtRate = sr;
}

void InkroomProcessor::timerCallback()
{
    if (builtRate > 0.0 && builtType != (int) pType->load())
        rebuildImpulses();
}

float InkroomProcessor::laneValue (int lane, double p, long long k, float drift) const noexcept
{
    float v = lanes.sample (lane, p);
    if (drift > 0.0f)
    {
        const double r1 = rndd (lane * 101.0 + (double) k * 7.3), r2 = rndd (lane * 53.0 + (double) k * 3.1 + 9.0);
        const double tp = juce::MathConstants<double>::twoPi;
        v += (float) (drift * 0.2 * (0.6 * std::sin (tp * (2.0 * p + r1)) + 0.4 * std::sin (tp * (5.0 * p + r2))));
    }
    return juce::jlimit (0.0f, 1.0f, v);
}

float InkroomProcessor::saturate (int ch, float x, float pre, float post, int type) noexcept
{
    float y = shapeCurve (type, x * pre);
    y = satHP.processSample (ch, y);
    y = toneLP.processSample (ch, y);
    return y * post;
}

void InkroomProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    if (numIn == 0)
        buffer.clear();
    else if (numIn == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // Follow the host's tempo and bar position; free-run when the transport is stopped.
    double bpm = lastBpm, ppq = freePpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition())
                {
                    ppq = *q;
                    playing = true;
                }
        }
    if (bpm < 20.0 || bpm > 999.0)
        bpm = 120.0;

    const double pps = bpm / 60.0 / sr;
    lastBpm = bpm;
    freePpq = ppq + n * pps;
    uiPlaying.store (playing);

    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = std::min (maxBlock, n - start);
        processChunk (buffer, start, len, ppq + start * pps, pps);
    }
}

void InkroomProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len, double ppq0, double pps)
{
    const float mix = pMix->load(), drift = pDrift->load(), swing = pSwing->load(), depth = pDepth->load();
    const int charType = (int) pChar->load();
    const bool intoRoom = (int) pPlace->load() == 0;
    const bool solo = pSolo->load() > 0.5f;
    const double beats = (double) loopBeats();

    predelay.setDelay ((float) (pPredelay->load() * 0.001 * sr));
    brightLP.setCutoffFrequency (1500.0f * std::pow (12.0f, pBright->load()));
    toneLP.setCutoffFrequency (juce::jmin (20000.0f, 600.0f * std::pow (30.0f, pTone->load())));

    // 1. Lane values at control rate, smoothed per sample into gain curves.
    const float coef = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    const double split = 0.5 + swing * 0.25;
    std::array<float, 4> tXf = sXf;
    float tLev = sLev, tPan = sPan, tPre = sPre, tPost = sPost;

    for (int i = 0; i < len; ++i)
    {
        if (i % kControlStep == 0)
        {
            const double B = std::max (0.0, ppq0 + i * pps);
            const double e8 = B * 2.0, ei = std::floor (e8), u = e8 - ei;
            const double v = u < split ? 0.5 * u / split : 0.5 + 0.5 * (u - split) / (1.0 - split);
            const double loops = ((ei + v) / 2.0) / beats, kf = std::floor (loops), p = loops - kf;
            const auto k = (long long) kf;

            std::array<float, ink::NumLanes> vals {};
            for (int l = 0; l < ink::NumLanes; ++l)
                vals[(size_t) l] = laneValue (l, p, k, drift);

            const float s = vals[ink::Size], l = vals[ink::Length];
            const float w[4] = { (1 - s) * (1 - l), (1 - s) * l, s * (1 - l), s * l };
            for (int c = 0; c < 4; ++c)
                tXf[(size_t) c] = std::sqrt (w[c]);
            tLev = vals[ink::Level] * vals[ink::Level];
            tPan = vals[ink::Pan];
            satGains (vals[ink::Drive], depth, tPre, tPost);

            if (i == 0)
            {
                uiPhase.store ((float) p);
                for (int c = 0; c < ink::NumLanes; ++c)
                    uiVals[(size_t) c].store (vals[(size_t) c]);
            }
        }

        for (int c = 0; c < 4; ++c)
        {
            sXf[(size_t) c] += coef * (tXf[(size_t) c] - sXf[(size_t) c]);
            pXf[(size_t) c][(size_t) i] = sXf[(size_t) c];
        }
        sLev += coef * (tLev - sLev);   pLev[(size_t) i] = sLev;
        sPan += coef * (tPan - sPan);   pPan[(size_t) i] = sPan;
        sPre += coef * (tPre - sPre);   pPre[(size_t) i] = sPre;
        sPost += coef * (tPost - sPost); pPost[(size_t) i] = sPost;
    }

    // 2. Send: optional drive into the room, then pre-delay.
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* in = buffer.getReadPointer (ch, start);
        float* send = sendBuf.getWritePointer (ch);
        for (int i = 0; i < len; ++i)
        {
            float x = in[i];
            if (intoRoom)
                x = saturate (ch, x, pPre[(size_t) i], pPost[(size_t) i], charType);
            predelay.pushSample (ch, x);
            send[i] = predelay.popSample (ch);
        }
    }

    // 3. Four convolution corners, crossfaded by Size and Length.
    wetBuf.clear (0, len);
    for (int c = 0; c < 4; ++c)
    {
        for (int ch = 0; ch < 2; ++ch)
            convTmp.copyFrom (ch, 0, sendBuf, ch, 0, len);

        juce::dsp::AudioBlock<float> block (convTmp);
        auto sub = block.getSubBlock (0, (size_t) len);
        juce::dsp::ProcessContextReplacing<float> context (sub);
        convs[(size_t) c]->process (context);

        const auto& gains = pXf[(size_t) c];
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* src = convTmp.getReadPointer (ch);
            float* dst = wetBuf.getWritePointer (ch);
            for (int i = 0; i < len; ++i)
                dst[i] += src[i] * gains[(size_t) i];
        }
    }

    // 4. Tail: brightness, optional drive on the tail, level, pan, mix.
    const float wetGain = mix * 1.3f;
    const float dryGain = solo ? 0.0f : 1.0f;
    const float halfPi = juce::MathConstants<float>::halfPi;
    float energy = 0.0f;

    float* outL = buffer.getWritePointer (0, start);
    float* outR = buffer.getWritePointer (1, start);
    float* wl = wetBuf.getWritePointer (0);
    float* wr = wetBuf.getWritePointer (1);

    for (int i = 0; i < len; ++i)
    {
        float l = brightLP.processSample (0, wl[i]);
        float r = brightLP.processSample (1, wr[i]);
        if (! intoRoom)
        {
            l = saturate (0, l, pPre[(size_t) i], pPost[(size_t) i], charType);
            r = saturate (1, r, pPre[(size_t) i], pPost[(size_t) i], charType);
        }
        const float x = (pPan[(size_t) i] - 0.5f) * 2.0f;
        const float gL = std::min (1.0f, juce::MathConstants<float>::sqrt2 * std::cos ((x + 1.0f) * halfPi * 0.5f));
        const float gR = std::min (1.0f, juce::MathConstants<float>::sqrt2 * std::sin ((x + 1.0f) * halfPi * 0.5f));
        const float g = pLev[(size_t) i] * wetGain;
        l *= g * gL;
        r *= g * gR;
        energy += l * l + r * r;
        outL[i] = outL[i] * dryGain + l;
        outR[i] = outR[i] * dryGain + r;
    }

    const float rms = std::sqrt (energy / (2.0f * (float) juce::jmax (1, len)));
    uiEnergy.store (juce::jmin (1.0f, rms * 7.0f));
}

void InkroomProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("lanes", lanes.toBase64(), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void InkroomProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType()))
            return;
        lanes.fromBase64 (state.getProperty ("lanes").toString());
        currentPreset.store ((int) state.getProperty ("preset", -1));
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* InkroomProcessor::createEditor()
{
    return new InkroomEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InkroomProcessor();
}
