#include "Geometry/Arrangement.h"

#include "FloorPlanLimits.h"
#include "Geometry/Bulge.h"

#include <algorithm>
#include <cmath>
#include <map>

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

        struct StraightEdge
        {
            std::size_t A = 0;
            std::size_t B = 0;
        };

        void Flatten(const std::vector<Segment>& segments, std::vector<Segment>& straight)
        {
            std::vector<Vec2> interior;
            for (const Segment& segment : segments)
            {
                if (Bulge::IsStraight(segment.Bulge))
                {
                    straight.push_back(Segment{segment.Start, segment.End, 0.0});
                    continue;
                }
                Bulge::Tessellate(segment.Start, segment.End, segment.Bulge,
                                  Limits::ArcTessellationSagittaMm, interior);
                Vec2 previous = segment.Start;
                for (const Vec2& point : interior)
                {
                    straight.push_back(Segment{previous, point, 0.0});
                    previous = point;
                }
                straight.push_back(Segment{previous, segment.End, 0.0});
            }
        }

        bool SegmentCrossing(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d,
                             double& tOut)
        {
            const Vec2 r = b - a;
            const Vec2 s = d - c;
            const double denominator = Cross(r, s);
            if (std::fabs(denominator) < 1e-12)
            {
                return false;
            }
            const double t = Cross(c - a, s) / denominator;
            const double u = Cross(c - a, r) / denominator;
            const double slack = Limits::VertexWeldToleranceMm / std::max(1.0, r.Length());
            if (t <= slack || t >= 1.0 - slack || u <= 0.0 || u >= 1.0)
            {
                return false;
            }
            tOut = t;
            return true;
        }

        double PseudoAngleLess(const Vec2& direction)
        {
            return std::atan2(direction.Y, direction.X);
        }

        /// A vertex resting on another edge's interior is a T-junction and must split it.
        bool PointSplitsSegment(const Vec2& point, const Segment& segment, double& tOut)
        {
            const Vec2 span = segment.End - segment.Start;
            const double lengthSquared = span.LengthSquared();
            if (lengthSquared < Limits::ZeroChordLengthMm)
            {
                return false;
            }
            const double t = Dot(point - segment.Start, span) / lengthSquared;
            const double slack = Limits::VertexWeldToleranceMm / std::sqrt(lengthSquared);
            if (t <= slack || t >= 1.0 - slack)
            {
                return false;
            }
            const Vec2 closest{segment.Start.X + span.X * t, segment.Start.Y + span.Y * t};
            if ((point - closest).Length() > Limits::EdgeProximityToleranceMm)
            {
                return false;
            }
            tOut = t;
            return true;
        }
    }

    bool Arrangement::Build(const std::vector<Segment>& segments, Arrangement& output,
                             ArrangementReport& report, Diagnostic& diagnostic)
    {
        report = ArrangementReport{};
        report.InputSegments = segments.size();

        std::vector<Segment> straight;
        Flatten(segments, straight);
        if (straight.size() > Limits::MaxEntityCount)
        {
            diagnostic.Code = DiagnosticCode::ArrangementTooComplex;
            diagnostic.Message = std::to_string(straight.size());
            return false;
        }

        std::vector<std::vector<double>> splits(straight.size());
        std::size_t intersections = 0;
        for (std::size_t i = 0; i < straight.size(); ++i)
        {
            for (std::size_t j = i + 1; j < straight.size(); ++j)
            {
                double t = 0.0;
                if (!SegmentCrossing(straight[i].Start, straight[i].End, straight[j].Start,
                                     straight[j].End, t))
                {
                    continue;
                }
                double u = 0.0;
                SegmentCrossing(straight[j].Start, straight[j].End, straight[i].Start,
                                straight[i].End, u);
                splits[i].push_back(t);
                splits[j].push_back(u);
                if (++intersections > Limits::ArrangementIntersectionsMax)
                {
                    diagnostic.Code = DiagnosticCode::ArrangementTooComplex;
                    diagnostic.Message = std::to_string(intersections);
                    return false;
                }
            }
        }
        std::vector<Vec2> endpoints;
        endpoints.reserve(straight.size() * 2);
        for (const Segment& segment : straight)
        {
            endpoints.push_back(segment.Start);
            endpoints.push_back(segment.End);
        }
        for (std::size_t index = 0; index < straight.size(); ++index)
        {
            for (const Vec2& point : endpoints)
            {
                double t = 0.0;
                if (PointSplitsSegment(point, straight[index], t))
                {
                    splits[index].push_back(t);
                    ++intersections;
                }
            }
            if (intersections > Limits::ArrangementIntersectionsMax)
            {
                diagnostic.Code = DiagnosticCode::ArrangementTooComplex;
                diagnostic.Message = std::to_string(intersections);
                return false;
            }
        }
        report.Intersections = intersections;

        std::map<GridKey, std::size_t> lookup;
        auto vertexFor = [&](const Vec2& point) {
            const GridKey key = Snap(point);
            const auto found = lookup.find(key);
            if (found != lookup.end())
            {
                return found->second;
            }
            const std::size_t index = output.Points.size();
            output.Points.push_back(point);
            lookup.emplace(key, index);
            return index;
        };

        std::vector<StraightEdge> edges;
        for (std::size_t index = 0; index < straight.size(); ++index)
        {
            std::vector<double> parameters = splits[index];
            parameters.push_back(0.0);
            parameters.push_back(1.0);
            std::sort(parameters.begin(), parameters.end());

            const Vec2 start = straight[index].Start;
            const Vec2 span = straight[index].End - straight[index].Start;
            for (std::size_t step = 0; step + 1 < parameters.size(); ++step)
            {
                const Vec2 from{start.X + span.X * parameters[step],
                                start.Y + span.Y * parameters[step]};
                const Vec2 to{start.X + span.X * parameters[step + 1],
                              start.Y + span.Y * parameters[step + 1]};
                if ((to - from).Length() < Limits::MinEdgeLengthMm)
                {
                    continue;
                }
                const std::size_t a = vertexFor(from);
                const std::size_t b = vertexFor(to);
                if (a != b)
                {
                    edges.push_back(StraightEdge{a, b});
                }
            }
        }

        std::sort(edges.begin(), edges.end(), [](const StraightEdge& l, const StraightEdge& r) {
            const std::size_t la = std::min(l.A, l.B);
            const std::size_t lb = std::max(l.A, l.B);
            const std::size_t ra = std::min(r.A, r.B);
            const std::size_t rb = std::max(r.A, r.B);
            return la != ra ? la < ra : lb < rb;
        });
        edges.erase(std::unique(edges.begin(), edges.end(),
                                [](const StraightEdge& l, const StraightEdge& r) {
                                    return std::min(l.A, l.B) == std::min(r.A, r.B) &&
                                           std::max(l.A, l.B) == std::max(r.A, r.B);
                                }),
                    edges.end());

        report.SplitEdges = edges.size();
        report.Vertices = output.Points.size();

        output.Edges.resize(edges.size() * 2);
        for (std::size_t index = 0; index < edges.size(); ++index)
        {
            output.Edges[index * 2].Origin = edges[index].A;
            output.Edges[index * 2 + 1].Origin = edges[index].B;
        }

        std::vector<std::vector<std::size_t>> rings(output.Points.size());
        for (std::size_t index = 0; index < output.Edges.size(); ++index)
        {
            rings[output.Edges[index].Origin].push_back(index);
        }

        for (std::size_t vertex = 0; vertex < rings.size(); ++vertex)
        {
            std::vector<std::size_t>& ring = rings[vertex];
            const Vec2 origin = output.Points[vertex];
            std::sort(ring.begin(), ring.end(), [&](std::size_t left, std::size_t right) {
                const Vec2 leftDirection =
                    output.Points[output.Edges[Twin(left)].Origin] - origin;
                const Vec2 rightDirection =
                    output.Points[output.Edges[Twin(right)].Origin] - origin;
                const double leftAngle = PseudoAngleLess(leftDirection);
                const double rightAngle = PseudoAngleLess(rightDirection);
                return leftAngle != rightAngle ? leftAngle < rightAngle : left < right;
            });

            const std::size_t count = ring.size();
            for (std::size_t index = 0; index < count; ++index)
            {
                const std::size_t incoming = Twin(ring[index]);
                const std::size_t successor = ring[(index + count - 1) % count];
                output.Edges[incoming].Next = successor;
            }
        }

        std::vector<bool> visited(output.Edges.size(), false);
        for (std::size_t seed = 0; seed < output.Edges.size(); ++seed)
        {
            if (visited[seed])
            {
                continue;
            }
            ArrangementFace face;
            std::size_t cursor = seed;
            std::size_t guard = 0;
            do
            {
                visited[cursor] = true;
                face.Boundary.push_back(cursor);
                face.Polygon.push_back(output.Points[output.Edges[cursor].Origin]);
                cursor = output.Edges[cursor].Next;
                if (++guard > output.Edges.size())
                {
                    diagnostic.Code = DiagnosticCode::ArrangementTooComplex;
                    diagnostic.Message = "half-edge cycle did not close";
                    return false;
                }
            } while (cursor != seed);

            double shoelace = 0.0;
            for (std::size_t index = 0; index < face.Polygon.size(); ++index)
            {
                const Vec2& current = face.Polygon[index];
                const Vec2& next = face.Polygon[(index + 1) % face.Polygon.size()];
                shoelace += current.X * next.Y - next.X * current.Y;
            }
            face.SignedArea = 0.5 * shoelace;
            face.Bounded = face.SignedArea > Limits::AreaEpsilonMm2;
            if (face.Bounded)
            {
                ++report.BoundedFaces;
            }
            for (const std::size_t halfEdge : face.Boundary)
            {
                output.Edges[halfEdge].Face = output.Cells.size();
            }
            output.Cells.push_back(std::move(face));
        }
        return true;
    }
}
