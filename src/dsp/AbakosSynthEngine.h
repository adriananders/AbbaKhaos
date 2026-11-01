///////////////////////////////////////////////////////////////////////////////////
// MIT License                                                                   //
//                                                                               //
// Copyright (c) 2014-2016 Robert P. Wolf (HERCs Music Systems / Lunar DSP)      //
// Copyright (c) 2026 GlitchyGloomy (64-bit JUCE 8 Synthesizer Implementation)   //
//                                                                               //
// Permission is hereby granted, free of charge, to any person obtaining a copy  //
// of this software and associated documentation files (the "Software"), to deal //
// in the Software without restriction, including without limitation the rights  //
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell     //
// copies of the Software, and to permit persons to whom the Software is         //
// furnished to do so, subject to the following conditions:                      //
//                                                                               //
// The above copyright notice and this permission notice shall be included in    //
// all copies or substantial portions of the Software.                           //
//                                                                               //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   //
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, //
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN     //
// THE SOFTWARE.                                                                 //
///////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "AbakosTypes.h"
#include "Arpeggiator.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace abakos
{

class WavetableBank
{
public:
    static constexpr int kNumWaveforms = 6;
    static constexpr int kNumOctaves   = 11;
    static constexpr int kTableSize    = 2048;

    WavetableBank();

    float sample(int waveform, float phase, float normalizedFreq) const noexcept;

private:
    // tables_[waveform][octave][sampleIndex]
    std::array<std::array<std::array<float, kTableSize>, kNumOctaves>, kNumWaveforms> tables_ {};
};

struct AdsEnvelope
{
    enum Stage { Idle = 0, Attack, Decay, Sustain };
    Stage stage = Idle;
    float value = 0.0f;

    void reset() noexcept { stage = Idle; value = 0.0f; }
    void noteOn() noexcept { stage = Attack; }
    void noteOff() noexcept { stage = Idle; value = 0.0f; }
    float process(float attackRate, float decayRate, float sustainLevel) noexcept;
};

struct AdsrEnvelope
{
    enum Stage { Idle = 0, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0.0f;

    void reset() noexcept { stage = Idle; value = 0.0f; }
    void noteOn() noexcept { stage = Attack; }
    void noteOff() noexcept { if (stage != Idle) stage = Release; }
    bool isActive() const noexcept { return stage != Idle; }
    float process(float attackRate, float decayRate, float sustainLevel, float releaseRate) noexcept;
};

class AbakosSynthEngine
{
public:
    static constexpr int kMaxVoices = 64;

    AbakosSynthEngine();

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // Parameter & configuration updates (lock-free / real-time safe)
    void setParameterValue(int paramIndex, float value) noexcept;
    float getParameterValue(int paramIndex) const noexcept;

    void applyPreset(const PresetData& preset) noexcept;
    void setConfigSettings(const ConfigSettings& config) noexcept;
    const ConfigSettings& getConfigSettings() const noexcept { return config_; }

    void setTuningScale(const TuningScale& scale) noexcept;
    const TuningScale& getTuningScale() const noexcept { return tuningScale_; }

    // Real-time MIDI events
    void noteOn(int midiNote, int velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;
    void setPitchBend(float normalizedBend) noexcept; // -1.0 .. +1.0
    void setModWheel(float normalizedMod) noexcept;    //  0.0 .. +1.0
    void setHostBpm(double bpm) noexcept;

    float getPitchBend() const noexcept { return pitchBend_; }
    float getModWheel() const noexcept { return modWheel_; }
    bool isNoteActive(int midiNote) const noexcept;

    // Render stereo audio block (100% real-time safe: zero allocations, zero locks)
    void renderBlock(float* outLeft, float* outRight, int numSamples) noexcept;

    // Expose deterministic LCG step for unit testing (matches 0x10023c6c)
    static uint32_t stepLcgNoise(uint32_t& state) noexcept
    {
        state = (state * 0x5599D1u + 1u) & 0xFFFFFFu;
        return state;
    }

private:
    struct Voice
    {
        bool active       = false;
        bool keyDown      = false;
        int midiNote      = 60;
        float velocity    = 1.0f;
        uint64_t age      = 0;

        float currentPitchCents = 6000.0f;
        float targetPitchCents  = 6000.0f;

        float osc1Phase   = 0.0f;
        float osc2Phase   = 0.0f;

        uint32_t noiseState = 0x123456u;
        float noiseFilterLp = 0.0f;

        // Chamberlin 2-pole SVF state per oversampled voice
        float svfLow      = 0.0f;
        float svfBand     = 0.0f;

        AdsEnvelope ads1;
        AdsEnvelope ads2;
        AdsrEnvelope adsr1;
        AdsrEnvelope adsr2;

        void reset(uint32_t seed) noexcept;
    };

    static void arpNoteOnBridge(void* ctx, int note, int vel) noexcept;
    static void arpNoteOffBridge(void* ctx, int note) noexcept;
    void internalNoteOn(int midiNote, int velocity) noexcept;
    void internalNoteOff(int midiNote) noexcept;

    float computeTimeRate(float param0to100, float minSec, float maxSec, float sr) const noexcept;

    static const WavetableBank& getWavetables();

    double sampleRate_ = 44100.0;
    ConfigSettings config_ {};
    TuningScale tuningScale_ {};
    Arpeggiator arpeggiator_ {};

    std::array<float, kNumParams> params_ {};
    std::array<Voice, kMaxVoices> voices_ {};
    uint64_t voiceCounter_ = 0;
    float lastMonoPitchCents_ = 6000.0f;
    bool hasLastMonoPitch_    = false;

    float pitchBend_ = 0.0f;
    float modWheel_  = 0.0f;
    float lfoPhase_  = 0.0f;

    // Smoothed master parameters
    float smoothCutoff_    = 78.0f;
    float smoothResonance_ = 20.0f;
    float smoothMasterVol_ = 0.85f;

    // Stereo delay circular buffers (pre-allocated in prepare())
    std::vector<float> delayBufL_;
    std::vector<float> delayBufR_;
    int delayWritePos_ = 0;
    float delayLpL_    = 0.0f;
    float delayLpR_    = 0.0f;

    // DC Blocker state
    float dcInPrevL_  = 0.0f;
    float dcOutPrevL_ = 0.0f;
    float dcInPrevR_  = 0.0f;
    float dcOutPrevR_ = 0.0f;

    // Bitmask of active MIDI notes for GUI keyboard feedback
    std::array<std::atomic<uint32_t>, 4> activeNotesMask_ {};
};

} // namespace abakos
