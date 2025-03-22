// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MMovementTypes.generated.h"

MMOVEMENT_API DECLARE_LOG_CATEGORY_EXTERN(LogMMovement, Log, All);

MMOVEMENT_API extern TAutoConsoleVariable<bool> CVarShowMovementDebugs;

inline FName WallRunnableTagName = TEXT("WR");

UENUM(BlueprintType)
enum EMCustomMovementMode : uint8
{
	CMOVE_WallRun = 0 UMETA(DisplayName = "Wall Run"),
	CMOVE_Dash = 1 UMETA(DisplayName = "Dash"),
	CMOVE_Slide = 2 UMETA(DisplayName = "Slide"),
	CMOVE_VerticalWallRun = 3 UMETA(DisplayName = "Vertical Wall Run"),
};

UENUM(BlueprintType)
enum class EMSlopeDirection : uint8
{
	Up,
	Down,
	EvenSurface
};
