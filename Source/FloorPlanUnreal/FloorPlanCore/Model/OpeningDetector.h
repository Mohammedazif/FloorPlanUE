#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfEntity.h"
#include "Model/BuildingModel.h"
#include "Model/FloorPlanCompiler.h"

#include <vector>

namespace FloorPlan::Model
{
    /// Turns the door and window blocks a plan places into openings hosted by the nearest wall.
    class OpeningDetector
    {
    public:
        static bool Detect(const std::vector<Dxf::DxfEntity>& placements,
                           const CompilerOptions& options, BuildingModel& model,
                           Diagnostic& diagnostic);
    };
}
