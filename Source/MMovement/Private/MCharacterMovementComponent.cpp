// Copyright (c) Miknios. All rights reserved.


#include "MCharacterMovementComponent.h"

#include "MControlledLaunchAsset.h"
#include "MControlledLaunchManager.h"
#include "MMath.h"
#include "MMovementMode_Base.h"
#include "MMovementMode_OrientToMovementInterface.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

UMCharacterMovementComponent::UMCharacterMovementComponent()
{
}

void UMCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	ControlledLaunchManager = NewObject<UMControlledLaunchManager>(this, TEXT("ControlledLaunchManager"));

	// Save defaults
	DefaultValues.MaxWalkSpeed = MaxWalkSpeed;
	DefaultValues.MaxAcceleration = GetMaxAcceleration();
	DefaultValues.BrakingDecelerationWalking = BrakingDecelerationWalking;
	DefaultValues.BrakingDecelerationFalling = BrakingDecelerationFalling;
	DefaultValues.BrakingFrictionFactor = BrakingFrictionFactor;
	DefaultValues.GravityScale = GravityScale;

	ControlledLaunchManager->Initialize(this);

	EnsureMovementModesInitialized();
}

void UMCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Cache input vector so it can be used in other contexts
	MovementInputVectorLast = GetPendingInputVector();
	if (!MovementInputVectorLast.IsNearlyZero())
		MovementInputVectorActiveLast = MovementInputVectorLast;

	// Manage controlled launch and apply multipliers
	if (IsValid(ControlledLaunchManager))
	{
		ControlledLaunchManager->TickLaunches(DeltaTime);

		const FMControlledLaunchManager_ProcessResult ControlledLaunchResult = ControlledLaunchManager->Process(Acceleration);

		BrakingDecelerationWalking = DefaultValues.BrakingDecelerationWalking * ControlledLaunchResult.BrakingDecelerationMultiplier;
		BrakingDecelerationFalling = DefaultValues.BrakingDecelerationFalling * ControlledLaunchResult.BrakingDecelerationMultiplier;
		BrakingFrictionFactor = DefaultValues.BrakingFrictionFactor * ControlledLaunchResult.BrakingDecelerationMultiplier;
		GravityScale = DefaultValues.GravityScale * ControlledLaunchResult.GravityMultiplier;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateTemporalHorizontalVelocityEntry();

	// Tick Movement Modes
	for (UMMovementMode_Base* CustomMovementModeInstance : CustomMovementModeInstances)
	{
		CustomMovementModeInstance->Tick(DeltaTime);
	}
}

FRotator UMCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime,
                                                                       FRotator& DeltaRotation) const
{
	// Compute for custom movement mode if it implements ISKMovementMode_OrientToMovementInterface
	UMMovementMode_Base* CustomMovementModeCurrent = GetActiveCustomMovementModeInstance();
	if (IsValid(CustomMovementModeCurrent) && CustomMovementModeCurrent->Implements<UMMovementMode_OrientToMovementInterface>())
	{
		return IMMovementMode_OrientToMovementInterface::Execute_ComputeOrientToMovementRotation(
			CustomMovementModeCurrent, CurrentRotation, DeltaTime, DeltaRotation);
	}

	return ComputeCharacterOrientation(CurrentRotation, DeltaTime, DeltaRotation);
}

void UMCharacterMovementComponent::AddControlledLaunchFromParams(const FVector& LaunchVelocity,
                                                                 const FMControlledLaunchParams& LaunchParams,
                                                                 UObject* Owner)
{
	if (!IsValid(ControlledLaunchManager))
	{
		return;
	}

	ControlledLaunchManager->AddControlledLaunch(LaunchVelocity, LaunchParams, Owner);
}

void UMCharacterMovementComponent::AddControlledLaunchFromAsset(const FVector& LaunchVelocity, UMControlledLaunchAsset* LaunchAsset,
                                                                UObject* Owner)
{
	AddControlledLaunchFromParams(LaunchVelocity, LaunchAsset->LaunchParams, Owner);
}

UMMovementMode_Base* UMCharacterMovementComponent::GetCustomMovementModeInstance(
	const TSubclassOf<UMMovementMode_Base> MovementModeClass) const
{
	for (const auto MovementModeInstance : CustomMovementModeInstances)
	{
		const UClass* CommonBaseClass = UClass::FindCommonBase(MovementModeClass, MovementModeInstance->GetClass());
		if (CommonBaseClass == MovementModeClass)
			return MovementModeInstance;
	}

	return nullptr;
}

FVector UMCharacterMovementComponent::GetHorizontalVelocity() const
{
	FVector Result = Velocity;
	Result.Z = 0;

	return Result;
}

FVector UMCharacterMovementComponent::GetPeakTemporalHorizontalVelocity() const
{
	FVector VelocityResult = GetHorizontalVelocity();
	float SpeedMax = VelocityResult.Size2D();
	for (const FVector& TemporalVelocity : TemporalHorizontalVelocityArray)
	{
		const float Speed = TemporalVelocity.Size2D();
		if (Speed > SpeedMax)
		{
			VelocityResult = TemporalVelocity;
			SpeedMax = Speed;
		}
	}

	return VelocityResult;
}

bool UMCharacterMovementComponent::IsMovingOnSurface() const
{
	if (IsCurrentMovementModeCustom())
	{
		UMMovementMode_Base* ActiveCustomMovementModeInstance = GetActiveCustomMovementModeInstance();
		if (IsValid(ActiveCustomMovementModeInstance))
		{
			return ActiveCustomMovementModeInstance->IsMovingOnSurface();
		}
	}

	return MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking;
}

void UMCharacterMovementComponent::ClearTemporalHorizontalVelocity()
{
	TemporalHorizontalVelocityArray.Empty();
}

FVector UMCharacterMovementComponent::GetDirectionAlongFloorForDirection(const FVector& Direction) const
{
	return MMath::GetDirectorAlongSurfaceForDirection(CurrentFloor.HitResult.ImpactNormal, Direction);
}

void UMCharacterMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

float UMCharacterMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit,
                                                      bool bHandleImpact)
{
	return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

bool UMCharacterMovementComponent::CanAttemptJump() const
{
	// Disables built-in jumping feature, so custom movement modes can implement their own jump
	if (MovementMode == MOVE_Custom)
		return false;

	return Super::CanAttemptJump();
}

#if ENABLE_VISUAL_LOG
void UMCharacterMovementComponent::GrabDebugSnapshot(struct FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory& CmpCategory = Snapshot->Status.Last();
	FName CmpCategoryName = FName(*CmpCategory.Category);

	// Draw Character Capsule
	auto CapsuleComponent = CharacterOwner->GetCapsuleComponent();
	const FVector CapsuleBase = CapsuleComponent->GetComponentLocation()
		- FVector::UpVector * CapsuleComponent->GetScaledCapsuleHalfHeight();
	const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const FQuat CapsuleQuat = CapsuleComponent->GetComponentQuat();

	Snapshot->AddCapsule(CapsuleBase, CapsuleHalfHeight, CapsuleRadius, CapsuleQuat, CmpCategoryName, ELogVerbosity::Display,
	                     FColor::White, TEXT("Character Capsule"), /*bInUseWires*/ true);

	static constexpr float ArrowLength = 150;
	const FVector ArrowStart = CharacterOwner->GetActorLocation();

	// Draw Facing Direction
	const FVector FacingDirectionZOffset = FVector::UpVector * 50;
	const FVector FacingDirectionArrowEnd = ArrowStart + CharacterOwner->GetActorForwardVector() * ArrowLength + FacingDirectionZOffset;
	Snapshot->AddArrow(ArrowStart + FacingDirectionZOffset, FacingDirectionArrowEnd, CmpCategoryName, ELogVerbosity::Display,
	                   FColor::White, TEXT("Facing Direction"));

	// Draw Velocity Direction
	if (!Velocity.IsNearlyZero())
	{
		const FVector VelocityDirectionArrowEnd = ArrowStart + Velocity.GetSafeNormal() * ArrowLength;
		Snapshot->AddArrow(ArrowStart, VelocityDirectionArrowEnd, CmpCategoryName, ELogVerbosity::Display,
		                   FColor::Blue, TEXT("Velocity Direction"));
	}

	// Draw Input
	const FVector InputVector = GetMovementInputVectorLast();
	if (!InputVector.IsNearlyZero())
	{
		const FVector InputDirectionZOffset = FVector::UpVector * 100;
		const FVector InputDirectionArrowEnd = ArrowStart + InputVector * ArrowLength +
			InputDirectionZOffset;
		Snapshot->AddArrow(ArrowStart + InputDirectionZOffset, InputDirectionArrowEnd, CmpCategoryName,
		                   ELogVerbosity::Display,
		                   FColor::Green, TEXT("Input Direction"));
	}

	// Draw Acceleration
	const FVector AccelerationDirectionZOffset = FVector::UpVector * 70;
	if (!Acceleration.IsNearlyZero())
	{
		const FVector AccelerationDirectionArrowEnd = ArrowStart + Acceleration.GetSafeNormal() * ArrowLength +
			AccelerationDirectionZOffset;
		Snapshot->AddArrow(ArrowStart + AccelerationDirectionZOffset, AccelerationDirectionArrowEnd, CmpCategoryName,
		                   ELogVerbosity::Display,
		                   FColor::Green.WithAlpha(0.5f), TEXT("Acceleration Direction"));
	}

	const float HorizontalSpeed = FVector2D(Velocity.X, Velocity.Y).Length();
	CmpCategory.Add(TEXT("H Speed"), FString::Printf(TEXT("%.2f"), HorizontalSpeed));

	const float VerticalSpeed = Velocity.Z;
	CmpCategory.Add(TEXT("V Speed"), FString::Printf(TEXT("%.2f"), VerticalSpeed));

	const float AccelerationLength = Acceleration.Size();
	CmpCategory.Add(TEXT("Acceleration"), FString::Printf(TEXT("%.2f"), AccelerationLength));

	for (const auto CustomMovementModeInstance : CustomMovementModeInstances)
	{
		const IVisualLoggerDebugSnapshotInterface* VisualLoggerMovementMode =
			Cast<IVisualLoggerDebugSnapshotInterface>(CustomMovementModeInstance);
		VisualLoggerMovementMode->GrabDebugSnapshot(Snapshot);
	}

	if (ControlledLaunchManager != nullptr)
	{
		ControlledLaunchManager->GrabDebugSnapshot(Snapshot);
	}
}
#endif

void UMCharacterMovementComponent::Reset_Implementation(bool bHardReset)
{
	SetMovementMode(MOVE_Falling);

	Velocity = FVector::ZeroVector;

	if (IsValid(ControlledLaunchManager))
	{
		ControlledLaunchManager->ClearAllLaunches();
	}

	// Call Reset on movement mode instances
	for (auto CustomMovementModeInstance : CustomMovementModeInstances)
	{
		if (CustomMovementModeInstance->Implements<UMResettable>())
		{
			IMResettable::Execute_Reset(CustomMovementModeInstance, bHardReset);
		}
	}

	ClearTemporalHorizontalVelocity();
}

void UMCharacterMovementComponent::EnsureMovementModesInitialized()
{
	// Fix for crash when this is called from Animation Blueprint outside of PIE
#if WITH_EDITOR
	if (!GEditor->IsPlayingSessionInEditor())
	{
		return;
	}
#endif

	if (bMovementModesInitialized)
		return;

	// Initialize movement modes
	for (auto MovementModeClass : AvailableMovementModes)
	{
		UMMovementMode_Base* CustomMovementModeInstance = NewObject<UMMovementMode_Base>(this, MovementModeClass);
		CustomMovementModeInstance->Initialize(GetCharacterOwner(), this);

		CustomMovementModeInstances.Emplace(CustomMovementModeInstance);
	}

	bMovementModesInitialized = true;
}

bool UMCharacterMovementComponent::IsCurrentMovementModeCustom() const
{
	return MovementMode == MOVE_Custom;
}

UMMovementMode_Base* UMCharacterMovementComponent::GetActiveCustomMovementModeInstance() const
{
	if (MovementMode != MOVE_Custom)
	{
		return nullptr;
	}

	UMMovementMode_Base* CustomMovementModeInstance = GetCustomMovementModeInstanceForEnum(CustomMovementMode);
	return CustomMovementModeInstance;
}

void UMCharacterMovementComponent::DrawDebugDirection(const FVector& Direction, const FColor& Color, const FString& Text) const
{
	FVector EyesPoint;
	FRotator EyesRotation;
	CharacterOwner->GetActorEyesViewPoint(EyesPoint, EyesRotation);

	const FVector DirectionDebugOrigin = EyesPoint
		+ CharacterOwner->GetControlRotation().Vector() * 150
		+ (CharacterOwner->GetActorUpVector() + CharacterOwner->GetActorRightVector()) * 50;
	DrawDebugPoint(GetWorld(), DirectionDebugOrigin, 20, FColor::Black);

	constexpr float DirectionDebugLength = 60;
	const FVector DirectionDebugLineEnd = DirectionDebugOrigin + Direction * DirectionDebugLength;
	constexpr float ArrowSize = 5;
	DrawDebugDirectionalArrow(GetWorld(), DirectionDebugOrigin, DirectionDebugLineEnd, ArrowSize, Color,
	                          false, -1, 0, 1);

	FString DebugString = FString::Printf(TEXT("%s (%.2f)"), *Text, Direction.Size());
	DrawDebugString(GetWorld(), DirectionDebugLineEnd, DebugString, 0, FColor::White, 0, true, 1);
}

void UMCharacterMovementComponent::DrawDebugCharacterCapsule(const FColor& Color, const float Duration, const FString& Text) const
{
	auto CapsuleComponent = CharacterOwner->GetCapsuleComponent();
	const FVector CapsuleCenter = CapsuleComponent->GetComponentLocation();
	const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	DrawDebugCapsule(GetWorld(), CapsuleCenter, CapsuleHalfHeight, CapsuleComponent->GetScaledCapsuleRadius(),
	                 FQuat::Identity, Color, false, 5, 5, 2);

	const FVector TextLocation = CapsuleCenter + FVector::UpVector * CapsuleHalfHeight;
	DrawDebugString(GetWorld(), TextLocation, Text, 0, Color, Duration, true, 1);
}

void UMCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	// Start custom movement mode
	for (int i = 0; i < CustomMovementModeInstances.Num(); ++i)
	{
		auto CustomMovementModeInstance = CustomMovementModeInstances[i];

		FString CanStartFailReason;
		if (CustomMovementModeInstance->CanStart(CanStartFailReason))
		{
			SetMovementMode(MOVE_Custom, i);
			break;
		}

		CustomMovementModeInstance->SetCanStartFailReasonCache(CanStartFailReason);
	}

	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

void UMCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// Run custom movement mode end
	if (PreviousMovementMode == MOVE_Custom)
	{
		if (auto CustomMovementModeInstance = GetCustomMovementModeInstanceForEnum(PreviousCustomMode))
			CustomMovementModeInstance->End();
	}

	// Run custom movement mode start
	if (MovementMode == MOVE_Custom)
	{
		if (auto CustomMovementModeInstance = GetCustomMovementModeInstanceForEnum(CustomMovementMode))
			CustomMovementModeInstance->Start();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UMCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (IsCurrentMovementModeCustom())
	{
		UMMovementMode_Base* ActiveCustomMovementModeInstance = GetActiveCustomMovementModeInstance();
		if (IsValid(ActiveCustomMovementModeInstance))
		{
			ActiveCustomMovementModeInstance->Phys(deltaTime, Iterations);
		}
	}

	Super::PhysCustom(deltaTime, Iterations);
}

bool UMCharacterMovementComponent::CanCrouchInCurrentState() const
{
	UMMovementMode_Base* ActiveCustomMovementMode = GetActiveCustomMovementModeInstance();
	if (IsValid(ActiveCustomMovementMode))
	{
		return ActiveCustomMovementMode->CanCrouch();
	}

	return Super::CanCrouchInCurrentState();
}

bool UMCharacterMovementComponent::IsMovingOnGround() const
{
	UMMovementMode_Base* ActiveCustomMovementMode = GetActiveCustomMovementModeInstance();
	if (IsValid(ActiveCustomMovementMode))
	{
		return ActiveCustomMovementMode->IsMovingOnGround();
	}

	return Super::IsMovingOnGround();
}

bool UMCharacterMovementComponent::IsFalling() const
{
	return !IsMovingOnSurface();
}

bool UMCharacterMovementComponent::IsWalkable(const FHitResult& Hit) const
{
	if (IsValid(ControlledLaunchManager) && ControlledLaunchManager->IsWalkBlockedByControlledLaunch())
		return false;

	return Super::IsWalkable(Hit);
}

FVector UMCharacterMovementComponent::ScaleInputAcceleration(const FVector& InputAcceleration) const
{
	const FVector InputAccelerationScaled = Super::ScaleInputAcceleration(InputAcceleration);

	FVector Result = InputAccelerationScaled;
	if (IsValid(ControlledLaunchManager))
	{
		const FMControlledLaunchManager_ProcessResult ControlledLaunchResult = ControlledLaunchManager->Process(InputAccelerationScaled);
		Result = ControlledLaunchResult.Acceleration;
	}

	return Result;
}

UPrimitiveComponent* UMCharacterMovementComponent::GetMovementBaseCustom() const
{
	UMMovementMode_Base* ActiveCustomMovementMode = GetActiveCustomMovementModeInstance();
	if (IsValid(ActiveCustomMovementMode) && ActiveCustomMovementMode->IsUsingCustomMovementBase())
	{
		return CustomMovementBase;
	}

	return GetMovementBase();
}

void UMCharacterMovementComponent::UpdateTemporalHorizontalVelocityEntry()
{
	TemporalHorizontalVelocityArray.Emplace(GetHorizontalVelocity());
	if (TemporalHorizontalVelocityArray.Num() > TemporalPeakHorizontalVelocityHistoryFramesAmount)
		TemporalHorizontalVelocityArray.RemoveAt(0);
}

void UMCharacterMovementComponent::SetCustomMovementBase(UPrimitiveComponent* InCustomMovementBase)
{
	CustomMovementBase = InCustomMovementBase;
}

UMMovementMode_Base* UMCharacterMovementComponent::GetCustomMovementModeInstanceForEnum(uint8 EnumValue) const
{
	if (!CustomMovementModeInstances.IsValidIndex(EnumValue))
	{
		UE_LOG(LogTemp, Error, TEXT("Trying to get Custom Movement Mode for value out of CustomMovementModeInstances range"));
		return nullptr;
	}

	return CustomMovementModeInstances[EnumValue];
}

FRotator UMCharacterMovementComponent::ComputeCharacterOrientation(const FRotator& CurrentRotation,
                                                                   const float DeltaTime,
                                                                   FRotator& DeltaRotation) const
{
	return Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);
}
