#pragma once

#include "Model/BuildingModel.h"

#include <string>

namespace FloorPlan::Model
{
    /// Writes the dimension, column, grid and block sections of a storey document.
    class AnnotationJson
    {
    public:
        static void Append(std::string& out, const BuildingModel& model);
    };
}
