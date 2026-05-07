// Copyright (c) Miknios. All rights reserved.


#include "MovementModes/MMovementMode_ForwardMovementFromAnimationCurve.h"

#include "MCharacterMovementComponent.h"
#include "MMath.h"
#include "MString.h"
#include "GameFramework/Character.h"

namespace
{
	FName CurveName = TEXT("Movement_Horizontal");
}

void UMAnimNotifyState_ForwardMovementMode::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                                        const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	bool bOverrideWalk = true;

	const UWorld* World = MeshComp->GetWorld();

#if WITH_EDITOR
	bOverrideWalk = IsValid(World) && World->IsGameWorld();
#endif

	if (!bOverrideWalk)
	{
		return;
	}

	const UMCharacterMovementComponent* CharacterMovementComponent =
		MeshComp->GetOwner()->FindComponentByClass<UMCharacterMovementComponent>();
	if (!ensure(IsValid(CharacterMovementComponent)))
	{
		return;
	}

	UMMovementMode_ForwardMovementFromAnimationCurve* MovementMode = Cast<UMMovementMode_ForwardMovementFromAnimationCurve>(
		CharacterMovementComponent->GetCustomMovementModeInstance(UMMovementMode_ForwardMovementFromAnimationCurve::StaticClass()));
	if (!ensure(IsValid(MovementMode)))
	{
		return;
	}

	MovementMode->StartAnimationMovement(DistanceMax);
}

void UMAnimNotifyState_ForwardMovementMode::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                      const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	bool bOverrideWalk = true;

	const UWorld* World = MeshComp->GetWorld();

#if WITH_EDITOR
	bOverrideWalk = IsValid(World) && World->IsGameWorld();
#endif

	if (!bOverrideWalk)
	{
		return;
	}

	const UMCharacterMovementComponent* CharacterMovementComponent =
		MeshComp->GetOwner()->FindComponentByClass<UMCharacterMovementComponent>();
	if (!IsValid(CharacterMovementComponent))
	{
		return;
	}

	UMMovementMode_ForwardMovementFromAnimationCurve* MovementMode = Cast<UMMovementMode_ForwardMovementFromAnimationCurve>(
		CharacterMovementComponent->GetCustomMovementModeInstance(UMMovementMode_ForwardMovementFromAnimationCurve::StaticClass()));
	if (!ensure(IsValid(MovementMode)))
	{
		return;
	}

	if (!MovementMode->IsMovementModeActive())
	{
		return;
	}

	MovementMode->FinishAnimationMovement();
}

void UMMovementMode_ForwardMovementFromAnimationCurve::Initialize_Implementation()
{
	Super::Initialize_Implementation();
}

void UMMovementMode_ForwardMovementFromAnimationCurve::Tick_Implementation(float DeltaTime)
{
	Super::Tick_Implementation(DeltaTime);
}

bool UMMovementMode_ForwardMovementFromAnimationCurve::CanStart_Implementation(FString& OutFailReason)
{
	if (!RuntimeData.bWantsToStart)
	{
		OutFailReason = TEXT("Not triggered by input");
		return false;
	}

	// Consume input
	RuntimeData.bWantsToStart = false;

	return true;
}

void UMMovementMode_ForwardMovementFromAnimationCurve::Start_Implementation()
{
	Super::Start_Implementation();

	const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	if (!ensure(IsValid(Mesh)))
	{
		RuntimeData.bWantsToFinish = true;
		return;
	}

	AnimInstance = Mesh->GetAnimInstance();
	if (!ensure(IsValid(AnimInstance)))
	{
		RuntimeData.bWantsToFinish = true;
		return;
	}

	RuntimeData.bWantsToFinish = false;

	const float CurrentCurveValue = AnimInstance->GetCurveValue(CurveName);
	RuntimeData.TotalDistancePreviousFrame = CurrentCurveValue * RuntimeData.DistanceMax;

	RuntimeData.bNeedsToCalculateOrientToMovementRotation = true;
}

void UMMovementMode_ForwardMovementFromAnimationCurve::Phys_Implementation(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < UCharacterMovementComponent::MIN_TICK_TIME)
	{
		return;
	}

	if (!ensure(IsValid(AnimInstance)))
	{
		RuntimeData.bWantsToFinish = true;
	}

	if (RuntimeData.bWantsToFinish)
	{
		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->StartNewPhysics(DeltaTime, Iterations);

		return;
	}

	if (RuntimeData.bNeedsToCalculateOrientToMovementRotation)
	{
		RuntimeData.OrientToMovementRotation = UpdatedComponent->GetComponentRotation();
		RuntimeData.bNeedsToCalculateOrientToMovementRotation = false;
	}

	// Update current floor
	FFindFloorResult FindFloorResult;
	MovementComponent->FindFloor(UpdatedComponent->GetComponentLocation(), FindFloorResult, false);

	MovementComponent->CurrentFloor = FindFloorResult;
	MovementComponent->SetBaseFromFloor(FindFloorResult);

	RuntimeData.bMovingOnSurface = FindFloorResult.bWalkableFloor;

	const float TotalDistanceThisFrame = AnimInstance->GetCurveValue(CurveName)
		* RuntimeData.DistanceMax;
	const float DistanceThisFrame = FMath::Max(0.0f, TotalDistanceThisFrame - RuntimeData.TotalDistancePreviousFrame);
	// Calculate movement direction projected along the current floor so we move along slopes like Dash does.
	const FVector RawOrientDir = RuntimeData.OrientToMovementRotation.Vector();
	const FVector MovementDirection = MovementComponent->GetDirectionAlongFloorForDirection(RawOrientDir);

	const FVector LocationDelta = MovementDirection * DistanceThisFrame;
	const FVector Velocity = (DeltaTime > 0.f) ? (LocationDelta / DeltaTime) : FVector::ZeroVector;

	RuntimeData.TotalDistancePreviousFrame = TotalDistanceThisFrame;

	MovementComponent->SetAcceleration((Velocity - MovementComponent->Velocity) / DeltaTime);
	MovementComponent->Velocity = Velocity;

	// Move along the surface
	FHitResult Hit(1.f);
	MovementComponent->SafeMoveUpdatedComponent(LocationDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		MovementComponent->HandleImpact(Hit, DeltaTime, LocationDelta);
		MovementComponent->SlideAlongSurface(LocationDelta, (1.0f - Hit.Time), Hit.Normal, Hit, true);
	}

	// Snap to surface using the pre-move floor result (mirror Dash behavior)
	if (FindFloorResult.bWalkableFloor)
	{
		const FVector FindFloorDir = MMath::FromToVectorNormalized(FindFloorResult.HitResult.TraceStart,
		                                                          FindFloorResult.HitResult.TraceEnd);
		FVector SnapLocationDelta = FindFloorDir * FindFloorResult.GetDistanceToFloor();
		if (!SnapLocationDelta.IsNearlyZero(1e-4f))
		{
			UpdatedComponent->MoveComponent(SnapLocationDelta * SurfaceSnapSpeed * DeltaTime, UpdatedComponent->GetComponentQuat(), true);
		}
		// ensure movement component sees the same floor data
		MovementComponent->CurrentFloor = FindFloorResult;
		MovementComponent->SetBaseFromFloor(FindFloorResult);
		RuntimeData.bMovingOnSurface = FindFloorResult.bWalkableFloor;
	}
}

void UMMovementMode_ForwardMovementFromAnimationCurve::End_Implementation()
{
	Super::End_Implementation();
}

bool UMMovementMode_ForwardMovementFromAnimationCurve::IsMovingOnGround_Implementation()
{
	return false;
}

bool UMMovementMode_ForwardMovementFromAnimationCurve::IsMovingOnSurface_Implementation()
{
	return RuntimeData.bMovingOnSurface;
}

#if ENABLE_VISUAL_LOG
void UMMovementMode_ForwardMovementFromAnimationCurve::AddVisualLoggerInfo(FVisualLogEntry* Snapshot,
                                                                           FVisualLogStatusCategory& MovementCmpCategory,
                                                                           FVisualLogStatusCategory& MovementModeCategory) const
{
	MovementModeCategory.Add(TEXT("WantsToStart"), UMString::ToString(RuntimeData.bWantsToStart));
	MovementModeCategory.Add(TEXT("WantsToFinish"), UMString::ToString(RuntimeData.bWantsToFinish));
	MovementModeCategory.Add(TEXT("Distance Max"), FString::Printf(TEXT("%.2f"), RuntimeData.TotalDistancePreviousFrame));
	MovementModeCategory.Add(TEXT("TotalDistancePreviousFrame"), FString::Printf(TEXT("%.2f"), RuntimeData.TotalDistancePreviousFrame));
}
#endif

FRotator UMMovementMode_ForwardMovementFromAnimationCurve::ComputeOrientToMovementRotation_Implementation(const FRotator& CurrentRotation,
	float DeltaTime, FRotator& DeltaRotation)
{
	return RuntimeData.OrientToMovementRotation;
}

void UMMovementMode_ForwardMovementFromAnimationCurve::StartAnimationMovement(const float DistanceMax)
{
	RuntimeData.bWantsToStart = true;
	RuntimeData.DistanceMax = DistanceMax;
}

void UMMovementMode_ForwardMovementFromAnimationCurve::FinishAnimationMovement()
{
	RuntimeData.bWantsToFinish = true;
}
