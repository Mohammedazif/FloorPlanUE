#include "Model/SingleLineRoomExtractor.h"

#include "FloorPlanLimits.h"
#include "Geometry/Arrangement.h"
#include "Model/Identity.h"

namespace FloorPlan::Model
{
    using Geometry::Arrangement;
    using Geometry::ArrangementFace;
    using Geometry::ArrangementReport;
    using Geometry::Loop;
    using Geometry::LoopVertex;
    using Geometry::Vec2;

    bool SingleLineRoomExtractor::Extract(const std::vector<Geometry::Segment>& loose,
                                           double wallHeight, const std::string& storeyKey,
                                           BuildingModel& model, Diagnostic& diagnostic)
    {
        std::vector<Geometry::Segment> input = loose;
        for (const Loop& loop : model.Loops)
        {
            const auto& vertices = loop.Vertices();
            for (std::size_t edge = 0; edge < vertices.size(); ++edge)
            {
                input.push_back(
                    Geometry::Segment{vertices[edge].Position,
                                      vertices[(edge + 1) % vertices.size()].Position,
                                      vertices[edge].Bulge});
            }
        }

        Arrangement arrangement;
        ArrangementReport report;
        if (!Arrangement::Build(input, arrangement, report, diagnostic))
        {
            return false;
        }
        model.ArrangementFaces = report.BoundedFaces;

        for (const ArrangementFace& face : arrangement.Faces())
        {
            if (!face.Bounded)
            {
                continue;
            }
            if (model.Rooms.size() >= Limits::MaxRoomCount)
            {
                diagnostic.Code = DiagnosticCode::RoomLimitExceeded;
                diagnostic.Message = std::to_string(model.Rooms.size());
                return false;
            }

            std::vector<LoopVertex> vertices;
            vertices.reserve(face.Polygon.size());
            for (const Vec2& point : face.Polygon)
            {
                vertices.push_back(LoopVertex{point, 0.0});
            }
            model.Loops.emplace_back(std::move(vertices));

            Room room;
            room.AreaMm2 = face.SignedArea;
            room.LoopIndex = model.Loops.size() - 1;
            Identity identity("room");
            identity.Add(storeyKey);
            for (const Vec2& point : face.Polygon)
            {
                identity.Add(point);
            }
            room.Id = identity.ToHex();
            model.Rooms.push_back(std::move(room));
        }

        const auto& halfEdges = arrangement.HalfEdges();
        for (std::size_t index = 0; index + 1 < halfEdges.size(); index += 2)
        {
            if (model.Walls.size() >= Limits::MaxWallCount)
            {
                diagnostic.Code = DiagnosticCode::WallLimitExceeded;
                diagnostic.Message = std::to_string(model.Walls.size());
                return false;
            }
            Wall wall;
            wall.Start = arrangement.Vertices()[halfEdges[index].Origin];
            wall.End = arrangement.Vertices()[halfEdges[index + 1].Origin];
            wall.ThicknessMm = Limits::DefaultSingleLineWallThicknessMm;
            wall.HeightMm = wallHeight;
            Identity identity("wall");
            identity.Add(storeyKey).Add(wall.Start).Add(wall.End).Add(wall.ThicknessMm);
            wall.Id = identity.ToHex();
            model.WallFootprintMm2 += (wall.End - wall.Start).Length() * wall.ThicknessMm;
            model.Walls.push_back(std::move(wall));
        }
        return true;
    }
}
