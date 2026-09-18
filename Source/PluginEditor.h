#pragma once

#include "PluginProcessor.h"
#include "UI/Components.h"

class HypernovaAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget,
                                      private juce::Timer
{
public:
    explicit HypernovaAudioProcessorEditor (HypernovaAudioProcessor&);
    ~HypernovaAudioProcessorEditor() override;

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    // Drop .hnpreset files or pack folders anywhere on the window to import them.
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void setDeckPage (int p) { showDeckPage (p); }
    void setSpaceMode (int m) { spaceMode.setSelected (m); space.setMode ((ab::ui::SoundSpace::Mode) m); }

    static constexpr int baseWidth = 1280, baseHeight = 914;

private:
    void timerCallback() override;
    void layoutCanvas();
    void paintCanvas (juce::Graphics&);
    void showPresetMenu();
    void showSaveDialog();
    void refreshPresetInfo();
    void exportCurrent();
    void importWithChooser();
    void importAndReport (const juce::Array<juce::File>&);
    void showMessage (const juce::String&);

    class Canvas : public juce::Component
    {
    public:
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    // Clickable preset name plate.
    class PresetPlate : public juce::Button
    {
    public:
        PresetPlate() : juce::Button ("preset") {}
        juce::String name, category;
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    ab::ui::Knob& knob (const juce::String& id, const juce::String& label, juce::Colour c, juce::Rectangle<int> bounds, int size = 42,
                        juce::Component* parent = nullptr);
    juce::ComboBox& combo (const juce::String& id, const juce::StringArray& items, juce::Rectangle<int> bounds, juce::Component* parent = nullptr);
    template <typename ButtonType>
    ButtonType& toggle (std::unique_ptr<ButtonType> b, const juce::String& id, juce::Rectangle<int> bounds, const juce::String& tip,
                        juce::Component* parent = nullptr);
    void layoutModPage();
    void layoutFxPage();
    void layoutPlayPage();
    void showDeckPage (int page);

    // One page of the tabbed deck along the bottom. Paints its own group captions and dividers.
    struct Caption { juce::Rectangle<int> area; juce::String text; juce::Colour colour; bool divider; };
    class DeckPage : public juce::Component
    {
    public:
        std::vector<Caption> captions;
        void paint (juce::Graphics& g) override;
    };

    HypernovaAudioProcessor& processor;
    ab::ui::LookAndFeel lookAndFeel;
    Canvas canvas;

    ab::ui::WavetableView viewA, viewB;
    ab::ui::SoundSpace space;
    ab::ui::Segmented spaceMode { { "SPECTRUM", "ORBIT" }, ab::ui::Palette::oscA };
    ab::ui::FilterView filterView;
    ab::ui::EnvView ampView, modView;
    ab::ui::LfoView lfoView1, lfoView2;
    std::vector<std::unique_ptr<ab::ui::ModRow>> modRows;
    std::array<DeckPage, 3> pages;
    ab::ui::Segmented deckTabs { { "MODULATION", "EFFECTS", "PLAY" }, ab::ui::Palette::mod };

    std::vector<std::unique_ptr<ab::ui::Knob>> knobs;
    std::array<ab::ui::Knob*, 4> macroKnobs {};
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::Button>> toggles;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    PresetPlate presetPlate;
    ab::ui::IconButton prevButton { ab::ui::IconButton::Prev }, nextButton { ab::ui::IconButton::Next },
                       diceButton { ab::ui::IconButton::Dice, ab::ui::Colours::warm }, saveButton { ab::ui::IconButton::Save };

    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::AlertWindow> saveWindow;

    int lastPresetVersion = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String message, lastAuthor;
    juce::int64 messageUntil = 0;
    bool dragHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HypernovaAudioProcessorEditor)
};
