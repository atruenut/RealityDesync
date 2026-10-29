#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "Presets.h"

namespace rdui {

namespace col {
inline const juce::Colour bg{0xff0a0c12};
inline const juce::Colour panel{0xff131722};
inline const juce::Colour panelEdge{0xff262d40};
inline const juce::Colour text{0xffdce2f0};
inline const juce::Colour dim{0xff7a859e};
inline const juce::Colour left{0xff35d8ff};
inline const juce::Colour right{0xffff4fd0};
inline const juce::Colour amber{0xffffb340};
inline const juce::Colour lime{0xffa4ff70};
}  // namespace col

class LookAndFeel : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle,
                          juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(13.0f)); }
    juce::Font getLabelFont(juce::Label&) override { return juce::Font(juce::FontOptions(12.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font(juce::FontOptions(14.0f)); }
};

// A rotary knob with a name label and a value read-out, bound to a parameter.
class Knob : public juce::Component {
public:
    Knob(juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, juce::Colour accent, int w = 70, int h = 84);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};

class Combo : public juce::Component {
public:
    Combo(juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, int w, int h = 56);
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
};

class StepSlider : public juce::Component {
public:
    StepSlider(juce::AudioProcessorValueTreeState&, const juce::String& paramId, int index, int w = 30, int h = 84);
    void resized() override;
    void paint(juce::Graphics&) override;
    juce::Slider slider;

private:
    int idx;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};

class Panel : public juce::Component {
public:
    Panel(const juce::String& title, juce::Colour accent) : title_(title), accent_(accent) {}
    void addRow(std::vector<juce::Component*> row) {
        for (auto* c : row) addAndMakeVisible(c);
        rows_.push_back(std::move(row));
        resized();  // lay out immediately: rows can be added after the panel already has bounds
    }
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String title_;
    juce::Colour accent_;
    std::vector<std::vector<juce::Component*>> rows_;
};

// Live picture of what the engine is doing: lanes, slice clocks and the alignment ring.
class Scope : public juce::Component, private juce::Timer {
public:
    explicit Scope(RealityDesyncProcessor&);
    ~Scope() override { stopTimer(); }
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    RealityDesyncProcessor& proc;

    struct Snap {
        float drift = 0, cycle = 0, barsToSnap = -1, bpm = 120;
        float delayMs[2] = {0, 0}, pitch[2] = {0, 0};
        float bandPhase[2][rd::kMaxBands] = {};
        bool synced = false;
    } s;
    float flash[2][rd::kMaxBands] = {};
    uint32_t lastTrig[2][rd::kMaxBands] = {};
    uint32_t lastSnapCount = 0;
    float snapFlash = 0;
    float trail[2][96] = {};
    int trailPos = 0;
    float phaseAnim = 0;
};

}  // namespace rdui

class RealityDesyncEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit RealityDesyncEditor(RealityDesyncProcessor&);
    ~RealityDesyncEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildPresetMenu();
    void presetChosen();
    void stepPreset(int dir);
    void promptSave();
    void setPresetLabel(const juce::String& name, const juce::String& desc);

    RealityDesyncProcessor& proc;
    rdui::LookAndFeel laf;

    // header
    juce::ComboBox presetBox;
    juce::TextButton prevBtn{"<"}, nextBtn{">"}, saveBtn{"Save"}, snapBtn{"SNAP"};
    juce::Label descLabel;
    std::vector<int> menuToFactory;  // combo item id -> factory index (or -1)
    std::vector<juce::File> userFiles;
    int currentItemId = 0;
    bool ignorePresetChange = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> snapAttach;

    rdui::Scope scope;

    // controls (heap-allocated so the panels can hold raw pointers)
    std::vector<std::unique_ptr<juce::Component>> owned;
    rdui::Panel pTime{"TIME SPLIT", rdui::col::left}, pPitch{"PITCH FRACTURE", rdui::col::right}, pDesync{"DESYNC & SNAP", rdui::col::amber},
        pSlices{"SPECTRAL SLICES", rdui::col::lime}, pMotion{"MOTION", rdui::col::right}, pFilter{"FILTER", rdui::col::left},
        pOut{"OUTPUT", rdui::col::amber};
    juce::Label status;

    template <typename T, typename... A>
    T* make(A&&... a) {
        auto p = std::make_unique<T>(std::forward<A>(a)...);
        T* raw = p.get();
        owned.push_back(std::move(p));
        return raw;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RealityDesyncEditor)
};
