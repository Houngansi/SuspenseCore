// SuspenseCoreCameraShakeComponent.cpp
// SuspenseCore - Camera Shake Component Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreCameraShakeComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/CameraShake/SuspenseCoreWeaponCameraShake.h"
#include "SuspenseCore/CameraShake/SuspenseCoreMovementCameraShake.h"
#include "SuspenseCore/CameraShake/SuspenseCoreDamageCameraShake.h"
#include "SuspenseCore/CameraShake/SuspenseCoreExplosionCameraShake.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

USuspenseCoreCameraShakeComponent::USuspenseCoreCameraShakeComponent()
{
	// No tick needed - event-driven
	PrimaryComponentTick.bCanEverTick = false;

	// Set default shake classes
	WeaponShakeClass = USuspenseCoreWeaponCameraShake::StaticClass();
	MovementShakeClass = USuspenseCoreMovementCameraShake::StaticClass();
	DamageShakeClass = USuspenseCoreDamageCameraShake::StaticClass();
	ExplosionShakeClass = USuspenseCoreExplosionCameraShake::StaticClass();
}

void USuspenseCoreCameraShakeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Subscribe to camera shake events
	SubscribeToEvents();

	UE_LOG(LogTemp, Log, TEXT("CameraShakeComponent: Initialized on %s"),
		*GetOwner()->GetName());
}

void USuspenseCoreCameraShakeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unsubscribe from EventBus
	UnsubscribeFromEvents();

	Super::EndPlay(EndPlayReason);
}

//========================================================================
// EventBus Integration
//========================================================================

void USuspenseCoreCameraShakeComponent::SubscribeToEvents()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraShakeComponent: No EventBus found, cannot subscribe"));
		return;
	}

	CachedEventBus = EventBus;

	// Subscribe to weapon shake events
	WeaponShakeHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::Camera::ShakeWeapon,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCameraShakeComponent::OnWeaponShakeEvent)
	);

	// Subscribe to movement shake events
	MovementShakeHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::Camera::ShakeMovement,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCameraShakeComponent::OnMovementShakeEvent)
	);

	// Subscribe to damage shake events
	DamageShakeHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::Camera::ShakeDamage,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCameraShakeComponent::OnDamageShakeEvent)
	);

	// Subscribe to explosion shake events
	ExplosionShakeHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::Camera::ShakeExplosion,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCameraShakeComponent::OnExplosionShakeEvent)
	);

	UE_LOG(LogTemp, Log, TEXT("CameraShakeComponent: Subscribed to EventBus for camera shake events"));
}

void USuspenseCoreCameraShakeComponent::UnsubscribeFromEvents()
{
	if (CachedEventBus.IsValid())
	{
		if (WeaponShakeHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(WeaponShakeHandle);
		}
		if (MovementShakeHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(MovementShakeHandle);
		}
		if (DamageShakeHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(DamageShakeHandle);
		}
		if (ExplosionShakeHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(ExplosionShakeHandle);
		}

		UE_LOG(LogTemp, Log, TEXT("CameraShakeComponent: Unsubscribed from EventBus"));
	}

	CachedEventBus.Reset();
	WeaponShakeHandle.Invalidate();
	MovementShakeHandle.Invalidate();
	DamageShakeHandle.Invalidate();
	ExplosionShakeHandle.Invalidate();
}

void USuspenseCoreCameraShakeComponent::OnWeaponShakeEvent(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	// Only respond to events from our owner
	if (EventData.Source != GetOwner())
	{
		return;
	}

	if (!bEnableCameraShakes)
	{
		return;
	}

	FString WeaponType = EventData.GetString(TEXT("Type"), TEXT("Rifle"));
	float Scale = EventData.GetFloat(TEXT("Scale"), 1.0f);

	PlayWeaponShake(WeaponType, Scale);
}

void USuspenseCoreCameraShakeComponent::OnMovementShakeEvent(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	// Only respond to events from our owner
	if (EventData.Source != GetOwner())
	{
		return;
	}

	if (!bEnableCameraShakes)
	{
		return;
	}

	FString MovementType = EventData.GetString(TEXT("Type"), TEXT("Landing"));
	float Scale = EventData.GetFloat(TEXT("Scale"), 1.0f);

	// For landing, scale can be based on fall height
	float FallHeight = EventData.GetFloat(TEXT("FallHeight"), 0.0f);
	if (FallHeight > 0.0f)
	{
		// Scale based on fall height (300 = soft landing, 600+ = hard landing)
		Scale *= FMath::GetMappedRangeValueClamped(FVector2D(100.0f, 600.0f), FVector2D(0.5f, 1.5f), FallHeight);

		// Switch to hard landing preset if fall is significant
		if (FallHeight > 400.0f)
		{
			MovementType = TEXT("HardLanding");
		}
	}

	PlayMovementShake(MovementType, Scale);
}

void USuspenseCoreCameraShakeComponent::OnDamageShakeEvent(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	// Only respond to events where we are the target
	if (EventData.Target != GetOwner())
	{
		return;
	}

	if (!bEnableCameraShakes)
	{
		return;
	}

	FString DamageType = EventData.GetString(TEXT("Type"), TEXT("Light"));
	float Scale = EventData.GetFloat(TEXT("Scale"), 1.0f);

	// Scale based on damage amount
	float DamageAmount = EventData.GetFloat(TEXT("DamageAmount"), 0.0f);
	if (DamageAmount > 0.0f)
	{
		// Scale: 10 damage = 0.5x, 50 damage = 1.0x, 100+ damage = 1.5x
		Scale *= FMath::GetMappedRangeValueClamped(FVector2D(10.0f, 100.0f), FVector2D(0.5f, 1.5f), DamageAmount);
	}

	PlayDamageShake(DamageType, Scale);
}

void USuspenseCoreCameraShakeComponent::OnExplosionShakeEvent(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	if (!bEnableCameraShakes)
	{
		return;
	}

	// Explosions affect everyone - no source check
	// Distance determines intensity
	float Distance = EventData.GetFloat(TEXT("Distance"), 1000.0f);
	float Scale = EventData.GetFloat(TEXT("Scale"), 1.0f);

	// Check if distance was provided
	if (Distance > 0.0f)
	{
		PlayExplosionShakeByDistance(Distance, Scale);
	}
	else
	{
		// Use type-based preset
		FString ExplosionType = EventData.GetString(TEXT("Type"), TEXT("Medium"));
		PlayExplosionShake(ExplosionType, Scale);
	}
}

USuspenseCoreEventBus* USuspenseCoreCameraShakeComponent::GetEventBus() const
{
	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetOwner()))
	{
		return Manager->GetEventBus();
	}

	return nullptr;
}

//========================================================================
// Public API
//========================================================================

void USuspenseCoreCameraShakeComponent::PlayWeaponShake(const FString& WeaponType, float Scale)
{
	if (!bEnableCameraShakes || !WeaponShakeClass)
	{
		return;
	}

	float FinalScale = Scale * WeaponShakeScale * MasterShakeScale;

	if (FinalScale > KINDA_SMALL_NUMBER)
	{
		StartCameraShake(WeaponShakeClass, FinalScale);

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Playing weapon shake Type=%s, Scale=%.2f"),
			*WeaponType, FinalScale);
	}
}

void USuspenseCoreCameraShakeComponent::PlayMovementShake(const FString& MovementType, float Scale)
{
	if (!bEnableCameraShakes || !MovementShakeClass)
	{
		return;
	}

	float FinalScale = Scale * MovementShakeScale * MasterShakeScale;

	if (FinalScale > KINDA_SMALL_NUMBER)
	{
		StartCameraShake(MovementShakeClass, FinalScale);

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Playing movement shake Type=%s, Scale=%.2f"),
			*MovementType, FinalScale);
	}
}

void USuspenseCoreCameraShakeComponent::PlayDamageShake(const FString& DamageType, float Scale)
{
	if (!bEnableCameraShakes || !DamageShakeClass)
	{
		return;
	}

	float FinalScale = Scale * DamageShakeScale * MasterShakeScale;

	if (FinalScale > KINDA_SMALL_NUMBER)
	{
		StartCameraShake(DamageShakeClass, FinalScale);

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Playing damage shake Type=%s, Scale=%.2f"),
			*DamageType, FinalScale);
	}
}

void USuspenseCoreCameraShakeComponent::PlayExplosionShake(const FString& ExplosionType, float Scale)
{
	if (!bEnableCameraShakes || !ExplosionShakeClass)
	{
		return;
	}

	float FinalScale = Scale * ExplosionShakeScale * MasterShakeScale;

	if (FinalScale > KINDA_SMALL_NUMBER)
	{
		StartCameraShake(ExplosionShakeClass, FinalScale);

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Playing explosion shake Type=%s, Scale=%.2f"),
			*ExplosionType, FinalScale);
	}
}

void USuspenseCoreCameraShakeComponent::PlayExplosionShakeByDistance(float Distance, float Scale)
{
	if (!bEnableCameraShakes || !ExplosionShakeClass)
	{
		return;
	}

	// Skip if too far (>30m = 3000cm)
	if (Distance > 3000.0f)
	{
		return;
	}

	// Distance-based scaling (closer = more intense)
	float DistanceScale = FMath::GetMappedRangeValueClamped(
		FVector2D(0.0f, 3000.0f),
		FVector2D(1.5f, 0.2f),
		Distance
	);

	float FinalScale = Scale * DistanceScale * ExplosionShakeScale * MasterShakeScale;

	if (FinalScale > KINDA_SMALL_NUMBER)
	{
		StartCameraShake(ExplosionShakeClass, FinalScale);

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Playing explosion shake Distance=%.0fcm, Scale=%.2f"),
			Distance, FinalScale);
	}
}

void USuspenseCoreCameraShakeComponent::StopAllShakes(bool bImmediately)
{
	APlayerController* PC = GetOwnerPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StopAllCameraShakes(bImmediately);

		UE_LOG(LogTemp, Log, TEXT("CameraShakeComponent: Stopped all shakes (immediate=%s)"),
			bImmediately ? TEXT("true") : TEXT("false"));
	}
}

//========================================================================
// Internal Methods
//========================================================================

APlayerController* USuspenseCoreCameraShakeComponent::GetOwnerPlayerController() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Try to get controller from Pawn
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}

	// Try to get controller from PlayerController directly
	if (APlayerController* PC = Cast<APlayerController>(Owner))
	{
		return PC;
	}

	return nullptr;
}

void USuspenseCoreCameraShakeComponent::StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	APlayerController* PC = GetOwnerPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraShakeComponent: No PlayerController found"));
		return;
	}

	if (!ShakeClass)
	{
		return;
	}

	PC->ClientStartCameraShake(ShakeClass, Scale);
}

