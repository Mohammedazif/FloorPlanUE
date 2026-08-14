#pragma once

#include "Dxf/DxfEntity.h"
#include "Model/BuildingModel.h"
#include "Model/FloorPlanCompiler.h"

#include <vector>

namespace FloorPlan::Model
{
    /// Finds the rooms that carry movement between storeys and the axis their run follows.
    class CirculationDetector
    {
    public:
        static void Detect(const std::vector<Dxf::DxfEntity>& entities,
                           const CompilerOptions& options, BuildingModel& model);
    };
}
