#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

// Renders the editor to PNGs without a DAW: UISnapshot <outDir>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    HypernovaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    auto snap = [&] (const juce::String& name, const juce::String& preset, int mode, int page)
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
        editor->setDeckPage (page);
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
    snap ("ui_3_hypernova_play.png", "Hypernova", 0, 2);
    {
        std::unique_ptr<HypernovaAudioProcessorEditor> editor (dynamic_cast<HypernovaAudioProcessorEditor*> (proc.createEditor()));
        editor->setSize (HypernovaAudioProcessorEditor::baseWidth, HypernovaAudioProcessorEditor::baseHeight);
        editor->setBrowserOpen (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile ("ui_4_browser.png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (image, out);
    }
    return 0;
}
