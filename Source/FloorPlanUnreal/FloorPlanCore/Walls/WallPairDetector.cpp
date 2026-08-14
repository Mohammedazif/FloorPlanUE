#include "Walls/WallPairDetector.h"

#include "FloorPlanLimits.h"
#include "Geometry/Bulge.h"

#include <algorithm>
#include <cmath>

namespace FloorPlan::Walls
{
    using Geometry::BulgeArc;
    using Geometry::Cross;
    using Geometry::Dot;
    using Geometry::Segment;
    using Geometry::Vec2;

    namespace
    {
        constexpr double TwoPi = 6.28318530717958647692;

        struct Line
        {
            Vec2 Origin;
            Vec2 Direction;
            double Length = 0.0;
        };

        struct Arc
        {
            Vec2 Center;
            double Radius = 0.0;
            double StartAngle = 0.0;
            double Sweep = 0.0;
        };

        bool ToLine(const Segment& segment, Line& line)
        {
            const Vec2 span = segment.End - segment.Start;
            const double length = span.Length();
            if (length < Limits::MinEdgeLengthMm)
            {
                return false;
            }
            line.Origin = segment.Start;
            line.Direction = Vec2{span.X / length, span.Y / length};
            line.Length = length;
            return true;
        }

        bool ToArc(const Segment& segment, Arc& arc)
        {
            const BulgeArc resolved =
                Geometry::Bulge::Resolve(segment.Start, segment.End, segment.Bulge);
            if (resolved.IsStraight || resolved.Radius < Limits::MinEdgeLengthMm ||
                resolved.Radius > Limits::MaxRadiusMm)
            {
                return false;
            }
            const bool counterClockwise = resolved.IncludedAngle >= 0.0;
            arc.Center = resolved.Center;
            arc.Radius = resolved.Radius;
            arc.StartAngle = counterClockwise ? resolved.StartAngle
                                              : resolved.StartAngle + resolved.IncludedAngle;
            arc.Sweep = std::fabs(resolved.IncludedAngle);
            return arc.Sweep > 0.0;
        }

        double Project(const Line& line, const Vec2& point)
        {
            return Dot(point - line.Origin, line.Direction);
        }

        double PerpendicularOffset(const Line& line, const Vec2& point)
        {
            return Cross(line.Direction, point - line.Origin);
        }

        Vec2 OnCircle(const Vec2& centre, double radius, double angle)
        {
            return Vec2{centre.X + radius * std::cos(angle), centre.Y + radius * std::sin(angle)};
        }

        /// Longest run of angles both arcs cover, measured counter-clockwise from a's start.
        bool SharedSweep(const Arc& a, const Arc& b, double& startAngle, double& sweep)
        {
            double delta = std::fmod(b.StartAngle - a.StartAngle, TwoPi);
            if (delta < 0.0)
            {
                delta += TwoPi;
            }

            double bestLow = 0.0;
            double bestSweep = 0.0;
            for (const double offset : {delta, delta - TwoPi})
            {
                const double low = std::max(0.0, offset);
                const double high = std::min(a.Sweep, offset + b.Sweep);
                if (high - low > bestSweep)
                {
                    bestLow = low;
                    bestSweep = high - low;
                }
            }
            if (bestSweep <= 0.0)
            {
                return false;
            }
            startAngle = a.StartAngle + bestLow;
            sweep = bestSweep;
            return true;
        }

        bool PairLines(const Segment& second, const Line& a, const Line& b, WallCandidate& wall,
                       PairingReport& report)
        {
            if (std::fabs(Cross(a.Direction, b.Direction)) > Limits::MaxWallPairSineDeviation)
            {
                ++report.RejectedByAngle;
                return false;
            }

            const double offsetStart = PerpendicularOffset(a, second.Start);
            const double offsetEnd = PerpendicularOffset(a, second.End);
            if (std::fabs(offsetStart - offsetEnd) > Limits::VertexWeldToleranceMm)
            {
                ++report.RejectedByAngle;
                return false;
            }

            const double gap = std::fabs(0.5 * (offsetStart + offsetEnd));
            if (gap < Limits::MinWallThicknessMm || gap > Limits::MaxWallThicknessMm)
            {
                ++report.RejectedByGap;
                return false;
            }

            double low = Project(a, second.Start);
            double high = Project(a, second.End);
            if (low > high)
            {
                std::swap(low, high);
            }
            const double overlapLow = std::max(0.0, low);
            const double overlapHigh = std::min(a.Length, high);
            if (overlapHigh - overlapLow < Limits::MinWallOverlapMm)
            {
                ++report.RejectedByOverlap;
                return false;
            }

            const double signedGap = 0.5 * (offsetStart + offsetEnd);
            const Vec2 normal = a.Direction.PerpendicularCcw();
            const double shift = signedGap > 0.0 ? gap * 0.5 : -gap * 0.5;

            wall.Start = Vec2{a.Origin.X + a.Direction.X * overlapLow + normal.X * shift,
                              a.Origin.Y + a.Direction.Y * overlapLow + normal.Y * shift};
            wall.End = Vec2{a.Origin.X + a.Direction.X * overlapHigh + normal.X * shift,
                            a.Origin.Y + a.Direction.Y * overlapHigh + normal.Y * shift};
            wall.ThicknessMm = gap;
            return true;
        }

        bool PairArcs(const Arc& a, const Arc& b, WallCandidate& wall, PairingReport& report)
        {
            if ((a.Center - b.Center).Length() > Limits::MaxWallArcCentreOffsetMm)
            {
                ++report.RejectedByAngle;
                return false;
            }

            const double gap = std::fabs(a.Radius - b.Radius);
            if (gap < Limits::MinWallThicknessMm || gap > Limits::MaxWallThicknessMm)
            {
                ++report.RejectedByGap;
                return false;
            }

            double startAngle = 0.0;
            double sweep = 0.0;
            if (!SharedSweep(a, b, startAngle, sweep))
            {
                ++report.RejectedByOverlap;
                return false;
            }
            if (sweep > Limits::MaxWallArcSweepRadians)
            {
                ++report.RejectedByAngle;
                return false;
            }

            const double radius = 0.5 * (a.Radius + b.Radius);
            if (radius * sweep < Limits::MinWallOverlapMm)
            {
                ++report.RejectedByOverlap;
                return false;
            }

            const Vec2 centre{0.5 * (a.Center.X + b.Center.X), 0.5 * (a.Center.Y + b.Center.Y)};
            wall.Start = OnCircle(centre, radius, startAngle);
            wall.End = OnCircle(centre, radius, startAngle + sweep);
            wall.Bulge = std::tan(sweep * 0.25);
            wall.ThicknessMm = gap;
            return true;
        }
    }

    bool WallCandidate::IsCurved() const
    {
        return !Geometry::Bulge::IsStraight(Bulge);
    }

    void WallPairDetector::Detect(const std::vector<Segment>& faces,
                                   std::vector<WallCandidate>& walls, PairingReport& report)
    {
        report = PairingReport{};

        std::vector<Line> lines(faces.size());
        std::vector<Arc> arcs(faces.size());
        std::vector<bool> straight(faces.size(), false);
        std::vector<bool> curved(faces.size(), false);
        for (std::size_t index = 0; index < faces.size(); ++index)
        {
            if (Geometry::Bulge::IsStraight(faces[index].Bulge))
            {
                straight[index] = ToLine(faces[index], lines[index]);
            }
            else
            {
                curved[index] = ToArc(faces[index], arcs[index]);
            }
        }

        for (std::size_t first = 0; first < faces.size(); ++first)
        {
            if (!straight[first] && !curved[first])
            {
                continue;
            }
            for (std::size_t second = first + 1; second < faces.size(); ++second)
            {
                if (straight[first] != straight[second] || curved[first] != curved[second])
                {
                    continue;
                }
                ++report.Considered;

                WallCandidate wall;
                const bool paired =
                    curved[first]
                        ? PairArcs(arcs[first], arcs[second], wall, report)
                        : PairLines(faces[second], lines[first], lines[second], wall, report);
                if (!paired)
                {
                    continue;
                }

                wall.FirstFace = first;
                wall.SecondFace = second;
                walls.push_back(wall);
                ++report.Paired;
                if (curved[first])
                {
                    ++report.PairedCurved;
                }
            }
        }
    }
}
