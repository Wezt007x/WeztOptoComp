#include "../WeztCommon.h"

using namespace wezt;

/*  Wezt Phaser: chain of first-order all-pass filters swept by an LFO, with feedback. */
class Phaser : public WeztBase
{
public:
    Phaser() : WeztBase (makeLayout(), "Wezt Phaser")
    {
        panel = juce::Colour (0xff2a1f30);
        accent = juce::Colour (0xffc58be0);
    }

    static Layout makeLayout()
    {
        Layout l;
        addFloat (l, "rate", "Rate", 0.05f, 8.0f, 0.4f, " Hz", 0.5f);
        addFloat (l, "depth", "Depth", 0.0f, 100.0f, 70.0f, " %");
        addFloat (l, "center", "Center", 100.0f, 5000.0f, 800.0f, " Hz", 0.4f);
        addChoice (l, "stages", "Stages", { "4", "6", "8", "12" }, 2);
        addFloat (l, "fb", "Feedback", -90.0f, 90.0f, 40.0f, " %");
        addFloat (l, "stereo", "Stereo Phase", 0.0f, 180.0f, 90.0f, " deg");
        addFloat (l, "mix", "Mix", 0.0f, 100.0f, 50.0f, " %");
        return l;
    }

    void prepareToPlay (double sr, int) override
    {
        fs = (float) sr;
        phase = 0.0;
        for (int c = 0; c < 2; ++c)
        {
            fbState[c] = 0.0f;
            for (int s = 0; s < 12; ++s) x1[c][s] = y1[c][s] = 0.0f;
        }
    }

    void processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        static const int stageTable[4] = { 4, 6, 8, 12 };
        const int nCh = juce::jmin (buf.getNumChannels(), 2), n = buf.getNumSamples();
        const int stages = stageTable[juce::jlimit (0, 3, (int) p ("stages"))];
        const double inc = (double) p ("rate") / (double) fs;
        const float depth = p ("depth") * 0.01f;
        const float center = p ("center");
        const float fb = p ("fb") * 0.01f;
        const double off = (double) p ("stereo") / 360.0;
        const float mix = p ("mix") * 0.01f;
        const float fMax = 0.45f * fs;

        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < nCh; ++c)
            {
                double ph = phase + (double) c * off;
                ph -= std::floor (ph);
                const float lfo = (float) std::sin (2.0 * kPi * ph);
                const float f = juce::jlimit (20.0f, fMax, center * std::pow (2.0f, depth * 2.0f * lfo));
                const float t = std::tan ((float) kPi * f / fs);
                const float a = (t - 1.0f) / (t + 1.0f);

                float* d = buf.getWritePointer (c);
                const float x = d[i];
                float s = x + fb * fbState[c];
                for (int k = 0; k < stages; ++k)
                {
                    const float y = a * s + x1[c][k] - a * y1[c][k];
                    x1[c][k] = s;
                    y1[c][k] = y;
                    s = y;
                }
                fbState[c] = std::tanh (s);
                d[i] = x + mix * (s - x);
            }
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    float fs = 44100.0f;
    double phase = 0.0;
    float x1[2][12] = {}, y1[2][12] = {}, fbState[2] = { 0, 0 };
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Phaser(); }
