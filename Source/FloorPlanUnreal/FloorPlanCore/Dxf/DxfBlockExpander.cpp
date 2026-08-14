#include "Dxf/DxfBlockExpander.h"

#include "FloorPlanLimits.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace FloorPlan::Dxf
{
    namespace
    {
        constexpr double DegreesToRadians = 3.14159265358979323846 / 180.0;

        struct Placement
        {
            double OriginX = 0.0;
            double OriginY = 0.0;
            double ScaleX = 1.0;
            double ScaleY = 1.0;
            double Cos = 1.0;
            double Sin = 0.0;

            void Apply(double x, double y, double& outX, double& outY) const
            {
                const double sx = x * ScaleX;
                const double sy = y * ScaleY;
                outX = OriginX + sx * Cos - sy * Sin;
                outY = OriginY + sx * Sin + sy * Cos;
            }

            bool Mirrors() const { return ScaleX * ScaleY < 0.0; }

            double UniformScale() const { return std::sqrt(std::fabs(ScaleX * ScaleY)); }
        };

        Placement Compose(const Placement& outer, const DxfEntity& insert, const DxfBlock& block,
                          double offsetX, double offsetY)
        {
            const double radians = insert.RotationDegrees * DegreesToRadians;
            const double localCos = std::cos(radians);
            const double localSin = std::sin(radians);

            const double baseShiftX = -block.BaseX * insert.ScaleX;
            const double baseShiftY = -block.BaseY * insert.ScaleY;

            const double localX = insert.InsertX + offsetX + baseShiftX * localCos -
                                  baseShiftY * localSin;
            const double localY = insert.InsertY + offsetY + baseShiftX * localSin +
                                  baseShiftY * localCos;

            Placement result;
            outer.Apply(localX, localY, result.OriginX, result.OriginY);
            result.ScaleX = outer.ScaleX * insert.ScaleX;
            result.ScaleY = outer.ScaleY * insert.ScaleY;
            result.Cos = outer.Cos * localCos - outer.Sin * localSin;
            result.Sin = outer.Sin * localCos + outer.Cos * localSin;
            return result;
        }

        void Place(const Placement& placement, const DxfEntity& source, DxfEntity& target)
        {
            target = source;
            switch (source.Type)
            {
            case DxfEntityType::Line:
                placement.Apply(source.StartX, source.StartY, target.StartX, target.StartY);
                placement.Apply(source.EndX, source.EndY, target.EndX, target.EndY);
                break;
            case DxfEntityType::Arc:
            case DxfEntityType::Circle:
            {
                placement.Apply(source.CenterX, source.CenterY, target.CenterX, target.CenterY);
                target.Radius = source.Radius * placement.UniformScale();
                const double rotation =
                    std::atan2(placement.Sin, placement.Cos) / DegreesToRadians;
                target.StartAngleDegrees = source.StartAngleDegrees + rotation;
                target.EndAngleDegrees = source.EndAngleDegrees + rotation;
                break;
            }
            case DxfEntityType::LwPolyline:
            case DxfEntityType::Polyline:
                for (std::size_t index = 0; index < source.Vertices.size(); ++index)
                {
                    placement.Apply(source.Vertices[index].X, source.Vertices[index].Y,
                                    target.Vertices[index].X, target.Vertices[index].Y);
                    target.Vertices[index].Bulge = placement.Mirrors()
                                                       ? -source.Vertices[index].Bulge
                                                       : source.Vertices[index].Bulge;
                }
                break;
            case DxfEntityType::Hatch:
                for (std::size_t path = 0; path < source.BoundaryLoops.size(); ++path)
                {
                    const auto& from = source.BoundaryLoops[path].Vertices;
                    auto& into = target.BoundaryLoops[path].Vertices;
                    for (std::size_t index = 0; index < from.size(); ++index)
                    {
                        placement.Apply(from[index].X, from[index].Y, into[index].X,
                                        into[index].Y);
                        into[index].Bulge =
                            placement.Mirrors() ? -from[index].Bulge : from[index].Bulge;
                    }
                }
                break;
            case DxfEntityType::Text:
            case DxfEntityType::MText:
                placement.Apply(source.StartX, source.StartY, target.StartX, target.StartY);
                placement.Apply(source.AlignX, source.AlignY, target.AlignX, target.AlignY);
                target.TextHeight = source.TextHeight * placement.UniformScale();
                break;
            case DxfEntityType::Dimension:
                placement.Apply(source.StartX, source.StartY, target.StartX, target.StartY);
                placement.Apply(source.EndX, source.EndY, target.EndX, target.EndY);
                target.MeasurementMm = source.MeasurementMm * placement.UniformScale();
                break;
            case DxfEntityType::Insert:
            case DxfEntityType::Unknown:
                break;
            }
        }

        struct Walker
        {
            const DxfDocument& Document;
            std::vector<DxfEntity>& Output;
            Diagnostic& Failure;
            std::vector<std::string> Path;

            bool OnPath(const std::string& name) const
            {
                for (const std::string& entry : Path)
                {
                    if (entry == name)
                    {
                        return true;
                    }
                }
                return false;
            }

            bool Fail(DiagnosticCode code, const std::string& message, std::size_t line)
            {
                Failure.Code = code;
                Failure.Message = message;
                Failure.LineNumber = line;
                return false;
            }

            bool Emit(const DxfEntity& entity, const Placement& placement)
            {
                if (Output.size() >= Limits::MaxEntityCount)
                {
                    return Fail(DiagnosticCode::EntityLimitExceeded,
                                std::to_string(Output.size()), entity.LineNumber);
                }
                DxfEntity placed;
                Place(placement, entity, placed);
                placed.FromBlock = !Path.empty();
                Output.push_back(std::move(placed));
                return true;
            }

            bool Visit(const std::vector<DxfEntity>& entities, const Placement& placement)
            {
                for (const DxfEntity& entity : entities)
                {
                    if (entity.Type != DxfEntityType::Insert)
                    {
                        if (!Emit(entity, placement))
                        {
                            return false;
                        }
                        continue;
                    }
                    if (!Descend(entity, placement))
                    {
                        return false;
                    }
                }
                return true;
            }

            bool Descend(const DxfEntity& insert, const Placement& placement)
            {
                if (OnPath(insert.BlockName))
                {
                    return Fail(DiagnosticCode::BlockReferenceCycle, insert.BlockName,
                                insert.LineNumber);
                }
                if (Path.size() >= Limits::MaxBlockRecursionDepth)
                {
                    return Fail(DiagnosticCode::BlockRecursionTooDeep,
                                std::to_string(Path.size()), insert.LineNumber);
                }
                const DxfBlock* block = Document.FindBlock(insert.BlockName);
                if (block == nullptr || block->IsExternalReference)
                {
                    return true;
                }

                const int columns = insert.ColumnCount > 0 ? insert.ColumnCount : 1;
                const int rows = insert.RowCount > 0 ? insert.RowCount : 1;
                const std::int64_t copies =
                    static_cast<std::int64_t>(columns) * static_cast<std::int64_t>(rows);
                if (copies > static_cast<std::int64_t>(Limits::MaxEntityCount))
                {
                    return Fail(DiagnosticCode::EntityLimitExceeded, std::to_string(copies),
                                insert.LineNumber);
                }

                Path.push_back(insert.BlockName);
                for (int column = 0; column < columns; ++column)
                {
                    for (int row = 0; row < rows; ++row)
                    {
                        const Placement nested =
                            Compose(placement, insert, *block,
                                    insert.ColumnSpacing * static_cast<double>(column),
                                    insert.RowSpacing * static_cast<double>(row));
                        if (!Visit(block->Entities, nested))
                        {
                            Path.pop_back();
                            return false;
                        }
                    }
                }
                Path.pop_back();
                return true;
            }
        };
    }

    bool DxfBlockExpander::ValidateBlockGraph(const DxfDocument& document,
                                               Diagnostic& diagnostic)
    {
        enum class Mark
        {
            Unvisited,
            OnStack,
            Done
        };
        std::map<std::string, Mark> marks;
        for (const auto& entry : document.Blocks)
        {
            marks.emplace(entry.first, Mark::Unvisited);
        }

        struct Frame
        {
            const std::string* Name;
            std::size_t Index;
        };

        for (const auto& entry : document.Blocks)
        {
            if (marks[entry.first] != Mark::Unvisited)
            {
                continue;
            }
            std::vector<Frame> stack{Frame{&entry.first, 0}};
            marks[entry.first] = Mark::OnStack;

            while (!stack.empty())
            {
                if (stack.size() > Limits::MaxBlockRecursionDepth)
                {
                    diagnostic.Code = DiagnosticCode::BlockRecursionTooDeep;
                    diagnostic.Message = *stack.back().Name;
                    return false;
                }
                Frame& frame = stack.back();
                const DxfBlock* block = document.FindBlock(*frame.Name);
                if (block == nullptr || frame.Index >= block->Entities.size())
                {
                    marks[*frame.Name] = Mark::Done;
                    stack.pop_back();
                    continue;
                }

                const DxfEntity& child = block->Entities[frame.Index++];
                if (child.Type != DxfEntityType::Insert)
                {
                    continue;
                }
                const auto found = marks.find(child.BlockName);
                if (found == marks.end())
                {
                    continue;
                }
                if (found->second == Mark::OnStack)
                {
                    diagnostic.Code = DiagnosticCode::BlockReferenceCycle;
                    diagnostic.Message = child.BlockName;
                    diagnostic.LineNumber = child.LineNumber;
                    return false;
                }
                if (found->second == Mark::Unvisited)
                {
                    found->second = Mark::OnStack;
                    stack.push_back(Frame{&found->first, 0});
                }
            }
        }
        return true;
    }

    bool DxfBlockExpander::Expand(const DxfDocument& document, std::vector<DxfEntity>& output,
                                   Diagnostic& diagnostic)
    {
        if (!ValidateBlockGraph(document, diagnostic))
        {
            return false;
        }
        Walker walker{document, output, diagnostic, {}};
        return walker.Visit(document.ModelSpace, Placement{});
    }

    std::size_t DxfBlockExpander::UnresolvedReferenceCount(const DxfDocument& document)
    {
        std::size_t unresolved = 0;
        for (const DxfEntity& entity : document.ModelSpace)
        {
            if (entity.Type == DxfEntityType::Insert &&
                document.FindBlock(entity.BlockName) == nullptr)
            {
                ++unresolved;
            }
        }
        return unresolved;
    }
}
