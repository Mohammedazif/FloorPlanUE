#pragma once

#include "Model/BuildingModel.h"

namespace FloorPlan::Model
{
    /// A straight flight of steps filling a circulation run, in millimetres.
    struct StairFlight
    {
        Geometry::Vec2 Start;
        Geometry::Vec2 End;
        double WidthMm = 0.0;
        double RunLengthMm = 0.0;
        std::size_t StepCount = 0;
        double TreadDepthMm = 0.0;
        double RiserHeightMm = 0.0;

        /// True when the step count came from treads drawn in the plan, not from a rule.
        bool bFromDrawnTreads = false;
    };

    /// Divides a run into steps that climb exactly the distance between two storeys.
    class StairPlanner
    {
    public:
        static bool Plan(const CirculationRegion& region, double riseMm, StairFlight& flight);
    };
}
