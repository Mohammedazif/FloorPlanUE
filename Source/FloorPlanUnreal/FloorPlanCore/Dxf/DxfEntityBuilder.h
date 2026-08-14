#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfEntity.h"
#include "Dxf/DxfTag.h"

namespace FloorPlan::Dxf
{
    /// Folds group-code tags into a DxfEntity, validating every value as it is read.
    class DxfEntityBuilder
    {
    public:
        static DxfEntityType Classify(const std::string& name);

        /// Returns false and fills diagnostic when a value is out of range or the entity overflows.
        static bool Apply(DxfEntity& entity, const DxfTag& tag, Diagnostic& diagnostic);

        /// Resolves whatever could only be read once the whole record was in hand.
        static bool Finish(DxfEntity& entity, Diagnostic& diagnostic);

        /// Shared with the annotation reader, which folds the tags DIMENSION and HATCH carry.
        static bool ApplyCommon(DxfEntity& entity, const DxfTag& tag, Diagnostic& diagnostic);
        static bool ApplyPolylineVertexTag(DxfEntity& entity, const DxfTag& tag,
                                           Diagnostic& diagnostic);
        static bool CheckCoordinate(double value, const DxfTag& tag, Diagnostic& diagnostic);

    private:
        static bool ApplyInsertTag(DxfEntity& entity, const DxfTag& tag, Diagnostic& diagnostic);
        static bool ApplyTextTag(DxfEntity& entity, const DxfTag& tag, Diagnostic& diagnostic);
    };
}
