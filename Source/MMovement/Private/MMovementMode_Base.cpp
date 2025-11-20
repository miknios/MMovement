// Fill out your copyright notice in the Description page of Project Settings.


#include "MMovementMode_Base.h"

#include "MCharacterMovementComponent.h"
#include "MMovementTypes.h"
#include "GameFramework/Character.h"

#if ENABLE_VISUAL_LOG
void UMMovementMode_Base::GrabDebugSnapshot(struct FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory& MovementCmpCategory = Snapshot->Status.Last();
	FVisualLogStatusCategory MovementModeCategory(GetMovementModeName().ToString());

	// Log state
	FString StateLog;
	if (IsMovementModeActive())
	{
		StateLog = TEXT("Active");
	}
	else
	{
		StateLog = FString::Printf(
			TEXT("Inactive: %s"), *GetCanStartFailReasonCache());
	}

	MovementModeCategory.Add(TEXT("State"), StateLog);

	// This needs to be called before MovementCmpCategory.AddChild, so child is added with updated state
	AddVisualLoggerInfo(Snapshot, MovementCmpCategory, MovementModeCategory);

	MovementCmpCategory.AddChild(MovementModeCategory);
}
#endif

void UMMovementMode_Base::Reset_Implementation(bool bHardReset)
{
	IMResettable::Reset_Implementation(bHardReset);
}

void UMMovementMode_Base::Initialize(ACharacter* InCharacterOwner, UMCharacterMovementComponent* InMovementComponent)
{
	MovementComponent = InMovementComponent;
	CharacterOwner = InCharacterOwner;
	UpdatedComponent = MovementComponent->UpdatedComponent;

	Initialize();
}

bool UMMovementMode_Base::CanStart_Implementation(FString& OutFailReason)
{
	return true;
}

bool UMMovementMode_Base::CanCrouch_Implementation()
{
	return false;
}

bool UMMovementMode_Base::IsMovingOnGround_Implementation()
{
	return true;
}

bool UMMovementMode_Base::IsMovingOnSurface_Implementation()
{
	return false;
}

bool UMMovementMode_Base::IsUsingCustomMovementBase_Implementation()
{
	return false;
}

FName UMMovementMode_Base::GetMovementModeName() const
{
	if (MovementModeName == NAME_None)
		return FName(GetName());

	return MovementModeName;
}

FVector UMMovementMode_Base::GetOwnerLocation() const
{
	return CharacterOwner->GetActorLocation();
}

#if ENABLE_VISUAL_LOG
void UMMovementMode_Base::AddVisualLoggerInfo(struct FVisualLogEntry* Snapshot, FVisualLogStatusCategory& MovementCmpCategory,
                                              FVisualLogStatusCategory& MovementModeCategory) const
{
}
#endif

void UMMovementMode_Base::Initialize_Implementation()
{
}

void UMMovementMode_Base::Tick_Implementation(float DeltaTime)
{
}

void UMMovementMode_Base::Phys_Implementation(float DeltaTime, int32 Iterations)
{
	OnMovementModeUpdateDelegate.Broadcast();
}

void UMMovementMode_Base::Start_Implementation()
{
	bMovementModeActive = true;

	OnMovementModeStartDelegate.Broadcast();

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		MovementComponent->DrawDebugCharacterCapsule(FColor::Green, 5, GetMovementModeName().ToString());
		GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Green, FString::Printf(TEXT("%s: started"), *GetMovementModeName().ToString()));
	}
}

void UMMovementMode_Base::End_Implementation()
{
	bMovementModeActive = false;

	OnMovementModeEndDelegate.Broadcast();

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		MovementComponent->DrawDebugCharacterCapsule(FColor::Red, 5, GetMovementModeName().ToString());
		GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Red, FString::Printf(TEXT("%s: ended"), *GetMovementModeName().ToString()));
	}
}
