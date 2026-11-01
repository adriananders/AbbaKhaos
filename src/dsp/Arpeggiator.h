#pragma once

#include <array>
#include <cstdint>
#include <functional>

namespace abakos
{

struct ArpStep
{
    int8_t noteOffset;   // Index offset in held notes or semitone transposition
    uint8_t gatePercent; // Gate length percentage (10..95)
    uint8_t velocityScale; // Velocity percentage (50..120)
    int8_t octaveShift;  // Octave shift (-2..+2)
};

struct ArpPatternDef
{
    uint8_t length;      // Number of steps (4..16)
    uint8_t ticksPerStep;// In 24-PPQN ticks (e.g., 6 = 1/16th note, 12 = 1/8th, 4 = 1/16T)
    uint8_t mode;        // 0=Up, 1=Down, 2=UpDown, 3=Order/Chordal, 4=Seq
    std::array<ArpStep, 16> steps;
};

class Arpeggiator
{
public:
    using NoteCallback = std::function<void(int midiNote, int velocity, bool isNoteOn)>;

    Arpeggiator();

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept;
    void setHold(bool hold) noexcept;
    void setPatternIndex(int patternIdx) noexcept;
    void setHostBpm(double bpm) noexcept;

    bool isEnabled() const noexcept { return enabled_; }
    bool isHold() const noexcept { return hold_; }
    int getPatternIndex() const noexcept { return patternIndex_; }

    // Real-time safe MIDI input handlers
    void handleNoteOn(int note, int velocity,
                      void* callbackCtx,
                      void (*triggerNoteOn)(void* ctx, int note, int vel),
                      void (*triggerNoteOff)(void* ctx, int note)) noexcept;

    void handleNoteOff(int note,
                       void* callbackCtx,
                       void (*triggerNoteOff)(void* ctx, int note)) noexcept;

    // Advance arpeggiator by numSamples on the audio thread (zero allocations)
    void processSamples(int numSamples,
                        void* callbackCtx,
                        void (*triggerNoteOn)(void* ctx, int note, int vel),
                        void (*triggerNoteOff)(void* ctx, int note)) noexcept;

    static const std::array<ArpPatternDef, 24>& getFactoryPatterns() noexcept;

private:
    struct HeldNote
    {
        int note     = 60;
        int velocity = 100;
        bool physicallyDown = false;
    };

    void stopCurrentStepNote(void* callbackCtx, void (*triggerNoteOff)(void* ctx, int note)) noexcept;

    double sampleRate_ = 44100.0;
    double bpm_        = 120.0;
    bool enabled_      = false;
    bool hold_         = false;
    int patternIndex_  = 0;

    std::array<HeldNote, 32> heldNotes_ {};
    int numHeldNotes_  = 0;

    double sampleCounter_ = 0.0;
    int currentStep_      = 0;
    int walkIndex_        = 0;
    int walkDir_          = 1;
    int activeNote_       = -1;
    int gateRemainingSamples_ = 0;
};

} // namespace abakos
