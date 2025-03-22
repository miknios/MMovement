// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MMath.h"
#include "MMovementTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MMovementMath.generated.h"

/**
 * 
 */
UCLASS()
class MMOVEMENT_API UMMovementMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FVector GetSurfaceDownwardSlopeHorizontalDirection(const FVector& SurfaceNormal)
	{
		FVector HorizontalNormalSlopeDirection = SurfaceNormal;
		HorizontalNormalSlopeDirection.Z = 0;
		HorizontalNormalSlopeDirection.Normalize();

		return HorizontalNormalSlopeDirection;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static EMSlopeDirection GetSurfaceSlopeDirection(const FVector& SurfaceNormal, const FVector& MovementDirection)
	{
		const FVector DownwardSlopeHorizontalDirection = GetSurfaceDownwardSlopeHorizontalDirection(SurfaceNormal);

		FVector InputHorizontalDirection = MovementDirection;
		InputHorizontalDirection.Z = 0;
		InputHorizontalDirection.Normalize();

		const float Dot = DownwardSlopeHorizontalDirection.Dot(InputHorizontalDirection);
		if (Dot > 0)
			return EMSlopeDirection::Down;

		if (Dot < 0)
			return EMSlopeDirection::Up;

		return EMSlopeDirection::EvenSurface;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static bool IsDirectionForDownwardSlope(const FVector& SurfaceNormal, const FVector& MovementDirection)
	{
		return GetSurfaceSlopeDirection(SurfaceNormal, MovementDirection) == EMSlopeDirection::Down;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static bool IsDirectionForUpwardSlope(const FVector& SurfaceNormal, const FVector& MovementDirection)
	{
		return GetSurfaceSlopeDirection(SurfaceNormal, MovementDirection) == EMSlopeDirection::Up;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FVector GetDirectionRotatedTowardsTargetWithRate(const FVector& Direction, const FVector& DirectionTarget,
	                                                        const float Rate, const float DeltaTime)
	{
		return MMath::RotateDirectionTowardsTargetDirectionByAngle(Direction, DirectionTarget, Rate * DeltaTime);
	}
};
