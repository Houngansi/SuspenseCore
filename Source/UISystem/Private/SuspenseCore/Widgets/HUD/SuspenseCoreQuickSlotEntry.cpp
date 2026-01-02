// SuspenseCoreQuickSlotEntry.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreQuickSlotEntry.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

USuspenseCoreQuickSlotEntry::USuspenseCoreQuickSlotEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotEntry::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize to empty state
	ClearSlot();

	// Hide overlays by default
	if (HighlightOverlay)
	{
		HighlightOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UnavailableOverlay)
	{
		UnavailableOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Hide cooldown bar initially
	if (CooldownBar)
	{
		CooldownBar->SetVisibility(ESlateVisibility::Collapsed);
		CooldownBar->SetPercent(0.0f);
	}
}

void USuspenseCoreQuickSlotEntry::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Smooth cooldown interpolation
	if (bIsOnCooldown && bSmoothCooldown)
	{
		if (FMath::Abs(DisplayedCooldown - TargetCooldown) > KINDA_SMALL_NUMBER)
		{
			DisplayedCooldown = FMath::FInterpTo(
				DisplayedCooldown,
				TargetCooldown,
				InDeltaTime,
				CooldownInterpSpeed
			);

			if (CooldownBar)
			{
				CooldownBar->SetPercent(DisplayedCooldown);
			}
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotEntry::InitializeSlot(int32 InSlotIndex, const FText& InHotkeyText)
{
	SlotIndex = InSlotIndex;

	if (HotkeyText)
	{
		HotkeyText->SetText(InHotkeyText);
	}
}

void USuspenseCoreQuickSlotEntry::UpdateSlotData(const FSuspenseCoreQuickSlotHUDData& SlotData)
{
	bIsEmpty = SlotData.IsEmpty();

	if (bIsEmpty)
	{
		ClearSlot();
		return;
	}

	// Update icon
	if (ItemIcon)
	{
		if (SlotData.Icon)
		{
			ItemIcon->SetBrushFromTexture(SlotData.Icon);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (EmptySlotTexture)
		{
			ItemIcon->SetBrushFromTexture(EmptySlotTexture);
		}
	}

	// Update quantity text
	if (QuantityText)
	{
		if (SlotData.bIsMagazine && SlotData.MagazineRounds >= 0)
		{
			// Magazine: show rounds
			QuantityText->SetText(FText::AsNumber(SlotData.MagazineRounds));
		}
		else if (SlotData.Quantity > 1)
		{
			// Stackable: show quantity
			QuantityText->SetText(FText::Format(
				NSLOCTEXT("QuickSlot", "Quantity", "x{0}"),
				FText::AsNumber(SlotData.Quantity)
			));
		}
		else
		{
			// Single item: hide quantity
			QuantityText->SetText(FText::GetEmpty());
		}
	}

	// Update cooldown
	if (SlotData.CooldownRemaining > 0.0f)
	{
		bIsOnCooldown = true;
		TargetCooldown = SlotData.CooldownRemaining / FMath::Max(SlotData.CooldownDuration, 0.001f);

		if (CooldownBar)
		{
			CooldownBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	else
	{
		bIsOnCooldown = false;
		TargetCooldown = 0.0f;
		DisplayedCooldown = 0.0f;

		if (CooldownBar)
		{
			CooldownBar->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update availability
	SetAvailable(SlotData.bIsAvailable);
}

void USuspenseCoreQuickSlotEntry::ClearSlot()
{
	bIsEmpty = true;
	bIsOnCooldown = false;
	TargetCooldown = 0.0f;
	DisplayedCooldown = 0.0f;

	if (ItemIcon)
	{
		if (EmptySlotTexture)
		{
			ItemIcon->SetBrushFromTexture(EmptySlotTexture);
		}
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::GetEmpty());
	}

	if (CooldownBar)
	{
		CooldownBar->SetVisibility(ESlateVisibility::Collapsed);
		CooldownBar->SetPercent(0.0f);
	}
}

void USuspenseCoreQuickSlotEntry::UpdateCooldown(float Progress)
{
	if (bSmoothCooldown)
	{
		TargetCooldown = FMath::Clamp(Progress, 0.0f, 1.0f);
	}
	else
	{
		DisplayedCooldown = FMath::Clamp(Progress, 0.0f, 1.0f);
		if (CooldownBar)
		{
			CooldownBar->SetPercent(DisplayedCooldown);
		}
	}

	bIsOnCooldown = Progress > 0.0f;

	if (CooldownBar)
	{
		CooldownBar->SetVisibility(bIsOnCooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!bIsOnCooldown)
	{
		OnCooldownEndedBP();
	}
}

void USuspenseCoreQuickSlotEntry::SetCooldownTarget(float RemainingTime, float TotalTime)
{
	TotalCooldownTime = TotalTime;
	bIsOnCooldown = RemainingTime > 0.0f;

	if (bIsOnCooldown)
	{
		TargetCooldown = RemainingTime / FMath::Max(TotalTime, 0.001f);

		if (CooldownBar)
		{
			CooldownBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		OnCooldownStartedBP(TotalTime);
	}
	else
	{
		TargetCooldown = 0.0f;
		DisplayedCooldown = 0.0f;

		if (CooldownBar)
		{
			CooldownBar->SetVisibility(ESlateVisibility::Collapsed);
		}

		OnCooldownEndedBP();
	}
}

void USuspenseCoreQuickSlotEntry::UpdateMagazineRounds(int32 CurrentRounds, int32 MaxRounds)
{
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(CurrentRounds));
	}
}

void USuspenseCoreQuickSlotEntry::SetAvailable(bool bAvailable)
{
	bIsAvailable = bAvailable;

	if (UnavailableOverlay)
	{
		UnavailableOverlay->SetVisibility(bAvailable ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	OnAvailabilityChangedBP(bAvailable);
}

void USuspenseCoreQuickSlotEntry::SetHighlighted(bool bHighlighted)
{
	bIsHighlighted = bHighlighted;

	if (HighlightOverlay)
	{
		HighlightOverlay->SetVisibility(bHighlighted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreQuickSlotEntry::PlayUseAnimation()
{
	OnSlotUsedBP();
}
