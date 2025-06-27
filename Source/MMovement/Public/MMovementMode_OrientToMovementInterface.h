// Copyright (c) Miknios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MMovementMode_OrientToMovementInterface.generated.h"

UINTERFACE()
class UMMovementMode_OrientToMovementInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MMOVEMENT_API IMMovementMode_OrientToMovementInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation);
};
