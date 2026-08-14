#pragma once

#include "Diagnostic.h"
#include "Geometry/Loop.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Geometry
{
    struct Segment
    {
        Vec2 Start;
        Vec2 End;
        double Bulge = 0.0;
    };

    struct AssemblyReport
    {
        std::size_t ClosedLoops = 0;
        std::size_t DiscardedSegments = 0;
        std::size_t WeldedVertices = 0;
    };

    /// Chains loose segments into closed loops by welding coincident endpoints.
    class LoopAssembler
    {
    public:
        /// Open chains and stubs are discarded rather than closed by guesswork.
        static bool Assemble(const std::vector<Segment>& segments, std::vector<Loop>& loops,
                             AssemblyReport& report, Diagnostic& diagnostic);
    };
}
