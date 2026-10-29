#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
    // Start-Pattern (0-basierte Steps, 1 = an)
    const bool defStep[6][16] = {
        { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 },   // Bassdrum
        { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },   // Snare
        { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,0,0 },   // Closed Hat
        { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0 },   // Open Hat
        { 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0 },   // Clap
        { 0,0,0,0, 0,0,0,1, 0,0,1,0, 0,0,0,0 }    // Rimshot
    };
    const bool defAcc[16] = { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0 };

    const char* const kTrackKey[6] = { "bd", "sd", "ch", "oh", "cp", "rs" };

    float panL (float pan) { return std::cos ((pan + 1.0f) * 0.78539816f) * 1.41421356f; }
    float panR (float pan) { return std::sin ((pan + 1.0f) * 0.78539816f) * 1.41421356f; }
}

DrumProcessor::DrumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    const char* kickIds[7]  = { "bd_tune", "bd_decay", "bd_punch", "bd_click", "bd_drive", "bd_level", "bd_pan" };
    const char* snareIds[6] = { "sd_tune", "sd_tone", "sd_snappy", "sd_decay", "sd_level", "sd_pan" };
    const char* chIds[5]    = { "ch_tune", "ch_decay", "ch_tone", "ch_level", "ch_pan" };
    const char* ohIds[5]    = { "oh_tune", "oh_decay", "oh_tone", "oh_level", "oh_pan" };
    const char* clapIds[5]  = { "cp_tone", "cp_decay", "cp_spread", "cp_level", "cp_pan" };
    const char* rimIds[4]   = { "rs_tune", "rs_decay", "rs_level", "rs_pan" };

    for (int i = 0; i < 7; ++i) kickP[i]  = get (kickIds[i]);
    for (int i = 0; i < 6; ++i) snareP[i] = get (snareIds[i]);
    for (int i = 0; i < 5; ++i) chP[i]    = get (chIds[i]);
    for (int i = 0; i < 5; ++i) ohP[i]    = get (ohIds[i]);
    for (int i = 0; i < 5; ++i) clapP[i]  = get (clapIds[i]);
    for (int i = 0; i < 4; ++i) rimP[i]   = get (rimIds[i]);

    pMaster = get ("master"); pAccAmt = get ("accamt"); pBpm = get ("bpm");
    pSwing = get ("swing");   pLength = get ("length"); pRun = get ("run"); pSync = get ("sync");

    for (int t = 0; t < kTracks; ++t)
        for (int i = 0; i < kSteps; ++i)
            pStep[t][i] = get ("st" + juce::String (t) + "_" + juce::String (i));
    for (int i = 0; i < kSteps; ++i)
        pAcc[i] = get ("acc" + juce::String (i));
}

APVTS::ParameterLayout DrumProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using I = juce::AudioParameterInt;
    using R = juce::NormalisableRange<float>;
    APVTS::ParameterLayout l;

    auto addF = [&l] (const juce::String& id, const juce::String& name, float lo, float hi, float def, float skew = 1.0f)
    {
        l.add (std::make_unique<F> (juce::ParameterID { id, 1 }, name, R (lo, hi, 0.0f, skew), def));
    };

    // Bassdrum
    addF ("bd_tune",  "Kick Tune",  30.f, 120.f, 52.f);
    addF ("bd_decay", "Kick Decay", 0.1f, 1.5f, 0.55f, 0.5f);
    addF ("bd_punch", "Kick Punch", 0.f, 1.f, 0.55f);
    addF ("bd_click", "Kick Click", 0.f, 1.f, 0.35f);
    addF ("bd_drive", "Kick Drive", 0.f, 1.f, 0.25f);
    addF ("bd_level", "Kick Level", 0.f, 1.f, 0.9f);
    addF ("bd_pan",   "Kick Pan",  -1.f, 1.f, 0.f);

    // Snare
    addF ("sd_tune",   "Snare Tune",   120.f, 400.f, 190.f);
    addF ("sd_tone",   "Snare Tone",   0.f, 1.f, 0.5f);
    addF ("sd_snappy", "Snare Snappy", 0.f, 1.f, 0.65f);
    addF ("sd_decay",  "Snare Decay",  0.05f, 0.8f, 0.22f, 0.5f);
    addF ("sd_level",  "Snare Level",  0.f, 1.f, 0.85f);
    addF ("sd_pan",    "Snare Pan",   -1.f, 1.f, 0.f);

    // Closed Hat
    addF ("ch_tune",  "CHat Tune",  0.7f, 1.5f, 1.0f);
    addF ("ch_decay", "CHat Decay", 0.02f, 0.3f, 0.06f, 0.5f);
    addF ("ch_tone",  "CHat Tone",  0.f, 1.f, 0.5f);
    addF ("ch_level", "CHat Level", 0.f, 1.f, 0.7f);
    addF ("ch_pan",   "CHat Pan",  -1.f, 1.f, 0.f);

    // Open Hat
    addF ("oh_tune",  "OHat Tune",  0.7f, 1.5f, 1.0f);
    addF ("oh_decay", "OHat Decay", 0.15f, 1.5f, 0.45f, 0.5f);
    addF ("oh_tone",  "OHat Tone",  0.f, 1.f, 0.5f);
    addF ("oh_level", "OHat Level", 0.f, 1.f, 0.7f);
    addF ("oh_pan",   "OHat Pan",  -1.f, 1.f, 0.f);

    // Clap
    addF ("cp_tone",   "Clap Tone",   500.f, 3000.f, 1100.f, 0.5f);
    addF ("cp_decay",  "Clap Decay",  0.1f, 0.8f, 0.3f, 0.5f);
    addF ("cp_spread", "Clap Spread", 0.004f, 0.02f, 0.011f);
    addF ("cp_level",  "Clap Level",  0.f, 1.f, 0.8f);
    addF ("cp_pan",    "Clap Pan",   -1.f, 1.f, 0.f);

    // Rimshot
    addF ("rs_tune",  "Rim Tune",  250.f, 1200.f, 520.f);
    addF ("rs_decay", "Rim Decay", 0.01f, 0.12f, 0.035f);
    addF ("rs_level", "Rim Level", 0.f, 1.f, 0.8f);
    addF ("rs_pan",   "Rim Pan",  -1.f, 1.f, 0.f);

    // Global
    addF ("master", "Master",  0.f, 1.f, 0.8f);
    addF ("accamt", "Accent",  0.f, 1.f, 0.5f);
    addF ("bpm",    "Tempo",   50.f, 220.f, 124.f);
    addF ("swing",  "Swing",   0.f, 1.f, 0.f);
    l.add (std::make_unique<I> (juce::ParameterID { "length", 1 }, "Length", 1, 16, 16));
    l.add (std::make_unique<B> (juce::ParameterID { "run", 1 },  "Run",       false));
    l.add (std::make_unique<B> (juce::ParameterID { "sync", 1 }, "Host Sync", false));

    // Steps
    for (int t = 0; t < kTracks; ++t)
        for (int i = 0; i < kSteps; ++i)
            l.add (std::make_unique<B> (juce::ParameterID { "st" + juce::String (t) + "_" + juce::String (i), 1 },
                                        juce::String (kTrackKey[t]).toUpperCase() + " Step " + juce::String (i + 1),
                                        defStep[t][i]));
    for (int i = 0; i < kSteps; ++i)
        l.add (std::make_unique<B> (juce::ParameterID { "acc" + juce::String (i), 1 },
                                    "Accent Step " + juce::String (i + 1), defAcc[i]));
    return l;
}

juce::AudioProcessorEditor* DrumProcessor::createEditor() { return new DrumEditor (*this); }

void DrumProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    kick.prepare (sr); snare.prepare (sr); closedHat.prepare (sr); openHat.prepare (sr);
    clap.prepare (sr); rim.prepare (sr);
        wasRunning = false;
    currentStep.store (-1);
}

void DrumProcessor::updateVoiceParams()
{
    kick.p  = { kickP[0]->load(), kickP[1]->load(), kickP[2]->load(), kickP[3]->load(), kickP[4]->load() };
    snare.p = { snareP[0]->load(), snareP[1]->load(), snareP[2]->load(), snareP[3]->load() };
    closedHat.p = { chP[0]->load(), chP[1]->load(), chP[2]->load() };
    openHat.p   = { ohP[0]->load(), ohP[1]->load(), ohP[2]->load() };
    clap.p = { clapP[0]->load(), clapP[1]->load(), clapP[2]->load() };
    rim.p  = { rimP[0]->load(), rimP[1]->load() };

    lvl[0] = kickP[5]->load();  gainL[0] = panL (kickP[6]->load());  gainR[0] = panR (kickP[6]->load());
    lvl[1] = snareP[4]->load(); gainL[1] = panL (snareP[5]->load()); gainR[1] = panR (snareP[5]->load());
    lvl[2] = chP[3]->load();    gainL[2] = panL (chP[4]->load());    gainR[2] = panR (chP[4]->load());
    lvl[3] = ohP[3]->load();    gainL[3] = panL (ohP[4]->load());    gainR[3] = panR (ohP[4]->load());
    lvl[4] = clapP[3]->load();  gainL[4] = panL (clapP[4]->load());  gainR[4] = panR (clapP[4]->load());
    lvl[5] = rimP[2]->load();   gainL[5] = panL (rimP[3]->load());   gainR[5] = panR (rimP[3]->load());
}

void DrumProcessor::triggerVoice (int track, float vel)
{
    switch (track)
    {
        case 0: kick.trigger (vel); break;
        case 1: snare.trigger (vel); break;
        case 2: openHat.choke(); closedHat.trigger (vel); break;   // Closed würgt Open ab
        case 3: openHat.trigger (vel); break;
        case 4: clap.trigger (vel); break;
        case 5: rim.trigger (vel); break;
        default: break;
    }
}

void DrumProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (! m.isNoteOn()) return;
    const float vel = juce::jmax (0.1f, m.getFloatVelocity());
    switch (m.getNoteNumber())          // General-MIDI-Drumbelegung
    {
        case 35: case 36: triggerVoice (0, vel); break;
        case 38: case 40: triggerVoice (1, vel); break;
        case 42: case 44: triggerVoice (2, vel); break;
        case 46:          triggerVoice (3, vel); break;
        case 39:          triggerVoice (4, vel); break;
        case 37:          triggerVoice (5, vel); break;
        default: break;
    }
}

void DrumProcessor::triggerSeqStep (int stepIndex, int length)
{
    const int idx = ((stepIndex % length) + length) % length;
    currentStep.store (idx);

    const bool acc = pAcc[idx]->load() > 0.5f;
    const float vel = acc ? 1.0f : 1.0f - 0.5f * pAccAmt->load();

    for (int t = 0; t < kTracks; ++t)
        if (pStep[t][idx]->load() > 0.5f)
            triggerVoice (t, vel);
}

void DrumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int n = buffer.getNumSamples();
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    updateVoiceParams();

    // Pads aus dem Editor
    const unsigned pads = padMask.exchange (0);
    for (int t = 0; t < kTracks; ++t)
        if (pads & (1u << t)) triggerVoice (t, 1.0f);

    // Transport
    const bool runParam  = pRun->load()  > 0.5f;
    const bool syncParam = pSync->load() > 0.5f;
    double bpm = pBpm->load();
    bool hostPlaying = true, haveHostPpq = false;
    double hostPpq = 0.0;

    if (syncParam)
    {
        hostPlaying = false;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                hostPlaying = pos->getIsPlaying();
                if (auto b = pos->getBpm())         bpm = *b;
                if (auto p = pos->getPpqPosition()) { hostPpq = *p; haveHostPpq = true; }
            }
        }
    }

    const bool running = runParam && (! syncParam || hostPlaying);
    const double inc = bpm * 4.0 / (60.0 * sr);                  // 16tel-Schritte pro Sample
    const int length = juce::jlimit (1, kSteps, (int) std::lround (pLength->load()));
    const double swingOff = pSwing->load() * 0.5;                // Verzögerung der ungeraden Steps

    if (running && ! wasRunning)
    {
        stepPos = (syncParam && haveHostPpq) ? hostPpq * 4.0 : 0.0;
        nextStep = (int) std::ceil (stepPos - 1.0e-6);
    }
    else if (! running && wasRunning)
    {
        currentStep.store (-1);
    }
    else if (running && syncParam && haveHostPpq)
    {
        const double hs = hostPpq * 4.0;
        if (std::abs (hs - stepPos) > 0.05)
        {
            stepPos = hs;
            nextStep = (int) std::ceil (hs - 1.0e-6);
        }
    }
    wasRunning = running;

    const float master = pMaster->load();
    auto midiIt = midi.begin();
    const auto midiEnd = midi.end();

    for (int i = 0; i < n; ++i)
    {
        while (midiIt != midiEnd && (*midiIt).samplePosition <= i)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }

        if (running)
        {
            while (stepPos >= (double) nextStep + ((nextStep & 1) ? swingOff : 0.0))
            {
                triggerSeqStep (nextStep, length);
                ++nextStep;
            }
            stepPos += inc;
        }

        const float v[kTracks] = { kick.process(), snare.process(), closedHat.process(),
                                   openHat.process(), clap.process(), rim.process() };
        float l = 0.0f, r = 0.0f;
        for (int t = 0; t < kTracks; ++t)
        {
            const float s = v[t] * lvl[t];
            l += s * gainL[t];
            r += s * gainR[t];
        }

        outL[i] = std::tanh (l * master);
        if (outR) outR[i] = std::tanh (r * master);
    }
}

void DrumProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void DrumProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumProcessor(); }
