#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// Orbit: four captured sounds at the corners of a square, and a point that moves between them. Everything
// continuous is blended by how close the point is to each corner; anything that can only be one thing or
// another (a wavetable, a filter type, a switch) takes the value of the corner the point is nearest.
// The point can be dragged, automated, modulated, or left to travel a path of its own.
namespace ab
{

struct Orbit
{
    static constexpr int NumCorners = 4;   // A top left, B top right, C bottom left, D bottom right

    // A captured sound: one value per parameter, in the plugin's parameter order.
    struct Corners
    {
        std::array<std::vector<float>, NumCorners> values;
        std::array<bool, NumCorners> filled {};
        int howMany() const { int n = 0; for (bool f : filled) n += f ? 1 : 0; return n; }
    };

    static juce::StringArray cornerNames() { return { "A", "B", "C", "D" }; }
    static juce::StringArray pathNames() { return { "Still", "Circle", "Figure 8", "Drift", "Bounce" }; }
    enum Path { Still, Circle, Figure8, Drift, Bounce, NumPaths };

    // How much each corner is heard at this point. Empty corners give their share to the rest, so two
    // captured sounds morph along a line and three share the square without a silent quarter.
    static std::array<float, NumCorners> weights (float x, float y, const std::array<bool, NumCorners>& filled)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        y = juce::jlimit (0.0f, 1.0f, y);
        std::array<float, NumCorners> w { (1.0f - x) * (1.0f - y), x * (1.0f - y), (1.0f - x) * y, x * y };
        float total = 0;
        for (int i = 0; i < NumCorners; ++i) { if (! filled[(size_t) i]) w[(size_t) i] = 0.0f; total += w[(size_t) i]; }
        if (total < 1.0e-6f)
        {
            // Dead centre of an empty quarter: fall back to the nearest corner that has a sound in it.
            int best = -1;
            float bestDistance = 1.0e9f;
            static const float cx[] = { 0.0f, 1.0f, 0.0f, 1.0f }, cy[] = { 0.0f, 0.0f, 1.0f, 1.0f };
            for (int i = 0; i < NumCorners; ++i)
            {
                if (! filled[(size_t) i]) continue;
                const float d = (x - cx[i]) * (x - cx[i]) + (y - cy[i]) * (y - cy[i]);
                if (d < bestDistance) { bestDistance = d; best = i; }
            }
            if (best >= 0) w[(size_t) best] = 1.0f;
            return w;
        }
        for (auto& v : w) v /= total;
        return w;
    }

    static int strongest (const std::array<float, NumCorners>& w)
    {
        int best = 0;
        for (int i = 1; i < NumCorners; ++i) if (w[(size_t) i] > w[(size_t) best]) best = i;
        return best;
    }

    // A path the point travels on its own, as an offset from where it was left. One turn per cycle.
    struct Travel
    {
        float driftX = 0, driftY = 0, toX = 0, toY = 0;
        juce::Random rng { 0x0b17 };

        juce::Point<float> step (int path, double phase, float seconds)
        {
            const float p = (float) (phase - std::floor (phase));
            const float twoPi = juce::MathConstants<float>::twoPi;
            switch (path)
            {
                case Circle:  return { std::cos (twoPi * p), std::sin (twoPi * p) };
                case Figure8: return { std::sin (twoPi * p), std::sin (2.0f * twoPi * p) * 0.5f };
                case Bounce:  return { triangle (p), triangle ((float) std::fmod (phase * 0.73, 1.0)) };
                case Drift:
                {
                    // A slow wander: it always has somewhere to be, and takes its time getting there.
                    if (std::abs (driftX - toX) < 0.02f && std::abs (driftY - toY) < 0.02f)
                    {
                        toX = rng.nextFloat() * 2.0f - 1.0f;
                        toY = rng.nextFloat() * 2.0f - 1.0f;
                    }
                    const float k = juce::jlimit (0.0f, 1.0f, seconds * 1.5f);
                    driftX += (toX - driftX) * k;
                    driftY += (toY - driftY) * k;
                    return { driftX, driftY };
                }
                default: return {};
            }
        }

        static float triangle (float p) { return 4.0f * std::abs (p - std::floor (p + 0.5f)) - 1.0f; }
    };
};

} // namespace ab
