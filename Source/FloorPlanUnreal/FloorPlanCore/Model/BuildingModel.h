#pragma once

#include "Geometry/Loop.h"

#include <cstdint>
#include <string>
#include <vector>

namespace FloorPlan::Model
{
    enum class OpeningKind
    {
        Door,
        Window
    };

    enum class CirculationKind
    {
        None,
        Stair,
        Lift
    };

    /// A room that carries movement between storeys, with the axis its run follows.
    struct CirculationRegion
    {
        CirculationKind Kind = CirculationKind::None;
        std::size_t RoomIndex = 0;

        /// Ends of the run through the room's footprint, on its centreline.
        Geometry::Vec2 Start;
        Geometry::Vec2 End;
        double WidthMm = 0.0;

        /// Treads found drawn on a stair layer; zero means the run must be divided by rule.
        std::size_t DrawnTreads = 0;
    };

    struct Room
    {
        std::string Id;
        std::string Name;
        double AreaMm2 = 0.0;
        std::size_t LoopIndex = 0;
    };

    struct Wall
    {
        std::string Id;
        Geometry::Vec2 Start;
        Geometry::Vec2 End;
        double Bulge = 0.0;
        double ThicknessMm = 0.0;
        double HeightMm = 0.0;
    };

    struct Opening
    {
        std::string Id;
        OpeningKind Kind = OpeningKind::Door;
        std::string BlockName;
        Geometry::Vec2 Position;
        double WidthMm = 0.0;
        double SillHeightMm = 0.0;
        double HeadHeightMm = 0.0;
        double RotationDegrees = 0.0;
        std::string HostWallId;
    };

    enum class DimensionKind
    {
        Linear,
        Aligned,
        Angular,
        Diameter,
        Radius,
        Ordinate,
        Other
    };

    /// A measurement the draughtsman asserted, which the geometry can be checked against.
    struct Dimension
    {
        std::string Id;
        DimensionKind Kind = DimensionKind::Linear;
        std::string Layer;
        std::string TextOverride;

        Geometry::Vec2 Start;
        Geometry::Vec2 End;

        /// What the distance is, stated by the drawing where it says so and measured otherwise.
        double MeasurementMm = 0.0;

        /// What the entity's own extension line origins say it is; zero when they are absent.
        double GeometryMm = 0.0;

        /// True when the file carried a cached measurement, which many writers omit.
        bool MeasurementWasStated = false;

        /// False only when a stated measurement contradicts the geometry it is drawn against.
        bool AgreesWithGeometry = true;
    };

    /// Any block placed in the plan that is not a door or a window: furniture, fittings, plant.
    struct BlockInstance
    {
        std::string Id;
        std::string BlockName;
        std::string Layer;
        Geometry::Vec2 Position;
        double RotationDegrees = 0.0;
        double ScaleX = 1.0;
        double ScaleY = 1.0;
    };

    /// A structural column, either a block on a column layer or a small closed profile on one.
    struct Column
    {
        std::string Id;
        std::string BlockName;
        std::string Layer;
        Geometry::Vec2 Centre;
        double WidthMm = 0.0;
        double DepthMm = 0.0;
        double RotationDegrees = 0.0;

        /// Index into BuildingModel::ColumnProfiles, valid only when HasProfile is set.
        std::size_t ProfileIndex = 0;
        bool HasProfile = false;
    };

    /// A setting-out grid line, with the bubble label found nearest either end.
    struct GridLine
    {
        std::string Id;
        std::string Label;
        Geometry::Vec2 Start;
        Geometry::Vec2 End;
    };

    /// What Z the drawing carries, which for a true plan is a single level.
    struct ElevationRange
    {
        double MinimumMm = 0.0;
        double MaximumMm = 0.0;
        std::size_t DistinctLevels = 0;

        bool IsPlanar() const { return DistinctLevels <= 1; }
    };

    /// The compiled building: rooms and walls with stable identity, plus their openings.
    struct BuildingModel
    {
        std::vector<Geometry::Loop> Loops;
        std::vector<Room> Rooms;
        std::vector<Wall> Walls;
        std::vector<Opening> Openings;
        std::vector<CirculationRegion> Circulation;
        std::vector<Dimension> Dimensions;
        std::vector<BlockInstance> BlockInstances;
        std::vector<Column> Columns;

        /// Kept apart from Loops so a column drawn on its own layer is never a room boundary.
        std::vector<Geometry::Loop> ColumnProfiles;
        std::vector<GridLine> GridLines;
        std::vector<std::string> UnassignedLabels;

        ElevationRange Elevation;

        /// Hatch boundary paths built from edges this reader does not read, so areas may differ.
        std::size_t SkippedHatchPaths = 0;

        double MillimetresPerUnit = 1.0;
        bool UnitsWereDeclared = false;
        std::size_t AssembledLoops = 0;
        std::size_t DiscardedSegments = 0;
        std::size_t PairedWalls = 0;
        std::size_t ExtendedWallEnds = 0;
        std::size_t ArrangementFaces = 0;

        static constexpr std::size_t NoRoom = static_cast<std::size_t>(-1);

        double TotalRoomAreaMm2() const
        {
            double total = 0.0;
            for (const Room& room : Rooms)
            {
                total += room.AreaMm2;
            }
            return total;
        }

        /// Smallest room enclosing the point, so a room inside a room wins over its container.
        std::size_t SmallestRoomContaining(const Geometry::Vec2& point) const
        {
            std::size_t best = NoRoom;
            double bestArea = 0.0;
            for (std::size_t index = 0; index < Rooms.size(); ++index)
            {
                if (Rooms[index].LoopIndex >= Loops.size())
                {
                    continue;
                }
                const Geometry::Loop& loop = Loops[Rooms[index].LoopIndex];
                if (!loop.Contains(point))
                {
                    continue;
                }
                const double area = loop.AbsoluteArea();
                if (best == NoRoom || area < bestArea)
                {
                    best = index;
                    bestArea = area;
                }
            }
            return best;
        }

        double WallFootprintMm2 = 0.0;
    };
}
