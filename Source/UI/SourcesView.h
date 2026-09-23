#pragma once

#include "Components.h"

// Sources: every sound source in one place, like a mixer. Each row has its on light, its name (click to
// open its panel), a level bar, pan, and whether it goes through the filter. "+ OSC" adds an oscillator.
namespace ab::ui
{

// Which bus a source plays into. Two buses run side by side, each with its own effects, and they meet at the output.
class BusPill : public juce::Component, public juce::SettableTooltipClient
{
public:
    BusPill (HypernovaAudioProcessor& p, juce::String parameter, ThemeColour c)
        : proc (p), param (std::move (parameter)), colour (c)
    {
        setTooltip ("Where this source plays: the main bus, the alt bus (each with its own effects), or into the resonator.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    int bus() const
    {
        auto* v = proc.apvts.getRawParameterValue (param);
        return v != nullptr ? juce::jlimit (0, choices() - 1, (int) v->load()) : 0;
    }

    int choices() const
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (param)))
            return juce::jmax (2, c->choices.size());
        return 2;
    }

    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        const int next = (bus() + 1) % choices();
        proc.undoManager.beginNewTransaction (next == 0 ? "To the main bus" : next == 1 ? "To the alt bus" : "Into the resonator");
        proc.setParam (param, (float) next);
        proc.apvts.copyState();   // land it in this undo step now, not on the next timer tick
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int where = bus();
        const bool alt = where != 0;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const auto tint = where == 2 ? Palette::lfo.get() : colour.get();
        g.setColour (alt ? tint.withAlpha (over ? 0.32f : 0.24f) : (over ? Colours::panelHi.brighter (0.06f) : Colours::panelHi.withAlpha (0.8f)));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (alt ? tint.withAlpha (0.8f) : Colours::line.get());
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (alt ? Colours::text.get() : Colours::text.withAlpha (over ? 0.9f : 0.62f));
        g.setFont (mono (9.0f).boldened().withExtraKerningFactor (0.1f));
        g.drawText (where == 0 ? "MAIN" : where == 1 ? "ALT" : "RES", r, juce::Justification::centred, false);
    }

private:
    HypernovaAudioProcessor& proc;
    juce::String param;
    ThemeColour colour;
    bool over = false;
};

class SourcesView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SourcesView (HypernovaAudioProcessor& p) : proc (p) { rebuild(); }

    std::function<void (const juce::String& widgetId)> onShow;
    std::function<void (int osc)> onRemoveOsc;

    static ThemeColour colourFor (int o)
    {
        static const int slots[] = { SlotOscA, SlotOscB, SlotEnv, SlotLfo, SlotSub, SlotFilter, SlotMod, SlotFx };
        return ThemeColour { slots[juce::jlimit (0, 7, o)] };
    }

    // Solo: everything else is switched off until you click it again. It's one undo step, and it isn't
    // saved with the sound (it's a way of listening, not part of the patch).
    void toggleSolo (const juce::String& onParam)
    {
        proc.undoManager.beginNewTransaction (soloed == onParam ? "Unsolo" : "Solo");
        if (soloed == onParam)
        {
            for (auto& [id, was] : beforeSolo) proc.setParam (id, was);
            soloed.clear();
            beforeSolo.clear();
        }
        else
        {
            if (beforeSolo.empty())
                for (auto& id : soloable())
                    beforeSolo.push_back ({ id, proc.apvts.getRawParameterValue (id)->load() });
            for (auto& id : soloable()) proc.setParam (id, id == onParam ? 1.0f : 0.0f);
            soloed = onParam;
        }
        proc.apvts.copyState(); // land the changes in this undo step now, not on the next timer tick
        for (auto& r : rows) r->setSoloed (! soloed.isEmpty() && r->onParamId() == soloed);
        repaint();
    }

    // Rows follow what's switched on; checked from the editor's timer.
    void refresh()
    {
        if (signature() != lastSignature) rebuild();
        else for (auto& r : rows) r->repaint();
        // If anything else came back on (undo, a preset, automation), the solo is over.
        if (soloed.isNotEmpty())
            for (auto& id : soloable())
                if (id != soloed && proc.apvts.getRawParameterValue (id)->load() > 0.5f) { soloed.clear(); beforeSolo.clear(); break; }
        for (auto& r : rows) r->setSoloed (! soloed.isEmpty() && r->onParamId() == soloed);
    }

    void resized() override
    {
        int y = 0;
        for (auto& r : rows) { r->setBounds (0, y, getWidth(), rowH); y += rowH + 4; }
    }

    int preferredHeight() const { return (int) rows.size() * (rowH + 4); }
    juce::StringArray rowNames() const { juce::StringArray n; for (auto& r : rows) n.add (r->title()); return n; }

private:
    static constexpr int rowH = 34;
    HypernovaAudioProcessor& proc;

    class Row : public juce::Component
    {
    public:
        Row (SourcesView& o, juce::AudioProcessorValueTreeState& state, const juce::String& title, const juce::String& widgetId, ThemeColour c,
             const char* onParam, const char* levelParam, const char* panParam, const char* routeParam, int oscIndex,
             const char* busParam)
            : owner (o), name (title), widget (widgetId), colour (c), osc (oscIndex), on (onParam != nullptr ? juce::String (onParam) : juce::String()), level (c)
        {
            if (onParam != nullptr)
            {
                addAndMakeVisible (solo);
                solo.setTooltip ("Solo: hear this on its own. Click again to bring the rest back.");
                solo.onClick = [this] { owner.toggleSolo (on); };
                led = std::make_unique<PowerLed> (c);
                addAndMakeVisible (*led);
                attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, onParam, *led));
            }
            addAndMakeVisible (level);
            levelAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, levelParam, level);
            level.setTooltip ("Level");
            if (panParam != nullptr)
            {
                pan.setTooltip ("Pan (double-click for centre)");
                addAndMakeVisible (pan);
                panAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, panParam, pan);
                pan.setDoubleClickReturnValue (true, 0.0);
            }
            if (routeParam != nullptr)
            {
                route = std::make_unique<PillToggle> ("FILTER", c);
                route->setTooltip ("Send this source through the filter");
                addAndMakeVisible (*route);
                attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, routeParam, *route));
            }
            if (busParam != nullptr)
            {
                busPill = std::make_unique<BusPill> (o.proc, busParam, c);
                busPill->setComponentID ("bus_" + juce::String (onParam != nullptr ? onParam : levelParam));
                addAndMakeVisible (*busPill);
            }
            soloed = false;
            if (osc >= 2)
            {
                addAndMakeVisible (remove);
                remove.setTooltip ("Remove this oscillator");
                remove.onClick = [this] { if (owner.onRemoveOsc) owner.onRemoveOsc (osc); };
            }
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 3);
            if (led != nullptr) led->setBounds (r.removeFromLeft (22).withSizeKeepingCentre (18, 18));
            else r.removeFromLeft (22);
            r.removeFromLeft (6);
            if (osc >= 2) remove.setBounds (r.removeFromRight (20).withSizeKeepingCentre (18, 18)); else r.removeFromRight (20);
            r.removeFromRight (4);
            if (on.isNotEmpty()) solo.setBounds (r.removeFromRight (24).withSizeKeepingCentre (22, 20)); else r.removeFromRight (24);
            r.removeFromRight (4);
            if (route != nullptr) route->setBounds (r.removeFromRight (74).withSizeKeepingCentre (70, 20)); else r.removeFromRight (74);
            r.removeFromRight (6);
            if (busPill != nullptr) busPill->setBounds (r.removeFromRight (46).withSizeKeepingCentre (44, 20)); else r.removeFromRight (46);
            r.removeFromRight (6);
            // Squeezed narrow, the pan bar gives up its room so the level bar stays readable; pan is on the
            // source's own panel either way.
            const bool roomForPan = getWidth() >= 380;
            if (panAttach != nullptr && roomForPan)
            {
                pan.setVisible (true);
                pan.setBounds (r.removeFromRight (58));
                r.removeFromRight (6);
            }
            else if (panAttach != nullptr) pan.setVisible (false);
            else if (roomForPan) r.removeFromRight (64);
            // The name gives up room before the level bar does: the level is what the row is for.
            nameArea = r.removeFromLeft (juce::jlimit (48, 96, r.getWidth() - 90));
            r.removeFromLeft (6);
            level.setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (isMouseOver (true) ? Colours::panelHi.brighter (0.04f) : Colours::panelHi.withAlpha (0.6f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (Colours::text);
            g.setFont (mono (11.0f).boldened().withExtraKerningFactor (0.08f));
            g.drawText (name, nameArea, juce::Justification::centredLeft, true);
        }

        const juce::String& title() const { return name; }
        const juce::String& onParamId() const { return on; }
        void setSoloed (bool b) { if (b != solo.lit) { solo.lit = b; solo.repaint(); } }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (nameArea.contains (e.getPosition()) && owner.onShow) owner.onShow (widget);
        }

    private:
        SourcesView& owner;
        juce::String name, widget;
        ThemeColour colour;
        int osc;
        std::unique_ptr<PowerLed> led;
        std::unique_ptr<PillToggle> route;
        std::unique_ptr<BusPill> busPill;
        struct LevelBar : juce::Slider
        {
            explicit LevelBar (ThemeColour c) : colour (c)
            {
                setSliderStyle (juce::Slider::LinearBar);
                setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            }
            void paint (juce::Graphics& g) override
            {
                auto r = getLocalBounds().toFloat().reduced (0, 5);
                g.setColour (Colours::inset);
                g.fillRoundedRectangle (r, 4.0f);
                const float v = (float) valueToProportionOfLength (getValue());
                g.setColour (colour.withAlpha (0.85f));
                g.fillRoundedRectangle (r.withWidth (r.getWidth() * v), 4.0f);
                g.setColour (Colours::text.withAlpha (0.9f));
                g.setFont (mono (9.5f));
                g.drawText (juce::String (juce::roundToInt (getValue() * 100.0)) + "%", r.reduced (6, 0), juce::Justification::centredRight, false);
            }
            ThemeColour colour;
        } level;
        // Pan as a small bar that fills out from the centre, with L/C/R written on it.
        struct PanBar : juce::Slider
        {
            explicit PanBar (ThemeColour c) : colour (c)
            {
                setSliderStyle (juce::Slider::LinearBar);
                setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            }
            void paint (juce::Graphics& g) override
            {
                auto r = getLocalBounds().toFloat().reduced (0, 5);
                g.setColour (Colours::inset);
                g.fillRoundedRectangle (r, 4.0f);
                const float v = (float) valueToProportionOfLength (getValue()), mid = r.getCentreX();
                const float x = r.getX() + r.getWidth() * v;
                if (std::abs (x - mid) > 1.5f) // dead centre: just the tick, so the "C" stays readable
                {
                    g.setColour (colour.withAlpha (0.75f));
                    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, x), r.getY(), std::abs (x - mid), r.getHeight()), 3.0f);
                }
                g.setColour (Colours::textFaint);
                g.fillRect (mid - 0.5f, r.getY(), 1.0f, 3.0f);
                g.fillRect (mid - 0.5f, r.getBottom() - 3.0f, 1.0f, 3.0f);
                const int p = juce::roundToInt (getValue() * 100.0);
                g.setColour (Colours::text.withAlpha (0.9f));
                g.setFont (mono (9.5f));
                g.drawText (p == 0 ? "C" : (p < 0 ? "L" : "R") + juce::String (std::abs (p)), r, juce::Justification::centred, false);
            }
            ThemeColour colour;
        } pan { colour };
        juce::TextButton remove { "x" };
        struct SoloButton : juce::TextButton
        {
            SoloButton() : juce::TextButton ("S") {}
            bool lit = false;
            void paintButton (juce::Graphics& g, bool over, bool down) override
            {
                auto r = getLocalBounds().toFloat().reduced (0.5f);
                g.setColour (lit ? Palette::sub.withAlpha (0.85f) : (over || down ? Colours::panelHi.brighter (0.06f) : Colours::panelHi.withAlpha (0.8f)));
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (lit ? Palette::sub.get() : Colours::line.get());
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (lit ? Colours::bg0.get() : Colours::text.withAlpha (over ? 0.95f : 0.7f));
                g.setFont (mono (10.0f).boldened());
                g.drawText ("S", r, juce::Justification::centred, false);
            }
        } solo;
        juce::String on;
        bool soloed = false;
        juce::Rectangle<int> nameArea;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttach, panAttach;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> attachments;
    };

    std::vector<std::unique_ptr<Row>> rows;
    juce::String lastSignature, soloed;
    std::vector<std::pair<juce::String, float>> beforeSolo;

    // Everything that can be soloed: the oscillators that are on, the sub and the sampler.
    juce::StringArray soloable() const
    {
        juce::StringArray ids;
        for (int o = 0; o < NumOsc; ++o) ids.add (oscPrefix (o) + "On");
        ids.add ("subOn");
        ids.add ("smpOn");
        ids.add ("noiseLevel"); // the noise has no switch of its own: solo turns its level down
        return ids;
    }

    juce::String signature() const
    {
        juce::String s;
        for (int o = 2; o < NumOsc; ++o) s << (proc.apvts.getRawParameterValue (oscPrefix (o) + "On")->load() > 0.5f ? "1" : "0");
        s << (proc.sampleForUi() != nullptr || proc.apvts.getRawParameterValue ("smpOn")->load() > 0.5f ? "S" : "-");
        return s;
    }

    void rebuild()
    {
        lastSignature = signature();
        rows.clear();
        auto& st = proc.apvts;
        for (int o = 0; o < NumOsc; ++o)
        {
            const auto p = oscPrefix (o);
            if (o >= 2 && st.getRawParameterValue (p + "On")->load() < 0.5f) continue;
            const juce::String on = p + "On", lvl = p + "Level", pn = p + "Pan", rt = p + "Filter", bs = p + "Bus";
            rows.push_back (std::make_unique<Row> (*this, st, "OSC " + p.toUpperCase(), "osc" + p.toUpperCase(), colourFor (o),
                                                   on.toRawUTF8(), lvl.toRawUTF8(), pn.toRawUTF8(), rt.toRawUTF8(), o, bs.toRawUTF8()));
        }
        rows.push_back (std::make_unique<Row> (*this, st, "SUB", "sub", Palette::sub, "subOn", "subLevel", nullptr, "subFilter", -1, "subBus"));
        rows.push_back (std::make_unique<Row> (*this, st, "NOISE", "sub", Colours::textDim, nullptr, "noiseLevel", nullptr, "noiseFilter", -1, "noiseBus"));
        if (lastSignature.endsWith ("S"))
            rows.push_back (std::make_unique<Row> (*this, st, "SAMPLER", "sampler", Palette::oscA, "smpOn", "smpLevel", "smpPan", "smpFilter", -1, "smpBus"));
        for (auto& r : rows) addAndMakeVisible (*r);
        resized();
        repaint();
    }
};

} // namespace ab::ui
