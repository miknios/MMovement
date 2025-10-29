// Copyright (c) Miknios. All rights reserved.


#include "MControlledLaunchManager.h"

#include "MCharacterMovementComponent.h"
#include "MMovementTypes.h"

void FMControlledLaunchManager_LaunchInstance::ProcessAndCombine(FMControlledLaunchManager_ProcessResult& ProcessResult) const
{
	if (LaunchParams.bInfluenceInputAcceleration)
	{
		const float AccelerationMultiplier = FMath::Clamp(
			LaunchParams.AccelerationMultiplierCurve->GetFloatValue(DurationTimer.GetProgressNormalized()), 0, 1);
		ProcessResult.AccelerationMultiplier *= AccelerationMultiplier;

		if (LaunchParams.bAllowFullInputAccelerationPerpendicularToLaunchDirection)
		{
			FVector LaunchDirectionHorizontal = LaunchVelocity;
			LaunchDirectionHorizontal.Z = 0;
			LaunchDirectionHorizontal.Normalize();

			// Get acceleration perpendicular to horizontal launch direction 
			const FVector AccelerationPerp = FVector::VectorPlaneProject(ProcessResult.Acceleration, LaunchDirectionHorizontal);

			// Get acceleration in launch direction and scale it
			FVector AccelerationInLaunchDir = ProcessResult.Acceleration - AccelerationPerp;
			AccelerationInLaunchDir *= AccelerationMultiplier;

			// Combine separated acceleration vectors
			const FVector ResultAcceleration = AccelerationPerp + AccelerationInLaunchDir;
			ProcessResult.Acceleration = ResultAcceleration;
		}
		else
		{
			ProcessResult.Acceleration *= AccelerationMultiplier;
		}
	}

	if (LaunchParams.bInfluenceBreakingDeceleration)
	{
		ProcessResult.BrakingDecelerationMultiplier *= LaunchParams.BrakingDecelerationMultiplierCurve->GetFloatValue(
			DurationTimer.GetProgressNormalized());
	}

	if (LaunchParams.bInfluenceGravity)
	{
		ProcessResult.GravityMultiplier *= FMath::Clamp(
			LaunchParams.GravityMultiplierCurve->GetFloatValue(DurationTimer.GetProgressNormalized()), 0, 1);
	}
}

#if ENABLE_VISUAL_LOG
void UMControlledLaunchManager::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory& MovementCmpCategory = Snapshot->Status.Last();
	FVisualLogStatusCategory ControlledLaunchManagerCategory(TEXT("Controlled Launch Manager"));

	FMControlledLaunchManager_ProcessResult ProcessResult = Process(FVector::ForwardVector);
	ControlledLaunchManagerCategory.Add(TEXT("Acceleration Multiplier"),
	                                    FString::Printf(TEXT("%.2f"), ProcessResult.AccelerationMultiplier));

	ControlledLaunchManagerCategory.Add(TEXT("Braking Deceleration Multiplier"),
	                                    FString::Printf(TEXT("%.2f"), ProcessResult.BrakingDecelerationMultiplier));

	ControlledLaunchManagerCategory.Add(TEXT("Gravity Multiplier"),
	                                    FString::Printf(TEXT("%.2f"),
	                                                    ProcessResult.GravityMultiplier));

	FVisualLogStatusCategory LaunchInstancesCategory(TEXT("Instances"));

	int InstanceIndex = 0;
	ForEachActiveLaunchInstanceConst(
		[&InstanceIndex, &LaunchInstancesCategory](const FMControlledLaunchManager_LaunchInstance& LaunchInstance)
		{
			FVisualLogStatusCategory LaunchInstanceCategory(FString::Printf(TEXT("Instance %d"), InstanceIndex++));

			LaunchInstanceCategory.Add(TEXT("Duration"),
			                           FString::Printf(TEXT("%.2f/%.2f"),
			                                           LaunchInstance.DurationTimer.GetTimeElapsed(),
			                                           LaunchInstance.DurationTimer.GetDuration()));

			LaunchInstanceCategory.Add(TEXT("Walking Block"),
			                           FString::Printf(TEXT("%.2f/%.2f"),
			                                           LaunchInstance.WalkingBlockTimer.GetTimeElapsed(),
			                                           LaunchInstance.WalkingBlockTimer.GetDuration()));

			FMControlledLaunchManager_ProcessResult InstanceProcessResult;
			LaunchInstance.ProcessAndCombine(InstanceProcessResult);

			LaunchInstanceCategory.Add(TEXT("Acceleration Multiplier"),
			                           FString::Printf(TEXT("%.2f"), InstanceProcessResult.AccelerationMultiplier));

			LaunchInstanceCategory.Add(TEXT("Braking Deceleration Multiplier"),
			                           FString::Printf(TEXT("%.2f"), InstanceProcessResult.BrakingDecelerationMultiplier));

			LaunchInstanceCategory.Add(TEXT("Gravity Multiplier"),
			                           FString::Printf(TEXT("%.2f"),
			                                           InstanceProcessResult.GravityMultiplier));

			LaunchInstancesCategory.AddChild(LaunchInstanceCategory);

			return true;
		});

	ControlledLaunchManagerCategory.AddChild(LaunchInstancesCategory);

	MovementCmpCategory.AddChild(ControlledLaunchManagerCategory);
}
#endif

bool UMControlledLaunchManager::IsAnyControlledLaunchActive() const
{
	return LaunchInstanceForOwner.Num() > 0 || LaunchInstancesWithoutOwner.Num() > 0;
}

bool UMControlledLaunchManager::IsWalkBlockedByControlledLaunch() const
{
	bool bResult = false;
	ForEachActiveLaunchInstanceConst([&bResult](const FMControlledLaunchManager_LaunchInstance& LaunchInstance)
	{
		if (!LaunchInstance.WalkingBlockTimer.IsCompleted())
		{
			bResult = true;
			return false;
		}

		return true;
	});

	return bResult;
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
		{
			OwnersToRemove.Emplace(Element.Key);
			continue;
		}

		TickLaunchInstance(Element.Value, DeltaTime);
	}

	for (UObject* OwnerToRemove : OwnersToRemove)
	{
		LaunchInstanceForOwner.Remove(OwnerToRemove);
	}

	for (int i = LaunchInstancesWithoutOwner.Num() - 1; i >= 0; i--)
	{
		if (ShouldRemoveLaunchInstance(LaunchInstancesWithoutOwner[i]))
		{
			LaunchInstancesWithoutOwner.RemoveAt(i);
			continue;
		}

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
		UE_LOG(LogMMovement, Error, TEXT("Controlled Launch cannot be added, because Launch Velocity is 0 or nearly 0"));
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

FMControlledLaunchManager_ProcessResult UMControlledLaunchManager::Process(const FVector& AccelerationCurrent) const
{
	FMControlledLaunchManager_ProcessResult ProcessResult = FMControlledLaunchManager_ProcessResult(AccelerationCurrent);

	ForEachActiveLaunchInstanceConst([&ProcessResult](const FMControlledLaunchManager_LaunchInstance& LaunchInstance)
	{
		LaunchInstance.ProcessAndCombine(ProcessResult);
		return true;
	});

	return ProcessResult;
}

void UMControlledLaunchManager::ForEachActiveLaunchInstance(const TFunctionRef<bool(FMControlledLaunchManager_LaunchInstance&)>& Func)
{
	for (auto& [Owner, LaunchInstance] : LaunchInstanceForOwner)
	{
		const bool bContinue = Func(LaunchInstance);
		if (!bContinue)
		{
			return;
		}
	}

	for (FMControlledLaunchManager_LaunchInstance& LaunchInstance : LaunchInstancesWithoutOwner)
	{
		const bool bContinue = Func(LaunchInstance);
		if (!bContinue)
		{
			return;
		}
	}
}

void UMControlledLaunchManager::ForEachActiveLaunchInstanceConst(
	const TFunctionRef<bool(const FMControlledLaunchManager_LaunchInstance&)>& Func) const
{
	for (const auto& [Owner, LaunchInstance] : LaunchInstanceForOwner)
	{
		const bool bContinue = Func(LaunchInstance);
		if (!bContinue)
		{
			return;
		}
	}

	for (const FMControlledLaunchManager_LaunchInstance& LaunchInstance : LaunchInstancesWithoutOwner)
	{
		const bool bContinue = Func(LaunchInstance);
		if (!bContinue)
		{
			return;
		}
	}
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
