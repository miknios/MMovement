// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MResettable.h"
#include "UObject/Object.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "MMovementMode_Base.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOnMovementModeStartSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOnMovementModeUpdateSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOnMovementModeEndSignature);

enum EMCustomMovementMode : uint8;
class UMCharacterMovementComponent;
/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class MMOVEMENT_API UMMovementMode_Base : public UObject,
                                          public IVisualLoggerDebugSnapshotInterface,
                                          public IMResettable
{
	GENERATED_BODY()

public:
	// ~ IMResettable
	virtual void Reset_Implementation(bool bHardReset) override;
	// ~ IMResettable
	
	void Initialize(ACharacter* InCharacterOwner, UMCharacterMovementComponent* InMovementComponent);

	// Called at the moment of instantiation
	UFUNCTION(BlueprintNativeEvent)
	void Initialize();

	// Called every frame even when this movement mode is not active (called before Phys)
	UFUNCTION(BlueprintNativeEvent)
	void Tick(float DeltaTime);

	// Movement mode is started if this returns true
	UFUNCTION(BlueprintNativeEvent)
	bool CanStart(FString& OutFailReason);

	// Called when this movement mode goes active
	UFUNCTION(BlueprintNativeEvent)
	void Start();

	// Move character here. Called every frame while this movement mode is active
	UFUNCTION(BlueprintNativeEvent)
	void Phys(float DeltaTime, int32 Iterations);

	// Called when this movement mode is not active anymore
	UFUNCTION(BlueprintNativeEvent)
	void End();

	UFUNCTION(BLueprintNativeEvent)
	bool CanCrouch();

	UFUNCTION(BlueprintNativeEvent)
	bool IsMovingOnGround();

	UFUNCTION(BlueprintNativeEvent)
	bool IsMovingOnSurface();

	UFUNCTION(BlueprintNativeEvent)
	bool IsUsingCustomMovementBase();

	UFUNCTION(BlueprintCallable)
	bool IsMovementModeActive() const { return bMovementModeActive; }

	UFUNCTION(BlueprintCallable)
	FName GetMovementModeName() const;

	// Called every frame by MCharacterMovementComponent when this movement mode is active 
	void GetInputVector();

	FString GetCanStartFailReasonCache() const { return CanStartFailReasonCache; }
	void SetCanStartFailReasonCache(const FString& FailReason) { CanStartFailReasonCache = FailReason; }

protected:
	UFUNCTION(BlueprintCallable)
	FVector GetOwnerLocation() const;

#if ENABLE_VISUAL_LOG
	// Override to add info to visual log
	virtual void AddVisualLoggerInfo(struct FVisualLogEntry* Snapshot,
	                                 FVisualLogStatusCategory& MovementCmpCategory,
	                                 FVisualLogStatusCategory& MovementModeCategory) const;
#endif

private:
	// IVisualLoggerDebugSnapshotInterface
#if ENABLE_VISUAL_LOG
	virtual void GrabDebugSnapshot(struct FVisualLogEntry* Snapshot) const override;
#endif
	// ~ IVisualLoggerDebugSnapshotInterface

public:
	UPROPERTY(BlueprintAssignable)
	FMOnMovementModeStartSignature OnMovementModeStartDelegate;

	UPROPERTY(BlueprintAssignable)
	FMOnMovementModeUpdateSignature OnMovementModeUpdateDelegate;

	UPROPERTY(BlueprintAssignable)
	FMOnMovementModeEndSignature OnMovementModeEndDelegate;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName MovementModeName;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UMCharacterMovementComponent> MovementComponent;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ACharacter> CharacterOwner;

	// The component that is updated and moved
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<USceneComponent> UpdatedComponent;

	UPROPERTY(VisibleInstanceOnly, Category = "Debug")
	FVector InputVector;

	bool bMovementModeActive;

	FString CanStartFailReasonCache;
};
