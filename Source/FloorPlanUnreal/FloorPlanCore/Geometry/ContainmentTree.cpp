#include "Geometry/ContainmentTree.h"

#include <algorithm>

namespace FloorPlan::Geometry
{
    ContainmentTree ContainmentTree::Build(const std::vector<Loop>& loops)
    {
        ContainmentTree tree;
        tree.Entries.resize(loops.size());
        for (std::size_t index = 0; index < loops.size(); ++index)
        {
            tree.Entries[index].LoopIndex = index;
            tree.Entries[index].NetArea = loops[index].AbsoluteArea();
        }

        for (std::size_t child = 0; child < loops.size(); ++child)
        {
            Vec2 probe;
            if (!loops[child].InteriorPoint(probe))
            {
                continue;
            }
            std::size_t best = NoParent;
            double bestArea = 0.0;
            for (std::size_t parent = 0; parent < loops.size(); ++parent)
            {
                if (parent == child)
                {
                    continue;
                }
                const double parentArea = loops[parent].AbsoluteArea();
                if (parentArea <= loops[child].AbsoluteArea())
                {
                    continue;
                }
                if (!loops[parent].BoundingBoxContains(loops[child]))
                {
                    continue;
                }
                if (!loops[parent].Contains(probe))
                {
                    continue;
                }
                if (best == NoParent || parentArea < bestArea)
                {
                    best = parent;
                    bestArea = parentArea;
                }
            }
            tree.Entries[child].Parent = best;
        }

        for (std::size_t index = 0; index < tree.Entries.size(); ++index)
        {
            const std::size_t parent = tree.Entries[index].Parent;
            if (parent != NoParent)
            {
                tree.Entries[parent].Children.push_back(index);
                tree.Entries[parent].NetArea -= loops[index].AbsoluteArea();
            }
        }

        for (std::size_t index = 0; index < tree.Entries.size(); ++index)
        {
            std::size_t depth = 0;
            std::size_t walk = tree.Entries[index].Parent;
            while (walk != NoParent && depth <= tree.Entries.size())
            {
                ++depth;
                walk = tree.Entries[walk].Parent;
            }
            tree.Entries[index].Depth = depth;
        }
        return tree;
    }
}
