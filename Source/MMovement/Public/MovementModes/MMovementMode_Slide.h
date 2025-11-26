// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MManualTimer.h"
#include "MMovementMode_Base.h"
#include "MMovementMode_Slide.generated.h"

class UMControlledLaunchAsset;
struct FInputActionInstance;
class UInputAction;

UENUM(BlueprintType)
enum class EMMovementMode_SlidePlayerDesiredDirectionType : uint8
{
	CameraDirection,
	MovementInputDirection
};

UENUM(BlueprintType)
enum class EMMovementMode_SlideJumpOffVelocityCalculationType : uint8
{
	// Slide speed is converted into horizontal jump speed only (jump off height is constant)
	FixedVerticalSpeed,

	// Slide speed is fully converted into jump velocity in jump direction (jump off height is relative to slide speed)
	SlideVelocityFullyConvertedToJumpSpeed
};

USTRUCT(BlueprintType)
struct FMMovementMode_SlideConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	UInputAction* InputAction = nullptr;

	UPROPERTY(EditAnywhere)
	float CooldownTime = 0.5f;

	// Applied when initial slide speed converted from velocity is lower than this
	UPROPERTY(EditAnywhere, Category = "Slide Config|Speed")
	float SlideSpeedInitial = 2500;

	// Slide is ended when speed during slide is lower than this
	UPROPERTY(EditAnywhere, Category = "Slide Config|Speed")
	float SlideEndSpeedThreshold = 300;

	UPROPERTY(EditAnywhere, Category = "Slide Config|Speed")
	float DecelerationEvenSurface = 800;

	// How long after starting sliding on even surface we don't apply deceleration
	UPROPERTY(EditAnywhere, Category = "Slide Config|Speed")
	float NoDecelerationOnEvenSurfaceDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Slide Config|Speed")
	float DecelerationUpwardSlope = 3000;

	UPROPERTY(EditAnywhere, Category = "Slide Config|Speed")
	float AccelerationDownwardSlope = 3000;

	// Maximum speed we can accelerate to on downward slope
	UPROPERTY(EditAnywhere, Category = "Slide Config|Speed")
	float MaxDownwardSlopeSpeedFromAcceleration = 3000;

	// How angled surface must be to be considered a slope
	UPROPERTY(EditAnywhere, Category = "Slide Config|Slope")
	float SlopeNormalAngleMin = 20;

	// Surfaces with these tags will be excluded from slope detection
	UPROPERTY(EditAnywhere, Category = "Slide Config|Slope")
	TArray<FName> SlopeSurfaceDetectionExclusionTags = TArray<FName>();

	// Only surfaces with this tag will be included for slope detection. Leave empty to include all surfaces
	UPROPERTY(EditAnywhere, Category = "Slide Config|Slope")
	FName SlopeSurfaceDetectionRequirementTag = FName();

	UPROPERTY(EditAnywhere, Category = "Slide Config|Surface Detection")
	float SlideSurfaceDetectionMaxTraceDistance = 300;

	// Surfaces with these tags will be excluded from slidable surface detection 
	UPROPERTY(EditAnywhere, Category = "Slide Config|Surface Detection")
	TArray<FName> SlideSurfaceDetectionExclusionTags;

	// Only surfaces with this tag will be included for slidable surface detection. Leave empty to include all surfaces
	UPROPERTY(EditAnywhere, Category = "Slide Config|Surface Detection")
	FName SlideSurfaceDetectionRequirementTag = FName();

	UPROPERTY(EditAnywhere)
	float SurfaceSnapOffset = 100;

	UPROPERTY(EditAnywhere)
	float SurfaceSnapSpeed = 4;

	// How player desired slide direction is calculated
	UPROPERTY(EditAnywhere)
	EMMovementMode_SlidePlayerDesiredDirectionType PlayerDesiredDirectionType =
		EMMovementMode_SlidePlayerDesiredDirectionType::MovementInputDirection;

	// How fast direction is changed during slide. Degree per second. Use some crazy high number for instant direction change
	UPROPERTY(EditAnywhere)
	float DirectionChangeInterp = 7;

	UPROPERTY(EditAnywhere)
	bool bEnableRotationToPlayerDesiredDirection = true;

	// When sliding downward slope this will make slide direction face straight down with the slope (overrides also input direction)
	UPROPERTY(EditAnywhere)
	bool bOverrideSlideDirectionOnDownwardSlope = false;

	// Can we get into slide from falling for speed preservation?
	UPROPERTY(EditAnywhere, Category = "Slide Config|Slide From Falling")
	bool bEnableSlideFromFalling = true;

	// How close to the ground character can be to snap to the ground and start sliding immediately
	UPROPERTY(EditAnywhere, Category = "Slide Config|Slide From Falling",
		meta = (EditCondition = "bEnableSlideFromFalling", EditConditionHides))
	float SlideFromFalling_MaxDistanceFromGround = 30;

	// How long after character already landed slide input can be triggered to still get velocity preservation from falling
	UPROPERTY(EditAnywhere, Category = "Slide Config|Slide From Falling",
		meta = (EditCondition = "bEnableSlideFromFalling", EditConditionHides))
	float SlideFromFalling_PreservedVelocityGracePeriod = 0.3f;

	// Should we include vertical speed in calculating initial slide speed when sliding from falling
	UPROPERTY(EditAnywhere, Category = "Slide Config|Slide From Falling",
		meta = (EditCondition = "bEnableSlideFromFalling", EditConditionHides))
	bool bSlideFromFalling_IncludeVerticalSpeedInSlideSpeedFromFallingSpeedConversion = true;

	UPROPERTY(EditAnywhere, Category = "Slide Config|Jump Off")
	EMMovementMode_SlideJumpOffVelocityCalculationType JumpOffVelocityCalculationType =
		EMMovementMode_SlideJumpOffVelocityCalculationType::FixedVerticalSpeed;

	UPROPERTY(EditAnywhere, Category = "Slide Config|Jump Off",
		meta = (EditCondition = "JumpOffVelocityCalculationType == EMMovementMode_SlideJumpOffVelocityCalculationType::FixedVerticalSpeed"))
	float FixedJumpOffVerticalSpeed = 0;

	UPROPERTY(EditAnywhere, Category = "Slide Config|Jump Off")
	UMControlledLaunchAsset* JumpOffControlledLaunchAsset = nullptr;
};

USTRUCT(BlueprintType)
struct FMMovementMode_SlideSurfaceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	bool bValid = true;

	UPROPERTY(VisibleAnywhere)
	FVector Normal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	FVector SnapLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	bool bSlope = false;

	bool IsValid() const { return bValid; }

	bool IsSlope() const { return IsValid() && bSlope; }

	static FMMovementMode_SlideSurfaceData GetInvalid()
	{
		FMMovementMode_SlideSurfaceData SurfaceData;
		SurfaceData.bValid = false;

		return SurfaceData;
	}

	static FMMovementMode_SlideSurfaceData GetSurfaceData(const FVector& Normal, const FVector& SnapLocation, const bool bSlope)
	{
		FMMovementMode_SlideSurfaceData SurfaceData;
		SurfaceData.bValid = true;
		SurfaceData.Normal = Normal;
		SurfaceData.SnapLocation = SnapLocation;
		SurfaceData.bSlope = bSlope;

		return SurfaceData;
	}

	FVector GetSlideDirectionAlongSurfaceForDirection(const FVector& InputDirection) const
	{
		const FVector SlideDirectionAlongSurface = FVector::VectorPlaneProject(InputDirection, Normal).GetSafeNormal();
		return SlideDirectionAlongSurface;
	}

	FVector GetDownwardSlopeHorizontalDirection() const
	{
		FVector HorizontalNormalSlopeDirection = Normal;
		HorizontalNormalSlopeDirection.Z = 0;
		HorizontalNormalSlopeDirection.Normalize();

		return HorizontalNormalSlopeDirection;
	}

	bool IsDirectionForDownwardSlope(const FVector& InputDirection) const
	{
		if (!IsValid())
			return false;

		if (!IsSlope())
			return false;

		const FVector DownwardSlopeHorizontalDirection = GetDownwardSlopeHorizontalDirection();

		FVector InputHorizontalDirection = InputDirection;
		InputHorizontalDirection.Z = 0;
		InputHorizontalDirection.Normalize();

		const float Dot = DownwardSlopeHorizontalDirection.Dot(InputHorizontalDirection);
		return Dot > 0;
	}

	bool IsDirectionForUpwardSlope(const FVector& InputDirection) const
	{
		if (!IsValid())
			return false;

		if (!IsSlope())
			return false;

		const FVector UpwardSlopeHorizontalDirection = -GetDownwardSlopeHorizontalDirection();

		FVector InputHorizontalDirection = InputDirection;
		InputHorizontalDirection.Z = 0;
		InputHorizontalDirection.Normalize();

		const float Dot = UpwardSlopeHorizontalDirection.Dot(InputHorizontalDirection);
		return Dot > 0;
	}
};

USTRUCT(BlueprintType)
struct FMMovementMode_SlideRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	bool bInputHeld = false;

	UPROPERTY(VisibleAnywhere)
	FVector FallingVelocitySaved = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	double FallingVelocitySavedTime = 0;

	UPROPERTY(VisibleAnywhere)
	FMMovementMode_SlideSurfaceData SurfaceData = FMMovementMode_SlideSurfaceData::GetInvalid();

	UPROPERTY(VisibleAnywhere)
	FMManualTimer CooldownTimer;

	UPROPERTY(VisibleAnywhere)
	bool bInitialVelocityApplied = false;

	UPROPERTY(VisibleAnywhere)
	bool bAwaitsInputUp = false;

	UPROPERTY(VisibleAnywhere)
	FMManualTimer NoDecelerationOnEvenSurfaceTimer;
};

/**
 * 
 */
UCLASS()
class MMOVEMENT_API UMMovementMode_Slide : public UMMovementMode_Base
{
	GENERATED_BODY()

public:
	// ~ UMMovementMode_Base
	virtual void Initialize_Implementation() override;
	virtual void Tick_Implementation(float DeltaTime) override;
	virtual bool CanStart_Implementation(FString& OutFailReason) override;
	virtual void Start_Implementation() override;
	virtual void Phys_Implementation(float DeltaTime, int32 Iterations) override;
	virtual void End_Implementation() override;
	virtual bool CanCrouch_Implementation() override;
	virtual bool IsMovingOnGround_Implementation() override;
	virtual bool IsMovingOnSurface_Implementation() override;
	// ~ UMMovementMode_Base

protected:
	UFUNCTION()
	void OnSlideInput(const FInputActionInstance& Instance);

	bool CanStartSlideFromFalling(const FMMovementMode_SlideSurfaceData& SurfaceData) const;

	FVector GetJumpOffVector() const;

	FMMovementMode_SlideSurfaceData CalculateSlideSurfaceDataForCurrentLocation() const;

	FVector GetPlayerDesiredSlideDirection() const;

	FVector CalculateInitialSlideVelocity() const;

	bool IsSlidableSurface(UPrimitiveComponent* PrimitiveComponent) const;

	bool IsSlope(const FHitResult& HitResult) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Slide Config", meta = (ShowOnlyInnerProperties))
	FMMovementMode_SlideConfig SlideConfig;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Slide Runtime Data", meta = (ShowOnlyInnerProperties))
	FMMovementMode_SlideRuntimeData RuntimeData;

	FCollisionQueryParams TraceQueryParams;
};
