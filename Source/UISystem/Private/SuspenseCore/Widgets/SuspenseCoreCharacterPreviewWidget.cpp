// SuspenseCoreCharacterPreviewWidget.cpp
// SuspenseCore - Character Preview with Mouse Rotation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreCharacterPreviewWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

USuspenseCoreCharacterPreviewWidget::USuspenseCoreCharacterPreviewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// WIDGET LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupEventSubscriptions();

	if (bAutoEnableCapture)
	{
		EnableCapture();
	}
}

void USuspenseCoreCharacterPreviewWidget::NativeDestruct()
{
	DisableCapture();
	TeardownEventSubscriptions();

	Super::NativeDestruct();
}

// ═══════════════════════════════════════════════════════════════════════════════
// MOUSE INPUT (Rotation Control)
// ═══════════════════════════════════════════════════════════════════════════════

FReply USuspenseCoreCharacterPreviewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = true;
		LastMousePosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(GetCachedWidget().ToSharedRef());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USuspenseCoreCharacterPreviewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsDragging)
	{
		bIsDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USuspenseCoreCharacterPreviewWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging)
	{
		FVector2D CurrentMousePosition = InMouseEvent.GetScreenSpacePosition();
		float DeltaX = CurrentMousePosition.X - LastMousePosition.X;
		LastMousePosition = CurrentMousePosition;

		// Publish rotation delta to character via EventBus
		float DeltaYaw = DeltaX * RotationSensitivity;
		PublishRotationDelta(DeltaYaw);

		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void USuspenseCoreCharacterPreviewWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	// Don't stop dragging on mouse leave - we have capture
	Super::NativeOnMouseLeave(InMouseEvent);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterPreviewWidget::EnableCapture()
{
	if (bCaptureEnabled)
	{
		return;
	}

	bCaptureEnabled = true;
	PublishCaptureRequest(true);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewWidget] Capture enabled"));
}

void USuspenseCoreCharacterPreviewWidget::DisableCapture()
{
	if (!bCaptureEnabled)
	{
		return;
	}

	bCaptureEnabled = false;
	PublishCaptureRequest(false);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewWidget] Capture disabled"));
}

void USuspenseCoreCharacterPreviewWidget::SetRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	if (RenderTarget)
	{
		UpdatePreviewImage(RenderTarget);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterPreviewWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		return;
	}

	// Subscribe to render target ready events
	RenderTargetReadyEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.RenderTargetReady")),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCharacterPreviewWidget::OnRenderTargetReady),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewWidget] EventBus subscriptions established"));
}

void USuspenseCoreCharacterPreviewWidget::TeardownEventSubscriptions()
{
	if (CachedEventBus.IsValid() && RenderTargetReadyEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(RenderTargetReadyEventHandle);
	}
}

void USuspenseCoreCharacterPreviewWidget::UpdatePreviewImage(UTextureRenderTarget2D* RenderTarget)
{
	if (!PreviewImage || !RenderTarget)
	{
		return;
	}

	CachedRenderTarget = RenderTarget;

	// Create or update dynamic material instance
	if (PreviewBaseMaterial)
	{
		if (!PreviewMaterial || PreviewMaterial->Parent != PreviewBaseMaterial)
		{
			PreviewMaterial = UMaterialInstanceDynamic::Create(PreviewBaseMaterial, this);
		}

		if (PreviewMaterial)
		{
			PreviewMaterial->SetTextureParameterValue(FName("RenderTargetTexture"), RenderTarget);
		}
	}

	// Apply to image widget
	FSlateBrush Brush;
	if (PreviewMaterial)
	{
		Brush.SetResourceObject(PreviewMaterial);
	}
	else
	{
		Brush.SetResourceObject(RenderTarget);
	}

	Brush.ImageSize = PreviewImageSize;
	Brush.DrawAs = ESlateBrushDrawType::Image;

	PreviewImage->SetBrush(Brush);
	PreviewImage->SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewWidget] Preview image updated"));
}

void USuspenseCoreCharacterPreviewWidget::PublishRotationDelta(float DeltaYaw)
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager && Manager->GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetFloat(FName("DeltaYaw"), DeltaYaw);
		Manager->GetEventBus()->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterPreview.RequestRotation")),
			EventData
		);
	}
}

void USuspenseCoreCharacterPreviewWidget::PublishCaptureRequest(bool bEnable)
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager && Manager->GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetBool(FName("Enabled"), bEnable);
		Manager->GetEventBus()->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterPreview.RequestCapture")),
			EventData
		);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterPreviewWidget::OnRenderTargetReady(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewWidget] Render target ready event received"));

	UTextureRenderTarget2D* RenderTarget = EventData.GetObject<UTextureRenderTarget2D>(FName("RenderTarget"));
	if (RenderTarget)
	{
		UpdatePreviewImage(RenderTarget);
	}
}
