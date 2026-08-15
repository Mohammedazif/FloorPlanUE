#pragma once

#include "CoreMinimal.h"

class UWorld;

/// Switches on the screen-space contact trace of directional lights that have it off.
class FFloorPlanContactShadows
{
public:
    /// Returns how many lights were changed; a light with any contact length set is left alone.
    static int32 EnsureOnDirectionalLights(UWorld& World);
};
