#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#endif

// Motion that behaves like things in the world: springs you can grab and redirect mid-flight, that pick up
// the speed you let go at, and edges that give a little before they stop. Everything here moves from where
// something is on screen now, never from where it was meant to be, so nothing ever jumps.
namespace ab::ui::motion
{

// macOS "Reduce motion" (System Settings > Accessibility > Display): moves then happen at once.
// Checked at most every couple of seconds; HYPERNOVA_REDUCE_MOTION=1 forces it (tests, screenshots).
inline bool systemReducesMotion()
{
    static bool cached = false;
    static double checkedAt = -1.0e9;
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - checkedAt < 2000.0) return cached;
    checkedAt = now;
    cached = juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_REDUCE_MOTION", {}) == "1";
   #if JUCE_MAC
    Boolean valid = false;
    if (! cached) cached = CFPreferencesGetAppBooleanValue (CFSTR ("reduceMotion"), CFSTR ("com.apple.universalaccess"), &valid) && valid;
   #endif
    return cached;
}

// A spring described the way designers think about it: how quickly it gets there (response, in seconds)
// and how much it overshoots (damping: 1 settles without any bounce, below 1 bounces).
struct Spring
{
    float value = 0.0f, velocity = 0.0f, target = 0.0f;
    float precision = 0.25f; // close enough to call it there (pixels by default)

    void snap (float v) { value = target = v; velocity = 0.0f; }

    // Advances by dt seconds. Returns true while it's still moving.
    bool step (float dt, float response = 0.35f, float damping = 1.0f)
    {
        const float omega = juce::MathConstants<float>::twoPi / juce::jmax (0.02f, response);
        const float stiffness = omega * omega, friction = 2.0f * damping * omega;
        // Small fixed sub-steps keep it stable however irregular the frames are.
        for (float left = juce::jmin (dt, 0.1f); left > 0.0f; left -= 1.0f / 240.0f)
        {
            const float h = juce::jmin (left, 1.0f / 240.0f);
            velocity += (stiffness * (target - value) - friction * velocity) * h;
            value += velocity * h;
        }
        if (std::abs (target - value) < precision && std::abs (velocity) < precision * 24.0f) { value = target; velocity = 0.0f; return false; }
        return true;
    }
};

// Where a flick would come to rest, the way scrolling decelerates (Apple's projection; 0.998 is normal scrolling).
inline float project (float velocityPerSecond, float decelerationRate = 0.998f)
{
    return velocityPerSecond / 1000.0f * decelerationRate / (1.0f - decelerationRate);
}

// Past an edge the content follows less and less the further you pull, so the edge feels soft, not frozen.
inline float rubberband (float overshoot, float dimension, float constant = 0.55f)
{
    if (dimension <= 0.0f) return 0.0f;
    return overshoot * dimension * constant / (dimension + constant * std::abs (overshoot));
}

// Pointer speed from the last few moves, in pixels per second.
class VelocityTracker
{
public:
    void reset() { count = 0; }
    void add (juce::Point<float> p, double timeMs = juce::Time::getMillisecondCounterHiRes())
    {
        samples[(size_t) (count % n)] = { p, timeMs };
        ++count;
    }
    juce::Point<float> velocity (double nowMs = juce::Time::getMillisecondCounterHiRes()) const
    {
        if (count < 2) return {};
        const auto& last = samples[(size_t) ((count - 1) % n)];
        if (nowMs - last.t > 80.0) return {}; // held still before letting go: no throw
        // Oldest sample within the last 100 ms.
        const Sample* first = &last;
        for (int i = 2; i <= juce::jmin (count, n); ++i)
        {
            const auto& s = samples[(size_t) ((count - i) % n)];
            if (last.t - s.t > 100.0) break;
            first = &s;
        }
        const double dt = (last.t - first->t) / 1000.0;
        if (dt < 0.004) return {};
        return (last.p - first->p) / (float) dt;
    }

private:
    struct Sample { juce::Point<float> p; double t = 0; };
    static constexpr int n = 8;
    std::array<Sample, n> samples {};
    int count = 0;
};

// Moves components to new bounds on springs. Asking for a new place mid-flight carries on from where the
// component is, at the speed it's going, so a change of mind never jerks.
class BoundsSprings : private juce::Timer
{
public:
    float response = 0.34f, damping = 1.0f;
    std::function<void()> onFrame; // after each frame's moves

    void moveTo (juce::Component& c, juce::Rectangle<int> target, juce::Point<float> releaseVelocity = {})
    {
        if (systemReducesMotion()) { stop (c); c.setBounds (target); return; }
        auto it = moving.find (&c);
        if (it == moving.end())
        {
            const auto b = c.getBounds().toFloat();
            State s;
            s.x.snap (b.getX()); s.y.snap (b.getY()); s.r.snap (b.getRight()); s.b.snap (b.getBottom());
            s.x.velocity = s.r.velocity = releaseVelocity.x;
            s.y.velocity = s.b.velocity = releaseVelocity.y;
            it = moving.emplace (&c, s).first;
            it->second.watcher = std::make_unique<Watcher> (*this, c);
        }
        auto& s = it->second;
        s.bounce = releaseVelocity.getDistanceFromOrigin() > 300.0f; // thrown: a touch of life when it lands
        s.x.target = (float) target.getX(); s.y.target = (float) target.getY();
        s.r.target = (float) target.getRight(); s.b.target = (float) target.getBottom();
        if (! isTimerRunning()) { lastTick = juce::Time::getMillisecondCounterHiRes(); startTimerHz (60); }
    }

    void stop (juce::Component& c) { moving.erase (&c); }
    bool isMoving (juce::Component& c) const { return moving.count (&c) > 0; }
    bool anyMoving() const { return ! moving.empty(); }

    // Finishes every move at once (e.g. before measuring in tests).
    void finish()
    {
        for (auto& [c, s] : moving) c->setBounds (juce::Rectangle<float>::leftTopRightBottom (s.x.target, s.y.target, s.r.target, s.b.target).toNearestInt());
        moving.clear();
        stopTimer();
    }

private:
    struct Watcher : juce::ComponentListener
    {
        Watcher (BoundsSprings& o, juce::Component& c) : owner (o), comp (&c) { c.addComponentListener (this); }
        ~Watcher() override { if (comp != nullptr) comp->removeComponentListener (this); }
        void componentBeingDeleted (juce::Component& c) override
        {
            c.removeComponentListener (this);
            comp = nullptr;
            juce::MessageManager::callAsync ([o = &owner, p = &c, alive = owner.alive] { if (*alive) o->moving.erase (p); });
        }
        BoundsSprings& owner;
        juce::Component* comp;
    };
    struct State
    {
        Spring x, y, r, b;
        bool bounce = false;
        std::shared_ptr<Watcher> watcher;
    };
    std::map<juce::Component*, State> moving;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    double lastTick = 0;

public:
    ~BoundsSprings() override { *alive = false; }

private:
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float dt = (float) juce::jlimit (0.001, 0.05, (now - lastTick) / 1000.0);
        lastTick = now;
        for (auto it = moving.begin(); it != moving.end();)
        {
            auto& s = it->second;
            if (s.watcher == nullptr || s.watcher->comp == nullptr) { it = moving.erase (it); continue; }
            const float d = s.bounce ? 0.82f : damping;
            bool going = false;
            for (auto* sp : { &s.x, &s.y, &s.r, &s.b }) going |= sp->step (dt, response, d);
            it->first->setBounds (juce::Rectangle<float>::leftTopRightBottom (s.x.value, s.y.value, s.r.value, s.b.value).toNearestInt());
            it = going ? std::next (it) : moving.erase (it);
        }
        if (onFrame) onFrame();
        if (moving.empty()) stopTimer();
    }
};

} // namespace ab::ui::motion
