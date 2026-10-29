#pragma once
#include "PluginProcessor.h"
#include "DrumLookAndFeel.h"
#include <array>
#include <memory>
#include <vector>

class DrumEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrumEditor (DrumProcessor&);
    ~DrumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void randomizePattern();
    void clearPattern();
    void setParam (const juce::String& id, bool on);

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    struct Strip
    {
        juce::TextButton pad;
        std::vector<std::unique_ptr<Knob>> knobs;
    };

    DrumLookAndFeel laf;   // muss vor allen Komponenten deklariert sein
    DrumProcessor& proc;

    std::vector<std::unique_ptr<Strip>> strips;
    std::vector<std::unique_ptr<Knob>> globalKnobs;

    juce::TextButton runBtn { "RUN" }, syncBtn { "HOST SYNC" }, randBtn { "RANDOM" }, clearBtn { "CLEAR" };
    std::unique_ptr<ButtonAtt> runAtt, syncAtt;

    std::array<std::array<juce::TextButton, 16>, 6> stepBtn;
    std::array<juce::TextButton, 16> accBtn;
    std::vector<std::unique_ptr<ButtonAtt>> stepAtts;

    std::array<juce::Rectangle<int>, 6> stripRects;
    int shownStep = -2;
    int gridTop = 0, gridBottom = 0, gridLeft = 0, colW = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEditor)
};
