#include "PluginEditor.h"

namespace
{
    const juce::Colour kRed    (0xffd9342b);
    const juce::Colour kOrange (0xffee7f2d);
    const juce::Colour kYellow (0xfff0c93a);
    const juce::Colour kCream  (0xfff2ecd8);
    const juce::Colour kBlue   (0xff6fb7e9);
    const juce::Colour kGreen  (0xff7bc96f);
    const juce::Colour kLight  (0xffd8d8dc);
    const juce::Colour kLabelCol (0xff9a9ca3);
    const int kLabelW = 66;

    struct KnobDef { const char* id; const char* label; };
    struct VoiceDef { const char* name; const char* key; juce::Colour colour; std::vector<KnobDef> knobs; };

    const std::vector<VoiceDef>& voices()
    {
        static const std::vector<VoiceDef> v = {
            { "BASS DRUM", "BD", kRed,    { { "bd_tune", "TUNE" }, { "bd_decay", "DECAY" }, { "bd_punch", "PUNCH" }, { "bd_click", "CLICK" },
                                            { "bd_drive", "DRIVE" }, { "bd_level", "LEVEL" }, { "bd_pan", "PAN" } } },
            { "SNARE",     "SD", kOrange, { { "sd_tune", "TUNE" }, { "sd_tone", "TONE" }, { "sd_snappy", "SNAPPY" }, { "sd_decay", "DECAY" },
                                            { "sd_level", "LEVEL" }, { "sd_pan", "PAN" } } },
            { "CLOSED HAT", "CH", kYellow, { { "ch_tune", "TUNE" }, { "ch_decay", "DECAY" }, { "ch_tone", "TONE" },
                                             { "ch_level", "LEVEL" }, { "ch_pan", "PAN" } } },
            { "OPEN HAT",  "OH", kCream,  { { "oh_tune", "TUNE" }, { "oh_decay", "DECAY" }, { "oh_tone", "TONE" },
                                            { "oh_level", "LEVEL" }, { "oh_pan", "PAN" } } },
            { "HAND CLAP", "CP", kBlue,   { { "cp_tone", "TONE" }, { "cp_decay", "DECAY" }, { "cp_spread", "SPREAD" },
                                            { "cp_level", "LEVEL" }, { "cp_pan", "PAN" } } },
            { "RIMSHOT",   "RS", kGreen,  { { "rs_tune", "TUNE" }, { "rs_decay", "DECAY" }, { "rs_level", "LEVEL" }, { "rs_pan", "PAN" } } }
        };
        return v;
    }

    juce::Colour groupColour (int step)
    {
        static const juce::Colour c[4] = { kRed, kOrange, kYellow, kCream };
        return c[(step / 4) % 4];
    }
}

DrumEditor::DrumEditor (DrumProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    auto makeKnob = [this] (const char* id, const char* text, juce::Colour pointer)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 14);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, pointer);
        k->label.setText (text, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->att = std::make_unique<SliderAtt> (proc.apvts, id, k->slider);
        const juce::String sid (id);
        if (sid != "length")
        {
            k->slider.textFromValueFunction = [] (double v)
            {
                const double a = std::abs (v);
                return juce::String (v, a >= 100.0 ? 0 : (a >= 10.0 ? 1 : 2));
            };
            k->slider.updateText();
        }
        return k;
    };

    int track = 0;
    for (const auto& vd : voices())
    {
        auto s = std::make_unique<Strip>();
        s->pad.setButtonText (vd.name);
        s->pad.setColour (juce::TextButton::buttonColourId, vd.colour);
        s->pad.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff141416));
        const int t = track;
        s->pad.onClick = [this, t] { proc.padMask.fetch_or (1u << t); };
        addAndMakeVisible (s->pad);
        for (const auto& kd : vd.knobs)
            s->knobs.push_back (makeKnob (kd.id, kd.label, vd.colour));
        strips.push_back (std::move (s));
        ++track;
    }

    globalKnobs.push_back (makeKnob ("bpm",    "TEMPO",  kOrange));
    globalKnobs.push_back (makeKnob ("swing",  "SWING",  kOrange));
    globalKnobs.push_back (makeKnob ("length", "LENGTH", kOrange));
    globalKnobs.push_back (makeKnob ("accamt", "ACCENT", kOrange));
    globalKnobs.push_back (makeKnob ("master", "MASTER", kRed));

    for (auto* b : { &runBtn, &syncBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    runBtn.setColour (juce::TextButton::buttonOnColourId, kOrange);
    syncBtn.setColour (juce::TextButton::buttonOnColourId, kBlue);
    runAtt  = std::make_unique<ButtonAtt> (proc.apvts, "run", runBtn);
    syncAtt = std::make_unique<ButtonAtt> (proc.apvts, "sync", syncBtn);

    addAndMakeVisible (randBtn);
    addAndMakeVisible (clearBtn);
    randBtn.onClick  = [this] { randomizePattern(); };
    clearBtn.onClick = [this] { clearPattern(); };

    for (int t = 0; t < 6; ++t)
        for (int i = 0; i < 16; ++i)
        {
            auto& b = stepBtn[(size_t) t][(size_t) i];
            b.setClickingTogglesState (true);
            b.setColour (juce::TextButton::buttonOnColourId, groupColour (i));
            addAndMakeVisible (b);
            stepAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, "st" + juce::String (t) + "_" + juce::String (i), b));
        }
    for (int i = 0; i < 16; ++i)
    {
        auto& b = accBtn[(size_t) i];
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff3b30));
        addAndMakeVisible (b);
        stepAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, "acc" + juce::String (i), b));
    }

    setSize (940, 700);   // erst nach dem Anlegen aller Komponenten
    startTimerHz (30);
}

DrumEditor::~DrumEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DrumEditor::setParam (const juce::String& id, bool on)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

void DrumEditor::randomizePattern()
{
    juce::Random rng;
    // Wahrscheinlichkeiten für Hauptschläge (Steps 0,4,8,12) und die übrigen Steps
    static const float onBeat[6]  = { 0.95f, 0.10f, 0.35f, 0.02f, 0.05f, 0.05f };
    static const float offBeat[6] = { 0.15f, 0.08f, 0.55f, 0.08f, 0.04f, 0.12f };

    for (int t = 0; t < 6; ++t)
        for (int i = 0; i < 16; ++i)
        {
            float prob = (i % 4 == 0) ? onBeat[t] : offBeat[t];
            if (t == 1 && (i == 4 || i == 12)) prob = 0.9f;
            if (t == 4 && (i == 4 || i == 12)) prob = 0.4f;
            if (t == 2 && (i % 4 == 2))        prob = 0.8f;
            setParam ("st" + juce::String (t) + "_" + juce::String (i), rng.nextFloat() < prob);
        }
    for (int i = 0; i < 16; ++i)
        setParam ("acc" + juce::String (i), rng.nextFloat() < 0.15f);
}

void DrumEditor::clearPattern()
{
    for (int t = 0; t < 6; ++t)
        for (int i = 0; i < 16; ++i)
            setParam ("st" + juce::String (t) + "_" + juce::String (i), false);
    for (int i = 0; i < 16; ++i)
        setParam ("acc" + juce::String (i), false);
}

void DrumEditor::timerCallback()
{
    const int c = proc.currentStep.load();
    if (c != shownStep)
    {
        shownStep = c;
        repaint();
    }
}

void DrumEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2f34), 0.0f, 0.0f,
                                             juce::Colour (0xff18191c), 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Titel
    g.setColour (juce::Colour (0xfff2f2f4));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("ANALOG DRUMS", 14, 8, 260, 28, juce::Justification::left);
    g.setColour (kLabelCol);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("RHYTHM COMPOSER  /  6 VOICES  /  16 STEPS", 220, 14, 360, 18, juce::Justification::left);

    // Farbstreifen wie bei klassischen Rhythmusmaschinen
    const int sx = 14, sw = (getWidth() - 28) / 4;
    const juce::Colour stripe[4] = { kRed, kOrange, kYellow, kCream };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (stripe[i]);
        g.fillRect (sx + i * sw, 38, sw, 3);
    }

    // Stimm-Panels
    for (size_t i = 0; i < stripRects.size(); ++i)
    {
        g.setColour (juce::Colour (0x22ffffff));
        g.fillRoundedRectangle (stripRects[i].toFloat(), 5.0f);
        g.setColour (voices()[i].colour.withAlpha (0.8f));
        g.fillRect (stripRects[i].getX() + 6, stripRects[i].getY() + 2, stripRects[i].getWidth() - 12, 2);
    }

    // Step-Bereich
    g.setColour (juce::Colour (0x22000000));
    g.fillRoundedRectangle (10.0f, (float) gridTop - 6.0f, (float) getWidth() - 20.0f,
                            (float) (gridBottom - gridTop) + 12.0f, 6.0f);

    if (shownStep >= 0 && shownStep < 16)
    {
        g.setColour (juce::Colour (0x33ffffff));
        g.fillRoundedRectangle ((float) (gridLeft + shownStep * colW), (float) gridTop,
                                (float) colW, (float) (gridBottom - gridTop), 4.0f);
    }

    // Lauflicht
    for (int i = 0; i < 16; ++i)
    {
        const float cx = (float) (gridLeft + i * colW) + (float) colW * 0.5f;
        const float cy = (float) gridTop + 7.0f;
        const bool lit = (i == shownStep);
        if (lit)
        {
            g.setColour (juce::Colour (0x59ff3b30));
            g.fillEllipse (cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
        }
        g.setColour (lit ? juce::Colour (0xffff3b30) : juce::Colour (0xff4a2422));
        g.fillEllipse (cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0xff08080a));
        g.drawEllipse (cx - 4.5f, cy - 4.5f, 9.0f, 9.0f, 1.0f);
    }

    // Zeilenbeschriftung
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (int t = 0; t < 6; ++t)
    {
        g.setColour (voices()[(size_t) t].colour);
        const auto& b = stepBtn[(size_t) t][0];
        g.drawText (voices()[(size_t) t].key, 18, b.getY(), kLabelW - 8, b.getHeight(), juce::Justification::centredLeft);
    }
    g.setColour (juce::Colour (0xffff3b30));
    g.drawText ("ACC", 18, accBtn[0].getY(), kLabelW - 8, accBtn[0].getHeight(), juce::Justification::centredLeft);

    // Taktnummern
    g.setColour (kLabelCol);
    g.setFont (juce::FontOptions (9.5f));
    for (int i = 0; i < 16; i += 4)
        g.drawText (juce::String (i / 4 + 1), gridLeft + i * colW, gridTop + 14, colW, 12, juce::Justification::centred);
}

void DrumEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (34);

    // Stimm-Streifen
    auto stripArea = r.removeFromTop (314);
    const int sw = stripArea.getWidth() / 6;
    for (size_t s = 0; s < strips.size(); ++s)
    {
        auto area = stripArea.removeFromLeft (sw).reduced (3, 0);
        stripRects[s] = area;
        area.removeFromTop (6);
        strips[s]->pad.setBounds (area.removeFromTop (26).reduced (6, 0));
        area.removeFromTop (4);
        const int cw = area.getWidth() / 2;
        for (size_t k = 0; k < strips[s]->knobs.size(); ++k)
        {
            auto cell = juce::Rectangle<int> (area.getX() + (int) (k % 2) * cw,
                                              area.getY() + (int) (k / 2) * 70, cw, 70);
            strips[s]->knobs[k]->label.setBounds (cell.removeFromTop (13));
            strips[s]->knobs[k]->slider.setBounds (cell);
        }
    }

    r.removeFromTop (8);

    // Transport
    auto rowB = r.removeFromTop (84);
    auto left = rowB.removeFromLeft (230);
    auto l1 = left.removeFromTop (38);
    runBtn.setBounds  (l1.removeFromLeft (110).reduced (2));
    syncBtn.setBounds (l1.removeFromLeft (110).reduced (2));
    left.removeFromTop (4);
    auto l2 = left.removeFromTop (34);
    randBtn.setBounds  (l2.removeFromLeft (110).reduced (2));
    clearBtn.setBounds (l2.removeFromLeft (110).reduced (2));
    const int kw = rowB.getWidth() / (int) globalKnobs.size();
    for (auto& k : globalKnobs)
    {
        auto cell = rowB.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (13));
        k->slider.setBounds (cell);
    }

    r.removeFromTop (10);

    // Step-Raster
    gridTop = r.getY();
    gridBottom = r.getBottom();
    auto grid = r;
    grid.removeFromLeft (kLabelW);
    gridLeft = grid.getX();
    colW = grid.getWidth() / 16;
    grid.removeFromTop (26);     // Lauflicht + Taktnummern

    for (int t = 0; t < 6; ++t)
    {
        auto row = grid.removeFromTop (24);
        for (int i = 0; i < 16; ++i)
            stepBtn[(size_t) t][(size_t) i].setBounds (row.removeFromLeft (colW).reduced (2, 0));
        grid.removeFromTop (3);
    }
    grid.removeFromTop (3);
    auto accRow = grid.removeFromTop (24);
    for (int i = 0; i < 16; ++i)
        accBtn[(size_t) i].setBounds (accRow.removeFromLeft (colW).reduced (2, 0));
}
