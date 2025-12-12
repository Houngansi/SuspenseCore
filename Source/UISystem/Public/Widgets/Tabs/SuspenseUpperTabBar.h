// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseWidget.h"
#include "SuspenseCore/Interfaces/Tabs/ISuspenseCoreTabBar.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseUpperTabBar.generated.h"

// Forward declarations
class UButton;
class UTextBlock;
class UWidgetSwitcher;
class UHorizontalBox;
class UProgressBar;
class UBorder;
class UImage;
class UWidgetTree;
class USuspenseBaseLayoutWidget;
class USuspenseCoreEventBus;

/**
 * Layout type for tab content
 */
UENUM(BlueprintType)
enum class ETabContentLayoutType : uint8
{
    /** Single widget - backward compatibility */
    Single UMETA(DisplayName = "Single Widget"),

    /** Multiple widgets with layout */
    Layout UMETA(DisplayName = "Layout Widget"),

    /** Custom composite widget */
    Custom UMETA(DisplayName = "Custom")
};

/**
 * Конфигурация вкладки верхней панели
 * Поддерживает как одиночные виджеты, так и композиции
 */
USTRUCT(BlueprintType)
struct FSuspenseTabConfig
{
    GENERATED_BODY()

    /** Отображаемое название вкладки */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText TabName;

    /** Тег вкладки для идентификации */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag TabTag;

    /** Layout type for this tab */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETabContentLayoutType LayoutType = ETabContentLayoutType::Single;

    /** Класс виджета контента для этой вкладки (для Single и Custom типов) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "LayoutType != ETabContentLayoutType::Layout"))
    TSubclassOf<UUserWidget> ContentWidgetClass;

    /** Layout widget class (for Layout type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "LayoutType == ETabContentLayoutType::Layout"))
    TSubclassOf<USuspenseBaseLayoutWidget> LayoutWidgetClass;

    /** Иконка вкладки (опционально) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* TabIcon = nullptr;

    /** Включена ли вкладка по умолчанию */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled = true;

    FSuspenseTabConfig()
    {
        TabName = FText::FromString("Tab");
        LayoutType = ETabContentLayoutType::Single;
        bEnabled = true;
    }
};

/**
 * Верхняя панель табов для переключения между различными экранами персонажа
 * Поддерживает как одиночные виджеты, так и сложные композиции
 * Обеспечивает обратную совместимость с существующими конфигурациями
 */
UCLASS()
class UISYSTEM_API USuspenseUpperTabBar : public USuspenseBaseWidget, public ISuspenseCoreTabBar
{
    GENERATED_BODY()

public:
    USuspenseUpperTabBar(const FObjectInitializer& ObjectInitializer);

    //=============================================================================
    // ISuspenseCoreTabBarInterface Implementation
    //=============================================================================

    virtual int32 GetTabCount_Implementation() const override;
    virtual bool SelectTabByIndex_Implementation(int32 TabIndex) override;
    virtual int32 GetSelectedTabIndex_Implementation() const override;
    virtual UUserWidget* GetTabContent_Implementation(int32 TabIndex) const override;
    virtual void SetTabEnabled_Implementation(int32 TabIndex, bool bEnabled) override;
    virtual bool IsTabEnabled_Implementation(int32 TabIndex) const override;
    virtual FGameplayTag GetTabBarTag_Implementation() const override;

    // Native delegate getters for C++ binding
    virtual FOnTabBarSelectionChanged* GetOnTabSelectionChanged() override;
    virtual FOnTabBarClosed* GetOnTabBarClosed() override;

    //=============================================================================
    // Основной API табов
    //=============================================================================

    /**
     * Выбрать вкладку по тегу
     * @param TabTag Тег вкладки для выбора
     * @return Успешность операции
     */
    UFUNCTION(BlueprintCallable, Category = "UI|TabBar")
    bool SelectTabByTag(FGameplayTag TabTag);

    /**
     * Получить индекс текущей активной вкладки
     * @return Индекс активной вкладки или -1
     */
    UFUNCTION(BlueprintCallable, Category = "UI|TabBar")
    int32 GetCurrentTabIndex() const { return CurrentTabIndex; }

    /**
     * Get layout widget for tab if it uses layout
     * @param TabIndex Index of tab
     * @return Layout widget or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|TabBar")
    USuspenseBaseLayoutWidget* GetTabLayoutWidget(int32 TabIndex) const;

    //=============================================================================
    // Управление данными персонажа
    //=============================================================================

    /**
     * Обновить отображение уровня персонажа
     * @param Level Текущий уровень
     * @param Experience Текущий опыт
     * @param MaxExperience Максимальный опыт для уровня
     */
    UFUNCTION(BlueprintCallable, Category = "UI|TabBar")
    void UpdateCharacterLevel(int32 Level, float Experience, float MaxExperience);

    /**
     * Обновить контент активной вкладки
     */
    UFUNCTION(BlueprintCallable, Category = "UI|TabBar")
    void RefreshActiveTabContent();

    /**
     * Получить конфигурацию вкладки
     * @param TabIndex Индекс вкладки
     * @return Конфигурация вкладки
     */
    UFUNCTION(BlueprintCallable, Category = "UI|TabBar")
    FSuspenseTabConfig GetTabConfig(int32 TabIndex) const;

    //=============================================================================
    // ISuspenseCoreUIWidgetInterface
    //=============================================================================

    virtual void NativePreConstruct() override;
    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;

    //=============================================================================
    // События Blueprint
    //=============================================================================

    /**
     * Вызывается при изменении выбранной вкладки
     * @param OldTabIndex Предыдущий индекс
     * @param NewTabIndex Новый индекс
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|TabBar", DisplayName = "On Tab Changed")
    void K2_OnTabChanged(int32 OldTabIndex, int32 NewTabIndex);

    /**
     * Вызывается при нажатии кнопки закрытия
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|TabBar", DisplayName = "On Close Clicked")
    void K2_OnCloseClicked();

protected:
    //=============================================================================
    // Конфигурация
    //=============================================================================

    /** Конфигурации всех вкладок */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Config")
    TArray<FSuspenseTabConfig> TabConfigs;

    /** Индекс вкладки по умолчанию */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Config", meta = (ClampMin = "0"))
    int32 DefaultTabIndex = 0;

    /** Тег панели табов для событий */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Config")
    FGameplayTag TabBarTag;

    /** Класс кнопки для табов */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Style")
    TSubclassOf<UButton> TabButtonClass;

    /** Стиль обычной вкладки */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Style")
    FButtonStyle NormalTabStyle;

    /** Стиль выбранной вкладки */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Style")
    FButtonStyle SelectedTabStyle;

    /** Цвет текста обычной вкладки */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Style")
    FSlateColor NormalTabTextColor;

    /** Цвет текста выбранной вкладки */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TabBar|Style")
    FSlateColor SelectedTabTextColor;

    //=============================================================================
    // Привязки виджетов
    //=============================================================================

    /** Контейнер для кнопок вкладок */
    UPROPERTY(BlueprintReadOnly, Category = "TabBar|Widgets", meta = (BindWidget))
    UHorizontalBox* TabButtonContainer;

    /** Переключатель контента вкладок */
    UPROPERTY(BlueprintReadOnly, Category = "TabBar|Widgets", meta = (BindWidget))
    UWidgetSwitcher* ContentSwitcher;

    /** Кнопка закрытия */
    UPROPERTY(BlueprintReadOnly, Category = "TabBar|Widgets", meta = (BindWidgetOptional))
    UButton* CloseButton;

    /** Текст уровня персонажа */
    UPROPERTY(BlueprintReadOnly, Category = "TabBar|Widgets", meta = (BindWidgetOptional))
    UTextBlock* LevelText;

    /** Прогресс-бар опыта */
    UPROPERTY(BlueprintReadOnly, Category = "TabBar|Widgets", meta = (BindWidgetOptional))
    UProgressBar* ExperienceBar;

    /** Аватар персонажа */
    UPROPERTY(BlueprintReadOnly, Category = "TabBar|Widgets", meta = (BindWidgetOptional))
    UImage* CharacterAvatar;

    //=============================================================================
    // Делегаты событий
    //=============================================================================

    /** Вызывается при изменении выбранной вкладки */
    FOnTabBarSelectionChanged OnTabSelectionChanged;

    /** Вызывается при закрытии панели табов */
    FOnTabBarClosed OnTabBarClosed;

private:
    //=============================================================================
    // Внутренние методы
    //=============================================================================

    /** Создать кнопки вкладок */
    void CreateTabButtons();

    /** Создать контент для вкладки с поддержкой композиций */
    UUserWidget* CreateTabContent(int32 TabIndex);

    /** Обновить визуальное состояние табов */
    void UpdateTabVisuals();

    /** Обработчик нажатия на вкладку */
    void OnTabButtonClicked(int32 TabIndex);

    /** Обработчик нажатия кнопки закрытия */
    UFUNCTION()
    void OnCloseButtonClicked();

    /** Создать кнопку вкладки */
    UButton* CreateTabButton(const FSuspenseTabConfig& Config, int32 Index);

    /** Применить стиль к кнопке */
    void ApplyButtonStyle(UButton* Button, bool bSelected);

    /** Подписаться на события системы */
    void SubscribeToEvents();

    /** Отписаться от событий */
    void UnsubscribeFromEvents();

    /** Уведомить об изменении выбора вкладки */
    void BroadcastTabSelectionChanged(int32 OldIndex, int32 NewIndex);

    /** Обработчик обновления данных персонажа */
    void OnCharacterDataUpdated();

    /** Обработчик активации экрана */
    void OnScreenActivated(int32 TabIndex);

    /** Create single widget content */
    UUserWidget* CreateSingleWidgetContent(const FSuspenseTabConfig& Config);

    /** Create layout widget content */
    UUserWidget* CreateLayoutWidgetContent(const FSuspenseTabConfig& Config);

    /** Маппинг кнопок на индексы */
    UPROPERTY(Transient)
    TMap<UButton*, int32> ButtonToIndexMap;

    /** Универсальный обработчик нажатия кнопки вкладки */
    UFUNCTION()
    void InternalOnTabButtonClicked();

    //=============================================================================
    // Состояние
    //=============================================================================

    /** Индекс текущей активной вкладки */
    UPROPERTY(Transient)
    int32 CurrentTabIndex;

    /** Массив кнопок вкладок */
    UPROPERTY(Transient)
    TArray<UButton*> TabButtons;

    /** Массив виджетов контента */
    UPROPERTY(Transient)
    TArray<UUserWidget*> ContentWidgets;

    /** EventBus event handlers */
    void OnInventoryEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
    void OnCharacterLevelEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /** Event subscription handles */
    FSuspenseCoreSubscriptionHandle CharacterLevelEventHandle;
    FSuspenseCoreSubscriptionHandle InventoryEventHandle;

    /** Cached EventBus reference */
    TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

    /** Get event bus */
    class USuspenseCoreEventBus* GetEventBus() const;
};
