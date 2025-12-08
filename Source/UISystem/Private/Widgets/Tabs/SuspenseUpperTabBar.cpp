// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Tabs/SuspenseUpperTabBar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "SuspenseCore/Delegates/SuspenseCoreEventManager.h"
#include "Widgets/Equipment/SuspenseEquipmentContainerWidget.h"
#include "Widgets/Layout/SuspenseBaseLayoutWidget.h"
#include "Blueprint/WidgetTree.h"
#include "SuspenseCore/Interfaces/Screens/ISuspenseCoreScreen.h"
#include "Framework/Application/SlateApplication.h"

USuspenseUpperTabBar::USuspenseUpperTabBar(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // ВАЖНО: В конструкторе только инициализация простых переменных
    CurrentTabIndex = -1;
    DefaultTabIndex = 0;

    // Default tag
    TabBarTag = FGameplayTag::RequestGameplayTag(TEXT("UI.TabBar.Character"));

    // Default text colors
    NormalTabTextColor = FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
    SelectedTabTextColor = FSlateColor(FLinearColor::White);
}

void USuspenseUpperTabBar::NativePreConstruct()
{
    Super::NativePreConstruct();

    // Preview in designer
    if (IsDesignTime() && TabButtonContainer)
    {
        TabButtonContainer->ClearChildren();

        for (int32 i = 0; i < TabConfigs.Num(); i++)
        {
            UButton* PreviewButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
            if (PreviewButton)
            {
                UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
                if (ButtonText)
                {
                    ButtonText->SetText(TabConfigs[i].TabName);
                    ButtonText->SetColorAndOpacity(NormalTabTextColor);
                    PreviewButton->AddChild(ButtonText);
                }

                PreviewButton->SetStyle(NormalTabStyle);

                UHorizontalBoxSlot* LocalSlot = TabButtonContainer->AddChildToHorizontalBox(PreviewButton);
                if (LocalSlot)
                {
                    LocalSlot->SetPadding(FMargin(2.0f, 0.0f));
                    LocalSlot->SetHorizontalAlignment(HAlign_Left);
                }
            }
        }
    }
}

void USuspenseUpperTabBar::InitializeWidget_Implementation()
{
    // ВАЖНО: Сначала вызываем родительский метод
    Super::InitializeWidget_Implementation();

    // Проверяем, что мы не в режиме дизайнера
    if (IsDesignTime())
    {
        return;
    }

    // Валидация обязательных виджетов
    if (!TabButtonContainer || !ContentSwitcher)
    {
        UE_LOG(LogTemp, Error, TEXT("[UpperTabBar] Required widgets not bound!"));
        return;
    }

    // Очищаем контейнеры от дизайн-тайм контента
    TabButtonContainer->ClearChildren();
    ContentSwitcher->ClearChildren();

    // Создаем виджеты контента
    ContentWidgets.SetNum(TabConfigs.Num());

    for (int32 i = 0; i < TabConfigs.Num(); i++)
    {
        ContentWidgets[i] = CreateTabContent(i);
    }

    // Создаем кнопки вкладок
    CreateTabButtons();

    // Привязываем кнопку закрытия
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &USuspenseUpperTabBar::OnCloseButtonClicked);
    }

    // Подписываемся на события
    SubscribeToEvents();

    // ВАЖНО: Выбираем первую вкладку ПОСЛЕ создания всех кнопок
    // Это установит правильное визуальное состояние
    if (TabButtons.Num() > 0 && TabConfigs.Num() > 0)
    {
        // Сначала устанавливаем визуальное состояние всех кнопок в "не выбрано"
        for (int32 i = 0; i < TabButtons.Num(); i++)
        {
            if (TabButtons[i])
            {
                ApplyButtonStyle(TabButtons[i], false);
            }
        }

        // Затем выбираем вкладку по умолчанию
        int32 IndexToSelect = FMath::Clamp(DefaultTabIndex, 0, TabButtons.Num() - 1);
        SelectTabByIndex_Implementation(IndexToSelect);
    }

    // Инициализируем отображение уровня персонажа
    UpdateCharacterLevel(1, 0.0f, 100.0f);

    UE_LOG(LogTemp, Log, TEXT("[UpperTabBar] Initialized with %d tab configs, selected tab: %d"),
        TabConfigs.Num(), CurrentTabIndex);
}

void USuspenseUpperTabBar::UninitializeWidget_Implementation()
{
    // Отписываемся от событий
    UnsubscribeFromEvents();

    // Очищаем делегаты кнопок
    for (int32 i = 0; i < TabButtons.Num(); i++)
    {
        if (TabButtons[i])
        {
            TabButtons[i]->OnClicked.RemoveAll(this);
        }
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.RemoveDynamic(this, &USuspenseUpperTabBar::OnCloseButtonClicked);
    }

    // Деактивируем текущий экран если есть
    if (CurrentTabIndex >= 0 && ContentWidgets.IsValidIndex(CurrentTabIndex))
    {
        if (UUserWidget* Content = ContentWidgets[CurrentTabIndex])
        {
            // Для layout виджетов особая обработка
            if (USuspenseBaseLayoutWidget* LayoutWidget = Cast<USuspenseBaseLayoutWidget>(Content))
            {
                LayoutWidget->ClearLayout_Implementation();
            }
            else if (Content->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
            {
                ISuspenseScreen::Execute_OnScreenDeactivated(Content);
            }
        }
    }

    // Очищаем массивы
    TabButtons.Empty();
    ContentWidgets.Empty();

    // Вызываем родительский метод
    Super::UninitializeWidget_Implementation();
}

int32 USuspenseUpperTabBar::GetTabCount_Implementation() const
{
    return TabConfigs.Num();
}

int32 USuspenseUpperTabBar::GetSelectedTabIndex_Implementation() const
{
    return CurrentTabIndex;
}

bool USuspenseUpperTabBar::SelectTabByIndex_Implementation(int32 TabIndex)
{
    if (TabIndex < 0 || TabIndex >= TabButtons.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("[UpperTabBar] Invalid tab index: %d"), TabIndex);
        return false;
    }

    // Проверяем, включена ли вкладка
    if (TabButtons[TabIndex] && !TabButtons[TabIndex]->GetIsEnabled())
    {
        UE_LOG(LogTemp, Warning, TEXT("[UpperTabBar] Tab %d is disabled"), TabIndex);
        return false;
    }

    int32 OldIndex = CurrentTabIndex;

    // Деактивируем старый контент (даже если это та же вкладка)
    if (OldIndex >= 0 && ContentWidgets.IsValidIndex(OldIndex))
    {
        if (UUserWidget* OldContent = ContentWidgets[OldIndex])
        {
            // Для layout виджетов особая обработка
            if (USuspenseBaseLayoutWidget* OldLayoutWidget = Cast<USuspenseBaseLayoutWidget>(OldContent))
            {
                // Layout виджеты обрабатывают деактивацию дочерних элементов самостоятельно
                for (UUserWidget* ChildWidget : OldLayoutWidget->GetLayoutWidgets_Implementation())
                {
                    if (ChildWidget && ChildWidget->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
                    {
                        ISuspenseScreen::Execute_OnScreenDeactivated(ChildWidget);
                    }
                }
            }
            else if (OldContent->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
            {
                ISuspenseScreen::Execute_OnScreenDeactivated(OldContent);
            }
        }
    }

    CurrentTabIndex = TabIndex;

    // Обновляем визуальное состояние
    UpdateTabVisuals();

    // Переключаем контент
    if (ContentWidgets.IsValidIndex(TabIndex) && ContentWidgets[TabIndex])
    {
        ContentSwitcher->SetActiveWidget(ContentWidgets[TabIndex]);

        // Проверяем, является ли это layout виджетом
        if (USuspenseBaseLayoutWidget* LayoutWidget = Cast<USuspenseBaseLayoutWidget>(ContentWidgets[TabIndex]))
        {
            // Обновляем все дочерние виджеты в layout
            LayoutWidget->RefreshLayout_Implementation();

            // Активируем все дочерние виджеты
            for (UUserWidget* ChildWidget : LayoutWidget->GetLayoutWidgets_Implementation())
            {
                if (ChildWidget && ChildWidget->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
                {
                    ISuspenseScreen::Execute_OnScreenActivated(ChildWidget);
                }
            }

            UE_LOG(LogTemp, Log, TEXT("[UpperTabBar] Activated layout widget with %d children"),
                LayoutWidget->GetLayoutWidgets_Implementation().Num());
        }
        else
        {
            // Обычная активация для одиночных виджетов
            if (ContentWidgets[TabIndex]->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
            {
                ISuspenseScreen::Execute_OnScreenActivated(ContentWidgets[TabIndex]);
            }
        }

        // КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: Отправляем событие обновления для конкретной вкладки
        if (TabConfigs.IsValidIndex(TabIndex))
        {
            const FSuspenseTabConfig& Config = TabConfigs[TabIndex];

            // Если это вкладка инвентаря, отправляем специальное событие
            if (Config.TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"))))
            {
                // Даем время виджету полностью активироваться
                FTimerHandle RefreshTimerHandle;
                GetWorld()->GetTimerManager().SetTimerForNextTick([this, TabIndex]()
                {
                    if (USuspenseEventManager* EventManager = GetDelegateManager())
                    {
                        // Отправляем событие обновления инвентаря
                        FGameplayTag UpdateTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.Updated"));
                        EventManager->NotifyUIEventGeneric(this, UpdateTag, TEXT("TabSelected"));

                        // Также отправляем запрос на обновление UI контейнера
                        FGameplayTag ContainerTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Container.Inventory"));
                        EventManager->NotifyUIContainerUpdateRequested(ContentWidgets[TabIndex], ContainerTag);

                        UE_LOG(LogTemp, Log, TEXT("[UpperTabBar] Sent inventory update request for tab selection"));
                    }

                    // Принудительно обновляем контент
                    if (ContentWidgets[TabIndex])
                    {
                        if (USuspenseBaseLayoutWidget* LayoutWidget = Cast<USuspenseBaseLayoutWidget>(ContentWidgets[TabIndex]))
                        {
                            LayoutWidget->RefreshLayout_Implementation();
                        }
                        else if (ContentWidgets[TabIndex]->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
                        {
                            ISuspenseScreen::Execute_RefreshScreenContent(ContentWidgets[TabIndex]);
                        }
                    }
                });
            }
        }
    }

    // Уведомляем об изменении только если индекс действительно изменился
    if (OldIndex != CurrentTabIndex)
    {
        K2_OnTabChanged(OldIndex, CurrentTabIndex);
        BroadcastTabSelectionChanged(OldIndex, CurrentTabIndex);
    }

    return true;
}

bool USuspenseUpperTabBar::SelectTabByTag(FGameplayTag TabTag)
{
    // Ищем индекс вкладки по тегу
    for (int32 i = 0; i < TabConfigs.Num(); i++)
    {
        if (TabConfigs[i].TabTag.MatchesTagExact(TabTag))
        {
            return SelectTabByIndex_Implementation(i);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[UpperTabBar] Tab with tag %s not found"), *TabTag.ToString());
    return false;
}

UUserWidget* USuspenseUpperTabBar::GetTabContent_Implementation(int32 TabIndex) const
{
    if (ContentWidgets.IsValidIndex(TabIndex))
    {
        return ContentWidgets[TabIndex];
    }
    return nullptr;
}

void USuspenseUpperTabBar::SetTabEnabled_Implementation(int32 TabIndex, bool bEnabled)
{
    if (TabButtons.IsValidIndex(TabIndex) && TabButtons[TabIndex])
    {
        TabButtons[TabIndex]->SetIsEnabled(bEnabled);

        // Update visual state
        if (!bEnabled && CurrentTabIndex == TabIndex)
        {
            // If disabling current tab, select another
            for (int32 i = 0; i < TabButtons.Num(); i++)
            {
                if (i != TabIndex && TabButtons[i] && TabButtons[i]->GetIsEnabled())
                {
                    SelectTabByIndex_Implementation(i);
                    break;
                }
            }
        }
    }
}

bool USuspenseUpperTabBar::IsTabEnabled_Implementation(int32 TabIndex) const
{
    if (TabButtons.IsValidIndex(TabIndex) && TabButtons[TabIndex])
    {
        return TabButtons[TabIndex]->GetIsEnabled();
    }
    return false;
}

void USuspenseUpperTabBar::UpdateCharacterLevel(int32 Level, float Experience, float MaxExperience)
{
    // Update level text
    if (LevelText)
    {
        LevelText->SetText(FText::Format(NSLOCTEXT("UI", "LevelFormat", "LEVEL {0}"), FText::AsNumber(Level)));
    }

    // Update experience bar
    if (ExperienceBar && MaxExperience > 0)
    {
        float Progress = FMath::Clamp(Experience / MaxExperience, 0.0f, 1.0f);
        ExperienceBar->SetPercent(Progress);
    }
}

void USuspenseUpperTabBar::RefreshActiveTabContent()
{
    if (CurrentTabIndex >= 0 && ContentWidgets.IsValidIndex(CurrentTabIndex))
    {
        if (UUserWidget* Content = ContentWidgets[CurrentTabIndex])
        {
            // Для layout виджетов
            if (USuspenseBaseLayoutWidget* LayoutWidget = Cast<USuspenseBaseLayoutWidget>(Content))
            {
                LayoutWidget->RefreshLayout_Implementation();
            }
            else if (Content->GetClass()->ImplementsInterface(USuspenseScreen::StaticClass()))
            {
                ISuspenseScreen::Execute_RefreshScreenContent(Content);
            }
        }
    }
}

FSuspenseTabConfig USuspenseUpperTabBar::GetTabConfig(int32 TabIndex) const
{
    if (TabConfigs.IsValidIndex(TabIndex))
    {
        return TabConfigs[TabIndex];
    }
    return FSuspenseTabConfig();
}

USuspenseBaseLayoutWidget* USuspenseUpperTabBar::GetTabLayoutWidget(int32 TabIndex) const
{
    if (ContentWidgets.IsValidIndex(TabIndex))
    {
        return Cast<USuspenseBaseLayoutWidget>(ContentWidgets[TabIndex]);
    }
    return nullptr;
}

void USuspenseUpperTabBar::CreateTabButtons()
{
    TabButtons.Empty();

    for (int32 i = 0; i < TabConfigs.Num(); i++)
    {
        UButton* TabButton = CreateTabButton(TabConfigs[i], i);
        if (TabButton)
        {
            TabButtons.Add(TabButton);

            // Add to container
            UHorizontalBoxSlot* LocalSlot = TabButtonContainer->AddChildToHorizontalBox(TabButton);
            if (LocalSlot)
            {
                LocalSlot->SetPadding(FMargin(2.0f, 0.0f));
                LocalSlot->SetHorizontalAlignment(HAlign_Left);
                LocalSlot->SetVerticalAlignment(VAlign_Fill);
            }
        }
    }
}

UUserWidget* USuspenseUpperTabBar::CreateTabContent(int32 TabIndex)
{
    if (!TabConfigs.IsValidIndex(TabIndex))
    {
        return nullptr;
    }

    const FSuspenseTabConfig& Config = TabConfigs[TabIndex];
    UUserWidget* ContentWidget = nullptr;

    switch (Config.LayoutType)
    {
        case ETabContentLayoutType::Single:
            ContentWidget = CreateSingleWidgetContent(Config);
            break;

        case ETabContentLayoutType::Layout:
            ContentWidget = CreateLayoutWidgetContent(Config);
            break;

        case ETabContentLayoutType::Custom:
            // Custom тип использует ContentWidgetClass как есть
            ContentWidget = CreateSingleWidgetContent(Config);
            break;
    }

    if (ContentWidget)
    {
        // Добавляем в switcher
        ContentSwitcher->AddChild(ContentWidget);

        // Убеждаемся, что виджет полностью сконструирован
        ContentWidget->ForceLayoutPrepass();

        UE_LOG(LogTemp, Log, TEXT("[TabBar] Created content for tab %d (%s) - Type: %s"),
            TabIndex,
            *Config.TabTag.ToString(),
            *UEnum::GetValueAsString(Config.LayoutType));
    }

    return ContentWidget;
}

UUserWidget* USuspenseUpperTabBar::CreateSingleWidgetContent(const FSuspenseTabConfig& Config)
{
    if (!Config.ContentWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[TabBar] No ContentWidgetClass specified for single widget tab"));
        return nullptr;
    }

    // Создаем виджет
    UUserWidget* Widget = CreateWidget<UUserWidget>(this, Config.ContentWidgetClass);

    if (Widget)
    {
        // Инициализируем виджет
        if (Widget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
        {
            ISuspenseUIWidget::Execute_InitializeWidget(Widget);
        }

        // Специальная инициализация для Equipment Widget
        if (Config.TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Equipment"))))
        {
            if (USuspenseEquipmentContainerWidget* EquipWidget = Cast<USuspenseEquipmentContainerWidget>(Widget))
            {
                UE_LOG(LogTemp, Log, TEXT("[TabBar] Equipment Widget created, will be initialized by bridge"));

                // Отправляем событие готовности Equipment виджета
                if (USuspenseEventManager* EventManager = GetDelegateManager())
                {
                    FTimerHandle InitTimerHandle;
                    GetWorld()->GetTimerManager().SetTimerForNextTick([EventManager, Widget]()
                    {
                        FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Equipment.ReadyToDisplay"));
                        EventManager->NotifyUIEventGeneric(Widget, ReadyTag, TEXT(""));
                    });
                }
            }
        }
    }

    return Widget;
}

UUserWidget* USuspenseUpperTabBar::CreateLayoutWidgetContent(const FSuspenseTabConfig& Config)
{
    if (!Config.LayoutWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[TabBar] No LayoutWidgetClass specified for layout tab"));
        return nullptr;
    }

    // Создаем layout widget
    USuspenseBaseLayoutWidget* LayoutWidget = CreateWidget<USuspenseBaseLayoutWidget>(this, Config.LayoutWidgetClass);

    if (LayoutWidget)
    {
        // Инициализируем layout
        if (LayoutWidget->GetClass()->ImplementsInterface(USuspenseUIWidget::StaticClass()))
        {
            ISuspenseUIWidget::Execute_InitializeWidget(LayoutWidget);
        }

        UE_LOG(LogTemp, Log, TEXT("[TabBar] Created layout widget with %d child widgets"),
            LayoutWidget->GetLayoutWidgets_Implementation().Num());
    }

    return LayoutWidget;
}

void USuspenseUpperTabBar::UpdateTabVisuals()
{
    for (int32 i = 0; i < TabButtons.Num(); i++)
    {
        if (TabButtons[i])
        {
            bool bSelected = (i == CurrentTabIndex);
            ApplyButtonStyle(TabButtons[i], bSelected);
        }
    }
}

void USuspenseUpperTabBar::OnTabButtonClicked(int32 TabIndex)
{
    SelectTabByIndex_Implementation(TabIndex);
}

void USuspenseUpperTabBar::OnCloseButtonClicked()
{
    K2_OnCloseClicked();

    // Broadcast close event
    OnTabBarClosed.Broadcast(this);

    // Notify event system
    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.TabBar.CloseClicked"));
        EventManager->NotifyUIEventGeneric(this, EventTag, TabBarTag.ToString());
    }
}

UButton* USuspenseUpperTabBar::CreateTabButton(const FSuspenseTabConfig& Config, int32 Index)
{
    // Создаём кнопку
    UButton* Button = WidgetTree->ConstructWidget<UButton>(TabButtonClass.Get() ? TabButtonClass.Get() : UButton::StaticClass());
    if (!Button)
    {
        return nullptr;
    }

    // Создаём overlay для иконки и текста
    UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    if (!Overlay)
    {
        return Button;
    }

    // Добавляем иконку, если указана
    if (Config.TabIcon)
    {
        UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (IconImage)
        {
            IconImage->SetBrushFromTexture(Config.TabIcon);
            IconImage->SetDesiredSizeOverride(FVector2D(24.0f, 24.0f));

            UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(IconImage);
            if (IconSlot)
            {
                IconSlot->SetHorizontalAlignment(HAlign_Center);
                IconSlot->SetVerticalAlignment(VAlign_Center);
            }
        }
    }

    // Добавляем текст
    UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    if (TextBlock)
    {
        TextBlock->SetText(Config.TabName);
        TextBlock->SetColorAndOpacity(NormalTabTextColor);

        FSlateFontInfo FontInfo = TextBlock->GetFont();
        FontInfo.Size = 14;
        TextBlock->SetFont(FontInfo);

        UOverlaySlot* TextSlot = Overlay->AddChildToOverlay(TextBlock);
        if (TextSlot)
        {
            TextSlot->SetHorizontalAlignment(HAlign_Center);
            TextSlot->SetVerticalAlignment(VAlign_Center);

            // Если есть иконка, добавляем отступ для текста
            if (Config.TabIcon)
            {
                TextSlot->SetPadding(FMargin(30.0f, 0.0f, 0.0f, 0.0f));
            }
        }
    }

    // Устанавливаем содержимое кнопки
    Button->AddChild(Overlay);

    // Применяем начальный стиль
    ApplyButtonStyle(Button, false);

    // Устанавливаем состояние enabled
    Button->SetIsEnabled(Config.bEnabled);

    // КЛЮЧЕВОЕ РЕШЕНИЕ: Сохраняем соответствие кнопка->индекс в TMap
    ButtonToIndexMap.Add(Button, Index);

    // Привязываем единый обработчик для всех кнопок
    Button->OnClicked.AddDynamic(this, &USuspenseUpperTabBar::InternalOnTabButtonClicked);

    UE_LOG(LogTemp, Log, TEXT("[UpperTabBar] Created tab button %d"), Index);

    return Button;
}

void USuspenseUpperTabBar::ApplyButtonStyle(UButton* Button, bool bSelected)
{
    if (!Button)
    {
        return;
    }

    // Применяем стиль кнопки
    Button->SetStyle(bSelected ? SelectedTabStyle : NormalTabStyle);

    // Обновляем цвет текста
    UWidget* ButtonContent = Button->GetChildAt(0);
    if (!ButtonContent)
    {
        return;
    }

    // Если это Overlay, ищем TextBlock внутри
    if (UOverlay* Overlay = Cast<UOverlay>(ButtonContent))
    {
        for (int32 i = 0; i < Overlay->GetChildrenCount(); i++)
        {
            if (UWidget* Child = Overlay->GetChildAt(i))
            {
                if (UTextBlock* TextBlock = Cast<UTextBlock>(Child))
                {
                    TextBlock->SetColorAndOpacity(bSelected ? SelectedTabTextColor : NormalTabTextColor);
                }
            }
        }
    }
    // Если это напрямую TextBlock
    else if (UTextBlock* TextBlock = Cast<UTextBlock>(ButtonContent))
    {
        TextBlock->SetColorAndOpacity(bSelected ? SelectedTabTextColor : NormalTabTextColor);
    }
}

void USuspenseUpperTabBar::SubscribeToEvents()
{
    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        // ВАЖНОЕ ИЗМЕНЕНИЕ: Подписываемся на правильные события инвентаря

        // Подписка на обновления инвентаря
        InventoryUpdateHandle = EventManager->SubscribeToUIEvent([this, EventManager](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
        {
            // Проверяем различные события инвентаря
            bool bShouldUpdate = false;

            if (EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.Updated"))) ||
                EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemAdded"))) ||
                EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemRemoved"))) ||
                EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemMoved"))))
            {
                bShouldUpdate = true;
            }

            if (bShouldUpdate)
            {
                // Обновляем вкладку инвентаря, если она активна
                if (CurrentTabIndex >= 0 && TabConfigs.IsValidIndex(CurrentTabIndex))
                {
                    if (TabConfigs[CurrentTabIndex].TabTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory"))))
                    {
                        UE_LOG(LogTemp, Log, TEXT("[UpperTabBar] Inventory event received: %s, refreshing content"),
                             *EventTag.ToString());

                        // Обновляем контент вкладки
                        RefreshActiveTabContent();

                        // Также отправляем событие обновления контейнера
                        if (ContentWidgets.IsValidIndex(CurrentTabIndex) && ContentWidgets[CurrentTabIndex])
                        {
                            FGameplayTag ContainerTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Container.Inventory"));
                            EventManager->NotifyUIContainerUpdateRequested(ContentWidgets[CurrentTabIndex], ContainerTag);
                        }
                    }
                }
            }
        });

        // Подписка на обновления уровня персонажа
        CharacterLevelUpdateHandle = EventManager->SubscribeToUIEvent([this](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
        {
            if (EventTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("UI.Character.LevelUpdated"))))
            {
                OnCharacterDataUpdated();
            }
        });
    }
}

void USuspenseUpperTabBar::UnsubscribeFromEvents()
{
    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        EventManager->UniversalUnsubscribe(CharacterLevelUpdateHandle);
        EventManager->UniversalUnsubscribe(InventoryUpdateHandle);
    }
}

void USuspenseUpperTabBar::BroadcastTabSelectionChanged(int32 OldIndex, int32 NewIndex)
{
    // Broadcast native delegate
    OnTabSelectionChanged.Broadcast(this, OldIndex, NewIndex);

    // Notify event system with detailed info
    if (USuspenseEventManager* EventManager = GetDelegateManager())
    {
        FGameplayTag OldTag = TabConfigs.IsValidIndex(OldIndex) ? TabConfigs[OldIndex].TabTag : FGameplayTag();
        FGameplayTag NewTag = TabConfigs.IsValidIndex(NewIndex) ? TabConfigs[NewIndex].TabTag : FGameplayTag();

        // Уведомляем через EventManager
        EventManager->NotifyTabSelectionChanged(this, OldTag, NewTag);
    }
}

void USuspenseUpperTabBar::OnCharacterDataUpdated()
{
    // Здесь должно быть подключение к системе данных персонажа
    // Пока просто логируем
    UE_LOG(LogTemp, Log, TEXT("[UpperTabBar] Character data updated"));
}

void USuspenseUpperTabBar::OnScreenActivated(int32 TabIndex)
{
    // Обработка активации экрана
    if (TabConfigs.IsValidIndex(TabIndex) && ContentWidgets.IsValidIndex(TabIndex))
    {
        UE_LOG(LogTemp, Log, TEXT("[UpperTabBar] Screen activated for tab %d"), TabIndex);
    }
}

void USuspenseUpperTabBar::InternalOnTabButtonClicked()
{
    // ИСПРАВЛЕНИЕ: Используем более надежный способ определения нажатой кнопки

    // Сначала пытаемся найти кнопку через FSlateApplication
    if (FSlateApplication::IsInitialized())
    {
        // Получаем виджет, который в данный момент имеет фокус мыши
        TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0);

        // Проходим по всем кнопкам и проверяем их Slate-виджеты
        for (const auto& Pair : ButtonToIndexMap)
        {
            UButton* Button = Pair.Key;
            int32 TabIndex = Pair.Value;

            if (!Button)
            {
                continue;
            }

            // Получаем Slate-виджет кнопки
            TSharedPtr<SWidget> ButtonSlateWidget = Button->GetCachedWidget();

            // Проверяем, совпадает ли это с фокусированным виджетом
            if (ButtonSlateWidget.IsValid() && FocusedWidget.IsValid())
            {
                // Проверяем, является ли фокусированный виджет нашей кнопкой или её дочерним элементом
                TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
                while (CurrentWidget.IsValid())
                {
                    if (CurrentWidget == ButtonSlateWidget)
                    {
                        UE_LOG(LogTemp, Verbose, TEXT("[UpperTabBar] Tab button clicked - Index: %d (found via focus)"), TabIndex);
                        OnTabButtonClicked(TabIndex);
                        return;
                    }
                    CurrentWidget = CurrentWidget->GetParentWidget();
                }
            }
        }
    }

    // Запасной вариант: используем IsPressed() с задержкой
    UE_LOG(LogTemp, Warning, TEXT("[UpperTabBar] Could not determine clicked button via focus, using IsPressed fallback"));

    // Даем небольшую задержку для обновления состояния кнопки
    if (UWorld* World = GetWorld())
    {
        FTimerHandle TimerHandle;
        World->GetTimerManager().SetTimerForNextTick([this]()
        {
            for (const auto& Pair : ButtonToIndexMap)
            {
                UButton* Button = Pair.Key;
                int32 TabIndex = Pair.Value;

                if (Button && Button->IsPressed())
                {
                    UE_LOG(LogTemp, Verbose, TEXT("[UpperTabBar] Tab button clicked - Index: %d (via IsPressed)"), TabIndex);
                    OnTabButtonClicked(TabIndex);
                    break;
                }
            }
        });
    }
}

FGameplayTag USuspenseUpperTabBar::GetTabBarTag_Implementation() const
{
    return TabBarTag;
}

FOnTabBarSelectionChanged* USuspenseUpperTabBar::GetOnTabSelectionChanged()
{
    return &OnTabSelectionChanged;
}

FOnTabBarClosed* USuspenseUpperTabBar::GetOnTabBarClosed()
{
    return &OnTabBarClosed;
}

