# Magazine Inspection Widget - Setup Guide

## Overview

`USuspenseCoreMagazineInspectionWidget` - это C++ виджет в стиле Escape from Tarkov для детальной инспекции магазина с визуализацией каждого патрона в сетке слотов.

**Расположение файлов:**
- Header: `Source/UISystem/Public/SuspenseCore/Widgets/HUD/SuspenseCoreMagazineInspectionWidget.h`
- Implementation: `Source/UISystem/Private/SuspenseCore/Widgets/HUD/SuspenseCoreMagazineInspectionWidget.cpp`
- Interface: `Source/BridgeSystem/Public/SuspenseCore/Interfaces/UI/ISuspenseCoreMagazineInspectionWidget.h`

---

## Step 1: Create Blueprint Widget

### 1.1 Create WBP_MagazineInspector

1. В Content Browser: **Right Click → User Interface → Widget Blueprint**
2. Назовите: `WBP_MagazineInspector`
3. **Parent Class**: выберите `SuspenseCoreMagazineInspectionWidget`

### 1.2 Create WBP_RoundSlot (Individual Round Widget)

1. В Content Browser: **Right Click → User Interface → Widget Blueprint**
2. Назовите: `WBP_RoundSlot`
3. **Parent Class**: `UserWidget` (стандартный)

**Минимальная структура WBP_RoundSlot:**
```
Canvas Panel
├── SlotBorder (Border)           ← Фон слота, меняет цвет при hover/occupied
│   └── RoundImage (Image)        ← Иконка патрона (скрыта если пусто)
```

**Важно:** Имена компонентов должны быть ТОЧНО `SlotBorder` и `RoundImage` - C++ код ищет их по имени.

---

## Step 2: Setup BindWidget Components

Откройте `WBP_MagazineInspector` в Designer и добавьте ВСЕ обязательные компоненты:

### 2.1 Required Components (все ОБЯЗАТЕЛЬНЫЕ)

| Component Name | Type | Purpose |
|----------------|------|---------|
| `CloseButton` | Button | Закрытие панели инспекции |
| `MagazineNameText` | TextBlock | Название магазина (e.g. "STANAG 30-Round") |
| `MagazineIcon` | Image | Иконка магазина (опционально скрыта) |
| `CaliberText` | TextBlock | Калибр (e.g. "5.56x45mm NATO") |
| `RoundSlotsContainer` | WrapBox | Контейнер для сетки слотов патронов |
| `RoundsCountText` | TextBlock | Счетчик "27/30" |
| `FillProgressBar` | ProgressBar | Полоса заполнения магазина |
| `HintText` | TextBlock | "Drag ammo here to load" |
| `DropZoneBorder` | Border | Подсветка зоны дропа |
| `LoadingProgressBar` | ProgressBar | Прогресс загрузки патрона |
| `LoadingStatusText` | TextBlock | "Loading round 15..." |

### 2.2 Recommended Layout Structure

```
Canvas Panel (Root)
├── BackgroundBorder (Border) [Fill Screen]
│   └── MainPanel (Vertical Box)
│       │
│       ├── HeaderSection (Horizontal Box)
│       │   ├── MagazineIcon (Image) [64x64]
│       │   └── HeaderText (Vertical Box)
│       │       ├── MagazineNameText (TextBlock)
│       │       └── CaliberText (TextBlock)
│       │
│       ├── RoundSlotsSection (Size Box)
│       │   └── DropZoneBorder (Border) [Highlight on drag]
│       │       └── RoundSlotsContainer (WrapBox) [Grid of round slots]
│       │
│       ├── LoadingSection (Horizontal Box) [Initially Collapsed]
│       │   ├── LoadingProgressBar (ProgressBar)
│       │   └── LoadingStatusText (TextBlock)
│       │
│       ├── FooterSection (Vertical Box)
│       │   ├── RoundsCountText (TextBlock) ["27/30"]
│       │   ├── FillProgressBar (ProgressBar)
│       │   └── HintText (TextBlock) ["Drag ammo to load"]
│       │
│       └── CloseButton (Button)
│           └── CloseText (TextBlock) ["X" or "Close"]
```

---

## Step 3: Configure Widget Properties

### 3.1 Blueprint Details Panel

В Details панели WBP_MagazineInspector настройте:

| Property | Category | Description |
|----------|----------|-------------|
| `RoundSlotWidgetClass` | SuspenseCore\|UI\|Config | Выберите WBP_RoundSlot |
| `LoadingStatusFormat` | SuspenseCore\|UI\|Text | `"Loading round {0}..."` |
| `UnloadingStatusFormat` | SuspenseCore\|UI\|Text | `"Unloading round {0}..."` |
| `DropHintText` | SuspenseCore\|UI\|Text | `"Drag ammo here to load"` |
| `FullHintText` | SuspenseCore\|UI\|Text | `"Magazine is full"` |

### 3.2 WrapBox Settings (RoundSlotsContainer)

- **Inner Slot Padding**: 4.0 (или по дизайну)
- **Explicit Wrap Size**: Включить и задать ширину для нужного количества колонок

---

## Step 4: Register in UIManager

### 4.1 Project Settings → Game Instance

Убедитесь что используется GameInstance с `USuspenseCoreUIManager`.

### 4.2 UIManager Configuration

В Blueprint или C++:

```cpp
// В Blueprint GameMode или HUD
USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
if (UIManager)
{
    // Виджет создается автоматически при первом вызове OpenInspection
    // Но можно задать класс заранее:
    UIManager->MagazineInspectionWidgetClass = WBP_MagazineInspector::StaticClass();
}
```

---

## Step 5: Opening Inspection

### 5.1 From Blueprint

```
// В ItemSlot или ContextMenu Blueprint:

Event OnInspectMagazine(MagazineData)
├── Get UIManager
├── Get or Create MagazineInspectionWidget
└── Call OpenInspection(MagazineData)
```

### 5.2 From C++

```cpp
void UMyInventoryWidget::OpenMagazineInspection(const FGuid& MagazineID)
{
    // Получаем данные магазина
    FSuspenseCoreMagazineInspectionData InspectionData;
    InspectionData.MagazineInstanceID = MagazineID;
    InspectionData.DisplayName = FText::FromString("STANAG 30-Round");
    InspectionData.CaliberDisplayName = FText::FromString("5.56x45mm NATO");
    InspectionData.MaxCapacity = 30;
    InspectionData.CurrentRounds = 27;

    // Заполняем слоты
    for (int32 i = 0; i < InspectionData.MaxCapacity; ++i)
    {
        FSuspenseCoreRoundSlotData Slot;
        Slot.SlotIndex = i;
        Slot.bIsOccupied = (i < InspectionData.CurrentRounds);
        if (Slot.bIsOccupied)
        {
            Slot.AmmoID = TEXT("Ammo.556.M855A1");
            Slot.AmmoDisplayName = FText::FromString("M855A1");
            Slot.bCanUnload = true;
        }
        InspectionData.RoundSlots.Add(Slot);
    }

    // Открываем инспекцию через интерфейс
    if (MagazineInspectionWidget)
    {
        ISuspenseCoreMagazineInspectionWidgetInterface::Execute_OpenInspection(
            MagazineInspectionWidget, InspectionData);
    }
}
```

---

## Step 6: Handle Events

### 6.1 Blueprint Events to Implement

В Event Graph WBP_MagazineInspector:

```
Event OnInspectionOpened
├── Play Open Animation
└── Focus Widget

Event OnInspectionClosed
├── Play Close Animation
└── Return Focus

Event OnRoundLoadingStarted (SlotIndex)
├── Highlight slot at SlotIndex
└── Play loading animation

Event OnRoundLoadingCompleted (SlotIndex)
├── Update slot visual
└── Play success sound

Event OnRoundClicked (SlotIndex)
├── If can unload → Start unload via AmmoLoadingService
└── Else → Show "Cannot unload" feedback

Event OnAmmoDropResult (DropResult)
├── Switch on DropResult:
│   ├── Loaded → Play success feedback
│   ├── IncompatibleCaliber → Show error "Wrong caliber"
│   ├── MagazineFull → Show error "Magazine full"
│   └── Busy → Show error "Loading in progress"
```

### 6.2 EventBus Integration (Automatic)

Виджет автоматически подписывается на события:

| GameplayTag | When Fired |
|-------------|------------|
| `TAG_Equipment_Event_Ammo_LoadStarted` | AmmoLoadingService начал загрузку |
| `TAG_Equipment_Event_Ammo_RoundLoaded` | Один патрон загружен |
| `TAG_Equipment_Event_Ammo_LoadCompleted` | Загрузка завершена |
| `TAG_Equipment_Event_Ammo_LoadCancelled` | Загрузка отменена |

---

## Step 7: Drag & Drop Integration

### 7.1 Accept Ammo Drops

Виджет автоматически обрабатывает дропы через `OnAmmoDropped_Implementation`.

Для кастомной логики дропа, переопределите в Blueprint:

```
Event OnDrop (Geometry, DragDropEvent, Operation)
├── Cast Operation to SuspenseCoreDragDropOperation
├── Get DraggedItemData
├── If Item.Category has "Item.Ammo":
│   ├── Call OnAmmoDropped(AmmoID, Quantity)
│   └── Return Handled
└── Return Unhandled
```

### 7.2 Publish Drop Highlight

```
Event OnDragEnter
├── Set DropZoneBorder Visibility = Visible
├── If compatible caliber:
│   └── Set Border Color = Green
└── Else:
    └── Set Border Color = Red

Event OnDragLeave
└── Set DropZoneBorder Visibility = Collapsed
```

---

## Step 8: Styling (WBP_RoundSlot)

### 8.1 Empty Slot Style
- Background: Dark gray (#2A2A2A)
- Border: 1px solid #444444
- RoundImage: Hidden

### 8.2 Occupied Slot Style
- Background: Slightly lighter (#3A3A3A)
- Border: 1px solid #666666
- RoundImage: Visible with ammo icon

### 8.3 Loading Slot Style (animate)
- Background: Pulsing yellow/orange
- Border: 2px solid #FFA500
- Show loading spinner overlay

### 8.4 Hover Slot Style
- Background: Lighter (#4A4A4A)
- Border: 1px solid #888888
- Tooltip with ammo name

---

## Troubleshooting

### Widget не появляется
1. Проверьте что все BindWidget компоненты добавлены с ТОЧНЫМИ именами
2. Проверьте NativeConstruct log - будет checkf если что-то отсутствует
3. Убедитесь что RoundSlotWidgetClass задан

### Слоты не создаются
1. Проверьте что RoundSlotWidgetClass указывает на валидный Widget Blueprint
2. Проверьте что RoundSlotsContainer - это WrapBox (не другой контейнер)
3. Проверьте что InspectionData.MaxCapacity > 0

### EventBus события не приходят
1. Проверьте что EventManager подсистема инициализирована
2. Проверьте что AmmoLoadingService публикует события с правильным MagazineInstanceID
3. Убедитесь что GUID сериализуется как строка: `MagazineID.ToString()`

### Ошибка "GetGuid not found"
Используйте:
```cpp
FGuid MagazineID;
FGuid::Parse(EventData.GetString(TEXT("MagazineInstanceID")), MagazineID);
```

---

## Complete Example: Opening from Context Menu

```cpp
// In UMyContextMenuWidget
void UMyContextMenuWidget::OnInspectMagazineClicked()
{
    if (!TargetItemData.IsValid())
        return;

    // Check if item is a magazine
    if (!TargetItemData.ItemTags.HasTag(SuspenseCoreEquipmentTags::Item::TAG_Item_Magazine))
        return;

    // Build inspection data from item
    FSuspenseCoreMagazineInspectionData Data = BuildInspectionData(TargetItemData);

    // Get or create inspection widget
    USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
    if (USuspenseCoreMagazineInspectionWidget* Inspector = UIManager->GetMagazineInspectionWidget())
    {
        ISuspenseCoreMagazineInspectionWidgetInterface::Execute_OpenInspection(Inspector, Data);
    }

    // Close context menu
    CloseMenu();
}
```

---

**Author:** SuspenseCore Team
**Version:** Phase 6 (January 2025)
**Related:** `TarkovStyle_Ammo_System_Design.md`, `UISystem_DeveloperGuide.md`
