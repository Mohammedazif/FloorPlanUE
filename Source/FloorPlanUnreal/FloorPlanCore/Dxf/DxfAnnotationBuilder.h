#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfEntity.h"
#include "Dxf/DxfTag.h"

namespace FloorPlan::Dxf
{
    /// Folds the tags a DIMENSION or a HATCH carries, which share no codes with the shapes.
    class DxfAnnotationBuilder
    {
    public:
        static bool ApplyDimensionTag(DxfEntity& entity, const DxfTag& tag,
                                      Diagnostic& diagnostic);

        static bool ApplyHatchTag(DxfEntity& entity, const DxfTag& tag, Diagnostic& diagnostic);
    };
}
