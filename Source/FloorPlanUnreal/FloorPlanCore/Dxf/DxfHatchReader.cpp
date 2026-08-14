#include "Dxf/DxfHatchReader.h"

#include "Dxf/DxfHatchEdges.h"
#include "FloorPlanLimits.h"

#include <cstdint>

namespace FloorPlan::Dxf
{
    namespace
    {
        constexpr int PolylineBoundaryFlag = 2;
        constexpr int ExternalBoundaryFlag = 1;
        constexpr int OutermostBoundaryFlag = 16;

        constexpr int LineEdge = 1;
        constexpr int CircularArcEdge = 2;
        constexpr int EllipticArcEdge = 3;
        constexpr int SplineEdge = 4;

        bool ReadPolylinePath(DxfHatchCursor& cursor, DxfHatchLoop& loop)
        {
            bool hasBulge = false;
            while (cursor.Code() == 72 || cursor.Code() == 73)
            {
                if (cursor.Code() == 72)
                {
                    hasBulge = cursor.Peek().Integer != 0;
                }
                cursor.Step();
            }
            if (cursor.Code() != 93)
            {
                return false;
            }
            const std::size_t count = cursor.Count(Limits::MaxVerticesPerPolyline);
            if (count > Limits::MaxVerticesPerPolyline)
            {
                return false;
            }

            for (std::size_t vertex = 0; vertex < count; ++vertex)
            {
                if (cursor.Code() != 10)
                {
                    return false;
                }
                const double x = cursor.Peek().Real;
                cursor.Step();
                if (cursor.Code() != 20)
                {
                    return false;
                }
                const double y = cursor.Peek().Real;
                cursor.Step();
                double bulge = 0.0;
                if (hasBulge && cursor.Code() == 42)
                {
                    bulge = cursor.Peek().Real;
                    cursor.Step();
                }
                loop.Vertices.push_back(DxfPolylineVertex{x, y, bulge});
            }
            return true;
        }

        bool ReadEdgePath(DxfHatchCursor& cursor, DxfHatchLoop& loop)
        {
            if (cursor.Code() != 93)
            {
                return false;
            }
            const std::size_t edges = cursor.Count(Limits::MaxHatchEdgesPerPath);
            if (edges > Limits::MaxHatchEdgesPerPath)
            {
                return false;
            }

            for (std::size_t edge = 0; edge < edges; ++edge)
            {
                if (cursor.Code() != 72)
                {
                    return false;
                }
                const std::int64_t kind = cursor.Peek().Integer;
                cursor.Step();

                bool ok = false;
                switch (kind)
                {
                case LineEdge: ok = DxfHatchEdges::ReadLine(cursor, loop); break;
                case CircularArcEdge: ok = DxfHatchEdges::ReadCircularArc(cursor, loop);
                    break;
                case EllipticArcEdge: ok = DxfHatchEdges::ReadEllipticArc(cursor, loop);
                    break;
                case SplineEdge: ok = DxfHatchEdges::ReadSpline(cursor, loop); break;
                default: ok = false; break;
                }
                if (!ok)
                {
                    return false;
                }
            }
            return true;
        }

        void SkipSourceObjects(DxfHatchCursor& cursor)
        {
            if (cursor.Code() != 97)
            {
                return;
            }
            cursor.Step();
            while (cursor.Code() == 330)
            {
                cursor.Step();
            }
        }
    }

    bool DxfHatchReader::Read(const std::vector<DxfTag>& tags, DxfHatchBoundary& boundary,
                               Diagnostic& diagnostic)
    {
        DxfHatchCursor cursor(tags);
        if (!cursor.SeekCode(91))
        {
            return true;
        }
        const std::size_t paths = cursor.Count(Limits::MaxHatchBoundaryPaths);
        if (paths > Limits::MaxHatchBoundaryPaths)
        {
            diagnostic.Code = DiagnosticCode::ValueOutOfRange;
            diagnostic.Message = "hatch boundary path count";
            return false;
        }

        for (std::size_t path = 0; path < paths; ++path)
        {
            if (cursor.Code() != 92)
            {
                ++boundary.SkippedPaths;
                break;
            }
            const std::int64_t flags = cursor.Peek().Integer;
            cursor.Step();

            DxfHatchLoop loop;
            loop.IsOutermost =
                (flags & (ExternalBoundaryFlag | OutermostBoundaryFlag)) != 0;
            const bool built = (flags & PolylineBoundaryFlag) != 0
                                   ? ReadPolylinePath(cursor, loop)
                                   : ReadEdgePath(cursor, loop);
            SkipSourceObjects(cursor);

            if (!built || loop.Vertices.size() < 3)
            {
                ++boundary.SkippedPaths;
                continue;
            }
            boundary.Loops.push_back(std::move(loop));
        }
        return true;
    }
}
