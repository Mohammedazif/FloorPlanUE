#pragma once

#include "Diagnostic.h"
#include "Geometry/LoopAssembler.h"
#include "Geometry/Vec2.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Geometry
{
    struct ArrangementHalfEdge
    {
        std::size_t Origin = 0;
        std::size_t Next = 0;
        std::size_t Face = 0;
    };

    struct ArrangementFace
    {
        std::vector<std::size_t> Boundary;
        std::vector<Vec2> Polygon;
        double SignedArea = 0.0;
        bool Bounded = false;
    };

    struct ArrangementReport
    {
        std::size_t InputSegments = 0;
        std::size_t SplitEdges = 0;
        std::size_t Intersections = 0;
        std::size_t Vertices = 0;
        std::size_t BoundedFaces = 0;
    };

    /// Planar subdivision of a segment soup, with faces recovered by half-edge traversal.
    class Arrangement
    {
    public:
        /// Arcs are tessellated first; every edge in the result is straight.
        static bool Build(const std::vector<Segment>& segments, Arrangement& output,
                          ArrangementReport& report, Diagnostic& diagnostic);

        const std::vector<Vec2>& Vertices() const { return Points; }
        const std::vector<ArrangementHalfEdge>& HalfEdges() const { return Edges; }
        const std::vector<ArrangementFace>& Faces() const { return Cells; }

        static std::size_t Twin(std::size_t halfEdge) { return halfEdge ^ 1u; }

    private:
        std::vector<Vec2> Points;
        std::vector<ArrangementHalfEdge> Edges;
        std::vector<ArrangementFace> Cells;
    };
}
