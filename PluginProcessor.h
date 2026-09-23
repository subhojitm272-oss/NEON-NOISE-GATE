#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class NeonNoiseGateAudioProcessor final : public juce::AudioProcessor
{
public:
    NeonNoiseGateAudioProcessor();
    ~NeonNoiseGateAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }
    float getInputLevelDb() const noexcept { return inputLevelDb.load(); }
    float getOutputLevelDb() const noexcept { return outputLevelDb.load(); }
    bool isGateOpen() const noexcept { return gateOpen.load(); }
    void copyWaveform (std::vector<float>& destination) const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static constexpr size_t waveformSize = 1024;

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* rangeParam = nullptr;
    std::atomic<float>* makeupParam = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> thresholdDb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> rangeDb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> makeupDb;

    double currentSampleRate = 44100.0;
    float detectorEnvelope = 0.0f;
    float currentGainReduction = 0.0f;
    int holdSamplesRemaining = 0;
    int waveformDecimator = 0;
    int waveformDecimation = 16;

    std::array<std::atomic<float>, waveformSize> waveform {};
    std::atomic<int> waveformWriteIndex { 0 };
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<bool> gateOpen { false };

    float calculateBallisticsCoefficient (float timeMs) const noexcept;
    void updateSmoothedParameterTargets() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonNoiseGateAudioProcessor)
};
