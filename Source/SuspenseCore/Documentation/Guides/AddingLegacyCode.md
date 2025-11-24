# Добавление Legacy кода для рефакторинга

Руководство по добавлению старого кода в проект SuspenseCore для последующего рефакторинга.

---

## 🎯 Концепция

Старый код добавляется в **отдельные директории** внутри целевых модулей для анализа и постепенного рефакторинга:

```
Source/
├── BridgeSystem/
│   ├── Public/              ← Новый код (рефакторенный)
│   ├── Private/             ← Новый код (рефакторенный)
│   └── MedComShared/        ← LEGACY: Старый код для анализа
├── GAS/
│   ├── Public/              ← Новый код
│   ├── Private/             ← Новый код
│   └── MedComGAS/           ← LEGACY: Старые GAS классы
└── PlayerCore/
    ├── Public/              ← Новый код
    ├── Private/             ← Новый код
    └── MedComCore/          ← LEGACY: Старые player системы
```

**Преимущества:**
- ✅ Legacy код изолирован в отдельных папках
- ✅ Не ломает существующую структуру
- ✅ Легко анализировать и сравнивать
- ✅ Можно удалить после завершения миграции
- ✅ Видна история рефакторинга

---

## 📦 Способы добавления файлов

### Способ 1: Ручное копирование и коммит

**Шаг 1: Скопируйте файлы**

```bash
cd /home/user/SuspenseCore

# Скопируйте legacy код в соответствующие модули
cp -r /path/to/old/project/MedComShared Source/BridgeSystem/
cp -r /path/to/old/project/MedComGAS Source/GAS/
cp -r /path/to/old/project/MedComCore Source/PlayerCore/

# Или любые другие legacy директории:
cp -r /path/to/old/project/YourLegacySystem Source/TargetModule/
```

**Шаг 2: Проверьте что скопировалось**

```bash
# Проверить структуру
ls -la Source/GAS/MedComGAS/
ls -la Source/PlayerCore/MedComCore/
ls -la Source/BridgeSystem/MedComShared/

# Посмотреть статус git
git status
```

**Шаг 3: Добавьте в git**

```bash
# Добавить все legacy директории
git add Source/BridgeSystem/MedComShared/
git add Source/GAS/MedComGAS/
git add Source/PlayerCore/MedComCore/

# Или добавить все сразу
git add Source/*/MedCom*/

# Проверить что будет закоммичено
git status
```

**Шаг 4: Закоммитьте**

```bash
git commit -m "chore: Add legacy code for refactoring

- Add MedComShared to BridgeSystem (legacy shared code)
- Add MedComGAS to GAS module (legacy GAS implementation)
- Add MedComCore to PlayerCore (legacy player systems)

This code will be analyzed and refactored to SuspenseCore standards."
```

**Шаг 5: Запушьте**

```bash
git push origin claude/audit-project-migration-01HFuENs9TnbYiKa93AZWBA7
```

---

### Способ 2: Через архив

Если старый проект большой, можно сначала создать архив:

```bash
# В директории старого проекта
tar -czf legacy-code.tar.gz MedComShared/ MedComGAS/ MedComCore/

# Скопировать архив в SuspenseCore
cp legacy-code.tar.gz /home/user/SuspenseCore/

# В SuspenseCore
cd /home/user/SuspenseCore

# Распаковать в нужные модули
tar -xzf legacy-code.tar.gz -C Source/BridgeSystem/ MedComShared/
tar -xzf legacy-code.tar.gz -C Source/GAS/ MedComGAS/
tar -xzf legacy-code.tar.gz -C Source/PlayerCore/ MedComCore/

# Удалить архив
rm legacy-code.tar.gz

# Добавить в git (см. Способ 1, Шаги 3-5)
```

---

### Способ 3: Выборочное копирование

Если нужны только определенные файлы:

```bash
cd /home/user/SuspenseCore

# Создать структуру
mkdir -p Source/GAS/MedComGAS/{Public,Private}

# Скопировать только нужные файлы
cp /old/project/MedComGAS/SomeClass.h Source/GAS/MedComGAS/Public/
cp /old/project/MedComGAS/SomeClass.cpp Source/GAS/MedComGAS/Private/

# Добавить в git
git add Source/GAS/MedComGAS/
git commit -m "chore: Add SomeClass for refactoring"
git push origin claude/audit-project-migration-01HFuENs9TnbYiKa93AZWBA7
```

---

## 🗂️ Рекомендуемая структура Legacy директорий

### Для модуля GAS:

```
Source/GAS/
├── Public/                  ← Новый рефакторенный код
├── Private/                 ← Новый рефакторенный код
├── GAS.Build.cs
└── MedComGAS/              ← LEGACY
    ├── AbilitySystem/
    ├── Attributes/
    ├── Abilities/
    ├── Effects/
    └── Tags/
```

### Для модуля PlayerCore:

```
Source/PlayerCore/
├── Public/
├── Private/
├── PlayerCore.Build.cs
└── MedComCore/             ← LEGACY
    ├── Character/
    ├── Controller/
    ├── Camera/
    ├── Input/
    └── Movement/
```

### Для модуля BridgeSystem:

```
Source/BridgeSystem/
├── Public/
├── Private/
├── BridgeSystem.Build.cs
└── MedComShared/           ← LEGACY
    ├── Utilities/
    ├── Interfaces/
    ├── Delegates/
    └── Types/
```

---

## 📋 Чек-лист добавления legacy кода

- [ ] Определить целевой модуль для legacy кода
- [ ] Создать директорию `LegacyName/` внутри модуля
- [ ] Скопировать файлы старого проекта
- [ ] Проверить структуру: `ls -la Source/Module/LegacyName/`
- [ ] Добавить в git: `git add Source/Module/LegacyName/`
- [ ] Проверить статус: `git status`
- [ ] Закоммитить с описательным сообщением
- [ ] Запушить на feature branch
- [ ] Обновить список в разделе "Legacy код" ниже

---

## 📊 Реестр Legacy кода

### Добавленный legacy код:

| Legacy директория | Целевой модуль | Дата добавления | Статус | Оценка | Примечание |
|-------------------|----------------|-----------------|--------|--------|------------|
| MedComShared | BridgeSystem | 2025-11-24 | ✅ Проанализирован | 9/10 | Общие утилиты, 60 интерфейсов |
| MedComGAS | GAS | 2025-11-24 | ✅ Проанализирован | 9/10 | GAS реализация, 22 класса |
| MedComCore | PlayerCore | 2025-11-24 | ✅ Проанализирован | 8.5/10 | Character, Controller, PlayerState |
| MedComInventory | InventorySystem | 2025-11-24 | ✅ Проанализирован | **9/10** | 🌟 **Отличная архитектура**! Command Pattern, FastArraySerializer, 36 классов, 27.8K LOC. Production-ready multiplayer. [Детали](../Architecture/MedComInventory_Analysis.md) |
| MedComEquipment | EquipmentSystem | 2025-11-24 | ⏳ Ожидает анализа | - | Самый большой модуль (54K LOC) |
| MedComInteraction | InteractionSystem | 2025-11-24 | ⏳ Ожидает анализа | - | Компактная система (3.5K LOC) |
| MedComUI | UISystem | 2025-11-24 | ⏳ Ожидает анализа | - | UI виджеты (26.7K LOC) |

**Легенда статусов:**
- ⏳ Ожидает анализа
- 🔄 Анализируется
- ✅ Проанализирован
- 🔨 Рефакторинг в процессе
- ✅ Завершен
- 🗑️ Можно удалить

---

## 🔍 После добавления кода

### Что делает AI:

1. **Анализ кода:**
   - Изучает архитектуру legacy классов
   - Определяет зависимости
   - Выявляет паттерны использования

2. **Планирование рефакторинга:**
   - Составляет mapping: Legacy → New
   - Определяет порядок миграции
   - Выявляет breaking changes

3. **Создание плана:**
   - Пошаговый план рефакторинга
   - Приоритизация классов
   - Оценка сложности

4. **Рефакторинг:**
   - Создание новых классов в Public/Private
   - Обновление под UE 5.7 стандарты
   - Интеграция с GAS
   - Настройка репликации

---

## 🛠️ Команды для работы с legacy кодом

### Поиск всех legacy директорий:

```bash
find Source -type d -name "MedCom*"
```

### Подсчет файлов в legacy коде:

```bash
find Source/GAS/MedComGAS -type f | wc -l
```

### Список всех .h файлов в legacy:

```bash
find Source -path "*/MedCom*/*.h" -type f
```

### Поиск класса в legacy коде:

```bash
grep -r "class UMyClass" Source/*/MedCom*/
```

### Удаление legacy кода после завершения:

```bash
# ОСТОРОЖНО! Только после полного завершения рефакторинга!
git rm -r Source/GAS/MedComGAS/
git commit -m "chore: Remove legacy MedComGAS (refactoring completed)"
git push
```

---

## ⚠️ Важные замечания

1. **НЕ компилируйте legacy код**
   - Legacy директории НЕ включаются в Build.cs
   - Они служат только для reference
   - Компилируется только новый код в Public/Private

2. **Именование legacy директорий**
   - Используйте понятные имена: `MedComGAS`, `OldInventory`, `LegacyPlayer`
   - Избегайте generic имен: `Old`, `Backup`, `Temp`
   - Префикс помогает: `Legacy_`, `MedCom_`, `Old_`

3. **Размер legacy кода**
   - Если legacy код очень большой (>100MB), рассмотрите:
     - Git LFS для больших бинарников
     - Добавление только необходимых файлов
     - Создание отдельной ветки для legacy

4. **Git ignore**
   - Legacy код **должен** быть в git (для истории)
   - НЕ добавляйте legacy директории в `.gitignore`
   - Исключение: временные файлы внутри legacy (.obj, .pdb, etc.)

---

## 🎯 Примеры добавления разных типов кода

### Добавить inventory систему:

```bash
# Старый проект: InventorySystem/
# Целевой модуль: InventorySystem

cp -r /old/project/InventorySystem Source/InventorySystem/LegacyInventory/
git add Source/InventorySystem/LegacyInventory/
git commit -m "chore: Add legacy inventory for refactoring"
git push origin claude/audit-project-migration-01HFuENs9TnbYiKa93AZWBA7
```

### Добавить UI систему:

```bash
# Старый проект: UI/
# Целевой модуль: UISystem

cp -r /old/project/UI Source/UISystem/LegacyUI/
git add Source/UISystem/LegacyUI/
git commit -m "chore: Add legacy UI widgets for refactoring"
git push origin claude/audit-project-migration-01HFuENs9TnbYiKa93AZWBA7
```

### Добавить отдельный класс:

```bash
# Один файл для quick reference
mkdir -p Source/PlayerCore/LegacyReference
cp /old/project/MyOldCharacter.h Source/PlayerCore/LegacyReference/
cp /old/project/MyOldCharacter.cpp Source/PlayerCore/LegacyReference/
git add Source/PlayerCore/LegacyReference/
git commit -m "chore: Add MyOldCharacter for reference"
git push origin claude/audit-project-migration-01HFuENs9TnbYiKa93AZWBA7
```

---

## 📝 Шаблон коммита для legacy кода

```bash
git commit -m "chore: Add [SystemName] legacy code for refactoring

- Add [LegacyDir] to [TargetModule] module
- Contains [brief description of what's inside]
- [Number] classes, [Number] files
- Will be refactored to SuspenseCore standards

Related systems: [list of related modules]"
```

**Пример:**

```bash
git commit -m "chore: Add InventorySystem legacy code for refactoring

- Add LegacyInventory to InventorySystem module
- Contains item management, container system, crafting
- 25 classes, 50 files
- Will be refactored to SuspenseCore standards

Related systems: EquipmentSystem, UISystem"
```

---

## 🚀 Следующие шаги после добавления

1. **AI проанализирует код** и создаст:
   - Architecture analysis document
   - Refactoring plan
   - Class mapping table
   - Dependency graph

2. **Совместное планирование:**
   - Обсуждение подхода к рефакторингу
   - Приоритизация классов
   - Определение breaking changes

3. **Пошаговый рефакторинг:**
   - Создание новых классов
   - Тестирование
   - Обновление документации

---

---

## 📈 Статистика анализа

### Проанализированные модули (4/7):

#### ✅ MedComInventory - ЗАВЕРШЕН
**Оценка архитектуры:** 9/10 🌟

**Ключевые метрики:**
- 36 классов для миграции
- 27,862 строк кода
- 16 UCLASS, 16 USTRUCT
- Сложность миграции: Medium-High
- Время миграции: ~3-4 недели

**Архитектурные паттерны:**
- ✅ Command Pattern (Undo/Redo)
- ✅ FastArraySerializer (Network)
- ✅ Repository Pattern (Storage)
- ✅ Observer Pattern (Events)
- ✅ Transaction Pattern (Atomic ops)

**Приоритетные классы для миграции:**
1. `UMedComInventoryComponent` → `USuspenseInventoryComponent`
2. `UMedComInventoryStorage` → `USuspenseInventoryStorage`
3. `UInventoryReplicator` → `USuspenseInventoryReplicator`
4. `UMedComInventoryTransaction` → `USuspenseInventoryTransaction`

**Документация:** [MedComInventory_Analysis.md](../Architecture/MedComInventory_Analysis.md)

---

**Последнее обновление:** 2025-11-24
**Расположение:** Source/SuspenseCore/Documentation/Guides/
