#include "Model/AnnotationExtractor.h"

#include "FloorPlanLimits.h"
#include "Model/Identity.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace FloorPlan::Model
{
    using Geometry::Vec2;

    namespace
    {
        constexpr int OrdinateDimensionMask = 6;
        constexpr int DimensionKindMask = 7;

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

        bool OnAnyLayer(const std::string& layer, const std::vector<std::string>& allowed)
        {
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

        DimensionKind KindOf(int flags)
        {
            switch (flags & DimensionKindMask)
            {
            case 0:
                return DimensionKind::Linear;
            case 1:
                return DimensionKind::Aligned;
            case 2:
            case 5:
                return DimensionKind::Angular;
            case 3:
                return DimensionKind::Diameter;
            case 4:
                return DimensionKind::Radius;
            case OrdinateDimensionMask:
                return DimensionKind::Ordinate;
            default:
                return DimensionKind::Other;
            }
        }

        bool MeasuresADistance(DimensionKind kind)
        {
            return kind == DimensionKind::Linear || kind == DimensionKind::Aligned;
        }

        void ExtractDimensions(const std::vector<Dxf::DxfEntity>& placements,
                               const CompilerOptions& options, BuildingModel& model)
        {
            for (const Dxf::DxfEntity& entity : placements)
            {
                if (entity.Type != Dxf::DxfEntityType::Dimension)
                {
                    continue;
                }
                Dimension dimension;
                dimension.Kind = KindOf(entity.DimensionType);
                dimension.Layer = entity.Layer;
                dimension.TextOverride = entity.Text;
                dimension.Start = Vec2{entity.StartX, entity.StartY};
                dimension.End = Vec2{entity.EndX, entity.EndY};
                const double spanned = (dimension.End - dimension.Start).Length();
                if (MeasuresADistance(dimension.Kind) && spanned > Limits::ZeroChordLengthMm)
                {
                    dimension.GeometryMm = spanned;
                }

                dimension.MeasurementWasStated = entity.MeasurementMm > 0.0;
                dimension.MeasurementMm = dimension.MeasurementWasStated ? entity.MeasurementMm
                                                                        : dimension.GeometryMm;
                if (dimension.MeasurementWasStated && dimension.GeometryMm > 0.0)
                {
                    dimension.AgreesWithGeometry =
                        std::fabs(dimension.GeometryMm - dimension.MeasurementMm) <=
                        options.DimensionToleranceMm;
                }

                Identity identity("dimension");
                identity.Add(options.StoreyKey)
                    .Add(dimension.Start)
                    .Add(dimension.End)
                    .Add(dimension.MeasurementMm);
                dimension.Id = identity.ToHex();
                model.Dimensions.push_back(std::move(dimension));
            }
        }

        void ExtractGrid(const std::vector<Dxf::DxfEntity>& entities,
                         const CompilerOptions& options, BuildingModel& model)
        {
            std::vector<const Dxf::DxfEntity*> bubbles;
            for (const Dxf::DxfEntity& entity : entities)
            {
                const bool isText = entity.Type == Dxf::DxfEntityType::Text ||
                                    entity.Type == Dxf::DxfEntityType::MText;
                if (isText && OnAnyLayer(entity.Layer, options.GridLayers))
                {
                    bubbles.push_back(&entity);
                }
            }

            for (const Dxf::DxfEntity& entity : entities)
            {
                if (entity.Type != Dxf::DxfEntityType::Line ||
                    !OnAnyLayer(entity.Layer, options.GridLayers))
                {
                    continue;
                }
                GridLine line;
                line.Start = Vec2{entity.StartX, entity.StartY};
                line.End = Vec2{entity.EndX, entity.EndY};

                double nearest = std::numeric_limits<double>::max();
                for (const Dxf::DxfEntity* bubble : bubbles)
                {
                    const Vec2 anchor{bubble->AnchorX(), bubble->AnchorY()};
                    const double distance = std::min((anchor - line.Start).Length(),
                                                     (anchor - line.End).Length());
                    if (distance < nearest)
                    {
                        nearest = distance;
                        line.Label = bubble->Text;
                    }
                }

                Identity identity("grid");
                identity.Add(options.StoreyKey).Add(line.Start).Add(line.End);
                line.Id = identity.ToHex();
                model.GridLines.push_back(std::move(line));
            }
        }

        void ExtractBlocks(const std::vector<Dxf::DxfEntity>& placements,
                           const CompilerOptions& options, BuildingModel& model)
        {
            for (const Dxf::DxfEntity& entity : placements)
            {
                if (entity.Type != Dxf::DxfEntityType::Insert || entity.FromBlock)
                {
                    continue;
                }
                const bool opening = MatchesAny(entity.BlockName, options.DoorBlockPrefixes) ||
                                     MatchesAny(entity.BlockName, options.WindowBlockPrefixes);
                if (opening)
                {
                    continue;
                }

                const Vec2 position{entity.InsertX, entity.InsertY};
                if (OnAnyLayer(entity.Layer, options.ColumnLayers))
                {
                    Column column;
                    column.BlockName = entity.BlockName;
                    column.Layer = entity.Layer;
                    column.Centre = position;
                    column.RotationDegrees = entity.RotationDegrees;
                    Identity identity("column");
                    identity.Add(options.StoreyKey).Add(position).Add(entity.RotationDegrees);
                    column.Id = identity.ToHex();
                    model.Columns.push_back(std::move(column));
                    continue;
                }

                BlockInstance instance;
                instance.BlockName = entity.BlockName;
                instance.Layer = entity.Layer;
                instance.Position = position;
                instance.RotationDegrees = entity.RotationDegrees;
                instance.ScaleX = entity.ScaleX;
                instance.ScaleY = entity.ScaleY;
                Identity identity("block");
                identity.Add(options.StoreyKey)
                    .Add(position)
                    .Add(entity.RotationDegrees)
                    .Add(entity.BlockName);
                instance.Id = identity.ToHex();
                model.BlockInstances.push_back(std::move(instance));
            }
        }

        void ExtractColumnProfiles(const std::vector<Dxf::DxfEntity>& entities,
                                   const CompilerOptions& options, BuildingModel& model)
        {
            for (const Dxf::DxfEntity& entity : entities)
            {
                const bool polyline = entity.Type == Dxf::DxfEntityType::LwPolyline ||
                                      entity.Type == Dxf::DxfEntityType::Polyline;
                if (!polyline || !entity.Closed || entity.Vertices.size() < 3 ||
                    !OnAnyLayer(entity.Layer, options.ColumnLayers))
                {
                    continue;
                }

                std::vector<Geometry::LoopVertex> vertices;
                vertices.reserve(entity.Vertices.size());
                for (const Dxf::DxfPolylineVertex& vertex : entity.Vertices)
                {
                    vertices.push_back(Geometry::LoopVertex{Vec2{vertex.X, vertex.Y},
                                                            vertex.Bulge});
                }
                const Geometry::Loop profile(std::move(vertices));
                const Vec2 low = profile.Minimum();
                const Vec2 high = profile.Maximum();

                model.ColumnProfiles.push_back(profile);
                Column column;
                column.Layer = entity.Layer;
                column.Centre = Vec2{(low.X + high.X) * 0.5, (low.Y + high.Y) * 0.5};
                column.WidthMm = high.X - low.X;
                column.DepthMm = high.Y - low.Y;
                column.ProfileIndex = model.ColumnProfiles.size() - 1;
                column.HasProfile = true;
                Identity identity("column");
                identity.Add(options.StoreyKey).Add(column.Centre).Add(column.WidthMm);
                column.Id = identity.ToHex();
                model.Columns.push_back(std::move(column));
            }
        }

        void MeasureElevation(const std::vector<Dxf::DxfEntity>& entities, BuildingModel& model)
        {
            std::vector<double> levels;
            const auto Note = [&levels](double value) {
                if (!std::isfinite(value))
                {
                    return;
                }
                const auto found = std::find_if(levels.begin(), levels.end(),
                                                [value](double known) {
                                                    return std::fabs(known - value) <=
                                                           Limits::VertexWeldToleranceMm;
                                                });
                if (found == levels.end())
                {
                    levels.push_back(value);
                }
            };

            for (const Dxf::DxfEntity& entity : entities)
            {
                Note(entity.Elevation);
                if (entity.Type == Dxf::DxfEntityType::Line)
                {
                    Note(entity.StartZ);
                    Note(entity.EndZ);
                }
            }
            if (levels.empty())
            {
                return;
            }
            model.Elevation.MinimumMm = *std::min_element(levels.begin(), levels.end());
            model.Elevation.MaximumMm = *std::max_element(levels.begin(), levels.end());
            model.Elevation.DistinctLevels = levels.size();
        }
    }

    void AnnotationExtractor::Extract(const std::vector<Dxf::DxfEntity>& expanded,
                                       const std::vector<Dxf::DxfEntity>& placements,
                                       const CompilerOptions& options, BuildingModel& model)
    {
        ExtractDimensions(placements, options, model);
        ExtractGrid(expanded, options, model);
        ExtractBlocks(placements, options, model);
        ExtractColumnProfiles(expanded, options, model);
        MeasureElevation(expanded, model);

        for (const Dxf::DxfEntity& entity : expanded)
        {
            if (entity.Type == Dxf::DxfEntityType::Hatch)
            {
                model.SkippedHatchPaths += entity.SkippedBoundaryPaths;
            }
        }
    }
}
