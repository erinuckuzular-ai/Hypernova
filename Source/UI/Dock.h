#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

// Docking layout for the editor's widgets, like the panel docking in Audition or an IDE.
// The work area is a tree: a split lays its children out side by side (or top to bottom) with a
// gutter between them, and a leaf is one place on screen that holds a stack of widgets shown as
// tabs. There are never holes or overlaps: removing a widget gives its space to its neighbours,
// and dropping one splits the place it lands on. Pure logic: no components.
namespace ab::ui::dock
{

constexpr int gutter = 12;
constexpr int collapsedSpine = 40;   // a collapsed widget in a row: a vertical strip
constexpr int collapsedStrip = 40;   // a collapsed widget in a column: its title row

struct Node
{
    bool isLeaf = true;
    bool horizontal = true;                     // split: children left to right (true) or top to bottom
    std::vector<std::unique_ptr<Node>> children;
    std::vector<float> weights;
    Node* parent = nullptr;

    juce::StringArray widgets;                  // leaf: widget ids in tab order
    int active = 0;
    bool collapsed = false;

    juce::Rectangle<int> bounds;

    juce::String activeId() const { return widgets.isEmpty() ? juce::String() : widgets[juce::jlimit (0, widgets.size() - 1, active)]; }
};

// Minimum size of one widget, in canvas units, supplied by the host.
using MinSize = std::function<juce::Point<int> (const juce::String& widgetId)>;

enum class Zone { None, Stack, Left, Right, Top, Bottom };

struct Drop
{
    Node* leaf = nullptr;       // nullptr with an edge zone: the whole work area's edge
    Zone zone = Zone::None;
    juce::Rectangle<int> preview;
};

struct Divider
{
    Node* split = nullptr;
    int index = 0;              // between children index and index + 1
    juce::Rectangle<int> area;
};

class Tree
{
public:
    Tree() : root (std::make_unique<Node>()) {}

    Node* getRoot() const { return root.get(); }
    bool empty() const { return root->isLeaf && root->widgets.isEmpty(); }

    //==========================================================================
    // Serialising. Layout state lives in workspaces, never in the patch.
    juce::ValueTree toValueTree() const { return write (*root); }

    void fromValueTree (const juce::ValueTree& v)
    {
        root = v.isValid() ? read (v, nullptr) : std::make_unique<Node>();
        normalise();
    }

    //==========================================================================
    Node* findLeaf (const juce::String& id) const { return findLeaf (*root, id); }
    bool contains (const juce::String& id) const { return findLeaf (id) != nullptr; }

    juce::StringArray allWidgets() const
    {
        juce::StringArray out;
        forEachLeaf ([&] (Node& n) { out.addArray (n.widgets); });
        return out;
    }

    void forEachLeaf (const std::function<void (Node&)>& f) const { forEachLeaf (*root, f); }

    // Takes a widget out. Its place closes up and the neighbours get the space.
    void remove (const juce::String& id)
    {
        auto* leaf = findLeaf (id);
        if (leaf == nullptr) return;
        const int i = leaf->widgets.indexOf (id);
        leaf->widgets.remove (i);
        if (leaf->active >= i && leaf->active > 0) --leaf->active;
        if (leaf->widgets.isEmpty()) removeNode (leaf);
        normalise();
    }

    // Puts a widget into the tree: stacked with a leaf as a new tab, or beside it on one side.
    // A null leaf with an edge zone docks along the whole work area's edge.
    void insert (const juce::String& id, Node* target, Zone zone, float share = 0.5f)
    {
        if (id.isEmpty() || zone == Zone::None) return;
        // Taking the widget out first can reshape the tree, so remember the target by a widget it holds.
        juce::String anchor;
        if (target != nullptr && target->isLeaf)
            for (auto& w : target->widgets) if (w != id) { anchor = w; break; }
        if (target != nullptr && anchor.isEmpty()) return; // dropped onto its own place
        remove (id);
        if (empty()) { root->widgets.add (id); root->active = 0; return; }
        target = anchor.isNotEmpty() ? findLeaf (anchor) : nullptr;
        if (zone == Zone::Stack)
        {
            if (target == nullptr || ! target->isLeaf) return;
            target->widgets.add (id);
            target->active = target->widgets.size() - 1;
            return;
        }
        auto fresh = std::make_unique<Node>();
        fresh->widgets.add (id);
        splitBeside (target != nullptr ? target : root.get(), std::move (fresh), zone, share);
        normalise();
    }

    // Puts a widget where it reads naturally when added without dragging: beside the biggest place,
    // along that place's longer side.
    void insertSomewhere (const juce::String& id)
    {
        if (empty()) { insert (id, nullptr, Zone::Left); return; }
        Node* best = nullptr;
        forEachLeaf ([&] (Node& n)
        {
            if (n.collapsed) return;
            if (best == nullptr || n.bounds.getWidth() * n.bounds.getHeight() > best->bounds.getWidth() * best->bounds.getHeight()) best = &n;
        });
        if (best == nullptr) { insert (id, nullptr, Zone::Right); return; }
        insert (id, best, best->bounds.getWidth() >= best->bounds.getHeight() ? Zone::Right : Zone::Bottom);
    }

    // Swaps one widget for another in the same place (same tab position).
    void replace (const juce::String& oldId, const juce::String& newId)
    {
        auto* leaf = findLeaf (oldId);
        if (leaf == nullptr || oldId == newId) return;
        remove (newId);
        leaf = findLeaf (oldId);
        if (leaf == nullptr) return;
        const int i = leaf->widgets.indexOf (oldId);
        leaf->widgets.set (i, newId);
        leaf->active = i;
    }

    void activate (const juce::String& id)
    {
        if (auto* leaf = findLeaf (id)) leaf->active = leaf->widgets.indexOf (id);
    }

    //==========================================================================
    // Layout: fills `area` exactly. Collapsed leaves take a fixed strip along their parent's axis;
    // everything else shares the rest by weight, never going under its minimum size.
    void layout (juce::Rectangle<int> area, const MinSize& minSize) { layoutNode (*root, area, minSize); }

    juce::Point<int> minimumSize (const MinSize& minSize) const { return minimumOf (*root, minSize); }

    //==========================================================================
    // What a drag would do if released at `p`.
    Drop dropAt (juce::Point<int> p, juce::Rectangle<int> area, const juce::String& dragged) const
    {
        Drop d;
        if (! area.contains (p)) return d;
        // Along the very edge of the work area: dock across the whole side.
        const int edge = 18;
        if (! empty() && ! (root->isLeaf && root->widgets.size() == 1 && root->widgets[0] == dragged))
        {
            if (p.x < area.getX() + edge)       { d.zone = Zone::Left;   d.preview = area.withWidth (area.getWidth() / 4); return d; }
            if (p.x > area.getRight() - edge)   { d.zone = Zone::Right;  d.preview = area.withTrimmedLeft (area.getWidth() * 3 / 4); return d; }
            if (p.y < area.getY() + edge)       { d.zone = Zone::Top;    d.preview = area.withHeight (area.getHeight() / 4); return d; }
            if (p.y > area.getBottom() - edge)  { d.zone = Zone::Bottom; d.preview = area.withTrimmedTop (area.getHeight() * 3 / 4); return d; }
        }
        Node* leaf = nullptr;
        forEachLeaf ([&] (Node& n) { if (n.bounds.expanded (gutter / 2).contains (p)) leaf = &n; });
        if (leaf == nullptr) return d;
        const bool onlyDragged = leaf->widgets.size() == 1 && leaf->widgets[0] == dragged;
        const auto b = leaf->bounds;
        d.leaf = leaf;
        // The middle (and the title row) stacks; nearer an edge splits on that side.
        const float fx = (float) (p.x - b.getX()) / (float) juce::jmax (1, b.getWidth());
        const float fy = (float) (p.y - b.getY()) / (float) juce::jmax (1, b.getHeight());
        const bool titleRow = p.y < b.getY() + 44;
        const float dl = fx, dr = 1.0f - fx, dt = fy, db = 1.0f - fy;
        const float nearest = juce::jmin (juce::jmin (dl, dr), juce::jmin (dt, db));
        if ((titleRow && dl > 0.12f && dr > 0.12f) || nearest > 0.28f)
        {
            if (onlyDragged || leaf->collapsed) { d.zone = Zone::None; d.leaf = nullptr; return d; }
            d.zone = Zone::Stack;
            d.preview = b;
            return d;
        }
        if (onlyDragged) { d.zone = Zone::None; d.leaf = nullptr; return d; }
        if (nearest == dl)      { d.zone = Zone::Left;   d.preview = b.withWidth (b.getWidth() / 2); }
        else if (nearest == dr) { d.zone = Zone::Right;  d.preview = b.withTrimmedLeft (b.getWidth() / 2); }
        else if (nearest == dt) { d.zone = Zone::Top;    d.preview = b.withHeight (b.getHeight() / 2); }
        else                    { d.zone = Zone::Bottom; d.preview = b.withTrimmedTop (b.getHeight() / 2); }
        return d;
    }

    // The gutters between siblings, which can be dragged to share space differently.
    std::vector<Divider> dividers() const
    {
        std::vector<Divider> out;
        collectDividers (*root, out);
        return out;
    }

    // Moves a divider by `delta` canvas units from the sizes recorded in `startSizes`.
    // Collapsed neighbours can't be resized; minimum sizes always hold.
    static std::vector<int> childSizes (const Node& split)
    {
        std::vector<int> s;
        for (auto& c : split.children) s.push_back (split.horizontal ? c->bounds.getWidth() : c->bounds.getHeight());
        return s;
    }

    void dragDivider (Node& split, int index, const std::vector<int>& startSizes, int delta, const MinSize& minSize)
    {
        if (split.isLeaf || index < 0 || index + 1 >= (int) split.children.size() || startSizes.size() != split.children.size()) return;
        auto* a = split.children[(size_t) index].get();
        auto* b = split.children[(size_t) index + 1].get();
        if (isCollapsedLeaf (*a) || isCollapsedLeaf (*b)) return;
        const auto minA = minimumOf (*a, minSize), minB = minimumOf (*b, minSize);
        const int ma = split.horizontal ? minA.x : minA.y, mb = split.horizontal ? minB.x : minB.y;
        const int total = startSizes[(size_t) index] + startSizes[(size_t) index + 1];
        const int sa = juce::jlimit (ma, juce::jmax (ma, total - mb), startSizes[(size_t) index] + delta);
        auto sizes = startSizes;
        sizes[(size_t) index] = sa;
        sizes[(size_t) index + 1] = total - sa;
        // Weights are proportional to the sizes of the flexible children.
        for (size_t i = 0; i < sizes.size(); ++i)
            split.weights[i] = isCollapsedLeaf (*split.children[i]) ? split.weights[i] : (float) juce::jmax (1, sizes[i]);
    }

    static bool isCollapsedLeaf (const Node& n) { return n.isLeaf && n.collapsed; }

    // Where a leaf sits: which axis its parent lays out on (a collapsed widget folds along it).
    static bool foldsSideways (const Node& leaf) { return leaf.parent != nullptr && leaf.parent->horizontal; }

private:
    std::unique_ptr<Node> root;

    static Node* findLeaf (const Node& n, const juce::String& id)
    {
        if (n.isLeaf) return n.widgets.contains (id) ? const_cast<Node*> (&n) : nullptr;
        for (auto& c : n.children)
            if (auto* f = findLeaf (*c, id)) return f;
        return nullptr;
    }

    static void forEachLeaf (Node& n, const std::function<void (Node&)>& f)
    {
        if (n.isLeaf) { f (n); return; }
        for (auto& c : n.children) forEachLeaf (*c, f);
    }

    void removeNode (Node* n)
    {
        auto* p = n->parent;
        if (p == nullptr) { n->widgets.clear(); n->isLeaf = true; n->children.clear(); n->weights.clear(); return; }
        for (size_t i = 0; i < p->children.size(); ++i)
            if (p->children[i].get() == n)
            {
                p->children.erase (p->children.begin() + (long) i);
                p->weights.erase (p->weights.begin() + (long) i);
                break;
            }
    }

    void splitBeside (Node* target, std::unique_ptr<Node> fresh, Zone zone, float share)
    {
        const bool horizontal = zone == Zone::Left || zone == Zone::Right;
        const bool before = zone == Zone::Left || zone == Zone::Top;
        share = juce::jlimit (0.1f, 0.9f, share);
        auto* p = target->parent;
        if (p != nullptr && p->horizontal == horizontal)
        {
            // Same direction as the parent: slot in next to the target and take part of its weight.
            size_t i = 0;
            while (p->children[i].get() != target) ++i;
            const float w = p->weights[i];
            p->weights[i] = w * (1.0f - share);
            fresh->parent = p;
            const size_t at = before ? i : i + 1;
            p->children.insert (p->children.begin() + (long) at, std::move (fresh));
            p->weights.insert (p->weights.begin() + (long) at, w * share);
            return;
        }
        // Otherwise the target becomes a split of itself and the newcomer.
        auto moved = std::make_unique<Node>();
        std::swap (moved->isLeaf, target->isLeaf);
        std::swap (moved->horizontal, target->horizontal);
        std::swap (moved->children, target->children);
        std::swap (moved->weights, target->weights);
        std::swap (moved->widgets, target->widgets);
        std::swap (moved->active, target->active);
        std::swap (moved->collapsed, target->collapsed);
        moved->bounds = target->bounds;
        for (auto& c : moved->children) c->parent = moved.get();
        target->isLeaf = false;
        target->horizontal = horizontal;
        target->widgets.clear();
        moved->parent = target;
        fresh->parent = target;
        if (before)
        {
            target->children.push_back (std::move (fresh));
            target->children.push_back (std::move (moved));
            target->weights = { share, 1.0f - share };
        }
        else
        {
            target->children.push_back (std::move (moved));
            target->children.push_back (std::move (fresh));
            target->weights = { 1.0f - share, share };
        }
    }

    // Tidies the tree: no empty leaves, no splits of one, no split inside a split of the same direction.
    void normalise() { normaliseNode (*root); relink (*root, nullptr); }

    void normaliseNode (Node& n)
    {
        if (n.isLeaf) { n.active = juce::jlimit (0, juce::jmax (0, n.widgets.size() - 1), n.active); return; }
        for (size_t i = 0; i < n.children.size();)
        {
            normaliseNode (*n.children[i]);
            auto& c = *n.children[i];
            if ((c.isLeaf && c.widgets.isEmpty()) || (! c.isLeaf && c.children.empty()))
            {
                n.children.erase (n.children.begin() + (long) i);
                n.weights.erase (n.weights.begin() + (long) i);
                continue;
            }
            if (! c.isLeaf && c.horizontal == n.horizontal)
            {
                // Flatten: its children join ours, sharing its weight in proportion.
                float sum = 0;
                for (auto w : c.weights) sum += w;
                const float scale = n.weights[i] / juce::jmax (0.0001f, sum);
                std::vector<std::unique_ptr<Node>> kids;
                std::vector<float> ws;
                for (size_t k = 0; k < c.children.size(); ++k) { kids.push_back (std::move (c.children[k])); ws.push_back (c.weights[k] * scale); }
                n.children.erase (n.children.begin() + (long) i);
                n.weights.erase (n.weights.begin() + (long) i);
                for (size_t k = 0; k < kids.size(); ++k)
                {
                    n.children.insert (n.children.begin() + (long) (i + k), std::move (kids[k]));
                    n.weights.insert (n.weights.begin() + (long) (i + k), ws[k]);
                }
                continue;
            }
            ++i;
        }
        if (n.children.size() == 1)
        {
            // A split of one is just that child.
            auto only = std::move (n.children[0]);
            n.isLeaf = only->isLeaf;
            n.horizontal = only->horizontal;
            n.widgets = only->widgets;
            n.active = only->active;
            n.collapsed = only->collapsed;
            n.weights = only->weights;
            n.children = std::move (only->children);
            normaliseNode (n);
        }
        else if (n.children.empty())
        {
            n.isLeaf = true;
            n.weights.clear();
        }
        for (auto& w : n.weights) w = juce::jmax (0.0001f, w);
    }

    static void relink (Node& n, Node* parent)
    {
        n.parent = parent;
        for (auto& c : n.children) relink (*c, &n);
    }

    static juce::Point<int> minimumOf (const Node& n, const MinSize& minSize)
    {
        if (n.isLeaf)
        {
            juce::Point<int> m;
            for (auto& id : n.widgets) { const auto s = minSize (id); m.x = juce::jmax (m.x, s.x); m.y = juce::jmax (m.y, s.y); }
            if (n.collapsed)
            {
                if (foldsSideways (n)) m.x = collapsedSpine;
                else m.y = collapsedStrip;
            }
            return m;
        }
        juce::Point<int> m;
        for (size_t i = 0; i < n.children.size(); ++i)
        {
            const auto c = minimumOf (*n.children[i], minSize);
            if (n.horizontal) { m.x += c.x + (i > 0 ? gutter : 0); m.y = juce::jmax (m.y, c.y); }
            else              { m.y += c.y + (i > 0 ? gutter : 0); m.x = juce::jmax (m.x, c.x); }
        }
        return m;
    }

    void layoutNode (Node& n, juce::Rectangle<int> area, const MinSize& minSize)
    {
        n.bounds = area;
        if (n.isLeaf) return;
        const int count = (int) n.children.size();
        const int length = (n.horizontal ? area.getWidth() : area.getHeight()) - gutter * (count - 1);
        std::vector<int> sizes ((size_t) count, 0);
        std::vector<bool> fixed ((size_t) count, false);
        int left = length;
        for (int i = 0; i < count; ++i)
            if (isCollapsedLeaf (*n.children[(size_t) i]))
            {
                sizes[(size_t) i] = n.horizontal ? collapsedSpine : collapsedStrip;
                fixed[(size_t) i] = true;
                left -= sizes[(size_t) i];
            }
        // Share by weight; anything that would go under its minimum is pinned at the minimum and the rest re-shared.
        for (int pass = 0; pass < count; ++pass)
        {
            float wsum = 0;
            int room = left;
            for (int i = 0; i < count; ++i) if (! fixed[(size_t) i]) wsum += n.weights[(size_t) i];
            bool pinned = false;
            for (int i = 0; i < count; ++i)
            {
                if (fixed[(size_t) i]) continue;
                const auto m = minimumOf (*n.children[(size_t) i], minSize);
                const int mn = n.horizontal ? m.x : m.y;
                const int want = juce::roundToInt ((float) room * n.weights[(size_t) i] / juce::jmax (0.0001f, wsum));
                if (want < mn) { sizes[(size_t) i] = mn; fixed[(size_t) i] = true; left -= mn; pinned = true; }
            }
            if (! pinned) break;
        }
        float wsum = 0;
        for (int i = 0; i < count; ++i) if (! fixed[(size_t) i]) wsum += n.weights[(size_t) i];
        int given = 0, lastFlex = -1;
        for (int i = 0; i < count; ++i)
            if (! fixed[(size_t) i])
            {
                sizes[(size_t) i] = juce::jmax (0, juce::roundToInt ((float) left * n.weights[(size_t) i] / juce::jmax (0.0001f, wsum)));
                given += sizes[(size_t) i];
                lastFlex = i;
            }
        if (lastFlex >= 0) sizes[(size_t) lastFlex] += left - given; // rounding goes to the last flexible child
        int pos = n.horizontal ? area.getX() : area.getY();
        for (int i = 0; i < count; ++i)
        {
            const int s = sizes[(size_t) i];
            const auto r = n.horizontal ? juce::Rectangle<int> (pos, area.getY(), s, area.getHeight())
                                        : juce::Rectangle<int> (area.getX(), pos, area.getWidth(), s);
            layoutNode (*n.children[(size_t) i], r, minSize);
            pos += s + gutter;
        }
    }

    static void collectDividers (const Node& n, std::vector<Divider>& out)
    {
        if (n.isLeaf) return;
        for (size_t i = 0; i + 1 < n.children.size(); ++i)
        {
            const auto a = n.children[i]->bounds, b = n.children[i + 1]->bounds;
            Divider d;
            d.split = const_cast<Node*> (&n);
            d.index = (int) i;
            d.area = n.horizontal ? juce::Rectangle<int> (a.getRight(), n.bounds.getY(), b.getX() - a.getRight(), n.bounds.getHeight())
                                  : juce::Rectangle<int> (n.bounds.getX(), a.getBottom(), n.bounds.getWidth(), b.getY() - a.getBottom());
            out.push_back (d);
        }
        for (auto& c : n.children) collectDividers (*c, out);
    }

    static juce::ValueTree write (const Node& n)
    {
        if (n.isLeaf)
        {
            juce::ValueTree v ("Leaf");
            v.setProperty ("widgets", n.widgets.joinIntoString (","), nullptr);
            v.setProperty ("active", n.active, nullptr);
            v.setProperty ("collapsed", n.collapsed, nullptr);
            return v;
        }
        juce::ValueTree v ("Split");
        v.setProperty ("dir", n.horizontal ? "h" : "v", nullptr);
        for (size_t i = 0; i < n.children.size(); ++i)
        {
            auto c = write (*n.children[i]);
            c.setProperty ("weight", n.weights[i], nullptr);
            v.appendChild (c, nullptr);
        }
        return v;
    }

    static std::unique_ptr<Node> read (const juce::ValueTree& v, Node* parent)
    {
        auto n = std::make_unique<Node>();
        n->parent = parent;
        if (v.hasType ("Leaf"))
        {
            n->widgets = juce::StringArray::fromTokens (v.getProperty ("widgets").toString(), ",", {});
            n->widgets.removeEmptyStrings();
            n->active = (int) v.getProperty ("active", 0);
            n->collapsed = (bool) v.getProperty ("collapsed", false);
            return n;
        }
        n->isLeaf = false;
        n->horizontal = v.getProperty ("dir", "h").toString() == "h";
        for (auto c : v)
        {
            n->children.push_back (read (c, n.get()));
            n->weights.push_back (juce::jmax (0.0001f, (float) c.getProperty ("weight", 1.0f)));
        }
        return n;
    }
};

// Handy for building default layouts in code.
inline juce::ValueTree leaf (juce::StringArray ids, float weight, int active = 0)
{
    juce::ValueTree v ("Leaf");
    v.setProperty ("widgets", ids.joinIntoString (","), nullptr);
    v.setProperty ("active", active, nullptr);
    v.setProperty ("weight", weight, nullptr);
    return v;
}

inline juce::ValueTree split (bool horizontal, float weight, std::initializer_list<juce::ValueTree> kids)
{
    juce::ValueTree v ("Split");
    v.setProperty ("dir", horizontal ? "h" : "v", nullptr);
    v.setProperty ("weight", weight, nullptr);
    for (auto& k : kids) v.appendChild (k, nullptr);
    return v;
}

} // namespace ab::ui::dock
