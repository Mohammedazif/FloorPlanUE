#pragma once

#include "Dxf/DxfEntity.h"
#include "Dxf/DxfTag.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace FloorPlan::Dxf
{
    /// Walks a hatch's tags once, so the codes it reuses stay unambiguous by position alone.
    class DxfHatchCursor
    {
    public:
        explicit DxfHatchCursor(const std::vector<DxfTag>& tags) : Tags(tags) {}

        bool AtEnd() const { return Index >= Tags.size(); }
        const DxfTag& Peek() const { return Tags[Index]; }
        int Code() const { return AtEnd() ? -1 : Tags[Index].Code; }
        void Step() { ++Index; }

        bool SeekCode(int code);

        /// Reads a count, returning one past the limit when it is negative or absurd.
        std::size_t Count(std::size_t limit);

    private:
        const std::vector<DxfTag>& Tags;
        std::size_t Index = 0;
    };

    /// Builds one boundary edge of a hatch: a line, a circular or elliptic arc, or a spline.
    class DxfHatchEdges
    {
    public:
        static bool ReadLine(DxfHatchCursor& cursor, DxfHatchLoop& loop);
        static bool ReadCircularArc(DxfHatchCursor& cursor, DxfHatchLoop& loop);
        static bool ReadEllipticArc(DxfHatchCursor& cursor, DxfHatchLoop& loop);
        static bool ReadSpline(DxfHatchCursor& cursor, DxfHatchLoop& loop);
    };
}
