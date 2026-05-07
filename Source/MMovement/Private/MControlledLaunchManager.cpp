// Copyright (c) Miknios. All rights reserved.


#include "MControlledLaunchManager.h"

#include "MCharacterMovementComponent.h"
#include "MDebug.h"
#include "MMovementTypes.h"
#include "MMultisourceLockableBool.h"

void FMControlledLaunchManager_LaunchInstance::ProcessAndCombine(FMControlledLaunchManager_ProcessResult& ProcessResult) const
{
	if (LaunchParams.bInfluenceInputAcceleration)
	{
		const float AccelerationMultiplier = FMath::Clamp(
			LaunchParams.AccelerationMultiplierCurve->GetFloatValue(DurationTimer.GetProgressNormalized()), 0, 1);
		ProcessResult.InputAccelerationMultiplier *= AccelerationMultiplier;

		if (LaunchParams.bAllowFullInputAccelerationPerpendicularToLaunchDirection)
		{
			FVector LaunchDirectionHorizontal = LaunchVelocity;
			LaunchDirectionHorizontal.Z = 0;
			LaunchDirectionHorizontal.Normalize();

			// Get acceleration perpendicular to horizontal launch direction 
			const FVector AccelerationPerp = FVector::VectorPlaneProject(ProcessResult.InputAcceleration, LaunchDirectionHorizontal);

			// Get acceleration in launch direction and scale it
			FVector AccelerationInLaunchDir = ProcessResult.InputAcceleration - AccelerationPerp;
			AccelerationInLaunchDir *= AccelerationMultiplier;

			// Combine separated acceleration vectors
			const FVector ResultAcceleration = AccelerationPerp + AccelerationInLaunchDir;
			ProcessResult.InputAcceleration = ResultAcceleration;
		}
		else
		{
			ProcessResult.InputAcceleration *= AccelerationMultiplier;
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
	if (!IsValid(OwnerMovementComponent))
	{
		return;
	}

	FVisualLogStatusCategory& MovementCmpCategory = Snapshot->Status.Last();
	FVisualLogStatusCategory ControlledLaunchManagerCategory(TEXT("Controlled Launch Manager"));

	FMControlledLaunchManager_ProcessResult ProcessResult = Process(OwnerMovementComponent->GetCurrentAcceleration());
	ControlledLaunchManagerCategory.Add(TEXT("Acceleration Multiplier"),
	                                    FString::Printf(TEXT("%.2f"), ProcessResult.InputAccelerationMultiplier));

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
			                           FString::Printf(TEXT("%.2f"), InstanceProcessResult.InputAccelerationMultiplier));

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

bool UMControlledLaunchManager::IsLaunchImmunityActive() const
{
	return !LaunchImmunityLockableBool->IsActive();
}

void UMControlledLaunchManager::AddLaunchImmunitySource(const UObject* Source)
{
	if (!IsValid(Source))
	{
		return;
	}

	if (!IsValid(LaunchImmunityLockableBool))
	{
		return;
	}

	LaunchImmunityLockableBool->AddBlockSource(Source);
}

void UMControlledLaunchManager::RemoveLaunchImmunitySource(const UObject* Source)
{
	if (!IsValid(Source))
	{
		return;
	}

	if (!IsValid(LaunchImmunityLockableBool))
	{
		return;
	}

	LaunchImmunityLockableBool->RemoveBlockSource(Source);
}

void UMControlledLaunchManager::Initialize(UMCharacterMovementComponent* InOwnerMovementComponent)
{
	OwnerMovementComponent = InOwnerMovementComponent;
	LaunchImmunityLockableBool = NewObject<UMMultisourceLockableBool>();

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

void UMControlledLaunchManager::AddControlledLaunch(const FVector& LaunchVelocity,
                                                    const FMControlledLaunchParams& LaunchParams,
                                                    UObject* Owner)
{
	if ((LaunchParams.bInfluenceInputAcceleration && LaunchParams.AccelerationMultiplierCurve == nullptr)
		|| (LaunchParams.bInfluenceBreakingDeceleration && LaunchParams.BrakingDecelerationMultiplierCurve == nullptr)
		|| (LaunchParams.bInfluenceGravity && LaunchParams.GravityMultiplierCurve == nullptr))
	{
		M::Debug::LogUserError(
			LogMMovement, TEXT("Controlled Launch cannot be added, because some of the curves in Launch Parameters are null"));
		return;
	}

	if (LaunchVelocity.IsNearlyZero())
	{
		M::Debug::LogUserError(LogMMovement, TEXT("Controlled Launch cannot be added, because Launch Velocity is 0 or nearly 0"));
		return;
	}
	
	if (!ShouldAddLaunchInstance(Owner))
	{
		return;
	}

	// Combine active launch velocities to find part of current Movement Component Velocity that comes from active launches
	// Then we combine the result with new LaunchVelocity
	// This makes the launch responsive but doesn't completely cancel other launches
	const FVector LaunchVelocityCombined = [&]
	{
		FVector LaunchInstancesCombinedVelocity = FVector::ZeroVector;
		ForEachActiveLaunchInstanceConst(
			[&LaunchInstancesCombinedVelocity](const FMControlledLaunchManager_LaunchInstance& LaunchInstance) -> bool
			{
				LaunchInstancesCombinedVelocity += LaunchInstance.LaunchVelocity;

				return true;
			});

		const FVector LaunchInstancedCombinedVelocityDirection = LaunchInstancesCombinedVelocity.GetSafeNormal();
		const FVector CurrentVelocityInDirectionOfLaunch =
			OwnerMovementComponent->Velocity.ProjectOnToNormal(LaunchInstancedCombinedVelocityDirection);
		const FVector CurrentVelocityInDirectionOfLaunchWithNewLaunchVelocity = CurrentVelocityInDirectionOfLaunch + LaunchVelocity;

		return CurrentVelocityInDirectionOfLaunchWithNewLaunchVelocity;
	}();

	OwnerMovementComponent->Launch(LaunchVelocityCombined);

	FMControlledLaunchManager_LaunchInstance LaunchInstance = FMControlledLaunchManager_LaunchInstance(LaunchParams, LaunchVelocity);
	if (Owner != nullptr)
	{
		if (LaunchInstanceForOwner.Contains(Owner))
		{
			LaunchInstanceForOwner[Owner] = LaunchInstance;
		}
		else
		{
			LaunchInstanceForOwner.Add(Owner, LaunchInstance);
		}
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

FMControlledLaunchManager_ProcessResult UMControlledLaunchManager::Process(const FVector& InputAcceleration) const
{
	FMControlledLaunchManager_ProcessResult ProcessResult = FMControlledLaunchManager_ProcessResult(InputAcceleration);

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

bool UMControlledLaunchManager::ShouldAddLaunchInstance(const UObject* LaunchOwner)
{
	if (!IsValid(OwnerMovementComponent))
	{
		return false;
	}

	if (!IsValid(OwnerMovementComponent->GetOwner()))
	{
		return false;
	}

	if (!IsValid(LaunchOwner))
	{
		return false;
	}
	
	if (!IsLaunchImmunityActive())
	{
		return true;
	}

	// We don't check for immunity if controlled launch comes from the same entity
	if (LaunchOwner == OwnerMovementComponent->GetOwner())
	{
		return true;
	}

	if (const AController* LaunchOwnerController = Cast<AController>(LaunchOwner))
	{
		const APawn* LaunchOwnerPawn = LaunchOwnerController->GetPawn();
		if (!IsValid(LaunchOwnerPawn))
		{
			return false;
		}

		if (LaunchOwnerPawn == OwnerMovementComponent->GetOwner())
		{
			return true;
		}
	}
	
	return false;
}
