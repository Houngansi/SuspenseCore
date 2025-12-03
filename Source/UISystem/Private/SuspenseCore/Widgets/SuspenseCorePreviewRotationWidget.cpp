// SuspenseCorePreviewRotationWidget.cpp
// SuspenseCore - Clean Architecture UISystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCorePreviewRotationWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Actors/SuspenseCoreCharacterPreviewActor.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCorePreviewRotation, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCorePreviewRotationWidget::USuspenseCorePreviewRotationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Widget needs to be visible for hit testing
	SetVisibility(ESlateVisibility::Visible);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePreviewRotationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] NativeConstruct CALLED"));
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Widget: %s"), *GetName());
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Visibility: %d"), (int32)GetVisibility());
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("═══════════════════════════════════════════════════════════════"));

	// Setup hit test area bindings
	SetupHitTestArea();

	// Auto-find preview actor in the world
	if (bAutoFindPreviewActor)
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] bAutoFindPreviewActor=true, searching..."));
		ASuspenseCoreCharacterPreviewActor* FoundActor = FindPreviewActorInWorld();
		if (FoundActor)
		{
			CachedPreviewActor = FoundActor;
			UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] SUCCESS: Auto-found PreviewActor: %s"), *FoundActor->GetName());
		}
		else
		{
			UE_LOG(LogSuspenseCorePreviewRotation, Error, TEXT("[PreviewRotationWidget] FAILED: No PreviewActor found in world!"));
		}
	}
	else
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] bAutoFindPreviewActor=false, skipping search"));
	}

	// Cache EventBus
	USuspenseCoreEventBus* EventBus = GetEventBus();
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] EventBus: %s"), EventBus ? TEXT("FOUND") : TEXT("NOT FOUND"));

	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Init complete. Sensitivity: %.2f, bRotationEnabled: %s"),
		RotationSensitivity, bRotationEnabled ? TEXT("true") : TEXT("false"));
}

void USuspenseCorePreviewRotationWidget::NativeDestruct()
{
	bIsDragging = false;
	CachedEventBus.Reset();
	CachedPreviewActor.Reset();

	Super::NativeDestruct();
}

FReply USuspenseCorePreviewRotationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("████████████████████████████████████████████████████████████████"));
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] NativeOnMouseButtonDown CALLED!"));
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Button: %s"), *InMouseEvent.GetEffectingButton().ToString());
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] bRotationEnabled: %s"), bRotationEnabled ? TEXT("true") : TEXT("false"));
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("████████████████████████████████████████████████████████████████"));

	if (!bRotationEnabled)
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Rotation DISABLED, returning Unhandled"));
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

		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] DRAG STARTED at: %.1f, %.1f"),
			LastMousePosition.X, LastMousePosition.Y);
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] CachedPreviewActor: %s"),
			CachedPreviewActor.IsValid() ? *CachedPreviewActor->GetName() : TEXT("INVALID/NULL"));

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

		UE_LOG(LogSuspenseCorePreviewRotation, Log, TEXT("[PreviewRotationWidget] Drag ended. Total rotation: %.1f"),
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
			UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] MouseMove DeltaX: %.2f, DeltaYaw: %.2f"), DeltaX, DeltaYaw);

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
	CachedPreviewActor = InPreviewActor;

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
	if (CachedPreviewActor.IsValid())
	{
		CachedPreviewActor->SetPreviewRotation(Yaw);
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

void USuspenseCorePreviewRotationWidget::SetupHitTestArea()
{
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] SetupHitTestArea called"));
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] HitTestArea (Border): %s"), HitTestArea ? TEXT("BOUND") : TEXT("NULL"));
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] HitTestImage (Image): %s"), HitTestImage ? TEXT("BOUND") : TEXT("NULL"));

	// Check if HitTestArea (Border) is bound
	if (HitTestArea)
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] HitTestArea Visibility: %d"), (int32)HitTestArea->GetVisibility());
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] HitTestArea DesiredSize: %s"), *HitTestArea->GetDesiredSize().ToString());
		return;
	}

	// Check if HitTestImage (Image) is bound
	if (HitTestImage)
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] HitTestImage Visibility: %d"), (int32)HitTestImage->GetVisibility());
		return;
	}

	// No hit test area bound - warn the user
	UE_LOG(LogSuspenseCorePreviewRotation, Error,
		TEXT("[PreviewRotationWidget] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
	UE_LOG(LogSuspenseCorePreviewRotation, Error,
		TEXT("[PreviewRotationWidget] NO HIT TEST AREA BOUND!"));
	UE_LOG(LogSuspenseCorePreviewRotation, Error,
		TEXT("[PreviewRotationWidget] Add a Border named 'HitTestArea' in UMG Designer!"));
	UE_LOG(LogSuspenseCorePreviewRotation, Error,
		TEXT("[PreviewRotationWidget] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));
}

ASuspenseCoreCharacterPreviewActor* USuspenseCorePreviewRotationWidget::FindPreviewActorInWorld()
{
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] FindPreviewActorInWorld called"));

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Error, TEXT("[PreviewRotationWidget] GetWorld() returned NULL!"));
		return nullptr;
	}

	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] World: %s"), *World->GetName());

	// Find first CharacterPreviewActor in the world
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, ASuspenseCoreCharacterPreviewActor::StaticClass(), FoundActors);

	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Found %d CharacterPreviewActor(s) in world"), FoundActors.Num());

	for (int32 i = 0; i < FoundActors.Num(); i++)
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget]   [%d] %s"), i, *FoundActors[i]->GetName());
	}

	if (FoundActors.Num() > 0)
	{
		return Cast<ASuspenseCoreCharacterPreviewActor>(FoundActors[0]);
	}

	return nullptr;
}

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
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] ApplyRotationDelta: %.2f"), DeltaYaw);
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] CachedPreviewActor valid: %s"),
		CachedPreviewActor.IsValid() ? TEXT("YES") : TEXT("NO"));

	// Direct reference takes priority (faster, no event overhead)
	if (CachedPreviewActor.IsValid())
	{
		UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Calling RotatePreview on: %s"),
			*CachedPreviewActor->GetName());
		CachedPreviewActor->RotatePreview(DeltaYaw);
		return;
	}

	// Fallback to EventBus
	UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] Using EventBus fallback"));
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
