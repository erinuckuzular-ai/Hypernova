// Widgets and workspaces for the editor: the widget catalogue, tool widgets, docking, workspaces, menus.
// Nothing in here touches the sound: layouts are per user and separate from the patch.
#include "PluginEditor.h"

using namespace ab;
using namespace ab::ui;

namespace
{
    // Everything the widget library offers, in the order it lists them. Multi widgets can be added
    // as many times as you like; the rest are single panels of the synth.
    struct TypeInfo { const char* type; const char* name; const char* group; const char* description; bool multi; int colourSlot; };
    const TypeInfo catalogue[] = {
        { "oscA",     "Osc A",          "SOUND",      "Wavetable oscillator with its 3D table view",                   false, SlotOscA },
        { "oscB",     "Osc B",          "SOUND",      "The second wavetable oscillator",                               false, SlotOscB },
        { "sub",      "Sub + Noise",    "SOUND",      "Sub oscillator and the noise source",                           false, SlotSub },
        { "pitch",    "Pitch",          "SOUND",      "Drop, glide, bend, voice mode and velocity",                    false, SlotSub },
        { "filter",   "Filter",         "SOUND",      "The filter and its response curve",                             false, SlotFilter },
        { "env",      "Envelopes",      "SOUND",      "Amp and mod envelopes: drag their points",                      false, SlotEnv },
        { "mod",      "Modulation",     "MODULATION", "Two LFOs and the 8-slot mod matrix",                            false, SlotLfo },
        { "macros",   "Macros",         "MODULATION", "The four macros, big: rename them, see what they move",         false, SlotFx },
        { "xy",       "XY Pad",         "MODULATION", "Two macros on one pad: drag to move both",                      true,  SlotMod },
        { "modmon",   "Mod Monitor",    "MODULATION", "Every active modulation source, live, and what it moves",       true,  SlotLfo },
        { "fx",       "Effects",        "EFFECTS",    "Distortion, OTT, chorus, delay, space, EQ and width",           false, SlotFx },
        { "morefx",   "More FX",        "EFFECTS",    "Flanger, tape, gate, filter, pitch and effect styles",          false, SlotFx },
        { "play",     "Play",           "PLAYING",    "Arp, chords, tuning, unison width and cross mod",               false, SlotEnv },
        { "space",    "Sound Space",    "VIEWS",      "Spectrum or orbit view of the output",                          false, SlotAccent },
        { "scope",    "Scope",          "VIEWS",      "Oscilloscope that locks to the note you play",                  true,  SlotOscA },
        { "meter",    "Loudness Meter", "VIEWS",      "LUFS (EBU R128), peaks and a loudness history",                 true,  SlotSub },
        { "pinboard", "Pinboard",       "TOOLS",      "Your own panel: right-click any knob, Pin to pinboard",         true,  SlotMod },
    };

    const TypeInfo* infoFor (const juce::String& type)
    {
        for (auto& t : catalogue) if (type == t.type) return &t;
        return nullptr;
    }

    juce::String typeOfId (const juce::String& id) { return id.upToFirstOccurrenceOf ("-", false, false); }

    // Design sizes of the tool widgets (the synth's own panels are sized in PluginEditor.cpp).
    juce::Rectangle<int> toolDesign (const juce::String& type)
    {
        if (type == "scope")    return { 0, 0, 420, 230 };
        if (type == "meter")    return { 0, 0, 300, 240 };
        if (type == "xy")       return { 0, 0, 330, 290 };
        if (type == "modmon")   return { 0, 0, 460, 230 };
        if (type == "macros")   return { 0, 0, 520, 250 };
        if (type == "pinboard") return { 0, 0, 420, 200 };
        return { 0, 0, 300, 200 };
    }

    const juce::Rectangle<int> keysArea { 24, 918, 1232, 56 };
    const juce::Rectangle<int> workArea { 24, 96, 1232, 810 };
}

//==============================================================================
Widget* HypernovaAudioProcessorEditor::findWidget (const juce::String& id) const
{
    for (auto& w : widgets) if (w->id == id) return w.get();
    return nullptr;
}

bool HypernovaAudioProcessorEditor::isMultiType (const juce::String& type)
{
    const auto* t = infoFor (type);
    return t != nullptr && t->multi;
}

juce::Rectangle<int> HypernovaAudioProcessorEditor::layoutArea() const
{
    auto a = workArea.withBottom (keyboard.isVisible() ? keysArea.getY() - dock::gutter : baseHeight - dock::gutter);
    // The library is a sidebar: while it's open the widgets make room for it rather than hiding under it.
    if (library != nullptr && library->isVisible()) a.removeFromRight (libraryWidth + dock::gutter);
    return a;
}

//==============================================================================
// Tool widgets
ToolServices HypernovaAudioProcessorEditor::toolServices()
{
    ToolServices s;
    s.makeKnob = [this] (const juce::String& paramId, const juce::String& label, ThemeColour c, int size)
    {
        auto k = std::make_unique<Knob> (processor.apvts, paramId, label, c, size);
        k->modLookup = [this] (const juce::String& p) { return modInfoFor (p); };
        k->onModDrop = [this] (const juce::String& src, const juce::String& p) { assignMod (src, p); };
        k->onModMenu = [this] (const juce::String& p) { showModMenu (p); };
        return k;
    };
    s.makeChip = [this] (int source, const juce::String& label)
    {
        auto chip = std::make_unique<ModChip> (label, source, modSourceColourFor (source));
        chip->onHover = [this] (int src) { hoveredModSource = src; for (auto& k : knobs) k->repaint(); };
        return chip;
    };
    s.message = [this] (const juce::String& m) { showMessage (m); };
    s.changed = [this] { layoutChanged(); };
    return s;
}

Widget* HypernovaAudioProcessorEditor::createTool (const juce::String& type, const juce::String& id, juce::ValueTree config)
{
    if (auto* existing = findWidget (id)) return existing;
    const auto* info = infoFor (type);
    if (info == nullptr) return nullptr;
    if (! config.isValid()) config = juce::ValueTree ("Tool");
    config.setProperty ("id", id, nullptr);
    config.setProperty ("type", type, nullptr);

    std::unique_ptr<ToolContent> tool;
    if (type == "scope")         tool = std::make_unique<ScopeTool> (processor, config);
    else if (type == "meter")    tool = std::make_unique<MeterTool> (processor, config);
    else if (type == "xy")       tool = std::make_unique<XYTool> (processor, config);
    else if (type == "modmon")   tool = std::make_unique<ModMonitorTool> (processor, config, toolServices());
    else if (type == "macros")   tool = std::make_unique<MacroTool> (processor, config, toolServices());
    else if (type == "pinboard") tool = std::make_unique<PinboardTool> (processor, config, toolServices());
    if (tool == nullptr) return nullptr;

    // Numbered titles for second and later copies: SCOPE, SCOPE 2...
    juce::String title = juce::String (info->name).toUpperCase();
    if (info->multi)
    {
        const int n = id.fromLastOccurrenceOf ("-", false, false).getIntValue();
        if (n > 1) title << " " << n;
    }
    const auto design = toolDesign (type);
    auto& w = makeWidget (id, type, title, ThemeColour { info->colourSlot }, design);
    w.content.addAndMakeVisible (*tool);
    tool->setBounds (12, 44, design.getWidth() - 24, design.getHeight() - 56);
    tool->addHeaderControls (w.content, design.getWidth());
    w.finishBuilding();
    w.spread.setFlags (*tool, Spread::Stretch);
    w.setVisible (false);
    tools[id] = std::move (tool);
    return &w;
}

void HypernovaAudioProcessorEditor::destroyTool (const juce::String& id)
{
    if (! isMultiType (typeOfId (id))) return; // the synth's own panels (and the macros panel) stay built
    if (tree.contains (id)) return;
    auto it = tools.find (id);
    for (auto w = widgets.begin(); w != widgets.end(); ++w)
        if ((*w)->id == id)
        {
            juce::Desktop::getInstance().getAnimator().cancelAnimation (w->get(), false);
            if (it != tools.end()) (*w)->content.removeChildComponent (it->second.get());
            widgets.erase (w);
            break;
        }
    if (it != tools.end()) tools.erase (it);
}

juce::String HypernovaAudioProcessorEditor::newToolId (const juce::String& type) const
{
    if (! isMultiType (type)) return type;
    for (int n = 1;; ++n)
    {
        const auto id = type + "-" + juce::String (n);
        if (findWidget (id) == nullptr) return id;
    }
}

void HypernovaAudioProcessorEditor::ensureWidget (const juce::String& type)
{
    if (type == "macros" && findWidget ("macros") == nullptr) createTool ("macros", "macros", {});
}

//==============================================================================
// The built-in arrangements. Sound Design is Hypernova as it has always looked.
juce::ValueTree HypernovaAudioProcessorEditor::defaultLayout (const juce::String& name) const
{
    using namespace ab::ui::dock;
    juce::ValueTree ws ("Workspace");
    juce::ValueTree toolsTree ("Tools");
    auto addTool = [&] (const char* id, const char* type)
    {
        juce::ValueTree t ("Tool");
        t.setProperty ("id", id, nullptr).setProperty ("type", type, nullptr);
        toolsTree.appendChild (t, nullptr);
    };
    const int page = juce::jlimit (0, 3, processor.uiDeckPage);
    juce::ValueTree root;
    if (name == "Effects")
    {
        // Both effect racks open in full, modulation under them, the core of the sound along the bottom.
        root = split (false, 1.0f, {
            leaf ({ "fx" }, 190), leaf ({ "morefx" }, 190), leaf ({ "mod", "play" }, 190),
            split (true, 212, { leaf ({ "filter" }, 332), leaf ({ "env" }, 368), leaf ({ "pitch" }, 256), leaf ({ "sub" }, 240) }) });
    }
    else if (name == "Analysis")
    {
        // Watching the sound: big spectrum, a scope locked to the note, loudness, and the modulation live.
        addTool ("scope-1", "scope");
        addTool ("meter-1", "meter");
        addTool ("modmon-1", "modmon");
        addTool ("xy-1", "xy");
        root = split (false, 1.0f, {
            split (true, 330, { leaf ({ "space" }, 560), leaf ({ "scope-1" }, 400), leaf ({ "meter-1" }, 260) }),
            split (true, 260, { leaf ({ "modmon-1" }, 520), leaf ({ "xy-1" }, 300), leaf ({ "filter" }, 400) }),
            leaf ({ "mod", "fx", "morefx", "play" }, 190, page) });
    }
    else
    {
        root = split (false, 1.0f, {
            split (true, 378, { leaf ({ "oscA" }, 400), leaf ({ "oscB" }, 400), leaf ({ "space" }, 408) }),
            split (true, 212, { leaf ({ "sub" }, 240), leaf ({ "pitch" }, 256), leaf ({ "filter" }, 332), leaf ({ "env" }, 368) }),
            leaf ({ "mod", "fx", "morefx", "play" }, 190, page) });
    }
    ws.appendChild (toolsTree, nullptr);
    juce::ValueTree dockTree ("Dock");
    dockTree.appendChild (root, nullptr);
    ws.appendChild (dockTree, nullptr);
    return ws;
}

juce::ValueTree HypernovaAudioProcessorEditor::captureLayout() const
{
    juce::ValueTree ws ("Workspace");
    juce::ValueTree toolsTree ("Tools");
    for (auto& [id, tool] : tools)
        if (tree.contains (id)) toolsTree.appendChild (tool->config.createCopy(), nullptr);
    ws.appendChild (toolsTree, nullptr);
    juce::ValueTree dockTree ("Dock");
    dockTree.appendChild (tree.toValueTree(), nullptr);
    ws.appendChild (dockTree, nullptr);
    return ws;
}

void HypernovaAudioProcessorEditor::applyLayout (const juce::ValueTree& layout)
{
    // Tools first, so the tree can refer to them; then the arrangement; anything unknown is dropped.
    for (auto t : layout.getChildWithName ("Tools"))
    {
        const auto id = t.getProperty ("id").toString();
        const auto type = t.getProperty ("type").toString();
        if (id.isEmpty() || infoFor (type) == nullptr) continue;
        if (auto it = tools.find (id); it != tools.end())
        {
            it->second->config.copyPropertiesFrom (t, nullptr); // same tool: take the saved settings
            continue;
        }
        createTool (type, id, t.createCopy());
    }
    const auto dockTree = layout.getChildWithName ("Dock");
    tree.fromValueTree (dockTree.getNumChildren() > 0 ? dockTree.getChild (0) : juce::ValueTree());
    for (auto& id : tree.allWidgets())
    {
        ensureWidget (id);
        if (findWidget (id) == nullptr) tree.remove (id);
    }
    // Tool instances the layout doesn't use any more go away (the synth's own panels stay built, just hidden).
    std::vector<juce::String> gone;
    for (auto& [id, tool] : tools) if (! tree.contains (id)) gone.push_back (id);
    for (auto& id : gone) destroyTool (id);
    maximisedId.clear();
    relayoutWidgets (false);
}

//==============================================================================
// Puts every widget where the tree says: the front widget of each place shows, the others wait behind tabs.
void HypernovaAudioProcessorEditor::relayoutWidgets (bool animate)
{
    const auto area = layoutArea();
    if (maximisedId.isNotEmpty() && ! tree.contains (maximisedId)) maximisedId.clear();
    tree.layout (area, dockMinSize());
    auto& animator = juce::Desktop::getInstance().getAnimator();
    for (auto& w : widgets)
    {
        auto* leaf = tree.findLeaf (w->id);
        const bool front = leaf != nullptr && leaf->activeId() == w->id && (maximisedId.isEmpty() || maximisedId == w->id);
        if (! front)
        {
            animator.cancelAnimation (w.get(), false);
            w->setVisible (false);
            continue;
        }
        juce::StringArray titles;
        for (auto& id : leaf->widgets) if (auto* m = findWidget (id)) titles.add (m->title);
        const bool maximised = maximisedId == w->id;
        w->maximised = maximised;
        w->collapsed = leaf->collapsed && ! maximised;
        w->sideways = dock::Tree::foldsSideways (*leaf);
        w->setStack (titles, leaf->active);
        w->tabs.onSelect = [this, id = w->id] (int i)
        {
            if (auto* l = tree.findLeaf (id)) activateWidget (l->widgets[i]);
        };
        const auto target = maximised ? area : leaf->bounds;
        bool wasVisible = w->isVisible();
        w->setEditing (layoutEditing);
        if (animate && w->id == landingId && ! landingFrom.isEmpty())
        {
            // Just dropped: start from where the ghost was let go and glide into the slot.
            w->setBounds (landingFrom);
            wasVisible = true;
        }
        w->setVisible (true);
        if (animate && wasVisible && w->getBounds() != target)
            animator.animateComponent (w.get(), target, 1.0f, 220, false, 0.2, 0.0);
        else
        {
            animator.cancelAnimation (w.get(), false);
            w->setBounds (target);
        }
    }
    overlay->setBounds (area);
    overlay->toFront (false);
    if (library->isVisible())
    {
        library->setBounds (area.getRight() + dock::gutter, area.getY(), libraryWidth, area.getHeight());
        library->toFront (false);
    }
    if (browser.isVisible()) browser.toFront (false);
    if (updateBanner.isVisible()) updateBanner.toFront (false);
    overlay->repaint();
}

dock::MinSize HypernovaAudioProcessorEditor::dockMinSize() const
{
    return [this] (const juce::String& id)
    {
        if (auto* w = findWidget (id)) return w->minimumSize();
        return juce::Point<int> (120, 80);
    };
}

//==============================================================================
// Operations. Each one changes the arrangement, then records it for undo and saves the workspace.
juce::String HypernovaAudioProcessorEditor::addWidgetType (const juce::String& type)
{
    if (infoFor (type) == nullptr) return {};
    juce::String id = type;
    if (isMultiType (type))
    {
        id = newToolId (type);
        createTool (type, id, {});
    }
    ensureWidget (type);
    if (tree.contains (id))
    {
        // Already there: bring it to the front of its stack (and out of a maximised view).
        activateWidget (id);
        showMessage (juce::String (infoFor (type)->name) + " is already on screen");
        return id;
    }
    maximisedId.clear();
    tree.layout (layoutArea(), dockMinSize());
    tree.insertSomewhere (id);
    relayoutWidgets (true);
    layoutChanged();
    showMessage ("Added " + juce::String (infoFor (type)->name) + (layoutEditing ? "" : ". Turn on layout mode to move it."));
    return id;
}

void HypernovaAudioProcessorEditor::hideWidget (const juce::String& id)
{
    // Hiding is only about the screen: the module keeps running exactly as it was.
    auto* w = findWidget (id);
    if (w == nullptr || ! tree.contains (id)) return;
    const auto name = w->title;
    tree.remove (id);
    if (maximisedId == id) maximisedId.clear();
    relayoutWidgets (true);
    destroyTool (id);
    layoutChanged();
    showMessage (name + (isMultiType (typeOfId (id)) ? " removed" : " hidden. The sound is unchanged: add it back from the library."));
}

void HypernovaAudioProcessorEditor::replaceWidget (const juce::String& id, const juce::String& type)
{
    if (! tree.contains (id) || infoFor (type) == nullptr) return;
    juce::String newId = type;
    if (isMultiType (type)) { newId = newToolId (type); createTool (type, newId, {}); }
    ensureWidget (type);
    if (newId == id) return;
    tree.replace (id, newId);
    relayoutWidgets (true);
    destroyTool (id);
    layoutChanged();
}

juce::String HypernovaAudioProcessorEditor::duplicateWidget (const juce::String& id)
{
    const auto type = typeOfId (id);
    auto it = tools.find (id);
    if (! isMultiType (type) || it == tools.end() || ! tree.contains (id)) return {};
    const auto newId = newToolId (type);
    createTool (type, newId, it->second->config.createCopy());
    tree.insert (newId, tree.findLeaf (id), dock::Zone::Right);
    relayoutWidgets (true);
    layoutChanged();
    return newId;
}

void HypernovaAudioProcessorEditor::toggleCollapse (const juce::String& id)
{
    auto* leaf = tree.findLeaf (id);
    if (leaf == nullptr) return;
    leaf->collapsed = ! leaf->collapsed;
    if (maximisedId == id) maximisedId.clear();
    relayoutWidgets (true);
    layoutChanged();
}

void HypernovaAudioProcessorEditor::toggleMaximise (const juce::String& id)
{
    if (! tree.contains (id)) return;
    maximisedId = maximisedId == id ? juce::String() : id;
    if (maximisedId.isNotEmpty())
        if (auto* leaf = tree.findLeaf (id)) { leaf->collapsed = false; tree.activate (id); }
    relayoutWidgets (true);
    if (maximisedId.isNotEmpty()) showMessage ("Maximised. Double-click the title again (or right-click) to restore.");
}

void HypernovaAudioProcessorEditor::activateWidget (const juce::String& id)
{
    if (! tree.contains (id)) return;
    tree.activate (id);
    if (maximisedId.isNotEmpty() && maximisedId != id)
        if (auto* leaf = tree.findLeaf (maximisedId); leaf != nullptr && leaf->widgets.contains (id)) maximisedId = id;
    static const char* pages[] = { "mod", "fx", "morefx", "play" };
    for (int i = 0; i < 4; ++i) if (id == pages[i]) processor.uiDeckPage = i;
    relayoutWidgets (false);
    layoutChanged();
}

void HypernovaAudioProcessorEditor::moveWidget (const juce::String& id, const juce::String& targetId, dock::Zone zone)
{
    if (findWidget (id) == nullptr) return;
    dock::Drop d;
    d.zone = zone;
    d.leaf = targetId.isNotEmpty() ? tree.findLeaf (targetId) : nullptr;
    dockDrop (id, false, d, {});
}

//==============================================================================
// Pinboard: the first pinboard on screen gets the knob (one is added if there isn't one).
PinboardTool* HypernovaAudioProcessorEditor::firstPinboard() const
{
    for (auto& id : tree.allWidgets())
        if (typeOfId (id) == "pinboard")
            if (auto it = tools.find (id); it != tools.end()) return dynamic_cast<PinboardTool*> (it->second.get());
    return nullptr;
}

bool HypernovaAudioProcessorEditor::isPinned (const juce::String& paramId) const
{
    auto* p = firstPinboard();
    return p != nullptr && p->has (paramId);
}

void HypernovaAudioProcessorEditor::pinParameter (const juce::String& paramId, bool pin)
{
    auto* board = firstPinboard();
    if (board == nullptr)
    {
        if (! pin) return;
        const auto id = addWidgetType ("pinboard");
        if (auto it = tools.find (id); it != tools.end()) board = dynamic_cast<PinboardTool*> (it->second.get());
    }
    if (board == nullptr) return;
    auto* param = processor.apvts.getParameter (paramId);
    if (pin) board->pin (paramId); else board->unpin (paramId);
    layoutChanged();
    if (param != nullptr) showMessage (param->getName (40) + (pin ? " pinned to the pinboard" : " unpinned"));
}

//==============================================================================
// DockOverlay::Host
void HypernovaAudioProcessorEditor::dockCommit (const juce::String&) { layoutChanged(); }

void HypernovaAudioProcessorEditor::dockDrop (const juce::String& idOrType, bool isNewType, const dock::Drop& drop, juce::Rectangle<int> from)
{
    juce::String id = idOrType;
    if (isNewType)
    {
        if (isMultiType (idOrType)) { id = newToolId (idOrType); createTool (idOrType, id, {}); }
        ensureWidget (idOrType);
    }
    if (findWidget (id) == nullptr || drop.zone == dock::Zone::None) return;
    // A drop on its own place does nothing.
    if (drop.leaf != nullptr && drop.leaf->widgets.size() == 1 && drop.leaf->widgets[0] == id) return;
    maximisedId.clear();
    tree.insert (id, drop.leaf, drop.zone, drop.leaf != nullptr ? 0.5f : 0.3f);
    landingId = id;
    landingFrom = from;
    relayoutWidgets (true);
    landingId.clear();
    destroyTool (id); // only acts if it somehow didn't make it into the tree
    layoutChanged();
}

void HypernovaAudioProcessorEditor::dockButton (Widget& w, int button, juce::Point<int> screenPos)
{
    if (button == Widget::Close) hideWidget (w.id);
    else if (button == Widget::Collapse) toggleCollapse (w.id);
    else showWidgetMenu (w, screenPos);
}

void HypernovaAudioProcessorEditor::dockActivate (const juce::String& id) { activateWidget (id); }

//==============================================================================
void HypernovaAudioProcessorEditor::wireWidget (Widget& w)
{
    w.onMenu = [this] (Widget& wd, juce::Point<int> pos) { showWidgetMenu (wd, pos); };
    w.onMaximise = [this] (Widget& wd) { toggleMaximise (wd.id); };
    w.onExpand = [this] (Widget& wd) { toggleCollapse (wd.id); };
}

void HypernovaAudioProcessorEditor::showWidgetMenu (Widget& w, juce::Point<int> screenPos)
{
    const auto id = w.id;
    auto* leaf = tree.findLeaf (id);
    if (leaf == nullptr) return;
    juce::PopupMenu m, replace, add;
    m.setLookAndFeel (&lookAndFeel);
    m.addSectionHeader (w.title);
    m.addItem (1, maximisedId == id ? "Restore" : "Maximise");
    m.addItem (2, leaf->collapsed ? "Expand" : "Collapse", maximisedId != id);
    m.addItem (3, "Take out of the tabs", leaf->widgets.size() > 1 && maximisedId != id);
    int index = 0;
    std::vector<juce::String> types;
    for (auto& t : catalogue)
    {
        types.push_back (t.type);
        const bool onScreen = ! t.multi && tree.contains (t.type);
        replace.addItem (100 + index, t.name, ! onScreen && typeOfId (id) != t.type);
        add.addItem (200 + index, t.name, t.multi || ! tree.contains (t.type));
        ++index;
    }
    m.addSubMenu ("Replace with", replace);
    m.addItem (4, "Duplicate", isMultiType (typeOfId (id)));
    m.addItem (5, isMultiType (typeOfId (id)) ? "Remove" : "Hide (the sound keeps playing)");
    m.addSeparator();
    m.addSubMenu ("Add a widget", add);
    m.addItem (6, layoutEditing ? "Done arranging" : "Arrange widgets...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }), [this, id, types] (int r)
    {
        if (r == 1) toggleMaximise (id);
        else if (r == 2) toggleCollapse (id);
        else if (r == 3)
        {
            if (auto* l = tree.findLeaf (id); l != nullptr && l->widgets.size() > 1)
                moveWidget (id, l->widgets[0] == id ? l->widgets[1] : l->widgets[0], dock::Zone::Right);
        }
        else if (r == 4) duplicateWidget (id);
        else if (r == 5) hideWidget (id);
        else if (r == 6) setLayoutEditing (! layoutEditing);
        else if (r >= 100 && r < 200) replaceWidget (id, types[(size_t) (r - 100)]);
        else if (r >= 200 && r < 300) addWidgetType (types[(size_t) (r - 200)]);
    });
}

std::vector<WidgetLibrary::Entry> HypernovaAudioProcessorEditor::libraryEntries() const
{
    std::vector<WidgetLibrary::Entry> out;
    for (auto& t : catalogue)
    {
        WidgetLibrary::Entry e;
        e.type = t.type; e.name = t.name; e.group = t.group; e.description = t.description; e.multi = t.multi;
        e.colour = ThemeColour { t.colourSlot };
        if (! t.multi)
            if (auto* leaf = tree.findLeaf (t.type))
                e.state = leaf->activeId() == t.type ? 1 : 2;
        out.push_back (e);
    }
    return out;
}

void HypernovaAudioProcessorEditor::setLibraryOpen (bool open)
{
    if (open) library->refresh();
    library->setVisible (open);
    editBar.add.setToggleState (open, juce::dontSendNotification);
    relayoutWidgets (false);
}

//==============================================================================
void HypernovaAudioProcessorEditor::layoutChanged()
{
    // Layout undo history, and the current workspace keeps what was arranged.
    auto now = captureLayout();
    if (layoutHistoryIndex >= 0 && layoutHistoryIndex < (int) layoutHistory.size()
        && layoutHistory[(size_t) layoutHistoryIndex].isEquivalentTo (now)) return;
    if (layoutHistoryIndex + 1 < (int) layoutHistory.size())
        layoutHistory.erase (layoutHistory.begin() + layoutHistoryIndex + 1, layoutHistory.end());
    layoutHistory.push_back (now);
    if (layoutHistory.size() > 80) layoutHistory.erase (layoutHistory.begin());
    layoutHistoryIndex = (int) layoutHistory.size() - 1;
    ab::ui::WorkspaceStore::save (workspaceName, now);
    if (library->isVisible()) library->refresh();
}

void HypernovaAudioProcessorEditor::undoLayout()
{
    if (layoutHistoryIndex <= 0) { showMessage ("Nothing to undo in the layout"); return; }
    applyLayout (layoutHistory[(size_t) --layoutHistoryIndex]);
    ab::ui::WorkspaceStore::save (workspaceName, layoutHistory[(size_t) layoutHistoryIndex]);
    if (library->isVisible()) library->refresh();
}

void HypernovaAudioProcessorEditor::redoLayout()
{
    if (layoutHistoryIndex + 1 >= (int) layoutHistory.size()) { showMessage ("Nothing to redo in the layout"); return; }
    applyLayout (layoutHistory[(size_t) ++layoutHistoryIndex]);
    ab::ui::WorkspaceStore::save (workspaceName, layoutHistory[(size_t) layoutHistoryIndex]);
    if (library->isVisible()) library->refresh();
}

void HypernovaAudioProcessorEditor::loadWorkspace (const juce::String& name, bool recordHistory)
{
    // Only the arrangement changes. The sound, its parameters and the audio engine are untouched.
    workspaceName = name.isNotEmpty() ? name : juce::String ("Sound Design");
    auto layout = ab::ui::WorkspaceStore::load (workspaceName);
    if (! layout.isValid() || ! layout.getChildWithName ("Dock").isValid()) layout = defaultLayout (workspaceName);
    applyLayout (layout);
    if (tree.empty()) applyLayout (defaultLayout ("Sound Design"));
    ab::ui::WorkspaceStore::setCurrent (workspaceName);
    editBar.workspace.setButtonText (workspaceName.toUpperCase() + "  v");
    if (! recordHistory) { layoutHistory.clear(); layoutHistoryIndex = -1; }
    layoutChanged();
}

void HypernovaAudioProcessorEditor::setLayoutEditing (bool editing)
{
    layoutEditing = editing;
    overlay->setEditing (editing);
    for (auto& w : widgets) w->setEditing (editing);
    editBar.setVisible (editing);
    for (auto* c : std::initializer_list<juce::Component*> { &presetPlate, &prevButton, &nextButton, &diceButton, &saveButton, &undoButton, &redoButton, &layoutButton, &gearButton })
        c->setVisible (! editing);
    layoutButton.setToggleState (editing, juce::dontSendNotification);
    if (editing) showMessage ("layout mode: drag a widget, drop it mid-panel to stack or near an edge to split; drag the gaps to resize");
    if (! editing && library->isVisible()) setLibraryOpen (false);
    else relayoutWidgets (false);
    canvas.repaint();
}

void HypernovaAudioProcessorEditor::setupEditBar()
{
    canvas.addChildComponent (editBar);
    editBar.add.onClick = [this] { setLibraryOpen (! library->isVisible()); };
    editBar.workspace.onClick = [this] { showWorkspaceMenu(); };
    editBar.undo.onClick = [this] { undoLayout(); };
    editBar.redo.onClick = [this] { redoLayout(); };
    editBar.reset.setTooltip ("Put this workspace back to how it started");
    editBar.reset.onClick = [this]
    {
        applyLayout (defaultLayout (ab::ui::WorkspaceStore::builtIn().contains (workspaceName) ? workspaceName : juce::String ("Sound Design")));
        layoutChanged();
        showMessage ("layout reset");
    };
    editBar.done.onClick = [this] { setLayoutEditing (false); };
    for (auto* b : { &editBar.add, &editBar.workspace, &editBar.undo, &editBar.redo, &editBar.reset, &editBar.done })
        b->setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    // Done is the one obvious way out, so it's the filled one.
    editBar.done.setColour (juce::TextButton::buttonColourId, Colours::accent);
    editBar.done.setColour (juce::TextButton::textColourOffId, Colours::bg0);

    overlay = std::make_unique<DockOverlay> (*this);
    canvas.addAndMakeVisible (*overlay);
    library = std::make_unique<WidgetLibrary>();
    canvas.addChildComponent (*library);
    library->entries = [this] { return libraryEntries(); };
    library->onAdd = [this] (const juce::String& type) { addWidgetType (type); };
    library->onClose = [this] { setLibraryOpen (false); };
    library->onDragStart = [this] (const juce::String& type, juce::Image ghost, const juce::MouseEvent& e)
    {
        overlay->startNewDrag (type, ghost, e.getEventRelativeTo (overlay.get()).getPosition());
    };
    library->onDragMove = [this] (const juce::MouseEvent& e) { overlay->moveDrag (e.getEventRelativeTo (overlay.get()).getPosition()); };
    library->onDragEnd = [this] (const juce::MouseEvent& e)
    {
        overlay->moveDrag (e.getEventRelativeTo (overlay.get()).getPosition());
        overlay->endDrag();
    };
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

void HypernovaAudioProcessorEditor::tickTools (bool sounding)
{
    // Only tools on screen do any work. (Off the desktop, as in the snapshot tests, "on screen" means visible.)
    const bool onDesktop = isShowing();
    for (auto& [id, tool] : tools)
    {
        auto* w = findWidget (id);
        if (onDesktop ? tool->isShowing() : (w != nullptr && w->isVisible())) tool->tick (sounding);
    }
}
