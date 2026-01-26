#include "Core/Enemy/Components/MedComWeaponHandlerComponent.h"
#include "Equipment/Components/MedComEquipmentComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Equipment/Base/WeaponActor.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/Enemy/Components/MedComAbilityInitializer.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

UMedComWeaponHandlerComponent::UMedComWeaponHandlerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    CurrentFireMode = EFireMode::Single;
}

void UMedComWeaponHandlerComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeWeaponHandler();
}

void UMedComWeaponHandlerComponent::InitializeWeaponHandler()
{
    // Получаем владельца
    AActor* MyOwner = GetOwner();
    if (!MyOwner)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeWeaponHandler: Нет владельца!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("InitializeWeaponHandler: Owner=%s"), *MyOwner->GetName());

    // Получаем AbilitySystemComponent через интерфейс
    IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(MyOwner);
    if (ASCInterface)
    {
        OwnerASC = ASCInterface->GetAbilitySystemComponent();
        UE_LOG(LogTemp, Warning, TEXT("InitializeWeaponHandler: ASC=%s"), 
               OwnerASC ? TEXT("VALID") : TEXT("NULL"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeWeaponHandler: Owner не реализует IAbilitySystemInterface"));
    }

    // ИСПРАВЛЕНИЕ: Очищаем старую ссылку на оружие и атрибуты
    CurrentWeapon = nullptr;
    CurrentWeaponAttributeSet = nullptr;

    // Пытаемся получить активное оружие - сначала через AMedComEnemyCharacter
    if (AMedComEnemyCharacter* EnemyOwner = Cast<AMedComEnemyCharacter>(MyOwner))
    {
        CurrentWeapon = EnemyOwner->GetCurrentWeapon();
        UE_LOG(LogTemp, Warning, TEXT("InitializeWeaponHandler: Получено оружие из EnemyCharacter: %s"), 
               CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
    }
    
    // Если не получили оружие через Enemy, пробуем через EquipmentComponent
    if (!CurrentWeapon)
    {
        if (UMedComEquipmentComponent* EquipComp = MyOwner->FindComponentByClass<UMedComEquipmentComponent>())
        {
            CurrentWeapon = EquipComp->GetActiveWeapon();
            UE_LOG(LogTemp, Warning, TEXT("InitializeWeaponHandler: Получено оружие из EquipmentComponent: %s"), 
                   CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
        }
    }
    
    // Если нашли оружие, обновляем ссылку на атрибуты
    if (CurrentWeapon)
    {
        CurrentWeaponAttributeSet = CurrentWeapon->GetWeaponAttributeSet();
        if (CurrentWeaponAttributeSet)
        {
            UE_LOG(LogTemp, Warning, TEXT("InitializeWeaponHandler: Оружие имеет валидный WeaponAttributeSet"));
            
            // Логируем текущие параметры оружия
            UE_LOG(LogTemp, Warning, TEXT("Weapon stats: Ammo=%.1f/%.1f, Reserve=%.1f, Damage=%.1f"),
                  CurrentWeaponAttributeSet->GetCurrentAmmo(),
                  CurrentWeaponAttributeSet->GetMagazineSize(),
                  CurrentWeaponAttributeSet->GetRemainingAmmo(),
                  CurrentWeaponAttributeSet->GetDamage());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("InitializeWeaponHandler: Оружие не имеет WeaponAttributeSet!"));
        }
    }

    // Получаем хендлы способностей из AbilityInitializer
    if (UMedComAbilityInitializer* InitComp = MyOwner->FindComponentByClass<UMedComAbilityInitializer>())
    {
        // Получение хендлов способностей
        AbilityHandles = InitComp->GetAbilityHandles();
        
        // Проверка наличия хендла Fire
        FGameplayAbilitySpecHandle* FireHandle = AbilityHandles.Find("Fire");
        UE_LOG(LogTemp, Warning, TEXT("WeaponHandler Init: Хендл способности Fire существует = %s"), 
               (FireHandle && FireHandle->IsValid()) ? TEXT("ДА") : TEXT("НЕТ"));
        
        // Если не нашли хендл Fire, просим переинициализировать способности
        if (!FireHandle || !FireHandle->IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("WeaponHandler Init: Повторная инициализация способностей"));
            InitComp->InitializeAbilities();
            
            // После переинициализации, получаем ручки заново
            AbilityHandles = InitComp->GetAbilityHandles();
            
            // Проверяем еще раз
            FireHandle = AbilityHandles.Find("Fire");
            UE_LOG(LogTemp, Warning, TEXT("WeaponHandler Init: После реинициализации хендл Fire существует = %s"), 
                   (FireHandle && FireHandle->IsValid()) ? TEXT("ДА") : TEXT("НЕТ"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeWeaponHandler: Не найден компонент AbilityInitializer"));
    }
    
    // ИСПРАВЛЕНИЕ: Очищаем все существующие теги режимов огня и добавляем нужные
    if (OwnerASC)
    {
        // Получаем текущие теги для логирования
        FGameplayTagContainer CurrentTags;
        OwnerASC->GetOwnedGameplayTags(CurrentTags);
        UE_LOG(LogTemp, Warning, TEXT("WeaponHandler Init: Теги ДО обновления: %s"), *CurrentTags.ToString());
        
        // Создаем контейнер тегов для удаления
        FGameplayTagContainer TagsToRemove;
        TagsToRemove.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
        TagsToRemove.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
        TagsToRemove.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
        
        // Удаляем все теги режимов огня одной операцией
        OwnerASC->RemoveLooseGameplayTags(TagsToRemove);
        
        // Добавляем теги по умолчанию
        if (!OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.HasWeapon")))
        {
            OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.HasWeapon"));
            UE_LOG(LogTemp, Warning, TEXT("WeaponHandler Init: Добавлен тег Weapon.HasWeapon"));
        }
        
        // По умолчанию одиночный режим
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
        UE_LOG(LogTemp, Warning, TEXT("WeaponHandler Init: Добавлен тег Weapon.FireMode.Single (режим по умолчанию)"));
        
        // Устанавливаем соответствующее значение перечисления
        CurrentFireMode = EFireMode::Single;
        
        // Проверяем теги после изменений
        FGameplayTagContainer FinalTags;
        OwnerASC->GetOwnedGameplayTags(FinalTags);
        UE_LOG(LogTemp, Warning, TEXT("WeaponHandler Init: Теги ПОСЛЕ обновления: %s"), *FinalTags.ToString());
    }
    
    UE_LOG(LogTemp, Warning, TEXT("InitializeWeaponHandler: Финальное состояние - ASC=%s, CurrentWeapon=%s"), 
           OwnerASC ? TEXT("VALID") : TEXT("NULL"),
           CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
}

bool UMedComWeaponHandlerComponent::TryFireWeapon()
{
    // Проверяем, что мы на сервере и есть ASC
    if (!OwnerASC || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Owner ASC is null или не authority!"));
        return false;
    }

    // Проверяем наличие оружия и подробно логируем результат
    UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Owner=%s, Has weapon=%s"), 
           *GetOwner()->GetName(),
           CurrentWeapon ? TEXT("YES") : TEXT("NO"));

    // Если оружие не найдено, попробуем восстановить ссылку из владельца
    if (!CurrentWeapon)
    {
        // Попытка получить оружие через AMedComEnemyCharacter
        if (AMedComEnemyCharacter* EnemyOwner = Cast<AMedComEnemyCharacter>(GetOwner()))
        {
            CurrentWeapon = EnemyOwner->GetCurrentWeapon();
            UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Восстановлена ссылка на оружие: %s"), 
                   CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
        }
        
        // Если всё еще нет оружия, пробуем через EquipmentComponent
        if (!CurrentWeapon)
        {
            if (UMedComEquipmentComponent* EquipComp = GetOwner()->FindComponentByClass<UMedComEquipmentComponent>())
            {
                CurrentWeapon = EquipComp->GetActiveWeapon();
                UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Получено оружие из EquipmentComponent: %s"), 
                       CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
            }
        }
    }
    
    // ИСПРАВЛЕНИЕ: Получаем актуальные способности стрельбы для текущего режима
    FGameplayTag CurrentFireModeTag;
    if (OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single")))
        CurrentFireModeTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single");
    else if (OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst")))
        CurrentFireModeTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst");
    else if (OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto")))
        CurrentFireModeTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto");
    else
        CurrentFireModeTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"); // По умолчанию
        
    UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Текущий режим огня по тегам: %s"), *CurrentFireModeTag.ToString());
    
    // Убеждаемся, что теги установлены
    bool bHasWeaponTag = OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.HasWeapon"));
    bool bHasFireModeTag = OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single")) ||
                           OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst")) ||
                           OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
    
    UE_LOG(LogTemp, Warning, TEXT("Pre-fire check: HasWeapon=%s, HasFireMode=%s"), 
           bHasWeaponTag ? TEXT("YES") : TEXT("NO"),
           bHasFireModeTag ? TEXT("YES") : TEXT("NO"));
    
    // Если тегов нет - добавляем их
    if (!bHasWeaponTag)
    {
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.HasWeapon"));
    }
    if (!bHasFireModeTag)
    {
        OwnerASC->AddLooseGameplayTag(CurrentFireModeTag);
    }

    // ИСПРАВЛЕНИЕ: Выбираем правильный хендл в зависимости от режима огня
    FGameplayAbilitySpecHandle* FireHandleToUse = nullptr;
    
    if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"))
        FireHandleToUse = AbilityHandles.Find("Fire");
    else if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"))
        FireHandleToUse = AbilityHandles.Find("BurstFire");
    else if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"))
        FireHandleToUse = AbilityHandles.Find("AutoFire");
        
    // Если нашли правильный хендл - используем его
    if (FireHandleToUse && FireHandleToUse->IsValid())
    {
        bool bActivated = OwnerASC->TryActivateAbility(*FireHandleToUse);
        UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Активация по хендлу %s для режима %s, результат = %s"), 
               *FireHandleToUse->ToString(), *CurrentFireModeTag.ToString(), 
               bActivated ? TEXT("УСПЕХ") : TEXT("НЕУДАЧА"));
        
        if (bActivated)
        {
            return true;
        }
    }
    
    // ИСПРАВЛЕНИЕ: Если не удалось активировать по хендлу, ищем способность для конкретного режима
    TArray<FGameplayAbilitySpec> Abilities = OwnerASC->GetActivatableAbilities();
    FGameplayAbilitySpec* FireSpec = nullptr;
    
    FGameplayTag AbilityModeTag;
    if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"))
        AbilityModeTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Single");
    else if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"))
        AbilityModeTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Burst");
    else if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"))
        AbilityModeTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Auto");
    
    // Ищем специфическую способность для режима
    for (const FGameplayAbilitySpec& Spec : Abilities)
    {
        if (Spec.Ability)
        {
            // Получаем теги способности через новый API
            FGameplayTagContainer AbilityTags = Spec.Ability->GetAssetTags();
    
            if (AbilityTags.HasTag(AbilityModeTag))
            {
                FireSpec = const_cast<FGameplayAbilitySpec*>(&Spec);
                UE_LOG(LogTemp, Warning, TEXT("Найдена способность стрельбы для режима %s: %s"), 
                       *CurrentFireModeTag.ToString(), *Spec.Ability->GetName());
                break;
            }
        }
    }

    // Если не нашли специфическую, ищем общую способность стрельбы
    if (!FireSpec)
    {
        for (const FGameplayAbilitySpec& Spec : Abilities)
        {
            if (Spec.Ability)
            {
                // Получаем теги способности через новый API
                FGameplayTagContainer AbilityTags = Spec.Ability->GetAssetTags();
        
                if (AbilityTags.HasTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Fire")) || 
                    AbilityTags.HasTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Shoot")))
                {
                    FireSpec = const_cast<FGameplayAbilitySpec*>(&Spec);
                    UE_LOG(LogTemp, Warning, TEXT("Найдена общая способность стрельбы: %s"), *Spec.Ability->GetName());
                    break;
                }
            }
        }
    }
    
    // Если нашли способность напрямую, активируем её
    if (FireSpec && FireSpec->Handle.IsValid())
    {
        bool bActivated = OwnerASC->TryActivateAbility(FireSpec->Handle);
        UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Прямая активация найденной способности, результат = %s"), 
               bActivated ? TEXT("УСПЕХ") : TEXT("НЕУДАЧА"));
        
        if (bActivated)
        {
            return true;
        }
    }
    
    // ИСПРАВЛЕНИЕ: Попытка активации по тегам с учетом текущего режима
    FGameplayTagContainer FireTag;
    if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"))
        FireTag.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Single"));
    else if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"))
        FireTag.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Burst"));
    else if (CurrentFireModeTag == FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"))
        FireTag.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Auto"));
    
    bool bActivated = OwnerASC->TryActivateAbilitiesByTag(FireTag);
    UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Активация по специфичному тегу режима, результат = %s"), 
           bActivated ? TEXT("УСПЕХ") : TEXT("НЕУДАЧА"));
    
    if (!bActivated)
    {
        // Если специфичный тег не сработал, используем общий тег стрельбы
        FireTag.Reset();
        FireTag.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Fire"));
        
        bActivated = OwnerASC->TryActivateAbilitiesByTag(FireTag);
        UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Активация по общему тегу Fire, результат = %s"), 
               bActivated ? TEXT("УСПЕХ") : TEXT("НЕУДАЧА"));
    }
    
    if (!bActivated)
    {
        // Пробуем альтернативный тег
        FireTag.Reset();
        FireTag.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Shoot"));
        
        bActivated = OwnerASC->TryActivateAbilitiesByTag(FireTag);
        UE_LOG(LogTemp, Warning, TEXT("TryFireWeapon: Активация по тегу Shoot, результат = %s"), 
               bActivated ? TEXT("УСПЕХ") : TEXT("НЕУДАЧА"));
    }
    
    return bActivated;
}

bool UMedComWeaponHandlerComponent::TryReloadWeapon()
{
    if (!OwnerASC || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Error, TEXT("TryReloadWeapon: No ASC or not authority"));
        return false;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("TryReloadWeapon: Attempting reload..."));
    
    // Check weapon and ammo status
    if (!CurrentWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryReloadWeapon: No current weapon"));
        
        // Try to recover the weapon reference
        if (AMedComEnemyCharacter* EnemyOwner = Cast<AMedComEnemyCharacter>(GetOwner()))
        {
            CurrentWeapon = EnemyOwner->GetCurrentWeapon();
        }
        
        if (!CurrentWeapon)
        {
            UE_LOG(LogTemp, Error, TEXT("TryReloadWeapon: Still no weapon after recovery attempt"));
            return false;
        }
    }
    
    // Get weapon attribute set
    UMedComWeaponAttributeSet* WeaponAttr = CurrentWeapon->GetWeaponAttributeSet();
    if (!WeaponAttr)
    {
        UE_LOG(LogTemp, Error, TEXT("TryReloadWeapon: No weapon attribute set"));
        return false;
    }
    
    // Log ammo status before reload
    float CurrentAmmo = WeaponAttr->GetCurrentAmmo();
    float MaxAmmo = WeaponAttr->GetMagazineSize();
    float RemainingAmmo = WeaponAttr->GetRemainingAmmo();
    
    UE_LOG(LogTemp, Warning, TEXT("Pre-reload ammo status: Current=%.1f, Max=%.1f, Remaining=%.1f"),
           CurrentAmmo, MaxAmmo, RemainingAmmo);
    
    // Try different reload methods
    
    // 1. First try: Via ability handle
    FGameplayAbilitySpecHandle* ReloadHandle = AbilityHandles.Find("Reload");
    if (ReloadHandle && ReloadHandle->IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Attempting reload via stored handle"));
        bool bActivated = OwnerASC->TryActivateAbility(*ReloadHandle);
        
        UE_LOG(LogTemp, Warning, TEXT("Reload via handle result: %s"), 
               bActivated ? TEXT("SUCCESS") : TEXT("FAILED"));
               
        if (bActivated)
            return true;
    }
    
    // 2. Second try: Via tag
    UE_LOG(LogTemp, Warning, TEXT("Attempting reload via tag"));
    FGameplayTagContainer ReloadTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Reload"));
    bool bActivated = OwnerASC->TryActivateAbilitiesByTag(ReloadTag);
    
    UE_LOG(LogTemp, Warning, TEXT("Reload via tag result: %s"), 
           bActivated ? TEXT("SUCCESS") : TEXT("FAILED"));
           
    if (bActivated)
        return true;
        
    // 3. Last resort: Direct attribute modification
    UE_LOG(LogTemp, Warning, TEXT("Attempting manual reload via attribute modification"));
    
    if (CurrentAmmo < MaxAmmo && RemainingAmmo > 0)
    {
        float AmmoToLoad = FMath::Min(MaxAmmo, RemainingAmmo);
        
        // Get ASC from weapon
        UAbilitySystemComponent* WeaponASC = CurrentWeapon->GetAbilitySystemComponent();
        if (!WeaponASC)
        {
            UE_LOG(LogTemp, Error, TEXT("Manual reload failed: No weapon ASC"));
            return false;
        }
        
        // Set ammo values directly
        WeaponASC->SetNumericAttributeBase(WeaponAttr->GetCurrentAmmoAttribute(), AmmoToLoad);
        WeaponASC->SetNumericAttributeBase(WeaponAttr->GetRemainingAmmoAttribute(), 
                                          RemainingAmmo - AmmoToLoad);
        
        UE_LOG(LogTemp, Warning, TEXT("Manual reload SUCCESS: Set ammo to %.1f"), AmmoToLoad);
        return true;
    }
    
    UE_LOG(LogTemp, Error, TEXT("All reload attempts failed"));
    return false;
}

void UMedComWeaponHandlerComponent::CycleFireMode()
{
    if (!GetOwner()->HasAuthority() || !OwnerASC)
    {
        return;
    }

    // Снимаем все теги режима огня
    OwnerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    OwnerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    OwnerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));

    // Цикл по перечислению
    switch (CurrentFireMode)
    {
    case EFireMode::Single:
        CurrentFireMode = EFireMode::Burst;
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
        break;
    case EFireMode::Burst:
        CurrentFireMode = EFireMode::Auto;
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
        break;
    case EFireMode::Auto:
        CurrentFireMode = EFireMode::Single;
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
        break;
    }
}
void UMedComWeaponHandlerComponent::SetFireMode(EFireMode NewMode)
{
    if (!GetOwner()->HasAuthority() || !OwnerASC)
    {
        UE_LOG(LogTemp, Error, TEXT("SetFireMode: Не authority или нет ASC!"));
        return;
    }

    // Подробный лог перед изменением
    UE_LOG(LogTemp, Warning, TEXT("SetFireMode: Изменение с %d на %d"), (int32)CurrentFireMode, (int32)NewMode);

    // Получаем актуальные теги
    FGameplayTagContainer CurrentTags;
    OwnerASC->GetOwnedGameplayTags(CurrentTags);
    UE_LOG(LogTemp, Warning, TEXT("Все теги ДО изменения: %s"), *CurrentTags.ToString());

    // Определяем теги режимов огня
    FGameplayTag SingleTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single");
    FGameplayTag BurstTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst");
    FGameplayTag AutoTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto");
    
    // ИСПРАВЛЕНИЕ: принудительно удаляем все теги режимов огня перед добавлением нового
    FGameplayTagContainer TagsToRemove;
    TagsToRemove.AddTag(SingleTag);
    TagsToRemove.AddTag(BurstTag);
    TagsToRemove.AddTag(AutoTag);
    
    // Удаляем все теги одной операцией
    OwnerASC->RemoveLooseGameplayTags(TagsToRemove);
    UE_LOG(LogTemp, Warning, TEXT("Удалены все теги режимов огня одной операцией"));

    // Проверяем, что теги удалены
    FGameplayTagContainer RemainingTags;
    OwnerASC->GetOwnedGameplayTags(RemainingTags);
    UE_LOG(LogTemp, Warning, TEXT("Все теги ПОСЛЕ удаления: %s"), *RemainingTags.ToString());

    // Устанавливаем новый режим
    CurrentFireMode = NewMode;

    // Добавляем соответствующий тег
    switch (NewMode)
    {
    case EFireMode::Single:
        OwnerASC->AddLooseGameplayTag(SingleTag);
        UE_LOG(LogTemp, Warning, TEXT("Добавлен тег: Weapon.FireMode.Single"));
        break;
    case EFireMode::Burst:
        OwnerASC->AddLooseGameplayTag(BurstTag);
        UE_LOG(LogTemp, Warning, TEXT("Добавлен тег: Weapon.FireMode.Burst"));
        break;
    case EFireMode::Auto:
        OwnerASC->AddLooseGameplayTag(AutoTag);
        UE_LOG(LogTemp, Warning, TEXT("Добавлен тег: Weapon.FireMode.Auto"));
        break;
    }
    
    // Финальная проверка тегов
    FGameplayTagContainer FinalTags;
    OwnerASC->GetOwnedGameplayTags(FinalTags);
    UE_LOG(LogTemp, Warning, TEXT("Все теги ПОСЛЕ добавления: %s"), *FinalTags.ToString());
    
    // Проверяем, что новый тег действительно добавлен
    if (NewMode == EFireMode::Single && !OwnerASC->HasMatchingGameplayTag(SingleTag))
    {
        UE_LOG(LogTemp, Error, TEXT("КРИТИЧЕСКАЯ ОШИБКА: Тег Single не был добавлен! Повторная попытка..."));
        OwnerASC->AddLooseGameplayTag(SingleTag);
    }
    else if (NewMode == EFireMode::Burst && !OwnerASC->HasMatchingGameplayTag(BurstTag))
    {
        UE_LOG(LogTemp, Error, TEXT("КРИТИЧЕСКАЯ ОШИБКА: Тег Burst не был добавлен! Повторная попытка..."));
        OwnerASC->AddLooseGameplayTag(BurstTag);
    }
    else if (NewMode == EFireMode::Auto && !OwnerASC->HasMatchingGameplayTag(AutoTag))
    {
        UE_LOG(LogTemp, Error, TEXT("КРИТИЧЕСКАЯ ОШИБКА: Тег Auto не был добавлен! Повторная попытка..."));
        OwnerASC->AddLooseGameplayTag(AutoTag);
    }
}

bool UMedComWeaponHandlerComponent::NeedsReload() const
{
    // Проверяем наличие оружия
    if (!CurrentWeapon) 
    {
        UE_LOG(LogTemp, Warning, TEXT("NeedsReload: No current weapon"));
        return false;
    }
    
    // Проверяем атрибуты оружия
    UMedComWeaponAttributeSet* WeaponAttr = CurrentWeapon->GetWeaponAttributeSet();
    if (!WeaponAttr)
    {
        UE_LOG(LogTemp, Warning, TEXT("NeedsReload: No weapon attribute set"));
        return false;
    }
    
    // Получаем текущие значения боеприпасов
    float CurrentAmmo = WeaponAttr->GetCurrentAmmo();
    float MaxAmmo = WeaponAttr->GetMagazineSize();
    float RemainingAmmo = WeaponAttr->GetRemainingAmmo();
    
    UE_LOG(LogTemp, Warning, TEXT("NeedsReload check: Current=%.1f, Max=%.1f, Remaining=%.1f"), 
           CurrentAmmo, MaxAmmo, RemainingAmmo);
    
    // Перезарядка нужна если:
    // 1. Текущие патроны закончились (== 0)
    // 2. И есть запасные патроны (> 0)
    return (CurrentAmmo <= 0.0f && RemainingAmmo > 0.0f);
}

void UMedComWeaponHandlerComponent::SelectAppropriateFireMode(float DistanceToTarget)
{
    if (!GetOwner()->HasAuthority() || !OwnerASC)
    {
        UE_LOG(LogTemp, Error, TEXT("SelectAppropriateFireMode: Не authority или нет ASC!"));
        return;
    }
    
    // Принудительный лог для подтверждения вызова метода
    UE_LOG(LogTemp, Warning, TEXT("SelectAppropriateFireMode: Расстояние = %.1f м"), DistanceToTarget);
    
    // ИСПРАВЛЕНИЕ: Увеличиваем пороговые значения для смены режимов
    // Теперь режим Burst будет активен до 10 метров (вместо прежних 5)
    const float NewBurstDistance = 10.0f;
    // Авто режим до 6 метров (вместо прежних 2)
    const float NewAutoDistance = 6.0f;
    
    // Определяем подходящий режим в зависимости от дистанции
    EFireMode NewMode;
    FString ModeString;
    
    if (DistanceToTarget <= NewAutoDistance)
    {
        NewMode = EFireMode::Auto;
        ModeString = TEXT("АВТО");
    }
    else if (DistanceToTarget <= NewBurstDistance)
    {
        NewMode = EFireMode::Burst;
        ModeString = TEXT("ОЧЕРЕДЬ");
    }
    else
    {
        NewMode = EFireMode::Single;
        ModeString = TEXT("ОДИНОЧНЫЙ");
    }
    
    // Получаем текущий режим и проверяем текущие теги
    FGameplayTagContainer CurrentTags;
    OwnerASC->GetOwnedGameplayTags(CurrentTags);
    UE_LOG(LogTemp, Warning, TEXT("Текущие теги: %s"), *CurrentTags.ToString());
    
    bool bHasSingleTag = OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    bool bHasBurstTag = OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    bool bHasAutoTag = OwnerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
    
    // Определяем текущий режим по тегам
    EFireMode CurrentMode = CurrentFireMode;
    if (bHasSingleTag) CurrentMode = EFireMode::Single;
    else if (bHasBurstTag) CurrentMode = EFireMode::Burst;
    else if (bHasAutoTag) CurrentMode = EFireMode::Auto;
    
    // Если теги не совпадают с CurrentFireMode, обновляем CurrentFireMode
    if (CurrentMode != CurrentFireMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("Обнаружено несоответствие! CurrentFireMode=%d, но активные теги указывают на %d"), 
               (int32)CurrentFireMode, (int32)CurrentMode);
        CurrentFireMode = CurrentMode;
    }
    
    // Подробный лог текущих и целевых режимов + расстояния
    UE_LOG(LogTemp, Warning, TEXT("Выбор режима огня: Текущий=%d, Новый=%d (%s), Дистанция=%.1f м"),
           (int32)CurrentMode, (int32)NewMode, *ModeString, DistanceToTarget);
    
    // Меняем режим только если он отличается от текущего
    if (CurrentMode != NewMode)
    {
        // Вызываем установку режима огня (которая обновит теги)
        UE_LOG(LogTemp, Warning, TEXT("Режим отличается, вызываем SetFireMode(%d)"), (int32)NewMode);
        SetFireMode(NewMode);
        
        // Проверяем теги ASC после изменения для подтверждения
        FGameplayTagContainer FinalTags;
        OwnerASC->GetOwnedGameplayTags(FinalTags);
        UE_LOG(LogTemp, Warning, TEXT("Финальные теги после изменения: %s"), *FinalTags.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Режим огня остался прежним: %d (%s) для дистанции %.1f м"),
               (int32)CurrentMode, *ModeString, DistanceToTarget);
    }
}

void UMedComWeaponHandlerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMedComWeaponHandlerComponent, CurrentFireMode);
}

void UMedComWeaponHandlerComponent::SetCurrentWeapon(AWeaponActor* NewWeapon)
{
    if (CurrentWeapon != NewWeapon)
    {
        CurrentWeapon = NewWeapon;
        CurrentWeaponAttributeSet = CurrentWeapon ? CurrentWeapon->GetWeaponAttributeSet() : nullptr;
        
        // Обновляем теги
        UpdateFireModeTags();
        
        UE_LOG(LogTemp, Warning, TEXT("WeaponHandlerComponent: Weapon changed to %s"), 
               CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
    }
}

void UMedComWeaponHandlerComponent::UpdateFireModeTags()
{
    if (!OwnerASC)
    {
        return;
    }

    // Удаляем все существующие теги режимов огня
    OwnerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    OwnerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    OwnerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));

    // Добавляем нужный в зависимости от текущего режима
    switch (CurrentFireMode)
    {
    case EFireMode::Single:
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
        break;
    case EFireMode::Burst:
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
        break;
    case EFireMode::Auto:
        OwnerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
        break;
    }
}