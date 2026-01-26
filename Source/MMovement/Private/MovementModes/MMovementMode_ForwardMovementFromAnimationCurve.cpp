// Copyright (c) Miknios. All rights reserved.


#include "MovementModes/MMovementMode_ForwardMovementFromAnimationCurve.h"

#include "MCharacterMovementComponent.h"
#include "GameFramework/Character.h"

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
	RuntimeData.TotalDistancePreviousFrame = 0;
	RuntimeData.bNeedsToCalculateOrientToMovementRotation = true;
}

void UMMovementMode_ForwardMovementFromAnimationCurve::Phys_Implementation(float DeltaTime, int32 Iterations)
{
	static FName CurveName = TEXT("Movement_Horizontal");

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

	const float TotalDistanceThisFrame = AnimInstance->GetCurveValue(CurveName)
		* RuntimeData.DistanceMax;
	const float DistanceThisFrame = FMath::Max(0, TotalDistanceThisFrame - RuntimeData.TotalDistancePreviousFrame);
	const FVector MovementDir = RuntimeData.OrientToMovementRotation.Vector();
	const FVector Velocity = MovementDir * DistanceThisFrame / DeltaTime * CharacterOwner->GetActorTimeDilation();

	RuntimeData.TotalDistancePreviousFrame = TotalDistanceThisFrame;

	MovementComponent->SetAcceleration((Velocity - MovementComponent->Velocity) / DeltaTime);
	MovementComponent->Velocity = Velocity;

	// Move along surface
	const FVector LocationDelta = MovementComponent->Velocity * DeltaTime;
	FHitResult Hit(1.f);
	MovementComponent->SafeMoveUpdatedComponent(LocationDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		MovementComponent->HandleImpact(Hit, DeltaTime, LocationDelta);
		MovementComponent->SlideAlongSurface(LocationDelta, (1.0f - Hit.Time), Hit.Normal, Hit, true);
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
	return false;
}

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
