#include "../WeztCommon.h"
#include "../OptoDSP.h"

using namespace wezt;

/*  Wezt Vari-Mu: stereo variable-mu style compressor.
    No hard threshold: the compression ratio rises smoothly the harder the signal pushes,
    from ~1:1 up to ~4:1. Six time-constant settings; setting 6 is program-dependent. */
class VariMu : public WeztBase
{
public:
    VariMu() : WeztBase (makeLayout(), "Wezt Vari-Mu")
    {
        hasMeter = true;
        meterLabel = "GAIN REDUCTION dB";
        panel = juce::Colour (0xff1f2a2a);
        accent = juce::Colour (0xffd9c27a);
    }

    static Layout makeLayout()
    {
        Layout l;
        addFloat (l, "in", "Input", 0.0f, 24.0f, 6.0f, " dB");
        addFloat (l, "thr", "Threshold", -40.0f, 0.0f, -18.0f, " dB");
        addChoice (l, "tc", "Time Constant", { "1 Fast", "2", "3", "4", "5 Slow", "6 Auto" }, 1);
        addFloat (l, "link", "Stereo Link", 0.0f, 100.0f, 100.0f, " %");
        addFloat (l, "warm", "Warmth", 0.0f, 100.0f, 30.0f, " %");
        addFloat (l, "out", "Output", -12.0f, 24.0f, 0.0f, " dB");
        addFloat (l, "mix", "Mix", 0.0f, 100.0f, 100.0f, " %");
        return l;
    }

    void prepareToPlay (double sr, int) override
    {
        fs = (float) sr;
        rmsC = std::exp (-1.0f / (0.002f * fs));
        for (int c = 0; c < 2; ++c) { rms[c] = 0; gr[c] = 0; tube[c].reset(); }
    }

    void processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals nd;
        const int nCh = juce::jmin (buf.getNumChannels(), 2), n = buf.getNumSamples();

        const float inG  = juce::Decibels::decibelsToGain (p ("in"));
        const float outG = juce::Decibels::decibelsToGain (p ("out"));
        const float thr  = p ("thr");
        const int   tc   = juce::jlimit (0, 5, (int) p ("tc"));
        const float link = p ("link") * 0.01f;
        const float warm = p ("warm") * 0.01f;
        const float mix  = p ("mix") * 0.01f;

        static const float attT[6] = { 0.0002f, 0.0002f, 0.0004f, 0.0008f, 0.002f, 0.0004f };
        static const float relT[6] = { 0.3f, 0.8f, 2.0f, 5.0f, 8.0f, 0.2f };
        const float aC = std::exp (-1.0f / (attT[tc] * fs));
        const float rFixed = std::exp (-1.0f / (relT[tc] * fs));

        float grMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float lvl[2] = { -120.0f, -120.0f }, mx = -120.0f;
            for (int c = 0; c < nCh; ++c)
            {
                const float x = buf.getReadPointer (c)[i] * inG;
                rms[c] = rmsC * rms[c] + (1.0f - rmsC) * x * x;
                lvl[c] = 10.0f * std::log10 (rms[c] + 1.0e-12f);
                mx = std::fmax (mx, lvl[c]);
            }
            for (int c = 0; c < nCh; ++c)
            {
                const float l = lvl[c] + link * (mx - lvl[c]);
                const float over = l - thr;
                const float target = over > 0.0f ? 0.75f * over * over / (over + 12.0f) : 0.0f;
                float rC = rFixed;
                if (tc == 5) rC = std::exp (-1.0f / (juce::jlimit (0.15f, 8.0f, 0.2f + 0.12f * gr[c]) * fs));
                const float k = target > gr[c] ? aC : rC;
                gr[c] = k * gr[c] + (1.0f - k) * target;
                grMax = std::fmax (grMax, gr[c]);

                float* d = buf.getWritePointer (c);
                const float dry = d[i];
                float wet = dry * inG * juce::Decibels::decibelsToGain (-gr[c]);
                wet = tube[c].process (wet, warm) * outG / inG;
                d[i] = dry + mix * (wet - dry);
            }
        }
        meter.store (grMax);
    }

private:
    float fs = 44100.0f, rmsC = 0.0f;
    float rms[2] = { 0, 0 }, gr[2] = { 0, 0 };
    TubeStage tube[2];
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VariMu(); }
