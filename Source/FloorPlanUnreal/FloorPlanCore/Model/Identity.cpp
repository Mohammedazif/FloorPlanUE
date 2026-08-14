#include "Model/Identity.h"

#include "FloorPlanLimits.h"

#include <cmath>

namespace FloorPlan::Model
{
    namespace
    {
        const char* HexDigits = "0123456789abcdef";
    }

    Identity::Identity(const char* kind) : Hash(Limits::IdentityHashOffsetBasis)
    {
        for (const char* cursor = kind; cursor != nullptr && *cursor != '\0'; ++cursor)
        {
            Mix(static_cast<std::uint64_t>(static_cast<unsigned char>(*cursor)));
        }
    }

    void Identity::Mix(std::uint64_t value)
    {
        for (int byte = 0; byte < 8; ++byte)
        {
            Hash ^= (value >> (byte * 8)) & 0xffu;
            Hash *= Limits::IdentityHashPrime;
        }
    }

    std::int64_t Identity::Quantise(double millimetres)
    {
        if (!std::isfinite(millimetres))
        {
            return 0;
        }
        return static_cast<std::int64_t>(
            std::llround(millimetres / Limits::IdentityQuantumMm));
    }

    Identity& Identity::Add(double value)
    {
        Mix(static_cast<std::uint64_t>(Quantise(value)));
        return *this;
    }

    Identity& Identity::Add(const Geometry::Vec2& point)
    {
        Add(point.X);
        Add(point.Y);
        return *this;
    }

    Identity& Identity::Add(const std::string& text)
    {
        for (const char character : text)
        {
            Mix(static_cast<std::uint64_t>(static_cast<unsigned char>(character)));
        }
        return *this;
    }

    std::string Identity::ToHex() const
    {
        std::string text(16, '0');
        for (int index = 0; index < 16; ++index)
        {
            const int shift = (15 - index) * 4;
            text[static_cast<std::size_t>(index)] =
                HexDigits[(Hash >> shift) & 0xfu];
        }
        return text;
    }
}
