#include "FloorPlanPolygonTriangulator.h"

using UE::Geometry::FIndex3i;

namespace
{
    double SignedArea(const TArray<FVector2D>& Points)
    {
        double Total = 0.0;
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            const FVector2D& Current = Points[Index];
            const FVector2D& Next = Points[(Index + 1) % Points.Num()];
            Total += Current.X * Next.Y - Next.X * Current.Y;
        }
        return 0.5 * Total;
    }

    bool PointInTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B,
                         const FVector2D& C)
    {
        const auto Side = [](const FVector2D& L, const FVector2D& R, const FVector2D& Q) {
            return (R.X - L.X) * (Q.Y - L.Y) - (R.Y - L.Y) * (Q.X - L.X);
        };
        const double D1 = Side(A, B, P);
        const double D2 = Side(B, C, P);
        const double D3 = Side(C, A, P);
        const bool HasNegative = D1 < 0.0 || D2 < 0.0 || D3 < 0.0;
        const bool HasPositive = D1 > 0.0 || D2 > 0.0 || D3 > 0.0;
        return !(HasNegative && HasPositive);
    }
}

bool FFloorPlanPolygonTriangulator::Triangulate(const TArray<FVector2D>& Boundary,
                                                 TArray<FIndex3i>& OutTriangles)
{
    const int32 Count = Boundary.Num();
    if (Count < 3)
    {
        return false;
    }

    TArray<int32> Remaining;
    Remaining.Reserve(Count);
    const bool bCounterClockwise = SignedArea(Boundary) > 0.0;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Remaining.Add(bCounterClockwise ? Index : Count - 1 - Index);
    }

    int32 Guard = 0;
    while (Remaining.Num() > 3 && Guard < Count * Count)
    {
        bool bClipped = false;
        for (int32 Slot = 0; Slot < Remaining.Num(); ++Slot)
        {
            const int32 PreviousSlot = (Slot + Remaining.Num() - 1) % Remaining.Num();
            const int32 NextSlot = (Slot + 1) % Remaining.Num();
            const FVector2D& A = Boundary[Remaining[PreviousSlot]];
            const FVector2D& B = Boundary[Remaining[Slot]];
            const FVector2D& C = Boundary[Remaining[NextSlot]];

            const double Cross = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
            if (Cross <= 0.0)
            {
                continue;
            }

            bool bContainsOther = false;
            for (int32 Other = 0; Other < Remaining.Num(); ++Other)
            {
                if (Other == PreviousSlot || Other == Slot || Other == NextSlot)
                {
                    continue;
                }
                if (PointInTriangle(Boundary[Remaining[Other]], A, B, C))
                {
                    bContainsOther = true;
                    break;
                }
            }
            if (bContainsOther)
            {
                continue;
            }

            OutTriangles.Add(
                FIndex3i(Remaining[PreviousSlot], Remaining[Slot], Remaining[NextSlot]));
            Remaining.RemoveAt(Slot);
            bClipped = true;
            break;
        }
        if (!bClipped)
        {
            return false;
        }
        ++Guard;
    }

    if (Remaining.Num() == 3)
    {
        OutTriangles.Add(FIndex3i(Remaining[0], Remaining[1], Remaining[2]));
    }
    return OutTriangles.Num() > 0;
}
