#pragma once

#include "Dxf/DxfEntity.h"
#include "Model/BuildingModel.h"
#include "Model/FloorPlanCompiler.h"

#include <vector>

namespace FloorPlan::Model
{
    /// Pulls out everything a plan carries besides its rooms and walls.
    class AnnotationExtractor
    {
    public:
        /// Geometry is read after block expansion; placements are model space as it was drawn,
        /// because expansion consumes the INSERTs a furniture schedule is made of.
        static void Extract(const std::vector<Dxf::DxfEntity>& expanded,
                            const std::vector<Dxf::DxfEntity>& placements,
                            const CompilerOptions& options, BuildingModel& model);
    };
}
