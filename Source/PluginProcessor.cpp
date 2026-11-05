#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout WeztOptoCompProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { kPeak, 1 }, "Peak Reduction",
                                                  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { kGain, 1 }, "Gain",
                                                  NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { kLimit, 1 }, "Limit", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { kWarmth, 1 }, "Warmth",
                                                  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { kMix, 1 }, "Mix",
                                                  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { kBypass, 1 }, "Bypass", false));
    return l;
}

WeztOptoCompProcessor::WeztOptoCompProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool WeztOptoCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void WeztOptoCompProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
    for (auto& t : tube) t.reset();
    gainSmooth.reset (sr, 0.02);
    mixSmooth.reset  (sr, 0.02);
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (kGain)->load()));
    mixSmooth.setCurrentAndTargetValue  (apvts.getRawParameterValue (kMix)->load() * 0.01f);
    bypassFade = apvts.getRawParameterValue (kBypass)->load() > 0.5f ? 0.0f : 1.0f;
}

void WeztOptoCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();

    const float peak   = apvts.getRawParameterValue (kPeak)->load() * 0.01f;
    const bool  limit  = apvts.getRawParameterValue (kLimit)->load() > 0.5f;
    const float warmth = apvts.getRawParameterValue (kWarmth)->load() * 0.01f;
    const bool  bypass = apvts.getRawParameterValue (kBypass)->load() > 0.5f;

    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (kGain)->load()));
    mixSmooth.setTargetValue  (apvts.getRawParameterValue (kMix)->load() * 0.01f);
    engine.setParams (peak, limit);

    const float bypassTarget = bypass ? 0.0f : 1.0f;
    const float bypassStep   = 1.0f / (0.01f * (float) getSampleRate()); // 10 ms crossfade

    float grMax = 0.0f, outPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        // Linked detector: loudest channel drives both, so the stereo image stays put
        float det = 0.0f;
        for (int c = 0; c < numCh; ++c)
            det = std::max (det, std::abs (buffer.getReadPointer (c)[i]));

        const float grDb = engine.process (det);
        const float grLin = juce::Decibels::decibelsToGain (grDb);
        grMax = std::max (grMax, -grDb);

        const float g   = gainSmooth.getNextValue();
        const float mix = mixSmooth.getNextValue();

        if (bypassFade < bypassTarget)      bypassFade = std::min (bypassTarget, bypassFade + bypassStep);
        else if (bypassFade > bypassTarget) bypassFade = std::max (bypassTarget, bypassFade - bypassStep);

        for (int c = 0; c < numCh; ++c)
        {
            float* d = buffer.getWritePointer (c);
            const float dry = d[i];
            float wet = dry * grLin;
            wet = tube[c].process (wet, warmth) * g;
            const float processed = dry + mix * (wet - dry);
            d[i] = dry + bypassFade * (processed - dry);
            outPeak = std::max (outPeak, std::abs (d[i]));
        }
    }

    // Meters (relaxed atomics, read by the GUI thread)
    grMeterDb.store (grMax);
    outMeterDb.store (juce::Decibels::gainToDecibels (outPeak, -100.0f));
}

juce::AudioProcessorEditor* WeztOptoCompProcessor::createEditor()
{
    return new WeztOptoCompEditor (*this);
}

void WeztOptoCompProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void WeztOptoCompProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WeztOptoCompProcessor();
}
