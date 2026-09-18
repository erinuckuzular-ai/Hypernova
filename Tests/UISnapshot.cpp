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

    snap ("ui_1_rager808_mod.png", "Rager 808", 0, 0);
    snap ("ui_2_classiclog_fx.png", "Classic Log", 1, 1);
    snap ("ui_3_hypernova_play.png", "Hypernova", 0, 2);
    return 0;
}
