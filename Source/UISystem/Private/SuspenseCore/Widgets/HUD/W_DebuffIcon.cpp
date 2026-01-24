// W_DebuffIcon.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.
//
// ARCHITECTURE:
// - Individual debuff icon widget for HUD display
// - Managed by W_DebuffContainer with object pooling
// - Async icon loading for performance
// - Animation support via Blueprint

#include "SuspenseCore/Widgets/HUD/W_DebuffIcon.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/Texture2D.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDebuffIcon, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════════

UW_DebuffIcon::UW_DebuffIcon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffIcon::NativeConstruct()
{
	Super::NativeConstruct();

	// Start hidden until SetDebuffData is called
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_DebuffIcon::NativeDestruct()
{
	// Cancel any pending async loads
	if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
	{
		IconLoadHandle->CancelHandle();
	}
	IconLoadHandle.Reset();

	Super::NativeDestruct();
}

void UW_DebuffIcon::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Skip tick if not active
	if (!bIsActive || bIsRemoving)
	{
		return;
	}

	// Update duration for timed effects
	if (!IsInfinite() && RemainingDuration > 0.0f)
	{
		RemainingDuration -= InDeltaTime;

		// Clamp to 0
		if (RemainingDuration < 0.0f)
		{
			RemainingDuration = 0.0f;
		}

		// Update display
		UpdateTimer(RemainingDuration);

		// Check for critical state transition
		UpdateCriticalState();
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffIcon::SetDebuffData(FGameplayTag InDoTType, float InDuration, int32 InStackCount)
{
	DoTType = InDoTType;
	TotalDuration = InDuration;
	RemainingDuration = InDuration;
	StackCount = InStackCount;
	bIsActive = true;
	bIsRemoving = false;
	bIsInCriticalState = false;

	UE_LOG(LogDebuffIcon, Log, TEXT("SetDebuffData: Type=%s, Duration=%.1f, Stacks=%d"),
		*DoTType.ToString(), TotalDuration, StackCount);

	// Debug: Check if bound widgets are valid
	UE_LOG(LogDebuffIcon, Warning, TEXT("  DebuffImage: %s, TimerText: %s, StackText: %s, DurationBar: %s"),
		DebuffImage ? TEXT("Valid") : TEXT("NULL"),
		TimerText ? TEXT("Valid") : TEXT("NULL"),
		StackText ? TEXT("Valid") : TEXT("NULL"),
		DurationBar ? TEXT("Valid") : TEXT("NULL"));

	// Update all visuals
	UpdateVisuals();
	UpdateTimer(RemainingDuration);
	UpdateStackCount(StackCount);

	// Show widget
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UE_LOG(LogDebuffIcon, Warning, TEXT("  Visibility set to HitTestInvisible"));

	// Play fade in animation
	if (FadeInAnimation)
	{
		PlayAnimation(FadeInAnimation);
	}

	// Hide duration bar for infinite effects
	if (DurationBar)
	{
		DurationBar->SetVisibility(IsInfinite() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

		if (!IsInfinite())
		{
			DurationBar->SetPercent(1.0f);
		}
	}

	// Notify Blueprint
	OnDebuffApplied(DoTType);
}

void UW_DebuffIcon::SetDebuffDataFromSSOT(FName EffectID, float InDuration, int32 InStackCount)
{
	if (EffectID.IsNone())
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("SetDebuffDataFromSSOT: EffectID is None"));
		return;
	}

	// Query SSOT visual data from DataManager (v2.0 API)
	USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
	if (!DataManager || !DataManager->IsStatusEffectVisualsReady())
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("SetDebuffDataFromSSOT: DataManager not available or StatusEffect visuals not ready"));
		return;
	}

	FSuspenseCoreStatusEffectVisualRow VisualData;
	if (!DataManager->GetStatusEffectVisuals(EffectID, VisualData))
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("SetDebuffDataFromSSOT: Effect '%s' not found in SSOT visuals"), *EffectID.ToString());
		return;
	}

	// Cache SSOT visual data
	CachedEffectID = EffectID;
	SSOTIconPath = VisualData.Icon;
	SSOTNormalTint = VisualData.IconTint;
	SSOTCriticalTint = VisualData.CriticalIconTint;

	// Use SSOT tag
	DoTType = VisualData.EffectTypeTag;

	// v2.0: Duration is managed by GameplayEffect, not SSOT
	// InDuration comes from GAS (via SetByCaller or GE config)
	// -1 = infinite, 0 = instant, >0 = timed
	TotalDuration = InDuration;
	RemainingDuration = TotalDuration;
	StackCount = InStackCount;
	bIsActive = true;
	bIsRemoving = false;
	bIsInCriticalState = false;

	UE_LOG(LogDebuffIcon, Log, TEXT("SetDebuffDataFromSSOT: EffectID=%s, Type=%s, Duration=%.1f, Stacks=%d"),
		*EffectID.ToString(), *DoTType.ToString(), TotalDuration, StackCount);

	// Update all visuals using SSOT data
	UpdateVisuals();
	UpdateTimer(RemainingDuration);
	UpdateStackCount(StackCount);

	// Show widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Play fade in animation
	if (FadeInAnimation)
	{
		PlayAnimation(FadeInAnimation);
	}

	// Hide duration bar for infinite effects
	if (DurationBar)
	{
		DurationBar->SetVisibility(IsInfinite() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

		if (!IsInfinite())
		{
			DurationBar->SetPercent(1.0f);
		}
	}

	// Notify Blueprint
	OnDebuffApplied(DoTType);
}

void UW_DebuffIcon::UpdateTimer(float InRemainingDuration)
{
	RemainingDuration = InRemainingDuration;

	// Update timer text
	if (TimerText)
	{
		TimerText->SetText(FormatDuration(RemainingDuration));
	}

	// Update duration bar for timed effects
	if (DurationBar && !IsInfinite() && TotalDuration > 0.0f)
	{
		const float Percent = FMath::Clamp(RemainingDuration / TotalDuration, 0.0f, 1.0f);
		DurationBar->SetPercent(Percent);
	}
}

void UW_DebuffIcon::UpdateStackCount(int32 NewStackCount)
{
	StackCount = NewStackCount;

	if (StackText)
	{
		// Hide stack text if count is 1 or less
		if (StackCount <= 1)
		{
			StackText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			StackText->SetVisibility(ESlateVisibility::HitTestInvisible);
			StackText->SetText(FText::Format(
				NSLOCTEXT("Debuff", "StackFormat", "x{0}"),
				FText::AsNumber(StackCount)
			));
		}
	}

	// Notify Blueprint
	OnStackCountChanged(NewStackCount);
}

void UW_DebuffIcon::PlayRemovalAnimation()
{
	if (bIsRemoving)
	{
		return;
	}

	bIsRemoving = true;

	UE_LOG(LogDebuffIcon, Log, TEXT("PlayRemovalAnimation: Type=%s"), *DoTType.ToString());

	// Stop pulse animation if playing
	if (PulseAnimation && IsAnimationPlaying(PulseAnimation))
	{
		StopAnimation(PulseAnimation);
	}

	// Play fade out animation
	if (FadeOutAnimation)
	{
		// Bind to animation finished
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &UW_DebuffIcon::OnFadeOutFinished);
		BindToAnimationFinished(FadeOutAnimation, FinishedEvent);

		PlayAnimation(FadeOutAnimation);
	}
	else
	{
		// No animation - immediately complete
		OnFadeOutFinished();
	}
}

void UW_DebuffIcon::ResetToDefault()
{
	// Cancel any pending loads
	if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
	{
		IconLoadHandle->CancelHandle();
	}
	IconLoadHandle.Reset();

	// Reset state
	DoTType = FGameplayTag();
	TotalDuration = -1.0f;
	RemainingDuration = -1.0f;
	StackCount = 1;
	bIsActive = false;
	bIsRemoving = false;
	bIsInCriticalState = false;

	// Reset SSOT cached data
	CachedEffectID = NAME_None;
	SSOTIconPath.Reset();
	SSOTNormalTint = FLinearColor::White;
	SSOTCriticalTint = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

	// Reset visuals
	if (DebuffImage)
	{
		DebuffImage->SetBrushFromTexture(nullptr);
		DebuffImage->SetColorAndOpacity(NormalTintColor);
	}

	if (TimerText)
	{
		TimerText->SetText(FText::GetEmpty());
	}

	if (DurationBar)
	{
		DurationBar->SetPercent(1.0f);
	}

	if (StackText)
	{
		StackText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Stop animations
	if (PulseAnimation && IsAnimationPlaying(PulseAnimation))
	{
		StopAnimation(PulseAnimation);
	}

	// Hide widget
	SetVisibility(ESlateVisibility::Collapsed);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void UW_DebuffIcon::UpdateVisuals()
{
	// Load icon for this debuff type
	LoadIconForType();

	// Reset tint color
	if (DebuffImage)
	{
		DebuffImage->SetColorAndOpacity(NormalTintColor);
		// Ensure image is visible
		DebuffImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		UE_LOG(LogDebuffIcon, Warning, TEXT("  UpdateVisuals: DebuffImage visibility set, tint applied"));
	}
	else
	{
		UE_LOG(LogDebuffIcon, Error, TEXT("  UpdateVisuals: DebuffImage is NULL!"));
	}
}

FText UW_DebuffIcon::FormatDuration(float Duration) const
{
	// Infinite effect
	if (Duration < 0.0f)
	{
		return InfiniteSymbol;
	}

	// Zero or expired
	if (Duration <= 0.0f)
	{
		return NSLOCTEXT("Debuff", "Expired", "0s");
	}

	// Long duration (>= 60 seconds): show minutes:seconds
	if (Duration >= 60.0f)
	{
		const int32 Minutes = FMath::FloorToInt(Duration / 60.0f);
		const int32 Seconds = FMath::FloorToInt(FMath::Fmod(Duration, 60.0f));

		return FText::Format(
			NSLOCTEXT("Debuff", "MinSecFormat", "{0}:{1}"),
			FText::AsNumber(Minutes),
			FText::Format(NSLOCTEXT("Debuff", "SecPad", "{0}"), FText::AsNumber(Seconds))
		);
	}

	// Short duration (< 60 seconds): show seconds with decimal
	if (Duration < 10.0f)
	{
		// Show one decimal for durations under 10s
		return FText::Format(
			NSLOCTEXT("Debuff", "SecondsDecimal", "{0}s"),
			FText::AsNumber(FMath::RoundToFloat(Duration * 10.0f) / 10.0f)
		);
	}

	// 10-60 seconds: show whole seconds
	return FText::Format(
		NSLOCTEXT("Debuff", "SecondsWhole", "{0}s"),
		FText::AsNumber(FMath::FloorToInt(Duration))
	);
}

void UW_DebuffIcon::LoadIconForType()
{
	if (!DebuffImage)
	{
		return;
	}

	// Cancel previous load
	if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
	{
		IconLoadHandle->CancelHandle();
	}

	// Try SSOT first if enabled
	if (bUseSSOTData && LoadIconFromSSOT())
	{
		return;
	}

	// Find icon for this DoT type from local map
	const TSoftObjectPtr<UTexture2D>* IconPtr = DebuffIcons.Find(DoTType);

	if (!IconPtr || IconPtr->IsNull())
	{
		// Try parent tag (e.g., State.Health.Bleeding if State.Health.Bleeding.Light not found)
		FGameplayTag ParentTag = DoTType.RequestDirectParent();
		IconPtr = DebuffIcons.Find(ParentTag);

		if (!IconPtr || IconPtr->IsNull())
		{
			UE_LOG(LogDebuffIcon, Warning, TEXT("No icon found for DoT type: %s"), *DoTType.ToString());
			return;
		}
	}

	// Check if already loaded
	if (IconPtr->IsValid())
	{
		DebuffImage->SetBrushFromTexture(IconPtr->Get());
		return;
	}

	// Async load
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	IconLoadHandle = StreamableManager.RequestAsyncLoad(
		IconPtr->ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UW_DebuffIcon::OnIconLoaded)
	);
}

bool UW_DebuffIcon::LoadIconFromSSOT()
{
	// Check if we have a cached SSOT icon path
	if (!SSOTIconPath.IsNull())
	{
		// Already have cached SSOT data, use it
		if (SSOTIconPath.IsValid())
		{
			// Already loaded
			DebuffImage->SetBrushFromTexture(SSOTIconPath.Get());
			return true;
		}

		// Async load SSOT icon
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		IconLoadHandle = StreamableManager.RequestAsyncLoad(
			SSOTIconPath.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &UW_DebuffIcon::OnIconLoaded)
		);
		return true;
	}

	// Query SSOT by tag if no cached data (v2.0 API)
	USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
	if (!DataManager)
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("LoadIconFromSSOT: DataManager not available for tag %s"), *DoTType.ToString());
		return false;
	}

	if (!DataManager->IsStatusEffectVisualsReady())
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("LoadIconFromSSOT: StatusEffectVisuals not ready for tag %s"), *DoTType.ToString());
		return false;
	}

	FSuspenseCoreStatusEffectVisualRow VisualData;
	if (!DataManager->GetStatusEffectVisualsByTag(DoTType, VisualData))
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("LoadIconFromSSOT: No SSOT visuals found for tag %s"), *DoTType.ToString());
		return false;
	}

	// Cache SSOT visual data
	CachedEffectID = VisualData.EffectID;
	SSOTIconPath = VisualData.Icon;
	SSOTNormalTint = VisualData.IconTint;
	SSOTCriticalTint = VisualData.CriticalIconTint;

	// Update tint colors from SSOT
	NormalTintColor = SSOTNormalTint;
	CriticalTintColor = SSOTCriticalTint;

	UE_LOG(LogDebuffIcon, Log, TEXT("LoadIconFromSSOT: Loaded SSOT visuals for %s (EffectID: %s)"),
		*DoTType.ToString(), *CachedEffectID.ToString());

	if (SSOTIconPath.IsNull())
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("LoadIconFromSSOT: SSOT effect '%s' has no icon configured"), *CachedEffectID.ToString());
		return false;
	}

	UE_LOG(LogDebuffIcon, Warning, TEXT("  Icon path: %s"), *SSOTIconPath.ToString());

	// Check if already loaded
	if (SSOTIconPath.IsValid())
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("  Icon already loaded, setting brush directly"));
		DebuffImage->SetBrushFromTexture(SSOTIconPath.Get());
		return true;
	}

	// Async load SSOT icon
	UE_LOG(LogDebuffIcon, Warning, TEXT("  Starting async load for icon..."));
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	IconLoadHandle = StreamableManager.RequestAsyncLoad(
		SSOTIconPath.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UW_DebuffIcon::OnIconLoaded)
	);

	return true;
}

void UW_DebuffIcon::OnIconLoaded()
{
	UE_LOG(LogDebuffIcon, Warning, TEXT("OnIconLoaded called for: %s"), *DoTType.ToString());

	if (!DebuffImage)
	{
		UE_LOG(LogDebuffIcon, Error, TEXT("  DebuffImage is NULL! Cannot set texture"));
		return;
	}

	// Check if we loaded SSOT icon
	if (bUseSSOTData && SSOTIconPath.IsValid())
	{
		UTexture2D* LoadedTexture = SSOTIconPath.Get();
		if (LoadedTexture)
		{
			DebuffImage->SetBrushFromTexture(LoadedTexture);
			UE_LOG(LogDebuffIcon, Warning, TEXT("  SSOT icon SET for effect: %s (Texture: %s)"),
				*CachedEffectID.ToString(), *LoadedTexture->GetName());
		}
		else
		{
			UE_LOG(LogDebuffIcon, Error, TEXT("  SSOTIconPath.Get() returned NULL for: %s"), *CachedEffectID.ToString());
		}
		return;
	}
	else
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("  SSOT path not valid after async load: bUseSSOT=%d, PathValid=%d"),
			bUseSSOTData, SSOTIconPath.IsValid());
	}

	// Find the icon again and set it from local map
	const TSoftObjectPtr<UTexture2D>* IconPtr = DebuffIcons.Find(DoTType);

	if (!IconPtr)
	{
		// Try parent tag
		FGameplayTag ParentTag = DoTType.RequestDirectParent();
		IconPtr = DebuffIcons.Find(ParentTag);
	}

	if (IconPtr && IconPtr->IsValid())
	{
		DebuffImage->SetBrushFromTexture(IconPtr->Get());
		UE_LOG(LogDebuffIcon, Verbose, TEXT("Icon loaded for DoT type: %s"), *DoTType.ToString());
	}
}

void UW_DebuffIcon::UpdateCriticalState()
{
	// Only check for timed effects
	if (IsInfinite())
	{
		return;
	}

	const bool bShouldBeCritical = (RemainingDuration > 0.0f && RemainingDuration <= CriticalDurationThreshold);

	if (bShouldBeCritical != bIsInCriticalState)
	{
		bIsInCriticalState = bShouldBeCritical;

		// Update visual tint (use SSOT colors if available)
		if (DebuffImage)
		{
			const FLinearColor& NormalColor = (bUseSSOTData && !CachedEffectID.IsNone()) ? SSOTNormalTint : NormalTintColor;
			const FLinearColor& CriticalColor = (bUseSSOTData && !CachedEffectID.IsNone()) ? SSOTCriticalTint : CriticalTintColor;
			DebuffImage->SetColorAndOpacity(bIsInCriticalState ? CriticalColor : NormalColor);
		}

		// Start/stop pulse animation
		if (PulseAnimation)
		{
			if (bIsInCriticalState)
			{
				PlayAnimation(PulseAnimation, 0.0f, 0);  // 0 = loop forever
			}
			else if (IsAnimationPlaying(PulseAnimation))
			{
				StopAnimation(PulseAnimation);
			}
		}

		// Notify Blueprint
		OnCriticalState(bIsInCriticalState);
	}
}

void UW_DebuffIcon::OnFadeOutFinished()
{
	// Reset and hide
	SetVisibility(ESlateVisibility::Collapsed);
	bIsActive = false;

	// Broadcast completion for container to release to pool
	OnRemovalComplete.Broadcast(this);

	UE_LOG(LogDebuffIcon, Log, TEXT("Removal animation complete for: %s"), *DoTType.ToString());
}
