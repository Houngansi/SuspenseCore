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
	SetVisibility(ESlateVisibility::Visible);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePreviewRotationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupHitTestArea();

	// Auto-find preview actor in the world
	if (bAutoFindPreviewActor)
	{
		// Try 1: Find by C++ class
		ASuspenseCoreCharacterPreviewActor* FoundActor = FindPreviewActorInWorld();
		if (FoundActor)
		{
			CachedPreviewActor = FoundActor;
		}
		else
		{
			// Try 2: Find by name pattern
			AActor* FoundByName = FindActorByNamePattern(PreviewActorNamePattern);
			if (FoundByName)
			{
				ASuspenseCoreCharacterPreviewActor* AsPreviewActor = Cast<ASuspenseCoreCharacterPreviewActor>(FoundByName);
				if (AsPreviewActor)
				{
					CachedPreviewActor = AsPreviewActor;
				}
				else
				{
					CachedGenericActor = FoundByName;
				}
			}
			else
			{
				// Try 3: Find by tag
				AActor* FoundByTag = FindActorByTag(PreviewActorTag);
				if (FoundByTag)
				{
					CachedGenericActor = FoundByTag;
				}
				else
				{
					UE_LOG(LogSuspenseCorePreviewRotation, Warning, TEXT("[PreviewRotationWidget] No preview actor found by class, name, or tag"));
				}
			}
		}
	}

	GetEventBus();
}

void USuspenseCorePreviewRotationWidget::NativeDestruct()
{
	bIsDragging = false;
	CachedEventBus.Reset();
	CachedPreviewActor.Reset();
	CachedGenericActor.Reset();

	Super::NativeDestruct();
}

FReply USuspenseCorePreviewRotationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bRotationEnabled)
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = true;
		LastMousePosition = InMouseEvent.GetScreenSpacePosition();

		OnDragStarted();
		OnDragStartedDelegate.Broadcast();

		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return FReply::Unhandled();
}

FReply USuspenseCorePreviewRotationWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsDragging)
	{
		bIsDragging = false;

		OnDragEnded();
		OnDragEndedDelegate.Broadcast();

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

		float DeltaYaw = DeltaX * RotationSensitivity;
		if (bInvertRotation)
		{
			DeltaYaw = -DeltaYaw;
		}

		if (!FMath::IsNearlyZero(DeltaYaw))
		{
			ApplyRotationDelta(DeltaYaw);

			AccumulatedYaw += DeltaYaw;
			AccumulatedYaw = FMath::Fmod(AccumulatedYaw, 360.0f);

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
	Super::NativeOnMouseLeave(InMouseEvent);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePreviewRotationWidget::SetPreviewActor(ASuspenseCoreCharacterPreviewActor* InPreviewActor)
{
	CachedPreviewActor = InPreviewActor;
}

void USuspenseCorePreviewRotationWidget::ResetRotation()
{
	SetRotation(0.0f);
}

void USuspenseCorePreviewRotationWidget::SetRotation(float Yaw)
{
	AccumulatedYaw = FMath::Fmod(Yaw, 360.0f);

	if (CachedPreviewActor.IsValid())
	{
		CachedPreviewActor->SetPreviewRotation(Yaw);
	}
	else
	{
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
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePreviewRotationWidget::SetupHitTestArea()
{
	if (HitTestArea || HitTestImage)
	{
		return;
	}

	UE_LOG(LogSuspenseCorePreviewRotation, Warning,
		TEXT("[PreviewRotationWidget] No HitTestArea or HitTestImage bound. Add a Border named 'HitTestArea' in UMG Designer."));
}

ASuspenseCoreCharacterPreviewActor* USuspenseCorePreviewRotationWidget::FindPreviewActorInWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, ASuspenseCoreCharacterPreviewActor::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		return Cast<ASuspenseCoreCharacterPreviewActor>(FoundActors[0]);
	}

	return nullptr;
}

AActor* USuspenseCorePreviewRotationWidget::FindActorByNamePattern(const FString& Pattern)
{
	UWorld* World = GetWorld();
	if (!World || Pattern.IsEmpty())
	{
		return nullptr;
	}

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetName().Contains(Pattern))
		{
			return Actor;
		}
	}

	return nullptr;
}

AActor* USuspenseCorePreviewRotationWidget::FindActorByTag(FName Tag)
{
	UWorld* World = GetWorld();
	if (!World || Tag.IsNone())
	{
		return nullptr;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(World, Tag, FoundActors);

	if (FoundActors.Num() > 0)
	{
		return FoundActors[0];
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
	// Priority 1: SuspenseCoreCharacterPreviewActor
	if (CachedPreviewActor.IsValid())
	{
		CachedPreviewActor->RotatePreview(DeltaYaw);
		return;
	}

	// Priority 2: Generic actor - rotate directly
	if (CachedGenericActor.IsValid())
	{
		FRotator CurrentRotation = CachedGenericActor->GetActorRotation();
		CurrentRotation.Yaw += DeltaYaw * RotationSensitivity;
		CachedGenericActor->SetActorRotation(CurrentRotation);
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
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetFloat(FName("DeltaYaw"), DeltaYaw);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Preview.Rotate")),
		EventData
	);
}
