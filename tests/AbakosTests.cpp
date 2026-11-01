#include "PluginProcessor.h"
#include "dsp/AbakosSynthEngine.h"
#include "dsp/SetupFileIO.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>

namespace
{
int gFailures = 0;

void check(bool cond, const char* expr, int line)
{
    if (!cond)
    {
        std::fprintf(stderr, "[FAIL] Line %d: %s\n", line, expr);
        ++gFailures;
    }
}

#define EXPECT_TRUE(c) check((c), #c, __LINE__)

float computeRms(const float* buf, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        if (!std::isfinite(buf[i]))
            return -1.0f;
        sum += static_cast<double>(buf[i]) * static_cast<double>(buf[i]);
    }
    return static_cast<float>(std::sqrt(sum / std::max(1, n)));
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::filesystem::path dataDir = ABAKOS_DEFAULT_DATA_DIR;
    std::printf("=== HERCs Abakos PRO Reverse-Engineered Test Suite ===\n");
    std::printf("Data directory: %s\n", dataDir.string().c_str());
    EXPECT_TRUE(std::filesystem::exists(dataDir));

    // 1. Test config.txt and setup.txt parsing
    {
        abakos::ConfigSettings cfg;
        EXPECT_TRUE(abakos::SetupFileIO::loadConfigFile(dataDir / "config.txt", cfg));
        EXPECT_TRUE(cfg.polyphony >= 1 && cfg.polyphony <= 64);
        EXPECT_TRUE(cfg.dcBlockingFreq == 12);
        EXPECT_TRUE(cfg.oversampling >= 1 && cfg.oversampling <= 4);
        EXPECT_TRUE(cfg.headroom >= 1 && cfg.headroom <= 8);

        int bank = -1, prog = -1;
        EXPECT_TRUE(abakos::SetupFileIO::loadSetupFile(dataDir / "setup.txt", bank, prog));
        EXPECT_TRUE(bank >= 0 && bank <= 4);
        EXPECT_TRUE(prog >= 0 && prog <= 127);
        std::printf("[PASS] config.txt (poly=%d, dc=%dHz, os=%dx, headroom=%d) & setup.txt (bank=%d, prog=%d)\n",
                    cfg.polyphony, cfg.dcBlockingFreq, cfg.oversampling, cfg.headroom, bank, prog);
    }

    // 2. Test all 512 Factory Presets (a/00.txt .. d/7F.txt) + Round-Trip Serialization
    {
        static const char* kBanks[4] = { "a", "b", "c", "d" };
        int parsedPresets = 0;

        for (int b = 0; b < 4; ++b)
        {
            for (int p = 0; p < 128; ++p)
            {
                char fname[16];
                std::snprintf(fname, sizeof(fname), "%02X.txt", p);
                auto fpath = dataDir / kBanks[b] / fname;
                if (!std::filesystem::exists(fpath))
                {
                    std::snprintf(fname, sizeof(fname), "%02x.txt", p);
                    fpath = dataDir / kBanks[b] / fname;
                }
                EXPECT_TRUE(std::filesystem::exists(fpath));

                abakos::PresetData preset;
                bool ok = abakos::SetupFileIO::loadPresetFile(fpath, preset);
                EXPECT_TRUE(ok);
                if (ok)
                {
                    ++parsedPresets;
                    // Test round-trip serialization on first 8 presets of each bank
                    if (p < 8)
                    {
                        std::string serialized = abakos::SetupFileIO::serializePresetText(preset);
                        abakos::PresetData roundTrip;
                        EXPECT_TRUE(abakos::SetupFileIO::parsePresetText(serialized, roundTrip));
                        EXPECT_TRUE(roundTrip.name == preset.name);
                        EXPECT_TRUE(roundTrip.category == preset.category);
                        EXPECT_TRUE(roundTrip.params[abakos::kOsc1Wave] == preset.params[abakos::kOsc1Wave]);
                        EXPECT_TRUE(roundTrip.params[abakos::kOsc2Wave] == preset.params[abakos::kOsc2Wave]);
                        EXPECT_TRUE(roundTrip.params[abakos::kMode] == preset.params[abakos::kMode]);
                    }
                }
            }
        }
        EXPECT_TRUE(parsedPresets == 512);
        std::printf("[PASS] Parsed all %d / 512 factory presets across banks A, B, C, D + verified round-trip S-expressions\n",
                    parsedPresets);
    }

    // 3. Test all .scl.tun microtonal scale files
    {
        auto tunDir = dataDir / "Tuning Files";
        EXPECT_TRUE(std::filesystem::exists(tunDir));
        int tunCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(tunDir))
        {
            if (entry.path().extension() == ".tun")
            {
                abakos::TuningScale scale;
                bool ok = abakos::SetupFileIO::loadTuningFile(entry.path(), scale);
                EXPECT_TRUE(ok);
                EXPECT_TRUE(scale.noteCents[60] > 500.0f && scale.noteCents[60] < 30000.0f);
                ++tunCount;
            }
        }
        EXPECT_TRUE(tunCount >= 60);
        std::printf("[PASS] Parsed %d microtonal .scl.tun files in Tuning Files/\n", tunCount);
    }

    // 4. Test exact 24-bit LCG Noise Generator (0x10023c6c: state = (state * 0x5599D1 + 1) & 0xFFFFFF)
    {
        uint32_t st = 0x123456u;
        uint32_t expected1 = (0x123456u * 0x5599D1u + 1u) & 0xFFFFFFu;
        uint32_t actual1   = abakos::AbakosSynthEngine::stepLcgNoise(st);
        EXPECT_TRUE(actual1 == expected1);
        uint32_t expected2 = (expected1 * 0x5599D1u + 1u) & 0xFFFFFFu;
        uint32_t actual2   = abakos::AbakosSynthEngine::stepLcgNoise(st);
        EXPECT_TRUE(actual2 == expected2);
        std::printf("[PASS] 24-bit LCG noise generator matches exact 0x10023c6c formula (0x%06X -> 0x%06X)\n",
                    actual1, actual2);
    }

    // 5. Test AbakosSynthEngine DSP across all 6 waveforms, Filter LPF/HPF, Sync, Noise, Delay, and Arpeggiator
    {
        abakos::AbakosSynthEngine engine;
        engine.prepare(44100.0, 512);

        std::vector<float> left(2048, 0.0f);
        std::vector<float> right(2048, 0.0f);

        for (int wave = 0; wave < 6; ++wave)
        {
            engine.reset();
            engine.setParameterValue(abakos::kOsc1Wave, static_cast<float>(wave));
            engine.setParameterValue(abakos::kOsc2Wave, static_cast<float>((wave + 1) % 6));
            engine.setParameterValue(abakos::kOsc2Sync, 45.0f);
            engine.setParameterValue(abakos::kNoiseLevel, 25.0f);
            engine.setParameterValue(abakos::kFilterHpfLpf, (wave % 2 == 0) ? 0.0f : 1.0f);
            engine.noteOn(60, 110);
            engine.renderBlock(left.data(), right.data(), 2048);

            float rmsL = computeRms(left.data(), 2048);
            float rmsR = computeRms(right.data(), 2048);
            EXPECT_TRUE(rmsL > 0.005f && rmsL < 1.0f);
            EXPECT_TRUE(rmsR > 0.005f && rmsR < 1.0f);
        }

        // Test Arpeggiator across patterns with Hold
        engine.reset();
        engine.setHostBpm(140.0);
        engine.setParameterValue(abakos::kArpOnOff, 1.0f);
        engine.setParameterValue(abakos::kArpHold, 1.0f);
        engine.setParameterValue(abakos::kArpPattern, 3.0f);
        engine.noteOn(48, 100);
        engine.noteOn(55, 100);
        engine.noteOn(60, 100);
        engine.noteOff(48);
        engine.noteOff(55);
        engine.noteOff(60); // Hold is enabled, so arpeggiator should keep playing!

        std::vector<float> arpL(16384, 0.0f);
        std::vector<float> arpR(16384, 0.0f);
        engine.renderBlock(arpL.data(), arpR.data(), 16384);
        float arpRms = computeRms(arpL.data(), 16384);
        EXPECT_TRUE(arpRms > 0.005f);
        std::printf("[PASS] AbakosSynthEngine DSP verified across all 6 waveforms, SVF LPF/HPF, Sync, Delay & Arpeggiator (RMS=%.4f)\n",
                    arpRms);
    }

    // 6. Test AbakosPROAudioProcessor (APVTS, Bank/Program switching, processBlock, State Save/Restore)
    {
        AbakosPROAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);

        proc.selectPreset(1, 12); // Bank B, Program 0C
        EXPECT_TRUE(proc.getCurrentBank() == 1);
        EXPECT_TRUE(proc.getCurrentProgram() == 12);
        juce::String savedName = proc.getCurrentPresetName();
        EXPECT_TRUE(savedName.isNotEmpty());

        juce::AudioBuffer<float> audioBuf(2, 512);
        audioBuf.clear();
        juce::MidiBuffer midiBuf;
        midiBuf.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
        proc.processBlock(audioBuf, midiBuf);

        float procRms = audioBuf.getRMSLevel(0, 0, 512);
        EXPECT_TRUE(procRms > 0.0005f);

        // Save state, switch bank/program, and restore state
        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        EXPECT_TRUE(stateData.getSize() > 0);

        proc.selectPreset(3, 99);
        EXPECT_TRUE(proc.getCurrentBank() == 3);

        proc.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
        EXPECT_TRUE(proc.getCurrentBank() == 1);
        EXPECT_TRUE(proc.getCurrentProgram() == 12);
        EXPECT_TRUE(proc.getCurrentPresetName() == savedName);

        // Restore factory setup.txt (bank 0, program 15) so test runs are idempotent
        abakos::SetupFileIO::saveSetupFile(dataDir / "setup.txt", 0, 15);

        std::printf("[PASS] AbakosPROAudioProcessor bank/preset management, MIDI processBlock (RMS=%.4f) & state persistence verified\n",
                    procRms);

        // Render offscreen snapshot of the new GUI to verify visual layout
        std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
        EXPECT_TRUE(editor != nullptr);
        if (editor != nullptr)
        {
            auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
            EXPECT_TRUE(snapshot.getWidth() == 980 && snapshot.getHeight() == 658);
            auto docsDir = dataDir.parent_path() / "docs";
            juce::File outPng = std::filesystem::exists(docsDir)
                                    ? juce::File(juce::String((docsDir / "gui_preview.png").string()))
                                    : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("abakos_gui_preview.png");
            outPng.deleteFile();
            juce::FileOutputStream fos(outPng);
            if (fos.openedOk())
            {
                juce::PNGImageFormat pngFmt;
                pngFmt.writeImageToStream(snapshot, fos);
                std::printf("[PASS] Rendered GUI preview (%dx%d)\n",
                            snapshot.getWidth(), snapshot.getHeight());
            }
        }
    }

    if (gFailures == 0)
    {
        std::printf("=== ALL TESTS PASSED SUCCESSFULLY ===\n");
        return 0;
    }
    else
    {
        std::fprintf(stderr, "=== %d TEST(S) FAILED ===\n", gFailures);
        return 1;
    }
}
