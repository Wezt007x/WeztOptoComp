#include "../WeztCommon.h"
#include "../OptoDSP.h"

using namespace wezt;

/*  Wezt Tube EQ: passive-style program equalizer.
    Low boost and low attenuation are two separate curves at the same frequency (the boost is
    peaky, the cut is broad), so using both together gives the classic "boost and cut" bass contour
    rather than a single shelf. High section: switchable-frequency boost with variable bandwidth,
    plus a separate high-shelf attenuator. Tube-style output stage. */
class TubeEQ : public WeztBase
{
public:
    TubeEQ() : WeztBase (makeLayout(), "Wezt Tube EQ")
    {
        panel = juce::Colour (0xff3a342c);
        accent = juce::Colour (0xffc9b48a);
    }

    static Layout makeLayout()
    {
        Layout l;
        addChoice (l, "lf", "Low Freq", { "20 Hz", "30 Hz", "60 Hz", "100 Hz" }, 2);
        addFloat (l, "lb", "Low Boost", 0.0f, 10.0f, 0.0f);
        addFloat (l, "la", "Low Atten", 0.0f, 10.0f, 0.0f);
        addChoice (l, "hf", "High Freq", { "3 kHz", "4 kHz", "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" }, 4);
        addFloat (l, "hb", "High Boost", 0.0f, 10.0f, 0.0f);
        addFloat (l, "bw", "Bandwidth", 0.0f, 10.0f, 5.0f);
        addChoice (l, "haf", "Atten Freq", { "5 kHz", "10 kHz", "20 kHz" }, 1);
        addFloat (l, "ha", "High Atten", 0.0f, 10.0f, 0.0f);
        addFloat (l, "warm", "Warmth", 0.0f, 100.0f, 20.0f, " %");
        addFloat (l, "out", "Output", -12.0f, 12.0f, 0.0f, " dB");
        return l;
    }

    void prepareToPlay (double sr, int) override
    {
        fs = sr;
        for (int c = 0; c < 2; ++c) { tube[c].reset(); for (auto& b : band[c]) b.reset(); }
    }

    void processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        static const double lowF[4]  = { 20.0, 30.0, 60.0, 100.0 };
        static const double highF[7] = { 3000.0, 4000.0, 5000.0, 8000.0, 10000.0, 12000.0, 16000.0 };
        static const double attF[3]  = { 5000.0, 10000.0, 20000.0 };

        const double lf  = lowF[juce::jlimit (0, 3, (int) p ("lf"))];
        const double hf  = highF[juce::jlimit (0, 6, (int) p ("hf"))];
        const double haf = attF[juce::jlimit (0, 2, (int) p ("haf"))];
        const double lb = p ("lb"), la = p ("la"), hb = p ("hb"), ha = p ("ha");
        const double q = 2.0 - p ("bw") * 0.16; // bandwidth knob: narrow (Q 2.0) to broad (Q 0.4)

        const int nCh = juce::jmin (buf.getNumChannels(), 2), n = buf.getNumSamples();
        for (int c = 0; c < nCh; ++c)
        {
            band[c][0].lowShelf  (fs, lf, 0.9, lb * 1.2);
            band[c][1].lowShelf  (fs, lf, 0.35, -la * 1.6);
            band[c][2].peak      (fs, hf, q, hb * 1.6);
            band[c][3].highShelf (fs, haf, 0.6, -ha * 1.6);
        }

        const float warm = p ("warm") * 0.01f;
        const float outG = juce::Decibels::decibelsToGain (p ("out"));
        for (int c = 0; c < nCh; ++c)
        {
            float* d = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                for (auto& b : band[c]) x = b.process (x);
                d[i] = tube[c].process (x, warm) * outG;
            }
        }
    }

private:
    double fs = 44100.0;
    Biquad band[2][4];
    TubeStage tube[2];
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TubeEQ(); }
