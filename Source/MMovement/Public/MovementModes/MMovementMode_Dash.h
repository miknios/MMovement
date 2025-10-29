// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MManualTimer.h"
#include "MMovementMode_Base.h"
#include "MMovementMode_Dash.generated.h"

USTRUCT(BlueprintType)
struct FMOnDashChargeUpdatedData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Delta = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ChargeAmountCurrent = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bPlayUIAnimation;

	FMOnDashChargeUpdatedData() = default;

	FMOnDashChargeUpdatedData(int32 Delta, int32 ChargeAmountCurrent, bool bPlayUIAnimation)
		: Delta(Delta),
		  ChargeAmountCurrent(ChargeAmountCurrent),
		  bPlayUIAnimation(bPlayUIAnimation)
	{
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOnDashChargeUpdatedSignature, FMOnDashChargeUpdatedData, OnDashChargeUpdatedData);

class UMControlledLaunchAsset;
struct FInputActionInstance;
class UInputAction;

USTRUCT(BlueprintType)
struct FMCharacterMovement_DashConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditAnywhere)
	float Distance;

	UPROPERTY(EditAnywhere)
	float Duration = 0.3f;

	UPROPERTY(EditAnywhere)
	float CooldownTime = 1;

	// Normalized to Distance (Y) and Duration (X)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> DistanceCurve;

	// Should the dash happen only in horizontal space or in the exact direction we have in the moment of dash
	UPROPERTY(EditAnywhere)
	bool bUseVerticalDirection = true;

	// Use walk input direction to determine dash direction. If walk input is not pressed then aim direction is used
	UPROPERTY(EditAnywhere)
	bool bUseInputDirection = true;
	
	UPROPERTY(EditAnywhere, Category = "Dash Config|Controlled Launch")
	bool bApplyControlledLaunchOnFinish = false;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Controlled Launch", meta = (EditCondition = "bApplyControlledLaunchOnFinish"))
	UMControlledLaunchAsset* LaunchParams = nullptr;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Velocity Preservation")
	bool bPreserveVelocityOnlyInDashDirection = true;

	// If speed before dash was lower than this, this speed will be applied in dash direction
	UPROPERTY(EditAnywhere, Category = "Dash Config|Velocity Preservation")
	float PreservedSpeedMin = 1000;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Charges")
	bool bEnableDashCharges;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Charges")
	int ChargeAmountInitial = 1;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Charges")
	int ChargeAmountMax = 1;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Charges")
	bool bRestoreChargesOnGround = true;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Charges")
	bool bRestoreChargesOnWallRun = false;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Charges")
	bool bRestoreChargesOnVerticalWallRun = false;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Damage")
	bool bEnableDamage = true;

	UPROPERTY(EditAnywhere, Category = "Dash Config|Damage")
	float DamageAmount = 10;
	
	UPROPERTY(EditAnywhere, Category = "Dash Config|Damage")
	float DamageCapsuleScale = 1.5f;
};

USTRUCT(BlueprintType)
struct FMCharacterMovement_DashRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly)
	bool bInitialValuesCalculated;	

	UPROPERTY(VisibleInstanceOnly)
	int32 ChargesLeft;

	UPROPERTY(VisibleInstanceOnly)
	bool bWantsToDash;

	UPROPERTY(VisibleInstanceOnly)
	FVector DashDirection;

	UPROPERTY(VisibleInstanceOnly)
	FMManualTimer CooldownTimer;

	UPROPERTY(VisibleInstanceOnly)
	FMManualTimer DurationTimer;

	UPROPERTY(VisibleInstanceOnly)
	FVector LocationInitial;

	UPROPERTY(VisibleInstanceOnly)
	FVector VelocityPreserved;

	UPROPERTY(VisibleInstanceOnly)
	TSet<AActor*> DamagedActors;
};

UCLASS()
class MMOVEMENT_API UMMovementMode_Dash : public UMMovementMode_Base
{
	GENERATED_BODY()

public:
	// ~ UMMovementMode_Base
	virtual void Initialize_Implementation() override;
	virtual void Tick_Implementation(float DeltaTime) override;
	virtual bool CanStart_Implementation(FString& OutFailReason) override;
	virtual void Start_Implementation() override;
	virtual void Phys_Implementation(float DeltaTime, int32 Iterations) override;
	virtual void End_Implementation() override;
	virtual bool IsMovingOnGround_Implementation() override;
	virtual bool IsMovingOnSurface_Implementation() override;
	// ~ UMMovementMode_Base

	UFUNCTION(BlueprintCallable)
	void AddDashCharge(bool bPlayUIAnimation = false);

	UFUNCTION(BlueprintCallable)
	bool CanAddDashCharge() const;

	UFUNCTION(BlueprintCallable)
	void ResetCharges(bool bPlayUIAnimation = false);

	UFUNCTION(BlueprintCallable)
	void UseDashCharge(bool bPlayUIAnimation = false);

	UFUNCTION(BlueprintCallable)
	int32 GetChargeAmountMax() const { return DashConfig.ChargeAmountMax; }

	UFUNCTION(BlueprintCallable)
	int32 GetChargeAmountCurrent() const { return RuntimeData.ChargesLeft; }

public:
	UPROPERTY(BlueprintAssignable)
	FMOnDashChargeUpdatedSignature OnDashChargeUpdatedDelegate;

protected:
	UFUNCTION()
	void OnDashInput(const FInputActionInstance& Instance);

	void OnDashChargeAmountChanged(int32 ValueOld, int32 ValueNew, bool bPlayUIAnimation) const;

	void CalculateInitialValues();

	void DealDamage(const FVector& LocationOld, const FVector& LocationNew);

protected:
	UPROPERTY(EditAnywhere, Category = "Dash Config", meta = (ShowOnlyInnerProperties))
	FMCharacterMovement_DashConfig DashConfig;

	UPROPERTY(Transient, EditAnywhere, Category = "Dash Runtime Data", meta = (ShowOnlyInnerProperties))
	FMCharacterMovement_DashRuntimeData RuntimeData;
};
