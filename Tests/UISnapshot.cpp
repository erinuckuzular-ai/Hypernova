#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

// Renders the editor to PNGs without a DAW: UISnapshot <outDir>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    setenv ("HYPERNOVA_WORKSPACE_FOLDER", "Arrow/Hypernova/UISnapshot", 1);
    juce::File ("~/Library/Application Support/Arrow/Hypernova/UISnapshot").deleteRecursively();
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    HypernovaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    auto snap = [&] (const juce::String& name, const juce::String& preset, int mode, int page,
                     const juce::String& workspace = "Sound Design", bool editing = false,
                     std::function<void (HypernovaAudioProcessorEditor&)> prepare = {})
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            if (proc.getProgramName (i) == preset) proc.setCurrentProgram (i);

        std::unique_ptr<HypernovaAudioProcessorEditor> editor (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        editor->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        editor->loadWorkspace (workspace, false);
        if (workspace == "Effects")
        {
            proc.setParam ("lowOn", 1.0f); proc.setParam ("lowDuck", 0.5f); proc.setParam ("distMix", 0.6f);
            for (int fx : { ab::FxFilter, ab::FxGate, ab::FxDelay, ab::FxReverb }) proc.addToRack (fx);
            proc.setParam ("fxFltDepth", 0.5f); proc.setParam ("eqMidGain", 5.0f); proc.setParam ("eqMidFreq", 900.0f); proc.setParam ("eqLowCut", 60.0f);
            proc.setParam ("dlyFb", 0.55f);
        }

        // Hold a note while the visualisers run so the 3D views have something to show.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);
        for (int i = 0; i < 40; ++i)
        {
            juce::AudioBuffer<float> buf (2, 512);
            proc.processBlock (buf, midi);
            midi.clear();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
        }
        editor->setSpaceMode (mode);
        editor->setDeckPage (page);
        editor->setLayoutEditing (editing);
        if (editing) editor->setLibraryOpen (true);
        if (prepare) prepare (*editor);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (prepare ? 400 : 80);
        editor->finishMotion();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());

        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
        juce::AudioBuffer<float> buf (2, 512);
        proc.processBlock (buf, off);
    };

    // --layouttest: the docking rules, driven through the editor's real operations and the overlay.
    if (argc > 2 && juce::String (argv[2]) == "--layouttest")
    {
        using namespace ab::ui;
        int failures = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8()); failures += ok ? 0 : 1; };

        // Pure tree logic first.
        {
            dock::Tree t;
            t.fromValueTree (dock::split (false, 1, { dock::split (true, 1, { dock::leaf ({ "a" }, 1), dock::leaf ({ "b" }, 1) }), dock::leaf ({ "c", "d" }, 1) }));
            auto minSize = [] (const juce::String&) { return juce::Point<int> (100, 60); };
            t.layout ({ 0, 0, 1000, 600 }, minSize);
            check (t.findLeaf ("a")->bounds.getWidth() + t.findLeaf ("b")->bounds.getWidth() + dock::gutter == 1000, "tree: a row fills its width exactly");
            t.remove ("a");
            t.layout ({ 0, 0, 1000, 600 }, minSize);
            check (t.findLeaf ("b")->bounds.getWidth() == 1000, "tree: removing a widget gives its space to the neighbour");
            t.insert ("a", t.findLeaf ("c"), dock::Zone::Stack);
            check (t.findLeaf ("a") == t.findLeaf ("c") && t.findLeaf ("c")->activeId() == "a", "tree: stacking adds a tab in front");
            t.insert ("e", t.findLeaf ("b"), dock::Zone::Top);
            t.layout ({ 0, 0, 1000, 600 }, minSize);
            check (t.findLeaf ("e")->bounds.getBottom() < t.findLeaf ("b")->bounds.getY(), "tree: an edge drop splits on that side");
            for (auto id : { "a", "b", "c", "d" }) t.remove (id);
            check (t.getRoot()->isLeaf && t.allWidgets() == juce::StringArray { "e" }, "tree: emptied splits fold away");
            juce::ValueTree v = t.toValueTree();
            dock::Tree t2;
            t2.fromValueTree (v);
            check (t2.allWidgets() == juce::StringArray { "e" }, "tree: round-trips through its saved form");
        }

        // Motion: springs that settle without overshoot, redirect without jumping, and edges that give.
        {
            namespace m = ab::ui::motion;
            m::Spring sp;
            sp.snap (0.0f);
            sp.target = 100.0f;
            float peak = 0.0f, t = 0.0f;
            while (sp.step (1.0f / 60.0f) && t < 3.0f) { peak = juce::jmax (peak, sp.value); t += 1.0f / 60.0f; }
            check (peak <= 100.01f && sp.value == 100.0f && t < 0.9f, "a critically damped spring lands without overshoot (" + juce::String (t, 2) + " s)");
            sp.snap (0.0f);
            sp.target = 100.0f;
            for (int i = 0; i < 6; ++i) sp.step (1.0f / 60.0f);
            const float before = sp.value, speed = sp.velocity;
            sp.target = -50.0f; // change of mind mid-flight
            sp.step (1.0f / 60.0f);
            check (std::abs (sp.value - before) < speed / 60.0f + 1.0f && sp.value > before - 1.0f, "redirecting mid-flight carries on from where it is, no jump");
            m::Spring thrown;
            thrown.snap (0.0f);
            thrown.velocity = 2000.0f;
            thrown.step (1.0f / 60.0f);
            check (thrown.value > 20.0f, "a thrown spring keeps the speed it was let go at");
            check (std::abs (m::project (1000.0f) - 499.0f) < 0.5f, "a flick projects to where it would come to rest");
            check (m::rubberband (200.0f, 400.0f) < 200.0f && m::rubberband (400.0f, 400.0f) > m::rubberband (200.0f, 400.0f)
                   && m::rubberband (400.0f, 400.0f) - m::rubberband (200.0f, 400.0f) < m::rubberband (200.0f, 400.0f),
                   "past an edge it follows less and less");
        }

        std::unique_ptr<HypernovaAudioProcessorEditor> ed (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        ed->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        ed->loadWorkspace ("Sound Design", false);
        auto stateBefore = proc.apvts.copyState().toXmlString();
        const auto area = ed->layoutArea();

        // Structural changes slide into place; let them land before measuring.
        auto settle = [&] { juce::MessageManager::getInstance()->runDispatchLoopUntil (300); ed->finishMotion(); };
        // Every visible widget inside the area, and no two overlapping.
        auto tidy = [&] (const char* when)
        {
            settle();
            std::vector<juce::Rectangle<int>> shown;
            for (auto& id : ed->layoutTree().allWidgets())
                if (auto* w = ed->findWidget (id); w != nullptr && w->isVisible()) shown.push_back (w->getBounds());
            bool ok = ! shown.empty();
            for (size_t a = 0; a < shown.size(); ++a)
            {
                ok &= area.contains (shown[a]);
                for (size_t b = a + 1; b < shown.size(); ++b) ok &= ! shown[a].intersects (shown[b]);
            }
            check (ok, juce::String ("no gaps outside, no overlaps: ") + when);
        };
        auto* oscA = ed->findWidget ("oscA");
        auto* sub = ed->findWidget ("sub");
        auto* pitch = ed->findWidget ("pitch");
        check (oscA != nullptr && std::abs (oscA->getX() - 24) <= 1 && std::abs (oscA->getWidth() - 400) <= 8 && std::abs (oscA->getHeight() - 378) <= 8,
               "Sound Design is the classic layout (" + oscA->getBounds().toString() + ")");
        check (ed->findWidget ("mod")->isVisible() && ! ed->findWidget ("rack")->isVisible()
               && ed->layoutTree().findLeaf ("mod") == ed->layoutTree().findLeaf ("rack"), "deck pages share one place as tabs");
        tidy ("default");

        // Normal mode: the overlay only takes the gutters, never a control.
        auto& overlayRef = *ed->getChildComponent (0);
        juce::ignoreUnused (overlayRef);
        juce::Component* overlay = nullptr;
        std::function<void (juce::Component&)> findOverlay = [&] (juce::Component& c)
        {
            for (auto* ch : c.getChildren()) { if (dynamic_cast<DockOverlay*> (ch) != nullptr) overlay = ch; findOverlay (*ch); }
        };
        findOverlay (*ed);
        check (overlay != nullptr, "overlay exists");
        const auto knobPoint = oscA->getBounds().getCentre() - overlay->getPosition();
        const auto gutterPoint = juce::Point<int> (oscA->getRight() + 6, oscA->getBounds().getCentreY()) - overlay->getPosition();
        check (! overlay->hitTest (knobPoint.x, knobPoint.y), "normal mode: clicks on a panel reach its controls");
        check (overlay->hitTest (gutterPoint.x, gutterPoint.y), "normal mode: the gap between panels can be dragged");

        // Drag that divider through the overlay's own mouse handling: osc A gets wider, osc B narrower.
        {
            const int before = oscA->getWidth();
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            auto ev = [&] (juce::Point<int> p) { return juce::MouseEvent (src, p.toFloat(), {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, overlay, overlay,
                                                                         juce::Time::getCurrentTime(), gutterPoint.toFloat(), juce::Time::getCurrentTime(), 1, false); };
            auto* dockOverlay = dynamic_cast<DockOverlay*> (overlay);
            dockOverlay->takeDirty();
            overlay->mouseDown (ev (gutterPoint));
            overlay->mouseDrag (ev (gutterPoint + juce::Point<int> (60, 0)));
            const auto dirty = dockOverlay->takeDirty();
            check (dirty.getWidth() * dirty.getHeight() < overlay->getWidth() * overlay->getHeight() / 10,
                   "dragging a gap repaints only its bar, not the whole overlay (" + dirty.toString() + ")");
            check (! oscA->content.isVisible(), "while it's being resized, a panel shows a picture instead of laying out its contents at every step");
            overlay->mouseUp (ev (gutterPoint + juce::Point<int> (60, 0)));
            check (std::abs (oscA->getWidth() - (before + 60)) <= 1, "dragging a gap resizes the neighbours (" + juce::String (before) + " -> " + juce::String (oscA->getWidth()) + ")");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
            check (oscA->content.isVisible() && oscA->content.getWidth() > 0, "and lays them out once it stops");
            tidy ("after a resize");
        }

        // Grab a panel by its title in normal use and drop it on another: it joins it as a tab.
        {
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            auto* oscB = ed->findWidget ("oscB");
            const auto title = oscB->getPosition() + oscB->grabZone().getCentre() - overlay->getPosition();
            check (overlay->hitTest (title.x, title.y), "normal mode: a panel's title can be grabbed");
            const auto onto = oscA->getBounds().getCentre() - overlay->getPosition();
            auto ev = [&] (juce::Point<int> p) { return juce::MouseEvent (src, p.toFloat(), {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, overlay, overlay,
                                                                         juce::Time::getCurrentTime(), title.toFloat(), juce::Time::getCurrentTime(), 1, false); };
            overlay->mouseDown (ev (title));
            overlay->mouseDrag (ev (title + juce::Point<int> (-30, 10)));
            overlay->mouseDrag (ev (onto));
            overlay->mouseUp (ev (onto));
            settle();
            check (ed->layoutTree().findLeaf ("oscB") == ed->layoutTree().findLeaf ("oscA"), "dragging a title onto a panel stacks them");
            check (ed->findWidget ("space")->getX() == ed->layoutTree().findLeaf ("oscA")->bounds.getRight() + dock::gutter, "and the rest closes up");
            ed->undoLayout();
            settle();
            check (ed->layoutTree().findLeaf ("oscB") != ed->layoutTree().findLeaf ("oscA") && oscA->getWidth() > 420, "undo puts it back");
        }

        // Bigger shows more: the wavetable view grows with the panel instead of the knobs zooming.
        {
            WavetableView* view = nullptr;
            for (auto* c : oscA->content.getChildren()) if (auto* v = dynamic_cast<WavetableView*> (c)) view = v;
            const int designW = view->getWidth();
            ed->toggleMaximise ("oscA"); settle();
            check (oscA->getBounds() == area, "maximise fills the work area");
            check (view->getWidth() > designW + 200, "maximised, the wavetable display grows (" + juce::String (designW) + " -> " + juce::String (view->getWidth()) + ")");
            check (oscA->scale() <= 1.26f, "and the knobs don't balloon (scale " + juce::String (oscA->scale(), 2) + ")");
            ed->toggleMaximise ("oscA"); settle();
            check (oscA->getBounds() != area && ed->findWidget ("oscB")->isVisible(), "restore brings the others back");
        }

        // Hide: the neighbours close the gap, and the sound doesn't change.
        ed->hideWidget ("sub"); settle();
        check (! sub->isVisible(), "hide takes it off screen");
        check (pitch->getX() == area.getX(), "its neighbour moves over: no hole");
        check (proc.apvts.copyState().toXmlString() == stateBefore, "hiding never changes the sound");
        tidy ("after hide");

        // Add from the library: a new scope finds room of its own.
        const auto scopeId = ed->addWidgetType ("scope");
        auto* scope = ed->findWidget (scopeId);
        check (scope != nullptr && scope->isVisible(), "a scope can be added");
        const auto scope2 = ed->addWidgetType ("scope");
        check (scope2 != scopeId && ed->findWidget (scope2) != nullptr, "and a second one (" + scope2 + ")");
        tidy ("after adding two scopes");

        // Moves: stack onto a place, split beside one, dock along the whole bottom.
        ed->moveWidget ("filter", "oscA", dock::Zone::Stack); settle();
        check (ed->layoutTree().findLeaf ("filter") == ed->layoutTree().findLeaf ("oscA") && ed->findWidget ("filter")->isVisible(),
               "drop in the middle stacks as a tab (and shows it)");
        ed->activateWidget ("oscA");
        check (oscA->isVisible() && ! ed->findWidget ("filter")->isVisible(), "tabs switch which one is in front");
        ed->moveWidget ("filter", "space", dock::Zone::Right); settle();
        check (ed->findWidget ("filter")->getX() > ed->findWidget ("space")->getX() && ed->findWidget ("filter")->isVisible(), "drop near an edge splits beside it");
        ed->moveWidget (scope2, {}, dock::Zone::Bottom); settle();
        check (ed->findWidget (scope2)->getWidth() == area.getWidth() && ed->findWidget (scope2)->getBottom() == area.getBottom(),
               "drop on the window edge docks across the whole side");
        tidy ("after moves");

        // Collapse into a strip and back.
        const int envW = ed->findWidget ("env")->getWidth();
        ed->toggleCollapse ("env"); settle();
        check (ed->findWidget ("env")->getWidth() == dock::collapsedSpine, "collapse folds a widget in a row into a spine");
        ed->toggleCollapse ("env"); settle();
        check (std::abs (ed->findWidget ("env")->getWidth() - envW) <= 2, "and opens it again");

        // Replace and duplicate.
        ed->replaceWidget (scopeId, "meter");
        check (ed->findWidget (scopeId) == nullptr && ! ed->layoutTree().contains (scopeId), "replace swaps a widget for another in the same place");
        const auto xy = ed->addWidgetType ("xy");
        if (auto* t = ed->toolFor (xy)) t->config.setProperty ("x", 2, nullptr);
        const auto xy2 = ed->duplicateWidget (xy);
        check (ed->toolFor (xy2) != nullptr && (int) ed->toolFor (xy2)->config.getProperty ("x") == 2, "duplicate copies a tool's settings");
        tidy ("after replace and duplicate");

        // Pin a knob: a pinboard appears and holds it.
        ed->pinParameter ("cutoff", true);
        check (ed->isPinned ("cutoff"), "pinning a knob puts it on a pinboard");
        ed->pinParameter ("res", true);
        check (ed->isPinned ("res"), "more knobs join the same pinboard");
        ed->pinParameter ("cutoff", false);
        check (! ed->isPinned ("cutoff") && ed->isPinned ("res"), "and unpinning takes just that one off");

        // Layout mode: the overlay takes everything so knobs are locked.
        ed->setLayoutEditing (true);
        check (overlay->hitTest (knobPoint.x, knobPoint.y) && oscA->editing, "layout mode locks the controls");
        ed->setLayoutEditing (false);

        // Undo and redo walk the layout history.
        const auto now = ed->captureLayout().toXmlString();
        ed->undoLayout();
        check (ed->captureLayout().toXmlString() != now, "undo steps back");
        ed->redoLayout();
        check (ed->captureLayout().toXmlString() == now, "redo returns");

        // Workspaces: each keeps its own arrangement, and switching never touches the sound.
        ed->loadWorkspace ("Analysis", false);
        check (ed->findWidget ("scope-1") != nullptr && ed->findWidget ("scope-1")->isVisible() && ed->findWidget ("meter-1")->isVisible(),
               "the Analysis workspace brings its scope and meter");
        tidy ("Analysis");
        ed->loadWorkspace ("Effects", false);
        check (ed->findWidget ("rack")->isVisible() && ed->findWidget ("rack")->getWidth() == area.getWidth() && ! ed->findWidget ("oscA")->isVisible(), "Effects workspace");
        tidy ("Effects");

        // The effects rack: add effects, drag one along the chain by its name, remove one, and it scrolls when squeezed.
        {
            ed->loadWorkspace ("Effects", false);
            settle();
            EffectsRack* rackView = nullptr;
            for (auto* c : ed->findWidget ("rack")->content.getChildren()) if (auto* v = dynamic_cast<EffectsRack*> (c)) rackView = v;
            check (rackView != nullptr && rackView->isVisible(), "the Effects workspace shows the rack");
            for (int fx : { ab::FxDist, ab::FxDelay, ab::FxReverb }) proc.addToRack (fx);
            rackView->refresh (true);
            check (rackView->shownCount() == 3 && proc.fxAudible (ab::FxDelay), "added effects appear as modules, and are heard");
            check (! rackView->isScrollable(), "three modules fit without scrolling (" + rackView->metrics() + ")");
            auto& dist = rackView->module (ab::FxDist);
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            auto ev = [&] (juce::Point<float> p) { return juce::MouseEvent (src, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &dist, &dist,
                                                                           juce::Time::getCurrentTime(), { 60.0f, 12.0f }, juce::Time::getCurrentTime(), 1, false); };
            // Drag DIST by its name to the far end: it should land after DELAY and SPACE.
            dist.mouseDown (ev ({ 60.0f, 12.0f }));
            for (float dx = 20.0f; dx <= 700.0f; dx += 40.0f) dist.mouseDrag (ev ({ 60.0f + dx, 12.0f }));
            dist.mouseUp (ev ({ 760.0f, 12.0f }));
            const auto order = proc.getFxOrder();
            auto pos = [&] (int fx) { return (int) (std::find (order.begin(), order.end(), (juce::uint8) fx) - order.begin()); };
            check (pos (ab::FxDist) > pos (ab::FxDelay) && pos (ab::FxDist) > pos (ab::FxReverb), "dragging a module moves that effect along the chain (" + ab::fxOrderText (order) + ")");
            proc.undoManager.undo();
            check (pos (ab::FxDist) != -1 && proc.getFxOrder() != order, "and undo puts it back");
            proc.removeFromRack (ab::FxReverb);
            rackView->refresh (true);
            check (rackView->shownCount() == 2 && ! proc.fxAudible (ab::FxReverb), "removing a module switches the effect off");
            // Squeezed next to another panel, the rack scrolls rather than shrinking its modules.
            for (int fx : { ab::FxTape, ab::FxChorus, ab::FxFlanger, ab::FxGate }) proc.addToRack (fx);
            rackView->refresh (true);
            ed->moveWidget ("rack", "lowend", dock::Zone::Right);
            settle();
            check (rackView->isScrollable(), "a narrow rack scrolls (" + juce::String (rackView->getWidth()) + " px for " + juce::String (rackView->shownCount()) + " modules)");
            {
                // Pull the rack by its background: past the start it gives, less than the pointer moved, then springs back.
                float downX = 100.0f;
                auto rev = [&] (float x) { return juce::MouseEvent (src, { x, 20.0f }, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, rackView, rackView,
                                                                    juce::Time::getCurrentTime(), { downX, 20.0f }, juce::Time::getCurrentTime(), 1, false); };
                rackView->mouseDown (rev (100.0f));
                for (float x = 110.0f; x <= 300.0f; x += 10.0f) rackView->mouseDrag (rev (x));
                const float pulled = rackView->scrollPosition();
                check (pulled < -5.0f && pulled > -200.0f, "pulling past the start gives a little (" + juce::String (pulled, 1) + " px for 200)");
                rackView->mouseUp (rev (300.0f));
                juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
                check (std::abs (rackView->scrollPosition()) < 0.01f, "and springs back when let go");
                // A flick along the rack carries on after letting go.
                downX = 300.0f;
                rackView->mouseDown (rev (300.0f));
                for (float x = 290.0f; x >= 200.0f; x -= 15.0f) { rackView->mouseDrag (rev (x)); juce::Thread::sleep (8); }
                const float atRelease = rackView->scrollPosition();
                rackView->mouseUp (rev (200.0f));
                juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
                check (atRelease > 80.0f && rackView->scrollPosition() > atRelease + 20.0f && rackView->scrollPosition() <= rackView->scrollLimit() + 0.01f,
                       "a flick carries the rack on and stops inside its ends (" + juce::String (atRelease, 0) + " -> " + juce::String (rackView->scrollPosition(), 0) + ")");
            }
            ed->undoLayout();
            settle();
            stateBefore = proc.apvts.copyState().toXmlString(); // the rack test changed the sound on purpose
        }
        ed->loadWorkspace ("Sound Design", false);
        check (ed->captureLayout().toXmlString() == now, "Sound Design remembers its edits");
        check (proc.apvts.copyState().toXmlString() != juce::String() && ed->isPinned ("res"), "and its pinboard");
        // Macros moved by the XY pad are real parameter changes, so compare the sound before any of that.
        check (proc.apvts.copyState().toXmlString() == stateBefore, "no layout change touched the sound");

        // Oscillators: add up to eight, each gets its panel and a row in Sources; removing one switches it off.
        {
            ed->loadWorkspace ("Sound Design", false);
            settle();
            auto& srcView = ed->sourcesView();
            proc.setParam ("aOn", 1.0f);
            proc.setParam ("bOn", 1.0f); // "+ OSC" switches on A or B first when a patch has one off
            srcView.refresh();
            check (srcView.rowNames().contains ("OSC A") && srcView.rowNames().contains ("OSC B") && ! srcView.rowNames().contains ("OSC C"),
                   "Sources lists the two oscillators a patch starts with (" + srcView.rowNames().joinIntoString (", ") + ")");
            ed->addOscillator();
            settle();
            srcView.refresh();
            check (proc.apvts.getRawParameterValue ("cOn")->load() > 0.5f, "adding an oscillator switches on Osc C");
            check (ed->findWidget ("oscC") != nullptr && ed->findWidget ("oscC")->isVisible(), "and shows its panel");
            check (srcView.rowNames().contains ("OSC C"), "and gives it a row in Sources");
            tidy ("after adding Osc C");
            for (int i = 0; i < 6; ++i) ed->addOscillator();
            settle();
            bool allOn = true;
            for (int o = 0; o < ab::NumOsc; ++o) allOn &= o < 2 || proc.apvts.getRawParameterValue (ab::oscPrefix (o) + "On")->load() > 0.5f;
            check (allOn, "eight oscillators can play at once");
            tidy ("with eight oscillators");
            ed->removeOscillator (2);
            settle();
            srcView.refresh();
            check (proc.apvts.getRawParameterValue ("cOn")->load() < 0.5f && ! ed->layoutTree().contains ("oscC") && ! srcView.rowNames().contains ("OSC C"),
                   "removing Osc C switches it off, hides its panel and its row");
            proc.undoManager.undo();
            check (proc.apvts.getRawParameterValue ("cOn")->load() > 0.5f, "and undo brings it back");
            for (int o = 2; o < ab::NumOsc; ++o) proc.setParam (ab::oscPrefix (o) + "On", 0.0f);
            for (int o = 2; o < ab::NumOsc; ++o) if (ed->layoutTree().contains ("osc" + ab::oscPrefix (o).toUpperCase())) ed->hideWidget ("osc" + ab::oscPrefix (o).toUpperCase());
            settle();
        }
        // Tiny window area: everything still fits (widgets shrink to their minimum, never overlap).
        std::printf ("%d failures\n", failures);
        return failures == 0 ? 0 : 1;
    }

    // --lint: layout checks over every workspace, in normal use and in layout mode. Fails on overlapping
    // siblings, children poking out of their parent, anything in the header over the macro tray, and text that
    // doesn't fit its button or knob.
    if (argc > 2 && juce::String (argv[2]) == "--lint")
    {
        std::unique_ptr<HypernovaAudioProcessorEditor> ed (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        ed->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        juce::StringArray issues;
        auto nameOf = [] (juce::Component* c)
        {
            juce::String n = typeid (*c).name();
            n = n.fromLastOccurrenceOf ("ui", false, false).isNotEmpty() ? n.fromLastOccurrenceOf ("ui", false, false) : n;
            if (auto* b = dynamic_cast<juce::Button*> (c)) if (b->getButtonText().isNotEmpty()) n << " '" << b->getButtonText() << "'";
            if (auto* k = dynamic_cast<ab::ui::Knob*> (c)) n << " '" << k->paramId() << "'";
            if (auto* w = dynamic_cast<ab::ui::Widget*> (c)) n << " [" << w->id << "]";
            return n;
        };
        auto pathOf = [&] (juce::Component* c)
        {
            juce::StringArray parts;
            for (auto* p = c; p != nullptr && p != ed.get(); p = p->getParentComponent())
                if (dynamic_cast<ab::ui::Widget*> (p) != nullptr) { parts.insert (0, nameOf (p)); break; }
            parts.add (nameOf (c));
            return parts.joinIntoString (" > ");
        };
        // Things that are meant to sit over other things.
        auto layered = [] (juce::Component* c)
        {
            return dynamic_cast<ab::ui::DockOverlay*> (c) != nullptr || dynamic_cast<ab::ui::WidgetLibrary*> (c) != nullptr
                || dynamic_cast<ab::ui::PresetBrowser*> (c) != nullptr || dynamic_cast<ab::ui::UpdateBanner*> (c) != nullptr
                || dynamic_cast<ab::ui::InvisibleButton*> (c) != nullptr || dynamic_cast<ab::ui::StackTabs*> (c) != nullptr
                || dynamic_cast<juce::TooltipWindow*> (c) != nullptr || dynamic_cast<juce::ResizableCornerComponent*> (c) != nullptr;
        };
        std::function<void (juce::Component&, const juce::String&)> walk = [&] (juce::Component& parent, const juce::String& state)
        {
            std::vector<juce::Component*> kids;
            for (auto* c : parent.getChildren())
                if (c->isVisible() && c->getWidth() > 0 && c->getHeight() > 0) kids.push_back (c);
            for (size_t a = 0; a < kids.size(); ++a)
            {
                auto* c = kids[a];
                const bool scrolled = dynamic_cast<juce::Viewport*> (parent.getParentComponent()) != nullptr; // a viewport's content
                if (! layered (c) && ! scrolled && ! parent.getLocalBounds().expanded (1).contains (c->getBoundsInParent()))
                    issues.add (state + ": " + pathOf (c) + " pokes outside its parent " + c->getBoundsInParent().toString());
                for (size_t b = a + 1; b < kids.size(); ++b)
                {
                    auto* d = kids[b];
                    if (layered (c) || layered (d)) continue;
                    const auto overlap = c->getBoundsInParent().getIntersection (d->getBoundsInParent());
                    if (overlap.getWidth() > 1 && overlap.getHeight() > 1)
                        issues.add (state + ": " + pathOf (c) + " overlaps " + nameOf (d) + " by " + juce::String (overlap.getWidth()) + "x" + juce::String (overlap.getHeight()));
                }
                // Text that doesn't fit.
                if (auto* tb = dynamic_cast<juce::TextButton*> (c))
                {
                    const float textW = juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) tb->getHeight() * 0.6f))).getStringWidthFloat (tb->getButtonText());
                    const int room = tb->getWidth() - 12;
                    if (tb->getButtonText().isNotEmpty() && textW > (float) room * 1.12f)
                        issues.add (state + ": " + pathOf (c) + " text is too long for it (" + juce::String (textW, 0) + " > " + juce::String (room) + ")");
                }
                if (auto* w = dynamic_cast<ab::ui::Widget*> (c))
                    if (w->editing && ! w->collapsed && w->chromeCollides())
                        issues.add (state + ": " + pathOf (c) + " layout-mode title runs into its buttons (" + juce::String (w->getWidth()) + " px wide)");
                if (auto* k = dynamic_cast<ab::ui::Knob*> (c))
                    if (k->labelOverflow() > 1.15f)
                        issues.add (state + ": " + pathOf (c) + " label squashed to fit (" + juce::String (k->labelOverflow(), 2) + "x)");
                if (&parent == ed.get() || dynamic_cast<ab::ui::WidgetContent*> (&parent) == nullptr)
                    if (c->getParentComponent() != nullptr && c->getParentComponent()->getParentComponent() == ed.get()
                        && c->getBounds().intersects (HypernovaAudioProcessorEditor::macroTray()) && dynamic_cast<ab::ui::Knob*> (c) == nullptr && ! layered (c))
                        issues.add (state + ": " + pathOf (c) + " sits over the macro tray");
                walk (*c, state);
            }
        };
        for (auto ws : { "Sound Design", "Sampling", "Effects", "Analysis" })
        {
            ed->loadWorkspace (ws, false);
            for (bool editing : { false, true })
            {
                ed->setLayoutEditing (editing);
                if (editing) ed->setLibraryOpen (true);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (320);
                ed->finishMotion();
                walk (*ed, juce::String (ws) + (editing ? " (layout mode)" : ""));
                ed->setLayoutEditing (false);
            }
        }
        issues.removeDuplicates (false);
        for (auto& i : issues) std::printf ("ISSUE  %s\n", i.toRawUTF8());
        std::printf ("%d layout issues\n", issues.size());
        return issues.isEmpty() ? 0 : 1;
    }

    // --editbench: what a frame costs while you edit the layout: dragging a panel across the others, and
    // dragging the gap between two panels. Reports the area repainted per frame and the time to draw it.
    if (argc > 2 && juce::String (argv[2]) == "--editbench")
    {
        using namespace ab::ui;
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "Hypernova") proc.setCurrentProgram (i);
        std::unique_ptr<HypernovaAudioProcessorEditor> ed (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        ed->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        ed->loadWorkspace ("Sound Design", false);
        DockOverlay* overlay = nullptr;
        std::function<void (juce::Component&)> findOverlay = [&] (juce::Component& c)
        {
            for (auto* ch : c.getChildren()) { if (auto* o = dynamic_cast<DockOverlay*> (ch)) overlay = o; findOverlay (*ch); }
        };
        findOverlay (*ed);
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        juce::Image frame (juce::Image::ARGB, ed->getWidth() * 2, ed->getHeight() * 2, true);
        auto paintCost = [&] (juce::Rectangle<int> dirty)
        {
            if (dirty.isEmpty()) return 0.0;
            const auto t0 = juce::Time::getHighResolutionTicks();
            juce::Graphics g (frame);
            g.addTransform (juce::AffineTransform::scale (2.0f));
            g.reduceClipRegion (dirty);
            ed->paintEntireComponent (g, true);
            return juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
        };
        const double full = paintCost (overlay->getBounds());
        std::printf ("whole layout area redrawn: %.1f ms (what every frame of a drag used to cost)\n", full);
        for (bool editing : { true, false })
        {
            ed->setLayoutEditing (editing);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            ed->finishMotion();
            paintCost (overlay->getBounds()); // warm the caches
            overlay->takeDirty();
            // Drag osc A's title across the panels to the far side.
            auto* oscA = ed->findWidget ("oscA");
            const auto grab = oscA->getPosition() + (editing ? juce::Point<int> (oscA->getWidth() / 2, 60) : oscA->grabZone().getCentre()) - overlay->getPosition();
            auto ev = [&] (juce::Point<int> p) { return juce::MouseEvent (src, p.toFloat(), {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, overlay, overlay,
                                                                         juce::Time::getCurrentTime(), grab.toFloat(), juce::Time::getCurrentTime(), 1, false); };
            overlay->mouseDown (ev (grab));
            double worst = 0, total = 0;
            int frames = 0;
            juce::int64 area = 0;
            for (int step = 1; step <= 40; ++step)
            {
                overlay->mouseDrag (ev (grab + juce::Point<int> (step * 20, step * 8)));
                juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
                const auto dirty = overlay->takeDirty();
                const double ms = paintCost (dirty);
                worst = juce::jmax (worst, ms); total += ms; ++frames;
                area += (juce::int64) dirty.getWidth() * dirty.getHeight();
            }
            overlay->mouseUp (ev (grab + juce::Point<int> (820, 320)));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            ed->finishMotion();
            ed->undoLayout();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            ed->finishMotion();
            std::printf ("%s, dragging a panel: %.1f ms a frame on average, %.1f ms at worst, %.0f%% of the area repainted\n",
                         editing ? "layout mode" : "normal use", total / frames, worst,
                         100.0 * (double) area / frames / ((double) overlay->getWidth() * overlay->getHeight()));
        }
        // Dragging the gap between osc A and osc B (the neighbours resize every frame).
        {
            ed->setLayoutEditing (true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            ed->finishMotion();
            auto* oscA = ed->findWidget ("oscA");
            const int widthBefore = oscA->getWidth();
            const auto gap = juce::Point<int> (oscA->getRight() + dock::gutter / 2, oscA->getBounds().getCentreY()) - overlay->getPosition();
            auto ev = [&] (juce::Point<int> p) { return juce::MouseEvent (src, p.toFloat(), {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, overlay, overlay,
                                                                         juce::Time::getCurrentTime(), gap.toFloat(), juce::Time::getCurrentTime(), 1, false); };
            overlay->mouseDown (ev (gap));
            double worst = 0, total = 0, layoutTotal = 0;
            const int steps = juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_EDIT_LOOP", {}).isNotEmpty() ? 400 : 30;
            for (int step = 1; step <= steps; ++step)
            {
                const auto t0 = juce::Time::getHighResolutionTicks();
                overlay->mouseDrag (ev (gap + juce::Point<int> ((step % 2 == 0 ? 1 : -1) * 3 + (step % 30) * 3, 0)));
                const double layoutMs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
                const auto a = ed->findWidget ("oscA")->getBounds(), b = ed->findWidget ("oscB")->getBounds();
                const double ms = layoutMs + paintCost (a.getUnion (b));
                worst = juce::jmax (worst, ms); total += ms; layoutTotal += layoutMs;
                juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
            }
            const int widthAfter = oscA->getWidth();
            overlay->mouseUp (ev (gap));
            std::printf ("layout mode, dragging a gap: %.1f ms a frame on average (%.1f ms laying out), %.1f ms at worst (osc A %d -> %d px)\n",
                         total / steps, layoutTotal / steps, worst, widthBefore, widthAfter);
            if (juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_EDIT_PROFILE", {}).isNotEmpty())
            {
                // Which components cost the most to draw at the size they were just given.
                std::vector<std::pair<double, juce::String>> costs;
                std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                {
                    for (auto* child : c.getChildren())
                    {
                        if (! child->isVisible() || child->getWidth() == 0) continue;
                        juce::Image ci (juce::Image::ARGB, child->getWidth() * 2, child->getHeight() * 2, true);
                        const auto c0 = juce::Time::getHighResolutionTicks();
                        { juce::Graphics g (ci); g.addTransform (juce::AffineTransform::scale (2.0f)); child->paint (g); }
                        costs.push_back ({ juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - c0) * 1000.0,
                                           juce::String (typeid (*child).name()) + " " + child->getBounds().toString() });
                        walk (*child);
                    }
                };
                overlay->mouseDown (ev (gap));
                overlay->mouseDrag (ev (gap + juce::Point<int> (40, 0)));
                walk (*ed->findWidget ("oscA"));
                overlay->mouseUp (ev (gap));
                std::printf ("  (drag %s)\n", overlay->isDragging() ? "is a panel drag" : "did not start a panel drag");
                const auto t0 = juce::Time::getHighResolutionTicks();
                { juce::Graphics g (frame); g.addTransform (juce::AffineTransform::scale (2.0f)); ed->paint (g); }
                std::printf ("  %6.2f ms  the editor's own paint (backdrop and header)\n", juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0);
                for (auto id : { "oscA", "oscB", "space" })
                {
                    auto* w = ed->findWidget (id);
                    const auto w0 = juce::Time::getHighResolutionTicks();
                    for (int k = 0; k < 5; ++k) { juce::Graphics g (frame); g.addTransform (juce::AffineTransform::scale (2.0f)); w->paintEntireComponent (g, true); }
                    std::printf ("  %6.2f ms  whole widget %s (buffered: %s)\n", juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - w0) * 200.0, id,
                                 w->content.getCachedComponentImage() != nullptr ? "yes" : "no");
                    const auto p0 = juce::Time::getHighResolutionTicks();
                    for (int k = 0; k < 5; ++k) { juce::Graphics g (frame); g.addTransform (juce::AffineTransform::scale (2.0f)); w->paint (g); w->paintOverChildren (g); }
                    std::printf ("  %6.2f ms    its face and veil\n", juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - p0) * 200.0);
                }
                const auto e0 = juce::Time::getHighResolutionTicks();
                paintCost (ed->findWidget ("oscA")->getBounds().withWidth (8).translated (-10, 0));
                std::printf ("  %6.2f ms  an 8 px strip of backdrop\n", juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - e0) * 1000.0);
                std::sort (costs.rbegin(), costs.rend());
                for (size_t i = 0; i < std::min<size_t> (8, costs.size()); ++i) std::printf ("  %6.2f ms  %s\n", costs[i].first, costs[i].second.toRawUTF8());
            }
        }
        return 0;
    }

    // --paintbench: how long one full editor frame takes to draw at retina scale, and what the 30 fps timer costs.
    if (argc > 2 && juce::String (argv[2]) == "--paintbench")
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "Hypernova") proc.setCurrentProgram (i);
        std::unique_ptr<HypernovaAudioProcessorEditor> editor (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        editor->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
        juce::AudioBuffer<float> buf (2, 512);
        for (int i = 0; i < 20; ++i) { proc.processBlock (buf, midi); midi.clear(); }
        juce::Image img (juce::Image::ARGB, editor->getWidth() * 2, editor->getHeight() * 2, true);
        const auto t0 = juce::Time::getHighResolutionTicks();
        const int frames = 30;
        for (int f = 0; f < frames; ++f)
        {
            proc.processBlock (buf, midi);
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (2.0f));
            editor->paintEntireComponent (g, true);
        }
        const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0 / frames;
        std::printf ("full frame at 2x: %.1f ms  (at 30 fps that is %.0f%% of one core if everything repainted)\n", ms, ms * 30.0 / 10.0);
        {
            // What one panel face costs to redraw (it happens every frame while a panel is moving).
            juce::Image face (juce::Image::ARGB, 800, 760, true);
            for (int part = 0; part < 2; ++part)
            {
                const auto f0 = juce::Time::getHighResolutionTicks();
                for (int i = 0; i < 20; ++i)
                {
                    juce::Graphics fg (face);
                    fg.addTransform (juce::AffineTransform::scale (2.0f));
                    if (part == 0) ab::ui::panel (fg, { 1, 0, 398, 377 }, 14.0f);
                    else ab::ui::panelGrain (fg, { 1, 0, 398, 377 }, 14.0f);
                }
                std::printf ("one panel %s (400x380 at 2x): %.2f ms\n", part == 0 ? "face" : "grain",
                             juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - f0) * 1000.0 / 20.0);
            }
        }
        // Per-component cost, heaviest first.
        std::vector<std::pair<double, juce::String>> costs;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (! child->isVisible() || child->getWidth() == 0) continue;
                juce::Image ci (juce::Image::ARGB, child->getWidth() * 2, child->getHeight() * 2, true);
                const auto c0 = juce::Time::getHighResolutionTicks();
                for (int f = 0; f < 10; ++f)
                {
                    juce::Graphics g (ci);
                    g.addTransform (juce::AffineTransform::scale (2.0f));
                    child->paint (g);
                }
                const double cms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - c0) * 100.0;
                auto n = juce::String (typeid (*child).name());
                costs.push_back ({ cms, n.fromLastOccurrenceOf ("ui", false, false) + " " + child->getBounds().toString() });
                walk (*child);
            }
        };
        walk (*editor);
        std::sort (costs.rbegin(), costs.rend());
        for (size_t i = 0; i < std::min<size_t> (12, costs.size()); ++i) std::printf ("%6.2f ms  %s\n", costs[i].first, costs[i].second.toRawUTF8());
        return 0;
    }

    snap ("ui_1_rager808_mod.png", "Rager 808", 0, 0);
    snap ("ui_2_classiclog_fx.png", "Classic Log", 1, 1);
    snap ("ui_3_hypernova_play.png", "Hypernova", 0, 3);
    snap ("ui_5_morefx.png", "Trance Pluck", 0, 2);
    snap ("ui_6_layout_edit.png", "Reese Wide", 1, 0, "Sound Design", true);
    snap ("ui_7_effects_workspace.png", "Reese Wide", 0, 1, "Effects");
    snap ("ui_8_analysis.png", "Reese Wide", 0, 0, "Analysis");
    snap ("ui_10_sources.png", "Reese Wide", 0, 0, "Sound Design", false, [&] (HypernovaAudioProcessorEditor& e)
    {
        e.addOscillator();
        e.addOscillator();
        e.activateWidget ("sources");
        for (int o = 2; o < 4; ++o) proc.setParam (ab::oscPrefix (o) + "Level", 0.5f);
    });
    for (int o = 2; o < ab::NumOsc; ++o) proc.setParam (ab::oscPrefix (o) + "On", 0.0f);
    {
        // Sampling: a made-up recording (a plucked, slightly noisy tone) loaded and looping.
        const auto wavFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Glass Pluck.wav");
        {
            const int n = 44100 * 2;
            juce::AudioBuffer<float> b (2, n);
            juce::Random rnd (3);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / 44100.0;
                const double env = (1.0 - std::exp (-t * 400.0)) * std::exp (-t * 1.6);
                double v = 0;
                for (int h = 1; h < 9; ++h) v += std::sin (juce::MathConstants<double>::twoPi * 196.0 * h * t) / (h * (1.0 + t * h * 0.8));
                const float noise = (rnd.nextFloat() * 2.0f - 1.0f) * 0.05f * (float) std::exp (-t * 20.0);
                b.setSample (0, i, (float) (0.45 * env * v) + noise);
                b.setSample (1, i, (float) (0.45 * env * v * 0.95) + noise);
            }
            wavFile.deleteFile();
            juce::WavAudioFormat wav;
            auto stream = std::unique_ptr<juce::OutputStream> (wavFile.createOutputStream());
            auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 44100.0, 2, 24, {}, 0));
            stream.release();
            writer->writeFromAudioSampleBuffer (b, 0, n);
        }
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "Init") proc.setCurrentProgram (i);
        juce::String error;
        proc.loadSample (wavFile, error);
        proc.setParam ("aOn", 0.0f);
        proc.setParam ("smpLoop", 1.0f);
        proc.setParam ("smpLoopStart", 0.35f);
        proc.setParam ("smpLoopEnd", 0.8f);
        std::unique_ptr<HypernovaAudioProcessorEditor> editor (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        editor->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        editor->loadWorkspace ("Sampling", false);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 55, 1.0f), 0);
        for (int i = 0; i < 60; ++i)
        {
            juce::AudioBuffer<float> buf (2, 512);
            proc.processBlock (buf, midi);
            midi.clear();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
        }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile ("ui_9_sampling.png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        wavFile.deleteFile();
        proc.clearSample();
    }
    {
        std::unique_ptr<HypernovaAudioProcessorEditor> editor (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        editor->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        editor->setBrowserOpen (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (350);
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile ("ui_4_browser.png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    return 0;
}
