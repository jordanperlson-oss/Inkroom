#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "IRGen.h"
#include "Presets.h"

namespace
{
constexpr int kControlStep = 16;   // samples between lane reads
constexpr int kLoopBeats[] = { 2, 4, 8, 16 };
constexpr float kTailCalibration = 1.0f;
// Per-type trims (dB) so Room, Plate and Spring all sit about 2 dB under the input
// at Mix 100%, Level full and Tail level 0 dB. Measured with tests/Measure.cpp.
constexpr float kTypeTrimDb[] = { 2.5f, 7.6f, -3.3f };

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

/** Lane value to gain for Sustain and Punch: 0 = silent, 0.5 = unchanged, 1 = +6 dB. */
inline float laneGain (float v) noexcept
{
    return v <= 0.5f ? (v / 0.5f) * (v / 0.5f) : 1.0f + (v - 0.5f) * 2.0f;
}

inline void panGains (float pan, float& gL, float& gR) noexcept
{
    const float x = (pan - 0.5f) * 2.0f, q = (x + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    gL = std::min (1.0f, juce::MathConstants<float>::sqrt2 * std::cos (q));
    gR = std::min (1.0f, juce::MathConstants<float>::sqrt2 * std::sin (q));
}

juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String kHzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz)); }
juce::String dbText (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout InkroomProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using R = juce::NormalisableRange<float>;
    using C = juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<C> (juce::ParameterID { "space", 1 }, "Mode", juce::StringArray { "Add space", "Shape space" }, 0));
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
    layout.add (std::make_unique<F> (juce::ParameterID { "output", 1 }, "Tail level", R (-24.0f, 12.0f, 0.1f), 0.0f, A().withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<C> (juce::ParameterID { "type", 1 }, "Reverb type", juce::StringArray { "Room", "Plate", "Spring" }, 0));
    layout.add (std::make_unique<C> (juce::ParameterID { "character", 1 }, "Drive character", juce::StringArray { "Tape", "Tube", "Fold" }, 0));
    layout.add (std::make_unique<C> (juce::ParameterID { "placement", 1 }, "Drive placement", juce::StringArray { "Into room", "On tail" }, 1));
    layout.add (std::make_unique<C> (juce::ParameterID { "loop", 1 }, "Shape loop", juce::StringArray { "2 beats", "1 bar", "2 bars", "4 bars" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "solo", 1 }, "Tail solo", false));
    layout.add (std::make_unique<F> (juce::ParameterID { "hitlen", 1 }, "Hit length", R (5.0f, 150.0f, 1.0f), 40.0f,
        A().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })));
    layout.add (std::make_unique<F> (juce::ParameterID { "sens", 1 }, "Sensitivity", R (0.0f, 1.0f, 0.01f), 0.5f, A().withStringFromValueFunction (pctText)));
    layout.add (std::make_unique<F> (juce::ParameterID { "hits", 1 }, "Hits", R (-24.0f, 6.0f, 0.1f), 0.0f, A().withStringFromValueFunction (dbText)));
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
    pOutput = apvts.getRawParameterValue ("output");
    pSpace = apvts.getRawParameterValue ("space");
    pHitLen = apvts.getRawParameterValue ("hitlen");
    pSens = apvts.getRawParameterValue ("sens");
    pHits = apvts.getRawParameterValue ("hits");

    for (auto& v : uiVals)
        v.store (0.5f);

    loadPreset (ink::ShapeSpace, 0);
    loadPreset (ink::AddSpace, 0);
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

int InkroomProcessor::space() const
{
    return pSpace->load() > 0.5f ? ink::ShapeSpace : ink::AddSpace;
}

void InkroomProcessor::setSpace (int s)
{
    if (auto* param = apvts.getParameter ("space"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) s));
}

void InkroomProcessor::loadPreset (int sp, int index)
{
    const bool shape = sp == ink::ShapeSpace;
    const auto& list = shape ? ink::shapePresets() : ink::presets();
    if (index < 0 || index >= (int) list.size())
        return;

    const auto& p = list[(size_t) index];
    auto& lanes = lanesFor (sp);
    clearLinks (sp);
    if (shape)
        ink::shapeDefaults (lanes);
    p.apply (lanes);
    lanes.version.fetch_add (1);

    auto setChoice = [this] (const char* id, int idx) {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
    };
    setChoice ("loop", p.loop);
    setChoice ("character", p.character);
    if (! shape)
    {
        setChoice ("type", p.type);
        setChoice ("placement", p.placement);
    }
    currentPreset[(size_t) sp].store (index);
}

bool InkroomProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo out (from mono or stereo in), or mono in and out for a plain mono track.
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return out == juce::AudioChannelSet::mono() && in == juce::AudioChannelSet::mono();
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

    lookaheadSamples = juce::jmax (1, (int) std::round (0.003 * sr));
    lookahead.setMaximumDelayInSamples (lookaheadSamples + 2);
    lookahead.prepare (spec);
    lookahead.setDelay ((float) lookaheadSamples);
    setLatencySamples (lookaheadSamples);

    brightLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    satHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    for (auto* f : { &brightLP, &toneLP, &satHP })
    {
        f->prepare (spec);
        f->setResonance (0.5f);
    }
    satHP.setCutoffFrequency (30.0f);

    for (auto* b : { &sendBuf, &convTmp, &wetBuf, &undelayed, &delayed, &addOut, &shapeOut, &monoWork })
        b->setSize (2, maxBlock);
    for (auto& v : pXf)
        v.assign ((size_t) maxBlock, 0.0f);
    for (auto* v : { &pLev, &pPan, &pPre, &pPost, &qSus, &qPunch, &qLev, &qWidth, &qPan, &qPre, &qPost })
        v->assign ((size_t) maxBlock, 0.0f);

    detFast = detSlow = mask = 0.0f;
    holdLeft = 0;
    modeFade = space() == ink::ShapeSpace ? 1.0f : 0.0f;

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
                                                juce::dsp::Convolution::Normalise::no);
    builtType = type;
    builtRate = sr;
}

void InkroomProcessor::timerCallback()
{
    if (builtRate > 0.0 && builtType != (int) pType->load())
        rebuildImpulses();
}

void InkroomProcessor::loopPhase (double ppq, double& p, long long& k) const noexcept
{
    const double split = 0.5 + pSwing->load() * 0.25;
    const double B = std::max (0.0, ppq);
    const double e8 = B * 2.0, ei = std::floor (e8), u = e8 - ei;
    const double v = u < split ? 0.5 * u / split : 0.5 + 0.5 * (u - split) / (1.0 - split);
    const double loops = ((ei + v) / 2.0) / (double) loopBeats(), kf = std::floor (loops);
    p = loops - kf;
    k = (long long) kf;
}

float InkroomProcessor::laneValue (const ink::Lanes& lanes, int lane, double p, long long k, float drift) const noexcept
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

    const bool monoOut = buffer.getNumChannels() < 2;
    if (numIn == 0)
        buffer.clear();
    else if (numIn == 1 && ! monoOut)
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
    uiBpm.store ((float) bpm);
    freePpq = ppq + n * pps;
    uiPlaying.store (playing);

    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = std::min (maxBlock, n - start);
        if (monoOut)
        {
            // Mono track: run the stereo engine on a copy, then fold back to mono.
            monoWork.copyFrom (0, 0, buffer, 0, start, len);
            monoWork.copyFrom (1, 0, buffer, 0, start, len);
            processChunk (monoWork, 0, len, ppq + start * pps, pps);
            buffer.copyFrom (0, start, monoWork, 0, 0, len);
            buffer.applyGain (0, start, len, 0.5f);
            buffer.addFrom (0, start, monoWork, 1, 0, len, 0.5f);
        }
        else
            processChunk (buffer, start, len, ppq + start * pps, pps);
    }
}

void InkroomProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len, double ppq0, double pps)
{
    // Keep an undelayed copy for hit detection, and delay the audio itself by the lookahead.
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* in = buffer.getReadPointer (ch, start);
        float* u = undelayed.getWritePointer (ch);
        float* d = delayed.getWritePointer (ch);
        for (int i = 0; i < len; ++i)
        {
            u[i] = in[i];
            lookahead.pushSample (ch, in[i]);
            d[i] = lookahead.popSample (ch);
        }
    }

    toneLP.setCutoffFrequency (juce::jmin (20000.0f, 600.0f * std::pow (30.0f, pTone->load())));

    const bool toShape = space() == ink::ShapeSpace;
    const bool needAdd = ! toShape || modeFade < 1.0f;
    const bool needShape = toShape || modeFade > 0.0f;
    if (needAdd)   processAdd (len, ppq0, pps, ! toShape);
    if (needShape) processShape (len, ppq0, pps, toShape);

    // 20 ms crossfade when switching modes.
    const float step = 1.0f / (0.02f * (float) sr);
    const float target = toShape ? 1.0f : 0.0f;
    float* o0 = buffer.getWritePointer (0, start);
    float* o1 = buffer.getWritePointer (1, start);
    const float* a0 = addOut.getReadPointer (0);
    const float* a1 = addOut.getReadPointer (1);
    const float* s0 = shapeOut.getReadPointer (0);
    const float* s1 = shapeOut.getReadPointer (1);
    float g = modeFade;
    for (int i = 0; i < len; ++i)
    {
        g = g < target ? std::min (target, g + step) : std::max (target, g - step);
        const float wa = needAdd ? 1.0f - g : 0.0f, ws = needShape ? g : 0.0f;
        o0[i] = (needAdd ? a0[i] : 0.0f) * wa + (needShape ? s0[i] : 0.0f) * ws;
        o1[i] = (needAdd ? a1[i] : 0.0f) * wa + (needShape ? s1[i] : 0.0f) * ws;
    }
    modeFade = g;
}

void InkroomProcessor::processAdd (int len, double ppq0, double pps, bool publishUi)
{
    const float mix = pMix->load(), drift = pDrift->load(), depth = pDepth->load();
    const int charType = (int) pChar->load();
    const bool intoRoom = (int) pPlace->load() == 0;
    const bool solo = pSolo->load() > 0.5f;

    for (int ch = 0; ch < 2; ++ch)
        addOut.copyFrom (ch, 0, delayed, ch, 0, len);

    predelay.setDelay ((float) (pPredelay->load() * 0.001 * sr));
    brightLP.setCutoffFrequency (1500.0f * std::pow (12.0f, pBright->load()));

    // 1. Lane values at control rate, smoothed per sample into gain curves.
    const float coef = 1.0f - std::exp (-1.0f / (0.0007f * (float) sr));
    std::array<float, 4> tXf = sXf;
    float tLev = sLev, tPan = sPan, tPre = sPre, tPost = sPost;

    for (int i = 0; i < len; ++i)
    {
        if (i % kControlStep == 0)
        {
            double p; long long k;
            loopPhase (ppq0 + i * pps, p, k);
            std::array<float, ink::NumLanes> vals {};
            for (int l = 0; l < ink::NumLanes; ++l)
                vals[(size_t) l] = laneValue (addLanes, l, p, k, drift);

            const float s = vals[ink::Size], l = vals[ink::Length];
            const float w[4] = { (1 - s) * (1 - l), (1 - s) * l, s * (1 - l), s * l };
            for (int c = 0; c < 4; ++c)
                tXf[(size_t) c] = std::sqrt (w[c]);
            tLev = vals[ink::Level] * vals[ink::Level];
            tPan = vals[ink::Pan];
            satGains (vals[ink::Drive], depth, tPre, tPost);

            if (i == 0 && publishUi)
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
        const float* in = addOut.getReadPointer (ch);
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
    const int typeIdx = juce::jlimit (0, 2, builtType);
    const float wetGain = mix * kTailCalibration * juce::Decibels::decibelsToGain (pOutput->load() + kTypeTrimDb[typeIdx]);
    const float dryGain = solo ? 0.0f : 1.0f;
    float energy = 0.0f;

    float* outL = addOut.getWritePointer (0);
    float* outR = addOut.getWritePointer (1);
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
        float gL, gR;
        panGains (pPan[(size_t) i], gL, gR);
        const float g = pLev[(size_t) i] * wetGain;
        l *= g * gL;
        r *= g * gR;
        energy += l * l + r * r;
        outL[i] = outL[i] * dryGain + l;
        outR[i] = outR[i] * dryGain + r;
    }

    if (publishUi)
        uiEnergy.store (juce::jmin (1.0f, std::sqrt (energy / (2.0f * (float) juce::jmax (1, len))) * 7.0f));
}

void InkroomProcessor::processShape (int len, double ppq0, double pps, bool publishUi)
{
    const float mix = pMix->load(), drift = pDrift->load(), depth = pDepth->load();
    const int charType = (int) pChar->load();
    const bool solo = pSolo->load() > 0.5f;
    const float hitsGain = solo ? 0.0f : juce::Decibels::decibelsToGain (pHits->load(), -24.0f);
    const float dryGain = solo ? 0.0f : 1.0f - mix;

    // 1. Lanes at control rate, smoothed per sample.
    const float coef = 1.0f - std::exp (-1.0f / (0.0007f * (float) sr));
    float tSus = hSus, tPunch = hPunch, tLev = hLev, tWidth = hWidth, tPan = hPan, tPre = hPre, tPost = hPost;
    for (int i = 0; i < len; ++i)
    {
        if (i % kControlStep == 0)
        {
            double p; long long k;
            loopPhase (ppq0 + i * pps, p, k);
            std::array<float, ink::NumShapeLanes> v {};
            for (int l = 0; l < ink::NumShapeLanes; ++l)
                v[(size_t) l] = laneValue (shapeLanes, l, p, k, drift);

            tSus = laneGain (v[ink::Sustain]);
            tPunch = laneGain (v[ink::Punch]);
            tLev = v[ink::SLevel] * v[ink::SLevel];
            tWidth = v[ink::Width] * 2.0f;
            tPan = v[ink::SPan];
            satGains (v[ink::SDrive], depth, tPre, tPost);

            if (i == 0 && publishUi)
            {
                uiPhase.store ((float) p);
                for (int c = 0; c < ink::NumShapeLanes; ++c)
                    uiVals[(size_t) c].store (v[(size_t) c]);
            }
        }
        hSus += coef * (tSus - hSus);       qSus[(size_t) i] = hSus;
        hPunch += coef * (tPunch - hPunch); qPunch[(size_t) i] = hPunch;
        hLev += coef * (tLev - hLev);       qLev[(size_t) i] = hLev;
        hWidth += coef * (tWidth - hWidth); qWidth[(size_t) i] = hWidth;
        hPan += coef * (tPan - hPan);       qPan[(size_t) i] = hPan;
        hPre += coef * (tPre - hPre);       qPre[(size_t) i] = hPre;
        hPost += coef * (tPost - hPost);    qPost[(size_t) i] = hPost;
    }

    // 2. Split each hit from the ring after it. Detection runs on the undelayed signal,
    //    so the mask opens just before the hit reaches the delayed audio.
    const float fsr = (float) sr;
    // Onset test: short-term power (1 ms) jumping above the recent level (40 ms) by a margin.
    // Sensitivity 0 needs a 14 dB jump, 50% needs 9 dB, 100% needs 4 dB.
    const float aShort = 1.0f - std::exp (-1.0f / (fsr * 0.001f)), aLong = 1.0f - std::exp (-1.0f / (fsr * 0.04f));
    const float up = 1.0f - std::exp (-1.0f / (fsr * 0.0008f)), down = std::exp (-1.0f / (fsr * 0.012f));
    const float jump = std::pow (10.0f, (14.0f - pSens->load() * 10.0f) / 10.0f), floor = 0.002f * 0.002f;
    const int hold = (int) (pHitLen->load() * 0.001f * fsr);

    const float* u0 = undelayed.getReadPointer (0);
    const float* u1 = undelayed.getReadPointer (1);
    const float* d0 = delayed.getReadPointer (0);
    const float* d1 = delayed.getReadPointer (1);
    float* o0 = shapeOut.getWritePointer (0);
    float* o1 = shapeOut.getWritePointer (1);
    float energy = 0.0f, hitPeak = 0.0f;

    for (int i = 0; i < len; ++i)
    {
        const float x = std::max (u0[i] * u0[i], u1[i] * u1[i]);
        detFast += aShort * (x - detFast);
        detSlow += aLong * (x - detSlow);
        if (detFast > floor && detFast > detSlow * jump && holdLeft < hold / 2)
            holdLeft = hold;
        const bool open = holdLeft > 0;
        if (holdLeft > 0)
            --holdLeft;
        mask = open ? mask + (1.0f - mask) * up : mask * down;
        hitPeak = std::max (hitPeak, mask);

        const float m = mask;
        const float hg = hitsGain * qPunch[(size_t) i];
        const float hl = d0[i] * m * hg, hr = d1[i] * m * hg;

        // Drive blends in with its amount, so a neutral Drive lane leaves the ring untouched.
        const float cl = d0[i] * (1.0f - m) * qSus[(size_t) i], cr = d1[i] * (1.0f - m) * qSus[(size_t) i];
        const float blend = juce::jlimit (0.0f, 1.0f, (qPre[(size_t) i] - 0.04f) / 0.7f);
        float rl = cl + (saturate (0, cl, qPre[(size_t) i], qPost[(size_t) i], charType) - cl) * blend;
        float rr = cr + (saturate (1, cr, qPre[(size_t) i], qPost[(size_t) i], charType) - cr) * blend;

        const float mid = (rl + rr) * 0.5f, side = (rl - rr) * 0.5f * qWidth[(size_t) i];
        rl = mid + side;
        rr = mid - side;
        float gL, gR;
        panGains (qPan[(size_t) i], gL, gR);

        const float wl = (hl + rl * gL) * qLev[(size_t) i];
        const float wr = (hr + rr * gR) * qLev[(size_t) i];
        energy += wl * wl + wr * wr;
        o0[i] = wl * mix + d0[i] * dryGain;
        o1[i] = wr * mix + d1[i] * dryGain;
    }

    if (publishUi)
    {
        uiEnergy.store (juce::jmin (1.0f, std::sqrt (energy / (2.0f * (float) juce::jmax (1, len))) * 4.0f));
        uiHit.store (hitPeak);
    }
}

juce::ValueTree InkroomProcessor::captureState()
{
    auto state = apvts.copyState();
    state.setProperty ("lanesAdd", addLanes.toBase64(), nullptr);
    state.setProperty ("lanesShape", shapeLanes.toBase64(), nullptr);
    state.setProperty ("presetAdd", currentPreset[0].load(), nullptr);
    state.setProperty ("presetShape", currentPreset[1].load(), nullptr);
    juce::StringArray linkText;
    for (int sp = 0; sp < 2; ++sp)
        for (int l = 0; l < ink::kMaxLanes; ++l)
            if (const auto& k = links[(size_t) sp][(size_t) l]; k.source >= 0)
                linkText.add (juce::String (sp) + ":" + juce::String (l) + ":" + juce::String (k.source) + ":" + (k.inverted ? "1" : "0"));
    state.setProperty ("links", linkText.joinIntoString (";"), nullptr);
    return state;
}

void InkroomProcessor::applyState (const juce::ValueTree& state, bool includeEditor)
{
    if (! state.hasType (apvts.state.getType()))
        return;
    // Sessions saved by 0.1 stored the Add space lanes as "lanes".
    addLanes.fromBase64 (state.getProperty (state.hasProperty ("lanesAdd") ? "lanesAdd" : "lanes").toString());
    shapeLanes.fromBase64 (state.getProperty ("lanesShape").toString());
    currentPreset[0].store ((int) state.getProperty ("presetAdd", state.getProperty ("preset", -1)));
    currentPreset[1].store ((int) state.getProperty ("presetShape", -1));
    clearLinks (0);
    clearLinks (1);
    for (auto& item : juce::StringArray::fromTokens (state.getProperty ("links").toString(), ";", ""))
    {
        auto parts = juce::StringArray::fromTokens (item, ":", "");
        if (parts.size() == 4)
            setLink (parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getIntValue(), parts[3] == "1");
    }
    if (includeEditor)
        editorScale.store ((float) state.getProperty ("editorScale", -1.0f));
    auto copy = state.createCopy();
    for (auto* prop : { "lanesAdd", "lanesShape", "lanes", "links", "editorScale", "presetAdd", "presetShape", "preset" })
        copy.removeProperty (prop, nullptr);
    apvts.replaceState (copy);
}

void InkroomProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = captureState();
    state.setProperty ("editorScale", editorScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void InkroomProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyState (juce::ValueTree::fromXml (*xml), true);
}

//==============================================================================
void InkroomProcessor::setLink (int sp, int lane, int source, bool inverted)
{
    if (sp < 0 || sp > 1 || lane < 0 || lane >= ink::kMaxLanes || source == lane || source >= lanesFor (sp).count)
        return;
    links[(size_t) sp][(size_t) lane] = { source, inverted };
    propagateLinks (sp);
}

void InkroomProcessor::clearLinks (int sp)
{
    for (auto& k : links[(size_t) sp])
        k = {};
}

void InkroomProcessor::propagateLinks (int sp)
{
    auto& lanes = lanesFor (sp);
    // Two passes so a lane linked to a linked lane also follows.
    for (int pass = 0; pass < 2; ++pass)
        for (int l = 0; l < lanes.count; ++l)
            if (const auto& k = links[(size_t) sp][(size_t) l]; k.source >= 0 && k.source < lanes.count)
                lanes.copyLane (k.source, l, k.inverted);
}

//==============================================================================
juce::File InkroomProcessor::userPresetFolder()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support/Imagine Plugins/Inkroom/Presets");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Imagine Plugins/Inkroom/Presets");
   #endif
}

juce::Array<InkroomProcessor::UserPreset> InkroomProcessor::userPresets (int sp) const
{
    juce::Array<UserPreset> list;
    for (const auto& f : userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.inkroom"))
        if (auto xml = juce::XmlDocument::parse (f))
        {
            const int fileSpace = xml->getChildByAttribute ("id", "space") != nullptr
                                      ? juce::roundToInt (xml->getChildByAttribute ("id", "space")->getDoubleAttribute ("value"))
                                      : 0;
            if (fileSpace == sp)
                list.add ({ f.getFileNameWithoutExtension(), f, fileSpace });
        }
    std::sort (list.begin(), list.end(), [] (const UserPreset& a, const UserPreset& b) { return a.name.compareNatural (b.name) < 0; });
    return list;
}

bool InkroomProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    auto folder = userPresetFolder();
    if (! folder.createDirectory())
        return false;
    auto state = captureState();
    state.setProperty ("presetAdd", -1, nullptr);
    state.setProperty ("presetShape", -1, nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (folder.getChildFile (clean + ".inkroom"));
    return false;
}

bool InkroomProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        applyState (juce::ValueTree::fromXml (*xml), false);
        return true;
    }
    return false;
}

juce::AudioProcessorEditor* InkroomProcessor::createEditor()
{
    return new InkroomEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InkroomProcessor();
}
