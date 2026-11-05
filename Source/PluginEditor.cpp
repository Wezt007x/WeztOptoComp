#include "PluginEditor.h"

using namespace juce;

//==============================================================================
void WeztKnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, Slider&)
{
    const auto b  = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c  = b.getCentre();
    const float a = startAngle + pos * (endAngle - startAngle);

    // tick marks
    g.setColour (Colour (0xffd8c9a8));
    for (int i = 0; i <= 10; ++i)
    {
        const float t = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        Line<float> l (c.getPointOnCircumference (r + 1.0f, t), c.getPointOnCircumference (r + 5.0f, t));
        g.drawLine (l, i % 5 == 0 ? 2.0f : 1.0f);
    }

    // body: dark bakelite with soft highlight
    g.setGradientFill (ColourGradient (Colour (0xff3a3532), c.x - r * 0.4f, c.y - r * 0.6f,
                                       Colour (0xff141211), c.x + r * 0.5f, c.y + r * 0.7f, false));
    g.fillEllipse (c.x - r + 2, c.y - r + 2, (r - 2) * 2, (r - 2) * 2);
    g.setColour (Colour (0xff7a6f60));
    g.drawEllipse (c.x - r + 2, c.y - r + 2, (r - 2) * 2, (r - 2) * 2, 1.2f);

    // pointer
    Path p;
    p.addRoundedRectangle (-2.0f, -r + 6.0f, 4.0f, r * 0.45f, 1.5f);
    g.setColour (Colour (0xfff2e6c8));
    g.fillPath (p, AffineTransform::rotation (a).translated (c.x, c.y));
}

void WeztKnobLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& bt, bool hl, bool)
{
    auto b = bt.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = bt.getToggleState();
    g.setColour (on ? Colour (0xffe0a030) : Colour (0xff2a2623));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (hl ? Colour (0xffd8c9a8) : Colour (0xff6a6054));
    g.drawRoundedRectangle (b, 4.0f, 1.2f);
    g.setColour (on ? Colour (0xff1a1410) : Colour (0xffd8c9a8));
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText (bt.getButtonText(), b.toNearestInt(), Justification::centred);
}

//==============================================================================
WeztOptoCompEditor::WeztOptoCompEditor (WeztOptoCompProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    auto setup = [this] (Knob& k, const String& text, const char* id)
    {
        k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 18);
        k.slider.setColour (Slider::textBoxTextColourId, Colour (0xfff2e6c8));
        k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        k.slider.setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
        k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                      proc.apvts.getParameter (id)->getDefaultValue()));
        k.slider.setTooltip (text);
        k.label.setText (text.toUpperCase(), dontSendNotification);
        k.label.setJustificationType (Justification::centred);
        k.label.setColour (Label::textColourId, Colour (0xffd8c9a8));
        k.label.setFont (FontOptions (13.0f, Font::bold));
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
        k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    };
    setup (peak,   "Peak Reduction", WeztOptoCompProcessor::kPeak);
    setup (gain,   "Gain",           WeztOptoCompProcessor::kGain);
    setup (warmth, "Warmth",         WeztOptoCompProcessor::kWarmth);
    setup (mix,    "Mix",            WeztOptoCompProcessor::kMix);

    addAndMakeVisible (limitBtn);
    addAndMakeVisible (bypassBtn);
    limitAttach  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, WeztOptoCompProcessor::kLimit,  limitBtn);
    bypassAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, WeztOptoCompProcessor::kBypass, bypassBtn);

    setResizable (true, true);
    setResizeLimits (520, 300, 1040, 600);
    getConstrainer()->setFixedAspectRatio (640.0 / 360.0);
    setSize (640, 360);
    startTimerHz (30);
}

WeztOptoCompEditor::~WeztOptoCompEditor() { setLookAndFeel (nullptr); }

void WeztOptoCompEditor::timerCallback()
{
    const float gr = proc.grMeterDb.load();
    // VU-style ballistics: quick rise, slower fall
    needle += (gr > needle ? 0.5f : 0.12f) * (gr - needle);
    repaint();
}

void WeztOptoCompEditor::paint (Graphics& g)
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();

    g.setGradientFill (ColourGradient (Colour (0xff2b2724), 0, 0, Colour (0xff171412), 0, h, false));
    g.fillAll();
    g.setColour (Colour (0xff5a5046));
    g.drawRect (getLocalBounds(), 3);

    g.setColour (Colour (0xfff2e6c8));
    g.setFont (FontOptions (h * 0.075f, Font::bold));
    g.drawText ("WEZT  OPTO COMP", getLocalBounds().removeFromTop ((int) (h * 0.14f)).reduced (16, 0),
                Justification::centredLeft);

    drawMeter (g, { w * 0.30f, h * 0.15f, w * 0.40f, h * 0.38f });
}

void WeztOptoCompEditor::drawMeter (Graphics& g, Rectangle<float> r)
{
    g.setGradientFill (ColourGradient (Colour (0xfff3e3b5), r.getCentreX(), r.getY(),
                                       Colour (0xffcfb97c), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colour (0xff0e0c0b));
    g.drawRoundedRectangle (r, 6.0f, 3.0f);

    const Point<float> pivot (r.getCentreX(), r.getBottom() - r.getHeight() * 0.08f);
    const float radius = r.getHeight() * 0.85f;
    const float aMin = degreesToRadians (-48.0f), aMax = degreesToRadians (48.0f);

    g.setFont (FontOptions (r.getHeight() * 0.10f));
    for (int db = 0; db <= 20; db += 2)
    {
        const float t = aMin + (float) db / 20.0f * (aMax - aMin);
        g.setColour (db >= 14 ? Colour (0xffa02020) : Colour (0xff1a1410));
        g.drawLine ({ pivot.getPointOnCircumference (radius * 0.86f, t),
                      pivot.getPointOnCircumference (radius * (db % 4 == 0 ? 0.97f : 0.92f), t) }, 1.5f);
        if (db % 4 == 0)
            g.drawText (String (db), Rectangle<float> (30, 14).withCentre (pivot.getPointOnCircumference (radius * 0.76f, t)),
                        Justification::centred);
    }
    g.setColour (Colour (0xff1a1410));
    g.drawText ("GAIN REDUCTION dB", r.withTrimmedTop (r.getHeight() * 0.62f).withHeight (18), Justification::centred);

    // needle (clipped to the meter window)
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.reduced (3).toNearestInt());
    const float t = aMin + jlimit (0.0f, 22.0f, needle) / 20.0f * (aMax - aMin);
    g.setColour (Colour (0xff110e0c));
    g.drawLine ({ pivot, pivot.getPointOnCircumference (radius * 0.95f, t) }, 2.0f);
}

void WeztOptoCompEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop ((int) (getHeight() * 0.56f));
    area.reduce (getWidth() / 20, 0);

    auto buttons = area.removeFromBottom ((int) (getHeight() * 0.12f)).reduced (0, 4);
    bypassBtn.setBounds (buttons.removeFromRight (getWidth() / 6));
    limitBtn.setBounds  (buttons.removeFromRight (getWidth() / 6).translated (-8, 0));

    const int cw = area.getWidth() / 4;
    Knob* ks[] = { &peak, &gain, &warmth, &mix };
    for (auto* k : ks)
    {
        auto col = area.removeFromLeft (cw);
        k->label.setBounds (col.removeFromTop (18));
        k->slider.setBounds (col);
    }
}
