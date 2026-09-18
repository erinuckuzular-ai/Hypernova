#include "PluginEditor.h"
#include <map>

using namespace ab;
using namespace ab::ui;

namespace
{
    // Base-canvas layout
    const juce::Rectangle<int> oscAPanel   { 24, 90, 400, 350 };
    const juce::Rectangle<int> oscBPanel   { 436, 90, 400, 350 };
    const juce::Rectangle<int> spacePanel  { 848, 90, 408, 350 };
    const juce::Rectangle<int> subPanel    { 24, 452, 240, 186 };
    const juce::Rectangle<int> voicePanel  { 276, 452, 256, 186 };
    const juce::Rectangle<int> filterPanel { 544, 452, 332, 186 };
    const juce::Rectangle<int> envPanel    { 888, 452, 368, 186 };
    const juce::Rectangle<int> deckPanel   { 24, 650, 1232, 176 };
    const juce::Rectangle<int> deckContent { 24, 688, 1232, 136 };
    const juce::Rectangle<int> keysArea    { 24, 840, 1232, 60 };

    const juce::Rectangle<int> messageArea { 334, 68, 496, 16 };

    juce::Rectangle<int> at (const juce::Rectangle<int>& panel, int x, int y, int w, int h) { return { panel.getX() + x, panel.getY() + y, w, h }; }
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
Knob& HypernovaAudioProcessorEditor::knob (const juce::String& id, const juce::String& label, juce::Colour c, juce::Rectangle<int> bounds, int size,
                                            juce::Component* parent)
{
    knobs.push_back (std::make_unique<Knob> (processor.apvts, id, label, c, size));
    auto& k = *knobs.back();
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
      viewA (p, 0, Palette::oscA), viewB (p, 1, Palette::oscB), space (p), filterView (p),
      ampView (p, "amp", Palette::env, true), modView (p, "mod", Palette::lfo, false),
      lfoView1 (p, 0), lfoView2 (p, 1),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (false);
    addAndMakeVisible (canvas);
    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };

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
    for (auto* b : std::initializer_list<juce::Component*> { &presetPlate, &prevButton, &nextButton, &diceButton, &saveButton, &undoButton, &redoButton, &gearButton })
        canvas.addAndMakeVisible (b);

    spaceMode.onChange = [this] (int m) { space.setMode ((SoundSpace::Mode) m); };

    keyboard.setAvailableRange (24, 96);
    keyboard.setOctaveForMiddleC (3); // Ableton naming: C3 = middle C
    keyboard.setLowestVisibleKey (24);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xff1a2130));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, Colours::bg0);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, Colours::bg0);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::oscA.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::oscA.withAlpha (0.75f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, Colours::textFaint);
    canvas.addAndMakeVisible (keyboard);
    keyboard.setBounds (keysArea);
    keyboard.setKeyWidth ((float) keysArea.getWidth() / 43.0f);

    refreshPresetInfo();
    showDeckPage (processor.uiDeckPage);

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
    presetPlate.setBounds (370, 22, 226, 44);
    nextButton.setBounds (600, 22, 32, 44);
    diceButton.setBounds (640, 22, 40, 44);
    saveButton.setBounds (684, 22, 40, 44);
    undoButton.setBounds (732, 22, 30, 44);
    redoButton.setBounds (764, 22, 30, 44);
    gearButton.setBounds (798, 22, 32, 44);
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
        toggle (std::make_unique<PowerLed> (c), p + "On", at (P, 10, 10, 26, 26), "Oscillator on/off");
        combo (p + "Table", WavetableBank::names(), at (P, 92, 11, 138, 24));
        combo (p + "Warp", warpNames(), at (P, 236, 11, 80, 24));
        toggle (std::make_unique<PillToggle> ("FILTER", c), p + "Filter", at (P, 322, 12, 66, 22), "Send this oscillator through the filter");
        (o == 0 ? viewA : viewB).setBounds (at (P, 12, 44, 376, 160));

        const char* ids1[] = { "Pos", "WarpAmt", "Uni", "Detune", "Blend" };
        const char* names1[] = { "POSITION", "WARP", "UNISON", "DETUNE", "BLEND" };
        const char* ids2[] = { "Level", "Pan", "Oct", "Semi", "Fine" };
        const char* names2[] = { "LEVEL", "PAN", "OCTAVE", "SEMI", "FINE" };
        for (int i = 0; i < 5; ++i)
        {
            knob (p + ids1[i], names1[i], c, at (P, 12 + i * 75, 212, 75, 68));
            knob (p + ids2[i], names2[i], c, at (P, 12 + i * 75, 280, 75, 68));
        }
    }

    // Sound space
    spaceMode.setBounds (at (spacePanel, 228, 11, 168, 24));
    space.setBounds (at (spacePanel, 12, 44, 384, 294));

    // Sub + noise
    toggle (std::make_unique<PowerLed> (Palette::sub), "subOn", at (subPanel, 8, 8, 26, 26), "Sub oscillator on/off");
    combo ("subShape", subShapeNames(), at (subPanel, 12, 40, 104, 24));
    knob ("subOct", "OCTAVE", Palette::sub, at (subPanel, 6, 72, 56, 68), 40);
    knob ("subLevel", "LEVEL", Palette::sub, at (subPanel, 62, 72, 56, 68), 40);
    toggle (std::make_unique<PillToggle> ("FILTER", Palette::sub), "subFilter", at (subPanel, 12, 152, 104, 22), "Send the sub through the filter (off keeps the low end clean)");
    knob ("noiseLevel", "NOISE", Colours::textDim, at (subPanel, 126, 72, 56, 68), 40);
    knob ("noiseTone", "TONE", Colours::textDim, at (subPanel, 180, 72, 56, 68), 40);
    toggle (std::make_unique<PillToggle> ("FILTER", Colours::textDim), "noiseFilter", at (subPanel, 128, 152, 104, 22), "Send the noise through the filter");

    // Pitch + voice
    knob ("dropAmt", "DROP", Palette::sub, at (voicePanel, 10, 38, 59, 68));
    knob ("dropTime", "DROP TIME", Palette::sub, at (voicePanel, 69, 38, 59, 68));
    knob ("glide", "GLIDE", Palette::sub, at (voicePanel, 128, 38, 59, 68));
    knob ("bendRange", "BEND", Palette::sub, at (voicePanel, 187, 38, 59, 68));
    combo ("mode", voiceModeNames(), at (voicePanel, 12, 118, 100, 24));
    toggle (std::make_unique<PillToggle> ("RETRIG", Palette::sub), "retrig", at (voicePanel, 12, 150, 100, 22),
            "Restart the waveform on every note: same punch every hit (808s, log drums)");
    knob ("velSens", "VELOCITY", Palette::sub, at (voicePanel, 128, 110, 59, 68));

    // Filter
    toggle (std::make_unique<PowerLed> (Palette::filter), "fltOn", at (filterPanel, 8, 8, 26, 26), "Filter on/off");
    combo ("fltType", filterNames(), at (filterPanel, 190, 11, 130, 24));
    filterView.setBounds (at (filterPanel, 12, 42, 308, 62));
    const char* fIds[] = { "cutoff", "res", "fltDrive", "fltEnv", "fltKey", "fltMix" };
    const char* fNames[] = { "CUTOFF", "RES", "DRIVE", "ENV", "KEY", "MIX" };
    for (int i = 0; i < 6; ++i)
        knob (fIds[i], fNames[i], Palette::filter, at (filterPanel, 12 + i * 51, 110, 51, 68), 40);

    // Envelopes
    ampView.setBounds (at (envPanel, 12, 36, 164, 64));
    modView.setBounds (at (envPanel, 192, 36, 164, 64));
    const char* adsr[] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        knob (juce::String ("amp") + adsr[i], adsr[i], Palette::env, at (envPanel, 12 + i * 41, 108, 41, 66), 36);
        knob (juce::String ("mod") + adsr[i], adsr[i], Palette::lfo, at (envPanel, 192 + i * 41, 108, 41, 66), 36);
    }

    // Tabbed deck
    for (auto& pg : pages)
    {
        canvas.addChildComponent (pg);
        pg.setBounds (deckContent);
    }
    deckTabs.setBounds (deckPanel.getX() + 12, deckPanel.getY() + 9, 390, 26);
    deckTabs.onChange = [this] (int i) { showDeckPage (i); };
    canvas.addAndMakeVisible (deckTabs);
    layoutModPage();
    layoutFxPage();
    layoutPlayPage();
}

//==============================================================================
// The canvas is three layers: the backdrop (GPU shader, or a cached CPU picture when there's no OpenGL),
// a cached image of everything static (panels, titles, logo type), and a few live bits drawn on top.
void HypernovaAudioProcessorEditor::paintCanvas (juce::Graphics& g)
{
    if (! cosmosOnGpu())
    {
        if (! fallbackBackdrop.isValid())
            fallbackBackdrop = renderCosmosFallback (baseWidth, baseHeight, 2.0f, logoHole, logoHoleRadius);
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
    // Wordmark next to the black hole (the hole itself is drawn by the backdrop).
    {
        g.setGradientFill (juce::ColourGradient (Colours::text, 84, 20, juce::Colour (0xffc7b4ff), 330, 48, false));
        g.setFont (heavy (24.0f).withExtraKerningFactor (0.16f));
        g.drawText ("HYPERNOVA", juce::Rectangle<float> (84, 19, 250, 30), juce::Justification::centredLeft, false);
        g.setColour (Colours::textDim);
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.34f));
        g.drawText ("WAVETABLE SPACE SYNTH", juce::Rectangle<float> (85, 48, 250, 16), juce::Justification::centredLeft, false);
    }

    auto titled = [&] (const juce::Rectangle<int>& r, const juce::String& title, juce::Colour c, int titleX = 14)
    {
        panel (g, r.toFloat(), 14.0f);
        sectionLabel (g, title, juce::Rectangle<float> ((float) r.getX() + (float) titleX, (float) r.getY() + 10, 200, 24), c);
    };

    titled (oscAPanel, "OSC A", Palette::oscA, 40);
    titled (oscBPanel, "OSC B", Palette::oscB, 40);
    titled (spacePanel, "SOUND SPACE", Colours::text);
    titled (subPanel, "SUB", Palette::sub, 38);
    sectionLabel (g, "NOISE", juce::Rectangle<float> ((float) subPanel.getX() + 130, (float) subPanel.getY() + 10, 80, 24), Colours::textDim);
    g.setColour (Colours::line);
    g.drawVerticalLine (subPanel.getX() + 121, (float) subPanel.getY() + 14, (float) subPanel.getBottom() - 14);
    titled (voicePanel, "PITCH + VOICE", Palette::sub);
    titled (filterPanel, "FILTER", Palette::filter, 38);
    titled (envPanel, "AMP ENV", Palette::env);
    sectionLabel (g, "MOD ENV", juce::Rectangle<float> ((float) envPanel.getX() + 194, (float) envPanel.getY() + 10, 120, 24), Palette::lfo);
    panel (g, deckPanel.toFloat(), 14.0f);
    g.setColour (Colours::line);
    g.drawHorizontalLine (deckPanel.getY() + 42, (float) deckPanel.getX() + 12, (float) deckPanel.getRight() - 12);

    auto tray = juce::Rectangle<float> (838, 8, 342, 76);
    g.setColour (Colours::inset.withAlpha (0.55f));
    g.fillRoundedRectangle (tray, 12.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (tray.reduced (0.5f), 12.0f, 1.0f);
}

void HypernovaAudioProcessorEditor::paintDynamic (juce::Graphics& g)
{
    static const char* hints[] = { "LFOs and the mod matrix: any source to any destination, including the effects",
                                   "effects run top to bottom: distortion > OTT > chorus > delay > space > EQ",
                                   "arpeggiator, one-key chords, tuning and unison width" };
    g.setColour (Colours::textFaint);
    g.setFont (font (10.5f));
    g.drawText (hints[juce::jlimit (0, 2, processor.uiDeckPage)], juce::Rectangle<float> ((float) deckPanel.getX() + 420, (float) deckPanel.getY() + 9,
                (float) deckPanel.getWidth() - 434, 26), juce::Justification::centredRight, false);

    const int note = processor.shownNote.load();
    const int voices = processor.shownVoices.load();
    g.setColour (Colours::textDim);
    g.setFont (mono (10.0f));
    const juce::String info = note >= 0 ? juce::MidiMessage::getMidiNoteName (note, true, true, 3) + "   " + juce::String (voices) + (voices == 1 ? " voice" : " voices")
                                        : "play a note";
    g.drawText (info, juce::Rectangle<float> ((float) spacePanel.getX() + 118, (float) spacePanel.getY() + 10, 104, 24), juce::Justification::centredRight, false);

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
        cosmos.holeX = logoHole.x * k;
        cosmos.holeY = logoHole.y * k;
        cosmos.holeR = logoHoleRadius * k;
        const bool gpu = cosmosOnGpu();
        if (gpu != lastGpu) { lastGpu = gpu; canvas.repaint(); }
        // Backdrop frame rate: 30 fps while notes sound, 12 fps when idle, or per the Animation setting.
        const int mode = processor.uiAnimation.load(); // 0 full, 1 calm, 2 off
        const bool busy = processor.shownVoices.load() > 0;
        const int every = mode == 2 ? 0 : (mode == 1 ? (busy ? 2 : 5) : (busy ? 1 : 3));
        ++frameTick;
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
    viewA.refresh (sounding);
    viewB.refresh (sounding);
    space.refresh (sounding);
    // Small views: only while sound plays (their values move), plus a slow tick otherwise for parameter edits.
    if (sounding || (++idleTicks % 6) == 0)
    {
        filterView.repaint();
        ampView.repaint();
        modView.repaint();
        lfoView1.repaint();
        lfoView2.repaint();
    }
    const int readout = processor.shownNote.load() * 100 + processor.shownVoices.load();
    if (readout != lastReadout)
    {
        lastReadout = readout;
        canvas.repaint (spacePanel.getX() + 110, spacePanel.getY() + 8, 120, 28);
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
                                                   juce::String ("*") + HypernovaAudioProcessor::presetExtension + ";*.abpreset");
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
        if (f.isDirectory() || HypernovaAudioProcessor::isPresetFile (f)) return true;
    }
    return false;
}

void HypernovaAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int) { dragHover = true; repaint(); }
void HypernovaAudioProcessorEditor::fileDragExit (const juce::StringArray&) { dragHover = false; repaint(); }

void HypernovaAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    repaint();
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
            g.drawVerticalLine (c.area.getX(), (float) c.area.getY(), (float) c.area.getBottom());
            continue;
        }
        g.setColour (c.colour);
        if (c.colour == Colours::textFaint) g.setFont (font (11.0f)); // body text
        else g.setFont (font (9.5f, true).withExtraKerningFactor (0.2f));
        g.drawText (c.text, c.area, juce::Justification::centredLeft, false);
    }
}

void HypernovaAudioProcessorEditor::showDeckPage (int page)
{
    processor.uiDeckPage = juce::jlimit (0, 2, page);
    for (int i = 0; i < 3; ++i)
        pages[(size_t) i].setVisible (i == processor.uiDeckPage);
    deckTabs.setSelected (processor.uiDeckPage);
    canvas.repaint (deckPanel);
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
    auto group = [&] (const juce::String& title, int cells) -> int
    {
        const int start = x;
        pg->captions.push_back ({ { x, 4, cells * cell, 24 }, title, Colours::textDim, false });
        x += cells * cell + gap;
        pg->captions.push_back ({ { x - gap / 2, 6, 1, 122 }, {}, {}, true });
        return start;
    };
    auto fxKnob = [&] (const char* id, const char* label, int gx, int i) { knob (id, label, Palette::fx, { gx + i * cell, knobY, cell, 72 }, 42, pg); };

    int g = group ("DIST", 2);
    combo ("distType", distNames(), { g + 42, 4, 2 * cell - 42, 24 }, pg);
    fxKnob ("distDrive", "DRIVE", g, 0);
    fxKnob ("distMix", "MIX", g, 1);

    g = group ("OTT", 1);
    fxKnob ("ott", "SQUASH", g, 0);

    g = group ("CHORUS", 2);
    fxKnob ("chorusRate", "RATE", g, 0);
    fxKnob ("chorusMix", "MIX", g, 1);

    g = group ("DELAY", 3);
    combo ("dlyTime", delayTimeNames(), { g + 52, 4, 78, 24 }, pg);
    toggle (std::make_unique<PillToggle> ("PING", Palette::fx), "dlyPing", { g + 134, 5, 3 * cell - 134, 22 }, "Ping-pong: repeats bounce left and right", pg);
    fxKnob ("dlyFb", "FEEDBACK", g, 0);
    fxKnob ("dlyTone", "TONE", g, 1);
    fxKnob ("dlyMix", "MIX", g, 2);

    g = group ("SPACE", 3);
    fxKnob ("verbSize", "SIZE", g, 0);
    fxKnob ("verbShimmer", "SHIMMER", g, 1);
    fxKnob ("verbMix", "MIX", g, 2);

    g = group ("EQ", 2);
    fxKnob ("eqLow", "LOW", g, 0);
    fxKnob ("eqHigh", "HIGH", g, 1);

    g = group ("OUTPUT", 3);
    toggle (std::make_unique<PillToggle> ("MONO BASS", Palette::fx), "monoBass", { g + cell + 6, knobY + 12, 2 * cell - 12, 24 },
            "Keeps everything under 120 Hz in mono so the bass hits hard on club systems", pg);
    fxKnob ("width", "WIDTH", g, 0);
    pg->captions.pop_back(); // no divider after the last group
}

void HypernovaAudioProcessorEditor::layoutPlayPage()
{
    auto* pg = &pages[2];
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

    pg->captions.push_back ({ { 938, 4, 280, 24 }, "TIPS", Colours::textDim, false });
    pg->captions.push_back ({ { 938, 30, 282, 20 }, "Legato + GLIDE: overlap notes to slide.", Colours::textFaint, false });
    pg->captions.push_back ({ { 938, 50, 282, 20 }, "Chords need Poly mode (Pitch + Voice panel).", Colours::textFaint, false });
    pg->captions.push_back ({ { 938, 70, 282, 20 }, "Double-click any knob to reset it.", Colours::textFaint, false });
    pg->captions.push_back ({ { 938, 90, 282, 20 }, "Drag the 3D views to fly around them.", Colours::textFaint, false });
}

//==============================================================================
void HypernovaAudioProcessorEditor::showDiceMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("RANDOMISE");
    m.addItem (1, "Nudge this sound (a little)");
    m.addItem (2, "Mutate this sound (a lot)");
    m.addItem (3, "Roll a brand new bass");
    m.addSeparator();
    m.addItem (4, "Undo", processor.undoManager.canUndo());
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
    m.addSubMenu ("Window size", size);
    m.addSubMenu ("Animation", anim);
    m.addSeparator();
    m.addItem (40, "Check for updates now");
    m.addItem (41, "Check for updates automatically", true, ab::ui::Updater::autoCheckEnabled());
    m.addSeparator();
    m.addItem (20, "Show my sounds folder");
    m.addSectionHeader ("Hypernova " + ab::ui::Updater::currentVersion());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearButton), [this] (int r)
    {
        if (r >= 100) applyScale (r - 100);
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
