#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    const juce::Colour orange (0xffffa63d);

    lnf.setColour (juce::Slider::rotarySliderFillColourId,    orange);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a4258));
    lnf.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
    lnf.setColour (juce::Slider::textBoxTextColourId,         juce::Colour (0xffd0d6e6));
    lnf.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    lnf.setColour (juce::Label::textColourId,                 juce::Colour (0xff9aa3bb));
    lnf.setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0xff10141d));
    lnf.setColour (juce::ComboBox::textColourId,              juce::Colour (0xffd0d6e6));
    lnf.setColour (juce::ComboBox::outlineColourId,           juce::Colour (0xff343c52));
    lnf.setColour (juce::ComboBox::arrowColourId,             orange);
    lnf.setColour (juce::ToggleButton::textColourId,          juce::Colour (0xffd0d6e6));
    lnf.setColour (juce::ToggleButton::tickColourId,          orange);
    lnf.setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (0xff10141d));
    setLookAndFeel (&lnf);

    auto knob = [this] (const juce::String& id, const juce::String& name) -> juce::Component&
    {
        controls.push_back (std::make_unique<KnobControl> (proc.apvts, id, name));
        return *controls.back();
    };
    auto combo = [this] (const juce::String& id, const juce::String& name, const juce::StringArray& items) -> juce::Component&
    {
        controls.push_back (std::make_unique<ComboControl> (proc.apvts, id, name, items));
        return *controls.back();
    };
    auto toggle = [this] (const juce::String& id, const juce::String& text) -> juce::Component&
    {
        controls.push_back (std::make_unique<ToggleControl> (proc.apvts, id, text));
        return *controls.back();
    };
    auto makePanel = [this] (const juce::String& title, int cols, std::vector<Panel*>& row) -> Panel&
    {
        panels.push_back (std::make_unique<Panel> (title, cols));
        addAndMakeVisible (*panels.back());
        row.push_back (panels.back().get());
        return *panels.back();
    };

    // ---------- Oscillators ----------
    for (int i = 0; i < kNumOsc; ++i)
    {
        auto& pnl = makePanel ("Osc " + juce::String (i + 1), 2, oscRow);
        pnl.add (toggle (oscId (i, "on"), "On"));
        pnl.add (combo  (oscId (i, "wave"), "Wave", waveNames()));
        pnl.add (knob   (oscId (i, "oct"),   "Octave"));
        pnl.add (knob   (oscId (i, "semi"),  "Semi"));
        pnl.add (knob   (oscId (i, "fine"),  "Fine"));
        pnl.add (knob   (oscId (i, "level"), "Level"));
        pnl.add (knob   (oscId (i, "pan"),   "Pan"));
        pnl.add (knob   (oscId (i, "pw"),    "Pulse W"));
    }

    // ---------- Row 2: Unison / Filter / Envelopes ----------
    {
        auto& u = makePanel ("Unison", 3, row2);
        u.add (knob ("uni_voices", "Voices"));
        u.add (knob ("uni_detune", "Detune"));
        u.add (knob ("uni_spread", "Spread"));

        auto& f = makePanel ("Filter", 5, row2);
        f.add (combo ("f_type", "Type", filterTypeNames()));
        f.add (knob  ("f_cutoff", "Cutoff"));
        f.add (knob  ("f_res",    "Resonance"));
        f.add (knob  ("f_env",    "Env Amt"));
        f.add (knob  ("f_key",    "Key Track"));

        auto& a = makePanel ("Amp Envelope", 4, row2);
        a.add (knob ("amp_a", "Attack"));
        a.add (knob ("amp_d", "Decay"));
        a.add (knob ("amp_s", "Sustain"));
        a.add (knob ("amp_r", "Release"));

        auto& fe = makePanel ("Filter Envelope", 4, row2);
        fe.add (knob ("flt_a", "Attack"));
        fe.add (knob ("flt_d", "Decay"));
        fe.add (knob ("flt_s", "Sustain"));
        fe.add (knob ("flt_r", "Release"));
    }

    // ---------- Row 3: LFO / FX / Output ----------
    {
        auto& l = makePanel ("LFO", 5, row3);
        l.add (combo ("lfo_shape", "Shape", lfoShapeNames()));
        l.add (knob  ("lfo_rate",   "Rate"));
        l.add (knob  ("lfo_pitch",  "> Pitch"));
        l.add (knob  ("lfo_filter", "> Filter"));
        l.add (knob  ("lfo_amp",    "> Amp"));

        auto& d = makePanel ("Delay", 3, row3);
        d.add (knob ("dly_time", "Time"));
        d.add (knob ("dly_fb",   "Feedback"));
        d.add (knob ("dly_mix",  "Mix"));

        auto& r = makePanel ("Reverb", 3, row3);
        r.add (knob ("rvb_size", "Size"));
        r.add (knob ("rvb_damp", "Damping"));
        r.add (knob ("rvb_mix",  "Mix"));

        auto& o = makePanel ("Output", 2, row3);
        o.add (knob ("fx_drive", "Drive"));
        o.add (knob ("master",   "Master"));
    }

    // ---------- Presets ----------
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (1120, 650);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e1118));

    g.setColour (juce::Colours::white);
    g.setFont (22.f);
    g.drawText ("FIVE OSC SYNTH", 18, 8, 300, 30, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff9aa3bb));
    g.setFont (12.f);
    g.drawText ("Preset", getWidth() - 290, 12, 50, 22, juce::Justification::centredRight);
}

void PluginEditor::layoutRow (juce::Rectangle<int> area, const std::vector<Panel*>& row)
{
    if (row.empty()) return;

    const int gap = 8;
    int totalWeight = 0;
    for (auto* p : row) totalWeight += p->getColumns();

    const float avail = (float) (area.getWidth() - gap * ((int) row.size() - 1));
    float x = (float) area.getX();

    for (auto* p : row)
    {
        const float w = avail * (float) p->getColumns() / (float) totalWeight;
        p->setBounds ((int) x, area.getY(), (int) w, area.getHeight());
        x += w + (float) gap;
    }
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto top = area.removeFromTop (34);
    presetBox.setBounds (top.removeFromRight (220).reduced (0, 4));
    area.removeFromTop (4);

    layoutRow (area.removeFromTop (334), oscRow);
    area.removeFromTop (8);
    layoutRow (area.removeFromTop (118), row2);
    area.removeFromTop (8);
    layoutRow (area.removeFromTop (118), row3);
}
