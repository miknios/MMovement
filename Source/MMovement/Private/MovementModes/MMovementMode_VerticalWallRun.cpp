// Fill out your copyright notice in the Description page of Project Settings.


#include "MovementModes/MMovementMode_VerticalWallRun.h"

#include "MMath.h"
#include "MCharacterMovementComponent.h"
#include "MMovementTypes.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "MovementModes/MMovementMode_WallRun.h"

UMMovementMode_VerticalWallRun::UMMovementMode_VerticalWallRun()
{
	MovementModeName = TEXT("Vertical Wall Run");
}

void UMMovementMode_VerticalWallRun::Initialize_Implementation()
{
	Super::Initialize_Implementation();

	WallDetectionQueryParams.AddIgnoredActor(CharacterOwner);

	RuntimeData.CooldownTimer = FMManualTimer(ConfigData.CooldownTime);
}

void UMMovementMode_VerticalWallRun::Tick_Implementation(float DeltaTime)
{
	Super::Tick_Implementation(DeltaTime);

	SweepAndCalculateSurfaceInfo();

	RuntimeData.CooldownTimer.Tick(DeltaTime);

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		if (!RuntimeData.CooldownTimer.IsCompleted())
		{
			GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Red,
			                                 FString::Printf(
				                                 TEXT("Vertical Wall Run Cooldown: %.2f"), RuntimeData.CooldownTimer.GetTimeLeft()));
		}
	}
}

bool UMMovementMode_VerticalWallRun::CanStart_Implementation(FString& OutFailReason)
{
	if (!MovementComponent->IsFalling())
	{
		OutFailReason = TEXT("Character is not falling");
		return false;
	}

	UMMovementMode_Base* ActiveCustomMovementMode = MovementComponent->GetActiveCustomMovementModeInstance();
	if (IsValid(ActiveCustomMovementMode))
	{
		if (ActiveCustomMovementMode->IsA<UMMovementMode_WallRun>())
		{
			OutFailReason = TEXT("Wall run is active");
			return false;
		}

		if (ActiveCustomMovementMode->IsA<UMMovementMode_VerticalWallRun>())
		{
			OutFailReason = TEXT("Vertical Wall run is active");
			return false;
		}
	}

	if (!RuntimeData.CooldownTimer.IsCompleted())
	{
		OutFailReason = TEXT("Cooldown");
		return false;
	}

	if (!RuntimeData.SurfaceInfo.bValid)
	{
		OutFailReason = TEXT("Surface is not valid for vertical wall run");
		return false;
	}

	if (!IsHighEnoughFromGround())
	{
		OutFailReason = TEXT("Too close to ground");
		return false;
	}

	const FVector Velocity = MovementComponent->GetPeakTemporalHorizontalVelocity();
	FVector PeakHorizontalVelocity = Velocity;
	PeakHorizontalVelocity.Z = 0;

	if (PeakHorizontalVelocity.Size() > 100)
	{
		const float AngleBetweenSurfaceNormalAndHorizontalVelocity =
			MMath::AngleBetweenVectorsDeg(PeakHorizontalVelocity, -RuntimeData.SurfaceInfo.Normal);
		if (AngleBetweenSurfaceNormalAndHorizontalVelocity > ConfigData.HorizontalVelocityToSurfaceNormalAngleMax)
		{
			OutFailReason = FString::Printf(
				TEXT("Angle between surface normal and horizontal velocity too high (angle: %.2f, max: %.2f)"),
				AngleBetweenSurfaceNormalAndHorizontalVelocity, ConfigData.HorizontalVelocityToSurfaceNormalAngleMax);

			return false;
		}
	}

	const float AngleBetweenSurfaceNormalAndCharacterNormal =
		MMath::AngleBetweenVectorsDeg(MMath::ToHorizontalDirection(CharacterOwner->GetActorForwardVector()),
		                              -MMath::ToHorizontalDirection(RuntimeData.SurfaceInfo.Normal));
	if (AngleBetweenSurfaceNormalAndCharacterNormal > ConfigData.MaxAngleBetweenSurfaceNormalAndCharacterForwardToStart)
	{
		OutFailReason = FString::Printf(
			TEXT("Angle between surface normal and character forward too high (angle: %.2f, max: %.2f)"),
			AngleBetweenSurfaceNormalAndCharacterNormal, ConfigData.MaxAngleBetweenSurfaceNormalAndCharacterForwardToStart);

		return false;
	}

	const float HorizontalSpeed = PeakHorizontalVelocity.Size2D();
	if (HorizontalSpeed < ConfigData.MinHorizontalSpeedToStart)
	{
		OutFailReason = FString::Printf(
			TEXT("Horizontal speed too low (hspeed: %.2f, min: %.2f)"), HorizontalSpeed, ConfigData.MinHorizontalSpeedToStart);

		return false;
	}

	const float VerticalSpeed = MovementComponent->Velocity.Z;
	if (!ConfigData.bEnableSlideDown && VerticalSpeed < ConfigData.MinVerticalSpeedToStart)
	{
		OutFailReason = FString::Printf(
			TEXT("Vertical speed too low (vspeed: %.2f, min: %.2f)"), VerticalSpeed, ConfigData.MinVerticalSpeedToStart);

		return false;
	}

	return true;
}

void UMMovementMode_VerticalWallRun::Start_Implementation()
{
	Super::Start_Implementation();

	// Calculate initial speed based on InitialSpeedMode
	float SpeedInitial = 0;
	switch (ConfigData.InitialSpeedMode)
	{
	case EMCharacterMovement_VerticalWallRunInitialSpeedMode::HorizontalSpeedPreservation:
		SpeedInitial = MovementComponent->GetPeakTemporalHorizontalVelocity().Size2D();
		break;
	case EMCharacterMovement_VerticalWallRunInitialSpeedMode::VerticalSpeedPreservation:
		SpeedInitial = MovementComponent->Velocity.Z;
		break;
	case EMCharacterMovement_VerticalWallRunInitialSpeedMode::Fixed:
		SpeedInitial = ConfigData.FixedSpeedInitial;
		break;
	}

	// Cap initial speed
	if (ConfigData.bEnableSlideDown)
	{
		RuntimeData.SpeedCurrent = FMath::Max(SpeedInitial, ConfigData.MinSpeedInSlideDown);
	}
	else
	{
		RuntimeData.SpeedCurrent = FMath::Max(SpeedInitial, ConfigData.MinSpeedInitial);
	}

	RuntimeData.bSlideDownInProgress = RuntimeData.SpeedCurrent < 0;
}

void UMMovementMode_VerticalWallRun::Phys_Implementation(float DeltaTime, int32 Iterations)
{
	Super::Phys_Implementation(DeltaTime, Iterations);

	// Jump off
	if (CharacterOwner->bPressedJump)
	{
		const FVector JumpOffVelocity = GetJumpOffVelocity();

		RuntimeData.CooldownTimer.Reset();

		// Override character rotation on jump of
		if (ConfigData.bJumpOffRotateCharacterToVelocity)
		{
			const FVector CharacterForward = FVector(JumpOffVelocity.X, JumpOffVelocity.Y, 0);
			CharacterOwner->SetActorRotation(CharacterForward.ToOrientationQuat(), ETeleportType::TeleportPhysics);
		}

		MovementComponent->AddControlledLaunchFromAsset(JumpOffVelocity, ConfigData.JumpOffControlledLaunchAsset, this);
		UE_VLOG_ARROW(CharacterOwner, LogMMovement, Display, UpdatedComponent->GetComponentLocation(),
		              UpdatedComponent->GetComponentLocation() + JumpOffVelocity, FColor::Blue, TEXT("Vertical Wall Run jump off"));

		MovementComponent->OnJumpedDelegate.Broadcast();

		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	if (!CanContinue())
	{
		// TODO: why? is it for mantle?
		FVector EndingVelocity = -RuntimeData.SurfaceInfoOld.Normal;
		EndingVelocity.Z = 0;
		EndingVelocity.Normalize();
		EndingVelocity *= RuntimeData.SpeedCurrent;

		MovementComponent->Velocity = EndingVelocity;

		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	// Apply deceleration to speed
	const float SpeedMin = ConfigData.bEnableSlideDown ? ConfigData.MinSpeedInSlideDown : 0;
	RuntimeData.SpeedCurrent = FMath::Max(RuntimeData.SpeedCurrent - ConfigData.Deceleration * DeltaTime, SpeedMin);
	const FVector MovementDirection = GetAscendingDirectionAlongSurface();
	const FVector Velocity = MovementDirection * RuntimeData.SpeedCurrent;

	if (ConfigData.bEnableSlideDown && RuntimeData.SpeedCurrent < 0)
	{
		RuntimeData.bSlideDownInProgress = true;
		OnSlideDownStartedDelegate.Broadcast();
	}

	MovementComponent->Velocity = Velocity;
	MovementComponent->SetAcceleration(-MovementDirection * ConfigData.Deceleration);

	// Move along surface
	const FVector LocationDelta = MovementComponent->Velocity * DeltaTime;

	FHitResult Hit(1.f);

	MovementComponent->SafeMoveUpdatedComponent(LocationDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		MovementComponent->HandleImpact(Hit, DeltaTime, LocationDelta);
		MovementComponent->SlideAlongSurface(LocationDelta, (1.0f - Hit.Time), Hit.Normal, Hit, true);
	}

	// Snap to surface we are running on
	const FVector ToSurfaceNormal = FVector::VectorPlaneProject(-GetSurfaceNormal(), FVector::UpVector).GetSafeNormal();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();

	const FVector Difference = (GetSnapLocation() - Location).ProjectOnTo(ToSurfaceNormal);
	const FVector Offset = -GetSurfaceNormal() * (Difference.Length() - ConfigData.OffsetFromWall);

	constexpr bool bSweep = true;
	UpdatedComponent->MoveComponent(Offset * ConfigData.WallOffsetSnapSpeed * DeltaTime, Rotation, bSweep);

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		DrawDebugPoint(GetWorld(), UpdatedComponent->GetComponentLocation(), 5, FColor::White, false, 5, 5);
	}
}

void UMMovementMode_VerticalWallRun::End_Implementation()
{
	Super::End_Implementation();

	// Delay wall run after vertical wall run
	if (UMMovementMode_WallRun* WallRun =
		Cast<UMMovementMode_WallRun>(MovementComponent->GetCustomMovementModeInstance(UMMovementMode_WallRun::StaticClass())))
		WallRun->ActivateCooldown();
}

bool UMMovementMode_VerticalWallRun::IsMovingOnGround_Implementation()
{
	return false;
}

bool UMMovementMode_VerticalWallRun::IsMovingOnSurface_Implementation()
{
	return true;
}

void UMMovementMode_VerticalWallRun::AddVisualLoggerInfo(struct FVisualLogEntry* Snapshot,
                                                         FVisualLogStatusCategory& MovementCmpCategory,
                                                         FVisualLogStatusCategory& MovementModeCategory) const
{
	if (IsMovementModeActive())
	{
		MovementModeCategory.Add(TEXT("Speed"), FString::Printf(TEXT("%0.2f"), RuntimeData.SpeedCurrent));

		// Log slide down state
		if (ConfigData.bEnableSlideDown)
		{
			const FString SlideDownStateStr = FString::Printf(
				TEXT("%s"), RuntimeData.bSlideDownInProgress ? TEXT("Descending") : TEXT("Ascending"));
			MovementModeCategory.Add(TEXT("Slide Down State"), SlideDownStateStr);
		}
	}

	// Log info about calculated surface
	const FString SurfaceValidStr = RuntimeData.SurfaceInfo.bValid ? "Valid" : "Invalid";
	MovementModeCategory.Add(TEXT("Calc. Surf."), SurfaceValidStr);

	// Log info about surface hits
	for (int i = 0; i < RuntimeData.SurfaceInfo.SurfaceHitInfoArray.Num(); ++i)
	{
		auto& SurfaceHitInfo = RuntimeData.SurfaceInfo.SurfaceHitInfoArray[i];

		FString SurfaceHitValidStr = SurfaceHitInfo.bValid ? "Valid" : "Invalid";
		if (!SurfaceHitInfo.bValid)
		{
			SurfaceHitValidStr += FString::Printf(TEXT(":%s"), *SurfaceHitInfo.InvalidReason);
		}

		MovementModeCategory.Add(
			FString::Printf(TEXT("Surf. Hit Valid.(%d)"), i),
			SurfaceHitValidStr);
	}
}

void UMMovementMode_VerticalWallRun::SweepAndCalculateSurfaceInfo()
{
	auto CapsuleComponent = CharacterOwner->GetCapsuleComponent();
	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(
		CapsuleComponent->GetScaledCapsuleRadius() * ConfigData.WallDetectionCapsuleSizeMultiplier,
		CapsuleComponent->GetScaledCapsuleHalfHeight() * ConfigData.WallDetectionCapsuleSizeMultiplier);

	const FVector StartOffset = -UpdatedComponent->GetForwardVector() * 20;

	// Avoid using the same Start/End location for a Sweep, as it doesn't trigger hits on Landscapes.
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * 100;

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity,
	                                ECC_WorldStatic, CollisionShape, WallDetectionQueryParams);

	RuntimeData.SurfaceInfoOld = RuntimeData.SurfaceInfo;
	RuntimeData.SurfaceInfo = CalculateSurfaceInfo(Hits);

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		DrawDebugCapsule(GetWorld(), Start, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(),
		                 FQuat::Identity, FColor::White);
	}
}

FMCharacterMovement_VerticalWallRunSurfaceInfo UMMovementMode_VerticalWallRun::CalculateSurfaceInfo(const TArray<FHitResult>& Hits)
{
	FMCharacterMovement_VerticalWallRunSurfaceInfo SurfaceInfoResult;
	SurfaceInfoResult.SurfaceHitInfoArray.Init(FMCharacterMovement_VerticalWallRunSurfaceHitInfo(), Hits.Num());
	TArray<FMCharacterMovement_VerticalWallRunSurfaceHitInfo> SurfaceHitInfoArrayValid;

	// Filter hits to have only correct wall hits
	for (int i = 0; i < Hits.Num(); ++i)
	{
		const FHitResult& Hit = Hits[i];
		FMCharacterMovement_VerticalWallRunSurfaceHitInfo& SurfaceHitInfo = SurfaceInfoResult.SurfaceHitInfoArray[i];

		// Check if surface has wall runnable tag
		if (!ConfigData.SurfaceRequirementTag.IsNone() && !Hit.GetComponent()->ComponentHasTag(ConfigData.SurfaceRequirementTag))
		{
			SurfaceHitInfo.InvalidReason = FString::Printf(
				TEXT("Surface doesn't have required tag (tag required: %s)"), *ConfigData.SurfaceRequirementTag.ToString());

			continue;
		}

		// Check if surface doesn't have any of specified exclusion tags
		bool bExcludedFromTag = false;
		for (const FName& ExclusionTag : ConfigData.SurfaceExclusionTags)
		{
			if (Hit.GetComponent()->ComponentHasTag(ExclusionTag))
			{
				bExcludedFromTag = true;
				SurfaceHitInfo.InvalidReason = FString::Printf(
					TEXT("Surface contains exclusion tag (exclusion tag: %s)"), *ExclusionTag.ToString());

				break;
			}
		}

		if (bExcludedFromTag)
		{
			continue;
		}

		// Sweep assist hit to get correct normal and check if wall is close enough
		FHitResult AssistHit;
		const FVector Start = UpdatedComponent->GetComponentLocation();
		const FVector End = Start + MMath::FromToVectorNormalized(Start, Hit.ImpactPoint) * ConfigData.MaxDistanceFromWallToStart;
		const FCollisionShape CollisionSphere = FCollisionShape::MakeSphere(6);
		const bool bSurfaceHit = GetWorld()->SweepSingleByChannel(AssistHit, Start, End, FQuat::Identity,
		                                                          ECC_WorldStatic, CollisionSphere, WallDetectionQueryParams);
		if (!bSurfaceHit)
		{
			SurfaceHitInfo.InvalidReason = TEXT("Assist hit did not hit a surface");

			continue;
		}

		// Check surface has correct angle to run on it
		if (!IsMovementModeActive())
		{
			const float SurfaceAngle = MMath::SignedAngleBetweenVectorsDeg(FVector::UpVector, AssistHit.Normal);
			if (SurfaceAngle <= ConfigData.MinSurfaceAngle)
			{
				SurfaceHitInfo.InvalidReason = FString::Printf(TEXT("Surface angle is too low (angle: %.2f, min: %.2f)"),
				                                               SurfaceAngle,
				                                               ConfigData.MinSurfaceAngle);

				continue;
			}

			if (SurfaceAngle >= ConfigData.MaxSurfaceAngle)
			{
				SurfaceHitInfo.InvalidReason = FString::Printf(TEXT("Surface angle is too high (angle: %.2f, max: %.2f)"),
				                                               SurfaceAngle,
				                                               ConfigData.MaxSurfaceAngle);

				continue;
			}
		}
		else // Check wider angle range during wall run to allow mantle
		{
			const float SurfaceAngle = MMath::SignedAngleBetweenVectorsDeg(FVector::UpVector, AssistHit.Normal);
			constexpr float SurfaceAngleMin = 30;
			if (SurfaceAngle <= SurfaceAngleMin)
			{
				SurfaceHitInfo.InvalidReason = FString::Printf(TEXT("Surface angle is too low (mantle) (angle: %.2f, min: %.2f)"),
				                                               SurfaceAngle,
				                                               SurfaceAngleMin);

				continue;
			}

			constexpr float SurfaceAngleMax = 150;
			if (SurfaceAngle >= SurfaceAngleMax)
			{
				SurfaceHitInfo.InvalidReason = FString::Printf(TEXT("Surface angle is too high (mantle) (angle: %.2f, max: %.2f)"),
				                                               SurfaceAngle,
				                                               SurfaceAngleMax);

				continue;
			}
		}

		SurfaceHitInfo.bValid = true;
		SurfaceHitInfo.SnapLocation = AssistHit.ImpactPoint;
		SurfaceHitInfo.Normal = AssistHit.Normal;

		SurfaceHitInfoArrayValid.Add(SurfaceHitInfo);

		if (CVarShowMovementDebugs.GetValueOnGameThread())
			DrawDebugLine(GetWorld(), AssistHit.ImpactPoint, AssistHit.ImpactPoint + AssistHit.Normal * 50, FColor::Green, false, 5);
	}

	// TODO: only valid hits should be processed here. Maybe a separate array for valid hits?
	if (SurfaceHitInfoArrayValid.Num() > 0)
	{
		SurfaceInfoResult.bValid = true;

		for (const auto& Hit : SurfaceHitInfoArrayValid)
		{
			SurfaceInfoResult.SnapLocation += Hit.SnapLocation;
			SurfaceInfoResult.Normal += Hit.Normal;
		}

		SurfaceInfoResult.SnapLocation /= SurfaceHitInfoArrayValid.Num();

		// TODO: Why? Shouldn't it be divided like location?
		SurfaceInfoResult.Normal = SurfaceInfoResult.Normal.GetSafeNormal();
	}

	return SurfaceInfoResult;
}

FVector UMMovementMode_VerticalWallRun::GetAscendingDirectionAlongSurface() const
{
	FVector DirectionAlongSurface = FVector::VectorPlaneProject(MovementComponent->Velocity, GetSurfaceNormal());
	DirectionAlongSurface.X = 0;
	DirectionAlongSurface.Y = 0;
	DirectionAlongSurface.Z = FMath::Abs(DirectionAlongSurface.Z);
	DirectionAlongSurface.Normalize();

	return DirectionAlongSurface;
}

bool UMMovementMode_VerticalWallRun::IsHighEnoughFromGround() const
{
	// Check if ground is far enough
	FHitResult GroundTraceHitResult;
	bool bGroundHit = GetWorld()->LineTraceSingleByChannel(GroundTraceHitResult,
	                                                       UpdatedComponent->GetComponentLocation(),
	                                                       UpdatedComponent->GetComponentLocation()
	                                                       + FVector::DownVector * ConfigData.MinDistanceFromGround,
	                                                       ECC_WorldStatic, WallDetectionQueryParams);

	return !bGroundHit;
}

bool UMMovementMode_VerticalWallRun::CanContinue() const
{
	// Check min vertical speed only if slide down is disabled
	if (!ConfigData.bEnableSlideDown && RuntimeData.SpeedCurrent < ConfigData.MinVerticalSpeedToContinue)
		return false;

	if (!RuntimeData.SurfaceInfo.bValid)
		return false;

	if (!IsHighEnoughFromGround())
		return false;

	return true;
}

FVector UMMovementMode_VerticalWallRun::GetSurfaceNormal() const
{
	return RuntimeData.SurfaceInfo.Normal;
}

FVector UMMovementMode_VerticalWallRun::GetSnapLocation() const
{
	return RuntimeData.SurfaceInfo.SnapLocation;
}

FVector UMMovementMode_VerticalWallRun::GetJumpOffVelocity() const
{
	const FVector SurfaceNormal = GetSurfaceNormal();

	FVector HorizontalDirection = SurfaceNormal;
	HorizontalDirection.Z = 0;
	HorizontalDirection.Normalize();

	const float HorizontalSpeed = FMath::Max(RuntimeData.SpeedCurrent, ConfigData.MinJumpOffHorizontalSpeed);
	const FVector JumpOffVelocity = HorizontalDirection * HorizontalSpeed
		+ FVector::UpVector * ConfigData.FixedJumpOffVerticalSpeed;

	return JumpOffVelocity;
}
