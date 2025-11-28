// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/HUD/SuspenseMainHUDWidget.h"
#include "Widgets/HUD/SuspenseHealthStaminaWidget.h"
#include "Widgets/HUD/SuspenseCrosshairWidget.h"
#include "Widgets/HUD/SuspenseWeaponUIWidget.h"
#include "Widgets/Inventory/SuspenseInventoryWidget.h"
#include "Widgets/Tabs/SuspenseUpperTabBar.h"
#include "Interfaces/Core/ISuspenseAttributeProvider.h"
#include "Interfaces/UI/ISuspenseInventoryUIBridge.h"
#include "Components/SuspenseInventoryUIBridge.h"
#include "Components/TextBlock.h"
#include "Delegates/SuspenseEventManager.h"
#include "Math/UnrealMathUtility.h"
#include "GameFramework/PlayerController.h"

USuspenseMainHUDWidget::USuspenseMainHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Enable tick for dynamic updates
    bEnableTick = true;

    // Set the widget tag for identification
    WidgetTag = FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.Main"));

    // Initialize pointers
    OwningPawn = nullptr;
    bInventoryBridgeInitialized = false;
}

void USuspenseMainHUDWidget::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();

    // First, validate that all required widgets are bound
    if (!ValidateWidgetBindings())
    {
        UE_LOG(LogTemp, Error, TEXT("[MainHUDWidget] Failed to validate widget bindings! Check Blueprint setup."));
        UE_LOG(LogTemp, Error, TEXT("[MainHUDWidget] Make sure to bind required widgets in the Blueprint editor."));
        return;
    }

    // Initialize all child widgets with proper settings
    InitializeChildWidgets();

    // Subscribe to game events
    SetupEventSubscriptions();

    // Apply default HUD settings
    SetHUDOpacity_Implementation(DefaultHUDOpacity);

    // ВАЖНО: Принудительно скрываем инвентарь и CharacterScreen при инициализации
    if (CharacterScreen)
    {
        CharacterScreen->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Character screen properly hidden on initialization"));
    }

    if (InventoryWidget)
    {
        InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Legacy inventory widget properly hidden on initialization"));
    }

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Widget initialized successfully (inventory bridge will be initialized on first use)"));
}

void USuspenseMainHUDWidget::UninitializeWidget_Implementation()
{
    // Clean up in reverse order of initialization
    CleanupHUD_Implementation();
    ClearEventSubscriptions();

    Super::UninitializeWidget_Implementation();

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Widget uninitialized"));
}

void USuspenseMainHUDWidget::UpdateWidget_Implementation(float DeltaTime)
{
    Super::UpdateWidget_Implementation(DeltaTime);

    // Add any per-frame HUD updates here
    // For example: updating timers, animations, etc.
}

void USuspenseMainHUDWidget::SetupForPlayer_Implementation(APawn* Character)
{
    OwningPawn = Character;

    if (!Character)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] SetupForPlayer called with null character"));
        return;
    }

    // If the character provides attributes, set up the provider
    if (Character->GetClass()->ImplementsInterface(USuspenseAttributeProvider::StaticClass()))
    {
        TScriptInterface<ISuspenseAttributeProvider> Provider;
        Provider.SetObject(Character);
        Provider.SetInterface(Cast<ISuspenseAttributeProvider>(Character));
        SetupWithProvider(Provider);
    }

    // НЕ вызываем RequestInventoryInitialization здесь!
    // Инициализация будет происходить только при первом открытии инвентаря

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Setup complete for player: %s (inventory will be initialized on demand)"), *Character->GetName());
}

void USuspenseMainHUDWidget::SetupWithProvider(TScriptInterface<ISuspenseAttributeProvider> Provider)
{
    if (!Provider.GetInterface())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] SetupWithProvider called with invalid provider"));
        return;
    }

    AttributeProvider = Provider;

    // Connect the health/stamina widget to the provider
    if (HealthStaminaWidget)
    {
        HealthStaminaWidget->InitializeWithProvider(AttributeProvider);
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Connected health/stamina widget to attribute provider"));
    }

    bIsSetup = true;
}

void USuspenseMainHUDWidget::CleanupHUD_Implementation()
{
    // Clear attribute provider
    AttributeProvider.SetInterface(nullptr);
    AttributeProvider.SetObject(nullptr);

    // Disconnect widgets from their data sources
    if (HealthStaminaWidget)
    {
        HealthStaminaWidget->ClearProvider();
    }

    if (WeaponInfoWidget)
    {
        WeaponInfoWidget->ClearWeapon_Implementation();
    }

    // Hide any visible prompts
    HideInteractionPrompt_Implementation();

    // Hide character screen if it's open
    if (CharacterScreen && CharacterScreen->GetVisibility() != ESlateVisibility::Collapsed)
    {
        HideCharacterScreen();
    }

    // Hide inventory if it's open
    if (InventoryWidget && InventoryWidget->IsVisible())
    {
        HideInventory_Implementation();
    }

    // Reset state
    OwningPawn = nullptr;
    bIsSetup = false;
    bInventoryBridgeInitialized = false;

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] HUD cleaned up"));
}

UUserWidget* USuspenseMainHUDWidget::GetHealthStaminaWidget_Implementation() const
{
    return Cast<UUserWidget>(HealthStaminaWidget);
}

UUserWidget* USuspenseMainHUDWidget::GetCrosshairWidget_Implementation() const
{
    return Cast<UUserWidget>(DynamicCrosshair);
}

UUserWidget* USuspenseMainHUDWidget::GetWeaponInfoWidget_Implementation() const
{
    return Cast<UUserWidget>(WeaponInfoWidget);
}

UUserWidget* USuspenseMainHUDWidget::GetInventoryWidget_Implementation() const
{
    // Если есть CharacterScreen, возвращаем инвентарь из него
    if (CharacterScreen)
    {
        USuspenseUpperTabBar* TabBar = CharacterScreen->GetTabBar();
        if (TabBar)
        {
            // Находим индекс вкладки инвентаря
            for (int32 i = 0; i < TabBar->GetTabCount_Implementation(); i++)
            {
                FSuspenseTabConfig Config = TabBar->GetTabConfig(i);
                if (Config.TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"))))
                {
                    return TabBar->GetTabContent_Implementation(i);
                }
            }
        }
    }

    // Fallback на legacy виджет
    return Cast<UUserWidget>(InventoryWidget);
}

void USuspenseMainHUDWidget::ShowCombatElements_Implementation(bool bShow)
{
    bCombatElementsVisible = bShow;

    // Show/hide crosshair
    if (DynamicCrosshair)
    {
        DynamicCrosshair->SetCrosshairVisibility_Implementation(bShow);
    }

    // Show/hide weapon info
    if (WeaponInfoWidget)
    {
        WeaponInfoWidget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    }

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Combat elements visibility set to: %s"),
        bShow ? TEXT("Visible") : TEXT("Hidden"));
}

void USuspenseMainHUDWidget::ShowNonCombatElements_Implementation(bool bShow)
{
    bNonCombatElementsVisible = bShow;

    // Show/hide health and stamina
    if (HealthStaminaWidget)
    {
        HealthStaminaWidget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    }

    // You can add other non-combat elements here
    // For example: quest tracker, minimap, etc.

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Non-combat elements visibility set to: %s"),
        bShow ? TEXT("Visible") : TEXT("Hidden"));
}

void USuspenseMainHUDWidget::SetHUDOpacity_Implementation(float Opacity)
{
    float ClampedOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);

    // ВАЖНО: НЕ применяем opacity к самому root виджету,
    // а только к конкретным дочерним элементам
    // Это предотвратит влияние на тултипы

    // Применяем opacity только к конкретным виджетам HUD
    if (HealthStaminaWidget)
    {
        HealthStaminaWidget->SetRenderOpacity(ClampedOpacity);
    }

    if (DynamicCrosshair)
    {
        DynamicCrosshair->SetRenderOpacity(ClampedOpacity);
    }

    if (WeaponInfoWidget)
    {
        WeaponInfoWidget->SetRenderOpacity(ClampedOpacity);
    }

    if (InteractionPrompt)
    {
        InteractionPrompt->SetRenderOpacity(ClampedOpacity);
    }

    // НЕ применяем к CharacterScreen и InventoryWidget - они управляют своей прозрачностью

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] HUD elements opacity set to: %.2f"), ClampedOpacity);
}

void USuspenseMainHUDWidget::ShowInteractionPrompt_Implementation(const FText& PromptText)
{
    if (InteractionPrompt)
    {
        InteractionPrompt->SetText(PromptText);
        InteractionPrompt->SetVisibility(ESlateVisibility::Visible);

        // You could add fade-in animation here

        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Showing interaction prompt: %s"), *PromptText.ToString());
    }
}

void USuspenseMainHUDWidget::HideInteractionPrompt_Implementation()
{
    if (InteractionPrompt)
    {
        // You could add fade-out animation here
        InteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);

        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Hiding interaction prompt"));
    }
}

void USuspenseMainHUDWidget::ShowInventory_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] ShowInventory_Implementation called"));

    // Проверяем наличие CharacterScreen
    if (CharacterScreen)
    {
        ShowCharacterScreenWithTab(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory")));
        return;
    }

    // Fallback: старая логика для обратной совместимости
    if (InventoryWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Using legacy inventory widget"));

        // Инициализируем Bridge
        EnsureInventoryBridgeInitialized();

        // Инициализируем виджет данными через Bridge
        if (ISuspenseInventoryUIBridgeInterface* Bridge = ISuspenseInventoryUIBridgeInterface::GetInventoryUIBridge(this))
        {
            UObject* BridgeObject = Cast<UObject>(Bridge);
            if (BridgeObject)
            {
                USuspenseInventoryUIBridge* ConcreteBridge = Cast<USuspenseInventoryUIBridge>(BridgeObject);
                if (ConcreteBridge)
                {
                    ConcreteBridge->InitializeInventoryWidgetWithData(InventoryWidget);
                }
            }
        }

        InventoryWidget->SetVisibility(ESlateVisibility::Visible);
        InventoryWidget->ForceLayoutPrepass();
        OnInventoryVisibilityChanged(true);

        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Legacy inventory widget shown"));
        return;
    }

    // Final fallback: используем Bridge напрямую
    UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] No UI widgets found, using Bridge fallback"));
    if (ISuspenseInventoryUIBridgeInterface* Bridge = ISuspenseInventoryUIBridgeInterface::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            ISuspenseInventoryUIBridgeInterface::Execute_ShowInventoryUI(BridgeObject);
            OnInventoryVisibilityChanged(true);
        }
    }
}

void USuspenseMainHUDWidget::HideInventory_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] HideInventory_Implementation called"));

    // Проверяем CharacterScreen
    if (CharacterScreen && CharacterScreen->GetVisibility() != ESlateVisibility::Collapsed)
    {
        HideCharacterScreen();
        return;
    }

    // Legacy inventory widget
    if (InventoryWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Hiding legacy inventory widget"));
        InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
        OnInventoryVisibilityChanged(false);
        return;
    }

    // Fallback: используем Bridge
    if (ISuspenseInventoryUIBridgeInterface* Bridge = ISuspenseInventoryUIBridgeInterface::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            ISuspenseInventoryUIBridgeInterface::Execute_HideInventoryUI(BridgeObject);
            OnInventoryVisibilityChanged(false);
        }
    }
}

void USuspenseMainHUDWidget::ToggleInventory_Implementation()
{
    if (IsInventoryVisible_Implementation())
    {
        HideInventory_Implementation();
    }
    else
    {
        ShowInventory_Implementation();
    }
}

bool USuspenseMainHUDWidget::IsInventoryVisible_Implementation() const
{
    // Проверяем CharacterScreen
    if (CharacterScreen)
    {
        return CharacterScreen->GetVisibility() == ESlateVisibility::Visible;
    }

    // Legacy inventory widget
    if (InventoryWidget)
    {
        return InventoryWidget->GetVisibility() == ESlateVisibility::Visible;
    }

    // Fallback to Bridge
    if (ISuspenseInventoryUIBridgeInterface* Bridge = ISuspenseInventoryUIBridgeInterface::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            return ISuspenseInventoryUIBridgeInterface::Execute_IsInventoryUIVisible(BridgeObject);
        }
    }

    return false;
}

void USuspenseMainHUDWidget::ShowCharacterScreenWithTab(const FGameplayTag& TabTag)
{
    if (!CharacterScreen)
    {
        UE_LOG(LogTemp, Error, TEXT("[MainHUDWidget] CharacterScreen not bound! Please bind it in Blueprint."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Showing CharacterScreen with tab: %s"), *TabTag.ToString());

    // Показываем экран
    CharacterScreen->SetVisibility(ESlateVisibility::Visible);

    // Активируем через интерфейс
    if (CharacterScreen->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
    {
        ISuspenseScreen::Execute_OnScreenActivated(CharacterScreen);
    }

    // Открываем нужную вкладку
    if (TabTag.IsValid())
    {
        CharacterScreen->OpenTabByTag(TabTag);
    }

    // Уведомляем о смене видимости
    OnCharacterScreenVisibilityChanged(true);

    // Особое уведомление для инвентаря для совместимости
    if (TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"))))
    {
        OnInventoryVisibilityChanged(true);

        // Уведомляем через Bridge для совместимости
        if (ISuspenseInventoryUIBridgeInterface* Bridge = ISuspenseInventoryUIBridgeInterface::GetInventoryUIBridge(this))
        {
            UObject* BridgeObject = Cast<UObject>(Bridge);
            if (BridgeObject)
            {
                ISuspenseInventoryUIBridgeInterface::Execute_ShowCharacterScreenWithTab(BridgeObject, TabTag);
            }
        }
    }
}

void USuspenseMainHUDWidget::HideCharacterScreen()
{
    if (!CharacterScreen)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Hiding CharacterScreen"));

    // Проверяем, какая вкладка была открыта для правильного уведомления
    bool bWasInventoryOpen = false;
    if (USuspenseUpperTabBar* TabBar = CharacterScreen->GetTabBar())
    {
        int32 CurrentIndex = TabBar->GetSelectedTabIndex_Implementation();
        if (CurrentIndex >= 0)
        {
            FSuspenseTabConfig Config = TabBar->GetTabConfig(CurrentIndex);
            bWasInventoryOpen = Config.TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory")));
        }
    }

    // Деактивируем экран
    if (CharacterScreen->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
    {
        ISuspenseScreen::Execute_OnScreenDeactivated(CharacterScreen);
    }

    CharacterScreen->SetVisibility(ESlateVisibility::Collapsed);

    // Уведомляем о смене видимости
    OnCharacterScreenVisibilityChanged(false);

    // Особое уведомление для инвентаря
    if (bWasInventoryOpen)
    {
        OnInventoryVisibilityChanged(false);
    }

    // Уведомляем через Bridge
    if (ISuspenseInventoryUIBridgeInterface* Bridge = ISuspenseInventoryUIBridgeInterface::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            ISuspenseInventoryUIBridgeInterface::Execute_HideCharacterScreen(BridgeObject);
        }
    }
}

void USuspenseMainHUDWidget::ToggleCharacterScreen()
{
    if (IsCharacterScreenVisible())
    {
        HideCharacterScreen();
    }
    else
    {
        // Открываем с последней вкладкой или дефолтной
        ShowCharacterScreenWithTab(FGameplayTag());
    }
}

bool USuspenseMainHUDWidget::IsCharacterScreenVisible() const
{
    return CharacterScreen && CharacterScreen->GetVisibility() == ESlateVisibility::Visible;
}

void USuspenseMainHUDWidget::RequestInventoryInitialization_Implementation()
{
    // Логируем запрос инициализации
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Inventory initialization requested"));

    // ВАЖНО: Мы больше не используем события с GameplayTag,
    // так как инициализация теперь происходит напрямую при показе инвентаря
    // Этот метод остается для совместимости с интерфейсом,
    // но реальная инициализация происходит в ShowInventory_Implementation()

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Inventory will be initialized on first show"));
}

void USuspenseMainHUDWidget::EnsureInventoryBridgeInitialized()
{
    if (bInventoryBridgeInitialized)
    {
        return; // Уже инициализирован
    }

    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Initializing inventory bridge on first use"));

    // Запрашиваем инициализацию через событие
    RequestInventoryInitialization_Implementation();

    // Отмечаем, что инициализация была запрошена
    bInventoryBridgeInitialized = true;
}

void USuspenseMainHUDWidget::OnInventoryVisibilityChanged(bool bIsVisible)
{
    // Call the Blueprint event if implemented
    K2_OnInventoryVisibilityChanged(bIsVisible);

    // Отправляем события через систему событий
    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        FGameplayTag EventTag = bIsVisible ?
            FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.Opened")) :
            FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.Closed"));
        EventManager->NotifyUIEvent(this, EventTag, TEXT(""));
    }
}

void USuspenseMainHUDWidget::OnCharacterScreenVisibilityChanged(bool bIsVisible)
{
    // Call the Blueprint event if implemented
    K2_OnCharacterScreenVisibilityChanged(bIsVisible);

    // Отправляем события через систему событий
    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        FGameplayTag EventTag = bIsVisible ?
            FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Opened")) :
            FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Closed"));
        EventManager->NotifyUIEvent(this, EventTag, TEXT(""));

        // Также уведомляем о открытии/закрытии экрана персонажа
        if (bIsVisible)
        {
            EventManager->NotifyCharacterScreenOpened(CharacterScreen, FGameplayTag());
        }
        else
        {
            EventManager->NotifyCharacterScreenClosed(CharacterScreen);
        }
    }
}

// Backward compatibility methods
void USuspenseMainHUDWidget::SetCurrentHealth_UI(float CurrentHealth)
{
    if (HealthStaminaWidget)
    {
        float MaxHealth = HealthStaminaWidget->GetMaxHealth();
        HealthStaminaWidget->UpdateHealth_Implementation(CurrentHealth, MaxHealth);
    }
}

void USuspenseMainHUDWidget::SetMaxHealth_UI(float MaxHealth)
{
    if (HealthStaminaWidget)
    {
        float CurrentHealth = HealthStaminaWidget->GetCurrentHealth();
        HealthStaminaWidget->UpdateHealth_Implementation(CurrentHealth, MaxHealth);
    }
}

void USuspenseMainHUDWidget::SetHealthPercentage_UI(float HealthPercentage)
{
    if (HealthStaminaWidget)
    {
        float MaxHealth = HealthStaminaWidget->GetMaxHealth();
        float CurrentHealth = MaxHealth * HealthPercentage;
        HealthStaminaWidget->UpdateHealth_Implementation(CurrentHealth, MaxHealth);
    }
}

float USuspenseMainHUDWidget::GetHealthPercentage() const
{
    if (HealthStaminaWidget)
    {
        return HealthStaminaWidget->GetHealthPercentage_Implementation();
    }
    return 1.0f;
}

void USuspenseMainHUDWidget::SetCurrentStamina_UI(float CurrentStamina)
{
    if (HealthStaminaWidget)
    {
        float MaxStamina = HealthStaminaWidget->GetMaxStamina();
        HealthStaminaWidget->UpdateStamina_Implementation(CurrentStamina, MaxStamina);
    }
}

void USuspenseMainHUDWidget::SetMaxStamina_UI(float MaxStamina)
{
    if (HealthStaminaWidget)
    {
        float CurrentStamina = HealthStaminaWidget->GetCurrentStamina();
        HealthStaminaWidget->UpdateStamina_Implementation(CurrentStamina, MaxStamina);
    }
}

void USuspenseMainHUDWidget::SetStaminaPercentage_UI(float StaminaPercentage)
{
    if (HealthStaminaWidget)
    {
        float MaxStamina = HealthStaminaWidget->GetMaxStamina();
        float CurrentStamina = MaxStamina * StaminaPercentage;
        HealthStaminaWidget->UpdateStamina_Implementation(CurrentStamina, MaxStamina);
    }
}

float USuspenseMainHUDWidget::GetStaminaPercentage() const
{
    if (HealthStaminaWidget)
    {
        return HealthStaminaWidget->GetStaminaPercentage_Implementation();
    }
    return 1.0f;
}

void USuspenseMainHUDWidget::SetCrosshairVisibility(bool bVisible)
{
    if (DynamicCrosshair)
    {
        DynamicCrosshair->SetCrosshairVisibility_Implementation(bVisible);
    }
}

void USuspenseMainHUDWidget::InitializeChildWidgets()
{
    // Initialize health/stamina widget
    if (HealthStaminaWidget)
    {
        FGameplayTag CurrentTag = ISuspenseUIWidget::Execute_GetWidgetTag(HealthStaminaWidget);
        if (!CurrentTag.IsValid())
        {
            ISuspenseUIWidget::Execute_SetWidgetTag(
                HealthStaminaWidget,
                FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.HealthBar"))
            );
        }
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] HealthStaminaWidget initialized"));
    }

    // Initialize crosshair
    if (DynamicCrosshair)
    {
        FGameplayTag CurrentTag = ISuspenseUIWidget::Execute_GetWidgetTag(DynamicCrosshair);
        if (!CurrentTag.IsValid())
        {
            ISuspenseUIWidget::Execute_SetWidgetTag(
                DynamicCrosshair,
                FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.Crosshair"))
            );
        }

        // Set initial visibility
        ISuspenseCrosshairWidgetInterface::Execute_SetCrosshairVisibility(
            DynamicCrosshair,
            bCombatElementsVisible
        );
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] DynamicCrosshair initialized"));
    }

    // Initialize weapon info widget
    if (WeaponInfoWidget)
    {
        FGameplayTag CurrentTag = ISuspenseUIWidget::Execute_GetWidgetTag(WeaponInfoWidget);
        if (!CurrentTag.IsValid())
        {
            ISuspenseUIWidget::Execute_SetWidgetTag(
                WeaponInfoWidget,
                FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.WeaponInfo"))
            );
        }

        WeaponInfoWidget->SetVisibility(
            bCombatElementsVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden
        );
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] WeaponInfoWidget initialized"));
    }

    // Initialize CharacterScreen
    if (CharacterScreen)
    {
        FGameplayTag CurrentTag = ISuspenseUIWidget::Execute_GetWidgetTag(CharacterScreen);
        if (!CurrentTag.IsValid())
        {
            ISuspenseUIWidget::Execute_SetWidgetTag(
                CharacterScreen,
                FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Character"))
            );
        }

        // КРИТИЧЕСКИ ВАЖНО: CharacterScreen начинает скрытым
        CharacterScreen->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] CharacterScreen found and properly hidden"));
    }

    // Initialize legacy inventory widget
    if (InventoryWidget)
    {
        FGameplayTag CurrentTag = ISuspenseUIWidget::Execute_GetWidgetTag(InventoryWidget);
        if (!CurrentTag.IsValid())
        {
            ISuspenseUIWidget::Execute_SetWidgetTag(
                InventoryWidget,
                FGameplayTag::RequestGameplayTag(TEXT("UI.Container.Inventory"))
            );
        }

        // КРИТИЧЕСКИ ВАЖНО: Inventory starts hidden
        InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Legacy InventoryWidget found and properly hidden"));
    }

    // Initialize interaction prompt
    if (InteractionPrompt)
    {
        InteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] InteractionPrompt initialized"));
    }
}

void USuspenseMainHUDWidget::SetupEventSubscriptions()
{
    if (USuspenseEventManager* EventManager = USuspenseBaseWidget::GetDelegateManager())
    {
        // Subscribe to weapon changes
        WeaponChangedHandle = EventManager->SubscribeToActiveWeaponChanged(
            [this](AActor* NewWeapon)
            {
                OnActiveWeaponChanged(NewWeapon);
            });

        // Subscribe to crosshair updates
        CrosshairUpdateHandle = EventManager->SubscribeToCrosshairUpdated(
            [this](float Spread, float Recoil)
            {
                OnCrosshairUpdateRequested(Spread, Recoil);
            });

        // Subscribe to crosshair color changes
        CrosshairColorHandle = EventManager->SubscribeToCrosshairColorChanged(
            [this](FLinearColor NewColor)
            {
                OnCrosshairColorChanged(NewColor);
            });

        // Subscribe to general notifications
        NotificationHandle = EventManager->SubscribeToNotification(
            [this](const FString& Message, float Duration)
            {
                OnNotificationReceived(Message, Duration);
            });

        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Event subscriptions setup"));
    }
}

void USuspenseMainHUDWidget::ClearEventSubscriptions()
{
    if (USuspenseEventManager* EventManager = USuspenseBaseWidget::GetDelegateManager())
    {
        if (WeaponChangedHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(WeaponChangedHandle);
            WeaponChangedHandle.Reset();
        }

        if (CrosshairUpdateHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(CrosshairUpdateHandle);
            CrosshairUpdateHandle.Reset();
        }

        if (CrosshairColorHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(CrosshairColorHandle);
            CrosshairColorHandle.Reset();
        }

        if (NotificationHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(NotificationHandle);
            NotificationHandle.Reset();
        }

        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Event subscriptions cleared"));
    }
}

bool USuspenseMainHUDWidget::ValidateWidgetBindings() const
{
    bool bValid = true;

    // HealthStaminaWidget is required
    if (!HealthStaminaWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[MainHUDWidget] HealthStaminaWidget is not bound! This is REQUIRED."));
        UE_LOG(LogTemp, Error, TEXT("[MainHUDWidget] Add a health/stamina widget in Blueprint and bind it"));
        bValid = false;
    }

    // Other widgets are optional but we log warnings
    if (!DynamicCrosshair)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] DynamicCrosshair is not bound. Combat features will be limited."));
    }

    if (!WeaponInfoWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] WeaponInfoWidget is not bound. Weapon info will not be displayed."));
    }

    if (!CharacterScreen)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] CharacterScreen is not bound. Using legacy inventory system."));
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] For best results, bind a USuspenseCharacterScreen in Blueprint"));
    }

    if (!InventoryWidget && !CharacterScreen)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] Neither CharacterScreen nor InventoryWidget is bound."));
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] Bridge system will be used as fallback."));
    }

    if (!InteractionPrompt)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] InteractionPrompt is not bound. Interaction prompts will not be displayed."));
    }

    return bValid;
}

void USuspenseMainHUDWidget::OnActiveWeaponChanged(AActor* NewWeapon)
{
    if (WeaponInfoWidget)
    {
        // Since we can't cast to specific weapon type due to module separation,
        // we pass nullptr and let the widget handle it through events
        WeaponInfoWidget->SetWeapon_Implementation(nullptr);
    }

    // Auto-hide combat elements if no weapon
    if (bAutoHideCombatElements)
    {
        bool bHasWeapon = (NewWeapon != nullptr);
        ShowCombatElements_Implementation(bHasWeapon && bCombatElementsVisible);
    }

    UE_LOG(LogTemp, Verbose, TEXT("[MainHUDWidget] Active weapon changed"));
}

void USuspenseMainHUDWidget::OnCrosshairUpdateRequested(float Spread, float Recoil)
{
    if (DynamicCrosshair)
    {
        DynamicCrosshair->UpdateCrosshair_Implementation(Spread, Recoil, false);
    }
}

void USuspenseMainHUDWidget::OnCrosshairColorChanged(FLinearColor NewColor)
{
    if (DynamicCrosshair)
    {
        DynamicCrosshair->SetCrosshairColor_Implementation(NewColor);
    }
}

void USuspenseMainHUDWidget::OnNotificationReceived(const FString& Message, float Duration)
{
    // Show as interaction prompt
    ShowInteractionPrompt_Implementation(FText::FromString(Message));

    // Auto-hide after duration
    if (Duration > 0.0f)
    {
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle,
            [this]()
            {
                HideInteractionPrompt_Implementation();
            },
            Duration, false);
    }
}

void USuspenseMainHUDWidget::OnInventoryCloseRequested()
{
    HideInventory_Implementation();
}
