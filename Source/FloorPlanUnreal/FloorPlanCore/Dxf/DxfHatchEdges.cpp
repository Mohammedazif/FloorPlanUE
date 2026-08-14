#include "Dxf/DxfHatchEdges.h"

#include "FloorPlanLimits.h"
#include "Geometry/BSpline.h"
#include "Geometry/EllipticArc.h"

#include <cmath>

namespace FloorPlan::Dxf
{
    using Geometry::BSpline;
    using Geometry::EllipticArc;
    using Geometry::SplineCurve;
    using Geometry::Vec2;

    namespace
    {
        constexpr double DegreesToRadians = 3.14159265358979323846 / 180.0;

        void Append(DxfHatchLoop& loop, const Vec2& point, double bulge)
        {
            if (!std::isfinite(point.X) || !std::isfinite(point.Y))
            {
                return;
            }
            loop.Vertices.push_back(DxfPolylineVertex{point.X, point.Y, bulge});
        }

        /// Drops the last sample so the next edge's first point is not written twice.
        void AppendRun(DxfHatchLoop& loop, const std::vector<Vec2>& points)
        {
            for (std::size_t index = 0; index + 1 < points.size(); ++index)
            {
                Append(loop, points[index], 0.0);
            }
        }
    }

    bool DxfHatchCursor::SeekCode(int code)
    {
        while (!AtEnd() && Tags[Index].Code != code)
        {
            ++Index;
        }
        return !AtEnd();
    }

    std::size_t DxfHatchCursor::Count(std::size_t limit)
    {
        const std::int64_t value = Tags[Index].Integer;
        ++Index;
        if (value < 0 || static_cast<std::uint64_t>(value) > limit)
        {
            return limit + 1;
        }
        return static_cast<std::size_t>(value);
    }

    bool DxfHatchEdges::ReadLine(DxfHatchCursor& cursor, DxfHatchLoop& loop)
    {
        double startX = 0.0;
        double startY = 0.0;
        while (cursor.Code() == 10 || cursor.Code() == 20 || cursor.Code() == 11 ||
               cursor.Code() == 21)
        {
            if (cursor.Code() == 10) startX = cursor.Peek().Real;
            if (cursor.Code() == 20) startY = cursor.Peek().Real;
            cursor.Step();
        }
        Append(loop, Vec2{startX, startY}, 0.0);
        return true;
    }

    bool DxfHatchEdges::ReadCircularArc(DxfHatchCursor& cursor, DxfHatchLoop& loop)
    {
        Vec2 centre;
        double radius = 0.0;
        double startDegrees = 0.0;
        double endDegrees = 0.0;
        bool counterClockwise = true;
        while (cursor.Code() == 10 || cursor.Code() == 20 || cursor.Code() == 40 ||
               cursor.Code() == 50 || cursor.Code() == 51 || cursor.Code() == 73)
        {
            switch (cursor.Code())
            {
            case 10: centre.X = cursor.Peek().Real; break;
            case 20: centre.Y = cursor.Peek().Real; break;
            case 40: radius = cursor.Peek().Real; break;
            case 50: startDegrees = cursor.Peek().Real; break;
            case 51: endDegrees = cursor.Peek().Real; break;
            default: counterClockwise = cursor.Peek().Integer != 0; break;
            }
            cursor.Step();
        }
        if (!(radius > 0.0) || radius > Limits::MaxRadiusMm)
        {
            return false;
        }

        double sweep = (endDegrees - startDegrees) * DegreesToRadians;
        while (sweep <= 0.0)
        {
            sweep += 2.0 * 3.14159265358979323846;
        }
        if (!counterClockwise)
        {
            sweep = -(2.0 * 3.14159265358979323846 - sweep);
        }

        const double startRadians = startDegrees * DegreesToRadians;
        const Vec2 start{centre.X + radius * std::cos(startRadians),
                         centre.Y + radius * std::sin(startRadians)};
        Append(loop, start, std::tan(sweep * 0.25));
        return true;
    }

    bool DxfHatchEdges::ReadEllipticArc(DxfHatchCursor& cursor, DxfHatchLoop& loop)
    {
        Vec2 centre;
        Vec2 majorAxis;
        double ratio = 1.0;
        double startDegrees = 0.0;
        double endDegrees = 0.0;
        bool counterClockwise = true;
        while (cursor.Code() == 10 || cursor.Code() == 20 || cursor.Code() == 11 ||
               cursor.Code() == 21 || cursor.Code() == 40 || cursor.Code() == 50 ||
               cursor.Code() == 51 || cursor.Code() == 73)
        {
            switch (cursor.Code())
            {
            case 10: centre.X = cursor.Peek().Real; break;
            case 20: centre.Y = cursor.Peek().Real; break;
            case 11: majorAxis.X = cursor.Peek().Real; break;
            case 21: majorAxis.Y = cursor.Peek().Real; break;
            case 40: ratio = cursor.Peek().Real; break;
            case 50: startDegrees = cursor.Peek().Real; break;
            case 51: endDegrees = cursor.Peek().Real; break;
            default: counterClockwise = cursor.Peek().Integer != 0; break;
            }
            cursor.Step();
        }

        std::vector<Vec2> points;
        EllipticArc::Tessellate(centre, majorAxis, ratio, startDegrees * DegreesToRadians,
                                endDegrees * DegreesToRadians, counterClockwise,
                                Limits::ArcTessellationSagittaMm, points);
        if (points.empty())
        {
            return false;
        }
        AppendRun(loop, points);
        return true;
    }

    bool DxfHatchEdges::ReadSpline(DxfHatchCursor& cursor, DxfHatchLoop& loop)
    {
        SplineCurve curve;
        std::size_t knotCount = 0;
        std::size_t controlCount = 0;
        bool rational = false;
        while (cursor.Code() == 94 || cursor.Code() == 73 || cursor.Code() == 74 ||
               cursor.Code() == 95 || cursor.Code() == 96)
        {
            switch (cursor.Code())
            {
            case 94:
                curve.Degree = static_cast<std::size_t>(
                    cursor.Peek().Integer > 0 ? cursor.Peek().Integer : 0);
                cursor.Step();
                break;
            case 73:
                rational = cursor.Peek().Integer != 0;
                cursor.Step();
                break;
            case 95:
                knotCount = cursor.Count(Limits::MaxVerticesPerPolyline);
                break;
            case 96:
                controlCount = cursor.Count(Limits::MaxVerticesPerPolyline);
                break;
            default:
                cursor.Step();
                break;
            }
        }
        if (knotCount > Limits::MaxVerticesPerPolyline ||
            controlCount > Limits::MaxVerticesPerPolyline)
        {
            return false;
        }

        for (std::size_t index = 0; index < knotCount && cursor.Code() == 40; ++index)
        {
            curve.Knots.push_back(cursor.Peek().Real);
            cursor.Step();
        }
        for (std::size_t index = 0; index < controlCount && cursor.Code() == 10; ++index)
        {
            const double x = cursor.Peek().Real;
            cursor.Step();
            if (cursor.Code() != 20)
            {
                return false;
            }
            curve.ControlPoints.push_back(Vec2{x, cursor.Peek().Real});
            cursor.Step();
            if (rational && cursor.Code() == 42)
            {
                curve.Weights.push_back(cursor.Peek().Real);
                cursor.Step();
            }
        }

        std::vector<Vec2> fitPoints;
        if (cursor.Code() == 97)
        {
            const std::size_t fitCount = cursor.Count(Limits::MaxVerticesPerPolyline);
            for (std::size_t index = 0; index < fitCount && cursor.Code() == 11; ++index)
            {
                const double x = cursor.Peek().Real;
                cursor.Step();
                if (cursor.Code() != 21)
                {
                    break;
                }
                fitPoints.push_back(Vec2{x, cursor.Peek().Real});
                cursor.Step();
            }
        }
        while (cursor.Code() == 12 || cursor.Code() == 22 || cursor.Code() == 13 ||
               cursor.Code() == 23)
        {
            cursor.Step();
        }

        std::vector<Vec2> points;
        if (BSpline::IsWellFormed(curve))
        {
            const std::size_t spans = curve.ControlPoints.size() - curve.Degree;
            BSpline::Tessellate(curve, spans * Limits::SplineSamplesPerSpan, points);
        }
        if (points.empty())
        {
            points = fitPoints.size() >= 2 ? fitPoints : curve.ControlPoints;
        }
        if (points.size() < 2)
        {
            return false;
        }
        AppendRun(loop, points);
        return true;
    }
}
