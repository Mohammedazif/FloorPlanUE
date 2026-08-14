#include "Dxf/DxfParser.h"

#include "Dxf/DxfEntityBuilder.h"
#include "FloorPlanLimits.h"

namespace FloorPlan::Dxf
{
    namespace
    {
        constexpr int ExternalReferenceFlag = 4;
        constexpr int FrozenLayerFlag = 1;
        constexpr int LockedLayerFlag = 4;
    }

    DxfParser::DxfParser(DxfTagReader& reader) : Cursor(reader) {}

    bool DxfParser::ReadPolylineVertices(DxfEntity& polyline)
    {
        while (Cursor.IsStartOf("VERTEX"))
        {
            DxfPolylineVertex vertex;
            bool hasLocation = false;
            while (Cursor.Advance() && !Cursor.IsStart())
            {
                const DxfTag& tag = Cursor.Tag();
                if (tag.Code == 10)
                {
                    vertex.X = tag.Real;
                    hasLocation = true;
                }
                else if (tag.Code == 20)
                {
                    vertex.Y = tag.Real;
                }
                else if (tag.Code == 42)
                {
                    vertex.Bulge = tag.Real;
                }
            }
            if (hasLocation)
            {
                if (polyline.Vertices.size() >= Limits::MaxVerticesPerPolyline)
                {
                    return Cursor.Fail(DiagnosticCode::VertexLimitExceeded,
                                       std::to_string(polyline.Vertices.size()));
                }
                polyline.Vertices.push_back(vertex);
            }
            if (Cursor.Failed())
            {
                return false;
            }
        }
        if (Cursor.IsStartOf("SEQEND"))
        {
            Cursor.SkipRecord();
        }
        return !Cursor.Failed();
    }

    bool DxfParser::ReadEntity(std::vector<DxfEntity>& target)
    {
        const DxfEntityType type = DxfEntityBuilder::Classify(Cursor.Tag().Text);
        if (type == DxfEntityType::Unknown)
        {
            Cursor.SkipRecord();
            return !Cursor.Failed();
        }

        DxfEntity entity;
        entity.Type = type;
        entity.LineNumber = Cursor.Tag().LineNumber;

        Diagnostic failure;
        while (Cursor.Advance() && !Cursor.IsStart())
        {
            if (!DxfEntityBuilder::Apply(entity, Cursor.Tag(), failure))
            {
                return Cursor.Fail(failure.Code, failure.Message);
            }
        }
        if (Cursor.Failed())
        {
            return false;
        }

        if (type == DxfEntityType::Polyline && !ReadPolylineVertices(entity))
        {
            return false;
        }

        if (!DxfEntityBuilder::Finish(entity, failure))
        {
            return Cursor.Fail(failure.Code, failure.Message);
        }

        if (++EntityCount > Limits::MaxEntityCount)
        {
            return Cursor.Fail(DiagnosticCode::EntityLimitExceeded, std::to_string(EntityCount));
        }
        target.push_back(std::move(entity));
        return true;
    }

    bool DxfParser::ParseEntitiesUntil(const char* terminator, std::vector<DxfEntity>& target)
    {
        while (Cursor.Valid())
        {
            if (Cursor.IsStartOf(terminator) || Cursor.IsStartOf("EOF"))
            {
                return true;
            }
            if (!Cursor.IsStart())
            {
                if (!Cursor.SkipToNextStart())
                {
                    return !Cursor.Failed();
                }
                continue;
            }
            if (!ReadEntity(target))
            {
                return false;
            }
        }
        return !Cursor.Failed();
    }

    bool DxfParser::ParseHeader(DxfDocument& document)
    {
        std::string variable;
        while (Cursor.Advance())
        {
            if (Cursor.AtSectionEnd())
            {
                return true;
            }
            const DxfTag& tag = Cursor.Tag();
            if (tag.Code == 9)
            {
                variable = tag.Text;
            }
            else if (variable == "$ACADVER" && tag.Code == 1)
            {
                document.Version = tag.Text;
            }
            else if (variable == "$INSUNITS" && tag.Code == 70)
            {
                document.InsertUnits = static_cast<int>(tag.Integer);
                document.HasInsertUnits = true;
            }
        }
        return !Cursor.Failed();
    }

    bool DxfParser::ParseLayerTable(DxfDocument& document)
    {
        while (Cursor.Valid())
        {
            if (Cursor.IsStartOf("ENDTAB") || Cursor.AtSectionEnd())
            {
                return true;
            }
            if (!Cursor.IsStartOf("LAYER"))
            {
                if (!Cursor.SkipToNextStart())
                {
                    return !Cursor.Failed();
                }
                continue;
            }

            DxfLayer layer;
            while (Cursor.Advance() && !Cursor.IsStart())
            {
                const DxfTag& tag = Cursor.Tag();
                if (tag.Code == 2)
                {
                    if (tag.Text.size() > Limits::MaxStringValueBytes)
                    {
                        return Cursor.Fail(DiagnosticCode::StringTooLong, "layer name");
                    }
                    layer.Name = tag.Text;
                }
                else if (tag.Code == 62)
                {
                    layer.Color = static_cast<int>(tag.Integer);
                }
                else if (tag.Code == 70)
                {
                    layer.Frozen = (tag.Integer & FrozenLayerFlag) != 0;
                    layer.Locked = (tag.Integer & LockedLayerFlag) != 0;
                }
            }
            if (Cursor.Failed())
            {
                return false;
            }
            if (!layer.Name.empty())
            {
                if (document.Layers.size() >= Limits::MaxLayerCount)
                {
                    return Cursor.Fail(DiagnosticCode::LayerLimitExceeded,
                                       std::to_string(document.Layers.size()));
                }
                document.Layers.push_back(std::move(layer));
            }
        }
        return !Cursor.Failed();
    }

    bool DxfParser::ParseTables(DxfDocument& document)
    {
        while (Cursor.Advance())
        {
            if (Cursor.AtSectionEnd())
            {
                return true;
            }
            if (!Cursor.IsStartOf("TABLE"))
            {
                continue;
            }
            std::string tableName;
            while (Cursor.Advance() && !Cursor.IsStart())
            {
                if (Cursor.Tag().Code == 2)
                {
                    tableName = Cursor.Tag().Text;
                }
            }
            if (Cursor.Failed())
            {
                return false;
            }
            if (tableName == "LAYER" && !ParseLayerTable(document))
            {
                return false;
            }
        }
        return !Cursor.Failed();
    }

    bool DxfParser::ParseBlocks(DxfDocument& document)
    {
        if (!Cursor.Advance())
        {
            return !Cursor.Failed();
        }
        while (Cursor.Valid())
        {
            if (Cursor.AtSectionEnd())
            {
                return true;
            }
            if (!Cursor.IsStartOf("BLOCK"))
            {
                if (!Cursor.SkipToNextStart())
                {
                    return !Cursor.Failed();
                }
                continue;
            }

            DxfBlock block;
            int flags = 0;
            while (Cursor.Advance() && !Cursor.IsStart())
            {
                const DxfTag& tag = Cursor.Tag();
                if (tag.Code == 2 && block.Name.empty())
                {
                    block.Name = tag.Text;
                }
                else if (tag.Code == 10)
                {
                    block.BaseX = tag.Real;
                }
                else if (tag.Code == 20)
                {
                    block.BaseY = tag.Real;
                }
                else if (tag.Code == 70)
                {
                    flags = static_cast<int>(tag.Integer);
                }
            }
            if (Cursor.Failed())
            {
                return false;
            }
            block.IsExternalReference = (flags & ExternalReferenceFlag) != 0;

            if (!ParseEntitiesUntil("ENDBLK", block.Entities))
            {
                return false;
            }
            if (Cursor.IsStartOf("ENDBLK"))
            {
                Cursor.SkipRecord();
            }
            if (!block.Name.empty())
            {
                if (document.Blocks.size() >= Limits::MaxBlockDefinitionCount)
                {
                    return Cursor.Fail(DiagnosticCode::BlockLimitExceeded,
                                       std::to_string(document.Blocks.size()));
                }
                document.Blocks.emplace(block.Name, std::move(block));
            }
        }
        return !Cursor.Failed();
    }

    bool DxfParser::Parse(DxfDocument& document)
    {
        if (!Cursor.Advance())
        {
            return Cursor.Failed()
                       ? false
                       : Cursor.Fail(DiagnosticCode::UnexpectedEndOfInput, "no tags");
        }

        while (Cursor.Valid() && !Cursor.IsStartOf("EOF"))
        {
            if (!Cursor.IsStartOf("SECTION"))
            {
                if (!Cursor.SkipToNextStart())
                {
                    break;
                }
                continue;
            }

            std::string section;
            if (Cursor.Advance() && Cursor.Tag().Code == 2)
            {
                section = Cursor.Tag().Text;
            }
            if (Cursor.Failed())
            {
                return false;
            }

            bool ok = true;
            if (section == "HEADER")
            {
                ok = ParseHeader(document);
            }
            else if (section == "TABLES")
            {
                ok = ParseTables(document);
            }
            else if (section == "BLOCKS")
            {
                ok = ParseBlocks(document);
            }
            else if (section == "ENTITIES")
            {
                ok = ParseEntitiesUntil("ENDSEC", document.ModelSpace);
            }
            else
            {
                while (Cursor.Valid() && !Cursor.AtSectionEnd() && Cursor.SkipToNextStart())
                {
                }
            }
            if (!ok)
            {
                return false;
            }
            if (Cursor.IsStartOf("ENDSEC"))
            {
                Cursor.Advance();
            }
        }

        if (Cursor.Failed())
        {
            return false;
        }
        if (!Cursor.SawEndOfFile())
        {
            return Cursor.Fail(DiagnosticCode::MissingEndOfFileMarker, "no (0, EOF) tag");
        }
        return true;
    }
}
