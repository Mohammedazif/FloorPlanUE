#include "Model/FloorPlanCompiler.h"

#include "Dxf/DxfBlockExpander.h"
#include "FloorPlanLimits.h"
#include "Geometry/ContainmentTree.h"
#include "Geometry/LoopAssembler.h"
#include "Model/AnnotationExtractor.h"
#include "Model/CirculationDetector.h"
#include "Model/Identity.h"
#include "Model/OpeningDetector.h"
#include "Model/SingleLineRoomExtractor.h"
#include "Walls/WallJunctionResolver.h"
#include "Walls/WallPairDetector.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace FloorPlan::Model
{
    using Geometry::ContainmentTree;
    using Geometry::Loop;
    using Geometry::LoopVertex;
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



        bool LayerAllowed(const std::string& layer, const std::vector<std::string>& allowed)
        {
            if (allowed.empty())
            {
                return true;
            }
            const std::string upper = Upper(layer);
            for (const std::string& candidate : allowed)
            {
                if (upper == Upper(candidate))
                {
                    return true;
                }
            }
            return false;
        }

        double BulgeFromSweep(double startDegrees, double endDegrees)
        {
            double sweep = endDegrees - startDegrees;
            while (sweep <= 0.0)
            {
                sweep += 360.0;
            }
            const double radians = sweep * 3.14159265358979323846 / 180.0;
            return std::tan(radians / 4.0);
        }

        void CollectGeometry(const std::vector<Dxf::DxfEntity>& entities,
                             const std::vector<std::string>& allowedLayers,
                             std::vector<Loop>& loops,
                             std::vector<Geometry::Segment>& segments)
        {
            for (const Dxf::DxfEntity& entity : entities)
            {
                if (!LayerAllowed(entity.Layer, allowedLayers))
                {
                    continue;
                }
                // Every hatch path is its own loop, so containment nesting turns inner ones
                // into the holes they are without any special case here.
                if (entity.Type == Dxf::DxfEntityType::Hatch)
                {
                    for (const Dxf::DxfHatchLoop& path : entity.BoundaryLoops)
                    {
                        std::vector<LoopVertex> vertices;
                        vertices.reserve(path.Vertices.size());
                        for (const Dxf::DxfPolylineVertex& vertex : path.Vertices)
                        {
                            vertices.push_back(
                                LoopVertex{Vec2{vertex.X, vertex.Y}, vertex.Bulge});
                        }
                        loops.emplace_back(std::move(vertices));
                    }
                    continue;
                }
                const bool polyline = entity.Type == Dxf::DxfEntityType::LwPolyline ||
                                      entity.Type == Dxf::DxfEntityType::Polyline;
                if (polyline && entity.Closed && entity.Vertices.size() >= 3)
                {
                    std::vector<LoopVertex> vertices;
                    vertices.reserve(entity.Vertices.size());
                    for (const Dxf::DxfPolylineVertex& vertex : entity.Vertices)
                    {
                        vertices.push_back(LoopVertex{Vec2{vertex.X, vertex.Y}, vertex.Bulge});
                    }
                    loops.emplace_back(std::move(vertices));
                    continue;
                }
                // Block content is symbol geometry: a door swing closes into a plausible room.
                if (entity.FromBlock)
                {
                    continue;
                }
                if (polyline)
                {
                    for (std::size_t index = 0; index + 1 < entity.Vertices.size(); ++index)
                    {
                        const auto& from = entity.Vertices[index];
                        const auto& to = entity.Vertices[index + 1];
                        segments.push_back(Geometry::Segment{
                            Vec2{from.X, from.Y}, Vec2{to.X, to.Y}, from.Bulge});
                    }
                    continue;
                }
                if (entity.Type == Dxf::DxfEntityType::Line)
                {
                    segments.push_back(Geometry::Segment{Vec2{entity.StartX, entity.StartY},
                                                         Vec2{entity.EndX, entity.EndY}, 0.0});
                    continue;
                }
                if (entity.Type == Dxf::DxfEntityType::Arc && entity.Radius > 0.0)
                {
                    const double startRadians =
                        entity.StartAngleDegrees * 3.14159265358979323846 / 180.0;
                    const double endRadians =
                        entity.EndAngleDegrees * 3.14159265358979323846 / 180.0;
                    segments.push_back(Geometry::Segment{
                        Vec2{entity.CenterX + entity.Radius * std::cos(startRadians),
                             entity.CenterY + entity.Radius * std::sin(startRadians)},
                        Vec2{entity.CenterX + entity.Radius * std::cos(endRadians),
                             entity.CenterY + entity.Radius * std::sin(endRadians)},
                        BulgeFromSweep(entity.StartAngleDegrees, entity.EndAngleDegrees)});
                }
            }
        }

    }

    bool FloorPlanCompiler::Compile(const Dxf::DxfDocument& document,
                                     const CompilerOptions& options, BuildingModel& model,
                                     Diagnostic& diagnostic)
    {
        std::vector<Dxf::DxfEntity> entities;
        if (!Dxf::DxfBlockExpander::Expand(document, entities, diagnostic))
        {
            return false;
        }

        model.UnitsWereDeclared = document.HasInsertUnits;
        const double declared =
            document.HasInsertUnits ? Dxf::MillimetresPerUnit(document.InsertUnits) : 0.0;
        model.MillimetresPerUnit = declared > 0.0 ? declared : options.MillimetresPerUnit;

        std::vector<Geometry::Segment> segments;
        CollectGeometry(entities, options.WallLayers, model.Loops, segments);
        if (!segments.empty())
        {
            Geometry::AssemblyReport report;
            if (!Geometry::LoopAssembler::Assemble(segments, model.Loops, report, diagnostic))
            {
                return false;
            }
            model.AssembledLoops = report.ClosedLoops;
            model.DiscardedSegments = report.DiscardedSegments;
        }
        const ContainmentTree tree = ContainmentTree::Build(model.Loops);

        const double wallHeight =
            options.WallHeightMm > 0.0 ? options.WallHeightMm : Limits::DefaultWallHeightMm;

        model.WallFootprintMm2 = 0.0;
        if (options.Convention == WallConvention::SingleLine)
        {
            if (!SingleLineRoomExtractor::Extract(segments, wallHeight, options.StoreyKey, model,
                                                 diagnostic))
            {
                return false;
            }
        }
        else
        {
            for (const auto& node : tree.Nodes())
            {
                const Loop& loop = model.Loops[node.LoopIndex];
                if ((node.Depth % 2) == 1)
                {
                    if (model.Rooms.size() >= Limits::MaxRoomCount)
                    {
                        diagnostic.Code = DiagnosticCode::RoomLimitExceeded;
                        diagnostic.Message = std::to_string(model.Rooms.size());
                        return false;
                    }
                    Room room;
                    room.AreaMm2 = node.NetArea;
                    room.LoopIndex = node.LoopIndex;
                    Identity identity("room");
                    identity.Add(options.StoreyKey);
                    for (const LoopVertex& vertex : loop.Vertices())
                    {
                        identity.Add(vertex.Position);
                    }
                    room.Id = identity.ToHex();
                    model.Rooms.push_back(std::move(room));
                }
                else
                {
                    model.WallFootprintMm2 += node.NetArea;
                }
            }

            std::vector<Geometry::Segment> faces;
            for (const Loop& loop : model.Loops)
            {
                const auto& vertices = loop.Vertices();
                for (std::size_t edge = 0; edge < vertices.size(); ++edge)
                {
                    faces.push_back(Geometry::Segment{
                        vertices[edge].Position,
                        vertices[(edge + 1) % vertices.size()].Position,
                        vertices[edge].Bulge});
                }
            }

            std::vector<Walls::WallCandidate> candidates;
            Walls::PairingReport pairing;
            Walls::WallPairDetector::Detect(faces, candidates, pairing);
            model.PairedWalls = pairing.Paired;

            Walls::JunctionReport junctions;
            Walls::WallJunctionResolver::Close(candidates, junctions);
            model.ExtendedWallEnds = junctions.EndsExtended;

            for (const Walls::WallCandidate& candidate : candidates)
            {
                if (model.Walls.size() >= Limits::MaxWallCount)
                {
                    diagnostic.Code = DiagnosticCode::WallLimitExceeded;
                    diagnostic.Message = std::to_string(model.Walls.size());
                    return false;
                }
                Wall wall;
                wall.Start = candidate.Start;
                wall.End = candidate.End;
                wall.Bulge = candidate.Bulge;
                wall.ThicknessMm = candidate.ThicknessMm;
                wall.HeightMm = wallHeight;
                Identity identity("wall");
                identity.Add(options.StoreyKey)
                    .Add(wall.Start)
                    .Add(wall.End)
                    .Add(wall.ThicknessMm)
                    .Add(wall.Bulge);
                wall.Id = identity.ToHex();
                model.Walls.push_back(std::move(wall));
            }
        }

        for (const Dxf::DxfEntity& entity : entities)
        {
            if (entity.Type != Dxf::DxfEntityType::Text &&
                entity.Type != Dxf::DxfEntityType::MText)
            {
                continue;
            }
            if (entity.Text.empty())
            {
                continue;
            }
            const Vec2 anchor{entity.AnchorX(), entity.AnchorY()};
            std::size_t best = ContainmentTree::NoParent;
            double bestArea = 0.0;
            for (std::size_t index = 0; index < model.Rooms.size(); ++index)
            {
                const Loop& loop = model.Loops[model.Rooms[index].LoopIndex];
                if (!loop.Contains(anchor))
                {
                    continue;
                }
                const double area = loop.AbsoluteArea();
                if (best == ContainmentTree::NoParent || area < bestArea)
                {
                    best = index;
                    bestArea = area;
                }
            }
            if (best == ContainmentTree::NoParent)
            {
                model.UnassignedLabels.push_back(entity.Text);
            }
            else if (model.Rooms[best].Name.empty())
            {
                model.Rooms[best].Name = entity.Text;
            }
        }

        if (!OpeningDetector::Detect(document.ModelSpace, options, model, diagnostic))
        {
            return false;
        }

        CirculationDetector::Detect(entities, options, model);
        AnnotationExtractor::Extract(entities, document.ModelSpace, options, model);
        return true;
    }
}
