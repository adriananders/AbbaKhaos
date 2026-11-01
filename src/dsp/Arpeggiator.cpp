#include "Arpeggiator.h"
#include <algorithm>
#include <cmath>

namespace abakos
{

// 24 Factory Arpeggiator Patterns reconstructed from preset_pattern_bank (0x1001f0c0)
const std::array<ArpPatternDef, 24>& Arpeggiator::getFactoryPatterns() noexcept
{
    static const std::array<ArpPatternDef, 24> kPatterns {{
        // 0: 1/16 Up 1-Octave
        { 8, 6, 0, {{ {0,80,100,0}, {1,80,90,0}, {2,80,95,0}, {3,80,90,0}, {0,80,100,0}, {1,80,90,0}, {2,80,95,0}, {3,80,90,0} }} },
        // 1: 1/16 Down 1-Octave
        { 8, 6, 1, {{ {0,80,100,0}, {1,80,90,0}, {2,80,95,0}, {3,80,90,0}, {0,80,100,0}, {1,80,90,0}, {2,80,95,0}, {3,80,90,0} }} },
        // 2: 1/16 Up/Down 1-Octave
        { 8, 6, 2, {{ {0,80,100,0}, {1,80,90,0}, {2,80,95,0}, {3,80,90,0}, {2,80,100,0}, {1,80,90,0}, {0,80,95,0}, {1,80,90,0} }} },
        // 3: 1/16 Up 2-Octave
        { 8, 6, 0, {{ {0,75,100,0}, {1,75,90,0}, {2,75,95,0}, {3,75,90,0}, {0,75,100,1}, {1,75,90,1}, {2,75,95,1}, {3,75,90,1} }} },
        // 4: 1/16 Down 2-Octave
        { 8, 6, 1, {{ {0,75,100,1}, {1,75,90,1}, {2,75,95,1}, {3,75,90,1}, {0,75,100,0}, {1,75,90,0}, {2,75,95,0}, {3,75,90,0} }} },
        // 5: 1/16 Up/Down 2-Octave
        { 8, 6, 2, {{ {0,75,100,0}, {1,75,90,0}, {2,75,95,1}, {3,75,90,1}, {2,75,100,1}, {1,75,90,0}, {0,75,95,0}, {1,75,90,0} }} },
        // 6: 1/8 Up Classic
        { 8, 12, 0, {{ {0,85,100,0}, {1,85,90,0}, {2,85,95,0}, {3,85,90,0}, {0,85,100,1}, {1,85,90,0}, {2,85,95,0}, {3,85,90,0} }} },
        // 7: 1/8 Down Classic
        { 8, 12, 1, {{ {0,85,100,0}, {1,85,90,0}, {2,85,95,0}, {3,85,90,0}, {0,85,100,1}, {1,85,90,0}, {2,85,95,0}, {3,85,90,0} }} },
        // 8: 1/16 Octave Bounce
        { 8, 6, 0, {{ {0,65,100,0}, {0,65,95,1}, {1,65,90,0}, {1,65,95,1}, {2,65,100,0}, {2,65,95,1}, {3,65,90,0}, {3,65,95,1} }} },
        // 9: 1/16 Bass Octaver
        { 8, 6, 4, {{ {0,70,105,-1}, {0,50,85,0}, {0,70,95,-1}, {0,65,90,0}, {0,70,105,-1}, {0,50,85,0}, {7,70,95,-1}, {0,65,90,0} }} },
        // 10: 1/16 Trance Gate Pulse
        { 8, 6, 0, {{ {0,90,100,0}, {0,40,75,0}, {1,90,100,0}, {1,40,80,1}, {2,90,100,0}, {2,40,75,0}, {3,90,105,1}, {0,50,85,0} }} },
        // 11: 1/16T Triplet Up
        { 12, 4, 0, {{ {0,75,100,0}, {1,75,90,0}, {2,75,90,0}, {0,75,100,1}, {1,75,90,1}, {2,75,90,1},
                       {0,75,100,0}, {1,75,90,0}, {2,75,90,0}, {0,75,100,1}, {1,75,90,1}, {2,75,90,1} }} },
        // 12: 1/16T Triplet Down
        { 12, 4, 1, {{ {0,75,100,1}, {1,75,90,1}, {2,75,90,1}, {0,75,100,0}, {1,75,90,0}, {2,75,90,0},
                       {0,75,100,1}, {1,75,90,1}, {2,75,90,1}, {0,75,100,0}, {1,75,90,0}, {2,75,90,0} }} },
        // 13: Fifths & Octaves Seq
        { 8, 6, 4, {{ {0,80,100,0}, {7,70,90,0}, {12,80,95,0}, {7,70,85,0}, {0,80,100,0}, {7,70,90,0}, {12,80,100,1}, {7,70,85,0} }} },
        // 14: Acid Line 1
        { 8, 6, 4, {{ {0,95,105,0}, {0,45,80,1}, {3,95,100,0}, {0,45,75,0}, {7,95,105,0}, {10,50,90,0}, {12,95,105,0}, {0,45,80,-1} }} },
        // 15: Acid Line 2
        { 8, 6, 4, {{ {0,50,100,-1}, {12,95,105,0}, {0,50,80,0}, {7,90,95,0}, {5,50,85,0}, {3,95,100,0}, {0,50,80,-1}, {10,85,95,-1} }} },
        // 16: 3-Octave Cascade Up
        { 12, 6, 0, {{ {0,75,100,-1}, {1,75,90,-1}, {2,75,95,-1}, {0,75,100,0}, {1,75,90,0}, {2,75,95,0},
                       {0,75,100,1}, {1,75,90,1}, {2,75,95,1}, {0,75,100,0}, {1,75,90,0}, {2,75,95,0} }} },
        // 17: 3-Octave Cascade Down
        { 12, 6, 1, {{ {0,75,100,1}, {1,75,90,1}, {2,75,95,1}, {0,75,100,0}, {1,75,90,0}, {2,75,95,0},
                       {0,75,100,-1}, {1,75,90,-1}, {2,75,95,-1}, {0,75,100,0}, {1,75,90,0}, {2,75,95,0} }} },
        // 18: Syncopated Electro
        { 8, 6, 0, {{ {0,90,105,0}, {1,40,75,0}, {2,90,100,1}, {0,40,70,0}, {1,90,100,0}, {2,40,75,1}, {3,90,105,0}, {1,50,80,0} }} },
        // 19: Staccato Pluck
        { 8, 6, 2, {{ {0,35,105,0}, {1,35,90,0}, {2,35,95,0}, {3,35,90,1}, {2,35,100,1}, {1,35,90,0}, {0,35,95,0}, {1,35,90,-1} }} },
        // 20: Minor Arp Sequence
        { 8, 6, 4, {{ {0,80,100,0}, {3,80,90,0}, {7,80,95,0}, {10,80,90,0}, {12,80,105,0}, {10,80,90,0}, {7,80,95,0}, {3,80,90,0} }} },
        // 21: Major Arp Sequence
        { 8, 6, 4, {{ {0,80,100,0}, {4,80,90,0}, {7,80,95,0}, {11,80,90,0}, {12,80,105,0}, {11,80,90,0}, {7,80,95,0}, {4,80,90,0} }} },
        // 22: Sus4 Arp Sequence
        { 8, 6, 4, {{ {0,80,100,0}, {5,80,90,0}, {7,80,95,0}, {12,80,100,0}, {7,80,90,0}, {5,80,95,0}, {0,80,100,1}, {7,80,90,0} }} },
        // 23: Fast 1/32 Cyber Roll
        { 8, 3, 0, {{ {0,70,100,0}, {1,70,90,0}, {2,70,95,0}, {3,70,90,1}, {0,70,105,1}, {1,70,90,1}, {2,70,95,0}, {3,70,90,0} }} }
    }};
    return kPatterns;
}

Arpeggiator::Arpeggiator() = default;

void Arpeggiator::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    reset();
}

void Arpeggiator::reset() noexcept
{
    numHeldNotes_         = 0;
    sampleCounter_        = 0.0;
    currentStep_          = 0;
    walkIndex_            = 0;
    walkDir_              = 1;
    activeNote_           = -1;
    gateRemainingSamples_ = 0;
}

void Arpeggiator::setEnabled(bool enabled) noexcept
{
    enabled_ = enabled;
}

void Arpeggiator::setHold(bool hold) noexcept
{
    if (hold_ && !hold)
    {
        // Purge notes that are no longer physically held
        int dst = 0;
        for (int i = 0; i < numHeldNotes_; ++i)
        {
            if (heldNotes_[static_cast<size_t>(i)].physicallyDown)
                heldNotes_[static_cast<size_t>(dst++)] = heldNotes_[static_cast<size_t>(i)];
        }
        numHeldNotes_ = dst;
    }
    hold_ = hold;
}

void Arpeggiator::setPatternIndex(int patternIdx) noexcept
{
    patternIndex_ = std::max(0, std::min(23, patternIdx));
}

void Arpeggiator::setHostBpm(double bpm) noexcept
{
    if (bpm >= 20.0 && bpm <= 400.0)
        bpm_ = bpm;
}

void Arpeggiator::stopCurrentStepNote(void* callbackCtx, void (*triggerNoteOff)(void* ctx, int note)) noexcept
{
    if (activeNote_ >= 0)
    {
        if (triggerNoteOff != nullptr)
            triggerNoteOff(callbackCtx, activeNote_);
        activeNote_ = -1;
    }
    gateRemainingSamples_ = 0;
}

void Arpeggiator::handleNoteOn(int note, int velocity,
                               void* callbackCtx,
                               void (*triggerNoteOn)(void* ctx, int note, int vel),
                               void (*triggerNoteOff)(void* ctx, int note)) noexcept
{
    if (!enabled_)
    {
        if (triggerNoteOn)
            triggerNoteOn(callbackCtx, note, velocity);
        return;
    }

    // If hold is active and no keys are currently physically pressed, clear previous latched chord
    if (hold_)
    {
        bool anyPhysical = false;
        for (int i = 0; i < numHeldNotes_; ++i)
        {
            if (heldNotes_[static_cast<size_t>(i)].physicallyDown)
            {
                anyPhysical = true;
                break;
            }
        }
        if (!anyPhysical)
            numHeldNotes_ = 0;
    }

    // Check if note already exists
    for (int i = 0; i < numHeldNotes_; ++i)
    {
        if (heldNotes_[static_cast<size_t>(i)].note == note)
        {
            heldNotes_[static_cast<size_t>(i)].velocity = velocity;
            heldNotes_[static_cast<size_t>(i)].physicallyDown = true;
            return;
        }
    }

    if (numHeldNotes_ < static_cast<int>(heldNotes_.size()))
    {
        bool wasEmpty = (numHeldNotes_ == 0);
        heldNotes_[static_cast<size_t>(numHeldNotes_++)] = { note, velocity, true };

        // Keep sorted ascending by pitch (matching 0x1002b28c note pool)
        std::sort(heldNotes_.begin(), heldNotes_.begin() + numHeldNotes_,
                  [](const HeldNote& a, const HeldNote& b) { return a.note < b.note; });

        if (wasEmpty)
        {
            // Trigger first step immediately on key-down
            sampleCounter_ = 0.0;
            currentStep_   = 0;
            walkIndex_     = 0;
            walkDir_       = 1;
            stopCurrentStepNote(callbackCtx, triggerNoteOff);
        }
    }
}

void Arpeggiator::handleNoteOff(int note,
                                void* callbackCtx,
                                void (*triggerNoteOff)(void* ctx, int note)) noexcept
{
    if (!enabled_)
    {
        if (triggerNoteOff)
            triggerNoteOff(callbackCtx, note);
        return;
    }

    for (int i = 0; i < numHeldNotes_; ++i)
    {
        if (heldNotes_[static_cast<size_t>(i)].note == note)
        {
            heldNotes_[static_cast<size_t>(i)].physicallyDown = false;
            if (!hold_)
            {
                for (int j = i; j < numHeldNotes_ - 1; ++j)
                    heldNotes_[static_cast<size_t>(j)] = heldNotes_[static_cast<size_t>(j + 1)];
                --numHeldNotes_;
            }
            break;
        }
    }

    if (numHeldNotes_ == 0)
        stopCurrentStepNote(callbackCtx, triggerNoteOff);
}

void Arpeggiator::processSamples(int numSamples,
                                 void* callbackCtx,
                                 void (*triggerNoteOn)(void* ctx, int note, int vel),
                                 void (*triggerNoteOff)(void* ctx, int note)) noexcept
{
    if (!enabled_)
    {
        if (activeNote_ >= 0)
            stopCurrentStepNote(callbackCtx, triggerNoteOff);
        return;
    }

    if (numHeldNotes_ <= 0)
        return;

    const auto& pat = getFactoryPatterns()[static_cast<size_t>(patternIndex_)];
    // 24 PPQN: 1 quarter note = 24 ticks = (60 / bpm) seconds
    const double samplesPerTick = (sampleRate_ * 60.0) / (bpm_ * 24.0);
    const double samplesPerStep = std::max(64.0, samplesPerTick * static_cast<double>(pat.ticksPerStep));

    for (int s = 0; s < numSamples; ++s)
    {
        if (gateRemainingSamples_ > 0)
        {
            --gateRemainingSamples_;
            if (gateRemainingSamples_ == 0 && activeNote_ >= 0)
            {
                if (triggerNoteOff)
                    triggerNoteOff(callbackCtx, activeNote_);
                activeNote_ = -1;
            }
        }

        if (sampleCounter_ <= 0.0)
        {
            // Trigger current step
            stopCurrentStepNote(callbackCtx, triggerNoteOff);

            const int stepIdx = currentStep_ % std::max<int>(1, pat.length);
            const auto& step  = pat.steps[static_cast<size_t>(stepIdx)];

            int chosenIdx = 0;
            int semitoneTranspose = step.octaveShift * 12;

            if (pat.mode == 0) // Up
            {
                chosenIdx = walkIndex_ % numHeldNotes_;
                walkIndex_ = (walkIndex_ + 1) % numHeldNotes_;
            }
            else if (pat.mode == 1) // Down
            {
                chosenIdx = (numHeldNotes_ - 1) - (walkIndex_ % numHeldNotes_);
                walkIndex_ = (walkIndex_ + 1) % numHeldNotes_;
            }
            else if (pat.mode == 2) // Up/Down
            {
                if (numHeldNotes_ <= 1)
                {
                    chosenIdx = 0;
                }
                else
                {
                    chosenIdx = std::max(0, std::min(numHeldNotes_ - 1, walkIndex_));
                    walkIndex_ += walkDir_;
                    if (walkIndex_ >= numHeldNotes_ - 1)
                    {
                        walkIndex_ = numHeldNotes_ - 1;
                        walkDir_ = -1;
                    }
                    else if (walkIndex_ <= 0)
                    {
                        walkIndex_ = 0;
                        walkDir_ = 1;
                    }
                }
            }
            else if (pat.mode == 4) // Fixed interval sequence relative to lowest held note
            {
                chosenIdx = 0;
                semitoneTranspose += step.noteOffset;
            }

            const auto& base = heldNotes_[static_cast<size_t>(chosenIdx)];
            int outNote = std::max(0, std::min(127, base.note + semitoneTranspose));
            int outVel  = std::max(1, std::min(127, (base.velocity * static_cast<int>(step.velocityScale)) / 100));

            activeNote_ = outNote;
            gateRemainingSamples_ = std::max(16, static_cast<int>(samplesPerStep * (static_cast<double>(step.gatePercent) / 100.0)));

            if (triggerNoteOn)
                triggerNoteOn(callbackCtx, activeNote_, outVel);

            currentStep_ = (currentStep_ + 1) % std::max<int>(1, pat.length);
            sampleCounter_ += samplesPerStep;
        }

        sampleCounter_ -= 1.0;
    }
}

} // namespace abakos
