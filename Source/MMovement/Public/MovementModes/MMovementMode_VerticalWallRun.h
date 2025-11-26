// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MManualTimer.h"
#include "MMovementMode_Base.h"
#include "MUtilityTypes.h"
#include "MMovementMode_VerticalWallRun.generated.h"

class FMDynamicMulticastDelegateSignature;
class UMControlledLaunchAsset;

UENUM(BlueprintType, DisplayName = "Vertical Wall Run Initial Speed Mode")
enum class EMCharacterMovement_VerticalWallRunInitialSpeedMode : uint8
{
	HorizontalSpeedPreservation,
	VerticalSpeedPreservation,
	Fixed
};

USTRUCT(BlueprintType)
struct FMCharacterMovement_VerticalWallRunConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CooldownTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Speed")
	EMCharacterMovement_VerticalWallRunInitialSpeedMode InitialSpeedMode =
		EMCharacterMovement_VerticalWallRunInitialSpeedMode::HorizontalSpeedPreservation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Speed",
		meta = (EditCondition = "InitialSpeedMode == EMCharacterMovement_VerticalWallRunInitialSpeedMode::Fixed"))
	float FixedSpeedInitial = 1500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Speed")
	float Deceleration = 1000;

	// Applied at the start if it would be lower otherwise
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Speed")
	float MinSpeedInitial = 1000;

	// Can't start running on a wall below this speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Speed")
	float MinHorizontalSpeedToStart = 400;

	// Can't start running on a wall below this speed. Valid only if bEnableSlideDown is disabled
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Speed", meta = (EditCondition = "!bEnableSlideDown"))
	float MinVerticalSpeedToStart = 200;

	// Can't continue running on a wall below this speed. Valid only if bEnableSlideDown is disabled
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Speed", meta = (EditCondition = "!bEnableSlideDown"))
	float MinVerticalSpeedToContinue = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Jump Off")
	float FixedJumpOffVerticalSpeed = 800;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Jump Off")
	float MinJumpOffHorizontalSpeed = 500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Jump Off")
	UMControlledLaunchAsset* JumpOffControlledLaunchAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Jump Off")
	bool bJumpOffRotateCharacterToVelocity = false;

	// Continue vertical wall run when vertical velocity is below 0 by sliding down on the surface
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Slide Down")
	bool bEnableSlideDown = false;

	// Vertical speed will be capped to this value at the beginning of the slide down
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Slide Down")
	float MinSpeedInitialInSlideDown = 0;

	// Vertical speed will be capped to this value during slide down
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Slide Down")
	float MinSpeedInSlideDown = -500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	float HorizontalVelocityToSurfaceNormalAngleMax = 15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	float MaxAngleBetweenSurfaceNormalAndCharacterForwardToStart = 70;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	float MinDistanceFromGround = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	float MinSurfaceAngle = -15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	float MaxSurfaceAngle = 15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	float WallDetectionCapsuleSizeMultiplier = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	float MaxDistanceFromWallToStart = 0;

	// Surface with any of these tags will be excluded from wall run
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	TArray<FName> SurfaceExclusionTags = TArray<FName>();

	// Only surfaces with this tag will be included for vertical wall run. Leave empty to include all surfaces
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Detection")
	FName SurfaceRequirementTag = FName();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Offset")
	float OffsetFromWall = 45;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config|Surface Offset")
	float WallOffsetSnapSpeed = 4;
};

struct FMCharacterMovement_VerticalWallRunSurfaceHitInfo
{
	bool bValid = false;
	FVector SnapLocation = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	FString Name = TEXT("None");
	FString InvalidReason = FString();
};

struct FMCharacterMovement_VerticalWallRunSurfaceInfo
{
	bool bValid = false;
	FVector SnapLocation = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;

	// Query hits that were included to calculate final surface data
	TArray<FMCharacterMovement_VerticalWallRunSurfaceHitInfo> SurfaceHitInfoArray;
};

USTRUCT(BlueprintType)
struct FMCharacterMovement_VerticalWallRunRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FMManualTimer CooldownTimer = FMManualTimer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float SpeedCurrent = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bSlideDownInProgress = false;

	FMCharacterMovement_VerticalWallRunSurfaceInfo SurfaceInfo;

	FMCharacterMovement_VerticalWallRunSurfaceInfo SurfaceInfoOld;
};

/**
 * 
 */
UCLASS()
class MMOVEMENT_API UMMovementMode_VerticalWallRun : public UMMovementMode_Base
{
	GENERATED_BODY()

public:
	UMMovementMode_VerticalWallRun();

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

protected:
	// UMMovementMode_Base
#if ENABLE_VISUAL_LOG
	virtual void AddVisualLoggerInfo(struct FVisualLogEntry* Snapshot,
	                                 FVisualLogStatusCategory& MovementCmpCategory,
	                                 FVisualLogStatusCategory& MovementModeCategory) const override;
#endif
	// ~ UMMovementMode_Base

	void SweepAndCalculateSurfaceInfo();

	FMCharacterMovement_VerticalWallRunSurfaceInfo CalculateSurfaceInfo(const TArray<FHitResult>& Hits);

	FVector GetAscendingDirectionAlongSurface() const;

	bool IsHighEnoughFromGround() const;

	bool CanContinue() const;

	FVector GetSurfaceNormal() const;

	FVector GetSnapLocation() const;

	FVector GetJumpOffVelocity() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical Wall Run Config", meta = (ShowOnlyInnerProperties))
	FMCharacterMovement_VerticalWallRunConfig ConfigData;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = "Vertical Wall Run Runtime Data", meta = (ShowOnlyInnerProperties))
	FMCharacterMovement_VerticalWallRunRuntimeData RuntimeData;

	FCollisionQueryParams WallDetectionQueryParams;

public:
	UPROPERTY(BlueprintAssignable, Category = "Vertical Wall Run Config")
	FMDynamicMulticastDelegateSignature OnSlideDownStartedDelegate;
};
