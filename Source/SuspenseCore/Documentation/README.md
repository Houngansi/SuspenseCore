# SuspenseCore Plugin

**Модульная система для Unreal Engine 5.7+ с интеграцией Gameplay Ability System**

> Надежный фундамент для FPS, MMO и других мультиплеерных проектов

---

## 📋 Оглавление

- [О проекте](#о-проекте)
- [Архитектура](#архитектура)
- [Правила разработки](#правила-разработки)
- [Модули](#модули)
- [Стандарты кода](#стандарты-кода)
- [Документация](#документация)
- [Миграция](#миграция)

---

## 🎯 О проекте

**SuspenseCore** — это модульный плагин для Unreal Engine 5.7, разработанный для создания масштабируемых мультиплеерных проектов. Плагин построен на принципах:

- **Модульность**: каждая система — независимый модуль
- **Replication-first**: все системы спроектированы для сетевой игры
- **GAS-интеграция**: глубокая интеграция с Gameplay Ability System
- **Production-ready**: только чистый C++, без Blueprint-зависимостей
- **Масштабируемость**: готовность к MMO-нагрузкам через Replication Graph

**Версия:** 1.0 (Release)
**Автор:** Houngansi
**Целевая версия движка:** Unreal Engine 5.7+
**Язык:** C++ (UE Coding Standard)
**Статус миграции:** ✅ Завершена

---

## 🏗️ Архитектура

### Модульная структура

```
SuspenseCore/
├── Source/
│   ├── SuspenseCore/          # Core module - базовый функционал
│   │   └── Documentation/     # Вся документация проекта
│   ├── PlayerCore/            # Player systems - персонаж, контроллер
│   ├── GAS/                   # Gameplay Ability System integration
│   ├── EquipmentSystem/       # Equipment and weapon management
│   ├── InventorySystem/       # Inventory and item management
│   ├── InteractionSystem/     # Object interaction framework
│   ├── BridgeSystem/          # Inter-module communication
│   └── UISystem/              # User Interface framework
├── Resources/                 # Plugin resources and assets
└── SuspenseCore.uplugin       # Plugin descriptor
```

### Принципы архитектуры

1. **Слабая связность** — модули взаимодействуют через интерфейсы
2. **Высокая когезия** — каждый модуль отвечает за одну область
3. **Dependency Injection** — зависимости инжектируются через BridgeSystem
4. **Event-Driven** — системы общаются через делегаты и события
5. **Replication-Ready** — все данные готовы к сетевой репликации

---

## ⚙️ Правила разработки

### 🎓 Уровень компетенции

**Все разработчики и AI-ассистенты должны работать на уровне Senior Unreal Engine 5.7 Multiplayer Engineer:**

- Глубокое понимание сетевой архитектуры UE (Server Authority, Client Prediction)
- Экспертиза в Gameplay Ability System (GAS)
- Знание паттернов MMO и FPS разработки
- Профессиональное владение C++ и UE Coding Standard

### 📝 Стандарты кода

#### Общие правила

1. **Только C++** — никаких Blueprint-зависимостей в runtime-логике
2. **Полные файлы** — всегда предоставлять полный код файла без сокращений
3. **Английские комментарии** — inline-комментарии только на английском
4. **Русские объяснения** — все пояснения и документация на русском
5. **UE Coding Standard** — строгое следование [официальному стандарту](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)

#### Именование

```cpp
// Classes: PascalCase с префиксами U/A/F/E/I
class SUSPENSECORE_API UInventoryComponent : public UActorComponent { };
class PLAYERCORE_API APlayerCharacter : public ACharacter { };
struct FItemData { };
enum class EEquipmentSlot : uint8 { };
class IInteractable { }; // Interface

// Functions: PascalCase (UE style)
void HandleItemPickup(const FItemData& Item);
bool CanEquipItem(const UItemInstance* Item) const;

// Variables: PascalCase with descriptive names
int32 CurrentAmmoCount;
float MaxHealthPoints;
TArray<UAbilitySystemComponent*> RegisteredComponents;

// Member variables: prefix based on type
bool bIsEquipped;           // bool: b prefix
int32 MaxStackSize;         // no prefix for primitives in structs
UObject* CachedOwner;       // UObject*: no prefix
APlayerController* PC;      // Common abbreviations allowed
```

#### Комментарии

```cpp
// Short, precise English comments for complex logic
// Avoid obvious comments like "// Set velocity" for SetVelocity()

// GOOD: Explains WHY
// Clamp to max stack size to prevent inventory exploits
CurrentStack = FMath::Min(CurrentStack, MaxStackSize);

// BAD: Explains WHAT (code is self-documenting)
// Add item to array
Items.Add(NewItem);

// Documentation comments for API
/**
 * Attempts to equip item to specified slot
 * @param Item Item to equip
 * @param Slot Target equipment slot
 * @return true if successfully equipped, false otherwise
 */
UFUNCTION(BlueprintCallable, Category = "Equipment")
bool TryEquipItem(UItemInstance* Item, EEquipmentSlot Slot);
```

#### Replication

```cpp
// Always declare replication in header
class PLAYERCORE_API APlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    // Replicated property
    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
    AWeaponBase* CurrentWeapon;

    UFUNCTION()
    void OnRep_CurrentWeapon(AWeaponBase* OldWeapon);

    // Setup replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

// Implementation
void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(APlayerCharacter, CurrentWeapon, COND_OwnerOnly);
}
```

### 🔄 Процесс разработки

#### Workflow

1. **Анализ задачи** — понять техническую цель
2. **Связь с контекстом** — соотнести с архитектурой проекта
3. **Декомпозиция** — разбить на тестируемые подзадачи
4. **Реализация** — писать идиоматичный, компилируемый код
5. **Тестирование** — мысленно протестировать edge cases
6. **Оптимизация** — убедиться в производительности
7. **Документация** — добавить inline-комментарии

#### Критерии качества

Каждое решение должно быть **world-class** по всем параметрам:

- ✅ **Correctness** — код работает корректно
- ✅ **Optimization** — оптимален для production
- ✅ **Scalability** — масштабируется до MMO нагрузок
- ✅ **Readability** — читаем и понятен
- ✅ **Maintainability** — легко поддерживать и расширять
- ✅ **Replication** — корректно работает в сети

**Если решение не соответствует этим критериям — оно должно быть переработано.**

### 🚫 Запреты

1. **НЕ спрашивать подтверждения** — принимайте разумные решения и документируйте их
2. **НЕ оставлять TODO** — реализуйте сразу или создайте отдельную задачу
3. **НЕ использовать плейсхолдеры** — только рабочий код
4. **НЕ сокращать код** — всегда полные файлы
5. **НЕ добавлять Blueprint-зависимости** — только C++ в runtime

### 📦 Управление зависимостями

#### Build.cs конфигурация

```csharp
// Правильная структура Build.cs
public class ModuleName : ModuleRules
{
    public ModuleName(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Public: exposed to other modules
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks"
        });

        // Private: internal only
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "NetCore",
            "ReplicationGraph"
        });
    }
}
```

### 🧪 Тестирование

1. **Unit Tests** — для критичной логики (инвентарь, абилити)
2. **Multiplayer Tests** — всегда тестировать с репликацией
3. **Performance Tests** — профилировать перед коммитом
4. **Edge Cases** — проверять граничные условия

---

## 📚 Модули

### SuspenseCore (Ядро)

**Ответственность:** Базовый функционал, общие утилиты, core-интерфейсы

**Зависимости:**
- Core
- CoreUObject
- Engine

**Основные компоненты:**
- Logging system
- Config management
- Core interfaces
- Common utilities
- **Documentation** (все руководства и API reference)

---

### PlayerCore (Игрок)

**Ответственность:** Системы персонажа и контроллера

**Зависимости:**
- SuspenseCore
- GAS
- GameplayAbilities

**Планируемые классы:**
- `APlayerCharacterBase` — базовый класс персонажа
- `APlayerControllerBase` — базовый контроллер
- `UPlayerCameraComponent` — система камеры

---

### GAS (Gameplay Ability System)

**Ответственность:** Интеграция и расширение Gameplay Ability System

**Зависимости:**
- SuspenseCore
- GameplayAbilities
- GameplayTags
- GameplayTasks

**Планируемые классы:**
- `USuspenseAbilitySystemComponent`
- `USuspenseGameplayAbility`
- `USuspenseAttributeSet`

---

### EquipmentSystem (Экипировка)

**Ответственность:** Управление оружием и экипировкой

**Зависимости:**
- SuspenseCore
- PlayerCore
- GAS
- InventorySystem

**Планируемые классы:**
- `UEquipmentManagerComponent`
- `AWeaponBase`
- `UEquipmentInstance`

---

### InventorySystem (Инвентарь)

**Ответственность:** Управление предметами и инвентарем

**Зависимости:**
- SuspenseCore
- GAS

**Планируемые классы:**
- `UInventoryComponent`
- `UItemDefinition`
- `UItemInstance`

---

### InteractionSystem (Взаимодействия)

**Ответственность:** Система взаимодействия с объектами мира

**Зависимости:**
- SuspenseCore
- PlayerCore

**Планируемые классы:**
- `IInteractable` (interface)
- `UInteractionComponent`
- `AInteractableActor`

---

### BridgeSystem (Мост)

**Ответственность:** Коммуникация между модулями, Dependency Injection

**Зависимости:**
- SuspenseCore

**Планируемые классы:**
- `UModuleBridge`
- `UServiceRegistry`
- `USuspenseEventBus`

---

### UISystem (UI)

**Ответственность:** Пользовательский интерфейс

**Зависимости:**
- SuspenseCore
- UMG
- Slate
- SlateCore

**Планируемые классы:**
- `USuspenseHUD`
- `UInventoryWidget`
- `UEquipmentWidget`

---

## 📖 Документация

### Структура документации

```
Source/SuspenseCore/Documentation/
├── README.md                  ← Этот файл (главный)
├── Changelog.md               ← История изменений
├── Architecture/
│   ├── ModuleDesign.md       ← Дизайн модулей
│   ├── Replication.md        ← Стратегия репликации (TODO)
│   └── GASIntegration.md     ← Интеграция GAS (TODO)
├── API/
│   ├── README.md             ← API Reference индекс
│   └── [Module].md           ← API для каждого модуля (TODO)
└── Guides/
    ├── QuickStart.md         ← Быстрый старт
    ├── Migration.md          ← Руководство по миграции
    └── BestPractices.md      ← Лучшие практики
```

### Доступные руководства

- **[QuickStart.md](Guides/QuickStart.md)** — Быстрый старт и интеграция плагина
- **[Migration.md](Guides/Migration.md)** — Руководство по миграции кода
- **[BestPractices.md](Guides/BestPractices.md)** — Best practices UE5/GAS
- **[AddingLegacyCode.md](Guides/AddingLegacyCode.md)** — Как добавить старый код для рефакторинга

### Правила документации

1. **Актуальность** — обновлять при каждом значимом изменении
2. **Полнота** — документировать все public API
3. **Примеры** — всегда приводить примеры использования
4. **Русский язык** — вся документация на русском
5. **Markdown** — использовать GitHub Flavored Markdown

---

## 🔄 Миграция

### Процесс миграции из старого проекта

**Стратегия:** Постепенный перенос с обновлением неймспейсов и архитектуры

#### Этапы миграции

1. **Анализ файла** — понять назначение и зависимости
2. **Определение модуля** — выбрать целевой модуль
3. **Обновление namespace** — привести к новым стандартам
4. **Рефакторинг** — адаптировать под новую архитектуру
5. **Тестирование** — убедиться в работоспособности
6. **Документация** — обновить Changelog.md

#### Чеклист миграции файла

- [ ] Файл помещен в правильный модуль
- [ ] Copyright header обновлен
- [ ] Namespace соответствует модулю
- [ ] Зависимости добавлены в Build.cs
- [ ] Code style соответствует UE Standard
- [ ] Комментарии на английском
- [ ] Репликация настроена (если нужна)
- [ ] GAS интеграция (если применимо)
- [ ] Файл скомпилирован без ошибок
- [ ] Документация обновлена

**Подробнее:** См. [Guides/Migration.md](Guides/Migration.md)

---

## 🛠️ Инструменты и окружение

### Требования

- **Unreal Engine:** 5.7+
- **IDE:** Rider / Visual Studio 2022
- **Компилятор:** MSVC 14.3+ / Clang 15+
- **Git:** 2.30+

### Рекомендуемые плагины IDE

**JetBrains Rider:**
- ReSharper C++
- Unreal Engine Support

**Visual Studio:**
- Visual Assist
- UnrealVS Extension

---

## 📊 Статус проекта

**Текущая версия:** 1.0 Release
**Статус:** ✅ Миграция завершена, код компилируется
**Последнее обновление:** 2025-11-28

### Готовность модулей

| Модуль | Структура | Build.cs | Миграция | Компиляция | Статус |
|--------|-----------|----------|----------|------------|--------|
| SuspenseCore | ✅ | ✅ | ✅ | ✅ | Базовый модуль, документация |
| PlayerCore | ✅ | ✅ | ✅ | ✅ | Персонаж, контроллер, GameMode |
| GAS | ✅ | ✅ | ✅ | ✅ | Gameplay Ability System интеграция |
| EquipmentSystem | ✅ | ✅ | ✅ | ✅ | Система экипировки и оружия |
| InventorySystem | ✅ | ✅ | ✅ | ✅ | Инвентарь и предметы |
| InteractionSystem | ✅ | ✅ | ✅ | ✅ | Взаимодействие с объектами |
| BridgeSystem | ✅ | ✅ | ✅ | ✅ | Межмодульная коммуникация |
| UISystem | ✅ | ✅ | ✅ | ✅ | Пользовательский интерфейс |

**Легенда:**
- ✅ Готово/Завершено
- 🔄 В процессе
- ⏳ Запланировано

---

## 🎯 Статус миграции

### ✅ Завершённые этапы

1. ✅ Структура проекта создана
2. ✅ Документация создана и размещена в модуле SuspenseCore
3. ✅ Build.cs всех модулей настроены с правильными зависимостями
4. ✅ **Полная миграция MedCom → Suspense завершена**
5. ✅ Весь код компилируется без ошибок

### ✅ Мигрированные модули

| Волна | Модуль | LOC | Файлов | Статус |
|-------|--------|-----|--------|--------|
| Wave 2 | MedComInteraction → InteractionSystem | 3,486 | 12 | ✅ Завершено |
| Wave 2 | MedComCore → PlayerCore | 8,697 | 17 | ✅ Завершено |
| Wave 2 | MedComGAS → GAS | 8,003 | 46 | ✅ Завершено |
| Wave 3 | MedComInventory → InventorySystem | 27,862 | 43 | ✅ Завершено |
| Wave 4 | MedComEquipment → EquipmentSystem | 54,213 | 80 | ✅ Завершено |
| Wave 5 | MedComUI → UISystem | 26,706 | 48 | ✅ Завершено |
| Wave 1 | MedComShared → BridgeSystem | 26,680 | 123 | ✅ Завершено |

**Итого:** ~155,000 LOC успешно мигрировано

### 🎯 Следующие шаги (Post-Migration)

- [ ] Runtime тестирование функционала
- [ ] Blueprint compatibility проверка
- [ ] Интеграция Replication Graph для MMO-нагрузок
- [ ] Performance profiling и оптимизация
- [ ] Comprehensive unit testing
- [ ] Production deployment

---

## 📞 Контакты

**Автор:** Houngansi
**Repository:** https://github.com/Houngansi/SuspenseCore

---

## 📄 Лицензия

Proprietary - All rights reserved

---

**Последнее обновление:** 2025-11-28
**Версия документа:** 2.0 (Post-Migration)
**Расположение:** Source/SuspenseCore/Documentation/

---
{
"Name": "InteractionSystem",
"Type": "Runtime",
"LoadingPhase": "Default"
},
{
"Name": "InventorySystem",
"Type": "Runtime",
"LoadingPhase": "Default"
},
{
"Name": "EquipmentSystem",
"Type": "Runtime",
"LoadingPhase": "Default"
},
{
"Name": "UISystem",
"Type": "Runtime",
"LoadingPhase": "Default"
}