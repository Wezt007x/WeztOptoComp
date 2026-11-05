#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "OptoDSP.h"

class WeztOptoCompProcessor : public juce::AudioProcessor
{
public:
    WeztOptoCompProcessor();
    ~WeztOptoCompProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Wezt Opto Comp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Parameter IDs: keep these stable across versions (version hint = 1)
    static constexpr const char* kPeak   = "peak";
    static constexpr const char* kGain   = "gain";
    static constexpr const char* kLimit  = "limit";
    static constexpr const char* kWarmth = "warmth";
    static constexpr const char* kMix    = "mix";
    static constexpr const char* kBypass = "bypass";

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> grMeterDb { 0.0f };
    std::atomic<float> outMeterDb { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    wezt::OptoEngine engine;
    wezt::TubeStage tube[2];

    juce::SmoothedValue<float> gainSmooth, mixSmooth;
    float bypassFade = 1.0f; // 1 = processed, 0 = bypassed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeztOptoCompProcessor)
};
