#include "PluginEditor.h"
#include <map>

using namespace ab;
using namespace ab::ui;

namespace
{
    // Base-canvas layout
    // Rows breathe: every panel is taller than the content it was designed around, and at() spreads a
    // panel's contents over the extra height instead of leaving it all at the bottom.
    constexpr int topRowH = 378, midRowH = 212, topRowDesign = 350, midRowDesign = 186;
    const juce::Rectangle<int> oscAPanel   { 24, 96, 400, topRowH };
    const juce::Rectangle<int> oscBPanel   { 436, 96, 400, topRowH };
    const juce::Rectangle<int> spacePanel  { 848, 96, 408, topRowH };
    const juce::Rectangle<int> subPanel    { 24, 488, 240, midRowH };
    const juce::Rectangle<int> voicePanel  { 276, 488, 256, midRowH };
    const juce::Rectangle<int> filterPanel { 544, 488, 332, midRowH };
    const juce::Rectangle<int> envPanel    { 888, 488, 368, midRowH };
    const juce::Rectangle<int> deckPanel   { 24, 714, 1232, 190 };
    const juce::Rectangle<int> deckContent { 24, 760, 1232, 140 };
    const juce::Rectangle<int> keysArea    { 24, 918, 1232, 56 };

    const juce::Rectangle<int> messageArea { 334, 72, 496, 16 };

    // Where a control sits inside its widget (widget-relative, in the widget's design size). Rows breathe: the
    // design heights are taller than the layouts they came from, so y is spread over the extra height.
    juce::Rectangle<int> at (const juce::Rectangle<int>& panel, int x, int y, int w, int h)
    {
        const float design = panel.getHeight() == topRowH ? (float) topRowDesign : panel.getHeight() == midRowH ? (float) midRowDesign
                                                                                                           : (float) panel.getHeight();
        const float k = (float) panel.getHeight() / design;
        return { x, juce::roundToInt ((float) y * k), w, h };
    }

    // The work area widgets live in: under the header, above the keyboard.
    const juce::Rectangle<int> workArea { 24, 96, 1232, 810 };
    constexpr int gutter = 12, snapGrid = 4, snapReach = 10;
}

//==============================================================================
void HypernovaAudioProcessorEditor::PresetPlate::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::inset);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (over ? Colours::lineHi : Colours::line);
    g.drawRoundedRectangle (r, 9.0f, 1.0f);

    auto content = r.reduced (14, 0);
    g.setColour (Colours::textDim);
    g.setFont (font (9.0f, true).withExtraKerningFactor (0.22f));
    g.drawText (category.toUpperCase(), content.removeFromTop (r.getHeight() * 0.42f).withTrimmedTop (4), juce::Justification::bottomLeft, false);
    g.setColour (Colours::text);
    g.setFont (font (16.0f, true));
    g.drawText (name, content.withTrimmedBottom (4), juce::Justification::centredLeft, true);

    juce::Path chevron;
    const float cx = r.getRight() - 16.0f, cy = r.getCentreY();
    chevron.startNewSubPath (cx - 4, cy - 2);
    chevron.lineTo (cx, cy + 2);
    chevron.lineTo (cx + 4, cy - 2);
    g.setColour (Colours::textDim);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// Colour per modulation source, so a knob's ring says where its movement comes from.
static ThemeColour modSourceColour (int src)
{
    switch (src)
    {
        case 1: case 2: return Palette::lfo;   // LFO 1 / 2
        case 3:         return Palette::env;   // mod envelope
        case 4:         return Palette::sub;   // velocity
        case 11:        return Palette::oscB;  // random
        default:        return Palette::mod;   // macros, mod wheel, note
    }
}

ThemeColour HypernovaAudioProcessorEditor::modSourceColourFor (int src) { return modSourceColour (src); }

// Every panel is a widget. Controls are placed in widget coordinates at the panel's design size.
Widget& HypernovaAudioProcessorEditor::makeWidget (const juce::String& id, const juce::String& type, const juce::String& title, ThemeColour c,
                                                   juce::Rectangle<int> design, int titleX, int headerH)
{
    widgets.push_back (std::make_unique<Widget> (id, type, title, c, design.getWidth(), design.getHeight(), titleX, headerH));
    auto& w = *widgets.back();
    canvas.addChildComponent (w);
    w.setBounds (design);
    wireWidget (w);
    return w;
}

Knob& HypernovaAudioProcessorEditor::knob (const juce::String& id, const juce::String& label, ThemeColour c, juce::Rectangle<int> bounds, int size,
                                            juce::Component* parent)
{
    knobs.push_back (std::make_unique<Knob> (processor.apvts, id, label, c, size));
    auto& k = *knobs.back();
    k.modLookup = [this] (const juce::String& p) { return modInfoFor (p); };
    k.onModDrop = [this] (const juce::String& src, const juce::String& p) { assignMod (src, p); };
    k.onModMenu = [this] (const juce::String& p) { showModMenu (p); };
    (parent != nullptr ? *parent : canvas).addAndMakeVisible (k);
    k.setBounds (bounds);
    return k;
}

juce::ComboBox& HypernovaAudioProcessorEditor::combo (const juce::String& id, const juce::StringArray& items, juce::Rectangle<int> bounds,
                                                      juce::Component* parent)
{
    combos.push_back (std::make_unique<juce::ComboBox>());
    auto& c = *combos.back();
    c.addItemList (items, 1);
    comboAttachments.push_back (std::make_unique<ComboAttachment> (processor.apvts, id, c));
    if (auto* p = processor.apvts.getParameter (id)) c.setTooltip (p->getName (64));
    (parent != nullptr ? *parent : canvas).addAndMakeVisible (c);
    c.setBounds (bounds);
    return c;
}

template <typename ButtonType>
ButtonType& HypernovaAudioProcessorEditor::toggle (std::unique_ptr<ButtonType> b, const juce::String& id, juce::Rectangle<int> bounds, const juce::String& tip,
                                                  juce::Component* parent)
{
    auto& ref = *b;
    ref.setTooltip (tip);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (processor.apvts, id, ref));
    (parent != nullptr ? *parent : canvas).addAndMakeVisible (ref);
    ref.setBounds (bounds);
    toggles.push_back (std::move (b));
    return ref;
}

//==============================================================================
HypernovaAudioProcessorEditor::HypernovaAudioProcessorEditor (HypernovaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      viewA (p, 0, Palette::oscA), viewB (p, 1, Palette::oscB), space (p), samplerView (p), filterView (p),
      ampView (p, "amp", Palette::env, true), modView (p, "mod", Palette::lfo, false),
      lfoView1 (p, 0), lfoView2 (p, 1),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (false);
    addAndMakeVisible (canvas);
    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };

    viewA.onMessage = [this] (const juce::String& m) { showMessage (m); };
    viewB.onMessage = [this] (const juce::String& m) { showMessage (m); };
    for (auto* c : std::initializer_list<juce::Component*> { &viewA, &viewB, &space, &spaceMode, &filterView, &ampView, &modView, &lfoView1, &lfoView2 })
        canvas.addAndMakeVisible (c);

    layoutCanvas();

    // Header
    presetPlate.setTooltip ("Browse sounds (right-click for the quick menu)");
    presetPlate.onClick = [this]
    {
        if (juce::ModifierKeys::currentModifiers.isPopupMenu()) showPresetMenu();
        else setBrowserOpen (! browser.isVisible());
    };
    browser.onClose = [this] { setBrowserOpen (false); };
    browser.onSave = [this] { showSaveDialog(); };
    browser.onExport = [this] { exportCurrent(); };
    browser.onImport = [this] { importWithChooser(); };
    canvas.addChildComponent (browser);
    browser.setBounds (0, 86, baseWidth, baseHeight - 86);

    // Clicking the logo flares the black hole.
    logoButton.setTooltip ("Hypernova");
    logoButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    logoButton.onClick = [this]
    {
        logoFlare = 1.0f;
        static const char* lines[] = { "Hypernova.", "Collapse. Ignite.", "Light bends here.", "Event horizon crossed.", "Made by Arrow." };
        showMessage (lines[juce::Random::getSystemRandom().nextInt (5)]);
        glContext.triggerRepaint();
    };
    canvas.addAndMakeVisible (logoButton);
    logoButton.setBounds (20, 16, 360, 58);

    // Updates: a banner under the top bar when a newer release is out; one click fetches and opens the installer.
    canvas.addChildComponent (updateBanner);
    updateBanner.setBounds ((baseWidth - 440) / 2, 90, 440, 32);
    updateBanner.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    updater.onAvailable = [this] (const ab::ui::UpdateInfo& info)
    {
        updateBanner.setInfo (info);
        updateBanner.setVisible (true);
        updateBanner.toFront (false);
    };
    updater.onProgress = [this] (float f) { updateBanner.setProgress (f); };
    updater.onFinished = [this] (bool ok, const juce::String& text)
    {
        showMessage (text);
        if (ok) updateBanner.setVisible (false);
        else updateBanner.setProgress (-1.0f);
    };
    updateBanner.onUpdate = [this]
    {
        if (updater.downloading()) return;
        updateBanner.setProgress (0.0f);
        updater.downloadAndOpen (updateBanner.getInfo());
    };
    updateBanner.onDismiss = [this]
    {
        ab::ui::Updater::skip (updateBanner.getInfo().version);
        updateBanner.setVisible (false);
    };
    if (juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_NO_UPDATE_CHECK", {}).isEmpty())
        updater.check (false);

    ab::ui::LookSettings::load();
    applyTheme();
    prevButton.setTooltip ("Previous preset");
    prevButton.onClick = [this] { processor.stepPreset (-1); };
    nextButton.setTooltip ("Next preset");
    nextButton.onClick = [this] { processor.stepPreset (1); };
    diceButton.setTooltip ("Randomise: mutate this sound a little or a lot, or roll a new one");
    diceButton.onClick = [this] { showDiceMenu(); };
    undoButton.setTooltip ("Undo (Cmd+Z)");
    undoButton.onClick = [this] { if (! processor.undoManager.undo()) showMessage ("Nothing to undo"); };
    redoButton.setTooltip ("Redo (Cmd+Shift+Z)");
    redoButton.onClick = [this] { if (! processor.undoManager.redo()) showMessage ("Nothing to redo"); };
    gearButton.setTooltip ("Sound quality, window size and animation");
    gearButton.onClick = [this] { showSettingsMenu(); };
    saveButton.setTooltip ("Save this sound to My Sounds (then export it to share with friends)");
    saveButton.onClick = [this] { showSaveDialog(); };
    layoutButton.setTooltip ("Arrange the panels: move, resize, stack, hide and add widgets, and switch workspaces");
    layoutButton.onClick = [this] { setLayoutEditing (! layoutEditing); };
    for (auto* b : std::initializer_list<juce::Component*> { &presetPlate, &prevButton, &nextButton, &diceButton, &saveButton, &undoButton, &redoButton, &layoutButton, &gearButton })
        canvas.addAndMakeVisible (b);
    setupEditBar();
    loadWorkspace (ab::ui::WorkspaceStore::current(), false);

    spaceMode.onChange = [this] (int m)
    {
        space.setMode ((SoundSpace::Mode) m);
        if (spaceWindow != nullptr) spaceWindow->view().setMode ((SoundSpace::Mode) m);
    };

    keyboard.setAvailableRange (24, 96);
    keyboard.setOctaveForMiddleC (3); // Ableton naming: C3 = middle C
    keyboard.setLowestVisibleKey (24);
    keyboard.setWantsKeyboardFocus (false);
    colourKeyboard();
    canvas.addAndMakeVisible (keyboard);
    keyboard.setBounds (keysArea);
    keyboard.setKeyWidth ((float) keysArea.getWidth() / 43.0f);

    refreshPresetInfo();

    setResizable (true, true);
    setResizeLimits (baseWidth / 2, baseHeight / 2, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setWantsKeyboardFocus (true);
    applyScale (processor.uiScalePercent.load());
    // The nebula/black hole shader renders under the components; component painting is cached by JUCE,
    // so only what actually changes gets redrawn on the CPU.
    cosmosRenderer = std::make_unique<CosmosRenderer> (glContext, *this, cosmos);
    glContext.setRenderer (cosmosRenderer.get());
    glContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    glContext.setComponentPaintingEnabled (true);
    glContext.setContinuousRepainting (false);
    glContext.attachTo (*this);

    startTimerHz (30);
}

HypernovaAudioProcessorEditor::~HypernovaAudioProcessorEditor()
{
    stopTimer();
    glContext.detach();
    setLookAndFeel (nullptr);
}

void HypernovaAudioProcessorEditor::resized()
{
    canvas.setBounds (0, 0, baseWidth, baseHeight);
    canvas.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));
}

//==============================================================================
void HypernovaAudioProcessorEditor::layoutCanvas()
{
    // Header
    prevButton.setBounds (334, 22, 32, 44);
    presetPlate.setBounds (370, 22, 206, 44);
    nextButton.setBounds (580, 22, 32, 44);
    diceButton.setBounds (618, 22, 38, 44);
    saveButton.setBounds (660, 22, 38, 44);
    undoButton.setBounds (704, 22, 30, 44);
    redoButton.setBounds (736, 22, 30, 44);
    layoutButton.setBounds (770, 22, 32, 44);
    gearButton.setBounds (804, 22, 30, 44);
    editBar.setBounds (334, 22, 520, 44);
    for (int m = 0; m < 4; ++m)
        macroKnobs[(size_t) m] = &knob ("macro" + juce::String (m + 1), "MACRO " + juce::String (m + 1), Palette::fx,
                                        { 842 + m * 84, 12, 84, 68 }, 40);
    knob ("volume", "VOLUME", Colours::text, { 1188, 12, 68, 68 }, 40);

    // Oscillators
    for (int o = 0; o < 2; ++o)
    {
        const auto& P = o == 0 ? oscAPanel : oscBPanel;
        const juce::String p = o == 0 ? "a" : "b";
        const auto c = o == 0 ? Palette::oscA : Palette::oscB;
        auto& w = makeWidget (o == 0 ? "oscA" : "oscB", o == 0 ? "oscA" : "oscB", o == 0 ? "OSC A" : "OSC B", c, P, 40);
        auto* W = &w.content;
        toggle (std::make_unique<PowerLed> (c), p + "On", at (P, 10, 10, 26, 26), "Oscillator on/off", W);
        combo (p + "Table", WavetableBank::names(), at (P, 92, 11, 138, 24), W);
        combo (p + "Warp", warpNames(), at (P, 236, 11, 80, 24), W);
        toggle (std::make_unique<PillToggle> ("FILTER", c), p + "Filter", at (P, 322, 12, 66, 22), "Send this oscillator through the filter", W);
        auto& view = o == 0 ? viewA : viewB;
        W->addAndMakeVisible (view);
        view.setBounds (at (P, 12, 44, 376, 168));

        const char* ids1[] = { "Pos", "WarpAmt", "Uni", "Detune", "Blend" };
        const char* names1[] = { "POSITION", "WARP", "UNISON", "DETUNE", "BLEND" };
        const char* ids2[] = { "Level", "Pan", "Oct", "Semi", "Fine" };
        const char* names2[] = { "LEVEL", "PAN", "OCTAVE", "SEMI", "FINE" };
        for (int i = 0; i < 5; ++i)
        {
            knob (p + ids1[i], names1[i], c, at (P, 12 + i * 75, 212, 75, 68), 42, W);
            knob (p + ids2[i], names2[i], c, at (P, 12 + i * 75, 280, 75, 68), 42, W);
        }
        w.finishBuilding();
        w.spread.setFlags (view, Spread::Stretch);
    }

    // Sound space
    {
        auto& w = makeWidget ("space", "space", "SOUND SPACE", Colours::text, spacePanel);
        auto* W = &w.content;
        for (auto* c : std::initializer_list<juce::Component*> { &space, &spaceMode, &expandButton, &popOutButton }) W->addAndMakeVisible (c);
        spaceMode.setBounds (at (spacePanel, 214, 11, 122, 24));
        space.setBounds (at (spacePanel, 12, 44, 384, 318));
        expandButton.setBounds (at (spacePanel, 340, 11, 26, 24));
        popOutButton.setBounds (at (spacePanel, 370, 11, 26, 24));
        w.extraPaint = [this, &w] (juce::Graphics& g)
        {
            const float shift = (float) (w.spread.width() - spacePanel.getWidth());
            const int note = processor.shownNote.load();
            const int voices = processor.shownVoices.load();
            g.setColour (Colours::textDim);
            g.setFont (mono (10.0f));
            const juce::String info = note >= 0 ? juce::MidiMessage::getMidiNoteName (note, true, true, 3) + "   " + juce::String (voices) + (voices == 1 ? " voice" : " voices")
                                                : "play a note";
            g.drawText (info, juce::Rectangle<float> (118 + shift, 10, 90, 24), juce::Justification::centredRight, false);
        };
        w.finishBuilding();
        w.spread.setFlags (space, Spread::Stretch);
        spaceWidget = &w;
    }
    expandButton.setTooltip ("Fill the window with the Sound Space");
    popOutButton.setTooltip ("Open the Sound Space in its own resizable window");
    expandButton.onClick = [this] { setSpaceExpanded (maximisedId != "space"); };
    popOutButton.onClick = [this]
    {
        if (spaceWindow != nullptr) { spaceWindow.reset(); return; }
        spaceWindow = std::make_unique<SpaceWindow> (processor, (int) space.getMode(), [this] { spaceWindow.reset(); });
    };

    // Sub + noise
    {
        auto& w = makeWidget ("sub", "sub", "SUB", Palette::sub, subPanel, 38);
        auto* W = &w.content;
        toggle (std::make_unique<PowerLed> (Palette::sub), "subOn", at (subPanel, 8, 8, 26, 26), "Sub oscillator on/off", W);
        combo ("subShape", subShapeNames(), at (subPanel, 12, 40, 104, 24), W);
        knob ("subOct", "OCTAVE", Palette::sub, at (subPanel, 6, 72, 56, 68), 40, W);
        knob ("subLevel", "LEVEL", Palette::sub, at (subPanel, 62, 72, 56, 68), 40, W);
        toggle (std::make_unique<PillToggle> ("FILTER", Palette::sub), "subFilter", at (subPanel, 12, 152, 104, 22), "Send the sub through the filter (off keeps the low end clean)", W);
        combo ("noiseType", ab::dsp::noiseNames(), at (subPanel, 126, 40, 110, 24), W);
        knob ("noiseLevel", "NOISE", Colours::textDim, at (subPanel, 126, 72, 56, 68), 40, W);
        knob ("noiseTone", "TONE", Colours::textDim, at (subPanel, 180, 72, 56, 68), 40, W);
        toggle (std::make_unique<PillToggle> ("FILTER", Colours::textDim), "noiseFilter", at (subPanel, 128, 152, 104, 22), "Send the noise through the filter", W);
        w.extraPaint = [&w] (juce::Graphics& g)
        {
            sectionLabel (g, "NOISE", juce::Rectangle<float> (w.spread.mapX (130), 10, 80, 24), Colours::textDim);
            g.setColour (Colours::line);
            g.drawVerticalLine (juce::roundToInt (w.spread.mapX (121)), 14.0f, (float) w.spread.height() - 14.0f);
        };
        w.finishBuilding();
    }

    // Pitch + voice
    {
        auto& w = makeWidget ("pitch", "pitch", "PITCH", Palette::sub, voicePanel);
        auto* W = &w.content;
        knob ("dropAmt", "DROP", Palette::sub, at (voicePanel, 10, 30, 59, 68), 42, W);
        knob ("dropTime", "DROP TIME", Palette::sub, at (voicePanel, 69, 30, 59, 68), 42, W);
        knob ("glide", "GLIDE", Palette::sub, at (voicePanel, 128, 30, 59, 68), 42, W);
        knob ("bendRange", "BEND", Palette::sub, at (voicePanel, 187, 30, 59, 68), 42, W);
        combo ("mode", voiceModeNames(), at (voicePanel, 12, 118, 100, 24), W)
            .setTooltip ("Poly: chords. Mono: one note at a time. Legato: one note, and overlapping notes slide using GLIDE.");
        toggle (std::make_unique<PillToggle> ("RETRIG", Palette::sub), "retrig", at (voicePanel, 12, 150, 100, 22),
                "Restart the waveform on every note: same punch every hit (808s, log drums)", W);
        knob ("velSens", "VELOCITY", Palette::sub, at (voicePanel, 128, 110, 59, 68), 42, W);
        modChips.push_back (std::make_unique<ModChip> ("DRAG VEL", 4, modSourceColour (4)));
        modChips.back()->onHover = [this] (int src) { hoveredModSource = src; for (auto& k : knobs) k->repaint(); };
        W->addAndMakeVisible (*modChips.back());
        modChips.back()->setBounds (at (voicePanel, 186, 150, 60, 20));
        w.extraPaint = [&w] (juce::Graphics& g)
        {
            sectionLabel (g, "MONO / GLIDE / LEGATO", w.spread.map ({ 12, 108, 240, 18 }).withX (12), Palette::sub);
        };
        w.finishBuilding();
    }

    // Filter
    {
        auto& w = makeWidget ("filter", "filter", "FILTER", Palette::filter, filterPanel, 38);
        auto* W = &w.content;
        toggle (std::make_unique<PowerLed> (Palette::filter), "fltOn", at (filterPanel, 8, 8, 26, 26), "Filter on/off", W);
        combo ("fltType", filterNames(), at (filterPanel, 190, 11, 130, 24), W);
        W->addAndMakeVisible (filterView);
        filterView.setBounds (at (filterPanel, 12, 42, 308, 62));
        const char* fIds[] = { "cutoff", "res", "fltDrive", "fltEnv", "fltKey", "fltMix" };
        const char* fNames[] = { "CUTOFF", "RES", "DRIVE", "ENV", "KEY", "MIX" };
        for (int i = 0; i < 6; ++i)
            knob (fIds[i], fNames[i], Palette::filter, at (filterPanel, 12 + i * 51, 110, 51, 68), 40, W);
        w.finishBuilding();
        w.spread.setFlags (filterView, Spread::Stretch);
    }

    // Sampler
    {
        const juce::Rectangle<int> design { 0, 0, 560, 300 };
        auto& w = makeWidget ("sampler", "sampler", "SAMPLER", Palette::oscA, design, 38);
        auto* W = &w.content;
        toggle (std::make_unique<PowerLed> (Palette::oscA), "smpOn", { 8, 8, 26, 26 }, "Sampler on/off", W);
        W->addAndMakeVisible (sampleButton);
        sampleButton.setBounds (262, 11, 62, 24);
        sampleButton.setTooltip ("Load a sample (or drop one on the waveform)");
        sampleButton.onClick = [this] { showSampleMenu(); };
        combo ("smpLoop", ab::sampleLoopNames(), { 330, 11, 120, 24 }, W);
        toggle (std::make_unique<PillToggle> ("FILTER", Palette::oscA), "smpFilter", { 456, 12, 92, 22 }, "Send the sampler through the filter", W);
        W->addAndMakeVisible (samplerView);
        samplerView.setBounds (12, 44, 536, 146);
        samplerView.onMessage = [this] (const juce::String& m) { showMessage (m); };
        samplerView.onLoadRequest = [this] { chooseSample(); };
        const char* ids[] = { "smpLevel", "smpPan", "smpRoot", "smpSemi", "smpFine" };
        const char* names[] = { "LEVEL", "PAN", "ROOT", "SEMI", "FINE" };
        for (int i = 0; i < 5; ++i) knob (ids[i], names[i], Palette::oscA, { 12 + i * 58, 194, 58, 68 }, 40, W);
        const char* env[] = { "smpA", "smpD", "smpS", "smpR" };
        const char* envNames[] = { "A", "D", "S", "R" };
        for (int i = 0; i < 4; ++i) knob (env[i], envNames[i], Palette::env, { 330 + i * 54, 194, 54, 68 }, 38, W);
        toggle (std::make_unique<PillToggle> ("TRACK KEYS", Palette::oscA), "smpTrack", { 12, 268, 118, 22 },
                "On: the sample follows the keyboard. Off: every key plays it at its own pitch (drums, one-shots).", W);
        toggle (std::make_unique<PillToggle> ("REVERSE", Palette::oscA), "smpReverse", { 138, 268, 100, 22 }, "Play the sample backwards", W);
        w.extraPaint = [&w] (juce::Graphics& g)
        {
            g.setColour (Colours::textFaint);
            g.setFont (font (9.5f, true).withExtraKerningFactor (0.2f));
            g.drawText ("SAMPLE ENVELOPE", w.spread.map ({ 330, 270, 216, 18 }), juce::Justification::centredLeft, false);
        };
        w.finishBuilding();
        w.spread.setFlags (samplerView, Spread::Stretch);
    }

    // Envelopes
    {
        auto& w = makeWidget ("env", "env", "AMP ENV", Palette::env, envPanel);
        auto* W = &w.content;
        W->addAndMakeVisible (ampView);
        W->addAndMakeVisible (modView);
        ampView.setBounds (at (envPanel, 12, 36, 164, 64));
        modView.setBounds (at (envPanel, 192, 36, 164, 64));
        modChips.push_back (std::make_unique<ModChip> ("DRAG", 3, modSourceColour (3)));
        modChips.back()->onHover = [this] (int src) { hoveredModSource = src; for (auto& k : knobs) k->repaint(); };
        W->addAndMakeVisible (*modChips.back());
        modChips.back()->setBounds (at (envPanel, 268, 12, 52, 20));
        const char* adsr[] = { "A", "D", "S", "R" };
        for (int i = 0; i < 4; ++i)
        {
            knob (juce::String ("amp") + adsr[i], adsr[i], Palette::env, at (envPanel, 12 + i * 41, 108, 41, 66), 36, W);
            knob (juce::String ("mod") + adsr[i], adsr[i], Palette::lfo, at (envPanel, 192 + i * 41, 108, 41, 66), 36, W);
        }
        auto* dragChip = modChips.back().get();
        w.extraPaint = [&w] (juce::Graphics& g)
        {
            sectionLabel (g, "MOD ENV", juce::Rectangle<float> (w.spread.mapX (194), 10, 120, 24), Palette::lfo);
        };
        w.finishBuilding();
        w.spread.setFlags (ampView, Spread::Stretch);
        w.spread.setFlags (modView, Spread::Stretch);
        w.spread.setFlags (*dragChip, Spread::FollowX);
    }

    // The four deck pages are widgets too, stacked as tabs where the deck used to be.
    const char* pageIds[] = { "mod", "fx", "morefx", "play" };
    const char* pageTitles[] = { "MODULATION", "EFFECTS", "MORE FX", "PLAY" };
    for (int i = 0; i < 4; ++i)
    {
        auto& w = makeWidget (pageIds[i], pageIds[i], pageTitles[i], Palette::mod, deckPanel, 14, 44);
        w.headerFreeWidth = deckPanel.getWidth() - 40;
        w.content.addAndMakeVisible (pages[(size_t) i]);
        pages[(size_t) i].setVisible (true);
        pages[(size_t) i].setBounds (0, deckContent.getY() - deckPanel.getY(), deckContent.getWidth(), deckContent.getHeight());
        w.extraPaint = [&w] (juce::Graphics& g)
        {
            g.setColour (Colours::line);
            g.drawHorizontalLine (42, 12.0f, (float) w.spread.width() - 12.0f);
        };
    }
    layoutModPage();
    layoutFxPage();
    layoutMoreFxPage();
    layoutPlayPage();
    // Pages spread their groups out when there's room; the LFO views and the matrix grow.
    for (int i = 0; i < 4; ++i)
    {
        auto& pg = pages[(size_t) i];
        pg.spread.capture (pg, deckContent.getWidth(), deckContent.getHeight(), 0);
        if (auto* w = findWidget (pageIds[i]))
        {
            w->finishBuilding();
            w->spread.setFlags (pg, Spread::Stretch);
        }
    }
    pages[0].spread.setFlags (lfoView1, Spread::Stretch);
    pages[0].spread.setFlags (lfoView2, Spread::Stretch);
    for (auto& row : modRows) pages[0].spread.setFlags (*row, Spread::StretchX);
}

//==============================================================================
// The canvas is three layers: the backdrop (GPU shader, or a cached CPU picture when there's no OpenGL),
// a cached image of everything static (panels, titles, logo type), and a few live bits drawn on top.
void HypernovaAudioProcessorEditor::paintCanvas (juce::Graphics& g)
{
    if (! cosmosOnGpu())
    {
        if (! fallbackBackdrop.isValid())
            fallbackBackdrop = renderCosmosFallback (baseWidth, baseHeight, 2.0f, logoHole, 0.0f);
        g.drawImage (fallbackBackdrop, juce::Rectangle<float> (0, 0, (float) baseWidth, (float) baseHeight));
    }

    if (! staticLayer.isValid())
    {
        staticLayer = juce::Image (juce::Image::ARGB, baseWidth * 2, baseHeight * 2, true);
        juce::Graphics sg (staticLayer);
        sg.addTransform (juce::AffineTransform::scale (2.0f));
        paintStatic (sg);
    }
    g.drawImage (staticLayer, juce::Rectangle<float> (0, 0, (float) baseWidth, (float) baseHeight));
    paintDynamic (g);
}

void HypernovaAudioProcessorEditor::paintStatic (juce::Graphics& g)
{
    // Wordmark: an accent dot and "hypernova" in lowercase, the same as the website.
    {
        g.setColour (Colours::accent);
        g.fillEllipse (juce::Rectangle<float> (logoHoleRadius * 2.0f, logoHoleRadius * 2.0f).withCentre (logoHole));
        g.setColour (Colours::text);
        g.setFont (heavy (34.0f).withExtraKerningFactor (-0.04f));
        g.drawText ("hypernova", juce::Rectangle<float> (70, 14, 260, 40), juce::Justification::centredLeft, false);
        g.setColour (Colours::textDim);
        g.setFont (mono (11.0f));
        g.drawText ("wavetable synthesizer", juce::Rectangle<float> (72, 52, 250, 16), juce::Justification::centredLeft, false);
    }

    auto tray = juce::Rectangle<float> (838, 8, 342, 76);
    g.setColour (Colours::inset.withAlpha (0.55f));
    g.fillRoundedRectangle (tray, 12.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (tray.reduced (0.5f), 12.0f, 1.0f);
}

void HypernovaAudioProcessorEditor::paintDynamic (juce::Graphics& g)
{
    // Logo flare: a burst of light around the black hole, on top of whatever the backdrop is doing.
    if (logoFlare > 0.002f)
    {
        const float f = logoFlare;
        const auto c = logoHole;
        for (int i = 3; i >= 1; --i)
        {
            const float rad = logoHoleRadius * (1.6f + 2.6f * i) * (1.0f + 1.4f * (1.0f - f));
            g.setColour (Colours::warm.withAlpha (0.22f * f / (float) i));
            g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c));
        }
        g.setColour (juce::Colours::white.withAlpha (0.5f * f));
        g.fillEllipse (juce::Rectangle<float> (logoHoleRadius * 1.5f, logoHoleRadius * 1.5f).withCentre (c));
    }


    if (message.isNotEmpty() && juce::Time::currentTimeMillis() < messageUntil)
    {
        g.setColour (Palette::env);
        g.setFont (font (11.0f, true));
        g.drawText (message, messageArea.toFloat(), juce::Justification::centred, true);
    }
}

//==============================================================================
void HypernovaAudioProcessorEditor::refreshPresetInfo()
{
    presetPlate.name = processor.getPresetName();
    presetPlate.category = processor.getPresetCategory();
    presetPlate.repaint();
    for (int m = 0; m < 4; ++m)
        macroKnobs[(size_t) m]->setLabel (processor.getMacroName (m));
}

void HypernovaAudioProcessorEditor::timerCallback()
{
    // Backdrop: feed the shader and ask for a frame. When the GPU path comes up (or goes), redraw the canvas
    // so the CPU fallback picture isn't left underneath.
    {
        float l[512], r[512];
        processor.scope.latest (l, r, 512);
        double sum = 0;
        for (int i = 0; i < 512; ++i) sum += (double) (l[i] * l[i] + r[i] * r[i]);
        const float rms = (float) std::sqrt (sum / 1024.0);
        cosmos.level = juce::jlimit (0.0f, 1.0f, rms * 3.0f);
        const float k = (float) getWidth() / (float) baseWidth;
        cosmos.flare = logoFlare;
        cosmos.holeX = logoHole.x * k;
        cosmos.holeY = logoHole.y * k;
        cosmos.holeR = 0.0f; // the logo is the dot now; no black hole behind it
        const bool gpu = cosmosOnGpu();
        if (gpu != lastGpu) { lastGpu = gpu; canvas.repaint(); }
        // Backdrop frame rate: 30 fps while notes sound, 12 fps when idle, or per the Animation setting.
        const int mode = processor.uiAnimation.load(); // 0 full, 1 calm, 2 off
        const bool busy = processor.shownVoices.load() > 0;
        const bool staticScene = ThemeState::get().backdropStyle() == ab::ui::BackdropFlat;
        const int every = (mode == 2 || staticScene) ? 0 : (mode == 1 ? (busy ? 2 : 5) : (busy ? 1 : 3));
        ++frameTick;
        // While the flare fades, draw every frame whatever the animation setting says.
        if (logoFlare > 0.002f)
        {
            logoFlare *= 0.90f;
            if (logoFlare <= 0.002f) logoFlare = 0.0f;
            if (gpu) glContext.triggerRepaint();
            else canvas.repaint (0, 0, getWidth(), 120);
            staticFramePainted = false;
        }
        if (gpu && every > 0 && frameTick % every == 0) glContext.triggerRepaint();
        else if (gpu && every == 0 && ! staticFramePainted) { glContext.triggerRepaint(); staticFramePainted = true; }
        if (every != 0) staticFramePainted = false;
    }

    const int v = processor.presetVersion.load();
    if (v != lastPresetVersion)
    {
        lastPresetVersion = v;
        refreshPresetInfo();
    }

    // Undo: once edits pause for ~0.4 s, close the step so the next change is a new undo.
    {
        const int n = processor.undoManager.getNumActionsInCurrentTransaction();
        if (n != lastActionCount) { lastActionCount = n; editQuietTicks = 0; }
        else if (n > 0 && ++editQuietTicks > 12) { processor.undoManager.beginNewTransaction(); lastActionCount = 0; }
    }

    const bool sounding = processor.shownVoices.load() > 0;
    const float outLevel = cosmos.level.load();
    for (auto* v : { static_cast<ab::ui::OrbitView*> (&viewA), static_cast<ab::ui::OrbitView*> (&viewB), static_cast<ab::ui::OrbitView*> (&space) })
        v->setLevel (outLevel);
    for (auto& k : knobs) k->ageTrail();
    // The 3D views run at 20 fps (two ticks in three): smooth to the eye, a third less drawing than 30.
    if ((++viewTick % 3) != 0)
    {
        viewA.refresh (sounding);
        viewB.refresh (sounding);
        space.refresh (sounding);
    }
    if (spaceWindow != nullptr) spaceWindow->view().refresh (sounding);
    tickTools (sounding);
    if (samplerView.isVisible()) samplerView.refresh();
    // Small views: while sound plays (their values move), or when a parameter changed.
    const int changes = processor.parameterChanges.load();
    if (sounding || changes != lastParameterChanges)
    {
        lastParameterChanges = changes;
        filterView.repaint();
        ampView.repaint();
        modView.repaint();
        lfoView1.repaint();
        lfoView2.repaint();
        // Modulated knobs animate: repaint only the ones a live source is actually reaching.
        bool anyMod = false;
        for (int i = 0; i < ab::NumModSlots && ! anyMod; ++i)
        {
            const juce::String p = "mod" + juce::String (i + 1);
            anyMod = (int) processor.apvts.getRawParameterValue (p + "Src")->load() != 0
                  && (int) processor.apvts.getRawParameterValue (p + "Dest")->load() != 0
                  && std::abs (processor.apvts.getRawParameterValue (p + "Amt")->load()) > 0.001f;
        }
        if (anyMod && sounding)
            for (auto& k : knobs)
                if (k->isShowing() && std::abs (modInfoFor (k->paramId()).depth) > 0.001f)
                    k->repaint();
    }
    const int readout = processor.shownNote.load() * 100 + processor.shownVoices.load();
    if (readout != lastReadout)
    {
        lastReadout = readout;
        if (spaceWidget != nullptr) spaceWidget->content.repaint (110 + spaceWidget->spread.width() - spacePanel.getWidth(), 8, 110, 28);
    }
    if (message.isNotEmpty() && juce::Time::currentTimeMillis() >= messageUntil)
    {
        message.clear();
        canvas.repaint (messageArea);
    }
}

void HypernovaAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& presets = factoryPresets();
    juce::StringArray categories;
    for (const auto& p : presets)
        categories.addIfNotAlreadyThere (p.category);

    menu.addSectionHeader ("FACTORY  (" + juce::String ((int) presets.size() - 1) + " sounds)");
    for (const auto& cat : categories)
    {
        if (cat == "Init") { menu.addItem (1, "Init", true, processor.getPresetName() == "Init"); continue; }
        juce::PopupMenu sub;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (cat == presets[(size_t) i].category)
                sub.addItem (1 + i, presets[(size_t) i].name, true, i == processor.getCurrentProgram() && processor.getPresetCategory() == cat);
        menu.addSubMenu (cat, sub);
    }

    // Installed packs and the user's own/imported sounds, grouped by folder.
    auto fileItems = std::make_shared<juce::Array<juce::File>>();
    auto addFolderMenus = [&] (const juce::File& root, juce::PopupMenu& into, bool rootItemsFlat)
    {
        std::map<juce::String, juce::PopupMenu> groups;
        juce::PopupMenu flat;
        for (const auto& f : HypernovaAudioProcessor::presetFilesIn (root))
        {
            const int id = 10000 + fileItems->size();
            fileItems->add (f);
            const bool top = f.getParentDirectory() == root;
            const auto name = f.getFileNameWithoutExtension();
            if (top && rootItemsFlat) flat.addItem (id, name, true, name == processor.getPresetName());
            else groups[top ? juce::String ("Other") : f.getParentDirectory().getFileName()].addItem (id, name, true, name == processor.getPresetName());
        }
        for (auto& [name, m] : groups) into.addSubMenu (name, m);
        if (flat.getNumItems() > 0)
        {
            if (! groups.empty()) into.addSeparator();
            for (juce::PopupMenu::MenuItemIterator it (flat); it.next();) into.addItem (it.getItem());
        }
    };

    juce::PopupMenu packs;
    addFolderMenus (HypernovaAudioProcessor::packFolder(), packs, false);
    menu.addSectionHeader ("SOUND PACKS");
    if (packs.getNumItems() > 0) for (juce::PopupMenu::MenuItemIterator it (packs); it.next();) menu.addItem (it.getItem());
    else menu.addItem (-1, "(install a pack or import one)", false, false);

    juce::PopupMenu user;
    addFolderMenus (HypernovaAudioProcessor::userPresetFolder(), user, true);
    if (user.getNumItems() == 0) user.addItem (-1, "(nothing yet: save or import a sound)", false, false);
    menu.addSubMenu ("My Sounds", user);

    menu.addSeparator();
    menu.addItem (2000, "Save current sound...");
    menu.addItem (2002, "Export current sound to share...");
    menu.addItem (2003, "Import sounds or a pack...");
    menu.addItem (2001, "Show my sounds folder");

    menu.setLookAndFeel (&lookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetPlate).withMinimumWidth (presetPlate.getWidth()),
                        [this, fileItems] (int result)
                        {
                            if (result <= 0) return;
                            if (result < 1000) processor.loadFactoryPreset (result - 1);
                            else if (result >= 10000 && result - 10000 < fileItems->size()) processor.loadUserPreset ((*fileItems)[result - 10000]);
                            else if (result == 2000) showSaveDialog();
                            else if (result == 2002) exportCurrent();
                            else if (result == 2003) importWithChooser();
                            else if (result == 2001)
                            {
                                HypernovaAudioProcessor::userPresetFolder().createDirectory();
                                HypernovaAudioProcessor::userPresetFolder().revealToUser();
                            }
                        });
}

void HypernovaAudioProcessorEditor::showSaveDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save sound", "Name it, sign it, and name the macros. It goes into My Sounds.",
                                                      juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", processor.getPresetName(), "Name");
    saveWindow->addTextEditor ("author", lastAuthor, "Made by");
    for (int m = 0; m < 4; ++m)
        saveWindow->addTextEditor ("macro" + juce::String (m), processor.getMacroName (m), "Macro " + juce::String (m + 1));
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
        {
            for (int m = 0; m < 4; ++m)
                processor.setMacroName (m, saveWindow->getTextEditorContents ("macro" + juce::String (m)));
            lastAuthor = saveWindow->getTextEditorContents ("author").trim();
            const auto name = saveWindow->getTextEditorContents ("name");
            showMessage (processor.saveUserPreset (name, lastAuthor) ? "Saved \"" + name.trim() + "\" to My Sounds"
                                                                      : juce::String ("Couldn't save: give it a name"));
        }
        saveWindow.reset();
    }), false);
}

void HypernovaAudioProcessorEditor::exportCurrent()
{
    const auto start = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                           .getChildFile (juce::File::createLegalFileName (processor.getPresetName()) + HypernovaAudioProcessor::presetExtension);
    chooser = std::make_unique<juce::FileChooser> ("Export sound to share", start, juce::String ("*") + HypernovaAudioProcessor::presetExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              showMessage (processor.exportPreset (f, lastAuthor) ? "Exported. Send the .hnpreset file to a friend."
                                                                                  : juce::String ("Export failed"));
                          });
}

void HypernovaAudioProcessorEditor::importWithChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Import sounds (files or a pack folder)",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads"),
                                                   juce::String ("*") + HypernovaAudioProcessor::presetExtension + ";*.abpreset;*.zip");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc) { importAndReport (fc.getResults()); });
}

void HypernovaAudioProcessorEditor::importAndReport (const juce::Array<juce::File>& items)
{
    if (items.isEmpty()) return;
    juce::File first;
    const int n = processor.importPresets (items, &first);
    if (n == 0) { showMessage ("No Hypernova sounds found there"); return; }
    processor.loadUserPreset (first);
    if (browser.isVisible()) browser.refresh();
    showMessage ("Imported " + juce::String (n) + (n == 1 ? " sound" : " sounds") + " into My Sounds");
}

bool HypernovaAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const juce::File f (path);
        if (f.isDirectory() || HypernovaAudioProcessor::isPresetFile (f) || f.hasFileExtension ("zip")) return true;
        if (SamplerView::isAudioFile (f)) return true;
    }
    return false;
}

void HypernovaAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int) { dragHover = true; repaint(); }
void HypernovaAudioProcessorEditor::fileDragExit (const juce::StringArray&) { dragHover = false; repaint(); }

void HypernovaAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    repaint();
    // A recording dropped anywhere goes into the sampler, which comes on screen if it wasn't.
    for (const auto& path : files)
        if (SamplerView::isAudioFile (juce::File (path)))
        {
            if (! layoutTree().contains ("sampler")) addWidgetType ("sampler");
            else activateWidget ("sampler");
            samplerView.load (juce::File (path));
            return;
        }
    juce::Array<juce::File> items;
    for (const auto& path : files) items.add (juce::File (path));
    importAndReport (items);
}

void HypernovaAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dragHover) return;
    auto r = getLocalBounds().toFloat().reduced (10.0f);
    g.setColour (Colours::bg0.withAlpha (0.72f));
    g.fillRoundedRectangle (r, 18.0f);
    g.setColour (Palette::oscA);
    g.drawRoundedRectangle (r, 18.0f, 2.0f);
    g.setFont (heavy (juce::jmax (16.0f, (float) getWidth() * 0.02f)));
    g.drawText ("DROP TO IMPORT SOUNDS", r, juce::Justification::centred, false);
}

void HypernovaAudioProcessorEditor::showMessage (const juce::String& m)
{
    message = m;
    messageUntil = juce::Time::currentTimeMillis() + 3500;
    canvas.repaint (messageArea);
}

//==============================================================================
void HypernovaAudioProcessorEditor::DeckPage::paint (juce::Graphics& g)
{
    for (const auto& c : captions)
    {
        if (c.divider)
        {
            g.setColour (Colours::line);
            g.drawVerticalLine (juce::roundToInt (spread.mapX ((float) c.area.getX())), (float) c.area.getY(),
                                (float) (getHeight() - (140 - c.area.getBottom())));
            continue;
        }
        g.setColour (c.colour);
        if (c.colour.slot == SlotTextFaint) g.setFont (font (11.0f)); // body text
        else g.setFont (font (9.5f, true).withExtraKerningFactor (0.2f));
        // Captions move with the controls they name.
        auto a = c.area.toFloat();
        g.drawText (c.text, a.withX (spread.mapX (a.getX())).withY (spread.map (a).getY()), juce::Justification::centredLeft, false);
    }
}

void HypernovaAudioProcessorEditor::showDeckPage (int page)
{
    static const char* ids[] = { "mod", "fx", "morefx", "play" };
    processor.uiDeckPage = juce::jlimit (0, 3, page);
    const juce::String id = ids[processor.uiDeckPage];
    if (tree.contains (id)) activateWidget (id);
    else addWidgetType (id);
}

void HypernovaAudioProcessorEditor::layoutModPage()
{
    auto* pg = &pages[0];
    for (int l = 0; l < 2; ++l)
    {
        const int x = 12 + l * 296;
        const juce::String p = "lfo" + juce::String (l + 1);
        pg->captions.push_back ({ { x, 4, 60, 24 }, "LFO " + juce::String (l + 1), Palette::lfo, false });
        combo (p + "Shape", lfoShapeNames(), { x + 52, 4, 112, 24 }, pg);
        combo (p + "Sync", lfoSyncNames(), { x + 170, 4, 110, 24 }, pg);
        auto& view = l == 0 ? lfoView1 : lfoView2;
        pg->addAndMakeVisible (view);
        view.setBounds (x, 36, 160, 62);
        knob (p + "Rate", "RATE", Palette::lfo, { x + 164, 32, 58, 68 }, 40, pg);
        knob (p + "Fade", "FADE IN", Palette::lfo, { x + 222, 32, 58, 68 }, 40, pg);
        toggle (std::make_unique<PillToggle> ("RETRIGGER", Palette::lfo), p + "Retrig", { x, 106, 160, 22 },
                "Restart the LFO on each note. Off = free-running, locked to the song when synced.", pg);
        // Drag this onto any knob to have the LFO move it.
        modChips.push_back (std::make_unique<ModChip> ("DRAG LFO " + juce::String (l + 1), l + 1, modSourceColour (l + 1)));
        modChips.back()->onHover = [this] (int src) { hoveredModSource = src; for (auto& k : knobs) k->repaint(); };
        pg->addAndMakeVisible (*modChips.back());
        modChips.back()->setBounds (x + 168, 106, 112, 22);
        if (l == 0) pg->captions.push_back ({ { x + 290, 4, 1, 124 }, {}, {}, true });
    }

    const int mx = 616;
    pg->captions.push_back ({ { mx - 12, 4, 1, 124 }, {}, {}, true });
    for (int i = 0; i < NumModSlots; ++i)
    {
        modRows.push_back (std::make_unique<ModRow> (processor.apvts, i));
        pg->addAndMakeVisible (*modRows.back());
        const int col = i / 4, row = i % 4;
        modRows.back()->setBounds (mx + col * 308, 4 + row * 32, 296, 26);
    }
}

void HypernovaAudioProcessorEditor::layoutFxPage()
{
    auto* pg = &pages[1];
    constexpr int cell = 64, knobY = 36, gap = 22;
    int x = 12;
    // Each effect's name is its bypass switch.
    auto group = [&] (const juce::String& title, int cells, const char* onParam = nullptr) -> int
    {
        const int start = x;
        if (onParam != nullptr)
            toggle (std::make_unique<ab::ui::SectionToggle> (title, Palette::fx), onParam, { x, 4, juce::jmin (cells * cell, 92), 24 },
                    "Click to switch " + title.toLowerCase() + " off and on", pg);
        else
            pg->captions.push_back ({ { x, 4, cells * cell, 24 }, title, Colours::textDim, false });
        x += cells * cell + gap;
        pg->captions.push_back ({ { x - gap / 2, 6, 1, 122 }, {}, {}, true });
        return start;
    };
    auto fxKnob = [&] (const char* id, const char* label, int gx, int i) { knob (id, label, Palette::fx, { gx + i * cell, knobY, cell, 72 }, 42, pg); };

    int g = group ("DIST", 2, "distOn");
    combo ("distType", distNames(), { g + 56, 4, 2 * cell - 56, 24 }, pg);
    fxKnob ("distDrive", "DRIVE", g, 0);
    fxKnob ("distMix", "MIX", g, 1);

    g = group ("OTT", 1, "ottOn");
    fxKnob ("ott", "SQUASH", g, 0);

    g = group ("CHORUS", 2, "chorusOn");
    fxKnob ("chorusRate", "RATE", g, 0);
    fxKnob ("chorusMix", "MIX", g, 1);

    g = group ("DELAY", 3, "dlyOn");
    combo ("dlyTime", delayTimeNames(), { g + 52, 4, 78, 24 }, pg);
    toggle (std::make_unique<PillToggle> ("PING", Palette::fx), "dlyPing", { g + 134, 5, 3 * cell - 134, 22 }, "Ping-pong: repeats bounce left and right", pg);
    fxKnob ("dlyFb", "FEEDBACK", g, 0);
    fxKnob ("dlyTone", "TONE", g, 1);
    fxKnob ("dlyMix", "MIX", g, 2);

    g = group ("SPACE", 3, "verbOn");
    fxKnob ("verbSize", "SIZE", g, 0);
    fxKnob ("verbShimmer", "SHIMMER", g, 1);
    fxKnob ("verbMix", "MIX", g, 2);

    g = group ("EQ", 2, "eqOn");
    fxKnob ("eqLow", "LOW", g, 0);
    fxKnob ("eqHigh", "HIGH", g, 1);

    g = group ("OUTPUT", 3);
    toggle (std::make_unique<PillToggle> ("MONO BASS", Palette::fx), "monoBass", { g + cell + 6, knobY + 12, 2 * cell - 12, 24 },
            "Keeps everything under 120 Hz in mono so the bass hits hard on club systems", pg);
    fxKnob ("width", "WIDTH", g, 0);
    pg->captions.pop_back(); // no divider after the last group
}

// Second effects page: movement, character and pitch. Same rules as the first: the name is the on/off switch.
void HypernovaAudioProcessorEditor::layoutMoreFxPage()
{
    auto* pg = &pages[2];
    constexpr int cell = 58, knobY = 36, gap = 16;
    int x = 12;
    auto group = [&] (const juce::String& title, int cells, const char* onParam) -> int
    {
        const int start = x;
        toggle (std::make_unique<ab::ui::SectionToggle> (title, Palette::fx), onParam, { x, 4, juce::jmin (cells * cell, 104), 24 },
                "Click to switch " + title.toLowerCase() + " off and on", pg);
        x += cells * cell + gap;
        pg->captions.push_back ({ { x - gap / 2, 6, 1, 122 }, {}, {}, true });
        return start;
    };
    auto fxKnob = [&] (const char* id, const char* label, int gx, int i) { knob (id, label, Palette::fx, { gx + i * cell, knobY, cell, 72 }, 42, pg); };

    int g = group ("FLANGER", 4, "flangOn");
    fxKnob ("flangRate", "RATE", g, 0);
    fxKnob ("flangDepth", "DEPTH", g, 1);
    fxKnob ("flangFb", "FEEDBACK", g, 2);
    fxKnob ("flangMix", "MIX", g, 3);

    g = group ("TAPE", 3, "tapeOn");
    fxKnob ("tapeWow", "WOBBLE", g, 0);
    fxKnob ("tapeNoise", "NOISE", g, 1);
    fxKnob ("tapeSat", "SATURATE", g, 2);

    g = group ("GATE", 4, "gateOn");
    // Pattern in the header; the two rate boxes sit under the knobs they belong to.
    combo ("gatePattern", ab::dsp::GateAndPan::patternNames(), { g + 104, 4, 4 * cell - 104, 24 }, pg);
    fxKnob ("gateDepth", "GATE", g, 0);
    fxKnob ("gateShape", "SHAPE", g, 1);
    fxKnob ("panDepth", "AUTO PAN", g, 2);
    combo ("gateRate", ab::dsp::syncRateNames(), { g + 4, knobY + 76, 2 * cell - 8, 22 }, pg).setTooltip ("Gate rate");
    combo ("panRate", ab::dsp::syncRateNames(), { g + 2 * cell + 2, knobY + 76, cell - 4, 22 }, pg).setTooltip ("Auto pan rate");

    g = group ("FILTER", 4, "fxFltOn");
    combo ("fxFltType", ab::dsp::FxFilter::typeNames(), { g + 104, 4, 4 * cell - 104, 24 }, pg);
    fxKnob ("fxFltFreq", "FREQ", g, 0);
    fxKnob ("fxFltRes", "RES", g, 1);
    fxKnob ("fxFltDepth", "SWEEP", g, 2);
    combo ("fxFltRate", ab::dsp::syncRateNames(), { g + 2 * cell + 2, knobY + 76, cell - 4, 22 }, pg).setTooltip ("Sweep rate");

    g = group ("PITCH", 2, "shiftOn");
    fxKnob ("shiftSemis", "SHIFT", g, 0);
    fxKnob ("shiftMix", "MIX", g, 1);

    // Styles for the effects on the first page, kept here so each page stays readable.
    pg->captions.push_back ({ { x, 4, 160, 24 }, "STYLES", Colours::textDim, false });
    auto styleCombo = [&] (const char* id, const juce::StringArray& items, const char* label, int row)
    {
        pg->captions.push_back ({ { x, 34 + row * 32, 50, 22 }, label, Colours::textFaint, false });
        combo (id, items, { x + 50, 32 + row * 32, 104, 24 }, pg);
    };
    styleCombo ("chorusMode", juce::StringArray { "Classic", "Ensemble", "Dimension" }, "CHORUS", 0);
    styleCombo ("dlyStyle", juce::StringArray { "Digital", "Reverse", "Granular" }, "DELAY", 1);
    styleCombo ("verbMode", juce::StringArray { "Space", "Plate", "Spring", "Room" }, "SPACE", 2);
}

void HypernovaAudioProcessorEditor::layoutPlayPage()
{
    auto* pg = &pages[3];
    constexpr int cell = 68, knobY = 36;
    const auto c = Palette::env;

    pg->captions.push_back ({ { 12, 4, 60, 24 }, "ARP", c, false });
    toggle (std::make_unique<PillToggle> ("ON", c), "arpOn", { 52, 5, 58, 22 }, "Arpeggiator on/off. Hold notes and it plays them as a pattern, locked to the song.", pg);
    combo ("arpMode", arpModeNames(), { 116, 4, 104, 24 }, pg);
    combo ("arpRate", arpRateNames(), { 226, 4, 76, 24 }, pg);
    knob ("arpOct", "OCTAVES", c, { 12, knobY, cell, 72 }, 42, pg);
    knob ("arpGate", "GATE", c, { 12 + cell, knobY, cell, 72 }, 42, pg);
    pg->captions.push_back ({ { 318, 6, 1, 122 }, {}, {}, true });

    pg->captions.push_back ({ { 332, 4, 70, 24 }, "CHORD", c, false });
    combo ("chord", chordNames(), { 390, 4, 126, 24 }, pg).setTooltip ("One key plays a whole chord (Poly mode). Great for house stabs.");
    knob ("strum", "STRUM", c, { 332, knobY, cell, 72 }, 42, pg);
    pg->captions.push_back ({ { 532, 6, 1, 122 }, {}, {}, true });

    pg->captions.push_back ({ { 546, 4, 120, 24 }, "TUNING", c, false });
    knob ("transpose", "TRANSPOSE", c, { 546, knobY, cell, 72 }, 42, pg);
    knob ("tune", "FINE", c, { 546 + cell, knobY, cell, 72 }, 42, pg);
    knob ("drift", "DRIFT", c, { 546 + 2 * cell, knobY, cell, 72 }, 42, pg);
    pg->captions.push_back ({ { 760, 6, 1, 122 }, {}, {}, true });

    pg->captions.push_back ({ { 774, 4, 160, 24 }, "UNISON WIDTH", c, false });
    knob ("aWidth", "OSC A", Palette::oscA, { 774, knobY, cell, 72 }, 42, pg);
    knob ("bWidth", "OSC B", Palette::oscB, { 774 + cell, knobY, cell, 72 }, 42, pg);
    pg->captions.push_back ({ { 924, 6, 1, 122 }, {}, {}, true });

    // Audio-rate cross modulation between the two oscillators.
    pg->captions.push_back ({ { 938, 4, 280, 24 }, "CROSS MOD", Palette::oscB, false });
    const int xw = 56;
    knob ("xFmAB", "FM A>B", Palette::oscB, { 938, knobY, xw, 72 }, 40, pg)
        .slider.setTooltip ("Osc A bends osc B's pitch at audio rate: metallic, bell-like tones");
    knob ("xFmBA", "FM B>A", Palette::oscA, { 938 + xw, knobY, xw, 72 }, 40, pg)
        .slider.setTooltip ("Osc B bends osc A's pitch at audio rate");
    knob ("xRing", "RING", Palette::oscB, { 938 + 2 * xw, knobY, xw, 72 }, 40, pg)
        .slider.setTooltip ("Ring modulation: the two oscillators multiplied, for clangy inharmonic tones");
    knob ("xAm", "AM", Palette::oscB, { 938 + 3 * xw, knobY, xw, 72 }, 40, pg)
        .slider.setTooltip ("Osc B chops osc A's level: tremolo at low pitches, sidebands at high ones");
    knob ("xFltFm", "FLT FM", Palette::filter, { 938 + 4 * xw, knobY, xw, 72 }, 40, pg)
        .slider.setTooltip ("Osc A shakes the filter cutoff at audio rate: growl and buzz");
}

//==============================================================================
// What modulation is reaching this knob: the strongest slot pointing at it.
Knob::ModInfo HypernovaAudioProcessorEditor::modInfoFor (const juce::String& paramId) const
{
    Knob::ModInfo info;
    const int dest = ab::modDestForParam (paramId);
    if (dest == 0) return info;
    for (int i = 0; i < ab::NumModSlots; ++i)
    {
        const juce::String p = "mod" + juce::String (i + 1);
        if ((int) processor.apvts.getRawParameterValue (p + "Src")->load() == 0) continue;
        if ((int) processor.apvts.getRawParameterValue (p + "Dest")->load() != dest) continue;
        const float amt = processor.apvts.getRawParameterValue (p + "Amt")->load();
        if (std::abs (amt) <= std::abs (info.depth)) continue;
        info.depth = amt;
        info.slot = i;
        const int src = (int) processor.apvts.getRawParameterValue (p + "Src")->load();
        info.colour = modSourceColour (src);
        info.live = processor.shownModSource[(size_t) juce::jlimit (0, 11, src)].load();
        info.highlight = src == hoveredModSource;
    }
    if (info.slot < 0 && hoveredModSource > 0)
    {
        // Hovering a source: outline everything it could reach that it isn't already reaching.
        for (int i = 0; i < ab::NumModSlots; ++i)
        {
            const juce::String p = "mod" + juce::String (i + 1);
            if ((int) processor.apvts.getRawParameterValue (p + "Src")->load() != hoveredModSource) continue;
            if ((int) processor.apvts.getRawParameterValue (p + "Dest")->load() != dest) continue;
            info.highlight = true;
            info.colour = modSourceColour (hoveredModSource);
        }
    }
    return info;
}

// Dropping a source chip on a knob fills the first free modulation slot (or reuses the matching one).
void HypernovaAudioProcessorEditor::assignMod (const juce::String& dragDescription, const juce::String& paramId)
{
    const int src = dragDescription.fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    const int dest = ab::modDestForParam (paramId);
    if (src <= 0 || dest == 0)
    {
        showMessage ("That knob can't be modulated yet");
        return;
    }
    int free = -1, existing = -1;
    for (int i = 0; i < ab::NumModSlots; ++i)
    {
        const juce::String p = "mod" + juce::String (i + 1);
        const int s = (int) processor.apvts.getRawParameterValue (p + "Src")->load();
        const int d = (int) processor.apvts.getRawParameterValue (p + "Dest")->load();
        if (s == src && d == dest) { existing = i; break; }
        if (free < 0 && (s == 0 || d == 0)) free = i;
    }
    const int slot = existing >= 0 ? existing : free;
    if (slot < 0) { showMessage ("All 8 modulation slots are in use"); return; }

    processor.undoManager.beginNewTransaction ("modulation");
    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* p = processor.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    const juce::String p = "mod" + juce::String (slot + 1);
    set (p + "Src", (float) src);
    set (p + "Dest", (float) dest);
    if (existing < 0) set (p + "Amt", 0.35f);
    showMessage (modSrcNames()[src] + " > " + modDestNames()[dest] + "   (slot " + juce::String (slot + 1) + ")");
    for (auto& k : knobs) k->repaint();
}

// Right-click a knob: change or clear what modulates it.
void HypernovaAudioProcessorEditor::showModMenu (const juce::String& paramId)
{
    const int dest = ab::modDestForParam (paramId);
    juce::PopupMenu m;
    auto* param = processor.apvts.getParameter (paramId);
    m.addSectionHeader (param != nullptr ? param->getName (40).toUpperCase() : paramId.toUpperCase());
    if (dest == 0)
    {
        m.addItem (-1, "This control can't be modulated", false, false);
    }
    else
    {
        juce::PopupMenu sources;
        const auto names = modSrcNames();
        for (int i = 1; i < names.size(); ++i) sources.addItem (100 + i, names[i]);
        m.addSubMenu ("Modulate with", sources);
        const auto info = modInfoFor (paramId);
        m.addItem (2, "Clear modulation", info.slot >= 0);
    }
    m.addItem (3, "Reset to default", param != nullptr);
    m.addSeparator();
    const bool pinned = isPinned (paramId);
    m.addItem (4, pinned ? "Unpin from pinboard" : "Pin to pinboard", param != nullptr);
    m.setLookAndFeel (&lookAndFeel);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, paramId, param, pinned] (int r)
    {
        if (r == 4) { pinParameter (paramId, ! pinned); return; }
        if (r >= 100) assignMod ("mod:" + juce::String (r - 100), paramId);
        else if (r == 2)
        {
            const auto info = modInfoFor (paramId);
            if (info.slot < 0) return;
            processor.undoManager.beginNewTransaction ("clear modulation");
            const juce::String p = "mod" + juce::String (info.slot + 1);
            for (auto* id : { "Src", "Dest" })
                if (auto* q = processor.apvts.getParameter (p + id)) q->setValueNotifyingHost (0.0f);
            if (auto* q = processor.apvts.getParameter (p + "Amt")) q->setValueNotifyingHost (q->convertTo0to1 (0.0f));
            for (auto& k : knobs) k->repaint();
        }
        else if (r == 3 && param != nullptr) param->setValueNotifyingHost (param->getDefaultValue());
    });
}

void HypernovaAudioProcessorEditor::colourKeyboard()
{
    const bool light = ThemeState::get().base.light;
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, light ? juce::Colour (0xfffbfbfd) : Colours::panelHi.get());
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, light ? juce::Colour (0xff2a2d38) : Colours::bg0.get());
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, light ? juce::Colour (0x33000000) : Colours::bg0.get());
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::oscA.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::oscA.withAlpha (0.75f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, Colours::textFaint);
}

// Pushes the current theme everywhere: shader, cached pictures, JUCE colour ids, every component.
void HypernovaAudioProcessorEditor::applyTheme()
{
    const auto& t = ThemeState::get();
    cosmos.scene = t.backdropStyle();
    cosmos.strength = t.strength();
    cosmos.base = Colours::bg0.get().getARGB();
    cosmos.accent1 = Colours::accent.get().getARGB();
    cosmos.accent2 = Colours::plasma.get().getARGB();
    lookAndFeel.refreshColours();
    colourKeyboard();
    staticLayer = juce::Image();
    fallbackBackdrop = juce::Image();
    staticFramePainted = false;
    keyboard.setVisible (ab::ui::LookSettings::flag ("keyboard", true));
    if (overlay != nullptr) relayoutWidgets (false);
    sendLookAndFeelChange();
    canvas.repaint();
    repaint();
    glContext.triggerRepaint();
}

// Look menu: themes and the user's own tweaks.
void HypernovaAudioProcessorEditor::addLookMenu (juce::PopupMenu& m)
{
    auto& t = ThemeState::get();
    juce::PopupMenu themes, accent, backdrop, strength, knobsMenu, panels;
    const auto all = ab::ui::builtInThemes();
    for (int i = 0; i < (int) all.size(); ++i)
        themes.addItem (500 + i, all[(size_t) i].name, true, all[(size_t) i].name == t.base.name);

    static const std::pair<const char*, juce::uint32> swatches[] {
        { "Ion blue", 0xff46e8ff }, { "Nebula violet", 0xff8a5cff }, { "Plasma pink", 0xffff4f9a }, { "Accretion gold", 0xffffa94a },
        { "Aurora green", 0xff5dffb0 }, { "Solar red", 0xffff5a4a }, { "Ice white", 0xfff2f5ff }, { "Acid lime", 0xffc8ff3a } };
    accent.addItem (520, "Theme's own", true, t.accentOverride.isTransparent());
    for (int i = 0; i < 8; ++i)
        accent.addItem (521 + i, swatches[i].first, true, t.accentOverride == juce::Colour (swatches[i].second));
    accent.addItem (530, "Custom...");

    const char* scenes[] = { "Cosmos (nebula and stars)", "Neon horizon", "Plain colour" };
    backdrop.addItem (540, "Theme's own", true, t.backdrop < 0);
    for (int i = 0; i < 3; ++i) backdrop.addItem (541 + i, scenes[i], true, t.backdrop == i);
    const std::pair<const char*, float> levels[] { { "Off", 0.0f }, { "Low", 0.35f }, { "Medium", 0.6f }, { "High", 0.9f } };
    for (int i = 0; i < 4; ++i)
        strength.addItem (550 + i, levels[i].first, true, std::abs (t.strength() - levels[i].second) < 0.01f);
    backdrop.addSubMenu ("Brightness", strength);

    const char* knobNames[] = { "Planet", "Ring", "Minimal", "Machined" };
    knobsMenu.addItem (560, "Theme's own", true, t.knobStyle < 0);
    for (int i = 0; i < 4; ++i) knobsMenu.addItem (561 + i, knobNames[i], true, t.knobStyle == i);
    const char* panelNames[] = { "Glass", "Flat", "Outlined" };
    panels.addItem (570, "Theme's own", true, t.panelStyle < 0);
    for (int i = 0; i < 3; ++i) panels.addItem (571 + i, panelNames[i], true, t.panelStyle == i);

    m.addSubMenu ("Theme", themes);
    m.addSubMenu ("Accent colour", accent);
    m.addSubMenu ("Backdrop", backdrop);
    m.addSubMenu ("Knobs", knobsMenu);
    m.addSubMenu ("Panels", panels);
    m.addItem (580, "Show keyboard", true, ab::ui::LookSettings::flag ("keyboard", true));
    m.addItem (581, "Always show knob values", true, t.alwaysShowValues);
}

bool HypernovaAudioProcessorEditor::handleLookMenu (int r)
{
    auto& t = ThemeState::get();
    static const juce::uint32 swatches[] { 0xff46e8ff, 0xff8a5cff, 0xffff4f9a, 0xffffa94a, 0xff5dffb0, 0xffff5a4a, 0xfff2f5ff, 0xffc8ff3a };
    const auto all = ab::ui::builtInThemes();
    if (r >= 500 && r < 500 + (int) all.size())
    {
        t.base = all[(size_t) (r - 500)];
        // A new theme brings its own look; drop the per-style overrides but keep a custom accent.
        t.backdrop = t.knobStyle = t.panelStyle = -1;
        t.backdropStrength = -1.0f;
    }
    else if (r == 520) t.accentOverride = juce::Colour();
    else if (r >= 521 && r <= 528) t.accentOverride = juce::Colour (swatches[r - 521]);
    else if (r == 530) { showColourPicker(); return true; }
    else if (r == 540) t.backdrop = -1;
    else if (r >= 541 && r <= 543) t.backdrop = r - 541;
    else if (r >= 550 && r <= 553) t.backdropStrength = std::array<float, 4> { 0.0f, 0.35f, 0.6f, 0.9f }[(size_t) (r - 550)];
    else if (r == 560) t.knobStyle = -1;
    else if (r >= 561 && r <= 564) t.knobStyle = r - 561;
    else if (r == 570) t.panelStyle = -1;
    else if (r >= 571 && r <= 573) t.panelStyle = r - 571;
    else if (r == 580) ab::ui::LookSettings::setFlag ("keyboard", ! ab::ui::LookSettings::flag ("keyboard", true));
    else if (r == 581) t.alwaysShowValues = ! t.alwaysShowValues;
    else return false;
    ++t.version;
    ab::ui::LookSettings::save();
    applyTheme();
    return true;
}

// Custom accent: a colour wheel in a call-out.
void HypernovaAudioProcessorEditor::showColourPicker()
{
    auto picker = std::make_unique<juce::ColourSelector> (juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders);
    picker->setSize (300, 320);
    picker->setCurrentColour (Colours::accent.get());
    struct Listener : juce::ChangeListener
    {
        HypernovaAudioProcessorEditor& ed;
        explicit Listener (HypernovaAudioProcessorEditor& e) : ed (e) {}
        void changeListenerCallback (juce::ChangeBroadcaster* b) override
        {
            if (auto* sel = dynamic_cast<juce::ColourSelector*> (b))
            {
                ThemeState::get().accentOverride = sel->getCurrentColour().withAlpha (1.0f);
                ++ThemeState::get().version;
                ab::ui::LookSettings::save();
                ed.applyTheme();
            }
        }
    };
    colourListener = std::make_unique<Listener> (*this);
    picker->addChangeListener (colourListener.get());
    juce::CallOutBox::launchAsynchronously (std::move (picker), gearButton.getScreenBounds(), nullptr);
}

// The Sound Space filling the whole work area, and back again: any widget can do this (double-click its title).
void HypernovaAudioProcessorEditor::setSpaceExpanded (bool expand)
{
    if (expand != (maximisedId == "space")) toggleMaximise ("space");
    space.setSolid (expand);
}

HypernovaAudioProcessorEditor::SpaceWindow::SpaceWindow (HypernovaAudioProcessor& p, int mode, std::function<void()> onGone)
    : juce::DocumentWindow ("Hypernova Sound Space", Colours::bg0, juce::DocumentWindow::closeButton),
      whenClosed (std::move (onGone))
{
    space = std::make_unique<ab::ui::SoundSpace> (p);
    space->setMode ((ab::ui::SoundSpace::Mode) mode);
    space->setSolid (true);
    space->setSize (720, 520);
    setUsingNativeTitleBar (true);
    setContentNonOwned (space.get(), true);
    setResizable (true, false);
    setResizeLimits (360, 260, 3000, 2200);
    centreWithSize (720, 520);
    setVisible (true);
    setAlwaysOnTop (true);
}

void HypernovaAudioProcessorEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sample", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.caf;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) samplerView.load (f);
                          });
}

void HypernovaAudioProcessorEditor::showSampleMenu()
{
    const auto current = processor.sampleForUi();
    if (current == nullptr) { chooseSample(); return; }
    juce::PopupMenu m;
    m.setLookAndFeel (&lookAndFeel);
    m.addSectionHeader (current->name.toUpperCase());
    m.addItem (1, "Replace with another sample...");
    m.addItem (2, "Remove the sample");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sampleButton), [this] (int r)
    {
        if (r == 1) chooseSample();
        else if (r == 2) { processor.clearSample(); processor.setParam ("smpOn", 0.0f); showMessage ("Sample removed"); }
    });
}

void HypernovaAudioProcessorEditor::showDiceMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("RANDOMISE");
    m.addItem (1, "Nudge this sound (a little)");
    m.addItem (2, "Mutate this sound (a lot)");
    m.addItem (3, "Roll a brand new bass");
    m.addSeparator();
    m.addItem (4, "Undo", processor.undoManager.canUndo());
    m.setLookAndFeel (&lookAndFeel);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&diceButton), [this] (int r)
    {
        if (r == 1) processor.mutate (0.06f);
        else if (r == 2) processor.mutate (0.22f);
        else if (r == 3) processor.randomize();
        else if (r == 4) processor.undoManager.undo();
    });
}

void HypernovaAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu size, anim, m;
    const int cur = processor.uiScalePercent.load();
    for (int pct : { 60, 75, 90, 100, 125, 150 })
        size.addItem (100 + pct, juce::String (pct) + "%", true, cur == pct);
    const int a = processor.uiAnimation.load();
    anim.addItem (10, "Full (30 fps while playing)", true, a == 0);
    anim.addItem (11, "Calm (lighter on the CPU)", true, a == 1);
    anim.addItem (12, "Off (still backdrop)", true, a == 2);
    juce::PopupMenu quality;
    const int qv = (int) processor.apvts.getRawParameterValue ("quality")->load();
    quality.addItem (30, "Eco (lightest CPU)", true, qv == 0);
    quality.addItem (31, "High (default: oversamples only patches that need it)", true, qv == 1);
    quality.addItem (32, "Ultra (4x oversampling, cleanest)", true, qv == 2);
    m.addSubMenu ("Sound quality", quality);
    juce::PopupMenu look;
    addLookMenu (look);
    m.addSubMenu ("Look", look);
    m.addSubMenu ("Window size", size);
    m.addSubMenu ("Animation", anim);
    m.addSeparator();
    m.addItem (40, "Check for updates now");
    m.addItem (41, "Check for updates automatically", true, ab::ui::Updater::autoCheckEnabled());
    m.addSeparator();
    m.addItem (20, "Show my sounds folder");
    m.addSectionHeader ("Hypernova " + ab::ui::Updater::currentVersion());
    m.setLookAndFeel (&lookAndFeel);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearButton), [this] (int r)
    {
        if (handleLookMenu (r)) return;
        if (r >= 100 && r < 500) applyScale (r - 100);
        else if (r >= 10 && r <= 12) { processor.uiAnimation = r - 10; glContext.triggerRepaint(); }
        else if (r >= 30 && r <= 32) { processor.setParam ("quality", (float) (r - 30)); showMessage ("Sound quality: " + juce::StringArray { "Eco", "High", "Ultra" }[r - 30]); }
        else if (r == 20) { HypernovaAudioProcessor::userPresetFolder().createDirectory(); HypernovaAudioProcessor::userPresetFolder().revealToUser(); }
        else if (r == 40) { showMessage ("Checking for updates..."); updater.check (true); }
        else if (r == 41) ab::ui::Updater::setAutoCheck (! ab::ui::Updater::autoCheckEnabled());
    });
}

void HypernovaAudioProcessorEditor::applyScale (int percent)
{
    percent = juce::jlimit (50, 200, percent);
    processor.uiScalePercent = percent;
    // 100% is a comfortable laptop size; the canvas scales from its fixed base layout.
    const int w = juce::roundToInt (1180.0 * percent / 100.0);
    setSize (w, juce::roundToInt ((double) w * baseHeight / baseWidth));
}

bool HypernovaAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { processor.undoManager.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { processor.undoManager.redo(); return true; }
    return false;
}

void HypernovaAudioProcessorEditor::setBrowserOpen (bool open)
{
    if (open)
    {
        browser.refresh();
        browser.setAlpha (0.0f);
        browser.setVisible (true);
        browser.toFront (false);
        browser.showCurrent();
        browser.grabSearchFocus();
        juce::Desktop::getInstance().getAnimator().animateComponent (&browser, browser.getBounds(), 1.0f, 140, false, 1.0, 0.0);
    }
    else
    {
        juce::Desktop::getInstance().getAnimator().cancelAnimation (&browser, false);
        browser.setVisible (false);
        browser.setAlpha (1.0f);
    }
}
