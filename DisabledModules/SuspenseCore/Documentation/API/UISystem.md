# UISystem API Reference

**Модуль:** UISystem
**Версия:** 2.0.0
**Статус:** Полностью мигрирован, компилируется
**LOC:** ~26,706
**Последнее обновление:** 2025-11-28

---

## Обзор модуля

UISystem — модуль пользовательского интерфейса для SuspenseCore. Включает централизованный UI Manager, базовые виджеты, систему layout'ов, drag-and-drop, тултипы и мосты к игровым системам.

### Архитектурная роль

UISystem реализует:

1. **UI Manager** — централизованное управление виджетами как GameInstanceSubsystem
2. **Widget Hierarchy** — базовые классы с lifecycle management
3. **Layout System** — управление табами и контейнерами
4. **Bridge Pattern** — связь UI с InventorySystem и EquipmentSystem
5. **Drag-Drop** — система перетаскивания предметов
6. **Tooltip System** — централизованные тултипы

### Зависимости модуля

```
UISystem
├── Core
├── GameplayTags
├── GameplayAbilities
├── BridgeSystem
├── InputCore
├── UMG
├── EquipmentSystem
└── InventorySystem
```

**Private зависимости:** CoreUObject, Engine, Slate, SlateCore

---

## Архитектура

### Принципы дизайна

1. **UIManager создаёт ТОЛЬКО root widgets** (HUD, CharacterScreen, Dialogs)
2. **Дочерние виджеты** создаются через Layout system
3. **Bridges** подключаются к виджетам через Layout, не напрямую
4. **On-demand initialization** — мосты создаются при открытии табов

```
┌──────────────────────────────────────────────────────┐
│                    UIManager                          │
│            (GameInstanceSubsystem)                   │
├──────────────────────────────────────────────────────┤
│  Root Widgets:                                        │
│  ├── MainHUD                                          │
│  ├── CharacterScreen                                  │
│  │   └── TabBar → Layout → InventoryWidget           │
│  │               └── Layout → EquipmentWidget         │
│  └── Notifications                                    │
├──────────────────────────────────────────────────────┤
│  Bridges:                                             │
│  ├── InventoryUIBridge                               │
│  └── EquipmentUIBridge                               │
└──────────────────────────────────────────────────────┘
```

---

## Ключевые компоненты

### 1. USuspenseUIManager

**Файл:** `Public/Components/SuspenseUIManager.h`
**Тип:** UGameInstanceSubsystem
**Назначение:** Центральное управление всеми UI виджетами

```cpp
UCLASS()
class UISYSTEM_API USuspenseUIManager : public UGameInstanceSubsystem
```

#### Widget Configuration

```cpp
USTRUCT(BlueprintType)
struct FSuspenseWidgetInfo
{
    TSubclassOf<UUserWidget> WidgetClass;  // Класс виджета
    FGameplayTag WidgetTag;                 // Уникальный тег
    int32 ZOrder = 0;                       // Слой (выше = поверх)
    bool bAutoCreate = false;               // Авто-создание
    bool bPersistent = false;               // Сохранять между уровнями
    bool bAutoAddToViewport = true;         // Добавить к viewport
};
```

#### Core Widget Management

```cpp
// Создать ROOT виджет (не для дочерних!)
UUserWidget* CreateWidget(TSubclassOf<UUserWidget> WidgetClass,
    FGameplayTag WidgetTag, UObject* OwningObject, bool bForceAddToViewport = false);

// Показать/скрыть/уничтожить
bool ShowWidget(FGameplayTag WidgetTag, bool bAddToViewport = true);
bool HideWidget(FGameplayTag WidgetTag, bool bRemoveFromParent = false);
bool DestroyWidget(FGameplayTag WidgetTag);

// Получение виджетов
UUserWidget* GetWidget(FGameplayTag WidgetTag) const;
bool WidgetExists(FGameplayTag WidgetTag) const;
TArray<FGameplayTag> GetAllWidgetTags() const;
```

#### Layout Widget Support

```cpp
// Регистрация виджета созданного в layout
bool RegisterLayoutWidget(UUserWidget* Widget, FGameplayTag WidgetTag, UUserWidget* ParentLayout = nullptr);
bool UnregisterLayoutWidget(FGameplayTag WidgetTag);

// Поиск виджета во всех layouts (рекомендуемый метод)
UUserWidget* FindWidgetInLayouts(FGameplayTag WidgetTag) const;

// Получить виджет из конкретного layout
UUserWidget* GetWidgetFromLayout(USuspenseBaseLayoutWidget* LayoutWidget, FGameplayTag WidgetTag) const;
```

#### Character Screen

```cpp
// Показать экран персонажа с табом
UUserWidget* ShowCharacterScreen(UObject* OwningObject, FGameplayTag TabTag);

// Скрыть экран персонажа
bool HideCharacterScreen();

// Проверить видимость
bool IsCharacterScreenVisible() const;
```

#### HUD Management

```cpp
// Инициализировать главный HUD
UUserWidget* InitializeMainHUD(UObject* OwningObject);

// Запросить обновление HUD
void RequestHUDUpdate();
```

#### Bridge Management

```cpp
// Создание мостов (legacy, предпочтительно через ShowCharacterScreen)
USuspenseInventoryUIBridge* CreateInventoryUIBridge(APlayerController* PlayerController);
USuspenseEquipmentUIBridge* CreateEquipmentUIBridge(APlayerController* PlayerController);

// Получение мостов
USuspenseInventoryUIBridge* GetInventoryUIBridge() const;
USuspenseEquipmentUIBridge* GetEquipmentUIBridge() const;
```

#### Notifications

```cpp
void ShowNotification(const FText& Message, float Duration = 3.0f, FLinearColor Color = FLinearColor::White);
void ShowNotificationWithIcon(const FText& Message, UTexture2D* Icon, float Duration = 3.0f, FLinearColor Color = FLinearColor::White);
void ClearAllNotifications();
```

---

### 2. USuspenseBaseWidget

**Файл:** `Public/Widgets/Base/SuspenseBaseWidget.h`
**Тип:** UUserWidget
**Назначение:** Базовый класс для всех виджетов проекта

```cpp
UCLASS(Abstract)
class UISYSTEM_API USuspenseBaseWidget
    : public UUserWidget
    , public ISuspenseUIWidget
```

#### ISuspenseUIWidget Implementation

```cpp
void InitializeWidget_Implementation();
void UninitializeWidget_Implementation();
void UpdateWidget_Implementation(float DeltaTime);
FGameplayTag GetWidgetTag_Implementation() const;
void SetWidgetTag_Implementation(const FGameplayTag& NewTag);
bool IsInitialized_Implementation() const;
void ShowWidget_Implementation(bool bAnimate);
void HideWidget_Implementation(bool bAnimate);
void OnVisibilityChanged_Implementation(bool bIsVisible);
USuspenseEventManager* GetDelegateManager() const;
```

#### Animation Support

```cpp
// Анимации (опционально привязать в Blueprint)
UWidgetAnimation* ShowAnimation;
UWidgetAnimation* HideAnimation;

void PlayShowAnimation();
void PlayHideAnimation();
virtual void OnShowAnimationFinished();
virtual void OnHideAnimationFinished();
```

#### Properties

```cpp
FGameplayTag WidgetTag;     // Уникальный тег виджета
bool bEnableTick = false;   // Включить тикание
bool bIsShowing = true;     // Текущее состояние видимости
bool bIsInitialized = false; // Инициализирован ли виджет
```

---

### 3. Иерархия виджетов

#### Base Classes

```
USuspenseBaseWidget
├── USuspenseBaseContainerWidget (контейнеры слотов)
│   ├── USuspenseInventoryWidget
│   └── USuspenseEquipmentContainerWidget
├── USuspenseBaseSlotWidget (отдельные слоты)
│   ├── USuspenseInventorySlotWidget
│   └── USuspenseEquipmentSlotWidget
└── USuspenseBaseLayoutWidget (layout с табами)
    └── USuspenseHorizontalLayoutWidget
```

#### HUD Widgets

```
USuspenseMainHUDWidget
├── USuspenseHealthStaminaWidget
├── USuspenseWeaponUIWidget
└── USuspenseCrosshairWidget
```

#### Screen Widgets

```
USuspenseCharacterScreen
└── USuspenseUpperTabBar
    ├── Tab: Inventory → USuspenseInventoryWidget
    ├── Tab: Equipment → USuspenseEquipmentContainerWidget
    └── Tab: Skills → (...)
```

---

### 4. USuspenseInventoryUIBridge

**Файл:** `Public/Components/SuspenseInventoryUIBridge.h`
**Тип:** UObject
**Назначение:** Мост между InventoryComponent и UI виджетами

```cpp
UCLASS(BlueprintType)
class UISYSTEM_API USuspenseInventoryUIBridge : public UObject
```

#### Initialization

```cpp
// Инициализация с компонентом инвентаря
bool Initialize(USuspenseInventoryComponent* InInventoryComponent);

// Подключение к виджету инвентаря
void ConnectToInventoryWidget(USuspenseInventoryWidget* InWidget);
```

#### Data Flow

```cpp
// Обновить UI из состояния инвентаря
void RefreshUI();

// Получить данные для слота
bool GetSlotData(int32 SlotIndex, FSlotUIData& OutData) const;

// Получить все данные слотов
TArray<FSlotUIData> GetAllSlotData() const;
```

#### Operations

```cpp
// Drag-drop операции
void OnItemDragStarted(int32 SlotIndex);
void OnItemDragEnded(int32 SourceSlot, int32 TargetSlot, bool bSuccess);
void OnItemDropped(int32 TargetSlot, const FSlotUIData& DroppedItem);

// Rotate item
bool RotateItem(int32 SlotIndex);

// Split stack
bool SplitStack(int32 SlotIndex, int32 Amount);
```

---

### 5. USuspenseEquipmentUIBridge

**Файл:** `Public/Components/SuspenseEquipmentUIBridge.h`
**Тип:** UObject
**Назначение:** Мост между Equipment компонентами и UI

```cpp
UCLASS(BlueprintType)
class UISYSTEM_API USuspenseEquipmentUIBridge : public UObject
```

#### Connection to Equipment

```cpp
// Подключение к DataStore
bool ConnectToDataStore(USuspenseEquipmentDataStore* InDataStore);

// Подключение к виджету экипировки
void ConnectToEquipmentWidget(USuspenseEquipmentContainerWidget* InWidget);
```

#### Data Flow

```cpp
void RefreshUI();
bool GetEquipmentSlotData(int32 SlotIndex, FEquipmentSlotUIData& OutData) const;
TArray<FEquipmentSlotUIData> GetAllEquipmentSlotData() const;
```

#### Operations

```cpp
void OnEquipmentDragStarted(int32 SlotIndex);
void OnEquipmentDropped(int32 TargetSlot, const FSuspenseCoreInventoryItemInstance& Item);
```

---

### 6. Drag-Drop System

#### USuspenseDragDropOperation

**Файл:** `Public/Widgets/DragDrop/SuspenseDragDropOperation.h`

```cpp
UCLASS()
class UISYSTEM_API USuspenseDragDropOperation : public UDragDropOperation
{
    // Данные перетаскиваемого предмета
    FSuspenseCoreInventoryItemInstance ItemInstance;

    // Источник
    int32 SourceSlotIndex;
    ESlotContainerType SourceContainerType;

    // Визуальные данные
    FSlotUIData DragVisualData;
};
```

#### USuspenseDragVisualWidget

**Файл:** `Public/Widgets/DragDrop/SuspenseDragVisualWidget.h`

Визуальный виджет для отображения перетаскиваемого предмета.

#### USuspenseDragDropHandler

**Файл:** `Public/DragDrop/SuspenseDragDropHandler.h`

Обработчик drag-drop операций:
```cpp
bool CanDrop(const USuspenseDragDropOperation* Operation, int32 TargetSlot, ESlotContainerType TargetContainer) const;
FDragDropResult ExecuteDrop(USuspenseDragDropOperation* Operation, int32 TargetSlot, ESlotContainerType TargetContainer);
```

---

### 7. Tooltip System

#### USuspenseTooltipManager

**Файл:** `Public/Tooltip/SuspenseTooltipManager.h`

Централизованное управление тултипами:

```cpp
UCLASS()
class UISYSTEM_API USuspenseTooltipManager : public UGameInstanceSubsystem
{
    // Показать тултип для предмета
    void ShowItemTooltip(const FSuspenseCoreInventoryItemInstance& Item, const FVector2D& Position);

    // Скрыть текущий тултип
    void HideTooltip();

    // Обновить позицию тултипа
    void UpdateTooltipPosition(const FVector2D& NewPosition);
};
```

#### USuspenseItemTooltipWidget

**Файл:** `Public/Widgets/Tooltip/SuspenseItemTooltipWidget.h`

Виджет тултипа с информацией о предмете:
- Название и редкость
- Иконка
- Характеристики оружия/брони
- Runtime свойства (durability, ammo)

---

## Паттерны использования

### Открытие экрана инвентаря

```cpp
// В PlayerController
void AMyPlayerController::ToggleInventory()
{
    USuspenseUIManager* UIManager = USuspenseUIManager::Get(this);
    if (UIManager)
    {
        if (UIManager->IsCharacterScreenVisible())
        {
            UIManager->HideCharacterScreen();
            SetInputMode(FInputModeGameOnly());
        }
        else
        {
            UIManager->ShowCharacterScreen(this,
                FGameplayTag::RequestGameplayTag(TEXT("UI.Tab.Inventory")));
            SetInputMode(FInputModeGameAndUI());
        }
    }
}
```

### Обновление HUD

```cpp
// При изменении здоровья/выносливости
void OnHealthChanged(float Current, float Max, float Percent)
{
    if (USuspenseUIManager* UIManager = USuspenseUIManager::Get(this))
    {
        UIManager->RequestHUDUpdate();
    }
}
```

### Показ нотификации

```cpp
// При подборе предмета
void OnItemPickedUp(const FText& ItemName)
{
    if (USuspenseUIManager* UIManager = USuspenseUIManager::Get(this))
    {
        FText Message = FText::Format(LOCTEXT("ItemPickedUp", "Подобрано: {0}"), ItemName);
        UIManager->ShowNotification(Message, 2.0f, FLinearColor::Green);
    }
}
```

### Drag-Drop между Inventory и Equipment

```cpp
// В InventorySlotWidget
void USuspenseInventorySlotWidget::OnDragDetected(...)
{
    USuspenseDragDropOperation* Op = NewObject<USuspenseDragDropOperation>();
    Op->ItemInstance = SlotData.ItemInstance;
    Op->SourceSlotIndex = SlotIndex;
    Op->SourceContainerType = ESlotContainerType::Inventory;
    Op->DragVisualData = SlotData;

    // Создаём визуальный виджет
    Op->DefaultDragVisual = CreateDragVisual(Op);

    // Уведомляем Bridge
    if (InventoryUIBridge)
    {
        InventoryUIBridge->OnItemDragStarted(SlotIndex);
    }
}

// В EquipmentSlotWidget
bool USuspenseEquipmentSlotWidget::NativeOnDrop(...)
{
    USuspenseDragDropOperation* Op = Cast<USuspenseDragDropOperation>(Operation);
    if (Op && DragDropHandler->CanDrop(Op, SlotIndex, ESlotContainerType::Equipment))
    {
        return DragDropHandler->ExecuteDrop(Op, SlotIndex, ESlotContainerType::Equipment).bSuccess;
    }
    return false;
}
```

---

## Риски и потенциальные проблемы

### 1. Complexity UIManager

**Проблема:** UIManager имеет слишком много ответственностей (500+ LOC).

**Последствия:**
- Сложность поддержки
- Tight coupling между различными widget types
- Риск memory leaks при неправильном cleanup

**Рекомендации:**
- Разделить на UIWidgetManager и UIBridgeManager
- Вынести notification system в отдельный subsystem
- Добавить unit tests для lifecycle

### 2. Связь с InventorySystem и EquipmentSystem

**Проблема:** UISystem напрямую зависит от InventorySystem и EquipmentSystem.

**Последствия:**
- Невозможность использовать UI без игровых систем
- Циклические зависимости при изменениях

**Рекомендации:**
- Использовать interfaces вместо прямых зависимостей
- Рассмотреть event-based связь через BridgeSystem

### 3. bEnableExceptions в Build.cs

**Проблема:** Включены C++ exceptions для всего модуля.

**Последствия:**
- Performance overhead
- Нестандартное поведение для UE

**Рекомендации:**
- Определить конкретные места требующие exceptions
- Рассмотреть альтернативные подходы (Result types)

### 4. Отсутствие Widget Pooling

**Проблема:** Слоты инвентаря создаются/уничтожаются при изменениях.

**Последствия:**
- GC spikes при большом инвентаре
- Memory churn

**Рекомендации:**
- Реализовать slot pool
- Reuse slots вместо destroy/create

### 5. Layout Tight Coupling

**Проблема:** Layout widgets тесно связаны с конкретными tab типами.

**Последствия:**
- Сложность добавления новых табов
- Code duplication

**Рекомендации:**
- Создать generic TabContent interface
- Factory pattern для создания tab content

### 6. Отсутствие Input Handling в виджетах

**Проблема:** Виджеты не перехватывают input напрямую, полагаются на PlayerController.

**Последствия:**
- Дублирование input logic
- Потенциальные race conditions

**Рекомендации:**
- Добавить IInputProcessor interface для виджетов
- Использовать CommonUI для input routing

### 7. Legacy MedCom references

**Проблема:** В комментариях остались упоминания MedCom.

**Последствия:**
- Путаница при поиске
- Несоответствие naming conventions

**Рекомендации:**
- Провести search/replace для комментариев
- Обновить Blueprint категории

---

## Метрики модуля

| Метрика | Значение |
|---------|----------|
| Файлов (.h) | 24 |
| Виджетов | 18+ |
| Bridges | 2 |
| Managers | 2 |
| LOC (приблизительно) | 26,706 |

---

## Связь с другими модулями

```
┌─────────────┐
│ PlayerCore  │
│(Controller) │
└──────┬──────┘
       │ owns
       ▼
┌─────────────────────────────────────────────────────────┐
│                       UISystem                           │
│  ┌──────────────┐  ┌─────────────┐  ┌───────────────┐   │
│  │  UIManager   │──│InventoryUI │──│EquipmentUI   │   │
│  │              │  │  Bridge    │  │    Bridge    │   │
│  └──────────────┘  └──────┬─────┘  └───────┬──────┘   │
└────────────────────────────┼───────────────┼──────────┘
                             │               │
                             ▼               ▼
                     ┌───────────────┐ ┌─────────────┐
                     │InventorySystem│ │EquipmentSys│
                     │ (Component)   │ │ (DataStore) │
                     └───────────────┘ └─────────────┘
```

**UISystem зависит от:**
- **BridgeSystem** — EventManager, типы
- **InventorySystem** — для InventoryUIBridge
- **EquipmentSystem** — для EquipmentUIBridge

**От UISystem зависят:**
- **PlayerCore** — создаёт HUD через PlayerController

---

**Дата создания:** 2025-11-28
**Автор:** Tech Lead
**Версия документа:** 1.0
