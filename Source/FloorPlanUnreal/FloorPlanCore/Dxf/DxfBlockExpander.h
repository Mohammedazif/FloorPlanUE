#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfDocument.h"

#include <vector>

namespace FloorPlan::Dxf
{
    /// Flattens block references into world-space entities, refusing cycles and runaway nesting.
    class DxfBlockExpander
    {
    public:
        /// Unresolvable block names are skipped rather than failing; cycles are a hard failure.
        static bool Expand(const DxfDocument& document, std::vector<DxfEntity>& output,
                           Diagnostic& diagnostic);

        /// Reports a cycle anywhere in the block table, whether or not model space reaches it.
        static bool ValidateBlockGraph(const DxfDocument& document, Diagnostic& diagnostic);

        static std::size_t UnresolvedReferenceCount(const DxfDocument& document);
    };
}
