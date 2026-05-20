#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <cmath>

// Filtre All-Pass pur (Zéro écho, 100% "Smearing" de transitoire)
class DiffuserAP {
public:
    void prepare(double sr, float delayMs) {
        buffer.assign(static_cast<size_t>(sr * (delayMs / 1000.0f)) + 10, 0.0f);
        writePos = 0;
    }
    float process(float in) {
        float delayed = buffer[writePos];
        float out = -0.6f * in + delayed;
        buffer[writePos] = in + 0.6f * delayed;
        writePos++;
        if (writePos >= buffer.size()) writePos = 0;
        return out;
    }
private:
    std::vector<float> buffer;
    int writePos = 0;
};

// Ligne de délai sans résonance
class SimpleDelay {
public:
    void prepare(double sr, float delayMs) {
        buffer.assign(static_cast<size_t>(sr * (delayMs / 1000.0f)) + 10, 0.0f);
        writePos = 0;
    }
    float process(float in) {
        float out = buffer[writePos];
        buffer[writePos] = in;
        writePos++;
        if (writePos >= buffer.size()) writePos = 0;
        return out;
    }
private:
    std::vector<float> buffer;
    int writePos = 0;
};

// Filtre radical pour sculpter le "Tone" du bois ou du métal
class ToneFilter {
public:
    void prepare(double sr) { sampleRate = sr; z1 = 0.0f; }
    void setFreq(float freq, bool isLP) {
        float w0 = 6.28318530718f * freq / static_cast<float>(sampleRate);
        alpha = w0 / (1.0f + w0);
        isLowPass = isLP;
    }
    float process(float in) {
        z1 = z1 + alpha * (in - z1);
        return isLowPass ? z1 : (in - z1);
    }
private:
    float sampleRate = 44100.0f, alpha = 0.0f, z1 = 0.0f;
    bool isLowPass = true;
};

// Moteur de résonance physique de type "Plate / Soundboard"
class PhysicalBloomEngine {
public:
    void prepare(double sr) {
        // Temps ultra-courts (<15ms) pour interdire la sensation de "pièce"
        ap1L.prepare(sr, 2.1f); ap2L.prepare(sr, 3.5f); ap3L.prepare(sr, 5.8f); ap4L.prepare(sr, 8.4f);
        ap1R.prepare(sr, 2.5f); ap2R.prepare(sr, 3.9f); ap3R.prepare(sr, 6.1f); ap4R.prepare(sr, 8.9f);
        
        delL.prepare(sr, 12.0f); delR.prepare(sr, 13.0f);
        
        toneFilterL.prepare(sr); toneFilterR.prepare(sr);
        antiMudL.prepare(sr); antiMudL.setFreq(150.0f, false); // Coupe-bas à 150Hz
        antiMudR.prepare(sr); antiMudR.setFreq(150.0f, false);
    }

    void process(float inL, float inR, float& outL, float& outR, float bloom, float tone) {
        // La courbe exponentielle magique pour que le Bloom soit parfait de 0 à 100%
        float feedback = 0.3f + (0.68f * std::pow(bloom, 0.6f));
        
        // Configuration radicale du Tone (De 400Hz sourd à 12000Hz brillant)
        float freq = 400.0f + std::pow(tone, 2.0f) * 11600.0f;
        toneFilterL.setFreq(freq, true);
        toneFilterR.setFreq(freq, true);

        // Boucle de résonance physique Gauche
        float loopInL = inL + (fbR * feedback); // Cross-feedback
        float washL = ap4L.process(ap3L.process(ap2L.process(ap1L.process(loopInL))));
        fbL = toneFilterL.process(delL.process(washL));
        
        // Boucle de résonance physique Droite (avec inversion de phase pour la chaleur stéréo)
        float loopInR = inR - (fbL * feedback); // Inversion pour éviter les résonances métalliques statiques
        float washR = ap4R.process(ap3R.process(ap2R.process(ap1R.process(loopInR))));
        fbR = toneFilterR.process(delR.process(washR));
        
        // On nettoie les basses du corps de résonance pour ne pas salir le mix
        outL = antiMudL.process(fbL);
        outR = antiMudR.process(fbR);
    }
private:
    DiffuserAP ap1L, ap2L, ap3L, ap4L, ap1R, ap2R, ap3R, ap4R;
    SimpleDelay delL, delR;
    ToneFilter toneFilterL, toneFilterR, antiMudL, antiMudR;
    float fbL = 0.0f, fbR = 0.0f;
};

class PhysicalBloomMaster : public juce::AudioProcessor {
public:
    PhysicalBloomMaster();
    ~PhysicalBloomMaster() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PhysicalBloom Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    juce::AudioProcessorValueTreeState apvts;
    PhysicalBloomEngine bloomEngine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalBloomMaster)
};
