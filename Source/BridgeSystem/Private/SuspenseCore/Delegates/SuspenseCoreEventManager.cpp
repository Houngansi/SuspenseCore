// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Delegates/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"

void USuspenseEventManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Initialize the hybrid delegate system
    bIsInitialized = true;
    EventCounter = 0;
    
    UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Hybrid delegate system successfully initialized"));
}

void USuspenseEventManager::Deinitialize()
{
    // Clear all subscriptions before destruction to prevent memory leaks
    ClearAllSubscriptions();
    bIsInitialized = false;
    
    UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: System deinitialized. Events processed: %d"), EventCounter);
    Super::Deinitialize();
}

USuspenseEventManager* USuspenseEventManager::Get(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Error, TEXT("EventDelegateManager::Get: WorldContext is null"));
        return nullptr;
    }

    const UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("EventDelegateManager::Get: Cannot get World from context"));
        return nullptr;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("EventDelegateManager::Get: GameInstance not found"));
        return nullptr;
    }

    USuspenseEventManager* Manager = GameInstance->GetSubsystem<USuspenseEventManager>();
    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("EventDelegateManager::Get: Subsystem not registered"));
        return nullptr;
    }

    if (!Manager->bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager::Get: System not yet initialized"));
    }

    return Manager;
}

//================================================
// UI Notification Methods Implementation
//================================================

void USuspenseEventManager::NotifyUIWidgetCreated(UUserWidget* Widget)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnUIWidgetCreated.Broadcast(Widget);
    OnUIWidgetCreatedNative.Broadcast(Widget);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI Widget created: %s"), 
           Widget ? *Widget->GetClass()->GetName() : TEXT("None"));
}

void USuspenseEventManager::NotifyUIWidgetDestroyed(UUserWidget* Widget)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnUIWidgetDestroyed.Broadcast(Widget);
    OnUIWidgetDestroyedNative.Broadcast(Widget);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI Widget destroyed: %s"), 
           Widget ? *Widget->GetClass()->GetName() : TEXT("None"));
}

void USuspenseEventManager::NotifyUIVisibilityChanged(UUserWidget* Widget, bool bIsVisible)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnUIVisibilityChanged.Broadcast(Widget, bIsVisible);
    OnUIVisibilityChangedNative.Broadcast(Widget, bIsVisible);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI visibility changed: %s - %s"), 
           Widget ? *Widget->GetClass()->GetName() : TEXT("None"),
           bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void USuspenseEventManager::NotifyHealthUpdated(float CurrentHealth, float MaxHealth, float HealthPercent)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnHealthUpdated.Broadcast(CurrentHealth, MaxHealth, HealthPercent);
    OnHealthUpdatedNative.Broadcast(CurrentHealth, MaxHealth, HealthPercent);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("EventDelegateManager: Health updated: %.1f/%.1f (%.1f%%)"), 
           CurrentHealth, MaxHealth, HealthPercent * 100.0f);
}

void USuspenseEventManager::NotifyStaminaUpdated(float CurrentStamina, float MaxStamina, float StaminaPercent)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnStaminaUpdated.Broadcast(CurrentStamina, MaxStamina, StaminaPercent);
    OnStaminaUpdatedNative.Broadcast(CurrentStamina, MaxStamina, StaminaPercent);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("EventDelegateManager: Stamina updated: %.1f/%.1f (%.1f%%)"), 
           CurrentStamina, MaxStamina, StaminaPercent * 100.0f);
}

void USuspenseEventManager::NotifyCrosshairUpdated(float Spread, float Recoil)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnCrosshairUpdated.Broadcast(Spread, Recoil);
    OnCrosshairUpdatedNative.Broadcast(Spread, Recoil);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("EventDelegateManager: Crosshair updated: Spread=%.2f, Recoil=%.2f"), 
           Spread, Recoil);
}

void USuspenseEventManager::NotifyCrosshairColorChanged(const FLinearColor& NewColor)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnCrosshairColorChanged.Broadcast(NewColor);
    OnCrosshairColorChangedNative.Broadcast(NewColor);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Crosshair color changed to R=%.2f, G=%.2f, B=%.2f, A=%.2f"), 
           NewColor.R, NewColor.G, NewColor.B, NewColor.A);
}

void USuspenseEventManager::NotifyUI(const FString& Message, float Duration)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnNotification.Broadcast(Message, Duration);
    OnNotificationNative.Broadcast(Message, Duration);
    
    UE_LOG(LogTemp, Log, TEXT("EventDelegateManager: UI Notification: %s (Duration: %.1fs)"), 
           *Message, Duration);
}

void USuspenseEventManager::NotifyCharacterScreenOpened(UObject* Screen, const FGameplayTag& DefaultTab)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnCharacterScreenOpenedNative.Broadcast(Screen, DefaultTab);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Character screen opened with default tab: %s"), 
           *DefaultTab.ToString());
}

void USuspenseEventManager::NotifyCharacterScreenClosed(UObject* Screen)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnCharacterScreenClosedNative.Broadcast(Screen);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Character screen closed"));
}

void USuspenseEventManager::NotifyTabBarInitialized(UObject* TabBar, const FGameplayTag& TabBarTag)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnTabBarInitializedNative.Broadcast(TabBar, TabBarTag);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Tab bar initialized: %s"), 
           *TabBarTag.ToString());
}
void USuspenseEventManager::NotifyUIEventGeneric(UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to native delegates
    OnUIEventGenericNative.Broadcast(Source, EventTag, EventData);
    
    // Log the event
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Generic UI event: %s - %s"), 
           *EventTag.ToString(), *EventData);
}

void USuspenseEventManager::NotifyTabClicked(UObject* TabWidget, const FGameplayTag& TabTag)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to native delegates
    OnTabClickedNative.Broadcast(TabWidget, TabTag);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Tab clicked: %s"), 
           *TabTag.ToString());
}

void USuspenseEventManager::NotifyTabSelectionChanged(UObject* TabController, const FGameplayTag& OldTab, const FGameplayTag& NewTab)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to native delegates
    OnTabSelectionChangedNative.Broadcast(TabController, OldTab, NewTab);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Tab selection changed from %s to %s"), 
           *OldTab.ToString(), *NewTab.ToString());
}

void USuspenseEventManager::NotifyScreenActivated(UObject* Screen, const FGameplayTag& ScreenTag)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to native delegates
    OnScreenActivatedNative.Broadcast(Screen, ScreenTag);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Screen activated: %s"), 
           *ScreenTag.ToString());
}

void USuspenseEventManager::NotifyScreenDeactivated(UObject* Screen, const FGameplayTag& ScreenTag)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to native delegates
    OnScreenDeactivatedNative.Broadcast(Screen, ScreenTag);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Screen deactivated: %s"), 
           *ScreenTag.ToString());
}

void USuspenseEventManager::NotifyInventoryUIRefreshRequested(const FGameplayTag& ContainerTag)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnInventoryUIRefreshRequestedNative.Broadcast(ContainerTag);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Inventory UI refresh requested for: %s"), 
           *ContainerTag.ToString());
}

// inventory notify

void USuspenseEventManager::NotifyUIContainerUpdateRequested(UUserWidget* ContainerWidget, const FGameplayTag& ContainerType)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnUIContainerUpdateRequested.Broadcast(ContainerWidget, ContainerType);
    OnUIContainerUpdateRequestedNative.Broadcast(ContainerWidget, ContainerType);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI container update requested: %s"), 
           *ContainerType.ToString());
}

void USuspenseEventManager::NotifyUISlotInteraction(UUserWidget* ContainerWidget, int32 SlotIndex, const FGameplayTag& InteractionType)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnUISlotInteraction.Broadcast(ContainerWidget, SlotIndex, InteractionType);
    OnUISlotInteractionNative.Broadcast(ContainerWidget, SlotIndex, InteractionType);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI slot interaction: Slot %d, Type %s"), 
           SlotIndex, *InteractionType.ToString());
}

void USuspenseEventManager::NotifyUIDragStarted(UUserWidget* SourceWidget, const FDragDropUIData& DragData)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnUIDragStarted.Broadcast(SourceWidget, DragData);
    OnUIDragStartedNative.Broadcast(SourceWidget, DragData);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI drag started: Item %s"), 
           *DragData.ItemData.ItemID.ToString());
}

void USuspenseEventManager::NotifyUIDragCompleted(UUserWidget* SourceWidget, UUserWidget* TargetWidget, bool bSuccess)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnUIDragCompleted.Broadcast(SourceWidget, TargetWidget, bSuccess);
    OnUIDragCompletedNative.Broadcast(SourceWidget, TargetWidget, bSuccess);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI drag completed: Success = %s"), 
           bSuccess ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyUIEvent(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // This is a generic UI event notification that can be expanded later
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: UI event: %s - %s"), 
           *EventTag.ToString(), *EventData);
}

//================================================
// Equipment Notification Methods Implementation
//================================================

void USuspenseEventManager::NotifyEquipmentUpdated()
{
    if (!ValidateSystemState()) return;

    EventCounter++;
    
    OnEquipmentUpdated.Broadcast();
    OnEquipmentUpdatedNative.Broadcast();
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Equipment updated"));
}

void USuspenseEventManager::NotifyActiveWeaponChanged(AActor* NewActiveWeapon)
{
    if (!ValidateSystemState()) return;

    EventCounter++;
    
    OnActiveWeaponChanged.Broadcast(NewActiveWeapon);
    OnActiveWeaponChangedNative.Broadcast(NewActiveWeapon);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Active weapon changed: %s"), 
           NewActiveWeapon ? *NewActiveWeapon->GetName() : TEXT("None"));
}

void USuspenseEventManager::NotifyEquipmentEvent(UObject* Equipment, FGameplayTag EventTag, const FString& EventData)
{
    if (!ValidateSystemState()) return;

    EventCounter++;
    
    OnEquipmentEvent.Broadcast(Equipment, EventTag, EventData);
    OnEquipmentEventNative.Broadcast(Equipment, EventTag, EventData);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Equipment event: %s on %s"), 
           *EventTag.ToString(), Equipment ? *Equipment->GetName() : TEXT("None"));
}

void USuspenseEventManager::NotifyEquipmentStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bWasInterrupted)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnEquipmentStateChanged.Broadcast(OldState, NewState, bWasInterrupted);
    OnEquipmentStateChangedNative.Broadcast(OldState, NewState, bWasInterrupted);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Equipment state changed: %s -> %s (interrupted: %s)"), 
           *OldState.ToString(), *NewState.ToString(), bWasInterrupted ? TEXT("Yes") : TEXT("No"));
}

//================================================
// Equipment Operation Methods Implementation
//================================================

void USuspenseEventManager::BroadcastEquipmentOperationRequest(const FEquipmentOperationRequest& Request)
{
    if (!ValidateSystemState()) return;

    EventCounter++;

    // Рассылаем в BP и native-подписчиков
    OnEquipmentOperationRequest.Broadcast(Request);
    OnEquipmentOperationRequestNative.Broadcast(Request);

    // Логи: используем поля, которые реально есть в FEquipmentOperationRequest
    const FString OpStr  = StaticEnum<EEquipmentOperationType>()->GetValueAsString(Request.OperationType);
    const int32   Slot   = Request.TargetSlotIndex;
    const FName   ItemId = Request.ItemInstance.ItemID;

    UE_LOG(LogTemp, Log,
        TEXT("EventDelegateManager: Equipment operation requested - Type: %s, TargetSlot: %d, Item: %s, OpId: %s"),
        *OpStr,
        Slot,
        *ItemId.ToString(),
        *Request.OperationId.ToString());
}

void USuspenseEventManager::BroadcastEquipmentOperationCompleted(const FEquipmentOperationResult& Result)
{
    if (!ValidateSystemState()) return;

    EventCounter++;

    // Рассылаем в BP и native-подписчиков
    OnEquipmentOperationCompleted.Broadcast(Result);
    OnEquipmentOperationCompletedNative.Broadcast(Result);

    // Собираем список задетых слотов (в Result нет поля SlotIndex/OperationType)
    FString SlotsStr = TEXT("-");
    if (Result.AffectedSlots.Num() > 0)
    {
        TArray<FString> SlotStrings;
        SlotStrings.Reserve(Result.AffectedSlots.Num());
        for (int32 S : Result.AffectedSlots)
        {
            SlotStrings.Add(FString::FromInt(S));
        }
        SlotsStr = FString::Join(SlotStrings, TEXT(","));
    }

    UE_LOG(LogTemp, Log,
        TEXT("EventDelegateManager: Equipment operation completed - OpId: %s, Success: %s, AffectedSlots: %s"),
        *Result.OperationId.ToString(),
        Result.bSuccess ? TEXT("Yes") : TEXT("No"),
        *SlotsStr);

    if (!Result.bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Operation error - %s"),
            *Result.ErrorMessage.ToString());
    }
}

void USuspenseEventManager::NotifyAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize)
{
    if (!ValidateSystemState()) return;

    EventCounter++;
    
    OnAmmoChanged.Broadcast(CurrentAmmo, RemainingAmmo, MagazineSize);
    OnAmmoChangedNative.Broadcast(CurrentAmmo, RemainingAmmo, MagazineSize);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("EventDelegateManager: Ammo changed - Current: %.1f, Remaining: %.1f, Magazine: %.1f"), 
           CurrentAmmo, RemainingAmmo, MagazineSize);
}

void USuspenseEventManager::NotifyWeaponStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bWasInterrupted)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnWeaponStateChanged.Broadcast(OldState, NewState, bWasInterrupted);
    OnWeaponStateChangedNative.Broadcast(OldState, NewState, bWasInterrupted);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Weapon state changed: %s -> %s (interrupted: %s)"), 
           *OldState.ToString(), *NewState.ToString(), bWasInterrupted ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyWeaponFired(const FVector& Origin, const FVector& Impact, bool bSuccess, FName ShotType)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnWeaponFired.Broadcast(Origin, Impact, bSuccess, ShotType);
    OnWeaponFiredNative.Broadcast(Origin, Impact, bSuccess, ShotType);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("EventDelegateManager: Weapon fired (type: %s, success: %s)"), 
           *ShotType.ToString(), bSuccess ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyWeaponSpreadUpdated(float NewSpread)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnWeaponSpreadUpdated.Broadcast(NewSpread);
    OnWeaponSpreadUpdatedNative.Broadcast(NewSpread);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("EventDelegateManager: Weapon spread updated: %.2f"), NewSpread);
}

void USuspenseEventManager::NotifyWeaponReloadStart()
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnWeaponReloadStart.Broadcast();
    OnWeaponReloadStartNative.Broadcast();
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Weapon reload started"));
}

void USuspenseEventManager::NotifyWeaponReloadEnd()
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnWeaponReloadEnd.Broadcast();
    OnWeaponReloadEndNative.Broadcast();
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Weapon reload ended"));
}

void USuspenseEventManager::NotifyFireModeChanged(const FGameplayTag& NewFireMode, float CurrentSpread)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnFireModeChanged.Broadcast(NewFireMode, CurrentSpread);
    OnFireModeChangedNative.Broadcast(NewFireMode, CurrentSpread);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Fire mode changed: %s (spread: %.2f)"), 
           *NewFireMode.ToString(), CurrentSpread);
}

void USuspenseEventManager::NotifyFireModeProviderChanged(const FGameplayTag& FireModeTag, bool bEnabled)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnFireModeProviderChanged.Broadcast(FireModeTag, bEnabled);
    OnFireModeProviderChangedNative.Broadcast(FireModeTag, bEnabled);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Fire mode provider changed: %s (%s)"), 
           *FireModeTag.ToString(), bEnabled ? TEXT("Enabled") : TEXT("Disabled"));
}

void USuspenseEventManager::BroadcastWeaponSwitchStarted(int32 FromSlot, int32 ToSlot)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnWeaponSwitchStarted.Broadcast(FromSlot, ToSlot);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Weapon switch started from slot %d to slot %d"), 
           FromSlot, ToSlot);
}

void USuspenseEventManager::BroadcastWeaponSwitchCompleted(int32 FromSlot, int32 ToSlot)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnWeaponSwitchCompleted.Broadcast(FromSlot, ToSlot);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Weapon switch completed from slot %d to slot %d"), 
           FromSlot, ToSlot);
}

void USuspenseEventManager::NotifyEquipmentSlotUpdated(int32 SlotIndex, const FGameplayTag& SlotType, bool bIsOccupied)
{
    OnEquipmentSlotUpdatedNative.Broadcast(SlotIndex, SlotType, bIsOccupied);
    OnEquipmentSlotUpdated.Broadcast(SlotIndex, SlotType, bIsOccupied);
    
    UE_LOG(LogTemp, Verbose, TEXT("Equipment slot %d updated: %s, Occupied: %s"), 
        SlotIndex, *SlotType.ToString(), bIsOccupied ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyEquipmentDropValidation(const FDragDropUIData& DragData, int32 TargetSlot, bool bIsValid, const FText& Message)
{
    OnEquipmentDropValidationNative.Broadcast(DragData, TargetSlot, bIsValid, Message);
    
    UE_LOG(LogTemp, Verbose, TEXT("Equipment drop validation: Slot %d, Valid: %s"), 
        TargetSlot, bIsValid ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyEquipmentUIRefreshRequested(UUserWidget* Widget)
{
    OnEquipmentUIRefreshRequestedNative.Broadcast(Widget);
    OnEquipmentUIRefreshRequested.Broadcast(Widget);
    
    UE_LOG(LogTemp, Verbose, TEXT("Equipment UI refresh requested from widget: %s"), 
        Widget ? *Widget->GetName() : TEXT("Unknown"));
}


//================================================
// Movement Notification Methods Implementation
//================================================

void USuspenseEventManager::NotifyMovementSpeedChanged(float OldSpeed, float NewSpeed, bool bIsSprinting)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnMovementSpeedChanged.Broadcast(OldSpeed, NewSpeed, bIsSprinting);
    OnMovementSpeedChangedNative.Broadcast(OldSpeed, NewSpeed, bIsSprinting);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Movement speed changed from %.1f to %.1f (Sprinting: %s)"), 
           OldSpeed, NewSpeed, bIsSprinting ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyMovementStateChanged(FGameplayTag NewState, bool bIsTransitioning)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnMovementStateChanged.Broadcast(NewState, bIsTransitioning);
    OnMovementStateChangedNative.Broadcast(NewState, bIsTransitioning);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Movement state changed to %s (Transitioning: %s)"), 
           *NewState.ToString(), bIsTransitioning ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyJumpStateChanged(bool bIsJumping)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnJumpStateChanged.Broadcast(bIsJumping);
    OnJumpStateChangedNative.Broadcast(bIsJumping);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Jump state changed: %s"), 
           bIsJumping ? TEXT("Started") : TEXT("Ended"));
}

void USuspenseEventManager::NotifyCrouchStateChanged(bool bIsCrouching)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnCrouchStateChanged.Broadcast(bIsCrouching);
    OnCrouchStateChangedNative.Broadcast(bIsCrouching);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Crouch state changed: %s"), 
           bIsCrouching ? TEXT("Started") : TEXT("Ended"));
}

void USuspenseEventManager::NotifyLanded(float ImpactVelocity)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnLanded.Broadcast(ImpactVelocity);
    OnLandedNative.Broadcast(ImpactVelocity);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Character landed with impact velocity: %.1f"), 
           ImpactVelocity);
}

void USuspenseEventManager::NotifyMovementModeChanged(FName PreviousMode, FName NewMode, FGameplayTag StateTag)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnMovementModeChanged.Broadcast(PreviousMode, NewMode, StateTag);
    OnMovementModeChangedNative.Broadcast(PreviousMode, NewMode, StateTag);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Movement mode changed from %s to %s (State: %s)"), 
           *PreviousMode.ToString(), *NewMode.ToString(), *StateTag.ToString());
}

//================================================
// Loadout Notification Methods Implementation
//================================================

void USuspenseEventManager::NotifyLoadoutTableLoaded(UDataTable* LoadoutTable, int32 LoadedCount)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to both dynamic and native delegates
    OnLoadoutTableLoaded.Broadcast(LoadoutTable, LoadedCount);
    OnLoadoutTableLoadedNative.Broadcast(LoadoutTable, LoadedCount);
    
    UE_LOG(LogTemp, Log, TEXT("EventDelegateManager: Loadout table loaded: %s with %d configurations"), 
        LoadoutTable ? *LoadoutTable->GetName() : TEXT("None"), LoadedCount);
}

void USuspenseEventManager::NotifyLoadoutChanged(const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to both dynamic and native delegates
    OnLoadoutChanged.Broadcast(LoadoutID, PlayerState, bSuccess);
    OnLoadoutChangedNative.Broadcast(LoadoutID, PlayerState, bSuccess);
    
    // Безопасное получение имени игрока
    FString PlayerName = TEXT("Unknown");
    if (PlayerState)
    {
        // Используем GetPlayerName() если доступно, иначе GetName()
        PlayerName = PlayerState->GetPlayerName();
        if (PlayerName.IsEmpty())
        {
            PlayerName = PlayerState->GetName();
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("EventDelegateManager: Loadout changed to %s for player %s (Success: %s)"), 
        *LoadoutID.ToString(),
        *PlayerName,
        bSuccess ? TEXT("Yes") : TEXT("No"));
}

void USuspenseEventManager::NotifyLoadoutApplied(const FName& LoadoutID, UObject* TargetObject, const FGameplayTag& ComponentType, bool bSuccess)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Broadcast to both dynamic and native delegates
    OnLoadoutApplied.Broadcast(LoadoutID, TargetObject, ComponentType, bSuccess);
    OnLoadoutAppliedNative.Broadcast(LoadoutID, TargetObject, ComponentType, bSuccess);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Loadout %s applied to %s component %s (Success: %s)"), 
        *LoadoutID.ToString(),
        *ComponentType.ToString(),
        TargetObject ? *TargetObject->GetName() : TEXT("Unknown"),
        bSuccess ? TEXT("Yes") : TEXT("No"));
}

//================================================
// Tooltip Notification Methods
//================================================

void USuspenseEventManager::NotifyTooltipRequested(const FItemUIData& ItemData, const FVector2D& ScreenPosition)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnTooltipRequested.Broadcast(ItemData, ScreenPosition);
    OnTooltipRequestedNative.Broadcast(ItemData, ScreenPosition);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Tooltip requested for item %s"), 
        *ItemData.ItemID.ToString());
}

void USuspenseEventManager::NotifyTooltipHideRequested()
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnTooltipHideRequested.Broadcast();
    OnTooltipHideRequestedNative.Broadcast();
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Tooltip hide requested"));
}

void USuspenseEventManager::NotifyTooltipUpdatePosition(const FVector2D& ScreenPosition)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    OnTooltipUpdatePositionNative.Broadcast(ScreenPosition);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("EventDelegateManager: Tooltip position update"));
}

void USuspenseEventManager::BroadcastGenericEvent(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // Сначала broadcast через общий native делегат (для тех, кто слушает все события)
    OnGenericEventNative.Broadcast(Source, EventTag, EventData);
    
    // Затем broadcast подписчикам конкретного тега
    if (GenericEventSubscribers.Contains(EventTag))
    {
        // Копируем массив, чтобы избежать проблем если подписчик отпишется во время broadcast
        TArray<TPair<FDelegateHandle, FGenericEventDelegate>> Subscribers = GenericEventSubscribers[EventTag];
        
        for (const auto& Subscriber : Subscribers)
        {
            if (Subscriber.Value.IsBound())
            {
                Subscriber.Value.Execute(Source, EventTag, EventData);
            }
        }
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Generic event broadcasted - Tag: %s, Data: %s"), 
        *EventTag.ToString(), *EventData);
}
FDelegateHandle USuspenseEventManager::SubscribeToGenericEvent(const FGameplayTag& EventTag, const FGenericEventDelegate& Delegate)
{
    if (!ValidateSystemState() || !EventTag.IsValid() || !Delegate.IsBound())
    {
        return FDelegateHandle();
    }
    
    // Создаем уникальный handle
    FDelegateHandle NewHandle;
    NewHandle.Reset();
    
    // Используем счетчик для создания уникального ID
    GenericEventHandleCounter++;
    
    // Хак для создания валидного handle (в реальном коде лучше использовать GUID)
    // Это внутренняя деталь реализации Unreal, но работает
    *((int32*)&NewHandle) = GenericEventHandleCounter;
    
    // Добавляем подписчика
    if (!GenericEventSubscribers.Contains(EventTag))
    {
        GenericEventSubscribers.Add(EventTag, TArray<TPair<FDelegateHandle, FGenericEventDelegate>>());
    }
    
    GenericEventSubscribers[EventTag].Add(TPair<FDelegateHandle, FGenericEventDelegate>(NewHandle, Delegate));
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Subscribed to generic event - Tag: %s"), 
        *EventTag.ToString());
    
    return NewHandle;
}

void USuspenseEventManager::UnsubscribeFromGenericEvent(const FDelegateHandle& Handle)
{
    if (!ValidateSystemState() || !Handle.IsValid())
    {
        return;
    }
    
    bool bFound = false;
    
    // CRITICAL FIX: Collect tags to remove AFTER iteration
    TArray<FGameplayTag> TagsToRemove;
    
    // Ищем и удаляем подписчика во всех тегах
    for (auto& TagPair : GenericEventSubscribers)
    {
        TagPair.Value.RemoveAll([&Handle, &bFound](const TPair<FDelegateHandle, FGenericEventDelegate>& Subscriber)
        {
            if (Subscriber.Key == Handle)
            {
                bFound = true;
                return true;
            }
            return false;
        });
        
        // Mark empty entries for removal (don't remove during iteration!)
        if (TagPair.Value.Num() == 0)
        {
            TagsToRemove.Add(TagPair.Key);
        }
    }
    
    // CRITICAL FIX: Remove empty entries AFTER iteration completes
    // This fixes "Container has changed during ranged-for iteration" error
    for (const FGameplayTag& Tag : TagsToRemove)
    {
        GenericEventSubscribers.Remove(Tag);
    }
    
    if (bFound)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Unsubscribed from generic event (removed %d empty tag entries)"), 
            TagsToRemove.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Handle not found in generic event subscribers"));
    }
}
//================================================
// Unsubscription Methods Implementation
//================================================

void USuspenseEventManager::UnsubscribeFromNativeEvent(const FString& EventType, const FDelegateHandle& Handle)
{
    if (!ValidateSystemState() || !Handle.IsValid())
    {
        return;
    }
    
    // UI Events
    if (EventType == TEXT("UIWidgetCreated"))
    {
        OnUIWidgetCreatedNative.Remove(Handle);
    }
    else if (EventType == TEXT("UIWidgetDestroyed"))
    {
        OnUIWidgetDestroyedNative.Remove(Handle);
    }
    else if (EventType == TEXT("UIVisibilityChanged"))
    {
        OnUIVisibilityChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("HealthUpdated"))
    {
        OnHealthUpdatedNative.Remove(Handle);
    }
    else if (EventType == TEXT("StaminaUpdated"))
    {
        OnStaminaUpdatedNative.Remove(Handle);
    }
    else if (EventType == TEXT("CrosshairUpdated"))
    {
        OnCrosshairUpdatedNative.Remove(Handle);
    }
    else if (EventType == TEXT("CrosshairColorChanged"))
    {
        OnCrosshairColorChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("Notification"))
    {
        OnNotificationNative.Remove(Handle);
    }
    else if (EventType == TEXT("CharacterScreenOpened"))
    {
        OnCharacterScreenOpenedNative.Remove(Handle);
    }
    else if (EventType == TEXT("CharacterScreenClosed"))
    {
        OnCharacterScreenClosedNative.Remove(Handle);
    }
    else if (EventType == TEXT("TabBarInitialized"))
    {
        OnTabBarInitializedNative.Remove(Handle);
    }
    // Tab System Events
    else if (EventType == TEXT("UIEventGeneric"))
    {
        OnUIEventGenericNative.Remove(Handle);
    }
    else if (EventType == TEXT("TabClicked"))
    {
        OnTabClickedNative.Remove(Handle);
    }
    else if (EventType == TEXT("TabSelectionChanged"))
    {
        OnTabSelectionChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("ScreenActivated"))
    {
        OnScreenActivatedNative.Remove(Handle);
    }
    else if (EventType == TEXT("ScreenDeactivated"))
    {
        OnScreenDeactivatedNative.Remove(Handle);
    }

    // Equipment Events
    else if (EventType == TEXT("EquipmentOperationRequest"))
    {
        OnEquipmentOperationRequestNative.Remove(Handle);
    }
    else if (EventType == TEXT("EquipmentOperationCompleted"))
    {
        OnEquipmentOperationCompletedNative.Remove(Handle);
    }
    
    else if (EventType == TEXT("EquipmentUpdated"))
    {
        OnEquipmentUpdatedNative.Remove(Handle);
    }
    else if (EventType == TEXT("ActiveWeaponChanged"))
    {
        OnActiveWeaponChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("EquipmentEvent"))
    {
        OnEquipmentEventNative.Remove(Handle);
    }
    else if (EventType == TEXT("EquipmentStateChanged"))
    {
        OnEquipmentStateChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("AmmoChanged"))
    {
        OnAmmoChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("WeaponStateChanged"))
    {
        OnWeaponStateChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("WeaponFired"))
    {
        OnWeaponFiredNative.Remove(Handle);
    }
    else if (EventType == TEXT("WeaponSpreadUpdated"))
    {
        OnWeaponSpreadUpdatedNative.Remove(Handle);
    }
    else if (EventType == TEXT("WeaponReloadStart"))
    {
        OnWeaponReloadStartNative.Remove(Handle);
    }
    else if (EventType == TEXT("WeaponReloadEnd"))
    {
        OnWeaponReloadEndNative.Remove(Handle);
    }
    else if (EventType == TEXT("FireModeChanged"))
    {
        OnFireModeChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("FireModeProviderChanged"))
    {
        OnFireModeProviderChangedNative.Remove(Handle);
    }
    // Inventory Events
    else if (EventType == TEXT("UIContainerUpdateRequested"))
    {
        OnUIContainerUpdateRequestedNative.Remove(Handle);
    }
    else if (EventType == TEXT("UISlotInteraction"))
    {
        OnUISlotInteractionNative.Remove(Handle);
    }
    else if (EventType == TEXT("UIDragStarted"))
    {
        OnUIDragStartedNative.Remove(Handle);
    }
    else if (EventType == TEXT("UIDragCompleted"))
    {
        OnUIDragCompletedNative.Remove(Handle);
    }
    else if (EventType == TEXT("UIItemDropped"))
    {
        OnUIItemDroppedNative.Remove(Handle);
    }
    // Movement Events
    else if (EventType == TEXT("MovementSpeedChanged"))
    {
        OnMovementSpeedChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("MovementStateChanged"))
    {
        OnMovementStateChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("JumpStateChanged"))
    {
        OnJumpStateChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("CrouchStateChanged"))
    {
        OnCrouchStateChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("Landed"))
    {
        OnLandedNative.Remove(Handle);
    }
    else if (EventType == TEXT("MovementModeChanged"))
    {
        OnMovementModeChangedNative.Remove(Handle);
    }

    // Loadout Events
    else if (EventType == TEXT("LoadoutTableLoaded"))
    {
        OnLoadoutTableLoadedNative.Remove(Handle);
    }
    else if (EventType == TEXT("LoadoutChanged"))
    {
        OnLoadoutChangedNative.Remove(Handle);
    }
    else if (EventType == TEXT("LoadoutApplied"))
    {
        OnLoadoutAppliedNative.Remove(Handle);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Unknown native event type for unsubscription: %s"), *EventType);
        return;
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Unsubscribed from native %s event"), *EventType);
}

void USuspenseEventManager::UniversalUnsubscribe(const FDelegateHandle& Handle)
{
    if (!ValidateSystemState() || !Handle.IsValid())
    {
        return;
    }
    
    bool bFoundAndRemoved = false;
    
    // Try to remove from all UI native delegates
    if (OnUIWidgetCreatedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnUIWidgetDestroyedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnUIVisibilityChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnHealthUpdatedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnStaminaUpdatedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnCrosshairUpdatedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnCrosshairColorChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnNotificationNative.Remove(Handle)) bFoundAndRemoved = true;

    if (OnEquipmentOperationRequestNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnEquipmentOperationCompletedNative.Remove(Handle)) bFoundAndRemoved = true;
    
    if (OnCharacterScreenOpenedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnCharacterScreenClosedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnTabBarInitializedNative.Remove(Handle)) bFoundAndRemoved = true;
    // Try to remove from loadout native delegates
    if (OnLoadoutTableLoadedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnLoadoutChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnLoadoutAppliedNative.Remove(Handle)) bFoundAndRemoved = true;
    // Try to remove from tab system native delegates
    if (OnUIEventGenericNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnTabClickedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnTabSelectionChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnScreenActivatedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnScreenDeactivatedNative.Remove(Handle)) bFoundAndRemoved = true;
    
    // Try to remove from all equipment native delegates
    if (OnEquipmentUpdatedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnActiveWeaponChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnEquipmentEventNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnEquipmentStateChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnAmmoChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnWeaponStateChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnWeaponFiredNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnWeaponSpreadUpdatedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnWeaponReloadStartNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnWeaponReloadEndNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnFireModeChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnFireModeProviderChangedNative.Remove(Handle)) bFoundAndRemoved = true;

    // Try to remove from inventory native delegates
    if (OnUIContainerUpdateRequestedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnUISlotInteractionNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnUIDragStartedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnUIDragCompletedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnUIItemDroppedNative.Remove(Handle)) bFoundAndRemoved = true;
    
    // Try to remove from movement native delegates
    if (OnMovementSpeedChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnMovementStateChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnJumpStateChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnCrouchStateChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnLandedNative.Remove(Handle)) bFoundAndRemoved = true;
    if (OnMovementModeChangedNative.Remove(Handle)) bFoundAndRemoved = true;
    UnsubscribeFromGenericEvent(Handle);
    if (bFoundAndRemoved)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EventDelegateManager: Successfully unsubscribed using universal method"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Handle not found in any native delegate during universal unsubscribe"));
    }
}

//================================================
// Debug and Utility Implementation
//================================================

void USuspenseEventManager::LogSubscriptionStatus() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== Event System Delegate Statistics ==="));
    UE_LOG(LogTemp, Warning, TEXT("System initialized: %s"), bIsInitialized ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Warning, TEXT("Events processed: %d"), EventCounter);
    UE_LOG(LogTemp, Warning, TEXT("=== UI Blueprint Dynamic Delegates ==="));
    
    // UI Dynamic delegates
    UE_LOG(LogTemp, Warning, TEXT("OnUIWidgetCreated: %s"), OnUIWidgetCreated.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnUIWidgetDestroyed: %s"), OnUIWidgetDestroyed.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnUIVisibilityChanged: %s"), OnUIVisibilityChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnHealthUpdated: %s"), OnHealthUpdated.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnStaminaUpdated: %s"), OnStaminaUpdated.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnCrosshairUpdated: %s"), OnCrosshairUpdated.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnCrosshairColorChanged: %s"), OnCrosshairColorChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnNotification: %s"), OnNotification.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== Equipment Blueprint Dynamic Delegates ==="));
    UE_LOG(LogTemp, Warning, TEXT("=== Loadout Blueprint Dynamic Delegates ==="));
    // Equipment operation delegates
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentOperationRequest: %s"), OnEquipmentOperationRequest.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentOperationCompleted: %s"), OnEquipmentOperationCompleted.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentOperationRequestNative: %s"), OnEquipmentOperationRequestNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentOperationCompletedNative: %s"), OnEquipmentOperationCompletedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    // Loadout Dynamic delegates
    UE_LOG(LogTemp, Warning, TEXT("OnLoadoutTableLoaded: %s"), OnLoadoutTableLoaded.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnLoadoutChanged: %s"), OnLoadoutChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnLoadoutApplied: %s"), OnLoadoutApplied.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== Loadout Native C++ Delegates ==="));
    
    // Loadout Native delegates
    UE_LOG(LogTemp, Warning, TEXT("OnLoadoutTableLoadedNative: %s"), OnLoadoutTableLoadedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnLoadoutChangedNative: %s"), OnLoadoutChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnLoadoutAppliedNative: %s"), OnLoadoutAppliedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    // Equipment Dynamic delegates
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentUpdated: %s"), OnEquipmentUpdated.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnActiveWeaponChanged: %s"), OnActiveWeaponChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentEvent: %s"), OnEquipmentEvent.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentStateChanged: %s"), OnEquipmentStateChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnAmmoChanged: %s"), OnAmmoChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponStateChanged: %s"), OnWeaponStateChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponFired: %s"), OnWeaponFired.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponSpreadUpdated: %s"), OnWeaponSpreadUpdated.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponReloadStart: %s"), OnWeaponReloadStart.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponReloadEnd: %s"), OnWeaponReloadEnd.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnFireModeChanged: %s"), OnFireModeChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnFireModeProviderChanged: %s"), OnFireModeProviderChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== UI Native C++ Delegates ==="));
    
    // UI Native delegates
    UE_LOG(LogTemp, Warning, TEXT("OnUIWidgetCreatedNative: %s"), OnUIWidgetCreatedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnUIWidgetDestroyedNative: %s"), OnUIWidgetDestroyedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnUIVisibilityChangedNative: %s"), OnUIVisibilityChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnHealthUpdatedNative: %s"), OnHealthUpdatedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnStaminaUpdatedNative: %s"), OnStaminaUpdatedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnCrosshairUpdatedNative: %s"), OnCrosshairUpdatedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnCrosshairColorChangedNative: %s"), OnCrosshairColorChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnNotificationNative: %s"), OnNotificationNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== Equipment Native C++ Delegates ==="));
    
    // Equipment Native delegates
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentUpdatedNative: %s"), OnEquipmentUpdatedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnActiveWeaponChangedNative: %s"), OnActiveWeaponChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentEventNative: %s"), OnEquipmentEventNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnEquipmentStateChangedNative: %s"), OnEquipmentStateChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnAmmoChangedNative: %s"), OnAmmoChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponStateChangedNative: %s"), OnWeaponStateChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponFiredNative: %s"), OnWeaponFiredNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponSpreadUpdatedNative: %s"), OnWeaponSpreadUpdatedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponReloadStartNative: %s"), OnWeaponReloadStartNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnWeaponReloadEndNative: %s"), OnWeaponReloadEndNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnFireModeChangedNative: %s"), OnFireModeChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnFireModeProviderChangedNative: %s"), OnFireModeProviderChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== Movement Blueprint Dynamic Delegates ==="));
    
    // Movement Dynamic delegates
    UE_LOG(LogTemp, Warning, TEXT("OnMovementSpeedChanged: %s"), OnMovementSpeedChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnMovementStateChanged: %s"), OnMovementStateChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnJumpStateChanged: %s"), OnJumpStateChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnCrouchStateChanged: %s"), OnCrouchStateChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnLanded: %s"), OnLanded.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnMovementModeChanged: %s"), OnMovementModeChanged.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== Movement Native C++ Delegates ==="));
    
    // Movement Native delegates
    UE_LOG(LogTemp, Warning, TEXT("OnMovementSpeedChangedNative: %s"), OnMovementSpeedChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnMovementStateChangedNative: %s"), OnMovementStateChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnJumpStateChangedNative: %s"), OnJumpStateChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnCrouchStateChangedNative: %s"), OnCrouchStateChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnLandedNative: %s"), OnLandedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    UE_LOG(LogTemp, Warning, TEXT("OnMovementModeChangedNative: %s"), OnMovementModeChangedNative.IsBound() ? TEXT("Has subscribers") : TEXT("No subscribers"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== End Statistics ==="));
}


void USuspenseEventManager::ClearAllSubscriptions()
{
    UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Clearing all subscriptions..."));
    
    // Clear all UI dynamic delegates
    OnUIWidgetCreated.Clear();
    OnUIWidgetDestroyed.Clear();
    OnUIVisibilityChanged.Clear();
    OnHealthUpdated.Clear();
    OnStaminaUpdated.Clear();
    OnCrosshairUpdated.Clear();
    OnCrosshairColorChanged.Clear();
    OnNotification.Clear();

    OnCharacterScreenOpenedNative.Clear();
    OnCharacterScreenClosedNative.Clear();
    OnTabBarInitializedNative.Clear();

    // Clear equipment operation delegates
    OnEquipmentOperationRequest.Clear();
    OnEquipmentOperationCompleted.Clear();
    OnEquipmentOperationRequestNative.Clear();
    OnEquipmentOperationCompletedNative.Clear();
    
    // Clear all tab system native delegates
    OnUIEventGenericNative.Clear();
    OnTabClickedNative.Clear();
    OnTabSelectionChangedNative.Clear();
    OnScreenActivatedNative.Clear();
    OnScreenDeactivatedNative.Clear();
    // Clear all loadout dynamic delegates
    OnLoadoutTableLoaded.Clear();
    OnLoadoutChanged.Clear();
    OnLoadoutApplied.Clear();
    
    // Clear all loadout native delegates
    OnLoadoutTableLoadedNative.Clear();
    OnLoadoutChangedNative.Clear();
    OnLoadoutAppliedNative.Clear();
    // Clear all equipment dynamic delegates
    OnEquipmentUpdated.Clear();
    OnActiveWeaponChanged.Clear();
    OnEquipmentEvent.Clear();
    OnEquipmentStateChanged.Clear();
    OnAmmoChanged.Clear();
    OnWeaponStateChanged.Clear();
    OnWeaponFired.Clear();
    OnWeaponSpreadUpdated.Clear();
    OnWeaponReloadStart.Clear();
    OnWeaponReloadEnd.Clear();
    OnFireModeChanged.Clear();
    OnFireModeProviderChanged.Clear();

    // Clear all UI native delegates
    OnUIWidgetCreatedNative.Clear();
    OnUIWidgetDestroyedNative.Clear();
    OnUIVisibilityChangedNative.Clear();
    OnHealthUpdatedNative.Clear();
    OnStaminaUpdatedNative.Clear();
    OnCrosshairUpdatedNative.Clear();
    OnCrosshairColorChangedNative.Clear();
    OnNotificationNative.Clear();
    
    // Clear all equipment native delegates
    OnEquipmentUpdatedNative.Clear();
    OnActiveWeaponChangedNative.Clear();
    OnEquipmentEventNative.Clear();
    OnEquipmentStateChangedNative.Clear();
    OnAmmoChangedNative.Clear();
    OnWeaponStateChangedNative.Clear();
    OnWeaponFiredNative.Clear();
    OnWeaponSpreadUpdatedNative.Clear();
    OnWeaponReloadStartNative.Clear();
    OnWeaponReloadEndNative.Clear();
    OnFireModeChangedNative.Clear();
    OnFireModeProviderChangedNative.Clear();

    // Clear all movement dynamic delegates
    OnMovementSpeedChanged.Clear();
    OnMovementStateChanged.Clear();
    OnJumpStateChanged.Clear();
    OnCrouchStateChanged.Clear();
    OnLanded.Clear();
    OnMovementModeChanged.Clear();
    
    // Clear all movement native delegates
    OnMovementSpeedChangedNative.Clear();
    OnMovementStateChangedNative.Clear();
    OnJumpStateChangedNative.Clear();
    OnCrouchStateChangedNative.Clear();
    OnLandedNative.Clear();
    OnMovementModeChangedNative.Clear();

    UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: All subscriptions cleared. Events processed this session: %d"), EventCounter);
}

int32 USuspenseEventManager::GetNativeSubscriberCount() const
{
    int32 Count = 0;
    
    // Count UI native delegates
    if (OnUIWidgetCreatedNative.IsBound()) Count++;
    if (OnUIWidgetDestroyedNative.IsBound()) Count++;
    if (OnUIVisibilityChangedNative.IsBound()) Count++;
    if (OnHealthUpdatedNative.IsBound()) Count++;
    if (OnStaminaUpdatedNative.IsBound()) Count++;
    if (OnCrosshairUpdatedNative.IsBound()) Count++;
    if (OnCrosshairColorChangedNative.IsBound()) Count++;
    if (OnNotificationNative.IsBound()) Count++;

    if (OnEquipmentOperationRequestNative.IsBound()) Count++;
    if (OnEquipmentOperationCompletedNative.IsBound()) Count++;
    
    // Count tab system native delegates
    if (OnUIEventGenericNative.IsBound()) Count++;
    if (OnTabClickedNative.IsBound()) Count++;
    if (OnTabSelectionChangedNative.IsBound()) Count++;
    if (OnScreenActivatedNative.IsBound()) Count++;
    if (OnScreenDeactivatedNative.IsBound()) Count++;
    // Count loadout native delegates
    if (OnLoadoutTableLoadedNative.IsBound()) Count++;
    if (OnLoadoutChangedNative.IsBound()) Count++;
    if (OnLoadoutAppliedNative.IsBound()) Count++;
    // Count equipment native delegates
    if (OnEquipmentUpdatedNative.IsBound()) Count++;
    if (OnActiveWeaponChangedNative.IsBound()) Count++;
    if (OnEquipmentEventNative.IsBound()) Count++;
    if (OnEquipmentStateChangedNative.IsBound()) Count++;
    if (OnAmmoChangedNative.IsBound()) Count++;
    if (OnWeaponStateChangedNative.IsBound()) Count++;
    if (OnWeaponFiredNative.IsBound()) Count++;
    if (OnWeaponSpreadUpdatedNative.IsBound()) Count++;
    if (OnWeaponReloadStartNative.IsBound()) Count++;
    if (OnWeaponReloadEndNative.IsBound()) Count++;
    if (OnFireModeChangedNative.IsBound()) Count++;
    if (OnFireModeProviderChangedNative.IsBound()) Count++;
    
    // Count movement native delegates
    if (OnMovementSpeedChangedNative.IsBound()) Count++;
    if (OnMovementStateChangedNative.IsBound()) Count++;
    if (OnJumpStateChangedNative.IsBound()) Count++;
    if (OnCrouchStateChangedNative.IsBound()) Count++;
    if (OnLandedNative.IsBound()) Count++;
    if (OnMovementModeChangedNative.IsBound()) Count++;
    
    return Count;
}

bool USuspenseEventManager::ValidateSystemState() const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Attempt to send event before system initialization"));
        return false;
    }
    
    return true;
}

void USuspenseEventManager::NotifyUIItemDropped(UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot)
{
    if (!ValidateSystemState()) return;
    
    EventCounter++;
    
    // CRITICAL: Broadcast to BOTH dynamic and native delegates
    OnUIItemDropped.Broadcast(ContainerWidget, DragData, TargetSlot);
    OnUIItemDroppedNative.Broadcast(ContainerWidget, DragData, TargetSlot);
    
    UE_LOG(LogTemp, Warning, TEXT("EventDelegateManager: Item dropped - %s from slot %d to target slot %d (Broadcasted to %d dynamic + %d native listeners)"), 
        *DragData.ItemData.ItemID.ToString(),
        DragData.SourceSlotIndex, 
        TargetSlot,
        OnUIItemDropped.IsBound() ? 1 : 0,
        OnUIItemDroppedNative.IsBound() ? 1 : 0);
}