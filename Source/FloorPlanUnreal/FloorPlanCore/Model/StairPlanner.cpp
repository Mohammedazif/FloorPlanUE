#include "Model/StairPlanner.h"

#include "FloorPlanLimits.h"

#include <algorithm>
#include <cmath>

namespace FloorPlan::Model
{
    namespace
    {
        std::size_t StepsForRise(double riseMm)
        {
            const double raw = std::ceil(riseMm / Limits::TargetRiserHeightMm);
            if (!std::isfinite(raw) || raw < 1.0)
            {
                return 1;
            }
            return static_cast<std::size_t>(
                std::min(raw, static_cast<double>(Limits::MaxStepsPerFlight)));
        }

        bool RiserIsBuildable(double riserMm)
        {
            return riserMm >= Limits::MinRiserHeightMm && riserMm <= Limits::MaxRiserHeightMm;
        }
    }

    bool StairPlanner::Plan(const CirculationRegion& region, double riseMm, StairFlight& flight)
    {
        if (region.Kind != CirculationKind::Stair || !std::isfinite(riseMm) ||
            riseMm < Limits::MinRiserHeightMm)
        {
            return false;
        }

        const double run = (region.End - region.Start).Length();
        if (run < Limits::MinEdgeLengthMm || region.WidthMm < Limits::MinEdgeLengthMm)
        {
            return false;
        }

        std::size_t steps = region.DrawnTreads;
        bool fromDrawn = steps >= 2;
        if (fromDrawn && !RiserIsBuildable(riseMm / static_cast<double>(steps)))
        {
            // Tread lines can be hatching or a landing outline rather than one line per step.
            fromDrawn = false;
        }
        if (!fromDrawn)
        {
            steps = StepsForRise(riseMm);
        }

        flight.Start = region.Start;
        flight.End = region.End;
        flight.WidthMm = region.WidthMm;
        flight.RunLengthMm = run;
        flight.StepCount = steps;
        flight.TreadDepthMm = run / static_cast<double>(steps);
        flight.RiserHeightMm = riseMm / static_cast<double>(steps);
        flight.bFromDrawnTreads = fromDrawn;
        return true;
    }
}
