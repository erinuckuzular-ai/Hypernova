#pragma once

#include "Components.h"
#include "Motion.h"
#include "Dock.h"

// Widgets: every panel of the synth is a widget that keeps its own look and contents. Widgets dock into the
// work area (UI/Dock.h): they share it with no gaps, can be stacked into tabs, collapsed to a strip,
// maximised, hidden and added back, and new ones (scopes, meters, an XY pad, a pinboard...) can be added
// from the widget library. Rearranging happens in layout mode; in normal use the knobs work and nothing
// moves, except that the gutters between widgets can always be dragged to share space differently.
//
// A widget's contents are built once at a design size. Given more room, a widget shows more instead of
// zooming: its displays (wavetables, envelopes, filter curve...) grow and its controls spread out.
// Hiding or moving a widget never touches the sound; layouts are stored per user, apart from the patch.
namespace ab::ui
{

//==============================================================================
// Re-lays a component's children for a size at least as big as the one they were designed at.
// Children flagged Stretch grow (the displays); everything else keeps its size and moves with them.
// Along each axis the stretching children form bands; the space between bands keeps its size, so
// controls stay close to the display they belong to. With nothing stretching, controls spread evenly.
class Spread
{
public:
    enum Flags { StretchX = 1, StretchY = 2, Stretch = 3, Pinned = 8, FollowX = 16 }; // FollowX: a title-row item that moves with the body

    // Records every child's current bounds as its design position. Children in the header band
    // (above headerH) stay in the header: left-hand ones stay put, right-hand ones follow the right edge.
    void capture (juce::Component& parent, int designW, int designH, int headerHeight, std::initializer_list<juce::Component*> skip = {})
    {
        dW = designW; dH = designH; headerH = headerHeight;
        W = dW; H = dH;
        items.clear();
        for (auto* c : parent.getChildren())
        {
            bool skipped = false;
            for (auto* s : skip) skipped |= s == c;
            if (! skipped) items.push_back ({ c, c->getBounds(), 0 });
        }
        rebuildBands();
    }

    void setFlags (juce::Component& c, int flags)
    {
        for (auto& i : items) if (i.c == &c) i.flags = flags;
        rebuildBands();
    }

    void apply (int width, int height)
    {
        W = juce::jmax (dW, width);
        H = juce::jmax (dH, height);
        for (auto& i : items)
        {
            if (i.c == nullptr) continue;
            if ((i.flags & Pinned) != 0) { i.c->setBounds (i.r); continue; }
            i.c->setBounds (mapItem (i.r, i.flags));
        }
    }

    // For painted captions and dividers: where a design rectangle sits now.
    juce::Rectangle<float> map (juce::Rectangle<float> r) const { return mapItem (r.toNearestInt(), 0).toFloat(); }
    float mapX (float x) const { return (float) px (x, colsX, (float) dW, (float) W); }
    float mapY (float y) const { return y <= (float) headerH ? y : (float) py (y); }
    int width() const { return W; }
    int height() const { return H; }

private:
    struct Item { juce::Component::SafePointer<juce::Component> c; juce::Rectangle<int> r; int flags; };
    std::vector<Item> items;
    std::vector<juce::Range<float>> colsX, rowsY;
    int dW = 1, dH = 1, headerH = 0, W = 1, H = 1;
    static constexpr float margin = 12.0f;

    void rebuildBands()
    {
        auto merge = [] (std::vector<juce::Range<float>> v)
        {
            std::sort (v.begin(), v.end(), [] (auto a, auto b) { return a.getStart() < b.getStart(); });
            std::vector<juce::Range<float>> out;
            for (auto r : v)
            {
                if (! out.empty() && r.getStart() <= out.back().getEnd() + 1.0f) out.back() = out.back().getUnionWith (r);
                else out.push_back (r);
            }
            return out;
        };
        std::vector<juce::Range<float>> xs, ys;
        for (auto& i : items)
        {
            if ((i.flags & StretchX) != 0) xs.push_back ({ (float) i.r.getX(), (float) i.r.getRight() });
            if ((i.flags & StretchY) != 0) ys.push_back ({ (float) juce::jmax (headerH, i.r.getY()), (float) i.r.getBottom() });
        }
        colsX = merge (xs);
        rowsY = merge (ys);
    }

    // Piecewise mapping of one coordinate from [lo, design] to [lo, target]: bands take all the growth in
    // proportion to their size, gaps between them keep theirs. No bands: an even spread inside the margins.
    static double piecewise (float v, const std::vector<juce::Range<float>>& bands, float lo, float design, float target)
    {
        const float extra = target - design;
        if (extra <= 0.0f) return v;
        if (bands.empty())
        {
            const float a = lo + margin, b = design - margin;
            if (v <= a || b <= a) return v;
            return a + (v - a) * (target - margin - a) / (b - a);
        }
        float total = 0;
        for (auto r : bands) total += r.getLength();
        double out = v, shift = 0;
        for (auto r : bands)
        {
            const double grow = extra * r.getLength() / juce::jmax (1.0f, total);
            if (v >= r.getEnd()) shift += grow;
            else if (v > r.getStart()) { out = r.getStart() + shift + (v - r.getStart()) * (r.getLength() + grow) / juce::jmax (1.0f, r.getLength()); return out; }
            else break;
        }
        return v + shift;
    }

    double px (float x, const std::vector<juce::Range<float>>& cols, float design, float target) const { return piecewise (x, cols, 0.0f, design, target); }
    double py (float y) const { return piecewise (y, rowsY, (float) headerH, (float) dH, (float) H); }

    juce::Rectangle<int> mapItem (juce::Rectangle<int> r, int flags) const
    {
        if (headerH > 0 && r.getBottom() <= headerH + 2)
        {
            // Header: stays in the title row, left-hand items put, right-hand ones following the right edge.
            if ((flags & FollowX) != 0)
                return r.withX (juce::roundToInt (px ((float) r.getCentreX(), colsX, (float) dW, (float) W) - r.getWidth() * 0.5));
            if ((float) r.getCentreX() > (float) dW * 0.5f) return r.translated (W - dW, 0);
            return r;
        }
        juce::Rectangle<int> out = r;
        if ((flags & StretchX) != 0)
        {
            const int l = juce::roundToInt (px ((float) r.getX(), colsX, (float) dW, (float) W));
            const int rr = juce::roundToInt (px ((float) r.getRight(), colsX, (float) dW, (float) W));
            out.setX (l); out.setWidth (rr - l);
        }
        else out.setX (juce::roundToInt (px ((float) r.getCentreX(), colsX, (float) dW, (float) W) - r.getWidth() * 0.5));
        if ((flags & StretchY) != 0)
        {
            const int t = juce::roundToInt (py ((float) r.getY())), b = juce::roundToInt (py ((float) r.getBottom()));
            out.setY (t); out.setHeight (b - t);
        }
        else out.setY (juce::roundToInt (py ((float) r.getCentreY()) - r.getHeight() * 0.5));
        return out;
    }
};

//==============================================================================
class Widget;

// The contents of a widget: a plain component whose painter draws the panel's own titles and captions.
// A right-click on its background opens the widget menu; a double-click on its title row maximises it.
class WidgetContent : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    std::function<void (const juce::MouseEvent&)> onMenu, onTitleDoubleClick;
    int headerH = 40;
    void paint (juce::Graphics& g) override { if (painter) painter (g); }
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && onMenu) onMenu (e); }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (e.y < headerH && onTitleDoubleClick) onTitleDoubleClick (e); }
};

// Tabs for a stack of widgets sharing one place.
class StackTabs : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    std::function<void (int, const juce::MouseEvent&)> onMenu;

    void setTabs (juce::StringArray names, int active, ThemeColour c)
    {
        labels = std::move (names);
        current = active;
        colour = c;
        repaint();
    }
    int count() const { return labels.size(); }
    static int preferredWidth (int n) { return juce::jmin (150, 124) * n; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
        const int n = juce::jmax (1, labels.size());
        const float w = r.getWidth() / (float) n;
        for (int i = 0; i < labels.size(); ++i)
        {
            auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f);
            if (i == current)
            {
                g.setColour (colour.withAlpha (0.16f));
                g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
                g.setColour (colour.withAlpha (0.7f));
                g.drawRoundedRectangle (cell, cell.getHeight() * 0.5f, 1.0f);
            }
            g.setColour (i == current ? Colours::text : Colours::textDim);
            g.setFont (font (10.0f, true).withExtraKerningFactor (0.14f));
            g.drawFittedText (labels[i], cell.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
        }
    }

    int indexAt (int x) const
    {
        if (labels.isEmpty()) return -1;
        return juce::jlimit (0, labels.size() - 1, (int) ((float) x / ((float) getWidth() / (float) labels.size())));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.x);
        if (i < 0) return;
        if (e.mods.isPopupMenu()) { if (onMenu) onMenu (i, e); return; }
        if (i != current && onSelect) onSelect (i);
    }

private:
    juce::StringArray labels;
    int current = 0;
    ThemeColour colour { SlotMod };
};

//==============================================================================
class Widget : public juce::Component, public juce::SettableTooltipClient
{
public:
    Widget (const juce::String& widgetId, const juce::String& widgetType, const juce::String& widgetTitle, ThemeColour c,
            int designWidth, int designHeight, int titleOffsetX = 14, int headerHeight = 40)
        : id (widgetId), type (widgetType), title (widgetTitle), colour (c), designW (designWidth), designH (designHeight),
          titleX (titleOffsetX), headerH (headerHeight)
    {
        addAndMakeVisible (content);
        content.setBounds (0, 0, designW, designH);
        content.headerH = headerH;
        addChildComponent (tabs);
        content.painter = [this] (juce::Graphics& g)
        {
            if (! tabsInHeader()) sectionLabel (g, title, { (float) titleX, 10.0f, 260.0f, 24.0f }, colour);
            if (extraPaint) extraPaint (g);
        };
        content.onMenu = [this] (const juce::MouseEvent& e) { if (onMenu) onMenu (*this, e.getScreenPosition()); };
        content.onTitleDoubleClick = [this] (const juce::MouseEvent&) { if (onMaximise) onMaximise (*this); };
        tabs.onMenu = [this] (int, const juce::MouseEvent& e) { if (onMenu) onMenu (*this, e.getScreenPosition()); };
    }

    const juce::String id, type;
    juce::String title;
    ThemeColour colour;
    const int designW, designH, titleX, headerH;
    WidgetContent content;
    StackTabs tabs;
    Spread spread;
    std::function<void (juce::Graphics&)> extraPaint;  // the panel's own captions and dividers (map them with spread)
    std::function<void (int w, int h)> onLayout;        // custom layout in design units, after the spread
    std::function<void (Widget&, juce::Point<int> screenPos)> onMenu;
    std::function<void (Widget&)> onMaximise, onExpand;
    int headerFreeWidth = 0;  // room in the title row for tabs, from titleX (0: tabs get their own strip)
    bool fitHeight = false;   // scale by height only and lay out to any width (onLayout): for strips that scroll
    bool collapsed = false, sideways = false;           // sideways: collapsed into a vertical spine
    bool editing = false, lifted = false, maximised = false;
    bool dimmed = false; // its source is switched off: the body fades back, the title and its power light don't

    // Called once the contents are built: remembers every control's design position.
    void finishBuilding (std::initializer_list<juce::Component*> skip = {}) { spread.capture (content, designW, designH, headerH, skip); }

    // The size a panel needs before its controls stop being readable: splitting to anything smaller makes
    // a tab instead. (The hard minimum above is what the tree may shrink to when a window is small.)
    juce::Point<int> comfortableSize() const
    {
        return { juce::roundToInt ((float) designW * 0.72f), juce::roundToInt ((float) (designH + tabStripHeight()) * 0.72f) };
    }
    juce::Point<int> minimumSize() const
    {
        if (fitHeight) return { 300, juce::roundToInt ((float) (designH + tabStripHeight()) * minScale) };
        return { juce::roundToInt ((float) designW * minScale), juce::roundToInt ((float) (designH + tabStripHeight()) * minScale) };
    }
    float scale() const { return scaleNow; }

    void setStack (const juce::StringArray& titles, int index)
    {
        // Called on every relayout: only lay out again when the tabs or the folded state actually changed
        // (otherwise a resize in progress would be laid out at every step after all).
        const auto sig = titles.joinIntoString ("|") + "#" + juce::String (index) + (collapsed ? "c" : "") + (sideways ? "s" : "") + (maximised ? "m" : "");
        if (sig == stackSig) return;
        stackSig = sig;
        stackTitles = titles;
        stackIndex = index;
        tabs.setVisible (titles.size() > 1 && ! collapsed);
        tabs.setTabs (titles, index, colour);
        layoutContent();
        repaint();
    }
    bool stacked() const { return stackTitles.size() > 1; }
    const juce::StringArray& getStackTitles() const { return stackTitles; }

    void setEditing (bool e)
    {
        if (e == editing) return;
        editing = e;
        // In layout mode the controls are locked: the overlay above takes every click.
        content.setInterceptsMouseClicks (! e, ! e);
        tabs.setInterceptsMouseClicks (! e, false);
        updatePicture();
        repaint();
    }

    // While something is being dragged over the layout, panels are kept as pictures too, so a ghost passing
    // over one costs a copy instead of a redraw of its knobs and 3D views.
    void setDragging (bool d)
    {
        if (d == draggingOver) return;
        draggingOver = d;
        updatePicture();
    }

    void resized() override
    {
        const auto r = getLocalBounds();
        const bool sizeChanged = r.getWidth() != laidOut.x || r.getHeight() != laidOut.y;
        // A burst means the size is still changing (a gap being dragged, a spring settling). A single change
        // is laid out straight away: taking a picture for it would cost more than the layout it saves.
        const auto now = juce::Time::getMillisecondCounter();
        const bool burst = sizeChanged && now - lastResize < 60u;
        if (sizeChanged) lastResize = now;
        // Being resized (a gap dragged, a spring settling): show a stretched picture of the contents and lay
        // them out for real once the size stops changing. Laying out and redrawing every knob and 3D view at
        // every intermediate size is what made dragging gaps lag.
        if (sizeChanged && burst && ! collapsed && laidOut.x > 0 && isVisible() && (frozen.isValid() || (content.isVisible() && content.getWidth() > 0)))
        {
            if (frozen.isNull())
                frozen = content.createComponentSnapshot (content.getLocalBounds(), true, 2.0f * scaleNow);
            content.setVisible (false);
            tabs.setVisible (false);
            scheduleThaw();
            repaint();
            return;
        }
        layoutContent();
    }

    // A picture is only ever meant to be shown for a moment. If anything goes wrong with the timer that
    // puts the real contents back, this catches it on the next UI tick.
    void thawIfStale()
    {
        if (frozen.isValid() && juce::Time::getMillisecondCounter() - lastResize > 300u) thaw();
    }

    // Lays the contents out at the current size now (tests call this through the editor to skip the wait).
    void thaw()
    {
        if (frozen.isNull() && laidOut == juce::Point<int> (getWidth(), getHeight())) return;
        frozen = {};
        layoutContent();
    }

    void layoutContent()
    {
        const auto r = getLocalBounds();
        laidOut = { r.getWidth(), r.getHeight() };
        frozen = {};
        if (collapsed)
        {
            content.setVisible (false);
            tabs.setVisible (false);
            return;
        }
        content.setVisible (true);
        const int strip = tabStripHeight();
        const float fit = fitHeight ? (float) (r.getHeight() - strip) / (float) designH
                                    : juce::jmin ((float) r.getWidth() / (float) designW, (float) (r.getHeight() - strip) / (float) designH);
        scaleNow = juce::jlimit (fitHeight ? 0.6f : 0.3f, fitHeight ? 1.0f : maxScale, fit); // strips grow in size, not zoom
        // Floor, not ceil: scaled back up the content must never reach past the panel's edge.
        const int w = (int) std::floor ((float) r.getWidth() / scaleNow);
        const int h = (int) std::floor ((float) (r.getHeight() - strip) / scaleNow);
        content.setBounds (0, 0, w, h);
        content.setTransform (juce::AffineTransform::scale (scaleNow).translated (0.0f, (float) strip));
        if (fitHeight) { if (onLayout) onLayout (w, h); }
        else
        {
            spread.apply (w, h);
            if (onLayout) onLayout (spread.width(), spread.height());
        }
        tabs.setVisible (stacked());
        if (stacked())
        {
            if (tabsInHeader())
            {
                const int tw = juce::jmin (juce::roundToInt ((float) headerFreeWidth * scaleNow), StackTabs::preferredWidth (stackTitles.size()));
                tabs.setBounds (juce::roundToInt ((float) (titleX - 2) * scaleNow), juce::roundToInt (9.0f * scaleNow), tw, juce::roundToInt (26.0f * scaleNow));
            }
            else tabs.setBounds (10, 6, juce::jmin (r.getWidth() - 20, StackTabs::preferredWidth (stackTitles.size())), 24);
            tabs.toFront (false);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        // The glass panel is the costly part of a repaint, so it's drawn once per size and theme and reused.
        const int themeVersion = ThemeState::get().version;
        const bool stale = ! panelCache.isValid() || panelCache.getWidth() != getWidth() * 2 || panelCache.getHeight() != getHeight() * 2 || cachedTheme != themeVersion;
        // While it's springing to a new size, stretch the last face rather than redrawing it every frame;
        // it's redrawn crisp a moment after it settles.
        const bool resizing = juce::Time::getMillisecondCounter() - lastResize < 140u;
        if (stale && resizing && panelCache.isValid() && cachedTheme == themeVersion)
        {
            if (! crispPending)
            {
                crispPending = true;
                juce::Timer::callAfterDelay (180, [safe = juce::Component::SafePointer<Widget> (this)]
                {
                    if (safe != nullptr) { safe->crispPending = false; safe->repaint(); }
                });
            }
        }
        else if (stale)
        {
            panelCache = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth() * 2), juce::jmax (1, getHeight() * 2), true);
            juce::Graphics cg (panelCache);
            cg.addTransform (juce::AffineTransform::scale (2.0f));
            panel (cg, panelRect(), panelRadius());
            panelGrain (cg, panelRect(), panelRadius());
            cachedTheme = themeVersion;
        }
        g.drawImage (panelCache, r);
        if (frozen.isValid() && ! collapsed)
        {
            g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
            g.drawImage (frozen, r.withTrimmedTop ((float) tabStripHeight()));
        }
        if (collapsed) paintFolded (g, r);
        else if (stacked() && ! tabsInHeader())
        {
            g.setColour (Colours::line);
            g.drawHorizontalLine (tabStripHeight() - 1, 12.0f, r.getRight() - 12.0f);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (lifted)
        {
            // Being dragged: a quiet placeholder stays where it came from.
            g.setColour (Colours::bg0.withAlpha (0.6f));
            g.fillRoundedRectangle (panelRect(), panelRadius());
        }
        if (dimmed && ! editing && ! collapsed)
        {
            const float top = (float) tabStripHeight() + 40.0f * scaleNow;
            juce::Path body;
            const auto b = panelRect().withTop (top);
            body.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(), panelRadius(), panelRadius(), false, false, true, true);
            g.setColour (Colours::panel.withAlpha (0.55f));
            g.fillPath (body);
        }
        if (! editing) return;
        auto r = panelRect();
        const float rad = panelRadius();
        // Layout mode: the controls rest under a light veil and the title row becomes a calm bar with a handle
        // in the middle and round buttons at the ends (remove, fold, more). The whole panel can be picked up.
        g.setColour (Colours::bg0.withAlpha (collapsed ? 0.1f : 0.25f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::accent.withAlpha (0.65f));
        g.drawRoundedRectangle (r, rad, 1.5f);
        if (collapsed) return;
        {
            // The bar is as tall as the panel's own title row, so it covers the title and nothing else.
            const float h = (float) barHeight();
            juce::Path bar;
            bar.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), h, rad, rad, true, true, false, false);
            g.setColour (Colours::bg0.withAlpha (1.0f));
            g.fillPath (bar);
            g.setColour (Colours::panel.withAlpha (0.5f));
            g.fillPath (bar);
            g.setColour (Colours::line);
            g.drawHorizontalLine ((int) (r.getY() + h), r.getX() + 1.0f, r.getRight() - 1.0f);
        }
        if (stacked())
        {
            for (int i = 0; i < stackTitles.size(); ++i)
                drawPill (g, tabChip (i), stackTitles[i], i == stackIndex);
        }
        else drawPill (g, handle(), title, true);
        drawRound (g, button (Close), "minus");
        drawRound (g, button (Collapse), sideways ? "fold-side" : "fold");
        drawRound (g, button (Menu), "more");
    }

    // Header chrome, in widget coordinates, for the overlay's hit-testing.
    enum Button { Close, Collapse, Menu, NumButtons };
    // The panel's face inside the widget: a little room at the bottom for its shadow, so it's never cut off.
    juce::Rectangle<float> panelRect() const { return getLocalBounds().toFloat().reduced (1.0f, 0.0f).withTrimmedBottom (3.0f); }
    float panelRadius() const { return 14.0f * juce::jlimit (0.72f, 1.0f, scaleNow); }

    // Layout-mode bar: the height of the title row as it's drawn now (scaled), never less than fits the buttons.
    int barHeight() const
    {
        const int header = juce::roundToInt (40.0f * scaleNow) + 4;
        return juce::jlimit (28, 40, juce::jmax (header, tabStripHeight()));
    }
    int chromeSize() const { return juce::jlimit (18, 24, barHeight() - 12); }
    juce::Rectangle<int> button (int b) const
    {
        const int d = chromeSize(), y = (barHeight() - d) / 2;
        if (b == Close) return { 8, y, d, d };
        return { getWidth() - 8 - d - (b == Menu ? 0 : d + 6), y, d, d };
    }
    // Room between the remove button and the fold/more buttons, where the title or tabs go.
    juce::Range<int> titleRoom() const
    {
        return { button (Close).getRight() + 8, button (Collapse).getX() - 8 };
    }
    juce::Rectangle<int> handle() const
    {
        const auto room = titleRoom();
        const int w = juce::jmax (0, juce::jmin (room.getLength(), juce::roundToInt (mono (11.0f).boldened().getStringWidthFloat (title)) + 44));
        // Centred on the panel when it fits, otherwise slid over into the free room.
        const int x = juce::jlimit (room.getStart(), juce::jmax (room.getStart(), room.getEnd() - w), (getWidth() - w) / 2);
        return { x, button (Close).getY(), w, chromeSize() };
    }
    juce::Rectangle<int> tabChip (int i) const
    {
        const auto room = titleRoom();
        const int n = juce::jmax (1, stackTitles.size());
        const int w = juce::jmax (24, juce::jmin (130, (room.getLength() - 6 * (n - 1)) / n));
        const int total = w * n + 6 * (n - 1);
        const int x0 = juce::jlimit (room.getStart(), juce::jmax (room.getStart(), room.getEnd() - total), (getWidth() - total) / 2);
        return { x0 + i * (w + 6), button (Close).getY(), w, chromeSize() };
    }
    // For the layout checks: does the title (or a tab) in layout mode run into the round buttons?
    bool chromeCollides() const
    {
        juce::Array<juce::Rectangle<int>> labels;
        if (stacked()) for (int i = 0; i < stackTitles.size(); ++i) labels.add (tabChip (i));
        else labels.add (handle());
        for (auto& l : labels)
            for (int b = 0; b < NumButtons; ++b)
                if (l.intersects (button (b)) || l.getWidth() < 24) return true;
        return false;
    }
    int tabChipAt (juce::Point<int> p) const
    {
        if (! stacked()) return -1;
        for (int i = 0; i < stackTitles.size(); ++i) if (tabChip (i).contains (p)) return i;
        return -1;
    }

    // Where the widget can be picked up in normal use: its title (or tabs), or all of it when folded.
    juce::Rectangle<int> grabZone() const
    {
        if (collapsed) return getLocalBounds();
        if (stacked()) return tabs.getBounds();
        const float s = scaleNow;
        const int tw = juce::roundToInt (mono (11.0f).boldened().withExtraKerningFactor (0.06f).getStringWidthFloat (title)) + 16;
        return { juce::roundToInt ((float) (titleX - 6) * s), juce::roundToInt (6.0f * s), juce::roundToInt ((float) tw * s), juce::roundToInt (28.0f * s) };
    }

    // Collapsed: clicking the strip opens it again.
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (collapsed && ! editing && ! e.mods.isPopupMenu() && onExpand) onExpand (*this);
        else if (collapsed && e.mods.isPopupMenu() && onMenu) onMenu (*this, e.getScreenPosition());
    }

private:
    static constexpr float minScale = 0.55f, maxScale = 1.25f;
    float scaleNow = 1.0f;
    juce::Image panelCache;
    int cachedTheme = -1;
    juce::uint32 lastResize = 0;
    bool crispPending = false, thawPending = false;
    bool draggingOver = false;
    void updatePicture() { content.setBufferedToImage (editing || draggingOver); }
    juce::String stackSig;
    juce::Image frozen;            // the contents as they looked when a resize began
    juce::Point<int> laidOut;      // the size the contents were last laid out for

    void scheduleThaw()
    {
        if (thawPending) return;
        thawPending = true;
        juce::Timer::callAfterDelay (150, [safe = juce::Component::SafePointer<Widget> (this)]
        {
            if (safe == nullptr) return;
            safe->thawPending = false;
            if (juce::Time::getMillisecondCounter() - safe->lastResize < 120u) safe->scheduleThaw();
            else safe->thaw();
        });
    }
    juce::StringArray stackTitles;
    int stackIndex = 0;

    bool tabsInHeader() const { return stacked() && headerFreeWidth >= 90 * stackTitles.size(); }
    int tabStripHeight() const { return stacked() && ! tabsInHeader() ? 36 : 0; }

    void paintFolded (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre ({ sideways ? r.getCentreX() : 18.0f, sideways ? 18.0f : r.getCentreY() }));
        g.setColour (Colours::text);
        g.setFont (mono (11.0f).boldened().withExtraKerningFactor (0.08f));
        const auto label = stacked() ? stackTitles.joinIntoString ("  /  ") : title;
        if (sideways)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi, r.getCentreX(), r.getCentreY()));
            const auto rotated = juce::Rectangle<float> (r.getHeight(), r.getWidth()).withCentre (r.getCentre());
            g.drawText (label, rotated.reduced (34, 0), juce::Justification::centredLeft, true);
        }
        else
        {
            g.drawText (label, r.reduced (32, 0), juce::Justification::centredLeft, true);
            g.setColour (Colours::textDim);
            g.setFont (font (10.0f));
            g.drawText ("click to open", r.reduced (14, 0), juce::Justification::centredRight, false);
        }
    }

    void drawPill (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool strong) const
    {
        auto f = r.toFloat();
        g.setColour (Colours::bg0.withAlpha (0.92f));
        g.fillRoundedRectangle (f, f.getHeight() * 0.5f);
        g.setColour (strong ? colour.withAlpha (0.9f) : Colours::lineHi.get());
        g.drawRoundedRectangle (f.reduced (0.5f), f.getHeight() * 0.5f, 1.0f);
        // Grip: two rows of dots say "you can pick this up".
        const bool roomy = r.getWidth() > 60;
        if (roomy)
        {
            g.setColour (Colours::textDim);
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 2; ++j)
                    g.fillEllipse (f.getX() + 11.0f + (float) i * 4.0f, f.getCentreY() - 3.5f + (float) j * 5.0f, 2.0f, 2.0f);
        }
        g.setColour (strong ? Colours::text.get() : Colours::text.withAlpha (0.7f));
        g.setFont (mono (f.getHeight() < 22.0f ? 9.5f : 10.5f).boldened());
        g.drawFittedText (text, roomy ? r.withTrimmedLeft (28).withTrimmedRight (10) : r.reduced (6, 0), roomy ? juce::Justification::centredLeft : juce::Justification::centred, 1, 0.7f);
    }

    void drawRound (juce::Graphics& g, juce::Rectangle<int> r, const char* glyph) const
    {
        auto f = r.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (f.translated (0, 1.5f));
        g.setColour (juce::String (glyph) == "minus" ? juce::Colour (0xffff5a52) : Colours::panelHi.brighter (0.1f));
        g.fillEllipse (f);
        g.setColour (juce::String (glyph) == "minus" ? juce::Colours::white : Colours::text.get());
        const auto c = f.getCentre();
        const juce::String kind (glyph);
        if (kind == "minus") g.fillRoundedRectangle (juce::Rectangle<float> (10, 2.4f).withCentre (c), 1.2f);
        else if (kind == "more")
            for (int i = -1; i <= 1; ++i) g.fillEllipse (juce::Rectangle<float> (3, 3).withCentre (c.translated ((float) i * 5.0f, 0)));
        else
        {
            // Fold: a chevron pointing where the panel folds to.
            juce::Path p;
            if (kind == "fold") { p.startNewSubPath (c.x - 4, c.y - 2); p.lineTo (c.x, c.y + 2); p.lineTo (c.x + 4, c.y - 2); }
            else                { p.startNewSubPath (c.x + 2, c.y - 4); p.lineTo (c.x - 2, c.y); p.lineTo (c.x + 2, c.y + 4); }
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
};

//==============================================================================
// The overlay above the work area. It never covers a control: in normal use it only takes each widget's
// title (grab it to move the widget, like a window), the tabs of a stack, folded widgets, and the gaps
// between widgets (drag them to share space). In layout mode it also takes the widgets' bodies, so the
// whole panel can be picked up, and it adds the remove, fold and menu buttons.
class DockOverlay : public juce::Component, public juce::TooltipClient, private juce::Timer
{
public:
    struct Host
    {
        virtual ~Host() = default;
        virtual dock::Tree& dockTree() = 0;
        virtual juce::Rectangle<int> dockArea() const = 0;
        virtual Widget* widgetById (const juce::String&) const = 0;
        virtual dock::MinSize dockMinSize() const = 0;
        virtual void dockRelayout (bool animate) = 0;
        virtual void dockCommit (const juce::String& what) = 0;       // after an edit: history and autosave
        virtual void dockDrop (const juce::String& widgetOrType, bool isNewType, const dock::Drop&, juce::Rectangle<int> landingFrom) = 0;
        virtual void dockButton (Widget&, int button, juce::Point<int> screenPos) = 0;
        virtual void dockActivate (const juce::String& widgetId) = 0;
        virtual void dockDragging (bool active) = 0;   // something is being dragged over the layout
        virtual void dockMaximise (const juce::String& widgetId) = 0;
    };

    explicit DockOverlay (Host& h) : host (h) { setWantsKeyboardFocus (false); }

    void setEditing (bool e) { editing = e; cancelDrag(); repaint(); lastExtent = {}; }
    bool isEditing() const { return editing; }

    bool hitTest (int x, int y) override
    {
        if (editing || drag.active) return true;
        const juce::Point<int> p (x, y);
        return dividerAt (p) >= 0 || grabbedAt (p) != nullptr;
    }

    juce::String getTooltip() override
    {
        const auto p = getMouseXYRelative();
        if (dividerAt (p) >= 0) return "Drag to resize the panels on either side";
        if (auto* w = grabbedAt (p))
            return w->collapsed ? "Click to open. Drag to move."
                                : "Drag to move it anywhere. Double-click to fill the window. Right-click for more.";
        return {};
    }

    void paint (juce::Graphics& g) override
    {
        if (hoverDivider >= 0 || activeDivider >= 0)
        {
            const auto d = dividers[(size_t) (activeDivider >= 0 ? activeDivider : hoverDivider)].area.toFloat();
            const bool vertical = d.getWidth() < d.getHeight();
            auto bar = vertical ? d.withSizeKeepingCentre (4.0f, juce::jmin (64.0f, d.getHeight() - 16.0f))
                                : d.withSizeKeepingCentre (juce::jmin (64.0f, d.getWidth() - 16.0f), 4.0f);
            g.setColour (Colours::text.withAlpha (activeDivider >= 0 ? 0.9f : 0.5f));
            g.fillRoundedRectangle (bar, 2.0f);
        }
        if (! drag.active) return;
        if (! shownPreview.isEmpty() && drag.drop.zone != dock::Zone::None)
        {
            // Where it will land: a soft translucent slot that glides between targets.
            auto p = shownPreview.reduced (2.0f);
            g.setColour (Colours::accent.withAlpha (0.14f));
            g.fillRoundedRectangle (p, 14.0f);
            g.setColour (Colours::accent.withAlpha (0.75f));
            g.drawRoundedRectangle (p, 14.0f, 1.5f);
            g.setColour (Colours::text.withAlpha (0.85f));
            g.setFont (font (12.0f, true));
            const char* what = drag.drop.zone == dock::Zone::Stack ? "Add as a tab" : "Place here";
            g.drawText (what, p.toNearestInt(), juce::Justification::centred, false);
        }
        if (drag.ghost.isValid())
        {
            // The ghost and its shadow are one picture, made when the drag starts: every frame is a single
            // copy rather than five big rounded rectangles and an image.
            g.setOpacity (0.94f);
            g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
            g.drawImage (drag.lifted, drawnGhost().expanded (shadowPad, shadowPad).translated (0.0f, shadowPad * 0.5f));
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        refreshDividers();
        const int d = dividerAt (e.getPosition());
        if (d != hoverDivider) { hoverDivider = d; refreshOverlay(); }
        if (d >= 0)
        {
            const auto a = dividers[(size_t) d].area;
            setMouseCursor (a.getWidth() < a.getHeight() ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
            return;
        }
        auto* w = editing ? widgetAt (e.getPosition()) : grabbedAt (e.getPosition());
        if (w == nullptr) { setMouseCursor (juce::MouseCursor::NormalCursor); return; }
        const auto local = e.getPosition() + getPosition() - w->getPosition();
        bool onButton = false;
        if (editing) for (int b = 0; b < Widget::NumButtons; ++b) onButton |= ! w->collapsed && w->button (b).contains (local);
        setMouseCursor (onButton ? juce::MouseCursor::PointingHandCursor
                                 : w->collapsed ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { if (hoverDivider >= 0) { hoverDivider = -1; refreshOverlay(); } }

    void mouseDown (const juce::MouseEvent& e) override
    {
        refreshDividers();
        activeDivider = dividerAt (e.getPosition());
        downPos = e.getPosition();
        if (activeDivider >= 0)
        {
            auto& d = dividers[(size_t) activeDivider];
            dividerSplit = d.split;
            dividerStart = dock::Tree::childSizes (*d.split);
            dividerIndex = d.index;
            refreshOverlay();
            return;
        }
        pressed = editing ? widgetAt (e.getPosition()) : grabbedAt (e.getPosition());
        pressedTab = -1;
        pressedButton = -1;
        if (pressed == nullptr) return;
        const auto local = e.getPosition() + getPosition() - pressed->getPosition();
        if (e.mods.isPopupMenu()) { host.dockButton (*pressed, Widget::Menu, e.getScreenPosition()); pressed = nullptr; return; }
        if (editing && ! pressed->collapsed)
            for (int b = 0; b < Widget::NumButtons; ++b)
                if (pressed->button (b).contains (local)) pressedButton = b;
        if (pressed->stacked() && ! pressed->collapsed)
        {
            if (editing) pressedTab = pressed->tabChipAt (local);
            else if (pressed->tabs.getBounds().contains (local)) pressedTab = pressed->tabs.indexAt (local.x - pressed->tabs.getX());
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (activeDivider >= 0)
        {
            if (dividerSplit != nullptr)
            {
                const int delta = dividerSplit->horizontal ? e.getPosition().x - downPos.x : e.getPosition().y - downPos.y;
                host.dockTree().dragDivider (*dividerSplit, dividerIndex, dividerStart, delta, host.dockMinSize());
                host.dockRelayout (false);
                refreshDividers();
                refreshOverlay();
            }
            return;
        }
        if (pressed == nullptr || pressedButton >= 0) return;
        if (! drag.active && e.getPosition().getDistanceFrom (downPos) < 5) return;
        if (! drag.active)
        {
            juce::String id = pressed->id;
            if (pressedTab >= 0)
                if (auto* leaf = host.dockTree().findLeaf (pressed->id))
                    id = leaf->widgets[pressedTab];
            if (auto* w = host.widgetById (id)) startDrag (*w, downPos, id);
        }
        moveDrag (e.getPosition());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (activeDivider >= 0)
        {
            activeDivider = -1;
            dividerSplit = nullptr;
            if (e.getPosition() != downPos) host.dockCommit ("resize");
            refreshOverlay();
            return;
        }
        if (drag.active) { endDrag(); return; }
        if (pressed == nullptr) return;
        auto* w = pressed;
        pressed = nullptr;
        if (pressedButton >= 0) { host.dockButton (*w, pressedButton, e.getScreenPosition()); return; }
        if (pressedTab >= 0)
            if (auto* leaf = host.dockTree().findLeaf (w->id)) { host.dockActivate (leaf->widgets[pressedTab]); return; }
        if (w->collapsed) host.dockButton (*w, Widget::Collapse, e.getScreenPosition());
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* w = editing ? widgetAt (e.getPosition()) : grabbedAt (e.getPosition()))
            if (! w->collapsed) host.dockMaximise (w->id);
    }

    //==========================================================================
    // Dragging a brand-new widget in from the library. Positions are in this overlay's coordinates.
    void startNewDrag (const juce::String& type, juce::Image ghost, juce::Point<int> pos)
    {
        drag = {};
        drag.active = true;
        drag.isNew = true;
        drag.id = type;
        drag.ghost = ghost;
        drag.ghostSize = { ghost.getWidth() / 2, ghost.getHeight() / 2 };
        drag.grab = { drag.ghostSize.x / 2, 18 };
        makeLiftedGhost();
        host.dockDragging (true);
        drag.lift.value = 0.9f; // from the library card: it grows a little as it comes off
        drag.lift.target = 1.0f;
        tracker.reset();
        lastFrame = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
        moveDrag (pos);
    }

    void moveDrag (juce::Point<int> pos)
    {
        if (! drag.active) return;
        drag.pos = pos;
        tracker.add (pos.toFloat());
        auto drop = host.dockTree().dropAt (pos + getPosition(), host.dockArea(), drag.isNew ? juce::String() : drag.id);
        drop.preview = drop.preview - getPosition();
        if (shownPreview.isEmpty() && drop.zone != dock::Zone::None)
        {
            shownPreview = drop.preview.toFloat().withSizeKeepingCentre (40, 40);
            previewX.snap (shownPreview.getX()); previewY.snap (shownPreview.getY());
            previewW.snap (shownPreview.getWidth()); previewH.snap (shownPreview.getHeight());
        }
        drag.drop = drop;
        refreshOverlay();
    }

    void endDrag()
    {
        if (! drag.active) return;
        auto d = drag;
        const auto from = drawnGhost().toNearestInt() + getPosition();
        released = tracker.velocity();
        cancelDrag();
        if (d.drop.zone != dock::Zone::None)
        {
            d.drop.preview = d.drop.preview + getPosition();
            host.dockDrop (d.id, d.isNew, d.drop, from);
        }
    }

    bool isDragging() const { return drag.active; }
    // How fast the pointer was going when a dragged widget was let go (px/s), so it lands with that speed.
    juce::Point<float> releaseVelocity() const { return released; }

    // Everything the overlay repainted since the last call (for the edit benchmark).
    juce::Rectangle<int> takeDirty() { auto d = dirtySinceQuery; dirtySinceQuery = {}; return d; }

private:
    Host& host;
    juce::Rectangle<int> lastExtent, dirtySinceQuery;

    // Only the parts the overlay draws are repainted (the ghost, the landing slot, a divider's bar), old
    // place and new. Repainting the whole overlay redrew every panel underneath on every frame of a drag.
    juce::Rectangle<int> drawnExtent() const
    {
        juce::Rectangle<float> r;
        auto add = [&] (juce::Rectangle<float> x) { r = r.getUnion (x); };
        const int d = activeDivider >= 0 ? activeDivider : hoverDivider;
        if (juce::isPositiveAndBelow (d, (int) dividers.size())) add (dividers[(size_t) d].area.toFloat().expanded (2.0f));
        if (drag.active)
        {
            if (! shownPreview.isEmpty()) add (shownPreview.expanded (3.0f));
            if (drag.ghost.isValid()) { const auto g = drawnGhost(); add (g.expanded (shadowPad + 2.0f).translated (0.0f, shadowPad * 0.5f)); }
        }
        return r.getSmallestIntegerContainer();
    }
    void refreshOverlay()
    {
        const auto now = drawnExtent();
        const auto dirty = lastExtent.getUnion (now);
        if (! dirty.isEmpty())
        {
            repaint (dirty);
            dirtySinceQuery = dirtySinceQuery.getUnion (dirty);
        }
        lastExtent = now;
    }
    bool editing = false;
    std::vector<dock::Divider> dividers;
    int hoverDivider = -1, activeDivider = -1, dividerIndex = 0;
    dock::Node* dividerSplit = nullptr; // the tree isn't restructured while a gap is dragged
    std::vector<int> dividerStart;
    juce::Point<int> downPos;
    Widget* pressed = nullptr;
    int pressedTab = -1, pressedButton = -1;
    juce::Rectangle<float> shownPreview;
    motion::Spring previewX, previewY, previewW, previewH;
    motion::VelocityTracker tracker;
    juce::Point<float> released;
    double lastFrame = 0;

    struct Drag
    {
        bool active = false, isNew = false;
        juce::String id;
        juce::Image ghost;
        juce::Point<int> ghostSize, grab, pos;
        dock::Drop drop;
        motion::Spring lift { 1.0f, 0.0f, 1.0f, 0.001f }; // ghost size relative to its final size
        juce::Image lifted;                               // the ghost with its shadow, drawn once
    } drag;
    static constexpr float shadowPad = 22.0f;

    // Paints the ghost onto its shadow once, so dragging only copies a picture.
    void makeLiftedGhost()
    {
        if (! drag.ghost.isValid()) { drag.lifted = {}; return; }
        const auto w = (float) drag.ghostSize.x, h = (float) drag.ghostSize.y;
        drag.lifted = juce::Image (juce::Image::ARGB, juce::roundToInt ((w + shadowPad * 2.0f) * 2.0f), juce::roundToInt ((h + shadowPad * 2.0f) * 2.0f), true);
        juce::Graphics g (drag.lifted);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        const auto r = juce::Rectangle<float> (w, h).withPosition (shadowPad, shadowPad);
        for (int i = 5; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.045f * (float) (6 - i)));
            g.fillRoundedRectangle (r.translated (0, 4.0f * (float) i).expanded (2.5f * (float) i), 16.0f);
        }
        g.drawImage (drag.ghost, r);
    }

    juce::Rectangle<int> ghostRect() const
    {
        return juce::Rectangle<int> (drag.ghostSize.x, drag.ghostSize.y).withPosition (drag.pos - drag.grab);
    }
    // Picked up, it shrinks from full size into a ghost around the point you hold, rather than jumping.
    juce::Rectangle<float> drawnGhost() const
    {
        const float k = drag.lift.value;
        return juce::Rectangle<float> ((float) drag.ghostSize.x * k, (float) drag.ghostSize.y * k)
                   .withPosition (drag.pos.toFloat() - drag.grab.toFloat() * k);
    }

    void timerCallback() override
    {
        // The landing slot springs towards its target, redirecting smoothly as you move between targets.
        if (! drag.active) { stopTimer(); return; }
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float dt = (float) juce::jlimit (0.001, 0.05, (now - lastFrame) / 1000.0);
        lastFrame = now;
        const bool still = motion::systemReducesMotion();
        if (drag.drop.zone != dock::Zone::None)
        {
            const auto target = drag.drop.preview.toFloat();
            if (shownPreview.isEmpty() || still)
            {
                if (still) shownPreview = target;
                previewX.snap (shownPreview.getX()); previewY.snap (shownPreview.getY());
                previewW.snap (shownPreview.getWidth()); previewH.snap (shownPreview.getHeight());
            }
            previewX.target = target.getX(); previewY.target = target.getY();
            previewW.target = target.getWidth(); previewH.target = target.getHeight();
            for (auto* sp : { &previewX, &previewY, &previewW, &previewH }) sp->step (dt, 0.26f, 1.0f);
            shownPreview = { previewX.value, previewY.value, previewW.value, previewH.value };
        }
        if (still) drag.lift.snap (1.0f); else drag.lift.step (dt, 0.24f, 1.0f);
        refreshOverlay();
    }

    void startDrag (Widget& w, juce::Point<int> pos, const juce::String& id)
    {
        drag = {};
        drag.active = true;
        drag.id = id;
        // The ghost is a snapshot of the widget, shrunk if it's big so the drop targets stay visible.
        const float k = juce::jmin (1.0f, 340.0f / (float) juce::jmax (1, w.getWidth()), 260.0f / (float) juce::jmax (1, w.getHeight()));
        auto* shown = host.widgetById (id);
        auto& snapOf = shown != nullptr && shown->isVisible() ? *shown : w;
        drag.ghost = snapOf.createComponentSnapshot (snapOf.getLocalBounds(), true, 2.0f * k);
        drag.ghostSize = { juce::roundToInt ((float) snapOf.getWidth() * k), juce::roundToInt ((float) snapOf.getHeight() * k) };
        const auto grabInWidget = pos + getPosition() - w.getPosition();
        drag.grab = { juce::jlimit (8, drag.ghostSize.x - 8, juce::roundToInt ((float) grabInWidget.x * k)),
                      juce::jlimit (8, drag.ghostSize.y - 8, juce::roundToInt ((float) grabInWidget.y * k)) };
        drag.pos = pos;
        host.dockDragging (true);
        makeLiftedGhost();
        drag.lift.value = 1.0f / juce::jmax (0.05f, k);
        drag.lift.target = 1.0f;
        tracker.reset();
        tracker.add (pos.toFloat());
        lastFrame = juce::Time::getMillisecondCounterHiRes();
        shownPreview = {};
        w.lifted = true;
        w.repaint();
        startTimerHz (60);
    }

    void cancelDrag()
    {
        if (drag.active)
            if (auto* w = host.widgetById (drag.id)) { w->lifted = false; w->repaint(); }
        // Tabs of a stack: the front widget may be the one that showed the lifted state.
        host.dockTree().forEachLeaf ([&] (dock::Node& n)
        {
            if (auto* w = host.widgetById (n.activeId()); w != nullptr && w->lifted) { w->lifted = false; w->repaint(); }
        });
        drag = {};
        shownPreview = {};
        stopTimer();
        host.dockDragging (false);
        refreshOverlay();
    }

    void refreshDividers()
    {
        dividers = host.dockTree().dividers();
        for (auto& d : dividers) d.area = d.area - getPosition();
    }

    int dividerAt (juce::Point<int> p)
    {
        if (dividers.empty()) refreshDividers();
        for (size_t i = 0; i < dividers.size(); ++i)
        {
            const bool vertical = dividers[i].area.getWidth() < dividers[i].area.getHeight();
            if (dividers[i].area.expanded (vertical ? 2 : 0, vertical ? 0 : 2).contains (p)) return (int) i;
        }
        return -1;
    }

    Widget* widgetAt (juce::Point<int> p) const
    {
        Widget* found = nullptr;
        host.dockTree().forEachLeaf ([&] (dock::Node& n)
        {
            if ((n.bounds - getPosition()).contains (p))
                if (auto* w = host.widgetById (n.activeId()); w != nullptr && w->isVisible()) found = w;
        });
        return found;
    }

    // Normal mode: only a widget's grab zone (its title, its tabs, or all of it when folded).
    Widget* grabbedAt (juce::Point<int> p) const
    {
        auto* w = widgetAt (p);
        if (w == nullptr) return nullptr;
        return w->grabZone().contains (p + getPosition() - w->getPosition()) ? w : nullptr;
    }
};

//==============================================================================
// Named workspaces, saved per user next to the look settings. A workspace is only an arrangement of widgets:
// loading one never touches the sound.
struct WorkspaceStore
{
    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "workspaces";
        o.filenameSuffix = ".xml";
        o.folderName = "Arrow/Hypernova";
        // Tests point this somewhere scratch so they never touch the user's own workspaces.
        const auto testFolder = juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_WORKSPACE_FOLDER", {});
        if (testFolder.isNotEmpty()) o.folderName = testFolder;
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return o;
    }

    static juce::StringArray builtIn() { return { "Sound Design", "Sampling", "Effects", "Analysis" }; }

    static juce::String current()
    {
        juce::PropertiesFile p (options());
        return p.getValue ("current", "Sound Design");
    }

    static void setCurrent (const juce::String& name)
    {
        juce::PropertiesFile p (options());
        p.setValue ("current", name);
        p.saveIfNeeded();
    }

    static juce::StringArray names()
    {
        auto all = builtIn();
        juce::PropertiesFile p (options());
        for (auto& key : p.getAllProperties().getAllKeys())
            if (key.startsWith (prefix)) all.addIfNotAlreadyThere (key.fromFirstOccurrenceOf (prefix, false, false));
        return all;
    }

    static juce::ValueTree load (const juce::String& name)
    {
        juce::PropertiesFile p (options());
        if (auto xml = juce::parseXML (p.getValue (prefix + name)))
            return juce::ValueTree::fromXml (*xml);
        return {};
    }

    static void save (const juce::String& name, const juce::ValueTree& layout)
    {
        juce::PropertiesFile p (options());
        p.setValue (prefix + name, layout.toXmlString());
        p.saveIfNeeded();
    }

    static void remove (const juce::String& name)
    {
        juce::PropertiesFile p (options());
        p.removeValue (prefix + name);
        p.saveIfNeeded();
    }

private:
    static constexpr const char* prefix = "dock:"; // docking layouts (the earlier free-placement ones used "ws:")
};

} // namespace ab::ui
