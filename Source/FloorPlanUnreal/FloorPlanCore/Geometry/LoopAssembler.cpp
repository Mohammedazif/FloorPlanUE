#include "Geometry/LoopAssembler.h"

#include "FloorPlanLimits.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace FloorPlan::Geometry
{
    namespace
    {
        struct GridKey
        {
            std::int64_t X;
            std::int64_t Y;

            bool operator<(const GridKey& other) const
            {
                return X != other.X ? X < other.X : Y < other.Y;
            }
        };

        GridKey Snap(const Vec2& point)
        {
            const double cell = Limits::VertexWeldToleranceMm;
            return GridKey{static_cast<std::int64_t>(std::llround(point.X / cell)),
                           static_cast<std::int64_t>(std::llround(point.Y / cell))};
        }

        struct Endpoint
        {
            std::size_t SegmentIndex;
            bool AtStart;
        };

        bool SamePoint(const Vec2& left, const Vec2& right)
        {
            return (left - right).Length() <= Limits::VertexWeldToleranceMm;
        }
    }

    bool LoopAssembler::Assemble(const std::vector<Segment>& segments, std::vector<Loop>& loops,
                                  AssemblyReport& report, Diagnostic& diagnostic)
    {
        report = AssemblyReport{};
        if (segments.size() > Limits::MaxEntityCount)
        {
            diagnostic.Code = DiagnosticCode::EntityLimitExceeded;
            diagnostic.Message = std::to_string(segments.size());
            return false;
        }

        std::map<GridKey, std::vector<Endpoint>> buckets;
        std::vector<bool> used(segments.size(), false);
        for (std::size_t index = 0; index < segments.size(); ++index)
        {
            const Segment& segment = segments[index];
            if ((segment.End - segment.Start).Length() < Limits::MinEdgeLengthMm)
            {
                used[index] = true;
                ++report.DiscardedSegments;
                continue;
            }
            buckets[Snap(segment.Start)].push_back(Endpoint{index, true});
            buckets[Snap(segment.End)].push_back(Endpoint{index, false});
        }
        report.WeldedVertices = buckets.size();

        for (std::size_t seed = 0; seed < segments.size(); ++seed)
        {
            if (used[seed])
            {
                continue;
            }

            std::vector<LoopVertex> vertices;
            std::size_t current = seed;
            bool forward = true;
            const Vec2 origin = segments[seed].Start;
            bool closed = false;

            while (vertices.size() <= Limits::MaxVerticesPerPolyline)
            {
                used[current] = true;
                const Segment& segment = segments[current];
                const Vec2 from = forward ? segment.Start : segment.End;
                const Vec2 to = forward ? segment.End : segment.Start;
                vertices.push_back(
                    LoopVertex{from, forward ? segment.Bulge : -segment.Bulge});

                if (vertices.size() > 2 && SamePoint(to, origin))
                {
                    closed = true;
                    break;
                }

                const auto bucket = buckets.find(Snap(to));
                if (bucket == buckets.end())
                {
                    break;
                }
                std::size_t next = segments.size();
                bool nextForward = true;
                for (const Endpoint& candidate : bucket->second)
                {
                    if (used[candidate.SegmentIndex])
                    {
                        continue;
                    }
                    const Segment& other = segments[candidate.SegmentIndex];
                    const Vec2& join = candidate.AtStart ? other.Start : other.End;
                    if (!SamePoint(join, to))
                    {
                        continue;
                    }
                    next = candidate.SegmentIndex;
                    nextForward = candidate.AtStart;
                    break;
                }
                if (next == segments.size())
                {
                    break;
                }
                current = next;
                forward = nextForward;
            }

            if (closed && vertices.size() >= 3)
            {
                loops.emplace_back(std::move(vertices));
                ++report.ClosedLoops;
            }
            else
            {
                report.DiscardedSegments += vertices.size();
            }
        }
        return true;
    }
}
