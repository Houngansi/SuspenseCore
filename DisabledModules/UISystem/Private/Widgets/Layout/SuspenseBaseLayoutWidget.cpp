// MedComBaseLayoutWidget.cpp
// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Layout/SuspenseBaseLayoutWidget.h"
#include "Components/PanelWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIWidget.h"
#include "SuspenseCore/Interfaces/Screens/ISuspenseCoreScreen.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/SuspenseUIManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

USuspenseBaseLayoutWidget::USuspenseBaseLayoutWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bAutoCreateWidgets = true;
    bValidateOnInit = true;
    bRegisterLayoutInUIManager = false;
}

void USuspenseBaseLayoutWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void USuspenseBaseLayoutWidget::NativeDestruct()
{
    ClearCreatedWidgets();
    Super::NativeDestruct();
}

void USuspenseBaseLayoutWidget::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();

    // Validate configuration if requested
    if (bValidateOnInit && !ValidateConfigurationInternal())
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] Configuration validation failed!"), *GetName());
        return;
    }

    // Register this layout in UIManager if needed
    if (bRegisterLayoutInUIManager && WidgetTag.IsValid())
    {
        if (USuspenseUIManager* UIManager = GetUIManager())
        {
            UIManager->RegisterExternalWidget(this, WidgetTag);
            UE_LOG(LogTemp, Log, TEXT("[%s] Layout registered in UIManager with tag %s"),
                *GetName(), *WidgetTag.ToString());
        }
    }

    // Auto-create widgets if enabled
    if (bAutoCreateWidgets)
    {
        InitializeFromConfig();
    }

    UE_LOG(LogTemp, Log, TEXT("[%s] Layout widget initialized with %d child widgets configured, %d created"),
        *GetName(), WidgetConfigurations.Num(), LayoutWidgets.Num());
}

void USuspenseBaseLayoutWidget::UninitializeWidget_Implementation()
{
    ClearCreatedWidgets();

    // Unregister from UIManager if registered
    if (bRegisterLayoutInUIManager && WidgetTag.IsValid())
    {
        if (USuspenseUIManager* UIManager = GetUIManager())
        {
            UIManager->UnregisterWidget(WidgetTag);
        }
    }

    Super::UninitializeWidget_Implementation();
}

bool USuspenseBaseLayoutWidget::AddWidgetToLayout_Implementation(UUserWidget* Widget, FGameplayTag SlotTag)
{
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] AddWidgetToLayout: Widget is null"), *GetName());
        return false;
    }

    if (!SlotTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] AddWidgetToLayout: SlotTag is invalid. All widgets must have explicit tags."),
            *GetName());
        return false;
    }

    // Check if widget with this tag already exists
    if (LayoutWidgets.Contains(SlotTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] AddWidgetToLayout: Widget with tag %s already exists"),
            *GetName(), *SlotTag.ToString());
        return false;
    }

    // Find configuration for this slot tag
    const FLayoutWidgetConfig* Config = FindConfigByTag(SlotTag);

    if (!Config)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] AddWidgetToLayout: No configuration found for tag %s"),
            *GetName(), *SlotTag.ToString());
        // We can still add the widget without configuration, but with default settings
    }

    // Add to panel
    if (!AddWidgetToPanel(Widget, Config))
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] AddWidgetToLayout: Failed to add widget to panel"), *GetName());
        return false;
    }

    // Store reference
    LayoutWidgets.Add(SlotTag, Widget);

    // Initialize widget if it supports the interface and config says to
    if (Config && Config->bAutoInitialize)
    {
        if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
        {
            ISuspenseUIWidget::Execute_InitializeWidget(Widget);
        }
    }

    // Register in UIManager if needed
    if (Config && Config->bRegisterInUIManager)
    {
        RegisterWidgetInUIManager(Widget, SlotTag);
    }

    // Notify about addition
    NotifyWidgetCreated(Widget, SlotTag);
    K2_OnWidgetAdded(Widget, SlotTag);

    UE_LOG(LogTemp, Log, TEXT("[%s] Added widget %s with tag %s"),
        *GetName(), *Widget->GetName(), *SlotTag.ToString());

    return true;
}

bool USuspenseBaseLayoutWidget::RemoveWidgetFromLayout_Implementation(UUserWidget* Widget)
{
    if (!Widget)
    {
        return false;
    }

    // Find the tag for this widget
    FGameplayTag FoundTag;
    for (const auto& Pair : LayoutWidgets)
    {
        if (Pair.Value == Widget)
        {
            FoundTag = Pair.Key;
            break;
        }
    }

    if (!FoundTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] RemoveWidgetFromLayout: Widget not found in layout"), *GetName());
        return false;
    }

    // Check if widget should be unregistered from UIManager
    const FLayoutWidgetConfig* Config = FindConfigByTag(FoundTag);
    if (Config && Config->bRegisterInUIManager)
    {
        UnregisterWidgetFromUIManager(FoundTag);
    }

    // Uninitialize if supported
    if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_UninitializeWidget(Widget);
    }

    // Remove from layout
    LayoutWidgets.Remove(FoundTag);
    Widget->RemoveFromParent();

    // Notify about removal
    NotifyWidgetDestroyed(FoundTag);
    K2_OnWidgetRemoved(Widget, FoundTag);

    UE_LOG(LogTemp, Log, TEXT("[%s] Removed widget %s with tag %s"),
        *GetName(), *Widget->GetName(), *FoundTag.ToString());

    return true;
}

TArray<UUserWidget*> USuspenseBaseLayoutWidget::GetLayoutWidgets_Implementation() const
{
    TArray<UUserWidget*> Widgets;
    LayoutWidgets.GenerateValueArray(Widgets);
    return Widgets;
}

void USuspenseBaseLayoutWidget::ClearLayout_Implementation()
{
    K2_OnLayoutClearing();
    ClearCreatedWidgets();
}

void USuspenseBaseLayoutWidget::RefreshLayout_Implementation()
{
    // Force layout refresh on the panel
    if (UPanelWidget* Panel = GetLayoutPanel())
    {
        Panel->ForceLayoutPrepass();
    }

    // Refresh all child widgets that support the screen interface
    for (const auto& Pair : LayoutWidgets)
    {
        if (Pair.Value && Pair.Value->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
        {
            ISuspenseScreen::Execute_RefreshScreenContent(Pair.Value);
        }
    }

    K2_OnLayoutRefreshed();
}

void USuspenseBaseLayoutWidget::InitializeFromConfig()
{
    CreateConfiguredWidgets();
}

UUserWidget* USuspenseBaseLayoutWidget::GetWidgetByTag(FGameplayTag InWidgetTag) const
{
    if (!InWidgetTag.IsValid())
    {
        return nullptr;
    }

    if (UUserWidget* const* WidgetPtr = LayoutWidgets.Find(InWidgetTag))
    {
        return *WidgetPtr;
    }
    return nullptr;
}

UUserWidget* USuspenseBaseLayoutWidget::CreateWidgetByTag(FGameplayTag InWidgetTag)
{
    if (!InWidgetTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] CreateWidgetByTag: Invalid tag"), *GetName());
        return nullptr;
    }

    // Check if already exists
    if (LayoutWidgets.Contains(InWidgetTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] CreateWidgetByTag: Widget with tag %s already exists"),
            *GetName(), *InWidgetTag.ToString());
        return LayoutWidgets[InWidgetTag];
    }

    // Find configuration
    const FLayoutWidgetConfig* Config = FindConfigByTag(InWidgetTag);
    if (!Config)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] CreateWidgetByTag: No configuration found for tag %s"),
            *GetName(), *InWidgetTag.ToString());
        return nullptr;
    }

    // Create widget
    UUserWidget* NewWidget = CreateLayoutWidget(*Config);
    if (!NewWidget)
    {
        return nullptr;
    }

    // Add to layout using the configured tag
    if (!AddWidgetToPanel(NewWidget, Config))
    {
        NewWidget->RemoveFromParent();
        UE_LOG(LogTemp, Error, TEXT("[%s] CreateWidgetByTag: Failed to add widget to panel"), *GetName());
        return nullptr;
    }

    // Store in map
    LayoutWidgets.Add(InWidgetTag, NewWidget);

    // Register in UIManager if needed
    if (Config->bRegisterInUIManager)
    {
        RegisterWidgetInUIManager(NewWidget, InWidgetTag);
    }

    // Notify about creation
    NotifyWidgetCreated(NewWidget, InWidgetTag);
    K2_OnWidgetAdded(NewWidget, InWidgetTag);

    UE_LOG(LogTemp, Log, TEXT("[%s] Created widget on demand: %s with tag %s"),
        *GetName(), *Config->WidgetClass->GetName(), *InWidgetTag.ToString());

    return NewWidget;
}

bool USuspenseBaseLayoutWidget::HasWidget(FGameplayTag InWidgetTag) const
{
    return InWidgetTag.IsValid() && LayoutWidgets.Contains(InWidgetTag);
}

const FLayoutWidgetConfig* USuspenseBaseLayoutWidget::GetWidgetConfig(FGameplayTag InWidgetTag) const
{
    return FindConfigByTag(InWidgetTag);
}

bool USuspenseBaseLayoutWidget::ValidateConfiguration() const
{
    return ValidateConfigurationInternal();
}

TArray<FGameplayTag> USuspenseBaseLayoutWidget::GetAllWidgetTags() const
{
    TArray<FGameplayTag> Tags;
    LayoutWidgets.GetKeys(Tags);
    return Tags;
}

UUserWidget* USuspenseBaseLayoutWidget::CreateLayoutWidget(const FLayoutWidgetConfig& Config)
{
    if (!Config.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] CreateLayoutWidget: Invalid configuration"), *GetName());
        return nullptr;
    }

    UUserWidget* NewWidget = CreateWidget<UUserWidget>(this, Config.WidgetClass);
    if (NewWidget)
    {
        InitializeLayoutWidget(NewWidget, Config);
    }

    return NewWidget;
}

void USuspenseBaseLayoutWidget::InitializeLayoutWidget(UUserWidget* Widget, const FLayoutWidgetConfig& Config)
{
    if (!Widget)
    {
        return;
    }

    // Устанавливаем тег виджета
    if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
    {
        ISuspenseUIWidget::Execute_SetWidgetTag(Widget, Config.WidgetTag);

        if (Config.bAutoInitialize)
        {
            ISuspenseUIWidget::Execute_InitializeWidget(Widget);
        }
    }

    // ИСПРАВЛЕНИЕ: Отправляем событие готовности для обоих типов виджетов
    if (USuspenseCoreEventManager* EventManager = GetEventManager())
    {
        FTimerHandle InitTimerHandle;
        GetWorld()->GetTimerManager().SetTimerForNextTick([EventManager, Widget, Config, this]()
        {
            // Для инвентаря
            if (Config.WidgetTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Inventory"))))
            {
                FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.ReadyToDisplay"));
                EventManager->NotifyUIEventGeneric(Widget, ReadyTag, TEXT(""));
            }

            // НОВОЕ: Для экипировки тоже отправляем событие готовности
            if (Config.WidgetTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Equipment"))))
            {
                FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Equipment.ReadyToDisplay"));
                EventManager->NotifyUIEventGeneric(Widget, ReadyTag, TEXT(""));

                UE_LOG(LogTemp, Log, TEXT("[Layout] Equipment widget ready for display"));
            }
        });
    }
}

void USuspenseBaseLayoutWidget::CreateConfiguredWidgets()
{
    for (const FLayoutWidgetConfig& Config : WidgetConfigurations)
    {
        // Skip invalid configs
        if (!Config.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] Skipping invalid configuration"), *GetName());
            continue;
        }

        // Skip if not marked for immediate creation
        if (!Config.bCreateImmediately)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[%s] Skipping widget %s - not marked for immediate creation"),
                *GetName(), *Config.WidgetTag.ToString());
            continue;
        }

        // Skip if already exists
        if (LayoutWidgets.Contains(Config.WidgetTag))
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] Widget with tag %s already exists"),
                *GetName(), *Config.WidgetTag.ToString());
            continue;
        }

        // Create widget
        if (UUserWidget* NewWidget = CreateLayoutWidget(Config))
        {
            if (AddWidgetToPanel(NewWidget, &Config))
            {
                LayoutWidgets.Add(Config.WidgetTag, NewWidget);

                // Register in UIManager if needed
                if (Config.bRegisterInUIManager)
                {
                    RegisterWidgetInUIManager(NewWidget, Config.WidgetTag);
                }

                // Notify about creation
                NotifyWidgetCreated(NewWidget, Config.WidgetTag);

                UE_LOG(LogTemp, Log, TEXT("[%s] Created widget %s with tag %s"),
                    *GetName(),
                    *Config.WidgetClass->GetName(),
                    *Config.WidgetTag.ToString());
            }
            else
            {
                NewWidget->RemoveFromParent();
                UE_LOG(LogTemp, Error, TEXT("[%s] Failed to add widget to panel"), *GetName());
            }
        }
    }
}

void USuspenseBaseLayoutWidget::ClearCreatedWidgets()
{
    for (const auto& Pair : LayoutWidgets)
    {
        if (Pair.Value)
        {
            // Check if widget should be unregistered from UIManager
            const FLayoutWidgetConfig* Config = FindConfigByTag(Pair.Key);
            if (Config && Config->bRegisterInUIManager)
            {
                UnregisterWidgetFromUIManager(Pair.Key);
            }

            // Uninitialize if supported
            if (Pair.Value->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
            {
                ISuspenseUIWidget::Execute_UninitializeWidget(Pair.Value);
            }

            // Notify about destruction
            NotifyWidgetDestroyed(Pair.Key);

            Pair.Value->RemoveFromParent();
        }
    }

    LayoutWidgets.Empty();
}

bool USuspenseBaseLayoutWidget::ValidateConfigurationInternal() const
{
    bool bIsValid = true;

    // Check for duplicate tags
    if (HasDuplicateTags())
    {
        bIsValid = false;
    }

    // Validate each configuration
    for (const FLayoutWidgetConfig& Config : WidgetConfigurations)
    {
        if (!Config.WidgetClass)
        {
            // Blueprint events cannot be called from const methods
            // So just log the error
            UE_LOG(LogTemp, Error, TEXT("[%s] Configuration validation failed: WidgetClass is null for tag %s"),
                *GetName(), *Config.WidgetTag.ToString());
            bIsValid = false;
        }

        if (!Config.WidgetTag.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[%s] Configuration validation failed: WidgetTag is invalid"),
                *GetName());
            bIsValid = false;
        }

        if (Config.SizeWeight < 0.0f)
        {
            UE_LOG(LogTemp, Error, TEXT("[%s] Configuration validation failed: SizeWeight is negative for tag %s"),
                *GetName(), *Config.WidgetTag.ToString());
            bIsValid = false;
        }
    }

    return bIsValid;
}

bool USuspenseBaseLayoutWidget::HasDuplicateTags() const
{
    TSet<FGameplayTag> SeenTags;
    bool bHasDuplicates = false;

    for (const FLayoutWidgetConfig& Config : WidgetConfigurations)
    {
        if (!Config.WidgetTag.IsValid())
        {
            continue;
        }

        if (SeenTags.Contains(Config.WidgetTag))
        {
            UE_LOG(LogTemp, Error, TEXT("[%s] Duplicate WidgetTag found: %s"),
                *GetName(), *Config.WidgetTag.ToString());
            bHasDuplicates = true;
        }
        else
        {
            SeenTags.Add(Config.WidgetTag);
        }
    }

    return bHasDuplicates;
}

const FLayoutWidgetConfig* USuspenseBaseLayoutWidget::FindConfigByTag(FGameplayTag InTag) const
{
    if (!InTag.IsValid())
    {
        return nullptr;
    }

    return WidgetConfigurations.FindByPredicate([&InTag](const FLayoutWidgetConfig& Config)
    {
        return Config.WidgetTag.MatchesTagExact(InTag);
    });
}

void USuspenseBaseLayoutWidget::RegisterWidgetInUIManager(UUserWidget* Widget, const FGameplayTag& InWidgetTag)
{
    if (!Widget || !InWidgetTag.IsValid())
    {
        return;
    }

    if (USuspenseUIManager* UIManager = GetUIManager())
    {
        UIManager->RegisterLayoutWidget(Widget, InWidgetTag, this);
        UE_LOG(LogTemp, Log, TEXT("[%s] Registered widget %s in UIManager"),
            *GetName(), *InWidgetTag.ToString());
    }
}

void USuspenseBaseLayoutWidget::UnregisterWidgetFromUIManager(const FGameplayTag& InWidgetTag)
{
    if (!InWidgetTag.IsValid())
    {
        return;
    }

    if (USuspenseUIManager* UIManager = GetUIManager())
    {
        UIManager->UnregisterLayoutWidget(InWidgetTag);
        UE_LOG(LogTemp, Log, TEXT("[%s] Unregistered widget %s from UIManager"),
            *GetName(), *InWidgetTag.ToString());
    }
}

void USuspenseBaseLayoutWidget::NotifyWidgetCreated(UUserWidget* Widget, const FGameplayTag& InWidgetTag)
{
    if (USuspenseCoreEventManager* EventManager = GetEventManager())
    {
        // Create event data string
        FString EventData = FString::Printf(TEXT("Widget:%s,Tag:%s,Parent:%s"),
            *Widget->GetName(),
            *InWidgetTag.ToString(),
            *GetName());

        // Send creation event
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Layout.WidgetCreated"));
        EventManager->NotifyUIEvent(Widget, EventTag, EventData);
    }
}

void USuspenseBaseLayoutWidget::NotifyWidgetDestroyed(const FGameplayTag& InWidgetTag)
{
    if (USuspenseCoreEventManager* EventManager = GetEventManager())
    {
        // Create event data string
        FString EventData = FString::Printf(TEXT("Tag:%s"), *InWidgetTag.ToString());

        // Send destruction event
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Layout.WidgetDestroyed"));
        EventManager->NotifyUIEvent(this, EventTag, EventData);
    }
}

USuspenseUIManager* USuspenseBaseLayoutWidget::GetUIManager() const
{
    return USuspenseUIManager::Get(this);
}

USuspenseCoreEventManager* USuspenseBaseLayoutWidget::GetEventManager() const
{
    if (USuspenseCoreEventManager* EventManager = GetDelegateManager())
    {
        return EventManager;
    }

    // Fallback: try to get from game instance
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            return GameInstance->GetSubsystem<USuspenseCoreEventManager>();
        }
    }

    return nullptr;
}
