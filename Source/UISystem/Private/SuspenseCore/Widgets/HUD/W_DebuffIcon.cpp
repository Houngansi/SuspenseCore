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
#include "SuspenseCore/Types/UI/SuspenseCoreDoTUITypes.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
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
	bIsCritical = false;

	UE_LOG(LogDebuffIcon, Log, TEXT("SetDebuffData: Type=%s, Duration=%.1f, Stacks=%d"),
		*DoTType.ToString(), TotalDuration, StackCount);

	// Update all visuals
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
	bIsCritical = false;

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

	TSoftObjectPtr<UTexture2D> IconToLoad;

	// SSOT Strategy 1: Try DataManager (centralized DataTable)
	if (USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this))
	{
		FSuspenseCoreDoTUIData UIData;
		if (DataManager->GetDoTUIData(DoTType, UIData))
		{
			IconToLoad = UIData.Icon;

			// Also apply colors from SSOT
			NormalTintColor = UIData.NormalColor;
			CriticalTintColor = UIData.CriticalColor;

			UE_LOG(LogDebuffIcon, Verbose, TEXT("Using SSOT icon for DoT type: %s"), *DoTType.ToString());
		}
	}

	// Fallback Strategy 2: Use Blueprint-configured TMap (EditDefaultsOnly)
	if (IconToLoad.IsNull())
	{
		const TSoftObjectPtr<UTexture2D>* IconPtr = DebuffIcons.Find(DoTType);

		if (!IconPtr || IconPtr->IsNull())
		{
			// Try parent tag (e.g., State.Health.Bleeding if State.Health.Bleeding.Light not found)
			FGameplayTag ParentTag = DoTType.RequestDirectParent();
			IconPtr = DebuffIcons.Find(ParentTag);
		}

		if (IconPtr && !IconPtr->IsNull())
		{
			IconToLoad = *IconPtr;
		}
	}

	// No icon found anywhere
	if (IconToLoad.IsNull())
	{
		UE_LOG(LogDebuffIcon, Warning, TEXT("No icon found for DoT type: %s (checked SSOT and Blueprint TMap)"), *DoTType.ToString());
		return;
	}

	// Check if already loaded
	if (IconToLoad.IsValid())
	{
		DebuffImage->SetBrushFromTexture(IconToLoad.Get());
		return;
	}

	// Async load - store path for OnIconLoaded callback
	CachedIconPath = IconToLoad;

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	IconLoadHandle = StreamableManager.RequestAsyncLoad(
		IconToLoad.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UW_DebuffIcon::OnIconLoaded)
	);
}

void UW_DebuffIcon::OnIconLoaded()
{
	// Use cached path from LoadIconForType
	if (CachedIconPath.IsValid() && DebuffImage)
	{
		DebuffImage->SetBrushFromTexture(CachedIconPath.Get());
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

	if (bShouldBeCritical != bIsCritical)
	{
		bIsCritical = bShouldBeCritical;

		// Update visual tint
		if (DebuffImage)
		{
			DebuffImage->SetColorAndOpacity(bIsCritical ? CriticalTintColor : NormalTintColor);
		}

		// Start/stop pulse animation
		if (PulseAnimation)
		{
			if (bIsCritical)
			{
				PlayAnimation(PulseAnimation, 0.0f, 0);  // 0 = loop forever
			}
			else if (IsAnimationPlaying(PulseAnimation))
			{
				StopAnimation(PulseAnimation);
			}
		}

		// Notify Blueprint
		OnCriticalState(bIsCritical);
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
