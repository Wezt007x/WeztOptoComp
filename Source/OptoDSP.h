#pragma once
#include <cmath>
#include <algorithm>

namespace wezt
{
/*  Optical-style gain computer.

    Program dependence is modelled with two parallel gain-reduction trackers:
      - fast : ~10 ms attack, ~60 ms release  (the "cell" reacting quickly)
      - slow : ~250 ms attack, ~2.5 s release (the "cell" remembering history)
    The applied reduction is max(fast, 0.7 * slow). Short peaks barely charge the
    slow tracker, so they release quickly. Sustained loud material charges it, and
    release then becomes long and gradual, like a real electro-luminescent opto cell. */
class OptoEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hpA   = 1.0f - std::exp (-2.0f * 3.14159265f * 60.0f / (float) fs);
        rmsC  = coef (0.005f);
        aFast = coef (0.010f);  rFast = coef (0.060f);
        aSlow = coef (0.250f);  rSlow = coef (2.500f);
        reset();
    }

    void reset() { hpState = rms = fast = slow = 0.0f; }

    // peak01: 0..1 (Peak Reduction knob), limit: false = Compress, true = Limit
    void setParams (float peak01, bool limit)
    {
        threshDb = -45.0f * peak01;
        ratio    = limit ? 12.0f : 3.0f;
    }

    // Takes the (already channel-linked) detector sample, returns gain in dB (<= 0)
    float process (float x)
    {
        hpState += hpA * (x - hpState);
        const float y = x - hpState;                     // 60 Hz sidechain high-pass
        rms = rmsC * rms + (1.0f - rmsC) * y * y;
        const float levelDb = 10.0f * std::log10 (rms + 1.0e-12f);

        const float over = levelDb - threshDb;
        const float slope = 1.0f - 1.0f / ratio;
        float target;
        if (2.0f * over < -kneeDb)             target = 0.0f;
        else if (2.0f * std::abs (over) <= kneeDb)
        {
            const float t = over + kneeDb * 0.5f;
            target = slope * t * t / (2.0f * kneeDb);
        }
        else                                   target = slope * over;

        fast = track (fast, target, aFast, rFast);
        slow = track (slow, target, aSlow, rSlow);

        grDb = std::max (fast, 0.7f * slow);
        return -grDb;
    }

    float currentGrDb() const { return grDb; }

private:
    float coef (float seconds) const { return std::exp (-1.0f / (seconds * (float) fs)); }
    static float track (float s, float target, float a, float r)
    {
        const float c = target > s ? a : r;
        return c * s + (1.0f - c) * target;
    }

    double fs = 44100.0;
    float hpA = 0, hpState = 0, rmsC = 0, rms = 0;
    float aFast = 0, rFast = 0, aSlow = 0, rSlow = 0, fast = 0, slow = 0;
    float threshDb = 0, ratio = 3.0f, grDb = 0;
    static constexpr float kneeDb = 10.0f;
};

/*  Tube-style output stage: biased tanh with small-signal gain normalised to 1,
    followed by a DC blocker. Warmth 0 = bypass-clean, 1 = obvious 2nd/3rd harmonics. */
struct TubeStage
{
    void reset() { x1 = y1 = 0.0f; }

    float process (float in, float warmth)
    {
        if (warmth < 1.0e-4f) return in;
        const float drive = 1.0f + 3.0f * warmth;
        const float bias  = 0.12f * warmth;
        float y = (std::tanh (drive * in + bias) - std::tanh (bias)) / drive;
        // DC blocker (R ~ 0.995)
        const float out = y - x1 + 0.995f * y1;
        x1 = y; y1 = out;
        return out;
    }
    float x1 = 0, y1 = 0;
};
} // namespace wezt
