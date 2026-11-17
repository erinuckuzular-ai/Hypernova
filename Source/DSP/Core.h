#pragma once

// The small set of helpers the DSP headers need, available with or without JUCE.
//
// HN_HAS_JUCE=1 (the default, used by the plug-in): every name below is JUCE's own, re-exported into `hn`,
// so the plug-in compiles exactly the same code it always has.
// HN_HAS_JUCE=0 (hardware builds): plain C++17 replacements that copy JUCE's definitions line for line,
// so a block compiled for a microcontroller computes the same numbers as the plug-in.
// Tests/DspCoreCheck.cpp checks the two builds against each other.

#ifndef HN_HAS_JUCE
 #define HN_HAS_JUCE 1
#endif

#if HN_HAS_JUCE
 #include <juce_dsp/juce_dsp.h>
#else
 #include <cstdint>
 #include <limits>
 #include <string>
 #include <type_traits>
#endif

#include <cmath>

namespace hn
{

#if HN_HAS_JUCE

using juce::jlimit;
using juce::jmax;
using juce::jmin;
using juce::isPositiveAndBelow;
template <typename FloatType> using MathConstants = juce::MathConstants<FloatType>;

using uint16 = juce::uint16;
using uint32 = juce::uint32;
using uint64 = juce::uint64;
using int64  = juce::int64;
using pointer_sized_uint = juce::pointer_sized_uint;

using Random = juce::Random;
using Text = juce::String; // names carried by tables and samples; never touched by the audio thread

#else

// juce_MathsFunctions.h: same argument order (lower, upper, value) and the same comparisons.
template <typename Type>
constexpr Type jmax (Type a, Type b)                    { return a < b ? b : a; }
template <typename Type>
constexpr Type jmax (Type a, Type b, Type c)            { return a < b ? (b < c ? c : b) : (a < c ? c : a); }
template <typename Type>
constexpr Type jmin (Type a, Type b)                    { return b < a ? b : a; }
template <typename Type>
constexpr Type jmin (Type a, Type b, Type c)            { return b < a ? (c < b ? c : b) : (c < a ? c : a); }

template <typename Type>
constexpr Type jlimit (Type lowerLimit, Type upperLimit, Type valueToConstrain) noexcept
{
    return valueToConstrain < lowerLimit ? lowerLimit
                                         : (upperLimit < valueToConstrain ? upperLimit : valueToConstrain);
}

template <typename Type1, typename Type2>
bool isPositiveAndBelow (Type1 valueToTest, Type2 upperLimit) noexcept
{
    return Type1() <= valueToTest && valueToTest < static_cast<Type1> (upperLimit);
}

template <typename Type>
bool isPositiveAndBelow (int valueToTest, Type upperLimit) noexcept
{
    return static_cast<unsigned int> (valueToTest) < static_cast<unsigned int> (upperLimit);
}

template <typename FloatType>
struct MathConstants
{
    static constexpr FloatType pi     = static_cast<FloatType> (3.141592653589793238L);
    static constexpr FloatType twoPi  = static_cast<FloatType> (2 * 3.141592653589793238L);
    static constexpr FloatType halfPi = static_cast<FloatType> (3.141592653589793238L / 2);
};

// Same widths and signedness as JUCE's typedefs (juce_MathsFunctions.h / juce_TargetPlatform.h).
using uint16 = unsigned short;
using uint32 = unsigned int;
using uint64 = unsigned long long;
using int64  = long long;
using pointer_sized_uint = std::conditional_t<sizeof (void*) == 8, unsigned long long, unsigned int>;

// juce::Random's 48-bit linear congruential generator, reproduced exactly (juce_Random.cpp), so seeded
// randomness (grain scatter, freeze phases) matches the plug-in sample for sample.
class Random
{
public:
    explicit Random (int64 seedValue) noexcept : seed (seedValue) {}
    // No default constructor: juce::Random's seeds itself from the clock, which can't be reproduced. Seed it.
    Random() = delete;

    void setSeed (int64 newSeed) noexcept { seed = newSeed; }

    int nextInt() noexcept
    {
        seed = (int64) (((((uint64) seed) * 0x5deece66dLL) + 11) & 0xffffffffffffLL);
        return (int) (seed >> 16);
    }

    int nextInt (int maxValue) noexcept
    {
        return (int) ((((unsigned int) nextInt()) * (uint64) maxValue) >> 32);
    }

    int64 nextInt64() noexcept
    {
        return (int64) ((((uint64) (unsigned int) nextInt()) << 32) | (uint64) (unsigned int) nextInt());
    }

    bool nextBool() noexcept { return (nextInt() & 0x40000000) != 0; }

    float nextFloat() noexcept
    {
        auto result = static_cast<float> (static_cast<uint32> (nextInt()))
                      / (static_cast<float> (std::numeric_limits<uint32>::max()) + 1.0f);
        return jmin (result, 1.0f - std::numeric_limits<float>::epsilon());
    }

    double nextDouble() noexcept
    {
        return static_cast<uint32> (nextInt()) / (std::numeric_limits<uint32>::max() + 1.0);
    }

private:
    int64 seed;
};

using Text = std::string;

#endif

// juce::Decibels::decibelsToGain with its default floor of -100 dB.
template <typename Type>
Type decibelsToGain (Type decibels, Type minusInfinityDb = Type (-100))
{
    return decibels > minusInfinityDb ? std::pow (Type (10.0), decibels * Type (0.05)) : Type();
}

// Fast xorshift noise used by the oscillators and noise generators: -1..1. Moved here from Synth.h
// (dsp::Rng there is now an alias of this) so every module can share it.
struct Rng
{
    uint32 s = 0x9e3779b9u;
    inline float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) s * (2.0f / 4294967295.0f) - 1.0f;
    }
};

} // namespace hn
