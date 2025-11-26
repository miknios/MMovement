// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MManualTimer.h"
#include "MMovementMode_Base.h"
#include "MMovementMode_WallRun.generated.h"

class UMControlledLaunchAsset;

struct MMOVEMENT_API FMCharacterMovement_WallRunSurfaceHitInfo
{
	FVector SnapLocation;
	FVector Normal;
	UPrimitiveComponent* PrimitiveComponent;

	FMCharacterMovement_WallRunSurfaceHitInfo() = default;

	FMCharacterMovement_WallRunSurfaceHitInfo(const FVector& SnapLocation, const FVector& Normal, UPrimitiveComponent* PrimitiveComponent)
		: SnapLocation(SnapLocation),
		  Normal(Normal),
		  PrimitiveComponent(PrimitiveComponent)
	{
	}
};

USTRUCT(BlueprintType)
struct MMOVEMENT_API FMCharacterMovement_WallRunSurfaceInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	bool bValid = false;

	UPROPERTY(VisibleAnywhere)
	FVector SnapLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	FVector Normal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	UPrimitiveComponent* PrimitiveComponent = nullptr;
};

USTRUCT(BlueprintType)
struct MMOVEMENT_API FMCharacterMovement_WallRunConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float CooldownTime = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed")
	float Acceleration = 1000;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed")
	float MaxSpeedFromAcceleration = 1000;

	// If converted speed before wall run will be capped at Max Speed From Acceleration
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed")
	bool bEnableSpeedPreservation = true;

	// How fast we decelerate to MaxSpeedFromAcceleration when speed is higher and speed preservation is disabled
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed", meta = (EditCondition = "!bEnableSpeedPreservation"))
	float DecelerationToSpeedCap = 2000;

	// Wall Run can't be started or continued if horizontal speed is lower than this
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed")
	float MinHorizontalSpeedToStart = 300;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed")
	float MinVerticalSpeedToStart = 300;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed")
	float MinVerticalSpeedToContinue = -1000;

	// Used to determine jump off speed when speed preservation is disabled
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed|Jump Off", meta = (EditCondition = "!bEnableSpeedPreservation"))
	float FixedJumpOffSpeed = 1200;

	// Used to determine jump off vertical speed when speed preservation is enabled
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed|Jump Off", meta = (EditCondition = "bEnableSpeedPreservation"))
	float FixedJumpOffVerticalSpeed = 800;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed|Jump Off")
	float JumpOffAngleHorizontalFromSurface = 45;

	// Used to determine jump off vertical angle when speed preservation is disabled
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed|Jump Off", meta = (EditCondition = "!bEnableSpeedPreservation"))
	float JumpOffAngleVertical = 45;

	// Should be override character rotation to match velocity in the moment of jump off
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed|Jump Off")
	bool bJumpOffRotateCharacterToVelocity = false;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Speed|Jump Off")
	UMControlledLaunchAsset* JumpOffControlledLaunchAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Gravity")
	bool bGravityEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Gravity", meta = (EditCondition = "bGravityEnabled", EditConditionHides))
	float Gravity = 9.81;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Gravity", meta = (EditCondition = "bGravityEnabled", EditConditionHides))
	float GravityApexTime = 2;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	float MinAngleBetweenSurfaceNormalAndCharacterForwardToStart = 0;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	float MaxAngleBetweenSurfaceNormalAndCharacterForwardToStart = 0;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	bool bCanStartWallRunningBackwards = false;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	float WallRunnableSurfaceNormalAngleMin = -40;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	float WallRunnableSurfaceNormalAngleMax = 10;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	float MinDistanceFromGround = 100;

	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	float WallDetectionCapsuleSizeMultiplier = 2;

	// Surface with any of these tags will be excluded from wall run
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	TArray<FName> SurfaceExclusionTags = TArray<FName>();

	// Only surfaces with this tag will be included for wall run. Leave empty to include all surfaces
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	FName WallRunnableSurfaceTag = FName();

	// Wall run will be stopped when angle between current and previous surface normal was higher than this
	UPROPERTY(EditAnywhere, Category = "Wall Run Config|Surface Detection")
	float MaxSurfaceNormalAngleChangeToContinue = 60;

	UPROPERTY(EditAnywhere)
	float OffsetFromWall = 45;

	UPROPERTY(EditAnywhere)
	float WallOffsetSnapSpeed = 4;
};

USTRUCT(BlueprintType)
struct MMOVEMENT_API FMCharacterMovement_WallRunRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FMManualTimer CooldownTimer = FMManualTimer();

	UPROPERTY(VisibleAnywhere)
	FMManualTimer GravityApexTimer = FMManualTimer();

	UPROPERTY(VisibleAnywhere)
	float HorizontalSpeed = 0;

	UPROPERTY(VisibleAnywhere)
	FMCharacterMovement_WallRunSurfaceInfo SurfaceInfo = FMCharacterMovement_WallRunSurfaceInfo();

	UPROPERTY(VisibleAnywhere)
	FMCharacterMovement_WallRunSurfaceInfo SurfaceInfoOld = FMCharacterMovement_WallRunSurfaceInfo();
};

UENUM(BlueprintType)
enum class EMWallRunWallSide : uint8
{
	None, // When Wall Run is not active
	Left,
	Right
};

/**
 * 
 */
UCLASS()
class MMOVEMENT_API UMMovementMode_WallRun : public UMMovementMode_Base
{
	GENERATED_BODY()

public:
	UMMovementMode_WallRun();

	// UMMovementMode_Base
	virtual void Initialize_Implementation() override;
	virtual void Tick_Implementation(float DeltaTime) override;
	virtual bool CanStart_Implementation(FString& OutFailReason) override;
	virtual void Start_Implementation() override;
	virtual void Phys_Implementation(float DeltaTime, int32 Iterations) override;
	virtual void End_Implementation() override;
	virtual bool IsMovingOnGround_Implementation() override;
	virtual bool IsMovingOnSurface_Implementation() override;
	// ~ UMMovementMode_Base


	UFUNCTION(BlueprintCallable)
	EMWallRunWallSide GetWallRunWallSide() const;

	UFUNCTION(BlueprintCallable)
	void ActivateCooldown();

protected:
	// UMMovementMode_Base
#if ENABLE_VISUAL_LOG
	virtual void AddVisualLoggerInfo(FVisualLogEntry* Snapshot,
	                                 FVisualLogStatusCategory& MovementCmpCategory,
	                                 FVisualLogStatusCategory& MovementModeCategory) const override;
#endif
	// ~ UMMovementMode_Base

	virtual bool CanContinue(FString& OutFailReason) const;

	void SweepAndCalculateSurfaceInfo();

	FMCharacterMovement_WallRunSurfaceInfo CalculateSurfaceInfo(const TArray<FHitResult>& Hits);

	FVector GetSurfaceNormal() const;

	FVector GetSnapLocation() const;

	FVector GetHorizontalDirectionAlongSurfaceFromVelocity() const;

	float GetHorizontalSpeedFromVelocity() const;

	FVector GetWallRunHorizontalVelocityAlongSurface() const;

	FVector GetJumpOffVelocity() const;

	bool IsHighEnoughFromGround() const;

protected:
	UPROPERTY(EditAnywhere, Category = "Wall Run Config", meta = (ShowOnlyInnerProperties))
	FMCharacterMovement_WallRunConfig ConfigData;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Wall Run Runtime Data", meta = (ShowOnlyInnerProperties))
	FMCharacterMovement_WallRunRuntimeData RuntimeData;

	FCollisionQueryParams WallDetectionQueryParams;
};
