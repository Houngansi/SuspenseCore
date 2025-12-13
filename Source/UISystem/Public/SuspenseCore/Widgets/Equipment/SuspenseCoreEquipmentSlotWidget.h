// SuspenseCoreEquipmentSlotWidget.h
// SuspenseCore - Equipment Slot Widget for Named Slots
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseSlotWidget.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCoreEquipmentSlotWidget.generated.h"

// Forward declarations
class UImage;

/**
 * USuspenseCoreEquipmentSlotWidget
 *
 * Slot widget for named equipment slots (not grid-based).
 * Used for weapon slots, armor slots, accessory slots, etc.
 *
 * BINDWIDGET COMPONENTS (create in Blueprint with EXACT names):
 * - SlotSizeBox: USizeBox - Controls slot dimensions
 * - BackgroundBorder: UBorder - Background with state-based colors
 * - HighlightBorder: UBorder - Highlight overlay for selection/hover
 * - ItemIcon: UImage - Equipped item icon
 * - StackCountText: UTextBlock - Stack count (rare for equipment)
 * - EmptySlotIcon: UImage - Icon shown when slot is empty (silhouette)
 * - SlotTypeIndicator: UImage - Small icon indicating slot type (optional)
 *
 * KEY DIFFERENCES FROM INVENTORY SLOT:
 * - Identified by SlotTypeTag (FGameplayTag) instead of SlotIndex
 * - Shows EmptySlotIcon silhouette when empty
 * - Validates drops against AllowedItemTypes
 * - No grid position (named slot system)
 *
 * @see USuspenseCoreBaseSlotWidget
 * @see USuspenseCoreEquipmentWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreEquipmentSlotWidget : public USuspenseCoreBaseSlotWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	//==================================================================
	// Equipment Slot Configuration
	//==================================================================

	/**
	 * Initialize slot from equipment slot config
	 * @param InSlotConfig Configuration from FLoadoutConfiguration
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void InitializeFromConfig(const FEquipmentSlotConfig& InSlotConfig);

	/**
	 * Set slot type
	 * @param InSlotType Equipment slot type enum
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void SetSlotType(EEquipmentSlotType InSlotType);

	/**
	 * Get slot type
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	EEquipmentSlotType GetSlotType() const { return SlotType; }

	/**
	 * Set slot type tag
	 * @param InSlotTypeTag GameplayTag identifying slot (Equipment.Slot.*)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void SetSlotTypeTag(const FGameplayTag& InSlotTypeTag) { SlotTypeTag = InSlotTypeTag; }

	/**
	 * Get slot type tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	FGameplayTag GetSlotTypeTag() const { return SlotTypeTag; }

	/**
	 * Set allowed item types for this slot
	 * @param InAllowedTypes Container of allowed item type tags
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void SetAllowedItemTypes(const FGameplayTagContainer& InAllowedTypes) { AllowedItemTypes = InAllowedTypes; }

	/**
	 * Get allowed item types
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	const FGameplayTagContainer& GetAllowedItemTypes() const { return AllowedItemTypes; }

	/**
	 * Check if item type is allowed in this slot
	 * @param ItemTypeTag Item's type tag
	 * @return True if item can be equipped in this slot
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	bool CanAcceptItemType(const FGameplayTag& ItemTypeTag) const;

	/**
	 * Set display name for this slot
	 * @param InDisplayName Localized display name
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void SetDisplayName(const FText& InDisplayName) { DisplayName = InDisplayName; }

	/**
	 * Get display name
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Equipment")
	FText GetDisplayName() const { return DisplayName; }

	//==================================================================
	// Empty Slot Icon
	//==================================================================

	/**
	 * Set empty slot icon (silhouette)
	 * @param InIconPath Path to icon texture
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void SetEmptySlotIconPath(const FSoftObjectPath& InIconPath);

	/**
	 * Set empty slot icon directly
	 * @param InTexture Icon texture
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Equipment")
	void SetEmptySlotIconTexture(UTexture2D* InTexture);

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when slot config is initialized */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Equipment", meta = (DisplayName = "On Config Initialized"))
	void K2_OnConfigInitialized(const FEquipmentSlotConfig& SlotConfig);

	/** Called when item is equipped to this slot */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Equipment", meta = (DisplayName = "On Item Equipped"))
	void K2_OnItemEquipped(const FSuspenseCoreItemUIData& ItemData);

	/** Called when item is unequipped from this slot */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Equipment", meta = (DisplayName = "On Item Unequipped"))
	void K2_OnItemUnequipped();

protected:
	//==================================================================
	// Visual Updates (Overrides)
	//==================================================================

	virtual void UpdateVisuals_Implementation() override;
	virtual void UpdateItemIcon() override;

	/** Update empty slot icon visibility and appearance */
	virtual void UpdateEmptySlotIcon();

	//==================================================================
	// Additional Widget References (Bind in Blueprint)
	//==================================================================

	/** Empty slot icon - shown when no item equipped (silhouette) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UImage> EmptySlotIcon;

	/** Slot type indicator icon (optional, small icon in corner) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UImage> SlotTypeIndicator;

	//==================================================================
	// Configuration
	//==================================================================

	/** Equipment slot type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Slot")
	EEquipmentSlotType SlotType;

	/** Slot type as gameplay tag (Equipment.Slot.*) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Slot", meta = (Categories = "Equipment.Slot"))
	FGameplayTag SlotTypeTag;

	/** Display name for this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Slot")
	FText DisplayName;

	/** Allowed item types for this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Slot", meta = (Categories = "Item"))
	FGameplayTagContainer AllowedItemTypes;

	/** Path to empty slot icon texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Visuals")
	FSoftObjectPath EmptySlotIconPath;

	/** Tint color for empty slot icon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor EmptySlotIconTint;

	/** Path to slot type indicator icon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Visuals")
	FSoftObjectPath SlotTypeIndicatorPath;

private:
	/** Cached empty slot icon texture */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CachedEmptySlotIconTexture;

	/** Track if we had item before (for equipped/unequipped events) */
	bool bHadItemBefore;
};
