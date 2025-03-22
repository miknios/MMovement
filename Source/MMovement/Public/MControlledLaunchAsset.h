// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MControlledLaunchAsset.generated.h"

USTRUCT(BlueprintType)
struct FMControlledLaunchParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	float Duration = 1;

	UPROPERTY(EditAnywhere, meta = (ClampMin = 0))
	float WalkingBlockDuration = 0;

	UPROPERTY(EditAnywhere)
	bool bInfluenceInputAcceleration = true;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceInputAcceleration", EditConditionHides))
	bool bAllowFullInputAccelerationPerpendicularToLaunchDirection;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceInputAcceleration", EditConditionHides))
	TObjectPtr<UCurveFloat> AccelerationMultiplierCurve = nullptr;

	UPROPERTY(EditAnywhere)
	bool bInfluenceBreakingDeceleration = true;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceBreakingDeceleration", EditConditionHides))
	TObjectPtr<UCurveFloat> BrakingDecelerationMultiplierCurve = nullptr;

	UPROPERTY(EditAnywhere)
	bool bInfluenceGravity = false;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bInfluenceGravity", EditConditionHides))
	TObjectPtr<UCurveFloat> GravityMultiplierCurve = nullptr;

	UPROPERTY(EditAnywhere)
	bool bDisableOnSurface = false;

	UPROPERTY(EditAnywhere)
	bool bDisableOnSpeedInHorizontalLaunchDirectionBelowThreshold = true;
};

/**
 * 
 */
UCLASS()
class MMOVEMENT_API UMControlledLaunchAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (ShowOnlyInnerProperties))
	FMControlledLaunchParams LaunchParams;
};
