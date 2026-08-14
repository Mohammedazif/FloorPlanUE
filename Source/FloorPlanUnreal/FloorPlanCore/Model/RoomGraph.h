#pragma once

#include "Model/BuildingModel.h"

#include <cstddef>
#include <vector>

namespace FloorPlan::Model
{
    /// Two spaces separated by wall material, and the openings that join them.
    struct RoomLink
    {
        /// Index into BuildingModel::Rooms, or Outside for the world beyond the building.
        std::size_t FirstRoom = 0;
        std::size_t SecondRoom = 0;

        std::vector<std::size_t> WallIndices;
        std::vector<std::size_t> OpeningIndices;

        bool IsExterior() const;

        /// A link with no opening is a partition you cannot walk through.
        bool IsTraversable() const { return !OpeningIndices.empty(); }
    };

    /// Which rooms touch which, derived by probing the space either side of every wall.
    class RoomGraph
    {
    public:
        static constexpr std::size_t Outside = BuildingModel::NoRoom;

        static RoomGraph Build(const BuildingModel& model);

        const std::vector<RoomLink>& Links() const { return Edges; }

        /// Rooms reachable from the given one through an opening, in ascending index order.
        std::vector<std::size_t> Neighbours(std::size_t room) const;

        std::size_t ExteriorLinkCount() const;

    private:
        std::vector<RoomLink> Edges;
    };
}
