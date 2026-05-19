#include "PluginProcessor.h"
#include <memory>

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Le volume de la résonance du corps (Volume du Wet)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resonance", 1), "Body Resonance", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));
        
    // La durée de l'épanouissement harmonique (Bloom/Sustain)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bloom", 1), "Harmonic Bloom", 
        juce::NormalisableRange<float>(0.1f, 0.95f, 0.01f), 0.6f));
        
    // La couleur du corps : Bois chaud (bas) ou Métal/Plate (haut)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tone", 1), "Acoustic Tone (Hz)", 
        juce::NormalisableRange<float>(800.0f, 12000.0f, 10.0f, 0.3f), 4500.0f));
        
    return layout;
}

SuperNaturalResonator::SuperNaturalResonator()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

void SuperNaturalResonator::prepareToPlay(double sampleRate, int samplesPerBlock) {
    resonator.prepare(sampleRate);
}

void SuperNaturalResonator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    auto* channelDataL = buffer.getWritePointer(0);
    auto* channelDataR = buffer.getWritePointer(1);

    float resVol = apvts.getRawParameterValue("resonance")->load();
    float bloom = apvts.getRawParameterValue("bloom")->load();
    float tone = apvts.getRawParameterValue("tone")->load();

    for (int i = 0; i < numSamples; ++i) {
        float inL = channelDataL[i];
        float inR = channelDataR[i];
        float resL = 0.0f, resR = 0.0f;

        // Le moteur génère la résonance organique
        resonator.process(inL, inR, resL, resR, bloom, tone);

        // LE PRINCIPE ABSOLU : Le Timbre Dry n'est JAMAIS touché.
        // On glisse simplement le nuage de réflexions sous la note.
        channelDataL[i] = inL + (resL * resVol);
        channelDataR[i] = inR + (resR * resVol);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SuperNaturalResonator();
}
