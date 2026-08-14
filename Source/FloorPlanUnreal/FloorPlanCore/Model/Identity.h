#pragma once

#include "Geometry/Vec2.h"

#include <cstdint>
#include <string>
#include <vector>

namespace FloorPlan::Model
{
    /// Deterministic element identity derived from quantised geometry, stable across reloads.
    class Identity
    {
    public:
        explicit Identity(const char* kind);

        Identity& Add(const Geometry::Vec2& point);
        Identity& Add(double value);
        Identity& Add(const std::string& text);

        std::uint64_t Value() const { return Hash; }

        std::string ToHex() const;

        static std::int64_t Quantise(double millimetres);

    private:
        void Mix(std::uint64_t value);

        std::uint64_t Hash;
    };
}
