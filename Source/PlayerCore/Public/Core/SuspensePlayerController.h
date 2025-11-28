// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "GameplayTagContainer.h"
#include "Input/SuspenseAbilityInputID.h"
#include "Interfaces/Core/ISuspenseController.h"
#include "SuspensePlayerController.generated.h"

// Forward declarations
class UInputMappingContext;
class UInputAction;
class ASuspenseCharacter;
class UAbilitySystemComponent;
class USuspenseEventManager;
class USuspenseUIManager;
class UUserWidget;
class USuspenseInventoryUIBridge;
class USuspenseEquipmentUIConnector;
/**
 * Player controller with integrated equipment and ability system support
 * Handles movement input and routes Jump/Sprint/Crouch to GAS
 * Manages HUD creation and lifecycle through UIManager
 * Implements ISuspenseController for weapon management
 *
 * ИЗМЕНЕНИЯ:
 * - Удалены методы OpenInventory/CloseInventory
 * - ToggleInventory теперь открывает Character Screen
 */
UCLASS()
class PLAYERCORE_API ASuspensePlayerController : public APlayerController, public ISuspenseController
{
    GENERATED_BODY()

public:
    ASuspensePlayerController();

    //================================================
    // HUD Management
    //================================================

    /**
     * Creates and initializes the main HUD widget
     * Called after possessing a pawn with valid PlayerState
     */
    UFUNCTION(BlueprintCallable, Category = "UI|HUD")
    void CreateHUD();

    /**
     * Destroys the HUD widget and cleans up references
     */
    UFUNCTION(BlueprintCallable, Category = "UI|HUD")
    void DestroyHUD();

    /**
     * Gets the main HUD widget instance
     * @return HUD widget or nullptr if not created
     */
    UFUNCTION(BlueprintCallable, Category = "UI|HUD")
    UUserWidget* GetHUDWidget() const;

    /**
     * Shows or hides the HUD
     * @param bShow Whether to show the HUD
     */
    UFUNCTION(BlueprintCallable, Category = "UI|HUD")
    void SetHUDVisibility(bool bShow);

    /**
     * Checks if HUD is currently created and active
     * @return true if HUD exists
     */
    UFUNCTION(BlueprintCallable, Category = "UI|HUD")
    bool IsHUDCreated() const;

    /**
     * Shows in-game menu (pause menu)
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Menu")
    void ShowInGameMenu();

    /**
     * Hides in-game menu and returns to game
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Menu")
    void HideInGameMenu();

    /**
     * Toggles Character Screen with inventory tab
     * ИЗМЕНЕНО: Теперь открывает Character Screen вместо отдельного инвентаря
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Menu")
    void ToggleInventory();

    //================================================
    // ISuspenseController Implementation
    //================================================

    virtual void NotifyWeaponChanged_Implementation(AActor* NewWeapon);
    virtual AActor* GetCurrentWeapon_Implementation() const override;
    virtual void NotifyWeaponStateChanged_Implementation(FGameplayTag WeaponState) override;
    virtual APawn* GetControlledPawn_Implementation() const override;
    virtual bool CanUseWeapon_Implementation() const override;
    virtual bool HasValidPawn_Implementation() const override;
    virtual void UpdateInputBindings_Implementation() override;
    virtual int32 GetInputPriority_Implementation() const override;

    /** Handle equipment state changes from delegate manager */
    void HandleEquipmentStateChange(FGameplayTag OldState, FGameplayTag NewState, bool bInterrupted);

    // Delegate manager access for interface
    virtual USuspenseEventManager* GetDelegateManager() const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void OnRep_PlayerState() override;

    //================================================
    // HUD Configuration
    //================================================

    /** Main HUD widget class to spawn - set this in Blueprint! */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HUD",
        meta = (DisplayName = "HUD Widget Class"))
    TSubclassOf<UUserWidget> MainHUDClass;

    /** Delay before creating HUD after possession */
    UPROPERTY(EditDefaultsOnly, Category = "UI|HUD",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HUDCreationDelay = 0.1f;

    /** Whether HUD should be created automatically on possess */
    UPROPERTY(EditDefaultsOnly, Category = "UI|HUD")
    bool bAutoCreateHUD = true;

    /** Whether to show FPS counter on HUD */
    UPROPERTY(EditDefaultsOnly, Category = "UI|HUD|Debug")
    bool bShowFPSCounter = false;

    /** Whether to show debug info on HUD */
    UPROPERTY(EditDefaultsOnly, Category = "UI|HUD|Debug")
    bool bShowDebugInfo = false;
 /** Show character screen with specific tab */
 UFUNCTION(BlueprintCallable, Category = "UI|Character")
 void ShowCharacterScreen(const FGameplayTag& DefaultTab = FGameplayTag());
private:
    //================================================
    // Enhanced Input System
    //================================================

    /** Sets up enhanced input mapping context */
    void SetupEnhancedInput();

    //================================================
    // Input Callback Methods
    //================================================

    /** Movement input handlers */
    void HandleMove(const FInputActionValue& Value);
    void HandleLook(const FInputActionValue& Value);

    /** Jump input handlers */
    void OnJumpPressed(const FInputActionValue& Value);
    void OnJumpReleased(const FInputActionValue& Value);

    /** Sprint input handlers */
    void OnSprintPressed(const FInputActionValue& Value);
    void OnSprintReleased(const FInputActionValue& Value);

    /** Crouch input handlers */
    void OnCrouchPressed(const FInputActionValue& Value);
    void OnCrouchReleased(const FInputActionValue& Value);

    /** Interaction input handler */
    void OnInteractPressed(const FInputActionValue& Value);

    /** Inventory toggle input handler */
    void OnInventoryToggle(const FInputActionValue& Value);

 /** Weapon switch input handlers */
 void OnNextWeapon(const FInputActionValue& Value);
 void OnPrevWeapon(const FInputActionValue& Value);
 void OnQuickSwitch(const FInputActionValue& Value);
 void OnWeaponSlot1(const FInputActionValue& Value);
 void OnWeaponSlot2(const FInputActionValue& Value);
 void OnWeaponSlot3(const FInputActionValue& Value);
 void OnWeaponSlot4(const FInputActionValue& Value);
 void OnWeaponSlot5(const FInputActionValue& Value);

    //================================================
    // Gameplay Ability System Integration
    //================================================

    /** Activates ability through GAS with specified tag and pressed state */
    void ActivateAbility(const FGameplayTag& Tag, bool bPressed);

    /** Gets the ability system component from character */
    UAbilitySystemComponent* GetCharacterASC() const;

    /** Gets the MedCom character reference */
    ASuspenseCharacter* GetMedComCharacter() const;

    //================================================
    // HUD Internal Methods
    //================================================

    /**
     * Tries to create HUD if all prerequisites are met
     * Checks for valid PlayerState and UIManager
     */
    void TryCreateHUD();

    /**
     * Updates HUD with current player state data
     */
    void UpdateHUDData();

    /**
     * Handles attribute changes for HUD updates
     * @param AttributeTag Tag of the changed attribute
     * @param NewValue New attribute value
     * @param OldValue Previous attribute value
     */
    void HandleAttributeChanged(const FGameplayTag& AttributeTag, float NewValue, float OldValue);

    /**
     * Gets the UI Manager instance
     * @return UI Manager or nullptr
     */
    USuspenseUIManager* GetUIManager() const;

    //================================================
    // Inventory Management
    //================================================

    /**
     * Обеспечить инициализацию Bridge инвентаря при первом использовании
     * Отложенная инициализация - вызывается только при первом открытии инвентаря
     */
    void EnsureInventoryBridgeInitialized();

    /**
     * Подключить инвентарь к Bridge
     * @param Bridge Мост для подключения инвентаря
     */
    void ConnectInventoryToBridge(USuspenseInventoryUIBridge* Bridge);

    /**
     * Обработчик запросов инициализации инвентаря
     * Теперь не инициализирует сразу, а только логирует запрос
     * @param Source Источник запроса
     * @param EventTag Тег события
     * @param EventData Данные события
     */
    void HandleInventoryInitializationRequest(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData);

    //================================================
    // Weapon State Management
    //================================================

    /** Currently active weapon actor */
    UPROPERTY(Transient)
    AActor* CurrentWeapon = nullptr;

    /** Current weapon state for tracking */
    UPROPERTY(Transient)
    FGameplayTag CurrentWeaponState;

    //================================================
    // UI State
    //================================================

    /** Active HUD widget instance reference */
    UPROPERTY(Transient)
    UUserWidget* MainHUDWidget = nullptr;

    /** Timer handle for delayed HUD creation */
    FTimerHandle HUDCreationTimerHandle;

    /** Cached UI Manager reference */
    UPROPERTY(Transient)
    USuspenseUIManager* CachedUIManager = nullptr;

    //================================================
    // Input Assets Configuration
    //================================================

    /** Default input mapping context for enhanced input */
    UPROPERTY(EditDefaultsOnly, Category="Input|Context")
    UInputMappingContext* DefaultContext;

    /** Movement input action */
    UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
    UInputAction* IA_Move;

    /** Look input action */
    UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
    UInputAction* IA_Look;

    /** Jump input action */
    UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
    UInputAction* IA_Jump;

    /** Sprint input action */
    UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
    UInputAction* IA_Sprint;

    /** Crouch input action */
    UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
    UInputAction* IA_Crouch;

    /** Interaction input action */
    UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
    UInputAction* IA_Interact;

    /** Inventory toggle input action */
    UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
    UInputAction* IA_OpenInventory;

 /** Weapon switch input actions */
 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_NextWeapon;

 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_PrevWeapon;

 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_QuickSwitch;

 /** Direct weapon slot selection */
 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_WeaponSlot1;

 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_WeaponSlot2;

 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_WeaponSlot3;

 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_WeaponSlot4;

 UPROPERTY(EditDefaultsOnly, Category="Input|Actions|Weapon")
 UInputAction* IA_WeaponSlot5;

    //================================================
    // Event Subscription Handles
    //================================================

    /** Handle for equipment state change events */
    FDelegateHandle EquipmentStateChangeHandle;

    /** Handle for attribute change events */
    FDelegateHandle AttributeChangeHandle;

    /** Handles for various UI-related events */
    TArray<FDelegateHandle> UIEventHandles;

    // Debug commands
    void RegisterDebugCommands();

 FDelegateHandle InventoryInitHandle;
 FDelegateHandle EquipmentInitHandle;
 FDelegateHandle LoadoutReadyHandle;
 FDelegateHandle LoadoutFailedHandle;

 // Флаг для предотвращения повторной инициализации
 bool bInventoryBridgeReady = false;

 // Обработчики событий
 void OnInventoryInitialized(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData);
 void OnLoadoutReady(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData);
 void OnLoadoutFailed(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData);

 // Equipment UI Bridge Management
 /** Flag indicating if equipment bridge is ready */
 bool bEquipmentBridgeReady = false;

 /** Ensure equipment UI bridge is initialized and connected */
 void EnsureEquipmentBridgeInitialized();

 /** Connect equipment component to UI bridge */
 void ConnectEquipmentToBridge(class USuspenseEquipmentUIBridge* Bridge);

 /** Handle equipment initialization request from event system */
 void HandleEquipmentInitializationRequest(const UObject* Source, const FGameplayTag& EventTag, const FString& EventData);

};
