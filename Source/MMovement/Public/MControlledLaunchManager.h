// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MCharacterMovementComponent.h"
#include "MControlledLaunchAsset.h"
#include "MManualTimer.h"
#include "UObject/Object.h"
#include "MControlledLaunchManager.generated.h"

class UMCharacterMovementComponent;

USTRUCT()
struct FMControlledLaunchManager_ProcessResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	float BrakingDecelerationMultiplier = 1;

	UPROPERTY(VisibleAnywhere)
	float GravityMultiplier = 1;

	FMControlledLaunchManager_ProcessResult() = default;

	FMControlledLaunchManager_ProcessResult(const FVector& Acceleration)
		: Acceleration(Acceleration),
		  BrakingDecelerationMultiplier(1),
		  GravityMultiplier(1)
	{
	}
};

USTRUCT(BlueprintType)
struct FMControlledLaunchManager_LaunchInstance
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FMManualTimer DurationTimer;

	UPROPERTY(VisibleAnywhere)
	FMManualTimer WalkingBlockTimer;

	UPROPERTY(VisibleAnywhere)
	FMControlledLaunchParams LaunchParams;

	UPROPERTY(VisibleAnywhere)
	FVector LaunchVelocity;

	FMControlledLaunchManager_LaunchInstance() = default;

	FMControlledLaunchManager_LaunchInstance(const FMControlledLaunchParams& LaunchParams, const FVector& LaunchVelocity)
		: DurationTimer(FMManualTimer(LaunchParams.Duration)),
		  WalkingBlockTimer(FMManualTimer(LaunchParams.WalkingBlockDuration)),
		  LaunchParams(LaunchParams),
		  LaunchVelocity(LaunchVelocity)
	{
	}

	void ProcessAndCombine(FMControlledLaunchManager_ProcessResult& ProcessResult) const;
};

class UCharacterMovementComponent;
/**
 * 
 */
UCLASS(BlueprintType)
class MMOVEMENT_API UMControlledLaunchManager : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	bool IsAnyControlledLaunchActive() const;

	UFUNCTION(BlueprintCallable)
	bool IsWalkBlockedByControlledLaunch() const;

	void Initialize(UMCharacterMovementComponent* InOwnerMovementComponent);

	void TickLaunches(float DeltaTime);

	void AddControlledLaunch(const FVector& LaunchVelocity, const FMControlledLaunchParams& LaunchParams, UObject* Owner);

	UFUNCTION(BlueprintCallable)
	void ClearAllLaunches();

	FMControlledLaunchManager_ProcessResult Process(const FVector& AccelerationCurrent);

protected:
	void TickLaunchInstance(FMControlledLaunchManager_LaunchInstance& LaunchInstance, float DeltaTime);

	bool ShouldRemoveLaunchInstance(const FMControlledLaunchManager_LaunchInstance& LaunchInstance) const;

protected:
	UPROPERTY(EditDefaultsOnly)
	float ControlledLaunchSpeedThreshold = 300;

	UPROPERTY()
	UMCharacterMovementComponent* OwnerMovementComponent;

	UPROPERTY()
	TArray<FMControlledLaunchManager_LaunchInstance> LaunchInstancesWithoutOwner;

	UPROPERTY()
	TMap<UObject*, FMControlledLaunchManager_LaunchInstance> LaunchInstanceForOwner;
};
