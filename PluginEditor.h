#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class NeonNoiseGateAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit NeonNoiseGateAudioProcessorEditor (NeonNoiseGateAudioProcessor&);
    ~NeonNoiseGateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct NeonLookAndFeel final : juce::LookAndFeel_V4
    {
        NeonLookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider&) override;
        void drawLabel (juce::Graphics&, juce::Label&) override;
    };

    class Knob final : public juce::Component
    {
    public:
        Knob (juce::String title, juce::String suffix);
        void resized() override;

        juce::Slider slider;
        juce::Label name;
        juce::Label value;

    private:
        juce::String valueSuffix;
    };

    class WaveformView final : public juce::Component
    {
    public:
        explicit WaveformView (NeonNoiseGateAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        NeonNoiseGateAudioProcessor& processor;
        std::vector<float> samples;
    };

    class GainReductionMeter final : public juce::Component
    {
    public:
        explicit GainReductionMeter (NeonNoiseGateAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        NeonNoiseGateAudioProcessor& processor;
    };

    class GateIndicator final : public juce::Component
    {
    public:
        explicit GateIndicator (NeonNoiseGateAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        NeonNoiseGateAudioProcessor& processor;
    };

    void timerCallback() override;
    void attachKnob (Knob& knob, const juce::String& parameterId);

    NeonNoiseGateAudioProcessor& audioProcessor;
    NeonLookAndFeel lookAndFeel;

    Knob threshold { "THRESHOLD", " dB" };
    Knob attack { "ATTACK", " ms" };
    Knob release { "RELEASE", " ms" };
    Knob hold { "HOLD", " ms" };
    Knob range { "RANGE", " dB" };
    Knob makeup { "MAKEUP", " dB" };

    WaveformView waveform;
    GainReductionMeter gainReductionMeter;
    GateIndicator gateIndicator;

    juce::Label title;
    juce::Label subtitle;
    juce::Label inputMeter;
    juce::Label outputMeter;

    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonNoiseGateAudioProcessorEditor)
};
