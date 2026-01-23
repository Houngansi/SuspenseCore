// SuspenseCoreThrowableAssetPreloader.cpp
// Async preloader for throwable assets to eliminate microfreeze on first grenade use
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreThrowableAssetPreloader.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
#include "Engine/GameInstance.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraShakeBase.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogThrowablePreloader, Log, All);

#define PRELOADER_LOG(Verbosity, Format, ...) \
	UE_LOG(LogThrowablePreloader, Verbosity, TEXT("[ThrowablePreloader] " Format), ##__VA_ARGS__)

//==================================================================
// Static Access
//==================================================================

USuspenseCoreThrowableAssetPreloader* USuspenseCoreThrowableAssetPreloader::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreThrowableAssetPreloader>();
}

//==================================================================
// Subsystem Lifecycle
//==================================================================

void USuspenseCoreThrowableAssetPreloader::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PRELOADER_LOG(Log, TEXT("Initializing..."));

	// Get DataManager dependency
	Collection.InitializeDependency<USuspenseCoreDataManager>();

	const UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		PRELOADER_LOG(Error, TEXT("No GameInstance - cannot initialize"));
		return;
	}

	DataManager = GI->GetSubsystem<USuspenseCoreDataManager>();

	// Get EventBus via EventManager
	if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
	{
		EventBus = EventManager->GetEventBus();
	}

	// If DataManager is already initialized, start preload immediately
	if (DataManager.IsValid() && DataManager->IsInitialized())
	{
		PRELOADER_LOG(Log, TEXT("DataManager already ready - starting preload immediately"));
		StartPreload();
	}
	else if (EventBus.IsValid())
	{
		// Subscribe to DataManager ready event
		// NOTE: DataManager uses dynamic tag "SuspenseCore.Event.Data.Initialized" (not a native tag from SuspenseCoreTags)
		PRELOADER_LOG(Log, TEXT("Subscribing to DataManager initialized event"));

		static const FGameplayTag DataInitializedTag =
			FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.Initialized"));

		EventBus->SubscribeNative(
			DataInitializedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreThrowableAssetPreloader::OnDataManagerReady),
			ESuspenseCoreEventPriority::Normal);
	}
	else
	{
		PRELOADER_LOG(Warning, TEXT("No EventBus - will try manual preload later"));
	}

	PRELOADER_LOG(Log, TEXT("Initialized"));
}

void USuspenseCoreThrowableAssetPreloader::Deinitialize()
{
	PRELOADER_LOG(Log, TEXT("Deinitializing..."));

	// Cancel any pending loads
	for (auto& Handle : ActiveLoadHandles)
	{
		if (Handle.IsValid() && Handle->IsLoadingInProgress())
		{
			Handle->CancelHandle();
		}
	}
	ActiveLoadHandles.Empty();

	// Clear caches
	PreloadedAssets.Empty();

	DataManager.Reset();
	EventBus.Reset();

	Super::Deinitialize();
}

//==================================================================
// Public API - Asset Access
//==================================================================

TSubclassOf<AActor> USuspenseCoreThrowableAssetPreloader::GetPreloadedActorClass(FName ThrowableID) const
{
	if (const FSuspenseCoreThrowableAssetCache* Cache = PreloadedAssets.Find(ThrowableID))
	{
		return Cache->ActorClass;
	}

	// Fallback: Try to get from DataManager and load synchronously (with warning)
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(ThrowableID, ItemData))
		{
			if (!ItemData.EquipmentActorClass.IsNull())
			{
				PRELOADER_LOG(Warning, TEXT("GetPreloadedActorClass: '%s' not preloaded - fallback to sync load"),
					*ThrowableID.ToString());
				return ItemData.EquipmentActorClass.LoadSynchronous();
			}
		}
	}

	return nullptr;
}

bool USuspenseCoreThrowableAssetPreloader::GetPreloadedAssets(FName ThrowableID, FSuspenseCoreThrowableAssetCache& OutCache) const
{
	if (const FSuspenseCoreThrowableAssetCache* Cache = PreloadedAssets.Find(ThrowableID))
	{
		OutCache = *Cache;
		return Cache->IsLoaded();
	}

	return false;
}

bool USuspenseCoreThrowableAssetPreloader::AreAssetsPreloaded(FName ThrowableID) const
{
	if (const FSuspenseCoreThrowableAssetCache* Cache = PreloadedAssets.Find(ThrowableID))
	{
		return Cache->IsLoaded();
	}
	return false;
}

//==================================================================
// Public API - Manual Control
//==================================================================

void USuspenseCoreThrowableAssetPreloader::StartPreload()
{
	if (bPreloadStarted)
	{
		PRELOADER_LOG(Verbose, TEXT("Preload already started"));
		return;
	}

	bPreloadStarted = true;
	PRELOADER_LOG(Log, TEXT("Starting asset preload..."));

	LoadAllThrowableAssets();
}

void USuspenseCoreThrowableAssetPreloader::PreloadThrowable(FName ThrowableID)
{
	if (ThrowableID.IsNone())
	{
		return;
	}

	if (PreloadedAssets.Contains(ThrowableID))
	{
		PRELOADER_LOG(Verbose, TEXT("Throwable '%s' already preloaded"), *ThrowableID.ToString());
		return;
	}

	if (!DataManager.IsValid())
	{
		PRELOADER_LOG(Warning, TEXT("Cannot preload '%s' - DataManager not available"), *ThrowableID.ToString());
		return;
	}

	FSuspenseCoreUnifiedItemData ItemData;
	if (DataManager->GetUnifiedItemData(ThrowableID, ItemData))
	{
		LoadThrowableAssets(ThrowableID, ItemData);
	}
}

//==================================================================
// Internal Methods
//==================================================================

void USuspenseCoreThrowableAssetPreloader::LoadAllThrowableAssets()
{
	if (!DataManager.IsValid())
	{
		PRELOADER_LOG(Warning, TEXT("DataManager not available - cannot preload"));
		return;
	}

	// Get all throwable item IDs from the unified item cache
	TArray<FName> AllItemIDs = DataManager->GetAllItemIDs();

	int32 ThrowableCount = 0;

	for (const FName& ItemID : AllItemIDs)
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(ItemID, ItemData))
		{
			// Check if this is a throwable item
			if (ItemData.bIsThrowable)
			{
				LoadThrowableAssets(ItemID, ItemData);
				ThrowableCount++;
			}
		}
	}

	PRELOADER_LOG(Log, TEXT("Queued %d throwables for async preload"), ThrowableCount);

	// If no throwables found, mark as complete
	if (ThrowableCount == 0 || PendingLoadCount == 0)
	{
		OnAllAssetsLoaded();
	}
}

void USuspenseCoreThrowableAssetPreloader::LoadThrowableAssets(FName ThrowableID, const FSuspenseCoreUnifiedItemData& ItemData)
{
	PRELOADER_LOG(Log, TEXT("Loading assets for throwable: %s"), *ThrowableID.ToString());

	// Create cache entry
	FSuspenseCoreThrowableAssetCache Cache;
	Cache.ThrowableID = ThrowableID;

	// Collect all soft references to load
	TArray<FSoftObjectPath> AssetsToLoad;

	// 1. Actor Class (most important - this is what causes the main freeze)
	if (!ItemData.EquipmentActorClass.IsNull())
	{
		AssetsToLoad.Add(ItemData.EquipmentActorClass.ToSoftObjectPath());
	}

	// 2. Get throwable attributes for VFX/Audio
	FSuspenseCoreThrowableAttributeRow ThrowableAttrs;
	FName AttrKey = ItemData.GetThrowableAttributesKey();

	if (!AttrKey.IsNone() && DataManager->GetThrowableAttributes(AttrKey, ThrowableAttrs))
	{
		// VFX
		if (!ThrowableAttrs.ExplosionEffect.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.ExplosionEffect.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.ExplosionEffectLegacy.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.ExplosionEffectLegacy.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.SmokeEffect.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.SmokeEffect.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.SmokeEffectLegacy.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.SmokeEffectLegacy.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.TrailEffect.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.TrailEffect.ToSoftObjectPath());
		}

		// Audio
		if (!ThrowableAttrs.ExplosionSound.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.ExplosionSound.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.PinPullSound.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.PinPullSound.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.BounceSound.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.BounceSound.ToSoftObjectPath());
		}

		// Camera Shake
		if (!ThrowableAttrs.ExplosionCameraShake.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.ExplosionCameraShake.ToSoftObjectPath());
		}

		// Damage Effects
		if (!ThrowableAttrs.DamageEffectClass.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.DamageEffectClass.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.FlashbangEffectClass.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.FlashbangEffectClass.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.IncendiaryEffectClass.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.IncendiaryEffectClass.ToSoftObjectPath());
		}

		// DoT Effects (Bleeding)
		if (!ThrowableAttrs.BleedingLightEffectClass.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.BleedingLightEffectClass.ToSoftObjectPath());
		}
		if (!ThrowableAttrs.BleedingHeavyEffectClass.IsNull())
		{
			AssetsToLoad.Add(ThrowableAttrs.BleedingHeavyEffectClass.ToSoftObjectPath());
		}
	}

	if (AssetsToLoad.Num() == 0)
	{
		PRELOADER_LOG(Warning, TEXT("No assets to load for throwable: %s"), *ThrowableID.ToString());
		return;
	}

	PRELOADER_LOG(Verbose, TEXT("  Queuing %d assets for async load"), AssetsToLoad.Num());

	// Increment pending count
	PendingLoadCount++;

	// Create async load request
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateLambda([this, ThrowableID, ItemData, ThrowableAttrs, AttrKey]()
		{
			// Assets loaded - populate cache
			FSuspenseCoreThrowableAssetCache& CacheEntry = PreloadedAssets.FindOrAdd(ThrowableID);
			CacheEntry.ThrowableID = ThrowableID;

			// Get loaded actor class
			if (!ItemData.EquipmentActorClass.IsNull())
			{
				CacheEntry.ActorClass = ItemData.EquipmentActorClass.Get();
			}

			// Get loaded VFX/Audio (only if we had attributes)
			if (!AttrKey.IsNone())
			{
				// VFX
				if (!ThrowableAttrs.ExplosionEffect.IsNull())
				{
					CacheEntry.ExplosionEffect = ThrowableAttrs.ExplosionEffect.Get();
				}
				if (!ThrowableAttrs.ExplosionEffectLegacy.IsNull())
				{
					CacheEntry.ExplosionEffectLegacy = ThrowableAttrs.ExplosionEffectLegacy.Get();
				}
				if (!ThrowableAttrs.SmokeEffect.IsNull())
				{
					CacheEntry.SmokeEffect = ThrowableAttrs.SmokeEffect.Get();
				}
				if (!ThrowableAttrs.SmokeEffectLegacy.IsNull())
				{
					CacheEntry.SmokeEffectLegacy = ThrowableAttrs.SmokeEffectLegacy.Get();
				}
				if (!ThrowableAttrs.TrailEffect.IsNull())
				{
					CacheEntry.TrailEffect = ThrowableAttrs.TrailEffect.Get();
				}

				// Audio
				if (!ThrowableAttrs.ExplosionSound.IsNull())
				{
					CacheEntry.ExplosionSound = ThrowableAttrs.ExplosionSound.Get();
				}
				if (!ThrowableAttrs.PinPullSound.IsNull())
				{
					CacheEntry.PinPullSound = ThrowableAttrs.PinPullSound.Get();
				}
				if (!ThrowableAttrs.BounceSound.IsNull())
				{
					CacheEntry.BounceSound = ThrowableAttrs.BounceSound.Get();
				}

				// Camera Shake
				if (!ThrowableAttrs.ExplosionCameraShake.IsNull())
				{
					CacheEntry.ExplosionCameraShake = ThrowableAttrs.ExplosionCameraShake.Get();
				}

				// Damage Effects
				if (!ThrowableAttrs.DamageEffectClass.IsNull())
				{
					CacheEntry.DamageEffectClass = ThrowableAttrs.DamageEffectClass.Get();
				}
				if (!ThrowableAttrs.FlashbangEffectClass.IsNull())
				{
					CacheEntry.FlashbangEffectClass = ThrowableAttrs.FlashbangEffectClass.Get();
				}
				if (!ThrowableAttrs.IncendiaryEffectClass.IsNull())
				{
					CacheEntry.IncendiaryEffectClass = ThrowableAttrs.IncendiaryEffectClass.Get();
				}

				// DoT Effects (Bleeding)
				if (!ThrowableAttrs.BleedingLightEffectClass.IsNull())
				{
					CacheEntry.BleedingLightEffectClass = ThrowableAttrs.BleedingLightEffectClass.Get();
				}
				if (!ThrowableAttrs.BleedingHeavyEffectClass.IsNull())
				{
					CacheEntry.BleedingHeavyEffectClass = ThrowableAttrs.BleedingHeavyEffectClass.Get();
				}
			}

			OnThrowableAssetsLoaded(ThrowableID);
		}),
		FStreamableManager::AsyncLoadHighPriority
	);

	if (Handle.IsValid())
	{
		ActiveLoadHandles.Add(Handle);
	}
	else
	{
		PRELOADER_LOG(Warning, TEXT("Failed to create async load handle for: %s"), *ThrowableID.ToString());
		PendingLoadCount--;
	}
}

void USuspenseCoreThrowableAssetPreloader::OnThrowableAssetsLoaded(FName ThrowableID)
{
	PRELOADER_LOG(Log, TEXT("Assets loaded for: %s"), *ThrowableID.ToString());

	PendingLoadCount--;

	if (PendingLoadCount <= 0)
	{
		OnAllAssetsLoaded();
	}
}

void USuspenseCoreThrowableAssetPreloader::OnAllAssetsLoaded()
{
	bPreloadComplete = true;

	PRELOADER_LOG(Log, TEXT("═══════════════════════════════════════════════════════════════"));
	PRELOADER_LOG(Log, TEXT("PRELOAD COMPLETE: %d throwables loaded"), PreloadedAssets.Num());
	PRELOADER_LOG(Log, TEXT("═══════════════════════════════════════════════════════════════"));

	// Clean up handles
	ActiveLoadHandles.Empty();

	// Publish event
	PublishPreloadCompleteEvent();
}

void USuspenseCoreThrowableAssetPreloader::OnDataManagerReady(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	PRELOADER_LOG(Log, TEXT("DataManager ready - starting preload"));
	StartPreload();
}

void USuspenseCoreThrowableAssetPreloader::PublishPreloadCompleteEvent()
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.SetInt(TEXT("PreloadedCount"), PreloadedAssets.Num());

	// Use Throwable.AssetsLoaded tag
	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Throwable.AssetsLoaded"), false);
	if (EventTag.IsValid())
	{
		EventBus->Publish(EventTag, EventData);
		PRELOADER_LOG(Log, TEXT("Published Throwable.AssetsLoaded event"));
	}
}
