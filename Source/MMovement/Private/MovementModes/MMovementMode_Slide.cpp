// Fill out your copyright notice in the Description page of Project Settings.


#include "MovementModes/MMovementMode_Slide.h"

#include "EnhancedInputComponent.h"
#include "MMath.h"
#include "MCharacterMovementComponent.h"
#include "MMovementTypes.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetStringLibrary.h"

void UMMovementMode_Slide::Initialize_Implementation()
{
	Super::Initialize_Implementation();

	TraceQueryParams.AddIgnoredActor(CharacterOwner);

	RuntimeData.NoDecelerationOnEvenSurfaceTimer = FMManualTimer(SlideConfig.NoDecelerationOnEvenSurfaceDuration);
	RuntimeData.NoDecelerationOnEvenSurfaceTimer.Complete();

	RuntimeData.CooldownTimer = FMManualTimer(SlideConfig.CooldownTime);
	RuntimeData.CooldownTimer.Complete();

	// Bind input action
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(CharacterOwner->InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogMMovement, Error, TEXT("MovementMode_Dash: Player does not have Enhanced Input Component"))
		return;
	}

	EnhancedInputComponent->BindAction(SlideConfig.InputAction, ETriggerEvent::Triggered, this, &UMMovementMode_Slide::OnSlideInput);
}

void UMMovementMode_Slide::Tick_Implementation(float DeltaTime)
{
	Super::Tick_Implementation(DeltaTime);

	RuntimeData.CooldownTimer.Tick(DeltaTime);
	RuntimeData.NoDecelerationOnEvenSurfaceTimer.Tick(DeltaTime);

	if (MovementComponent->IsFalling())
	{
		RuntimeData.FallingVelocitySaved = MovementComponent->Velocity;
		RuntimeData.FallingVelocitySavedTime = GetWorld()->TimeSeconds;

		// Reset waiting for input up when got into falling
		RuntimeData.bAwaitsInputUp = false;
	}

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		if (!RuntimeData.CooldownTimer.IsCompleted())
		{
			GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Red,
			                                 FString::Printf(TEXT("Slide Cooldown: %.2f"), RuntimeData.CooldownTimer.GetTimeLeft()));
		}
	}
}

bool UMMovementMode_Slide::CanStart_Implementation(FString& OutFailReason)
{
	if (!RuntimeData.CooldownTimer.IsCompleted())
	{
		OutFailReason = TEXT("Cooldown");
		return false;
	}

	if (!RuntimeData.bInputHeld)
	{
		OutFailReason = TEXT("Input not held");
		return false;
	}

	if (RuntimeData.bAwaitsInputUp)
	{
		OutFailReason = TEXT("Awaits input up");
		return false;
	}

	// Slide direction can't be determined if we are not moving and want to slide when movement input direction is used to calculate it
	if (SlideConfig.PlayerDesiredDirectionType == EMMovementMode_SlidePlayerDesiredDirectionType::MovementInputDirection
		&& MovementComponent->Velocity.Size() <= 0)
	{
		OutFailReason = TEXT("Slide direction can't be determined from movement input");
		return false;
	}

	FMMovementMode_SlideSurfaceData SurfaceData = CalculateSlideSurfaceDataForCurrentLocation();
	if (!SurfaceData.bValid)
	{
		OutFailReason = TEXT("Surface is not valid for slide");
		return false;
	}

	// Is walking or is falling and can go from falling to sliding
	if (!(MovementComponent->IsWalking() || CanStartSlideFromFalling(SurfaceData)))
	{
		OutFailReason = TEXT("Can't start slide from falling");
		return false;
	}

	return true;
}

void UMMovementMode_Slide::Start_Implementation()
{
	Super::Start_Implementation();

	RuntimeData.bInitialVelocityApplied = false;
	RuntimeData.bAwaitsInputUp = true;
	RuntimeData.NoDecelerationOnEvenSurfaceTimer.Reset();

	if (GetWorld()->TimeSince(RuntimeData.FallingVelocitySavedTime) <= SlideConfig.SlideFromFalling_PreservedVelocityGracePeriod)
		CharacterOwner->LandedDelegate.Broadcast(FHitResult());

	CharacterOwner->Crouch();
}

void UMMovementMode_Slide::Phys_Implementation(float DeltaTime, int32 Iterations)
{
	Super::Phys_Implementation(DeltaTime, Iterations);

	// Apply initial velocity (because player input is not yet updated in Start())
	if (!RuntimeData.bInitialVelocityApplied)
	{
		MovementComponent->Velocity = CalculateInitialSlideVelocity();
		RuntimeData.bInitialVelocityApplied = true;
	}

	// Jump off
	if (CharacterOwner->bPressedJump)
	{
		const FVector JumpOffVector = GetJumpOffVector();

		if (CVarShowMovementDebugs.GetValueOnGameThread())
		{
			DrawDebugLine(GetWorld(), UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentLocation() + JumpOffVector,
			              FColor::Black, false, 10, 5, 3);
		}

		// Allow to go into slide from fall while holding slide all the time
		RuntimeData.bAwaitsInputUp = false;

		MovementComponent->AddControlledLaunchFromAsset(JumpOffVector, SlideConfig.JumpOffControlledLaunchAsset, this);

		MovementComponent->OnJumpedDelegate.Broadcast();

		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	// Check out of sliding surface
	FMMovementMode_SlideSurfaceData SlideSurfaceDataOld = CalculateSlideSurfaceDataForCurrentLocation();
	if (!SlideSurfaceDataOld.bValid)
	{
		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);

		if (CVarShowMovementDebugs.GetValueOnGameThread())
			GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Red,TEXT("Slide end: Out of sliding surface"));
	}

	// Check input not held
	if (!RuntimeData.bInputHeld)
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);

		if (CVarShowMovementDebugs.GetValueOnGameThread())
			GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Red,TEXT("Slide end: Input not held"));
	}

	// Check sufficient speed
	float SpeedOld = MovementComponent->Velocity.Size();
	if (SpeedOld < SlideConfig.SlideEndSpeedThreshold)
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);

		if (CVarShowMovementDebugs.GetValueOnGameThread())
			GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Red,TEXT("Slide end: Not sufficient speed"));
	}

	// Update slide direction
	bool bMovingInDownwardSlopeDirection = SlideSurfaceDataOld.IsDirectionForDownwardSlope(MovementComponent->Velocity);

	FVector HorizontalDirectionTarget = FVector::ZeroVector;
	if (SlideConfig.bOverrideSlideDirectionOnDownwardSlope && bMovingInDownwardSlopeDirection)
	{
		HorizontalDirectionTarget = SlideSurfaceDataOld.GetDownwardSlopeHorizontalDirection();
	}
	else if (SlideConfig.bEnableRotationToPlayerDesiredDirection)
	{
		HorizontalDirectionTarget = GetPlayerDesiredSlideDirection();
	}

	HorizontalDirectionTarget = SlideSurfaceDataOld.GetSlideDirectionAlongSurfaceForDirection(HorizontalDirectionTarget);

	// TODO: should direction change speed be dependent on current speed?
	FVector SlideDirectionRotatedToTarget = FMath::VInterpTo(MovementComponent->Velocity.GetSafeNormal(),
	                                                         HorizontalDirectionTarget.GetSafeNormal(),
	                                                         DeltaTime, SlideConfig.DirectionChangeInterp).GetSafeNormal();

	// Calculate acceleration and apply to speed
	bool bMovingInUpwardSlopeDirection = SlideSurfaceDataOld.IsDirectionForUpwardSlope(SlideDirectionRotatedToTarget);

	float SpeedNew = SpeedOld;
	float AccelerationNew = 0;
	if (bMovingInUpwardSlopeDirection)
	{
		float Deceleration = SlideConfig.DecelerationUpwardSlope;
		SpeedNew -= Deceleration * DeltaTime;
		SpeedNew = FMath::Max(SpeedNew, 0);

		AccelerationNew = -Deceleration;
	}
	else if (bMovingInDownwardSlopeDirection)
	{
		if (SpeedOld < SlideConfig.MaxDownwardSlopeSpeedFromAcceleration)
		{
			float Acceleration = SlideConfig.AccelerationDownwardSlope;
			SpeedNew += Acceleration * DeltaTime;
			SpeedNew = FMath::Min(SpeedNew, SlideConfig.MaxDownwardSlopeSpeedFromAcceleration);

			AccelerationNew = Acceleration;
		}
	}
	else // Moving on even surface
	{
		if (RuntimeData.NoDecelerationOnEvenSurfaceTimer.IsCompleted())
		{
			float Deceleration = SlideConfig.DecelerationEvenSurface;
			SpeedNew -= Deceleration * DeltaTime;

			AccelerationNew = -Deceleration;
		}
	}

	MovementComponent->Velocity = SlideDirectionRotatedToTarget * SpeedNew;
	MovementComponent->SetAcceleration(SlideDirectionRotatedToTarget * AccelerationNew);

	// Move along surface
	const FVector LocationDelta = MovementComponent->Velocity * DeltaTime;

	FHitResult Hit(1.f);

	MovementComponent->SafeMoveUpdatedComponent(LocationDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		MovementComponent->HandleImpact(Hit, DeltaTime, LocationDelta);
		MovementComponent->SlideAlongSurface(LocationDelta, (1.0f - Hit.Time), Hit.Normal, Hit, true);
	}

	// Snap to surface at new location (if there is any)
	FMMovementMode_SlideSurfaceData SurfaceDataNew = CalculateSlideSurfaceDataForCurrentLocation();
	if (SurfaceDataNew.bValid)
	{
		FVector SnapLocationDelta = MMath::FromToVector(UpdatedComponent->GetComponentLocation(), SurfaceDataNew.SnapLocation);
		bool bSweep = true;
		UpdatedComponent->MoveComponent(SnapLocationDelta * SlideConfig.SurfaceSnapSpeed * DeltaTime, UpdatedComponent->GetComponentQuat(),
		                                bSweep);
	}

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		// Show slope info
		if (SurfaceDataNew.bValid && SurfaceDataNew.bSlope)
		{
			FString SlopeDebugStr = TEXT("Slope ");
			if (bMovingInDownwardSlopeDirection)
				SlopeDebugStr += TEXT("\\/");

			if (bMovingInUpwardSlopeDirection)
				SlopeDebugStr += TEXT("/\\");

			GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Blue, SlopeDebugStr);
		}

		MovementComponent->DrawDebugDirection(SlideDirectionRotatedToTarget, FColor::White);
	}
}

void UMMovementMode_Slide::End_Implementation()
{
	Super::End_Implementation();

	RuntimeData.CooldownTimer.Reset();

	CharacterOwner->UnCrouch();
}

bool UMMovementMode_Slide::CanCrouch_Implementation()
{
	return true;
}

bool UMMovementMode_Slide::IsMovingOnGround_Implementation()
{
	return true;
}

bool UMMovementMode_Slide::IsMovingOnSurface_Implementation()
{
	return true;
}

void UMMovementMode_Slide::OnSlideInput(const FInputActionInstance& Instance)
{
	bool bInputValue = Instance.GetValue().Get<bool>();
	RuntimeData.bInputHeld = bInputValue;

	if (RuntimeData.bAwaitsInputUp && !bInputValue)
		RuntimeData.bAwaitsInputUp = false;

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Green,
		                                 FString::Printf(
			                                 TEXT("Slide input triggered. InputHeldValue = %s"),
			                                 *UKismetStringLibrary::Conv_BoolToString(bInputValue)));
	}
}

bool UMMovementMode_Slide::CanStartSlideFromFalling(const FMMovementMode_SlideSurfaceData& SurfaceData) const
{
	if (!SlideConfig.bEnableSlideFromFalling)
		return false;

	bool bSlideFromFallingGracePeriod =
		GetWorld()->TimeSince(RuntimeData.FallingVelocitySavedTime) <= SlideConfig.SlideFromFalling_PreservedVelocityGracePeriod;

	if (!(MovementComponent->IsFalling() || bSlideFromFallingGracePeriod))
		return false;

	const float DistanceFromGround = FVector::Distance(SurfaceData.SnapLocation, UpdatedComponent->GetComponentLocation());
	if (DistanceFromGround > SlideConfig.SlideFromFalling_MaxDistanceFromGround)
		return false;

	return true;
}

FVector UMMovementMode_Slide::GetJumpOffVector() const
{
	FVector HorizontalDirection = MovementComponent->Velocity;
	HorizontalDirection.Z = 0;
	HorizontalDirection.Normalize();

	FVector JumpOffVector;
	if (SlideConfig.JumpOffVelocityCalculationType ==
		EMMovementMode_SlideJumpOffVelocityCalculationType::SlideVelocityFullyConvertedToJumpSpeed)
	{
		const float JumpOffSpeed = MovementComponent->Velocity.Size();
		JumpOffVector = (HorizontalDirection + FVector::UpVector).GetSafeNormal() * JumpOffSpeed;
	}
	else if (SlideConfig.JumpOffVelocityCalculationType == EMMovementMode_SlideJumpOffVelocityCalculationType::FixedVerticalSpeed)
	{
		FVector HorizontalVelocity = MovementComponent->Velocity;
		HorizontalVelocity.Z = 0;

		JumpOffVector = HorizontalVelocity + FVector::UpVector * SlideConfig.FixedJumpOffVerticalSpeed;
	}

	return JumpOffVector;
}

FMMovementMode_SlideSurfaceData UMMovementMode_Slide::CalculateSlideSurfaceDataForCurrentLocation() const
{
	FVector TraceStart = UpdatedComponent->GetComponentLocation();
	FVector TraceEnd = UpdatedComponent->GetComponentLocation()
		+ FVector::DownVector * SlideConfig.SlideSurfaceDetectionMaxTraceDistance;

	FHitResult GroundTraceHitResult;
	bool bGroundHit = GetWorld()->LineTraceSingleByChannel(GroundTraceHitResult, TraceStart, TraceEnd,
	                                                       ECC_WorldStatic, TraceQueryParams);

	if (!bGroundHit || !IsSlidableSurface(GroundTraceHitResult.GetComponent()))
	{
		return FMMovementMode_SlideSurfaceData::GetInvalid();
	}

	const bool bSlope = IsSlope(GroundTraceHitResult);

	FVector SnapLocation = GroundTraceHitResult.ImpactPoint + FVector::UpVector * SlideConfig.SurfaceSnapOffset;
	return FMMovementMode_SlideSurfaceData::GetSurfaceData(GroundTraceHitResult.Normal, SnapLocation, bSlope);
}

FVector UMMovementMode_Slide::GetPlayerDesiredSlideDirection() const
{
	FVector PlayerDesiredDirection;
	if (SlideConfig.PlayerDesiredDirectionType == EMMovementMode_SlidePlayerDesiredDirectionType::CameraDirection)
	{
		PlayerDesiredDirection = CharacterOwner->GetControlRotation().Vector();
	}
	else if (SlideConfig.PlayerDesiredDirectionType == EMMovementMode_SlidePlayerDesiredDirectionType::MovementInputDirection)
	{
		const FVector InputVector = MovementComponent->GetMovementInputVectorLast();
		if (InputVector.SizeSquared() > 0)
		{
			PlayerDesiredDirection = InputVector.GetSafeNormal();
		}
		else
		{
			PlayerDesiredDirection = MovementComponent->Velocity.GetSafeNormal();
		}
	}

	return PlayerDesiredDirection;
}

FVector UMMovementMode_Slide::CalculateInitialSlideVelocity() const
{
	FMMovementMode_SlideSurfaceData SlideSurfaceData = CalculateSlideSurfaceDataForCurrentLocation();

	// Calculate slide direction
	FVector PlayerDesiredDirection = GetPlayerDesiredSlideDirection();
	const FVector SlideDirectionAlongSurface = SlideSurfaceData.GetSlideDirectionAlongSurfaceForDirection(PlayerDesiredDirection);

	// Calculate initial slide speed
	float SpeedCurrent;
	if (CanStartSlideFromFalling(SlideSurfaceData))
	{
		FVector FallingVelocity = RuntimeData.FallingVelocitySaved;
		if (!SlideConfig.bSlideFromFalling_IncludeVerticalSpeedInSlideSpeedFromFallingSpeedConversion)
			FallingVelocity.Z = 0;

		SpeedCurrent = FallingVelocity.Size();
	}
	else // IsWalking()
	{
		SpeedCurrent = MovementComponent->Velocity.Size();
	}

	const float SlideSpeed = FMath::Max(SpeedCurrent, SlideConfig.SlideSpeedInitial);

	// Apply to velocity
	const FVector InitialVelocity = SlideDirectionAlongSurface * SlideSpeed;

	return InitialVelocity;
}

bool UMMovementMode_Slide::IsSlidableSurface(UPrimitiveComponent* PrimitiveComponent) const
{
	// Check if surface has required tag to be considered slidable surface
	if (!SlideConfig.SlideSurfaceDetectionRequirementTag.IsNone()
		&& !PrimitiveComponent->ComponentHasTag(SlideConfig.SlideSurfaceDetectionRequirementTag))
	{
		return false;
	}

	// Exclude surface from being considered a slidable surface if it has any of the specified exclusion tags
	for (const FName& SlideSurfaceExclusionTag : SlideConfig.SlideSurfaceDetectionExclusionTags)
	{
		if (PrimitiveComponent->ComponentHasTag(SlideSurfaceExclusionTag))
		{
			return false;
		}
	}

	return true;
}

bool UMMovementMode_Slide::IsSlope(const FHitResult& HitResult) const
{
	// Check if surface has correct angle to be considered a slope
	float SurfaceAngle = MMath::AngleBetweenVectorsDeg(HitResult.Normal, FVector::UpVector);
	if (SurfaceAngle < SlideConfig.SlopeNormalAngleMin)
	{
		return false;
	}

	// Check if surface has required tag to be considered a slope
	if (!SlideConfig.SlopeSurfaceDetectionRequirementTag.IsNone()
		&& !HitResult.GetComponent()->ComponentHasTag(SlideConfig.SlopeSurfaceDetectionRequirementTag))
	{
		return false;
	}

	// Exclude surface from being considered a slope if it has any of the specified exclusion tags
	for (const FName& SlopeExclusionTag : SlideConfig.SlopeSurfaceDetectionExclusionTags)
	{
		if (HitResult.GetComponent()->ComponentHasTag(SlopeExclusionTag))
		{
			return false;
		}
	}

	return true;
}
