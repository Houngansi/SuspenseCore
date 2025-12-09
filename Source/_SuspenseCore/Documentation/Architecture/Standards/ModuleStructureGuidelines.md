# Module Structure Guidelines - ВАЖНО СЛЕДОВАТЬ!

**КРИТИЧЕСКИ ВАЖНО**: Эти правила ОБЯЗАТЕЛЬНЫ для всех миграций!

---

## ❌ НЕПРАВИЛЬНАЯ Структура (МОЯ ОШИБКА)

```
Source/InteractionSystem/
└── SuspenseInteraction/          ❌ НЕ СОЗДАВАТЬ ЭТУ ПАПКУ!
    ├── Private/
    ├── Public/
    └── SuspenseInteraction.Build.cs
```

**Проблема**: Создание вложенной папки `SuspenseInteraction/` внутри `InteractionSystem/`

---

## ✅ ПРАВИЛЬНАЯ Структура

```
Source/InteractionSystem/
├── InteractionSystem.Build.cs             (wrapper module, НЕ ТРОГАТЬ)
├── SuspenseInteraction.Build.cs           ✅ В КОРНЕ InteractionSystem/
├── Private/
│   ├── InteractionSystem.cpp              (wrapper, НЕ ТРОГАТЬ)
│   ├── SuspenseInteraction.cpp            ✅ Напрямую в Private/
│   ├── Components/
│   │   └── SuspenseInteractionComponent.cpp
│   ├── Pickup/
│   │   └── SuspensePickupItem.cpp
│   └── Utils/
│       ├── SuspenseHelpers.cpp
│       └── SuspenseItemFactory.cpp
└── Public/
    ├── InteractionSystem.h                (wrapper, НЕ ТРОГАТЬ)
    ├── SuspenseInteraction.h              ✅ Напрямую в Public/
    ├── Components/
    │   └── SuspenseInteractionComponent.h
    ├── Pickup/
    │   └── SuspensePickupItem.h
    └── Utils/
        ├── SuspenseHelpers.h
        ├── SuspenseItemFactory.h
        └── SuspenseInteractionSettings.h
```

---

## 📐 Общие Правила Для ВСЕХ Систем

### Системы с Wrapper Module

Эти системы имеют wrapper модуль (пустой модуль-обертка):

**InteractionSystem, EquipmentSystem, InventorySystem, UISystem, GAS, PlayerCore, BridgeSystem**

Для них структура:
```
Source/{SystemName}/
├── {SystemName}.Build.cs              (wrapper, НЕ ТРОГАТЬ)
├── Suspense{ModuleName}.Build.cs      ✅ Новый Build.cs в корне системы
├── Private/
│   ├── {SystemName}.cpp               (wrapper, НЕ ТРОГАТЬ)
│   ├── Suspense{ModuleName}.cpp       ✅ Файлы напрямую в Private/
│   └── (subdirectories...)            ✅ Поддиректории для организации
└── Public/
    ├── {SystemName}.h                 (wrapper, НЕ ТРОГАТЬ)
    ├── Suspense{ModuleName}.h         ✅ Файлы напрямую в Public/
    └── (subdirectories...)            ✅ Поддиректории для организации
```

**Примеры**:
- `Source/InteractionSystem/` → файлы в `Private/` и `Public/`, НЕ в `SuspenseInteraction/`
- `Source/EquipmentSystem/` → файлы в `Private/` и `Public/`, НЕ в `SuspenseEquipment/`
- `Source/InventorySystem/` → файлы в `Private/` и `Public/`, НЕ в `SuspenseInventory/`

---

## 🎯 Правило Размещения Файлов

### ✅ ДА - Размещать напрямую:
```
Source/InteractionSystem/Private/SuspenseInteraction.cpp
Source/InteractionSystem/Public/SuspenseInteraction.h
```

### ❌ НЕТ - НЕ создавать вложенные папки:
```
Source/InteractionSystem/SuspenseInteraction/Private/...  ❌ НЕПРАВИЛЬНО!
Source/InteractionSystem/SuspenseInteraction/Public/...   ❌ НЕПРАВИЛЬНО!
```

---

## 📋 Build.cs Files

Каждая система будет иметь **ДВА** Build.cs файла:

1. **Wrapper Build.cs** (НЕ ТРОГАТЬ):
   ```
   Source/InteractionSystem/InteractionSystem.Build.cs
   ```

2. **Module Build.cs** (СОЗДАТЬ):
   ```
   Source/InteractionSystem/SuspenseInteraction.Build.cs
   ```

**Важно**: Build.cs нового модуля размещается в **КОРНЕ** системы, НЕ во вложенной папке!

---

## 🔍 Legacy Module Location

Legacy модули (MedCom*) остаются во вложенных папках (не трогаем их):

```
Source/InteractionSystem/
├── MedComInteraction/          ✅ Legacy остается здесь (не трогать)
│   ├── MedComInteraction.Build.cs
│   ├── Private/
│   └── Public/
└── (новые Suspense файлы в корневых Private/Public/)
```

**После миграции**: Удалим все `MedComInteraction/` папки целиком.

---

## 📝 Checklist Для Каждой Миграции

Перед созданием файлов, проверь:

- [ ] Создаю `{Module}.Build.cs` в **корне** системы (не во вложенной папке)
- [ ] Создаю файлы напрямую в `Private/` (не в `Suspense*/Private/`)
- [ ] Создаю файлы напрямую в `Public/` (не в `Suspense*/Public/`)
- [ ] НЕ создаю вложенную папку `Suspense{ModuleName}/`
- [ ] Использую поддиректории (`Components/`, `Utils/`) для организации ВНУТРИ `Private/` и `Public/`
- [ ] НЕ трогаю wrapper файлы (`InteractionSystem.h`, `InteractionSystem.cpp`)

---

## 🚀 Примеры Для Других Модулов

### MedComCore → SuspenseCore

```
Source/PlayerCore/
├── PlayerCore.Build.cs              (wrapper, НЕ ТРОГАТЬ)
├── SuspenseCore.Build.cs            ✅ СОЗДАТЬ ЗДЕСЬ
├── Private/
│   ├── PlayerCore.cpp               (wrapper, НЕ ТРОГАТЬ)
│   ├── SuspenseCore.cpp             ✅ СОЗДАТЬ ЗДЕСЬ
│   ├── Characters/
│   │   └── SuspenseCharacter.cpp
│   └── Core/
└── Public/
    ├── PlayerCore.h                 (wrapper, НЕ ТРОГАТЬ)
    ├── SuspenseCore.h               ✅ СОЗДАТЬ ЗДЕСЬ
    ├── Characters/
    │   └── SuspenseCharacter.h
    └── Core/
```

### MedComInventory → SuspenseInventory

```
Source/InventorySystem/
├── InventorySystem.Build.cs         (wrapper, НЕ ТРОГАТЬ)
├── SuspenseInventory.Build.cs       ✅ СОЗДАТЬ ЗДЕСЬ
├── Private/
│   ├── InventorySystem.cpp          (wrapper, НЕ ТРОГАТЬ)
│   ├── SuspenseInventory.cpp        ✅ СОЗДАТЬ ЗДЕСЬ
│   ├── Components/
│   ├── Operations/
│   └── Storage/
└── Public/
    ├── InventorySystem.h            (wrapper, НЕ ТРОГАТЬ)
    ├── SuspenseInventory.h          ✅ СОЗДАТЬ ЗДЕСЬ
    ├── Components/
    ├── Operations/
    └── Storage/
```

### MedComEquipment → SuspenseEquipment

```
Source/EquipmentSystem/
├── EquipmentSystem.Build.cs         (wrapper, НЕ ТРОГАТЬ)
├── SuspenseEquipment.Build.cs       ✅ СОЗДАТЬ ЗДЕСЬ
├── Private/
│   ├── EquipmentSystem.cpp          (wrapper, НЕ ТРОГАТЬ)
│   ├── SuspenseEquipment.cpp        ✅ СОЗДАТЬ ЗДЕСЬ
│   ├── Components/
│   ├── Services/
│   └── Base/
└── Public/
    ├── EquipmentSystem.h            (wrapper, НЕ ТРОГАТЬ)
    ├── SuspenseEquipment.h          ✅ СОЗДАТЬ ЗДЕСЬ
    ├── Components/
    ├── Services/
    └── Base/
```

---

## ⚠️ Частые Ошибки (НЕ ДЕЛАЙ ТАК!)

### ❌ Ошибка 1: Создание вложенной папки
```
Source/InteractionSystem/SuspenseInteraction/  ❌ НЕТ!
```

### ❌ Ошибка 2: Build.cs во вложенной папке
```
Source/InteractionSystem/SuspenseInteraction/SuspenseInteraction.Build.cs  ❌ НЕТ!
```

### ❌ Ошибка 3: Игнорирование wrapper файлов
```
Удаление InteractionSystem.h/cpp  ❌ НЕТ! Не трогать wrapper!
```

---

## ✅ Правильный Workflow

1. **Проверить wrapper**:
   ```bash
   ls Source/{SystemName}/{SystemName}.Build.cs  # Должен существовать
   ls Source/{SystemName}/Private/{SystemName}.cpp  # Должен существовать
   ```

2. **Создать Build.cs в корне**:
   ```bash
   touch Source/{SystemName}/Suspense{Module}.Build.cs
   ```

3. **Создать файлы напрямую в Private/Public**:
   ```bash
   touch Source/{SystemName}/Private/Suspense{Module}.cpp
   touch Source/{SystemName}/Public/Suspense{Module}.h
   ```

4. **Создать поддиректории по необходимости**:
   ```bash
   mkdir -p Source/{SystemName}/Private/Components
   mkdir -p Source/{SystemName}/Public/Components
   ```

5. **НЕ создавать вложенную папку модуля**:
   ```bash
   # ❌ НЕ ДЕЛАТЬ:
   mkdir Source/{SystemName}/Suspense{Module}/
   ```

---

## 📚 Почему Такая Структура?

1. **Wrapper module pattern**:
   - `InteractionSystem` - wrapper (пустая обертка)
   - Содержимое напрямую в `Private/` и `Public/`

2. **Упрощение навигации**:
   - Меньше уровней вложенности
   - Легче найти файлы

3. **Соответствие Unreal Engine conventions**:
   - Стандартная структура для модулей UE

4. **Единообразие**:
   - Все системы следуют одному паттерну

---

## 🎓 Резюме

**ЗОЛОТОЕ ПРАВИЛО**:

> Файлы Suspense идут **НАПРЯМУЮ** в `Private/` и `Public/` родительской системы.
>
> **НЕ** создавай вложенную папку `Suspense{ModuleName}/`!

**Файлы в корне системы**:
- ✅ `{System}.Build.cs` (wrapper, не трогать)
- ✅ `Suspense{Module}.Build.cs` (создать)

**Файлы в Private/**:
- ✅ `{System}.cpp` (wrapper, не трогать)
- ✅ `Suspense{Module}.cpp` (создать)
- ✅ Поддиректории: `Components/`, `Utils/`, etc.

**Файлы в Public/**:
- ✅ `{System}.h` (wrapper, не трогать)
- ✅ `Suspense{Module}.h` (создать)
- ✅ Поддиректории: `Components/`, `Utils/`, etc.

---

**ВСЕГДА** следуй этому документу перед созданием новых модулей!

**Document Version**: 1.0
**Created**: 2025-11-24
**Purpose**: Prevent module structure mistakes
