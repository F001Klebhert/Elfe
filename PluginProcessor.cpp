#include "PluginProcessor.h"
#include <memory>

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Intensité pure de la vibration sous l'instrument
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("body", 1), "Body Vibration", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));
        
    // Longueur de la résonance (Réponse de la pédale forte / Sympathetic Strings)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bloom", 1), "Harmonic Bloom", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        
    // Matériau du corps : 0 = Bois lourd et mat / 1.0 = Plaque de métal étincelante
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tone", 1), "Material Tone", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));
        
    return layout;
}

PhysicalBloomMaster::PhysicalBloomMaster()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

void PhysicalBloomMaster::prepareToPlay(double sampleRate, int samplesPerBlock) {
    bloomEngine.prepare(sampleRate);
}

void PhysicalBloomMaster::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    auto* channelDataL = buffer.getWritePointer(0);
    auto* channelDataR = buffer.getWritePointer(1);

    float bodyVol = apvts.getRawParameterValue("body")->load();
    float bloom = apvts.getRawParameterValue("bloom")->load();
    float tone = apvts.getRawParameterValue("tone")->load();

    for (int i = 0; i < numSamples; ++i) {
        float inL = channelDataL[i];
        float inR = channelDataR[i];
        float bloomL = 0.0f, bloomR = 0.0f;

        // Génération du nuage harmonique (Zéro traitement sur le signal inL/inR d'origine)
        bloomEngine.process(inL, inR, bloomL, bloomR, bloom, tone);

        // Sommation parallèle absolue. Le timbre original est respecté à 100%.
        channelDataL[i] = inL + (bloomL * bodyVol);
        channelDataR[i] = inR + (bloomR * bodyVol);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhysicalBloomMaster();
}
