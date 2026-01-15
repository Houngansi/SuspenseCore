// SuspenseCoreCameraShakeManager.cpp
// SuspenseCore - Layered Camera Shake Manager Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/CameraShake/SuspenseCoreCameraShakeManager.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "Engine/World.h"

USuspenseCoreCameraShakeManager::USuspenseCoreCameraShakeManager()
	: GlobalShakeScale(1.0f)
	, bEnablePriorityBlending(true)
	, CleanupInterval(0.5f)
	, LastCleanupTime(0.0f)
{
}

void USuspenseCoreCameraShakeManager::Initialize(APlayerController* InPlayerController)
{
	if (!InPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraShakeManager: Cannot initialize with null PlayerController"));
		return;
	}

	PlayerController = InPlayerController;

	// Setup cleanup timer
	if (CleanupInterval > 0.0f && InPlayerController->GetWorld())
	{
		InPlayerController->GetWorld()->GetTimerManager().SetTimer(
			CleanupTimerHandle,
			FTimerDelegate::CreateUObject(this, &USuspenseCoreCameraShakeManager::Update),
			CleanupInterval,
			true
		);
	}

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: Initialized for %s"), *InPlayerController->GetName());
}

// =========================================================================
// Shake Playback
// =========================================================================

UCameraShakeBase* USuspenseCoreCameraShakeManager::PlayShake(const FSuspenseCoreShakeConfig& Config)
{
	if (!Config.ShakeClass)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: PlayShake called with null ShakeClass"));
		return nullptr;
	}

	APlayerCameraManager* CameraManager = GetCameraManager();
	if (!CameraManager)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: No CameraManager available"));
		return nullptr;
	}

	// Apply blend mode logic (may stop existing shakes)
	ApplyBlendMode(Config);

	// Check concurrent limit
	if (Config.MaxConcurrent > 0)
	{
		const int32 CurrentCount = GetActiveShakeCountByCategory(Config.Category);
		if (CurrentCount >= Config.MaxConcurrent)
		{
			UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: Max concurrent reached for category %s (%d/%d)"),
				*Config.Category.ToString(), CurrentCount, Config.MaxConcurrent);
			return nullptr;
		}
	}

	// Calculate final scale with priority blending
	float FinalScale = Config.Scale * GlobalShakeScale;

	if (bEnablePriorityBlending)
	{
		const ESuspenseCoreShakePriority HighestPriority = GetHighestActivePriority();
		FinalScale *= FSuspenseCoreShakeLayerUtils::CalculateBlendWeight(
			Config.Priority,
			HighestPriority,
			Config.BlendWeight
		);
	}

	// Start the shake
	UCameraShakeBase* ShakeInstance = CameraManager->StartCameraShake(Config.ShakeClass, FinalScale);

	if (ShakeInstance)
	{
		// Create and register layer
		FSuspenseCoreShakeLayer NewLayer;
		NewLayer.ShakeInstance = ShakeInstance;
		NewLayer.ShakeClass = Config.ShakeClass;
		NewLayer.Priority = Config.Priority;
		NewLayer.BlendMode = Config.BlendMode;
		NewLayer.BlendWeight = Config.BlendWeight;
		NewLayer.Category = Config.Category;
		NewLayer.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		ActiveLayers.Add(NewLayer);

		UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: Started shake %s (Priority=%d, Category=%s, Scale=%.2f)"),
			*Config.ShakeClass->GetName(),
			static_cast<int32>(Config.Priority),
			*Config.Category.ToString(),
			FinalScale);
	}

	return ShakeInstance;
}

UCameraShakeBase* USuspenseCoreCameraShakeManager::PlayShakeSimple(
	TSubclassOf<UCameraShakeBase> ShakeClass,
	float Scale,
	ESuspenseCoreShakePriority Priority,
	FName Category)
{
	FSuspenseCoreShakeConfig Config;
	Config.ShakeClass = ShakeClass;
	Config.Scale = Scale;
	Config.Priority = Priority;
	Config.Category = Category;
	Config.BlendMode = FSuspenseCoreShakeLayerUtils::GetRecommendedBlendMode(Category);

	return PlayShake(Config);
}

// =========================================================================
// Shake Control
// =========================================================================

void USuspenseCoreCameraShakeManager::StopShakesByCategory(FName Category, bool bImmediately)
{
	APlayerCameraManager* CameraManager = GetCameraManager();
	if (!CameraManager)
	{
		return;
	}

	for (int32 i = ActiveLayers.Num() - 1; i >= 0; --i)
	{
		if (ActiveLayers[i].Category == Category)
		{
			if (ActiveLayers[i].ShakeInstance.IsValid())
			{
				CameraManager->StopCameraShake(ActiveLayers[i].ShakeInstance.Get(), bImmediately);
			}
			ActiveLayers.RemoveAt(i);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: Stopped shakes in category %s"), *Category.ToString());
}

void USuspenseCoreCameraShakeManager::StopShakesByPriority(ESuspenseCoreShakePriority Priority, bool bImmediately)
{
	APlayerCameraManager* CameraManager = GetCameraManager();
	if (!CameraManager)
	{
		return;
	}

	const int32 MaxPriorityValue = FSuspenseCoreShakeLayerUtils::GetPriorityValue(Priority);

	for (int32 i = ActiveLayers.Num() - 1; i >= 0; --i)
	{
		const int32 LayerPriority = FSuspenseCoreShakeLayerUtils::GetPriorityValue(ActiveLayers[i].Priority);
		if (LayerPriority <= MaxPriorityValue)
		{
			if (ActiveLayers[i].ShakeInstance.IsValid())
			{
				CameraManager->StopCameraShake(ActiveLayers[i].ShakeInstance.Get(), bImmediately);
			}
			ActiveLayers.RemoveAt(i);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: Stopped shakes at priority <= %d"), MaxPriorityValue);
}

void USuspenseCoreCameraShakeManager::StopAllShakes(bool bImmediately)
{
	APlayerCameraManager* CameraManager = GetCameraManager();
	if (CameraManager)
	{
		CameraManager->StopAllCameraShakes(bImmediately);
	}

	ActiveLayers.Empty();

	UE_LOG(LogTemp, Verbose, TEXT("CameraShakeManager: Stopped all shakes"));
}

void USuspenseCoreCameraShakeManager::StopShakeClass(TSubclassOf<UCameraShakeBase> ShakeClass, bool bImmediately)
{
	if (!ShakeClass)
	{
		return;
	}

	APlayerCameraManager* CameraManager = GetCameraManager();
	if (!CameraManager)
	{
		return;
	}

	for (int32 i = ActiveLayers.Num() - 1; i >= 0; --i)
	{
		if (ActiveLayers[i].ShakeClass == ShakeClass)
		{
			if (ActiveLayers[i].ShakeInstance.IsValid())
			{
				CameraManager->StopCameraShake(ActiveLayers[i].ShakeInstance.Get(), bImmediately);
			}
			ActiveLayers.RemoveAt(i);
		}
	}
}

// =========================================================================
// Query
// =========================================================================

int32 USuspenseCoreCameraShakeManager::GetActiveShakeCount() const
{
	int32 Count = 0;
	for (const FSuspenseCoreShakeLayer& Layer : ActiveLayers)
	{
		if (Layer.IsActive())
		{
			++Count;
		}
	}
	return Count;
}

int32 USuspenseCoreCameraShakeManager::GetActiveShakeCountByCategory(FName Category) const
{
	int32 Count = 0;
	for (const FSuspenseCoreShakeLayer& Layer : ActiveLayers)
	{
		if (Layer.Category == Category && Layer.IsActive())
		{
			++Count;
		}
	}
	return Count;
}

bool USuspenseCoreCameraShakeManager::HasActiveShakeAtPriority(ESuspenseCoreShakePriority Priority) const
{
	const int32 TargetPriority = FSuspenseCoreShakeLayerUtils::GetPriorityValue(Priority);

	for (const FSuspenseCoreShakeLayer& Layer : ActiveLayers)
	{
		if (Layer.IsActive())
		{
			const int32 LayerPriority = FSuspenseCoreShakeLayerUtils::GetPriorityValue(Layer.Priority);
			if (LayerPriority >= TargetPriority)
			{
				return true;
			}
		}
	}
	return false;
}

ESuspenseCoreShakePriority USuspenseCoreCameraShakeManager::GetHighestActivePriority() const
{
	ESuspenseCoreShakePriority Highest = ESuspenseCoreShakePriority::Ambient;

	for (const FSuspenseCoreShakeLayer& Layer : ActiveLayers)
	{
		if (Layer.IsActive())
		{
			if (FSuspenseCoreShakeLayerUtils::GetPriorityValue(Layer.Priority) >
				FSuspenseCoreShakeLayerUtils::GetPriorityValue(Highest))
			{
				Highest = Layer.Priority;
			}
		}
	}

	return Highest;
}

bool USuspenseCoreCameraShakeManager::IsShakeClassPlaying(TSubclassOf<UCameraShakeBase> ShakeClass) const
{
	for (const FSuspenseCoreShakeLayer& Layer : ActiveLayers)
	{
		if (Layer.ShakeClass == ShakeClass && Layer.IsActive())
		{
			return true;
		}
	}
	return false;
}

// =========================================================================
// Internal
// =========================================================================

void USuspenseCoreCameraShakeManager::Update()
{
	CleanupFinishedLayers();
}

void USuspenseCoreCameraShakeManager::CleanupFinishedLayers()
{
	for (int32 i = ActiveLayers.Num() - 1; i >= 0; --i)
	{
		if (!ActiveLayers[i].IsActive())
		{
			ActiveLayers.RemoveAt(i);
		}
	}
}

void USuspenseCoreCameraShakeManager::ApplyBlendMode(const FSuspenseCoreShakeConfig& Config)
{
	switch (Config.BlendMode)
	{
	case ESuspenseCoreShakeBlendMode::Replace:
		// Stop same category or lower priority
		if (Config.Category != NAME_None)
		{
			StopShakesByCategory(Config.Category, false);
		}
		break;

	case ESuspenseCoreShakeBlendMode::Override:
		// Stop all shakes
		StopAllShakes(false);
		break;

	case ESuspenseCoreShakeBlendMode::Weighted:
	case ESuspenseCoreShakeBlendMode::Additive:
		// Just stop same category if configured
		if (Config.bStopSameCategory && Config.Category != NAME_None)
		{
			StopShakesByCategory(Config.Category, false);
		}
		break;
	}
}

APlayerCameraManager* USuspenseCoreCameraShakeManager::GetCameraManager() const
{
	if (PlayerController.IsValid())
	{
		return PlayerController->PlayerCameraManager;
	}
	return nullptr;
}
