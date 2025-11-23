# Быстрый старт

Руководство по интеграции SuspenseCore в ваш проект Unreal Engine.

---

## Требования

- **Unreal Engine:** 5.7 или выше
- **IDE:** JetBrains Rider или Visual Studio 2022
- **Компилятор:** MSVC 14.3+ (Windows) или Clang 15+ (Mac/Linux)
- **Git:** 2.30+

---

## Установка

### Метод 1: Как плагин проекта

1. Создайте папку `Plugins` в корне вашего UE проекта (если её нет)
2. Склонируйте репозиторий:
   ```bash
   cd YourProject/Plugins
   git clone https://github.com/Houngansi/SuspenseCore.git
   ```

3. Откройте ваш `.uproject` файл и добавьте плагин:
   ```json
   {
       "Plugins": [
           {
               "Name": "SuspenseCore",
               "Enabled": true
           }
       ]
   }
   ```

4. Регенерируйте project files:
   - **Windows:** ПКМ на `.uproject` → Generate Visual Studio project files
   - **Mac/Linux:** `./GenerateProjectFiles.sh`

5. Скомпилируйте проект в вашей IDE

### Метод 2: Как движковый плагин

1. Скопируйте папку `SuspenseCore` в `[UE_ROOT]/Engine/Plugins/`
2. Перезапустите Unreal Engine
3. Включите плагин в вашем проекте: `Edit → Plugins → SuspenseCore`

---

## Настройка проекта

### 1. Добавьте зависимости в Build.cs

Откройте `YourProject.Build.cs` и добавьте необходимые модули:

```csharp
public class YourProject : ModuleRules
{
    public YourProject(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

            // SuspenseCore modules
            "SuspenseCore",
            "PlayerCore",
            "GAS",
            "EquipmentSystem",
            "InventorySystem",
            "InteractionSystem",
            "BridgeSystem",
            "UISystem",

            // Required for GAS
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks"
        });
    }
}
```

### 2. Настройте GameplayTags

Создайте или отредактируйте `Config/DefaultGameplayTags.ini`:

```ini
[/Script/GameplayTags.GameplayTagsSettings]
ImportTagsFromConfig=True
WarnOnInvalidTags=True
FastReplication=True
InvalidTagCharacters="\"\',"

; SuspenseCore Tags Structure
+GameplayTagList=(Tag="Ability",DevComment="Root tag for abilities")
+GameplayTagList=(Tag="Ability.Attack",DevComment="Attack abilities")
+GameplayTagList=(Tag="Ability.Defend",DevComment="Defensive abilities")

+GameplayTagList=(Tag="Status",DevComment="Status effects")
+GameplayTagList=(Tag="Status.Debuff",DevComment="Negative effects")
+GameplayTagList=(Tag="Status.Buff",DevComment="Positive effects")

+GameplayTagList=(Tag="Item",DevComment="Item types")
+GameplayTagList=(Tag="Item.Weapon",DevComment="Weapons")
+GameplayTagList=(Tag="Item.Consumable",DevComment="Consumables")
```

### 3. Создайте базовые классы

#### GameMode

```cpp
// YourGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "YourGameMode.generated.h"

UCLASS()
class YOURPROJECT_API AYourGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AYourGameMode();
};
```

```cpp
// YourGameMode.cpp
#include "YourGameMode.h"
#include "PlayerCore/Public/PlayerCore.h" // Example include

AYourGameMode::AYourGameMode()
{
    // Configure defaults
    PrimaryActorTick.bCanEverTick = false;
}
```

---

## Первое использование

### Создание персонажа с GAS

```cpp
// MyCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "MyCharacter.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

UCLASS()
class YOURPROJECT_API AMyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AMyCharacter();

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
    virtual void BeginPlay() override;

    // GAS Components (will be implemented via SuspenseCore)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UAttributeSet> AttributeSet;
};
```

```cpp
// MyCharacter.cpp
#include "MyCharacter.h"
#include "AbilitySystemComponent.h"
// TODO: Include SuspenseCore GAS components when implemented
// #include "GAS/Public/SuspenseAbilitySystemComponent.h"
// #include "GAS/Public/SuspenseAttributeSet.h"

AMyCharacter::AMyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create Ability System Component
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // TODO: Use SuspenseCore attribute set when available
    // AttributeSet = CreateDefaultSubobject<USuspenseAttributeSet>(TEXT("AttributeSet"));
}

void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystemComponent)
    {
        // Initialize ASC with owner
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }
}

UAbilitySystemComponent* AMyCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}
```

---

## Проверка установки

1. Запустите компиляцию проекта
2. Убедитесь, что все модули SuspenseCore загружаются:
   - Откройте `Output Log` в редакторе
   - Найдите строки вида: `LogModuleManager: Module 'SuspenseCore' loaded successfully`

3. Проверьте доступность GameplayTags:
   - `Project Settings → GameplayTags`
   - Убедитесь, что теги из конфига видны

---

## Следующие шаги

- Изучите [Architecture documentation](../Architecture/ModuleDesign.md)
- Ознакомьтесь с [Best Practices](BestPractices.md)
- Изучите [Migration Guide](Migration.md) для переноса существующего кода

---

## Частые проблемы

### Ошибка компиляции: "Module 'SuspenseCore' not found"

**Решение:** Убедитесь, что:
1. Плагин находится в `Plugins/SuspenseCore/`
2. Файл `SuspenseCore.uplugin` присутствует
3. Project files регенерированы

### Ошибка: "GameplayAbilities module not found"

**Решение:** Добавьте в `YourProject.Build.cs`:
```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "GameplayAbilities",
    "GameplayTags",
    "GameplayTasks"
});
```

### GAS не работает в multiplayer

**Решение:** Проверьте:
1. `AbilitySystemComponent->SetIsReplicated(true)`
2. Replication mode установлен корректно
3. Character/PlayerState имеет правильную replication

---

**Последнее обновление:** 2025-11-23
