#pragma once

#include "Diagnostic.h"
#include "Geometry/LoopAssembler.h"
#include "Model/BuildingModel.h"

#include <string>
#include <vector>

namespace FloorPlan::Model
{
    /// Recovers rooms from a centreline drawing, where the walls themselves bound each room.
    class SingleLineRoomExtractor
    {
    public:
        static bool Extract(const std::vector<Geometry::Segment>& loose, double wallHeight,
                            const std::string& storeyKey, BuildingModel& model,
                            Diagnostic& diagnostic);
    };
}
