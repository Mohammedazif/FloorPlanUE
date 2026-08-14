#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfEntity.h"
#include "Dxf/DxfTag.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Dxf
{
    /// What one HATCH's boundary reduced to: closed loops of vertices, straight or bulged.
    struct DxfHatchBoundary
    {
        std::vector<DxfHatchLoop> Loops;

        /// Paths abandoned because a count or an edge type could not be read.
        std::size_t SkippedPaths = 0;
    };

    /// Reads the boundary of a HATCH: polyline paths, and edge paths of lines, arcs and splines.
    class DxfHatchReader
    {
    public:
        /// False only on a malformed count or an unreadable value; a skipped path is not failure.
        static bool Read(const std::vector<DxfTag>& tags, DxfHatchBoundary& boundary,
                         Diagnostic& diagnostic);
    };
}
