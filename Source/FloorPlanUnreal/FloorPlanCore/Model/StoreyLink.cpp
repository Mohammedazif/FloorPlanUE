#include "Model/StoreyLink.h"

#include "FloorPlanLimits.h"

#include <algorithm>

namespace FloorPlan::Model
{
    using Geometry::Loop;
    using Geometry::Vec2;

    namespace
    {
        const Loop* LoopOf(const BuildingModel& model, const CirculationRegion& region)
        {
            if (region.RoomIndex >= model.Rooms.size())
            {
                return nullptr;
            }
            const std::size_t loopIndex = model.Rooms[region.RoomIndex].LoopIndex;
            return loopIndex < model.Loops.size() ? &model.Loops[loopIndex] : nullptr;
        }

        /// Grid sample of the lower footprint, counting how much of it the upper one covers.
        double OverlapFraction(const Loop& lower, const Loop& upper)
        {
            const Vec2 low = lower.Minimum();
            const Vec2 high = lower.Maximum();
            const double width = high.X - low.X;
            const double height = high.Y - low.Y;
            if (width <= 0.0 || height <= 0.0)
            {
                return 0.0;
            }

            const std::size_t steps = Limits::CirculationOverlapSamples;
            std::size_t inside = 0;
            std::size_t covered = 0;
            for (std::size_t column = 0; column < steps; ++column)
            {
                for (std::size_t row = 0; row < steps; ++row)
                {
                    const double u = (static_cast<double>(column) + 0.5) /
                                     static_cast<double>(steps);
                    const double v =
                        (static_cast<double>(row) + 0.5) / static_cast<double>(steps);
                    const Vec2 point{low.X + width * u, low.Y + height * v};
                    if (!lower.Contains(point))
                    {
                        continue;
                    }
                    ++inside;
                    if (upper.Contains(point))
                    {
                        ++covered;
                    }
                }
            }
            if (inside == 0)
            {
                return 0.0;
            }
            return static_cast<double>(covered) / static_cast<double>(inside);
        }
    }

    std::vector<StoreyConnection> StoreyLink::Between(const BuildingModel& lower,
                                                       const BuildingModel& upper)
    {
        std::vector<StoreyConnection> connections;
        for (std::size_t below = 0; below < lower.Circulation.size(); ++below)
        {
            const CirculationRegion& source = lower.Circulation[below];
            const Loop* sourceLoop = LoopOf(lower, source);
            if (sourceLoop == nullptr)
            {
                continue;
            }

            std::size_t bestIndex = 0;
            double bestOverlap = 0.0;
            bool found = false;
            for (std::size_t above = 0; above < upper.Circulation.size(); ++above)
            {
                const CirculationRegion& target = upper.Circulation[above];
                if (target.Kind != source.Kind)
                {
                    continue;
                }
                const Loop* targetLoop = LoopOf(upper, target);
                if (targetLoop == nullptr)
                {
                    continue;
                }
                const double overlap = OverlapFraction(*sourceLoop, *targetLoop);
                if (overlap > bestOverlap)
                {
                    bestOverlap = overlap;
                    bestIndex = above;
                    found = true;
                }
            }

            if (!found || bestOverlap < Limits::MinCirculationOverlapFraction)
            {
                continue;
            }

            StoreyConnection connection;
            connection.Kind = source.Kind;
            connection.LowerCirculation = below;
            connection.UpperCirculation = bestIndex;
            connection.LowerRoom = source.RoomIndex;
            connection.UpperRoom = upper.Circulation[bestIndex].RoomIndex;
            connection.OverlapFraction = bestOverlap;
            connections.push_back(connection);
        }
        return connections;
    }
}
