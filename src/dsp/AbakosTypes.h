#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace abakos
{

enum ParamIndex : int
{
    kBankA          = 0,
    kBankB          = 1,
    kBankC          = 2,
    kBankD          = 3,
    kBankExternal   = 4,
    kPresetPrev     = 5,
    kPresetNext     = 6,
    kTune           = 7,
    kPitchBendRange = 8,
    kMode           = 9,
    kOctave         = 10,
    kArpPattern     = 11,
    kArpOnOff       = 12,
    kArpHold        = 13,
    kOsc1Semis      = 14,
    kOsc1Wave       = 15,
    kOsc1Cents      = 16,
    kOsc1Level      = 17,
    kOsc2Semis      = 18,
    kOsc2Wave       = 19,
    kOsc2Cents      = 20,
    kOsc2Sync       = 21,
    kOsc2Level      = 22,
    kNoiseColor     = 23,
    kNoisePitchMod  = 24,
    kNoiseLevel     = 25,
    kAds1Attack     = 26,
    kAds1Decay      = 27,
    kAds1Sustain    = 28,
    kAds1Mod        = 29,
    kAds2Attack     = 30,
    kAds2Decay      = 31,
    kAds2Sustain    = 32,
    kAds2Mod        = 33,
    kFilterCutoff   = 34,
    kFilterResonance= 35,
    kFilterHpfLpf   = 36,
    kPortamento     = 37,
    kFilterPitchMod = 38,
    kLfoPan         = 39,
    kLfoPitch       = 40,
    kLfoFilter      = 41,
    kLfoRate        = 42,
    kAdsr1Attack    = 43,
    kAdsr1Decay     = 44,
    kAdsr1Sustain   = 45,
    kAdsr1Release   = 46,
    kAdsr2Attack    = 47,
    kAdsr2Decay     = 48,
    // Extended internal parameters (stored in APVTS alongside VST parameters)
    kAdsr2Sustain   = 49,
    kAdsr2Release   = 50,
    kDelay1         = 51,
    kDelay2         = 52,
    kDelayFeedback  = 53,
    kDelayMix       = 54,
    kMasterLevel    = 55,
    kNumParams      = 56
};

struct ParamMeta
{
    int index;
    const char* id;
    const char* name;
    int minVal;
    int maxVal;
    int defaultVal;
    bool isBooleanOrStepped;
};

// Complete metadata table for all Abakos PRO parameters:
// Indices 0..48 match the exact 49 VST parameters from AbakosPRO.dll (0x100083dc..0x10008c99),
// and 49..55 expose the remaining GUI controls (ADSR2 S/R, Delay 1/2/Feedback/Mix, Master Level)
// that in the original DLL shared adapter slots or direct GUI bindings.
inline const std::array<ParamMeta, kNumParams>& getParamMetaTable()
{
    static const std::array<ParamMeta, kNumParams> kTable {{
        { kBankA,           "bank_a",           "BANK A",           0, 1,   1,   true  },
        { kBankB,           "bank_b",           "BANK B",           0, 1,   0,   true  },
        { kBankC,           "bank_c",           "BANK C",           0, 1,   0,   true  },
        { kBankD,           "bank_d",           "BANK D",           0, 1,   0,   true  },
        { kBankExternal,    "bank_external",    "BANK EXTERNAL",    0, 1,   0,   true  },
        { kPresetPrev,      "preset_prev",      "PRESET PREVIOUS",  0, 1,   0,   true  },
        { kPresetNext,      "preset_next",      "PRESET NEXT",      0, 1,   0,   true  },
        { kTune,            "tune",             "TUNE",            -50, 50, 0,   false },
        { kPitchBendRange,  "pitch_bend_range", "PITCH BEND",       0, 1,   0,   true  }, // 0 = 2 semis, 1 = 12 semis
        { kMode,            "mode",             "MODE",             0, 1,   0,   true  }, // 0 = Poly, 1 = Mono
        { kOctave,          "octave",           "OCTAVE",          -2, 2,   0,   true  },
        { kArpPattern,      "arp_pattern",      "ARP PATTERN",      0, 23,  0,   true  },
        { kArpOnOff,        "arp_on_off",       "ARP ON/OFF",       0, 1,   0,   true  },
        { kArpHold,         "arp_hold",         "ARP HOLD",         0, 1,   0,   true  },
        { kOsc1Semis,       "osc1_semis",       "OSC1 SEMIS",      -24, 24, 0,   false },
        { kOsc1Wave,        "osc1_wave",        "OSC1 WAVE",        0, 5,   0,   true  },
        { kOsc1Cents,       "osc1_cents",       "OSC1 CENTS",      -50, 50, 0,   false },
        { kOsc1Level,       "osc1_level",       "OSC1 LEVEL",       0, 100, 100, false },
        { kOsc2Semis,       "osc2_semis",       "OSC2 SEMIS",      -24, 24, 0,   false },
        { kOsc2Wave,        "osc2_wave",        "OSC2 WAVE",        0, 5,   1,   true  },
        { kOsc2Cents,       "osc2_cents",       "OSC2 CENTS",      -50, 50, 7,   false },
        { kOsc2Sync,        "osc2_sync",        "OSC2 SYNC",        0, 100, 0,   false },
        { kOsc2Level,       "osc2_level",       "OSC2 LEVEL",       0, 100, 80,  false },
        { kNoiseColor,      "noise_color",      "NOISE COLOR",     -50, 50, 0,   false },
        { kNoisePitchMod,   "noise_pitch_mod",  "NOISE PITCH MOD",  0, 100, 0,   false },
        { kNoiseLevel,      "noise_level",      "NOISE LEVEL",      0, 100, 0,   false },
        { kAds1Attack,      "ads1_attack",      "ADS1 ATTACK",      0, 100, 0,   false },
        { kAds1Decay,       "ads1_decay",       "ADS1 DECAY",       0, 100, 35,  false },
        { kAds1Sustain,     "ads1_sustain",     "ADS1 SUSTAIN",     0, 100, 0,   false },
        { kAds1Mod,         "ads1_mod",         "ADS1 MOD",        -50, 50, 0,   false },
        { kAds2Attack,      "ads2_attack",      "ADS2 ATTACK",      0, 100, 5,   false },
        { kAds2Decay,       "ads2_decay",       "ADS2 DECAY",       0, 100, 45,  false },
        { kAds2Sustain,     "ads2_sustain",     "ADS2 SUSTAIN",     0, 100, 25,  false },
        { kAds2Mod,         "ads2_mod",         "ADS2 MOD",        -50, 50, 25,  false },
        { kFilterCutoff,    "filter_cutoff",    "FILTER CUTOFF",    0, 100, 78,  false },
        { kFilterResonance, "filter_resonance", "FILTER RESONANCE", 0, 100, 20,  false },
        { kFilterHpfLpf,    "filter_hpf_lpf",   "FILTER HPF/LPF",   0, 1,   0,   true  }, // 0 = LPF, 1 = HPF
        { kPortamento,      "portamento",       "PORTAMENTO",       0, 100, 0,   false },
        { kFilterPitchMod,  "filter_pitch_mod", "FILTER PITCH MOD", 0, 100, 50,  false },
        { kLfoPan,          "lfo_pan",          "LFO PAN",          0, 100, 0,   false },
        { kLfoPitch,        "lfo_pitch",        "LFO PITCH",        0, 100, 0,   false },
        { kLfoFilter,       "lfo_filter",       "LFO FILTER",       0, 100, 0,   false },
        { kLfoRate,         "lfo_rate",         "LFO RATE",         0, 100, 35,  false },
        { kAdsr1Attack,     "adsr1_attack",     "ADSR1 ATTACK",     0, 100, 2,   false },
        { kAdsr1Decay,      "adsr1_decay",      "ADSR1 DECAY",      0, 100, 40,  false },
        { kAdsr1Sustain,    "adsr1_sustain",    "ADSR1 SUSTAIN",    0, 100, 85,  false },
        { kAdsr1Release,    "adsr1_release",    "ADSR1 RELEASE",    0, 100, 25,  false },
        { kAdsr2Attack,     "adsr2_attack",     "ADSR2 ATTACK",     0, 100, 2,   false },
        { kAdsr2Decay,      "adsr2_decay",      "ADSR2 DECAY",      0, 100, 40,  false },
        { kAdsr2Sustain,    "adsr2_sustain",    "ADSR2 SUSTAIN",    0, 100, 85,  false },
        { kAdsr2Release,    "adsr2_release",    "ADSR2 RELEASE",    0, 100, 25,  false },
        { kDelay1,          "delay_1",          "DELAY 1",          0, 100, 35,  false },
        { kDelay2,          "delay_2",          "DELAY 2",          0, 100, 50,  false },
        { kDelayFeedback,   "delay_feedback",   "FEEDBACK",         0, 100, 30,  false },
        { kDelayMix,        "delay_mix",        "DELAY MIX",        0, 100, 18,  false },
        { kMasterLevel,     "master_level",     "LEVEL",            0, 100, 85,  false }
    }};
    return kTable;
}

// Engine configuration matching config.exe and config.txt (0x10025a34 / 0x004016b0)
struct ConfigSettings
{
    int polyphony      = 8;  // 1..64 voices (default 8)
    int dcBlockingFreq = 12; // Hz (default 12)
    int oversampling   = 1;  // 1..4x (default 1)
    int headroom       = 2;  // 1..8 headroom divisor (default 2)
    int multitimbral   = 0;  // 0 = Omni, 1 = Multi-channel (default 0)
};

// Microtonal scale table matching .scl.tun files (0x100083dc load_tuning_table)
struct TuningScale
{
    std::string name = "12-TET Standard";
    std::string filePath;
    // Pitch in cents for each MIDI note 0..127 (default: note * 100.0f)
    std::array<float, 128> noteCents {};

    TuningScale()
    {
        resetTo12TET();
    }

    void resetTo12TET()
    {
        name = "12-TET Standard";
        filePath.clear();
        for (int i = 0; i < 128; ++i)
            noteCents[static_cast<size_t>(i)] = static_cast<float>(i * 100);
    }
};

// Complete parsed representation of an Abakos PRO preset (.txt S-expression file)
struct PresetData
{
    std::string category = "SYNTH";
    std::string name     = "INIT PRO";
    int bankIndex        = 0; // 0=A, 1=B, 2=C, 3=D, 4=Ext
    int programIndex     = 0; // 0..127

    // Parameter values in GUI domain (matching ParamMeta min..max)
    std::array<int, kNumParams> params {};

    // Raw internal HERCs fields preserved for lossless round-trip serialization
    int subAlgo = 0;
    int pan     = 0;

    PresetData()
    {
        const auto& meta = getParamMetaTable();
        for (size_t i = 0; i < kNumParams; ++i)
            params[i] = meta[i].defaultVal;
    }
};

} // namespace abakos
