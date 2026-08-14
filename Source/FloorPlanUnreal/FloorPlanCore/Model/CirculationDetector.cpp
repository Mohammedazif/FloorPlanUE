#include "Model/CirculationDetector.h"

#include "FloorPlanLimits.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace FloorPlan::Model
{
    using Geometry::Loop;
    using Geometry::Vec2;

    namespace
    {
        std::string Upper(const std::string& text)
        {
            std::string result = text;
            for (char& character : result)
            {
                character =
                    static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
            return result;
        }

        bool MatchesAny(const std::string& text, const std::vector<std::string>& prefixes)
        {
            const std::string upper = Upper(text);
            for (const std::string& prefix : prefixes)
            {
                const std::string candidate = Upper(prefix);
                if (upper.size() >= candidate.size() &&
                    upper.compare(0, candidate.size(), candidate) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsAny(const std::string& text, const std::vector<std::string>& names)
        {
            const std::string upper = Upper(text);
            for (const std::string& name : names)
            {
                if (upper == Upper(name))
                {
                    return true;
                }
            }
            return false;
        }

        struct Tread
        {
            Vec2 Midpoint;
            Vec2 Direction;
            double Length = 0.0;
        };

        bool ToTread(const Dxf::DxfEntity& entity, Tread& tread)
        {
            if (entity.Type != Dxf::DxfEntityType::Line)
            {
                return false;
            }
            const Vec2 start{entity.StartX, entity.StartY};
            const Vec2 end{entity.EndX, entity.EndY};
            const Vec2 span = end - start;
            const double length = span.Length();
            if (length < Limits::MinEdgeLengthMm)
            {
                return false;
            }
            tread.Midpoint = Vec2{(start.X + end.X) * 0.5, (start.Y + end.Y) * 0.5};
            tread.Direction = Vec2{span.X / length, span.Y / length};
            tread.Length = length;
            return true;
        }

        /// Treads point either way along the same axis, so average the doubled angle.
        Vec2 AxialMean(const std::vector<Tread>& treads)
        {
            double sumCos = 0.0;
            double sumSin = 0.0;
            for (const Tread& tread : treads)
            {
                const double angle = 2.0 * std::atan2(tread.Direction.Y, tread.Direction.X);
                sumCos += std::cos(angle);
                sumSin += std::sin(angle);
            }
            if (sumCos == 0.0 && sumSin == 0.0)
            {
                return Vec2{1.0, 0.0};
            }
            const double mean = 0.5 * std::atan2(sumSin, sumCos);
            return Vec2{std::cos(mean), std::sin(mean)};
        }

        Vec2 LongestBoxAxis(const Loop& loop)
        {
            const Vec2 span = loop.Maximum() - loop.Minimum();
            return std::fabs(span.X) >= std::fabs(span.Y) ? Vec2{1.0, 0.0} : Vec2{0.0, 1.0};
        }

        void MeasureRun(const Loop& loop, const Vec2& along, CirculationRegion& region)
        {
            const Vec2 across = along.PerpendicularCcw();
            double lowAlong = 0.0;
            double highAlong = 0.0;
            double lowAcross = 0.0;
            double highAcross = 0.0;
            bool first = true;

            for (const Vec2& point : loop.Tessellated())
            {
                const double onAlong = Geometry::Dot(point, along);
                const double onAcross = Geometry::Dot(point, across);
                if (first)
                {
                    lowAlong = highAlong = onAlong;
                    lowAcross = highAcross = onAcross;
                    first = false;
                    continue;
                }
                lowAlong = std::min(lowAlong, onAlong);
                highAlong = std::max(highAlong, onAlong);
                lowAcross = std::min(lowAcross, onAcross);
                highAcross = std::max(highAcross, onAcross);
            }

            const double centreAcross = 0.5 * (lowAcross + highAcross);
            region.Start = Vec2{along.X * lowAlong + across.X * centreAcross,
                                along.Y * lowAlong + across.Y * centreAcross};
            region.End = Vec2{along.X * highAlong + across.X * centreAcross,
                              along.Y * highAlong + across.Y * centreAcross};
            region.WidthMm = highAcross - lowAcross;
        }
    }

    void CirculationDetector::Detect(const std::vector<Dxf::DxfEntity>& entities,
                                      const CompilerOptions& options, BuildingModel& model)
    {
        std::vector<std::vector<Tread>> treadsPerRoom(model.Rooms.size());
        for (const Dxf::DxfEntity& entity : entities)
        {
            if (!IsAny(entity.Layer, options.StairLayers))
            {
                continue;
            }
            Tread tread;
            if (!ToTread(entity, tread))
            {
                continue;
            }
            const std::size_t room = model.SmallestRoomContaining(tread.Midpoint);
            if (room != BuildingModel::NoRoom)
            {
                treadsPerRoom[room].push_back(tread);
            }
        }

        for (std::size_t index = 0; index < model.Rooms.size(); ++index)
        {
            const Room& room = model.Rooms[index];
            CirculationKind kind = CirculationKind::None;
            if (MatchesAny(room.Name, options.LiftNamePrefixes))
            {
                kind = CirculationKind::Lift;
            }
            else if (MatchesAny(room.Name, options.StairNamePrefixes) ||
                     treadsPerRoom[index].size() >= 2)
            {
                kind = CirculationKind::Stair;
            }
            if (kind == CirculationKind::None || room.LoopIndex >= model.Loops.size())
            {
                continue;
            }

            const Loop& loop = model.Loops[room.LoopIndex];
            const std::vector<Tread>& treads = treadsPerRoom[index];
            const Vec2 along = treads.size() >= 2 ? AxialMean(treads).PerpendicularCcw()
                                                  : LongestBoxAxis(loop);

            CirculationRegion region;
            region.Kind = kind;
            region.RoomIndex = index;
            region.DrawnTreads = kind == CirculationKind::Stair ? treads.size() : 0;
            MeasureRun(loop, along, region);
            model.Circulation.push_back(region);
        }
    }
}
