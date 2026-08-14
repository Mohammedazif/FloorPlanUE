#include "Geometry/EllipticArc.h"

#include "FloorPlanLimits.h"

#include <algorithm>
#include <cmath>

namespace FloorPlan::Geometry
{
    namespace
    {
        constexpr double TwoPi = 6.28318530717958647692;

        double PositiveSweep(double startRadians, double endRadians)
        {
            double sweep = std::fmod(endRadians - startRadians, TwoPi);
            if (sweep <= 0.0)
            {
                sweep += TwoPi;
            }
            return sweep;
        }

        std::size_t StepsFor(double radius, double sweep, double sagittaTolerance)
        {
            if (radius <= 0.0 || sagittaTolerance <= 0.0)
            {
                return 1;
            }
            const double ratio = 1.0 - sagittaTolerance / radius;
            if (ratio <= -1.0)
            {
                return 1;
            }
            const double step = 2.0 * std::acos(std::min(1.0, ratio));
            if (!(step > 0.0))
            {
                return 1;
            }
            const double count = std::ceil(std::fabs(sweep) / step);
            if (!std::isfinite(count) || count < 1.0)
            {
                return 1;
            }
            return static_cast<std::size_t>(
                std::min(count, static_cast<double>(Limits::MaxCurveSegments)));
        }
    }

    Vec2 EllipticArc::Evaluate(const Vec2& centre, const Vec2& majorAxis, double minorRatio,
                                double parameterRadians)
    {
        const Vec2 minorAxis = majorAxis.PerpendicularCcw() * minorRatio;
        const double cosine = std::cos(parameterRadians);
        const double sine = std::sin(parameterRadians);
        return Vec2{centre.X + majorAxis.X * cosine + minorAxis.X * sine,
                    centre.Y + majorAxis.Y * cosine + minorAxis.Y * sine};
    }

    void EllipticArc::Tessellate(const Vec2& centre, const Vec2& majorAxis, double minorRatio,
                                  double startRadians, double endRadians,
                                  bool counterClockwise, double sagittaTolerance,
                                  std::vector<Vec2>& points)
    {
        points.clear();
        const double major = majorAxis.Length();
        if (major <= 0.0 || !std::isfinite(minorRatio))
        {
            return;
        }

        const double sweep = counterClockwise ? PositiveSweep(startRadians, endRadians)
                                              : -PositiveSweep(endRadians, startRadians);
        const std::size_t steps = StepsFor(major, sweep, sagittaTolerance);
        points.reserve(steps + 1);
        for (std::size_t index = 0; index <= steps; ++index)
        {
            const double fraction =
                static_cast<double>(index) / static_cast<double>(steps);
            points.push_back(
                Evaluate(centre, majorAxis, minorRatio, startRadians + sweep * fraction));
        }
    }
}
