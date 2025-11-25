// Copyright Suspense Team Team. All Rights Reserved.

#include "Components/SuspenseInventoryUIBridge.h"
#include "Widgets/Inventory/SuspenseInventoryWidget.h"
#include "Components/SuspenseUIManager.h"
#include "Delegates/EventDelegateManager.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Equipment/EquipmentTypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SuspenseEquipmentUIBridge.h"
#include "Interfaces/UI/ISuspenseContainerUIInterface.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Interfaces/UI/ISuspenseUIWidgetInterface.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Tabs/SuspenseUpperTabBar.h"
#include "Widgets/Layout/SuspenseBaseLayoutWidget.h"
#include "Components/GridPanel.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/UI/ISuspenseEquipmentUIBridgeWidget.h"

// Global static variable for bridge instance storage
static TWeakInterfacePtr<ISuspenseInventoryUIBridgeWidget> GInventoryUIBridge;

// =====================================================
// Constructor & Core Lifecycle
// =====================================================

USuspenseInventoryUIBridge::USuspenseInventoryUIBridge()
{
    bIsInitialized = false;
    OwningPlayerController = nullptr;
    UIManager = nullptr;
    EventManager = nullptr;
    LastWidgetCacheValidationTime = 0.0f;
    
    // Try to load default widget class from known project location
    static ConstructorHelpers::FClassFinder<USuspenseInventoryWidget> DefaultWidgetFinder(
        TEXT("/Game/MEDCOM/UI/Inventory/W_InventoryGrid")
    );
    
    if (DefaultWidgetFinder.Succeeded())
    {
        InventoryWidgetClass = DefaultWidgetFinder.Class;
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found default inventory widget class"));
    }
    else
    {
        // If blueprint doesn't exist, use base C++ class
        InventoryWidgetClass = USuspenseInventoryWidget::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Using C++ base class for inventory widget"));
    }
}

bool USuspenseInventoryUIBridge::Initialize(APlayerController* InPlayerController)
{
    if (bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Already initialized"));
        return true;
    }
    
    if (!InPlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Invalid player controller"));
        return false;
    }
    
    OwningPlayerController = InPlayerController;
    
    // Get UI manager from world
    UIManager = USuspenseUIManager::Get(InPlayerController);
    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to get UI Manager"));
        return false;
    }
    
    // Register self globally
    RegisterBridge(this);
    
    bIsInitialized = true;
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Initialized successfully"));
    return true;
}

void USuspenseInventoryUIBridge::SetInventoryInterface(TScriptInterface<ISuspenseInventoryInterface> InInventory)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Attempt to set inventory before initialization"));
        return;
    }
    
    // Unsubscribe from old events
    if (GameInventory.GetInterface())
    {
        UnsubscribeFromEvents();
    }
    
    // Clear cached data when inventory changes
    InvalidateWidgetCache();
    PendingUIUpdates.Empty();
    
    GameInventory = InInventory;
    
    if (GameInventory.GetInterface())
    {
        // Get event manager
        EventManager = ISuspenseInventoryInterface::GetDelegateManagerStatic(GameInventory.GetObject());
        if (!EventManager)
        {
            UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to get event manager"));
            return;
        }
        
        // Subscribe to events
        SubscribeToEvents();
        
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Connected to inventory interface"));
        
        // Schedule initial refresh
        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.Full")));
    }
}

void USuspenseInventoryUIBridge::SetInventoryWidgetClass(TSubclassOf<USuspenseInventoryWidget> WidgetClass)
{
    if (WidgetClass)
    {
        InventoryWidgetClass = WidgetClass;
        // Invalidate cache when widget class changes
        InvalidateWidgetCache();
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Inventory widget class set to: %s"), 
            *WidgetClass->GetName());
    }
}

void USuspenseInventoryUIBridge::Shutdown()
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Shutdown called"));
    
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Already shutdown"));
        return;
    }
    
    // Cancel any pending updates
    if (UWorld* World = GetWorld())
    {
        if (BatchedUpdateTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(BatchedUpdateTimerHandle);
        }
        // Clear all timers for this object
        World->GetTimerManager().ClearAllTimersForObject(this);
    }
    
    // Hide Character Screen if open
    if (IsCharacterScreenVisible_Implementation())
    {
        HideCharacterScreen_Implementation();
    }
    
    // Unsubscribe from all events
    UnsubscribeFromEvents();
    
    // Unregister bridge
    UnregisterBridge();
    
    // Find and uninitialize inventory widget in Character Screen
    if (USuspenseInventoryWidget* InventoryWidget = GetCachedInventoryWidget())
    {
        // Unsubscribe widget from events through interface
        if (InventoryWidget->GetClass()->ImplementsInterface(USuspenseUIWidgetInterface::StaticClass()))
        {
            ISuspenseUIWidgetInterface::Execute_UninitializeWidget(InventoryWidget);
        }
        
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Inventory widget found and uninitialized"));
    }
    
    // Clear all cached data
    CurrentDragData = FDragDropUIData();
    InvalidateWidgetCache();
    PendingUIUpdates.Empty();
    
    // Clear all references
    OwningPlayerController = nullptr;
    GameInventory = nullptr;
    UIManager = nullptr;
    EventManager = nullptr;
    
    // Reset initialization flag
    bIsInitialized = false;
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Shutdown completed"));
}

// =====================================================
// External Bridge Operations
// =====================================================

bool USuspenseInventoryUIBridge::RemoveItemFromInventorySlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutRemovedInstance)
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] RemoveItemFromInventorySlot - Slot: %d"), SlotIndex);
    
    if (!ValidateInventoryConnection())
    {
        return false;
    }
    
    ISuspenseInventoryInterface* Inventory = GameInventory.GetInterface();
    
    // Get item instance at slot
    FSuspenseInventoryItemInstance ItemInstance;
    if (!Inventory->GetItemInstanceAtSlot(SlotIndex, ItemInstance))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No item at slot %d to remove"), SlotIndex);
        return false;
    }
    
    // Save data for return
    OutRemovedInstance = ItemInstance;
    
    // Remove item from inventory
    FInventoryOperationResult Result = Inventory->RemoveItemByID(
        ItemInstance.ItemID, 
        ItemInstance.Quantity
    );
    
    if (Result.IsSuccess())
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Successfully removed %s (x%d) from slot %d"), 
            *ItemInstance.ItemID.ToString(), 
            ItemInstance.Quantity, 
            SlotIndex);
            
        // Schedule UI update instead of immediate refresh
        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemRemoved")));
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to remove item: %s"), 
            *Result.ErrorMessage.ToString());
    }
    
    return false;
}

bool USuspenseInventoryUIBridge::RestoreItemToInventory(const FSuspenseInventoryItemInstance& ItemInstance)
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] RestoreItemToInventory - Item: %s"), 
        *ItemInstance.ItemID.ToString());
    
    if (!ValidateInventoryConnection())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No inventory connection for restore"));
        return false;
    }
    
    // Add item instance through interface
    FInventoryOperationResult Result = GameInventory.GetInterface()->AddItemInstance(ItemInstance);
    
    if (Result.IsSuccess())
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Item restored successfully"));
        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemAdded")));
        return true;
    }
    
    // Try fallback method through Blueprint interface
    bool bFallbackSuccess = ISuspenseInventoryInterface::Execute_AddItemByID(
        GameInventory.GetObject(),
        ItemInstance.ItemID,
        ItemInstance.Quantity
    );
    
    if (bFallbackSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Item restored using fallback method (runtime properties may be lost)"));
        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemAdded")));
        return true;
    }
    
    UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to restore item: %s"), 
        *Result.ErrorMessage.ToString());
    return false;
}

// =====================================================
// ISuspenseInventoryUIBridgeWidget Interface Implementation
// =====================================================

void USuspenseInventoryUIBridge::ShowInventoryUI_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] ShowInventoryUI -> Delegating to ShowCharacterScreenWithTab"));
    
    // Show Character Screen with inventory tab
    ShowCharacterScreenWithTab_Implementation(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory")));
}

void USuspenseInventoryUIBridge::HideInventoryUI_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] HideInventoryUI -> Delegating to HideCharacterScreen"));
    
    // Hide Character Screen
    HideCharacterScreen_Implementation();
}

void USuspenseInventoryUIBridge::ToggleInventoryUI_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] ToggleInventoryUI -> Delegating to ToggleCharacterScreen"));
    
    // Toggle Character Screen
    ToggleCharacterScreen_Implementation();
}

bool USuspenseInventoryUIBridge::IsInventoryUIVisible_Implementation() const
{
    // Check Character Screen visibility
    return IsCharacterScreenVisible_Implementation();
}

void USuspenseInventoryUIBridge::RefreshInventoryUI_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] === RefreshInventoryUI START ==="));
    
    // Step 1: Check connection
    if (!ValidateInventoryConnection())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No inventory connection"));
        return;
    }
    
    // Step 2: Find inventory widget (use cached version for performance)
    USuspenseInventoryWidget* InventoryWidget = GetCachedInventoryWidget();
    if (!InventoryWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No inventory widget found"));
        return;
    }
    
    // Step 3: Convert inventory data to UI format
    FContainerUIData ContainerData;
    bool bConversionSuccess = ConvertInventoryToUIData(ContainerData);
    
    if (!bConversionSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to convert inventory data"));
        return;
    }
    
    // Step 4: Check widget initialization
    if (!InventoryWidget->IsFullyInitialized())
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Widget not initialized, initializing now"));
        ISuspenseContainerUIInterface::Execute_InitializeContainer(InventoryWidget, ContainerData);
    }
    else
    {
        // Step 5: Update existing widget
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Updating widget with %d items"), 
            ContainerData.Items.Num());
        ISuspenseContainerUIInterface::Execute_UpdateContainer(InventoryWidget, ContainerData);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] === RefreshInventoryUI END ==="));
}

void USuspenseInventoryUIBridge::OnInventoryDataChanged_Implementation(const FGameplayTag& ChangeType)
{
    // Log change type
    UE_LOG(LogTemp, Verbose, TEXT("[InventoryUIBridge] Inventory data changed: %s"), 
        *ChangeType.ToString());
    
    // Schedule update instead of immediate refresh
    ScheduleUIUpdate(ChangeType);
}

bool USuspenseInventoryUIBridge::IsInventoryConnected_Implementation() const
{
    return GameInventory.GetInterface() != nullptr;
}

bool USuspenseInventoryUIBridge::GetInventoryGridSize_Implementation(int32& OutColumns, int32& OutRows) const
{
    if (GameInventory.GetInterface())
    {
        FVector2D InventorySize = GameInventory.GetInterface()->GetInventorySize();
        OutColumns = FMath::RoundToInt(InventorySize.X);
        OutRows = FMath::RoundToInt(InventorySize.Y);
        
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Using actual inventory size %dx%d from game component"), 
            OutColumns, OutRows);
        
        return true;
    }
    
    // Default fallback values
    OutColumns = 10;
    OutRows = 10;
    
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No inventory interface, using default size %dx%d"), 
        OutColumns, OutRows);
    
    return true;
}

int32 USuspenseInventoryUIBridge::GetInventorySlotCount_Implementation() const
{
    int32 Columns, Rows;
    if (GetInventoryGridSize_Implementation(Columns, Rows))
    {
        return Columns * Rows;
    }
    return 50; // Default fallback
}

// =====================================================
// Character Screen Management
// TODO: Extract to CharacterScreenManager
// =====================================================

void USuspenseInventoryUIBridge::ShowCharacterScreenWithTab_Implementation(const FGameplayTag& TabTag)
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] ShowCharacterScreenWithTab called with tab: %s"), *TabTag.ToString());
    
    // Validate initialization
    if (!bIsInitialized || !UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Not initialized"));
        return;
    }
    
    // Validate inventory connection
    if (!ValidateInventoryConnection())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No valid inventory connection"));
        return;
    }
    
    // Show Character Screen through UIManager
    UUserWidget* CharacterScreen = UIManager->ShowCharacterScreen(OwningPlayerController, TabTag);
    if (!CharacterScreen)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to show Character Screen"));
        return;
    }
    
    // Invalidate widget cache when showing new screen
    InvalidateWidgetCache();
    
    // ✅ УБИРАЕМ ДВОЙНУЮ ИНИЦИАЛИЗАЦИЮ
    // UIManager теперь сам инициализирует виджет через InitializeInventoryBridgeForLayout
    // Нам НЕ нужно делать это второй раз
    
    // Notify about opening
    if (EventManager)
    {
        FGameplayTag OpenedTag = FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Opened"));
        EventManager->NotifyUIEvent(this, OpenedTag, TabTag.ToString());
    }
}

void USuspenseInventoryUIBridge::HideCharacterScreen_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] HideCharacterScreen called"));
    
    if (!bIsInitialized || !UIManager)
    {
        return;
    }
    
    // Cancel any pending updates when hiding
    if (UWorld* World = GetWorld())
    {
        if (BatchedUpdateTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(BatchedUpdateTimerHandle);
        }
    }
    PendingUIUpdates.Empty();
    
    // Hide Character Screen through UIManager
    UIManager->HideCharacterScreen();
    
    // Invalidate widget cache
    InvalidateWidgetCache();
    
    // Notify about closing
    if (EventManager)
    {
        FGameplayTag ClosedTag = FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Closed"));
        EventManager->NotifyUIEvent(this, ClosedTag, TEXT(""));
    }
}

void USuspenseInventoryUIBridge::ToggleCharacterScreen_Implementation()
{
    if (IsCharacterScreenVisible_Implementation())
    {
        HideCharacterScreen_Implementation();
    }
    else
    {
        ShowCharacterScreenWithTab_Implementation(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory")));
    }
}

bool USuspenseInventoryUIBridge::IsCharacterScreenVisible_Implementation() const
{
    if (!bIsInitialized || !UIManager)
    {
        return false;
    }
    
    return UIManager->IsCharacterScreenVisible();
}

// =====================================================
// Static Registration
// =====================================================

void USuspenseInventoryUIBridge::RegisterBridge(USuspenseInventoryUIBridge* Bridge)
{
    if (Bridge)
    {
        // Set global bridge using interface method
        GInventoryUIBridge = TWeakInterfacePtr<ISuspenseInventoryUIBridgeWidget>(Bridge);
        ISuspenseInventoryUIBridgeWidget::SetGlobalBridge(Bridge);
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Bridge registered globally"));
    }
}

void USuspenseInventoryUIBridge::UnregisterBridge()
{
    GInventoryUIBridge.Reset();
    ISuspenseInventoryUIBridgeWidget::ClearGlobalBridge();
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Bridge unregistered"));
}

// =====================================================
// Widget Discovery & Management
// =====================================================

USuspenseInventoryWidget* USuspenseInventoryUIBridge::FindInventoryWidgetInCharacterScreen() const
{
    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No UIManager available"));
        return nullptr;
    }
    
    // Get Character Screen
    FGameplayTag CharacterScreenTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Character"));
    UUserWidget* CharacterScreen = UIManager->GetWidget(CharacterScreenTag);
    if (!CharacterScreen)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Character screen not found"));
        return nullptr;
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Searching for inventory widget in character screen"));
    
    // Method 1: Direct search for InventoryWidget
    TArray<UWidget*> AllWidgets;
    CharacterScreen->WidgetTree->GetAllWidgets(AllWidgets);
    
    // First, try to find inventory widget directly
    for (UWidget* Widget : AllWidgets)
    {
        if (USuspenseInventoryWidget* InventoryWidget = Cast<USuspenseInventoryWidget>(Widget))
        {
            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found inventory widget directly: %s"), 
                *Widget->GetName());
            return InventoryWidget;
        }
    }
    
    // Method 2: Find through TabBar
    for (UWidget* Widget : AllWidgets)
    {
        if (USuspenseUpperTabBar* TabBar = Cast<USuspenseUpperTabBar>(Widget))
        {
            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found TabBar, searching tabs"));
            
            // Get tab count
            int32 TabCount = TabBar->GetTabCount_Implementation();
            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] TabBar has %d tabs"), TabCount);
            
            // Search all tabs
            for (int32 i = 0; i < TabCount; i++)
            {
                FSuspenseTabConfig Config = TabBar->GetTabConfig(i);
                UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Tab[%d]: Name=%s, Tag=%s"), 
                    i, *Config.TabName.ToString(), *Config.TabTag.ToString());
                
                // Check both possible tags
                if (Config.TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"))) ||
                    Config.TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Character"))))
                {
                    // Get tab content
                    UUserWidget* TabContent = TabBar->GetTabContent_Implementation(i);
                    if (!TabContent)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Tab content is null for tab %d"), i);
                        continue;
                    }
                    
                    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found tab content: %s"), 
                        *TabContent->GetClass()->GetName());
                    
                    // Check if it's inventory widget
                    if (USuspenseInventoryWidget* InventoryWidget = Cast<USuspenseInventoryWidget>(TabContent))
                    {
                        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Successfully found inventory widget in tab"));
                        return InventoryWidget;
                    }
                    
                    // Check if it's a layout widget containing inventory
                    if (USuspenseBaseLayoutWidget* LayoutWidget = Cast<USuspenseBaseLayoutWidget>(TabContent))
                    {
                        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found layout widget, searching inside"));
                        
                        // Search for inventory inside layout
                        FGameplayTag InventoryTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Inventory"));
                        if (USuspenseInventoryWidget* InventoryInLayout = 
                            Cast<USuspenseInventoryWidget>(LayoutWidget->GetWidgetByTag(InventoryTag)))
                        {
                            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found inventory widget inside layout"));
                            return InventoryInLayout;
                        }
                    }
                }
            }
            
            // Also check currently active tab
            int32 CurrentTabIndex = TabBar->GetSelectedTabIndex_Implementation();
            if (CurrentTabIndex >= 0 && CurrentTabIndex < TabCount)
            {
                UUserWidget* CurrentContent = TabBar->GetTabContent_Implementation(CurrentTabIndex);
                if (USuspenseInventoryWidget* InventoryWidget = Cast<USuspenseInventoryWidget>(CurrentContent))
                {
                    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found inventory widget in current tab"));
                    return InventoryWidget;
                }
                
                // Check if current content is a layout
                if (USuspenseBaseLayoutWidget* LayoutWidget = Cast<USuspenseBaseLayoutWidget>(CurrentContent))
                {
                    FGameplayTag InventoryTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Inventory"));
                    if (USuspenseInventoryWidget* InventoryInLayout = 
                        Cast<USuspenseInventoryWidget>(LayoutWidget->GetWidgetByTag(InventoryTag)))
                    {
                        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found inventory widget in current layout tab"));
                        return InventoryInLayout;
                    }
                }
            }
        }
    }
    
    // Method 3: Search through UIManager's FindWidgetInLayouts
    FGameplayTag InventoryTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Inventory"));
    if (USuspenseInventoryWidget* InventoryWidget = Cast<USuspenseInventoryWidget>(UIManager->FindWidgetInLayouts(InventoryTag)))
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found inventory widget through UIManager layout search"));
        return InventoryWidget;
    }
    
    UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to find inventory widget in character screen"));
    return nullptr;
}

USuspenseInventoryWidget* USuspenseInventoryUIBridge::GetCachedInventoryWidget() const
{
    // Check if cache is still valid
    if (CachedInventoryWidget.IsValid())
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (CurrentTime - LastWidgetCacheValidationTime < WIDGET_CACHE_LIFETIME)
        {
            return CachedInventoryWidget.Get();
        }
    }
    
    // Cache is invalid, find widget and update cache
    USuspenseInventoryWidget* Widget = FindInventoryWidgetInCharacterScreen();
    if (Widget)
    {
        CachedInventoryWidget = Widget;
        LastWidgetCacheValidationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    }
    
    return Widget;
}

void USuspenseInventoryUIBridge::InvalidateWidgetCache()
{
    CachedInventoryWidget.Reset();
    LastWidgetCacheValidationTime = 0.0f;
}

void USuspenseInventoryUIBridge::InitializeInventoryWidgetWithData(USuspenseInventoryWidget* Widget)
{
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Cannot initialize widget - null widget"));
        return;
    }
    
    // ✅ ВАЖНО: Проверяем видимость виджета
    if (!Widget->IsVisible())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Widget is not visible, making visible"));
        Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    
    if (!ValidateInventoryConnection())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No inventory connection during widget initialization!"));
        
        // Try to reconnect to inventory
        if (OwningPlayerController && OwningPlayerController->PlayerState)
        {
            TArray<UActorComponent*> Components = OwningPlayerController->PlayerState->GetComponents().Array();
            for (UActorComponent* Component : Components)
            {
                if (Component && Component->GetClass()->ImplementsInterface(USuspenseInventoryInterface::StaticClass()))
                {
                    TScriptInterface<ISuspenseInventoryInterface> InventoryInterface;
                    InventoryInterface.SetObject(Component);
                    InventoryInterface.SetInterface(Cast<ISuspenseInventoryInterface>(Component));
                    
                    SetInventoryInterface(InventoryInterface);
                    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Successfully reconnected to inventory during widget init"));
                    break;
                }
            }
        }
        
        // Если всё ещё нет соединения - ошибка
        if (!ValidateInventoryConnection())
        {
            UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to establish inventory connection!"));
            return;
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Initializing inventory widget with data"));
    
    // Convert inventory data to UI format
    FContainerUIData ContainerData;
    bool bConversionSuccess = ConvertInventoryToUIData(ContainerData);
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Data conversion result: %s, Grid: %dx%d, Slots: %d, Items: %d, Weight: %.1f/%.1f"), 
        bConversionSuccess ? TEXT("Success") : TEXT("Failed"),
        ContainerData.GridSize.X,
        ContainerData.GridSize.Y,
        ContainerData.Slots.Num(),
        ContainerData.Items.Num(),
        ContainerData.CurrentWeight,
        ContainerData.MaxWeight);
    
    // ✅ КРИТИЧНО: Всегда инициализируем если слоты есть
    if (ContainerData.Slots.Num() > 0)
    {
        // Initialize widget with container interface
        if (Widget->GetClass()->ImplementsInterface(USuspenseContainerUIInterface::StaticClass()))
        {
            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Initializing widget (IsFullyInitialized=%s)"), 
                Widget->IsFullyInitialized() ? TEXT("true") : TEXT("false"));
            
            // Всегда вызываем Initialize, даже если виджет "инициализирован"
            // Это гарантирует что слоты созданы
            ISuspenseContainerUIInterface::Execute_InitializeContainer(Widget, ContainerData);
            
            // Force immediate layout update
            Widget->ForceLayoutPrepass();
            
            // Log final state
            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Widget initialized - GridSize: %dx%d, Slots created: %d, Items: %d"), 
                ContainerData.GridSize.X, 
                ContainerData.GridSize.Y,
                ContainerData.Slots.Num(),
                ContainerData.Items.Num());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Widget doesn't implement container interface"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No slots in container data! Cannot initialize widget"));
    }
}

void USuspenseInventoryUIBridge::UpdateInventoryWidgetData(USuspenseInventoryWidget* Widget)
{
    if (!Widget)
    {
        return;
    }
    
    // Convert inventory data to UI format
    FContainerUIData ContainerData;
    bool bConversionSuccess = ConvertInventoryToUIData(ContainerData);
    
    UE_LOG(LogTemp, Verbose, TEXT("[InventoryUIBridge] Update - Weight: %.1f/%.1f kg, Items: %d"), 
        ContainerData.CurrentWeight,
        ContainerData.MaxWeight,
        ContainerData.Items.Num());
    
    // Update widget even if conversion failed (will show empty inventory)
    if (Widget->GetClass()->ImplementsInterface(USuspenseContainerUIInterface::StaticClass()))
    {
        // Check if widget is already initialized
        if (!Widget->IsFullyInitialized())
        {
            ISuspenseContainerUIInterface::Execute_InitializeContainer(Widget, ContainerData);
            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Widget was not initialized, initialized now"));
        }
        else
        {
            ISuspenseContainerUIInterface::Execute_UpdateContainer(Widget, ContainerData);
            UE_LOG(LogTemp, Verbose, TEXT("[InventoryUIBridge] Widget updated"));
        }
    }
}

// =====================================================
// Data Conversion
// TODO: Extract to InventoryDataConverter class
// =====================================================

bool USuspenseInventoryUIBridge::ConvertInventoryToUIData(FContainerUIData& OutContainerData) const
{
    // Initialize basic container data
    OutContainerData.ContainerType = FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"));
    OutContainerData.DisplayName = NSLOCTEXT("Inventory", "InventoryTitle", "Inventory");
    
    // Step 1: Get grid size
    int32 GridCols, GridRows;
    GetInventoryGridSize_Implementation(GridCols, GridRows);
    OutContainerData.GridSize = FIntPoint(GridCols, GridRows);
    
    // Step 2: Check inventory connection
    if (!GameInventory.GetInterface())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No inventory interface for conversion"));
        
        // Create empty grid
        int32 TotalSlots = GridCols * GridRows;
        OutContainerData.Slots.Reserve(TotalSlots);
        
        for (int32 i = 0; i < TotalSlots; i++)
        {
            FSlotUIData EmptySlot;
            EmptySlot.SlotIndex = i;
            EmptySlot.GridX = i % GridCols;
            EmptySlot.GridY = i / GridCols;
            EmptySlot.bIsOccupied = false;
            OutContainerData.Slots.Add(EmptySlot);
        }
        
        OutContainerData.CurrentWeight = 0.0f;
        OutContainerData.MaxWeight = 100.0f;
        return true;
    }
    
    // Step 3: Get interface and data
    ISuspenseInventoryInterface* InventoryInterface = GameInventory.GetInterface();
    
    // Get weight parameters
    OutContainerData.CurrentWeight = ISuspenseInventoryInterface::Execute_GetCurrentWeight(GameInventory.GetObject());
    OutContainerData.MaxWeight = ISuspenseInventoryInterface::Execute_GetMaxWeight(GameInventory.GetObject());
    OutContainerData.bHasWeightLimit = true;
    
    // Get allowed item types
    OutContainerData.AllowedItemTypes = ISuspenseInventoryInterface::Execute_GetAllowedItemTypes(GameInventory.GetObject());
    
    // Step 4: Create slots for entire grid
    int32 TotalSlots = GridCols * GridRows;
    OutContainerData.Slots.Reserve(TotalSlots);
    
    // Pre-allocate slots for better performance
    for (int32 i = 0; i < TotalSlots; i++)
    {
        FSlotUIData SlotData;
        SlotData.SlotIndex = i;
        SlotData.GridX = i % GridCols;
        SlotData.GridY = i / GridCols;
        SlotData.bIsOccupied = false;
        SlotData.bIsAnchor = false;
        SlotData.bIsPartOfItem = false;
        SlotData.AllowedItemTypes = OutContainerData.AllowedItemTypes;
        
        OutContainerData.Slots.Add(SlotData);
    }
    
    // Step 5: Get all items
    TArray<FSuspenseInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Converting %d items to UI data"), AllInstances.Num());
    
    // Pre-allocate items array
    OutContainerData.Items.Reserve(AllInstances.Num());
    
    // Step 6: Process each item
    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        // Validate instance
        if (!Instance.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Skipping invalid instance"));
            continue;
        }
        
        // Check position
        if (Instance.AnchorIndex < 0 || Instance.AnchorIndex >= TotalSlots)
        {
            UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Item %s has invalid anchor %d"), 
                *Instance.ItemID.ToString(), Instance.AnchorIndex);
            continue;
        }
        
        // Convert to UI data
        FItemUIData ItemData;
        if (ConvertItemInstanceToUIData(Instance, Instance.AnchorIndex, ItemData))
        {
            OutContainerData.Items.Add(ItemData);
            
            // Mark occupied slots
            TArray<int32> OccupiedSlots = InventoryInterface->GetOccupiedSlots(
                Instance.AnchorIndex,
                FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y),
                Instance.bIsRotated
            );
            
            for (int32 SlotIdx : OccupiedSlots)
            {
                if (SlotIdx >= 0 && SlotIdx < OutContainerData.Slots.Num())
                {
                    OutContainerData.Slots[SlotIdx].bIsOccupied = true;
                    OutContainerData.Slots[SlotIdx].bIsAnchor = (SlotIdx == Instance.AnchorIndex);
                    OutContainerData.Slots[SlotIdx].bIsPartOfItem = (SlotIdx != Instance.AnchorIndex);
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Conversion complete - Grid: %dx%d, Items: %d, Weight: %.1f/%.1f"), 
        GridCols, GridRows,
        OutContainerData.Items.Num(),
        OutContainerData.CurrentWeight,
        OutContainerData.MaxWeight);
    
    return true;
}

bool USuspenseInventoryUIBridge::ConvertItemInstanceToUIData(const FSuspenseInventoryItemInstance& Instance, int32 SlotIndex, FItemUIData& OutItemData) const
{
    // Basic properties from instance
    OutItemData.ItemID = Instance.ItemID;
    OutItemData.ItemInstanceID = Instance.InstanceID;
    OutItemData.Quantity = Instance.Quantity;
    OutItemData.AnchorSlotIndex = SlotIndex;
    OutItemData.bIsRotated = Instance.bIsRotated;
    
    // Validate InstanceID
    if (!Instance.InstanceID.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] ConvertItemInstanceToUIData: Invalid InstanceID for item %s at slot %d"), 
            *Instance.ItemID.ToString(), SlotIndex);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] ConvertItemInstanceToUIData: ItemID=%s, InstanceID=%s, Slot=%d"), 
        *Instance.ItemID.ToString(), 
        *Instance.InstanceID.ToString(),
        SlotIndex);
    
    // Get unified data from ItemManager through game instance
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (USuspenseItemManager* ItemManager = GameInstance->GetSubsystem<USuspenseItemManager>())
            {
                FSuspenseUnifiedItemData UnifiedData;
                if (ItemManager->GetUnifiedItemData(Instance.ItemID, UnifiedData))
                {
                    // Display properties
                    OutItemData.DisplayName = UnifiedData.DisplayName;
                    OutItemData.Description = UnifiedData.Description;
                    OutItemData.Weight = UnifiedData.Weight;
                    OutItemData.MaxStackSize = UnifiedData.MaxStackSize;
                    
                    // Grid size
                    OutItemData.GridSize = FIntPoint(UnifiedData.GridSize.X, UnifiedData.GridSize.Y);
                    
                    // Type information
                    OutItemData.ItemType = UnifiedData.ItemType;
                    OutItemData.EquipmentSlotType = UnifiedData.EquipmentSlot;
                    
                    // Flags
                    OutItemData.bIsEquippable = UnifiedData.bIsEquippable;
                    OutItemData.bIsUsable = UnifiedData.bIsConsumable;
                    
                    // Load icon
                    if (!UnifiedData.Icon.IsNull())
                    {
                        if (UTexture2D* IconTexture = UnifiedData.Icon.LoadSynchronous())
                        {
                            OutItemData.SetIcon(IconTexture);
                        }
                    }
                    
                    // Durability info for equipment
                    if (UnifiedData.bIsEquippable && Instance.HasRuntimeProperty(TEXT("Durability")))
                    {
                        float CurrentDurability = Instance.GetRuntimeProperty(TEXT("Durability"), 100.0f);
                        float MaxDurability = Instance.GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
                        float DurabilityPercent = (MaxDurability > 0) ? (CurrentDurability / MaxDurability) : 1.0f;
                        
                        if (DurabilityPercent < 1.0f)
                        {
                            FText DurabilityText = FText::Format(
                                NSLOCTEXT("Inventory", "DurabilityFormat", "Durability: {0}%"),
                                FText::AsNumber(FMath::RoundToInt(DurabilityPercent * 100.0f))
                            );
                            
                            OutItemData.Description = FText::Format(
                                NSLOCTEXT("Inventory", "DescWithDurability", "{0}\n{1}"),
                                OutItemData.Description,
                                DurabilityText
                            );
                        }
                    }
                    
                    // Ammo info for weapons
                    if (UnifiedData.bIsWeapon && Instance.HasRuntimeProperty(TEXT("Ammo")))
                    {
                        int32 CurrentAmmo = Instance.GetCurrentAmmo();
                        int32 MaxAmmo = FMath::RoundToInt(Instance.GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
                        
                        OutItemData.bHasAmmo = true;
                        OutItemData.AmmoText = FText::Format(
                            NSLOCTEXT("Inventory", "AmmoFormat", "{0}/{1}"),
                            FText::AsNumber(CurrentAmmo), 
                            FText::AsNumber(MaxAmmo)
                        );
                    }
                    
                    return true;
                }
            }
        }
    }
    
    // Fallback - minimal data
    OutItemData.DisplayName = FText::FromName(Instance.ItemID);
    OutItemData.Weight = 1.0f;
    OutItemData.MaxStackSize = 1;
    OutItemData.GridSize = FIntPoint(1, 1);
    
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Using fallback data for item %s"), 
        *Instance.ItemID.ToString());
    
    return true;
}

// =====================================================
// Event Management
// =====================================================

void USuspenseInventoryUIBridge::SubscribeToEvents()
{
    if (!EventManager || !GameInventory.GetInterface())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Cannot subscribe - EventManager: %s, GameInventory: %s"), 
            EventManager ? TEXT("Valid") : TEXT("NULL"),
            GameInventory.GetInterface() ? TEXT("Valid") : TEXT("NULL"));
        return;
    }
    
    // Subscribe to dynamic delegates
    EventManager->OnUIContainerUpdateRequested.AddDynamic(this, &USuspenseInventoryUIBridge::OnUIRequestingUpdate);
    EventManager->OnUISlotInteraction.AddDynamic(this, &USuspenseInventoryUIBridge::OnUISlotInteraction);
    EventManager->OnUIDragStarted.AddDynamic(this, &USuspenseInventoryUIBridge::OnUIDragStarted);
    EventManager->OnUIDragCompleted.AddDynamic(this, &USuspenseInventoryUIBridge::OnUIDragCompleted);
    EventManager->OnUIItemDropped.AddDynamic(this, &USuspenseInventoryUIBridge::OnUIItemDropped);
    
    // КРИТИЧЕСКОЕ ДОБАВЛЕНИЕ: Подписка на завершение операций с экипировкой
    EventManager->OnEquipmentOperationCompleted.AddDynamic(this, &USuspenseInventoryUIBridge::OnEquipmentOperationCompleted);
    
    // Subscribe to inventory refresh requests
    InventoryRefreshHandle = EventManager->OnInventoryUIRefreshRequestedNative.AddLambda(
        [this](const FGameplayTag& ContainerTag)
        {
            if (ContainerTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"))))
            {
                UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Received inventory refresh request"));
                ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.RefreshRequested")));
            }
        }
    );
    
    // Subscribe to native delegate for performance-critical drop events
    ItemDroppedNativeHandle = EventManager->OnUIItemDroppedNative.AddLambda(
        [this](UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot)
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Native delegate OnUIItemDroppedNative fired!"));
            OnUIItemDropped(ContainerWidget, DragData, TargetSlot);
        }
    );
    
    // Subscribe to inventory item moved event
    if (!EventManager->OnInventoryItemMoved.IsBound())
    {
        EventManager->OnInventoryItemMoved.AddDynamic(this, &USuspenseInventoryUIBridge::OnInventoryItemMoved);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Subscribed to all events successfully"));
}

void USuspenseInventoryUIBridge::UnsubscribeFromEvents()
{
    if (EventManager)
    {
        // Unsubscribe from dynamic delegates
        EventManager->OnUIContainerUpdateRequested.RemoveDynamic(this, &USuspenseInventoryUIBridge::OnUIRequestingUpdate);
        EventManager->OnUISlotInteraction.RemoveDynamic(this, &USuspenseInventoryUIBridge::OnUISlotInteraction);
        EventManager->OnUIDragStarted.RemoveDynamic(this, &USuspenseInventoryUIBridge::OnUIDragStarted);
        EventManager->OnUIDragCompleted.RemoveDynamic(this, &USuspenseInventoryUIBridge::OnUIDragCompleted);
        EventManager->OnUIItemDropped.RemoveDynamic(this, &USuspenseInventoryUIBridge::OnUIItemDropped);
        EventManager->OnInventoryItemMoved.RemoveDynamic(this, &USuspenseInventoryUIBridge::OnInventoryItemMoved);
        
        // ВАЖНО: Отписываемся от событий экипировки
        EventManager->OnEquipmentOperationCompleted.RemoveDynamic(this, &USuspenseInventoryUIBridge::OnEquipmentOperationCompleted);
        
        // Remove native delegate handles
        if (ItemDroppedNativeHandle.IsValid())
        {
            EventManager->OnUIItemDroppedNative.Remove(ItemDroppedNativeHandle);
            ItemDroppedNativeHandle.Reset();
        }
        
        if (InventoryRefreshHandle.IsValid())
        {
            EventManager->OnInventoryUIRefreshRequestedNative.Remove(InventoryRefreshHandle);
            InventoryRefreshHandle.Reset();
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Unsubscribed from events"));
}

// =====================================================
// Event Handlers
// =====================================================

void USuspenseInventoryUIBridge::OnGameInventoryUpdated()
{
    // Schedule UI update instead of immediate refresh
    ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.InventoryChanged")));
    
    // Notify about inventory update
    if (EventManager)
    {
        FGameplayTag UpdatedTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.Updated"));
        EventManager->NotifyUIEvent(this, UpdatedTag, TEXT(""));
    }
}

void USuspenseInventoryUIBridge::OnUIRequestingUpdate(UUserWidget* Widget, const FGameplayTag& ContainerType)
{
    if (ContainerType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"))))
    {
        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ContainerRequested")));
    }
}

void USuspenseInventoryUIBridge::OnUISlotInteraction(UUserWidget* Widget, int32 SlotIndex, const FGameplayTag& InteractionType)
{
    // Check if this is our inventory widget
    bool bIsOurWidget = false;
    
    // Check if widget is from Character Screen
    if (USuspenseInventoryWidget* InventoryWidget = GetCachedInventoryWidget())
    {
        bIsOurWidget = (Widget == InventoryWidget);
    }
    
    if (!bIsOurWidget)
    {
        return;
    }
    
    // Handle drop interaction
    if (InteractionType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.Drop"))))
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Drop interaction detected at slot %d"), SlotIndex);
        
        // After successful drop, schedule refresh
        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.DropCompleted")));
    }
    else if (InteractionType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.RotateItem"))))
    {
        ProcessItemRotationRequest(SlotIndex);
    }
    else if (InteractionType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.RequestSort"))))
    {
        ProcessSortRequest();
    }
    else if (InteractionType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Interaction.DoubleClick"))))
    {
        if (USuspenseInventoryWidget* InvWidget = Cast<USuspenseInventoryWidget>(Widget))
        {
            FGuid ItemInstanceID;
            FContainerUIData CurrentData = InvWidget->GetCurrentContainerData();
            for (const FItemUIData& Item : CurrentData.Items)
            {
                if (Item.AnchorSlotIndex == SlotIndex)
                {
                    ItemInstanceID = Item.ItemInstanceID;
                    break;
                }
            }
            
            ProcessItemDoubleClick(SlotIndex, ItemInstanceID);
        }
    }
}

void USuspenseInventoryUIBridge::OnUIDragStarted(UUserWidget* SourceWidget, const FDragDropUIData& DragData)
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Drag started: Item %s from slot %d"), 
        *DragData.ItemData.ItemID.ToString(), DragData.SourceSlotIndex);
    
    // Store current drag data for later use
    CurrentDragData = DragData;
}

void USuspenseInventoryUIBridge::OnUIDragCompleted(UUserWidget* SourceWidget, UUserWidget* TargetWidget, bool bSuccess)
{
    if (!bSuccess)
    {
        CurrentDragData = FDragDropUIData(); // Clear drag data
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Drag operation was cancelled"));
        return;
    }
    
    // This method is now mainly for logging and cleanup
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Drag operation completed successfully"));
    
    // Clear stored drag data
    CurrentDragData = FDragDropUIData();
    
    // The actual drop handling happens in OnUIItemDropped
}

void USuspenseInventoryUIBridge::OnUIItemDropped(UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot)
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === OnUIItemDropped START ==="));
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Widget: %s, Source: %s, Target slot: %d"),
        ContainerWidget ? *ContainerWidget->GetClass()->GetName() : TEXT("NULL"),
        *DragData.SourceContainerType.ToString(),
        TargetSlot);

    // 1) Валидация
    if (!DragData.IsValidDragData())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Invalid drag data"));
        HandleInvalidDrop(ContainerWidget, DragData, TargetSlot);
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === OnUIItemDropped END ==="));
        return;
    }
    if (!GameInventory.GetInterface())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No inventory interface connected"));
        HandleInvalidDrop(ContainerWidget, DragData, TargetSlot);
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === OnUIItemDropped END ==="));
        return;
    }

    // 2) Источник
    const bool bSourceIsInventory = DragData.SourceContainerType.MatchesTag(
        FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"), /*bErrorIfNotFound*/false));

    // 3) Цель
    bool bTargetIsInventory = false;
    bool bTargetIsEquipment = false;
    FGameplayTag TargetContainerType;

    if (ContainerWidget && ContainerWidget->GetClass()->ImplementsInterface(USuspenseContainerUIInterface::StaticClass()))
    {
        TargetContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(ContainerWidget);

        bTargetIsInventory = TargetContainerType.MatchesTag(
            FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"), false));

        bTargetIsEquipment = TargetContainerType.MatchesTag(
            FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"), false));

        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Container type via interface: %s"), *TargetContainerType.ToString());
    }

    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Drop operation - Source: %s, Target: %s"),
        bSourceIsInventory ? TEXT("Inventory") : TEXT("External"),
        bTargetIsInventory ? TEXT("Inventory") : (bTargetIsEquipment ? TEXT("Equipment") : TEXT("Unknown")));

    // 4) Маршрутизация
    if (bSourceIsInventory && bTargetIsInventory)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Processing inventory → inventory drop"));
        HandleInventoryToInventoryDrop(DragData, TargetSlot);
    }
    else if (!bSourceIsInventory && bTargetIsInventory)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Processing external → inventory drop"));
        HandleExternalToInventoryDrop(ContainerWidget, DragData, TargetSlot);
    }
    else if (bSourceIsInventory && bTargetIsEquipment)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Processing inventory → equipment drop - delegating to EquipmentUIBridge"));
        HandleInventoryToEquipmentDrop(ContainerWidget, DragData, TargetSlot);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Unsupported drop route"));
        HandleInvalidDrop(ContainerWidget, DragData, TargetSlot);
    }

    // 5) Немедленный мягкий UI-рефреш
    if (UWorld* World = GetWorld())
    {
        const FGameplayTag EquipTag = FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"), false);

        World->GetTimerManager().SetTimerForNextTick([this, TargetContainerType, EquipTag]()
        {
            // *** ВАЖНО: НЕ вызывать event-интерфейсы напрямую! Только Execute_***

            // Инвентарь (через интерфейс виджета)
            if (UUserWidget* InvWidget = GetCachedInventoryWidget())
            {
                if (InvWidget->GetClass()->ImplementsInterface(USuspenseInventoryUIBridgeWidgetInterface::StaticClass()))
                {
                    ISuspenseInventoryUIBridgeWidget::Execute_RefreshInventoryUI(InvWidget);
                }
            }

            // Экипировка — попросим через делегат менеджера (без кросс-зависимостей)
            if (TargetContainerType.IsValid() && TargetContainerType.MatchesTag(EquipTag))
            {
                if (EventManager)
                {
                    EventManager->NotifyEquipmentUIRefreshRequested(nullptr);
                }
            }

            UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Immediate UI refresh completed"));
        });
    }

    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === OnUIItemDropped END ==="));
}


void USuspenseInventoryUIBridge::HandleInvalidDrop(UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot)
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInvalidDrop START ==="));

    // Лог причины
    const bool bHasInstance = DragData.ItemData.ItemInstanceID.IsValid();
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Invalid drop: ItemID=%s, InstanceID=%s, TargetSlot=%d"),
        *DragData.ItemData.ItemID.ToString(),
        bHasInstance ? *DragData.ItemData.ItemInstanceID.ToString() : TEXT("INVALID"),
        TargetSlot);

    // Сообщение пользователю
    if (EventManager)
    {
        const FString Reason = !bHasInstance ? TEXT("Invalid item identifier") : TEXT("Unsupported drop target");
        EventManager->NotifyUI(Reason, 2.0f);
    }

    // Безопасные обновления UI (теги без ensure)
    ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.InvalidDrop"), false));

    // Если был экран персонажа — попросим общий рефреш лейаута
    if (EventManager)
    {
        const FGameplayTag LayoutRefresh = FGameplayTag::RequestGameplayTag(TEXT("UI.Layout.RefreshAll"), false);
        EventManager->NotifyUIEvent(this, LayoutRefresh, TEXT("InvalidDrop"));
    }

    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInvalidDrop END ==="));
}

void USuspenseInventoryUIBridge::OnEquipmentOperationCompleted(const FEquipmentOperationResult& Result)
{
    // Попробуем восстановить тип операции из метаданных
    EEquipmentOperationType OpType = EEquipmentOperationType::None;
    FString OpTypeStr;
    if (Result.ResultMetadata.Contains(TEXT("OperationType")))
    {
        OpTypeStr = Result.ResultMetadata[TEXT("OperationType")];

        // Парсинг имени enum: "Equip", "Unequip", "Swap" и т.д.
        if (UEnum* Enum = StaticEnum<EEquipmentOperationType>())
        {
            const int64 Value = Enum->GetValueByNameString(OpTypeStr);
            if (Value != INDEX_NONE)
            {
                OpType = static_cast<EEquipmentOperationType>(Value);
            }
        }
    }

    // Готовим удобное имя для логов (если не удалось распарсить — используем сырую строку/плейсхолдер)
    FString OpTypeForLog;
    if (UEnum* Enum = StaticEnum<EEquipmentOperationType>())
    {
        OpTypeForLog = (OpType != EEquipmentOperationType::None)
            ? Enum->GetNameStringByValue(static_cast<int64>(OpType))
            : (!OpTypeStr.IsEmpty() ? OpTypeStr : TEXT("Unknown"));
    }
    else
    {
        OpTypeForLog = !OpTypeStr.IsEmpty() ? OpTypeStr : TEXT("Unknown");
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[InventoryUIBridge] Equipment operation completed - Type: %s, Success: %s"),
        *OpTypeForLog,
        Result.bSuccess ? TEXT("Yes") : TEXT("No"));

    // Решаем, надо ли обновлять инвентарь
    bool bNeedsInventoryRefresh = false;

    // Если сервер положил флаг в метаданные — используем его как главный сигнал
    if (const FString* AffectsPtr = Result.ResultMetadata.Find(TEXT("AffectsInventory")))
    {
        // ожидаем "true"/"false"
        bNeedsInventoryRefresh = Result.bSuccess && (*AffectsPtr).Equals(TEXT("true"), ESearchCase::IgnoreCase);
    }
    else
    {
        // Иначе — эвристика по типу операции
        switch (OpType)
        {
            case EEquipmentOperationType::Equip:      // предмет ушёл из инвентаря
            case EEquipmentOperationType::Unequip:    // предмет вернулся в инвентарь
            case EEquipmentOperationType::Swap:       // перестановка почти наверняка затрагивает инвентарь
            case EEquipmentOperationType::Move:
            case EEquipmentOperationType::Drop:
            case EEquipmentOperationType::Transfer:
                bNeedsInventoryRefresh = Result.bSuccess;
                break;
            default:
                // По умолчанию — не обновляем
                bNeedsInventoryRefresh = false;
                break;
        }

        // Фоллбэк: если тип неизвестен, но операция прошла — можно перестраховаться и обновить
        if (OpType == EEquipmentOperationType::None && Result.bSuccess)
        {
            bNeedsInventoryRefresh = true;
        }
    }

    if (bNeedsInventoryRefresh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Refreshing inventory after equipment operation"));

        // Немедленное обновление
        RefreshInventoryUI_Implementation();

        // Дополнительное обновление с небольшой задержкой
        if (UWorld* World = GetWorld())
        {
            FTimerHandle DelayedRefreshHandle;
            World->GetTimerManager().SetTimer(
                DelayedRefreshHandle,
                [this]()
                {
                    RefreshInventoryUI_Implementation();
                    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Delayed refresh after equipment operation"));
                },
                0.1f,
                false
            );
        }
    }
}

void USuspenseInventoryUIBridge::OnInventoryItemMoved(const FGuid& ItemID, int32 FromSlot, int32 ToSlot, bool bSuccess)
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Item moved - ID: %s, From: %d, To: %d, Success: %s"),
        *ItemID.ToString(), FromSlot, ToSlot, bSuccess ? TEXT("Yes") : TEXT("No"));
    
    if (bSuccess)
    {
        // Refresh all widgets in active layout
        RefreshAllWidgetsInActiveLayout();
        
        // Schedule additional update with delay
        if (UWorld* World = GetWorld())
        {
            FTimerHandle DelayedRefreshHandle;
            World->GetTimerManager().SetTimer(DelayedRefreshHandle, [this]()
            {
                RefreshAllWidgetsInActiveLayout();
                UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Delayed universal refresh completed"));
            }, 0.15f, false);
        }
    }
}

void USuspenseInventoryUIBridge::OnInventoryUIClosed()
{
    ISuspenseInventoryUIBridgeWidget::Execute_HideCharacterScreen(this);
    
    if (EventManager)
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.CharacterScreen.Closed"));
        EventManager->NotifyUIEvent(this, EventTag, TEXT(""));
    }
}

// =====================================================
// UI Update Management
// =====================================================

void USuspenseInventoryUIBridge::ScheduleUIUpdate(const FGameplayTag& UpdateType)
{
    // Add update type to pending updates
    PendingUIUpdates.AddUnique(UpdateType);
    
    // Schedule batch update if not already scheduled
    if (UWorld* World = GetWorld())
    {
        if (!BatchedUpdateTimerHandle.IsValid())
        {
            World->GetTimerManager().SetTimer(
                BatchedUpdateTimerHandle,
                this,
                &USuspenseInventoryUIBridge::ProcessBatchedUIUpdates,
                UPDATE_BATCH_DELAY,
                false
            );
        }
    }
}

void USuspenseInventoryUIBridge::ProcessBatchedUIUpdates()
{
    if (PendingUIUpdates.Num() == 0)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Processing %d batched UI updates"), PendingUIUpdates.Num());
    
    // Determine if we need a full refresh
    bool bNeedsFullRefresh = false;
    
    for (const FGameplayTag& UpdateTag : PendingUIUpdates)
    {
        // Check for updates that require full refresh
        if (UpdateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.Full"))) ||
            UpdateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ScreenOpened"))) ||
            UpdateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemRemoved"))) ||
            UpdateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemAdded"))))
        {
            bNeedsFullRefresh = true;
            break;
        }
    }
    
    // Clear pending updates
    PendingUIUpdates.Empty();
    
    // Perform appropriate update
    if (bNeedsFullRefresh)
    {
        RefreshInventoryUI_Implementation();
    }
    else
    {
        // For lighter updates, just notify widgets
        if (EventManager)
        {
            FGameplayTag UpdateTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Update.Light"));
            EventManager->NotifyUIEvent(this, UpdateTag, TEXT(""));
        }
    }
}

void USuspenseInventoryUIBridge::RefreshAllWidgetsInActiveLayout()
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] === Refreshing ALL widgets in active layout ==="));
    
    // Refresh inventory
    RefreshInventoryUI_Implementation();
    
    // Request equipment update through events
    if (EventManager)
    {
        FGameplayTag EquipmentTag = FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"));
        EventManager->NotifyUIContainerUpdateRequested(nullptr, EquipmentTag);
        
        // Universal layout refresh event
        FGameplayTag LayoutUpdateTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Layout.RefreshAll"));
        EventManager->NotifyUIEvent(this, LayoutUpdateTag, TEXT("ItemTransfer"));
    }
    
    // Use UIManager directly
    if (UIManager)
        {
       FGameplayTag CharacterScreenTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Character"));
       if (UUserWidget* CharacterScreen = UIManager->GetWidget(CharacterScreenTag))
       {
           TArray<UWidget*> AllWidgets;
           CharacterScreen->WidgetTree->GetAllWidgets(AllWidgets);
           
           for (UWidget* Widget : AllWidgets)
           {
               if (USuspenseUpperTabBar* TabBar = Cast<USuspenseUpperTabBar>(Widget))
               {
                   // Refresh active tab content
                   TabBar->RefreshActiveTabContent();
                   break;
               }
           }
       }
   }
}

void USuspenseInventoryUIBridge::ForceFullInventoryRefresh()
{
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === FORCE FULL REFRESH START ==="));
   
   if (!ValidateInventoryConnection())
   {
       return;
   }
   
   // Find widget
   USuspenseInventoryWidget* InventoryWidget = GetCachedInventoryWidget();
   if (!InventoryWidget)
   {
       return;
   }
   
   // Fully reinitialize widget
   if (InventoryWidget->GetClass()->ImplementsInterface(USuspenseUIWidgetInterface::StaticClass()))
   {
       // Uninitialize
       ISuspenseUIWidgetInterface::Execute_UninitializeWidget(InventoryWidget);
   }
   
   // Delay for cleanup completion
   if (UWorld* World = GetWorld())
   {
       FTimerHandle ReinitHandle;
       World->GetTimerManager().SetTimer(ReinitHandle, [this, InventoryWidget]()
       {
           // Convert fresh data
           FContainerUIData ContainerData;
           if (ConvertInventoryToUIData(ContainerData))
           {
               // Reinitialize
               ISuspenseContainerUIInterface::Execute_InitializeContainer(InventoryWidget, ContainerData);
               
               UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Full refresh completed"));
           }
       }, 0.1f, false);
   }
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === FORCE FULL REFRESH END ==="));
}

// =====================================================
// Drop Operation Handlers
// =====================================================

void USuspenseInventoryUIBridge::HandleInventoryToInventoryDrop(const FDragDropUIData& DragData, int32 TargetSlot)
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInventoryToInventoryDrop START ==="));
    
    // Use centralized drop processing method
    FInventoryOperationResult Result = ProcessInventoryDrop(
        DragData,
        FSlateApplication::Get().GetCursorPos(),
        nullptr
    );
    
    if (Result.IsSuccess())
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Drop operation succeeded"));
        
        // КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: Немедленное обновление UI
        RefreshInventoryUI_Implementation();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Drop operation failed: %s"), 
            *Result.ErrorMessage.ToString());
        
        // При ошибке тоже обновляем UI для сброса визуального состояния
        RefreshInventoryUI_Implementation();
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInventoryToInventoryDrop END ==="));
}

void USuspenseInventoryUIBridge::HandleExternalToInventoryDrop(
    UUserWidget* ContainerWidget, 
    const FDragDropUIData& DragData, 
    int32 TargetSlot)
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] === HandleExternalToInventoryDrop START ==="));

    // Для предметов из экипировки отправляем Unequip в конкретный слот инвентаря
    if (DragData.SourceContainerType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"))))
    {
        // Валидация целевого слота
        if (TargetSlot == INDEX_NONE)
        {
            UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Invalid target slot for equipment drop"));

            if (EventManager)
            {
                EventManager->NotifyUI(TEXT("Drop the item over a valid inventory slot"), 2.0f);
            }
            return;
        }

        // Проверим возможность размещения именно в этот слот (если подключение к инвентарю валидно)
        if (ValidateInventoryConnection())
        {
            if (ISuspenseInventoryInterface* InventoryInterface = GameInventory.GetInterface())
            {
                // Размер предмета (учтите поворот выше по стеку, если нужно)
                const FVector2D ItemSize(DragData.ItemData.GridSize.X, DragData.ItemData.GridSize.Y);

                // Простая проверка размещения без свопа
                if (!InventoryInterface->CanPlaceItemAtSlot(ItemSize, TargetSlot, /*bAllowSwap*/ false))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Cannot place equipment item at slot %d"), TargetSlot);

                    if (EventManager)
                    {
                        EventManager->NotifyUI(TEXT("Cannot place item at this location"), 2.0f);
                    }
                    return;
                }
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Item from equipment slot %d, requesting unequip to inventory slot %d"),
            DragData.SourceSlotIndex, TargetSlot);

        if (!EventManager)
        {
            UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No event manager available"));
            return;
        }

        // Формируем запрос Unequip под текущий формат FEquipmentOperationRequest
        FEquipmentOperationRequest Request;
        Request.OperationType   = EEquipmentOperationType::Unequip;
        Request.SourceSlotIndex = DragData.SourceSlotIndex;   // слот экипировки, откуда снимаем
        Request.TargetSlotIndex = TargetSlot;                 // целевой слот инвентаря
        Request.Timestamp       = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        // Доп. данные — через Parameters
        Request.Parameters.Add(TEXT("ItemID"),          DragData.ItemData.ItemID.ToString());
        Request.Parameters.Add(TEXT("ItemInstanceID"),  DragData.ItemData.ItemInstanceID.ToString());
        Request.Parameters.Add(TEXT("Quantity"),        FString::FromInt(DragData.ItemData.Quantity));
        Request.Parameters.Add(TEXT("SourceContainer"), DragData.SourceContainerType.ToString());

        // Отправляем событие — компонент экипировки выполнит снятие и добавление в инвентарь
        EventManager->BroadcastEquipmentOperationRequest(Request);

        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Sent unequip request via EventManager to exact slot %d"), TargetSlot);

        // UI не обновляем здесь — ждём завершения операции в OnEquipmentOperationCompleted
    }

    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] === HandleExternalToInventoryDrop END ==="));
}

void USuspenseInventoryUIBridge::HandleInventoryToEquipmentDrop(
    UUserWidget* ContainerWidget,
    const FDragDropUIData& DragData,
    int32 TargetSlot)
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInventoryToEquipmentDrop START ==="));
    UE_LOG(LogTemp, Warning, TEXT("  Item: %s (InstanceID: %s)"),
        *DragData.ItemData.ItemID.ToString(),
        *DragData.ItemData.ItemInstanceID.ToString());
    UE_LOG(LogTemp, Warning, TEXT("  Target Equipment Slot: %d"), TargetSlot);

    if (!UIManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No UIManager available for equipment bridge access"));
        if (EventManager) { EventManager->NotifyUI(TEXT("UI Manager not available"), 3.0f); }
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInventoryToEquipmentDrop END ==="));
        return;
    }

    USuspenseEquipmentUIBridge* EquipmentBridge = UIManager->GetEquipmentUIBridge();
    if (!EquipmentBridge)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] EquipmentUIBridge not found in UIManager"));
        if (EventManager) { EventManager->NotifyUI(TEXT("Equipment system not initialized"), 3.0f); }
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInventoryToEquipmentDrop END ==="));
        return;
    }

    if (!DragData.ItemData.ItemInstanceID.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Invalid InstanceID in drag data"));
        if (EventManager) { EventManager->NotifyUI(TEXT("Invalid item identifier"), 3.0f); }
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInventoryToEquipmentDrop END ==="));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Calling EquipmentUIBridge->ProcessEquipmentDrop"));
    UE_LOG(LogTemp, Warning, TEXT("  Parameters: SlotIndex=%d, ItemID=%s, InstanceID=%s, Quantity=%d"),
        TargetSlot,
        *DragData.ItemData.ItemID.ToString(),
        *DragData.ItemData.ItemInstanceID.ToString(),
        DragData.ItemData.Quantity);

    // === PATCH: Remove immediate SUCCESS notification, wait for OnEquipmentOperationCompleted ===
    const bool bEquipRequested = EquipmentBridge->ProcessEquipmentDrop_Implementation(TargetSlot, DragData);

    if (bEquipRequested)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Request sent to Equipment bridge"));
        // No SUCCESS/Equipped notification here. Wait for OnEquipmentOperationCompleted(...)
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] FAILED: Equipment drop request not accepted"));
        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.EquipmentOperationFailed"), false));
    }

    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === HandleInventoryToEquipmentDrop END ==="));
}


// =====================================================
// Drag & Drop Operations
// =====================================================

FInventoryOperationResult USuspenseInventoryUIBridge::ProcessInventoryDrop(
    const FDragDropUIData& DragData,
    const FVector2D& ScreenPosition,
    UUserWidget* TargetWidget)
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] === ProcessInventoryDrop START ==="));

    // Немного диагностики
    DiagnoseDropPosition(ScreenPosition);

    // 1) Базовая валидация
    if (!DragData.IsValidDragData())
    {
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid drag data")),
            TEXT("ProcessInventoryDrop"),
            nullptr
        );
    }

    // 2) Проверка подключения к инвентарю
    if (!ValidateInventoryConnection())
    {
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("No inventory connection")),
            TEXT("ProcessInventoryDrop"),
            nullptr
        );
    }

    // 3) Определяем: внутреннее перемещение по инвентарю или внешнее (экипировка и т.п.)
    const bool bIsInternalMove = DragData.SourceContainerType.MatchesTag(
        FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"))
    );

    // 4) Вычисляем целевой слот по позиции курсора
    int32 TargetSlot = CalculateDropTargetSlot(
        ScreenPosition,
        DragData.DragOffset,
        DragData.ItemData.GridSize,
        DragData.ItemData.bIsRotated
    );

    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Calculated target slot: %d"), TargetSlot);

    // 5) Внешние источники (в частности — экипировка)
    if (!bIsInternalMove)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] External drop from %s to inventory"),
            *DragData.SourceContainerType.ToString());

        if (DragData.SourceContainerType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"))))
        {
            if (ISuspenseInventoryInterface* InventoryInterface = GameInventory.GetInterface())
            {
                // Учитываем поворот при определении занимаемого размера, если логика интерфейса это требует
                const FVector2D ItemSizeVec(DragData.ItemData.GridSize.X, DragData.ItemData.GridSize.Y);

                if (TargetSlot == INDEX_NONE)
                {
                    UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No valid slot under cursor"));

                    DiagnoseInventoryState();

                    return FInventoryOperationResult::Failure(
                        EInventoryErrorCode::InvalidSlot,
                        FText::FromString(TEXT("Drop the item over a valid inventory slot")),
                        TEXT("ProcessInventoryDrop"),
                        nullptr
                    );
                }

                // Проверяем возможность поставить именно в TargetSlot (без свопа)
                const bool bCanPlace = InventoryInterface->CanPlaceItemAtSlot(
                    ItemSizeVec,
                    TargetSlot,
                    /*bAllowSwap*/ false
                );

                if (!bCanPlace)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Cannot place item at slot %d"), TargetSlot);

                    // Для диагностики — кто там лежит
                    FSuspenseInventoryItemInstance ExistingItem;
                    if (InventoryInterface->GetItemInstanceAtSlot(TargetSlot, ExistingItem))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Slot %d occupied by item %s"),
                            TargetSlot, *ExistingItem.ItemID.ToString());
                    }

                    return FInventoryOperationResult::Failure(
                        EInventoryErrorCode::SlotOccupied,
                        FText::FromString(TEXT("Cannot place item at this location")),
                        TEXT("ProcessInventoryDrop"),
                        nullptr
                    );
                }
            }

            // Готовим запрос Unequip под текущую структуру
            FEquipmentOperationRequest Request;
            Request.OperationType   = EEquipmentOperationType::Unequip;
            Request.SourceSlotIndex = DragData.SourceSlotIndex;
            Request.TargetSlotIndex = TargetSlot; // конкретный слот под курсором
            Request.Timestamp       = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

            // Вспомогательные данные — через Parameters
            Request.Parameters.Add(TEXT("ItemID"),          DragData.ItemData.ItemID.ToString());
            Request.Parameters.Add(TEXT("ItemInstanceID"),  DragData.ItemData.ItemInstanceID.ToString());
            Request.Parameters.Add(TEXT("Quantity"),        FString::FromInt(DragData.ItemData.Quantity));
            Request.Parameters.Add(TEXT("SourceContainer"), DragData.SourceContainerType.ToString());

            UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Sending unequip request to exact slot %d"),
                Request.TargetSlotIndex);

            if (EventManager)
            {
                EventManager->BroadcastEquipmentOperationRequest(Request);

                // Запланируем обновление UI после операции (конкретный тег не критичен — используется вашей подсистемой)
                ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.EquipmentTransfer")));

                return FInventoryOperationResult::Success(TEXT("ProcessInventoryDrop"));
            }
            else
            {
                return FInventoryOperationResult::Failure(
                    EInventoryErrorCode::UnknownError,
                    FText::FromString(TEXT("No event manager available")),
                    TEXT("ProcessInventoryDrop"),
                    nullptr
                );
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Unsupported external source: %s"),
                *DragData.SourceContainerType.ToString());

            return FInventoryOperationResult::Failure(
                EInventoryErrorCode::UnknownError,
                FText::FromString(TEXT("Unsupported drag source")),
                TEXT("ProcessInventoryDrop"),
                nullptr
            );
        }
    }

    // 6) Внутреннее перемещение по инвентарю
    UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Internal inventory move from slot %d to %d"),
        DragData.SourceSlotIndex, TargetSlot);

    // Валидация размещения
    TArray<int32> OccupiedSlots;
    const bool bCanPlaceInternal = ValidateDropPlacement(
        TargetSlot,
        DragData.ItemData.GridSize,
        DragData.ItemData.bIsRotated,
        OccupiedSlots
    );

    int32 FinalTargetSlot = TargetSlot;

    if (!bCanPlaceInternal)
    {
        // Пытаемся найти ближайший подходящий слот
        const int32 StartSlotForSearch = (TargetSlot != INDEX_NONE) ? TargetSlot : 0;
        const int32 AlternativeSlot = FindNearestValidSlot(
            StartSlotForSearch,
            DragData.ItemData.GridSize,
            DragData.ItemData.bIsRotated
        );

        if (AlternativeSlot != INDEX_NONE)
        {
            FinalTargetSlot = AlternativeSlot;
            UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Using alternative slot: %d"), FinalTargetSlot);
        }
        else
        {
            ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.InvalidDrop")));
            return FInventoryOperationResult::NoSpace(TEXT("ProcessInventoryDrop"));
        }
    }

    // Выполняем перемещение через интерфейс инвентаря
    const bool bSuccess = ISuspenseInventoryInterface::Execute_MoveItemBySlots(
        GameInventory.GetObject(),
        DragData.SourceSlotIndex,
        FinalTargetSlot,
        /*bMaintainRotation*/ true
    );

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Move operation successful"));

        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemMoved")));
        return FInventoryOperationResult::Success(TEXT("ProcessInventoryDrop"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Move operation failed"));

        ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.MoveFailed")));
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Move operation failed")),
            TEXT("ProcessInventoryDrop"),
            nullptr
        );
    }
}

int32 USuspenseInventoryUIBridge::CalculateDropTargetSlot(
   const FVector2D& ScreenPosition,
   const FVector2D& DragOffset,
   const FIntPoint& ItemSize,
   bool bIsRotated) const
{
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === CalculateDropTargetSlot START ==="));
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Screen position: %s"), *ScreenPosition.ToString());
   
   // Get grid parameters
   int32 GridColumns, GridRows;
   float CellSize;
   FGeometry GridGeometry;
   
   if (!GetInventoryGridParams(GridColumns, GridRows, CellSize, GridGeometry))
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Failed to get grid parameters"));
       return INDEX_NONE;
   }
   
   // Transform screen position to local grid coordinates
   FVector2D LocalCursorPos = GridGeometry.AbsoluteToLocal(ScreenPosition);
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Local cursor position in grid: %s"), 
       *LocalCursorPos.ToString());
   
   // Check if cursor is within grid bounds
   FVector2D GridSize = GridGeometry.GetLocalSize();
   if (LocalCursorPos.X < 0 || LocalCursorPos.Y < 0 || 
       LocalCursorPos.X > GridSize.X || LocalCursorPos.Y > GridSize.Y)
   {
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Cursor outside grid bounds. LocalPos: %s, GridSize: %s"), 
           *LocalCursorPos.ToString(), *GridSize.ToString());
       
       // Try to clamp position
       LocalCursorPos.X = FMath::Clamp(LocalCursorPos.X, 0.0f, GridSize.X);
       LocalCursorPos.Y = FMath::Clamp(LocalCursorPos.Y, 0.0f, GridSize.Y);
   }
   
   // Calculate effective size with rotation
   FIntPoint EffectiveSize = bIsRotated ? 
       FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;
   
   // Adjust for drag offset
   FVector2D AdjustedLocalPos = LocalCursorPos;
   
   // For items larger than 1x1, account for drag offset
   if (EffectiveSize.X > 1 || EffectiveSize.Y > 1)
   {
       // Calculate offset in pixels
       FVector2D ItemPixelSize(EffectiveSize.X * CellSize, EffectiveSize.Y * CellSize);
       FVector2D PixelOffset = ItemPixelSize * DragOffset;
       
       // Adjust position based on offset
       AdjustedLocalPos = LocalCursorPos - PixelOffset;
       
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Multi-cell item adjustment - Offset: %s, Adjusted: %s"), 
           *PixelOffset.ToString(), *AdjustedLocalPos.ToString());
   }
   
   // Calculate grid coordinates
   int32 GridX = FMath::FloorToInt(AdjustedLocalPos.X / CellSize);
   int32 GridY = FMath::FloorToInt(AdjustedLocalPos.Y / CellSize);
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Initial grid coords: (%d, %d)"), GridX, GridY);
   
   // Check and adjust bounds considering item size
   if (GridX < 0 || GridY < 0 || 
       GridX + EffectiveSize.X > GridColumns || 
       GridY + EffectiveSize.Y > GridRows)
   {
       // Adjust position so item fits in grid
       GridX = FMath::Clamp(GridX, 0, GridColumns - EffectiveSize.X);
       GridY = FMath::Clamp(GridY, 0, GridRows - EffectiveSize.Y);
       
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Adjusted grid coords to fit: (%d, %d)"), GridX, GridY);
       
       // If still invalid after adjustment
       if (GridX < 0 || GridY < 0)
       {
           UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Item doesn't fit in grid"));
           return INDEX_NONE;
       }
   }
   
   // Convert to linear index
   int32 TargetSlot = GridY * GridColumns + GridX;
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === Drop calculation result ==="));
   UE_LOG(LogTemp, Warning, TEXT("  - Screen pos: %s"), *ScreenPosition.ToString());
   UE_LOG(LogTemp, Warning, TEXT("  - Local cursor: %s"), *LocalCursorPos.ToString());
   UE_LOG(LogTemp, Warning, TEXT("  - Cell size: %.1f"), CellSize);
   UE_LOG(LogTemp, Warning, TEXT("  - Item size: %dx%d%s"), 
       EffectiveSize.X, EffectiveSize.Y, bIsRotated ? TEXT(" (rotated)") : TEXT(""));
   UE_LOG(LogTemp, Warning, TEXT("  - Grid position: (%d, %d) -> Slot %d"), GridX, GridY, TargetSlot);
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === CalculateDropTargetSlot END ==="));
   
   return TargetSlot;
}

bool USuspenseInventoryUIBridge::ValidateDropPlacement(
   int32 TargetSlot,
   const FIntPoint& ItemSize,
   bool bIsRotated,
   TArray<int32>& OutOccupiedSlots) const
{
   OutOccupiedSlots.Empty();
   
   if (!ValidateInventoryConnection())
   {
       return false;
   }
   
   // Get inventory interface
   ISuspenseInventoryInterface* Inventory = GameInventory.GetInterface();
   if (!Inventory)
   {
       return false;
   }
   
   // Get grid dimensions
   FVector2D GridSizeVec = Inventory->GetInventorySize();
   int32 GridColumns = FMath::RoundToInt(GridSizeVec.X);
   int32 GridRows = FMath::RoundToInt(GridSizeVec.Y);
   
   // Check target slot validity
   if (TargetSlot < 0 || TargetSlot >= GridColumns * GridRows)
   {
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Invalid target slot: %d"), TargetSlot);
       return false;
   }
   
   // Account for rotation
   FIntPoint EffectiveSize = bIsRotated ? 
       FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;
   
   // Get target slot coordinates
   int32 StartX = TargetSlot % GridColumns;
   int32 StartY = TargetSlot / GridColumns;
   
   // Check grid bounds
   if (StartX + EffectiveSize.X > GridColumns || 
       StartY + EffectiveSize.Y > GridRows)
   {
       UE_LOG(LogTemp, Verbose, TEXT("[InventoryUIBridge] Item doesn't fit at (%d,%d) - size %dx%d exceeds grid"), 
           StartX, StartY, EffectiveSize.X, EffectiveSize.Y);
       return false;
   }
   
   // Collect all slots that would be occupied
   for (int32 Y = 0; Y < EffectiveSize.Y; Y++)
   {
       for (int32 X = 0; X < EffectiveSize.X; X++)
       {
           int32 SlotIndex = (StartY + Y) * GridColumns + (StartX + X);
           OutOccupiedSlots.Add(SlotIndex);
       }
   }
   
   // Check through inventory interface
   FVector2D ItemSizeVec(EffectiveSize.X, EffectiveSize.Y);
   bool bCanPlace = Inventory->CanPlaceItemAtSlot(ItemSizeVec, TargetSlot, true);
   
   UE_LOG(LogTemp, Verbose, TEXT("[InventoryUIBridge] Placement validation: slot %d, size %dx%d, result: %s"), 
       TargetSlot, EffectiveSize.X, EffectiveSize.Y, bCanPlace ? TEXT("Valid") : TEXT("Invalid"));
   
   return bCanPlace;
}

int32 USuspenseInventoryUIBridge::FindNearestValidSlot(
   int32 PreferredSlot,
   const FIntPoint& ItemSize,
   bool bIsRotated,
   int32 SearchRadius) const
{
   if (!ValidateInventoryConnection())
   {
       return INDEX_NONE;
   }
   
   // Get grid parameters
   ISuspenseInventoryInterface* Inventory = GameInventory.GetInterface();
   FVector2D GridSizeVec = Inventory->GetInventorySize();
   int32 GridColumns = FMath::RoundToInt(GridSizeVec.X);
   int32 GridRows = FMath::RoundToInt(GridSizeVec.Y);
   
   // Effective size with rotation
   FIntPoint EffectiveSize = bIsRotated ? 
       FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;
   
   // Check if item can fit in grid at all
   if (EffectiveSize.X > GridColumns || EffectiveSize.Y > GridRows)
   {
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Item size %dx%d exceeds grid size %dx%d"), 
           EffectiveSize.X, EffectiveSize.Y, GridColumns, GridRows);
       return INDEX_NONE;
   }
   
   // If preferred slot not set, start from center
   if (PreferredSlot < 0 || PreferredSlot >= GridColumns * GridRows)
   {
       PreferredSlot = (GridRows / 2) * GridColumns + (GridColumns / 2);
   }
   
   // Starting coordinates
   int32 StartX = PreferredSlot % GridColumns;
   int32 StartY = PreferredSlot / GridColumns;
   
   // Adjust starting coordinates so item fits
   StartX = FMath::Clamp(StartX, 0, GridColumns - EffectiveSize.X);
   StartY = FMath::Clamp(StartY, 0, GridRows - EffectiveSize.Y);
   
   int32 CorrectedPreferredSlot = StartY * GridColumns + StartX;
   
   UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Searching for valid slot near %d (%d,%d) for item %dx%d"), 
       CorrectedPreferredSlot, StartX, StartY, EffectiveSize.X, EffectiveSize.Y);
   
   // First check corrected preferred slot
   TArray<int32> TestOccupiedSlots;
   if (ValidateDropPlacement(CorrectedPreferredSlot, ItemSize, bIsRotated, TestOccupiedSlots))
   {
       return CorrectedPreferredSlot;
   }
   
   // Limit search radius
   if (SearchRadius <= 0)
   {
       SearchRadius = FMath::Max(GridColumns, GridRows);
   }
   
   // Spiral search from preferred position
   for (int32 Radius = 1; Radius <= SearchRadius; ++Radius)
   {
       // Check top and bottom rows
       for (int32 dx = -Radius; dx <= Radius; ++dx)
       {
           for (int32 dy : {-Radius, Radius})
           {
               int32 TestX = StartX + dx;
               int32 TestY = StartY + dy;
               
               // Check bounds considering item size
               if (TestX >= 0 && TestY >= 0 && 
                   TestX + EffectiveSize.X <= GridColumns && 
                   TestY + EffectiveSize.Y <= GridRows)
               {
                   int32 TestSlot = TestY * GridColumns + TestX;
                   TestOccupiedSlots.Empty();
                   
                   if (ValidateDropPlacement(TestSlot, ItemSize, bIsRotated, TestOccupiedSlots))
                   {
                       UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found valid slot %d at (%d,%d), radius %d"), 
                           TestSlot, TestX, TestY, Radius);
                       return TestSlot;
                   }
               }
           }
       }
       
       // Check left and right columns (excluding corners already checked)
       for (int32 dy = -Radius + 1; dy < Radius; ++dy)
       {
           for (int32 dx : {-Radius, Radius})
           {
               int32 TestX = StartX + dx;
               int32 TestY = StartY + dy;
               
               // Check bounds considering item size
               if (TestX >= 0 && TestY >= 0 && 
                   TestX + EffectiveSize.X <= GridColumns && 
                   TestY + EffectiveSize.Y <= GridRows)
               {
                   int32 TestSlot = TestY * GridColumns + TestX;
                   TestOccupiedSlots.Empty();
                   
                   if (ValidateDropPlacement(TestSlot, ItemSize, bIsRotated, TestOccupiedSlots))
                   {
                       UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Found valid slot %d at (%d,%d), radius %d"), 
                           TestSlot, TestX, TestY, Radius);
                       return TestSlot;
                   }
               }
           }
       }
   }
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No valid slot found within radius %d"), SearchRadius);
   return INDEX_NONE;
}

// =====================================================
// Processing Methods
// =====================================================

void USuspenseInventoryUIBridge::ProcessItemMoveRequest(const FGuid& ItemInstanceID, int32 TargetSlotIndex, bool bIsRotated)
{
   if (!GameInventory.GetInterface())
   {
       return;
   }
   
   UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Move request: Item %s to slot %d, rotated=%s"), 
       *ItemInstanceID.ToString(), TargetSlotIndex, bIsRotated ? TEXT("Yes") : TEXT("No"));
   
   // TODO: Implementation of movement through inventory interface
   
   // Notify about movement
   if (EventManager)
   {
       FGameplayTag MovedTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemMoved"));
       FString EventData = FString::Printf(TEXT("Item:%s,Slot:%d,Rotated:%d"), 
           *ItemInstanceID.ToString(), TargetSlotIndex, bIsRotated ? 1 : 0);
       EventManager->NotifyUIEvent(this, MovedTag, EventData);
   }
   
   // Schedule UI update
   ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemMoved")));
}

void USuspenseInventoryUIBridge::ProcessItemRotationRequest(int32 SlotIndex)
{
   if (!ValidateInventoryConnection())
   {
       return;
   }
   
   ISuspenseInventoryInterface* Inventory = GameInventory.GetInterface();
   
   // Check if there's an item at the slot
   FSuspenseInventoryItemInstance ItemInstance;
   if (!Inventory->GetItemInstanceAtSlot(SlotIndex, ItemInstance))
   {
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No item at slot %d to rotate"), SlotIndex);
       return;
   }
   
   // Check if rotation is possible
   if (Inventory->CanRotateItemAtSlot(SlotIndex))
   {
       // Perform rotation
       bool bRotateSuccess = Inventory->RotateItemAtSlot(SlotIndex);
       
       if (bRotateSuccess)
       {
           UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Rotated item %s in slot %d"), 
               *ItemInstance.ItemID.ToString(), SlotIndex);
           
           // Schedule UI update
           ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.ItemRotated")));
       }
       else
       {
           UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Failed to rotate item in slot %d"), SlotIndex);
       }
   }
   else
   {
       UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Cannot rotate item in slot %d (constraints check failed)"), SlotIndex);
   }
}

void USuspenseInventoryUIBridge::ProcessSortRequest()
{
   UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Sort request received"));
   // TODO: Implement inventory sorting
   // For now, just schedule a refresh
   ScheduleUIUpdate(FGameplayTag::RequestGameplayTag(TEXT("UI.Update.Sorted")));
}

void USuspenseInventoryUIBridge::ProcessItemDoubleClick(int32 SlotIndex, const FGuid& ItemInstanceID)
{
   if (!ValidateInventoryConnection())
   {
       return;
   }
   
   UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Double click on slot %d, item %s"), 
       SlotIndex, *ItemInstanceID.ToString());
   
   // TODO: Implement item usage/equip
   // This would typically:
   // 1. Check if item is usable/equippable
   // 2. Send appropriate event through EventManager
   // 3. Handle response and update UI
}

// =====================================================
// Utility Methods
// =====================================================

bool USuspenseInventoryUIBridge::ValidateInventoryConnection() const
{
   if (!GameInventory.GetInterface())
   {
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] No inventory interface connected"));
       return false;
   }
   
   return true;
}

USuspenseUIManager* USuspenseInventoryUIBridge::GetUIManager() const
{
   return UIManager;
}

FString USuspenseInventoryUIBridge::GetErrorCodeString(EInventoryErrorCode ErrorCode) const
{
   switch (ErrorCode)
   {
   case EInventoryErrorCode::Success:
       return TEXT("Success");
   case EInventoryErrorCode::NoSpace:
       return TEXT("Not enough space at target location");
   case EInventoryErrorCode::WeightLimit:
       return TEXT("Item would exceed weight limit");
   case EInventoryErrorCode::InvalidItem:
       return TEXT("Invalid item");
   case EInventoryErrorCode::ItemNotFound:
       return TEXT("Source item not found");
   case EInventoryErrorCode::InsufficientQuantity:
       return TEXT("Insufficient quantity");
   case EInventoryErrorCode::InvalidSlot:
       return TEXT("Invalid slot");
   case EInventoryErrorCode::SlotOccupied:
       return TEXT("Slot is occupied");
   case EInventoryErrorCode::TransactionActive:
       return TEXT("Transaction is active");
   case EInventoryErrorCode::NotInitialized:
       return TEXT("Not initialized");
   case EInventoryErrorCode::NetworkError:
       return TEXT("Network error");
   case EInventoryErrorCode::UnknownError:
   default:
       return TEXT("Unknown error occurred");
   }
}

// =====================================================
// Grid Calculation Helpers
// =====================================================

bool USuspenseInventoryUIBridge::GetInventoryGridParams(
   int32& OutColumns,
   int32& OutRows,
   float& OutCellSize,
   FGeometry& OutGridGeometry) const
{
   // Find inventory widget
   USuspenseInventoryWidget* InventoryWidget = GetCachedInventoryWidget();
   if (!InventoryWidget)
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Inventory widget not found"));
       return false;
   }
   
   // Get GridPanel
   UGridPanel* GridPanel = InventoryWidget->GetInventoryGrid();
   if (!GridPanel)
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Grid panel not found"));
       return false;
   }
   
   // Check visibility and geometry
   if (!GridPanel->IsVisible())
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Grid panel is not visible"));
       return false;
   }
   
   // Get actual geometry
   const FGeometry& CachedGeometry = GridPanel->GetCachedGeometry();
   if (CachedGeometry.GetLocalSize().IsNearlyZero())
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Grid panel has zero size"));
       return false;
   }
   
   // Get parameters
   OutColumns = InventoryWidget->GetGridColumns();
   OutRows = InventoryWidget->GetGridRows();
   OutCellSize = InventoryWidget->GetCellSize();
   OutGridGeometry = CachedGeometry;
   
   // Diagnostics
   UE_LOG(LogTemp, Log, TEXT("[InventoryUIBridge] Grid params: %dx%d, CellSize: %.1f, LocalSize: %s, AbsPos: %s"), 
       OutColumns, OutRows, OutCellSize,
       *CachedGeometry.GetLocalSize().ToString(),
       *CachedGeometry.GetAbsolutePosition().ToString());
   
   // Validation
   if (OutColumns <= 0 || OutRows <= 0 || OutCellSize <= 0.0f)
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Invalid grid parameters: %dx%d, cell size: %.1f"), 
           OutColumns, OutRows, OutCellSize);
       return false;
   }
   
   return true;
}

FVector2D USuspenseInventoryUIBridge::ScreenToGridCoordinates(
   const FVector2D& ScreenPos,
   const FGeometry& GridGeometry) const
{
   // Transform screen coordinates to local GridPanel coordinates
   return GridGeometry.AbsoluteToLocal(ScreenPos);
}

bool USuspenseInventoryUIBridge::AreRequiredSlotsFree(
   int32 AnchorSlot,
   const FIntPoint& ItemSize,
   int32 GridColumns,
   const FGuid& ExcludeItemID) const
{
   if (!ValidateInventoryConnection())
   {
       return false;
   }
   
   ISuspenseInventoryInterface* Inventory = GameInventory.GetInterface();
   
   // Get anchor slot coordinates
   int32 StartX = AnchorSlot % GridColumns;
   int32 StartY = AnchorSlot / GridColumns;
   
   // Check each required slot
   for (int32 Y = 0; Y < ItemSize.Y; Y++)
   {
       for (int32 X = 0; X < ItemSize.X; X++)
       {
           int32 SlotIndex = (StartY + Y) * GridColumns + (StartX + X);
           
           // Get instance at slot
           FSuspenseInventoryItemInstance Instance;
           if (Inventory->GetItemInstanceAtSlot(SlotIndex, Instance))
           {
               // Slot is occupied - check if it's the excluded item
               if (!ExcludeItemID.IsValid() || Instance.InstanceID != ExcludeItemID)
               {
                   return false; // Slot occupied by another item
               }
           }
       }
   }
   
   return true;
}

// =====================================================
// Diagnostics
// =====================================================

void USuspenseInventoryUIBridge::DiagnoseDropPosition(const FVector2D& ScreenPosition) const
{
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === DROP POSITION DIAGNOSIS ==="));
   
   // Find inventory widget
   USuspenseInventoryWidget* InventoryWidget = GetCachedInventoryWidget();
   if (!InventoryWidget)
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] Inventory widget not found"));
       return;
   }
   
   // Check visibility
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Inventory widget visible: %s"), 
       InventoryWidget->IsVisible() ? TEXT("Yes") : TEXT("No"));
   
   // Get inventory geometry
   const FGeometry& InventoryGeometry = InventoryWidget->GetCachedGeometry();
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Inventory geometry:"));
   UE_LOG(LogTemp, Warning, TEXT("  - Absolute position: %s"), 
       *InventoryGeometry.GetAbsolutePosition().ToString());
   UE_LOG(LogTemp, Warning, TEXT("  - Local size: %s"), 
       *InventoryGeometry.GetLocalSize().ToString());
   
   // Check if cursor is over inventory
   bool bIsUnderCursor = InventoryGeometry.IsUnderLocation(ScreenPosition);
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Cursor over inventory: %s"), 
       bIsUnderCursor ? TEXT("Yes") : TEXT("No"));
   
   // Get GridPanel
   if (UGridPanel* GridPanel = InventoryWidget->GetInventoryGrid())
   {
       const FGeometry& GridGeometry = GridPanel->GetCachedGeometry();
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Grid panel geometry:"));
       UE_LOG(LogTemp, Warning, TEXT("  - Absolute position: %s"), 
           *GridGeometry.GetAbsolutePosition().ToString());
       UE_LOG(LogTemp, Warning, TEXT("  - Local size: %s"), 
           *GridGeometry.GetLocalSize().ToString());
       
       // Check if cursor is over grid
       bool bIsOverGrid = GridGeometry.IsUnderLocation(ScreenPosition);
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Cursor over grid: %s"), 
           bIsOverGrid ? TEXT("Yes") : TEXT("No"));
       
       // Convert to local coordinates
       FVector2D LocalPos = GridGeometry.AbsoluteToLocal(ScreenPosition);
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Local position in grid: %s"), 
           *LocalPos.ToString());
   }
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === END DIAGNOSIS ==="));
}

void USuspenseInventoryUIBridge::DiagnoseInventoryState() const
{
   if (!ValidateInventoryConnection())
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] === DIAGNOSIS: No inventory connection ==="));
       return;
   }
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === INVENTORY STATE DIAGNOSIS ==="));
   
   // Get inventory interface
   ISuspenseInventoryInterface* InventoryInterface = GameInventory.GetInterface();
   if (!InventoryInterface)
   {
       UE_LOG(LogTemp, Error, TEXT("[InventoryUIBridge] No inventory interface available"));
       return;
   }
   
   // Get all items through interface
   TArray<FSuspenseInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Total items in inventory: %d"), AllInstances.Num());
   
   // Output information about each item
   for (int32 i = 0; i < AllInstances.Num(); i++)
   {
       const FSuspenseInventoryItemInstance& Instance = AllInstances[i];
       UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Item[%d]:"), i);
       UE_LOG(LogTemp, Warning, TEXT("  - ItemID: %s"), *Instance.ItemID.ToString());
       UE_LOG(LogTemp, Warning, TEXT("  - InstanceID: %s"), 
           Instance.InstanceID.IsValid() ? *Instance.InstanceID.ToString() : TEXT("INVALID"));
       UE_LOG(LogTemp, Warning, TEXT("  - Quantity: %d"), Instance.Quantity);
       UE_LOG(LogTemp, Warning, TEXT("  - Anchor Slot: %d"), Instance.AnchorIndex);
       UE_LOG(LogTemp, Warning, TEXT("  - Is Rotated: %s"), Instance.bIsRotated ? TEXT("Yes") : TEXT("No"));
   }
   
   // Check inventory size through interface
   FVector2D InventorySize = InventoryInterface->GetInventorySize();
   int32 TotalSlots = FMath::RoundToInt(InventorySize.X * InventorySize.Y);
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Inventory grid: %.0fx%.0f (%d total slots)"), 
       InventorySize.X, InventorySize.Y, TotalSlots);
   
   // Check weight through Blueprint-compatible interface methods
   float CurrentWeight = ISuspenseInventoryInterface::Execute_GetCurrentWeight(GameInventory.GetObject());
   float MaxWeight = ISuspenseInventoryInterface::Execute_GetMaxWeight(GameInventory.GetObject());
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Weight: %.1f / %.1f kg"), CurrentWeight, MaxWeight);
   
   // Check slot occupancy through GetItemInstanceAtSlot
   int32 OccupiedSlots = 0;
   for (int32 SlotIndex = 0; SlotIndex < FMath::Min(20, TotalSlots); SlotIndex++)
   {
       FSuspenseInventoryItemInstance InstanceAtSlot;
       if (InventoryInterface->GetItemInstanceAtSlot(SlotIndex, InstanceAtSlot))
       {
           OccupiedSlots++;
           
           UE_LOG(LogTemp, Warning, TEXT("  - Slot[%d]: %s (InstanceID: %s)"), 
               SlotIndex, 
               *InstanceAtSlot.ItemID.ToString(),
               InstanceAtSlot.InstanceID.IsValid() ? 
                   *InstanceAtSlot.InstanceID.ToString() : TEXT("INVALID"));
       }
   }
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Occupied slots in first 20: %d"), OccupiedSlots);
   
   // Check inventory settings
   FGameplayTagContainer AllowedTypes = ISuspenseInventoryInterface::Execute_GetAllowedItemTypes(GameInventory.GetObject());
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] Allowed item types: %d"), AllowedTypes.Num());
   
   UE_LOG(LogTemp, Warning, TEXT("[InventoryUIBridge] === END DIAGNOSIS ==="));
}