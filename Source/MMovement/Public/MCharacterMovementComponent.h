// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MResettable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "MCharacterMovementComponent.generated.h"

class UMControlledLaunchManager;
class UMControlledLaunchAsset;
struct FMControlledLaunchParams;
enum EMCustomMovementMode : uint8;
class UMMovementMode_Base;

USTRUCT(BlueprintType)
struct FMCharacterMovementComponent_DefaultValues
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly)
	float MaxWalkSpeed;

	UPROPERTY(VisibleInstanceOnly)
	float MaxAcceleration;

	UPROPERTY(VisibleInstanceOnly)
	float BrakingDecelerationWalking;

	UPROPERTY(VisibleInstanceOnly)
	float BrakingDecelerationFalling;

	UPROPERTY(VisibleInstanceOnly)
	float GravityScale;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOnJumpedSignature);

/**
 * Base class for CMC that can use custom movement modes and some other features
 */
UCLASS()
class MMOVEMENT_API UMCharacterMovementComponent : public UCharacterMovementComponent,
                                                   public IVisualLoggerDebugSnapshotInterface,
                                                   public IMResettable
{
	GENERATED_BODY()

public:
	UMCharacterMovementComponent();

	// ~ UActorComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// ~ UActorComponent

	// ~ UCharacterMovementComponent
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;
	virtual bool CanAttemptJump() const override;
	virtual bool IsMovingOnGround() const override;
	virtual bool IsFalling() const override;
	// ~ UCharacterMovementComponent

	// IVisualLoggerDebugSnapshotInterface
#if ENABLE_VISUAL_LOG
	virtual void GrabDebugSnapshot(struct FVisualLogEntry* Snapshot) const override;
#endif
	// ~ IVisualLoggerDebugSnapshotInterface

	// ~ IMResettable
	virtual void Reset_Implementation(bool bHardReset) override;
	// ~ IMResettable

	UFUNCTION(BlueprintCallable)
	void EnsureMovementModesInitialized();

	UFUNCTION(BlueprintCallable)
	UPrimitiveComponent* GetMovementBaseCustom() const;

	UFUNCTION(BlueprintCallable)
	void AddControlledLaunchFromParams(const FVector& LaunchVelocity, const FMControlledLaunchParams& LaunchParams,
	                                   UObject* Owner = nullptr);

	UFUNCTION(BlueprintCallable)
	void AddControlledLaunchFromAsset(const FVector& LaunchVelocity, UMControlledLaunchAsset* LaunchAsset, UObject* Owner = nullptr);

	UFUNCTION(BlueprintCallable)
	UMMovementMode_Base* GetCustomMovementModeInstance(TSubclassOf<UMMovementMode_Base> MovementModeClass) const;

	UFUNCTION(BlueprintCallable)
	UMControlledLaunchManager* GetControlledLaunchManager() const { return ControlledLaunchManager; }

	UFUNCTION(BlueprintCallable)
	FVector GetHorizontalVelocity() const;

	UFUNCTION(BlueprintCallable)
	FVector GetPeakTemporalHorizontalVelocity() const;

	UFUNCTION(BlueprintCallable)
	bool IsMovingOnSurface() const;

	UFUNCTION(BlueprintCallable)
	void ClearTemporalHorizontalVelocity();

	UFUNCTION(BlueprintCallable)
	FVector GetDirectionAlongFloorForDirection(const FVector& Direction) const;

	// Last movement direction input vector
	UFUNCTION(BlueprintCallable)
	FVector GetMovementInputVectorLast() const { return MovementInputVectorLast; }

	// Last non-zero movement direction input vector
	UFUNCTION(BlueprintCallable)
	FVector GetMovementInputVectorActiveLast() const { return MovementInputVectorActiveLast; }

	UFUNCTION(BlueprintCallable)
	bool IsCurrentMovementModeCustom() const;

	/**
	 * Returns currently active custom movement mode instance
	 * nullptr if current movement mode is not custom
	 */
	UFUNCTION(BlueprintCallable)
	UMMovementMode_Base* GetActiveCustomMovementModeInstance() const;

	void SetAcceleration(const FVector& AccelerationNew) { Acceleration = AccelerationNew; }
	void SetCustomMovementBase(UPrimitiveComponent* InCustomMovementBase);
	
	void DrawDebugDirection(const FVector& Direction, const FColor& Color, const FString& Text = FString()) const;
	void DrawDebugCharacterCapsule(const FColor& Color, const float Duration = -1, const FString& Text = FString()) const;

public:
	UPROPERTY(BlueprintAssignable)
	FMOnJumpedSignature OnJumpedDelegate;

protected:
	// ~ UCharacterMovementComponent
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual bool IsWalkable(const FHitResult& Hit) const override;
	virtual FVector ScaleInputAcceleration(const FVector& InputAcceleration) const override;
	// ~ UCharacterMovementComponent

	void UpdateTemporalHorizontalVelocityEntry();
	UMMovementMode_Base* GetCustomMovementModeInstanceForEnum(uint8 EnumValue) const;

	/**
	 * Override for custom character orientation logic (rotate to aim in top-down, rotate to velocity instead of acceleration etc.)
	 * This is skipped if the current movement mode implements IMMovementMode_OrientToMovementInterface
	 */
	virtual FRotator ComputeCharacterOrientation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const;

protected:
	UPROPERTY(EditAnywhere, Category = "Movement|Movement Modes")
	TArray<TSubclassOf<UMMovementMode_Base>> AvailableMovementModes;

	// How many frames are included to get Temporal Peak Horizontal Velocity
	UPROPERTY(EditAnywhere, Category = "Movement|Modes")
	float TemporalPeakHorizontalVelocityHistoryFramesAmount = 3;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Movement|Modes")
	TArray<UMMovementMode_Base*> CustomMovementModeInstances;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Movement|Controlled Launch")
	TObjectPtr<UMControlledLaunchManager> ControlledLaunchManager;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Movement")
	FMCharacterMovementComponent_DefaultValues DefaultValues;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Movement")
	TArray<FVector> TemporalHorizontalVelocityArray;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Movement")
	UPrimitiveComponent* CustomMovementBase;

	// Last movement direction input vector
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Movement")
	FVector MovementInputVectorLast;

	// Last non-zero movement direction input vector
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Movement")
	FVector MovementInputVectorActiveLast;

	bool bMovementModesInitialized;
};
