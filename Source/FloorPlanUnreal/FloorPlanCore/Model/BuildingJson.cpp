#include "Model/BuildingJson.h"

#include "Geometry/Bulge.h"
#include "Model/AnnotationJson.h"
#include "Model/JsonWriter.h"

#include <cmath>

namespace FloorPlan::Model
{
    using Geometry::Vec2;

    namespace
    {
        void AppendPoint(std::string& out, const Vec2& point)
        {
            out += '[';
            out += JsonWriter::Number(point.X);
            out += ", ";
            out += JsonWriter::Number(point.Y);
            out += ']';
        }

        void AppendIdList(std::string& out, const std::vector<std::string>& ids)
        {
            out += '[';
            for (std::size_t index = 0; index < ids.size(); ++index)
            {
                if (index > 0)
                {
                    out += ", ";
                }
                JsonWriter::AppendText(out, ids[index]);
            }
            out += ']';
        }

        std::string RoomReference(const BuildingModel& model, std::size_t room)
        {
            if (room >= model.Rooms.size())
            {
                return "null";
            }
            std::string text;
            JsonWriter::AppendText(text, model.Rooms[room].Id);
            return text;
        }

        double WallLength(const Wall& wall)
        {
            const Geometry::BulgeArc arc =
                Geometry::Bulge::Resolve(wall.Start, wall.End, wall.Bulge);
            if (arc.IsStraight)
            {
                return (wall.End - wall.Start).Length();
            }
            return arc.Radius * std::fabs(arc.IncludedAngle);
        }

        void AppendRooms(std::string& out, const BuildingModel& model)
        {
            for (std::size_t index = 0; index < model.Rooms.size(); ++index)
            {
                const Room& room = model.Rooms[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"id\": ";
                JsonWriter::AppendText(out, room.Id);
                out += ", \"name\": ";
                JsonWriter::AppendText(out, room.Name);
                out += ", \"areaMm2\": ";
                out += JsonWriter::Number(room.AreaMm2);
                out += ", \"boundaryMm\": [";
                if (room.LoopIndex < model.Loops.size())
                {
                    const auto& points = model.Loops[room.LoopIndex].Tessellated();
                    for (std::size_t point = 0; point < points.size(); ++point)
                    {
                        if (point > 0)
                        {
                            out += ", ";
                        }
                        AppendPoint(out, points[point]);
                    }
                }
                out += "]}";
                out += index + 1 < model.Rooms.size() ? ",\n" : "\n";
            }
        }

        void AppendWalls(std::string& out, const BuildingModel& model)
        {
            for (std::size_t index = 0; index < model.Walls.size(); ++index)
            {
                const Wall& wall = model.Walls[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"id\": ";
                JsonWriter::AppendText(out, wall.Id);
                out += ", \"startMm\": ";
                AppendPoint(out, wall.Start);
                out += ", \"endMm\": ";
                AppendPoint(out, wall.End);
                out += ", \"bulge\": ";
                out += JsonWriter::Number(wall.Bulge);
                out += ", \"lengthMm\": ";
                out += JsonWriter::Number(WallLength(wall));
                out += ", \"thicknessMm\": ";
                out += JsonWriter::Number(wall.ThicknessMm);
                out += ", \"heightMm\": ";
                out += JsonWriter::Number(wall.HeightMm);
                out += '}';
                out += index + 1 < model.Walls.size() ? ",\n" : "\n";
            }
        }

        void AppendOpenings(std::string& out, const BuildingModel& model)
        {
            for (std::size_t index = 0; index < model.Openings.size(); ++index)
            {
                const Opening& opening = model.Openings[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"id\": ";
                JsonWriter::AppendText(out, opening.Id);
                out += ", \"kind\": ";
                JsonWriter::AppendText(out, opening.Kind == OpeningKind::Door ? "door" : "window");
                out += ", \"block\": ";
                JsonWriter::AppendText(out, opening.BlockName);
                out += ", \"hostWallId\": ";
                JsonWriter::AppendText(out, opening.HostWallId);
                out += ", \"positionMm\": ";
                AppendPoint(out, opening.Position);
                out += ", \"widthMm\": ";
                out += JsonWriter::Number(opening.WidthMm);
                out += ", \"sillMm\": ";
                out += JsonWriter::Number(opening.SillHeightMm);
                out += ", \"headMm\": ";
                out += JsonWriter::Number(opening.HeadHeightMm);
                out += ", \"rotationDegrees\": ";
                out += JsonWriter::Number(opening.RotationDegrees);
                out += '}';
                out += index + 1 < model.Openings.size() ? ",\n" : "\n";
            }
        }

        void AppendAdjacency(std::string& out, const BuildingModel& model, const RoomGraph& graph)
        {
            const std::vector<RoomLink>& links = graph.Links();
            for (std::size_t index = 0; index < links.size(); ++index)
            {
                const RoomLink& link = links[index];
                std::vector<std::string> wallIds;
                for (const std::size_t wall : link.WallIndices)
                {
                    wallIds.push_back(model.Walls[wall].Id);
                }
                std::vector<std::string> openingIds;
                for (const std::size_t opening : link.OpeningIndices)
                {
                    openingIds.push_back(model.Openings[opening].Id);
                }

                JsonWriter::AppendIndent(out, 3);
                out += "{\"rooms\": [";
                out += RoomReference(model, link.FirstRoom);
                out += ", ";
                out += RoomReference(model, link.SecondRoom);
                out += "], \"wallIds\": ";
                AppendIdList(out, wallIds);
                out += ", \"openingIds\": ";
                AppendIdList(out, openingIds);
                out += ", \"traversable\": ";
                out += link.IsTraversable() ? "true" : "false";
                out += ", \"exterior\": ";
                out += link.IsExterior() ? "true" : "false";
                out += '}';
                out += index + 1 < links.size() ? ",\n" : "\n";
            }
        }

        const char* KindName(CirculationKind kind)
        {
            return kind == CirculationKind::Lift ? "lift" : "stair";
        }

        void AppendCirculation(std::string& out, const BuildingModel& model)
        {
            for (std::size_t index = 0; index < model.Circulation.size(); ++index)
            {
                const CirculationRegion& region = model.Circulation[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"kind\": ";
                JsonWriter::AppendText(out, KindName(region.Kind));
                out += ", \"roomId\": ";
                JsonWriter::AppendText(out, region.RoomIndex < model.Rooms.size()
                                    ? model.Rooms[region.RoomIndex].Id
                                    : std::string());
                out += ", \"runStartMm\": ";
                AppendPoint(out, region.Start);
                out += ", \"runEndMm\": ";
                AppendPoint(out, region.End);
                out += ", \"widthMm\": ";
                out += JsonWriter::Number(region.WidthMm);
                out += ", \"drawnTreads\": ";
                out += JsonWriter::Number(static_cast<double>(region.DrawnTreads));
                out += '}';
                out += index + 1 < model.Circulation.size() ? ",\n" : "\n";
            }
        }

        void AppendSection(std::string& out, const char* name, const std::string& body)
        {
            JsonWriter::AppendIndent(out, 2);
            out += '"';
            out += name;
            out += "\": [";
            if (body.empty())
            {
                out += "],\n";
                return;
            }
            out += '\n';
            out += body;
            JsonWriter::AppendIndent(out, 2);
            out += "],\n";
        }
    }

    std::string BuildingJson::Storey(const BuildingModel& model, const RoomGraph& graph,
                                      const std::string& name, double elevationMm)
    {
        std::string rooms;
        AppendRooms(rooms, model);
        std::string walls;
        AppendWalls(walls, model);
        std::string openings;
        AppendOpenings(openings, model);
        std::string adjacency;
        AppendAdjacency(adjacency, model, graph);
        std::string circulation;
        AppendCirculation(circulation, model);

        std::string out;
        JsonWriter::AppendIndent(out, 1);
        out += "{\n";
        JsonWriter::AppendIndent(out, 2);
        out += "\"storey\": ";
        JsonWriter::AppendText(out, name);
        out += ",\n";
        JsonWriter::AppendIndent(out, 2);
        out += "\"elevationMm\": ";
        out += JsonWriter::Number(elevationMm);
        out += ",\n";
        JsonWriter::AppendIndent(out, 2);
        out += "\"millimetresPerUnit\": ";
        out += JsonWriter::Number(model.MillimetresPerUnit);
        out += ",\n";
        JsonWriter::AppendIndent(out, 2);
        out += "\"unitsWereDeclared\": ";
        out += model.UnitsWereDeclared ? "true" : "false";
        out += ",\n";
        JsonWriter::AppendIndent(out, 2);
        out += "\"totalFloorAreaMm2\": ";
        out += JsonWriter::Number(model.TotalRoomAreaMm2());
        out += ",\n";
        JsonWriter::AppendIndent(out, 2);
        out += "\"wallFootprintMm2\": ";
        out += JsonWriter::Number(model.WallFootprintMm2);
        out += ",\n";

        AppendSection(out, "rooms", rooms);
        AppendSection(out, "walls", walls);
        AppendSection(out, "openings", openings);
        AppendSection(out, "adjacency", adjacency);
        AppendSection(out, "circulation", circulation);
        AnnotationJson::Append(out, model);

        JsonWriter::AppendIndent(out, 2);
        out += "\"unassignedLabels\": [";
        for (std::size_t index = 0; index < model.UnassignedLabels.size(); ++index)
        {
            if (index > 0)
            {
                out += ", ";
            }
            JsonWriter::AppendText(out, model.UnassignedLabels[index]);
        }
        out += "]\n";
        JsonWriter::AppendIndent(out, 1);
        out += '}';
        return out;
    }

    std::string BuildingJson::Connection(const BuildingModel& lower, const BuildingModel& upper,
                                          const std::string& lowerStorey,
                                          const std::string& upperStorey,
                                          const StoreyConnection& link)
    {
        const auto RoomId = [](const BuildingModel& model, std::size_t room) {
            return room < model.Rooms.size() ? model.Rooms[room].Id : std::string();
        };

        std::string out;
        JsonWriter::AppendIndent(out, 2);
        out += "{\"kind\": ";
        JsonWriter::AppendText(out, KindName(link.Kind));
        out += ", \"fromStorey\": ";
        JsonWriter::AppendText(out, lowerStorey);
        out += ", \"toStorey\": ";
        JsonWriter::AppendText(out, upperStorey);
        out += ", \"fromRoomId\": ";
        JsonWriter::AppendText(out, RoomId(lower, link.LowerRoom));
        out += ", \"toRoomId\": ";
        JsonWriter::AppendText(out, RoomId(upper, link.UpperRoom));
        out += ", \"overlapFraction\": ";
        out += JsonWriter::Number(link.OverlapFraction);
        out += '}';
        return out;
    }

    std::string BuildingJson::Building(const std::vector<std::string>& storeys,
                                        const std::vector<std::string>& connections)
    {
        std::string out = "{\n  \"storeys\": [";
        if (storeys.empty())
        {
            out += ']';
        }
        else
        {
            out += '\n';
            for (std::size_t index = 0; index < storeys.size(); ++index)
            {
                out += storeys[index];
                out += index + 1 < storeys.size() ? ",\n" : "\n";
            }
            out += "  ]";
        }

        out += ",\n  \"verticalConnections\": [";
        if (!connections.empty())
        {
            out += '\n';
            for (std::size_t index = 0; index < connections.size(); ++index)
            {
                out += connections[index];
                out += index + 1 < connections.size() ? ",\n" : "\n";
            }
            out += "  ";
        }
        out += "]\n}\n";
        return out;
    }
}
