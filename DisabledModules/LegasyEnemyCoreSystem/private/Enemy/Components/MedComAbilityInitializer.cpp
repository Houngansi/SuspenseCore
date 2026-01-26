#include "Core/Enemy/Components/MedComAbilityInitializer.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"

UMedComAbilityInitializer::UMedComAbilityInitializer() : OwnerASC(nullptr)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMedComAbilityInitializer::BeginPlay()
{
    Super::BeginPlay();

    // Получаем ASC сразу в BeginPlay для раннего доступа
    if (GetOwner())
    {
       IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(GetOwner());
       if (ASCInterface)
       {
          OwnerASC = ASCInterface->GetAbilitySystemComponent();
          UE_LOG(LogTemp, Warning, TEXT("AbilityInitializer BeginPlay: Owner=%s, ASC=%s"), 
                 *GetOwner()->GetName(), 
                 OwnerASC ? TEXT("VALID") : TEXT("NULL"));
       }
       else
       {
          UE_LOG(LogTemp, Error, TEXT("Owner не реализует IAbilitySystemInterface"));
       }
    }
}

void UMedComAbilityInitializer::InitializeAbilities()
{
    // Более тщательная проверка компонентов перед инициализацией
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeAbilities: Owner отсутствует или не authority"));
        return;
    }
    
    // Если ASC не был получен ранее, пробуем получить его здесь
    if (!OwnerASC)
    {
        IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(GetOwner());
        if (ASCInterface)
        {
            OwnerASC = ASCInterface->GetAbilitySystemComponent();
            UE_LOG(LogTemp, Warning, TEXT("InitializeAbilities: Получен ASC во время инициализации"));
        }
    }
    
    if (!OwnerASC)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeAbilities: ASC не найден. Owner=%s, Authority=%s"), 
               *GetOwner()->GetName(),
               GetOwner()->HasAuthority() ? TEXT("ДА") : TEXT("НЕТ"));
        return;
    }

    // Очищаем предыдущие записи
    AbilityHandles.Empty();

    // Проверка существования классов способностей
    UE_LOG(LogTemp, Warning, TEXT("AbilityInitializer: FireAbility=%s, BurstFireAbility=%s, AutoFireAbility=%s, ReloadAbility=%s"),
           FireAbility ? *FireAbility->GetName() : TEXT("NULL"),
           BurstFireAbility ? *BurstFireAbility->GetName() : TEXT("NULL"),
           AutoFireAbility ? *AutoFireAbility->GetName() : TEXT("NULL"),
           ReloadAbility ? *ReloadAbility->GetName() : TEXT("NULL"));

    // Выдаём основные способности с добавлением тегов
    GrantAbility("Fire", FireAbility, true);
    GrantAbility("BurstFire", BurstFireAbility, true);
    GrantAbility("AutoFire", AutoFireAbility, true);
    GrantAbility("Reload", ReloadAbility);
    GrantAbility("SwitchFireMode", SwitchFireModeAbility);
    
    // Выдаём дополнительные способности
    for (int32 i = 0; i < AdditionalAbilities.Num(); ++i)
    {
        GrantAbility(FString::Printf(TEXT("Additional_%d"), i), AdditionalAbilities[i]);
    }

    // Проверка, были ли выданы способности
    UE_LOG(LogTemp, Warning, TEXT("После выдачи: FireHandle существует=%s"), 
           (AbilityHandles.Contains("Fire") && AbilityHandles["Fire"].IsValid()) ? TEXT("ДА") : TEXT("НЕТ"));
    UE_LOG(LogTemp, Warning, TEXT("После выдачи: BurstFireHandle существует=%s"), 
           (AbilityHandles.Contains("BurstFire") && AbilityHandles["BurstFire"].IsValid()) ? TEXT("ДА") : TEXT("НЕТ"));
    UE_LOG(LogTemp, Warning, TEXT("После выдачи: AutoFireHandle существует=%s"), 
           (AbilityHandles.Contains("AutoFire") && AbilityHandles["AutoFire"].IsValid()) ? TEXT("ДА") : TEXT("НЕТ"));

    // Дополнительная проверка абилок
    TArray<FGameplayAbilitySpec> Abilities = OwnerASC->GetActivatableAbilities();
    UE_LOG(LogTemp, Warning, TEXT("Всего активируемых способностей: %d"), Abilities.Num());
    
    for (const FGameplayAbilitySpec& Spec : Abilities)
    {
        if (Spec.Ability)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - Способность: %s"), *Spec.Ability->GetName());
            // Выводим теги способности
            FString TagsString;
            for (const FGameplayTag& Tag : Spec.Ability->AbilityTags.GetGameplayTagArray())
            {
                TagsString.Append(Tag.ToString() + " ");
            }
            UE_LOG(LogTemp, Warning, TEXT("    Теги: %s"), *TagsString);
        }
    }
}

void UMedComAbilityInitializer::GrantAbility(const FString& KeyName, TSubclassOf<UGameplayAbility> AbilityClass, bool bAddFireTags)
{
    if (!AbilityClass || !OwnerASC) 
    {
        UE_LOG(LogTemp, Error, TEXT("GrantAbility: Невозможно выдать %s - AbilityClass=%s, OwnerASC=%s"), 
               *KeyName,
               AbilityClass ? TEXT("VALID") : TEXT("NULL"),
               OwnerASC ? TEXT("VALID") : TEXT("NULL"));
        return;
    }

    // Создаем спецификацию способности
    FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, GetOwner());
    
    // Важный момент! Добавляем явные теги для способностей стрельбы
    if (bAddFireTags && AbilitySpec.Ability)
    {
        // Получаем контейнер динамических тегов спецификации
        FGameplayTagContainer& DynamicTags = AbilitySpec.GetDynamicSpecSourceTags();
        
        // Явно добавляем теги для поиска
        DynamicTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Fire"));
        DynamicTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Shoot"));
        
        // Добавляем специальные теги для разных режимов стрельбы
        if (KeyName == "Fire")
        {
            DynamicTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Single"));
        }
        else if (KeyName == "BurstFire")
        {
            DynamicTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Burst"));
        }
        else if (KeyName == "AutoFire")
        {
            DynamicTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Auto"));
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Добавлены явные теги к способности %s"), *KeyName);
    }
    else if (KeyName == "Reload" && AbilitySpec.Ability)
    {
        // Получаем контейнер динамических тегов спецификации
        FGameplayTagContainer& DynamicTags = AbilitySpec.GetDynamicSpecSourceTags();
        DynamicTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Reload"));
    }
    else if (KeyName == "SwitchFireMode" && AbilitySpec.Ability)
    {
        // Получаем контейнер динамических тегов спецификации
        FGameplayTagContainer& DynamicTags = AbilitySpec.GetDynamicSpecSourceTags();
        DynamicTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.SwitchFireMode"));
    }
    
    // Выдаем способность
    FGameplayAbilitySpecHandle Handle = OwnerASC->GiveAbility(AbilitySpec);
    AbilityHandles.Add(FName(*KeyName), Handle);
    
    // Проверяем, что хендл валидный
    UE_LOG(LogTemp, Warning, TEXT("Выдана способность %s, хендл валидный: %s"), 
           *KeyName, Handle.IsValid() ? TEXT("ДА") : TEXT("НЕТ"));
}