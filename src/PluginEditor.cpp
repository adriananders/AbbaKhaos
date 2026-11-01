#include "PluginEditor.h"
#include <cmath>
#include <cstdio>

namespace
{
// Modern Boutique Dark Palette
const juce::Colour kBgTop         { 0xff0d1117 };
const juce::Colour kBgBottom      { 0xff111823 };
const juce::Colour kCardBg        { 0xff151d2a };
const juce::Colour kCardHeader    { 0xff1b2536 };
const juce::Colour kCardBorder    { 0xff26354d };
const juce::Colour kTrackDark     { 0xff0b1018 };
const juce::Colour kTextBright    { 0xffe8f1ff };
const juce::Colour kTextMuted     { 0xff8a9fba };

// Section Accent Colors
const juce::Colour kAccentOsc     { 0xff00d4ff }; // Electric Cyan
const juce::Colour kAccentFilter  { 0xffffb020 }; // Warm Amber
const juce::Colour kAccentEnv     { 0xff10e898 }; // Emerald Mint
const juce::Colour kAccentMod     { 0xffb57bff }; // Violet
const juce::Colour kAccentFx      { 0xff38bdf8 }; // Sky Blue
} // namespace

// ==============================================================================
// ActionBadge: Sleek borderless/hairline vector button for header bar & banks
// ==============================================================================
class AbakosPROAudioProcessorEditor::ActionBadge : public juce::Component
{
public:
    ActionBadge(const juce::String& text, juce::Colour accent, std::function<void()> onClick)
        : text_(text), accent_(accent), onClick_(std::move(onClick))
    {
    }

    void setText(const juce::String& t)
    {
        if (text_ != t)
        {
            text_ = t;
            repaint();
        }
    }

    void setHighlighted(bool h)
    {
        if (highlighted_ != h)
        {
            highlighted_ = h;
            repaint();
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { hover_ = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hover_ = false; down_ = false; repaint(); }
    void mouseDown(const juce::MouseEvent&) override  { down_ = true; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        bool wasDown = down_;
        down_ = false;
        repaint();
        if (wasDown && getLocalBounds().contains(e.getPosition()) && onClick_)
            onClick_();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        juce::Colour bg = highlighted_ ? accent_.withAlpha(0.26f)
                        : (down_ ? kCardHeader.brighter(0.15f)
                        : (hover_ ? kCardHeader : kTrackDark));
        g.setColour(bg);
        g.fillRoundedRectangle(r, 4.5f);

        g.setColour(highlighted_ ? accent_ : (hover_ ? kTextMuted : kCardBorder));
        g.drawRoundedRectangle(r, 4.5f, highlighted_ ? 1.4f : 1.0f);

        g.setColour(highlighted_ ? kTextBright : (hover_ ? kTextBright : kTextMuted));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText(text_, getLocalBounds(), juce::Justification::centred, false);
    }

private:
    juce::String text_;
    juce::Colour accent_;
    std::function<void()> onClick_;
    bool highlighted_ = false;
    bool hover_ = false;
    bool down_  = false;
};

// ==============================================================================
// ModernKnob: Vector arc rotary control with bipolar/unipolar support & live readout
// ==============================================================================
class AbakosPROAudioProcessorEditor::ModernKnob : public juce::Component,
                                                  private juce::Slider::Listener
{
public:
    ModernKnob(juce::AudioProcessorValueTreeState& apvts,
               const juce::String& paramID,
               const juce::String& labelText,
               juce::Colour accent,
               bool bipolar = false,
               const juce::String& suffix = "")
        : label_(labelText), accent_(accent), bipolar_(bipolar), suffix_(suffix)
    {
        slider_.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider_.setPopupDisplayEnabled(false, false, nullptr);
        slider_.setAlpha(0.0f); // Hide default JUCE look-and-feel knob; render 100% custom vector graphics
        slider_.addListener(this);
        addAndMakeVisible(slider_);

        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider_);
        if (auto* param = apvts.getParameter(paramID))
            slider_.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    }

    void resized() override
    {
        slider_.setBounds(0, 12, getWidth(), getHeight() - 24);
    }

    void sliderValueChanged(juce::Slider*) override
    {
        repaint();
        if (auto* p = getParentComponent())
            p->repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Top label
        g.setColour(kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(label_, 0, 0, getWidth(), 12, juce::Justification::centred, false);

        const float minV = static_cast<float>(slider_.getMinimum());
        const float maxV = static_cast<float>(slider_.getMaximum());
        const float val  = static_cast<float>(slider_.getValue());
        const float norm = (maxV > minV) ? (val - minV) / (maxV - minV) : 0.0f;

        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        const float curAngle   = startAngle + norm * (endAngle - startAngle);

        auto dialArea = juce::Rectangle<float>(3.0f, 13.0f, static_cast<float>(getWidth() - 6), static_cast<float>(getHeight() - 27));
        float diameter = std::min(dialArea.getWidth(), dialArea.getHeight());
        auto circle = dialArea.withSizeKeepingCentre(diameter, diameter);
        float radius = diameter * 0.5f;
        float cx = circle.getCentreX();
        float cy = circle.getCentreY();

        // Background track arc
        juce::Path bgArc;
        bgArc.addCentredArc(cx, cy, radius - 2.5f, radius - 2.5f, 0.0f, startAngle, endAngle, true);
        g.setColour(kTrackDark);
        g.strokePath(bgArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active glowing value arc
        float originAngle = bipolar_ ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs(curAngle - originAngle) > 0.01f)
        {
            juce::Path valArc;
            valArc.addCentredArc(cx, cy, radius - 2.5f, radius - 2.5f, 0.0f, originAngle, curAngle, true);
            g.setColour(accent_);
            g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Inner metallic cap
        float bodyR = radius - 6.2f;
        if (bodyR > 4.0f)
        {
            juce::ColourGradient bodyGrad(juce::Colour(0xff243248), cx, cy - bodyR,
                                          juce::Colour(0xff101722), cx, cy + bodyR, false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

            g.setColour(juce::Colour(0xff344766));
            g.drawEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

            // Crisp pointer line + glowing tip
            float ptrLen = bodyR - 1.8f;
            float ptrIn  = bodyR * 0.28f;
            float sinA   = std::sin(curAngle);
            float cosA   = -std::cos(curAngle);
            g.setColour(accent_);
            g.drawLine(cx + sinA * ptrIn, cy + cosA * ptrIn,
                       cx + sinA * ptrLen, cy + cosA * ptrLen, 2.2f);
        }

        // Bottom numeric readout
        int iVal = static_cast<int>(std::lround(val));
        juce::String valStr = (bipolar_ && iVal > 0) ? ("+" + juce::String(iVal)) : juce::String(iVal);
        valStr += suffix_;

        g.setColour(kTextBright.withAlpha(0.92f));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(valStr, 0, getHeight() - 13, getWidth(), 13, juce::Justification::centred, false);
    }

private:
    juce::Slider slider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    juce::String label_;
    juce::Colour accent_;
    bool bipolar_ = false;
    juce::String suffix_;
};

// ==============================================================================
// ModernBarSlider: Sleek horizontal bar slider for GLIDE and MASTER LEVEL
// ==============================================================================
class AbakosPROAudioProcessorEditor::ModernBarSlider : public juce::Component,
                                                       private juce::Slider::Listener
{
public:
    ModernBarSlider(juce::AudioProcessorValueTreeState& apvts,
                    const juce::String& paramID,
                    const juce::String& labelText,
                    juce::Colour accent)
        : label_(labelText), accent_(accent)
    {
        slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider_.setAlpha(0.0f);
        slider_.addListener(this);
        addAndMakeVisible(slider_);

        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider_);
        if (auto* param = apvts.getParameter(paramID))
            slider_.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    }

    void resized() override
    {
        slider_.setBounds(getLocalBounds());
    }

    void sliderValueChanged(juce::Slider*) override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        const float minV = static_cast<float>(slider_.getMinimum());
        const float maxV = static_cast<float>(slider_.getMaximum());
        const float val  = static_cast<float>(slider_.getValue());
        const float norm = (maxV > minV) ? (val - minV) / (maxV - minV) : 0.0f;

        g.setColour(kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(label_, 0, 0, getWidth() / 2, 13, juce::Justification::centredLeft, false);

        int pct = static_cast<int>(std::lround(val));
        g.setColour(kTextBright);
        g.drawText(juce::String(pct) + "%", getWidth() / 2, 0, getWidth() / 2, 13, juce::Justification::centredRight, false);

        auto track = juce::Rectangle<float>(0.0f, 16.0f, static_cast<float>(getWidth()), 8.0f);
        g.setColour(kTrackDark);
        g.fillRoundedRectangle(track, 4.0f);
        g.setColour(kCardBorder);
        g.drawRoundedRectangle(track, 4.0f, 1.0f);

        if (norm > 0.005f)
        {
            auto fill = track.withWidth( std::max(6.0f, track.getWidth() * norm) );
            g.setColour(accent_);
            g.fillRoundedRectangle(fill, 4.0f);
        }
    }

private:
    juce::Slider slider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    juce::String label_;
    juce::Colour accent_;
};

// ==============================================================================
// WaveformStrip: 6-button graphical waveform selector (SAW, SQR, TRI, SIN, PLS, ORG)
// ==============================================================================
class AbakosPROAudioProcessorEditor::WaveformStrip : public juce::Component,
                                                     private juce::Slider::Listener
{
public:
    WaveformStrip(juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramID,
                  juce::Colour accent)
        : accent_(accent)
    {
        hiddenSlider_.setRange(0.0, 5.0, 1.0);
        hiddenSlider_.addListener(this);
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, hiddenSlider_);
    }

    void sliderValueChanged(juce::Slider*) override { repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        int idx = (e.x * 6) / std::max(1, getWidth());
        idx = std::max(0, std::min(5, idx));
        hiddenSlider_.setValue(static_cast<double>(idx), juce::sendNotificationSync);
    }

    static void drawMiniWave(juce::Graphics& g, int waveType, juce::Rectangle<float> r)
    {
        juce::Path p;
        const float x0 = r.getX() + 3.0f;
        const float w  = r.getWidth() - 6.0f;
        const float cy = r.getCentreY();
        const float a  = r.getHeight() * 0.34f;

        const int steps = 28;
        for (int i = 0; i <= steps; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float y = 0.0f;
            switch (waveType)
            {
                case 0: y = 2.0f * t - 1.0f; break;
                case 1: y = (t < 0.5f) ? -1.0f : 1.0f; break;
                case 2: y = (t < 0.5f) ? (-1.0f + 4.0f * t) : (3.0f - 4.0f * t); break;
                case 3: y = -std::sin(t * juce::MathConstants<float>::twoPi); break;
                case 4: y = (t < 0.25f) ? -1.0f : 0.65f; break;
                case 5:
                    y = -0.6f * std::sin(t * juce::MathConstants<float>::twoPi)
                        -0.4f * std::sin(t * juce::MathConstants<float>::twoPi * 2.0f)
                        -0.25f * std::sin(t * juce::MathConstants<float>::twoPi * 3.0f);
                    break;
                default: break;
            }
            float px = x0 + t * w;
            float py = cy + y * a;
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void paint(juce::Graphics& g) override
    {
        static const char* kNames[6] = { "SAW", "SQR", "TRI", "SIN", "PLS", "ORG" };
        const int active = std::max(0, std::min(5, static_cast<int>(std::lround(hiddenSlider_.getValue()))));
        const float cellW = static_cast<float>(getWidth()) / 6.0f;

        for (int i = 0; i < 6; ++i)
        {
            auto cell = juce::Rectangle<float>(static_cast<float>(i) * cellW + 1.5f, 0.0f, cellW - 3.0f, static_cast<float>(getHeight()));
            bool isSel = (i == active);

            g.setColour(isSel ? accent_.withAlpha(0.22f) : kTrackDark);
            g.fillRoundedRectangle(cell, 4.0f);

            g.setColour(isSel ? accent_ : kCardBorder);
            g.drawRoundedRectangle(cell, 4.0f, isSel ? 1.4f : 1.0f);

            g.setColour(isSel ? accent_ : kTextMuted);
            drawMiniWave(g, i, cell.withTrimmedBottom(12.0f));

            g.setColour(isSel ? kTextBright : kTextMuted);
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.drawText(kNames[i], cell.removeFromBottom(12.0f).toNearestInt(), juce::Justification::centred, false);
        }
    }

private:
    juce::Slider hiddenSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    juce::Colour accent_;
};

// ==============================================================================
// PillToggle: Clean 2-state pill switch (e.g. LPF/HPF, POLY/MONO, PB 2/12, ARP ON)
// ==============================================================================
class AbakosPROAudioProcessorEditor::PillToggle : public juce::Component,
                                                  private juce::Button::Listener
{
public:
    PillToggle(juce::AudioProcessorValueTreeState& apvts,
               const juce::String& paramID,
               const juce::String& offLabel,
               const juce::String& onLabel,
               juce::Colour accent)
        : offText_(offLabel), onText_(onLabel), accent_(accent)
    {
        btn_.setClickingTogglesState(true);
        btn_.setAlpha(0.0f);
        btn_.addListener(this);
        addAndMakeVisible(btn_);
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramID, btn_);
    }

    void resized() override { btn_.setBounds(getLocalBounds()); }

    void buttonClicked(juce::Button*) override
    {
        repaint();
        if (auto* p = getParentComponent())
            p->repaint();
    }

    void paint(juce::Graphics& g) override
    {
        bool on = btn_.getToggleState();
        auto r  = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(on ? accent_.withAlpha(0.24f) : kTrackDark);
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(on ? accent_ : kCardBorder);
        g.drawRoundedRectangle(r, 4.0f, 1.2f);

        // Status LED dot
        g.setColour(on ? accent_ : kTextMuted.withAlpha(0.4f));
        g.fillEllipse(r.getX() + 7.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);

        g.setColour(on ? kTextBright : kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(on ? onText_ : offText_, getLocalBounds().withTrimmedLeft(15), juce::Justification::centred, false);
    }

private:
    juce::ToggleButton btn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
    juce::String offText_;
    juce::String onText_;
    juce::Colour accent_;
};

// ==============================================================================
// ValueStepper: Compact - / value / + stepper for TUNE, OCTAVE, ARP PATTERN
// ==============================================================================
class AbakosPROAudioProcessorEditor::ValueStepper : public juce::Component,
                                                    private juce::Slider::Listener
{
public:
    ValueStepper(juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramID,
                 const juce::String& title,
                 juce::Colour accent,
                 int displayOffset = 0,
                 bool showPlus = false,
                 std::function<void()> onCenterClick = nullptr)
        : title_(title), accent_(accent), offset_(displayOffset), showPlus_(showPlus), centerClick_(std::move(onCenterClick))
    {
        slider_.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider_.addListener(this);
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider_);
    }

    void sliderValueChanged(juce::Slider*) override { repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.y < 14)
            return;

        if (e.x < 22)
            slider_.setValue(slider_.getValue() - 1.0, juce::sendNotificationSync);
        else if (e.x > getWidth() - 22)
            slider_.setValue(slider_.getValue() + 1.0, juce::sendNotificationSync);
        else if (centerClick_)
            centerClick_();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(title_, 0, 0, getWidth(), 13, juce::Justification::centred, false);

        auto box = juce::Rectangle<float>(1.0f, 14.0f, static_cast<float>(getWidth() - 2), static_cast<float>(getHeight() - 15));
        g.setColour(kTrackDark);
        g.fillRoundedRectangle(box, 4.0f);
        g.setColour(kCardBorder);
        g.drawRoundedRectangle(box, 4.0f, 1.0f);

        g.setColour(kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("-", 2, 14, 18, getHeight() - 15, juce::Justification::centred, false);
        g.drawText("+", getWidth() - 20, 14, 18, getHeight() - 15, juce::Justification::centred, false);

        int v = static_cast<int>(std::lround(slider_.getValue())) + offset_;
        juce::String txt = (showPlus_ && v > 0) ? ("+" + juce::String(v)) : juce::String(v);
        g.setColour(accent_);
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.drawText(txt, 18, 14, getWidth() - 36, getHeight() - 15, juce::Justification::centred, false);
    }

private:
    juce::Slider slider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    juce::String title_;
    juce::Colour accent_;
    int offset_ = 0;
    bool showPlus_ = false;
    std::function<void()> centerClick_;
};

// ==============================================================================
// ModernWheel: Vector Pitch Bend & Mod Wheel
// ==============================================================================
class AbakosPROAudioProcessorEditor::ModernWheel : public juce::Component
{
public:
    ModernWheel(AbakosPROAudioProcessor& proc, const juce::String& label, bool isPitchBend, juce::Colour accent)
        : proc_(proc), label_(label), isPitch_(isPitchBend), accent_(accent)
    {
        value_ = isPitch_ ? 0.5f : 0.0f;
    }

    void mouseDown(const juce::MouseEvent& e) override { updateFromMouse(e.y); }
    void mouseDrag(const juce::MouseEvent& e) override { updateFromMouse(e.y); }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (isPitch_)
        {
            value_ = 0.5f;
            proc_.getSynthEngine().setPitchBend(0.0f);
            repaint();
        }
    }

    void syncFromEngine()
    {
        float nv = isPitch_ ? (proc_.getSynthEngine().getPitchBend() * 0.5f + 0.5f)
                            : proc_.getSynthEngine().getModWheel();
        if (std::abs(nv - value_) > 0.005f)
        {
            value_ = nv;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto track = juce::Rectangle<float>(2.0f, 2.0f, static_cast<float>(getWidth() - 4), static_cast<float>(getHeight() - 16));
        g.setColour(kTrackDark);
        g.fillRoundedRectangle(track, 4.0f);
        g.setColour(kCardBorder);
        g.drawRoundedRectangle(track, 4.0f, 1.0f);

        float thumbY = track.getY() + (1.0f - value_) * (track.getHeight() - 8.0f);
        if (!isPitch_)
        {
            auto fillRect = track.withTop(thumbY + 4.0f);
            g.setColour(accent_.withAlpha(0.28f));
            g.fillRoundedRectangle(fillRect, 3.0f);
        }

        g.setColour(accent_);
        g.fillRoundedRectangle(track.getX() + 2.0f, thumbY, track.getWidth() - 4.0f, 7.0f, 2.5f);

        g.setColour(kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(label_, 0, getHeight() - 13, getWidth(), 13, juce::Justification::centred, false);
    }

private:
    void updateFromMouse(int y)
    {
        float h = static_cast<float>(std::max(1, getHeight() - 16));
        float norm = 1.0f - std::max(0.0f, std::min(1.0f, static_cast<float>(y - 2) / h));
        value_ = norm;
        if (isPitch_)
            proc_.getSynthEngine().setPitchBend(norm * 2.0f - 1.0f);
        else
            proc_.getSynthEngine().setModWheel(norm);
        repaint();
    }

    AbakosPROAudioProcessor& proc_;
    juce::String label_;
    bool isPitch_ = false;
    juce::Colour accent_;
    float value_ = 0.5f;
};

// ==============================================================================
// VectorKeyboard: Clean 61-key vector MIDI keyboard (C2=36 .. C7=96)
// ==============================================================================
class AbakosPROAudioProcessorEditor::VectorKeyboard : public juce::Component
{
public:
    explicit VectorKeyboard(AbakosPROAudioProcessor& proc) : proc_(proc) {}

    static bool isBlack(int noteInOct) noexcept
    {
        return noteInOct == 1 || noteInOct == 3 || noteInOct == 6 || noteInOct == 8 || noteInOct == 10;
    }

    int getNoteAt(juce::Point<int> pt) const noexcept
    {
        const float whiteW = static_cast<float>(getWidth()) / 36.0f;
        const float blackW = whiteW * 0.62f;
        const int blackH   = static_cast<int>(static_cast<float>(getHeight()) * 0.60f);
        const float px     = static_cast<float>(pt.x);

        if (pt.y >= 0 && pt.y <= blackH)
        {
            int wIdx = 0;
            for (int n = 36; n <= 96; ++n)
            {
                if (!isBlack(n % 12))
                {
                    ++wIdx;
                }
                else
                {
                    float bx = static_cast<float>(wIdx) * whiteW - blackW * 0.5f;
                    if (px >= bx && px < bx + blackW)
                        return n;
                }
            }
        }

        if (pt.y >= 0 && pt.y <= getHeight())
        {
            int targetW = std::max(0, std::min(35, static_cast<int>(px / whiteW)));
            int wIdx = 0;
            for (int n = 36; n <= 96; ++n)
            {
                if (!isBlack(n % 12))
                {
                    if (wIdx == targetW)
                        return n;
                    ++wIdx;
                }
            }
        }
        return -1;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        int n = getNoteAt(e.getPosition());
        if (n >= 0)
        {
            mouseNote_ = n;
            proc_.getKeyboardState().noteOn(1, mouseNote_, 0.88f);
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        int n = getNoteAt(e.getPosition());
        if (n != mouseNote_)
        {
            if (mouseNote_ >= 0)
                proc_.getKeyboardState().noteOff(1, mouseNote_, 0.0f);
            mouseNote_ = n;
            if (mouseNote_ >= 0)
                proc_.getKeyboardState().noteOn(1, mouseNote_, 0.88f);
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (mouseNote_ >= 0)
        {
            proc_.getKeyboardState().noteOff(1, mouseNote_, 0.0f);
            mouseNote_ = -1;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        const float whiteW = static_cast<float>(getWidth()) / 36.0f;
        const float blackW = whiteW * 0.62f;
        const float h      = static_cast<float>(getHeight());
        const float blackH = h * 0.60f;

        int wIdx = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (isBlack(n % 12))
                continue;

            bool active = proc_.getSynthEngine().isNoteActive(n) || proc_.getKeyboardState().isNoteOn(1, n);
            auto r = juce::Rectangle<float>(static_cast<float>(wIdx) * whiteW + 0.5f, 0.0f, whiteW - 1.0f, h - 1.0f);

            g.setColour(active ? kAccentOsc : juce::Colour(0xffdde5f0));
            g.fillRoundedRectangle(r, 2.5f);
            g.setColour(juce::Colour(0xff101722));
            g.drawRoundedRectangle(r, 2.5f, 0.8f);

            if (n % 12 == 0)
            {
                int oct = (n / 12) - 1;
                g.setColour(active ? kBgTop : juce::Colour(0xff52647c));
                g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
                g.drawText("C" + juce::String(oct), r.removeFromBottom(14.0f).toNearestInt(), juce::Justification::centred, false);
            }
            ++wIdx;
        }

        wIdx = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (!isBlack(n % 12))
            {
                ++wIdx;
                continue;
            }

            bool active = proc_.getSynthEngine().isNoteActive(n) || proc_.getKeyboardState().isNoteOn(1, n);
            auto r = juce::Rectangle<float>(static_cast<float>(wIdx) * whiteW - blackW * 0.5f, 0.0f, blackW, blackH);

            g.setColour(active ? kAccentOsc.brighter(0.2f) : juce::Colour(0xff151c28));
            g.fillRoundedRectangle(r, 2.0f);
            g.setColour(juce::Colour(0xff080c11));
            g.drawRoundedRectangle(r, 2.0f, 1.0f);
        }
    }

private:
    AbakosPROAudioProcessor& proc_;
    int mouseNote_ = -1;
};

// ==============================================================================
// AbakosPROAudioProcessorEditor Implementation
// ==============================================================================
AbakosPROAudioProcessorEditor::AbakosPROAudioProcessorEditor(AbakosPROAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef_(p)
{
    auto& apvts = processorRef_.getAPVTS();

    auto addKnob = [this, &apvts](const char* id, const char* label, int x, int y, int w, int h,
                                  juce::Colour accent, bool bipolar = false, const char* suffix = "")
    {
        auto k = std::make_unique<ModernKnob>(apvts, id, label, accent, bipolar, suffix);
        k->setBounds(x, y, w, h);
        addAndMakeVisible(*k);
        knobs_.push_back(std::move(k));
    };

    // --- Header Bar Controls (y = 14..40) ---
    static const char* kBankLabels[5] = { "A", "B", "C", "D", "EXT" };
    int bx = 228;
    for (int i = 0; i < 5; ++i)
    {
        int bw = (i == 4) ? 40 : 30;
        bankBadges_[static_cast<size_t>(i)] = std::make_unique<ActionBadge>(
            kBankLabels[i], kAccentOsc, [this, i]() { processorRef_.selectBank(i); repaint(); });
        bankBadges_[static_cast<size_t>(i)]->setBounds(bx, 14, bw, 26);
        addAndMakeVisible(*bankBadges_[static_cast<size_t>(i)]);
        bx += bw + 4;
    }

    prevPresetBadge_ = std::make_unique<ActionBadge>("<", kAccentOsc, [this]() { processorRef_.stepPreset(-1); repaint(); });
    prevPresetBadge_->setBounds(416, 14, 26, 26);
    addAndMakeVisible(*prevPresetBadge_);

    presetLcdBounds_ = { 446, 12, 214, 30 };

    nextPresetBadge_ = std::make_unique<ActionBadge>(">", kAccentOsc, [this]() { processorRef_.stepPreset(1); repaint(); });
    nextPresetBadge_->setBounds(664, 14, 26, 26);
    addAndMakeVisible(*nextPresetBadge_);

    loadPresetBadge_ = std::make_unique<ActionBadge>("LOAD", kAccentOsc, [this]() { loadPresetWithChooser(); });
    loadPresetBadge_->setBounds(696, 14, 44, 26);
    addAndMakeVisible(*loadPresetBadge_);

    savePresetBadge_ = std::make_unique<ActionBadge>("SAVE", kAccentOsc, [this]() { savePresetWithChooser(); });
    savePresetBadge_->setBounds(744, 14, 44, 26);
    addAndMakeVisible(*savePresetBadge_);

    tunBadge_ = std::make_unique<ActionBadge>("12-TET", kAccentFilter, [this]() { showTuningPopupMenu(); });
    tunBadge_->setBounds(794, 14, 84, 26);
    addAndMakeVisible(*tunBadge_);

    cfgBadge_ = std::make_unique<ActionBadge>("ENGINE CFG", kAccentFx, [this]() { showConfigDialog(); });
    cfgBadge_->setBounds(882, 14, 84, 26);
    addAndMakeVisible(*cfgBadge_);

    // --- ROW 1 (y = 62..220): OSC 1 | OSC 2 | NOISE & MASTER PITCH ---
    osc1WaveStrip_ = std::make_unique<WaveformStrip>(apvts, "osc1_wave", kAccentOsc);
    osc1WaveStrip_->setBounds(26, 94, 292, 34);
    addAndMakeVisible(*osc1WaveStrip_);

    addKnob("osc1_semis", "SEMIS", 38,  134, 72, 78, kAccentOsc, true, " st");
    addKnob("osc1_cents", "CENTS", 136, 134, 72, 78, kAccentOsc, true, " ct");
    addKnob("osc1_level", "LEVEL", 234, 134, 72, 78, kAccentOsc, false, "%");

    osc2WaveStrip_ = std::make_unique<WaveformStrip>(apvts, "osc2_wave", kAccentOsc);
    osc2WaveStrip_->setBounds(352, 94, 322, 34);
    addAndMakeVisible(*osc2WaveStrip_);

    addKnob("osc2_semis", "SEMIS", 354, 134, 72, 78, kAccentOsc, true, " st");
    addKnob("osc2_cents", "CENTS", 436, 134, 72, 78, kAccentOsc, true, " ct");
    addKnob("osc2_sync",  "SYNC",  518, 134, 72, 78, kAccentOsc, false, "%");
    addKnob("osc2_level", "LEVEL", 600, 134, 72, 78, kAccentOsc, false, "%");

    // Noise & Master Pitch Card (x = 694..966)
    addKnob("noise_color",     "COLOR",    706, 94, 68, 76, kAccentOsc, true);
    addKnob("noise_pitch_mod", "PITCH FM", 796, 94, 68, 76, kAccentOsc, false, "%");
    addKnob("noise_level",     "LEVEL",    886, 94, 68, 76, kAccentOsc, false, "%");

    tuneStepper_ = std::make_unique<ValueStepper>(apvts, "tune", "FINE TUNE", kAccentOsc, 0, true);
    tuneStepper_->setBounds(712, 174, 112, 38);
    addAndMakeVisible(*tuneStepper_);

    octaveStepper_ = std::make_unique<ValueStepper>(apvts, "octave", "OCTAVE", kAccentOsc, 0, true);
    octaveStepper_->setBounds(840, 174, 112, 38);
    addAndMakeVisible(*octaveStepper_);

    // --- ROW 2 (y = 228..386): CHAMBERLIN SVF FILTER | ADS 1 (OSC) & ADS 2 (FILTER) ---
    filterGraphBounds_ = { 26, 262, 134, 82 };

    filterModeToggle_ = std::make_unique<PillToggle>(apvts, "filter_hpf_lpf", "MODE: LPF 12dB", "MODE: HPF 12dB", kAccentFilter);
    filterModeToggle_->setBounds(26, 350, 134, 24);
    addAndMakeVisible(*filterModeToggle_);

    addKnob("filter_cutoff",    "CUTOFF",    170, 272, 68, 92, kAccentFilter, false, "%");
    addKnob("filter_resonance", "RESONANCE", 240, 272, 68, 92, kAccentFilter, false, "%");
    addKnob("filter_pitch_mod", "KEY TRACK", 310, 272, 68, 92, kAccentFilter, false, "%");

    addKnob("ads1_attack",  "ADS1 ATK", 406, 264, 64, 80, kAccentEnv, false);
    addKnob("ads1_decay",   "ADS1 DEC", 472, 264, 64, 80, kAccentEnv, false);
    addKnob("ads1_sustain", "ADS1 SUS", 538, 264, 64, 80, kAccentEnv, false, "%");
    addKnob("ads1_mod",     "OSC MOD",  604, 264, 64, 80, kAccentEnv, true);

    addKnob("ads2_attack",  "ADS2 ATK", 694, 264, 64, 80, kAccentEnv, false);
    addKnob("ads2_decay",   "ADS2 DEC", 760, 264, 64, 80, kAccentEnv, false);
    addKnob("ads2_sustain", "ADS2 SUS", 826, 264, 64, 80, kAccentEnv, false, "%");
    addKnob("ads2_mod",     "FLT MOD",  892, 264, 64, 80, kAccentEnv, true);

    adsGraphBounds_ = { 410, 350, 542, 26 };

    // --- ROW 3 (y = 394..548): ADSR 1 & ADSR 2 | LFO & VOICE | STEREO DELAY & OUTPUT ---
    addKnob("adsr1_attack",  "A1 ATK", 22,  426, 54, 76, kAccentEnv);
    addKnob("adsr1_decay",   "A1 DEC", 76,  426, 54, 76, kAccentEnv);
    addKnob("adsr1_sustain", "A1 SUS", 130, 426, 54, 76, kAccentEnv);
    addKnob("adsr1_release", "A1 REL", 184, 426, 54, 76, kAccentEnv);

    addKnob("adsr2_attack",  "A2 ATK", 248, 426, 54, 76, kAccentEnv);
    addKnob("adsr2_decay",   "A2 DEC", 302, 426, 54, 76, kAccentEnv);
    addKnob("adsr2_sustain", "A2 SUS", 356, 426, 54, 76, kAccentEnv);
    addKnob("adsr2_release", "A2 REL", 410, 426, 54, 76, kAccentEnv);

    adsrGraphBounds_ = { 26, 508, 434, 30 };

    // LFO & Voice Card (x = 480..726)
    addKnob("lfo_rate",   "RATE", 488, 426, 56, 76, kAccentMod);
    addKnob("lfo_pitch",  "VIBR", 546, 426, 56, 76, kAccentMod);
    addKnob("lfo_filter", "WAH",  604, 426, 56, 76, kAccentMod);
    addKnob("lfo_pan",    "PAN",  662, 426, 56, 76, kAccentMod);

    voiceModeToggle_ = std::make_unique<PillToggle>(apvts, "mode", "POLYPHONIC", "MONOPHONIC", kAccentMod);
    voiceModeToggle_->setBounds(490, 510, 110, 26);
    addAndMakeVisible(*voiceModeToggle_);

    pbRangeToggle_ = std::make_unique<PillToggle>(apvts, "pitch_bend_range", "BEND: +/-2", "BEND: +/-12", kAccentMod);
    pbRangeToggle_->setBounds(608, 510, 108, 26);
    addAndMakeVisible(*pbRangeToggle_);

    // Stereo Delay & Output Card (x = 734..966)
    addKnob("delay_1",        "DELAY L", 740, 426, 54, 76, kAccentFx);
    addKnob("delay_2",        "DELAY R", 794, 426, 54, 76, kAccentFx);
    addKnob("delay_feedback", "FEEDBK",  848, 426, 54, 76, kAccentFx);
    addKnob("delay_mix",      "DLY MIX", 902, 426, 54, 76, kAccentFx);

    glideSlider_ = std::make_unique<ModernBarSlider>(apvts, "portamento", "GLIDE", kAccentFx);
    glideSlider_->setBounds(746, 509, 98, 28);
    addAndMakeVisible(*glideSlider_);

    masterSlider_ = std::make_unique<ModernBarSlider>(apvts, "master_level", "MASTER", kAccentOsc);
    masterSlider_->setBounds(856, 509, 98, 28);
    addAndMakeVisible(*masterSlider_);

    // --- ROW 4 (y = 556..648): ARPEGGIATOR + WHEELS + 61-KEY VECTOR KEYBOARD ---
    arpOnToggle_ = std::make_unique<PillToggle>(apvts, "arp_on_off", "ARP OFF", "ARP ON", kAccentMod);
    arpOnToggle_->setBounds(24, 586, 86, 26);
    addAndMakeVisible(*arpOnToggle_);

    arpHoldToggle_ = std::make_unique<PillToggle>(apvts, "arp_hold", "HOLD OFF", "HOLD ON", kAccentMod);
    arpHoldToggle_->setBounds(24, 616, 86, 26);
    addAndMakeVisible(*arpHoldToggle_);

    arpPatternStepper_ = std::make_unique<ValueStepper>(
        apvts, "arp_pattern", "ARP PATTERN", kAccentMod, 1, false,
        [this]() { showArpPatternPopupMenu(); });
    arpPatternStepper_->setBounds(118, 594, 96, 44);
    addAndMakeVisible(*arpPatternStepper_);

    pitchWheel_ = std::make_unique<ModernWheel>(processorRef_, "PB", true, kAccentOsc);
    pitchWheel_->setBounds(234, 564, 24, 78);
    addAndMakeVisible(*pitchWheel_);

    modWheel_ = std::make_unique<ModernWheel>(processorRef_, "MOD", false, kAccentMod);
    modWheel_->setBounds(262, 564, 24, 78);
    addAndMakeVisible(*modWheel_);

    keyboardComp_ = std::make_unique<VectorKeyboard>(processorRef_);
    keyboardComp_->setBounds(294, 564, 662, 78);
    addAndMakeVisible(*keyboardComp_);

    setSize(980, 658);
    startTimerHz(30);
}

AbakosPROAudioProcessorEditor::~AbakosPROAudioProcessorEditor()
{
    stopTimer();
}

void AbakosPROAudioProcessorEditor::timerCallback()
{
    if (pitchWheel_)
        pitchWheel_->syncFromEngine();
    if (modWheel_)
        modWheel_->syncFromEngine();
    if (keyboardComp_)
        keyboardComp_->repaint();

    const int activeBank = processorRef_.getCurrentBank();
    for (int i = 0; i < 5; ++i)
    {
        if (bankBadges_[static_cast<size_t>(i)])
            bankBadges_[static_cast<size_t>(i)]->setHighlighted(i == activeBank);
    }

    if (tunBadge_)
        tunBadge_->setText(processorRef_.getActiveTuningName().substring(0, 11));

    repaint(presetLcdBounds_);
}

void AbakosPROAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (presetLcdBounds_.contains(e.getPosition()))
        showPresetPopupMenu();
}

void AbakosPROAudioProcessorEditor::showPresetPopupMenu()
{
    juce::PopupMenu menu;
    const int curProg = processorRef_.getCurrentProgram();

    for (int group = 0; group < 4; ++group)
    {
        juce::PopupMenu sub;
        int startP = group * 32;
        int endP   = startP + 31;
        for (int p = startP; p <= endP; ++p)
        {
            char prefix[16];
            std::snprintf(prefix, sizeof(prefix), "%02X: ", p);
            sub.addItem(p + 1, juce::String(prefix) + processorRef_.getProgramName(p), true, p == curProg);
        }
        char groupTitle[32];
        std::snprintf(groupTitle, sizeof(groupTitle), "Programs %02X - %02X", startP, endP);
        menu.addSubMenu(groupTitle, sub);
    }

    menu.showMenuAsync(juce::PopupMenu::Options(),
                       [this](int res)
                       {
                           if (res >= 1 && res <= 128)
                           {
                               processorRef_.setCurrentProgram(res - 1);
                               repaint();
                           }
                       });
}

void AbakosPROAudioProcessorEditor::showArpPatternPopupMenu()
{
    static const char* kPatNames[24] = {
        "01: 1/16 Up 1-Oct",          "02: 1/16 Down 1-Oct",        "03: 1/16 Up/Down 1-Oct",
        "04: 1/16 Up 2-Oct",          "05: 1/16 Down 2-Oct",        "06: 1/16 Up/Down 2-Oct",
        "07: 1/8 Up Classic",         "08: 1/8 Down Classic",       "09: 1/16 Octave Bounce",
        "10: 1/16 Bass Octaver",      "11: 1/16 Trance Gate",       "12: 1/16T Triplet Up",
        "13: 1/16T Triplet Down",     "14: Fifths & Octaves",       "15: Acid Line 1",
        "16: Acid Line 2",            "17: 3-Oct Cascade Up",       "18: 3-Oct Cascade Down",
        "19: Syncopated Electro",     "20: Staccato Pluck",         "21: Minor Arp Seq",
        "22: Major Arp Seq",          "23: Sus4 Arp Seq",           "24: Fast 1/32 Cyber Roll"
    };

    juce::PopupMenu m;
    int cur = static_cast<int>(std::lround(processorRef_.getSynthEngine().getParameterValue(abakos::kArpPattern)));
    for (int i = 0; i < 24; ++i)
        m.addItem(i + 1, kPatNames[i], true, i == cur);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(arpPatternStepper_.get()),
                    [this](int res)
                    {
                        if (res >= 1 && res <= 24)
                        {
                            if (auto* p = processorRef_.getAPVTS().getParameter("arp_pattern"))
                                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                        }
                    });
}

void AbakosPROAudioProcessorEditor::showTuningPopupMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Reset to 12-TET Standard", true, processorRef_.getActiveTuningName() == "12-TET Standard");
    menu.addItem(2, "Load Custom .tun File...");
    menu.addSeparator();

    auto tunDir = processorRef_.getTuningDirectory();
    juce::Array<juce::File> tunFiles;
    if (tunDir.isDirectory())
    {
        tunDir.findChildFiles(tunFiles, juce::File::findFiles, false, "*.tun");
        tunFiles.sort();
        for (int i = 0; i < tunFiles.size(); ++i)
        {
            juce::String fname = tunFiles[i].getFileNameWithoutExtension();
            menu.addItem(100 + i, fname, true, processorRef_.getActiveTuningName() == fname);
        }
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(tunBadge_.get()),
                       [this, tunFiles](int result)
                       {
                           if (result == 1)
                           {
                               processorRef_.resetTuningTo12TET();
                               repaint();
                           }
                           else if (result == 2)
                           {
                               activeFileChooser_ = std::make_unique<juce::FileChooser>(
                                   "Load Abakos PRO Microtonal Scale (.tun)",
                                   processorRef_.getTuningDirectory(),
                                   "*.tun");
                               activeFileChooser_->launchAsync(
                                   juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this](const juce::FileChooser& fc)
                                   {
                                       auto f = fc.getResult();
                                       if (f.existsAsFile())
                                       {
                                           processorRef_.loadTuningFile(f);
                                           repaint();
                                       }
                                   });
                           }
                           else if (result >= 100 && result - 100 < tunFiles.size())
                           {
                               processorRef_.loadTuningFile(tunFiles[result - 100]);
                               repaint();
                           }
                       });
}

void AbakosPROAudioProcessorEditor::showConfigDialog()
{
    auto cfg = processorRef_.getConfigSettings();
    juce::PopupMenu menu;

    juce::PopupMenu polyMenu;
    for (int poly : { 1, 4, 8, 12, 16, 24, 32, 64 })
        polyMenu.addItem(1000 + poly, juce::String(poly) + " Voices", true, cfg.polyphony == poly);
    menu.addSubMenu("Polyphony (" + juce::String(cfg.polyphony) + ")", polyMenu);

    juce::PopupMenu osMenu;
    for (int os : { 1, 2, 4 })
        osMenu.addItem(2000 + os, juce::String(os) + "x Oversampling", true, cfg.oversampling == os);
    menu.addSubMenu("Oversampling (" + juce::String(cfg.oversampling) + "x)", osMenu);

    juce::PopupMenu hrMenu;
    for (int hr : { 1, 2, 4, 8 })
        hrMenu.addItem(3000 + hr, "Headroom 1/" + juce::String(hr), true, cfg.headroom == hr);
    menu.addSubMenu("Headroom (1/" + juce::String(cfg.headroom) + ")", hrMenu);

    juce::PopupMenu dcMenu;
    for (int dc : { 5, 12, 20, 30 })
        dcMenu.addItem(4000 + dc, juce::String(dc) + " Hz", true, cfg.dcBlockingFreq == dc);
    menu.addSubMenu("DC Blocking (" + juce::String(cfg.dcBlockingFreq) + " Hz)", dcMenu);

    menu.addItem(5000, "Multitimbral Mode", true, cfg.multitimbral != 0);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(cfgBadge_.get()),
                       [this, cfg](int res) mutable
                       {
                           if (res >= 1001 && res <= 1064) cfg.polyphony = res - 1000;
                           else if (res >= 2001 && res <= 2004) cfg.oversampling = res - 2000;
                           else if (res >= 3001 && res <= 3008) cfg.headroom = res - 3000;
                           else if (res >= 4001 && res <= 4100) cfg.dcBlockingFreq = res - 4000;
                           else if (res == 5000) cfg.multitimbral = cfg.multitimbral ? 0 : 1;
                           else return;

                           processorRef_.applyConfigSettings(cfg, true);
                           repaint();
                       });
}

void AbakosPROAudioProcessorEditor::loadPresetWithChooser()
{
    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Load AbbaKhaos / Abakos Preset (.txt)",
        processorRef_.getDataDirectory(),
        "*.txt");
    activeFileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile())
            {
                processorRef_.loadExternalPresetFile(f);
                repaint();
            }
        });
}

void AbakosPROAudioProcessorEditor::savePresetWithChooser()
{
    activeFileChooser_ = std::make_unique<juce::FileChooser>(
        "Save AbbaKhaos / Abakos Preset (.txt)",
        processorRef_.getDataDirectory().getChildFile(processorRef_.getCurrentPresetName() + ".txt"),
        "*.txt");
    activeFileChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File{})
                processorRef_.saveCurrentPresetToFile(f);
        });
}

void AbakosPROAudioProcessorEditor::drawCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                                             const juce::String& title, juce::Colour accent) const
{
    auto rf = bounds.toFloat();
    g.setColour(kCardBg);
    g.fillRoundedRectangle(rf, 6.0f);

    if (title.isNotEmpty())
    {
        auto header = bounds.removeFromTop(24).toFloat();
        g.setColour(kCardHeader);
        g.fillRoundedRectangle(header.getX(), header.getY(), header.getWidth(), header.getHeight(), 6.0f);

        g.setColour(accent);
        g.fillRoundedRectangle(header.getX() + 8.0f, header.getY() + 6.0f, 3.5f, 12.0f, 1.5f);

        g.setColour(kTextBright);
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText(title, header.withTrimmedLeft(18.0f).toNearestInt(), juce::Justification::centredLeft, false);
    }

    g.setColour(kCardBorder);
    g.drawRoundedRectangle(rf, 6.0f, 1.0f);
}

void AbakosPROAudioProcessorEditor::drawFilterResponse(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    auto rf = bounds.toFloat();
    g.setColour(kTrackDark);
    g.fillRoundedRectangle(rf, 4.0f);
    g.setColour(kCardBorder);
    g.drawRoundedRectangle(rf, 4.0f, 1.0f);

    const auto& eng = processorRef_.getSynthEngine();
    const float cut = eng.getParameterValue(abakos::kFilterCutoff) * 0.01f;
    const float res = eng.getParameterValue(abakos::kFilterResonance) * 0.01f;
    const bool hpf  = eng.getParameterValue(abakos::kFilterHpfLpf) >= 0.5f;

    juce::Path curve;
    const int w = bounds.getWidth() - 8;
    const float x0 = rf.getX() + 4.0f;
    const float yBase = rf.getBottom() - 8.0f;
    const float h = rf.getHeight() - 18.0f;

    for (int i = 0; i <= w; ++i)
    {
        float xNorm = static_cast<float>(i) / static_cast<float>(w);
        float d     = xNorm - (0.08f + cut * 0.84f);
        float pass  = hpf ? (1.0f / (1.0f + std::exp(-d * 14.0f)))
                          : (1.0f / (1.0f + std::exp(d * 14.0f)));
        float resBump = res * 0.55f * std::exp(-(d * d) * 140.0f);
        float mag = std::min(1.15f, pass * 0.68f + resBump);

        float px = x0 + static_cast<float>(i);
        float py = yBase - mag * h;
        if (i == 0) curve.startNewSubPath(px, py);
        else        curve.lineTo(px, py);
    }

    juce::Path fillPath = curve;
    fillPath.lineTo(x0 + static_cast<float>(w), yBase);
    fillPath.lineTo(x0, yBase);
    fillPath.closeSubPath();

    g.setColour(kAccentFilter.withAlpha(0.18f));
    g.fillPath(fillPath);

    g.setColour(kAccentFilter);
    g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void AbakosPROAudioProcessorEditor::drawAdsEnvelopePreview(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    auto rf = bounds.toFloat();
    g.setColour(kTrackDark);
    g.fillRoundedRectangle(rf, 4.0f);
    g.setColour(kCardBorder);
    g.drawRoundedRectangle(rf, 4.0f, 1.0f);

    const auto& eng = processorRef_.getSynthEngine();
    auto drawAdsBar = [&g](juce::Rectangle<float> area, float a, float d, float s, juce::Colour col)
    {
        juce::Path p;
        float x0 = area.getX() + 6.0f;
        float w  = area.getWidth() - 12.0f;
        float yB = area.getBottom() - 4.0f;
        float yT = area.getY() + 4.0f;
        float h  = yB - yT;

        float xA = x0 + (0.04f + 0.30f * (a * 0.01f)) * w;
        float xD = xA + (0.08f + 0.34f * (d * 0.01f)) * w;
        float yS = yB - (s * 0.01f) * h;

        p.startNewSubPath(x0, yB);
        p.lineTo(xA, yT);
        p.lineTo(xD, yS);
        p.lineTo(x0 + w, yS);

        juce::Path fill = p;
        fill.lineTo(x0 + w, yB);
        fill.closeSubPath();
        g.setColour(col.withAlpha(0.14f));
        g.fillPath(fill);

        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(1.8f));
    };

    auto leftEnv  = rf.withWidth(rf.getWidth() * 0.48f);
    auto rightEnv = rf.withLeft(rf.getX() + rf.getWidth() * 0.52f);

    drawAdsBar(leftEnv,
               eng.getParameterValue(abakos::kAds1Attack),
               eng.getParameterValue(abakos::kAds1Decay),
               eng.getParameterValue(abakos::kAds1Sustain),
               kAccentEnv);
    drawAdsBar(rightEnv,
               eng.getParameterValue(abakos::kAds2Attack),
               eng.getParameterValue(abakos::kAds2Decay),
               eng.getParameterValue(abakos::kAds2Sustain),
               kAccentFilter);
}

void AbakosPROAudioProcessorEditor::drawAdsrEnvelopePreview(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    auto rf = bounds.toFloat();
    g.setColour(kTrackDark);
    g.fillRoundedRectangle(rf, 4.0f);
    g.setColour(kCardBorder);
    g.drawRoundedRectangle(rf, 4.0f, 1.0f);

    const auto& eng = processorRef_.getSynthEngine();
    auto drawAdsrBar = [&g](juce::Rectangle<float> area, float a, float d, float s, float r, juce::Colour col)
    {
        juce::Path p;
        float x0 = area.getX() + 6.0f;
        float w  = area.getWidth() - 12.0f;
        float yB = area.getBottom() - 4.0f;
        float yT = area.getY() + 4.0f;
        float h  = yB - yT;

        float xA = x0 + (0.04f + 0.24f * (a * 0.01f)) * w;
        float xD = xA + (0.06f + 0.24f * (d * 0.01f)) * w;
        float xS = x0 + 0.72f * w;
        float xR = xS + (0.05f + 0.23f * (r * 0.01f)) * w;
        float yS = yB - (s * 0.01f) * h;

        p.startNewSubPath(x0, yB);
        p.lineTo(xA, yT);
        p.lineTo(xD, yS);
        p.lineTo(xS, yS);
        p.lineTo(xR, yB);

        juce::Path fill = p;
        fill.closeSubPath();
        g.setColour(col.withAlpha(0.14f));
        g.fillPath(fill);

        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(1.8f));
    };

    auto leftEnv  = rf.withWidth(rf.getWidth() * 0.48f);
    auto rightEnv = rf.withLeft(rf.getX() + rf.getWidth() * 0.52f);

    drawAdsrBar(leftEnv,
                eng.getParameterValue(abakos::kAdsr1Attack),
                eng.getParameterValue(abakos::kAdsr1Decay),
                eng.getParameterValue(abakos::kAdsr1Sustain),
                eng.getParameterValue(abakos::kAdsr1Release),
                kAccentEnv);
    drawAdsrBar(rightEnv,
                eng.getParameterValue(abakos::kAdsr2Attack),
                eng.getParameterValue(abakos::kAdsr2Decay),
                eng.getParameterValue(abakos::kAdsr2Sustain),
                eng.getParameterValue(abakos::kAdsr2Release),
                kAccentOsc);
}

void AbakosPROAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Main Chassis Gradient
    juce::ColourGradient bgGrad(kBgTop, 0.0f, 0.0f, kBgBottom, 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Top Header Bar (y = 0..54)
    g.setColour(juce::Colour(0xff131b26));
    g.fillRect(0, 0, getWidth(), 54);
    g.setColour(kCardBorder);
    g.drawLine(0.0f, 54.0f, static_cast<float>(getWidth()), 54.0f, 1.0f);

    // Brand Title
    g.setColour(kAccentOsc);
    g.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
    g.drawText("ABBAKHAOS", 16, 8, 200, 22, juce::Justification::centredLeft, false);

    g.setColour(kTextMuted);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText("GLITCHYGLOOMY // HERCs LUNAR DSP", 16, 29, 215, 16, juce::Justification::centredLeft, false);

    // Preset LCD Display
    g.setColour(kTrackDark);
    g.fillRoundedRectangle(presetLcdBounds_.toFloat(), 5.0f);
    g.setColour(kAccentOsc.withAlpha(0.55f));
    g.drawRoundedRectangle(presetLcdBounds_.toFloat(), 5.0f, 1.2f);

    static const char* kBankChars[5] = { "A", "B", "C", "D", "E" };
    int bIdx = std::max(0, std::min(4, processorRef_.getCurrentBank()));
    char progStr[24];
    std::snprintf(progStr, sizeof(progStr), "%s-%02X  ", kBankChars[bIdx], processorRef_.getCurrentProgram());

    g.setColour(kAccentOsc);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(juce::String(progStr) + processorRef_.getCurrentPresetName(),
               presetLcdBounds_.reduced(8, 0), juce::Justification::centredLeft, true);

    g.setColour(kTextMuted);
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText(processorRef_.getCurrentPresetCategory(),
               presetLcdBounds_.reduced(8, 0), juce::Justification::centredRight, true);

    // Draw Module Cards
    // Row 1
    drawCard(g, { 14,  62, 316, 158 }, "OSC 1 // MASTER WAVETABLE", kAccentOsc);
    drawCard(g, { 338, 62, 348, 158 }, "OSC 2 // SYNC WAVETABLE",   kAccentOsc);
    drawCard(g, { 694, 62, 272, 158 }, "NOISE & MASTER PITCH",      kAccentOsc);

    // Row 2
    drawCard(g, { 14,  228, 372, 158 }, "CHAMBERLIN 2-POLE SVF FILTER",            kAccentFilter);
    drawCard(g, { 394, 228, 572, 158 }, "MODULATION ENVELOPES // ADS 1 (OSC) & ADS 2 (FILTER)", kAccentEnv);

    drawFilterResponse(g, filterGraphBounds_);
    drawAdsEnvelopePreview(g, adsGraphBounds_);

    // Row 3
    drawCard(g, { 14,  394, 458, 154 }, "AMPLITUDE ENVELOPES // ADSR 1 & ADSR 2", kAccentEnv);
    drawCard(g, { 480, 394, 246, 154 }, "LFO & VOICE ARCHITECTURE",               kAccentMod);
    drawCard(g, { 734, 394, 232, 154 }, "STEREO DELAY & OUTPUT",                  kAccentFx);

    drawAdsrEnvelopePreview(g, adsrGraphBounds_);

    // Row 4
    drawCard(g, { 14,  556, 208, 92 },  "24-PPQN ARPEGGIATOR",                    kAccentMod);
    drawCard(g, { 226, 556, 740, 92 },  "",                                       kAccentOsc);
}

void AbakosPROAudioProcessorEditor::resized()
{
}
