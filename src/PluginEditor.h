#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>
#include <vector>

class AbakosPROAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit AbakosPROAudioProcessorEditor(AbakosPROAudioProcessor&);
    ~AbakosPROAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void showPresetPopupMenu();
    void showTuningPopupMenu();
    void showArpPatternPopupMenu();
    void showConfigDialog();
    void loadPresetWithChooser();
    void savePresetWithChooser();

    void drawCard(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent) const;
    void drawFilterResponse(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawAdsEnvelopePreview(juce::Graphics& g, juce::Rectangle<int> bounds) const;
    void drawAdsrEnvelopePreview(juce::Graphics& g, juce::Rectangle<int> bounds) const;

    AbakosPROAudioProcessor& processorRef_;

    class ModernKnob;
    class ModernBarSlider;
    class WaveformStrip;
    class PillToggle;
    class ValueStepper;
    class ActionBadge;
    class ModernWheel;
    class VectorKeyboard;

    std::vector<std::unique_ptr<ModernKnob>> knobs_;
    std::unique_ptr<ModernBarSlider> glideSlider_;
    std::unique_ptr<ModernBarSlider> masterSlider_;
    std::unique_ptr<WaveformStrip> osc1WaveStrip_;
    std::unique_ptr<WaveformStrip> osc2WaveStrip_;
    std::unique_ptr<PillToggle> filterModeToggle_;
    std::unique_ptr<PillToggle> voiceModeToggle_;
    std::unique_ptr<PillToggle> pbRangeToggle_;
    std::unique_ptr<PillToggle> arpOnToggle_;
    std::unique_ptr<PillToggle> arpHoldToggle_;
    std::unique_ptr<ValueStepper> tuneStepper_;
    std::unique_ptr<ValueStepper> octaveStepper_;
    std::unique_ptr<ValueStepper> arpPatternStepper_;
    std::unique_ptr<ModernWheel> pitchWheel_;
    std::unique_ptr<ModernWheel> modWheel_;
    std::unique_ptr<VectorKeyboard> keyboardComp_;

    // Header & Bank custom vector badges
    std::array<std::unique_ptr<ActionBadge>, 5> bankBadges_;
    std::unique_ptr<ActionBadge> prevPresetBadge_;
    std::unique_ptr<ActionBadge> nextPresetBadge_;
    std::unique_ptr<ActionBadge> loadPresetBadge_;
    std::unique_ptr<ActionBadge> savePresetBadge_;
    std::unique_ptr<ActionBadge> tunBadge_;
    std::unique_ptr<ActionBadge> cfgBadge_;

    juce::Rectangle<int> presetLcdBounds_;
    juce::Rectangle<int> filterGraphBounds_;
    juce::Rectangle<int> adsGraphBounds_;
    juce::Rectangle<int> adsrGraphBounds_;

    std::unique_ptr<juce::FileChooser> activeFileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbakosPROAudioProcessorEditor)
};
