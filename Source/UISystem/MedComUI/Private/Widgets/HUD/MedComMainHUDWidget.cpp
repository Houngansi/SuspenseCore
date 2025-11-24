// Copyright MedCom Team. All Rights Reserved.

#include "Widgets/HUD/MedComMainHUDWidget.h"
#include "Widgets/HUD/MedComHealthStaminaWidget.h"
#include "Widgets/HUD/MedComCrosshairWidget.h"
#include "Widgets/HUD/MedComWeaponUIWidget.h"
#include "Widgets/Inventory/MedComInventoryWidget.h"
#include "Widgets/Tabs/MedComUpperTabBar.h"
#include "Interfaces/Core/IMedComAttributeProviderInterface.h"
#include "Interfaces/UI/IMedComInventoryUIBridgeWidget.h"
#include "Components/MedComInventoryUIBridge.h"
#include "Components/TextBlock.h"
#include "Delegates/EventDelegateManager.h"
#include "Math/UnrealMathUtility.h"
#include "GameFramework/PlayerController.h"

UMedComMainHUDWidget::UMedComMainHUDWidget(const FObjectInitializer& ObjectInitializer)
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

void UMedComMainHUDWidget::InitializeWidget_Implementation()
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

void UMedComMainHUDWidget::UninitializeWidget_Implementation()
{
    // Clean up in reverse order of initialization
    CleanupHUD_Implementation();
    ClearEventSubscriptions();
    
    Super::UninitializeWidget_Implementation();
    
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Widget uninitialized"));
}

void UMedComMainHUDWidget::UpdateWidget_Implementation(float DeltaTime)
{
    Super::UpdateWidget_Implementation(DeltaTime);
    
    // Add any per-frame HUD updates here
    // For example: updating timers, animations, etc.
}

void UMedComMainHUDWidget::SetupForPlayer_Implementation(APawn* Character)
{
    OwningPawn = Character;
    
    if (!Character)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] SetupForPlayer called with null character"));
        return;
    }
    
    // If the character provides attributes, set up the provider
    if (Character->GetClass()->ImplementsInterface(UMedComAttributeProviderInterface::StaticClass()))
    {
        TScriptInterface<IMedComAttributeProviderInterface> Provider;
        Provider.SetObject(Character);
        Provider.SetInterface(Cast<IMedComAttributeProviderInterface>(Character));
        SetupWithProvider(Provider);
    }
    
    // НЕ вызываем RequestInventoryInitialization здесь!
    // Инициализация будет происходить только при первом открытии инвентаря
    
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Setup complete for player: %s (inventory will be initialized on demand)"), *Character->GetName());
}

void UMedComMainHUDWidget::SetupWithProvider(TScriptInterface<IMedComAttributeProviderInterface> Provider)
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

void UMedComMainHUDWidget::CleanupHUD_Implementation()
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

UUserWidget* UMedComMainHUDWidget::GetHealthStaminaWidget_Implementation() const
{
    return Cast<UUserWidget>(HealthStaminaWidget);
}

UUserWidget* UMedComMainHUDWidget::GetCrosshairWidget_Implementation() const
{
    return Cast<UUserWidget>(DynamicCrosshair);
}

UUserWidget* UMedComMainHUDWidget::GetWeaponInfoWidget_Implementation() const
{
    return Cast<UUserWidget>(WeaponInfoWidget);
}

UUserWidget* UMedComMainHUDWidget::GetInventoryWidget_Implementation() const
{
    // Если есть CharacterScreen, возвращаем инвентарь из него
    if (CharacterScreen)
    {
        UMedComUpperTabBar* TabBar = CharacterScreen->GetTabBar();
        if (TabBar)
        {
            // Находим индекс вкладки инвентаря
            for (int32 i = 0; i < TabBar->GetTabCount_Implementation(); i++)
            {
                FMedComTabConfig Config = TabBar->GetTabConfig(i);
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

void UMedComMainHUDWidget::ShowCombatElements_Implementation(bool bShow)
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

void UMedComMainHUDWidget::ShowNonCombatElements_Implementation(bool bShow)
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

void UMedComMainHUDWidget::SetHUDOpacity_Implementation(float Opacity)
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

void UMedComMainHUDWidget::ShowInteractionPrompt_Implementation(const FText& PromptText)
{
    if (InteractionPrompt)
    {
        InteractionPrompt->SetText(PromptText);
        InteractionPrompt->SetVisibility(ESlateVisibility::Visible);
        
        // You could add fade-in animation here
        
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Showing interaction prompt: %s"), *PromptText.ToString());
    }
}

void UMedComMainHUDWidget::HideInteractionPrompt_Implementation()
{
    if (InteractionPrompt)
    {
        // You could add fade-out animation here
        InteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);
        
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Hiding interaction prompt"));
    }
}

void UMedComMainHUDWidget::ShowInventory_Implementation()
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
        if (IMedComInventoryUIBridgeWidget* Bridge = IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(this))
        {
            UObject* BridgeObject = Cast<UObject>(Bridge);
            if (BridgeObject)
            {
                UMedComInventoryUIBridge* ConcreteBridge = Cast<UMedComInventoryUIBridge>(BridgeObject);
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
    if (IMedComInventoryUIBridgeWidget* Bridge = IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            IMedComInventoryUIBridgeWidget::Execute_ShowInventoryUI(BridgeObject);
            OnInventoryVisibilityChanged(true);
        }
    }
}

void UMedComMainHUDWidget::HideInventory_Implementation()
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
    if (IMedComInventoryUIBridgeWidget* Bridge = IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            IMedComInventoryUIBridgeWidget::Execute_HideInventoryUI(BridgeObject);
            OnInventoryVisibilityChanged(false);
        }
    }
}

void UMedComMainHUDWidget::ToggleInventory_Implementation()
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

bool UMedComMainHUDWidget::IsInventoryVisible_Implementation() const
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
    if (IMedComInventoryUIBridgeWidget* Bridge = IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            return IMedComInventoryUIBridgeWidget::Execute_IsInventoryUIVisible(BridgeObject);
        }
    }
    
    return false;
}

void UMedComMainHUDWidget::ShowCharacterScreenWithTab(const FGameplayTag& TabTag)
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
    if (CharacterScreen->GetClass()->ImplementsInterface(UMedComScreenInterface::StaticClass()))
    {
        IMedComScreenInterface::Execute_OnScreenActivated(CharacterScreen);
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
        if (IMedComInventoryUIBridgeWidget* Bridge = IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(this))
        {
            UObject* BridgeObject = Cast<UObject>(Bridge);
            if (BridgeObject)
            {
                IMedComInventoryUIBridgeWidget::Execute_ShowCharacterScreenWithTab(BridgeObject, TabTag);
            }
        }
    }
}

void UMedComMainHUDWidget::HideCharacterScreen()
{
    if (!CharacterScreen)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Hiding CharacterScreen"));
    
    // Проверяем, какая вкладка была открыта для правильного уведомления
    bool bWasInventoryOpen = false;
    if (UMedComUpperTabBar* TabBar = CharacterScreen->GetTabBar())
    {
        int32 CurrentIndex = TabBar->GetSelectedTabIndex_Implementation();
        if (CurrentIndex >= 0)
        {
            FMedComTabConfig Config = TabBar->GetTabConfig(CurrentIndex);
            bWasInventoryOpen = Config.TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory")));
        }
    }
    
    // Деактивируем экран
    if (CharacterScreen->GetClass()->ImplementsInterface(UMedComScreenInterface::StaticClass()))
    {
        IMedComScreenInterface::Execute_OnScreenDeactivated(CharacterScreen);
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
    if (IMedComInventoryUIBridgeWidget* Bridge = IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(this))
    {
        UObject* BridgeObject = Cast<UObject>(Bridge);
        if (BridgeObject)
        {
            IMedComInventoryUIBridgeWidget::Execute_HideCharacterScreen(BridgeObject);
        }
    }
}

void UMedComMainHUDWidget::ToggleCharacterScreen()
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

bool UMedComMainHUDWidget::IsCharacterScreenVisible() const
{
    return CharacterScreen && CharacterScreen->GetVisibility() == ESlateVisibility::Visible;
}

void UMedComMainHUDWidget::RequestInventoryInitialization_Implementation()
{
    // Логируем запрос инициализации
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Inventory initialization requested"));
    
    // ВАЖНО: Мы больше не используем события с GameplayTag,
    // так как инициализация теперь происходит напрямую при показе инвентаря
    // Этот метод остается для совместимости с интерфейсом,
    // но реальная инициализация происходит в ShowInventory_Implementation()
    
    UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] Inventory will be initialized on first show"));
}

void UMedComMainHUDWidget::EnsureInventoryBridgeInitialized()
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

void UMedComMainHUDWidget::OnInventoryVisibilityChanged(bool bIsVisible)
{
    // Call the Blueprint event if implemented
    K2_OnInventoryVisibilityChanged(bIsVisible);
    
    // Отправляем события через систему событий
    if (UEventDelegateManager* EventManager = GetDelegateManager())
    {
        FGameplayTag EventTag = bIsVisible ? 
            FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.Opened")) :
            FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.Closed"));
        EventManager->NotifyUIEvent(this, EventTag, TEXT(""));
    }
}

void UMedComMainHUDWidget::OnCharacterScreenVisibilityChanged(bool bIsVisible)
{
    // Call the Blueprint event if implemented
    K2_OnCharacterScreenVisibilityChanged(bIsVisible);
    
    // Отправляем события через систему событий
    if (UEventDelegateManager* EventManager = GetDelegateManager())
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
void UMedComMainHUDWidget::SetCurrentHealth_UI(float CurrentHealth)
{
    if (HealthStaminaWidget)
    {
        float MaxHealth = HealthStaminaWidget->GetMaxHealth();
        HealthStaminaWidget->UpdateHealth_Implementation(CurrentHealth, MaxHealth);
    }
}

void UMedComMainHUDWidget::SetMaxHealth_UI(float MaxHealth)
{
    if (HealthStaminaWidget)
    {
        float CurrentHealth = HealthStaminaWidget->GetCurrentHealth();
        HealthStaminaWidget->UpdateHealth_Implementation(CurrentHealth, MaxHealth);
    }
}

void UMedComMainHUDWidget::SetHealthPercentage_UI(float HealthPercentage)
{
    if (HealthStaminaWidget)
    {
        float MaxHealth = HealthStaminaWidget->GetMaxHealth();
        float CurrentHealth = MaxHealth * HealthPercentage;
        HealthStaminaWidget->UpdateHealth_Implementation(CurrentHealth, MaxHealth);
    }
}

float UMedComMainHUDWidget::GetHealthPercentage() const
{
    if (HealthStaminaWidget)
    {
        return HealthStaminaWidget->GetHealthPercentage_Implementation();
    }
    return 1.0f;
}

void UMedComMainHUDWidget::SetCurrentStamina_UI(float CurrentStamina)
{
    if (HealthStaminaWidget)
    {
        float MaxStamina = HealthStaminaWidget->GetMaxStamina();
        HealthStaminaWidget->UpdateStamina_Implementation(CurrentStamina, MaxStamina);
    }
}

void UMedComMainHUDWidget::SetMaxStamina_UI(float MaxStamina)
{
    if (HealthStaminaWidget)
    {
        float CurrentStamina = HealthStaminaWidget->GetCurrentStamina();
        HealthStaminaWidget->UpdateStamina_Implementation(CurrentStamina, MaxStamina);
    }
}

void UMedComMainHUDWidget::SetStaminaPercentage_UI(float StaminaPercentage)
{
    if (HealthStaminaWidget)
    {
        float MaxStamina = HealthStaminaWidget->GetMaxStamina();
        float CurrentStamina = MaxStamina * StaminaPercentage;
        HealthStaminaWidget->UpdateStamina_Implementation(CurrentStamina, MaxStamina);
    }
}

float UMedComMainHUDWidget::GetStaminaPercentage() const
{
    if (HealthStaminaWidget)
    {
        return HealthStaminaWidget->GetStaminaPercentage_Implementation();
    }
    return 1.0f;
}

void UMedComMainHUDWidget::SetCrosshairVisibility(bool bVisible)
{
    if (DynamicCrosshair)
    {
        DynamicCrosshair->SetCrosshairVisibility_Implementation(bVisible);
    }
}

void UMedComMainHUDWidget::InitializeChildWidgets()
{
    // Initialize health/stamina widget
    if (HealthStaminaWidget)
    {
        FGameplayTag CurrentTag = IMedComUIWidgetInterface::Execute_GetWidgetTag(HealthStaminaWidget);
        if (!CurrentTag.IsValid())
        {
            IMedComUIWidgetInterface::Execute_SetWidgetTag(
                HealthStaminaWidget, 
                FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.HealthBar"))
            );
        }
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] HealthStaminaWidget initialized"));
    }
    
    // Initialize crosshair
    if (DynamicCrosshair)
    {
        FGameplayTag CurrentTag = IMedComUIWidgetInterface::Execute_GetWidgetTag(DynamicCrosshair);
        if (!CurrentTag.IsValid())
        {
            IMedComUIWidgetInterface::Execute_SetWidgetTag(
                DynamicCrosshair,
                FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.Crosshair"))
            );
        }
        
        // Set initial visibility
        IMedComCrosshairWidgetInterface::Execute_SetCrosshairVisibility(
            DynamicCrosshair, 
            bCombatElementsVisible
        );
        UE_LOG(LogTemp, Log, TEXT("[MainHUDWidget] DynamicCrosshair initialized"));
    }
    
    // Initialize weapon info widget
    if (WeaponInfoWidget)
    {
        FGameplayTag CurrentTag = IMedComUIWidgetInterface::Execute_GetWidgetTag(WeaponInfoWidget);
        if (!CurrentTag.IsValid())
        {
            IMedComUIWidgetInterface::Execute_SetWidgetTag(
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
        FGameplayTag CurrentTag = IMedComUIWidgetInterface::Execute_GetWidgetTag(CharacterScreen);
        if (!CurrentTag.IsValid())
        {
            IMedComUIWidgetInterface::Execute_SetWidgetTag(
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
        FGameplayTag CurrentTag = IMedComUIWidgetInterface::Execute_GetWidgetTag(InventoryWidget);
        if (!CurrentTag.IsValid())
        {
            IMedComUIWidgetInterface::Execute_SetWidgetTag(
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

void UMedComMainHUDWidget::SetupEventSubscriptions()
{
    if (UEventDelegateManager* EventManager = UMedComBaseWidget::GetDelegateManager())
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

void UMedComMainHUDWidget::ClearEventSubscriptions()
{
    if (UEventDelegateManager* EventManager = UMedComBaseWidget::GetDelegateManager())
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

bool UMedComMainHUDWidget::ValidateWidgetBindings() const
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
        UE_LOG(LogTemp, Warning, TEXT("[MainHUDWidget] For best results, bind a UMedComCharacterScreen in Blueprint"));
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

void UMedComMainHUDWidget::OnActiveWeaponChanged(AActor* NewWeapon)
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

void UMedComMainHUDWidget::OnCrosshairUpdateRequested(float Spread, float Recoil)
{
    if (DynamicCrosshair)
    {
        DynamicCrosshair->UpdateCrosshair_Implementation(Spread, Recoil, false);
    }
}

void UMedComMainHUDWidget::OnCrosshairColorChanged(FLinearColor NewColor)
{
    if (DynamicCrosshair)
    {
        DynamicCrosshair->SetCrosshairColor_Implementation(NewColor);
    }
}

void UMedComMainHUDWidget::OnNotificationReceived(const FString& Message, float Duration)
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

void UMedComMainHUDWidget::OnInventoryCloseRequested()
{
    HideInventory_Implementation();
}