// Copyright Suspense Team Team. All Rights Reserved.

#include "Components/SuspenseUIManager.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/UI/ISuspenseUIWidget.h"
#include "Interfaces/UI/ISuspenseHUDWidget.h"
#include "Interfaces/Core/ISuspenseController.h"
#include "Interfaces/Screens/ISuspenseScreen.h"
#include "Delegates/SuspenseEventManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SuspenseInventoryUIBridge.h"
#include "Components/SuspenseEquipmentUIBridge.h"
#include "DragDrop/SuspenseDragDropHandler.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Widgets/Base/SuspenseBaseWidget.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Widgets/Tabs/SuspenseUpperTabBar.h"
#include "Widgets/Layout/SuspenseBaseLayoutWidget.h"
#include "Widgets/Inventory/SuspenseInventoryWidget.h"
#include "Widgets/Equipment/SuspenseEquipmentContainerWidget.h"

USuspenseUIManager::USuspenseUIManager()
{
    static ConstructorHelpers::FClassFinder<UUserWidget> CharacterScreenFinder(
        TEXT("/Game/MEDCOM/UI/TabScreens/W_CharacterScreen")
    );
    if (CharacterScreenFinder.Succeeded())
    {
        CharacterScreenClass = CharacterScreenFinder.Class;
        UE_LOG(LogTemp, Log, TEXT("[UIManager] CharacterScreenClass set from constructor"));
    }
}

void USuspenseUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Clean up any remnants from previous session
    CleanupPreviousSession();

    // Build configuration cache for fast lookup
    BuildConfigurationCache();

    // Cache event manager
    if (UGameInstance* GI = GetGameInstance())
    {
        CachedEventManager = GI->GetSubsystem<USuspenseEventManager>();
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Initialized with %d widget configurations"),
        WidgetConfigurations.Num());

    // DO NOT initialize bridges here - they will be created on demand
    UE_LOG(LogTemp, Log, TEXT("[UIManager] Bridges will be initialized on-demand"));

    // Subscribe to layout events
    SubscribeToLayoutEvents();
}

void USuspenseUIManager::Deinitialize()
{
    UE_LOG(LogTemp, Warning, TEXT("[UIManager] Deinitialize called"));

    // Unsubscribe from layout events
    UnsubscribeFromLayoutEvents();

    // Clean up inventory bridge
    if (InventoryUIBridge)
    {
        InventoryUIBridge->Shutdown();
        InventoryUIBridge->ConditionalBeginDestroy();
        InventoryUIBridge = nullptr;
    }

    // Clean up equipment bridge
    if (EquipmentUIBridge)
    {
        EquipmentUIBridge->Shutdown();
        EquipmentUIBridge->ConditionalBeginDestroy();
        EquipmentUIBridge = nullptr;
    }

    // Clear all widgets on shutdown
    DestroyAllWidgets();

    // Clear all caches and references
    ConfigurationCache.Empty();
    CachedEventManager = nullptr;

    // Clear maps
    ActiveWidgets.Empty();
    WidgetParentMap.Empty();

    Super::Deinitialize();
}

void USuspenseUIManager::CleanupPreviousSession()
{
    UE_LOG(LogTemp, Warning, TEXT("[UIManager] Cleaning up previous session"));

    // Check and clean lingering widgets
    if (ActiveWidgets.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Found %d lingering widgets from previous session"),
            ActiveWidgets.Num());
        DestroyAllWidgets();
    }

    // Clean inventory bridge
    if (InventoryUIBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Found lingering InventoryUIBridge from previous session"));
        InventoryUIBridge->Shutdown();
        InventoryUIBridge->ConditionalBeginDestroy();
        InventoryUIBridge = nullptr;
    }

    // Clean equipment bridge
    if (EquipmentUIBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Found lingering EquipmentUIBridge from previous session"));
        EquipmentUIBridge->Shutdown();
        EquipmentUIBridge->ConditionalBeginDestroy();
        EquipmentUIBridge = nullptr;
    }

    // Reset all delegate handles
    LayoutWidgetCreatedHandle.Reset();
    LayoutWidgetDestroyedHandle.Reset();

    // Clear parent map
    WidgetParentMap.Empty();
}

void USuspenseUIManager::DestroyAllWidgets()
{
    // Create copy of keys for safe deletion
    TArray<FGameplayTag> AllTags;
    ActiveWidgets.GetKeys(AllTags);

    UE_LOG(LogTemp, Warning, TEXT("[UIManager] Destroying %d widgets"), AllTags.Num());

    for (const FGameplayTag& Tag : AllTags)
    {
        if (UUserWidget* Widget = ActiveWidgets.FindRef(Tag))
        {
            // Full widget cleanup
            CleanupWidget(Widget);

            // Force destruction
            Widget->ConditionalBeginDestroy();
        }
    }

    // Clear maps
    ActiveWidgets.Empty();
    WidgetParentMap.Empty();
}

USuspenseUIManager* USuspenseUIManager::Get(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        return nullptr;
    }

    const UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }

    return GameInstance->GetSubsystem<USuspenseUIManager>();
}

UUserWidget* USuspenseUIManager::CreateWidget(
    TSubclassOf<UUserWidget> WidgetClass,
    FGameplayTag WidgetTag,
    UObject* OwningObject,
    bool bForceAddToViewport)
{
    // IMPORTANT: This method should ONLY be used for root widgets like HUD and CharacterScreen
    // All child widgets must be created through Layout system

    // Validate parameters
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] CreateWidget failed - WidgetClass is null"));
        return nullptr;
    }

    if (!WidgetTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] CreateWidget failed - WidgetTag is not valid"));
        return nullptr;
    }

    if (!OwningObject)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] CreateWidget failed - OwningObject is null"));
        return nullptr;
    }

    // Verify this is a root widget tag
    if (!IsRootWidgetTag(WidgetTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] CreateWidget called for non-root widget %s. Use Layout system instead!"),
            *WidgetTag.ToString());
    }

    // Check if widget already exists
    if (ActiveWidgets.Contains(WidgetTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Widget with tag %s already exists"),
            *WidgetTag.ToString());
        return ActiveWidgets[WidgetTag];
    }

    // Get player controller from owning object
    APlayerController* PC = GetPlayerControllerFromObject(OwningObject);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Cannot get PlayerController from owning object"));
        return nullptr;
    }

    // Create widget
    UUserWidget* NewWidget = ::CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!NewWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to create widget of class %s"),
            *WidgetClass->GetName());
        return nullptr;
    }

    // Set widget tag through interface
    if (NewWidget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_SetWidgetTag(NewWidget, WidgetTag);
    }
    else if (USuspenseBaseWidget* BaseWidget = Cast<USuspenseBaseWidget>(NewWidget))
    {
        BaseWidget->WidgetTag = WidgetTag;
    }

    // Register widget
    ActiveWidgets.Add(WidgetTag, NewWidget);

    // Initialize through interface
    if (NewWidget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_InitializeWidget(NewWidget);

        // If HUD widget, setup with owner
        if (NewWidget->GetClass()->ImplementsInterface(USuspenseHUDWidget::StaticClass()))
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                ISuspenseHUDWidget::Execute_SetupForPlayer(NewWidget, Pawn);
            }
        }
    }

    // Check if we should add to viewport
    bool bShouldAddToViewport = bForceAddToViewport || ShouldAutoAddToViewport(WidgetTag);

    if (bShouldAddToViewport)
    {
        int32 ZOrder = GetZOrderForWidget(WidgetTag);
        NewWidget->AddToViewport(ZOrder);

        UE_LOG(LogTemp, Log, TEXT("[UIManager] Root widget %s added to viewport with Z-order %d"),
            *WidgetTag.ToString(), ZOrder);
    }

    // Notify about widget creation
    NotifyWidgetCreated(NewWidget, WidgetTag);

    return NewWidget;
}

bool USuspenseUIManager::RegisterLayoutWidget(UUserWidget* Widget, FGameplayTag WidgetTag, UUserWidget* ParentLayout)
{
    if (!Widget || !WidgetTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] RegisterLayoutWidget - Invalid parameters"));
        return false;
    }

    // Check if already registered
    if (ActiveWidgets.Contains(WidgetTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Widget with tag %s already registered"),
            *WidgetTag.ToString());
        return false;
    }

    // Register widget
    ActiveWidgets.Add(WidgetTag, Widget);

    // Store parent relationship
    if (ParentLayout)
    {
        WidgetParentMap.Add(Widget, ParentLayout);
    }

    // Set widget tag through interface
    if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_SetWidgetTag(Widget, WidgetTag);
    }
    else if (USuspenseBaseWidget* BaseWidget = Cast<USuspenseBaseWidget>(Widget))
    {
        BaseWidget->WidgetTag = WidgetTag;
    }

    // Notify about registration
    NotifyWidgetCreated(Widget, WidgetTag);

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Registered layout widget %s with tag %s"),
        *Widget->GetName(), *WidgetTag.ToString());

    return true;
}

bool USuspenseUIManager::UnregisterLayoutWidget(FGameplayTag WidgetTag)
{
    if (!WidgetTag.IsValid())
    {
        return false;
    }

    UUserWidget* Widget = nullptr;
    if (ActiveWidgets.RemoveAndCopyValue(WidgetTag, Widget))
    {
        // Remove parent relationship
        WidgetParentMap.Remove(Widget);

        // Notify about unregistration
        NotifyWidgetDestroyed(WidgetTag);

        UE_LOG(LogTemp, Log, TEXT("[UIManager] Unregistered widget with tag %s"),
            *WidgetTag.ToString());

        return true;
    }

    return false;
}

UUserWidget* USuspenseUIManager::FindWidgetInLayouts(FGameplayTag WidgetTag) const
{
    if (!WidgetTag.IsValid())
    {
        return nullptr;
    }

    // First check if it's a root widget in direct registry
    if (UUserWidget* DirectWidget = GetWidget(WidgetTag))
    {
        // Only return if it's actually a root widget
        if (IsRootWidgetTag(WidgetTag))
        {
            return DirectWidget;
        }
    }

    // For non-root widgets, ALWAYS search in layouts
    for (const auto& Pair : ActiveWidgets)
    {
        // Check if this is a layout widget
        if (USuspenseBaseLayoutWidget* LayoutWidget = Cast<USuspenseBaseLayoutWidget>(Pair.Value))
        {
            if (UUserWidget* FoundWidget = LayoutWidget->GetWidgetByTag(WidgetTag))
            {
                UE_LOG(LogTemp, Verbose, TEXT("[UIManager] Found widget %s in layout %s"),
                    *WidgetTag.ToString(), *LayoutWidget->GetName());
                return FoundWidget;
            }
        }

        // Also check TabBar widgets which might contain layouts
        if (USuspenseUpperTabBar* TabBar = Cast<USuspenseUpperTabBar>(Pair.Value))
        {
            int32 CurrentIndex = TabBar->GetCurrentTabIndex();
            if (CurrentIndex >= 0)
            {
                if (USuspenseBaseLayoutWidget* ActiveLayout = TabBar->GetTabLayoutWidget(CurrentIndex))
                {
                    if (UUserWidget* FoundWidget = ActiveLayout->GetWidgetByTag(WidgetTag))
                    {
                        UE_LOG(LogTemp, Verbose, TEXT("[UIManager] Found widget %s in TabBar's active layout"),
                            *WidgetTag.ToString());
                        return FoundWidget;
                    }
                }
            }
        }
    }

    return nullptr;
}

UUserWidget* USuspenseUIManager::GetWidgetFromLayout(USuspenseBaseLayoutWidget* LayoutWidget, FGameplayTag WidgetTag) const
{
    if (!LayoutWidget || !WidgetTag.IsValid())
    {
        return nullptr;
    }

    return LayoutWidget->GetWidgetByTag(WidgetTag);
}

TArray<FGameplayTag> USuspenseUIManager::GetAllWidgetTags() const
{
    TArray<FGameplayTag> AllTags;
    ActiveWidgets.GetKeys(AllTags);
    return AllTags;
}

bool USuspenseUIManager::ShowWidget(FGameplayTag WidgetTag, bool bAddToViewport)
{
    UUserWidget* Widget = GetWidget(WidgetTag);
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] ShowWidget - widget %s not found"),
            *WidgetTag.ToString());
        return false;
    }

    // Log current state
    bool bWasInViewport = Widget->IsInViewport();
    ESlateVisibility OldVisibility = Widget->GetVisibility();

    UE_LOG(LogTemp, Log, TEXT("[UIManager] ShowWidget - Widget: %s, WasInViewport: %s, OldVisibility: %s"),
        *Widget->GetName(),
        bWasInViewport ? TEXT("Yes") : TEXT("No"),
        *UEnum::GetValueAsString(OldVisibility));

    // Add to viewport if needed and not already added
    if (bAddToViewport && !bWasInViewport)
    {
        int32 ZOrder = GetZOrderForWidget(WidgetTag);
        Widget->AddToViewport(ZOrder);
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Added widget to viewport with Z-order %d"), ZOrder);
    }
    else if (bWasInViewport)
    {
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Widget already in viewport"));
    }

    // Set visibility
    Widget->SetVisibility(ESlateVisibility::Visible);

    // Notify through interface if supported
    if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_ShowWidget(Widget, true);
    }

    // Final check
    bool bIsNowInViewport = Widget->IsInViewport();
    ESlateVisibility NewVisibility = Widget->GetVisibility();

    UE_LOG(LogTemp, Log, TEXT("[UIManager] ShowWidget result - InViewport: %s, Visibility: %s"),
        bIsNowInViewport ? TEXT("Yes") : TEXT("No"),
        *UEnum::GetValueAsString(NewVisibility));

    return bIsNowInViewport && NewVisibility == ESlateVisibility::Visible;
}

bool USuspenseUIManager::HideWidget(FGameplayTag WidgetTag, bool bRemoveFromParent)
{
    UUserWidget* Widget = GetWidget(WidgetTag);
    if (!Widget)
    {
        return false;
    }

    // Hide widget
    Widget->SetVisibility(ESlateVisibility::Hidden);

    // Remove from viewport if needed
    if (bRemoveFromParent && Widget->IsInViewport())
    {
        Widget->RemoveFromParent();
    }

    // Notify through interface
    if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_HideWidget(Widget, true);
    }

    return true;
}

bool USuspenseUIManager::DestroyWidget(FGameplayTag WidgetTag)
{
    UUserWidget** WidgetPtr = ActiveWidgets.Find(WidgetTag);
    if (!WidgetPtr || !*WidgetPtr)
    {
        return false;
    }

    UUserWidget* Widget = *WidgetPtr;

    // Cleanup widget
    CleanupWidget(Widget);

    // Remove from registry
    ActiveWidgets.Remove(WidgetTag);
    WidgetParentMap.Remove(Widget);

    // Notify about destruction
    NotifyWidgetDestroyed(WidgetTag);

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Destroyed widget with tag %s"),
        *WidgetTag.ToString());

    return true;
}

UUserWidget* USuspenseUIManager::GetWidget(FGameplayTag WidgetTag) const
{
    if (UUserWidget* const* WidgetPtr = ActiveWidgets.Find(WidgetTag))
    {
        return *WidgetPtr;
    }
    return nullptr;
}

bool USuspenseUIManager::WidgetExists(FGameplayTag WidgetTag) const
{
    return ActiveWidgets.Contains(WidgetTag);
}

UUserWidget* USuspenseUIManager::ShowCharacterScreen(UObject* OwningObject, FGameplayTag TabTag)
{
    UE_LOG(LogTemp, Log, TEXT("[UIManager] ShowCharacterScreen called with tab: %s"), *TabTag.ToString());

    // Проверяем, что класс Character Screen установлен
    if (!CharacterScreenClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] CharacterScreenClass not set! Please set it in UIManager configuration"));
        return nullptr;
    }

    // Получаем существующий экран или создаем новый
    UUserWidget* CharacterScreen = GetWidget(CharacterScreenTag);

    if (!CharacterScreen)
    {
        // Создаем только если не существует - это ЕДИНСТВЕННЫЙ root widget, который мы создаем напрямую
        CharacterScreen = CreateWidget(CharacterScreenClass, CharacterScreenTag, OwningObject, true);
        if (!CharacterScreen)
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to create CharacterScreen"));
            return nullptr;
        }
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Created new CharacterScreen"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Using existing CharacterScreen"));
    }

    // Показываем экран сначала
    if (!ShowWidget(CharacterScreenTag, true))
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to show CharacterScreen"));
        return nullptr;
    }

    // КРИТИЧНО: Ждем полного построения layout перед инициализацией bridge
    // Это гарантирует, что все дочерние виджеты созданы и готовы к анализу
    FTimerHandle InitializationTimer;
    GetWorld()->GetTimerManager().SetTimerForNextTick([this, CharacterScreen, TabTag, OwningObject]()
    {
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Starting intelligent bridge initialization"));

        // Получаем PlayerController для создания bridge
        APlayerController* PC = GetPlayerControllerFromObject(OwningObject);
        if (!PC)
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] No PlayerController available for bridge initialization"));
            return;
        }

        // Находим TabBar в Character Screen
        USuspenseUpperTabBar* TabBar = FindTabBarInCharacterScreen(CharacterScreen);
        if (!TabBar)
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to find TabBar in CharacterScreen"));
            return;
        }

        // Выбираем запрошенную вкладку
        bool bTabSelected = TabBar->SelectTabByTag(TabTag);
        if (!bTabSelected)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIManager] Failed to select tab %s"), *TabTag.ToString());
            return;
        }

        // Получаем активный layout для анализа содержимого
        int32 CurrentTabIndex = TabBar->GetCurrentTabIndex();
        if (CurrentTabIndex < 0)
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] No active tab selected"));
            return;
        }

        USuspenseBaseLayoutWidget* ActiveLayout = TabBar->GetTabLayoutWidget(CurrentTabIndex);

        if (ActiveLayout)
        {
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Found layout widget, analyzing contents for intelligent bridge initialization"));

            // НОВАЯ ЛОГИКА: Анализируем содержимое layout и создаем соответствующие bridge
            // Это ключевое нововведение - система "понимает" что находится в layout
            AnalyzeLayoutAndCreateBridges(PC, ActiveLayout);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIManager] No layout widget found, checking direct content"));

            // Fallback: проверяем прямое содержимое вкладки
            UUserWidget* ContentWidget = TabBar->GetTabContent_Implementation(CurrentTabIndex);
            if (ContentWidget)
            {
                // Возможно, content widget сам является layout
                ActiveLayout = Cast<USuspenseBaseLayoutWidget>(ContentWidget);
                if (ActiveLayout)
                {
                    UE_LOG(LogTemp, Log, TEXT("[UIManager] Content widget is layout, analyzing"));
                    AnalyzeLayoutAndCreateBridges(PC, ActiveLayout);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[UIManager] Using legacy bridge initialization"));
                    InitializeBridgesByTabTag(PC, TabTag);
                }
            }
        }

        // Принудительно обновляем контент после инициализации bridge
        TabBar->RefreshActiveTabContent();

        UE_LOG(LogTemp, Log, TEXT("[UIManager] Character Screen initialization completed with intelligent bridge system"));
    });

    return CharacterScreen;
}

void USuspenseUIManager::InitializeBridgesByTabTag(APlayerController* PlayerController, FGameplayTag TabTag)
{
    UE_LOG(LogTemp, Log, TEXT("[UIManager] Using legacy tab-based bridge initialization"));

    // Создаем Inventory Bridge если открыта inventory вкладка
    if (TabTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"))) && !InventoryUIBridge)
    {
        InventoryUIBridge = CreateInventoryUIBridge(PlayerController);
        if (InventoryUIBridge)
        {
            ConnectInventoryBridgeToGameComponent(InventoryUIBridge, PlayerController);
        }
    }

    // Создаем Equipment Bridge если открыта equipment вкладка
    if (TabTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Equipment"))) && !EquipmentUIBridge)
    {
        EquipmentUIBridge = CreateEquipmentUIBridge(PlayerController);
        if (EquipmentUIBridge)
        {
            ConnectEquipmentBridgeToGameComponent(EquipmentUIBridge, PlayerController);
        }
    }
}

void USuspenseUIManager::AnalyzeLayoutAndCreateBridges(APlayerController* PlayerController, USuspenseBaseLayoutWidget* LayoutWidget)
{
    if (!PlayerController || !LayoutWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] AnalyzeLayoutAndCreateBridges - invalid parameters"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] === Analyzing layout for ALL contained widgets ==="));

    // Собираем все виджеты и теги из layout
    const TArray<UUserWidget*> LayoutWidgets = LayoutWidget->GetLayoutWidgets_Implementation();
    const TArray<FGameplayTag>  WidgetTags   = LayoutWidget->GetAllWidgetTags();

    bool bNeedsInventoryBridge = false;
    bool bNeedsEquipmentBridge = false;

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Found %d widgets in layout"), LayoutWidgets.Num());

    // Анализ 1: по тегам
    const FGameplayTag TagInventory = FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Inventory"));
    const FGameplayTag TagEquipment = FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Equipment"));

    for (const FGameplayTag& WidgetTag : WidgetTags)
    {
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Checking widget tag: %s"), *WidgetTag.ToString());
        if (WidgetTag.MatchesTag(TagInventory))
        {
            bNeedsInventoryBridge = true;
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Found inventory widget by tag"));
        }
        if (WidgetTag.MatchesTag(TagEquipment))
        {
            bNeedsEquipmentBridge = true;
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Found equipment widget by tag"));
        }
    }

    // Анализ 2: по типам классов/именам
    for (UUserWidget* Widget : LayoutWidgets)
    {
        if (!Widget) continue;
        const FString WidgetClassName = Widget->GetClass()->GetName();
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Analyzing widget class: %s"), *WidgetClassName);

        // Инвентарь
        if (Widget->IsA<USuspenseInventoryWidget>() || WidgetClassName.Contains(TEXT("Inventory")))
        {
            bNeedsInventoryBridge = true;
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Found inventory widget by class type"));
        }

        // Экип
        if (Widget->IsA<USuspenseEquipmentContainerWidget>() || WidgetClassName.Contains(TEXT("Equipment")))
        {
            bNeedsEquipmentBridge = true;
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Found equipment widget by class type"));
        }
    }

    // Fallback: если явных подсказок нет — создаём оба моста (лениво, без дубликатов)
    if (!bNeedsInventoryBridge && !bNeedsEquipmentBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] No explicit UI hints; creating both bridges as fallback"));
        bNeedsInventoryBridge = true;
        bNeedsEquipmentBridge = true;
    }

    // Создание/подключение Inventory Bridge
    if (bNeedsInventoryBridge && !InventoryUIBridge)
    {
        InventoryUIBridge = CreateInventoryUIBridge(PlayerController);
        if (InventoryUIBridge)
        {
            ConnectInventoryBridgeToGameComponent(InventoryUIBridge, PlayerController);
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Inventory Bridge created and connected"));

            // Отложенный refresh (UMG готовится кадр-два)
            GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
            {
                if (InventoryUIBridge)
                {
                    ISuspenseInventoryUIBridgeInterface::Execute_RefreshInventoryUI(InventoryUIBridge);
                }
            });
        }
    }

    // Создание/подключение Equipment Bridge
    if (bNeedsEquipmentBridge && !EquipmentUIBridge)
    {
        EquipmentUIBridge = CreateEquipmentUIBridge(PlayerController);
        if (EquipmentUIBridge)
        {
            ConnectEquipmentBridgeToGameComponent(EquipmentUIBridge, PlayerController);
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Equipment Bridge created and connected"));

            GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
            {
                if (EquipmentUIBridge)
                {
                    ISuspenseEquipmentUIBridgeInterface::Execute_RefreshEquipmentUI(EquipmentUIBridge);
                }
            });
        }
    }

    // Универсальный рефреш layout/дерева (твоя существующая логика)
    SetupUniversalLayoutRefresh(LayoutWidget);

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Layout analysis complete - Created bridges: Inventory=%s, Equipment=%s"),
        InventoryUIBridge ? TEXT("Yes") : TEXT("No"),
        EquipmentUIBridge ? TEXT("Yes") : TEXT("No"));
}


void USuspenseUIManager::SetupUniversalLayoutRefresh(USuspenseBaseLayoutWidget* LayoutWidget)
{
    if (!LayoutWidget)
    {
        return;
    }

    // Подписываемся на события, которые требуют обновления всех виджетов
    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        // Создаем обработчик для универсального обновления
        auto UniversalRefreshLambda = [this, LayoutWidget](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
        {
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Universal refresh triggered by event: %s"),
                *EventTag.ToString());

            // Обновляем ВСЕ виджеты в layout
            RefreshAllWidgetsInLayout(LayoutWidget);
        };

        //  Используем SubscribeToUIEvent для событий экипировки
        EventManager->SubscribeToUIEvent(
            [UniversalRefreshLambda](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
            {
                if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event"))))
                {
                    UniversalRefreshLambda(Source, EventTag, EventData);
                }
            });

        //Используем SubscribeToUIEvent для событий инвентаря
        EventManager->SubscribeToUIEvent(
            [UniversalRefreshLambda](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
            {
                if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event"))))
                {
                    UniversalRefreshLambda(Source, EventTag, EventData);
                }
            });

        // Альтернативный подход - подписка на универсальное событие обновления layout
        EventManager->SubscribeToGenericEventLambda(
            FGameplayTag::RequestGameplayTag(TEXT("UI.Layout.RefreshAll")),
            [this, LayoutWidget](const UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
            {
                UE_LOG(LogTemp, Log, TEXT("[UIManager] Layout refresh requested: %s"), *EventData);
                RefreshAllWidgetsInLayout(LayoutWidget);
            });
    }
}

void USuspenseUIManager::RefreshAllWidgetsInLayout(USuspenseBaseLayoutWidget* LayoutWidget)
{
    if (!LayoutWidget)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] === Refreshing ALL widgets in layout ==="));

    // Получаем все виджеты из layout
    TArray<FGameplayTag> WidgetTags = LayoutWidget->GetAllWidgetTags();

    for (const FGameplayTag& Tag : WidgetTags)
    {
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Refreshing widget with tag: %s"), *Tag.ToString());

        // Обновляем через соответствующий bridge
        if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Inventory"))) && InventoryUIBridge)
        {
            ISuspenseInventoryUIBridgeInterface::Execute_RefreshInventoryUI(InventoryUIBridge);
        }
        else if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Equipment"))) && EquipmentUIBridge)
        {
            ISuspenseEquipmentUIBridgeInterface::Execute_RefreshEquipmentUI(EquipmentUIBridge);
        }

        // Обновляем сам виджет если он поддерживает интерфейс
        if (UUserWidget* Widget = LayoutWidget->GetWidgetByTag(Tag))
        {
            if (Widget->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
            {
                ISuspenseScreen::Execute_RefreshScreenContent(Widget);
            }
        }
    }

    // Обновляем сам layout
    LayoutWidget->RefreshLayout_Implementation();

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Layout refresh completed"));
}

USuspenseUpperTabBar* USuspenseUIManager::FindTabBarInCharacterScreen(UUserWidget* CharacterScreen) const
{
    if (!CharacterScreen)
    {
        return nullptr;
    }

    // Способ 1: Проверяем прямое приведение типа (если Character Screen сам является TabBar)
    if (USuspenseUpperTabBar* DirectTabBar = Cast<USuspenseUpperTabBar>(CharacterScreen))
    {
        UE_LOG(LogTemp, Log, TEXT("[UIManager] Character Screen is TabBar directly"));
        return DirectTabBar;
    }

    // Способ 2: Ищем в дереве виджетов
    TArray<UWidget*> AllWidgets;
    CharacterScreen->WidgetTree->GetAllWidgets(AllWidgets);

    for (UWidget* Widget : AllWidgets)
    {
        if (USuspenseUpperTabBar* TabBar = Cast<USuspenseUpperTabBar>(Widget))
        {
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Found TabBar in widget tree"));
            return TabBar;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[UIManager] TabBar not found in Character Screen"));
    return nullptr;
}

void USuspenseUIManager::ConnectEquipmentBridgeToGameComponent(
    USuspenseEquipmentUIBridge* Bridge,
    APlayerController* PlayerController)
{
    if (!Bridge || !PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] ConnectEquipmentBridgeToGameComponent - Invalid parameters"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Connecting Equipment Bridge to game component"));

    // Step 1: Try to find equipment component on Pawn first
    if (APawn* Pawn = PlayerController->GetPawn())
    {
        TArray<UActorComponent*> Components = Pawn->GetComponents().Array();
        for (UActorComponent* Component : Components)
        {
            // Check for ISuspenseEquipmentDataProvider interface (new architecture)
            if (Component && Component->GetClass()->ImplementsInterface(USuspenseEquipmentDataProvider::StaticClass()))
            {
                TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;
                DataProvider.SetObject(Component);
                DataProvider.SetInterface(Cast<ISuspenseEquipmentDataProvider>(Component));

                // Bridge can use DataProvider directly
                UE_LOG(LogTemp, Log, TEXT("[UIManager] Found EquipmentDataProvider on Pawn: %s"),
                    *Component->GetName());

                // Notify success
                if (USuspenseEventManager* EventManager = GetEventManager())
                {
                    FGameplayTag ContainerUpdateTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Event.ContainerUpdated"));
                    EventManager->NotifyUIEvent(Bridge, ContainerUpdateTag, TEXT("Equipment"));
                }

                // Refresh UI after connection
                FTimerHandle RefreshTimer;
                GetWorld()->GetTimerManager().SetTimerForNextTick([Bridge]()
                {
                    ISuspenseEquipmentUIBridgeInterface::Execute_RefreshEquipmentUI(Bridge);
                });

                return;
            }

            // Fallback: Check for legacy ISuspenseEquipment
            if (Component && Component->GetClass()->ImplementsInterface(USuspenseEquipment::StaticClass()))
            {
                TScriptInterface<ISuspenseEquipment> EquipInterface;
                EquipInterface.SetObject(Component);
                EquipInterface.SetInterface(Cast<ISuspenseEquipment>(Component));

                ISuspenseEquipmentUIBridgeInterface::Execute_SetEquipmentInterface(Bridge, EquipInterface);
                UE_LOG(LogTemp, Log, TEXT("[UIManager] Equipment bridge connected to Pawn component (legacy interface)"));

                // Notify and refresh
                if (USuspenseEventManager* EventManager = GetEventManager())
                {
                    FGameplayTag ContainerUpdateTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Event.ContainerUpdated"));
                    EventManager->NotifyUIEvent(Bridge, ContainerUpdateTag, TEXT("Equipment"));
                }

                FTimerHandle RefreshTimer;
                GetWorld()->GetTimerManager().SetTimerForNextTick([Bridge]()
                {
                    ISuspenseEquipmentUIBridgeInterface::Execute_RefreshEquipmentUI(Bridge);
                });

                return;
            }
        }
    }

    // Step 2: If not found on Pawn, search on PlayerState
    if (APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>())
    {
        TArray<UActorComponent*> Components = PlayerState->GetComponents().Array();
        for (UActorComponent* Component : Components)
        {
            // Check for ISuspenseEquipmentDataProvider interface (new architecture)
            if (Component && Component->GetClass()->ImplementsInterface(USuspenseEquipmentDataProvider::StaticClass()))
            {
                TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;
                DataProvider.SetObject(Component);
                DataProvider.SetInterface(Cast<ISuspenseEquipmentDataProvider>(Component));

                UE_LOG(LogTemp, Log, TEXT("[UIManager] Found EquipmentDataProvider on PlayerState: %s"),
                    *Component->GetName());

                // Notify success
                if (USuspenseEventManager* EventManager = GetEventManager())
                {
                    FGameplayTag ContainerUpdateTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Event.ContainerUpdated"));
                    EventManager->NotifyUIEvent(Bridge, ContainerUpdateTag, TEXT("Equipment"));
                }

                // Refresh UI after connection
                FTimerHandle RefreshTimer;
                GetWorld()->GetTimerManager().SetTimerForNextTick([Bridge]()
                {
                    ISuspenseEquipmentUIBridgeInterface::Execute_RefreshEquipmentUI(Bridge);
                });

                return;
            }

            // Fallback: Check for legacy interface
            if (Component && Component->GetClass()->ImplementsInterface(USuspenseEquipment::StaticClass()))
            {
                TScriptInterface<ISuspenseEquipment> EquipInterface;
                EquipInterface.SetObject(Component);
                EquipInterface.SetInterface(Cast<ISuspenseEquipment>(Component));

                ISuspenseEquipmentUIBridgeInterface::Execute_SetEquipmentInterface(Bridge, EquipInterface);
                UE_LOG(LogTemp, Log, TEXT("[UIManager] Equipment bridge connected to PlayerState component (legacy interface)"));

                // Notify and refresh
                if (USuspenseEventManager* EventManager = GetEventManager())
                {
                    FGameplayTag ContainerUpdateTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Event.ContainerUpdated"));
                    EventManager->NotifyUIEvent(Bridge, ContainerUpdateTag, TEXT("Equipment"));
                }

                FTimerHandle RefreshTimer;
                GetWorld()->GetTimerManager().SetTimerForNextTick([Bridge]()
                {
                    ISuspenseEquipmentUIBridgeInterface::Execute_RefreshEquipmentUI(Bridge);
                });

                return;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[UIManager] No equipment component found for bridge connection"));
    UE_LOG(LogTemp, Warning, TEXT("[UIManager] Make sure EquipmentDataStore is present on PlayerState"));
}

void USuspenseUIManager::ConnectInventoryBridgeToGameComponent(USuspenseInventoryUIBridge* Bridge, APlayerController* PlayerController)
{
    if (!Bridge || !PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] ConnectInventoryBridgeToGameComponent - Invalid parameters"));
        return;
    }

    APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();
    if (!PlayerState)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] No PlayerState found"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Searching for inventory component on PlayerState..."));

    TArray<UActorComponent*> Components = PlayerState->GetComponents().Array();
    bool bFound = false;

    for (UActorComponent* Component : Components)
    {
        if (Component && Component->GetClass()->ImplementsInterface(USuspenseInventory::StaticClass()))
        {
            TScriptInterface<ISuspenseInventory> InventoryInterface;
            InventoryInterface.SetObject(Component);
            InventoryInterface.SetInterface(Cast<ISuspenseInventory>(Component));

            Bridge->SetInventoryInterface(InventoryInterface);

            UE_LOG(LogTemp, Log, TEXT("[UIManager] ✅ Connected inventory bridge to component: %s"),
                *Component->GetName());

            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] ❌ Inventory component NOT FOUND on PlayerState!"));
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Available components:"));
        for (UActorComponent* Component : Components)
        {
            if (Component)
            {
                UE_LOG(LogTemp, Error, TEXT("  - %s (Class: %s)"),
                    *Component->GetName(),
                    *Component->GetClass()->GetName());
            }
        }
    }
}

void USuspenseUIManager::InitializeInventoryBridgeForLayout(
    APlayerController* PlayerController,
    USuspenseBaseLayoutWidget* LayoutWidget)
{
    if (!PlayerController || !LayoutWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] InitializeInventoryBridgeForLayout - Invalid parameters"));
        return;
    }

    // Create bridge if needed
    if (!InventoryUIBridge)
    {
        InventoryUIBridge = CreateInventoryUIBridge(PlayerController);
        if (!InventoryUIBridge)
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to create inventory bridge"));
            return;
        }
    }

    // ❌ СТАРЫЙ КОД: Подключение к game inventory
    if (APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>())
    {
        TArray<UActorComponent*> Components = PlayerState->GetComponents().Array();
        for (UActorComponent* Component : Components)
        {
            if (Component && Component->GetClass()->ImplementsInterface(USuspenseInventory::StaticClass()))
            {
                TScriptInterface<ISuspenseInventory> InventoryInterface;
                InventoryInterface.SetObject(Component);
                InventoryInterface.SetInterface(Cast<ISuspenseInventory>(Component));

                InventoryUIBridge->SetInventoryInterface(InventoryInterface);
                UE_LOG(LogTemp, Log, TEXT("[UIManager] Connected inventory bridge to game inventory"));
                break;
            }
        }
    }

    // ✅ НОВОЕ: Немедленная инициализация виджета с данными
    // Задержка нужна только для того, чтобы layout успел создать виджеты
    FTimerHandle InitHandle;
    GetWorld()->GetTimerManager().SetTimer(InitHandle, [this, PlayerController]()
    {
        if (!InventoryUIBridge)
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] Inventory bridge lost during delayed init"));
            return;
        }

        // Убеждаемся что bridge подключён к inventory
        if (!InventoryUIBridge->IsInventoryConnected_Implementation())
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIManager] Inventory not connected, reconnecting..."));
            ConnectInventoryBridgeToGameComponent(InventoryUIBridge, PlayerController);
        }

        // Находим виджет в layout и инициализируем ЕГО
        if (USuspenseInventoryWidget* InventoryWidget = Cast<USuspenseInventoryWidget>(
            FindWidgetInLayouts(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Inventory")))))
        {
            UE_LOG(LogTemp, Log, TEXT("[UIManager] Found inventory widget, initializing with data"));
            InventoryUIBridge->InitializeInventoryWidgetWithData(InventoryWidget);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] Inventory widget not found in layout after delay"));
        }

    }, 0.05f, false); // Короткая задержка только для создания виджетов
}

void USuspenseUIManager::InitializeEquipmentBridgeForLayout(APlayerController* PlayerController, USuspenseBaseLayoutWidget* LayoutWidget)
{
    if (!PlayerController || !LayoutWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] InitializeEquipmentBridgeForLayout - Invalid parameters"));
        return;
    }

    // Find equipment widget in the layout
    UUserWidget* EquipmentWidget = LayoutWidget->GetWidgetByTag(
        FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Equipment"))
    );

    if (!EquipmentWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Equipment widget not found in layout"));
        return;
    }

    // Create bridge if needed
    if (!EquipmentUIBridge)
    {
        EquipmentUIBridge = CreateEquipmentUIBridge(PlayerController);
        if (!EquipmentUIBridge)
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to create equipment bridge"));
            return;
        }
    }

    // The bridge will find the equipment widget through the layout system
    // No need to pass it explicitly - the bridge knows how to find it
    UE_LOG(LogTemp, Log, TEXT("[UIManager] Equipment bridge initialized for layout"));
}

bool USuspenseUIManager::HideCharacterScreen()
{
    return HideWidget(CharacterScreenTag, false);
}

bool USuspenseUIManager::IsCharacterScreenVisible() const
{
    UUserWidget* CharacterScreen = GetWidget(CharacterScreenTag);
    return CharacterScreen && CharacterScreen->IsVisible();
}

void USuspenseUIManager::CreateAutoCreateWidgets(UObject* OwningObject)
{
    if (!OwningObject)
    {
        return;
    }

    // Create all widgets with AutoCreate flag
    for (const FSuspenseWidgetInfo& Config : WidgetConfigurations)
    {
        if (Config.bAutoCreate && Config.WidgetClass)
        {
            // Use configuration to determine viewport addition
            CreateWidget(Config.WidgetClass, Config.WidgetTag, OwningObject, false);
        }
    }
}

void USuspenseUIManager::DestroyNonPersistentWidgets()
{
    TArray<FGameplayTag> WidgetsToDestroy;

    // Collect widgets to destroy
    for (const auto& Pair : ActiveWidgets)
    {
        if (FSuspenseWidgetInfo* Config = ConfigurationCache.Find(Pair.Key))
        {
            if (!Config->bPersistent)
            {
                WidgetsToDestroy.Add(Pair.Key);
            }
        }
    }

    // Destroy collected widgets
    for (const FGameplayTag& Tag : WidgetsToDestroy)
    {
        DestroyWidget(Tag);
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Destroyed %d non-persistent widgets"),
        WidgetsToDestroy.Num());
}

UUserWidget* USuspenseUIManager::InitializeMainHUD(UObject* OwningObject)
{
    if (!MainHUDClass || !OwningObject)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Cannot initialize HUD - invalid parameters"));
        return nullptr;
    }

    // Create main HUD - HUD always added to viewport
    UUserWidget* HUDWidget = CreateWidget(MainHUDClass, MainHUDTag, OwningObject, true);
    if (!HUDWidget)
    {
        return nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Main HUD initialized successfully"));
    return HUDWidget;
}

void USuspenseUIManager::RequestHUDUpdate()
{
    // Get main HUD
    UUserWidget* HUDWidget = GetWidget(MainHUDTag);
    if (!HUDWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] No HUD widget found for update"));
        return;
    }

    // HUD will handle update internally by querying interfaces
    if (HUDWidget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_UpdateWidget(HUDWidget, 0.0f);
    }

    // Notify through event system
    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        EventManager->NotifyEquipmentUpdated();
    }
}

bool USuspenseUIManager::RegisterExternalWidget(UUserWidget* Widget, FGameplayTag WidgetTag)
{
    if (!Widget || !WidgetTag.IsValid())
    {
        return false;
    }

    // Check if tag already exists
    if (ActiveWidgets.Contains(WidgetTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Widget tag %s already registered"),
            *WidgetTag.ToString());
        return false;
    }

    // Register widget
    ActiveWidgets.Add(WidgetTag, Widget);

    // Set tag through interface if supported
    if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_SetWidgetTag(Widget, WidgetTag);
    }
    else if (USuspenseBaseWidget* BaseWidget = Cast<USuspenseBaseWidget>(Widget))
    {
        // Fallback for base widget without interface
        BaseWidget->WidgetTag = WidgetTag;
    }

    // Notify about creation
    NotifyWidgetCreated(Widget, WidgetTag);

    return true;
}

UUserWidget* USuspenseUIManager::UnregisterWidget(FGameplayTag WidgetTag)
{
    UUserWidget** WidgetPtr = ActiveWidgets.Find(WidgetTag);
    if (!WidgetPtr || !*WidgetPtr)
    {
        return nullptr;
    }

    UUserWidget* Widget = *WidgetPtr;
    ActiveWidgets.Remove(WidgetTag);
    WidgetParentMap.Remove(Widget);

    // Don't destroy, just unregister
    NotifyWidgetDestroyed(WidgetTag);

    return Widget;
}

void USuspenseUIManager::BuildConfigurationCache()
{
    ConfigurationCache.Empty();

    for (const FSuspenseWidgetInfo& Config : WidgetConfigurations)
    {
        if (Config.WidgetTag.IsValid())
        {
            ConfigurationCache.Add(Config.WidgetTag, Config);
        }
    }
}

bool USuspenseUIManager::IsWidgetValid(UUserWidget* Widget) const
{
    return Widget && IsValid(Widget);
}

void USuspenseUIManager::CleanupWidget(UUserWidget* Widget)
{
    if (!IsWidgetValid(Widget))
    {
        return;
    }

    // Call uninitialize if supported
    if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_UninitializeWidget(Widget);
    }

    // Remove from viewport
    if (Widget->IsInViewport())
    {
        Widget->RemoveFromParent();
    }

    // Mark for garbage collection
    Widget->ConditionalBeginDestroy();
}

void USuspenseUIManager::NotifyWidgetCreated(UUserWidget* Widget, FGameplayTag WidgetTag)
{
    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        EventManager->NotifyUIWidgetCreated(Widget);

        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Event.WidgetCreated"));
        FString EventData = FString::Printf(TEXT("Tag:%s"), *WidgetTag.ToString());

        EventManager->NotifyUIEvent(Widget, EventTag, EventData);
    }
}

void USuspenseUIManager::NotifyWidgetDestroyed(FGameplayTag WidgetTag)
{
    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        // Send destruction event
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag("UI.Event.WidgetDestroyed");
        FString EventData = FString::Printf(TEXT("Tag:%s"), *WidgetTag.ToString());
        EventManager->NotifyEquipmentEvent(this, EventTag, EventData);
    }
}

USuspenseEventManager* USuspenseUIManager::GetEventManager() const
{
    if (CachedEventManager)
    {
        return CachedEventManager;
    }

    // Try to get from game instance
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<USuspenseEventManager>();
    }

    return nullptr;
}

APlayerController* USuspenseUIManager::GetPlayerControllerFromObject(UObject* Object) const
{
    if (!Object)
    {
        return nullptr;
    }

    // Direct cast
    if (APlayerController* PC = Cast<APlayerController>(Object))
    {
        return PC;
    }

    // Try to get from actor
    if (AActor* Actor = Cast<AActor>(Object))
    {
        if (APawn* Pawn = Cast<APawn>(Actor))
        {
            return Cast<APlayerController>(Pawn->GetController());
        }

        // Check if actor implements controller interface
        if (Actor->GetClass()->ImplementsInterface(USuspenseController::StaticClass()))
        {
            if (APawn* ControlledPawn = ISuspenseController::Execute_GetControlledPawn(Actor))
            {
                return Cast<APlayerController>(ControlledPawn->GetController());
            }
        }
    }

    // Try world context
    if (UWorld* World = Object->GetWorld())
    {
        // Get first player controller as fallback
        return World->GetFirstPlayerController();
    }

    return nullptr;
}

int32 USuspenseUIManager::GetZOrderForWidget(FGameplayTag WidgetTag) const
{
    // Special Z-order values for different widget types
    if (WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.HUD"))))
    {
        return 50; // HUD elements - base level
    }
    else if (WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Character"))))
    {
        return 200; // Character Screen above HUD
    }
    else if (WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Menu"))))
    {
        return 150; // Menus
    }
    else if (WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Dialog"))))
    {
        return 180; // Dialogs
    }
    else if (WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Tooltip"))))
    {
        return 1000; // Tooltips above everything
    }

    // Check configuration
    if (const FSuspenseWidgetInfo* Config = ConfigurationCache.Find(WidgetTag))
    {
        return Config->ZOrder;
    }

    // Default value
    return 100;
}

bool USuspenseUIManager::ShouldAutoAddToViewport(FGameplayTag WidgetTag) const
{
    // Character Screen NOT auto-added to viewport
    if (WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Character"))))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[UIManager] Character screen will NOT be auto-added to viewport"));
        return false;
    }

    // Check configuration
    if (const FSuspenseWidgetInfo* Config = ConfigurationCache.Find(WidgetTag))
    {
        return Config->bAutoAddToViewport;
    }

    // Default add to viewport
    return true;
}

bool USuspenseUIManager::IsRootWidgetTag(FGameplayTag WidgetTag) const
{
    return WidgetTag.MatchesTag(MainHUDTag) ||
           WidgetTag.MatchesTag(CharacterScreenTag) ||
           WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Menu"))) ||
           WidgetTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Dialog")));
}

void USuspenseUIManager::SubscribeToLayoutEvents()
{
    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        // Subscribe to layout widget creation events
        LayoutWidgetCreatedHandle = EventManager->SubscribeToUIEvent(
            [this](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
            {
                if (EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Layout.WidgetCreated"))))
                {
                    OnLayoutWidgetCreated(Source, EventData);
                }
            });

        // Subscribe to layout widget destruction events
        LayoutWidgetDestroyedHandle = EventManager->SubscribeToUIEvent(
            [this](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
            {
                if (EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Layout.WidgetDestroyed"))))
                {
                    OnLayoutWidgetDestroyed(EventData);
                }
            });
    }
}

void USuspenseUIManager::UnsubscribeFromLayoutEvents()
{
    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        EventManager->UniversalUnsubscribe(LayoutWidgetCreatedHandle);
        EventManager->UniversalUnsubscribe(LayoutWidgetDestroyedHandle);
    }
}

void USuspenseUIManager::OnLayoutWidgetCreated(UObject* Source, const FString& EventData)
{
    // EventData format: "Widget:WidgetName,Tag:TagName,Parent:ParentName"
    TMap<FString, FString> ParsedData;
    TArray<FString> Pairs;
    EventData.ParseIntoArray(Pairs, TEXT(","));

    for (const FString& Pair : Pairs)
    {
        FString Key, Value;
        if (Pair.Split(TEXT(":"), &Key, &Value))
        {
            ParsedData.Add(Key, Value);
        }
    }

    // Extract widget tag
    FString TagString = ParsedData.FindRef(TEXT("Tag"));
    if (!TagString.IsEmpty())
    {
        FGameplayTag WidgetTag = FGameplayTag::RequestGameplayTag(*TagString);
        if (WidgetTag.IsValid() && Source)
        {
            if (UUserWidget* Widget = Cast<UUserWidget>(Source))
            {
                // Find parent layout
                UUserWidget* ParentLayout = nullptr;
                FString ParentName = ParsedData.FindRef(TEXT("Parent"));
                if (!ParentName.IsEmpty())
                {
                    // Search for parent by name
                    for (const auto& Pair : ActiveWidgets)
                    {
                        if (Pair.Value && Pair.Value->GetName() == ParentName)
                        {
                            ParentLayout = Pair.Value;
                            break;
                        }
                    }
                }

                // Register the widget
                RegisterLayoutWidget(Widget, WidgetTag, ParentLayout);
            }
        }
    }
}

void USuspenseUIManager::OnLayoutWidgetDestroyed(const FString& EventData)
{
    // EventData format: "Tag:TagName"
    FString TagString;
    if (EventData.Split(TEXT(":"), nullptr, &TagString))
    {
        FGameplayTag WidgetTag = FGameplayTag::RequestGameplayTag(*TagString);
        if (WidgetTag.IsValid())
        {
            UnregisterLayoutWidget(WidgetTag);
        }
    }
}

USuspenseInventoryUIBridge* USuspenseUIManager::CreateInventoryUIBridge(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] CreateInventoryUIBridge - invalid PlayerController"));
        return nullptr;
    }

    // Check if bridge already exists
    if (InventoryUIBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Inventory bridge already exists"));
        return InventoryUIBridge;
    }

    // Create bridge instance
    USuspenseInventoryUIBridge* Bridge = NewObject<USuspenseInventoryUIBridge>(this,
        USuspenseInventoryUIBridge::StaticClass(),
        TEXT("InventoryUIBridge"));

    if (!Bridge)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to create inventory bridge"));
        return nullptr;
    }

    // Initialize bridge
    if (!Bridge->Initialize(PlayerController))
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to initialize inventory bridge"));
        Bridge->ConditionalBeginDestroy();
        return nullptr;
    }

    // Store reference
    InventoryUIBridge = Bridge;

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Inventory bridge created and initialized"));
    return Bridge;
}

USuspenseInventoryUIBridge* USuspenseUIManager::GetInventoryUIBridge() const
{
    return InventoryUIBridge;
}

void USuspenseUIManager::InitializeInventoryBridge()
{
    // DO NOT create bridges at startup
    // They will be created on-demand when specific tabs are opened
    UE_LOG(LogTemp, Log, TEXT("[UIManager] Inventory bridge will be initialized on-demand"));
}

//=============================================================================
// Equipment Bridge Implementation
//=============================================================================

void USuspenseUIManager::InitializeEquipmentBridge()
{
    // DO NOT create bridges at startup
    // They will be created on-demand when specific tabs are opened
    UE_LOG(LogTemp, Log, TEXT("[UIManager] Equipment bridge will be initialized on-demand"));
}

USuspenseEquipmentUIBridge* USuspenseUIManager::CreateEquipmentUIBridge(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] CreateEquipmentUIBridge - invalid PlayerController"));
        return nullptr;
    }

    if (EquipmentUIBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Equipment bridge already exists"));
        return EquipmentUIBridge;
    }

    USuspenseEquipmentUIBridge* Bridge = NewObject<USuspenseEquipmentUIBridge>(
        this,
        USuspenseEquipmentUIBridge::StaticClass(),
        TEXT("EquipmentUIBridge")
    );

    if (!Bridge)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManager] Failed to create equipment bridge"));
        return nullptr;
    }

    // Initialize() возвращает void — просто вызываем
    Bridge->Initialize(PlayerController);

    // сохраняем ссылку
    EquipmentUIBridge = Bridge;

    UE_LOG(LogTemp, Log, TEXT("[UIManager] Equipment bridge created and initialized"));
    return Bridge;
}

USuspenseEquipmentUIBridge* USuspenseUIManager::GetEquipmentUIBridge() const
{
    return EquipmentUIBridge;
}

void USuspenseUIManager::ShowNotification(const FText& Message, float Duration, FLinearColor Color)
{
    // Базовая реализация - отправляем событие через EventDelegateManager
    // Реальное отображение будет обрабатываться HUD или специальным notification widget

    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        // Конвертируем FText в FString для передачи через событие
        FString MessageString = Message.ToString();

        // Отправляем событие уведомления
        EventManager->NotifyUI(MessageString, Duration);

        // Дополнительно можем создать специальный виджет уведомления если класс задан
        if (NotificationWidgetClass)
        {
            // Получаем PlayerController
            APlayerController* PC = nullptr;
            if (UWorld* World = GetWorld())
            {
                PC = World->GetFirstPlayerController();
            }

            if (PC)
            {
                // Создаем временный виджет уведомления
                UUserWidget* NotificationWidget = ::CreateWidget<UUserWidget>(PC, NotificationWidgetClass);
                if (NotificationWidget)
                {
                    // Устанавливаем данные через интерфейс если поддерживается
                    if (NotificationWidget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
                    {
                        // Передаем текст через специальный метод интерфейса
                        // Это потребует расширения интерфейса ISuspenseUIWidgetInterface
                    }

                    // Добавляем в viewport с высоким Z-order
                    NotificationWidget->AddToViewport(500);

                    // Устанавливаем таймер на удаление
                    FTimerHandle RemovalTimer;
                    GetWorld()->GetTimerManager().SetTimer(RemovalTimer,
                        [NotificationWidget]()
                        {
                            if (IsValid(NotificationWidget))
                            {
                                NotificationWidget->RemoveFromParent();
                                NotificationWidget->ConditionalBeginDestroy();
                            }
                        },
                        Duration, false);
                }
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[UIManager] Notification shown: %s (Duration: %.1fs, Color: R=%.2f G=%.2f B=%.2f)"),
            *MessageString, Duration, Color.R, Color.G, Color.B);
    }
}

void USuspenseUIManager::ShowNotificationWithIcon(const FText& Message, UTexture2D* Icon, float Duration, FLinearColor Color)
{
    // Расширенная версия с иконкой
    // Пока просто вызываем базовую версию
    ShowNotification(Message, Duration, Color);

    // В будущем можно передавать иконку через расширенное событие
    if (Icon)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[UIManager] Notification with icon: %s"), *Icon->GetName());
    }
}

void USuspenseUIManager::ClearAllNotifications()
{
    // Отправляем событие очистки всех уведомлений
    if (USuspenseEventManager* EventManager = GetEventManager())
    {
        // Можно создать специальное событие для очистки
        FGameplayTag ClearTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Notification.ClearAll"));
        EventManager->NotifyUIEvent(this, ClearTag, TEXT("ClearAll"));
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManager] All notifications cleared"));
}

// Convert data from table to UI data
FItemUIData USuspenseUIManager::ConvertUnifiedItemDataToUI(const FSuspenseUnifiedItemData& UnifiedData, int32 Quantity) const
{
    FItemUIData UIData;

    // Generate unique ID for instance
    UIData.ItemInstanceID = FGuid::NewGuid();

    // Basic data
    UIData.ItemID = UnifiedData.ItemID;
    UIData.DisplayName = UnifiedData.DisplayName;
    UIData.Description = UnifiedData.Description;

    // Set icon through safe setter
    if (!UnifiedData.Icon.IsNull())
    {
        if (UTexture2D* IconTexture = UnifiedData.Icon.LoadSynchronous())
        {
            UIData.SetIcon(IconTexture);
        }
    }

    // Size and quantity - GridSize уже FIntPoint, просто копируем
    UIData.GridSize = UnifiedData.GridSize;
    UIData.MaxStackSize = UnifiedData.MaxStackSize;
    UIData.Quantity = FMath::Clamp(Quantity, 1, UnifiedData.MaxStackSize);
    UIData.Weight = UnifiedData.Weight;
    UIData.ItemType = UnifiedData.ItemType;

    // Equipment data
    UIData.bIsEquippable = UnifiedData.bIsEquippable;
    UIData.EquipmentSlotType = UnifiedData.EquipmentSlot;
    UIData.bIsUsable = UnifiedData.bIsConsumable;

    // Weapon data - теперь просто показываем тип оружия и патронов
    // Реальные характеристики (damage, fire rate и т.д.) хранятся в AttributeSet
    if (UnifiedData.bIsWeapon)
    {
        UIData.bHasAmmo = true;

        // Создаем текст с базовой информацией об оружии
        UIData.AmmoText = FText::Format(
            NSLOCTEXT("Item", "WeaponInfo", "Type: {0} | Ammo: {1}"),
            GameplayTagToDisplayText(UnifiedData.WeaponArchetype),
            GameplayTagToDisplayText(UnifiedData.AmmoType)
        );
    }
    else
    {
        UIData.bHasAmmo = false;
        UIData.AmmoText = FText::GetEmpty();
    }

    // Initialize other fields
    UIData.AnchorSlotIndex = INDEX_NONE;
    UIData.bIsRotated = false;

    return UIData;
}

// Helper method to convert tag to text
FText USuspenseUIManager::GameplayTagToDisplayText(const FGameplayTag& Tag) const
{
    if (!Tag.IsValid())
    {
        return FText::GetEmpty();
    }

    FString TagString = Tag.ToString();
    FString DisplayName;

    if (TagString.Split(TEXT("."), nullptr, &DisplayName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        return FText::FromString(DisplayName);
    }

    return FText::FromString(TagString);
}
