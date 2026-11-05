#pragma once
#include "PluginProcessor.h"

// Original vintage-style knob look (not a copy of any real faceplate)
class WeztKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class WeztOptoCompEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WeztOptoCompEditor (WeztOptoCompProcessor&);
    ~WeztOptoCompEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<float>);

    WeztOptoCompProcessor& proc;
    WeztKnobLookAndFeel laf;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    Knob peak, gain, warmth, mix;

    juce::ToggleButton limitBtn { "LIMIT" }, bypassBtn { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limitAttach, bypassAttach;

    float needle = 0.0f; // smoothed gain reduction in dB

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeztOptoCompEditor)
};
