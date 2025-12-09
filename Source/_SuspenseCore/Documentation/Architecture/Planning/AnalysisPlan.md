# Legacy Code Analysis Plan

> **✅ АНАЛИЗ И МИГРАЦИЯ ЗАВЕРШЕНЫ (2025-11-28)**
>
> Все этапы анализа были выполнены, миграция завершена успешно.
> Документ сохранён для исторических целей.

План анализа legacy кода для SuspenseCore проекта.

---

## Статус: ✅ ЗАВЕРШЕНО

Все 7 модулей проанализированы и успешно мигрированы:
- MedComShared → BridgeSystem ✅
- MedComGAS → GAS ✅
- MedComCore → PlayerCore ✅
- MedComInteraction → InteractionSystem ✅
- MedComInventory → InventorySystem ✅
- MedComEquipment → EquipmentSystem ✅
- MedComUI → UISystem ✅

---

## 🎯 Цель анализа (Выполнено)

Провести комплексный анализ legacy кода (MedComShared, MedComGAS, MedComCore) для подготовки к рефакторингу под стандарты SuspenseCore.

---

## 📊 Этапы анализа

### Этап 1: Инвентаризация кода

**Задачи:**
1. Подсчитать общее количество файлов (.h, .cpp)
2. Определить размер кодовой базы (строки кода)
3. Выявить основные директории и их назначение
4. Составить список всех классов

**Инструменты:**
```bash
# Количество файлов
find Source/*/MedCom* -type f -name "*.h" | wc -l
find Source/*/MedCom* -type f -name "*.cpp" | wc -l

# Строки кода
find Source/*/MedCom* -name "*.cpp" -o -name "*.h" | xargs wc -l

# Список классов
grep -r "^class\s" Source/*/MedCom*/*.h

# Список UCLASS
grep -r "UCLASS" Source/*/MedCom*/*.h
```

**Вывод:** Сводная таблица инвентаризации

---

### Этап 2: Architecture Analysis

**Задачи:**
1. Определить архитектурные паттерны в legacy коде
2. Выявить основные подсистемы
3. Понять separation of concerns
4. Определить coupling между классами

**Для каждого модуля:**

#### MedComGAS (GAS Module)
- Какие классы наследуются от UAbilitySystemComponent?
- Есть ли custom AttributeSets?
- Какие Gameplay Abilities реализованы?
- Есть ли Gameplay Effects?
- Как настроены GameplayTags?
- Интеграция с персонажем

#### MedComCore (PlayerCore Module)
- Character класс и его структура
- PlayerController функциональность
- PlayerState (если есть)
- Camera system
- Input handling
- Movement components
- Интеграция с GAS

#### MedComShared (BridgeSystem Module)
- Общие утилиты
- Shared interfaces
- Delegates и события
- Common data structures
- Helper функции

**Вывод:** Architecture Analysis Document

---

### Этап 3: Dependency Graph

**Задачи:**
1. Построить граф зависимостей между классами
2. Выявить циклические зависимости
3. Определить core классы (на которые многие зависят)
4. Найти листовые классы (ни от чего не зависят)

**Анализ включений:**
```bash
# Для каждого .h файла посмотреть #include
grep -r "^#include" Source/*/MedCom*/*.h

# Найти взаимные зависимости
```

**Методология:**
- Для каждого класса определить:
  - От каких классов зависит (includes)
  - Какие классы от него зависят (reverse lookup)
  - Уровень в иерархии зависимостей

**Вывод:** Dependency Graph (текстовый/диаграмма)

---

### Этап 4: Class Mapping

**Задачи:**
1. Сопоставить legacy классы с целевыми модулями SuspenseCore
2. Определить какие классы:
   - Мигрируются 1:1
   - Требуют рефакторинга
   - Разбиваются на несколько классов
   - Объединяются с другими
   - Удаляются (устаревшие)

**Mapping таблица:**

| Legacy класс | Старый модуль | → | Новый модуль | Новое имя | Сложность | Примечание |
|--------------|---------------|---|--------------|-----------|-----------|------------|
| UMedComASC | MedComGAS | → | GAS | USuspenseAbilitySystemComponent | Medium | Rename + refactor |
| UMedComAttributeSet | MedComGAS | → | GAS | USuspenseAttributeSet | Low | Rename |
| AMedComCharacter | MedComCore | → | PlayerCore | APlayerCharacterBase | High | Major refactor |

**Сложность:**
- **Low** — простое переименование, минимальные изменения
- **Medium** — рефакторинг API, обновление под UE 5.7
- **High** — значительная переработка архитектуры
- **Critical** — полная переписка

**Вывод:** Class Mapping Table

---

### Этап 5: Code Quality Assessment

**Задачи:**
1. Оценить качество legacy кода
2. Выявить anti-patterns
3. Найти технический долг
4. Определить проблемы совместимости с UE 5.7

**Критерии оценки:**

#### Coding Standards
- ✅/❌ Следование UE Coding Standard
- ✅/❌ Правильное именование (U/A/F/E/I префиксы)
- ✅/❌ Комментарии и документация
- ✅/❌ Const correctness

#### Replication
- ✅/❌ Правильная настройка репликации
- ✅/❌ DOREPLIFETIME использован
- ✅/❌ OnRep функции для визуала
- ✅/❌ Server authority паттерн

#### GAS Integration
- ✅/❌ Правильное использование GAS API
- ✅/❌ GameplayTags вместо booleans
- ✅/❌ AttributeSets для stats
- ✅/❌ GameplayEffects для модификаций

#### Performance
- ✅/❌ Избыточное использование Tick
- ✅/❌ Кеширование компонентов
- ✅/❌ Object pooling где нужно
- ✅/❌ Правильные replication conditions

**Вывод:** Code Quality Report

---

### Этап 6: Breaking Changes Analysis

**Задачи:**
1. Определить API changes при миграции
2. Найти несовместимости с новой архитектурой
3. Выявить Blueprint-зависимости
4. Определить data asset миграции

**Категории breaking changes:**

#### Renamed Classes
- Legacy class → New class name
- Impact: Blueprint references, C++ includes

#### Changed API
- Modified function signatures
- Removed deprecated functions
- New required parameters

#### Architectural Changes
- Class moved to different module
- Dependencies changed
- New design patterns applied

#### Data Migration
- Asset format changes
- Config file updates
- GameplayTags restructure

**Вывод:** Breaking Changes Document

---

### Этап 7: Refactoring Prioritization

**Задачи:**
1. Определить порядок рефакторинга классов
2. Создать зависимостные группы
3. Оценить усилия (effort estimation)
4. Распределить по спринтам

**Приоритизация по:**

1. **Dependency Level** (сначала low-level)
   - Classes с минимальными зависимостями
   - Core utilities
   - Base interfaces

2. **Impact** (сначала high-impact)
   - Classes используемые многими другими
   - Critical для функциональности
   - Core gameplay systems

3. **Complexity** (сначала simple)
   - Low complexity для momentum
   - Medium для основной работы
   - High/Critical в конце когда есть опыт

**Группировка:**

**Wave 1: Foundation** (1-2 дня)
- Core interfaces
- Common utilities from MedComShared
- Base data structures

**Wave 2: GAS Core** (3-5 дней)
- AbilitySystemComponent
- AttributeSets
- Base GameplayAbility classes

**Wave 3: Player Systems** (5-7 дней)
- Character base class
- Controller
- Camera/Input

**Wave 4: Abilities & Effects** (7-10 дней)
- Specific abilities
- Gameplay effects
- Tags structure

**Wave 5: Integration & Polish** (3-5 дней)
- Integration testing
- Performance optimization
- Documentation
- Cleanup legacy code

**Вывод:** Refactoring Roadmap

---

## 📋 Deliverables

После завершения анализа будут созданы:

### 1. Architecture Analysis Document
**Файл:** `Documentation/Architecture/LegacyCodeAnalysis.md`

**Содержание:**
- Общая архитектура legacy кода
- Основные подсистемы
- Паттерны проектирования
- Сильные стороны
- Проблемные области

### 2. Refactoring Plan
**Файл:** `Documentation/Architecture/RefactoringPlan.md`

**Содержание:**
- Пошаговый план рефакторинга
- Приоритизация задач
- Зависимостные группы (waves)
- Timeline estimation
- Risk assessment

### 3. Class Mapping Table
**Файл:** `Documentation/Architecture/ClassMapping.md`

**Содержание:**
- Полная таблица маппинга классов
- Legacy → New mapping
- Сложность миграции
- Breaking changes для каждого класса
- Migration notes

### 4. Dependency Graph
**Файл:** `Documentation/Architecture/DependencyGraph.md`

**Содержание:**
- Текстовое представление графа
- Критические зависимости
- Циклы (если есть)
- Уровни иерархии
- ASCII диаграммы

### 5. Code Quality Report
**Файл:** `Documentation/Architecture/CodeQualityReport.md`

**Содержание:**
- Оценка качества по категориям
- Выявленные anti-patterns
- Технический долг
- Рекомендации по улучшению

### 6. Breaking Changes Document
**Файл:** `Documentation/Architecture/BreakingChanges.md`

**Содержание:**
- Список всех breaking changes
- Влияние на существующий код
- Migration guide для каждого change
- Automated migration scripts (если возможно)

---

## 🛠️ Инструменты анализа

### Автоматические скрипты:

#### analyze-legacy.sh
```bash
#!/bin/bash
# Автоматический анализ legacy кода

echo "=== Legacy Code Analysis ==="
echo ""

echo "1. File count:"
echo "Headers: $(find Source/*/MedCom* -name "*.h" 2>/dev/null | wc -l)"
echo "Sources: $(find Source/*/MedCom* -name "*.cpp" 2>/dev/null | wc -l)"
echo ""

echo "2. Lines of code:"
find Source/*/MedCom* -name "*.cpp" -o -name "*.h" 2>/dev/null | xargs wc -l | tail -1
echo ""

echo "3. Classes (UCLASS):"
grep -r "UCLASS" Source/*/MedCom*/*.h 2>/dev/null | wc -l
echo ""

echo "4. Interfaces:"
grep -r "UINTERFACE" Source/*/MedCom*/*.h 2>/dev/null | wc -l
echo ""

echo "5. Structs:"
grep -r "USTRUCT" Source/*/MedCom*/*.h 2>/dev/null | wc -l
echo ""
```

#### find-dependencies.sh
```bash
#!/bin/bash
# Поиск зависимостей между файлами

for file in $(find Source/*/MedCom* -name "*.h"); do
    echo "=== $file ==="
    grep "^#include" "$file" | grep -v "CoreMinimal\|Generated"
    echo ""
done
```

---

## 📊 Метрики успеха

После анализа мы будем знать:

- ✅ **Точное количество** классов для миграции
- ✅ **Сложность** каждого класса (Low/Medium/High/Critical)
- ✅ **Зависимости** между классами
- ✅ **Порядок** рефакторинга
- ✅ **Время** на выполнение (estimation)
- ✅ **Риски** и проблемные области
- ✅ **Breaking changes** для планирования

---

## 🚀 После анализа

**Следующие действия:**

1. **Review с разработчиком:**
   - Обсудить findings
   - Скорректировать приоритеты
   - Уточнить unclear моменты

2. **Утверждение плана:**
   - Согласовать refactoring roadmap
   - Выбрать starting point
   - Определить definition of done

3. **Начало рефакторинга:**
   - Wave 1: Foundation classes
   - Iterative development
   - Continuous testing

---

**Дата создания:** 2025-11-24
**Дата завершения:** 2025-11-28
**Статус:** ✅ ЗАВЕРШЕНО - Анализ выполнен, миграция завершена
**Расположение:** Source/SuspenseCore/Documentation/Architecture/Planning/

---
