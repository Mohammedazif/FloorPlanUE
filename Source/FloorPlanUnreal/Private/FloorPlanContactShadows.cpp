#include "FloorPlanContactShadows.h"

#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

THIRD_PARTY_INCLUDES_START
#include "FloorPlanLimits.h"
THIRD_PARTY_INCLUDES_END

int32 FFloorPlanContactShadows::EnsureOnDirectionalLights(UWorld& World)
{
    int32 Changed = 0;
    for (TActorIterator<AActor> Actors(&World); Actors; ++Actors)
    {
        TInlineComponentArray<UDirectionalLightComponent*> Lights;
        Actors->GetComponents(Lights);
        for (UDirectionalLightComponent* Light : Lights)
        {
            if (Light == nullptr || Light->ContactShadowLength > 0.0f)
            {
                continue;
            }
            // No setter exists for these fields; direct write is the engine's own pattern.
            Light->Modify();
            Light->ContactShadowLength =
                static_cast<float>(FloorPlan::Limits::SunContactShadowScreenFraction);
            Light->ContactShadowLengthInWS = false;
            Light->MarkRenderStateDirty();
            ++Changed;
        }
    }
    return Changed;
}
