// Copyright (c) Miknios. All rights reserved.


#include "MControlledLaunchManager.h"

#include "MCharacterMovementComponent.h"
#include "MMovementMode_Base.h"
#include "MMovementMode_Dash.h"
#include "MMovementMode_Slide.h"
#include "MMovementMode_WallRun.h"
#include "MMovementTypes.h"

void FMControlledLaunchManager_LaunchInstance::ProcessAndCombine(FMControlledLaunchManager_ProcessResult& ProcessResult) const
{
	if (LaunchParams.bInfluenceInputAcceleration)
	{
		if (LaunchParams.bAllowFullInputAccelerationPerpendicularToLaunchDirection)
		{
			FVector LaunchDirectionHorizontal = LaunchVelocity;
			LaunchDirectionHorizontal.Z = 0;
			LaunchDirectionHorizontal.Normalize();

			// Get acceleration perpendicular to horizontal launch direction 
			const FVector AccelerationPerp = FVector::VectorPlaneProject(ProcessResult.Acceleration, LaunchDirectionHorizontal);

			// Get acceleration in launch direction and scale it
			FVector AccelerationInLaunchDir = ProcessResult.Acceleration - AccelerationPerp;
			AccelerationInLaunchDir *= FMath::Clamp(
				LaunchParams.AccelerationMultiplierCurve->GetFloatValue(DurationTimer.GetProgressNormalized()), 0, 1);

			// Combine separated acceleration vectors
			const FVector ResultAcceleration = AccelerationPerp + AccelerationInLaunchDir;
			ProcessResult.Acceleration = ResultAcceleration;
		}
		else
		{
			ProcessResult.Acceleration *= FMath::Clamp(
				LaunchParams.AccelerationMultiplierCurve->GetFloatValue(DurationTimer.GetProgressNormalized()), 0, 1);
		}
	}

	if (LaunchParams.bInfluenceBreakingDeceleration)
		ProcessResult.BrakingDecelerationMultiplier *= LaunchParams.BrakingDecelerationMultiplierCurve->GetFloatValue(
			DurationTimer.GetProgressNormalized());

	if (LaunchParams.bInfluenceGravity)
		ProcessResult.GravityMultiplier *= FMath::Clamp(
			LaunchParams.GravityMultiplierCurve->GetFloatValue(DurationTimer.GetProgressNormalized()), 0, 1);
}

bool UMControlledLaunchManager::IsAnyControlledLaunchActive() const
{
	return LaunchInstanceForOwner.Num() > 0 || LaunchInstancesWithoutOwner.Num() > 0;
}

bool UMControlledLaunchManager::IsWalkBlockedByControlledLaunch() const
{
	for (auto& Element : LaunchInstanceForOwner)
	{
		if (!Element.Value.WalkingBlockTimer.IsCompleted())
			return true;
	}

	for (const auto& LaunchInstance : LaunchInstancesWithoutOwner)
	{
		if (!LaunchInstance.WalkingBlockTimer.IsCompleted())
			return true;
	}

	return false;
}

void UMControlledLaunchManager::Initialize(UMCharacterMovementComponent* InOwnerMovementComponent)
{
	OwnerMovementComponent = InOwnerMovementComponent;
	ClearAllLaunches();
}

void UMControlledLaunchManager::TickLaunches(float DeltaTime)
{
	TArray<UObject*> OwnersToRemove;
	for (auto& Element : LaunchInstanceForOwner)
	{
		if (ShouldRemoveLaunchInstance(Element.Value))
			OwnersToRemove.Emplace(Element.Key);

		TickLaunchInstance(Element.Value, DeltaTime);
	}

	for (UObject* OwnerToRemove : OwnersToRemove)
	{
		LaunchInstanceForOwner.Remove(OwnerToRemove);
	}

	for (int i = LaunchInstancesWithoutOwner.Num() - 1; i >= 0; i--)
	{
		if (ShouldRemoveLaunchInstance(LaunchInstancesWithoutOwner[i]))
			LaunchInstancesWithoutOwner.RemoveAt(i);

		TickLaunchInstance(LaunchInstancesWithoutOwner[i], DeltaTime);
	}

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		for (auto& Element : LaunchInstanceForOwner)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Orange,
			                                 FString::Printf(
				                                 TEXT("Controlled Launch (%s): %0.1f"), *Element.Key->GetName(),
				                                 Element.Value.DurationTimer.GetTimeLeft()));
		}

		for (const FMControlledLaunchManager_LaunchInstance& LaunchInstance : LaunchInstancesWithoutOwner)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Orange,
			                                 FString::Printf(
				                                 TEXT("Controlled Launch: %0.1f"), LaunchInstance.DurationTimer.GetTimeLeft()));
		}
	}
}

void UMControlledLaunchManager::AddControlledLaunch(const FVector& LaunchVelocity, const FMControlledLaunchParams& LaunchParams,
                                                    UObject* Owner)
{
	if ((LaunchParams.bInfluenceInputAcceleration && LaunchParams.AccelerationMultiplierCurve == nullptr)
		|| (LaunchParams.bInfluenceBreakingDeceleration && LaunchParams.BrakingDecelerationMultiplierCurve == nullptr)
		|| (LaunchParams.bInfluenceGravity && LaunchParams.GravityMultiplierCurve == nullptr))
	{
		UE_LOG(LogMMovement, Error, TEXT("Controlled Launch cannot be added, because some of the curves in Launch Parameters are null"));
		return;
	}

	if (LaunchVelocity.IsNearlyZero())
	{
		UE_LOG(LogMMovement, Error, TEXT("Controlled Launch cannot be added, because direction is 0 or nearly 0"));
		return;
	}

	OwnerMovementComponent->Launch(LaunchVelocity);

	FMControlledLaunchManager_LaunchInstance LaunchInstance = FMControlledLaunchManager_LaunchInstance(LaunchParams, LaunchVelocity);

	if (Owner != nullptr)
	{
		if (LaunchInstanceForOwner.Contains(Owner))
			LaunchInstanceForOwner[Owner] = LaunchInstance;
		else
			LaunchInstanceForOwner.Add(Owner, LaunchInstance);
	}
	else
	{
		LaunchInstancesWithoutOwner.Emplace(LaunchInstance);
	}
}

void UMControlledLaunchManager::ClearAllLaunches()
{
	LaunchInstanceForOwner.Empty();
	LaunchInstancesWithoutOwner.Empty();
}

FMControlledLaunchManager_ProcessResult UMControlledLaunchManager::Process(const FVector& AccelerationCurrent)
{
	FMControlledLaunchManager_ProcessResult ProcessResult = FMControlledLaunchManager_ProcessResult(AccelerationCurrent);

	for (const auto& Element : LaunchInstanceForOwner)
	{
		Element.Value.ProcessAndCombine(ProcessResult);
	}

	for (const FMControlledLaunchManager_LaunchInstance& LaunchInstance : LaunchInstancesWithoutOwner)
	{
		LaunchInstance.ProcessAndCombine(ProcessResult);
	}

	return ProcessResult;
}

void UMControlledLaunchManager::TickLaunchInstance(FMControlledLaunchManager_LaunchInstance& LaunchInstance, float DeltaTime)
{
	LaunchInstance.DurationTimer.Tick(DeltaTime);
	LaunchInstance.WalkingBlockTimer.Tick(DeltaTime);
}

bool UMControlledLaunchManager::ShouldRemoveLaunchInstance(const FMControlledLaunchManager_LaunchInstance& LaunchInstance) const
{
	if (LaunchInstance.DurationTimer.GetTimeElapsed() == 0)
		return false;

	if (LaunchInstance.DurationTimer.IsCompleted())
	{
		return true;
	}

	if (LaunchInstance.LaunchParams.bDisableOnSpeedInHorizontalLaunchDirectionBelowThreshold)
	{
		FVector VelocityHorizontal = OwnerMovementComponent->Velocity;
		VelocityHorizontal.Z = 0;

		FVector LaunchVelocityHorizontal = LaunchInstance.LaunchVelocity;
		LaunchVelocityHorizontal.Z = 0;

		const FVector HorizontalVelocityProjected = VelocityHorizontal.ProjectOnToNormal(LaunchVelocityHorizontal.GetSafeNormal());

		const bool bBelowSpeedThreshold = HorizontalVelocityProjected.Size() <= ControlledLaunchSpeedThreshold;
		if (bBelowSpeedThreshold)
		{
			return true;
		}
	}

	if (LaunchInstance.LaunchParams.bDisableOnSurface
		&& LaunchInstance.DurationTimer.GetTimeElapsed() > 0.5f
		&& OwnerMovementComponent->IsMovingOnSurface())
	{
		return true;
	}

	return false;
}
