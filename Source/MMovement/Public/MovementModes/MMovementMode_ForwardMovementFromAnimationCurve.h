// Copyright (c) Miknios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMovementMode_Base.h"
#include "MMovementMode_OrientToMovementInterface.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MMovementMode_ForwardMovementFromAnimationCurve.generated.h"

UCLASS(meta = (DisplayName = "Forward Movement Mode"))
class MMOVEMENT_API UMAnimNotifyState_ForwardMovementMode : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// ~ UAnimNotifyState
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	                         const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                       const FAnimNotifyEventReference& EventReference) override;
	// ~ UAnimNotifyState

protected:
	UPROPERTY(EditAnywhere)
	float DistanceMax;
};

USTRUCT()
struct FMMovementMode_ForwardMovementFromAnimationCurve_RuntimeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly)
	bool bWantsToStart = false;

	UPROPERTY(VisibleInstanceOnly)
	bool bWantsToFinish = false;

	UPROPERTY(VisibleInstanceOnly)
	float DistanceMax = 0;

	UPROPERTY(VisibleInstanceOnly)
	float TotalDistancePreviousFrame = 0;

	UPROPERTY(VisibleInstanceOnly)
	bool bNeedsToCalculateOrientToMovementRotation = false;

	UPROPERTY(VisibleInstanceOnly)
	FRotator OrientToMovementRotation = FRotator::ZeroRotator;
};

UCLASS()
class MMOVEMENT_API UMMovementMode_ForwardMovementFromAnimationCurve : public UMMovementMode_Base,
                                                                       public IMMovementMode_OrientToMovementInterface
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
	virtual bool IsMovingOnGround_Implementation() override;
	virtual bool IsMovingOnSurface_Implementation() override;
	// ~ UMMovementMode_Base

	// ~ IMMovementMode_OrientToMovementInterface
	FRotator ComputeOrientToMovementRotation_Implementation(const FRotator& CurrentRotation, float DeltaTime,
	                                                        FRotator& DeltaRotation) override;
	// ~ IMMovementMode_OrientToMovementInterface

	UFUNCTION(BlueprintCallable)
	void StartAnimationMovement(const float DistanceMax);

	UFUNCTION(BlueprintCallable)
	void FinishAnimationMovement();

protected:
	UPROPERTY(Transient, EditAnywhere, Category = "Forward Movement From Animation Curve Data", meta = (ShowOnlyInnerProperties))
	FMMovementMode_ForwardMovementFromAnimationCurve_RuntimeData RuntimeData;

	UPROPERTY(Transient)
	UAnimInstance* AnimInstance;
};
