#pragma once
#include "PluginProcessor.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob + label bound to a parameter
class KnobControl : public juce::Component
{
public:
    KnobControl (APVTS& s, const juce::String& id, const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 14);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        attach = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (1);
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attach;
};

// Drop-down bound to a choice parameter
class ComboControl : public juce::Component
{
public:
    ComboControl (APVTS& s, const juce::String& id, const juce::String& name, const juce::StringArray& items)
    {
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        combo.addItemList (items, 1);
        addAndMakeVisible (label);
        addAndMakeVisible (combo);
        attach = std::make_unique<APVTS::ComboBoxAttachment> (s, id, combo);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 1);
        label.setBounds (r.removeFromTop (14));
        combo.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
    }

private:
    juce::Label label;
    juce::ComboBox combo;
    std::unique_ptr<APVTS::ComboBoxAttachment> attach;
};

// On/off button bound to a bool parameter
class ToggleControl : public juce::Component
{
public:
    ToggleControl (APVTS& s, const juce::String& id, const juce::String& text)
    {
        button.setButtonText (text);
        addAndMakeVisible (button);
        attach = std::make_unique<APVTS::ButtonAttachment> (s, id, button);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 1);
        button.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
    }

private:
    juce::ToggleButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attach;
};

// Titled rounded panel that lays its children out on a grid
class Panel : public juce::Component
{
public:
    Panel (const juce::String& t, int cols) : title (t), columns (cols) {}

    int getColumns() const { return columns; }

    void add (juce::Component& c)
    {
        items.push_back (&c);
        addAndMakeVisible (c);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.f);
        g.setColour (juce::Colour (0xff1c2230));
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (juce::Colour (0xff343c52));
        g.drawRoundedRectangle (r, 8.f, 1.f);
        g.setColour (juce::Colour (0xffffa63d));
        g.setFont (13.f);
        g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (24).reduced (10, 0),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        r.removeFromTop (22);
        if (items.empty()) return;

        const int rows = ((int) items.size() + columns - 1) / columns;
        const int cw = r.getWidth() / columns;
        const int ch = r.getHeight() / rows;

        for (size_t i = 0; i < items.size(); ++i)
            items[i]->setBounds (r.getX() + ((int) i % columns) * cw,
                                 r.getY() + ((int) i / columns) * ch, cw, ch);
    }

private:
    juce::String title;
    int columns;
    std::vector<juce::Component*> items;
};

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void layoutRow (juce::Rectangle<int> area, const std::vector<Panel*>& row);

    PluginProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<Panel>> panels;
    std::vector<Panel*> oscRow, row2, row3;

    juce::ComboBox presetBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
