#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Dunkles Rhythmcomputer-Design: schwarze Drehknöpfe mit farbigem Zeiger, Step-Tasten mit Farbgruppen.
class DrumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DrumLookAndFeel()
    {
        const auto light = juce::Colour (0xffd8d8dc);
        setColour (juce::Slider::textBoxTextColourId, light);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffee7f2d));
        setColour (juce::Label::textColourId, light);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3b40));
        setColour (juce::TextButton::textColourOffId, light);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff111111));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const float size = juce::jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float outer = size * 0.5f;
        const float knobR = outer * 0.74f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto pointer = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // Skala
        g.setColour (juce::Colour (0xff8a8c92));
        for (int i = 0; i <= 8; ++i)
        {
            const float a = startAngle + (float) i / 8.0f * (endAngle - startAngle);
            const float s = std::sin (a), co = std::cos (a);
            const float r1 = outer * 0.86f, r2 = outer * (i % 4 == 0 ? 1.0f : 0.95f);
            g.drawLine (c.x + s * r1, c.y - co * r1, c.x + s * r2, c.y - co * r2, i % 4 == 0 ? 1.5f : 1.0f);
        }

        // Schatten + Rand
        g.setColour (juce::Colour (0x66000000));
        g.fillEllipse (c.x - knobR + 1.0f, c.y - knobR + 2.5f, knobR * 2.0f, knobR * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff80828a), c.x, c.y - knobR,
                                                 juce::Colour (0xff25262a), c.x, c.y + knobR, false));
        g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);

        // Körper
        const float bodyR = knobR * 0.88f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff484950), c.x - bodyR * 0.3f, c.y - bodyR * 0.4f,
                                                 juce::Colour (0xff0e0e10), c.x + bodyR, c.y + bodyR, true));
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        // Zeiger
        const float s = std::sin (angle), co = std::cos (angle);
        g.setColour (pointer);
        g.drawLine (c.x + s * bodyR * 0.25f, c.y - co * bodyR * 0.25f,
                    c.x + s * bodyR * 0.95f, c.y - co * bodyR * 0.95f, 2.6f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = bg;
        if (down)      base = base.darker (0.25f);
        else if (over) base = base.brighter (0.12f);

        g.setGradientFill (juce::ColourGradient (base.brighter (0.2f), 0.0f, r.getY(),
                                                 base.darker (0.25f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.5f);

        // Ausgeschaltete Toggle-Tasten zeigen ihre Farbe als schmalen Streifen
        if (b.getClickingTogglesState() && ! b.getToggleState())
        {
            g.setColour (b.findColour (juce::TextButton::buttonOnColourId).withAlpha (0.85f));
            g.fillRoundedRectangle (r.getX() + 3.0f, r.getBottom() - 5.0f, r.getWidth() - 6.0f, 2.5f, 1.2f);
        }
        else if (b.getToggleState())
        {
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.drawLine (r.getX() + 4.0f, r.getY() + 2.0f, r.getRight() - 4.0f, r.getY() + 2.0f, 1.0f);
        }

        g.setColour (juce::Colour (0xff08080a));
        g.drawRoundedRectangle (r, 3.5f, 1.0f);
    }
};
