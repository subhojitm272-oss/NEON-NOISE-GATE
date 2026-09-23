#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto thresholdId = "threshold";
constexpr auto attackId = "attack";
constexpr auto releaseId = "release";
constexpr auto holdId = "hold";
constexpr auto rangeId = "range";
constexpr auto makeupId = "makeup";

juce::String dbText (float value, int)
{
    return juce::String (value, 1) + " dB";
}

juce::String msText (float value, int)
{
    return juce::String (value, value < 10.0f ? 1 : 0) + " ms";
}
}

NeonNoiseGateAudioProcessor::NeonNoiseGateAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    thresholdParam = parameters.getRawParameterValue (thresholdId);
    attackParam = parameters.getRawParameterValue (attackId);
    releaseParam = parameters.getRawParameterValue (releaseId);
    holdParam = parameters.getRawParameterValue (holdId);
    rangeParam = parameters.getRawParameterValue (rangeId);
    makeupParam = parameters.getRawParameterValue (makeupId);

    for (auto& sample : waveform)
        sample.store (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout NeonNoiseGateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -40.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.35f), 5.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float> (5.0f, 1000.0f, 0.1f, 0.45f), 120.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { holdId, 1 }, "Hold",
        juce::NormalisableRange<float> (0.0f, 500.0f, 0.1f, 0.5f), 25.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { rangeId, 1 }, "Range",
        juce::NormalisableRange<float> (0.0f, 80.0f, 0.1f), 60.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { makeupId, 1 }, "Makeup Gain",
        juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    return { params.begin(), params.end() };
}

void NeonNoiseGateAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    detectorEnvelope = 0.0f;
    currentGainReduction = 0.0f;
    holdSamplesRemaining = 0;
    waveformDecimator = 0;
    waveformDecimation = juce::jlimit (1, 128, static_cast<int> (sampleRate / 3000.0));

    thresholdDb.reset (sampleRate, 0.02);
    rangeDb.reset (sampleRate, 0.02);
    makeupDb.reset (sampleRate, 0.02);

    thresholdDb.setCurrentAndTargetValue (thresholdParam->load());
    rangeDb.setCurrentAndTargetValue (rangeParam->load());
    makeupDb.setCurrentAndTargetValue (makeupParam->load());
}

void NeonNoiseGateAudioProcessor::releaseResources()
{
}

bool NeonNoiseGateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    return mainIn == mainOut
        && (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo());
}

float NeonNoiseGateAudioProcessor::calculateBallisticsCoefficient (float timeMs) const noexcept
{
    const auto samples = juce::jmax (1.0f, static_cast<float> (currentSampleRate) * timeMs * 0.001f);
    return std::exp (-1.0f / samples);
}

void NeonNoiseGateAudioProcessor::updateSmoothedParameterTargets() noexcept
{
    thresholdDb.setTargetValue (thresholdParam->load());
    rangeDb.setTargetValue (rangeParam->load());
    makeupDb.setTargetValue (makeupParam->load());
}

void NeonNoiseGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    updateSmoothedParameterTargets();

    const auto attackCoeff = calculateBallisticsCoefficient (attackParam->load());
    const auto releaseCoeff = calculateBallisticsCoefficient (releaseParam->load());
    const auto grSmoothCoeff = calculateBallisticsCoefficient (5.0f);
    const auto holdSamples = static_cast<int> (currentSampleRate * holdParam->load() * 0.001f);

    float peakIn = 0.0f;
    float peakOut = 0.0f;
    auto writeIndex = waveformWriteIndex.load (std::memory_order_relaxed);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float linkedInput = 0.0f;

        for (int ch = 0; ch < totalNumInputChannels; ++ch)
            linkedInput = juce::jmax (linkedInput, std::abs (buffer.getReadPointer (ch)[sample]));

        peakIn = juce::jmax (peakIn, linkedInput);

        const auto envCoeff = linkedInput > detectorEnvelope ? attackCoeff : releaseCoeff;
        detectorEnvelope = (envCoeff * detectorEnvelope) + ((1.0f - envCoeff) * linkedInput);

        const auto detectorDb = juce::Decibels::gainToDecibels (detectorEnvelope, -100.0f);
        const auto threshold = thresholdDb.getNextValue();
        const auto range = rangeDb.getNextValue();
        const auto makeup = makeupDb.getNextValue();

        const auto open = detectorDb >= threshold;
        if (open)
            holdSamplesRemaining = holdSamples;
        else if (holdSamplesRemaining > 0)
            --holdSamplesRemaining;

        const auto heldOpen = open || holdSamplesRemaining > 0;
        const auto targetGainReduction = heldOpen ? 0.0f : -range;
        currentGainReduction = (grSmoothCoeff * currentGainReduction) + ((1.0f - grSmoothCoeff) * targetGainReduction);

        const auto linearGain = juce::Decibels::decibelsToGain (currentGainReduction + makeup);
        float linkedOutput = 0.0f;

        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            auto* channel = buffer.getWritePointer (ch);
            channel[sample] *= linearGain;
            linkedOutput = juce::jmax (linkedOutput, std::abs (channel[sample]));
        }

        peakOut = juce::jmax (peakOut, linkedOutput);

        if (++waveformDecimator >= waveformDecimation)
        {
            waveformDecimator = 0;
            waveform[static_cast<size_t> (writeIndex)].store (linkedOutput, std::memory_order_relaxed);
            writeIndex = (writeIndex + 1) % static_cast<int> (waveformSize);
        }
    }

    waveformWriteIndex.store (writeIndex, std::memory_order_release);
    gainReductionDb.store (currentGainReduction, std::memory_order_relaxed);
    inputLevelDb.store (juce::Decibels::gainToDecibels (peakIn, -100.0f), std::memory_order_relaxed);
    outputLevelDb.store (juce::Decibels::gainToDecibels (peakOut, -100.0f), std::memory_order_relaxed);
    gateOpen.store (currentGainReduction > -1.0f, std::memory_order_relaxed);
}

void NeonNoiseGateAudioProcessor::copyWaveform (std::vector<float>& destination) const
{
    destination.resize (waveformSize);
    auto writeIndex = waveformWriteIndex.load (std::memory_order_acquire);

    for (size_t i = 0; i < waveformSize; ++i)
    {
        const auto sourceIndex = (writeIndex + static_cast<int> (i)) % static_cast<int> (waveformSize);
        destination[i] = waveform[static_cast<size_t> (sourceIndex)].load (std::memory_order_relaxed);
    }
}

void NeonNoiseGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void NeonNoiseGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto state = getXmlFromBinary (data, sizeInBytes))
        if (state->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*state));
}

juce::AudioProcessorEditor* NeonNoiseGateAudioProcessor::createEditor()
{
    return new NeonNoiseGateAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeonNoiseGateAudioProcessor();
}
