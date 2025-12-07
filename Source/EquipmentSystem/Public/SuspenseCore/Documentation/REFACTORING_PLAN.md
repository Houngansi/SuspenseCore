# SuspenseCore Refactoring Plan

## Обзор

Этот документ описывает план миграции легаси кода EquipmentSystem на современную архитектуру SuspenseCore, следуя принципам из `BestPractices.md`.

**Цель:** Создание AAA MMO Shooter архитектуры с высокой масштабируемостью, безопасностью и производительностью.

---

## Архитектурные принципы

### 1. Event Bus (Децентрализованная коммуникация)

**Было (Legacy):**
```cpp
// Прямые вызовы между компонентами
WeaponComponent->OnAmmoChanged.AddDynamic(UIWidget, &UWidget::UpdateAmmo);
InventoryComponent->OnItemAdded.AddDynamic(EquipmentActor, &AEquipment::HandleItem);
```

**Станет (SuspenseCore):**
```cpp
// Все события через EventBus
EventBus->Publish(SUSPENSE_CORE_EVENT_WEAPON_AMMO_CHANGED, AmmoData);
EventBus->Subscribe(SUSPENSE_CORE_EVENT_INVENTORY_ITEM_ADDED, this, &MyClass::HandleItem);
```

**Преимущества:**
- Слабая связность между модулями
- Простое тестирование (mock EventBus)
- Логирование всех событий в одном месте
- Поддержка приоритетов обработки

### 2. Dependency Injection (ServiceLocator)

**Было (Legacy):**
```cpp
// Прямой доступ к подсистемам
UGameInstance* GI = GetGameInstance();
USuspenseItemManager* IM = GI->GetSubsystem<USuspenseItemManager>();
```

**Станет (SuspenseCore):**
```cpp
// Через ServiceLocator
USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
auto* DataService = Provider->GetService<USuspenseCoreEquipmentDataService>();

// Или макросами
SUSPENSE_GET_SERVICE(this, USuspenseCoreEquipmentDataService, DataService);
```

**Преимущества:**
- Централизованное управление lifecycle сервисов
- Простая замена для тестирования
- Гарантированный порядок инициализации
- Lazy initialization

### 3. GameplayTags (Идентификация состояний)

**Было (Legacy):**
```cpp
// Boolean флаги
bool bIsReloading;
bool bIsFiring;
EWeaponState CurrentState;
```

**Станет (SuspenseCore):**
```cpp
// GameplayTags
FGameplayTag State_Weapon_Reloading = FGameplayTag::RequestGameplayTag("Weapon.State.Reloading");
FGameplayTag State_Weapon_Firing = FGameplayTag::RequestGameplayTag("Weapon.State.Firing");

// Проверка через GAS
ASC->HasMatchingGameplayTag(State_Weapon_Reloading);
```

**Преимущества:**
- Интеграция с GAS
- Автоматическая репликация
- Расширяемость без изменения кода
- Поддержка в редакторе

### 4. Service-Oriented Architecture

**Было (Legacy):**
```cpp
// Логика в акторах/компонентах
void ASuspenseWeaponActor::GrantAbilities() {
    // 200 строк логики в акторе
}
```

**Станет (SuspenseCore):**
```cpp
// Тонкий актор + сервис
void ASuspenseCoreWeaponActor::OnEquipped() {
    // Только уведомление сервиса
    auto* AbilityService = ServiceLocator->GetService<USuspenseCoreEquipmentAbilityService>();
    AbilityService->GrantWeaponAbilities(this, CachedASC);
}
```

---

## Этапы рефакторинга

### Этап 1: Core Infrastructure (ВЫПОЛНЕН)

✅ Создана структура директорий SuspenseCore
✅ Созданы базовые классы (SuspenseCoreEquipmentActor, SuspenseCoreWeaponActor)
✅ Созданы компоненты с поддержкой EventBus
✅ Созданы сервисы с поддержкой DI
✅ Создан SystemCoordinator для управления lifecycle

### Этап 2: EventBus Integration

**Задачи:**
1. Заменить прямые делегаты на EventBus публикации
2. Подписать UI виджеты на события через EventBus
3. Добавить логирование событий
4. Настроить приоритеты обработки

**Файлы для модификации:**
- `SuspenseCoreEquipmentComponentBase.cpp` - BroadcastX методы
- Все Components - использовать BroadcastEquipmentEvent
- UI виджеты - подписка через EventBus

**Пример миграции:**
```cpp
// ДО
OnAmmoChanged.Broadcast(CurrentAmmo, RemainingAmmo);

// ПОСЛЕ
BroadcastEquipmentEvent(
    FGameplayTag::RequestGameplayTag("SuspenseCore.Event.Weapon.AmmoChanged"),
    FString::Printf(TEXT("Current:%f,Remaining:%f"), CurrentAmmo, RemainingAmmo)
);
```

### Этап 3: ServiceLocator Integration

**Задачи:**
1. Зарегистрировать все сервисы в SystemCoordinator
2. Заменить прямой доступ к подсистемам на ServiceLocator
3. Добавить ISuspenseCoreServiceConsumer интерфейс где нужно
4. Настроить порядок инициализации сервисов

**Файлы для модификации:**
- `SuspenseCoreSystemCoordinator.cpp` - RegisterCoreServices
- Все Components - использовать ServiceLocator
- Все Actors - инъекция сервисов

**Пример миграции:**
```cpp
// ДО
USuspenseItemManager* IM = GetGameInstance()->GetSubsystem<USuspenseItemManager>();

// ПОСЛЕ
auto* Provider = USuspenseCoreServiceProvider::Get(this);
auto* DataService = Provider->GetService<USuspenseCoreEquipmentDataService>();
```

### Этап 4: GameplayTags Migration

**Задачи:**
1. Определить все теги в DefaultGameplayTags.ini
2. Создать кеш тегов для производительности
3. Заменить enum/bool на теги
4. Интегрировать с GAS для репликации

**Теги для добавления:**
```ini
; Equipment States
+GameplayTagList=(Tag="Equipment.State.Inactive")
+GameplayTagList=(Tag="Equipment.State.Equipped")
+GameplayTagList=(Tag="Equipment.State.Ready")

; Weapon States
+GameplayTagList=(Tag="Weapon.State.Idle")
+GameplayTagList=(Tag="Weapon.State.Firing")
+GameplayTagList=(Tag="Weapon.State.Reloading")
+GameplayTagList=(Tag="Weapon.State.Aiming")

; Events
+GameplayTagList=(Tag="SuspenseCore.Event.Equipment.Equipped")
+GameplayTagList=(Tag="SuspenseCore.Event.Equipment.Unequipped")
+GameplayTagList=(Tag="SuspenseCore.Event.Weapon.Fired")
+GameplayTagList=(Tag="SuspenseCore.Event.Weapon.Reloaded")
+GameplayTagList=(Tag="SuspenseCore.Event.Weapon.AmmoChanged")
```

### Этап 5: Security Hardening

**Задачи:**
1. Добавить Server RPC с WithValidation для всех операций
2. Реализовать Rate Limiting через SecurityValidator
3. Добавить логирование подозрительной активности
4. Настроить уровни безопасности операций

**Пример:**
```cpp
UFUNCTION(Server, Reliable, WithValidation)
void Server_EquipItem(FName ItemID, int32 SlotIndex);

bool ACharacter::Server_EquipItem_Validate(FName ItemID, int32 SlotIndex)
{
    SUSPENSE_VALIDATE_ITEM_RPC(ItemID, SlotIndex);
    SUSPENSE_CHECK_RATE_LIMIT(this, EquipItem, 10.0f);
    return true;
}
```

### Этап 6: Performance Optimization

**Задачи:**
1. Отключить Tick где возможно (уже сделано)
2. Реализовать Object Pooling для projectiles
3. Настроить Dormancy для Equipment Actors
4. Оптимизировать репликацию с ReplicationGraph

**Checklist:**
- [ ] PrimaryActorTick.bCanEverTick = false для всех Equipment
- [ ] Projectile pool в WeaponService
- [ ] DORM_DormantAll для holstered weapons
- [ ] Правильные COND_ для DOREPLIFETIME

### Этап 7: Testing & Validation

**Задачи:**
1. Написать unit тесты для сервисов
2. Написать integration тесты для EventBus
3. Написать multiplayer тесты
4. Провести нагрузочное тестирование

**Тестовые сценарии:**
- Equipment equip/unequip cycle
- Weapon fire/reload/ammo management
- Multi-player sync verification
- 100+ concurrent players test

---

## Приоритеты файлов для рефакторинга

### Высокий приоритет (критический путь)

1. **SuspenseCoreSystemCoordinator** - точка входа, управление lifecycle
2. **SuspenseCoreEquipmentComponentBase** - база для всех компонентов
3. **SuspenseCoreEquipmentDataService** - SSOT для данных
4. **SuspenseCoreEquipmentAbilityService** - GAS интеграция

### Средний приоритет (core gameplay)

5. **SuspenseCoreWeaponAmmoComponent** - патроны
6. **SuspenseCoreWeaponFireModeComponent** - режимы огня
7. **SuspenseCoreEquipmentNetworkService** - репликация
8. **SuspenseCoreEquipmentValidationService** - валидация

### Низкий приоритет (polish)

9. **SuspenseCoreEquipmentVisualizationService** - визуалы
10. **SuspenseCoreWeaponStanceComponent** - стойки
11. Остальные компоненты Core/

---

## Метрики успеха

| Метрика | Текущее | Цель |
|---------|---------|------|
| Связность модулей | Высокая | Низкая (через EventBus) |
| Покрытие тестами | ~0% | >70% |
| Server Authority | Частичная | 100% |
| Rate Limiting | Нет | Все операции |
| Tick компоненты | ~50% | <5% |
| Object Pooling | Нет | Projectiles, VFX |

---

## Риски и митигации

| Риск | Вероятность | Влияние | Митигация |
|------|-------------|---------|-----------|
| Регрессия функционала | Средняя | Высокое | Сохранить легаси код, постепенная миграция |
| Performance degradation | Низкая | Высокое | Benchmark перед/после каждого этапа |
| Breaking changes API | Высокая | Среднее | Версионирование, deprecation warnings |

---

## Примечания

- Легаси код НЕ удаляется - служит reference для структуры и логики
- Каждый этап должен быть отдельным PR с code review
- Обязательное тестирование в multiplayer перед merge
- Документировать все breaking changes

---

**Дата создания:** 2025-12-07
**Автор:** Claude Code Assistant
**Версия:** 1.0
