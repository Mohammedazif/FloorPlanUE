#pragma once

#include "Model/BuildingModel.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Model
{
    /// One circulation region on a lower plan continuing into the plan above it.
    struct StoreyConnection
    {
        CirculationKind Kind = CirculationKind::None;
        std::size_t LowerCirculation = 0;
        std::size_t UpperCirculation = 0;
        std::size_t LowerRoom = 0;
        std::size_t UpperRoom = 0;

        /// Share of the lower footprint that also lies within the upper one.
        double OverlapFraction = 0.0;
    };

    /// Matches stairs and lifts across two stacked plans by the footprint they share.
    class StoreyLink
    {
    public:
        static std::vector<StoreyConnection> Between(const BuildingModel& lower,
                                                     const BuildingModel& upper);
    };
}
