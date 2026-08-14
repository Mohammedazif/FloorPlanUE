#pragma once

#include "Dxf/DxfEntity.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace FloorPlan::Dxf
{
    struct DxfLayer
    {
        std::string Name;
        int Color = 7;
        bool Frozen = false;
        bool Locked = false;
    };

    struct DxfBlock
    {
        std::string Name;
        double BaseX = 0.0;
        double BaseY = 0.0;
        bool IsExternalReference = false;
        std::vector<DxfEntity> Entities;
    };

    /// Everything a floor plan needs from a DXF file, with blocks left unexpanded.
    struct DxfDocument
    {
        std::string Version;
        int InsertUnits = 0;
        bool HasInsertUnits = false;

        std::vector<DxfEntity> ModelSpace;
        std::vector<DxfLayer> Layers;
        std::map<std::string, DxfBlock> Blocks;

        const DxfBlock* FindBlock(const std::string& name) const
        {
            const auto found = Blocks.find(name);
            return found == Blocks.end() ? nullptr : &found->second;
        }
    };

    /// Millimetres per drawing unit for a $INSUNITS value, or 0 when the value carries no scale.
    double MillimetresPerUnit(int insertUnits);
}
