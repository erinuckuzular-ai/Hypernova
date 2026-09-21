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
        editor->loadWorkspace (workspace, false);
        editor->setDeckPage (page);
        editor->setLayoutEditing (editing);
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

    // --layouttest: drives the widget host through its real handlers and checks each rule of layout mode.
    if (argc > 2 && juce::String (argv[2]) == "--layouttest")
    {
        using W = ab::ui::Widget;
        int failures = 0;
        auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what); failures += ok ? 0 : 1; };
        std::unique_ptr<HypernovaAudioProcessorEditor> ed (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        ed->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        ed->loadWorkspace ("Sound Design", false);
        auto stateBefore = proc.getParameters().size() > 0 ? proc.apvts.copyState().toXmlString() : juce::String();
        auto* sub = ed->findWidget ("sub");
        auto* env = ed->findWidget ("env");
        auto* fx = ed->findWidget ("fx");
        auto* mod = ed->findWidget ("mod");
        check (sub && env && fx && mod, "widgets exist");
        check (sub->getBounds() == juce::Rectangle<int> (24, 488, 240, 212), "Sound Design matches the classic layout");
        check (mod->isVisible() && ! fx->isVisible() && mod->stack == fx->stack, "deck pages are one stack");

        // Normal mode: widget chrome ignores the mouse, so playing knobs can never move a panel.
        ed->setLayoutEditing (false);
        const auto home = sub->getBounds();
        sub->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 100.0f, 10.0f }, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          sub, sub, juce::Time::getCurrentTime(), { 100.0f, 10.0f }, juce::Time::getCurrentTime(), 1, false));
        check (sub->getBounds() == home, "normal mode never moves a widget");

        ed->setLayoutEditing (true);
        check (sub->isEditing(), "layout mode reaches every widget");

        // Hide sub: its space frees up, the sound doesn't change.
        sub->onHide (*sub);
        check (! sub->isVisible(), "hide removes it from screen");
        check (proc.apvts.copyState().toXmlString() == stateBefore, "hiding never changes the sound");

        // Move pitch into the freed space with a slightly-off drop; it should snap to the edge.
        auto* pitch = ed->findWidget ("pitch");
        pitch->onEdit (*pitch, W::Edit::MoveStart, {});
        pitch->onEdit (*pitch, W::Edit::Move, { -249, -1 });
        pitch->onEdit (*pitch, W::Edit::MoveEnd, { -249, -1 });
        std::printf ("  pitch at %s\n", pitch->getBounds().toString().toRawUTF8());
        check (pitch->getX() == 24 && pitch->getY() == 488, "move snaps to the area edge");

        // Drop pitch onto the filter's body: overlap is refused and it goes back.
        const auto before = pitch->getBounds();
        pitch->onEdit (*pitch, W::Edit::MoveStart, {});
        pitch->onEdit (*pitch, W::Edit::MoveEnd, { 300, 80 });
        check (pitch->getBounds() == before, "overlapping drop is reverted");

        // Resize: aspect kept, clamped to its minimum.
        pitch->onEdit (*pitch, W::Edit::ResizeStart, {});
        pitch->onEdit (*pitch, W::Edit::ResizeEnd, { -1000, 0 });
        check (pitch->getWidth() == pitch->minWidth() && pitch->getHeight() == pitch->heightForWidth (pitch->getWidth()), "resize keeps aspect and minimum size");

        // Collapse to a title bar and back.
        const int fullH = pitch->getHeight();
        pitch->onCollapse (*pitch);
        check (pitch->getHeight() < fullH / 3, "collapse shrinks to the title row");
        pitch->onCollapse (*pitch);
        check (pitch->getHeight() == fullH, "expand restores height");

        // Unstack an effects page out of the deck: it finds room of its own.
        ed->setDeckPage (1);
        check (fx->isVisible() && ! mod->isVisible(), "tab switches the front page");
        env->onHide (*env);
        auto* filter = ed->findWidget ("filter");
        filter->onHide (*filter);
        auto* play = ed->findWidget ("play");
        play->onUnstack (*play);
        check (play->stack.isEmpty() && play->isVisible(), "unstack shows it on its own");
        // Stack it back by dropping it onto the fx title.
        play->onEdit (*play, W::Edit::MoveStart, {});
        const auto target = fx->getPosition() - play->getPosition() + juce::Point<int> (10, 4);
        play->onEdit (*play, W::Edit::MoveEnd, target);
        std::printf ("  play %s stack '%s' vis %d, fx %s stack '%s' vis %d\n", play->getBounds().toString().toRawUTF8(), play->stack.toRawUTF8(), (int) play->isVisible(),
                     fx->getBounds().toString().toRawUTF8(), fx->stack.toRawUTF8(), (int) fx->isVisible());
        check (play->stack == fx->stack && play->isVisible() && ! fx->isVisible(), "drop on a title stacks as a tab");

        // Add back a hidden widget.
        ed->addWidget (*sub);
        check (sub->isVisible() && sub->getWidth() > 0, "add brings a hidden widget back");

        // Undo walks back, redo forward.
        const auto now = ed->captureLayout().toXmlString();
        ed->undoLayout();
        check (ed->captureLayout().toXmlString() != now, "undo changes the layout");
        ed->redoLayout();
        check (ed->captureLayout().toXmlString() == now, "redo returns to it");

        // Workspaces keep their own arrangement.
        ed->loadWorkspace ("Effects", false);
        check (fx->isVisible() && fx->getBounds().getWidth() == 1232 && ! ed->findWidget ("oscA")->isVisible(), "Effects workspace loads");
        ed->loadWorkspace ("Sound Design", false);
        check (ed->captureLayout().toXmlString() == now, "Sound Design remembers its edits");
        check (proc.apvts.copyState().toXmlString() == stateBefore, "no layout change touched the sound");
        ed->setLayoutEditing (false);
        check (! sub->isEditing(), "done leaves layout mode");
        std::printf ("%d failures\n", failures);
        return failures == 0 ? 0 : 1;
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
    snap ("ui_7_effects_workspace.png", "Reese Wide", 0, 0, "Effects");
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
