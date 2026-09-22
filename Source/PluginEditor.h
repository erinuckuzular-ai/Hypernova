#pragma once

#include "PluginProcessor.h"
#include <map>
#include "UI/Components.h"
#include "UI/Cosmos.h"
#include "UI/PresetBrowser.h"
#include "UI/Updater.h"
#include "UI/ToolWidgets.h"
#include "UI/SamplerView.h"
#include "UI/FxRack.h"
#include "UI/SourcesView.h"
#include "UI/LowEndView.h"

class HypernovaAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget,
                                      public juce::DragAndDropContainer,
                                      private juce::Timer,
                                      public ab::ui::DockOverlay::Host
{
public:
    explicit HypernovaAudioProcessorEditor (HypernovaAudioProcessor&);
    ~HypernovaAudioProcessorEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;   // only used while the window is being resized (see canvasShot)
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

    // Widgets and workspaces (layout only; the sound is never touched).
    ab::ui::Widget* findWidget (const juce::String& id) const;
    void loadWorkspace (const juce::String& name, bool recordHistory);
    void setLayoutEditing (bool editing);
    bool isLayoutEditing() const { return layoutEditing; }
    juce::ValueTree captureLayout() const;
    void applyLayout (const juce::ValueTree& layout);
    void applyTheme();
    juce::String addWidgetType (const juce::String& type);   // from the library: returns the new widget's id
    void hideWidget (const juce::String& id);                // off the screen; the sound is untouched
    void replaceWidget (const juce::String& id, const juce::String& type);
    juce::String duplicateWidget (const juce::String& id);
    void toggleCollapse (const juce::String& id);
    void toggleMaximise (const juce::String& id);
    void activateWidget (const juce::String& id);
    void moveWidget (const juce::String& id, const juce::String& targetId, ab::ui::dock::Zone zone);
    void pinParameter (const juce::String& paramId, bool pin);
    bool isPinned (const juce::String& paramId) const;
    ab::ui::dock::Tree& layoutTree() { return tree; }
    juce::String maximisedWidget() const { return maximisedId; }
    void undoLayout();
    void redoLayout();
    void relayoutWidgets (bool animate);
    juce::Rectangle<int> layoutArea() const;
    void setLibraryOpen (bool open);
    void finishMotion() { springs.finish(); for (auto& w : widgets) w->thaw(); } // lands every moving panel and lays it out now (tests)
    void addOscillator();                                     // switches on the next free oscillator and shows its panel
    void removeOscillator (int osc);                          // switches it off and hides its panel
    ab::ui::SourcesView& sourcesView() { return sources; }
    ab::ui::ToolContent* toolFor (const juce::String& id) const { auto it = tools.find (id); return it != tools.end() ? it->second.get() : nullptr; }

    static constexpr int baseWidth = 1280, baseHeight = 986;
    static juce::Rectangle<int> macroTray() { return { 838, 8, 342, 76 }; } // painted, so layout checks need to know it

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
    void colourKeyboard();
    void addLookMenu (juce::PopupMenu&);
    bool handleLookMenu (int result);
    void showColourPicker();
    std::unique_ptr<juce::ChangeListener> colourListener;
    // Modulation by drag and drop: chips carry a source, knobs receive it.
    void assignMod (const juce::String& dragDescription, const juce::String& paramId);
    void showModMenu (const juce::String& paramId);
    void wireModDepth (ab::ui::Knob&);
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

    ab::ui::Knob& knob (const juce::String& id, const juce::String& label, ab::ui::ThemeColour c, juce::Rectangle<int> bounds, int size = 42,
                        juce::Component* parent = nullptr);
    juce::ComboBox& combo (const juce::String& id, const juce::StringArray& items, juce::Rectangle<int> bounds, juce::Component* parent = nullptr);
    template <typename ButtonType>
    ButtonType& toggle (std::unique_ptr<ButtonType> b, const juce::String& id, juce::Rectangle<int> bounds, const juce::String& tip,
                        juce::Component* parent = nullptr);
    ab::ui::Widget& makeWidget (const juce::String& id, const juce::String& type, const juce::String& title, ab::ui::ThemeColour c,
                                juce::Rectangle<int> design, int titleX = 14, int headerH = 40);
    ab::ui::Widget* createTool (const juce::String& type, const juce::String& id, juce::ValueTree config);
    void destroyTool (const juce::String& id);
    juce::String newToolId (const juce::String& type) const;
    void ensureWidget (const juce::String& type);
    static bool isMultiType (const juce::String& type);
    ab::ui::ToolServices toolServices();
    ab::ui::PinboardTool* firstPinboard() const;
    void tickTools (bool sounding);
    void wireWidget (ab::ui::Widget&);
    void showWidgetMenu (ab::ui::Widget&, juce::Point<int> screenPos);
    std::vector<ab::ui::WidgetLibrary::Entry> libraryEntries() const;
    juce::ValueTree defaultLayout (const juce::String& name) const;
    void layoutChanged();                         // after every structural edit: history + autosave
    void setupEditBar();
    void showWorkspaceMenu();
    static ab::ui::ThemeColour modSourceColourFor (int src);

    // DockOverlay::Host
    ab::ui::dock::Tree& dockTree() override { return tree; }
    juce::Rectangle<int> dockArea() const override { return layoutArea(); }
    ab::ui::Widget* widgetById (const juce::String& id) const override { return findWidget (id); }
    ab::ui::dock::MinSize dockMinSize() const override;
    ab::ui::dock::MinSize dockRoomySize() const;
    void dockRelayout (bool animate) override { relayoutWidgets (animate); }
    void dockCommit (const juce::String& what) override;
    void dockDrop (const juce::String& widgetOrType, bool isNewType, const ab::ui::dock::Drop&, juce::Rectangle<int> landingFrom) override;
    void dockMaximise (const juce::String& widgetId) override { toggleMaximise (widgetId); }
    void dockDragging (bool active) override
    {
        if (active == dragOverLayout) return;
        dragOverLayout = active;                       // the live views wait while something is being dragged
        for (auto& w : widgets) w->setDragging (active);
    }
    void dockButton (ab::ui::Widget&, int button, juce::Point<int> screenPos) override;
    void dockActivate (const juce::String& widgetId) override;

    void layoutModPage();
    void layoutPlayPage();
    void showDeckPage (int page);

    // One page of the tabbed deck along the bottom. Paints its own group captions and dividers.
    struct Caption { juce::Rectangle<int> area; juce::String text; ab::ui::ThemeColour colour { ab::ui::SlotTextDim }; bool divider; };
    class DeckPage : public juce::Component
    {
    public:
        std::vector<Caption> captions;
        ab::ui::Spread spread;
        void paint (juce::Graphics& g) override;
        void resized() override { spread.apply (getWidth(), getHeight()); }
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
    ab::ui::InvisibleButton logoButton; // clicking the painted logo sets the black hole off
    float logoFlare = 0.0f;
    const juce::Point<float> logoHole { 48.0f, 44.0f };

    ab::ui::WavetableView viewA, viewB;
    std::vector<std::unique_ptr<ab::ui::WavetableView>> extraViews; // oscillators C to H
    ab::ui::SourcesView sources { processor };
    juce::TextButton addOscButton { "+ OSC" };
    ab::ui::SoundSpace space;
    ab::ui::SamplerView samplerView;
    ab::ui::EffectsRack rack;
    juce::TextButton rackAddButton { "+ ADD" };
    void buildRackModules();
    void showAddEffectMenu (juce::Component* target, juce::Point<int> screenPos);
    void showRackModuleMenu (int fxId, juce::Point<int> screenPos);   // right-click a unit in the rack
    ab::ui::LowEndView lowEndView;
    juce::TextButton phoneButton { "PHONE CHECK" };
    juce::TextButton chainButton { "CHAINS" };
    void showChainMenu (juce::Component* target, juce::Point<int> screenPos);
    juce::TextButton sampleButton { "LOAD" };
    void showSampleMenu();
    void chooseSample();
    std::vector<std::unique_ptr<ab::ui::Widget>> widgets;
    ab::ui::Widget* spaceWidget = nullptr;
    ab::ui::dock::Tree tree;
    std::map<juce::String, std::unique_ptr<ab::ui::ToolContent>> tools;
    std::unique_ptr<ab::ui::DockOverlay> overlay;
    std::unique_ptr<ab::ui::WidgetLibrary> library;
    juce::String maximisedId;
    static constexpr int libraryWidth = 316;
    juce::String landingId;             // a widget just dropped: it slides from where it was let go
    juce::Rectangle<int> landingFrom;
    juce::Point<float> landingVelocity;
    bool dragOverLayout = false;
    // While the window is being dragged bigger or smaller, a picture of the editor is stretched instead of
    // laying out and redrawing everything at every size; the real thing comes back once it settles.
    juce::Image canvasShot;
    float lastCanvasScale = 0.0f;
    juce::uint32 lastCanvasResize = 0;
    bool canvasThawPending = false, everPainted = false;
    void scheduleCanvasThaw();
    ab::ui::motion::BoundsSprings springs; // every panel move: interruptible, velocity-aware, no fixed durations
    bool layoutEditing = false;
    juce::String workspaceName;
    std::vector<juce::ValueTree> layoutHistory;
    int layoutHistoryIndex = -1;
    ab::ui::IconButton layoutButton { ab::ui::IconButton::Layout };
    class EditBar : public juce::Component
    {
    public:
        juce::TextButton add { "+ ADD WIDGET" }, workspace { "WORKSPACE" }, reset { "RESET" }, done { "DONE" };
        ab::ui::IconButton undo { ab::ui::IconButton::Undo }, redo { ab::ui::IconButton::Redo };
        EditBar()
        {
            for (auto* b : std::initializer_list<juce::Component*> { &add, &workspace, &undo, &redo, &reset, &done }) addAndMakeVisible (b);
            undo.setTooltip ("Undo the last layout change");
            redo.setTooltip ("Redo");
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (0, 6);
            add.setBounds (r.removeFromLeft (112)); r.removeFromLeft (6);
            workspace.setBounds (r.removeFromLeft (150)); r.removeFromLeft (6);
            undo.setBounds (r.removeFromLeft (34)); r.removeFromLeft (4);
            redo.setBounds (r.removeFromLeft (34)); r.removeFromLeft (6);
            done.setBounds (r.removeFromRight (74)); r.removeFromRight (6);
            reset.setBounds (r);
        }
        void paintOverChildren (juce::Graphics& g) override
        {
            // The workspace button's menu chevron, drawn rather than typed.
            auto b = workspace.getBounds().toFloat();
            const juce::Point<float> c (b.getRight() - 14.0f, b.getCentreY());
            juce::Path p;
            p.startNewSubPath (c.x - 4.0f, c.y - 2.0f);
            p.lineTo (c.x, c.y + 2.0f);
            p.lineTo (c.x + 4.0f, c.y - 2.0f);
            g.setColour (findColour (juce::TextButton::textColourOffId));
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };
    EditBar editBar;
    ab::ui::IconButton expandButton { ab::ui::IconButton::Expand }, popOutButton { ab::ui::IconButton::PopOut };

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
    ab::ui::Segmented spaceMode { { "SPECTRUM", "ORBIT" }, ab::ui::Palette::oscA };
    ab::ui::FilterView filterView;
    ab::ui::EnvView ampView, modView;
    ab::ui::LfoView lfoView1, lfoView2;
    std::vector<std::unique_ptr<ab::ui::LfoView>> extraLfoViews; // LFO 3 and 4
    std::vector<std::unique_ptr<ab::ui::ModRow>> modRows;
    std::vector<std::unique_ptr<ab::ui::ModChip>> modChips;
    int hoveredModSource = -1, lastParameterChanges = -1, viewTick = 0;
    std::array<DeckPage, 4> pages;

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
    ab::ui::CompareSwitch compare;
    void showCompareMenu();
    ab::ui::IconButton prevButton { ab::ui::IconButton::Prev }, nextButton { ab::ui::IconButton::Next },
                       diceButton { ab::ui::IconButton::Dice, ab::ui::Colours::warm }, saveButton { ab::ui::IconButton::Save },
                       undoButton { ab::ui::IconButton::Undo }, redoButton { ab::ui::IconButton::Redo }, gearButton { ab::ui::IconButton::Gear };

    ab::ui::DeepKeyboard keyboard;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::AlertWindow> saveWindow;

    int lastPresetVersion = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String message, lastAuthor;
    juce::int64 messageUntil = 0;
    bool dragHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HypernovaAudioProcessorEditor)
};
