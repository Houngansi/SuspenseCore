// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_DELEGATES_SUSPENSECOREEVENTMANAGER_H
#define SUSPENSECORE_DELEGATES_SUSPENSECOREEVENTMANAGER_H

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h" // Используем структуры из единого источника
#include "Engine/Engine.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseEventManager.generated.h"

// Forward declarations
class AActor;
class UObject;
class UUserWidget;
class APlayerState;
class UDataTable;

//================================================
// UI-специфичные структуры для событий
//================================================

/**
 * UI-специфичный запрос операции с экипировкой
 * Используется для передачи упрощенных данных между UI виджетами
 * Для полноценных операций используйте FEquipmentOperationRequest из SuspenseEquipmentTypes.h
 */
USTRUCT(BlueprintType)
struct FUIEquipmentRequest
{
    GENERATED_BODY()
    
    /** Тип операции UI */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FGameplayTag OperationType;
    
    /** Индекс слота в UI */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    int32 SlotIndex = INDEX_NONE;
    
    /** ID предмета для отображения */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FName ItemID;
    
    /** ID экземпляра для UI tracking */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FGuid ItemInstanceID;
    
    /** Виджет-источник события */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    TWeakObjectPtr<UUserWidget> SourceWidget;
    
    FUIEquipmentRequest()
    {
        ItemInstanceID = FGuid();
    }
};

/**
 * UI-специфичный результат операции
 * Упрощенная версия для отображения в интерфейсе
 */
USTRUCT(BlueprintType)
struct FUIEquipmentResult
{
    GENERATED_BODY()
    
    /** Успешность для UI отображения */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    bool bSuccess = false;
    
    /** Сообщение для пользователя */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FText UserMessage;
    
    /** Виджет для обновления */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    TWeakObjectPtr<UUserWidget> TargetWidget;
    
    FUIEquipmentResult() 
    {
        UserMessage = FText::GetEmpty();
    }
};

//================================================
// Native C++ Delegates
//================================================

// UI System Native Delegates
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIWidgetCreatedNative, UUserWidget*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIWidgetDestroyedNative, UUserWidget*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUIVisibilityChangedNative, UUserWidget*, bool);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnHealthUpdatedNative, float, float, float);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnStaminaUpdatedNative, float, float, float);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCrosshairUpdatedNative, float, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrosshairColorChangedNative, FLinearColor);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNotificationNative, FString, float);

// Tooltip System Native Delegates
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTooltipRequested, const FItemUIData&, const FVector2D&);
DECLARE_MULTICAST_DELEGATE(FOnTooltipHideRequested);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTooltipUpdatePosition, const FVector2D&);

// Character Screen and Tab System Native Delegates
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCharacterScreenOpened, UObject*, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharacterScreenClosed, UObject*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTabBarInitialized, UObject*, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUIEventGeneric, UObject*, const FGameplayTag&, const FString&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTabClicked, UObject*, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTabSelectionChanged, UObject*, const FGameplayTag&, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnScreenActivated, UObject*, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnScreenDeactivated, UObject*, const FGameplayTag&);

// Inventory UI Native Delegates
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryUIRefreshRequested, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUIContainerUpdateRequested, UUserWidget*, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUISlotInteraction, UUserWidget*, int32, const FGameplayTag&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUIDragStarted, UUserWidget*, const FDragDropUIData&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUIDragCompleted, UUserWidget*, UUserWidget*, bool);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUIItemDropped, UUserWidget*, const FDragDropUIData&, int32);

// Equipment System Native Delegates - используем правильные типы из SuspenseEquipmentTypes.h
DECLARE_MULTICAST_DELEGATE(FOnEquipmentUpdatedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveWeaponChangedNative, AActor*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEquipmentEventNative, UObject*, FGameplayTag, FString);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEquipmentStateChangedNative, FGameplayTag, FGameplayTag, bool);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEquipmentSlotUpdated, int32, const FGameplayTag&, bool);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnEquipmentDropValidation, const FDragDropUIData&, int32, bool, const FText&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquipmentUIRefreshRequested, UUserWidget*);

// ВАЖНО: Используем структуры из SuspenseEquipmentTypes.h
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquipmentOperationRequest, const FEquipmentOperationRequest&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquipmentOperationCompleted, const FEquipmentOperationResult&);

// Weapon System Native Delegates
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAmmoChangedNative, float, float, float);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWeaponStateChangedNative, FGameplayTag, FGameplayTag, bool);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnWeaponFiredNative, const FVector&, const FVector&, bool, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeaponSpreadUpdatedNative, float);
DECLARE_MULTICAST_DELEGATE(FOnWeaponReloadStartNative);
DECLARE_MULTICAST_DELEGATE(FOnWeaponReloadEndNative);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFireModeChangedNative, FGameplayTag, float);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFireModeProviderChangedNative, FGameplayTag, bool);

// Weapon Switch Native Delegates
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeaponSwitchStarted, int32 /*FromSlot*/, int32 /*ToSlot*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeaponSwitchCompleted, int32 /*FromSlot*/, int32 /*ToSlot*/);

// Movement System Native Delegates
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMovementSpeedChangedNative, float, float, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMovementStateChangedNative, FGameplayTag, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnJumpStateChangedNative, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrouchStateChangedNative, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLandedNative, float);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMovementModeChangedNative, FName, FName, FGameplayTag);

// Loadout System Native Delegates
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLoadoutTableLoaded, UDataTable*, int32);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnLoadoutChanged, const FName&, APlayerState*, bool);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnLoadoutApplied, const FName&, UObject*, const FGameplayTag&, bool);

// Generic Event System
DECLARE_DELEGATE_ThreeParams(FGenericEventDelegate, const UObject*, const FGameplayTag&, const FString&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnGenericEventNative, const UObject*, const FGameplayTag&, const FString&);

/**
 * Centralized event delegate manager for the entire game
 * 
 * ARCHITECTURE PHILOSOPHY:
 * This manager acts as a central event bus for UI and gameplay events.
 * It does NOT contain business logic - only event routing.
 * 
 * KEY PRINCIPLES:
 * 1. Single source of truth for game-wide events
 * 2. No circular dependencies between modules
 * 3. Thread-safe event dispatching
 * 4. Supports both Blueprint and C++ subscribers
 * 
 * USAGE PATTERN:
 * - Publishers: Call NotifyXXX methods to broadcast events
 * - Subscribers: Use SubscribeToXXX methods (C++) or bind to OnXXX delegates (Blueprint)
 * - The manager does NOT process events, only routes them
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseEventManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    //================================================
    // Subsystem Lifecycle
    //================================================
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 
     * Gets the singleton instance of the event manager
     * @param WorldContext - any UObject with valid World context
     * @return pointer to manager or nullptr on error
     */
    UFUNCTION(BlueprintCallable, Category = "Events", meta = (CallInEditor = "true"))
    static USuspenseEventManager* Get(const UObject* WorldContext);

    //================================================
    // UI System Blueprint Delegates
    //================================================
    
    /** Fired when any UI widget is created */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUIWidgetCreatedEvent, UUserWidget*, Widget);
    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnUIWidgetCreatedEvent OnUIWidgetCreated;
    
    /** Fired when any UI widget is destroyed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUIWidgetDestroyedEvent, UUserWidget*, Widget);
    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnUIWidgetDestroyedEvent OnUIWidgetDestroyed;
    
    /** Fired when UI visibility changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIVisibilityChangedEvent, UUserWidget*, Widget, bool, bIsVisible);
    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnUIVisibilityChangedEvent OnUIVisibilityChanged;
    
    /** Fired when health values update */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthUpdatedEvent, float, CurrentHealth, float, MaxHealth, float, HealthPercent);
    UPROPERTY(BlueprintAssignable, Category = "UI|Attributes")
    FOnHealthUpdatedEvent OnHealthUpdated;
    
    /** Fired when stamina values update */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStaminaUpdatedEvent, float, CurrentStamina, float, MaxStamina, float, StaminaPercent);
    UPROPERTY(BlueprintAssignable, Category = "UI|Attributes")
    FOnStaminaUpdatedEvent OnStaminaUpdated;
    
    /** Fired when inventory item is moved between slots */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
       FOnInventoryItemMoved,
       const FGuid&, ItemID,
       int32, FromSlot,
       int32, ToSlot,
       bool, bSuccess
    );
    UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
    FOnInventoryItemMoved OnInventoryItemMoved;
    
    /** Fired when crosshair needs update */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCrosshairUpdatedEvent, float, Spread, float, Recoil);
    UPROPERTY(BlueprintAssignable, Category = "UI|Weapon")
    FOnCrosshairUpdatedEvent OnCrosshairUpdated;
    
    /** Fired when crosshair color changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrosshairColorChangedEvent, FLinearColor, NewColor);
    UPROPERTY(BlueprintAssignable, Category = "UI|Weapon")
    FOnCrosshairColorChangedEvent OnCrosshairColorChanged;
    
    /** Fired for UI notifications */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNotificationEvent, const FString&, Message, float, Duration);
    UPROPERTY(BlueprintAssignable, Category = "UI|System")
    FOnNotificationEvent OnNotification;

    //================================================
    // Inventory UI Blueprint Delegates
    //================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIContainerUpdateRequestedDynamic, UUserWidget*, ContainerWidget, const FGameplayTag&, ContainerType);
    UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
    FOnUIContainerUpdateRequestedDynamic OnUIContainerUpdateRequested;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUISlotInteractionDynamic, UUserWidget*, ContainerWidget, int32, SlotIndex, const FGameplayTag&, InteractionType);
    UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
    FOnUISlotInteractionDynamic OnUISlotInteraction;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIDragStartedDynamic, UUserWidget*, SourceWidget, const FDragDropUIData&, DragData);
    UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
    FOnUIDragStartedDynamic OnUIDragStarted;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUIDragCompletedDynamic, UUserWidget*, SourceWidget, UUserWidget*, TargetWidget, bool, bSuccess);
    UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
    FOnUIDragCompletedDynamic OnUIDragCompleted;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUIItemDroppedDynamic, UUserWidget*, ContainerWidget, const FDragDropUIData&, DragData, int32, TargetSlot);
    UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
    FOnUIItemDroppedDynamic OnUIItemDropped;

    //================================================
    // Tooltip System Blueprint Delegates
    //================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTooltipRequestedDynamic, const FItemUIData&, ItemData, const FVector2D&, ScreenPosition);
    UPROPERTY(BlueprintAssignable, Category = "UI|Tooltip")
    FOnTooltipRequestedDynamic OnTooltipRequested;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTooltipHideRequestedDynamic);
    UPROPERTY(BlueprintAssignable, Category = "UI|Tooltip")
    FOnTooltipHideRequestedDynamic OnTooltipHideRequested;

    //================================================
    // Equipment System Blueprint Delegates
    //================================================

    /** Fired when equipment is updated */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentUpdatedEvent);
    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnEquipmentUpdatedEvent OnEquipmentUpdated;

    /** Fired when active weapon changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveWeaponChangedEvent, AActor*, NewActiveWeapon);
    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnActiveWeaponChangedEvent OnActiveWeaponChanged;

    /** Fired for equipment-related events */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEquipmentEvent, UObject*, Equipment, FGameplayTag, EventTag, FString, EventData);
    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnEquipmentEvent OnEquipmentEvent;

    /** Fired when equipment state changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEquipmentStateChangedEvent, FGameplayTag, OldState, FGameplayTag, NewState, bool, bWasInterrupted);
    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnEquipmentStateChangedEvent OnEquipmentStateChanged;

    /** Fired when equipment slot is updated */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEquipmentSlotUpdatedDynamic, int32, SlotIndex, const FGameplayTag&, SlotType, bool, bIsOccupied);
    UPROPERTY(BlueprintAssignable, Category = "Equipment|UI")
    FOnEquipmentSlotUpdatedDynamic OnEquipmentSlotUpdated;

    /** Fired when equipment UI requests refresh */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentUIRefreshRequestedDynamic, UUserWidget*, Widget);
    UPROPERTY(BlueprintAssignable, Category = "Equipment|UI")
    FOnEquipmentUIRefreshRequestedDynamic OnEquipmentUIRefreshRequested;

    /** Fired when equipment operation is requested - используем структуры из SuspenseEquipmentTypes.h */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentOperationRequestDynamic, const FEquipmentOperationRequest&, Request);
    UPROPERTY(BlueprintAssignable, Category = "Equipment|Operations")
    FOnEquipmentOperationRequestDynamic OnEquipmentOperationRequest;

    /** Fired when equipment operation completes - используем структуры из SuspenseEquipmentTypes.h */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentOperationCompletedDynamic, const FEquipmentOperationResult&, Result);
    UPROPERTY(BlueprintAssignable, Category = "Equipment|Operations")
    FOnEquipmentOperationCompletedDynamic OnEquipmentOperationCompleted;

    //================================================
    // Weapon System Blueprint Delegates
    //================================================

    /** Fired when ammo values change */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAmmoChangedEvent, float, CurrentAmmo, float, RemainingAmmo, float, MagazineSize);
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnAmmoChangedEvent OnAmmoChanged;

    /** Fired when weapon state changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponStateChangedEvent, FGameplayTag, OldState, FGameplayTag, NewState, bool, bWasInterrupted);
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponStateChangedEvent OnWeaponStateChanged;

    /** Fired when weapon is fired */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWeaponFiredEvent, const FVector&, Origin, const FVector&, Impact, bool, bSuccess, FName, ShotType);
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponFiredEvent OnWeaponFired;

    /** Fired when weapon spread updates */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponSpreadUpdatedEvent, float, NewSpread);
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponSpreadUpdatedEvent OnWeaponSpreadUpdated;

    /** Fired when weapon reload starts */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloadStartEvent);
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponReloadStartEvent OnWeaponReloadStart;

    /** Fired when weapon reload ends */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloadEndEvent);
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponReloadEndEvent OnWeaponReloadEnd;

    /** Fired when fire mode changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFireModeChangedEvent, FGameplayTag, NewFireMode, float, CurrentSpread);
    UPROPERTY(BlueprintAssignable, Category = "Weapon|FireMode")
    FOnFireModeChangedEvent OnFireModeChanged;

    /** Fired when fire mode provider changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFireModeProviderChangedEvent, FGameplayTag, FireModeTag, bool, bEnabled);
    UPROPERTY(BlueprintAssignable, Category = "Weapon|FireMode")
    FOnFireModeProviderChangedEvent OnFireModeProviderChanged;

    //================================================
    // Movement System Blueprint Delegates
    //================================================
    
    /** Fired when movement speed changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMovementSpeedChangedEvent, float, OldSpeed, float, NewSpeed, bool, bIsSprinting);
    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnMovementSpeedChangedEvent OnMovementSpeedChanged;
    
    /** Fired when movement state changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementStateChangedEvent, FGameplayTag, NewState, bool, bIsTransitioning);
    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnMovementStateChangedEvent OnMovementStateChanged;
    
    /** Fired when jump state changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJumpStateChangedEvent, bool, bIsJumping);
    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnJumpStateChangedEvent OnJumpStateChanged;
    
    /** Fired when crouch state changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrouchStateChangedEvent, bool, bIsCrouching);
    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnCrouchStateChangedEvent OnCrouchStateChanged;
    
    /** Fired when character lands after jump/fall */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLandedEvent, float, ImpactVelocity);
    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnLandedEvent OnLanded;
    
    /** Fired when movement mode changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMovementModeChangedEvent, FName, PreviousMode, FName, NewMode, FGameplayTag, StateTag);
    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnMovementModeChangedEvent OnMovementModeChanged;

    //================================================
    // Loadout System Blueprint Delegates
    //================================================
    
    /** Fired when loadout table is loaded */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoadoutTableLoadedEvent, UDataTable*, LoadoutTable, int32, LoadedCount);
    UPROPERTY(BlueprintAssignable, Category = "Loadout")
    FOnLoadoutTableLoadedEvent OnLoadoutTableLoaded;
    
    /** Fired when loadout is changed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLoadoutChangedEvent, const FName&, LoadoutID, APlayerState*, PlayerState, bool, bSuccess);
    UPROPERTY(BlueprintAssignable, Category = "Loadout")
    FOnLoadoutChangedEvent OnLoadoutChanged;
    
    /** Fired when loadout is applied to object */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLoadoutAppliedEvent, const FName&, LoadoutID, UObject*, TargetObject, const FGameplayTag&, ComponentType, bool, bSuccess);
    UPROPERTY(BlueprintAssignable, Category = "Loadout")
    FOnLoadoutAppliedEvent OnLoadoutApplied;

    //================================================
    // Native C++ Delegates (Direct Access)
    //================================================
    
    // UI System
    FOnUIWidgetCreatedNative OnUIWidgetCreatedNative;
    FOnUIWidgetDestroyedNative OnUIWidgetDestroyedNative;
    FOnUIVisibilityChangedNative OnUIVisibilityChangedNative;
    FOnHealthUpdatedNative OnHealthUpdatedNative;
    FOnStaminaUpdatedNative OnStaminaUpdatedNative;
    FOnCrosshairUpdatedNative OnCrosshairUpdatedNative;
    FOnCrosshairColorChangedNative OnCrosshairColorChangedNative;
    FOnNotificationNative OnNotificationNative;
    
    // Tooltip System
    FOnTooltipRequested OnTooltipRequestedNative;
    FOnTooltipHideRequested OnTooltipHideRequestedNative;
    FOnTooltipUpdatePosition OnTooltipUpdatePositionNative;

    // Character Screen and Tabs
    FOnCharacterScreenOpened OnCharacterScreenOpenedNative;
    FOnCharacterScreenClosed OnCharacterScreenClosedNative;
    FOnTabBarInitialized OnTabBarInitializedNative;
    FOnUIEventGeneric OnUIEventGenericNative;
    FOnTabClicked OnTabClickedNative;
    FOnTabSelectionChanged OnTabSelectionChangedNative;
    FOnScreenActivated OnScreenActivatedNative;
    FOnScreenDeactivated OnScreenDeactivatedNative;

    // Inventory UI
    FOnInventoryUIRefreshRequested OnInventoryUIRefreshRequestedNative;
    FOnUIContainerUpdateRequested OnUIContainerUpdateRequestedNative;
    FOnUISlotInteraction OnUISlotInteractionNative;
    FOnUIDragStarted OnUIDragStartedNative;
    FOnUIDragCompleted OnUIDragCompletedNative;
    FOnUIItemDropped OnUIItemDroppedNative;

    // Equipment System - используем правильные типы из SuspenseEquipmentTypes.h
    FOnEquipmentUpdatedNative OnEquipmentUpdatedNative;
    FOnActiveWeaponChangedNative OnActiveWeaponChangedNative;
    FOnEquipmentEventNative OnEquipmentEventNative;
    FOnEquipmentStateChangedNative OnEquipmentStateChangedNative;
    FOnEquipmentSlotUpdated OnEquipmentSlotUpdatedNative;
    FOnEquipmentDropValidation OnEquipmentDropValidationNative;
    FOnEquipmentUIRefreshRequested OnEquipmentUIRefreshRequestedNative;
    FOnEquipmentOperationRequest OnEquipmentOperationRequestNative;
    FOnEquipmentOperationCompleted OnEquipmentOperationCompletedNative;

    // Weapon System
    FOnAmmoChangedNative OnAmmoChangedNative;
    FOnWeaponStateChangedNative OnWeaponStateChangedNative;
    FOnWeaponFiredNative OnWeaponFiredNative;
    FOnWeaponSpreadUpdatedNative OnWeaponSpreadUpdatedNative;
    FOnWeaponReloadStartNative OnWeaponReloadStartNative;
    FOnWeaponReloadEndNative OnWeaponReloadEndNative;
    FOnFireModeChangedNative OnFireModeChangedNative;
    FOnFireModeProviderChangedNative OnFireModeProviderChangedNative;
    
    // Weapon Switch Events
    FOnWeaponSwitchStarted OnWeaponSwitchStarted;
    FOnWeaponSwitchCompleted OnWeaponSwitchCompleted;
    
    // Movement System
    FOnMovementSpeedChangedNative OnMovementSpeedChangedNative;
    FOnMovementStateChangedNative OnMovementStateChangedNative;
    FOnJumpStateChangedNative OnJumpStateChangedNative;
    FOnCrouchStateChangedNative OnCrouchStateChangedNative;
    FOnLandedNative OnLandedNative;
    FOnMovementModeChangedNative OnMovementModeChangedNative;

    // Loadout System
    FOnLoadoutTableLoaded OnLoadoutTableLoadedNative;
    FOnLoadoutChanged OnLoadoutChangedNative;
    FOnLoadoutApplied OnLoadoutAppliedNative;

    // Generic Event System
    FOnGenericEventNative OnGenericEventNative;

    //================================================
    // Notification Methods
    //================================================
    
    // UI System
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyUIWidgetCreated(UUserWidget* Widget);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyUIWidgetDestroyed(UUserWidget* Widget);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyUIVisibilityChanged(UUserWidget* Widget, bool bIsVisible);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyHealthUpdated(float CurrentHealth, float MaxHealth, float HealthPercent);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyStaminaUpdated(float CurrentStamina, float MaxStamina, float StaminaPercent);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyCrosshairUpdated(float Spread, float Recoil);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyCrosshairColorChanged(const FLinearColor& NewColor);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyUI(const FString& Message, float Duration = 3.0f);

    // Character Screen and Tabs
    UFUNCTION(BlueprintCallable, Category = "UI|Tabs")
    void NotifyCharacterScreenOpened(UObject* Screen, const FGameplayTag& DefaultTab);

    UFUNCTION(BlueprintCallable, Category = "UI|Tabs")
    void NotifyCharacterScreenClosed(UObject* Screen);

    UFUNCTION(BlueprintCallable, Category = "UI|Tabs")
    void NotifyTabBarInitialized(UObject* TabBar, const FGameplayTag& TabBarTag);

    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyUIEventGeneric(UObject* Source, const FGameplayTag& EventTag, const FString& EventData);

    UFUNCTION(BlueprintCallable, Category = "UI|Tabs")
    void NotifyTabClicked(UObject* TabWidget, const FGameplayTag& TabTag);

    UFUNCTION(BlueprintCallable, Category = "UI|Tabs")
    void NotifyTabSelectionChanged(UObject* TabController, const FGameplayTag& OldTab, const FGameplayTag& NewTab);

    UFUNCTION(BlueprintCallable, Category = "UI|Tabs")
    void NotifyScreenActivated(UObject* Screen, const FGameplayTag& ScreenTag);

    UFUNCTION(BlueprintCallable, Category = "UI|Tabs")
    void NotifyScreenDeactivated(UObject* Screen, const FGameplayTag& ScreenTag);

    // Inventory UI
    UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
    void NotifyInventoryUIRefreshRequested(const FGameplayTag& ContainerTag);
    
    UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
    void NotifyUIContainerUpdateRequested(UUserWidget* ContainerWidget, const FGameplayTag& ContainerType);

    UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
    void NotifyUISlotInteraction(UUserWidget* ContainerWidget, int32 SlotIndex, const FGameplayTag& InteractionType);

    UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
    void NotifyUIDragStarted(UUserWidget* SourceWidget, const FDragDropUIData& DragData);

    UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
    void NotifyUIDragCompleted(UUserWidget* SourceWidget, UUserWidget* TargetWidget, bool bSuccess);

    UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
    void NotifyUIItemDropped(UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot);

    UFUNCTION(BlueprintCallable, Category = "UI|Events")
    void NotifyUIEvent(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData);

    // Tooltip System
    UFUNCTION(BlueprintCallable, Category = "UI|Tooltip")
    void NotifyTooltipRequested(const FItemUIData& ItemData, const FVector2D& ScreenPosition);

    UFUNCTION(BlueprintCallable, Category = "UI|Tooltip")
    void NotifyTooltipHideRequested();

    UFUNCTION(BlueprintCallable, Category = "UI|Tooltip")
    void NotifyTooltipUpdatePosition(const FVector2D& ScreenPosition);
    
    // Equipment System
    UFUNCTION(BlueprintCallable, Category = "Equipment|Events")
    void NotifyEquipmentUpdated();

    UFUNCTION(BlueprintCallable, Category = "Equipment|Events")
    void NotifyActiveWeaponChanged(AActor* NewActiveWeapon);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Events")
    void NotifyEquipmentEvent(UObject* Equipment, FGameplayTag EventTag, const FString& EventData);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Events")
    void NotifyEquipmentStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bWasInterrupted = false);

    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void NotifyEquipmentSlotUpdated(int32 SlotIndex, const FGameplayTag& SlotType, bool bIsOccupied);

    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void NotifyEquipmentDropValidation(const FDragDropUIData& DragData, int32 TargetSlot, bool bIsValid, const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "Equipment|UI")
    void NotifyEquipmentUIRefreshRequested(UUserWidget* Widget);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    void BroadcastEquipmentOperationRequest(const FEquipmentOperationRequest& Request);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    void BroadcastEquipmentOperationCompleted(const FEquipmentOperationResult& Result);

    // Weapon System
    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyWeaponStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bWasInterrupted = false);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyWeaponFired(const FVector& Origin, const FVector& Impact, bool bSuccess, FName ShotType);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyWeaponSpreadUpdated(float NewSpread);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyWeaponReloadStart();

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyWeaponReloadEnd();

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyFireModeChanged(const FGameplayTag& NewFireMode, float CurrentSpread);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void NotifyFireModeProviderChanged(const FGameplayTag& FireModeTag, bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void BroadcastWeaponSwitchStarted(int32 FromSlot, int32 ToSlot);
    
    UFUNCTION(BlueprintCallable, Category = "Weapon|Events")
    void BroadcastWeaponSwitchCompleted(int32 FromSlot, int32 ToSlot);
    
    // Movement System
    UFUNCTION(BlueprintCallable, Category = "Movement|Events")
    void NotifyMovementSpeedChanged(float OldSpeed, float NewSpeed, bool bIsSprinting);
    
    UFUNCTION(BlueprintCallable, Category = "Movement|Events")
    void NotifyMovementStateChanged(FGameplayTag NewState, bool bIsTransitioning);
    
    UFUNCTION(BlueprintCallable, Category = "Movement|Events")
    void NotifyJumpStateChanged(bool bIsJumping);
    
    UFUNCTION(BlueprintCallable, Category = "Movement|Events")
    void NotifyCrouchStateChanged(bool bIsCrouching);
    
    UFUNCTION(BlueprintCallable, Category = "Movement|Events")
    void NotifyLanded(float ImpactVelocity);
    
    UFUNCTION(BlueprintCallable, Category = "Movement|Events")
    void NotifyMovementModeChanged(FName PreviousMode, FName NewMode, FGameplayTag StateTag);

    // Loadout System
    UFUNCTION(BlueprintCallable, Category = "Loadout|Events")
    void NotifyLoadoutTableLoaded(UDataTable* LoadoutTable, int32 LoadedCount);
    
    UFUNCTION(BlueprintCallable, Category = "Loadout|Events")
    void NotifyLoadoutChanged(const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess);
    
    UFUNCTION(BlueprintCallable, Category = "Loadout|Events")
    void NotifyLoadoutApplied(const FName& LoadoutID, UObject* TargetObject, const FGameplayTag& ComponentType, bool bSuccess);
    
    // Generic Event System
    UFUNCTION(BlueprintCallable, Category = "Events|Generic")
    void BroadcastGenericEvent(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData);

    //================================================
    // C++ Template Subscription Methods
    //================================================
    
    // UI System subscriptions
    template<typename LambdaType>
    FDelegateHandle SubscribeToUIWidgetCreated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnUIWidgetCreatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToUIWidgetDestroyed(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnUIWidgetDestroyedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToUIVisibilityChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnUIVisibilityChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToHealthUpdated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnHealthUpdatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToStaminaUpdated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnStaminaUpdatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToCrosshairUpdated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnCrosshairUpdatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToCrosshairColorChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnCrosshairColorChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToNotification(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnNotificationNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    // Inventory UI subscriptions
    template<typename LambdaType>
    FDelegateHandle SubscribeToUIItemDropped(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnUIItemDroppedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    // Equipment System subscriptions
    template<typename LambdaType>
    FDelegateHandle SubscribeToEquipmentUpdated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnEquipmentUpdatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToActiveWeaponChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnActiveWeaponChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToEquipmentEvent(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnEquipmentEventNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToEquipmentStateChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnEquipmentStateChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToEquipmentOperationRequest(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnEquipmentOperationRequestNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToEquipmentOperationCompleted(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnEquipmentOperationCompletedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    // Weapon System subscriptions
    template<typename LambdaType>
    FDelegateHandle SubscribeToAmmoChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnAmmoChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToWeaponStateChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnWeaponStateChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToWeaponFired(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnWeaponFiredNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToWeaponSpreadUpdated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnWeaponSpreadUpdatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToWeaponReloadStart(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnWeaponReloadStartNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToWeaponReloadEnd(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnWeaponReloadEndNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToFireModeChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnFireModeChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToFireModeProviderChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnFireModeProviderChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    // Movement System subscriptions
    template<typename LambdaType>
    FDelegateHandle SubscribeToMovementSpeedChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnMovementSpeedChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToMovementStateChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnMovementStateChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToJumpStateChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnJumpStateChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToCrouchStateChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnCrouchStateChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToLanded(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnLandedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToMovementModeChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnMovementModeChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    // Loadout System subscriptions
    template<typename LambdaType>
    FDelegateHandle SubscribeToLoadoutTableLoaded(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnLoadoutTableLoadedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToLoadoutChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnLoadoutChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToLoadoutApplied(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnLoadoutAppliedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    // Generic Event subscriptions
    FDelegateHandle SubscribeToGenericEvent(const FGameplayTag& EventTag, const FGenericEventDelegate& Delegate);
    
    template<typename LambdaType>
    FDelegateHandle SubscribeToGenericEventLambda(const FGameplayTag& EventTag, LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
    
        FGenericEventDelegate Delegate;
        Delegate.BindLambda(Forward<LambdaType>(Lambda));
    
        return SubscribeToGenericEvent(EventTag, Delegate);
    }

    // Additional subscription helpers for Character Screen and Tabs
    template<typename LambdaType>
    FDelegateHandle SubscribeToCharacterScreenOpened(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnCharacterScreenOpenedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToCharacterScreenClosed(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnCharacterScreenClosedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToUIEvent(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnUIEventGenericNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToTabClicked(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnTabClickedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToTabSelectionChanged(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnTabSelectionChangedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToScreenActivated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnScreenActivatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    template<typename LambdaType>
    FDelegateHandle SubscribeToScreenDeactivated(LambdaType&& Lambda)
    {
        if (!ValidateSystemState()) return FDelegateHandle();
        return OnScreenDeactivatedNative.AddLambda(Forward<LambdaType>(Lambda));
    }

    //================================================
    // UObject-based Subscription Methods
    //================================================

    template<typename UserClass>
    FDelegateHandle SubscribeToAmmoChangedUObject(UserClass* Object, void (UserClass::*Method)(float, float, float))
    {
        if (!ValidateSystemState() || !Object) return FDelegateHandle();
        return OnAmmoChangedNative.AddUObject(Object, Method);
    }

    template<typename UserClass>
    FDelegateHandle SubscribeToFireModeChangedUObject(UserClass* Object, void (UserClass::*Method)(FGameplayTag, float))
    {
        if (!ValidateSystemState() || !Object) return FDelegateHandle();
        return OnFireModeChangedNative.AddUObject(Object, Method);
    }

    template<typename UserClass>
    FDelegateHandle SubscribeToEquipmentUpdatedUObject(UserClass* Object, void (UserClass::*Method)())
    {
        if (!ValidateSystemState() || !Object) return FDelegateHandle();
        return OnEquipmentUpdatedNative.AddUObject(Object, Method);
    }

    //================================================
    // Unsubscription Methods
    //================================================

    void UnsubscribeFromNativeEvent(const FString& EventType, const FDelegateHandle& Handle);
    void UniversalUnsubscribe(const FDelegateHandle& Handle);
    void UnsubscribeFromGenericEvent(const FDelegateHandle& Handle);

    //================================================
    // Debug and Utility
    //================================================

    UFUNCTION(BlueprintCallable, Category = "Events|Debug")
    void LogSubscriptionStatus() const;

    UFUNCTION(BlueprintCallable, Category = "Events|Debug")
    void ClearAllSubscriptions();

    UFUNCTION(BlueprintCallable, Category = "Events|Debug")
    bool IsSystemReady() const { return bIsInitialized; }

    UFUNCTION(BlueprintCallable, Category = "Events|Debug")
    int32 GetEventCount() const { return EventCounter; }

    UFUNCTION(BlueprintCallable, Category = "Events|Debug")
    int32 GetNativeSubscriberCount() const;

protected:
    bool bIsInitialized = false;

    UPROPERTY(VisibleAnywhere, Category = "Debug")
    int32 EventCounter = 0;

    bool ValidateSystemState() const;

    // Map для хранения подписчиков по тегам
    TMap<FGameplayTag, TArray<TPair<FDelegateHandle, FGenericEventDelegate>>> GenericEventSubscribers;
    
    // Счетчик для уникальных handle
    int32 GenericEventHandleCounter = 0;
};

#endif // SUSPENSECORE_DELEGATES_SUSPENSECOREEVENTMANAGER_H