#pragma once

#include "Geometry/Vec2.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Geometry
{
    /// The curve a DXF spline edge defines: control points, a knot vector, optional weights.
    struct SplineCurve
    {
        std::vector<Vec2> ControlPoints;
        std::vector<double> Knots;
        std::vector<double> Weights;
        std::size_t Degree = 3;
    };

    /// Evaluates a non-uniform rational B-spline by de Boor's algorithm.
    class BSpline
    {
    public:
        /// False when the knot vector does not match the control points and degree.
        static bool IsWellFormed(const SplineCurve& curve);

        static bool Evaluate(const SplineCurve& curve, double parameter, Vec2& point);

        /// Samples the whole valid parameter range, both ends included.
        static void Tessellate(const SplineCurve& curve, std::size_t samples,
                               std::vector<Vec2>& points);
    };
}
