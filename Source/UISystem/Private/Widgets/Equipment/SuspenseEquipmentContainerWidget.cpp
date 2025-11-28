// MedComEquipmentContainerWidget.cpp
// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Equipment/SuspenseEquipmentContainerWidget.h"
#include "Widgets/Equipment/SuspenseEquipmentSlotWidget.h"
#include "Widgets/DragDrop/SuspenseDragDropOperation.h"
#include "DragDrop/SuspenseDragDropHandler.h"
#include "Interfaces/UI/ISuspenseEquipmentUIBridge.h"
#include "Interfaces/Core/ISuspenseLoadout.h"
#include "Components/SuspenseEquipmentUIBridge.h"

#include "Components/CanvasPanel.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"

#include "Delegates/SuspenseEventManager.h"
#include "Types/Loadout/SuspenseLoadoutManager.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

// ===== Constructor =====

USuspenseEquipmentContainerWidget::USuspenseEquipmentContainerWidget(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ContainerType = FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"));

    if (!SlotWidgetClass)
    {
        SlotWidgetClass = USuspenseEquipmentSlotWidget::StaticClass();
    }

    CellSize = 48.0f;
    CellPadding = 2.0f;

    bEventSubscriptionsActive = false;
    bAutoRefreshFromLoadoutManager = true;
    bHideUnusedSlots = true;

    FallbackLoadoutID = TEXT("Default_PMC");

    // Initialize MMO FPS taxonomy (see LoadoutSettings.h)
    AllAvailableSlotTypes = {
        EEquipmentSlotType::PrimaryWeapon,
        EEquipmentSlotType::SecondaryWeapon,
        EEquipmentSlotType::Holster,
        EEquipmentSlotType::Scabbard,
        EEquipmentSlotType::Headwear,
        EEquipmentSlotType::Earpiece,
        EEquipmentSlotType::Eyewear,
        EEquipmentSlotType::FaceCover,
        EEquipmentSlotType::BodyArmor,
        EEquipmentSlotType::TacticalRig,
        EEquipmentSlotType::Backpack,
        EEquipmentSlotType::SecureContainer,
        EEquipmentSlotType::QuickSlot1,
        EEquipmentSlotType::QuickSlot2,
        EEquipmentSlotType::QuickSlot3,
        EEquipmentSlotType::QuickSlot4,
        EEquipmentSlotType::Armband
    };
}

// ===== USuspenseBaseContainerWidget Overrides =====

void USuspenseEquipmentContainerWidget::InitializeContainer_Implementation(
    const FContainerUIData& ContainerData)
{
    Super::InitializeContainer_Implementation(ContainerData);
    RefreshFromLoadoutManager();

    UE_LOG(LogTemp, Log, TEXT("[%s] Equipment container initialized with type: %s"),
        *GetName(), *ContainerType.ToString());
}

void USuspenseEquipmentContainerWidget::UpdateContainer_Implementation(
    const FContainerUIData& ContainerData)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] UpdateContainer called before initialization"), *GetName());
        return;
    }

    CurrentContainerData = ContainerData;

    // Convert generic container data to equipment format
    FEquipmentContainerUIData EquipmentData;
    EquipmentData.ContainerType = ContainerData.ContainerType;
    EquipmentData.DisplayName = ContainerData.DisplayName;

    // Map slots 1:1 (equipment slots are single-cell)
    for (const FSlotUIData& SlotData : ContainerData.Slots)
    {
        FEquipmentSlotUIData EquipSlotData;
        EquipSlotData.SlotIndex = SlotData.SlotIndex;
        EquipSlotData.SlotType = SlotData.SlotType;
        EquipSlotData.AllowedItemTypes = SlotData.AllowedItemTypes;
        EquipSlotData.bIsOccupied = SlotData.bIsOccupied;
        EquipSlotData.GridPosition = FIntPoint(SlotData.GridX, SlotData.GridY);
        EquipSlotData.bIsRequired = false;
        EquipSlotData.bIsLocked = false;

        // Find matching item for this slot
        for (const FItemUIData& ItemData : ContainerData.Items)
        {
            if (ItemData.AnchorSlotIndex == SlotData.SlotIndex)
            {
                EquipSlotData.EquippedItem = ItemData;
                break;
            }
        }

        EquipmentData.Slots.Add(EquipSlotData);
    }

    UpdateEquipmentDisplay(EquipmentData);

    UE_LOG(LogTemp, Verbose, TEXT("[%s] Equipment container updated with %d slots"),
        *GetName(), ContainerData.Slots.Num());
}

// ===== UUserWidget Lifecycle =====

void USuspenseEquipmentContainerWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    InitializeAllSlotContainers();
    ValidateAllBorderBindings();

    if (IsDesignTime())
    {
        ShowAllSlotsForDesign();
    }
    else
    {
        UpdateAllSlotVisibility();
    }
}

void USuspenseEquipmentContainerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogTemp, Warning, TEXT("[%s] === Equipment NativeConstruct START ==="),
        *GetName());

    // Initialize slot container mappings
    InitializeAllSlotContainers();

    if (!ValidateAllBorderBindings())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] Some slot containers not bound (OK if not used in current loadout)"),
            *GetName());
    }

    InitializeSlotContainerMap();

    // Cache LoadoutManager
    CachedLoadoutManager = GetLoadoutManager();

    // Apply visibility based on current config
    UpdateAllSlotVisibility();

    // CRITICAL NEW LOGIC: Try to get UIBridge and subscribe directly
    if (!UIBridge)
    {
        // Try to get global bridge instance
        if (ISuspenseEquipmentUIBridgeInterface* BridgeInterface =
            ISuspenseEquipmentUIBridgeInterface::GetEquipmentUIBridge(this))
        {
            UIBridge = Cast<USuspenseEquipmentUIBridge>(BridgeInterface);

            if (UIBridge)
            {
                UE_LOG(LogTemp, Log,
                    TEXT("[%s] Found global UIBridge instance"), *GetName());
            }
        }
    }

    // Subscribe to UIBridge if available
    if (UIBridge)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] Subscribing to UIBridge OnEquipmentUIDataChanged..."),
            *GetName());

        // CRITICAL: Direct subscription to Bridge delegate
        // This replaces EventDelegateManager::NotifyInventoryUIRefreshRequested
        DataChangedHandle = UIBridge->OnEquipmentUIDataChanged.AddUObject(
            this, &USuspenseEquipmentContainerWidget::HandleEquipmentDataChanged
        );

        if (DataChangedHandle.IsValid())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[%s] ✓ Subscribed to UIBridge data changes"), *GetName());

            // Request initial data immediately
            const TArray<FEquipmentSlotUIData>& InitialData = UIBridge->GetCachedUIData();
            if (InitialData.Num() > 0)
            {
                UE_LOG(LogTemp, Log,
                    TEXT("[%s] Got initial data: %d slots"),
                    *GetName(), InitialData.Num());

                HandleEquipmentDataChanged(InitialData);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("[%s] ✗ Failed to subscribe to UIBridge!"), *GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] UIBridge not available yet - will retry later"), *GetName());
    }

    UE_LOG(LogTemp, Warning, TEXT("[%s] === Equipment NativeConstruct END ==="),
        *GetName());
}

void USuspenseEquipmentContainerWidget::NativeDestruct()
{
    UE_LOG(LogTemp, Warning, TEXT("[%s] === Equipment NativeDestruct START ==="),
        *GetName());

    // CRITICAL: Unsubscribe from UIBridge FIRST
    // This must happen before any other cleanup to prevent callbacks during destruction
    if (UIBridge && DataChangedHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] Unsubscribing from UIBridge..."), *GetName());

        UIBridge->OnEquipmentUIDataChanged.Remove(DataChangedHandle);
        DataChangedHandle.Reset();

        UE_LOG(LogTemp, Warning,
            TEXT("[%s] ✓ Unsubscribed from UIBridge"), *GetName());
    }

    // Unsubscribe from any legacy events
    UnsubscribeFromEvents();

    // Clear cached references
    UIBridge = nullptr;
    CachedLoadoutManager = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[%s] Cached references cleared"), *GetName());
    UE_LOG(LogTemp, Warning, TEXT("[%s] === Equipment NativeDestruct END ==="),
        *GetName());

    // Call parent destructor last
    Super::NativeDestruct();
}

// ===== NEW: Direct UIBridge Integration =====

void USuspenseEquipmentContainerWidget::SetUIBridge(USuspenseEquipmentUIBridge* InBridge)
{
    UE_LOG(LogTemp, Log, TEXT("[%s] SetUIBridge called"), *GetName());

    // Unsubscribe from old bridge if any
    if (UIBridge && DataChangedHandle.IsValid())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[%s] Unsubscribing from previous UIBridge"),
            *GetName());

        UIBridge->OnEquipmentUIDataChanged.Remove(DataChangedHandle);
        DataChangedHandle.Reset();
    }

    // Set new bridge
    UIBridge = InBridge;

    // Subscribe to new bridge if valid and widget is constructed
    if (UIBridge && IsConstructed())
    {
        UE_LOG(LogTemp, Log, TEXT("[%s] Subscribing to new UIBridge..."), *GetName());

        // Subscribe to data change delegate
        DataChangedHandle = UIBridge->OnEquipmentUIDataChanged.AddUObject(
            this, &USuspenseEquipmentContainerWidget::HandleEquipmentDataChanged
        );

        if (DataChangedHandle.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("[%s] ✓ Subscribed to UIBridge"), *GetName());

            // Get initial data
            const TArray<FEquipmentSlotUIData>& CurrentData = UIBridge->GetCachedUIData();
            if (CurrentData.Num() > 0)
            {
                HandleEquipmentDataChanged(CurrentData);
            }
        }
    }
}

void USuspenseEquipmentContainerWidget::HandleEquipmentDataChanged(
    const TArray<FEquipmentSlotUIData>& FreshData)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[%s] === HandleEquipmentDataChanged START ==="), *GetName());
    UE_LOG(LogTemp, Warning, TEXT("[%s] Received %d equipment slots"),
        *GetName(), FreshData.Num());

    // Build container data structure
    FEquipmentContainerUIData ContainerData;
    ContainerData.ContainerType = ContainerType;
    ContainerData.DisplayName = CurrentLoadoutConfig.LoadoutName;
    ContainerData.Slots = FreshData;

    // Calculate metrics from slot data
    ContainerData.TotalWeight = 0.0f;
    ContainerData.TotalArmor = 0.0f;

    for (const FEquipmentSlotUIData& eQSlot : FreshData)
    {
        if (eQSlot.bIsOccupied && eQSlot.EquippedItem.IsValid())
        {
            ContainerData.TotalWeight += eQSlot.EquippedItem.Weight;
            // Armor calculation if needed in future
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[%s] Container metrics: Weight=%.1f kg, Slots=%d"),
        *GetName(), ContainerData.TotalWeight, FreshData.Num());

    // CRITICAL: Update visual display with new data
    // This method will iterate all slot widgets and call UpdateEquipmentSlot
    UpdateEquipmentDisplay(ContainerData);

    // Force layout update to ensure visual consistency
    ForceLayoutPrepass();

    UE_LOG(LogTemp, Warning,
        TEXT("[%s] === HandleEquipmentDataChanged END ==="), *GetName());
}

void USuspenseEquipmentContainerWidget::RequestDataRefresh_Implementation()
{
    UE_LOG(LogTemp, Verbose, TEXT("[%s] RequestDataRefresh called"), *GetName());

    // NEW SIMPLIFIED LOGIC:
    // In new architecture, we don't need to request refresh through EventDelegateManager
    // Container is already subscribed to UIBridge and will receive updates automatically
    // This method is kept for compatibility but does minimal work

    if (UIBridge)
    {
        // Just trigger refresh on bridge if needed
        // Bridge will notify us through our subscription
        UIBridge->RefreshEquipmentUI_Implementation();
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] RequestDataRefresh: UIBridge not available"), *GetName());
    }
}

// ===== Equipment Display =====

void USuspenseEquipmentContainerWidget::UpdateEquipmentDisplay(
    const FEquipmentContainerUIData& EquipmentData)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] Cannot update - not initialized"), *GetName());
        return;
    }

    CurrentEquipmentData = EquipmentData;
    UpdateAllEquipmentSlots(EquipmentData);

    UE_LOG(LogTemp, Log, TEXT("[%s] Equipment display updated"), *GetName());
}

void USuspenseEquipmentContainerWidget::UpdateAllEquipmentSlots(
    const FEquipmentContainerUIData& EquipmentData)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[%s] === UpdateAllEquipmentSlots START ==="), *GetName());
    UE_LOG(LogTemp, Warning, TEXT("[%s] Updating %d slots"),
        *GetName(), EquipmentData.Slots.Num());

    // Build quick lookup map by SlotIndex
    TMap<int32, const FEquipmentSlotUIData*> EquipmentDataMap;
    for (const FEquipmentSlotUIData& SlotData : EquipmentData.Slots)
    {
        EquipmentDataMap.Add(SlotData.SlotIndex, &SlotData);

        UE_LOG(LogTemp, VeryVerbose,
            TEXT("[%s]   Slot %d: Type=%s, Occupied=%s"),
            *GetName(),
            SlotData.SlotIndex,
            *SlotData.SlotType.ToString(),
            SlotData.bIsOccupied ? TEXT("YES") : TEXT("NO"));
    }

    // CRITICAL: Update ALL slot widgets
    // Old logic only updated changed slots, which missed item data updates
    // New logic always updates all slots to ensure consistency

    int32 UpdatedCount = 0;

    for (const auto& SlotPair : SlotWidgets)
    {
        const int32 GlobalSlotIndex = SlotPair.Key;
        USuspenseBaseSlotWidget* BaseSlot = SlotPair.Value;

        if (!BaseSlot)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[%s]   Slot %d: Widget is NULL!"),
                *GetName(), GlobalSlotIndex);
            continue;
        }

        // Find matching equipment data
        const FEquipmentSlotUIData* const* FoundDataPtr =
            EquipmentDataMap.Find(GlobalSlotIndex);

        if (!FoundDataPtr || !(*FoundDataPtr))
        {
            UE_LOG(LogTemp, VeryVerbose,
                TEXT("[%s]   Slot %d: No equipment data"),
                *GetName(), GlobalSlotIndex);
            continue;
        }

        const FEquipmentSlotUIData& EquipSlotData = **FoundDataPtr;

        // Cast to equipment slot widget
        if (USuspenseEquipmentSlotWidget* EquipSlot =
            Cast<USuspenseEquipmentSlotWidget>(BaseSlot))
        {
            // CRITICAL: Call UpdateEquipmentSlot with full data
            // This will update slot state, item icon, and all visual elements
            EquipSlot->UpdateEquipmentSlot(EquipSlotData);
            ++UpdatedCount;

            // Update cache for this slot
            FGameplayTag SlotType;
            int32 LocalIndex;
            if (GetContainerFromGlobalIndex(GlobalSlotIndex, SlotType, LocalIndex))
            {
                if (FEquipmentSlotContainer* Container = EquipmentContainers.Find(SlotType))
                {
                    Container->CachedSlotStates.Add(
                        GlobalSlotIndex, EquipSlotData.bIsOccupied);
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("[%s]   Slot %d: Failed to cast to EquipmentSlotWidget!"),
                *GetName(), GlobalSlotIndex);
        }
    }

    // Force layout update
    ForceLayoutPrepass();

    UE_LOG(LogTemp, Warning,
        TEXT("[%s] === UpdateAllEquipmentSlots END (updated %d widgets) ==="),
        *GetName(), UpdatedCount);
}

USuspenseEquipmentSlotWidget* USuspenseEquipmentContainerWidget::GetEquipmentSlot(
    const FGameplayTag& SlotType, int32 LocalIndex) const
{
    if (const FEquipmentSlotContainer* Container = EquipmentContainers.Find(SlotType))
    {
        if (Container->SlotWidgets.IsValidIndex(LocalIndex))
        {
            return Container->SlotWidgets[LocalIndex];
        }
    }
    return nullptr;
}

USuspenseEquipmentSlotWidget* USuspenseEquipmentContainerWidget::GetEquipmentSlotByIndex(
    int32 GlobalIndex) const
{
    return Cast<USuspenseEquipmentSlotWidget>(GetSlotWidget(GlobalIndex));
}

FGameplayTag USuspenseEquipmentContainerWidget::GetSlotTypeForIndex(int32 GlobalIndex) const
{
    FGameplayTag SlotType;
    int32 LocalIndex;
    GetContainerFromGlobalIndex(GlobalIndex, SlotType, LocalIndex);
    return SlotType;
}

// ===== Loadout Management =====

void USuspenseEquipmentContainerWidget::RefreshFromLoadoutManager()
{
    UE_LOG(LogTemp, Log, TEXT("[%s] RefreshFromLoadoutManager called"), *GetName());

    USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();
    if (!LoadoutManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] LoadoutManager not found"), *GetName());
        UseDefaultLoadoutForTesting();
        return;
    }

    FName LoadoutID = GetCurrentLoadoutIDFromContext();
    if (LoadoutID.IsNone())
    {
        LoadoutID = FallbackLoadoutID;
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] No loadout ID found, using fallback: %s"),
            *GetName(), *LoadoutID.ToString());
    }

    FLoadoutConfiguration LoadoutConfig;
    if (!LoadoutManager->GetLoadoutConfigBP(LoadoutID, LoadoutConfig))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[%s] Failed to get loadout config for ID: %s"),
            *GetName(), *LoadoutID.ToString());
        UseDefaultLoadoutForTesting();
        return;
    }

    ApplyLoadoutConfigurationInternal(LoadoutConfig);
    CurrentLoadoutID = LoadoutID;
    K2_OnLoadoutChanged(LoadoutID);

    UE_LOG(LogTemp, Log,
        TEXT("[%s] Successfully refreshed from LoadoutManager with loadout: %s"),
        *GetName(), *LoadoutID.ToString());
}

void USuspenseEquipmentContainerWidget::ApplyLoadoutConfigurationInternal(
    const FLoadoutConfiguration& LoadoutConfig)
{
    if (!EquipmentCanvas)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[%s] Cannot initialize - EquipmentCanvas not bound"), *GetName());
        return;
    }

    CurrentLoadoutConfig = LoadoutConfig;

    UE_LOG(LogTemp, Log,
        TEXT("[%s] Applying loadout configuration: %s with %d equipment slots"),
        *GetName(), *LoadoutConfig.LoadoutName.ToString(),
        LoadoutConfig.EquipmentSlots.Num());

    if (LoadoutNameText)
    {
        LoadoutNameText->SetText(LoadoutConfig.LoadoutName);
    }

    // Build list of active (visible) slot types
    ActiveSlotTypes.Empty();
    for (const FEquipmentSlotConfig& SlotConfig : LoadoutConfig.EquipmentSlots)
    {
        if (SlotConfig.IsValid() && SlotConfig.bIsVisible)
        {
            ActiveSlotTypes.Add(SlotConfig.SlotType);
        }
    }

    InitializeSlotContainerMap();

    // Build container data structure
    FContainerUIData ContainerData;
    ContainerData.ContainerType = ContainerType;
    ContainerData.DisplayName = LoadoutConfig.LoadoutName;

    int32 TotalSlots = 0;
    for (const FEquipmentSlotConfig& SlotConfig : LoadoutConfig.EquipmentSlots)
    {
        if (SlotConfig.bIsVisible) { ++TotalSlots; }
    }
    ContainerData.Slots.Reserve(TotalSlots);

    EquipmentContainers.Empty();

    int32 CurrentSlotIndex = 0;
    for (const FEquipmentSlotConfig& SlotConfig : LoadoutConfig.EquipmentSlots)
    {
        if (!SlotConfig.IsValid() || !SlotConfig.bIsVisible) { continue; }

        // Create container info
        FEquipmentSlotContainer ContainerInfo;
        ContainerInfo.SlotConfig = SlotConfig;
        ContainerInfo.BaseSlotIndex = CurrentSlotIndex;

        // Create slot UI data
        FSlotUIData SlotData;
        SlotData.SlotIndex = CurrentSlotIndex++;
        SlotData.GridX = 0;
        SlotData.GridY = 0;
        SlotData.bIsOccupied = false;
        SlotData.AllowedItemTypes = SlotConfig.AllowedItemTypes;
        SlotData.SlotType = SlotConfig.SlotTag;

        ContainerData.Slots.Add(SlotData);

        const FGameplayTag SlotTypeTag = GetSlotTypeTag(SlotConfig.SlotType);
        if (SlotTypeTag.IsValid())
        {
            EquipmentContainers.Add(SlotTypeTag, ContainerInfo);
        }
    }

    CurrentContainerData = ContainerData;

    // Create visual slot containers
    CreateEquipmentContainers();
    UpdateAllSlotVisibility();

    UE_LOG(LogTemp, Log,
        TEXT("[%s] Created %d equipment slots from loadout configuration"),
        *GetName(), TotalSlots);
}

// ===== Slot Creation =====

void USuspenseEquipmentContainerWidget::CreateEquipmentContainers()
{
    if (SlotContainerMap.Num() == 0)
    {
        InitializeSlotContainerMap();
    }

    if (!ValidateSlotContainers())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] Slot container validation incomplete"), *GetName());
    }

    SlotWidgets.Empty();

    // Clear previous panels
    for (auto& Pair : EquipmentContainers)
    {
        if (Pair.Value.GridPanel)
        {
            Pair.Value.GridPanel->RemoveFromParent();
            Pair.Value.GridPanel = nullptr;
        }
        Pair.Value.SlotWidgets.Empty();
        Pair.Value.CachedSlotStates.Empty();
    }

    if (!WidgetTree || !SlotWidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[%s] WidgetTree or SlotWidgetClass is null"), *GetName());
        return;
    }

    int32 SuccessfulContainers = 0;

    // Create container for each active slot type
    for (auto& Pair : EquipmentContainers)
    {
        const FGameplayTag SlotType = Pair.Key;
        FEquipmentSlotContainer& Container = Pair.Value;

        UBorder** BorderPtr = SlotContainerMap.Find(SlotType);
        if (!BorderPtr || !(*BorderPtr))
        {
            UE_LOG(LogTemp, Verbose,
                TEXT("[%s] No Border container for slot type: %s"),
                *GetName(), *SlotType.ToString());
            continue;
        }

        UBorder* BorderContainer = *BorderPtr;
        BorderContainer->ClearChildren();

        UGridPanel* GridPanel = CreateGridPanelInContainer(BorderContainer, Container.SlotConfig);
        if (!GridPanel) { continue; }

        Container.GridPanel = GridPanel;
        CreateSlotsForContainer(SlotType, Container);
        ++SuccessfulContainers;
    }

    UE_LOG(LogTemp, Log, TEXT("[%s] Created %d equipment containers"),
        *GetName(), SuccessfulContainers);
}

UGridPanel* USuspenseEquipmentContainerWidget::CreateGridPanelInContainer(
    UBorder* Container, const FEquipmentSlotConfig& SlotConfig)
{
    if (!Container || !WidgetTree) { return nullptr; }

    UGridPanel* GridPanel = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass());
    if (!GridPanel) { return nullptr; }

    GridPanel->SetColumnFill(0, 1.0f);
    GridPanel->SetRowFill(0, 1.0f);

    Container->SetContent(GridPanel);
    return GridPanel;
}

void USuspenseEquipmentContainerWidget::CreateSlotsForContainer(
    FGameplayTag SlotType, FEquipmentSlotContainer& Container)
{
    if (!Container.GridPanel || !SlotWidgetClass) { return; }

    Container.SlotWidgets.Empty();
    Container.CachedSlotStates.Empty();

    const int32 GlobalIndex = Container.BaseSlotIndex;

    // Create single slot widget (equipment slots are 1x1)
    USuspenseEquipmentSlotWidget* SlotWidget =
        CreateWidget<USuspenseEquipmentSlotWidget>(this, SlotWidgetClass);

    if (!SlotWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] Failed to create slot widget"), *GetName());
        return;
    }

    SlotWidget->SetOwningContainer(this);

    // Initialize slot data
    FEquipmentSlotUIData EquipSlotData;
    EquipSlotData.SlotIndex = GlobalIndex;
    EquipSlotData.SlotType = SlotType;
    EquipSlotData.AllowedItemTypes = Container.SlotConfig.AllowedItemTypes;
    EquipSlotData.GridSize = FIntPoint(1, 1);
    EquipSlotData.GridPosition = FIntPoint(0, 0);
    EquipSlotData.bIsOccupied = false;
    EquipSlotData.bIsLocked = false;
    EquipSlotData.bIsRequired = Container.SlotConfig.bIsRequired;
    EquipSlotData.SlotName = Container.SlotConfig.DisplayName;

    SlotWidget->InitializeEquipmentSlot(EquipSlotData);
    SlotWidget->SetVisibility(ESlateVisibility::Visible);

    // Add to grid
    UGridSlot* GridSlotComponent = Container.GridPanel->AddChildToGrid(SlotWidget);
    if (GridSlotComponent)
    {
        GridSlotComponent->SetColumn(0);
        GridSlotComponent->SetRow(0);
        GridSlotComponent->SetPadding(FMargin(CellPadding));
        GridSlotComponent->SetHorizontalAlignment(HAlign_Fill);
        GridSlotComponent->SetVerticalAlignment(VAlign_Fill);
    }

    Container.SlotWidgets.Add(SlotWidget);
    Container.CachedSlotStates.Add(GlobalIndex, false);
    SlotWidgets.Add(GlobalIndex, SlotWidget);

    UE_LOG(LogTemp, Log, TEXT("[%s] Created equipment slot for %s"),
        *GetName(), *SlotType.ToString());
}

void USuspenseEquipmentContainerWidget::CreateSlots()
{
    if (!EquipmentCanvas || EquipmentContainers.Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] CreateSlots - No containers configured"), *GetName());
        return;
    }

    SlotWidgets.Empty();

    for (auto& Pair : EquipmentContainers)
    {
        CreateSlotsForContainer(Pair.Key, Pair.Value);
    }

    UE_LOG(LogTemp, Log, TEXT("[%s] Created %d equipment slots"),
        *GetName(), SlotWidgets.Num());
}

void USuspenseEquipmentContainerWidget::ClearSlots()
{
    Super::ClearSlots();

    for (auto& Pair : EquipmentContainers)
    {
        if (Pair.Value.GridPanel)
        {
            Pair.Value.GridPanel->RemoveFromParent();
            Pair.Value.GridPanel = nullptr;
        }
        Pair.Value.SlotWidgets.Empty();
        Pair.Value.CachedSlotStates.Empty();
    }

    EquipmentContainers.Empty();
}

// ===== Validation & Helpers =====

bool USuspenseEquipmentContainerWidget::ValidateSlotsPanel() const
{
    if (!EquipmentCanvas)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] EquipmentCanvas is not bound!"), *GetName());
        return false;
    }
    return true;
}

bool USuspenseEquipmentContainerWidget::ValidateAllBorderBindings() const
{
    struct LocalCheck
    {
        static void Check(const UUserWidget* Self, const UBorder* Border, const TCHAR* Name)
        {
            if (!Border)
            {
                UE_LOG(LogTemp, Verbose,
                    TEXT("[%s] Optional container not bound: %s"),
                    *Self->GetName(), Name);
            }
        }
    };

    LocalCheck::Check(this, PrimaryWeaponSlotContainer, TEXT("PrimaryWeaponSlotContainer"));
    LocalCheck::Check(this, SecondaryWeaponSlotContainer, TEXT("SecondaryWeaponSlotContainer"));
    LocalCheck::Check(this, HolsterSlotContainer, TEXT("HolsterSlotContainer"));
    LocalCheck::Check(this, ScabbardSlotContainer, TEXT("ScabbardSlotContainer"));
    LocalCheck::Check(this, HeadwearSlotContainer, TEXT("HeadwearSlotContainer"));
    LocalCheck::Check(this, EarpieceSlotContainer, TEXT("EarpieceSlotContainer"));
    LocalCheck::Check(this, EyewearSlotContainer, TEXT("EyewearSlotContainer"));
    LocalCheck::Check(this, FaceCoverSlotContainer, TEXT("FaceCoverSlotContainer"));
    LocalCheck::Check(this, BodyArmorSlotContainer, TEXT("BodyArmorSlotContainer"));
    LocalCheck::Check(this, TacticalRigSlotContainer, TEXT("TacticalRigSlotContainer"));
    LocalCheck::Check(this, BackpackSlotContainer, TEXT("BackpackSlotContainer"));
    LocalCheck::Check(this, SecureContainerSlotContainer, TEXT("SecureContainerSlotContainer"));
    LocalCheck::Check(this, QuickSlot1Container, TEXT("QuickSlot1Container"));
    LocalCheck::Check(this, QuickSlot2Container, TEXT("QuickSlot2Container"));
    LocalCheck::Check(this, QuickSlot3Container, TEXT("QuickSlot3Container"));
    LocalCheck::Check(this, QuickSlot4Container, TEXT("QuickSlot4Container"));
    LocalCheck::Check(this, ArmbandSlotContainer, TEXT("ArmbandSlotContainer"));

    return true;
}

bool USuspenseEquipmentContainerWidget::ValidateSlotContainers() const
{
    bool bAllValid = true;
    for (EEquipmentSlotType SlotType : ActiveSlotTypes)
    {
        const FGameplayTag SlotTag = GetSlotTypeTag(SlotType);
        if (!SlotContainerMap.Contains(SlotTag))
        {
            bAllValid = false;
            UE_LOG(LogTemp, Verbose,
                TEXT("[%s] No container bound for %s (may be hidden)"),
                *GetName(), *SlotTag.ToString());
        }
    }
    return bAllValid;
}

void USuspenseEquipmentContainerWidget::InitializeAllSlotContainers()
{
    AllSlotContainers.Empty();

    // Map ALL slots to their borders
    AllSlotContainers.Add(EEquipmentSlotType::PrimaryWeapon, PrimaryWeaponSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::SecondaryWeapon, SecondaryWeaponSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::Holster, HolsterSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::Scabbard, ScabbardSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::Headwear, HeadwearSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::Earpiece, EarpieceSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::Eyewear, EyewearSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::FaceCover, FaceCoverSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::BodyArmor, BodyArmorSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::TacticalRig, TacticalRigSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::Backpack, BackpackSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::SecureContainer, SecureContainerSlotContainer);
    AllSlotContainers.Add(EEquipmentSlotType::QuickSlot1, QuickSlot1Container);
    AllSlotContainers.Add(EEquipmentSlotType::QuickSlot2, QuickSlot2Container);
    AllSlotContainers.Add(EEquipmentSlotType::QuickSlot3, QuickSlot3Container);
    AllSlotContainers.Add(EEquipmentSlotType::QuickSlot4, QuickSlot4Container);
    AllSlotContainers.Add(EEquipmentSlotType::Armband, ArmbandSlotContainer);

    int32 ValidContainers = 0;
    for (const auto& Pair : AllSlotContainers)
    {
        if (Pair.Value) { ++ValidContainers; }
    }

    UE_LOG(LogTemp, Log, TEXT("[%s] Initialized %d/%d slot container mappings"),
        *GetName(), ValidContainers, AllSlotContainers.Num());
}

void USuspenseEquipmentContainerWidget::InitializeSlotContainerMap()
{
    SlotContainerMap.Empty();

    for (EEquipmentSlotType SlotType : ActiveSlotTypes)
    {
        FGameplayTag SlotTag = GetSlotTypeTag(SlotType);
        if (SlotTag.IsValid())
        {
            if (UBorder** ContainerPtr = AllSlotContainers.Find(SlotType))
            {
                if (*ContainerPtr)
                {
                    SlotContainerMap.Add(SlotTag, *ContainerPtr);
                }
            }
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[%s] Initialized slot container map with %d active entries"),
        *GetName(), SlotContainerMap.Num());
}

FGameplayTag USuspenseEquipmentContainerWidget::GetSlotTypeTag(EEquipmentSlotType SlotType) const
{
    switch (SlotType)
    {
        case EEquipmentSlotType::PrimaryWeapon:   return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.PrimaryWeapon"));
        case EEquipmentSlotType::SecondaryWeapon: return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecondaryWeapon"));
        case EEquipmentSlotType::Holster:         return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Holster"));
        case EEquipmentSlotType::Scabbard:        return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Scabbard"));
        case EEquipmentSlotType::Headwear:        return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Headwear"));
        case EEquipmentSlotType::Earpiece:        return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Earpiece"));
        case EEquipmentSlotType::Eyewear:         return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Eyewear"));
        case EEquipmentSlotType::FaceCover:       return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.FaceCover"));
        case EEquipmentSlotType::BodyArmor:       return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.BodyArmor"));
        case EEquipmentSlotType::TacticalRig:     return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.TacticalRig"));
        case EEquipmentSlotType::Backpack:        return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Backpack"));
        case EEquipmentSlotType::SecureContainer: return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecureContainer"));
        case EEquipmentSlotType::QuickSlot1:      return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot1"));
        case EEquipmentSlotType::QuickSlot2:      return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot2"));
        case EEquipmentSlotType::QuickSlot3:      return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot3"));
        case EEquipmentSlotType::QuickSlot4:      return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot4"));
        case EEquipmentSlotType::Armband:         return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Armband"));
        default: return FGameplayTag();
    }
}

int32 USuspenseEquipmentContainerWidget::CalculateGlobalIndex(
    const FGameplayTag& SlotType, int32 LocalIndex) const
{
    if (const FEquipmentSlotContainer* Container = EquipmentContainers.Find(SlotType))
    {
        return Container->BaseSlotIndex + LocalIndex;
    }
    return LocalIndex;
}

bool USuspenseEquipmentContainerWidget::GetContainerFromGlobalIndex(
    int32 GlobalIndex, FGameplayTag& OutSlotType, int32& OutLocalIndex) const
{
    for (const auto& Pair : EquipmentContainers)
    {
        const FEquipmentSlotContainer& Container = Pair.Value;
        if (GlobalIndex == Container.BaseSlotIndex)
        {
            OutSlotType = Pair.Key;
            OutLocalIndex = 0;
            return true;
        }
    }
    return false;
}

bool USuspenseEquipmentContainerWidget::CalculateOccupiedSlots(
    int32 TargetSlot, FIntPoint /*ItemSize*/, bool /*bIsRotated*/,
    TArray<int32>& OutOccupiedSlots) const
{
    // Equipment slots are always 1x1
    OutOccupiedSlots.Reset();
    OutOccupiedSlots.Add(TargetSlot);
    return true;
}

bool USuspenseEquipmentContainerWidget::CalculateOccupiedSlotsInContainer(
    const FGameplayTag& SlotType, int32 LocalIndex, FIntPoint /*ItemSize*/,
    TArray<int32>& OutGlobalIndices) const
{
    OutGlobalIndices.Reset();
    const FEquipmentSlotContainer* Container = EquipmentContainers.Find(SlotType);
    if (!Container) { return false; }
    OutGlobalIndices.Add(Container->BaseSlotIndex);
    return true;
}

FSmartDropZone USuspenseEquipmentContainerWidget::FindBestDropZone(
    const FVector2D& ScreenPosition, const FIntPoint& /*ItemSize*/,
    bool /*bIsRotated*/) const
{
    FSmartDropZone Result;

    if (USuspenseBaseSlotWidget* SlotWidget = GetSlotAtScreenPosition(ScreenPosition))
    {
        Result.SlotIndex = ISuspenseSlotUI::Execute_GetSlotIndex(SlotWidget);
        Result.bIsValid = true;
        Result.FeedbackPosition = SlotWidget->GetCachedGeometry().GetAbsolutePosition() +
                                  SlotWidget->GetCachedGeometry().GetLocalSize() * 0.5f;
    }
    return Result;
}

bool USuspenseEquipmentContainerWidget::IsItemTypeAllowedInSlot(
    const FGameplayTag& ItemType, const FGameplayTag& SlotType) const
{
    if (const FEquipmentSlotContainer* Container = EquipmentContainers.Find(SlotType))
    {
        FGameplayTagContainer ItemTypeContainer;
        ItemTypeContainer.AddTag(ItemType);

        const bool bAllowed = Container->SlotConfig.AllowedItemTypes.IsEmpty()
                            || Container->SlotConfig.AllowedItemTypes.HasAny(ItemTypeContainer);

        return bAllowed && !Container->SlotConfig.DisallowedItemTypes.HasTag(ItemType);
    }
    return false;
}

FSlotValidationResult USuspenseEquipmentContainerWidget::CanAcceptDrop_Implementation(
    const UDragDropOperation* DragOperation, int32 TargetSlotIndex) const
{
    const FSlotValidationResult BaseResult =
        Super::CanAcceptDrop_Implementation(DragOperation, TargetSlotIndex);

    if (!BaseResult.bIsValid)
    {
        return BaseResult;
    }

    const USuspenseDragDropOperation* MedComDragOp =
        Cast<USuspenseDragDropOperation>(DragOperation);

    if (!MedComDragOp || !MedComDragOp->IsValidOperation())
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Invalid drag operation")));
    }

    const FDragDropUIData& DragData = MedComDragOp->GetDragData();

    FGameplayTag SlotType;
    int32 LocalIndex;
    if (!GetContainerFromGlobalIndex(TargetSlotIndex, SlotType, LocalIndex))
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Invalid equipment slot")),
            EEquipmentValidationFailure::InvalidSlot,
            FGameplayTag::RequestGameplayTag(TEXT("UI.Error.InvalidSlot"))
        );
    }

    if (!IsItemTypeAllowedInSlot(DragData.ItemData.ItemType, SlotType))
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Item type not allowed in this equipment slot")),
            EEquipmentValidationFailure::IncompatibleType,
            FGameplayTag::RequestGameplayTag(TEXT("UI.Error.IncompatibleType"))
        );
    }

    return FSlotValidationResult::Success();
}

// ===== Visibility Management =====

void USuspenseEquipmentContainerWidget::UpdateAllSlotVisibility()
{
    // Hide all first
    for (auto& Pair : AllSlotContainers)
    {
        if (Pair.Value) { SetSlotVisibility(Pair.Key, false); }
    }

    // Show only active
    for (EEquipmentSlotType ActiveSlot : ActiveSlotTypes)
    {
        SetSlotVisibility(ActiveSlot, true);
    }

    if (!bHideUnusedSlots)
    {
        for (auto& Pair : AllSlotContainers)
        {
            if (Pair.Value) { SetSlotVisibility(Pair.Key, true); }
        }
    }
}

void USuspenseEquipmentContainerWidget::SetSlotVisibility(
    EEquipmentSlotType SlotType, bool bVisible)
{
    if (UBorder** ContainerPtr = AllSlotContainers.Find(SlotType))
    {
        if (*ContainerPtr)
        {
            (*ContainerPtr)->SetVisibility(
                bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            K2_OnSlotVisibilityChanged(SlotType, bVisible);
        }
    }
}

void USuspenseEquipmentContainerWidget::ShowAllSlotsForDesign()
{
    for (auto& Pair : AllSlotContainers)
    {
        if (Pair.Value)
        {
            Pair.Value->SetVisibility(ESlateVisibility::Visible);
        }
    }
}

void USuspenseEquipmentContainerWidget::UpdateSlotWidget(
    int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    if (USuspenseEquipmentSlotWidget* EquipSlot =
        Cast<USuspenseEquipmentSlotWidget>(GetSlotWidget(SlotIndex)))
    {
        FEquipmentSlotUIData EquipSlotData;
        EquipSlotData.SlotIndex = SlotData.SlotIndex;
        EquipSlotData.SlotType = SlotData.SlotType;
        EquipSlotData.AllowedItemTypes = SlotData.AllowedItemTypes;
        EquipSlotData.bIsOccupied = SlotData.bIsOccupied;
        EquipSlotData.GridPosition = FIntPoint(SlotData.GridX, SlotData.GridY);

        if (SlotData.bIsOccupied && ItemData.IsValid())
        {
            EquipSlotData.EquippedItem = ItemData;
        }

        EquipSlot->UpdateEquipmentSlot(EquipSlotData);

        FGameplayTag SlotType;
        int32 LocalIndex;
        if (GetContainerFromGlobalIndex(SlotIndex, SlotType, LocalIndex))
        {
            if (FEquipmentSlotContainer* Container = EquipmentContainers.Find(SlotType))
            {
                Container->CachedSlotStates.Add(SlotIndex, SlotData.bIsOccupied);
            }
        }
    }
    else
    {
        Super::UpdateSlotWidget(SlotIndex, SlotData, ItemData);
    }
}

// ===== Widget Initialization =====

void USuspenseEquipmentContainerWidget::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();

    if (bIsInitialized)
    {
        SubscribeToEvents();
        RefreshFromLoadoutManager();

        UE_LOG(LogTemp, Log,
            TEXT("[%s] Equipment container widget initialized"), *GetName());
    }
}

// ===== Event Subscriptions (Legacy Compatibility) =====

void USuspenseEquipmentContainerWidget::SubscribeToEvents()
{
    if (bEventSubscriptionsActive) { return; }

    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        EventManager->OnEquipmentSlotUpdated.AddDynamic(
            this, &USuspenseEquipmentContainerWidget::OnEquipmentSlotUpdated);
        EventManager->OnLoadoutChanged.AddDynamic(
            this, &USuspenseEquipmentContainerWidget::OnLoadoutChanged);

        bEventSubscriptionsActive = true;

        UE_LOG(LogTemp, Log, TEXT("[%s] Subscribed to legacy equipment events"),
            *GetName());
    }
}

void USuspenseEquipmentContainerWidget::UnsubscribeFromEvents()
{
    if (!bEventSubscriptionsActive) { return; }

    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        EventManager->OnEquipmentSlotUpdated.RemoveDynamic(
            this, &USuspenseEquipmentContainerWidget::OnEquipmentSlotUpdated);
        EventManager->OnLoadoutChanged.RemoveDynamic(
            this, &USuspenseEquipmentContainerWidget::OnLoadoutChanged);

        bEventSubscriptionsActive = false;

        UE_LOG(LogTemp, Log, TEXT("[%s] Unsubscribed from legacy equipment events"),
            *GetName());
    }
}

void USuspenseEquipmentContainerWidget::OnEquipmentSlotUpdated(
    int32 SlotIndex, const FGameplayTag& SlotType, bool bIsOccupied)
{
    // Legacy event - kept for compatibility
    if (FEquipmentSlotContainer* Container = EquipmentContainers.Find(SlotType))
    {
        Container->CachedSlotStates.Add(SlotIndex, bIsOccupied);
    }
}

void USuspenseEquipmentContainerWidget::OnEquipmentUIRefreshRequested(UUserWidget* Widget)
{
    // Legacy event - no longer used in new architecture
    // Kept for compatibility but does nothing
}

void USuspenseEquipmentContainerWidget::OnLoadoutChanged(
    const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess)
{
    if (PlayerState)
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            if (PC->GetPlayerState<APlayerState>() != PlayerState) { return; }
        }
    }

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[%s] Loadout changed to: %s"),
            *GetName(), *LoadoutID.ToString());
        RefreshFromLoadoutManager();
    }
}

// ===== Helper Lookups =====

USuspenseLoadoutManager* USuspenseEquipmentContainerWidget::GetLoadoutManager() const
{
    if (CachedLoadoutManager) { return CachedLoadoutManager; }
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<USuspenseLoadoutManager>();
    }
    return nullptr;
}

FName USuspenseEquipmentContainerWidget::GetCurrentLoadoutIDFromContext() const
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
        {
            if (PS->GetClass()->ImplementsInterface(USuspenseLoadout::StaticClass()))
            {
                return ISuspenseLoadout::Execute_GetCurrentLoadoutID(PS);
            }
        }
    }
    return NAME_None;
}

ISuspenseEquipmentUIBridgeInterface* USuspenseEquipmentContainerWidget::GetOrCreateEquipmentBridge()
{
    // Deprecated - use direct UIBridge reference instead
    return UIBridge;
}

bool USuspenseEquipmentContainerWidget::ProcessEquipmentOperationThroughBridge(
    const FDragDropUIData& DragData, int32 TargetSlotIndex)
{
    if (!UIBridge)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] No equipment bridge available"), *GetName());
        return false;
    }

    const bool bSuccess = UIBridge->ProcessEquipmentDrop_Implementation(
        TargetSlotIndex, DragData);

    return bSuccess;
}

bool USuspenseEquipmentContainerWidget::GetSlotConfigByType(
    EEquipmentSlotType SlotType, FEquipmentSlotConfig& OutConfig) const
{
    for (const FEquipmentSlotConfig& Config : CurrentLoadoutConfig.EquipmentSlots)
    {
        if (Config.SlotType == SlotType)
        {
            OutConfig = Config;
            return true;
        }
    }
    OutConfig = FEquipmentSlotConfig();
    return false;
}

bool USuspenseEquipmentContainerWidget::IsSlotActiveInCurrentLoadout(
    EEquipmentSlotType SlotType) const
{
    return ActiveSlotTypes.Contains(SlotType);
}

void USuspenseEquipmentContainerWidget::UpdateSlotVisibilityFromConfig()
{
    UpdateAllSlotVisibility();
}

void USuspenseEquipmentContainerWidget::UseDefaultLoadoutForTesting()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[%s] Using default loadout configuration for testing"), *GetName());

    // Build minimal test config
    FLoadoutConfiguration TestConfig;
    TestConfig.LoadoutName = FText::FromString(TEXT("Test PMC Loadout"));

    auto AddSlot = [&](EEquipmentSlotType Type, const TCHAR* Tag,
                       TArray<const TCHAR*> Allowed)
    {
        FEquipmentSlotConfig SlotConfig(Type, FGameplayTag::RequestGameplayTag(Tag));
        for (const TCHAR* T : Allowed)
        {
            SlotConfig.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(T));
        }
        SlotConfig.bIsVisible = true;
        SlotConfig.bIsRequired = false;
        TestConfig.EquipmentSlots.Add(SlotConfig);
    };

    AddSlot(EEquipmentSlotType::PrimaryWeapon, TEXT("Equipment.Slot.PrimaryWeapon"),
            { TEXT("Item.Weapon.AR"), TEXT("Item.Weapon.DMR") });
    AddSlot(EEquipmentSlotType::SecondaryWeapon, TEXT("Equipment.Slot.SecondaryWeapon"),
            { TEXT("Item.Weapon.SMG") });
    AddSlot(EEquipmentSlotType::Holster, TEXT("Equipment.Slot.Holster"),
            { TEXT("Item.Weapon.Pistol") });
    AddSlot(EEquipmentSlotType::BodyArmor, TEXT("Equipment.Slot.BodyArmor"),
            { TEXT("Item.Armor.BodyArmor") });
    AddSlot(EEquipmentSlotType::TacticalRig, TEXT("Equipment.Slot.TacticalRig"),
            { TEXT("Item.Gear.TacticalRig") });

    ApplyLoadoutConfigurationInternal(TestConfig);

    ActiveSlotTypes = {
        EEquipmentSlotType::PrimaryWeapon,
        EEquipmentSlotType::SecondaryWeapon,
        EEquipmentSlotType::Holster,
        EEquipmentSlotType::BodyArmor,
        EEquipmentSlotType::TacticalRig
    };

    UpdateAllSlotVisibility();
}
