#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfDocument.h"
#include "Dxf/DxfTagCursor.h"
#include "Dxf/DxfTagReader.h"

#include <vector>

namespace FloorPlan::Dxf
{
    /// Walks a tag stream into a DxfDocument: header variables, layers, blocks and model space.
    class DxfParser
    {
    public:
        explicit DxfParser(DxfTagReader& reader);

        bool Parse(DxfDocument& document);

        const Diagnostic& Failure() const { return Cursor.Failure(); }

    private:
        bool ParseHeader(DxfDocument& document);
        bool ParseTables(DxfDocument& document);
        bool ParseLayerTable(DxfDocument& document);
        bool ParseBlocks(DxfDocument& document);
        bool ParseEntitiesUntil(const char* terminator, std::vector<DxfEntity>& target);
        bool ReadEntity(std::vector<DxfEntity>& target);
        bool ReadPolylineVertices(DxfEntity& polyline);

        DxfTagCursor Cursor;
        std::size_t EntityCount = 0;
    };
}
