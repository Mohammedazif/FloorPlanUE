#include "Model/OpeningDetector.h"

#include "FloorPlanLimits.h"
#include "Model/Identity.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>

namespace FloorPlan::Model
{
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

        bool MatchesAny(const std::string& name, const std::vector<std::string>& prefixes)
        {
            const std::string upper = Upper(name);
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

        /// Door and window blocks are named for their nominal width: DOOR_900 is 900 wide.
        double TrailingNumber(const std::string& name)
        {
            std::size_t end = name.size();
            while (end > 0 && std::isdigit(static_cast<unsigned char>(name[end - 1])) != 0)
            {
                --end;
            }
            if (end == name.size())
            {
                return 0.0;
            }
            double value = 0.0;
            const char* begin = name.data() + end;
            const char* stop = name.data() + name.size();
            const std::from_chars_result parsed = std::from_chars(begin, stop, value);
            if (parsed.ec != std::errc{} || !std::isfinite(value) ||
                value > Limits::MaxCoordinateMm)
            {
                return 0.0;
            }
            return value;
        }

        double DistanceToSegment(const Vec2& point, const Vec2& start, const Vec2& end)
        {
            const Vec2 span = end - start;
            const double lengthSquared = span.LengthSquared();
            if (lengthSquared < Limits::ZeroChordLengthMm)
            {
                return (point - start).Length();
            }
            double t = Geometry::Dot(point - start, span) / lengthSquared;
            t = std::max(0.0, std::min(1.0, t));
            const Vec2 closest{start.X + span.X * t, start.Y + span.Y * t};
            return (point - closest).Length();
        }
    }

    bool OpeningDetector::Detect(const std::vector<Dxf::DxfEntity>& placements,
                                  const CompilerOptions& options, BuildingModel& model,
                                  Diagnostic& diagnostic)
    {
        for (const Dxf::DxfEntity& entity : placements)
        {
            if (entity.Type != Dxf::DxfEntityType::Insert)
            {
                continue;
            }
            const bool door = MatchesAny(entity.BlockName, options.DoorBlockPrefixes);
            const bool window = MatchesAny(entity.BlockName, options.WindowBlockPrefixes);
            if (!door && !window)
            {
                continue;
            }
            if (model.Openings.size() >= Limits::MaxOpeningCount)
            {
                diagnostic.Code = DiagnosticCode::OpeningLimitExceeded;
                diagnostic.Message = std::to_string(model.Openings.size());
                return false;
            }

            Opening opening;
            opening.Kind = door ? OpeningKind::Door : OpeningKind::Window;
            opening.BlockName = entity.BlockName;
            opening.Position = Vec2{entity.InsertX, entity.InsertY};
            opening.RotationDegrees = entity.RotationDegrees;
            opening.WidthMm = TrailingNumber(entity.BlockName) * std::fabs(entity.ScaleX);
            opening.SillHeightMm = door ? 0.0 : Limits::DefaultWindowSillHeightMm;
            opening.HeadHeightMm = door ? Limits::DefaultDoorHeadHeightMm
                                        : Limits::DefaultWindowHeadHeightMm;

            double bestDistance = std::numeric_limits<double>::max();
            for (const Wall& wall : model.Walls)
            {
                const double distance =
                    DistanceToSegment(opening.Position, wall.Start, wall.End);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    opening.HostWallId = wall.Id;
                }
            }

            Identity identity(door ? "door" : "window");
            identity.Add(options.StoreyKey)
                .Add(opening.Position)
                .Add(opening.WidthMm)
                .Add(opening.RotationDegrees);
            opening.Id = identity.ToHex();
            model.Openings.push_back(std::move(opening));
        }
        return true;
    }
}
