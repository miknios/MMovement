// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MMovementTypes.generated.h"

MMOVEMENT_API DECLARE_LOG_CATEGORY_EXTERN(LogMMovement, Log, All);

MMOVEMENT_API extern TAutoConsoleVariable<bool> CVarShowMovementDebugs;

UENUM(BlueprintType)
enum class EMSlopeDirection : uint8
{
	Up,
	Down,
	EvenSurface
};
