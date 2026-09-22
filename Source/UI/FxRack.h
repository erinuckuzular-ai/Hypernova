#pragma once

#include "Components.h"
#include "Motion.h"
#include <map>
#include <complex>

// The effects rack. Every effect is a module with its own faceplate: a live display of what it's doing and
// all of its controls. Modules sit left to right in the order the sound goes through them; grab one by its
// name and drag it along the chain. "+" adds an effect, the x on a module takes it out (switches it off).
// Width and mono bass are the output module, always last.
namespace ab::ui
{

// What the rack hears, updated once a frame and shared by every module's display.
struct RackLive
{
    static constexpr int bands = 64, wavePoints = 96;
    std::array<float, bands> spectrum {};      // 0..1 per band, 30 Hz .. 16 kHz, log spaced
    std::array<float, wavePoints> wave {};     // the latest ~20 ms, normalised to its peak
    std::array<float, 256> l {}, r {};         // for the stereo view
    float level = 0, peak = 0;
    double beats = 0;
    float bpm = 120, time = 0;
    bool sounding = false;
    static float bandHz (int b) { return 30.0f * std::pow (16000.0f / 30.0f, (float) b / (float) (bands - 1)); }
};

// Each effect has its own colour, so a rack of them reads like a rack of different boxes rather than
// one long panel. They come from the theme, so they change with it.
inline ThemeColour fxColour (int fxId)
{
    switch (fxId)
    {
        case FxDist:    return Palette::sub;
        case FxTape:    return Palette::fx;
        case FxOtt:     return Palette::env;
        case FxPitch:   return Palette::oscB;
        case FxChorus:  return Palette::oscA;
        case FxFlanger: return Palette::lfo;
        case FxFilter:  return Palette::filter;
        case FxGate:    return Palette::fx;
        case FxDelay:   return Palette::oscA;
        case FxReverb:  return Palette::mod;
        case FxEq:      return Palette::env;
        case FxCrush:   return ThemeColour { SlotWarm };
        case FxSpeaker: return Palette::oscB;
        default:        return ThemeColour { SlotText };
    }
}

class FxModule : public juce::Component, public juce::SettableTooltipClient
{
public:
    FxModule (HypernovaAudioProcessor& p, int effect, int width) : proc (p), fxId (effect), designWidth (width), colour (fxColour (effect))
    {
        setTooltip (fxId >= 0 ? "Drag the name to move this effect along the chain" : "The output stage: always last");
    }

    const int fxId;        // -1: the output module
    const int designWidth;
    bool lifted = false;
    int chainIndex = 0;    // where it sits in the chain, shown on its face
    ThemeColour colour { SlotText };

    std::function<void (FxModule&, const juce::MouseEvent&)> onHeaderDown, onHeaderDrag, onHeaderUp;
    std::function<void (FxModule&)> onRemove;
    std::function<void (FxModule&, juce::Point<int> screenPos)> onMenuRequest;

    static juce::String nameOf (int id) { return id < 0 ? juce::String ("OUTPUT") : fxRackNames()[id]; }
    // Taller than designed: the display takes the extra room, the controls stay along the bottom.
    static constexpr int designHeight = 194;
    juce::Rectangle<int> display() const { return { 10, 32, getWidth() - 20, 54 + juce::jmax (0, getHeight() - designHeight) }; }
    void resized() override
    {
        if (designBounds.empty())
            for (auto* c : getChildren()) designBounds.push_back ({ c, c->getBounds() });
        const int dy = juce::jmax (0, getHeight() - designHeight);
        for (auto& [c, r] : designBounds)
            if (c != nullptr) c->setBounds (r.getY() < 30 ? r : r.translated (0, dy)); // the title row stays put
    }
    juce::Rectangle<int> removeButton() const { return { getWidth() - 28, 6, 20, 20 }; }
    bool on() const { return fxId < 0 || proc.apvts.getRawParameterValue (HypernovaAudioProcessor::fxOnParam (fxId))->load() > 0.5f; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        if (lifted)
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (juce::Colours::black.withAlpha (0.06f * (float) (5 - i)));
                g.fillRoundedRectangle (r.translated (0, 3.0f * (float) i).expanded ((float) i * 1.5f), 12.0f);
            }
        // Faceplate: a slightly raised card with an accent line along the top, like a unit in a rack.
        g.setGradientFill (juce::ColourGradient (Colours::panelHi.brighter (0.05f), 0, r.getY(), Colours::panel.get(), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 11.0f);
        g.setColour (lifted ? Colours::accent.get() : Colours::lineHi.get());
        g.drawRoundedRectangle (r, 11.0f, lifted ? 1.6f : 1.0f);
        const auto accent = colour.get();
        g.setColour (accent.withAlpha (on() ? 0.9f : 0.3f));
        g.fillRoundedRectangle (juce::Rectangle<float> (r.getX() + 12.0f, r.getY() + 1.5f, r.getWidth() - 24.0f, 2.0f), 1.0f);
        // The unit's own tint: a wash of its colour behind the face, stronger at the top.
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (on() ? 0.1f : 0.03f), 0, r.getY(), accent.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.7f, false));
        g.fillRoundedRectangle (r, 11.0f);

        // Name (the drag handle, with a grip), its place in the chain, and the remove button.
        int textX = fxId < 0 ? 12 : 34;
        if (fxId >= 0)
        {
            g.setColour (accent.withAlpha (on() ? 0.8f : 0.4f));
            for (int i = 0; i < 6; ++i)
                g.fillEllipse ((float) textX + (float) (i % 2) * 3.4f, 10.0f + (float) (i / 2) * 3.4f, 1.7f, 1.7f);
            textX += 12;
        }
        g.setColour (on() ? Colours::text.get() : Colours::textDim.get());
        g.setFont (mono (11.0f).boldened().withExtraKerningFactor (0.12f));
        const int nameW = juce::jmax (40, getWidth() - textX - 56);
        g.drawText (nameOf (fxId), juce::Rectangle<int> (textX, 6, nameW, 20), juce::Justification::centredLeft, true);
        if (fxId >= 0 && chainIndex > 0)
        {
            auto badge = juce::Rectangle<float> (18.0f, 14.0f).withCentre ({ (float) (getWidth() - 40), 16.0f });
            g.setColour (accent.withAlpha (0.16f));
            g.fillRoundedRectangle (badge, 7.0f);
            g.setColour (accent.withAlpha (0.9f));
            g.setFont (mono (9.0f).boldened());
            g.drawText (juce::String (chainIndex), badge, juce::Justification::centred, false);
        }
        if (fxId >= 0)
        {
            const auto b = removeButton().toFloat();
            g.setColour (removeHover ? Colours::text.get() : Colours::textFaint.get());
            g.drawLine (b.getX() + 6, b.getY() + 6, b.getRight() - 6, b.getBottom() - 6, 1.4f);
            g.drawLine (b.getRight() - 6, b.getY() + 6, b.getX() + 6, b.getBottom() - 6, 1.4f);
        }

        // Display.
        const auto d = display().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (d, 7.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (d.toNearestInt());
            paintDisplay (g, d.reduced (6.0f, 5.0f));
        }
        g.setColour (Colours::line);
        g.drawRoundedRectangle (d.reduced (0.5f), 7.0f, 1.0f);
        if (! on())
        {
            g.setColour (Colours::bg0.withAlpha (0.35f));
            g.fillRoundedRectangle (d, 7.0f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool overRemove = fxId >= 0 && removeButton().contains (e.getPosition());
        if (overRemove != removeHover) { removeHover = overRemove; repaint (removeButton()); }
        setMouseCursor (overRemove ? juce::MouseCursor::PointingHandCursor
                                   : (fxId >= 0 && e.y < 30) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { if (removeHover) { removeHover = false; repaint (removeButton()); } }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (fxId < 0) return;
        if (e.mods.isPopupMenu()) { if (onMenuRequest) onMenuRequest (*this, e.getScreenPosition()); return; }
        if (eqMouseDown (e)) return;
        if (removeButton().contains (e.getPosition())) { pressedRemove = true; return; }
        pressedRemove = false;
        if (e.y < 30 && onHeaderDown) onHeaderDown (*this, e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (eqMouseDrag (e)) return;
        if (! pressedRemove && fxId >= 0 && e.getMouseDownY() < 30 && onHeaderDrag) onHeaderDrag (*this, e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (eqMouseUp()) return;
        if (pressedRemove) { pressedRemove = false; if (removeButton().contains (e.getPosition()) && onRemove) onRemove (*this); return; }
        if (fxId >= 0 && e.getMouseDownY() < 30 && onHeaderUp) onHeaderUp (*this, e);
    }

private:
    HypernovaAudioProcessor& proc;
    bool removeHover = false, pressedRemove = false;
    std::vector<std::pair<juce::Component::SafePointer<juce::Component>, juce::Rectangle<int>>> designBounds;

    float v (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    // --- live displays ------------------------------------------------------------------------------------
    // Each module turns what the rack hears into a curve of what the effect is doing right now; the last
    // frames recede into depth behind it, like the Sound Space and the wavetable views.
    static constexpr int points = 48, depth = 12;
    std::array<std::array<float, points>, depth> history {};
    int head = 0;
    const RackLive* live = nullptr;
    float eqDragGain = 0;
    int eqNode = -1;

public:
    void setLive (const RackLive* l) { live = l; }

    // Called once a frame while the module is on screen.
    void advance()
    {
        // A unit that's switched off has nothing to show: its display holds still until something changes.
        if (! on() && fxId >= 0) return;
        if (live == nullptr || fxId == FxGate || fxId == FxDelay || fxId < 0 || fxId == FxEq || fxId == FxPitch) { repaint (display()); return; }
        head = (head + depth - 1) % depth;
        auto& out = history[(size_t) head];
        curveNow (out.data());
        repaint (display());
    }

private:
    float spec (float t) const // the live spectrum at 0..1 across the display
    {
        if (live == nullptr) return 0.0f;
        const float b = juce::jlimit (0.0f, (float) (RackLive::bands - 1), t * (float) (RackLive::bands - 1));
        const int i = (int) b;
        const float f = b - (float) i;
        return live->spectrum[(size_t) i] * (1.0f - f) + live->spectrum[(size_t) juce::jmin (RackLive::bands - 1, i + 1)] * f;
    }
    float wav (float t) const
    {
        if (live == nullptr) return 0.0f;
        const float b = juce::jlimit (0.0f, (float) (RackLive::wavePoints - 1), t * (float) (RackLive::wavePoints - 1));
        return live->wave[(size_t) b];
    }
    float distort (float x) const
    {
        const int type = (int) v ("distType");
        const float drive = 1.0f + v ("distDrive") * 12.0f, mix = v ("distMix");
        float y;
        switch (type)
        {
            case DistHard:    y = juce::jlimit (-1.0f, 1.0f, x * drive * 0.6f); break;
            case DistFold:    y = std::sin (x * drive * 0.9f); break;
            case Dist808:     y = std::tanh (x * drive * 0.5f) * (0.9f + 0.1f * std::abs (x)); break;
            case DistRectify: y = std::abs (std::tanh (x * drive * 0.5f)) * 2.0f - 1.0f; break;
            case DistCrush:   y = std::round (x * (2.0f + 30.0f / drive)) / (2.0f + 30.0f / drive); break;
            default:          y = std::tanh (x * drive * 0.45f); break;
        }
        return x + (y - x) * mix;
    }
    // Filter magnitude (0..~1.5) of the FX filter at frequency hz with cutoff fc.
    float filterMag (float hz, float fc) const
    {
        const int type = (int) v ("fxFltType");
        const float ratio = hz / juce::jmax (20.0f, fc), q = 0.7f + v ("fxFltRes") * 6.0f;
        const float peak = 1.0f / std::sqrt ((1.0f - ratio * ratio) * (1.0f - ratio * ratio) + ratio * ratio / (q * q));
        return type == 1 ? ratio * ratio * peak : type == 2 ? ratio / q * peak : peak;
    }
    float sweptCutoff() const
    {
        const float base = v ("fxFltFreq"), depthAmt = v ("fxFltDepth");
        if (depthAmt < 0.01f || live == nullptr) return base;
        const double period = dsp::syncRateBeats ((int) v ("fxFltRate"));
        const double ph = live->beats / juce::jmax (0.01, period);
        const float lfo = (float) std::sin ((ph - std::floor (ph)) * juce::MathConstants<double>::twoPi);
        return juce::jlimit (20.0f, 20000.0f, base * std::exp2 (lfo * depthAmt * 4.0f));
    }

    void curveNow (float* out) const
    {
        for (int i = 0; i < points; ++i)
        {
            const float t = (float) i / (float) (points - 1);
            float y = 0.5f;
            switch (fxId)
            {
                case FxDist: y = 0.5f + 0.45f * distort (wav (t)); break;
                case FxTape:
                {
                    const float wob = v ("tapeWow") * 0.08f * std::sin ((live != nullptr ? live->time : 0.0f) * 2.3f + t * 3.0f);
                    const float sat = v ("tapeSat");
                    float x = wav (juce::jlimit (0.0f, 1.0f, t + wob));
                    x = std::tanh (x * (1.0f + sat * 3.0f)) / std::tanh (1.0f + sat * 3.0f);
                    y = 0.5f + 0.42f * x + (juce::Random::getSystemRandom().nextFloat() - 0.5f) * v ("tapeNoise") * 0.12f;
                    break;
                }
                case FxOtt:
                {
                    const float a = v ("ott"), sp = spec (t);
                    y = 0.12f + 0.8f * (sp + (0.55f - sp) * a * 0.7f); // loud bands come down, quiet ones come up
                    break;
                }
                case FxChorus:
                {
                    const float ph = (live != nullptr ? live->time : 0.0f) * (0.5f + v ("chorusRate")) * 3.0f;
                    const float mix = v ("chorusMix");
                    y = 0.5f + 0.42f * (wav (t) * (1.0f - mix * 0.5f) + wav (juce::jlimit (0.0f, 1.0f, t + 0.04f * std::sin (ph))) * mix * 0.5f);
                    break;
                }
                case FxFlanger:
                {
                    const float ph = (live != nullptr ? live->time : 0.0f) * v ("flangRate") * 6.283f;
                    const float delay = 0.3f + 0.7f * v ("flangDepth") * (0.5f + 0.5f * std::sin (ph));
                    const float comb = 0.5f + 0.5f * std::cos (t * 40.0f * delay) * std::abs (v ("flangFb"));
                    y = 0.1f + 0.85f * spec (t) * (1.0f - v ("flangMix") * (1.0f - comb));
                    break;
                }
                case FxFilter:
                {
                    const float hz = RackLive::bandHz ((int) (t * (RackLive::bands - 1)));
                    y = 0.08f + 0.85f * juce::jlimit (0.0f, 1.0f, spec (t) * juce::jmin (1.4f, filterMag (hz, sweptCutoff())));
                    break;
                }
                case FxReverb:
                {
                    // The spectrum held and let go at the reverb's decay, so the waterfall shows the tail.
                    const float decay = 0.55f + 0.42f * v ("verbSize");
                    const float prev = history[(size_t) ((head + 1) % depth)][(size_t) i];
                    y = juce::jmax (0.08f + 0.85f * spec (t), 0.08f + (prev - 0.08f) * decay * (0.6f + 0.4f * v ("verbMix")));
                    break;
                }
                default: y = 0.1f + 0.85f * spec (t); break;
            }
            out[i] = juce::jlimit (0.0f, 1.0f, y);
        }
    }

    // The camera is fitted to the display: the scene's corners are projected, then scaled and centred so
    // the whole thing lands inside the box. (Fixed scales cropped the waterfall at the edges.)
    Camera3D camera (juce::Rectangle<float> r) const
    {
        Camera3D cam;
        cam.yaw = -0.28f + (live != nullptr ? 0.05f * std::sin (live->time * 0.3f) : 0.0f);
        cam.pitch = 0.55f;
        cam.distance = 3.6f;
        cam.fitInto (r, { -1.0f, 1.0f }, { -0.45f, 0.56f }, { -0.72f, 0.72f }); // floor to the tallest curve, front row to back
        return cam;
    }

    // The receding stack of curves on a perspective floor.
    void paintWaterfall (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) const
    {
        const auto cam = camera (r);
        const float floorY = -0.42f, heightK = 0.95f;
        g.setColour (Colours::line.withAlpha (0.55f));
        for (int k = 0; k <= 4; ++k)
        {
            const float z = -0.6f + 1.2f * (float) k / 4.0f;
            g.drawLine (juce::Line<float> (cam.project (-1.0f, floorY, z), cam.project (1.0f, floorY, z)), 0.8f);
        }
        for (int k = 0; k <= 6; ++k)
        {
            const float x = -1.0f + 2.0f * (float) k / 6.0f;
            g.drawLine (juce::Line<float> (cam.project (x, floorY, -0.6f), cam.project (x, floorY, 0.6f)), 0.8f);
        }
        for (int k = depth - 1; k >= 0; --k)
        {
            const auto& curve = history[(size_t) ((head + k) % depth)];
            const float z = -0.6f + 1.2f * (float) k / (float) (depth - 1);
            juce::Path line, fill;
            for (int i = 0; i < points; ++i)
            {
                const float x = -1.0f + 2.0f * (float) i / (float) (points - 1);
                const auto pt = cam.project (x, floorY + curve[(size_t) i] * heightK, z);
                if (i == 0) { line.startNewSubPath (pt); fill.startNewSubPath (cam.project (x, floorY, z)); fill.lineTo (pt); }
                else { line.lineTo (pt); fill.lineTo (pt); }
            }
            fill.lineTo (cam.project (1.0f, floorY, z));
            fill.closeSubPath();
            const float fade = 1.0f - (float) k / (float) depth;
            g.setColour (Colours::inset.withAlpha (0.85f)); // hides what's behind, so it reads as solid
            g.fillPath (fill);
            g.setColour (c.withAlpha (0.08f * fade));
            g.fillPath (fill);
            if (k == 0) glowStroke (g, line, c, 1.6f, 0.8f);
            else { g.setColour (c.withAlpha (0.55f * fade)); g.strokePath (line, juce::PathStrokeType (1.0f)); }
        }
    }


    // EQ: a flat, draggable view (like a mixing EQ), over the live spectrum.
    static float eqX (float hz, juce::Rectangle<float> r) { return r.getX() + r.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); }
    static float eqHz (float x, juce::Rectangle<float> r) { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth())); }
    static float eqY (float db, juce::Rectangle<float> r) { return r.getCentreY() - db / 18.0f * r.getHeight() * 0.45f; }
    float eqResponseDb (float hz) const
    {
        const double sr = 48000.0;
        auto mag = [&] (Biquad::Kind k, double f0, double q, double gain)
        {
            Biquad b;
            b.set (k, sr, f0, q, gain);
            const std::complex<double> z = std::polar (1.0, -juce::MathConstants<double>::twoPi * hz / sr);
            return std::abs (((double) b.b0 + (double) b.b1 * z + (double) b.b2 * z * z) / (1.0 + (double) b.a1 * z + (double) b.a2 * z * z));
        };
        double m = mag (Biquad::LowShelf, v ("eqLowFreq"), 0.7, v ("eqLow")) * mag (Biquad::Peak, v ("eqMidFreq"), v ("eqMidQ"), v ("eqMidGain"))
                 * mag (Biquad::HighShelf, v ("eqHighFreq"), 0.7, v ("eqHigh"));
        if (v ("eqLowCut") > 21.0f) m *= mag (Biquad::HighPass, v ("eqLowCut"), 0.707, 0);
        if (v ("eqHighCut") < 19900.0f) m *= mag (Biquad::LowPass, v ("eqHighCut"), 0.707, 0);
        return (float) juce::Decibels::gainToDecibels (m, -60.0);
    }
    struct EqNode { const char* freq; const char* gain; juce::Colour colour; };
    static EqNode eqNodeInfo (int n)
    {
        switch (n)
        {
            case 0: return { "eqLowCut", nullptr, juce::Colour (0xff7fd1ff) };
            case 1: return { "eqLowFreq", "eqLow", juce::Colour (0xffffb347) };
            case 2: return { "eqMidFreq", "eqMidGain", juce::Colour (0xffff5fa2) };
            case 3: return { "eqHighFreq", "eqHigh", juce::Colour (0xff7dffb0) };
            default: return { "eqHighCut", nullptr, juce::Colour (0xffb28dff) };
        }
    }
    juce::Point<float> eqNodePos (int n, juce::Rectangle<float> r) const
    {
        const auto info = eqNodeInfo (n);
        return { eqX (v (info.freq), r), info.gain != nullptr ? eqY (v (info.gain), r) : r.getCentreY() };
    }
    juce::Rectangle<float> eqArea() const { return display().toFloat().reduced (6.0f, 5.0f); }

    void paintEq (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        // Live spectrum, filled.
        juce::Path sp;
        sp.startNewSubPath (r.getX(), r.getBottom());
        for (int i = 0; i <= 60; ++i)
        {
            const float t = (float) i / 60.0f;
            sp.lineTo (r.getX() + r.getWidth() * t, r.getBottom() - r.getHeight() * 0.9f * spec (t));
        }
        sp.lineTo (r.getRight(), r.getBottom());
        sp.closeSubPath();
        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (sp);
        g.setColour (Colours::line);
        for (float hz : { 100.0f, 1000.0f, 10000.0f }) g.drawVerticalLine ((int) eqX (hz, r), r.getY(), r.getBottom());
        // The curve.
        juce::Path curve;
        for (int i = 0; i <= 100; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 100.0f;
            const float y = juce::jlimit (r.getY(), r.getBottom(), eqY (eqResponseDb (eqHz (x, r)), r));
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        glowStroke (g, curve, colour, 1.8f, 0.8f);
        // Band points: drag them; scroll on the middle one for its width.
        for (int n = 0; n < 5; ++n)
        {
            const auto p = eqNodePos (n, r);
            const auto info = eqNodeInfo (n);
            const bool active = n == eqNode;
            g.setColour (info.colour.withAlpha (active ? 0.35f : 0.18f));
            g.fillEllipse (juce::Rectangle<float> (active ? 18.0f : 14.0f, active ? 18.0f : 14.0f).withCentre (p));
            g.setColour (info.colour);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
        }
    }

public:
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (fxId == FxEq && eqArea().contains (e.position))
            if (auto* p = proc.apvts.getParameter ("eqMidQ"))
            {
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.3f));
                repaint (display());
                return;
            }
        juce::Component::mouseWheelMove (e, w);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (fxId != FxEq || ! eqArea().contains (e.position)) return;
        const int n = eqNodeAt (e.position);
        if (n < 0) return;
        const auto info = eqNodeInfo (n);
        for (auto* id : { info.freq, info.gain })
            if (id != nullptr)
                if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue());
    }

private:
    int eqNodeAt (juce::Point<float> pt) const
    {
        const auto r = eqArea();
        int best = -1;
        float bestD = 14.0f;
        for (int n = 0; n < 5; ++n)
        {
            const float d = pt.getDistanceFrom (eqNodePos (n, r));
            if (d < bestD) { bestD = d; best = n; }
        }
        return best;
    }
    bool eqMouseDown (const juce::MouseEvent& e)
    {
        if (fxId != FxEq || ! eqArea().contains (e.position)) return false;
        eqNode = eqNodeAt (e.position);
        if (eqNode < 0) return true;
        const auto info = eqNodeInfo (eqNode);
        for (auto* id : { info.freq, info.gain })
            if (id != nullptr) if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
        return true;
    }
    bool eqMouseDrag (const juce::MouseEvent& e)
    {
        if (fxId != FxEq || eqNode < 0) return false;
        const auto r = eqArea();
        const auto info = eqNodeInfo (eqNode);
        if (auto* p = proc.apvts.getParameter (info.freq)) p->setValueNotifyingHost (p->convertTo0to1 (eqHz (e.position.x, r)));
        if (info.gain != nullptr)
            if (auto* p = proc.apvts.getParameter (info.gain))
            {
                const float db = juce::jlimit (-18.0f, 18.0f, (r.getCentreY() - e.position.y) / (r.getHeight() * 0.45f) * 18.0f);
                p->setValueNotifyingHost (p->convertTo0to1 (db));
            }
        repaint (display());
        return true;
    }
    bool eqMouseUp()
    {
        if (fxId != FxEq || eqNode < 0) return false;
        const auto info = eqNodeInfo (eqNode);
        for (auto* id : { info.freq, info.gain })
            if (id != nullptr) if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
        eqNode = -1;
        repaint (display());
        return true;
    }

    // What each effect is doing, live.
    void paintDisplay (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const auto c = colour.get().withMultipliedAlpha (on() ? 1.0f : 0.5f);
        switch (fxId)
        {
            case FxEq: paintEq (g, r); return;
            case FxSpeaker:
            {
                // The band this box passes, with its honk: the response curve over the live spectrum.
                const auto voicing = dsp::Speaker::voicingFor ((int) v ("spkType"));
                const float mix = v ("spkMix");
                if (live != nullptr)
                {
                    g.setColour (c.withAlpha (0.16f));
                    for (int b = 0; b < RackLive::bands; ++b)
                    {
                        const float x = r.getX() + r.getWidth() * (float) b / (float) (RackLive::bands - 1);
                        const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, live->spectrum[(size_t) b]);
                        g.fillRect (x - 1.0f, r.getBottom() - h, 2.0f, h);
                    }
                }
                auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * std::log (juce::jmax (20.0f, hz) / 20.0f) / std::log (20000.0f / 20.0f); };
                juce::Path curve;
                for (int i = 0; i <= 64; ++i)
                {
                    const float hz = 20.0f * std::pow (1000.0f, (float) i / 64.0f);
                    // Roughly what the filters do: a band between the cuts, with the bump on top.
                    const float hp = hz / std::sqrt (hz * hz + voicing.hp * voicing.hp);
                    const float lp = voicing.lp / std::sqrt (hz * hz + voicing.lp * voicing.lp);
                    const float bump = 1.0f + (std::pow (10.0f, voicing.bumpDb / 20.0f) - 1.0f)
                                              / (1.0f + std::pow ((hz - voicing.bumpHz) / juce::jmax (30.0f, voicing.bumpHz / (2.0f * voicing.bumpQ)), 2.0f));
                    const float gain = 1.0f + (hp * lp * bump - 1.0f) * mix;
                    const float y = r.getBottom() - r.getHeight() * juce::jlimit (0.05f, 1.0f, 0.5f + 0.7f * std::log10 (juce::jmax (0.02f, gain)));
                    if (i == 0) curve.startNewSubPath (xOf (hz), y); else curve.lineTo (xOf (hz), y);
                }
                glowStroke (g, curve, c, 1.6f, 0.8f);
                g.setColour (Colours::textDim);
                g.setFont (mono (9.0f));
                g.drawText (dsp::Speaker::typeNames()[(int) v ("spkType")].toUpperCase(), r.reduced (3, 2), juce::Justification::topLeft, false);
                return;
            }
            case FxCrush:
            {
                // The sound as the crusher leaves it: the live wave held in steps and rounded to fewer levels.
                if (live == nullptr) return;
                const float bits = juce::jlimit (1.0f, 16.0f, v ("crushBits"));
                const float levels = juce::jmax (1.0f, std::pow (2.0f, bits) - 1.0f);
                const int hold = juce::jmax (1, juce::roundToInt (24000.0f / juce::jmax (200.0f, v ("crushRate"))));
                const float mix = v ("crushMix");
                juce::Path steps;
                float heldValue = 0;
                for (int i = 0; i < RackLive::wavePoints; ++i)
                {
                    const float raw = juce::jlimit (-1.0f, 1.0f, live->wave[(size_t) i]);
                    if (i % hold == 0) heldValue = std::round (raw * levels) / levels;
                    const float y = r.getCentreY() - (raw + (heldValue - raw) * mix) * r.getHeight() * 0.42f;
                    const float x = r.getX() + r.getWidth() * (float) i / (float) (RackLive::wavePoints - 1);
                    if (i == 0) steps.startNewSubPath (x, y);
                    else { steps.lineTo (x, y); }
                }
                g.setColour (Colours::line.withAlpha (0.5f));
                g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
                glowStroke (g, steps, c, 1.6f, 0.8f);
                g.setColour (Colours::textDim);
                g.setFont (mono (9.0f));
                g.drawText (juce::String (bits, 1) + " bit", r.reduced (3, 2), juce::Justification::topLeft, false);
                return;
            }
            case FxPitch:
            {
                // The spectrum, and a copy of it shifted up or down by the interval.
                const int semis = juce::roundToInt (v ("shiftSemis"));
                const float shift = (float) semis / 12.0f / 9.1f; // 9.1 octaves across the display
                for (int pass = 0; pass < 2; ++pass)
                {
                    juce::Path p;
                    for (int i = 0; i <= 60; ++i)
                    {
                        const float t = (float) i / 60.0f;
                        const float y = spec (juce::jlimit (0.0f, 1.0f, t - (pass == 1 ? shift : 0.0f))) * (pass == 1 ? v ("shiftMix") : 1.0f);
                        const juce::Point<float> pt (r.getX() + r.getWidth() * t, r.getBottom() - r.getHeight() * (0.08f + 0.8f * y));
                        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
                    }
                    if (pass == 0) { g.setColour (Colours::textDim.withAlpha (0.6f)); g.strokePath (p, juce::PathStrokeType (1.0f)); }
                    else glowStroke (g, p, c, 1.5f, 0.7f);
                }
                g.setColour (c);
                g.setFont (heavy (juce::jmin (18.0f, r.getHeight() * 0.35f)));
                g.drawText ((semis > 0 ? "+" : "") + juce::String (semis), r.reduced (4, 2), juce::Justification::topRight, false);
                return;
            }
            case FxGate:
            {
                // The pattern as blocks on a floor; the step playing now lights up.
                const auto cam = camera (r);
                const auto mask = dsp::GateAndPan::patternMask ((int) v ("gatePattern"));
                const float depthAmt = v ("gateDepth");
                const double stepBeats = dsp::syncRateBeats ((int) v ("gateRate"));
                const int now = live != nullptr ? (int) std::floor (live->beats / juce::jmax (0.01, stepBeats)) % 16 : -1;
                for (int s = 0; s < 16; ++s)
                {
                    const bool open = ((mask >> (15 - s)) & 1) != 0;
                    const float h = open ? 0.8f : 0.8f * (1.0f - depthAmt);
                    const float x0 = -1.0f + 2.0f * (float) s / 16.0f + 0.02f, x1 = x0 + 2.0f / 16.0f - 0.04f;
                    juce::Path face;
                    face.startNewSubPath (cam.project (x0, -0.42f, -0.1f));
                    face.lineTo (cam.project (x0, -0.42f + h, -0.1f));
                    face.lineTo (cam.project (x1, -0.42f + h, -0.1f));
                    face.lineTo (cam.project (x1, -0.42f, -0.1f));
                    face.closeSubPath();
                    const bool lit = s == now && live != nullptr && live->sounding;
                    g.setColour (c.withAlpha (lit ? 0.95f : open ? 0.45f : 0.18f));
                    g.fillPath (face);
                    if (lit) glowRect (g, face.getBounds(), 2.0f, c, 1.4f);
                }
                if (v ("panDepth") > 0.01f && live != nullptr)
                {
                    const double panBeats = dsp::syncRateBeats ((int) v ("panRate"));
                    const float pan = (float) std::sin (live->beats / juce::jmax (0.01, panBeats) * juce::MathConstants<double>::twoPi) * v ("panDepth");
                    const auto dot = cam.project (pan, 0.55f, -0.1f);
                    g.setColour (Colours::text);
                    g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre (dot));
                }
                return;
            }
            case FxDelay:
            {
                // Echoes stepping back into the distance, bouncing left and right with ping-pong, lit by the sound.
                const auto cam = camera (r);
                const float fb = v ("dlyFb"), mix = v ("dlyMix");
                const bool ping = v ("dlyPing") > 0.5f;
                const float lvl = live != nullptr ? juce::jlimit (0.2f, 1.0f, live->level * 4.0f + 0.2f) : 0.4f;
                float amp = 1.0f;
                for (int k = 9; k >= 0; --k)
                {
                    float a = 1.0f;
                    for (int j = 0; j < k; ++j) a *= fb;
                    if (a < 0.03f) continue;
                    const float z = -0.6f + 1.2f * (float) k / 9.0f;
                    const float x = ping ? (k % 2 == 0 ? -0.35f : 0.35f) : 0.0f;
                    const float h = 0.85f * a * (k == 0 ? 1.0f : 0.35f + 0.65f * mix);
                    const auto base = cam.project (x, -0.42f, z), top = cam.project (x, -0.42f + h, z);
                    const float w = 7.0f * (1.0f - 0.5f * (float) k / 9.0f);
                    const auto bar = juce::Rectangle<float> (base.x - w * 0.5f, top.y, w, base.y - top.y);
                    g.setColour (c.withAlpha ((k == 0 ? 0.95f : 0.8f) * lvl));
                    g.fillRoundedRectangle (bar, 2.0f);
                    juce::ignoreUnused (amp);
                }
                g.setColour (Colours::textDim);
                g.setFont (mono (9.0f));
                g.drawText (delayTimeNames()[(int) v ("dlyTime")], r.reduced (3, 2), juce::Justification::topLeft, false);
                return;
            }
            case -1:
            {
                // Output: the real stereo field (a vectorscope), and the width setting as a faint outline.
                const auto centre = r.getCentre();
                const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.47f;
                g.setColour (Colours::line);
                g.drawLine (centre.x - rad, centre.y, centre.x + rad, centre.y, 0.8f);
                g.drawLine (centre.x, centre.y - rad, centre.x, centre.y + rad, 0.8f);
                if (live != nullptr)
                {
                    const float gain = rad * 0.9f / juce::jmax (0.05f, live->peak);
                    juce::Path p;
                    for (int i = 0; i < 256; ++i)
                    {
                        const float m = (live->l[(size_t) i] + live->r[(size_t) i]) * 0.5f, sd = (live->l[(size_t) i] - live->r[(size_t) i]) * 0.5f;
                        const juce::Point<float> pt (centre.x + sd * gain, centre.y - m * gain);
                        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
                    }
                    g.setColour (c.withAlpha (0.8f));
                    g.strokePath (p, juce::PathStrokeType (1.0f));
                }
                const float w = rad * juce::jlimit (0.08f, 1.0f, v ("width") * 0.5f);
                g.setColour (colour.withAlpha (0.5f));
                g.drawEllipse (juce::Rectangle<float> (w * 2.0f, rad * 1.8f).withCentre (centre), 1.0f);
                if (v ("monoBass") > 0.5f)
                {
                    g.setColour (Palette::sub);
                    g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, rad * 0.5f).withCentre (centre.translated (0, rad * 0.55f)), 1.5f);
                }
                return;
            }
            default:
                paintWaterfall (g, r, c);
                if (fxId == FxFilter)
                {
                    // Where the cutoff is right now.
                    const float fc = sweptCutoff();
                    const float t = std::log (fc / 30.0f) / std::log (16000.0f / 30.0f);
                    const auto cam = camera (r);
                    const auto a = cam.project (-1.0f + 2.0f * juce::jlimit (0.0f, 1.0f, t), -0.42f, -0.6f);
                    const auto b = cam.project (-1.0f + 2.0f * juce::jlimit (0.0f, 1.0f, t), 0.5f, -0.6f);
                    g.setColour (Colours::text.withAlpha (0.6f));
                    g.drawLine (juce::Line<float> (a, b), 1.0f);
                }
                if (fxId == FxDist)
                {
                    g.setColour (Colours::textDim);
                    g.setFont (mono (9.0f));
                    g.drawText (distNames()[(int) v ("distType")].toUpperCase(), r.reduced (3, 2), juce::Justification::topLeft, false);
                }
                return;
        }
    }
};

//==============================================================================
class EffectsRack : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    static int widthOf (int id)
    {
        switch (id)
        {
            case FxDist: return 200;   case FxTape: return 200;    case FxOtt: return 130;   case FxPitch: return 150;
            case FxChorus: return 190; case FxFlanger: return 250; case FxFilter: return 250; case FxGate: return 250;
            case FxDelay: return 250;  case FxReverb: return 210;  case FxEq: return 310;    case FxCrush: return 200;
            case FxSpeaker: return 210;
            default: return 208; // the output stage
        }
    }

    explicit EffectsRack (HypernovaAudioProcessor& p) : proc (p)
    {
        for (int i = 0; i < NumFx; ++i)
        {
            modules.push_back (std::make_unique<FxModule> (p, i, widthOf (i)));
            auto& m = *modules.back();
            strip.addChildComponent (m);
            m.onHeaderDown = [this] (FxModule& mod, const juce::MouseEvent& e) { beginDrag (mod, e); };
            m.onHeaderDrag = [this] (FxModule& mod, const juce::MouseEvent& e) { dragTo (mod, e); };
            m.onHeaderUp = [this] (FxModule& mod, const juce::MouseEvent&) { endDrag (mod); };
            m.onRemove = [this] (FxModule& mod) { proc.removeFromRack (mod.fxId); refresh (true); };
            m.onMenuRequest = [this] (FxModule& mod, juce::Point<int> p) { if (onModuleMenu) onModuleMenu (mod.fxId, p); };
            m.setLive (&live);
        }
        outputModule = std::make_unique<FxModule> (p, -1, widthOf (-1));
        outputModule->setLive (&live);
        addAndMakeVisible (*outputModule);
        // Modules live in a strip that stops short of the output, so scrolled ones are cut off cleanly.
        addAndMakeVisible (strip);
        strip.setInterceptsMouseClicks (false, true);
        strip.onPaint = [this] (juce::Graphics& g) { paintStrip (g); };
        setTooltip ("Effects, in the order the sound goes through them. Drag an effect by its name to move it, right-click it for more.");
    }

    FxModule& module (int id) { return *modules[(size_t) id]; }

    // Once a frame (from the editor's timer): listen to the output and move every visible display on.
    void tick (bool sounding)
    {
        if (! sounding && ++quietFrames > 45 && ! live.sounding) return; // settled after the sound stopped
        if (sounding) quietFrames = 0;
        live.sounding = sounding;
        live.time += 1.0f / 30.0f;
        live.beats = proc.shownBeats.load();
        live.bpm = proc.shownBpm.load();
        proc.scope.latest (scopeL.data(), scopeR.data(), fftSize);
        // Spectrum in log bands.
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = 0.5f * (scopeL[(size_t) i] + scopeR[(size_t) i]) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1)));
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        const double sr = juce::jmax (8000.0, proc.getCurrentSampleRate());
        for (int b = 0; b < RackLive::bands; ++b)
        {
            const float lo = RackLive::bandHz (b) * 0.94f, hi = RackLive::bandHz (b) * 1.06f;
            const int k0 = juce::jlimit (1, fftSize / 2 - 1, (int) (lo * fftSize / sr)), k1 = juce::jlimit (k0, fftSize / 2 - 1, (int) (hi * fftSize / sr));
            float m = 0;
            for (int k = k0; k <= k1; ++k) m = juce::jmax (m, fftData[(size_t) k]);
            const float db = juce::Decibels::gainToDecibels (m * 4.0f / (float) fftSize, -90.0f);
            const float target = juce::jlimit (0.0f, 1.0f, (db + 70.0f) / 64.0f);
            auto& v = live.spectrum[(size_t) b];
            v = target > v ? target : v + (target - v) * 0.3f;
        }
        // The latest ~20 ms of the waveform, normalised, starting on a rising zero crossing so it stands still.
        const int span = (int) (0.02 * sr);
        int start = fftSize - span - 1;
        for (int i = fftSize - span - 1; i > fftSize / 2; --i)
        {
            const float a = scopeL[(size_t) i - 1] + scopeR[(size_t) i - 1], b = scopeL[(size_t) i] + scopeR[(size_t) i];
            if (a < 0.0f && b >= 0.0f) { start = i; break; }
        }
        float peak = 1.0e-4f, sum = 0;
        for (int i = 0; i < span; ++i)
        {
            const float m = 0.5f * (scopeL[(size_t) (start + i)] + scopeR[(size_t) (start + i)]);
            peak = juce::jmax (peak, std::abs (m));
            sum += m * m;
        }
        live.level = std::sqrt (sum / (float) juce::jmax (1, span));
        for (int p = 0; p < RackLive::wavePoints; ++p)
            live.wave[(size_t) p] = 0.5f * (scopeL[(size_t) (start + p * span / RackLive::wavePoints)] + scopeR[(size_t) (start + p * span / RackLive::wavePoints)]) / juce::jmax (0.05f, peak);
        live.peak = 1.0e-4f;
        for (int i = 0; i < 256; ++i)
        {
            live.l[(size_t) i] = scopeL[(size_t) (fftSize - 256 + i)];
            live.r[(size_t) i] = scopeR[(size_t) (fftSize - 256 + i)];
            live.peak = juce::jmax (live.peak, std::abs (live.l[(size_t) i]), std::abs (live.r[(size_t) i]));
        }
        for (auto& m : modules) if (m->isVisible()) m->advance();
        outputModule->advance();
    }
    FxModule& output() { return *outputModule; }
    std::function<void (juce::Point<int> screenPos)> onAdd;
    std::function<void (int fxId, juce::Point<int> screenPos)> onModuleMenu; // right-click a unit

    // Picks up changes from elsewhere (presets, undo, automation switching effects in or out).
    void refresh (bool force = false)
    {
        const int changes = proc.parameterChanges.load();
        if (! force && changes == lastChanges) return;
        lastChanges = changes;
        for (auto& m : modules) m->repaint (m->display());
        outputModule->repaint (outputModule->display());
        if (dragId >= 0) return;
        const auto now = proc.rackEffects();
        if (now != shown || force) { shown = now; layout (true); }
    }

    int shownCount() const { return (int) shown.size(); }
    juce::String metrics() const
    {
        return "w " + juce::String (getWidth()) + " right " + juce::String (scrollRight()) + " content " + juce::String (contentWidth())
             + " scroll " + juce::String (scroll) + " shown " + juce::String ((int) shown.size());
    }
    bool isScrollable() const { return maxScroll() > 0.5f; }
    float scrollPosition() const { return scroll; }
    float scrollLimit() const { return maxScroll(); }

    void resized() override { layout (false); }

    void paint (juce::Graphics& g) override
    {
        // The case the units sit in: a shallow trough with a lit top edge, so the rack reads as a rack.
        const auto trough = juce::Rectangle<float> (0.0f, 0.0f, (float) scrollRight() + gap * 0.5f, (float) getHeight());
        g.setColour (Colours::bg0.withAlpha (0.35f));
        g.fillRoundedRectangle (trough, 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawRoundedRectangle (trough.reduced (0.5f), 10.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.16f), 0, trough.getY(), juce::Colours::transparentBlack, 0, trough.getY() + 8.0f, false));
        g.fillRoundedRectangle (trough, 10.0f);
        // The last patch lead, into the output.
        g.setColour (Colours::line);
        const float y = (float) getHeight() * 0.5f;
        g.drawHorizontalLine ((int) y, (float) scrollRight(), (float) outputModule->getX());
    }

    void paintStrip (juce::Graphics& g)
    {
        // Patch leads between the units, in the colour of the effect the sound is coming from.
        const float y = (float) getHeight() * 0.5f;
        float from = 0.0f;
        for (size_t i = 0; i <= shown.size(); ++i)
        {
            const float to = i < shown.size() ? module (shown[i]).getX() : slotX ((int) shown.size());
            if (to - from > 6.0f)
            {
                const auto c = i == 0 ? Colours::line.get() : fxColour (shown[i - 1]).get().withAlpha (0.55f);
                g.setColour (c);
                g.drawLine (from, y, to - 5.0f, y, 1.4f);
                juce::Path head;
                head.addTriangle (to - 5.0f, y - 3.5f, to - 5.0f, y + 3.5f, to, y);
                g.fillPath (head);
            }
            if (i < shown.size()) from = (float) module (shown[i]).getRight();
        }


        // "+" slot at the end of the chain.
        const auto add = addSlot().toFloat();
        juce::Path outline;
        outline.addRoundedRectangle (add.reduced (1.0f), 11.0f);
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (outline, outline, dashes, 2);
        g.setColour (addHover ? Colours::accent.get() : Colours::lineHi.get());
        g.fillPath (outline);
        g.setColour (addHover ? Colours::text.get() : Colours::textDim.get());
        g.setFont (heavy (26.0f));
        g.drawText ("+", add.withTrimmedBottom (add.getHeight() * 0.3f), juce::Justification::centred, false);
        g.setFont (mono (10.0f).boldened().withExtraKerningFactor (0.12f));
        g.drawText (shown.empty() ? "ADD AN EFFECT" : "ADD EFFECT", add.withTrimmedTop (add.getHeight() * 0.55f).withHeight (18.0f), juce::Justification::centred, false);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! isScrollable()) return;
        // Soft edges where modules run off, and where you are along the chain.
        auto fade = [&] (juce::Rectangle<float> r, bool left)
        {
            g.setGradientFill (juce::ColourGradient (Colours::panel.withAlpha (left ? 1.0f : 0.0f), r.getX(), 0,
                                                     Colours::panel.withAlpha (left ? 0.0f : 1.0f), r.getRight(), 0, false));
            g.fillRect (r);
        };
        const float right = (float) scrollRight();
        if (scroll > 0.5f) fade ({ 0.0f, 0.0f, 24.0f, (float) getHeight() }, true); // nothing to fade in while pulled past the start
        if (scroll < maxScroll() - 0.5f) fade ({ right - 24.0f, 0.0f, 24.0f, (float) getHeight() }, false);
        const float frac = right / (right + maxScroll());
        auto track = juce::Rectangle<float> (4.0f, (float) getHeight() - 3.0f, right - 8.0f, 2.5f);
        g.setColour (Colours::line);
        g.fillRoundedRectangle (track, 1.2f);
        g.setColour (Colours::textDim);
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * frac).withX (track.getX() + track.getWidth() * (1.0f - frac) * juce::jlimit (0.0f, 1.0f, scroll / maxScroll())), 1.2f);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool over = addSlot().contains (e.getPosition());
        if (over != addHover) { addHover = over; strip.repaint (addSlot()); }
        setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { if (addHover) { addHover = false; strip.repaint (addSlot()); } }

    // Grab the rack anywhere between modules and pull it along; let go and it carries on, then settles.
    void mouseDown (const juce::MouseEvent& e) override
    {
        panning = false;
        panStart = scroll;
        scrollSpring.velocity = 0.0f;
        settling = false; // grabbing it stops it where it is
        panTracker.reset();
        panTracker.add (e.position);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! isScrollable() && ! panning) return;
        if (! panning && std::abs (e.getDistanceFromDragStartX()) < 4) return;
        panning = true;
        panTracker.add (e.position);
        setScrollRubber (panStart - (float) e.getDistanceFromDragStartX());
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (panning)
        {
            panning = false;
            // Flick: aim for where it would come to rest, keeping the speed it was let go at.
            const float v = -panTracker.velocity().x;
            settleScroll (scroll + motion::project (v), v);
            return;
        }
        if (addSlot().contains (e.getPosition()) && onAdd) onAdd (e.getScreenPosition());
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! isScrollable()) { juce::Component::mouseWheelMove (e, w); return; }
        const float delta = (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * (w.isReversed ? -1.0f : 1.0f);
        if (! w.isSmooth)
        {
            // A mouse wheel's clicks glide rather than step.
            settleScroll ((settling ? scrollSpring.target : scroll) - delta * 260.0f, scrollSpring.velocity);
            return;
        }
        // Trackpad: follow the fingers 1:1, give a little at the ends, spring back once they let go.
        if (! wheeling) { rawScroll = scroll; wheeling = true; }
        settling = false;
        rawScroll -= delta * 260.0f * (w.isInertial && (rawScroll < 0.0f || rawScroll > maxScroll()) ? 0.35f : 1.0f);
        setScrollRubber (rawScroll);
        lastWheel = juce::Time::getMillisecondCounterHiRes();
        if (! isTimerRunning()) startMotion();
    }

    // Brings a module into view (e.g. one just added).
    void reveal (int id)
    {
        const auto it = std::find (shown.begin(), shown.end(), id);
        if (it == shown.end()) return;
        const float x = slotX ((int) (it - shown.begin())) + scroll;
        const float w = (float) module (id).getWidth();
        if (x - scroll < 0.0f) settleScroll (x - gap, 0.0f);
        else if (x - scroll + w > (float) scrollRight()) settleScroll (x + w - (float) scrollRight() + gap, 0.0f);
    }

private:
    HypernovaAudioProcessor& proc;
    std::vector<std::unique_ptr<FxModule>> modules;
    std::unique_ptr<FxModule> outputModule;
    std::vector<int> shown;
    struct Strip : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    } strip;
    RackLive live;
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> scopeL {}, scopeR {};
    int quietFrames = 0;
    std::map<int, motion::Spring> shownX, shownY; // where each module is drawn now (springs to its slot)
    int lastChanges = -1, dragId = -1;
    float scroll = 0, grabOffset = 0, dragX = 0, lastMouseX = 0;
    bool addHover = false;
    // Scrolling: 'scroll' is what's on screen and can run a little past either end while you pull.
    motion::Spring scrollSpring;
    motion::VelocityTracker panTracker, dragTracker;
    float panStart = 0, rawScroll = 0;
    bool panning = false, wheeling = false, settling = false;
    double lastWheel = 0, lastFrame = 0;
    static constexpr float gap = 12.0f;

    float moduleHeight() const { return (float) getHeight() - 8.0f; }
    int scrollRight() const { return outputModule->getX() - (int) gap; }
    float contentWidth() const
    {
        float w = gap;
        for (int id : shown) w += (float) widthOf (id) + gap;
        return w + 120.0f + gap; // and the "+" slot
    }
    float maxScroll() const { return juce::jmax (0.0f, contentWidth() - (float) scrollRight()); }
    float slotX (int index) const
    {
        float x = gap - scroll;
        for (int i = 0; i < index && i < (int) shown.size(); ++i) x += (float) widthOf (shown[(size_t) i]) + gap;
        return x;
    }
    juce::Rectangle<int> addSlot() const
    {
        return juce::Rectangle<float> (slotX ((int) shown.size()), 4.0f, 120.0f, moduleHeight()).toNearestInt();
    }

    // Moves what's on screen to 's' exactly (modules keep their own motion relative to the rack).
    void applyScroll (float s)
    {
        if (std::abs (s - scroll) < 0.01f) return;
        for (auto& [id, x] : shownX) { x.value += scroll - s; x.target += scroll - s; }
        scroll = s;
        layout (false);
    }
    void setScroll (float s) { applyScroll (juce::jlimit (0.0f, maxScroll(), s)); }
    // Past either end the rack follows less and less, so the end feels soft rather than a wall.
    void setScrollRubber (float s)
    {
        const float top = maxScroll(), dim = (float) juce::jmax (1, scrollRight());
        if (s < 0.0f) s = -motion::rubberband (-s, dim);
        else if (s > top) s = top + motion::rubberband (s - top, dim);
        applyScroll (s);
    }
    // Springs the scroll to 'target' (kept in range) starting at the current speed.
    void settleScroll (float target, float velocity)
    {
        if (motion::systemReducesMotion()) { setScroll (target); return; }
        scrollSpring.value = scroll;
        scrollSpring.velocity = velocity;
        scrollSpring.target = juce::jlimit (0.0f, maxScroll(), target);
        settling = true;
        wheeling = false;
        startMotion();
    }
    void startMotion()
    {
        if (! isTimerRunning()) { lastFrame = juce::Time::getMillisecondCounterHiRes(); startTimerHz (60); }
    }

    void layout (bool animate)
    {
        const int outW = widthOf (-1);
        outputModule->setBounds (getWidth() - outW - 4, 4, outW, (int) moduleHeight());
        strip.setBounds (0, 0, scrollRight(), getHeight());
        if (! panning && ! wheeling && ! settling) scroll = juce::jlimit (0.0f, maxScroll(), scroll);
        const bool still = motion::systemReducesMotion();
        for (auto& m : modules)
        {
            const auto it = std::find (shown.begin(), shown.end(), m->fxId);
            if (it == shown.end()) { m->setVisible (false); shownX.erase (m->fxId); shownY.erase (m->fxId); continue; }
            const float target = slotX ((int) (it - shown.begin()));
            m->chainIndex = (int) (it - shown.begin()) + 1;
            if (m->fxId == dragId) continue;
            auto& x = shownX[m->fxId];
            auto& y = shownY[m->fxId];
            if (! animate || still || ! m->isVisible()) { x.snap (target); y.snap (4.0f); }
            else x.target = target;
            m->setVisible (true);
            m->setBounds (juce::roundToInt (x.value), juce::roundToInt (y.value), widthOf (m->fxId), (int) moduleHeight());
        }
        if (animate) startMotion();
        repaint();
        strip.repaint();
    }

    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float dt = (float) juce::jlimit (0.001, 0.05, (now - lastFrame) / 1000.0);
        lastFrame = now;
        bool moving = false;
        if (dragId >= 0 && isScrollable())
        {
            // Dragging near either end scrolls the rack along with you.
            const float edge = 40.0f;
            float step = 0.0f;
            if (lastMouseX < edge) step = -(edge - lastMouseX) * 0.3f;
            else if (lastMouseX > (float) scrollRight() - edge) step = (lastMouseX - ((float) scrollRight() - edge)) * 0.3f;
            if (step != 0.0f) { setScroll (scroll + step); updateTarget(); moving = true; }
        }
        // Trackpad let go past an end: spring back once the fingers (and their momentum) stop.
        if (wheeling && now - lastWheel > 70.0)
        {
            wheeling = false;
            if (scroll < 0.0f || scroll > maxScroll()) settleScroll (scroll, 0.0f);
        }
        if (wheeling) moving = true;
        if (settling)
        {
            settling = scrollSpring.step (dt, 0.42f, 1.0f);
            applyScroll (scrollSpring.value);
            moving |= settling;
        }
        for (size_t i = 0; i < shown.size(); ++i)
        {
            const int id = shown[i];
            if (id == dragId) continue;
            auto& x = shownX[id];
            auto& y = shownY[id];
            x.target = slotX ((int) i);
            y.target = 4.0f;
            const bool thrown = std::abs (x.velocity) > 600.0f;
            moving |= x.step (dt, 0.3f, thrown ? 0.84f : 1.0f);
            moving |= y.step (dt, 0.22f, 1.0f);
            module (id).setTopLeftPosition (juce::roundToInt (x.value), juce::roundToInt (y.value));
        }
        repaint();
        strip.repaint();
        if (! moving && dragId < 0 && ! panning) stopTimer();
    }

    void beginDrag (FxModule& m, const juce::MouseEvent& e)
    {
        dragId = m.fxId;
        grabOffset = (float) e.getEventRelativeTo (&strip).x - (float) m.getX();
        dragX = (float) m.getX();
        dragTracker.reset();
        dragTracker.add ({ dragX, 0.0f });
        m.lifted = true;
        m.toFront (false);
        outputModule->toFront (false);
        startMotion();
    }

    void dragTo (FxModule& m, const juce::MouseEvent& e)
    {
        if (dragId != m.fxId) return;
        lastMouseX = (float) e.getEventRelativeTo (&strip).x;
        // 1:1 with the pointer; past either end it gives a little rather than stopping dead.
        const float free = lastMouseX - grabOffset, top = (float) scrollRight() - (float) m.getWidth();
        dragX = free < 0.0f ? -motion::rubberband (-free, (float) m.getWidth())
              : free > top ? top + motion::rubberband (free - top, (float) m.getWidth()) : free;
        dragTracker.add ({ dragX, 0.0f });
        m.setTopLeftPosition (juce::roundToInt (dragX), 0);
        updateTarget();
        m.repaint();
    }

    // The dragged module goes where its middle is; the others shuffle to make room.
    void updateTarget()
    {
        if (dragId < 0) return;
        const float mid = dragX + (float) widthOf (dragId) * 0.5f;
        auto order = shown;
        order.erase (std::find (order.begin(), order.end(), dragId));
        int index = 0;
        float x = gap - scroll;
        for (; index < (int) order.size(); ++index)
        {
            const float w = (float) widthOf (order[(size_t) index]);
            if (mid < x + w * 0.5f) break;
            x += w + gap;
        }
        order.insert (order.begin() + index, dragId);
        if (order != shown) { shown = order; startMotion(); }
    }

    void endDrag (FxModule& m)
    {
        if (dragId != m.fxId) return;
        m.lifted = false;
        // Let go: it carries on at the speed it was moving into its slot, and settles back down.
        auto& x = shownX[dragId];
        x.value = dragX;
        x.velocity = motion::systemReducesMotion() ? 0.0f : dragTracker.velocity().x;
        shownY[dragId].value = 0.0f;
        shownY[dragId].velocity = 0.0f;
        dragId = -1;
        // Write the new order back: the shown effects in their new order, the others keep their places.
        auto full = proc.getFxOrder();
        std::vector<int> slots;
        for (int i = 0; i < NumFx; ++i)
            if (std::find (shown.begin(), shown.end(), (int) full[(size_t) i]) != shown.end()) slots.push_back (i);
        for (size_t k = 0; k < slots.size() && k < shown.size(); ++k) full[(size_t) slots[k]] = (juce::uint8) shown[k];
        proc.setFxOrder (full);
        startMotion();
        m.repaint();
    }
};

} // namespace ab::ui
