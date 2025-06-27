// Fill out your copyright notice in the Description page of Project Settings.


#include "MovementModes/MMovementMode_WallRun.h"

#include "MCharacterMovementComponent.h"
#include "MMath.h"
#include "MMovementTypes.h"
#include "MString.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

UMMovementMode_WallRun::UMMovementMode_WallRun()
{
	MovementModeName = TEXT("Wall Run");
}

void UMMovementMode_WallRun::Initialize_Implementation()
{
	Super::Initialize_Implementation();

	WallDetectionQueryParams.AddIgnoredActor(CharacterOwner);
	WallDetectionQueryParams.bIgnoreTouches = true;

	RuntimeData.CooldownTimer = FMManualTimer(ConfigData.CooldownTime);
	RuntimeData.CooldownTimer.Complete();

	RuntimeData.GravityApexTimer = FMManualTimer(ConfigData.GravityApexTime);
	RuntimeData.GravityApexTimer.Complete();
}

void UMMovementMode_WallRun::Tick_Implementation(float DeltaTime)
{
	Super::Tick_Implementation(DeltaTime);

	SweepAndCalculateSurfaceInfo();

	RuntimeData.CooldownTimer.Tick(DeltaTime);
}

bool UMMovementMode_WallRun::CanStart_Implementation(FString& OutFailReason)
{
	if (!MovementComponent->IsFalling())
	{
		OutFailReason = TEXT("Character is not falling");
		return false;
	}

	UMMovementMode_Base* ActiveCustomMovementModeInstance = MovementComponent->GetActiveCustomMovementModeInstance();
	if (IsValid(ActiveCustomMovementModeInstance))
	{
		if (ActiveCustomMovementModeInstance->IsA<UMMovementMode_WallRun>())
		{
			OutFailReason = TEXT("Wall Run is active");
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
		OutFailReason = TEXT("Surface is not valid for wall run");
		return false;
	}

	const float HorizontalSpeed = MovementComponent->GetPeakTemporalHorizontalVelocity().Size();
	if (HorizontalSpeed < ConfigData.MinHorizontalSpeedToStart)
	{
		OutFailReason = FString::Printf(
			TEXT("Horizontal speed is too low (hspeed: %.2f, min: %.2f)"), HorizontalSpeed, ConfigData.MinHorizontalSpeedToStart);

		return false;
	}

	const float VerticalSpeed = MovementComponent->Velocity.Z;
	if (VerticalSpeed < ConfigData.MinVerticalSpeedToStart)
	{
		OutFailReason = FString::Printf(
			TEXT("Vertical speed is too low (vspeed: %.2f, min: %.2f)"), VerticalSpeed, ConfigData.MinVerticalSpeedToStart);

		return false;
	}

	if (!IsHighEnoughFromGround())
	{
		OutFailReason = TEXT("Too close to ground");
		return false;
	}

	const float AngleBetweenCharacterNormalAndSurfaceNormal = MMath::AngleBetweenVectorsDeg(
		-MMath::ToHorizontalDirection(RuntimeData.SurfaceInfo.Normal),
		MMath::ToHorizontalDirection(CharacterOwner->GetActorForwardVector()));

	if (AngleBetweenCharacterNormalAndSurfaceNormal < ConfigData.MinAngleBetweenSurfaceNormalAndCharacterForwardToStart)
	{
		OutFailReason = FString::Printf(
			TEXT("Angle between character forward and surface normal too low (angle: %.2f, min: %.2f)"),
			AngleBetweenCharacterNormalAndSurfaceNormal, ConfigData.MinAngleBetweenSurfaceNormalAndCharacterForwardToStart);

		return false;
	}

	if (AngleBetweenCharacterNormalAndSurfaceNormal > ConfigData.MaxAngleBetweenSurfaceNormalAndCharacterForwardToStart)
	{
		OutFailReason = FString::Printf(
			TEXT("Angle between character forward and surface normal too high (angle: %.2f, max: %.2f)"),
			AngleBetweenCharacterNormalAndSurfaceNormal, ConfigData.MaxAngleBetweenSurfaceNormalAndCharacterForwardToStart);

		return false;
	}

	if (!ConfigData.bCanStartWallRunningBackwards)
	{
		if (FVector::DotProduct(MMath::ToHorizontalDirection(CharacterOwner->GetActorForwardVector()),
		                        MMath::ToHorizontalDirection(MovementComponent->Velocity)) < 0)
		{
			OutFailReason = TEXT("Can't wall run backwards, because it's not activated in config");

			return false;
		}
	}

	return true;
}

bool UMMovementMode_WallRun::CanContinue(FString& OutFailReason) const
{
	if (!RuntimeData.SurfaceInfo.bValid)
	{
		OutFailReason = TEXT("Surface is not valid for wall run");
		return false;
	}

	const float VerticalSpeed = MovementComponent->Velocity.Z;
	if (VerticalSpeed < ConfigData.MinVerticalSpeedToContinue)
	{
		OutFailReason = FString::Printf(
			TEXT("Vertical speed is too low (vspeed: %.2f, min: %.2f)"), VerticalSpeed, ConfigData.MinVerticalSpeedToContinue);
		return false;
	}

	if (!IsHighEnoughFromGround())
	{
		OutFailReason = TEXT("Too close to ground");
		return false;
	}

	const float SurfaceNormalDeltaAngle = MMath::AngleBetweenVectorsDeg(RuntimeData.SurfaceInfo.Normal, RuntimeData.SurfaceInfoOld.Normal);
	if (SurfaceNormalDeltaAngle > ConfigData.MaxSurfaceNormalAngleChangeToContinue)
	{
		OutFailReason = FString::Printf(TEXT("Surface normal angle delta too high (angle: %.2f, max: %.2f)"),
		                                SurfaceNormalDeltaAngle,
		                                ConfigData.MaxSurfaceNormalAngleChangeToContinue);
		return false;
	}

	return true;
}

void UMMovementMode_WallRun::Start_Implementation()
{
	Super::Start_Implementation();

	RuntimeData.GravityApexTimer = ConfigData.GravityApexTime;
	RuntimeData.HorizontalSpeed = MovementComponent->GetPeakTemporalHorizontalVelocity().Size2D();
}

void UMMovementMode_WallRun::Phys_Implementation(float DeltaTime, int32 Iterations)
{
	Super::Phys_Implementation(DeltaTime, Iterations);

	if (DeltaTime < UCharacterMovementComponent::MIN_TICK_TIME)
		return;

	// Set Custom Movement Base
	MovementComponent->SetCustomMovementBase(RuntimeData.SurfaceInfo.PrimitiveComponent);

	// Jump off
	if (CharacterOwner->bPressedJump)
	{
		const FVector JumpOffVelocity = GetJumpOffVelocity();

		// Override character rotation on jump of
		if (ConfigData.bJumpOffRotateCharacterToVelocity)
		{
			const FVector CharacterForward = FVector(JumpOffVelocity.X, JumpOffVelocity.Y, 0);
			CharacterOwner->SetActorRotation(CharacterForward.ToOrientationQuat(), ETeleportType::TeleportPhysics);
		}

		ActivateCooldown();

		MovementComponent->AddControlledLaunchFromAsset(JumpOffVelocity, ConfigData.JumpOffControlledLaunchAsset, this);
		UE_VLOG_ARROW(CharacterOwner, LogMMovement, Display, UpdatedComponent->GetComponentLocation(),
		              UpdatedComponent->GetComponentLocation() + JumpOffVelocity, FColor::Blue, TEXT("Wall Run jump off"));

		MovementComponent->OnJumpedDelegate.Broadcast();

		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	FString CanContinueFailReason;
	if (!CanContinue(CanContinueFailReason))
	{
		UE_VLOG(CharacterOwner, LogMMovement, Display, TEXT("%s CanContinue fail reason: %s"),
		        *MovementModeName.ToString(),
		        *CanContinueFailReason);

		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	// Compute velocity
	const FVector HorizontalDirection = GetHorizontalDirectionAlongSurfaceFromVelocity();

	float HorizontalAcceleration = 0;
	if (RuntimeData.HorizontalSpeed < ConfigData.MaxSpeedFromAcceleration)
	{
		RuntimeData.HorizontalSpeed = FMath::Min(RuntimeData.HorizontalSpeed + ConfigData.Acceleration * DeltaTime,
		                                         ConfigData.MaxSpeedFromAcceleration);

		HorizontalAcceleration = ConfigData.Acceleration;
	}
	else if (!ConfigData.bEnableSpeedPreservation)
	{
		if (RuntimeData.HorizontalSpeed > ConfigData.MaxSpeedFromAcceleration)
		{
			RuntimeData.HorizontalSpeed = FMath::Max(RuntimeData.HorizontalSpeed - ConfigData.DecelerationToSpeedCap * DeltaTime,
			                                         ConfigData.MaxSpeedFromAcceleration);

			HorizontalAcceleration = -ConfigData.DecelerationToSpeedCap;
		}
	}

	const FVector HorizontalSurfaceVelocity = HorizontalDirection * RuntimeData.HorizontalSpeed;

	// Apply wall run gravity
	float VerticalSpeed = 0;
	if (ConfigData.bGravityEnabled)
	{
		VerticalSpeed = MovementComponent->Velocity.Z;

		float GravityToApply = ConfigData.Gravity * DeltaTime;
		if (VerticalSpeed < GravityToApply
			&& !RuntimeData.GravityApexTimer.IsCompleted())
		{
			RuntimeData.GravityApexTimer.Tick(DeltaTime);
			VerticalSpeed = 0;

			if (CVarShowMovementDebugs.GetValueOnGameThread())
			{
				GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Blue,
				                                 FString::Printf(TEXT("Wall Run Apex %.1f"), RuntimeData.GravityApexTimer.GetTimeLeft()));
			}
		}
		else
		{
			VerticalSpeed -= ConfigData.Gravity * DeltaTime;
		}
	}

	const FVector VelocityProjectedScaled = HorizontalSurfaceVelocity + FVector::UpVector * VerticalSpeed;

	MovementComponent->Velocity = VelocityProjectedScaled;
	MovementComponent->SetAcceleration(HorizontalAcceleration * HorizontalDirection);

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

void UMMovementMode_WallRun::End_Implementation()
{
	Super::End_Implementation();
}

bool UMMovementMode_WallRun::IsMovingOnGround_Implementation()
{
	return false;
}

bool UMMovementMode_WallRun::IsMovingOnSurface_Implementation()
{
	return true;
}

#if ENABLE_VISUAL_LOG
void UMMovementMode_WallRun::AddVisualLoggerInfo(struct FVisualLogEntry* Snapshot,
                                                 FVisualLogStatusCategory& MovementCmpCategory,
                                                 FVisualLogStatusCategory& MovementModeCategory) const
{
	MovementModeCategory.Add(TEXT("Cooldown"),
	                         FString::Printf(TEXT("%.2f/%.2f"),
	                                         RuntimeData.CooldownTimer.GetTimeElapsed(),
	                                         RuntimeData.CooldownTimer.GetDuration()));

	if (IsMovementModeActive())
	{
		MovementModeCategory.Add(TEXT("Side"),
		                         FString::Printf(TEXT("%s"),
		                                         *UMString::EnumToString(GetWallRunWallSide())));


		MovementModeCategory.Add(TEXT("Gravity Apex"),
		                         FString::Printf(TEXT("%.2f/%.2f"),
		                                         RuntimeData.GravityApexTimer.GetTimeElapsed(),
		                                         RuntimeData.GravityApexTimer.GetDuration()));
	}
}
#endif

EMWallRunWallSide UMMovementMode_WallRun::GetWallRunWallSide() const
{
	if (IsMovementModeActive())
	{
		const FVector MovementDir = GetWallRunHorizontalVelocityAlongSurface().GetSafeNormal();
		const float AngleSigned = MMath::SignedAngleBetweenVectorsRad(MovementDir, RuntimeData.SurfaceInfo.Normal);

		return AngleSigned < 0 ? EMWallRunWallSide::Left : EMWallRunWallSide::Right;
	}

	return EMWallRunWallSide::None;
}

void UMMovementMode_WallRun::ActivateCooldown()
{
	RuntimeData.CooldownTimer.Reset();
}

void UMMovementMode_WallRun::SweepAndCalculateSurfaceInfo()
{
	auto CapsuleComponent = CharacterOwner->GetCapsuleComponent();
	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(
		CapsuleComponent->GetScaledCapsuleRadius() * ConfigData.WallDetectionCapsuleSizeMultiplier,
		CapsuleComponent->GetScaledCapsuleHalfHeight() * ConfigData.WallDetectionCapsuleSizeMultiplier);

	// const FVector StartOffset = UpdatedComponent->GetForwardVector() * 20;
	const FVector StartOffset = FVector::ZeroVector;

	// Avoid using the same Start/End location for a Sweep, as it doesn't trigger hits on Landscapes.
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * 5;

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

FVector UMMovementMode_WallRun::GetSurfaceNormal() const
{
	return RuntimeData.SurfaceInfo.Normal;
}

FVector UMMovementMode_WallRun::GetSnapLocation() const
{
	return RuntimeData.SurfaceInfo.SnapLocation;
}

FVector UMMovementMode_WallRun::GetHorizontalDirectionAlongSurfaceFromVelocity() const
{
	FVector DirectionAlongSurface = FVector::VectorPlaneProject(MovementComponent->Velocity, GetSurfaceNormal());
	DirectionAlongSurface.Z = 0;
	DirectionAlongSurface.Normalize();

	return DirectionAlongSurface;
}

float UMMovementMode_WallRun::GetHorizontalSpeedFromVelocity() const
{
	const float HorizontalSpeed = MovementComponent->Velocity.Size2D();
	return HorizontalSpeed;
}

FVector UMMovementMode_WallRun::GetWallRunHorizontalVelocityAlongSurface() const
{
	const FVector HorizontalVelocityAlongSurface = GetHorizontalDirectionAlongSurfaceFromVelocity() * RuntimeData.HorizontalSpeed;
	return HorizontalVelocityAlongSurface;
}

FVector UMMovementMode_WallRun::GetJumpOffVelocity() const
{
	const FVector HorizontalWallRunVelocity = GetWallRunHorizontalVelocityAlongSurface();

	// Rotate Horizontal Wall Run Direction by angles specified in config
	const float WallSideHorizontalAngleMultiplier = GetWallRunWallSide() == EMWallRunWallSide::Left ? -1 : 1;
	const float JumpOffHorizontalAngle = ConfigData.JumpOffAngleHorizontalFromSurface * WallSideHorizontalAngleMultiplier;

	FVector JumpOffVelocity;
	if (!ConfigData.bEnableSpeedPreservation)
	{
		const FVector JumpOffDirectionUnrotated = FRotator::MakeFromEuler(
				FVector(0, ConfigData.JumpOffAngleVertical, JumpOffHorizontalAngle))
			.Vector();

		const auto WallRunDirectionRotationMatrix = FRotationMatrix::Make(HorizontalWallRunVelocity.GetSafeNormal().ToOrientationQuat());
		const FVector JumpOffDirectionRotated = WallRunDirectionRotationMatrix.TransformVector(JumpOffDirectionUnrotated);

		JumpOffVelocity = JumpOffDirectionRotated * ConfigData.FixedJumpOffSpeed;
	}
	else // JumpOffVelocity calculation for Speed Preservation
	{
		const FVector HorizontalJumpOffVelocity = HorizontalWallRunVelocity.RotateAngleAxis(JumpOffHorizontalAngle, FVector::UpVector);
		const FVector VerticalJumpOffVelocity = FVector::UpVector * ConfigData.FixedJumpOffVerticalSpeed;

		JumpOffVelocity = HorizontalJumpOffVelocity + VerticalJumpOffVelocity;
	}

	return JumpOffVelocity;
}

bool UMMovementMode_WallRun::IsHighEnoughFromGround() const
{
	// Check if ground is far enough
	FHitResult GroundTraceHitResult;
	const FVector TraceStart = UpdatedComponent->GetComponentLocation();
	const FVector TraceEnd = TraceStart + FVector::DownVector * ConfigData.MinDistanceFromGround;
	bool bGroundHit = GetWorld()->LineTraceSingleByChannel(GroundTraceHitResult, TraceStart, TraceEnd, ECC_WorldStatic,
	                                                       WallDetectionQueryParams);

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		const FColor DebugColor = bGroundHit ? FColor::Red : FColor::Green;
		DrawDebugLine(GetWorld(), TraceStart, TraceEnd, DebugColor, false, 3);
		DrawDebugString(GetWorld(), TraceEnd, TEXT("Ground height test"), 0, DebugColor, 3, true);
	}

	if (bGroundHit)
	{
		// TODO: check for tags in config instead of hardcoded value
		if (!GroundTraceHitResult.GetComponent()->ComponentHasTag(WallRunnableTagName))
			return false;
	}

	return true;
}


FMCharacterMovement_WallRunSurfaceInfo UMMovementMode_WallRun::CalculateSurfaceInfo(const TArray<FHitResult>& Hits)
{
	TArray<FMCharacterMovement_WallRunSurfaceHitInfo> SurfaceHitInfoArray;
	TArray<FString> SurfaceValidationLogArray;

	// Filter hits to have only correct wall hits
	for (const FHitResult& Hit : Hits)
	{
		SurfaceValidationLogArray.Add(FString::Printf(TEXT("%s surface validation %s> "),
		                                              *GetMovementModeName().ToString(),
		                                              *Hit.GetActor()->GetActorNameOrLabel()));

		// Check if surface has wall runnable tag
		if (!ConfigData.WallRunnableSurfaceTag.IsNone() && !Hit.GetComponent()->ComponentHasTag(ConfigData.WallRunnableSurfaceTag))
		{
			FString& ValidationLog = SurfaceValidationLogArray.Last();
			ValidationLog += FString::Printf(
				TEXT("Invalid - Surface doesn't contain %s tag"), *ConfigData.WallRunnableSurfaceTag.ToString());

			continue;
		}

		// Check if surface doesn't have any of specified exclusion tags
		bool bExcludedFromTag = false;
		for (const FName& ExclusionTag : ConfigData.SurfaceExclusionTags)
		{
			if (Hit.GetComponent()->ComponentHasTag(ExclusionTag))
			{
				bExcludedFromTag = true;
				break;
			}
		}

		if (bExcludedFromTag)
		{
			FString& ValidationLog = SurfaceValidationLogArray.Last();
			ValidationLog += TEXT("Invalid - Surface contains exclusion tag");

			continue;
		}

		const FVector AssistTraceStart = UpdatedComponent->GetComponentLocation();
		const FVector AssistTraceEnd = AssistTraceStart + MMath::FromToVectorNormalized(AssistTraceStart, Hit.ImpactPoint) * 300;

		FHitResult AssistHit;
		if (!GetWorld()->LineTraceSingleByChannel(AssistHit, AssistTraceStart, AssistTraceEnd, ECC_WorldStatic, WallDetectionQueryParams))
		{
			FString& ValidationLog = SurfaceValidationLogArray.Last();
			ValidationLog += TEXT("Invalid - Assist line trace did not hit the surface");

			continue;
		}

		FVector Normal = AssistHit.ImpactNormal;
		FVector HitLocation = AssistHit.ImpactPoint;

		// Check surface has correct angle to run on it
		const float SurfaceAngle = MMath::SignedAngleBetweenVectorsDeg(FVector::UpVector, Normal);
		if (SurfaceAngle < ConfigData.WallRunnableSurfaceNormalAngleMin)
		{
			FString& ValidationLog = SurfaceValidationLogArray.Last();
			ValidationLog += FString::Printf(TEXT("Invalid - Surface angle too low (angle: %.2f, min: %.2f)"),
			                                 SurfaceAngle,
			                                 ConfigData.WallRunnableSurfaceNormalAngleMin);

			continue;
		}

		if (SurfaceAngle > ConfigData.WallRunnableSurfaceNormalAngleMax)
		{
			FString& ValidationLog = SurfaceValidationLogArray.Last();
			ValidationLog += FString::Printf(TEXT("Invalid - Surface angle too high (angle: %.2f, max: %.2f)"),
			                                 SurfaceAngle,
			                                 ConfigData.WallRunnableSurfaceNormalAngleMax);

			continue;
		}

		SurfaceHitInfoArray.Emplace(FMCharacterMovement_WallRunSurfaceHitInfo(HitLocation, Normal, Hit.GetComponent()));

		FString& ValidationLog = SurfaceValidationLogArray.Last();
		ValidationLog += TEXT("Valid");

		if (CVarShowMovementDebugs.GetValueOnGameThread())
			DrawDebugLine(GetWorld(), HitLocation, HitLocation + AssistHit.Normal * 50, FColor::Green, false, 5);
	}

	FMCharacterMovement_WallRunSurfaceInfo SurfaceInfoResult = FMCharacterMovement_WallRunSurfaceInfo();

	if (SurfaceHitInfoArray.Num() > 0)
	{
		SurfaceInfoResult.bValid = true;

		for (const auto& Hit : SurfaceHitInfoArray)
		{
			SurfaceInfoResult.SnapLocation += Hit.SnapLocation;
			SurfaceInfoResult.Normal += Hit.Normal;
		}

		SurfaceInfoResult.SnapLocation /= SurfaceHitInfoArray.Num();
		SurfaceInfoResult.Normal = SurfaceInfoResult.Normal.GetSafeNormal();
		SurfaceInfoResult.PrimitiveComponent = SurfaceHitInfoArray[0].PrimitiveComponent;
	}

	if (Hits.Num() > 0)
	{
		for (auto& SurfaceValidationLog : SurfaceValidationLogArray)
		{
			UE_VLOG(CharacterOwner, LogMMovement, Display, TEXT("%s"), *SurfaceValidationLog);
		}
	}
	else
	{
		UE_VLOG(CharacterOwner, LogMMovement, Display, TEXT("%s surface validation> No Sweep hits"),
		        *GetMovementModeName().ToString());
	}

	return SurfaceInfoResult;
}
