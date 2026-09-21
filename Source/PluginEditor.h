#pragma once

#include "PluginProcessor.h"
#include "UI/Components.h"
#include "UI/Cosmos.h"
#include "UI/PresetBrowser.h"
#include "UI/Updater.h"

class HypernovaAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget,
                                      public juce::DragAndDropContainer,
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
    void setBrowserOpen (bool open);
    void setSpaceMode (int m) { spaceMode.setSelected (m); space.setMode ((ab::ui::SoundSpace::Mode) m); }
    void setSpaceExpanded (bool e);

    static constexpr int baseWidth = 1280, baseHeight = 914;

private:
    void timerCallback() override;
    void layoutCanvas();
    void paintCanvas (juce::Graphics&);
    void paintStatic (juce::Graphics&);
    void paintDynamic (juce::Graphics&);
    bool cosmosOnGpu() const { return cosmosRenderer != nullptr && cosmosRenderer->isReady() && glContext.isAttached(); }
    void showPresetMenu();
    void showSaveDialog();
    void refreshPresetInfo();
    void exportCurrent();
    void importWithChooser();
    void importAndReport (const juce::Array<juce::File>&);
    void showMessage (const juce::String&);
    void showDiceMenu();
    void showSettingsMenu();
    // Modulation by drag and drop: chips carry a source, knobs receive it.
    void assignMod (const juce::String& dragDescription, const juce::String& paramId);
    void showModMenu (const juce::String& paramId);
    ab::ui::Knob::ModInfo modInfoFor (const juce::String& paramId) const;
    void applyScale (int percent);
    bool keyPressed (const juce::KeyPress&) override;

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
    void layoutMoreFxPage();
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

    // Backdrop: GPU shader when OpenGL is available, cached CPU picture otherwise.
    juce::OpenGLContext glContext;
    ab::ui::CosmosState cosmos;
    std::unique_ptr<ab::ui::CosmosRenderer> cosmosRenderer;
    juce::Image fallbackBackdrop, staticLayer;
    bool lastGpu = false;
    int idleTicks = 0, frameTick = 0, lastReadout = -2;
    bool staticFramePainted = false;
    int editQuietTicks = 0, lastActionCount = 0;
    static constexpr float logoHoleRadius = 12.5f;
    juce::TextButton logoButton; // invisible: clicking the logo sets the black hole off
    float logoFlare = 0.0f;
    const juce::Point<float> logoHole { 48.0f, 44.0f };

    ab::ui::WavetableView viewA, viewB;
    ab::ui::SoundSpace space;
    ab::ui::IconButton expandButton { ab::ui::IconButton::Expand }, popOutButton { ab::ui::IconButton::PopOut };
    bool spaceExpanded = false;

    // A torn-off Sound Space in its own resizable window, for a second screen.
    class SpaceWindow : public juce::DocumentWindow
    {
    public:
        SpaceWindow (HypernovaAudioProcessor& p, int mode, std::function<void()> onGone);
        void closeButtonPressed() override { if (whenClosed) whenClosed(); }
        ab::ui::SoundSpace& view() { return *space; }
    private:
        std::unique_ptr<ab::ui::SoundSpace> space;
        std::function<void()> whenClosed;
    };
    std::unique_ptr<SpaceWindow> spaceWindow;
    ab::ui::Segmented spaceMode { { "SPECTRUM", "ORBIT", "STEREO" }, ab::ui::Palette::oscA };
    ab::ui::FilterView filterView;
    ab::ui::EnvView ampView, modView;
    ab::ui::LfoView lfoView1, lfoView2;
    std::vector<std::unique_ptr<ab::ui::ModRow>> modRows;
    std::vector<std::unique_ptr<ab::ui::ModChip>> modChips;
    std::array<DeckPage, 4> pages;
    ab::ui::Segmented deckTabs { { "MODULATION", "EFFECTS", "MORE FX", "PLAY" }, ab::ui::Palette::mod };

    std::vector<std::unique_ptr<ab::ui::Knob>> knobs;
    std::array<ab::ui::Knob*, 4> macroKnobs {};
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::Button>> toggles;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    PresetPlate presetPlate;
    ab::ui::PresetBrowser browser { processor };
    ab::ui::Updater updater;
    ab::ui::UpdateBanner updateBanner;
    ab::ui::IconButton prevButton { ab::ui::IconButton::Prev }, nextButton { ab::ui::IconButton::Next },
                       diceButton { ab::ui::IconButton::Dice, ab::ui::Colours::warm }, saveButton { ab::ui::IconButton::Save },
                       undoButton { ab::ui::IconButton::Undo }, redoButton { ab::ui::IconButton::Redo }, gearButton { ab::ui::IconButton::Gear };

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
