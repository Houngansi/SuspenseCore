// SuspenseCoreCameraShakeComponent.cpp
// SuspenseCore - Camera Shake Component Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreCameraShakeComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Settings/SuspenseCoreSettings.h"
#include "SuspenseCore/CameraShake/SuspenseCoreWeaponCameraShake.h"
#include "SuspenseCore/CameraShake/SuspenseCoreMovementCameraShake.h"
#include "SuspenseCore/CameraShake/SuspenseCoreDamageCameraShake.h"
#include "SuspenseCore/CameraShake/SuspenseCoreExplosionCameraShake.h"
#include "SuspenseCore/CameraShake/SuspenseCoreCameraShakeManager.h"
#include "SuspenseCore/CameraShake/SuspenseCoreCameraShakeDataAsset.h"
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

	// Apply SSOT settings first (before other initialization)
	ApplySSOTSettings();

	// Subscribe to camera shake events
	SubscribeToEvents();

	// Initialize layered shake manager if enabled
	if (bUseLayeredShakeManager)
	{
		InitializeShakeManager();
	}

	// Bind directly to Character's LandedDelegate for reliable landing detection
	if (bBindToLandedDelegate)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->LandedDelegate.AddDynamic(this, &USuspenseCoreCameraShakeComponent::OnCharacterLanded);
			UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Bound to LandedDelegate"));
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Initialized on %s"),
		*GetOwner()->GetName());
}

void USuspenseCoreCameraShakeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind from LandedDelegate
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LandedDelegate.RemoveDynamic(this, &USuspenseCoreCameraShakeComponent::OnCharacterLanded);
	}

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
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: No EventBus found, cannot subscribe"));
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
	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Received ShakeMovement event from %s (Owner: %s)"),
		EventData.Source ? *EventData.Source->GetName() : TEXT("NULL"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"));

	// Only respond to events from our owner
	if (EventData.Source != GetOwner())
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Source mismatch, ignoring event"));
		return;
	}

	if (!bEnableCameraShakes)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Camera shakes disabled"));
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
	// For damage events, check if we are the target (via ObjectPayload "Target")
	// or if Source is our owner (damage applied to self)
	UObject* DamageTarget = EventData.GetObject<UObject>(TEXT("Target"));
	if (DamageTarget && DamageTarget != GetOwner())
	{
		return;
	}
	// Fallback: if no Target specified, check Source
	if (!DamageTarget && EventData.Source != GetOwner())
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
		if (bUseLayeredShakeManager && ShakeManager)
		{
			FName Category = FName(*FString::Printf(TEXT("Weapon.%s"), *WeaponType));
			StartLayeredCameraShake(WeaponShakeClass, FinalScale, ESuspenseCoreShakePriority::Weapon, Category);
		}
		else
		{
			StartCameraShake(WeaponShakeClass, FinalScale);
		}

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Playing weapon shake Type=%s, Scale=%.2f"),
			*WeaponType, FinalScale);
	}
}

void USuspenseCoreCameraShakeComponent::PlayMovementShake(const FString& MovementType, float Scale)
{
	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: PlayMovementShake called - Type=%s, Scale=%.2f, ShakeClass=%s"),
		*MovementType, Scale, MovementShakeClass ? *MovementShakeClass->GetName() : TEXT("NULL"));

	if (!bEnableCameraShakes || !MovementShakeClass)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: PlayMovementShake aborted - EnableShakes=%d, Class=%s"),
			bEnableCameraShakes, MovementShakeClass ? TEXT("Valid") : TEXT("NULL"));
		return;
	}

	float FinalScale = Scale * MovementShakeScale * MasterShakeScale;

	if (FinalScale > KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Starting movement shake FinalScale=%.2f (Base=%.2f * Movement=%.2f * Master=%.2f)"),
			FinalScale, Scale, MovementShakeScale, MasterShakeScale);

		if (bUseLayeredShakeManager && ShakeManager)
		{
			FName Category = FName(*FString::Printf(TEXT("Movement.%s"), *MovementType));
			StartLayeredCameraShake(MovementShakeClass, FinalScale, ESuspenseCoreShakePriority::Movement, Category);
		}
		else
		{
			StartCameraShake(MovementShakeClass, FinalScale);
		}
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
		if (bUseLayeredShakeManager && ShakeManager)
		{
			FName Category = FName(*FString::Printf(TEXT("Damage.%s"), *DamageType));
			StartLayeredCameraShake(DamageShakeClass, FinalScale, ESuspenseCoreShakePriority::Combat, Category);
		}
		else
		{
			StartCameraShake(DamageShakeClass, FinalScale);
		}

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
		if (bUseLayeredShakeManager && ShakeManager)
		{
			FName Category = FName(*FString::Printf(TEXT("Explosion.%s"), *ExplosionType));
			StartLayeredCameraShake(ExplosionShakeClass, FinalScale, ESuspenseCoreShakePriority::Environmental, Category);
		}
		else
		{
			StartCameraShake(ExplosionShakeClass, FinalScale);
		}

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
		if (bUseLayeredShakeManager && ShakeManager)
		{
			// Categorize by distance
			FString DistanceCategory = Distance < 500.0f ? TEXT("Nearby") : (Distance < 1500.0f ? TEXT("Medium") : TEXT("Distant"));
			FName Category = FName(*FString::Printf(TEXT("Explosion.%s"), *DistanceCategory));
			StartLayeredCameraShake(ExplosionShakeClass, FinalScale, ESuspenseCoreShakePriority::Environmental, Category);
		}
		else
		{
			StartCameraShake(ExplosionShakeClass, FinalScale);
		}

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Playing explosion shake Distance=%.0fcm, Scale=%.2f"),
			Distance, FinalScale);
	}
}

void USuspenseCoreCameraShakeComponent::StopAllShakes(bool bImmediately)
{
	if (bUseLayeredShakeManager && ShakeManager)
	{
		ShakeManager->StopAllShakes(bImmediately);
	}
	else
	{
		APlayerController* PC = GetOwnerPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StopAllCameraShakes(bImmediately);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Stopped all shakes (immediate=%s)"),
		bImmediately ? TEXT("true") : TEXT("false"));
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
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: No PlayerController found for %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"));
		return;
	}

	if (!ShakeClass)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: ShakeClass is NULL"));
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: >>> ClientStartCameraShake(%s, Scale=%.2f) via PC=%s"),
		*ShakeClass->GetName(), Scale, *PC->GetName());

	PC->ClientStartCameraShake(ShakeClass, Scale);
}

void USuspenseCoreCameraShakeComponent::StartLayeredCameraShake(
	TSubclassOf<UCameraShakeBase> ShakeClass,
	float Scale,
	ESuspenseCoreShakePriority Priority,
	FName Category)
{
	if (!ShakeClass)
	{
		return;
	}

	// Use layered manager if available
	if (ShakeManager)
	{
		FSuspenseCoreShakeConfig Config;
		Config.ShakeClass = ShakeClass;
		Config.Scale = Scale;
		Config.Priority = Priority;
		Config.Category = Category;
		Config.BlendMode = FSuspenseCoreShakeLayerUtils::GetRecommendedBlendMode(Category);
		Config.BlendWeight = 1.0f;

		ShakeManager->PlayShake(Config);
	}
	else
	{
		// Fallback to simple shake
		StartCameraShake(ShakeClass, Scale);
	}
}

void USuspenseCoreCameraShakeComponent::InitializeShakeManager()
{
	APlayerController* PC = GetOwnerPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraShakeComponent: Cannot init ShakeManager - no PlayerController"));
		return;
	}

	// Create manager
	ShakeManager = NewObject<USuspenseCoreCameraShakeManager>(this);
	if (ShakeManager)
	{
		ShakeManager->Initialize(PC);
		ShakeManager->GlobalShakeScale = MasterShakeScale;
		ShakeManager->bEnablePriorityBlending = bEnablePriorityBlending;

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Layered ShakeManager initialized"));
	}
}

void USuspenseCoreCameraShakeComponent::ApplySSOTSettings()
{
	// Skip if component should use its own settings
	if (bOverrideSSOTSettings)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Using override settings (SSOT bypass)"));
		return;
	}

	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: No SSOT settings found, using defaults"));
		return;
	}

	// Apply global settings from SSOT
	bUseLayeredShakeManager = Settings->bUseLayeredCameraShakes;

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Applied SSOT settings - LayeredShakes=%s, PerlinNoise=%s"),
		bUseLayeredShakeManager ? TEXT("true") : TEXT("false"),
		Settings->bUsePerlinNoiseShakes ? TEXT("true") : TEXT("false"));

	// Load DataAsset if configured
	LoadDataAssetFromSettings();

	// Apply DataAsset settings if loaded
	if (CachedShakeDataAsset)
	{
		// Apply global scale from DataAsset
		MasterShakeScale = CachedShakeDataAsset->MasterScale;
		bEnablePriorityBlending = CachedShakeDataAsset->bEnablePriorityBlending;

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: Applied DataAsset settings - MasterScale=%.2f, PriorityBlending=%s"),
			MasterShakeScale, bEnablePriorityBlending ? TEXT("true") : TEXT("false"));
	}
}

void USuspenseCoreCameraShakeComponent::LoadDataAssetFromSettings()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return;
	}

	// Load DataAsset from SSOT
	if (Settings->CameraShakePresetsAsset.IsValid())
	{
		UDataAsset* LoadedAsset = Settings->CameraShakePresetsAsset.LoadSynchronous();
		CachedShakeDataAsset = Cast<USuspenseCoreCameraShakeDataAsset>(LoadedAsset);

		if (CachedShakeDataAsset)
		{
			UE_LOG(LogTemp, Log, TEXT("CameraShakeComponent: Loaded camera shake DataAsset from SSOT: %s"),
				*CachedShakeDataAsset->GetName());
		}
		else if (LoadedAsset)
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraShakeComponent: SSOT DataAsset is not of type USuspenseCoreCameraShakeDataAsset: %s"),
				*LoadedAsset->GetClass()->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: No camera shake DataAsset configured in SSOT"));
	}
}

void USuspenseCoreCameraShakeComponent::OnCharacterLanded(const FHitResult& Hit)
{
	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeComponent: OnCharacterLanded triggered!"));

	// Play landing shake directly - independent of ability system
	PlayMovementShake(TEXT("Landing"), 1.0f);
}

