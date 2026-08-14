#pragma once

#include "Model/BuildingModel.h"
#include "Model/RoomGraph.h"
#include "Model/StoreyLink.h"

#include <string>
#include <vector>

namespace FloorPlan::Model
{
    /// Serialises a compiled plan as JSON, keyed by the same element ids the actors carry.
    class BuildingJson
    {
    public:
        static std::string Storey(const BuildingModel& model, const RoomGraph& graph,
                                  const std::string& name, double elevationMm);

        /// One stair or lift continuing from the storey below into the one above.
        static std::string Connection(const BuildingModel& lower, const BuildingModel& upper,
                                      const std::string& lowerStorey,
                                      const std::string& upperStorey,
                                      const StoreyConnection& link);

        /// Wraps storey documents, in order, as one building.
        static std::string Building(const std::vector<std::string>& storeys,
                                    const std::vector<std::string>& connections = {});
    };
}
