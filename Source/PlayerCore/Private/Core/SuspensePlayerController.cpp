// SuspensePlayerController.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Core/SuspensePlayerController.h"
#include "Characters/SuspenseCharacter.h"
#include "Core/SuspensePlayerState.h"
#include "Components/MedComAbilitySystemComponent.h"
#include "Components/MedComUIManager.h"
#include "Components/MedComInventoryUIBridge.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Interfaces/Core/IMedComControllerInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/UI/IMedComHUDWidgetInterface.h"
#include "Components/MedComEquipmentUIBridge.h"
#include "Types/UI/EquipmentUITypes.h"
#include "Interfaces/UI/IMedComEquipmentUIBridgeWidget.h"
#include "Components/MedComInventoryComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// UPDATED: Remove UIConnector include - no longer needed
// OLD: #include "UI/MedComEquipmentUIConnector.h"
// NEW: Direct DataStore access only
#include "Components/Core/MedComEquipmentDataStore.h"

/* ===== Constructor & Basic ===== */
ASuspensePlayerController::ASuspensePlayerController()
{ 
    bShowMouseCursor = false;
    
    // Default HUD settings
    HUDCreationDelay = 0.1f;
    bAutoCreateHUD = true;
    bShowFPSCounter = false;
    bShowDebugInfo = false;
}

void ASuspensePlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    // Setup input first
    SetupEnhancedInput();
    
    // Cache UI Manager
    CachedUIManager = GetUIManager();
    
    // Subscribe to delegate system events
    if (UEventDelegateManager* Manager = GetDelegateManager())
    {
        // Subscribe to inventory initialization event
        FGenericEventDelegate InventoryDelegate;
        InventoryDelegate.BindUObject(this, &ASuspensePlayerController::OnInventoryInitialized);
        InventoryInitHandle = Manager->SubscribeToGenericEvent(
            FGameplayTag::RequestGameplayTag(TEXT("Player.Inventory.Initialized")),
            InventoryDelegate
        );
        
        // Subscribe to equipment initialization event
        FGenericEventDelegate EquipmentDelegate;
        EquipmentDelegate.BindUObject(this, &ASuspensePlayerController::HandleEquipmentInitializationRequest);
        Manager->SubscribeToGenericEvent(
            FGameplayTag::RequestGameplayTag(TEXT("Player.Equipment.Initialized")),
            EquipmentDelegate
        );
        
        // Subscribe to loadout ready event
        FGenericEventDelegate LoadoutReadyDelegate;
        LoadoutReadyDelegate.BindUObject(this, &ASuspensePlayerController::OnLoadoutReady);
        LoadoutReadyHandle = Manager->SubscribeToGenericEvent(
            FGameplayTag::RequestGameplayTag(TEXT("Player.Loadout.Ready")),
            LoadoutReadyDelegate
        );
        
        // Subscribe to loadout failed event
        FGenericEventDelegate LoadoutFailedDelegate;
        LoadoutFailedDelegate.BindUObject(this, &ASuspensePlayerController::OnLoadoutFailed);
        LoadoutFailedHandle = Manager->SubscribeToGenericEvent(
            FGameplayTag::RequestGameplayTag(TEXT("Player.Loadout.Failed")),
            LoadoutFailedDelegate
        );
        
        // Existing equipment state change subscription
        EquipmentStateChangeHandle = Manager->SubscribeToEquipmentStateChanged(
            [this](FGameplayTag OldState, FGameplayTag NewState, bool bInterrupted) 
            {
                HandleEquipmentStateChange(OldState, NewState, bInterrupted);
            });
        
        // Health update subscription
        FDelegateHandle HealthHandle = Manager->SubscribeToHealthUpdated(
            [this](float Current, float Max, float Percent)
            {
                if (Percent < 0.25f && Current > 0.0f)
                {
                    if (CachedUIManager)
                    {
                        FGameplayTag WarningTag = FGameplayTag::RequestGameplayTag("UI.Warning.LowHealth");
                        CachedUIManager->ShowWidget(WarningTag, true);
                    }
                }
            });
        UIEventHandles.Add(HealthHandle);
        
        // Ammo change subscription
        FDelegateHandle AmmoHandle = Manager->SubscribeToAmmoChanged(
            [this](float CurrentAmmo, float RemainingAmmo, float MagazineSize)
            {
                float AmmoPercent = (MagazineSize > 0.0f) ? (CurrentAmmo / MagazineSize) : 0.0f;
                if (AmmoPercent < 0.25f && CurrentAmmo > 0.0f)
                {
                    if (CachedUIManager)
                    {
                        FGameplayTag WarningTag = FGameplayTag::RequestGameplayTag("UI.Warning.LowAmmo");
                        CachedUIManager->ShowWidget(WarningTag, true);
                    }
                }
            });
        UIEventHandles.Add(AmmoHandle);
            
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Subscribed to event system"));
    }
    
    // Register debug commands
    RegisterDebugCommands();
}

void ASuspensePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up HUD through UI Manager
    DestroyHUD();
    
    // Unsubscribe from events
    if (UEventDelegateManager* Manager = GetDelegateManager())
    {
        if (InventoryInitHandle.IsValid())
        {
            Manager->UnsubscribeFromGenericEvent(InventoryInitHandle);
            InventoryInitHandle.Reset();
        }
        
        if (LoadoutReadyHandle.IsValid())
        {
            Manager->UnsubscribeFromGenericEvent(LoadoutReadyHandle);
            LoadoutReadyHandle.Reset();
        }
        
        if (LoadoutFailedHandle.IsValid())
        {
            Manager->UnsubscribeFromGenericEvent(LoadoutFailedHandle);
            LoadoutFailedHandle.Reset();
        }
        
        if (EquipmentStateChangeHandle.IsValid())
        {
            Manager->UniversalUnsubscribe(EquipmentStateChangeHandle);
            EquipmentStateChangeHandle.Reset();
        }
        
        if (AttributeChangeHandle.IsValid())
        {
            Manager->UniversalUnsubscribe(AttributeChangeHandle);
            AttributeChangeHandle.Reset();
        }
        
        for (const FDelegateHandle& Handle : UIEventHandles)
        {
            if (Handle.IsValid())
            {
                Manager->UniversalUnsubscribe(Handle);
            }
        }
        UIEventHandles.Empty();
        
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Unsubscribed from event system"));
    }
    
    // REMOVED: No more UIConnector cleanup needed
    // OLD CODE:
    // if (EquipmentUIConnector)
    // {
    //     EquipmentUIConnector->Teardown();
    //     EquipmentUIConnector->DestroyComponent();
    //     EquipmentUIConnector = nullptr;
    // }
    
    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HUDCreationTimerHandle);
    }
    
    // Clear cached references
    CachedUIManager = nullptr;
    bInventoryBridgeReady = false;
    bEquipmentBridgeReady = false;
    
    Super::EndPlay(EndPlayReason);
}

void ASuspensePlayerController::OnInventoryInitialized(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Received inventory initialized event from: %s, Data: %s"), 
        Source ? *Source->GetName() : TEXT("Unknown"), *EventData);
    
    // Check this is our PlayerState
    ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>();
    if (!PS || PS != Source)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[PlayerController] Ignoring inventory init event from different source"));
        return;
    }
    
    // Parse event data
    FString PlayerStateName;
    FString LoadoutID;
    
    TArray<FString> DataPairs;
    EventData.ParseIntoArray(DataPairs, TEXT(","), true);
    
    for (const FString& Pair : DataPairs)
    {
        FString Key, Value;
        if (Pair.Split(TEXT(":"), &Key, &Value))
        {
            if (Key == TEXT("PlayerState"))
            {
                PlayerStateName = Value;
            }
            else if (Key == TEXT("LoadoutID"))
            {
                LoadoutID = Value;
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Our inventory is ready. PlayerState: %s, LoadoutID: %s"), 
        *PlayerStateName, *LoadoutID);
}

void ASuspensePlayerController::OnLoadoutReady(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Loadout ready event received. Loadout ID: %s"), *EventData);
    
    ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>();
    if (!PS || PS != Source)
    {
        return;
    }
    
    // Loadout fully applied and ready
    if (CachedUIManager && IsHUDCreated())
    {
        CachedUIManager->RequestHUDUpdate();
    }
}

void ASuspensePlayerController::OnLoadoutFailed(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    UE_LOG(LogTemp, Error, TEXT("[PlayerController] Loadout failed event received: %s"), *EventData);
    
    if (CachedUIManager)
    {
        CachedUIManager->ShowNotification(
            FText::FromString(TEXT("Failed to load character loadout")),
            5.0f,
            FLinearColor::Red
        );
    }
}

void ASuspensePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    
    if(UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if(IA_Move)   EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASuspensePlayerController::HandleMove);
        if(IA_Look)   EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ASuspensePlayerController::HandleLook);
        
        if(IA_Jump)   {
            EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &ASuspensePlayerController::OnJumpPressed);
            EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ASuspensePlayerController::OnJumpReleased);
        }
        
        if(IA_Sprint) {
            EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &ASuspensePlayerController::OnSprintPressed);
            EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ASuspensePlayerController::OnSprintReleased);
        }
        
        if(IA_Crouch) {
            EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ASuspensePlayerController::OnCrouchPressed);
            EIC->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &ASuspensePlayerController::OnCrouchReleased);
        }
        
        if(IA_Interact) {
            EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ASuspensePlayerController::OnInteractPressed);
        }
        
        if(IA_OpenInventory) {
            EIC->BindAction(IA_OpenInventory, ETriggerEvent::Started, this, &ASuspensePlayerController::OnInventoryToggle);
        }

        if(IA_NextWeapon) {
            EIC->BindAction(IA_NextWeapon, ETriggerEvent::Started, this, &ASuspensePlayerController::OnNextWeapon);
        }
        
        if(IA_PrevWeapon) {
            EIC->BindAction(IA_PrevWeapon, ETriggerEvent::Started, this, &ASuspensePlayerController::OnPrevWeapon);
        }
        
        if(IA_QuickSwitch) {
            EIC->BindAction(IA_QuickSwitch, ETriggerEvent::Started, this, &ASuspensePlayerController::OnQuickSwitch);
        }
        
        if(IA_WeaponSlot1) {
            EIC->BindAction(IA_WeaponSlot1, ETriggerEvent::Started, this, &ASuspensePlayerController::OnWeaponSlot1);
        }
        if(IA_WeaponSlot2) {
            EIC->BindAction(IA_WeaponSlot2, ETriggerEvent::Started, this, &ASuspensePlayerController::OnWeaponSlot2);
        }
        if(IA_WeaponSlot3) {
            EIC->BindAction(IA_WeaponSlot3, ETriggerEvent::Started, this, &ASuspensePlayerController::OnWeaponSlot3);
        }
        if(IA_WeaponSlot4) {
            EIC->BindAction(IA_WeaponSlot4, ETriggerEvent::Started, this, &ASuspensePlayerController::OnWeaponSlot4);
        }
        if(IA_WeaponSlot5) {
            EIC->BindAction(IA_WeaponSlot5, ETriggerEvent::Started, this, &ASuspensePlayerController::OnWeaponSlot5);
        }
    }
}

void ASuspensePlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    // Initialize ASC
    if(ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>())
    {
        if(UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
        {
            ASC->InitAbilityActorInfo(PS, InPawn);
        }
    }
    
    // Create HUD after a short delay
    if (bAutoCreateHUD && IsLocalController())
    {
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(
                HUDCreationTimerHandle,
                this,
                &ASuspensePlayerController::TryCreateHUD,
                HUDCreationDelay,
                false
            );
        }
    }
}

void ASuspensePlayerController::OnUnPossess()
{
    DestroyHUD();
    Super::OnUnPossess();
}

void ASuspensePlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    
    // Initialize ASC on client
    if(ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>())
    {
        if(UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
        {
            ASC->InitAbilityActorInfo(PS, GetPawn());
        }
    }
    
    if (bAutoCreateHUD && IsLocalController() && !IsHUDCreated())
    {
        TryCreateHUD();
    }
}

/* ===== HUD Management ===== */

void ASuspensePlayerController::CreateHUD()
{
    if (!IsLocalController())
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerController] CreateHUD called on non-local controller"));
        return;
    }
    
    UMedComUIManager* UIManager = GetUIManager();
    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] Failed to get UIManager"));
        return;
    }
    
    FGameplayTag HUDTag = FGameplayTag::RequestGameplayTag("UI.HUD.Main");
    if (UIManager->WidgetExists(HUDTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerController] HUD already exists"));
        MainHUDWidget = UIManager->GetWidget(HUDTag);
        return;
    }
    
    if (!MainHUDClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] MainHUDClass not set in Blueprint!"));
        return;
    }
    
    UUserWidget* CreatedHUD = UIManager->CreateWidget(MainHUDClass, HUDTag, this, true);
    if (!CreatedHUD)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] Failed to create HUD widget"));
        return;
    }
    
    MainHUDWidget = CreatedHUD;
    
    if (CreatedHUD->GetClass()->ImplementsInterface(UMedComHUDWidgetInterface::StaticClass()))
    {
        if (APawn* CurrentPawn = GetPawn())
        {
            IMedComHUDWidgetInterface::Execute_SetupForPlayer(CreatedHUD, CurrentPawn);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] HUD created successfully"));
}

void ASuspensePlayerController::DestroyHUD()
{
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        FGameplayTag HUDTag = FGameplayTag::RequestGameplayTag("UI.HUD.Main");
        UIManager->DestroyWidget(HUDTag);
        MainHUDWidget = nullptr;
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] HUD destroyed"));
    }
}

UUserWidget* ASuspensePlayerController::GetHUDWidget() const
{
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        FGameplayTag HUDTag = FGameplayTag::RequestGameplayTag("UI.HUD.Main");
        return UIManager->GetWidget(HUDTag);
    }
    return MainHUDWidget;
}

void ASuspensePlayerController::SetHUDVisibility(bool bShow)
{
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        FGameplayTag HUDTag = FGameplayTag::RequestGameplayTag("UI.HUD.Main");
        
        if (bShow)
        {
            UIManager->ShowWidget(HUDTag, false);
        }
        else
        {
            UIManager->HideWidget(HUDTag, false);
        }
        
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] HUD visibility set to: %s"), 
            bShow ? TEXT("Visible") : TEXT("Hidden"));
    }
}

bool ASuspensePlayerController::IsHUDCreated() const
{
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        FGameplayTag HUDTag = FGameplayTag::RequestGameplayTag("UI.HUD.Main");
        return UIManager->WidgetExists(HUDTag);
    }
    return false;
}

void ASuspensePlayerController::ShowInGameMenu()
{
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        FGameplayTag MenuTag = FGameplayTag::RequestGameplayTag("UI.Menu.Pause");
        
        if (!UIManager->WidgetExists(MenuTag))
        {
            UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Pause menu not configured"));
            return;
        }
        
        UIManager->ShowWidget(MenuTag, true);
        
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(InputMode);
        SetShowMouseCursor(true);
        SetPause(true);
    }
}

void ASuspensePlayerController::HideInGameMenu()
{
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        FGameplayTag MenuTag = FGameplayTag::RequestGameplayTag("UI.Menu.Pause");
        UIManager->HideWidget(MenuTag, true);
        
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        SetShowMouseCursor(false);
        SetPause(false);
    }
}

void ASuspensePlayerController::ToggleInventory()
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] ToggleInventory called"));
    
    // Initialize both bridges when needed
    EnsureInventoryBridgeInitialized();
    EnsureEquipmentBridgeInitialized();
    
    UMedComUIManager* UIManager = GetUIManager();
    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] No UIManager found"));
        return;
    }
    
    bool bIsCharacterScreenVisible = UIManager->IsCharacterScreenVisible();
    
    if (bIsCharacterScreenVisible)
    {
        UIManager->HideCharacterScreen();
        
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        SetShowMouseCursor(false);
        
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Character screen closed"));
    }
    else
    {
        FGameplayTag InventoryTabTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"));
        UIManager->ShowCharacterScreen(this, InventoryTabTag);
        
        // Refresh both bridges after screen is shown
        FTimerHandle RefreshHandle;
        GetWorld()->GetTimerManager().SetTimer(RefreshHandle, [UIManager]()
        {
            if (UMedComInventoryUIBridge* InvBridge = UIManager->GetInventoryUIBridge())
            {
                IMedComInventoryUIBridgeWidget::Execute_RefreshInventoryUI(InvBridge);
            }
            
            if (UMedComEquipmentUIBridge* EquipBridge = UIManager->GetEquipmentUIBridge())
            {
                IMedComEquipmentUIBridgeWidget::Execute_RefreshEquipmentUI(EquipBridge);
            }
        }, 0.1f, false);
        
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
        SetShowMouseCursor(true);
        
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Character screen opened"));
    }
}

void ASuspensePlayerController::TryCreateHUD()
{
    HUDCreationTimerHandle.Invalidate();
    
    if (!IsLocalController())
    {
        return;
    }
    
    if (IsHUDCreated())
    {
        return;
    }
    
    ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>();
    if (!PS)
    {
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(
                HUDCreationTimerHandle,
                this,
                &ASuspensePlayerController::TryCreateHUD,
                HUDCreationDelay,
                false
            );
        }
        return;
    }
    
    CreateHUD();
}

void ASuspensePlayerController::UpdateHUDData()
{
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        UIManager->RequestHUDUpdate();
    }
}

void ASuspensePlayerController::HandleAttributeChanged(const FGameplayTag& AttributeTag, float NewValue, float OldValue)
{
    UE_LOG(LogTemp, Verbose, TEXT("[PlayerController] Attribute changed: %s (%.2f -> %.2f)"), 
        *AttributeTag.ToString(), OldValue, NewValue);
}

UMedComUIManager* ASuspensePlayerController::GetUIManager() const
{
    if (CachedUIManager)
    {
        return CachedUIManager;
    }
    return UMedComUIManager::Get(this);
}

/* ===== Enhanced Input ===== */
void ASuspensePlayerController::SetupEnhancedInput()
{
    if(ULocalPlayer* LP = GetLocalPlayer())
    {
        if(UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if(DefaultContext) 
            {
                Sub->AddMappingContext(DefaultContext, 0);
            }
        }
    }
}

/* ===== Movement ===== */
void ASuspensePlayerController::HandleMove(const FInputActionValue& V)
{
    if(ASuspenseCharacter* C = GetMedComCharacter())
    {
        C->Move(V.Get<FVector2D>());
    }
}

void ASuspensePlayerController::HandleLook(const FInputActionValue& V)
{
    if(ASuspenseCharacter* C = GetMedComCharacter())
    {
        C->Look(V.Get<FVector2D>());
    }
}

/* ===== Ability wrappers ===== */
#define FIRE_TAG(Name) FGameplayTag::RequestGameplayTag("Ability.Input." Name)

void ASuspensePlayerController::OnJumpPressed(const FInputActionValue&) 
{ 
    ActivateAbility(FIRE_TAG("Jump"), true); 
}

void ASuspensePlayerController::OnJumpReleased(const FInputActionValue&) 
{ 
    ActivateAbility(FIRE_TAG("Jump"), false); 
}

void ASuspensePlayerController::OnSprintPressed(const FInputActionValue&) 
{ 
    ActivateAbility(FIRE_TAG("Sprint"), true); 
}

void ASuspensePlayerController::OnSprintReleased(const FInputActionValue&) 
{ 
    ActivateAbility(FIRE_TAG("Sprint"), false); 
}

void ASuspensePlayerController::OnCrouchPressed(const FInputActionValue&) 
{ 
    ActivateAbility(FIRE_TAG("Crouch"), true); 
}

void ASuspensePlayerController::OnCrouchReleased(const FInputActionValue&) 
{ 
    ActivateAbility(FIRE_TAG("Crouch"), false); 
}

void ASuspensePlayerController::OnInteractPressed(const FInputActionValue&) 
{ 
    ActivateAbility(FIRE_TAG("Interact"), true); 
}

void ASuspensePlayerController::OnInventoryToggle(const FInputActionValue& Value)
{
    ToggleInventory();
}

void ASuspensePlayerController::OnNextWeapon(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("NextWeapon"), true);
}

void ASuspensePlayerController::OnPrevWeapon(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("PrevWeapon"), true);
}

void ASuspensePlayerController::OnQuickSwitch(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("QuickSwitch"), true);
}

void ASuspensePlayerController::OnWeaponSlot1(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("WeaponSlot1"), true);
}

void ASuspensePlayerController::OnWeaponSlot2(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("WeaponSlot2"), true);
}

void ASuspensePlayerController::OnWeaponSlot3(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("WeaponSlot3"), true);
}

void ASuspensePlayerController::OnWeaponSlot4(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("WeaponSlot4"), true);
}

void ASuspensePlayerController::OnWeaponSlot5(const FInputActionValue&)
{
    ActivateAbility(FIRE_TAG("WeaponSlot5"), true);
}

#undef FIRE_TAG

/* ===== GAS ===== */
void ASuspensePlayerController::ActivateAbility(const FGameplayTag& Tag, bool bPressed)
{
    if(!Tag.IsValid()) 
    {
        UE_LOG(LogTemp, Error, TEXT("ActivateAbility: Invalid tag!"));
        return;
    }
    
    UAbilitySystemComponent* ASC = GetCharacterASC();
    if(!ASC)
    {
        UE_LOG(LogTemp, Error, TEXT("ActivateAbility: No ASC found!"));
        return;
    }
    
    int32 ID = (int32)EMCAbilityInputID::None;
    
    if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.Jump")))
    {
        ID = (int32)EMCAbilityInputID::Jump;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.Sprint")))
    {
        ID = (int32)EMCAbilityInputID::Sprint;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.Crouch")))
    {
        ID = (int32)EMCAbilityInputID::Crouch;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.Interact")))
    {
        ID = (int32)EMCAbilityInputID::Interact;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.NextWeapon")))
    {
        ID = (int32)EMCAbilityInputID::NextWeapon;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.PrevWeapon")))
    {
        ID = (int32)EMCAbilityInputID::PrevWeapon;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.QuickSwitch")))
    {
        ID = (int32)EMCAbilityInputID::QuickSwitch;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.WeaponSlot1")))
    {
        ID = (int32)EMCAbilityInputID::WeaponSlot1;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.WeaponSlot2")))
    {
        ID = (int32)EMCAbilityInputID::WeaponSlot2;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.WeaponSlot3")))
    {
        ID = (int32)EMCAbilityInputID::WeaponSlot3;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.WeaponSlot4")))
    {
        ID = (int32)EMCAbilityInputID::WeaponSlot4;
    }
    else if(Tag.MatchesTagExact(FGameplayTag::RequestGameplayTag("Ability.Input.WeaponSlot5")))
    {
        ID = (int32)EMCAbilityInputID::WeaponSlot5;
    }

    if (ID == (int32)EMCAbilityInputID::None)
    {
        UE_LOG(LogTemp, Error, TEXT("ActivateAbility: Failed to map tag %s to InputID"), *Tag.ToString());
        return;
    }

    if(bPressed)
    {
        ASC->AbilityLocalInputPressed(ID);
    }
    else
    {
        ASC->AbilityLocalInputReleased(ID);
    }
}

ASuspenseCharacter* ASuspensePlayerController::GetMedComCharacter() const
{
    return Cast<ASuspenseCharacter>(GetPawn());
}

UAbilitySystemComponent* ASuspensePlayerController::GetCharacterASC() const
{
    if(ASuspenseCharacter* MedComCharacter = GetMedComCharacter())
    {
        if (MedComCharacter->GetClass()->ImplementsInterface(UMedComCharacterInterface::StaticClass()))
        {
            return IMedComCharacterInterface::Execute_GetASC(MedComCharacter);
        }
    }
    
    if (ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>())
    {
        return PS->GetAbilitySystemComponent();
    }
    
    return nullptr;
}

void ASuspensePlayerController::HandleEquipmentStateChange(FGameplayTag OldState, FGameplayTag NewState, bool bInterrupted)
{
    UE_LOG(LogTemp, Log, TEXT("PlayerController: Equipment state changed from %s to %s (Interrupted: %s)"),
        *OldState.ToString(), *NewState.ToString(), bInterrupted ? TEXT("Yes") : TEXT("No"));
}

/* ===== IMedComControllerInterface implementation ===== */
void ASuspensePlayerController::NotifyWeaponChanged_Implementation(AActor* NewWeapon)
{
    CurrentWeapon = NewWeapon;
    
    if (ASuspenseCharacter* CharacterPawn = GetMedComCharacter())
    {
        if (CharacterPawn->GetClass()->ImplementsInterface(UMedComCharacterInterface::StaticClass()))
        {
            IMedComCharacterInterface::Execute_SetCurrentWeaponActor(CharacterPawn, NewWeapon);
            IMedComCharacterInterface::Execute_SetHasWeapon(CharacterPawn, NewWeapon != nullptr);
        }
    }
    
    IMedComControllerInterface::BroadcastControllerWeaponChanged(this, NewWeapon);
    
    if (UMedComUIManager* UIManager = GetUIManager())
    {
        FGameplayTag WeaponInfoTag = FGameplayTag::RequestGameplayTag("UI.HUD.WeaponInfo");
        if (NewWeapon)
        {
            UIManager->ShowWidget(WeaponInfoTag, true);
        }
        else
        {
            UIManager->HideWidget(WeaponInfoTag, false);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Weapon changed to: %s"), 
        NewWeapon ? *NewWeapon->GetName() : TEXT("None"));
}

AActor* ASuspensePlayerController::GetCurrentWeapon_Implementation() const
{
    return CurrentWeapon;
}

void ASuspensePlayerController::NotifyWeaponStateChanged_Implementation(FGameplayTag WeaponState)
{
    CurrentWeaponState = WeaponState;
    
    if (UEventDelegateManager* Manager = GetDelegateManager())
    {
        Manager->NotifyWeaponStateChanged(FGameplayTag(), WeaponState, false);
    }
}

APawn* ASuspensePlayerController::GetControlledPawn_Implementation() const
{
    return GetPawn();
}

bool ASuspensePlayerController::CanUseWeapon_Implementation() const
{
    return CurrentWeapon != nullptr;
}

bool ASuspensePlayerController::HasValidPawn_Implementation() const
{
    return GetPawn() != nullptr && IsValid(GetPawn());
}

void ASuspensePlayerController::UpdateInputBindings_Implementation()
{
    SetupEnhancedInput();
}

int32 ASuspensePlayerController::GetInputPriority_Implementation() const
{
    return 0;
}

UEventDelegateManager* ASuspensePlayerController::GetDelegateManager() const
{
    return IMedComControllerInterface::GetDelegateManagerStatic(this);
}

/* ===== Inventory Management ===== */

void ASuspensePlayerController::EnsureInventoryBridgeInitialized()
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] EnsureInventoryBridgeInitialized called"));
    
    if (bInventoryBridgeReady)
    {
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Inventory bridge already initialized"));
        return;
    }
    
    UMedComUIManager* UIManager = GetUIManager();
    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] Failed to get UIManager"));
        return;
    }
    
    UMedComInventoryUIBridge* Bridge = UIManager->GetInventoryUIBridge();
    if (!Bridge)
    {
        Bridge = UIManager->CreateInventoryUIBridge(this);
        if (!Bridge)
        {
            UE_LOG(LogTemp, Error, TEXT("[PlayerController] Failed to create inventory UI bridge"));
            return;
        }
    }
    
    if (IMedComInventoryUIBridgeWidget::Execute_IsInventoryConnected(Bridge))
    {
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Inventory already connected to bridge"));
        bInventoryBridgeReady = true;
        return;
    }
    
    ConnectInventoryToBridge(Bridge);
}

void ASuspensePlayerController::ConnectInventoryToBridge(UMedComInventoryUIBridge* Bridge)
{
    if (!Bridge)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] ConnectInventoryToBridge - invalid bridge"));
        return;
    }
    
    ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] No PlayerState found"));
        return;
    }
    
    UMedComInventoryComponent* InventoryComp = PS->GetInventoryComponent();
    if (!InventoryComp)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] No InventoryComponent in PlayerState"));
        return;
    }
    
    if (!InventoryComp->IsInventoryInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] InventoryComponent not initialized"));
        return;
    }
    
    TScriptInterface<IMedComInventoryInterface> InventoryInterface;
    InventoryInterface.SetObject(InventoryComp);
    InventoryInterface.SetInterface(Cast<IMedComInventoryInterface>(InventoryComp));
    
    Bridge->SetInventoryInterface(InventoryInterface);
    bInventoryBridgeReady = true;
    
    FVector2D InvSize = InventoryComp->GetInventorySize();
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Successfully connected InventoryComponent to UI Bridge"));
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Inventory size: %dx%d"), 
        (int32)InvSize.X, (int32)InvSize.Y);
    
    IMedComInventoryUIBridgeWidget::Execute_RefreshInventoryUI(Bridge);
}

void ASuspensePlayerController::HandleInventoryInitializationRequest(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Received inventory initialization request from: %s"), 
        Source ? *Source->GetName() : TEXT("Unknown"));
}

/* ===== Equipment Management (SIMPLIFIED - NO MORE CONNECTOR) ===== */

void ASuspensePlayerController::EnsureEquipmentBridgeInitialized()
{
    if (!IsLocalController())
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[PlayerController] === EnsureEquipmentBridgeInitialized START ==="));

    UMedComUIManager* UIManager = GetUIManager();
    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] UIManager unavailable"));
        return;
    }

    // Create bridge if not yet created
    UMedComEquipmentUIBridge* Bridge = UIManager->GetEquipmentUIBridge();
    if (!Bridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Creating equipment bridge..."));
        Bridge = UIManager->CreateEquipmentUIBridge(this);
        if (!Bridge)
        {
            UE_LOG(LogTemp, Error, TEXT("[PlayerController] Failed to create equipment UI bridge"));
            return;
        }
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Created new equipment UI bridge"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Equipment bridge already exists"));
    }

    // Check connection status
    if (IMedComEquipmentUIBridgeWidget::Execute_IsEquipmentConnected(Bridge))
    {
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] Equipment already connected to bridge"));
        bEquipmentBridgeReady = true;
        UE_LOG(LogTemp, Warning, TEXT("[PlayerController] === EnsureEquipmentBridgeInitialized END (already connected) ==="));
        return;
    }

    // Connect through simplified method (no more UIConnector!)
    UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Connecting equipment to bridge..."));
    ConnectEquipmentToBridge(Bridge);
    
    UE_LOG(LogTemp, Warning, TEXT("[PlayerController] === EnsureEquipmentBridgeInitialized END ==="));
}

void ASuspensePlayerController::ConnectEquipmentToBridge(UMedComEquipmentUIBridge* Bridge)
{
    if (!Bridge)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] ConnectEquipmentToBridge: invalid Bridge"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerController] === Starting SIMPLIFIED Equipment Bridge Connection ==="));

    // STEP 1: Initialize bridge with PlayerController
    Bridge->Initialize(this);

    // STEP 2: Get PlayerState
    ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>();
    if (!PS)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] No PlayerState found"));
        return;
    }

    // STEP 3: Get EquipmentDataStore from PlayerState
    UMedComEquipmentDataStore* DataStore = PS->GetEquipmentDataStore();
    if (!DataStore)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] No EquipmentDataStore in PlayerState"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Found EquipmentDataStore: %s"), *DataStore->GetName());

    // STEP 4 (NEW SIMPLIFIED): Direct binding to DataStore - NO UIConnector!
    // Wrap DataStore in TScriptInterface for the call
    TScriptInterface<IMedComEquipmentDataProvider> DataStoreInterface;
    DataStoreInterface.SetObject(DataStore);
    DataStoreInterface.SetInterface(Cast<IMedComEquipmentDataProvider>(DataStore));
    
    Bridge->BindToDataStore(DataStoreInterface);

    // STEP 5: Check connection status
    bEquipmentBridgeReady = IMedComEquipmentUIBridgeWidget::Execute_IsEquipmentConnected(Bridge);
    
    if (bEquipmentBridgeReady)
    {
        UE_LOG(LogTemp, Log, TEXT("[PlayerController] ✅ Equipment bridge fully connected (SIMPLIFIED FLOW)"));
        
        // Trigger initial UI refresh
        IMedComEquipmentUIBridgeWidget::Execute_RefreshEquipmentUI(Bridge);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Equipment bridge initialized but connection uncertain"));
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerController] === Equipment Bridge Connection Complete (NO UIConnector!) ==="));
}

void ASuspensePlayerController::HandleEquipmentInitializationRequest(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] Received equipment initialization request from: %s"), 
        Source ? *Source->GetName() : TEXT("Unknown"));
}

void ASuspensePlayerController::ShowCharacterScreen(const FGameplayTag& DefaultTab)
{
    UE_LOG(LogTemp, Log, TEXT("[PlayerController] ShowCharacterScreen called with tab: %s"), 
        *DefaultTab.ToString());
    
    // Initialize both bridges BEFORE showing screen
    EnsureInventoryBridgeInitialized();
    EnsureEquipmentBridgeInitialized();
    
    UMedComUIManager* UIManager = GetUIManager();
    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerController] No UIManager available"));
        return;
    }
    
    // Check bridge readiness
    bool bInventoryReady = false;
    bool bEquipmentReady = false;
    
    if (UMedComInventoryUIBridge* InvBridge = UIManager->GetInventoryUIBridge())
    {
        bInventoryReady = IMedComInventoryUIBridgeWidget::Execute_IsInventoryConnected(InvBridge);
    }
    
    if (UMedComEquipmentUIBridge* EquipBridge = UIManager->GetEquipmentUIBridge())
    {
        bEquipmentReady = IMedComEquipmentUIBridgeWidget::Execute_IsEquipmentConnected(EquipBridge);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Bridge status: Inventory=%s, Equipment=%s"),
        bInventoryReady ? TEXT("READY") : TEXT("NOT READY"),
        bEquipmentReady ? TEXT("READY") : TEXT("NOT READY"));
    
    // Show screen through UI Manager
    UIManager->ShowCharacterScreen(this, DefaultTab);
    
    // Delayed refresh to ensure UI is fully constructed
    FTimerHandle RefreshHandle;
    GetWorld()->GetTimerManager().SetTimer(RefreshHandle, [UIManager, this]()
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Executing delayed refresh..."));
        
        if (UMedComInventoryUIBridge* InvBridge = UIManager->GetInventoryUIBridge())
        {
            if (IMedComInventoryUIBridgeWidget::Execute_IsInventoryConnected(InvBridge))
            {
                IMedComInventoryUIBridgeWidget::Execute_RefreshInventoryUI(InvBridge);
                UE_LOG(LogTemp, Log, TEXT("[PlayerController] Inventory UI refreshed"));
            }
        }
        
        if (UMedComEquipmentUIBridge* EquipBridge = UIManager->GetEquipmentUIBridge())
        {
            if (IMedComEquipmentUIBridgeWidget::Execute_IsEquipmentConnected(EquipBridge))
            {
                IMedComEquipmentUIBridgeWidget::Execute_RefreshEquipmentUI(EquipBridge);
                UE_LOG(LogTemp, Log, TEXT("[PlayerController] Equipment UI refreshed"));
            }
        }
        
    }, 0.15f, false);
    
    // Set UI input mode
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    SetShowMouseCursor(true);
}

/* ===== Debug Commands ===== */

void ASuspensePlayerController::RegisterDebugCommands()
{
    if (!IsRunningCommandlet())
    {
        IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Debug.Inventory.Status"),
            TEXT("Check inventory UI connection status"),
            FConsoleCommandDelegate::CreateLambda([this]()
            {
                UE_LOG(LogTemp, Warning, TEXT("=== Inventory Debug Status ==="));
                
                if (UMedComUIManager* UIManager = GetUIManager())
                {
                    if (UMedComInventoryUIBridge* Bridge = UIManager->GetInventoryUIBridge())
                    {
                        bool bConnected = IMedComInventoryUIBridgeWidget::Execute_IsInventoryConnected(Bridge);
                        UE_LOG(LogTemp, Warning, TEXT("Inventory Bridge Connected: %s"), 
                            bConnected ? TEXT("YES") : TEXT("NO"));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Inventory Bridge: NOT CREATED"));
                    }
                    
                    bool bCharScreenVisible = UIManager->IsCharacterScreenVisible();
                    UE_LOG(LogTemp, Warning, TEXT("Character Screen Visible: %s"), 
                        bCharScreenVisible ? TEXT("YES") : TEXT("NO"));
                }
                
                if (ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>())
                {
                    if (UMedComInventoryComponent* Inv = PS->GetInventoryComponent())
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Inventory Component: Valid"));
                        UE_LOG(LogTemp, Warning, TEXT("Inventory Initialized: %s"), 
                            Inv->IsInventoryInitialized() ? TEXT("YES") : TEXT("NO"));
                        
                        FVector2D Size = Inv->GetInventorySize();
                        UE_LOG(LogTemp, Warning, TEXT("Inventory Size: %dx%d"), 
                            (int32)Size.X, (int32)Size.Y);
                        
                        TArray<FInventoryItemInstance> Items = Inv->GetAllItemInstances();
                        UE_LOG(LogTemp, Warning, TEXT("Items Count: %d"), Items.Num());
                        
                        float CurrentWeight = Inv->GetCurrentWeight_Implementation();
                        float MaxWeight = Inv->GetMaxWeight_Implementation();
                        UE_LOG(LogTemp, Warning, TEXT("Weight: %.1f / %.1f kg"), 
                            CurrentWeight, MaxWeight);
                    }
                }
                
                UE_LOG(LogTemp, Warning, TEXT("=== End Debug Status ==="));
            })
        );
        
        IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Debug.Equipment.Status"),
            TEXT("Print equipment connection and slots UI data (SIMPLIFIED)"),
            FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                [this](const TArray<FString>&, UWorld*)
                {
                    UMedComUIManager* UIManager = GetUIManager();
                    if (!UIManager)
                    {
                        UE_LOG(LogTemp, Error, TEXT("UIManager unavailable"));
                        return;
                    }

                    UMedComEquipmentUIBridge* Bridge = UIManager->GetEquipmentUIBridge();
                    if (!Bridge)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("EquipmentUIBridge not created; run Debug.Equipment.ForceInit"));
                        return;
                    }

                    const bool bConnected = IMedComEquipmentUIBridgeWidget::Execute_IsEquipmentConnected(Bridge);
                    UE_LOG(LogTemp, Log, TEXT("Bridge connected: %s (SIMPLIFIED FLOW - NO UIConnector)"), 
                        bConnected ? TEXT("YES") : TEXT("NO"));
                    
                    if (!bConnected)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Hint: open Character Screen -> Equipment tab to trigger connection"));
                        return;
                    }

                    TArray<FEquipmentSlotUIData> Slots;
                    const bool bGot = IMedComEquipmentUIBridgeWidget::Execute_GetEquipmentSlotsUIData(Bridge, Slots);
                    if (!bGot)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("GetEquipmentSlotsUIData returned false"));
                        return;
                    }

                    UE_LOG(LogTemp, Log, TEXT("Slots: %d"), Slots.Num());
                    for (int32 i = 0; i < Slots.Num(); ++i)
                    {
                        const FEquipmentSlotUIData& S = Slots[i];
                        UE_LOG(LogTemp, Log, TEXT("  [%d] %s  SlotType=%s"),
                            i,
                            S.bIsOccupied ? TEXT("Occupied") : TEXT("Empty"),
                            *S.SlotType.ToString()
                        );
                    }
                }
            ),
            ECVF_Default
        );
        
        IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Debug.Equipment.ForceInit"),
            TEXT("Force initialize equipment bridge (SIMPLIFIED - NO UIConnector)"),
            FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                [this](const TArray<FString>&, UWorld*)
                {
                    if (!IsLocalController())
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Not local controller"));
                        return;
                    }
                    
                    UE_LOG(LogTemp, Warning, TEXT("Forcing SIMPLIFIED equipment bridge initialization..."));
                    EnsureEquipmentBridgeInitialized();
                }
            ),
            ECVF_Default
        );
    }
}