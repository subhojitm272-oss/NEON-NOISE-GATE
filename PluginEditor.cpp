#include "PluginEditor.h"

namespace
{
const auto panelColour = juce::Colour::fromRGBA (20, 24, 32, 190);
const auto borderColour = juce::Colour::fromRGBA (120, 245, 255, 70);
const auto cyan = juce::Colour::fromRGB (56, 239, 255);
const auto magenta = juce::Colour::fromRGB (255, 58, 182);
const auto green = juce::Colour::fromRGB (88, 255, 137);
const auto text = juce::Colour::fromRGB (232, 239, 246);
const auto mutedText = juce::Colour::fromRGB (132, 148, 166);

void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> area, float corner = 14.0f)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour::fromRGBA (255, 255, 255, 34), area.getX(), area.getY(),
                                             panelColour, area.getRight(), area.getBottom(), false));
    g.fillRoundedRectangle (area, corner);
    g.setColour (borderColour);
    g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
}
}

NeonNoiseGateAudioProcessorEditor::NeonLookAndFeel::NeonLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, cyan);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
}

void NeonNoiseGateAudioProcessorEditor::NeonLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float rotaryStartAngle, float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (width), static_cast<float> (height)).reduced (7.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colour::fromRGBA (0, 0, 0, 105));
    g.fillEllipse (bounds.translated (0.0f, 4.0f));

    g.setGradientFill (juce::ColourGradient (juce::Colour::fromRGB (47, 55, 68), bounds.getX(), bounds.getY(),
                                             juce::Colour::fromRGB (12, 15, 21), bounds.getRight(), bounds.getBottom(), false));
    g.fillEllipse (bounds);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 28));
    g.drawEllipse (bounds.reduced (1.0f), 1.0f);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour::fromRGB (50, 60, 76));
    g.strokePath (backgroundArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setGradientFill (juce::ColourGradient (cyan, bounds.getX(), centre.y, magenta, bounds.getRight(), centre.y, false));
    g.strokePath (valueArc, juce::PathStrokeType (4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto pointerLength = radius * 0.56f;
    const auto pointerThickness = 3.0f;
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (text);
    g.fillPath (pointer);

    g.setColour (juce::Colour::fromRGBA (56, 239, 255, 35));
    g.drawEllipse (bounds.expanded (2.0f), 2.0f);
}

void NeonNoiseGateAudioProcessorEditor::NeonLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(), 1);
}

NeonNoiseGateAudioProcessorEditor::Knob::Knob (juce::String knobTitle, juce::String suffix)
    : valueSuffix (std::move (suffix))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);

    name.setText (std::move (knobTitle), juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setColour (juce::Label::textColourId, mutedText);
    name.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    value.setJustificationType (juce::Justification::centred);
    value.setColour (juce::Label::textColourId, text);
    value.setFont (juce::FontOptions (13.5f, juce::Font::bold));

    slider.onValueChange = [this]
    {
        const auto decimals = std::abs (slider.getValue()) < 10.0 ? 1 : 0;
        value.setText (juce::String (slider.getValue(), decimals) + valueSuffix, juce::dontSendNotification);
    };

    addAndMakeVisible (slider);
    addAndMakeVisible (name);
    addAndMakeVisible (value);
}

void NeonNoiseGateAudioProcessorEditor::Knob::resized()
{
    auto bounds = getLocalBounds();
    name.setBounds (bounds.removeFromTop (18));
    value.setBounds (bounds.removeFromBottom (20));
    slider.setBounds (bounds.reduced (2));
}

NeonNoiseGateAudioProcessorEditor::NeonNoiseGateAudioProcessorEditor (NeonNoiseGateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), waveform (p), gainReductionMeter (p), gateIndicator (p)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (620, 420, 1180, 760);
    setSize (900, 560);

    title.setText ("NEON NOISE GATE", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, text);
    title.setFont (juce::FontOptions (25.0f, juce::Font::bold));

    subtitle.setText ("Realtime dynamics processor", juce::dontSendNotification);
    subtitle.setJustificationType (juce::Justification::centredLeft);
    subtitle.setColour (juce::Label::textColourId, mutedText);
    subtitle.setFont (juce::FontOptions (13.0f));

    const std::array<juce::Component*, 9> components {
        &threshold, &attack, &release, &hold, &range, &makeup,
        &waveform, &gainReductionMeter, &gateIndicator
    };

    for (auto* component : components)
        addAndMakeVisible (component);

    for (auto* label : { &title, &subtitle, &inputMeter, &outputMeter })
        addAndMakeVisible (label);

    for (auto* label : { &inputMeter, &outputMeter })
    {
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, text);
        label->setFont (juce::FontOptions (13.0f, juce::Font::bold));
    }

    attachKnob (threshold, "threshold");
    attachKnob (attack, "attack");
    attachKnob (release, "release");
    attachKnob (hold, "hold");
    attachKnob (range, "range");
    attachKnob (makeup, "makeup");

    startTimerHz (30);
}

NeonNoiseGateAudioProcessorEditor::~NeonNoiseGateAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void NeonNoiseGateAudioProcessorEditor::attachKnob (Knob& knob, const juce::String& parameterId)
{
    attachments.push_back (std::make_unique<SliderAttachment> (audioProcessor.getValueTreeState(), parameterId, knob.slider));
    knob.slider.onValueChange();
}

void NeonNoiseGateAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour::fromRGB (8, 10, 15), bounds.getX(), bounds.getY(),
                                             juce::Colour::fromRGB (19, 23, 32), bounds.getRight(), bounds.getBottom(), false));
    g.fillAll();

    g.setColour (juce::Colour::fromRGBA (56, 239, 255, 22));
    for (int x = 0; x < getWidth(); x += 28)
        g.drawVerticalLine (x, 0.0f, static_cast<float> (getHeight()));

    g.setColour (juce::Colour::fromRGBA (255, 58, 182, 18));
    for (int y = 0; y < getHeight(); y += 28)
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));

    auto content = getLocalBounds().reduced (18).toFloat();
    drawGlassPanel (g, content, 18.0f);
}

void NeonNoiseGateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    auto header = area.removeFromTop (58);
    auto leftHeader = header.removeFromLeft (juce::jmax (280, getWidth() / 2));
    title.setBounds (leftHeader.removeFromTop (32));
    subtitle.setBounds (leftHeader);
    gateIndicator.setBounds (header.removeFromRight (170).reduced (6, 7));

    area.removeFromTop (12);
    auto upper = area.removeFromTop (juce::jmax (135, area.getHeight() / 3));
    waveform.setBounds (upper.removeFromLeft (area.getWidth() - 128).reduced (0, 4));
    gainReductionMeter.setBounds (upper.reduced (12, 4));

    area.removeFromTop (12);
    auto meterStrip = area.removeFromTop (30);
    inputMeter.setBounds (meterStrip.removeFromLeft (170));
    outputMeter.setBounds (meterStrip.removeFromLeft (170));

    area.removeFromTop (12);
    auto knobArea = area;
    const auto columns = getWidth() < 760 ? 3 : 6;
    const auto rows = columns == 3 ? 2 : 1;
    const auto cellW = knobArea.getWidth() / columns;
    const auto cellH = knobArea.getHeight() / rows;

    std::array<Knob*, 6> knobs { &threshold, &attack, &release, &hold, &range, &makeup };
    for (int i = 0; i < static_cast<int> (knobs.size()); ++i)
    {
        const auto row = i / columns;
        const auto col = i % columns;
        knobs[static_cast<size_t> (i)]->setBounds (knobArea.getX() + col * cellW,
                                                   knobArea.getY() + row * cellH,
                                                   cellW,
                                                   cellH);
    }
}

void NeonNoiseGateAudioProcessorEditor::timerCallback()
{
    inputMeter.setText ("IN  " + juce::String (audioProcessor.getInputLevelDb(), 1) + " dB", juce::dontSendNotification);
    outputMeter.setText ("OUT  " + juce::String (audioProcessor.getOutputLevelDb(), 1) + " dB", juce::dontSendNotification);

    waveform.repaint();
    gainReductionMeter.repaint();
    gateIndicator.repaint();
}

void NeonNoiseGateAudioProcessorEditor::WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawGlassPanel (g, bounds);
    auto plot = bounds.reduced (16.0f, 18.0f);

    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 22));
    g.drawHorizontalLine (static_cast<int> (plot.getCentreY()), plot.getX(), plot.getRight());

    processor.copyWaveform (samples);
    juce::Path path;

    for (size_t i = 0; i < samples.size(); ++i)
    {
        const auto x = juce::jmap (static_cast<float> (i), 0.0f, static_cast<float> (samples.size() - 1),
                                   plot.getX(), plot.getRight());
        const auto y = juce::jmap (juce::jlimit (0.0f, 1.0f, samples[i]), 0.0f, 1.0f,
                                   plot.getCentreY(), plot.getY());
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (juce::Colour::fromRGBA (56, 239, 255, 60));
    g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (cyan, plot.getX(), plot.getCentreY(), magenta, plot.getRight(), plot.getCentreY(), false));
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (mutedText);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("WAVEFORM", plot.removeFromTop (18).toNearestInt(), juce::Justification::centredLeft);
}

void NeonNoiseGateAudioProcessorEditor::GainReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawGlassPanel (g, bounds);
    auto meter = bounds.reduced (26.0f, 18.0f);

    g.setColour (mutedText);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("GR", meter.removeFromTop (20).toNearestInt(), juce::Justification::centred);

    meter.reduce (12.0f, 0.0f);
    g.setColour (juce::Colour::fromRGB (25, 31, 42));
    g.fillRoundedRectangle (meter, 5.0f);

    const auto gr = juce::jlimit (0.0f, 80.0f, -processor.getGainReductionDb());
    auto fill = meter;
    fill.removeFromTop (meter.getHeight() * (1.0f - gr / 80.0f));

    g.setGradientFill (juce::ColourGradient (magenta, fill.getX(), fill.getY(), cyan, fill.getX(), fill.getBottom(), false));
    g.fillRoundedRectangle (fill, 5.0f);

    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 50));
    for (int i = 0; i <= 4; ++i)
    {
        const auto y = juce::jmap (static_cast<float> (i), 0.0f, 4.0f, meter.getY(), meter.getBottom());
        g.drawHorizontalLine (static_cast<int> (y), meter.getX(), meter.getRight());
    }

    g.setColour (text);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String (gr, 1) + " dB", bounds.removeFromBottom (20).toNearestInt(), juce::Justification::centred);
}

void NeonNoiseGateAudioProcessorEditor::GateIndicator::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawGlassPanel (g, bounds, 12.0f);

    const auto open = processor.isGateOpen();
    auto led = bounds.removeFromLeft (54.0f).reduced (15.0f);
    const auto colour = open ? green : magenta;

    g.setColour (colour.withAlpha (0.2f));
    g.fillEllipse (led.expanded (8.0f));
    g.setColour (colour);
    g.fillEllipse (led);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 110));
    g.drawEllipse (led.reduced (1.0f), 1.0f);

    g.setColour (text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (open ? "GATE OPEN" : "GATE CLOSED", bounds.toNearestInt(), juce::Justification::centredLeft);
}
