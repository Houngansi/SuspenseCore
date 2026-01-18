// SuspenseCoreReloadProgressWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreReloadProgressWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"

USuspenseCoreReloadProgressWidget::USuspenseCoreReloadProgressWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create dynamic material instance for progress bar (if using material-based progress)
	CreateMaterialInstanceForProgressBar();

	SetupEventSubscriptions();

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
}

void USuspenseCoreReloadProgressWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreReloadProgressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsReloading)
	{
		return;
	}

	// Update elapsed time and calculate target progress
	ElapsedReloadTime += InDeltaTime;

	if (TotalReloadDuration > 0.0f)
	{
		// Calculate target progress based on elapsed time (0.0 to 1.0)
		TargetProgress = FMath::Clamp(ElapsedReloadTime / TotalReloadDuration, 0.0f, 1.0f);
	}

	// Smooth interpolation of displayed progress
	if (bSmoothProgress)
	{
		if (FMath::Abs(DisplayedProgress - TargetProgress) > KINDA_SMALL_NUMBER)
		{
			DisplayedProgress = FMath::FInterpTo(
				DisplayedProgress,
				TargetProgress,
				InDeltaTime,
				ProgressInterpSpeed
			);
		}
	}
	else
	{
		DisplayedProgress = TargetProgress;
	}

	// Update UI
	UpdateProgressUI();
	UpdateTimeRemainingUI();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreReloadProgressWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadProgressWidget::ShowReloadProgress_Implementation(const FSuspenseCoreReloadProgressData& ReloadData)
{
	CachedReloadData = ReloadData;
	bIsReloading = true;
	bCanCancel = ReloadData.bCanCancel;
	CurrentPhase = 0;

	// Initialize progress tracking
	TotalReloadDuration = ReloadData.TotalDuration;
	ElapsedReloadTime = 0.0f;
	TargetProgress = 0.0f;
	DisplayedProgress = 0.0f;

	// Update reload type text
	if (ReloadTypeText)
	{
		ReloadTypeText->SetText(GetReloadTypeDisplayText(ReloadData.ReloadType));
	}

	// Update progress bar
	UpdateProgressUI();
	UpdateTimeRemainingUI();

	// Reset phase indicators to dim state
	UpdatePhaseIndicators(0);

	// Show cancel hint if applicable
	if (CancelHintText)
	{
		CancelHintText->SetText(CancelHintFormat);
		CancelHintText->SetVisibility(bCanCancel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Show widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Notify Blueprint
	OnReloadStarted(ReloadData.ReloadType);
}

void USuspenseCoreReloadProgressWidget::UpdateReloadProgress_Implementation(float Progress, float RemainingTime)
{
	// External progress update (optional - widget calculates its own progress)
	TargetProgress = FMath::Clamp(Progress, 0.0f, 1.0f);

	// Sync elapsed time from remaining time
	if (TotalReloadDuration > 0.0f)
	{
		ElapsedReloadTime = TotalReloadDuration - RemainingTime;
	}

	if (!bSmoothProgress)
	{
		DisplayedProgress = TargetProgress;
		UpdateProgressUI();
		UpdateTimeRemainingUI();
	}
}

void USuspenseCoreReloadProgressWidget::HideReloadProgress_Implementation(bool bCompleted)
{
	bIsReloading = false;

	if (bCompleted)
	{
		// Show full completion before hiding
		DisplayedProgress = 1.0f;
		TargetProgress = 1.0f;
		UpdateProgressUI();
		OnReloadCompleted();
	}

	// Hide widget
	SetVisibility(ESlateVisibility::Collapsed);

	// Reset all state
	CurrentPhase = 0;
	TargetProgress = 0.0f;
	DisplayedProgress = 0.0f;
	TotalReloadDuration = 0.0f;
	ElapsedReloadTime = 0.0f;
}

void USuspenseCoreReloadProgressWidget::OnMagazineEjected_Implementation()
{
	CurrentPhase = 1;
	UpdatePhaseIndicators(1);
	OnPhaseChanged(1);
}

void USuspenseCoreReloadProgressWidget::OnMagazineInserted_Implementation()
{
	CurrentPhase = 2;
	UpdatePhaseIndicators(2);
	OnPhaseChanged(2);
}

void USuspenseCoreReloadProgressWidget::OnChambering_Implementation()
{
	CurrentPhase = 3;
	UpdatePhaseIndicators(3);
	OnPhaseChanged(3);
}

void USuspenseCoreReloadProgressWidget::OnReloadCancelled_Implementation()
{
	bIsReloading = false;

	// Hide immediately on cancel
	SetVisibility(ESlateVisibility::Collapsed);

	// Notify Blueprint
	OnReloadCancelledBP();
}

void USuspenseCoreReloadProgressWidget::SetReloadTypeDisplay_Implementation(ESuspenseCoreReloadType ReloadType, const FText& DisplayText)
{
	if (ReloadTypeText)
	{
		FText TextToShow = DisplayText.IsEmpty() ? GetReloadTypeDisplayText(ReloadType) : DisplayText;
		ReloadTypeText->SetText(TextToShow);
	}
}

void USuspenseCoreReloadProgressWidget::SetCanCancelReload_Implementation(bool bInCanCancel)
{
	bCanCancel = bInCanCancel;

	if (CancelHintText)
	{
		CancelHintText->SetVisibility(bCanCancel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool USuspenseCoreReloadProgressWidget::IsReloadProgressVisible_Implementation() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

float USuspenseCoreReloadProgressWidget::GetCurrentReloadProgress_Implementation() const
{
	return DisplayedProgress;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadProgressWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Event;
	using namespace SuspenseCoreEquipmentTags::Magazine;

	// Reload start/end - main lifecycle events
	ReloadStartHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_ReloadStart,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreReloadProgressWidget::OnReloadStartEvent),
		ESuspenseCoreEventPriority::Normal
	);

	ReloadEndHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_ReloadEnd,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreReloadProgressWidget::OnReloadEndEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Phase events - for phase indicator updates
	MagazineEjectedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_Ejected,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreReloadProgressWidget::OnMagazineEjectedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	MagazineInsertedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_Inserted,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreReloadProgressWidget::OnMagazineInsertedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	ChamberHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Chamber_Chambered,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreReloadProgressWidget::OnChamberEvent),
		ESuspenseCoreEventPriority::Normal
	);
}

void USuspenseCoreReloadProgressWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	EventBus->Unsubscribe(ReloadStartHandle);
	EventBus->Unsubscribe(ReloadEndHandle);
	EventBus->Unsubscribe(MagazineEjectedHandle);
	EventBus->Unsubscribe(MagazineInsertedHandle);
	EventBus->Unsubscribe(ChamberHandle);
}

USuspenseCoreEventBus* USuspenseCoreReloadProgressWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		const_cast<USuspenseCoreReloadProgressWidget*>(this)->CachedEventBus = EventManager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

void USuspenseCoreReloadProgressWidget::OnReloadStartEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float TotalDuration = EventData.GetFloat(TEXT("Duration"), 2.0f);
	FString ReloadTypeStr = EventData.GetString(TEXT("ReloadType"));
	bool bCanCancelReload = EventData.GetBool(TEXT("CanCancel"), true);

	FSuspenseCoreReloadProgressData ReloadData;
	ReloadData.TotalDuration = TotalDuration;
	ReloadData.bCanCancel = bCanCancelReload;

	// Parse reload type
	if (ReloadTypeStr == TEXT("Tactical"))
	{
		ReloadData.ReloadType = ESuspenseCoreReloadType::Tactical;
	}
	else if (ReloadTypeStr == TEXT("Empty"))
	{
		ReloadData.ReloadType = ESuspenseCoreReloadType::Empty;
	}
	else if (ReloadTypeStr == TEXT("Emergency"))
	{
		ReloadData.ReloadType = ESuspenseCoreReloadType::Emergency;
		ReloadData.bIsQuickReload = true;
	}
	else if (ReloadTypeStr == TEXT("ChamberOnly"))
	{
		ReloadData.ReloadType = ESuspenseCoreReloadType::ChamberOnly;
	}

	ShowReloadProgress_Implementation(ReloadData);
}

void USuspenseCoreReloadProgressWidget::OnReloadEndEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	bool bCompleted = EventData.GetBool(TEXT("Completed"), true);
	HideReloadProgress_Implementation(bCompleted);
}

void USuspenseCoreReloadProgressWidget::OnMagazineEjectedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (bIsReloading)
	{
		OnMagazineEjected_Implementation();
	}
}

void USuspenseCoreReloadProgressWidget::OnMagazineInsertedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (bIsReloading)
	{
		OnMagazineInserted_Implementation();
	}
}

void USuspenseCoreReloadProgressWidget::OnChamberEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (bIsReloading)
	{
		OnChambering_Implementation();
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadProgressWidget::UpdateProgressUI()
{
	if (!ReloadProgressBar)
	{
		return;
	}

	// Use material parameter if available, otherwise fallback to standard SetPercent
	if (bUseMaterialProgress && ReloadProgressMaterial)
	{
		ReloadProgressMaterial->SetScalarParameterValue(MaterialProgressParameterName, DisplayedProgress);
	}
	else
	{
		ReloadProgressBar->SetPercent(DisplayedProgress);
	}
}

void USuspenseCoreReloadProgressWidget::UpdatePhaseIndicators(int32 NewPhase)
{
	if (!bShowPhaseIndicators)
	{
		return;
	}

	// Phase 1: Eject
	if (EjectPhaseIndicator)
	{
		// Opacity handled by Blueprint - we just set visibility
		EjectPhaseIndicator->SetRenderOpacity(NewPhase >= 1 ? 1.0f : 0.3f);
	}
	if (EjectPhaseText)
	{
		EjectPhaseText->SetRenderOpacity(NewPhase >= 1 ? 1.0f : 0.3f);
	}

	// Phase 2: Insert
	if (InsertPhaseIndicator)
	{
		InsertPhaseIndicator->SetRenderOpacity(NewPhase >= 2 ? 1.0f : 0.3f);
	}
	if (InsertPhaseText)
	{
		InsertPhaseText->SetRenderOpacity(NewPhase >= 2 ? 1.0f : 0.3f);
	}

	// Phase 3: Chamber
	if (ChamberPhaseIndicator)
	{
		ChamberPhaseIndicator->SetRenderOpacity(NewPhase >= 3 ? 1.0f : 0.3f);
	}
	if (ChamberPhaseText)
	{
		ChamberPhaseText->SetRenderOpacity(NewPhase >= 3 ? 1.0f : 0.3f);
	}
}

void USuspenseCoreReloadProgressWidget::UpdateTimeRemainingUI()
{
	if (!bShowTimeRemaining || !TimeRemainingText)
	{
		return;
	}

	// Calculate remaining time from elapsed and total
	const float RemainingTime = FMath::Max(0.0f, TotalReloadDuration - ElapsedReloadTime);

	// Format with one decimal place (e.g., "1.5s")
	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetMaximumFractionalDigits(1);
	NumberFormat.SetMinimumFractionalDigits(1);

	FText TimeText = FText::Format(
		NSLOCTEXT("Reload", "TimeRemaining", "{0}s"),
		FText::AsNumber(RemainingTime, &NumberFormat)
	);

	TimeRemainingText->SetText(TimeText);
}

FText USuspenseCoreReloadProgressWidget::GetReloadTypeDisplayText(ESuspenseCoreReloadType ReloadType) const
{
	switch (ReloadType)
	{
	case ESuspenseCoreReloadType::Tactical:
		return TacticalReloadText;
	case ESuspenseCoreReloadType::Empty:
		return EmptyReloadText;
	case ESuspenseCoreReloadType::Emergency:
		return EmergencyReloadText;
	case ESuspenseCoreReloadType::ChamberOnly:
		return ChamberOnlyText;
	default:
		return FText::GetEmpty();
	}
}

void USuspenseCoreReloadProgressWidget::CreateMaterialInstanceForProgressBar()
{
	if (!bUseMaterialProgress || !ReloadProgressBar)
	{
		return;
	}

	// Reset fill color to white so material displays correctly
	ReloadProgressBar->SetFillColorAndOpacity(FLinearColor::White);

	// Get the BACKGROUND image style from the progress bar (NOT FillImage!)
	// The material handles both background and fill portions via FillAmount parameter
	const FProgressBarStyle& Style = ReloadProgressBar->GetWidgetStyle();
	const FSlateBrush& BackgroundBrush = Style.BackgroundImage;

	// Check if brush has a material resource
	UObject* ResourceObject = BackgroundBrush.GetResourceObject();
	if (!ResourceObject)
	{
		return;
	}

	// Try to get material interface from the brush
	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(ResourceObject);
	if (!MaterialInterface)
	{
		return;
	}

	// Create dynamic material instance
	ReloadProgressMaterial = UMaterialInstanceDynamic::Create(MaterialInterface, this);
	if (ReloadProgressMaterial)
	{
		// Apply the dynamic material back to the progress bar's BACKGROUND image
		FProgressBarStyle NewStyle = Style;
		NewStyle.BackgroundImage.SetResourceObject(ReloadProgressMaterial);
		ReloadProgressBar->SetWidgetStyle(NewStyle);

		// Initialize to 0 progress
		ReloadProgressMaterial->SetScalarParameterValue(MaterialProgressParameterName, 0.0f);
	}
}
