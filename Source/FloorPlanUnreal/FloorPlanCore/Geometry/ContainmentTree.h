#pragma once

#include "Geometry/Loop.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Geometry
{
    struct ContainmentNode
    {
        std::size_t LoopIndex = 0;
        std::size_t Parent = static_cast<std::size_t>(-1);
        std::vector<std::size_t> Children;
        std::size_t Depth = 0;
        double NetArea = 0.0;
    };

    /// Nests closed loops by containment and gives each its area less its immediate children.
    class ContainmentTree
    {
    public:
        static ContainmentTree Build(const std::vector<Loop>& loops);

        const std::vector<ContainmentNode>& Nodes() const { return Entries; }

        static constexpr std::size_t NoParent = static_cast<std::size_t>(-1);

    private:
        std::vector<ContainmentNode> Entries;
    };
}
