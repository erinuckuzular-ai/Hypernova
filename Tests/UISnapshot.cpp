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
                     const juce::String& workspace = "Sound Design", bool editing = false)
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            if (proc.getProgramName (i) == preset) proc.setCurrentProgram (i);

        std::unique_ptr<HypernovaAudioProcessorEditor> editor (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        editor->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        editor->loadWorkspace (workspace, false);
        if (workspace == "Effects") { proc.setParam ("lowOn", 1.0f); proc.setParam ("lowDuck", 0.5f); proc.setParam ("distMix", 0.6f); }

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
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
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

        std::unique_ptr<HypernovaAudioProcessorEditor> ed (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        ed->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        ed->loadWorkspace ("Sound Design", false);
        const auto stateBefore = proc.apvts.copyState().toXmlString();
        const auto area = ed->layoutArea();

        // Structural changes slide into place; let them land before measuring.
        auto settle = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (300); };
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
        check (ed->findWidget ("mod")->isVisible() && ! ed->findWidget ("fx")->isVisible()
               && ed->layoutTree().findLeaf ("mod") == ed->layoutTree().findLeaf ("fx"), "deck pages share one place as tabs");
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
            overlay->mouseDown (ev (gutterPoint));
            overlay->mouseDrag (ev (gutterPoint + juce::Point<int> (60, 0)));
            overlay->mouseUp (ev (gutterPoint + juce::Point<int> (60, 0)));
            check (std::abs (oscA->getWidth() - (before + 60)) <= 1, "dragging a gap resizes the neighbours (" + juce::String (before) + " -> " + juce::String (oscA->getWidth()) + ")");
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
        check (ed->findWidget ("fx")->isVisible() && ed->findWidget ("fx")->getWidth() == area.getWidth() && ! ed->findWidget ("oscA")->isVisible(), "Effects workspace");
        tidy ("Effects");

        // The FX chain: drag the first effect three places along and the processor's order follows.
        {
            FxChainView* chain = nullptr;
            for (auto* c : ed->findWidget ("chain")->content.getChildren()) if (auto* v = dynamic_cast<FxChainView*> (c)) chain = v;
            check (chain != nullptr && chain->isVisible(), "the Effects workspace shows the FX chain");
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            const float slotW = ((float) chain->getWidth() - 108.0f) / (float) ab::NumFx;
            const juce::Point<float> from (54.0f + slotW * 0.5f, (float) chain->getHeight() * 0.4f);
            const auto to = from + juce::Point<float> (slotW * 3.0f, 0.0f);
            auto ev = [&] (juce::Point<float> p) { return juce::MouseEvent (src, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, chain, chain,
                                                                           juce::Time::getCurrentTime(), from, juce::Time::getCurrentTime(), 1, false); };
            chain->mouseDown (ev (from));
            chain->mouseDrag (ev (from + juce::Point<float> (10.0f, 0.0f)));
            chain->mouseDrag (ev (to));
            chain->mouseUp (ev (to));
            const auto order = proc.getFxOrder();
            check (order[3] == ab::FxDist && order[0] == ab::FxTape, "dragging a chip reorders the effects (" + ab::fxOrderText (order) + ")");
            proc.undoManager.undo();
            check (proc.getFxOrder() == ab::defaultFxOrder(), "and undo puts it back");

            // Squeezed beside another panel, the chain keeps its chips a usable size and scrolls instead.
            ed->moveWidget ("chain", "lowend", dock::Zone::Right);
            settle();
            check (chain->isScrollable() && chain->chipWidth() >= 95.0f, "a narrow chain scrolls instead of shrinking its chips (chip "
                   + juce::String (chain->chipWidth(), 0) + ", widget " + juce::String (ed->findWidget ("chain")->getWidth()) + " wide)");
            chain->mouseWheelMove (ev ({ 200.0f, 30.0f }), juce::MouseWheelDetails { -0.5f, 0.0f, false, false, false });
            check (chain->scrollPosition() > 50.0f, "a sideways swipe scrolls it (" + juce::String (chain->scrollPosition(), 0) + ")");
            ed->undoLayout();
            settle();
        }
        ed->loadWorkspace ("Sound Design", false);
        check (ed->captureLayout().toXmlString() == now, "Sound Design remembers its edits");
        check (proc.apvts.copyState().toXmlString() != juce::String() && ed->isPinned ("res"), "and its pinboard");
        // Macros moved by the XY pad are real parameter changes, so compare the sound before any of that.
        check (proc.apvts.copyState().toXmlString() == stateBefore, "no layout change touched the sound");

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
                walk (*ed, juce::String (ws) + (editing ? " (layout mode)" : ""));
                ed->setLayoutEditing (false);
            }
        }
        issues.removeDuplicates (false);
        for (auto& i : issues) std::printf ("ISSUE  %s\n", i.toRawUTF8());
        std::printf ("%d layout issues\n", issues.size());
        return issues.isEmpty() ? 0 : 1;
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
