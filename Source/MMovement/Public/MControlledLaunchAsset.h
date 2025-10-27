// Copyright (c) Miknios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MControlledLaunchAsset.generated.h"

USTRUCT(BlueprintType)
struct MMOVEMENT_API FMControlledLaunchParams
{
	GENERATED_BODY()

	// How long this launch is controlled
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	float Duration = 1;

	// How long are we blocking launched character from going back from falling to walking
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	float WalkingBlockDuration = 0;

	// Are we influencing launched character input acceleration (acceleration applied from inputs or nav mesh path following)
	UPROPERTY(EditAnywhere)
	bool bInfluenceInputAcceleration = true;

	// Do we allow full input acceleration in direction perpendicular to launch horizontal direction?
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceInputAcceleration", EditConditionHides))
	bool bAllowFullInputAccelerationPerpendicularToLaunchDirection;

	// How much are we influencing input acceleration over time (0-1)
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceInputAcceleration", EditConditionHides))
	TObjectPtr<UCurveFloat> AccelerationMultiplierCurve = nullptr;

	// Are we influencing launched character braking deceleration? (braking from no input)
	UPROPERTY(EditAnywhere)
	bool bInfluenceBreakingDeceleration = true;

	// How much are we influencing braking deceleration over time (0-1)
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceBreakingDeceleration", EditConditionHides))
	TObjectPtr<UCurveFloat> BrakingDecelerationMultiplierCurve = nullptr;

	// Are we influencing gravity applied to launched character?
	UPROPERTY(EditAnywhere)
	bool bInfluenceGravity = false;

	// How much are we influencing gravity over time (0-1) 
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceGravity", EditConditionHides))
	TObjectPtr<UCurveFloat> GravityMultiplierCurve = nullptr;

	// Disable launch when any surface is hit by character (e.g., landed on the ground)
	UPROPERTY(EditAnywhere)
	bool bDisableOnSurface = false;

	// Disable launch when horizontal speed in launch direction is below 300
	UPROPERTY(EditAnywhere)
	bool bDisableOnSpeedInHorizontalLaunchDirectionBelowThreshold = true;
};

UCLASS()
class MMOVEMENT_API UMControlledLaunchAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties))
	FMControlledLaunchParams LaunchParams;
};
