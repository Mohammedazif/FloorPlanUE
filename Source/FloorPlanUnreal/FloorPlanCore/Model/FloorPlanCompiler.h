#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfDocument.h"
#include "Model/BuildingModel.h"

#include <string>
#include <vector>

namespace FloorPlan::Model
{
    enum class WallConvention
    {
        /// Walls are drawn as paired faces; rooms are the voids between them.
        DoubleLine,
        /// Walls are drawn as single centrelines; rooms are the faces they enclose.
        SingleLine
    };

    struct CompilerOptions
    {
        WallConvention Convention = WallConvention::DoubleLine;

        /// Used when the file declares no $INSUNITS; R12 never declares them.
        double MillimetresPerUnit = 1.0;
        double WallHeightMm = 0.0;

        /// Mixed into every element id so identical storeys of one building stay distinguishable.
        std::string StoreyKey;

        /// Layers that may contribute wall geometry. Empty accepts every layer.
        std::vector<std::string> WallLayers;

        std::vector<std::string> DoorBlockPrefixes{"DOOR", "DR_", "D_"};
        std::vector<std::string> WindowBlockPrefixes{"WIN", "WINDOW", "W_"};

        /// Layers whose lines are stair treads. Their count and angle set the run's direction.
        std::vector<std::string> StairLayers{"A-FLOR-STRS", "A-STRS", "STAIR", "STAIRS"};

        std::vector<std::string> StairNamePrefixes{"STAIR", "STAIRCASE", "STR"};
        std::vector<std::string> LiftNamePrefixes{"LIFT", "ELEV"};

        std::vector<std::string> ColumnLayers{"A-COLS", "S-COLS", "COLUMN", "COLS"};
        std::vector<std::string> GridLayers{"A-GRID", "S-GRID", "GRID"};

        /// A dimension disagreeing with its own extension lines by more than this is flagged.
        double DimensionToleranceMm = 1.0;
    };

    /// Compiles a parsed DXF document into rooms, walls and openings with stable identity.
    class FloorPlanCompiler
    {
    public:
        static bool Compile(const Dxf::DxfDocument& document, const CompilerOptions& options,
                            BuildingModel& model, Diagnostic& diagnostic);
    };
}
