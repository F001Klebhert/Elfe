#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <cmath>

// Ligne de délai fractionnelle avec interpolation (pour la résonance type Plate)
class FractionalDelay {
public:
    void prepare(double sr, float maxDelayMs) {
        buffer.assign(static_cast<size_t>(sr * (maxDelayMs / 1000.0f)) + 100, 0.0f);
        writePos = 0;
    }
    float process(float input, float delaySamples) {
        buffer[writePos] = input;
        float readPos = static_cast<float>(writePos) - delaySamples;
        if (readPos < 0.0f) readPos += static_cast<float>(buffer.size());
        
        int i0 = static_cast<int>(readPos);
        int i1 = (i0 + 1) % buffer.size();
        float frac = readPos - static_cast<float>(i0);
        
        float out = buffer[i0] + frac * (buffer[i1] - buffer[i0]);
        
        writePos++;
        if (writePos >= buffer.size()) writePos = 0;
        return out;
    }
private:
    std::vector<float> buffer;
    int writePos = 0;
};

// Filtre d'étouffement acoustique (Damping du bois ou du métal)
class DampingFilter {
public:
    void prepare(double sr) { sampleRate = sr; z1 = 0.0f; }
    void setTone(float freq) {
        alpha = std::exp(-6.28318530718f * freq / static_cast<float>(sampleRate));
    }
    float process(float in) {
        z1 = in * (1.0f - alpha) + z1 * alpha;
        return z1;
    }
private:
    float sampleRate = 44100.0f, alpha = 0.0f, z1 = 0.0f;
};

// Moteur de résonance (Réseau FDN 4x4 Hautes Densités)
class AcousticBodyResonator {
public:
    void prepare(double sr) {
        sampleRate = sr;
        // Temps très courts, basés sur des nombres premiers pour créer l'illusion
        // de la résonance du corps sans créer une queue de cathédrale.
        delayL1.prepare(sr, 50.0f); baseTime1 = 11.3f * (sr / 1000.0f);
        delayL2.prepare(sr, 50.0f); baseTime2 = 17.1f * (sr / 1000.0f);
        delayL3.prepare(sr, 50.0f); baseTime3 = 23.3f * (sr / 1000.0f);
        delayL4.prepare(sr, 50.0f); baseTime4 = 31.7f * (sr / 1000.0f);
        
        damp1.prepare(sr); damp2.prepare(sr); damp3.prepare(sr); damp4.prepare(sr);
        lfoPhase = 0.0f;
    }

    void process(float inL, float inR, float& outL, float& outR, float sustain, float tone) {
        float input = (inL + inR) * 0.5f; // On excite la caisse de résonance avec le centre
        
        damp1.setTone(tone); damp2.setTone(tone); damp3.setTone(tone); damp4.setTone(tone);
        
        // Modulation très subtile pour l'effet "Plate / Harmonic Bloom"
        lfoPhase += 2.0f * 3.14159265359f * 1.5f / sampleRate; // 1.5 Hz
        if (lfoPhase > 6.28318530718f) lfoPhase -= 6.28318530718f;
        
        float mod = std::sin(lfoPhase) * 3.0f; 
        
        // Lecture des délais (Feedback)
        float read1 = delayL1.process(fb1, baseTime1 + mod);
        float read2 = delayL2.process(fb2, baseTime2 - mod);
        float read3 = delayL3.process(fb3, baseTime3 + mod * 0.5f);
        float read4 = delayL4.process(fb4, baseTime4 - mod * 0.5f);
        
        // Matrice de Hadamard (Diffusion ultrarapide de l'air)
        float v1 = read1 + read2 + read3 + read4;
        float v2 = read1 - read2 + read3 - read4;
        float v3 = read1 + read2 - read3 - read4;
        float v4 = read1 - read2 - read3 + read4;
        
        // Damping et renvoi dans la boucle
        fb1 = input + damp1.process(v1) * 0.5f * sustain;
        fb2 = input + damp2.process(v2) * 0.5f * sustain;
        fb3 = input + damp3.process(v3) * 0.5f * sustain;
        fb4 = input + damp4.process(v4) * 0.5f * sustain;
        
        outL = read1 - read3;
        outR = read2 - read4;
    }
private:
    FractionalDelay delayL1, delayL2, delayL3, delayL4;
    DampingFilter damp1, damp2, damp3, damp4;
    float fb1 = 0, fb2 = 0, fb3 = 0, fb4 = 0;
    float baseTime1, baseTime2, baseTime3, baseTime4;
    float sampleRate = 44100.0f, lfoPhase = 0.0f;
};

class SuperNaturalResonator : public juce::AudioProcessor {
public:
    SuperNaturalResonator();
    ~SuperNaturalResonator() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SuperNatural Resonator"; }
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
    AcousticBodyResonator resonator;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperNaturalResonator)
};
