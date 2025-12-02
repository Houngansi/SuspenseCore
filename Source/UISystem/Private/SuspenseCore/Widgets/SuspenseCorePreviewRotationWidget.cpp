// SuspenseCorePreviewRotationWidget.cpp
// SuspenseCore - Clean Architecture UISystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCorePreviewRotationWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Actors/SuspenseCoreCharacterPreviewActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCorePreviewRotation, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCorePreviewRotationWidget::USuspenseCorePreviewRotationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Widget needs to be hit-testable but can be invisible
	SetVisibility(ESlateVisibility::Visible);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePreviewRotationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache EventBus
	GetEventBus();

	UE_LOG(LogSuspenseCorePreviewRotation, Log, TEXT("[PreviewRotationWidget] Initialized. Sensitivity: %.2f"), RotationSensitivity);
}

void USuspenseCorePreviewRotationWidget::NativeDestruct()
{
	bIsDragging = false;
	CachedEventBus.Reset();

	Super::NativeDestruct();
}

FReply USuspenseCorePreviewRotationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bRotationEnabled)
	{
		return FReply::Unhandled();
	}

	// Only handle left mouse button
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = true;
		LastMousePosition = InMouseEvent.GetScreenSpacePosition();

		// Notify
		OnDragStarted();
		OnDragStartedDelegate.Broadcast();

		UE_LOG(LogSuspenseCorePreviewRotation, Verbose, TEXT("[PreviewRotationWidget] Drag started at: %.1f, %.1f"),
			LastMousePosition.X, LastMousePosition.Y);

		// Capture mouse to continue receiving events even if cursor leaves widget bounds
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return FReply::Unhandled();
}

FReply USuspenseCorePreviewRotationWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsDragging)
	{
		bIsDragging = false;

		// Notify
		OnDragEnded();
		OnDragEndedDelegate.Broadcast();

		UE_LOG(LogSuspenseCorePreviewRotation, Verbose, TEXT("[PreviewRotationWidget] Drag ended. Total rotation: %.1f"),
			AccumulatedYaw);

		// Release mouse capture
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply USuspenseCorePreviewRotationWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging && bRotationEnabled)
	{
		FVector2D CurrentMousePosition = InMouseEvent.GetScreenSpacePosition();
		float DeltaX = CurrentMousePosition.X - LastMousePosition.X;

		// Apply sensitivity and inversion
		float DeltaYaw = DeltaX * RotationSensitivity;
		if (bInvertRotation)
		{
			DeltaYaw = -DeltaYaw;
		}

		// Only apply if there's actual movement
		if (!FMath::IsNearlyZero(DeltaYaw))
		{
			ApplyRotationDelta(DeltaYaw);

			// Update accumulated rotation
			AccumulatedYaw += DeltaYaw;
			AccumulatedYaw = FMath::Fmod(AccumulatedYaw, 360.0f);

			// Notify
			OnRotationChanged(DeltaYaw, AccumulatedYaw);
			OnRotationChangedDelegate.Broadcast(DeltaYaw, AccumulatedYaw);
		}

		LastMousePosition = CurrentMousePosition;

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void USuspenseCorePreviewRotationWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	// Don't stop dragging on mouse leave - we capture the mouse
	// The drag will continue until mouse button is released
	Super::NativeOnMouseLeave(InMouseEvent);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePreviewRotationWidget::SetPreviewActor(ASuspenseCoreCharacterPreviewActor* InPreviewActor)
{
	PreviewActorReference = InPreviewActor;

	UE_LOG(LogSuspenseCorePreviewRotation, Log, TEXT("[PreviewRotationWidget] Preview actor set: %s"),
		InPreviewActor ? *InPreviewActor->GetName() : TEXT("None"));
}

void USuspenseCorePreviewRotationWidget::ResetRotation()
{
	SetRotation(0.0f);
}

void USuspenseCorePreviewRotationWidget::SetRotation(float Yaw)
{
	AccumulatedYaw = FMath::Fmod(Yaw, 360.0f);

	// Apply via direct reference if available
	if (PreviewActorReference)
	{
		PreviewActorReference->SetPreviewRotation(Yaw);
	}
	else
	{
		// Publish set rotation event
		if (USuspenseCoreEventBus* EventBus = GetEventBus())
		{
			FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
			EventData.SetFloat(FName("Yaw"), Yaw);

			EventBus->Publish(
				FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Preview.SetRotation")),
				EventData
			);
		}
	}

	UE_LOG(LogSuspenseCorePreviewRotation, Verbose, TEXT("[PreviewRotationWidget] Rotation set to: %.1f"), Yaw);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreEventBus* USuspenseCorePreviewRotationWidget::GetEventBus()
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld()))
	{
		USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
		if (EventBus)
		{
			CachedEventBus = EventBus;
		}
		return EventBus;
	}

	return nullptr;
}

void USuspenseCorePreviewRotationWidget::ApplyRotationDelta(float DeltaYaw)
{
	// Direct reference takes priority (faster, no event overhead)
	if (PreviewActorReference)
	{
		PreviewActorReference->RotatePreview(DeltaYaw);
		return;
	}

	// Fallback to EventBus
	PublishRotationEvent(DeltaYaw);
}

void USuspenseCorePreviewRotationWidget::PublishRotationEvent(float DeltaYaw)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] EventBus not available!"));
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetFloat(FName("DeltaYaw"), DeltaYaw);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Preview.Rotate")),
		EventData
	);
}
