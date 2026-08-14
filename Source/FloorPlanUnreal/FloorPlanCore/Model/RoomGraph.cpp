#include "Model/RoomGraph.h"

#include "FloorPlanLimits.h"
#include "Geometry/Bulge.h"

#include <algorithm>
#include <cmath>

namespace FloorPlan::Model
{
    using Geometry::BulgeArc;
    using Geometry::Loop;
    using Geometry::Vec2;

    namespace
    {
        struct Probe
        {
            Vec2 Position;
            Vec2 Normal;
        };

        /// Point and outward normal a fraction of the way along a wall, straight or curved.
        Probe SampleWall(const Wall& wall, double fraction)
        {
            const BulgeArc arc = Geometry::Bulge::Resolve(wall.Start, wall.End, wall.Bulge);
            if (arc.IsStraight)
            {
                const Vec2 span = wall.End - wall.Start;
                const double length = span.Length();
                const Vec2 direction =
                    length > 0.0 ? Vec2{span.X / length, span.Y / length} : Vec2{1.0, 0.0};
                return Probe{wall.Start + span * fraction, direction.PerpendicularCcw()};
            }

            const double angle = arc.StartAngle + arc.IncludedAngle * fraction;
            const Vec2 radial{std::cos(angle), std::sin(angle)};
            return Probe{Vec2{arc.Center.X + arc.Radius * radial.X,
                              arc.Center.Y + arc.Radius * radial.Y},
                         radial};
        }

        void Order(std::size_t& first, std::size_t& second)
        {
            if (first > second)
            {
                std::swap(first, second);
            }
        }

        void AddOnce(std::vector<std::size_t>& values, std::size_t value)
        {
            if (std::find(values.begin(), values.end(), value) == values.end())
            {
                values.push_back(value);
            }
        }
    }

    bool RoomLink::IsExterior() const
    {
        return FirstRoom == RoomGraph::Outside || SecondRoom == RoomGraph::Outside;
    }

    RoomGraph RoomGraph::Build(const BuildingModel& model)
    {
        RoomGraph graph;

        for (std::size_t wallIndex = 0; wallIndex < model.Walls.size(); ++wallIndex)
        {
            const Wall& wall = model.Walls[wallIndex];
            const double reach = wall.ThicknessMm * 0.5 + Limits::RoomProbeOffsetMm;

            for (std::size_t step = 0; step < Limits::RoomProbesPerWall; ++step)
            {
                const double fraction = (static_cast<double>(step) + 0.5) /
                                        static_cast<double>(Limits::RoomProbesPerWall);
                const Probe probe = SampleWall(wall, fraction);
                const Vec2 offset = probe.Normal * reach;
                std::size_t left = model.SmallestRoomContaining(probe.Position + offset);
                std::size_t right = model.SmallestRoomContaining(probe.Position - offset);
                if (left == right)
                {
                    continue;
                }
                Order(left, right);

                const auto existing =
                    std::find_if(graph.Edges.begin(), graph.Edges.end(),
                                 [left, right](const RoomLink& link) {
                                     return link.FirstRoom == left && link.SecondRoom == right;
                                 });
                if (existing == graph.Edges.end())
                {
                    RoomLink link;
                    link.FirstRoom = left;
                    link.SecondRoom = right;
                    link.WallIndices.push_back(wallIndex);
                    graph.Edges.push_back(std::move(link));
                }
                else
                {
                    AddOnce(existing->WallIndices, wallIndex);
                }
            }
        }

        for (RoomLink& link : graph.Edges)
        {
            for (std::size_t openingIndex = 0; openingIndex < model.Openings.size();
                 ++openingIndex)
            {
                const std::string& host = model.Openings[openingIndex].HostWallId;
                for (const std::size_t wallIndex : link.WallIndices)
                {
                    if (model.Walls[wallIndex].Id == host)
                    {
                        link.OpeningIndices.push_back(openingIndex);
                        break;
                    }
                }
            }
        }
        return graph;
    }

    std::vector<std::size_t> RoomGraph::Neighbours(std::size_t room) const
    {
        std::vector<std::size_t> found;
        for (const RoomLink& link : Edges)
        {
            if (!link.IsTraversable())
            {
                continue;
            }
            if (link.FirstRoom == room)
            {
                AddOnce(found, link.SecondRoom);
            }
            else if (link.SecondRoom == room)
            {
                AddOnce(found, link.FirstRoom);
            }
        }
        std::sort(found.begin(), found.end());
        return found;
    }

    std::size_t RoomGraph::ExteriorLinkCount() const
    {
        std::size_t count = 0;
        for (const RoomLink& link : Edges)
        {
            if (link.IsExterior())
            {
                ++count;
            }
        }
        return count;
    }
}
