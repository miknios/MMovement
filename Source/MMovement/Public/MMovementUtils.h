// Copyright (c) Miknios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UMCharacterMovementComponent;

namespace MMovementUtils
{
	enum class EGenericWalkResult : uint8
	{
		Completed, // Finished normally, still walking
		NeedsFalling, // Should transition to falling
		NeedsSwimming, // Should transition to swimming
		ChangedMovementMode, // External system changed movement mode
		Blocked, // Could not move (penetration / invalid state)
		None // No meaningful movement happened
	};

	struct FGenericWalkOutput
	{
		EGenericWalkResult Result = EGenericWalkResult::None;

		FVector FinalVelocity = FVector::ZeroVector;
		FVector FinalLocation = FVector::ZeroVector;

		int32 IterationsUsed = 0;
		float UnusedTime = 0.f;
	};

	struct FWalkingPhysicsParams
	{
		float MaxWalkSpeed = 500.f;
		float MaxAcceleration = 2048.f;
		float GroundFriction = 8.f;
		float BrakingDeceleration = 2048.f;
		bool bUseSeparateBrakingFriction = false;
		float BrakingFrictionFactor = 2.f;
		float BrakingFriction = 0.f;

		// Optional: Default constructor or convenience method to populate from CMC
		static FWalkingPhysicsParams FromMovementComponent(const UMCharacterMovementComponent* MoveComp);
	};

	MMOVEMENT_API void CalcVelocityGeneric(
		UMCharacterMovementComponent* MoveComp,
		float DeltaTime,
		const FWalkingPhysicsParams& Params,
		bool bFluid);

	MMOVEMENT_API FGenericWalkOutput PhysWalkingGeneric(
		UMCharacterMovementComponent* MoveComp,
		float DeltaTime,
		int32 Iterations,
		const FWalkingPhysicsParams& Params);
}
