// Fill out your copyright notice in the Description page of Project Settings.


#include "MMovementMode_Dash.h"

#include "EnhancedInputComponent.h"
#include "MMath.h"
#include "MCharacterMovementComponent.h"
#include "MControlledLaunchManager.h"
#include "MMovementMode_VerticalWallRun.h"
#include "MMovementMode_WallRun.h"
#include "MMovementTypes.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

void UMMovementMode_Dash::Initialize_Implementation()
{
	Super::Initialize_Implementation();

	// Bind input action
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(CharacterOwner->InputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogMMovement, Error, TEXT("MovementMode_Dash: Player does not have Enhanced Input Component"))
		return;
	}

	RuntimeData.ChargesLeft = DashConfig.ChargeAmountInitial;

	RuntimeData.CooldownTimer = FMManualTimer(DashConfig.CooldownTime);
	RuntimeData.CooldownTimer.Complete();

	RuntimeData.DurationTimer = FMManualTimer(DashConfig.Duration);
	RuntimeData.DurationTimer.Complete();

	EnhancedInputComponent->BindAction(DashConfig.InputAction, ETriggerEvent::Triggered, this, &UMMovementMode_Dash::OnDashInput);
}

void UMMovementMode_Dash::Tick_Implementation(float DeltaTime)
{
	Super::Tick_Implementation(DeltaTime);

	RuntimeData.CooldownTimer.Tick(DeltaTime);

	if (DashConfig.bEnableDashCharges)
	{
		if (DashConfig.bRestoreChargesOnGround && MovementComponent->IsMovingOnGround())
			ResetCharges(true);

		UMMovementMode_Base* ActiveCustomMovementModeInstance = MovementComponent->GetActiveCustomMovementModeInstance();
		if (IsValid(ActiveCustomMovementModeInstance))
		{
			if (DashConfig.bRestoreChargesOnWallRun && ActiveCustomMovementModeInstance->IsA<UMMovementMode_WallRun>())
			{
				ResetCharges(true);
			}

			if (DashConfig.bRestoreChargesOnVerticalWallRun && ActiveCustomMovementModeInstance->IsA<UMMovementMode_VerticalWallRun>())
			{
				ResetCharges(true);
			}
		}
	}

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		if (!RuntimeData.CooldownTimer.IsCompleted())
		{
			GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Red,
			                                 FString::Printf(TEXT("Dash Cooldown: %.2f"), RuntimeData.CooldownTimer.GetTimeLeft()));
		}

		if (DashConfig.bEnableDashCharges)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0, FColor::White,
			                                 FString::Printf(TEXT("Dash Charges Left: %d"), RuntimeData.ChargesLeft));
		}
	}
}

bool UMMovementMode_Dash::CanStart_Implementation(FString& OutFailReason)
{
	if (!RuntimeData.CooldownTimer.IsCompleted())
	{
		OutFailReason = TEXT("Cooldown");
		return false;
	}

	if (!RuntimeData.bWantsToDash)
	{
		OutFailReason = TEXT("Not triggered by input");
		return false;
	}

	if (DashConfig.bEnableDashCharges && RuntimeData.ChargesLeft == 0)
	{
		OutFailReason = TEXT("Charges depleted");
		return false;
	}

	// Consume input
	RuntimeData.bWantsToDash = false;

	return true;
}

void UMMovementMode_Dash::Start_Implementation()
{
	Super::Start_Implementation();

	RuntimeData.bInitialValuesCalculated = false;

	MovementComponent->GetControlledLaunchManager()->ClearAllLaunches();

	if (DashConfig.bEnableDashCharges)
		UseDashCharge(true);
}

void UMMovementMode_Dash::Phys_Implementation(float DeltaTime, int32 Iterations)
{
	Super::Phys_Implementation(DeltaTime, Iterations);

	if (!RuntimeData.bInitialValuesCalculated)
		CalculateInitialValues();

	float DeltaTimeClamped = FMath::Min(RuntimeData.DurationTimer.GetTimeLeft(), DeltaTime);
	RuntimeData.DurationTimer.Tick(DeltaTimeClamped);

	const float DistanceNormalized = DashConfig.DistanceCurve->GetFloatValue(RuntimeData.DurationTimer.GetProgressNormalized());
	const float Distance = DistanceNormalized * DashConfig.Distance;
	const FVector LocationTarget = RuntimeData.LocationInitial + RuntimeData.DashDirection * Distance;
	const FVector VelocityTarget = MMath::FromToVector(UpdatedComponent->GetComponentLocation(), LocationTarget);

	MovementComponent->SetAcceleration((VelocityTarget - MovementComponent->Velocity) / DeltaTime);
	MovementComponent->Velocity = VelocityTarget;

	// Move along surface
	const FVector LocationDelta = MovementComponent->Velocity * DeltaTime;

	FHitResult Hit(1.f);

	const FVector LocationOld = MovementComponent->GetActorLocation();

	MovementComponent->SafeMoveUpdatedComponent(LocationDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		MovementComponent->HandleImpact(Hit, DeltaTime, LocationDelta);
		MovementComponent->SlideAlongSurface(LocationDelta, (1.0f - Hit.Time), Hit.Normal, Hit, true);
	}

	const FVector LocationNew = MovementComponent->GetActorLocation();

	if (DashConfig.bEnableDamage)
		DealDamage(LocationOld, LocationNew);

	if (RuntimeData.DurationTimer.IsCompleted())
	{
		FVector DashEndVelocity = RuntimeData.VelocityPreserved.Size() > DashConfig.PreservedSpeedMin
			                          ? RuntimeData.VelocityPreserved
			                          : RuntimeData.DashDirection * DashConfig.PreservedSpeedMin;

		MovementComponent->Velocity = DashEndVelocity;

		if (DashConfig.bApplyControlledLaunchOnFinish)
			MovementComponent->AddControlledLaunchFromAsset(DashEndVelocity, DashConfig.LaunchParams, this);

		MovementComponent->ClearTemporalHorizontalVelocity();

		MovementComponent->SetMovementMode(MOVE_Falling);

		float DeltaTimeRemaining = DeltaTime - DeltaTimeClamped;
		if (DeltaTimeRemaining > 0)
		{
			MovementComponent->StartNewPhysics(DeltaTimeRemaining, Iterations);
		}
	}
}

void UMMovementMode_Dash::End_Implementation()
{
	Super::End_Implementation();

	RuntimeData.CooldownTimer.Reset();
}

bool UMMovementMode_Dash::IsMovingOnGround_Implementation()
{
	return false;
}

bool UMMovementMode_Dash::IsMovingOnSurface_Implementation()
{
	return false;
}

void UMMovementMode_Dash::AddDashCharge(bool bPlayUIAnimation)
{
	int32 ChargesLeftOld = RuntimeData.ChargesLeft;
	RuntimeData.ChargesLeft = FMath::Min(RuntimeData.ChargesLeft + 1, DashConfig.ChargeAmountMax);

	OnDashChargeAmountChanged(ChargesLeftOld, RuntimeData.ChargesLeft, bPlayUIAnimation);
}

bool UMMovementMode_Dash::CanAddDashCharge() const
{
	return RuntimeData.ChargesLeft < DashConfig.ChargeAmountMax;
}

void UMMovementMode_Dash::ResetCharges(bool bPlayUIAnimation)
{
	int32 ChargesLeftOld = RuntimeData.ChargesLeft;
	RuntimeData.ChargesLeft = DashConfig.ChargeAmountInitial;

	OnDashChargeAmountChanged(ChargesLeftOld, RuntimeData.ChargesLeft, bPlayUIAnimation);
}

void UMMovementMode_Dash::UseDashCharge(bool bPlayUIAnimation)
{
	int32 ChargesLeftOld = RuntimeData.ChargesLeft;
	RuntimeData.ChargesLeft = FMath::Max(RuntimeData.ChargesLeft - 1, 0);

	OnDashChargeAmountChanged(ChargesLeftOld, RuntimeData.ChargesLeft, bPlayUIAnimation);
}

void UMMovementMode_Dash::OnDashInput(const FInputActionInstance& Instance)
{
	RuntimeData.bWantsToDash = true;

	if (CVarShowMovementDebugs.GetValueOnGameThread())
	{
		GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Green, TEXT("Dash input triggered"));
	}
}

void UMMovementMode_Dash::OnDashChargeAmountChanged(int32 ValueOld, int32 ValueNew, bool bPlayUIAnimation) const
{
	if (ValueOld != ValueNew)
	{
		OnDashChargeUpdatedDelegate.Broadcast(
			FMOnDashChargeUpdatedData(ValueNew - ValueOld, ValueNew, bPlayUIAnimation));
	}
}

void UMMovementMode_Dash::CalculateInitialValues()
{
	FVector DashDirection = CharacterOwner->GetControlRotation().Vector();

	if (DashConfig.bUseInputDirection && InputVector.SizeSquared() > 0)
	{
		FVector InputVectorRotated = InputVector.RotateAngleAxis(-CharacterOwner->GetControlRotation().Pitch,
		                                                         CharacterOwner->GetActorRightVector());
		DashDirection = InputVectorRotated;
	}

	if (!DashConfig.bUseVerticalDirection)
		DashDirection.Z = 0;

	DashDirection.Normalize();

	FVector VelocityPreserved = MovementComponent->GetPeakTemporalHorizontalVelocity();
	if (DashConfig.bPreserveVelocityOnlyInDashDirection)
	{
		VelocityPreserved = VelocityPreserved.ProjectOnTo(DashDirection);
		if (VelocityPreserved.GetSafeNormal().Dot(DashDirection) < 0)
			VelocityPreserved = FVector::ZeroVector;
	}

	RuntimeData.VelocityPreserved = VelocityPreserved;

	RuntimeData.DurationTimer.Reset();
	RuntimeData.DashDirection = DashDirection;
	RuntimeData.LocationInitial = UpdatedComponent->GetComponentLocation();

	RuntimeData.bInitialValuesCalculated = true;
}

void UMMovementMode_Dash::DealDamage(const FVector& LocationOld, const FVector& LocationNew)
{
	TArray<FHitResult> Hits;

	UCapsuleComponent* CapsuleComponent = CharacterOwner->GetCapsuleComponent();
	FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(
		CapsuleComponent->GetScaledCapsuleRadius() * DashConfig.DamageCapsuleScale,
		CapsuleComponent->GetScaledCapsuleHalfHeight() * DashConfig.DamageCapsuleScale);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);

	GetWorld()->SweepMultiByChannel(Hits, LocationOld, LocationNew, FQuat::Identity, ECC_WorldStatic, CollisionShape, QueryParams);
	for (const FHitResult& Hit : Hits)
	{
		UGameplayStatics::ApplyDamage(Hit.GetActor(), DashConfig.DamageAmount, CharacterOwner->GetController(),
		                              CharacterOwner, UDamageType::StaticClass());
	}
}
