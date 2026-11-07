#include "../WeztCommon.h"

using namespace wezt;

/*  Wezt Chorus: modulated delay line per channel, with adjustable left/right LFO phase. */
class Chorus : public WeztBase
{
public:
    Chorus() : WeztBase (makeLayout(), "Wezt Chorus")
    {
        panel = juce::Colour (0xff1c2530);
        accent = juce::Colour (0xff6fc3df);
    }

    static Layout makeLayout()
    {
        Layout l;
        addFloat (l, "rate", "Rate", 0.05f, 5.0f, 0.6f, " Hz", 0.5f);
        addFloat (l, "depth", "Depth", 0.0f, 100.0f, 50.0f, " %");
        addFloat (l, "pre", "Pre-Delay", 1.0f, 30.0f, 12.0f, " ms");
        addFloat (l, "fb", "Feedback", 0.0f, 70.0f, 0.0f, " %");
        addFloat (l, "stereo", "L/R Phase", 0.0f, 180.0f, 90.0f, " deg");
        addChoice (l, "wave", "Waveform", { "Sine", "Triangle" }, 0);
        addFloat (l, "mix", "Mix", 0.0f, 100.0f, 50.0f, " %");
        return l;
    }

    void prepareToPlay (double sr, int) override
    {
        fs = (float) sr;
        for (auto& d : dl) d.prepare ((int) (0.08 * sr));
        phase = 0.0;
    }

    void processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        const int nCh = juce::jmin (buf.getNumChannels(), 2), n = buf.getNumSamples();
        const double inc = (double) p ("rate") / (double) fs;
        const float depthMs = p ("depth") * 0.01f * 8.0f;
        const float pre = p ("pre");
        const float fb = p ("fb") * 0.01f;
        const double off = (double) p ("stereo") / 360.0;
        const bool tri = (int) p ("wave") == 1;
        const float mix = p ("mix") * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < nCh; ++c)
            {
                double ph = phase + (double) c * off;
                ph -= std::floor (ph);
                const float lfo = tri ? (float) (1.0 - 4.0 * std::fabs (ph - 0.5))
                                      : (float) std::sin (2.0 * kPi * ph);
                const float ms = pre + depthMs * (0.5f + 0.5f * lfo);
                float* d = buf.getWritePointer (c);
                const float x = d[i];
                const float wet = dl[c].read (ms * 0.001f * fs);
                dl[c].push (x + fb * wet);
                d[i] = x + mix * (wet - x);
            }
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    float fs = 44100.0f;
    double phase = 0.0;
    DelayLine dl[2];
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Chorus(); }
