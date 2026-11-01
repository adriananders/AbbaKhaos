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

#include "AbakosSynthEngine.h"
#include <algorithm>
#include <cmath>

namespace abakos
{

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kPi    = 3.14159265358979323846f;

inline float clampf(float v, float lo, float hi) noexcept
{
    return std::max(lo, std::min(hi, v));
}
} // namespace

WavetableBank::WavetableBank()
{
    // Build band-limited mipmapped wavetables for all 6 Abakos PRO waveforms across 11 octaves
    for (int oct = 0; oct < kNumOctaves; ++oct)
    {
        // Max harmonic for this octave mipmap to prevent aliasing up to Nyquist
        const int maxHarmonic = std::max(1, (kTableSize / 4) >> oct);

        for (int w = 0; w < kNumWaveforms; ++w)
        {
            auto& tbl = tables_[static_cast<size_t>(w)][static_cast<size_t>(oct)];
            tbl.fill(0.0f);

            if (w == 3) // 3: Pure Sine
            {
                for (int i = 0; i < kTableSize; ++i)
                {
                    float ph = static_cast<float>(i) / static_cast<float>(kTableSize);
                    tbl[static_cast<size_t>(i)] = std::sin(kTwoPi * ph);
                }
                continue;
            }

            for (int h = 1; h <= maxHarmonic; ++h)
            {
                // Gibbs phenomenon Lanczos sigma factor
                float sigma = 1.0f;
                if (maxHarmonic > 1)
                {
                    float x = ( static_cast<float>(h) * kPi ) / static_cast<float>(maxHarmonic + 1);
                    sigma = std::sin(x) / x;
                }

                float amp = 0.0f;
                float phaseOffset = 0.0f;

                switch (w)
                {
                    case 0: // Sawtooth: sum (-1)^(h+1) / h
                        amp = ((h & 1) ? 1.0f : -1.0f) * (1.0f / static_cast<float>(h));
                        break;

                    case 1: // Square (50% duty): odd harmonics 1/h
                        if ((h & 1) == 1)
                            amp = 1.0f / static_cast<float>(h);
                        break;

                    case 2: // Triangle: odd harmonics alternating sign 1/h^2
                        if ((h & 1) == 1)
                        {
                            int k = (h - 1) / 2;
                            amp = ((k & 1) ? -1.0f : 1.0f) / static_cast<float>(h * h);
                        }
                        break;

                    case 4: // Pulse (25% duty): sin(h * pi * 0.25) / h
                        amp = std::sin(static_cast<float>(h) * kPi * 0.25f) * (2.0f / (static_cast<float>(h) * kPi));
                        phaseOffset = kPi * 0.5f;
                        break;

                    case 5: // Organ / Dual-Harmonic Rich Wave (drawbar-like + warm odd/octave harmonics)
                        if (h == 1) amp = 1.0f;
                        else if (h == 2) amp = 0.8f;
                        else if (h == 3) amp = 0.6f;
                        else if (h == 4) amp = 0.5f;
                        else if (h == 6) amp = 0.35f;
                        else if (h == 8) amp = 0.25f;
                        else if ((h & 1) == 1) amp = 0.3f / static_cast<float>(h);
                        break;

                    default:
                        break;
                }

                amp *= sigma;
                if (std::abs(amp) < 1e-7f)
                    continue;

                for (int i = 0; i < kTableSize; ++i)
                {
                    float angle = kTwoPi * static_cast<float>(h) * (static_cast<float>(i) / static_cast<float>(kTableSize)) + phaseOffset;
                    tbl[static_cast<size_t>(i)] += amp * std::sin(angle);
                }
            }

            // Normalize peak amplitude to 1.0
            float peak = 0.0f;
            for (int i = 0; i < kTableSize; ++i)
                peak = std::max(peak, std::abs(tbl[static_cast<size_t>(i)]));
            if (peak > 1e-5f)
            {
                float invPeak = 1.0f / peak;
                for (int i = 0; i < kTableSize; ++i)
                    tbl[static_cast<size_t>(i)] *= invPeak;
            }
        }
    }
}

float WavetableBank::sample(int waveform, float phase, float normalizedFreq) const noexcept
{
    const int w = std::max(0, std::min(kNumWaveforms - 1, waveform));

    // Determine octave sub-table from normalizedFreq (f / sr)
    // Base table 0 covers down to ~20 Hz (20 / 44100 ≈ 0.00045)
    float ratio = std::max(1.0f, normalizedFreq * 2048.0f);
    int oct = static_cast<int>(std::floor(std::log2(ratio)));
    oct = std::max(0, std::min(kNumOctaves - 1, oct));

    const auto& tbl = tables_[static_cast<size_t>(w)][static_cast<size_t>(oct)];

    float p = phase - std::floor(phase);
    float fIdx = p * static_cast<float>(kTableSize);
    int idx0 = static_cast<int>(fIdx) & (kTableSize - 1);
    int idx1 = (idx0 + 1) & (kTableSize - 1);
    float frac = fIdx - std::floor(fIdx);

    return tbl[static_cast<size_t>(idx0)] + frac * (tbl[static_cast<size_t>(idx1)] - tbl[static_cast<size_t>(idx0)]);
}

float AdsEnvelope::process(float attackRate, float decayRate, float sustainLevel) noexcept
{
    switch (stage)
    {
        case Idle:
            value = 0.0f;
            break;
        case Attack:
            value += attackRate;
            if (value >= 1.0f)
            {
                value = 1.0f;
                stage = Decay;
            }
            break;
        case Decay:
            value -= decayRate * (value - sustainLevel + 0.001f);
            if (value <= sustainLevel + 0.0005f)
            {
                value = sustainLevel;
                stage = Sustain;
            }
            break;
        case Sustain:
            value = sustainLevel;
            break;
    }
    return value;
}

float AdsrEnvelope::process(float attackRate, float decayRate, float sustainLevel, float releaseRate) noexcept
{
    switch (stage)
    {
        case Idle:
            value = 0.0f;
            break;
        case Attack:
            value += attackRate;
            if (value >= 1.0f)
            {
                value = 1.0f;
                stage = Decay;
            }
            break;
        case Decay:
            value -= decayRate * (value - sustainLevel + 0.002f);
            if (value <= sustainLevel + 0.0005f)
            {
                value = sustainLevel;
                stage = Sustain;
            }
            break;
        case Sustain:
            value = sustainLevel;
            break;
        case Release:
            value -= releaseRate * (value + 0.01f);
            if (value <= 0.0002f)
            {
                value = 0.0f;
                stage = Idle;
            }
            break;
    }
    return value;
}

void AbakosSynthEngine::Voice::reset(uint32_t seed) noexcept
{
    active        = false;
    keyDown       = false;
    midiNote      = 60;
    velocity      = 1.0f;
    age           = 0;
    osc1Phase     = 0.0f;
    osc2Phase     = 0.0f;
    noiseState    = seed & 0xFFFFFFu;
    noiseFilterLp = 0.0f;
    svfLow        = 0.0f;
    svfBand       = 0.0f;
    ads1.reset();
    ads2.reset();
    adsr1.reset();
    adsr2.reset();
}

const WavetableBank& AbakosSynthEngine::getWavetables()
{
    static const WavetableBank kBank;
    return kBank;
}

AbakosSynthEngine::AbakosSynthEngine()
{
    const auto& meta = getParamMetaTable();
    for (size_t i = 0; i < kNumParams; ++i)
        params_[i] = static_cast<float>(meta[i].defaultVal);

    for (auto& m : activeNotesMask_)
        m.store(0u, std::memory_order_relaxed);

    reset();
}

void AbakosSynthEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = (sampleRate > 8000.0) ? sampleRate : 44100.0;
    arpeggiator_.prepare(sampleRate_);

    // Pre-allocate 4 seconds of stereo delay buffer (matches 0x10019b70 sampleRate * 4)
    const size_t maxDelaySamples = static_cast<size_t>( std::ceil(sampleRate_ * 4.0) ) + 16;
    delayBufL_.assign(maxDelaySamples, 0.0f);
    delayBufR_.assign(maxDelaySamples, 0.0f);
    delayWritePos_ = 0;
    delayLpL_      = 0.0f;
    delayLpR_      = 0.0f;

    reset();
}

void AbakosSynthEngine::reset() noexcept
{
    for (int i = 0; i < kMaxVoices; ++i)
        voices_[static_cast<size_t>(i)].reset(0x13579Bu + static_cast<uint32_t>(i * 0x10203u));

    arpeggiator_.reset();
    std::fill(delayBufL_.begin(), delayBufL_.end(), 0.0f);
    std::fill(delayBufR_.begin(), delayBufR_.end(), 0.0f);
    delayWritePos_ = 0;
    delayLpL_      = 0.0f;
    delayLpR_      = 0.0f;
    dcInPrevL_     = 0.0f;
    dcOutPrevL_    = 0.0f;
    dcInPrevR_     = 0.0f;
    dcOutPrevR_    = 0.0f;

    for (auto& m : activeNotesMask_)
        m.store(0u, std::memory_order_relaxed);
}

void AbakosSynthEngine::setParameterValue(int paramIndex, float value) noexcept
{
    if (paramIndex < 0 || paramIndex >= kNumParams)
        return;

    const auto& meta = getParamMetaTable()[static_cast<size_t>(paramIndex)];
    float clamped = clampf(value, static_cast<float>(meta.minVal), static_cast<float>(meta.maxVal));
    params_[static_cast<size_t>(paramIndex)] = clamped;

    if (paramIndex == kArpOnOff)
        arpeggiator_.setEnabled(clamped >= 0.5f);
    else if (paramIndex == kArpHold)
        arpeggiator_.setHold(clamped >= 0.5f);
    else if (paramIndex == kArpPattern)
        arpeggiator_.setPatternIndex(static_cast<int>(std::lround(clamped)));
}

float AbakosSynthEngine::getParameterValue(int paramIndex) const noexcept
{
    if (paramIndex < 0 || paramIndex >= kNumParams)
        return 0.0f;
    return params_[static_cast<size_t>(paramIndex)];
}

void AbakosSynthEngine::applyPreset(const PresetData& preset) noexcept
{
    for (int i = 0; i < kNumParams; ++i)
        setParameterValue(i, static_cast<float>(preset.params[static_cast<size_t>(i)]));
}

void AbakosSynthEngine::setConfigSettings(const ConfigSettings& config) noexcept
{
    config_.polyphony      = std::max(1, std::min(kMaxVoices, config.polyphony));
    config_.dcBlockingFreq = std::max(1, std::min(100, config.dcBlockingFreq));
    config_.oversampling   = std::max(1, std::min(4, config.oversampling));
    config_.headroom       = std::max(1, std::min(8, config.headroom));
    config_.multitimbral   = std::max(0, std::min(1, config.multitimbral));
}

void AbakosSynthEngine::setTuningScale(const TuningScale& scale) noexcept
{
    tuningScale_ = scale;
}

void AbakosSynthEngine::arpNoteOnBridge(void* ctx, int note, int vel) noexcept
{
    static_cast<AbakosSynthEngine*>(ctx)->internalNoteOn(note, vel);
}

void AbakosSynthEngine::arpNoteOffBridge(void* ctx, int note) noexcept
{
    static_cast<AbakosSynthEngine*>(ctx)->internalNoteOff(note);
}

void AbakosSynthEngine::noteOn(int midiNote, int velocity) noexcept
{
    midiNote = std::max(0, std::min(127, midiNote));
    if (velocity <= 0)
    {
        noteOff(midiNote);
        return;
    }

    arpeggiator_.handleNoteOn(midiNote, velocity, this, &AbakosSynthEngine::arpNoteOnBridge, &AbakosSynthEngine::arpNoteOffBridge);
}

void AbakosSynthEngine::noteOff(int midiNote) noexcept
{
    midiNote = std::max(0, std::min(127, midiNote));
    arpeggiator_.handleNoteOff(midiNote, this, &AbakosSynthEngine::arpNoteOffBridge);
}

void AbakosSynthEngine::allNotesOff() noexcept
{
    arpeggiator_.reset();
    for (int i = 0; i < kMaxVoices; ++i)
    {
        voices_[static_cast<size_t>(i)].keyDown = false;
        voices_[static_cast<size_t>(i)].ads1.noteOff();
        voices_[static_cast<size_t>(i)].ads2.noteOff();
        voices_[static_cast<size_t>(i)].adsr1.noteOff();
        voices_[static_cast<size_t>(i)].adsr2.noteOff();
    }
    for (auto& m : activeNotesMask_)
        m.store(0u, std::memory_order_relaxed);
}

void AbakosSynthEngine::setPitchBend(float normalizedBend) noexcept
{
    pitchBend_ = clampf(normalizedBend, -1.0f, 1.0f);
}

void AbakosSynthEngine::setModWheel(float normalizedMod) noexcept
{
    modWheel_ = clampf(normalizedMod, 0.0f, 1.0f);
}

void AbakosSynthEngine::setHostBpm(double bpm) noexcept
{
    arpeggiator_.setHostBpm(bpm);
}

bool AbakosSynthEngine::isNoteActive(int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return false;
    int word = midiNote >> 5;
    uint32_t bit = 1u << (midiNote & 31);
    return (activeNotesMask_[static_cast<size_t>(word)].load(std::memory_order_relaxed) & bit) != 0u;
}

void AbakosSynthEngine::internalNoteOn(int midiNote, int velocity) noexcept
{
    const bool monoMode = (params_[kMode] >= 0.5f);
    const int maxPoly   = monoMode ? 1 : std::max(1, std::min(kMaxVoices, config_.polyphony));
    const float targetCents = tuningScale_.noteCents[static_cast<size_t>(midiNote)];
    const bool hasPorta = (params_[kPortamento] > 0.5f);

    int chosenIdx = -1;

    if (monoMode)
    {
        chosenIdx = 0;
        // Silence any higher polyphonic voices if user switched to Mono while playing
        for (int i = 1; i < kMaxVoices; ++i)
            voices_[static_cast<size_t>(i)].active = false;
    }
    else
    {
        // 1. Re-use existing voice playing the same note
        for (int i = 0; i < maxPoly; ++i)
        {
            if (voices_[static_cast<size_t>(i)].active && voices_[static_cast<size_t>(i)].midiNote == midiNote)
            {
                chosenIdx = i;
                break;
            }
        }
        // 2. Find an inactive voice
        if (chosenIdx < 0)
        {
            for (int i = 0; i < maxPoly; ++i)
            {
                if (!voices_[static_cast<size_t>(i)].active)
                {
                    chosenIdx = i;
                    break;
                }
            }
        }
        // 3. Steal oldest released voice, or oldest voice overall (voice_stealing_pool_cpp 0x10025fb4)
        if (chosenIdx < 0)
        {
            uint64_t oldestReleasedAge = UINT64_MAX;
            for (int i = 0; i < maxPoly; ++i)
            {
                if (!voices_[static_cast<size_t>(i)].keyDown && voices_[static_cast<size_t>(i)].age < oldestReleasedAge)
                {
                    oldestReleasedAge = voices_[static_cast<size_t>(i)].age;
                    chosenIdx = i;
                }
            }
        }
        if (chosenIdx < 0)
        {
            uint64_t oldestAge = UINT64_MAX;
            for (int i = 0; i < maxPoly; ++i)
            {
                if (voices_[static_cast<size_t>(i)].age < oldestAge)
                {
                    oldestAge = voices_[static_cast<size_t>(i)].age;
                    chosenIdx = i;
                }
            }
        }
    }

    if (chosenIdx < 0)
        chosenIdx = 0;

    auto& v = voices_[static_cast<size_t>(chosenIdx)];
    const bool wasActive = v.active;

    v.active   = true;
    v.keyDown  = true;
    v.midiNote = midiNote;
    v.velocity = clampf(static_cast<float>(velocity) / 127.0f, 0.05f, 1.0f);
    v.age      = ++voiceCounter_;

    v.targetPitchCents = targetCents;
    if (hasPorta && hasLastMonoPitch_)
        v.currentPitchCents = lastMonoPitchCents_;
    else
        v.currentPitchCents = targetCents;

    lastMonoPitchCents_ = targetCents;
    hasLastMonoPitch_    = true;

    if (!wasActive)
    {
        v.svfLow  = 0.0f;
        v.svfBand = 0.0f;
    }

    v.ads1.noteOn();
    v.ads2.noteOn();
    v.adsr1.noteOn();
    v.adsr2.noteOn();

    int word = midiNote >> 5;
    uint32_t bit = 1u << (midiNote & 31);
    activeNotesMask_[static_cast<size_t>(word)].fetch_or(bit, std::memory_order_relaxed);
}

void AbakosSynthEngine::internalNoteOff(int midiNote) noexcept
{
    const int maxPoly = std::max(1, std::min(kMaxVoices, config_.polyphony));
    for (int i = 0; i < maxPoly; ++i)
    {
        auto& v = voices_[static_cast<size_t>(i)];
        if (v.active && v.keyDown && v.midiNote == midiNote)
        {
            v.keyDown = false;
            v.ads1.noteOff();
            v.ads2.noteOff();
            v.adsr1.noteOff();
            v.adsr2.noteOff();
        }
    }

    int word = midiNote >> 5;
    uint32_t bit = ~(1u << (midiNote & 31));
    activeNotesMask_[static_cast<size_t>(word)].fetch_and(bit, std::memory_order_relaxed);
}

float AbakosSynthEngine::computeTimeRate(float param0to100, float minSec, float maxSec, float sr) const noexcept
{
    float norm = clampf(param0to100 * 0.01f, 0.0f, 1.0f);
    // Exponential feel matching hardware/Artvera envelopes
    float sec  = minSec * std::pow(maxSec / minSec, norm);
    return 1.0f / std::max(1.0f, sec * sr);
}

void AbakosSynthEngine::renderBlock(float* outLeft, float* outRight, int numSamples) noexcept
{
    if (numSamples <= 0 || outLeft == nullptr || outRight == nullptr)
        return;

    const auto& wt = getWavetables();
    const float sr = static_cast<float>(sampleRate_);
    const int oversample = std::max(1, std::min(4, config_.oversampling));
    const float osSr = sr * static_cast<float>(oversample);
    const int maxPoly = (params_[kMode] >= 0.5f) ? 1 : std::max(1, std::min(kMaxVoices, config_.polyphony));

    // Envelope rates at base sample rate
    const float ads1Att  = computeTimeRate(params_[kAds1Attack],  0.001f, 4.0f, sr);
    const float ads1Dec  = computeTimeRate(params_[kAds1Decay],   0.004f, 6.0f, sr);
    const float ads1Sus  = clampf(params_[kAds1Sustain] * 0.01f,  0.0f, 1.0f);
    const float ads1ModCents = (params_[kAds1Mod] / 50.0f) * 2400.0f; // +/- 24 semitones max

    const float ads2Att  = computeTimeRate(params_[kAds2Attack],  0.001f, 4.0f, sr);
    const float ads2Dec  = computeTimeRate(params_[kAds2Decay],   0.004f, 6.0f, sr);
    const float ads2Sus  = clampf(params_[kAds2Sustain] * 0.01f,  0.0f, 1.0f);
    const float ads2Mod  = (params_[kAds2Mod] / 50.0f);               // -1.0 .. +1.0

    const float adsr1Att = computeTimeRate(params_[kAdsr1Attack],  0.001f, 5.0f, sr);
    const float adsr1Dec = computeTimeRate(params_[kAdsr1Decay],   0.004f, 6.0f, sr);
    const float adsr1Sus = clampf(params_[kAdsr1Sustain] * 0.01f,  0.0f, 1.0f);
    const float adsr1Rel = computeTimeRate(params_[kAdsr1Release], 0.003f, 8.0f, sr);

    const float adsr2Att = computeTimeRate(params_[kAdsr2Attack],  0.001f, 5.0f, sr);
    const float adsr2Dec = computeTimeRate(params_[kAdsr2Decay],   0.004f, 6.0f, sr);
    const float adsr2Sus = clampf(params_[kAdsr2Sustain] * 0.01f,  0.0f, 1.0f);
    const float adsr2Rel = computeTimeRate(params_[kAdsr2Release], 0.003f, 8.0f, sr);

    // Oscillator & pitch parameters
    const float globalCents = params_[kTune] + ( std::round(params_[kOctave]) * 1200.0f );
    const float pbRangeCents = (params_[kPitchBendRange] >= 0.5f) ? 1200.0f : 200.0f;
    const float bendCents    = pitchBend_ * pbRangeCents;

    const float osc1OffsetCents = params_[kOsc1Semis] * 100.0f + params_[kOsc1Cents];
    const float osc2OffsetCents = params_[kOsc2Semis] * 100.0f + params_[kOsc2Cents];
    const int osc1Wave = static_cast<int>(std::lround(params_[kOsc1Wave]));
    const int osc2Wave = static_cast<int>(std::lround(params_[kOsc2Wave]));
    const float osc1Gain = clampf(params_[kOsc1Level] * 0.01f, 0.0f, 1.0f);
    const float osc2Gain = clampf(params_[kOsc2Level] * 0.01f, 0.0f, 1.0f);
    const float syncAmt  = clampf(params_[kOsc2Sync] * 0.01f, 0.0f, 1.0f);

    // Noise parameters
    const float noiseColor    = clampf(params_[kNoiseColor] / 50.0f, -1.0f, 1.0f); // -1 = dark LP, +1 = bright HP
    const float noisePitchMod = clampf(params_[kNoisePitchMod] * 0.01f, 0.0f, 1.0f);
    const float noiseGain     = clampf(params_[kNoiseLevel] * 0.01f, 0.0f, 1.0f);

    // Portamento rate
    const bool portaActive = (params_[kPortamento] > 0.5f);
    const float portaCoeff = portaActive ? computeTimeRate(params_[kPortamento], 0.005f, 1.5f, sr) : 1.0f;

    // LFO parameters
    const float lfoFreq = 0.08f * std::pow(250.0f, clampf(params_[kLfoRate] * 0.01f, 0.0f, 1.0f));
    const float lfoInc  = lfoFreq / sr;
    const float lfoPitchDepthCents = (clampf(params_[kLfoPitch] * 0.01f, 0.0f, 1.0f) + modWheel_ * 0.5f) * 600.0f;
    const float lfoFilterDepth     = clampf(params_[kLfoFilter] * 0.01f, 0.0f, 1.0f) + modWheel_ * 0.4f;
    const float lfoPanDepth        = clampf(params_[kLfoPan] * 0.01f, 0.0f, 1.0f);

    // Filter mode & keytrack
    const bool isHighPass = (params_[kFilterHpfLpf] >= 0.5f);
    const float keyTrack  = clampf(params_[kFilterPitchMod] * 0.01f, 0.0f, 1.0f);

    // Delay parameters
    const float delayMix = clampf(params_[kDelayMix] * 0.01f, 0.0f, 1.0f);
    const float delayFb  = clampf(params_[kDelayFeedback] * 0.0092f, 0.0f, 0.93f);
    const int bufSize    = static_cast<int>(delayBufL_.size());
    const int delaySamplesL = std::max(1, std::min(bufSize - 4, static_cast<int>((0.015f + 0.95f * (params_[kDelay1] * 0.01f)) * sr)));
    const int delaySamplesR = std::max(1, std::min(bufSize - 4, static_cast<int>((0.015f + 0.95f * (params_[kDelay2] * 0.01f)) * sr)));

    // DC Blocker coefficient from config_.dcBlockingFreq (default 12 Hz)
    const float dcR = clampf(1.0f - (kTwoPi * static_cast<float>(config_.dcBlockingFreq) / sr), 0.9f, 0.9995f);
    const float headroomScale = 1.0f / std::max(1.0f, std::sqrt(static_cast<float>(config_.headroom)));

    const float targetCutoff    = params_[kFilterCutoff];
    const float targetResonance = params_[kFilterResonance];
    const float targetMasterVol = clampf(params_[kMasterLevel] * 0.01f, 0.0f, 1.0f);

    for (int s = 0; s < numSamples; ++s)
    {
        // Step Arpeggiator by 1 sample for sample-accurate note triggers
        arpeggiator_.processSamples(1, this, &AbakosSynthEngine::arpNoteOnBridge, &AbakosSynthEngine::arpNoteOffBridge);

        // Smooth master controls
        smoothCutoff_    += 0.005f * (targetCutoff - smoothCutoff_);
        smoothResonance_ += 0.005f * (targetResonance - smoothResonance_);
        smoothMasterVol_ += 0.005f * (targetMasterVol - smoothMasterVol_);

        // LFO sine wave
        lfoPhase_ += lfoInc;
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;
        const float lfoVal = std::sin(kTwoPi * lfoPhase_);

        float mixL = 0.0f;
        float mixR = 0.0f;

        // Chamberlin SVF damping factor q from resonance (0..100 -> q in [2.0 .. 0.06])
        const float resNorm = clampf(smoothResonance_ * 0.01f, 0.0f, 1.0f);
        const float svfQ    = 2.0f * std::pow(1.0f - resNorm * 0.96f, 1.4f);

        for (int vIdx = 0; vIdx < maxPoly; ++vIdx)
        {
            auto& v = voices_[static_cast<size_t>(vIdx)];
            if (!v.active)
                continue;

            const float envAds1  = v.ads1.process(ads1Att, ads1Dec, ads1Sus);
            const float envAds2  = v.ads2.process(ads2Att, ads2Dec, ads2Sus);
            const float envAdsr1 = v.adsr1.process(adsr1Att, adsr1Dec, adsr1Sus, adsr1Rel);
            const float envAdsr2 = v.adsr2.process(adsr2Att, adsr2Dec, adsr2Sus, adsr2Rel);

            if (!v.adsr1.isActive() && !v.adsr2.isActive())
            {
                v.active = false;
                continue;
            }

            // Portamento pitch glide
            v.currentPitchCents += portaCoeff * (v.targetPitchCents - v.currentPitchCents);

            const float voicePitchCents = v.currentPitchCents + globalCents + bendCents +
                                          envAds1 * ads1ModCents + lfoVal * lfoPitchDepthCents;

            // Convert cents to Hz (MIDI 69 = 6900 cents = 440 Hz)
            const float osc1Cents = voicePitchCents + osc1OffsetCents;
            const float osc2Cents = voicePitchCents + osc2OffsetCents;
            const float osc1Freq  = clampf(440.0f * std::exp2((osc1Cents - 6900.0f) / 1200.0f), 8.0f, osSr * 0.47f);
            const float osc2Freq  = clampf(440.0f * std::exp2((osc2Cents - 6900.0f) / 1200.0f), 8.0f, osSr * 0.47f);

            const float baseInc1 = osc1Freq / osSr;
            const float baseInc2 = osc2Freq / osSr;

            // Compute voice filter cutoff frequency (Hz) with ADS2, LFO, and keyboard tracking
            // Uses the 2-pass oversampled Chamberlin SVF formulation from lunar_integrated_components.cpp (MIT License)
            float cutoffPitchCents = 2400.0f + (smoothCutoff_ * 0.01f) * 8400.0f
                                   + envAds2 * ads2Mod * 4800.0f
                                   + lfoVal * lfoFilterDepth * 2400.0f
                                   + (v.currentPitchCents - 6000.0f) * keyTrack;
            float cutoffHz = clampf(440.0f * std::exp2((cutoffPitchCents - 6900.0f) / 1200.0f), 20.0f, osSr * 0.45f);
            float svfF = clampf(2.0f * std::sin(kPi * 0.5f * cutoffHz / osSr), 0.0005f, 0.996f);

            float voiceOut = 0.0f;

            // Oversampled inner loop (lunar_integrated_abakos / integrated_filter)
            for (int os = 0; os < oversample; ++os)
            {
                // 1. 24-bit LCG Noise Generator (noise24b = (noise24b * 0x5599d1 + 1) & 0xffffff)
                uint32_t rawLcg = stepLcgNoise(v.noiseState);
                int32_t signedLcg = (static_cast<int32_t>(rawLcg << 8)) >> 8;
                float whiteNoise = static_cast<float>(signedLcg) * (1.0f / 8388608.0f);

                // Noise Color Tilt Filter
                v.noiseFilterLp += 0.18f * (whiteNoise - v.noiseFilterLp);
                float coloredNoise = whiteNoise;
                if (noiseColor < 0.0f)
                    coloredNoise = whiteNoise * (1.0f + noiseColor) - v.noiseFilterLp * noiseColor;
                else if (noiseColor > 0.0f)
                    coloredNoise = whiteNoise - v.noiseFilterLp * noiseColor * 0.85f;

                // 2. Noise Pitch FM Modulation
                const float fmScale = 1.0f + coloredNoise * noisePitchMod * 0.25f;
                const float inc1 = clampf(baseInc1 * fmScale, 0.0f, 0.48f);
                const float inc2 = clampf(baseInc2 * fmScale, 0.0f, 0.48f);

                // 3. Advance OSC 1 (Master) & OSC 2 (Slave with Sync & Ring Mod from lunar_integrated_abakos)
                v.osc1Phase += inc1;
                bool masterWrapped = false;
                if (v.osc1Phase >= 1.0f)
                {
                    v.osc1Phase -= std::floor(v.osc1Phase);
                    masterWrapped = true;
                }

                v.osc2Phase += inc2;
                if (v.osc2Phase >= 1.0f)
                    v.osc2Phase -= std::floor(v.osc2Phase);

                if (masterWrapped && syncAmt > 0.001f)
                {
                    // Pull slave phase back toward master reset phase proportional to syncAmt
                    v.osc2Phase = v.osc2Phase * (1.0f - syncAmt) + v.osc1Phase * syncAmt * 0.25f;
                }

                const float rawWave1 = wt.sample(osc1Wave, v.osc1Phase, inc1);
                const float rawWave2 = wt.sample(osc2Wave, v.osc2Phase, inc2);
                const float sig1 = rawWave1 * osc1Gain * envAdsr1;
                const float sig2 = rawWave2 * osc2Gain * envAdsr2;
                const float ringMod = (rawWave1 * rawWave2) * syncAmt * 0.35f * std::min(osc1Gain, osc2Gain) * envAdsr2;
                const float sigNoise = coloredNoise * noiseGain * envAdsr2;

                const float rawMix = (sig1 + sig2 + ringMod + sigNoise) * v.velocity * 0.5f;

                // 4. Two-Pass Chamberlin 2-Pole State-Variable Filter (integrated_filter::move in lunar_integrated_components.cpp)
                v.svfLow += v.svfBand * svfF;
                float high = rawMix - v.svfLow - v.svfBand * svfQ;
                v.svfBand += high * svfF;

                v.svfLow += v.svfBand * svfF;
                high = rawMix - v.svfLow - v.svfBand * svfQ;
                v.svfBand += high * svfF;

                v.svfBand = clampf(v.svfBand, -1.5f, 1.5f);
                if (std::abs(v.svfBand) > 0.95f)
                    v.svfBand -= 0.15f * v.svfBand * v.svfBand * v.svfBand;
                v.svfLow = clampf(v.svfLow, -1.5f, 1.5f);

                voiceOut += (isHighPass ? high : v.svfLow);
            }

            voiceOut /= static_cast<float>(oversample);

            // LFO stereo pan modulation
            const float pan = clampf(lfoVal * lfoPanDepth, -1.0f, 1.0f);
            const float gainL = std::sqrt(0.5f * (1.0f - pan));
            const float gainR = std::sqrt(0.5f * (1.0f + pan));

            mixL += voiceOut * gainL;
            mixR += voiceOut * gainR;
        }

        mixL *= headroomScale;
        mixR *= headroomScale;

        // Stereo Delay (preset_dsp_algo_bank 0x1001aa7c)
        if (!delayBufL_.empty())
        {
            int readPosL = delayWritePos_ - delaySamplesL;
            if (readPosL < 0) readPosL += bufSize;
            int readPosR = delayWritePos_ - delaySamplesR;
            if (readPosR < 0) readPosR += bufSize;

            const float delayedL = delayBufL_[static_cast<size_t>(readPosL)];
            const float delayedR = delayBufR_[static_cast<size_t>(readPosR)];

            // Warm feedback lowpass damping
            delayLpL_ += 0.45f * (delayedR - delayLpL_);
            delayLpR_ += 0.45f * (delayedL - delayLpR_);

            delayBufL_[static_cast<size_t>(delayWritePos_)] = clampf(mixL + delayLpL_ * delayFb, -2.0f, 2.0f);
            delayBufR_[static_cast<size_t>(delayWritePos_)] = clampf(mixR + delayLpR_ * delayFb, -2.0f, 2.0f);

            if (++delayWritePos_ >= bufSize)
                delayWritePos_ = 0;

            mixL = mixL * (1.0f - delayMix * 0.5f) + delayedL * delayMix;
            mixR = mixR * (1.0f - delayMix * 0.5f) + delayedR * delayMix;
        }

        // 1-Pole High-Pass DC Blocker (dc_blocking_freq from ConfigSettings)
        const float dcOutL = mixL - dcInPrevL_ + dcR * dcOutPrevL_;
        dcInPrevL_  = mixL;
        dcOutPrevL_ = dcOutL;

        const float dcOutR = mixR - dcInPrevR_ + dcR * dcOutPrevR_;
        dcInPrevR_  = mixR;
        dcOutPrevR_ = dcOutR;

        outLeft[s]  = clampf(dcOutL * smoothMasterVol_, -1.0f, 1.0f);
        outRight[s] = clampf(dcOutR * smoothMasterVol_, -1.0f, 1.0f);
    }
}

} // namespace abakos
