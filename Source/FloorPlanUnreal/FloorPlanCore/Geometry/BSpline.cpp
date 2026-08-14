#include "Geometry/BSpline.h"

#include "FloorPlanLimits.h"

#include <algorithm>
#include <cmath>

namespace FloorPlan::Geometry
{
    namespace
    {
        struct Homogeneous
        {
            double X = 0.0;
            double Y = 0.0;
            double W = 1.0;
        };

        Homogeneous Blend(const Homogeneous& low, const Homogeneous& high, double alpha)
        {
            const double keep = 1.0 - alpha;
            return Homogeneous{keep * low.X + alpha * high.X, keep * low.Y + alpha * high.Y,
                               keep * low.W + alpha * high.W};
        }

        std::size_t FindSpan(const SplineCurve& curve, double parameter)
        {
            const std::size_t degree = curve.Degree;
            const std::size_t last = curve.ControlPoints.size() - 1;
            if (parameter >= curve.Knots[last + 1])
            {
                return last;
            }
            std::size_t low = degree;
            std::size_t high = last + 1;
            while (high - low > 1)
            {
                const std::size_t middle = (low + high) / 2;
                if (parameter < curve.Knots[middle])
                {
                    high = middle;
                }
                else
                {
                    low = middle;
                }
            }
            return low;
        }
    }

    bool BSpline::IsWellFormed(const SplineCurve& curve)
    {
        if (curve.Degree < 1 || curve.Degree > Limits::MaxSplineDegree)
        {
            return false;
        }
        if (curve.ControlPoints.size() <= curve.Degree)
        {
            return false;
        }
        if (curve.Knots.size() != curve.ControlPoints.size() + curve.Degree + 1)
        {
            return false;
        }
        if (!curve.Weights.empty() && curve.Weights.size() != curve.ControlPoints.size())
        {
            return false;
        }
        for (std::size_t index = 1; index < curve.Knots.size(); ++index)
        {
            if (!std::isfinite(curve.Knots[index]) ||
                curve.Knots[index] < curve.Knots[index - 1])
            {
                return false;
            }
        }
        return curve.Knots.back() > curve.Knots.front();
    }

    bool BSpline::Evaluate(const SplineCurve& curve, double parameter, Vec2& point)
    {
        if (!IsWellFormed(curve))
        {
            return false;
        }
        const std::size_t degree = curve.Degree;
        const std::size_t span = FindSpan(curve, parameter);

        std::vector<Homogeneous> working(degree + 1);
        for (std::size_t index = 0; index <= degree; ++index)
        {
            const std::size_t control = span - degree + index;
            const double weight =
                curve.Weights.empty() ? 1.0 : curve.Weights[control];
            working[index] = Homogeneous{curve.ControlPoints[control].X * weight,
                                         curve.ControlPoints[control].Y * weight, weight};
        }

        for (std::size_t round = 1; round <= degree; ++round)
        {
            for (std::size_t index = degree; index >= round; --index)
            {
                const std::size_t low = index + span - degree;
                const double left = curve.Knots[low];
                const double right = curve.Knots[index + 1 + span - round];
                const double width = right - left;
                const double alpha = width > 0.0 ? (parameter - left) / width : 0.0;
                working[index] = Blend(working[index - 1], working[index], alpha);
            }
        }

        const Homogeneous& result = working[degree];
        if (std::fabs(result.W) <= 0.0 || !std::isfinite(result.W))
        {
            return false;
        }
        point = Vec2{result.X / result.W, result.Y / result.W};
        return std::isfinite(point.X) && std::isfinite(point.Y);
    }

    void BSpline::Tessellate(const SplineCurve& curve, std::size_t samples,
                              std::vector<Vec2>& points)
    {
        points.clear();
        if (!IsWellFormed(curve) || samples < 1)
        {
            return;
        }
        const std::size_t steps = std::min(samples, Limits::MaxCurveSegments);
        const double first = curve.Knots[curve.Degree];
        const double last = curve.Knots[curve.ControlPoints.size()];
        if (!(last > first))
        {
            return;
        }

        points.reserve(steps + 1);
        for (std::size_t index = 0; index <= steps; ++index)
        {
            const double fraction = static_cast<double>(index) / static_cast<double>(steps);
            Vec2 sample;
            if (Evaluate(curve, first + (last - first) * fraction, sample))
            {
                points.push_back(sample);
            }
        }
    }
}
