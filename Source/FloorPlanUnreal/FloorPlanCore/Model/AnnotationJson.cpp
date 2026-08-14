#include "Model/AnnotationJson.h"

#include "Model/JsonWriter.h"

namespace FloorPlan::Model
{
    using Geometry::Vec2;

    namespace
    {
        const char* DimensionName(DimensionKind kind)
        {
            switch (kind)
            {
            case DimensionKind::Linear: return "linear";
            case DimensionKind::Aligned: return "aligned";
            case DimensionKind::Angular: return "angular";
            case DimensionKind::Diameter: return "diameter";
            case DimensionKind::Radius: return "radius";
            case DimensionKind::Ordinate: return "ordinate";
            case DimensionKind::Other: return "other";
            }
            return "other";
        }

        void AppendPoint(std::string& out, const Vec2& point)
        {
            out += '[';
            out += JsonWriter::Number(point.X);
            out += ", ";
            out += JsonWriter::Number(point.Y);
            out += ']';
        }

        void OpenSection(std::string& out, const char* name)
        {
            JsonWriter::AppendIndent(out, 2);
            out += '"';
            out += name;
            out += "\": [";
        }

        void CloseEntry(std::string& out, std::size_t index, std::size_t count)
        {
            out += index + 1 < count ? ",\n" : "\n";
        }

        void CloseSection(std::string& out, std::size_t count)
        {
            if (count == 0)
            {
                out += "],\n";
                return;
            }
            JsonWriter::AppendIndent(out, 2);
            out += "],\n";
        }

        void AppendDimensions(std::string& out, const BuildingModel& model)
        {
            OpenSection(out, "dimensions");
            if (!model.Dimensions.empty())
            {
                out += '\n';
            }
            for (std::size_t index = 0; index < model.Dimensions.size(); ++index)
            {
                const Dimension& dimension = model.Dimensions[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"id\": ";
                JsonWriter::AppendText(out, dimension.Id);
                out += ", \"kind\": ";
                JsonWriter::AppendText(out, DimensionName(dimension.Kind));
                out += ", \"layer\": ";
                JsonWriter::AppendText(out, dimension.Layer);
                out += ", \"startMm\": ";
                AppendPoint(out, dimension.Start);
                out += ", \"endMm\": ";
                AppendPoint(out, dimension.End);
                out += ", \"measurementMm\": ";
                out += JsonWriter::Number(dimension.MeasurementMm);
                out += ", \"geometryMm\": ";
                out += JsonWriter::Number(dimension.GeometryMm);
                out += ", \"stated\": ";
                out += dimension.MeasurementWasStated ? "true" : "false";
                out += ", \"agreesWithGeometry\": ";
                out += dimension.AgreesWithGeometry ? "true" : "false";
                out += '}';
                CloseEntry(out, index, model.Dimensions.size());
            }
            CloseSection(out, model.Dimensions.size());
        }

        void AppendColumns(std::string& out, const BuildingModel& model)
        {
            OpenSection(out, "columns");
            if (!model.Columns.empty())
            {
                out += '\n';
            }
            for (std::size_t index = 0; index < model.Columns.size(); ++index)
            {
                const Column& column = model.Columns[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"id\": ";
                JsonWriter::AppendText(out, column.Id);
                out += ", \"block\": ";
                JsonWriter::AppendText(out, column.BlockName);
                out += ", \"layer\": ";
                JsonWriter::AppendText(out, column.Layer);
                out += ", \"centreMm\": ";
                AppendPoint(out, column.Centre);
                out += ", \"widthMm\": ";
                out += JsonWriter::Number(column.WidthMm);
                out += ", \"depthMm\": ";
                out += JsonWriter::Number(column.DepthMm);
                out += ", \"rotationDegrees\": ";
                out += JsonWriter::Number(column.RotationDegrees);
                out += ", \"hasProfile\": ";
                out += column.HasProfile ? "true" : "false";
                out += '}';
                CloseEntry(out, index, model.Columns.size());
            }
            CloseSection(out, model.Columns.size());
        }

        void AppendGrid(std::string& out, const BuildingModel& model)
        {
            OpenSection(out, "grid");
            if (!model.GridLines.empty())
            {
                out += '\n';
            }
            for (std::size_t index = 0; index < model.GridLines.size(); ++index)
            {
                const GridLine& line = model.GridLines[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"id\": ";
                JsonWriter::AppendText(out, line.Id);
                out += ", \"label\": ";
                JsonWriter::AppendText(out, line.Label);
                out += ", \"startMm\": ";
                AppendPoint(out, line.Start);
                out += ", \"endMm\": ";
                AppendPoint(out, line.End);
                out += '}';
                CloseEntry(out, index, model.GridLines.size());
            }
            CloseSection(out, model.GridLines.size());
        }

        void AppendBlocks(std::string& out, const BuildingModel& model)
        {
            OpenSection(out, "blocks");
            if (!model.BlockInstances.empty())
            {
                out += '\n';
            }
            for (std::size_t index = 0; index < model.BlockInstances.size(); ++index)
            {
                const BlockInstance& instance = model.BlockInstances[index];
                JsonWriter::AppendIndent(out, 3);
                out += "{\"id\": ";
                JsonWriter::AppendText(out, instance.Id);
                out += ", \"block\": ";
                JsonWriter::AppendText(out, instance.BlockName);
                out += ", \"layer\": ";
                JsonWriter::AppendText(out, instance.Layer);
                out += ", \"positionMm\": ";
                AppendPoint(out, instance.Position);
                out += ", \"rotationDegrees\": ";
                out += JsonWriter::Number(instance.RotationDegrees);
                out += ", \"scale\": [";
                out += JsonWriter::Number(instance.ScaleX);
                out += ", ";
                out += JsonWriter::Number(instance.ScaleY);
                out += "]}";
                CloseEntry(out, index, model.BlockInstances.size());
            }
            CloseSection(out, model.BlockInstances.size());
        }
    }

    void AnnotationJson::Append(std::string& out, const BuildingModel& model)
    {
        AppendDimensions(out, model);
        AppendColumns(out, model);
        AppendGrid(out, model);
        AppendBlocks(out, model);

        JsonWriter::AppendIndent(out, 2);
        out += "\"elevation\": {\"minimumMm\": ";
        out += JsonWriter::Number(model.Elevation.MinimumMm);
        out += ", \"maximumMm\": ";
        out += JsonWriter::Number(model.Elevation.MaximumMm);
        out += ", \"distinctLevels\": ";
        out += JsonWriter::Number(static_cast<double>(model.Elevation.DistinctLevels));
        out += ", \"planar\": ";
        out += model.Elevation.IsPlanar() ? "true" : "false";
        out += "},\n";

        JsonWriter::AppendIndent(out, 2);
        out += "\"skippedHatchPaths\": ";
        out += JsonWriter::Number(static_cast<double>(model.SkippedHatchPaths));
        out += ",\n";
    }
}
