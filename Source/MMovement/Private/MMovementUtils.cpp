// Copyright (c) Miknios. All rights reserved.

#include "MMovementUtils.h"

#include "MCharacterMovementComponent.h"
#include "GameFramework/Character.h"

namespace MMovementUtils
{
	FWalkingPhysicsParams FWalkingPhysicsParams::FromMovementComponent(const UMCharacterMovementComponent* MoveComp)
	{
		FWalkingPhysicsParams Params;
		if (MoveComp)
		{
			Params.MaxWalkSpeed = MoveComp->MaxWalkSpeed;
			Params.MaxAcceleration = MoveComp->MaxAcceleration;
			Params.GroundFriction = MoveComp->GroundFriction;
			Params.BrakingDeceleration = MoveComp->BrakingDecelerationWalking;
			Params.bUseSeparateBrakingFriction = MoveComp->bUseSeparateBrakingFriction;
			Params.BrakingFrictionFactor = MoveComp->BrakingFrictionFactor;
			Params.BrakingFriction = MoveComp->BrakingFriction;
		}
		return Params;
	}

	void CalcVelocityGeneric(
		UMCharacterMovementComponent* MoveComp,
		float DeltaTime,
		const FWalkingPhysicsParams& Params,
		bool bFluid)
	{
		if (!MoveComp || DeltaTime <= 0.f)
		{
			return;
		}

		FVector& Velocity = MoveComp->Velocity;
		const FVector InputAcceleration = MoveComp->GetCurrentAcceleration();

		const float MaxSpeed = Params.MaxWalkSpeed;
		const float GroundFriction = Params.GroundFriction;
		const float BrakingDeceleration = Params.BrakingDeceleration;

		const bool bZeroAcceleration = InputAcceleration.IsNearlyZero();

		// Rescale acceleration to use Params.MaxAcceleration
		FVector Acceleration = InputAcceleration;
		if (!bZeroAcceleration)
		{
			Acceleration = InputAcceleration.GetSafeNormal() * Params.MaxAcceleration;
		}

		// Apply friction
		if (bZeroAcceleration || bFluid)
		{
			const float ActualBrakingFriction = (Params.bUseSeparateBrakingFriction ? Params.BrakingFriction : GroundFriction) * Params.BrakingFrictionFactor;
			const float FrictionDamping = FMath::Max(0.f, ActualBrakingFriction * DeltaTime);
			Velocity -= Velocity * FMath::Min(FrictionDamping, 1.f);
		}
		else
		{
			// When accelerating, apply ground friction (helps with direction changes)
			const float FrictionDamping = FMath::Max(0.f, GroundFriction * DeltaTime);
			const FVector FrictionForce = Velocity * FrictionDamping;

			// Apply friction, but don't let it reduce speed in the direction of acceleration
			const FVector AccelDir = Acceleration.GetSafeNormal();
			const float VelocityInAccelDir = FVector::DotProduct(Velocity, AccelDir);

			if (VelocityInAccelDir > 0.f)
			{
				// Project friction force onto the plane perpendicular to acceleration to keep only the "turning" part of friction
				const FVector FrictionInAccelDir = AccelDir * FVector::DotProduct(FrictionForce, AccelDir);
				const FVector TurningFriction = FrictionForce - FrictionInAccelDir;

				Velocity -= TurningFriction;
			}
			else
			{
				// If moving away from acceleration direction, apply full friction to help turn faster
				Velocity -= FrictionForce;
			}
		}

		// Apply braking deceleration
		if (bZeroAcceleration && !bFluid && Velocity.SizeSquared() > 0.f)
		{
			if (BrakingDeceleration > 0.f)
			{
				const FVector RevDir = -Velocity.GetSafeNormal();
				const FVector DecelDelta = RevDir * BrakingDeceleration * DeltaTime;
				if (DecelDelta.SizeSquared() > Velocity.SizeSquared())
				{
					Velocity = FVector::ZeroVector;
				}
				else
				{
					Velocity += DecelDelta;
				}
			}
		}

		// Apply acceleration
		if (!bZeroAcceleration)
		{
			const FVector AccelDir = Acceleration.GetSafeNormal();
			const float VelSize = Velocity.Size();

			if (VelSize < MaxSpeed)
			{
				Velocity += Acceleration * DeltaTime;
				Velocity = Velocity.GetClampedToMaxSize(MaxSpeed);
			}
			else
			{
				// Already at or above max speed, only allow acceleration in directions that don't increase speed
				const FVector VelocityDir = Velocity.GetSafeNormal();
				const float CurrentDirDot = FVector::DotProduct(VelocityDir, AccelDir);

				if (CurrentDirDot < 0.f)
				{
					Velocity += Acceleration * DeltaTime;
				}
				else
				{
					// Project acceleration onto perpendicular plane to allow turning without increasing speed
					const FVector PerpendicularAccel = Acceleration - (VelocityDir * CurrentDirDot * Acceleration.Size());
					Velocity += PerpendicularAccel * DeltaTime;
				}
			}
		}

		if (bFluid)
		{
			Velocity = Velocity * (1.f - FMath::Min(GroundFriction * DeltaTime, 1.f));
		}
	}

	FGenericWalkOutput PhysWalkingGeneric(
		UMCharacterMovementComponent* MoveComp,
		float DeltaTime,
		int32 Iterations,
		const FWalkingPhysicsParams& Params)
	{
		FGenericWalkOutput Out;

		if (!MoveComp || DeltaTime < UCharacterMovementComponent::MIN_TICK_TIME)
		{
			Out.Result = EGenericWalkResult::None;
			return Out;
		}

		ACharacter* CharacterOwner = MoveComp->GetCharacterOwner();
		auto ShouldStopPhysics = [&]
		{
			return !CharacterOwner ||
				(!CharacterOwner->GetController() &&
					!MoveComp->bRunPhysicsWithNoController &&
					!MoveComp->HasAnimRootMotion() &&
					!MoveComp->CurrentRootMotion.HasOverrideVelocity() &&
					CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy);
		};

		if (ShouldStopPhysics())
		{
			MoveComp->SetAcceleration(FVector::ZeroVector);
			MoveComp->Velocity = FVector::ZeroVector;

			Out.Result = EGenericWalkResult::Blocked;
			return Out;
		}

		if (!MoveComp->UpdatedComponent || !MoveComp->UpdatedComponent->IsQueryCollisionEnabled())
		{
			Out.Result = EGenericWalkResult::Blocked;
			return Out;
		}

		bool bCheckedFall = false;
		bool bTriedLedgeMove = false;
		float RemainingTime = DeltaTime;

		const EMovementMode StartingMode = MoveComp->MovementMode;
		const uint8 StartingCustomMode = MoveComp->CustomMovementMode;

		auto CheckMovementModeChanged = [&]()
		{
			return MoveComp->MovementMode != StartingMode || MoveComp->CustomMovementMode != StartingCustomMode;
		};

		while (RemainingTime >= UCharacterMovementComponent::MIN_TICK_TIME &&
			Iterations < MoveComp->MaxSimulationIterations &&
			!ShouldStopPhysics())
		{
			Iterations++;

			const float TimeTick = MoveComp->GetSimulationTimeStep(RemainingTime, Iterations);
			RemainingTime -= TimeTick;

			const FVector OldLocation = MoveComp->UpdatedComponent->GetComponentLocation();
			UPrimitiveComponent* OldBase = MoveComp->GetMovementBase();
			const FFindFloorResult OldFloor = MoveComp->CurrentFloor;

			MoveComp->RestorePreAdditiveRootMotionVelocity();
			MoveComp->MaintainHorizontalGroundVelocity();

			MoveComp->SetAcceleration(FVector::VectorPlaneProject(MoveComp->GetCurrentAcceleration(), -MoveComp->GetGravityDirection()));

			if (!MoveComp->HasAnimRootMotion() && !MoveComp->CurrentRootMotion.HasOverrideVelocity())
			{
				CalcVelocityGeneric(MoveComp, TimeTick, Params, false);
			}

			MoveComp->ApplyRootMotionToVelocity(TimeTick);

			if (CheckMovementModeChanged())
			{
				Out.Result = EGenericWalkResult::ChangedMovementMode;
				break;
			}

			const FVector MoveVelocity = MoveComp->Velocity;
			const FVector Delta = MoveVelocity * TimeTick;

			FStepDownResult StepDown;

			if (!Delta.IsNearlyZero())
			{
				MoveComp->MoveAlongFloor(MoveVelocity, TimeTick, &StepDown);

				if (MoveComp->IsSwimming())
				{
					Out.Result = EGenericWalkResult::NeedsSwimming;
					break;
				}

				if (CheckMovementModeChanged())
				{
					Out.Result = EGenericWalkResult::ChangedMovementMode;
					break;
				}
			}
			else
			{
				RemainingTime = 0.f;
			}

			if (StepDown.bComputedFloor)
			{
				MoveComp->CurrentFloor = StepDown.FloorResult;
			}
			else
			{
				MoveComp->FindFloor(MoveComp->UpdatedComponent->GetComponentLocation(), MoveComp->CurrentFloor, false, nullptr);
			}

			const bool bCheckLedges = !MoveComp->CanWalkOffLedges();

			if (bCheckLedges && !MoveComp->CurrentFloor.IsWalkableFloor())
			{
				const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : MoveComp->GetLedgeMove(OldLocation, Delta, OldFloor);

				if (!NewDelta.IsZero())
				{
					MoveComp->RevertMove(OldLocation, OldBase, FVector::ZeroVector, OldFloor, false);

					bTriedLedgeMove = true;
					MoveComp->Velocity = NewDelta / TimeTick;

					RemainingTime += TimeTick;
					Iterations--;
					continue;
				}

				bool bMustFall = (OldBase == nullptr) || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase));

				if (bMustFall || !bCheckedFall)
				{
					if (MoveComp->CheckFall(OldFloor, MoveComp->CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, TimeTick, Iterations, bMustFall))
					{
						Out.Result = EGenericWalkResult::NeedsFalling;
						break;
					}
					bCheckedFall = true;
				}

				MoveComp->RevertMove(OldLocation, OldBase, FVector::ZeroVector, OldFloor, true);
				RemainingTime = 0.f;

				Out.Result = EGenericWalkResult::Blocked;
				break;
			}

			if (MoveComp->CurrentFloor.IsWalkableFloor())
			{
				if (MoveComp->ShouldCatchAir(OldFloor, MoveComp->CurrentFloor))
				{
					Out.Result = EGenericWalkResult::NeedsFalling;
					break;
				}

				MoveComp->AdjustFloorHeight();
				MoveComp->SetBaseFromFloor(MoveComp->CurrentFloor);
			}

			if (MoveComp->IsSwimming())
			{
				Out.Result = EGenericWalkResult::NeedsSwimming;
				break;
			}

			if (!MoveComp->CurrentFloor.IsWalkableFloor() && !MoveComp->CurrentFloor.HitResult.bStartPenetrating)
			{
				bool bMustFall = MoveComp->bJustTeleported || !OldBase || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase));

				if ((bMustFall || !bCheckedFall) &&
					MoveComp->CheckFall(OldFloor, MoveComp->CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, TimeTick, Iterations, bMustFall))
				{
					Out.Result = EGenericWalkResult::NeedsFalling;
					break;
				}
				bCheckedFall = true;
			}

			if (MoveComp->IsMovingOnGround())
			{
				if (TimeTick >= UCharacterMovementComponent::MIN_TICK_TIME)
				{
					MoveComp->Velocity = (MoveComp->UpdatedComponent->GetComponentLocation() - OldLocation) / TimeTick;
					MoveComp->MaintainHorizontalGroundVelocity();
				}
			}

			if (MoveComp->UpdatedComponent->GetComponentLocation() == OldLocation)
			{
				RemainingTime = 0.f;
				break;
			}
		}

		// Finalization (single exit path)
		Out.FinalLocation = MoveComp->UpdatedComponent->GetComponentLocation();
		Out.FinalVelocity = MoveComp->Velocity;
		Out.UnusedTime = RemainingTime;
		Out.IterationsUsed = Iterations;

		if (Out.Result == EGenericWalkResult::None)
		{
			Out.Result = EGenericWalkResult::Completed;
		}

		return Out;
	}
}
