#include "Walls/WallJunctionResolver.h"

#include "FloorPlanLimits.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace FloorPlan::Walls
{
    using Geometry::Cross;
    using Geometry::Vec2;

    namespace
    {
        struct Ray
        {
            Vec2 Origin;
            Vec2 Direction;
            double Length = 0.0;
        };

        bool ToRay(const WallCandidate& wall, Ray& ray)
        {
            if (wall.IsCurved())
            {
                return false;
            }
            const Vec2 span = wall.End - wall.Start;
            const double length = span.Length();
            if (length < Limits::MinEdgeLengthMm)
            {
                return false;
            }
            ray.Origin = wall.Start;
            ray.Direction = Vec2{span.X / length, span.Y / length};
            ray.Length = length;
            return true;
        }

        bool CrossingParameter(const Ray& self, const Ray& other, double& selfParam,
                               double& otherParam)
        {
            const double denominator = Cross(self.Direction, other.Direction);
            if (std::fabs(denominator) < Limits::MaxWallPairSineDeviation)
            {
                return false;
            }
            const Vec2 offset = other.Origin - self.Origin;
            selfParam = Cross(offset, other.Direction) / denominator;
            otherParam = Cross(offset, self.Direction) / denominator;
            return true;
        }

        /// Exactly one wall of a junction owns the corner, so the two never overlap.
        bool OwnsCorner(const std::vector<Ray>& rays, std::size_t self, std::size_t other)
        {
            if (rays[self].Length != rays[other].Length)
            {
                return rays[self].Length > rays[other].Length;
            }
            return self < other;
        }

        double DeltaAtEnd(const std::vector<WallCandidate>& walls, const std::vector<Ray>& rays,
                          const std::vector<bool>& usable, std::size_t index, bool atStart)
        {
            const Ray& self = rays[index];
            const double endParam = atStart ? 0.0 : self.Length;
            const double outward = atStart ? -1.0 : 1.0;
            const double reach = Limits::MaxWallThicknessMm * 2.0;
            const double slack = Limits::MaxWallThicknessMm;

            double nearest = std::numeric_limits<double>::max();
            double delta = 0.0;
            for (std::size_t other = 0; other < walls.size(); ++other)
            {
                if (other == index || !usable[other])
                {
                    continue;
                }
                double selfParam = 0.0;
                double otherParam = 0.0;
                if (!CrossingParameter(self, rays[other], selfParam, otherParam))
                {
                    continue;
                }

                const double beyond = (selfParam - endParam) * outward;
                if (beyond < -slack || beyond > reach)
                {
                    continue;
                }
                if (otherParam < -slack || otherParam > rays[other].Length + slack)
                {
                    continue;
                }
                if (std::fabs(beyond) >= nearest)
                {
                    continue;
                }
                nearest = std::fabs(beyond);
                const double half = walls[other].ThicknessMm * 0.5;
                delta = OwnsCorner(rays, index, other) ? beyond + half : beyond - half;
            }
            return delta;
        }
    }

    void WallJunctionResolver::Close(std::vector<WallCandidate>& walls, JunctionReport& report)
    {
        report = JunctionReport{};

        std::vector<Ray> rays(walls.size());
        std::vector<bool> usable(walls.size(), false);
        for (std::size_t index = 0; index < walls.size(); ++index)
        {
            usable[index] = ToRay(walls[index], rays[index]);
        }

        std::vector<double> startDelta(walls.size(), 0.0);
        std::vector<double> endDelta(walls.size(), 0.0);
        for (std::size_t index = 0; index < walls.size(); ++index)
        {
            if (!usable[index])
            {
                continue;
            }
            startDelta[index] = DeltaAtEnd(walls, rays, usable, index, true);
            endDelta[index] = DeltaAtEnd(walls, rays, usable, index, false);
        }

        for (std::size_t index = 0; index < walls.size(); ++index)
        {
            if (!usable[index])
            {
                continue;
            }
            const Vec2& direction = rays[index].Direction;
            WallCandidate& wall = walls[index];

            const double start = startDelta[index];
            const double end = endDelta[index];
            wall.Start = Vec2{wall.Start.X - direction.X * start,
                              wall.Start.Y - direction.Y * start};
            wall.End = Vec2{wall.End.X + direction.X * end, wall.End.Y + direction.Y * end};

            for (const double delta : {start, end})
            {
                if (delta > 0.0)
                {
                    ++report.EndsExtended;
                    report.LongestExtensionMm = std::max(report.LongestExtensionMm, delta);
                }
                else if (delta < 0.0)
                {
                    ++report.EndsTrimmed;
                }
                else
                {
                    ++report.EndsLeftFree;
                }
            }
        }
    }
}
