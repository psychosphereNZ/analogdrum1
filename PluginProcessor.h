#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "DrumVoices.h"

// Analoger Drumcomputer: Bassdrum, Snare, Closed/Open Hi-Hat, Clap, Rimshot
// mit 16-Step-Sequencer (Accent, Swing, Host-Sync) und MIDI-Eingang.
class DrumProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kTracks = 6;   // BD, SD, CH, OH, CP, RS
    static constexpr int kSteps  = 16;

    DrumProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AnalogDrums"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> currentStep { -1 };       // Lauflicht-Anzeige
    std::atomic<unsigned> padMask { 0 };       // Vorhören aus dem Editor (Bit = Spur)

private:
    using P = std::atomic<float>*;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void updateVoiceParams();
    void triggerVoice (int track, float velocity);
    void handleMidi (const juce::MidiMessage&);
    void triggerSeqStep (int stepIndex, int length);

    double sr = 44100.0;

    drums::Kick  kick;
    drums::Snare snare;
    drums::Hat   closedHat, openHat;
    drums::Clap  clap;
    drums::Rim   rim;

    float lvl[kTracks] {}, gainL[kTracks] {}, gainR[kTracks] {};

    // Sequencer
    double stepPos = 0.0;
    int nextStep = 0;
    bool wasRunning = false;

    // Gecachte Parameterzeiger
    P kickP[7] {}, snareP[6] {}, chP[5] {}, ohP[5] {}, clapP[5] {}, rimP[4] {};
    P pMaster {}, pAccAmt {}, pBpm {}, pSwing {}, pLength {}, pRun {}, pSync {};
    P pStep[kTracks][kSteps] {};
    P pAcc[kSteps] {};
};
