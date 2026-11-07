#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace wezt
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

//==============================================================================  parameter helpers
inline void addFloat (Layout& l, const juce::String& id, const juce::String& name,
                      float lo, float hi, float def, const juce::String& unit = {}, float skew = 1.0f)
{
    juce::NormalisableRange<float> r (lo, hi, 0.01f);
    r.skew = skew;
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                        juce::AudioParameterFloatAttributes().withLabel (unit)));
}
inline void addBool (Layout& l, const juce::String& id, const juce::String& name, bool def)
{
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}
inline void addChoice (Layout& l, const juce::String& id, const juce::String& name,
                       const juce::StringArray& choices, int def)
{
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
}

//==============================================================================  DSP helpers
constexpr double kPi = 3.14159265358979323846;

// RBJ biquad (transposed direct form II). No allocation, safe on the audio thread.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    static double clampF (double fs, double f) { return std::fmax (10.0, std::fmin (f, fs * 0.49)); }

    void lowShelf (double fs, double f, double Q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * clampF (fs, f) / fs;
        const double c = std::cos (w), al = std::sin (w) / (2 * Q), sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
             (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
    void highShelf (double fs, double f, double Q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * clampF (fs, f) / fs;
        const double c = std::cos (w), al = std::sin (w) / (2 * Q), sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
             (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }
    void peak (double fs, double f, double Q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * clampF (fs, f) / fs;
        const double c = std::cos (w), al = std::sin (w) / (2 * Q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
};

// Circular delay line with linear interpolation
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;
    void prepare (int maxSamples) { buf.assign ((size_t) maxSamples + 4, 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    float read (float delaySamples) const
    {
        const int n = (int) buf.size();
        float pos = (float) w - delaySamples;
        while (pos < 0) pos += (float) n;
        const int i0 = (int) pos;
        const float fr = pos - (float) i0;
        const int i1 = (i0 + 1) % n;
        return buf[(size_t) (i0 % n)] * (1.0f - fr) + buf[(size_t) i1] * fr;
    }
};

//==============================================================================  base processor
class WeztBase : public juce::AudioProcessor
{
public:
    WeztBase (Layout layout, const juce::String& displayName, bool synth = false)
        : juce::AudioProcessor (makeBuses (synth)),
          apvts (*this, nullptr, "STATE", std::move (layout)),
          name (displayName), isSynth (synth) {}

    static BusesProperties makeBuses (bool synth)
    {
        BusesProperties b;
        if (! synth) b = b.withInput ("Input", juce::AudioChannelSet::stereo(), true);
        return b.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto& out = l.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
        return isSynth || l.getMainInputChannelSet() == out;
    }

    void releaseResources() override {}
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return name; }
    bool acceptsMidi() const override { return isSynth; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    float p (const char* id) { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meter { 0.0f };
    juce::String meterLabel;
    bool hasMeter = false;
    juce::Colour panel { 0xff2b2724 }, accent { 0xffe0a030 };

private:
    juce::String name;
    bool isSynth;
};

//==============================================================================  generic editor
class WeztGenericLAF : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { 0xffe0a030 };

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float a = startAngle + pos * (endAngle - startAngle);

        g.setColour (juce::Colour (0xff4a4238));
        for (int i = 0; i <= 10; ++i)
        {
            const float t = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (r + 1.0f, t),
                                           c.getPointOnCircumference (r + 4.0f, t)), 1.0f);
        }
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r + 1, r + 1, 0.0f, startAngle, a, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (2.0f));

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3532), c.x - r * 0.4f, c.y - r * 0.6f,
                                                 juce::Colour (0xff141211), c.x + r * 0.5f, c.y + r * 0.7f, false));
        g.fillEllipse (c.x - r + 3, c.y - r + 3, (r - 3) * 2, (r - 3) * 2);
        g.setColour (juce::Colour (0xff7a6f60));
        g.drawEllipse (c.x - r + 3, c.y - r + 3, (r - 3) * 2, (r - 3) * 2, 1.0f);

        juce::Path p;
        p.addRoundedRectangle (-2.0f, -r + 7.0f, 4.0f, r * 0.4f, 1.5f);
        g.setColour (juce::Colour (0xfff2e6c8));
        g.fillPath (p, juce::AffineTransform::rotation (a).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& bt, bool hl, bool) override
    {
        auto b = bt.getLocalBounds().toFloat().reduced (2.0f);
        const bool on = bt.getToggleState();
        g.setColour (on ? accent : juce::Colour (0xff2a2623));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (hl ? juce::Colour (0xffd8c9a8) : juce::Colour (0xff6a6054));
        g.drawRoundedRectangle (b, 4.0f, 1.2f);
        g.setColour (on ? juce::Colour (0xff1a1410) : juce::Colour (0xffd8c9a8));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (bt.getButtonText(), b.toNearestInt(), juce::Justification::centred);
    }
};

class GenericEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GenericEditor (WeztBase& b) : juce::AudioProcessorEditor (&b), base (b)
    {
        laf.accent = base.accent;
        setLookAndFeel (&laf);

        for (auto* prm : base.getParameters())
        {
            auto item = std::make_unique<Item>();
            item->label = std::make_unique<juce::Label>();
            item->label->setJustificationType (juce::Justification::centred);
            item->label->setColour (juce::Label::textColourId, juce::Colour (0xffd8c9a8));
            item->label->setFont (juce::FontOptions (12.0f, juce::Font::bold));

            if (auto* f = dynamic_cast<juce::AudioParameterFloat*> (prm))
            {
                item->label->setText (f->getName (30).toUpperCase(), juce::dontSendNotification);
                auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
                s->setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff2e6c8));
                s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
                s->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
                s->setDoubleClickReturnValue (true, f->convertFrom0to1 (f->getDefaultValue()));
                sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (base.apvts, f->paramID, *s));
                item->comp = s.get();
                item->owned = std::move (s);
            }
            else if (auto* bo = dynamic_cast<juce::AudioParameterBool*> (prm))
            {
                auto t = std::make_unique<juce::ToggleButton> (bo->getName (30).toUpperCase());
                buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (base.apvts, bo->paramID, *t));
                item->comp = t.get();
                item->owned = std::move (t);
            }
            else if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (prm))
            {
                item->label->setText (ch->getName (30).toUpperCase(), juce::dontSendNotification);
                auto c = std::make_unique<juce::ComboBox>();
                c->addItemList (ch->choices, 1);
                comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (base.apvts, ch->paramID, *c));
                item->comp = c.get();
                item->owned = std::move (c);
            }
            else continue;

            addAndMakeVisible (*item->label);
            addAndMakeVisible (*item->comp);
            items.push_back (std::move (item));
        }

        const int n = (int) items.size();
        const int cols = juce::jmax (1, juce::jmin (n, 6));
        const int rows = juce::jmax (1, (n + cols - 1) / cols);
        setResizable (true, true);
        setResizeLimits (300, 200, 1600, 1000);
        setSize (juce::jlimit (380, 900, cols * 130 + 16), 54 + rows * 135 + 16 + (base.hasMeter ? 30 : 0));
        startTimerHz (30);
    }

    ~GenericEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (base.panel.brighter (0.1f), 0, 0,
                                                 base.panel.darker (0.5f), 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (juce::Colour (0xff5a5046));
        g.drawRect (getLocalBounds(), 2);

        g.setColour (juce::Colour (0xfff2e6c8));
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        g.drawText (base.getName().toUpperCase(), 16, 8, getWidth() - 32, 32, juce::Justification::centredLeft);
        g.setColour (base.accent);
        g.fillRect (16, 44, getWidth() - 32, 2);

        if (base.hasMeter)
        {
            auto m = juce::Rectangle<float> (16.0f, (float) getHeight() - 28.0f, (float) getWidth() - 32.0f, 14.0f);
            g.setColour (juce::Colour (0xff0e0c0b));
            g.fillRoundedRectangle (m, 3.0f);
            g.setColour (base.accent);
            g.fillRoundedRectangle (m.withWidth (m.getWidth() * juce::jlimit (0.0f, 1.0f, shown / 24.0f)), 3.0f);
            g.setColour (juce::Colour (0xffd8c9a8));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (base.meterLabel, m.toNearestInt(), juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedTop (54).reduced (8);
        if (base.hasMeter) area.removeFromBottom (30);
        const int n = (int) items.size();
        if (n == 0) return;
        const int cols = juce::jmin (n, 6), rows = (n + cols - 1) / cols;
        const int cw = area.getWidth() / cols, chh = area.getHeight() / rows;
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<int> (area.getX() + (i % cols) * cw, area.getY() + (i / cols) * chh, cw, chh).reduced (4);
            auto& it = *items[(size_t) i];
            if (it.label->getText().isNotEmpty()) it.label->setBounds (cell.removeFromTop (18));
            if (dynamic_cast<juce::Slider*> (it.comp) != nullptr)
                it.comp->setBounds (cell);
            else
                it.comp->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 120), 28));
        }
    }

private:
    struct Item
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Component> owned;
        juce::Component* comp = nullptr;
    };

    void timerCallback() override
    {
        const float m = base.meter.load();
        shown += (m > shown ? 0.5f : 0.12f) * (m - shown);
        if (base.hasMeter) repaint();
    }

    WeztBase& base;
    WeztGenericLAF laf;
    std::vector<std::unique_ptr<Item>> items;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    float shown = 0.0f;
};

inline juce::AudioProcessorEditor* WeztBase::createEditor() { return new GenericEditor (*this); }
} // namespace wezt
