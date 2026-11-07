#include "../WeztCommon.h"

using namespace wezt;

/*  Wezt Vacuum: monophonic analog-style synth.
    2 band-limited oscillators (polyBLEP) + sine sub, 4-pole transistor-ladder-style low-pass filter
    with nonlinear (tanh) stages and resonance, filter envelope, amp ADSR, glide, LFO, drive.
    Last-note priority with legato (no retrigger while keys overlap). */
class Vacuum : public WeztBase
{
public:
    Vacuum() : WeztBase (makeLayout(), "Wezt Vacuum", true)
    {
        panel = juce::Colour (0xff2e2118);
        accent = juce::Colour (0xffe8743b);
        held.reserve (64);
    }

    static Layout makeLayout()
    {
        Layout l;
        const juce::StringArray waves { "Saw", "Square", "Triangle", "Sine" };
        addChoice (l, "wave1", "Osc 1 Wave", waves, 0);
        addChoice (l, "wave2", "Osc 2 Wave", waves, 0);
        addFloat (l, "tune2", "Osc 2 Tune", -24.0f, 24.0f, 0.0f, " st");
        addFloat (l, "detune", "Detune", -50.0f, 50.0f, 7.0f, " ct");
        addFloat (l, "mixo", "Osc 2 Level", 0.0f, 100.0f, 50.0f, " %");
        addFloat (l, "sub", "Sub", 0.0f, 100.0f, 30.0f, " %");
        addFloat (l, "cut", "Cutoff", 20.0f, 18000.0f, 1200.0f, " Hz", 0.3f);
        addFloat (l, "res", "Resonance", 0.0f, 100.0f, 20.0f, " %");
        addFloat (l, "envamt", "Env Amount", 0.0f, 100.0f, 50.0f, " %");
        addFloat (l, "fdec", "Filter Decay", 0.01f, 3.0f, 0.4f, " s", 0.4f);
        addFloat (l, "keytrk", "Key Track", 0.0f, 100.0f, 50.0f, " %");
        addFloat (l, "drive", "Drive", 0.0f, 100.0f, 20.0f, " %");
        addFloat (l, "att", "Attack", 0.001f, 5.0f, 0.005f, " s", 0.3f);
        addFloat (l, "dec", "Decay", 0.01f, 5.0f, 0.3f, " s", 0.4f);
        addFloat (l, "sus", "Sustain", 0.0f, 100.0f, 70.0f, " %");
        addFloat (l, "rel", "Release", 0.005f, 8.0f, 0.3f, " s", 0.4f);
        addFloat (l, "glide", "Glide", 0.0f, 1000.0f, 0.0f, " ms", 0.5f);
        addFloat (l, "lfor", "LFO Rate", 0.1f, 20.0f, 5.0f, " Hz", 0.5f);
        addFloat (l, "lfop", "LFO > Pitch", 0.0f, 100.0f, 0.0f, " %");
        addFloat (l, "lfof", "LFO > Filter", 0.0f, 100.0f, 0.0f, " %");
        addFloat (l, "vol", "Volume", -24.0f, 6.0f, -6.0f, " dB");
        return l;
    }

    void prepareToPlay (double sr, int) override
    {
        fs = (float) sr;
        held.clear();
        stage = Idle; level = 0; fenv = 0; gate = false;
        s1 = s2 = s3 = s4 = 0;
        ph1 = ph2 = phSub = 0; lfoPh = 0;
    }

    void processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals nd;
        buf.clear();
        const int n = buf.getNumSamples();
        const int nCh = buf.getNumChannels();

        const int w1 = juce::jlimit (0, 3, (int) p ("wave1")), w2 = juce::jlimit (0, 3, (int) p ("wave2"));
        const float tune2 = p ("tune2"), detune = p ("detune") * 0.01f;
        const float mixo = p ("mixo") * 0.01f, sub = p ("sub") * 0.01f;
        const float cut = p ("cut"), res = p ("res") * 0.01f, envamt = p ("envamt") * 0.01f;
        const float keytrk = p ("keytrk") * 0.01f;
        const float driveG = 1.0f + p ("drive") * 0.04f;
        const float sus = p ("sus") * 0.01f;
        const float glideMs = p ("glide");
        const float lfoInc = p ("lfor") / fs;
        const float lfoPitch = p ("lfop") * 0.01f * 2.0f;   // up to 2 semitones
        const float lfoFilt = p ("lfof") * 0.01f * 2.0f;    // up to 2 octaves
        const float vol = juce::Decibels::decibelsToGain (p ("vol"));

        const float aCoef = 1.0f - std::exp (-1.0f / (p ("att") * 0.4f * fs));
        const float dCoef = std::exp (-1.0f / (p ("dec") * 0.3f * fs));
        const float rCoef = std::exp (-1.0f / (p ("rel") * 0.25f * fs));
        const float fdCoef = std::exp (-1.0f / (p ("fdec") * 0.3f * fs));
        const float glideCoef = glideMs < 1.0f ? 1.0f : 1.0f - std::exp (-1.0f / (glideMs * 0.001f * fs));
        const float k = 3.9f * res;

        auto it = midi.begin();
        for (int i = 0; i < n; ++i)
        {
            while (it != midi.end() && (*it).samplePosition <= i)
            {
                handleMidi ((*it).getMessage());
                ++it;
            }

            // envelopes
            switch (stage)
            {
                case Idle: break;
                case Att:  level += (1.1f - level) * aCoef; if (level >= 1.0f) { level = 1.0f; stage = Dec; } break;
                case Dec:  level = sus + (level - sus) * dCoef; if (std::fabs (level - sus) < 1.0e-4f) stage = Sus; break;
                case Sus:  level = sus; break;
                case Rel:  level *= rCoef; if (level < 1.0e-4f) { level = 0; stage = Idle; } break;
            }
            fenv *= fdCoef;

            if (stage == Idle)
            {
                s1 = s2 = s3 = s4 = 0.0f;
                continue;
            }

            // pitch (with glide in the semitone domain)
            curNote += (targetNote - curNote) * glideCoef;
            lfoPh += lfoInc; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
            const float lfo = std::sin (2.0f * (float) kPi * lfoPh);
            const float note = curNote + lfo * lfoPitch;
            const float f1 = 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f);
            const float f2 = f1 * std::pow (2.0f, (tune2 + detune) / 12.0f);
            const float dt1 = juce::jmin (0.45f, f1 / fs), dt2 = juce::jmin (0.45f, f2 / fs);

            const float o1 = osc (w1, ph1, dt1);
            const float o2 = osc (w2, ph2, dt2);
            const float so = std::sin (2.0f * (float) kPi * phSub);
            ph1 += dt1; if (ph1 >= 1.0f) ph1 -= 1.0f;
            ph2 += dt2; if (ph2 >= 1.0f) ph2 -= 1.0f;
            phSub += dt1 * 0.5f; if (phSub >= 1.0f) phSub -= 1.0f;

            float x = (o1 + mixo * o2 + sub * so) * 0.5f * driveG;

            // filter
            const float oct = envamt * 5.0f * fenv + keytrk * (curNote - 60.0f) / 12.0f + lfoFilt * lfo;
            const float fc = juce::jlimit (20.0f, juce::jmin (18000.0f, 0.45f * fs), cut * std::pow (2.0f, oct));
            const float g = 1.0f - std::exp (-2.0f * (float) kPi * fc / fs);
            const float u = std::tanh (x - k * s4);
            s1 += g * (u - std::tanh (s1));
            s2 += g * (std::tanh (s1) - std::tanh (s2));
            s3 += g * (std::tanh (s2) - std::tanh (s3));
            s4 += g * (std::tanh (s3) - std::tanh (s4));

            const float out = std::tanh (s4 * (1.0f + 0.5f * res)) * level * velocity * vol;
            for (int c = 0; c < nCh; ++c)
                buf.getWritePointer (c)[i] = out;
        }
    }

private:
    enum Stage { Idle, Att, Dec, Sus, Rel };

    static float polyBlep (float t, float dt)
    {
        if (t < dt)         { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)  { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    static float osc (int wave, float ph, float dt)
    {
        switch (wave)
        {
            case 0: return 2.0f * ph - 1.0f - polyBlep (ph, dt);
            case 1:
            {
                float p2 = ph + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                return (ph < 0.5f ? 1.0f : -1.0f) + polyBlep (ph, dt) - polyBlep (p2, dt);
            }
            case 2: return 1.0f - 4.0f * std::fabs (ph - 0.5f);
            default: return std::sin (2.0f * (float) kPi * ph);
        }
    }

    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            const bool legato = ! held.empty() && gate;
            held.push_back (note);
            targetNote = (float) note;
            velocity = 0.3f + 0.7f * m.getFloatVelocity();
            if (! legato)
            {
                if (stage == Idle) curNote = targetNote;
                fenv = 1.0f;
                stage = Att;
            }
            gate = true;
        }
        else if (m.isNoteOff())
        {
            const int note = m.getNoteNumber();
            for (size_t j = 0; j < held.size(); ++j)
                if (held[j] == note) { held.erase (held.begin() + (std::ptrdiff_t) j); break; }
            if (! held.empty()) targetNote = (float) held.back();
            else if (gate) { gate = false; stage = Rel; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            if (gate) { gate = false; stage = Rel; }
        }
    }

    float fs = 44100.0f;
    std::vector<int> held;
    bool gate = false;
    Stage stage = Idle;
    float level = 0, fenv = 0, velocity = 1.0f;
    float targetNote = 60, curNote = 60;
    float ph1 = 0, ph2 = 0, phSub = 0, lfoPh = 0;
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Vacuum(); }
