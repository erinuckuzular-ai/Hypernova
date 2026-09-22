#pragma once

#include "Components.h"

// The sampler's waveform: drop a sound on it, drag the start/end flags and the loop brackets, see every
// playing voice as a moving line. Markers are real parameters (automatable, undoable).
namespace ab::ui
{

class SamplerView : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit SamplerView (HypernovaAudioProcessor& p) : proc (p)
    {
        setTooltip ("Drop a sample here. Drag the flags to set where it starts and ends, the brackets to set the loop.");
    }

    std::function<void (const juce::String&)> onMessage;
    std::function<void()> onLoadRequest; // click on the empty view

    static bool isAudioFile (const juce::File& f)
    {
        return f.hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;caf;ogg");
    }

    void refresh()
    {
        const int v = proc.sampleVersion.load();
        const float head = proc.shownSample.load();
        const int changes = proc.parameterChanges.load();
        if (v != version || std::abs (head - lastHead) > 0.0005f || changes != lastChanges)
        {
            if (v != version) { version = v; peaks.clear(); }
            lastHead = head;
            lastChanges = changes;
            repaint();
        }
    }

    void resized() override { peaks.clear(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (dropHover ? Palette::oscA.get() : Colours::line.get());
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, dropHover ? 2.0f : 1.0f);

        const auto sample = proc.sampleForUi();
        const auto plot = plotArea();
        if (sample == nullptr)
        {
            g.setColour (Colours::textDim);
            g.setFont (font (13.0f, true));
            g.drawText ("drop a sample here", plot.toNearestInt(), juce::Justification::centred, false);
            g.setFont (font (10.5f));
            g.setColour (Colours::textFaint);
            g.drawText ("WAV, AIFF, FLAC, MP3 up to a minute. or click to choose one.", plot.toNearestInt().translated (0, 20),
                        juce::Justification::centred, false);
            return;
        }
        buildPeaks (*sample, (int) plot.getWidth() * 2);

        const float start = value ("smpStart"), end = value ("smpEnd");
        const float s0 = juce::jmin (start, end), s1 = juce::jmax (start, end);
        const bool looping = (int) value ("smpLoop") != LoopOff;
        const float ls = s0 + juce::jmin (value ("smpLoopStart"), value ("smpLoopEnd")) * (s1 - s0);
        const float le = s0 + juce::jmax (value ("smpLoopStart"), value ("smpLoopEnd")) * (s1 - s0);
        auto xOf = [&] (float v) { return plot.getX() + plot.getWidth() * v; };

        if (looping)
        {
            g.setColour (Palette::oscA.withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (xOf (ls), plot.getY(), xOf (le) - xOf (ls), plot.getHeight()));
        }

        // The waveform as one smooth filled shape (peaks at twice the pixel density, so it stays crisp on
        // retina screens): bright inside the region that plays, dim outside it.
        const float mid = plot.getCentreY(), half = plot.getHeight() * 0.48f;
        if (peaks.size() > 1)
        {
            juce::Path shape;
            const float step = plot.getWidth() / (float) (peaks.size() - 1);
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const juce::Point<float> p (plot.getX() + step * (float) i, mid - juce::jmax (peaks[i].second * half, 0.5f));
                if (i == 0) shape.startNewSubPath (p); else shape.lineTo (p);
            }
            for (size_t i = peaks.size(); i-- > 0;)
                shape.lineTo (plot.getX() + step * (float) i, mid - juce::jmin (peaks[i].first * half, -0.5f));
            shape.closeSubPath();
            const auto playing = juce::Rectangle<float> (xOf (s0), plot.getY(), xOf (s1) - xOf (s0), plot.getHeight());
            {
                juce::Graphics::ScopedSaveState keep (g);
                g.excludeClipRegion (playing.toNearestInt());
                g.setColour (Colours::textFaint.withAlpha (0.45f));
                g.fillPath (shape);
            }
            {
                juce::Graphics::ScopedSaveState keep (g);
                g.reduceClipRegion (playing.toNearestInt());
                g.setGradientFill (juce::ColourGradient (Palette::oscA.brighter (0.2f), 0, mid - half, Palette::oscA.withAlpha (0.7f), 0, mid, true));
                g.fillPath (shape);
            }
        }
        g.setColour (Colours::line);
        g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());

        // Start / end flags and loop brackets.
        auto flag = [&] (float v, const juce::String& label, juce::Colour c, bool top)
        {
            const float x = xOf (v);
            g.setColour (c);
            g.fillRect (x - 0.75f, plot.getY(), 1.5f, plot.getHeight());
            auto tab = juce::Rectangle<float> (26, 14).withPosition (x - (v > 0.9f ? 26.0f : 0.0f), top ? plot.getY() : plot.getBottom() - 14);
            g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (Colours::bg0);
            g.setFont (mono (9.0f).boldened());
            g.drawText (label, tab, juce::Justification::centred, false);
        };
        flag (s0, "S", Colours::text, true);
        flag (s1, "E", Colours::text, true);
        if (looping)
        {
            flag (ls, "L", Palette::oscA, false);
            flag (le, "L", Palette::oscA, false);
        }

        // Playhead of the newest voice.
        const float head = proc.shownSample.load();
        if (head >= 0.0f)
        {
            g.setColour (Palette::env.withAlpha (0.9f));
            g.fillRect (xOf (head) - 1.0f, plot.getY(), 2.0f, plot.getHeight());
        }

        g.setColour (Colours::textDim);
        g.setFont (mono (9.5f));
        const double secs = sample->length / sample->rate;
        g.drawText (sample->name + "   " + juce::String (secs, secs < 10 ? 2 : 1) + " s   " + juce::String (sample->rate / 1000.0, 1) + " kHz",
                    plot.toNearestInt().reduced (4, 2), juce::Justification::bottomLeft, true);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (markerAt (e.position) != nullptr ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (proc.sampleForUi() == nullptr) { if (onLoadRequest) onLoadRequest(); return; }
        dragging = markerAt (e.position);
        if (dragging != nullptr)
            if (auto* p = proc.apvts.getParameter (dragging)) p->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == nullptr) return;
        const auto plot = plotArea();
        float v = juce::jlimit (0.0f, 1.0f, (e.position.x - plot.getX()) / plot.getWidth());
        const juce::String id (dragging);
        if (id.startsWith ("smpLoop"))
        {
            // Loop points are stored relative to the playing region.
            const float s0 = juce::jmin (value ("smpStart"), value ("smpEnd")), s1 = juce::jmax (value ("smpStart"), value ("smpEnd"));
            v = juce::jlimit (0.0f, 1.0f, (v - s0) / juce::jmax (0.001f, s1 - s0));
        }
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging != nullptr)
            if (auto* p = proc.apvts.getParameter (dragging)) p->endChangeGesture();
        dragging = nullptr;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        // Double-click a flag to put it back where it started.
        if (auto* id = markerAt (e.position))
            if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue());
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (isAudioFile (juce::File (f))) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dropHover = false;
        for (auto& f : files)
            if (isAudioFile (juce::File (f))) { load (juce::File (f)); break; }
        repaint();
    }

    void load (const juce::File& f)
    {
        juce::String error;
        const bool ok = proc.loadSample (f, error);
        if (onMessage)
        {
            if (! ok) onMessage (error);
            else
            {
                const auto s = proc.sampleForUi();
                juce::String m = "Loaded " + f.getFileNameWithoutExtension();
                if (s != nullptr && s->rootGuess >= 0.0f)
                    m << ", tuned to " << juce::MidiMessage::getMidiNoteName (juce::roundToInt (s->rootGuess), true, true, 3);
                if (error.isNotEmpty()) m << " (" << error << ")";
                onMessage (m);
            }
        }
        repaint();
    }

private:
    HypernovaAudioProcessor& proc;
    std::vector<std::pair<float, float>> peaks;
    int version = -1, lastChanges = -1;
    float lastHead = -2.0f;
    const char* dragging = nullptr;
    bool dropHover = false;

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 6.0f); }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    void buildPeaks (const SampleData& s, int width)
    {
        if ((int) peaks.size() == width || width <= 0) return;
        peaks.assign ((size_t) width, { 0.0f, 0.0f });
        for (int x = 0; x < width; ++x)
        {
            const int a = (int) ((juce::int64) s.length * x / width), b = juce::jmax (a + 1, (int) ((juce::int64) s.length * (x + 1) / width));
            float lo = 0, hi = 0;
            for (int i = a; i < b && i < s.length; ++i)
            {
                const float v = 0.5f * (s.l[(size_t) i + 2] + s.r[(size_t) i + 2]);
                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
            }
            peaks[(size_t) x] = { lo, hi };
        }
    }

    // The marker under the mouse (within 7 px), as a parameter id.
    const char* markerAt (juce::Point<float> p) const
    {
        if (proc.sampleForUi() == nullptr) return nullptr;
        const auto plot = plotArea();
        const float s0 = juce::jmin (value ("smpStart"), value ("smpEnd")), s1 = juce::jmax (value ("smpStart"), value ("smpEnd"));
        const bool looping = (int) value ("smpLoop") != LoopOff;
        struct M { const char* id; float v; };
        std::vector<M> ms { { value ("smpStart") <= value ("smpEnd") ? "smpStart" : "smpEnd", s0 },
                            { value ("smpStart") <= value ("smpEnd") ? "smpEnd" : "smpStart", s1 } };
        if (looping)
        {
            const float a = value ("smpLoopStart"), b = value ("smpLoopEnd");
            ms.push_back ({ a <= b ? "smpLoopStart" : "smpLoopEnd", s0 + juce::jmin (a, b) * (s1 - s0) });
            ms.push_back ({ a <= b ? "smpLoopEnd" : "smpLoopStart", s0 + juce::jmax (a, b) * (s1 - s0) });
        }
        // Loop brackets grab from the lower half, start/end flags from the upper half, when they overlap.
        const bool lower = p.y > plot.getCentreY();
        const char* best = nullptr;
        float bestD = 7.0f;
        for (auto& m : ms)
        {
            float d = std::abs (plot.getX() + plot.getWidth() * m.v - p.x);
            const bool isLoop = juce::String (m.id).startsWith ("smpLoop");
            if (isLoop != lower) d += 3.0f;
            if (d < bestD) { bestD = d; best = m.id; }
        }
        return best;
    }
};

} // namespace ab::ui
