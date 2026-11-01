#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/AbakosTypes.h"
#include "dsp/AbakosSynthEngine.h"
#include "dsp/SetupFileIO.h"
#include <array>
#include <atomic>
#include <filesystem>

class AbakosPROAudioProcessor : public juce::AudioProcessor
{
public:
    AbakosPROAudioProcessor();
    ~AbakosPROAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter management
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Bank & Preset management (Banks 0=A, 1=B, 2=C, 3=D, 4=External)
    void selectBank(int bankIndex);
    int getCurrentBank() const noexcept { return currentBank_.load(std::memory_order_relaxed); }
    void selectPreset(int bankIndex, int programIndex);
    void stepPreset(int delta);

    juce::String getCurrentPresetName() const;
    juce::String getCurrentPresetCategory() const;
    void setCurrentPresetName(const juce::String& name);

    bool loadExternalPresetFile(const juce::File& file);
    bool saveCurrentPresetToFile(const juce::File& file);
    abakos::PresetData captureCurrentPresetData() const;

    // Microtonal Tuning (.scl.tun) management
    bool loadTuningFile(const juce::File& file);
    void resetTuningTo12TET();
    juce::String getActiveTuningName() const;
    juce::File getTuningDirectory() const;
    juce::File getDataDirectory() const;

    // Engine Configuration (matching config.exe / config.txt)
    abakos::ConfigSettings getConfigSettings() const noexcept { return synthEngine_.getConfigSettings(); }
    void applyConfigSettings(const abakos::ConfigSettings& cfg, bool saveToDisk = true);

    // GUI Keyboard & Wheel interaction
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState_; }
    abakos::AbakosSynthEngine& getSynthEngine() noexcept { return synthEngine_; }
    const abakos::AbakosSynthEngine& getSynthEngine() const noexcept { return synthEngine_; }

private:
    void locateAndLoadFactoryData();
    void syncEngineFromAPVTS() noexcept;
    void pushPresetToAPVTS(const abakos::PresetData& preset);

    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, abakos::kNumParams> paramPtrs_ {};

    abakos::AbakosSynthEngine synthEngine_;
    juce::MidiKeyboardState keyboardState_;

    std::filesystem::path dataDir_;
    // 4 factory banks (A, B, C, D) + 1 External bank, 128 programs each
    std::array<std::array<abakos::PresetData, 128>, 5> banks_ {};
    std::atomic<int> currentBank_ { 0 };
    std::atomic<int> currentProgram_ { 0 };
    juce::CriticalSection presetLock_;
    juce::String activePresetName_ { "Abakos Pro" };
    juce::String activePresetCategory_ { "SYNTH" };
    juce::String activeTuningName_ { "12-TET Standard" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbakosPROAudioProcessor)
};
