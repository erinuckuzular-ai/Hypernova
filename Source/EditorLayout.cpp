// Widgets and workspaces for the editor: arranging panels, snapping, stacking, hiding, and named workspaces.
// Nothing in here touches the sound: layouts are per user and separate from the patch.
#include "PluginEditor.h"
#include <map>

using namespace ab;
using namespace ab::ui;

namespace
{
    // Same base-canvas figures as PluginEditor.cpp: the pre-widget layout, used for Sound Design.
    constexpr int topRowH = 378, midRowH = 212;
    const juce::Rectangle<int> oscAPanel   { 24, 96, 400, topRowH };
    const juce::Rectangle<int> oscBPanel   { 436, 96, 400, topRowH };
    const juce::Rectangle<int> spacePanel  { 848, 96, 408, topRowH };
    const juce::Rectangle<int> subPanel    { 24, 488, 240, midRowH };
    const juce::Rectangle<int> voicePanel  { 276, 488, 256, midRowH };
    const juce::Rectangle<int> filterPanel { 544, 488, 332, midRowH };
    const juce::Rectangle<int> envPanel    { 888, 488, 368, midRowH };
    const juce::Rectangle<int> deckPanel   { 24, 714, 1232, 190 };
    const juce::Rectangle<int> keysArea    { 24, 918, 1232, 56 };
    const juce::Rectangle<int> workArea    { 24, 96, 1232, 810 };
    constexpr int gutter = 12, snapGrid = 4, snapReach = 10;
}

Widget* HypernovaAudioProcessorEditor::findWidget (const juce::String& id) const
{
    for (auto& w : widgets) if (w->id == id) return w.get();
    return nullptr;
}

bool HypernovaAudioProcessorEditor::layoutKeyboardVisible() const { return keyboard.isVisible(); }

juce::Rectangle<int> HypernovaAudioProcessorEditor::layoutArea() const
{
    return workArea.withBottom (layoutKeyboardVisible() ? keysArea.getY() - gutter : baseHeight - gutter);
}

// The built-in arrangements. Sound Design is Hypernova as it has always looked.
juce::ValueTree HypernovaAudioProcessorEditor::defaultLayout (const juce::String& name) const
{
    juce::ValueTree t ("Layout");
    auto add = [&] (const char* id, int x, int y, int w, bool shown = true, const char* stack = "", bool active = true, bool collapsed = false)
    {
        juce::ValueTree n ("W");
        n.setProperty ("id", id, nullptr).setProperty ("x", x, nullptr).setProperty ("y", y, nullptr).setProperty ("w", w, nullptr)
         .setProperty ("shown", shown, nullptr).setProperty ("stack", stack, nullptr).setProperty ("active", active, nullptr)
         .setProperty ("collapsed", collapsed, nullptr);
        t.appendChild (n, nullptr);
    };
    static const char* pageIds[] = { "mod", "fx", "morefx", "play" };
    if (name == "Effects")
    {
        // Both effect racks open in full, modulation under them, the sound's core along the bottom.
        add ("fx", 24, 96, 1232);
        add ("morefx", 24, 298, 1232);
        add ("mod", 24, 500, 1232, true, "deck", true);
        add ("play", 24, 500, 1232, true, "deck", false);
        add ("filter", 24, 702, 320);
        add ("env", 356, 702, 356);
        add ("pitch", 724, 702, 248);
        add ("sub", 984, 702, 236);
        add ("oscA", 24, 96, 400, false);
        add ("oscB", 436, 96, 400, false);
        add ("space", 848, 96, 408, false);
        return t;
    }
    add ("oscA", oscAPanel.getX(), oscAPanel.getY(), oscAPanel.getWidth());
    add ("oscB", oscBPanel.getX(), oscBPanel.getY(), oscBPanel.getWidth());
    add ("space", spacePanel.getX(), spacePanel.getY(), spacePanel.getWidth());
    add ("sub", subPanel.getX(), subPanel.getY(), subPanel.getWidth());
    add ("pitch", voicePanel.getX(), voicePanel.getY(), voicePanel.getWidth());
    add ("filter", filterPanel.getX(), filterPanel.getY(), filterPanel.getWidth());
    add ("env", envPanel.getX(), envPanel.getY(), envPanel.getWidth());
    for (int i = 0; i < 4; ++i)
        add (pageIds[i], deckPanel.getX(), deckPanel.getY(), deckPanel.getWidth(), true, "deck", i == juce::jlimit (0, 3, processor.uiDeckPage));
    return t;
}

juce::ValueTree HypernovaAudioProcessorEditor::captureLayout() const
{
    juce::ValueTree t ("Layout");
    for (auto& w : widgets)
    {
        juce::ValueTree n ("W");
        n.setProperty ("id", w->id, nullptr).setProperty ("x", w->getX(), nullptr).setProperty ("y", w->getY(), nullptr)
         .setProperty ("w", w->getWidth(), nullptr).setProperty ("shown", w->shown, nullptr).setProperty ("stack", w->stack, nullptr)
         .setProperty ("active", w->stackActive, nullptr).setProperty ("collapsed", w->collapsed, nullptr);
        t.appendChild (n, nullptr);
    }
    return t;
}

void HypernovaAudioProcessorEditor::applyLayout (const juce::ValueTree& layout)
{
    for (auto& w : widgets)
    {
        const auto n = layout.getChildWithProperty ("id", w->id);
        if (! n.isValid()) { w->shown = false; w->setVisible (false); continue; } // not part of this layout
        w->shown = (bool) n.getProperty ("shown", true);
        w->stack = n.getProperty ("stack", "").toString();
        w->stackActive = (bool) n.getProperty ("active", true);
        w->collapsed = (bool) n.getProperty ("collapsed", false);
        const int width = juce::jlimit (w->minWidth(), w->maxWidth(), (int) n.getProperty ("w", w->designW));
        w->setBounds ((int) n.getProperty ("x", 24), (int) n.getProperty ("y", 96), width, w->heightForWidth (width));
    }
    updateStacks();
}

// One member of each stack is in front; the others share its place and show as its tabs.
void HypernovaAudioProcessorEditor::updateStacks()
{
    std::map<juce::String, std::vector<Widget*>> stacks;
    for (auto& w : widgets)
    {
        if (w->stack.isNotEmpty() && w->shown) stacks[w->stack].push_back (w.get());
        else { w->setVisible (w->shown); w->tabs.setVisible (false); if (! w->shown) w->stack.clear(); }
    }
    for (auto& [name, members] : stacks)
    {
        if (members.size() == 1)
        {
            members[0]->stack.clear();
            members[0]->stackActive = true;
            members[0]->setVisible (true);
            members[0]->tabs.setVisible (false);
            continue;
        }
        Widget* front = nullptr;
        for (auto* m : members) if (m->stackActive && front == nullptr) front = m;
        if (front == nullptr) front = members[0];
        juce::StringArray titles;
        int activeIndex = 0;
        for (size_t i = 0; i < members.size(); ++i)
        {
            titles.add (members[i]->title);
            if (members[i] == front) activeIndex = (int) i;
        }
        for (auto* m : members)
        {
            m->stackActive = m == front;
            m->collapsed = front->collapsed;
            m->setBounds (front->getBounds().withHeight (m->heightForWidth (front->getWidth())));
            m->setVisible (m == front);
            m->tabs.setVisible (true);
            m->tabs.setTabs (titles, activeIndex, Palette::mod);
            m->tabs.onSelect = [this, members] (int index)
            {
                if (juce::isPositiveAndBelow (index, (int) members.size())) activateInStack (*members[(size_t) index], true);
            };
        }
    }
    // Remember which deck page is in front, so sessions reopen on it.
    static const char* ids[] = { "mod", "fx", "morefx", "play" };
    for (int i = 0; i < 4; ++i)
        if (auto* w = findWidget (ids[i]))
            if (w->isVisible() && w->stack == "deck") processor.uiDeckPage = i;
}

void HypernovaAudioProcessorEditor::activateInStack (Widget& w, bool recordHistory)
{
    for (auto& other : widgets) if (other->stack == w.stack) other->stackActive = other.get() == &w;
    updateStacks();
    if (recordHistory) layoutChanged();
}

// Snap to a small grid, then to the edges of neighbours (flush, or one gutter apart), and stay inside the area.
juce::Rectangle<int> HypernovaAudioProcessorEditor::snapped (const Widget& w, juce::Rectangle<int> r) const
{
    auto grid = [] (int v) { return juce::roundToInt ((float) v / (float) snapGrid) * snapGrid; };
    r.setPosition (grid (r.getX()), grid (r.getY()));
    const auto area = layoutArea();
    int bestDx = snapReach + 1, bestDy = snapReach + 1, dx = 0, dy = 0;
    auto tryX = [&] (int from, int to) { if (std::abs (to - from) < bestDx) { bestDx = std::abs (to - from); dx = to - from; } };
    auto tryY = [&] (int from, int to) { if (std::abs (to - from) < bestDy) { bestDy = std::abs (to - from); dy = to - from; } };
    tryX (r.getX(), area.getX());
    tryX (r.getRight(), area.getRight());
    tryY (r.getY(), area.getY());
    tryY (r.getBottom(), area.getBottom());
    for (auto& o : widgets)
    {
        if (o.get() == &w || ! o->isVisible() || (w.stack.isNotEmpty() && o->stack == w.stack)) continue;
        const auto b = o->getBounds();
        tryX (r.getX(), b.getRight() + gutter); tryX (r.getRight(), b.getX() - gutter);
        tryX (r.getX(), b.getX());              tryX (r.getRight(), b.getRight());
        tryY (r.getY(), b.getBottom() + gutter); tryY (r.getBottom(), b.getY() - gutter);
        tryY (r.getY(), b.getY());               tryY (r.getBottom(), b.getBottom());
    }
    if (bestDx <= snapReach) r.translate (dx, 0);
    if (bestDy <= snapReach) r.translate (0, dy);
    r.setX (juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - r.getWidth()), r.getX()));
    r.setY (juce::jlimit (area.getY(), juce::jmax (area.getY(), area.getBottom() - r.getHeight()), r.getY()));
    return r;
}

bool HypernovaAudioProcessorEditor::overlapsOthers (const Widget& w, juce::Rectangle<int> r) const
{
    if (! layoutArea().contains (r)) return true;
    for (auto& o : widgets)
    {
        if (o.get() == &w || ! o->isVisible() || (w.stack.isNotEmpty() && o->stack == w.stack)) continue;
        if (o->getBounds().expanded (gutter / 2 - 1).intersects (r)) return true;
    }
    return false;
}

// The first place a widget fits, trying its current width, its design width, then its smallest width.
juce::Rectangle<int> HypernovaAudioProcessorEditor::freeSpotFor (const Widget& w) const
{
    const auto area = layoutArea();
    for (int width : { w.getWidth() > 0 ? w.getWidth() : w.designW, w.designW, w.minWidth() })
    {
        const juce::Rectangle<int> probe (0, 0, width, w.heightForWidth (width));
        for (int y = area.getY(); y + probe.getHeight() <= area.getBottom(); y += 8)
            for (int x = area.getX(); x + probe.getWidth() <= area.getRight(); x += 8)
                if (! overlapsOthers (w, probe.withPosition (x, y))) return probe.withPosition (x, y);
    }
    return { area.getX(), area.getY(), w.minWidth(), w.heightForWidth (w.minWidth()) };
}

void HypernovaAudioProcessorEditor::placeWidget (Widget& w, juce::Rectangle<int> b)
{
    w.setBounds (b);
    for (auto& o : widgets)
        if (w.stack.isNotEmpty() && o->stack == w.stack && o.get() != &w)
            o->setBounds (b.withHeight (o->heightForWidth (b.getWidth())));
}

void HypernovaAudioProcessorEditor::showWidget (Widget& w)
{
    w.shown = true;
    w.stack.clear();
    w.stackActive = true;
    w.setVisible (true);
    w.setEditing (layoutEditing);
    w.setBounds (freeSpotFor (w));
    w.toFront (false);
    updateStacks();
}

void HypernovaAudioProcessorEditor::addWidget (Widget& w)
{
    if (w.shown && ! w.isVisible() && w.stack.isNotEmpty()) activateInStack (w, false);
    else if (! w.isVisible()) showWidget (w);
    layoutChanged();
}

void HypernovaAudioProcessorEditor::hideWidget (Widget& w)
{
    // Hiding is only about the screen: the module keeps running exactly as it was.
    const auto stack = w.stack;
    w.shown = false;
    w.stack.clear();
    if (stack.isNotEmpty())
        for (auto& o : widgets) if (o->stack == stack && o->shown) { o->stackActive = true; break; }
    updateStacks();
}

void HypernovaAudioProcessorEditor::wireWidget (Widget& w)
{
    w.onEdit = [this] (Widget& wd, Widget::Edit e, juce::Point<int> d)
    {
        using E = Widget::Edit;
        if (e == E::MoveStart || e == E::ResizeStart) { dragStart = wd.getBounds(); wd.toFront (false); return; }
        juce::Rectangle<int> r;
        if (e == E::Move || e == E::MoveEnd)
            r = snapped (wd, dragStart.translated (d.x, d.y));
        else
        {
            int width = juce::jlimit (wd.minWidth(), wd.maxWidth(), dragStart.getWidth() + d.x);
            width = juce::jmin (width, layoutArea().getRight() - dragStart.getX());
            r = dragStart.withWidth (width).withHeight (wd.heightForWidth (width));
        }
        const bool bad = overlapsOthers (wd, r);
        if (e == E::Move || e == E::Resize) { placeWidget (wd, r); wd.setInvalidDrop (bad); return; }

        wd.setInvalidDrop (false);
        if (e == E::MoveEnd)
        {
            // Dropped onto another widget's title row: join it as a tab.
            const auto dropPoint = dragStart.translated (d.x, d.y).getPosition() + juce::Point<int> (40, 14);
            for (auto& o : widgets)
            {
                if (o.get() == &wd || ! o->isVisible()) continue;
                if (o->getBounds().withHeight (juce::roundToInt (40.0f * o->scale())).contains (dropPoint))
                {
                    if (o->stack.isEmpty()) o->stack = "stack-" + o->id;
                    wd.stack = o->stack;
                    wd.collapsed = o->collapsed;
                    for (auto& m : widgets) if (m->stack == wd.stack) m->stackActive = m.get() == &wd;
                    wd.setBounds (o->getBounds().withHeight (wd.heightForWidth (o->getWidth())));
                    updateStacks();
                    layoutChanged();
                    showMessage (wd.title + " stacked with " + o->title);
                    return;
                }
            }
        }
        if (bad) { placeWidget (wd, dragStart); showMessage ("No room there: widgets can't overlap"); return; }
        placeWidget (wd, r);
        layoutChanged();
    };
    w.onCollapse = [this] (Widget& wd)
    {
        wd.collapsed = ! wd.collapsed;
        for (auto& o : widgets) if (wd.stack.isNotEmpty() && o->stack == wd.stack) o->collapsed = wd.collapsed;
        placeWidget (wd, wd.getBounds().withHeight (wd.heightForWidth (wd.getWidth())));
        layoutChanged();
    };
    w.onHide = [this] (Widget& wd)
    {
        hideWidget (wd);
        layoutChanged();
        showMessage (wd.title + " hidden. Add it back from + ADD WIDGET; the sound is unchanged.");
    };
    w.onUnstack = [this] (Widget& wd)
    {
        const auto stack = wd.stack;
        wd.stack.clear();
        for (auto& o : widgets) if (o->stack == stack && o.get() != &wd) { o->stackActive = true; break; }
        wd.stackActive = true;
        updateStacks();
        wd.setBounds (freeSpotFor (wd));
        wd.setEditing (layoutEditing);
        wd.toFront (false);
        layoutChanged();
    };
}

void HypernovaAudioProcessorEditor::layoutChanged()
{
    // Layout undo history, and the current workspace keeps what was arranged.
    if (layoutHistoryIndex + 1 < (int) layoutHistory.size())
        layoutHistory.erase (layoutHistory.begin() + layoutHistoryIndex + 1, layoutHistory.end());
    layoutHistory.push_back (captureLayout());
    if (layoutHistory.size() > 80) layoutHistory.erase (layoutHistory.begin());
    layoutHistoryIndex = (int) layoutHistory.size() - 1;
    ab::ui::WorkspaceStore::save (workspaceName, layoutHistory.back());
}

void HypernovaAudioProcessorEditor::undoLayout()
{
    if (layoutHistoryIndex <= 0) { showMessage ("Nothing to undo in the layout"); return; }
    applyLayout (layoutHistory[(size_t) --layoutHistoryIndex]);
    for (auto& w : widgets) w->setEditing (layoutEditing);
    ab::ui::WorkspaceStore::save (workspaceName, layoutHistory[(size_t) layoutHistoryIndex]);
}

void HypernovaAudioProcessorEditor::redoLayout()
{
    if (layoutHistoryIndex + 1 >= (int) layoutHistory.size()) { showMessage ("Nothing to redo in the layout"); return; }
    applyLayout (layoutHistory[(size_t) ++layoutHistoryIndex]);
    for (auto& w : widgets) w->setEditing (layoutEditing);
    ab::ui::WorkspaceStore::save (workspaceName, layoutHistory[(size_t) layoutHistoryIndex]);
}

void HypernovaAudioProcessorEditor::loadWorkspace (const juce::String& name, bool recordHistory)
{
    // Only the arrangement changes. The sound, its parameters and the audio engine are untouched.
    workspaceName = name.isNotEmpty() ? name : juce::String ("Sound Design");
    auto layout = ab::ui::WorkspaceStore::load (workspaceName);
    if (! layout.isValid()) layout = defaultLayout (workspaceName);
    applyLayout (layout);
    for (auto& w : widgets) w->setEditing (layoutEditing);
    ab::ui::WorkspaceStore::setCurrent (workspaceName);
    editBar.workspace.setButtonText (workspaceName.toUpperCase() + "  v");
    if (! recordHistory) { layoutHistory.clear(); layoutHistoryIndex = -1; }
    layoutChanged();
}

void HypernovaAudioProcessorEditor::setLayoutEditing (bool editing)
{
    layoutEditing = editing;
    if (spaceExpanded) setSpaceExpanded (false);
    for (auto& w : widgets) w->setEditing (editing);
    editBar.setVisible (editing);
    for (auto* c : std::initializer_list<juce::Component*> { &presetPlate, &prevButton, &nextButton, &diceButton, &saveButton, &undoButton, &redoButton, &layoutButton, &gearButton })
        c->setVisible (! editing);
    layoutButton.setToggleState (editing, juce::dontSendNotification);
    if (editing) showMessage ("layout mode: drag a widget anywhere, drop it on a title to stack, resize from the corner");
    canvas.repaint();
}

void HypernovaAudioProcessorEditor::setupEditBar()
{
    canvas.addChildComponent (editBar);
    editBar.add.onClick = [this] { showAddWidgetMenu(); };
    editBar.workspace.onClick = [this] { showWorkspaceMenu(); };
    editBar.undo.onClick = [this] { undoLayout(); };
    editBar.redo.onClick = [this] { redoLayout(); };
    editBar.reset.setTooltip ("Put this workspace back to how it started");
    editBar.reset.onClick = [this]
    {
        applyLayout (defaultLayout (ab::ui::WorkspaceStore::builtIn().contains (workspaceName) ? workspaceName : juce::String ("Sound Design")));
        for (auto& w : widgets) w->setEditing (layoutEditing);
        layoutChanged();
        showMessage ("layout reset");
    };
    editBar.done.onClick = [this] { setLayoutEditing (false); };
    for (auto* b : { &editBar.add, &editBar.workspace, &editBar.undo, &editBar.redo, &editBar.reset, &editBar.done })
        b->setColour (juce::TextButton::buttonOnColourId, Colours::accent);
}

void HypernovaAudioProcessorEditor::showAddWidgetMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lookAndFeel);
    m.addSectionHeader ("ADD A WIDGET");
    int hidden = 0;
    for (int i = 0; i < (int) widgets.size(); ++i)
    {
        auto& w = *widgets[(size_t) i];
        const bool onScreen = w.shown && w.isVisible();
        const bool inStack = w.shown && ! w.isVisible();
        m.addItem (1 + i, w.title + (inStack ? "  (behind a tab)" : ""), ! onScreen, onScreen);
        hidden += onScreen ? 0 : 1;
    }
    if (hidden == 0) m.addItem (-1, "everything is already on screen", false, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&editBar.add), [this] (int r)
    {
        if (r <= 0 || r > (int) widgets.size()) return;
        addWidget (*widgets[(size_t) (r - 1)]);
    });
}

void HypernovaAudioProcessorEditor::showWorkspaceMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lookAndFeel);
    m.addSectionHeader ("WORKSPACES");
    const auto names = ab::ui::WorkspaceStore::names();
    for (int i = 0; i < names.size(); ++i) m.addItem (100 + i, names[i], true, names[i] == workspaceName);
    m.addSeparator();
    const bool builtIn = ab::ui::WorkspaceStore::builtIn().contains (workspaceName);
    m.addItem (1, "Save as new workspace...");
    m.addItem (2, "Rename this workspace...", ! builtIn);
    m.addItem (3, "Duplicate this workspace");
    m.addItem (4, "Delete this workspace", ! builtIn);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&editBar.workspace), [this, names] (int r)
    {
        auto ask = [this] (const juce::String& titleText, const juce::String& initial, std::function<void (juce::String)> then)
        {
            auto* box = new juce::AlertWindow (titleText, "A workspace remembers where every widget sits. It never changes the sound.",
                                               juce::MessageBoxIconType::NoIcon, this);
            box->addTextEditor ("name", initial);
            box->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            box->enterModalState (true, juce::ModalCallbackFunction::create ([box, then] (int result)
            {
                const auto text = box->getTextEditorContents ("name").trim();
                if (result == 1 && text.isNotEmpty()) then (text);
            }), true);
        };
        if (r >= 100 && r - 100 < names.size()) { loadWorkspace (names[r - 100], false); return; }
        if (r == 1)
            ask ("Save workspace", workspaceName + " 2", [this] (juce::String n)
            {
                ab::ui::WorkspaceStore::save (n, captureLayout());
                loadWorkspace (n, false);
            });
        if (r == 2)
            ask ("Rename workspace", workspaceName, [this] (juce::String n)
            {
                const auto layout = captureLayout();
                ab::ui::WorkspaceStore::remove (workspaceName);
                ab::ui::WorkspaceStore::save (n, layout);
                loadWorkspace (n, false);
            });
        if (r == 3)
        {
            const auto copyName = workspaceName + " copy";
            ab::ui::WorkspaceStore::save (copyName, captureLayout());
            loadWorkspace (copyName, false);
        }
        if (r == 4)
        {
            ab::ui::WorkspaceStore::remove (workspaceName);
            loadWorkspace ("Sound Design", false);
        }
    });
}
