// SuspenseCoreBaseContainerWidget.h
// SuspenseCore - Base Container Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreBaseContainerWidget.generated.h"

// Forward declarations
class ISuspenseCoreUIDataProvider;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreBaseContainerWidget
 *
 * Base class for all container widgets (inventory, equipment, stash, etc.)
 * Implements ISuspenseCoreUIContainer for standardized provider binding.
 *
 * ARCHITECTURE:
 * - Binds to ISuspenseCoreUIDataProvider for data
 * - Receives updates via OnUIDataChanged delegate
 * - Routes user actions through provider
 * - Does NOT know about specific systems (inventory, equipment)
 *
 * INHERITANCE:
 * - USuspenseCoreInventoryWidget (grid-based)
 * - USuspenseCoreEquipmentWidget (named slots) [future]
 *
 * @see ISuspenseCoreUIContainer
 * @see ISuspenseCoreUIDataProvider
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreBaseContainerWidget : public UUserWidget, public ISuspenseCoreUIContainer
{
	GENERATED_BODY()

public:
	USuspenseCoreBaseContainerWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//==================================================================
	// ISuspenseCoreUIContainer - Provider Binding
	//==================================================================

	virtual void BindToProvider(TScriptInterface<ISuspenseCoreUIDataProvider> Provider) override;
	virtual void UnbindFromProvider() override;
	virtual bool IsBoundToProvider() const override;
	virtual TScriptInterface<ISuspenseCoreUIDataProvider> GetBoundProvider() const override;

	//==================================================================
	// ISuspenseCoreUIContainer - Refresh
	//==================================================================

	virtual void RefreshFromProvider() override;
	virtual void RefreshSlot(int32 SlotIndex) override;
	virtual void RefreshItem(const FGuid& InstanceID) override;

	//==================================================================
	// ISuspenseCoreUIContainer - Slot Access
	//==================================================================

	virtual UWidget* GetSlotWidget(int32 SlotIndex) const override;
	virtual TArray<UWidget*> GetAllSlotWidgets() const override;
	virtual int32 GetSlotAtPosition(const FVector2D& ScreenPosition) const override;
	virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPosition) const override;

	//==================================================================
	// ISuspenseCoreUIContainer - Selection
	//==================================================================

	virtual void SetSelectedSlot(int32 SlotIndex) override;
	virtual int32 GetSelectedSlot() const override;
	virtual void ClearSelection() override;

	//==================================================================
	// ISuspenseCoreUIContainer - Highlighting
	//==================================================================

	virtual void SetSlotHighlight(int32 SlotIndex, ESuspenseCoreUISlotState State) override;
	virtual void HighlightDropTarget(const FSuspenseCoreDragData& DragData, int32 HoverSlot) override;
	virtual void ClearHighlights() override;

	//==================================================================
	// ISuspenseCoreUIContainer - Drag-Drop
	//==================================================================

	virtual bool AcceptsDrop() const override;
	virtual bool StartDragFromSlot(int32 SlotIndex, bool bSplitStack = false) override;
	virtual bool HandleDrop(const FSuspenseCoreDragData& DragData, int32 TargetSlot) override;
	virtual void HandleDragCancelled() override;

	//==================================================================
	// ISuspenseCoreUIContainer - Configuration
	//==================================================================

	virtual ESuspenseCoreContainerType GetContainerType() const override;
	virtual FGameplayTag GetContainerTypeTag() const override;
	virtual bool IsReadOnly() const override;
	virtual void SetReadOnly(bool bReadOnly) override;

	//==================================================================
	// ISuspenseCoreUIContainer - Events
	//==================================================================

	virtual FOnSuspenseCoreContainerEvent& OnContainerEvent() override { return ContainerEventDelegate; }

	//==================================================================
	// ISuspenseCoreUIContainer - Tooltip
	//==================================================================

	virtual void ShowSlotTooltip(int32 SlotIndex) override;
	virtual void HideTooltip() override;

	//==================================================================
	// ISuspenseCoreUIContainer - Context Menu
	//==================================================================

	virtual void ShowContextMenu(int32 SlotIndex, const FVector2D& ScreenPosition) override;
	virtual void HideContextMenu() override;

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when provider is bound */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Container", meta = (DisplayName = "On Provider Bound"))
	void K2_OnProviderBound();

	/** Called when provider is unbound */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Container", meta = (DisplayName = "On Provider Unbound"))
	void K2_OnProviderUnbound();

	/** Called when container data needs refresh */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Container", meta = (DisplayName = "On Refresh"))
	void K2_OnRefresh();

	/** Called when slot is selected */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Container", meta = (DisplayName = "On Slot Selected"))
	void K2_OnSlotSelected(int32 SlotIndex);

	/** Called when drop is received */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Container", meta = (DisplayName = "On Drop Received"))
	void K2_OnDropReceived(const FSuspenseCoreDragData& DragData, int32 TargetSlot, bool bSuccess);

protected:
	//==================================================================
	// Provider Event Handlers
	//==================================================================

	/** Handle provider data changed */
	UFUNCTION()
	void OnProviderDataChanged(const FGameplayTag& ChangeType, const FGuid& AffectedItemID);

	//==================================================================
	// Protected Accessors
	//==================================================================

	/** Get cached container data */
	const FSuspenseCoreContainerUIData& GetContainerData() const { return CachedContainerData; }

	/** Get EventBus */
	USuspenseCoreEventBus* GetEventBus() const;

	//==================================================================
	// Override Points
	//==================================================================

	/** Called to create slot widgets - override in subclasses */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Container")
	void CreateSlotWidgets();
	virtual void CreateSlotWidgets_Implementation() {}

	/** Called to update slot widget - override in subclasses */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Container")
	void UpdateSlotWidget(int32 SlotIndex, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData);
	virtual void UpdateSlotWidget_Implementation(int32 SlotIndex, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData) {}

	/** Called to clear all slot widgets - override in subclasses */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Container")
	void ClearSlotWidgets();
	virtual void ClearSlotWidgets_Implementation() {}

private:
	//==================================================================
	// Provider Binding
	//==================================================================

	/** Current bound provider */
	UPROPERTY(Transient)
	TScriptInterface<ISuspenseCoreUIDataProvider> BoundProvider;

	/** Cached container data */
	FSuspenseCoreContainerUIData CachedContainerData;

	//==================================================================
	// State
	//==================================================================

	/** Currently selected slot */
	int32 SelectedSlotIndex;

	/** Is container read-only */
	bool bIsReadOnly;

	/** Container event delegate */
	FOnSuspenseCoreContainerEvent ContainerEventDelegate;

	/** Cached EventBus */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
