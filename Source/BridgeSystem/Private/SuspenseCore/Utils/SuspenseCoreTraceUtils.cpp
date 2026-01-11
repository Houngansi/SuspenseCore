// SuspenseCoreTraceUtils.cpp
// SuspenseCore - Weapon Trace Utilities Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreTraceUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"

// Static constant initialization
const FName USuspenseCoreTraceUtils::DefaultWeaponTraceProfile = FName("Weapon");

//========================================================================
// Line Tracing
//========================================================================

bool USuspenseCoreTraceUtils::PerformLineTrace(
	const UObject* WorldContextObject,
	const FVector& Start,
	const FVector& End,
	FName TraceProfile,
	const TArray<AActor*>& ActorsToIgnore,
	bool bDebug,
	float DebugDrawTime,
	TArray<FHitResult>& OutHits)
{
	if (!WorldContextObject)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	// Setup collision query params
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	// Perform multi-hit line trace
	bool bHasBlockingHit = World->LineTraceMultiByProfile(
		OutHits,
		Start,
		End,
		TraceProfile,
		QueryParams
	);

	// If no hits, add a "miss" result at the end point
	if (OutHits.Num() == 0)
	{
		FHitResult MissHit;
		MissHit.TraceStart = Start;
		MissHit.TraceEnd = End;
		MissHit.Location = End;
		MissHit.ImpactPoint = End;
		MissHit.bBlockingHit = false;
		OutHits.Add(MissHit);
	}

	// Debug visualization
	if (bDebug)
	{
		DrawDebugTrace(WorldContextObject, Start, OutHits, DebugDrawTime);
	}

	return bHasBlockingHit;
}

bool USuspenseCoreTraceUtils::PerformLineTraceSingle(
	const UObject* WorldContextObject,
	const FVector& Start,
	const FVector& End,
	FName TraceProfile,
	const TArray<AActor*>& ActorsToIgnore,
	FHitResult& OutHit)
{
	if (!WorldContextObject)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	// Setup collision query params
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	// Perform single line trace
	return World->LineTraceSingleByProfile(
		OutHit,
		Start,
		End,
		TraceProfile,
		QueryParams
	);
}

//========================================================================
// Aim Point Calculation
//========================================================================

bool USuspenseCoreTraceUtils::GetAimPoint(
	APlayerController* PlayerController,
	float MaxRange,
	FVector& OutCameraLocation,
	FVector& OutAimPoint)
{
	if (!PlayerController)
	{
		return false;
	}

	// Get camera viewpoint
	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	OutCameraLocation = CameraLocation;

	// Calculate trace end point
	const FVector TraceDirection = CameraRotation.Vector();
	const FVector TraceEnd = CameraLocation + (TraceDirection * MaxRange);

	// Perform trace to find aim target
	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		OutAimPoint = TraceEnd;
		return true;
	}

	// Setup collision query params
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	QueryParams.AddIgnoredActor(PlayerController->GetPawn());

	FHitResult HitResult;
	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		CameraLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	if (bHit && HitResult.bBlockingHit)
	{
		OutAimPoint = HitResult.ImpactPoint;
	}
	else
	{
		OutAimPoint = TraceEnd;
	}

	return true;
}

bool USuspenseCoreTraceUtils::GetAimDirection(
	APlayerController* PlayerController,
	FVector& OutDirection)
{
	if (!PlayerController)
	{
		OutDirection = FVector::ForwardVector;
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	OutDirection = CameraRotation.Vector();
	return true;
}

//========================================================================
// Debug Visualization
//========================================================================

void USuspenseCoreTraceUtils::DrawDebugTrace(
	const UObject* WorldContextObject,
	const FVector& Start,
	const TArray<FHitResult>& HitResults,
	float DrawTime)
{
	if (!WorldContextObject)
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	FVector LastPoint = Start;
	bool bFoundBlockingHit = false;

	for (const FHitResult& Hit : HitResults)
	{
		// Determine color based on hit type
		FColor LineColor;
		if (Hit.bBlockingHit)
		{
			LineColor = FColor::Red;
			bFoundBlockingHit = true;
		}
		else if (Hit.GetActor() != nullptr)
		{
			LineColor = FColor::Orange; // Non-blocking hit (penetration)
		}
		else
		{
			LineColor = FColor::Green; // No hit
		}

		// Draw line segment
		const FVector HitPoint = Hit.bBlockingHit ? Hit.ImpactPoint : Hit.Location;
		DrawDebugLine(World, LastPoint, HitPoint, LineColor, false, DrawTime, 0, 2.0f);

		// Draw hit visualization
		if (Hit.bBlockingHit || Hit.GetActor() != nullptr)
		{
			DrawDebugHit(WorldContextObject, Hit, Hit.bBlockingHit, DrawTime);
		}

		LastPoint = HitPoint;

		// Stop after blocking hit
		if (Hit.bBlockingHit)
		{
			break;
		}
	}

	// If no blocking hit, draw remaining line in green
	if (!bFoundBlockingHit && HitResults.Num() > 0)
	{
		const FVector EndPoint = HitResults.Last().TraceEnd;
		if (!LastPoint.Equals(EndPoint))
		{
			DrawDebugLine(World, LastPoint, EndPoint, FColor::Green, false, DrawTime, 0, 2.0f);
		}
	}
}

void USuspenseCoreTraceUtils::DrawDebugHit(
	const UObject* WorldContextObject,
	const FHitResult& Hit,
	bool bIsBlockingHit,
	float DrawTime)
{
	if (!WorldContextObject)
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector HitPoint = Hit.ImpactPoint;
	const FColor SphereColor = bIsBlockingHit ? FColor::Red : FColor::Orange;

	// Draw impact sphere
	DrawDebugSphere(World, HitPoint, DebugSphereRadius, 8, SphereColor, false, DrawTime);

	// Draw surface normal
	DrawDebugLine(
		World,
		HitPoint,
		HitPoint + (Hit.ImpactNormal * DebugNormalLength),
		FColor::Blue,
		false,
		DrawTime,
		0,
		1.5f
	);

	// Draw actor name if available
	if (AActor* HitActor = Hit.GetActor())
	{
		DrawDebugString(
			World,
			HitPoint + FVector(0, 0, 20),
			HitActor->GetName(),
			nullptr,
			FColor::White,
			DrawTime
		);
	}
}

//========================================================================
// Utility Functions
//========================================================================

FVector USuspenseCoreTraceUtils::ApplySpreadToDirection(
	const FVector& Direction,
	float SpreadAngle,
	int32 RandomSeed)
{
	if (SpreadAngle <= 0.0f)
	{
		return Direction;
	}

	// Create random stream for deterministic spread
	FRandomStream RandomStream;
	if (RandomSeed != 0)
	{
		RandomStream.Initialize(RandomSeed);
	}
	else
	{
		RandomStream.GenerateNewSeed();
	}

	// Convert spread angle to radians (half-cone angle)
	const float SpreadRadians = FMath::DegreesToRadians(SpreadAngle);
	const float ConeHalfAngle = SpreadRadians * 0.5f;

	// Generate random point within cone
	const float RandomConeAngle = RandomStream.FRandRange(0.0f, ConeHalfAngle);
	const float RandomRotAngle = RandomStream.FRandRange(0.0f, 2.0f * PI);

	// Find perpendicular vectors for rotation
	FVector UpVector = FVector::UpVector;
	if (FMath::Abs(Direction.Z) > 0.99f)
	{
		UpVector = FVector::RightVector;
	}

	const FVector RightVector = FVector::CrossProduct(Direction, UpVector).GetSafeNormal();
	const FVector TrueUpVector = FVector::CrossProduct(RightVector, Direction).GetSafeNormal();

	// Apply cone spread
	FVector SpreadDirection = Direction.RotateAngleAxis(
		FMath::RadiansToDegrees(RandomConeAngle),
		RightVector
	);

	// Apply random rotation around original direction
	SpreadDirection = SpreadDirection.RotateAngleAxis(
		FMath::RadiansToDegrees(RandomRotAngle),
		Direction
	);

	return SpreadDirection.GetSafeNormal();
}

FVector USuspenseCoreTraceUtils::CalculateTraceEndPoint(
	const FVector& Start,
	const FVector& Direction,
	float Range)
{
	return Start + (Direction.GetSafeNormal() * Range);
}

bool USuspenseCoreTraceUtils::IsHeadshot(FName BoneName)
{
	if (BoneName.IsNone())
	{
		return false;
	}

	const FString BoneString = BoneName.ToString().ToLower();

	// Check for head-related bone names
	return BoneString.Contains(TEXT("head")) ||
	       BoneString.Contains(TEXT("skull")) ||
	       BoneString.Contains(TEXT("neck")) ||
	       BoneString.Contains(TEXT("face"));
}

float USuspenseCoreTraceUtils::GetHitZoneDamageMultiplier(FName BoneName)
{
	if (BoneName.IsNone())
	{
		return 1.0f;
	}

	const FString BoneString = BoneName.ToString().ToLower();

	// Headshot
	if (BoneString.Contains(TEXT("head")) ||
	    BoneString.Contains(TEXT("skull")) ||
	    BoneString.Contains(TEXT("face")))
	{
		return HeadshotDamageMultiplier;
	}

	// Neck (critical but not quite headshot)
	if (BoneString.Contains(TEXT("neck")))
	{
		return 1.5f;
	}

	// Limbs (reduced damage)
	if (BoneString.Contains(TEXT("arm")) ||
	    BoneString.Contains(TEXT("hand")) ||
	    BoneString.Contains(TEXT("leg")) ||
	    BoneString.Contains(TEXT("foot")) ||
	    BoneString.Contains(TEXT("calf")) ||
	    BoneString.Contains(TEXT("thigh")))
	{
		return LimbDamageMultiplier;
	}

	// Torso (normal damage)
	return 1.0f;
}
