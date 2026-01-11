#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComSwitchFireModeAbility.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Equipment/Components/MedComEquipmentComponent.h"
#include "Core/MedComPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Core/Character/MedComCharacter.h"

UMedComSwitchFireModeAbility::UMedComSwitchFireModeAbility()
{
    // Настройка инстанцирования и репликации
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // ИЗМЕНЕНО: ServerOnly вместо LocalPredicted для предотвращения двойной активации
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

    // Настройка тегов способности
    FGameplayTagContainer AbilityTagContainer;
    AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.SwitchFireMode"));
    SetAssetTags(AbilityTagContainer);

    // Добавление доступных режимов огня
    FireModeTags.Add(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    FireModeTags.Add(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    FireModeTags.Add(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));

    // Добавление отображаемых имен для режимов огня
    FireModeNames.Add(FText::FromString("Single"));
    FireModeNames.Add(FText::FromString("Burst"));
    FireModeNames.Add(FText::FromString("Auto"));

    // Настройка контейнера тегов для очистки
    FireModeTagsToRemove.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    FireModeTagsToRemove.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    FireModeTagsToRemove.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
    
    // Настройка параметров визуализации переключения режимов
    bPlaySwitchAnimation = true;
}

void UMedComSwitchFireModeAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // Вызываем родительский метод для стандартных проверок
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("UMedComSwitchFireModeAbility::ActivateAbility: Invalid Actor Info or ASC"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Проверяем, есть ли у персонажа оружие
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMedComSwitchFireModeAbility::ActivateAbility: No weapon equipped"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ИЗМЕНЕНО: Переключаем режим только на сервере
    if (HasAuthority(&ActivationInfo))
    {
        // Переключаем режим огня на сервере
        SwitchFireMode(ActorInfo->AbilitySystemComponent.Get(), Weapon);
        
        // Воспроизводим звук переключения для всех клиентов, если настроен
        if (SwitchSound && ActorInfo->AvatarActor.IsValid())
        {
            UGameplayStatics::PlaySoundAtLocation(
                GetWorld(),
                SwitchSound,
                ActorInfo->AvatarActor->GetActorLocation()
            );
        }
    }

    // Применяем визуальные эффекты переключения (работают и на клиенте)
    if (bPlaySwitchAnimation && Weapon)
    {
        PlaySwitchAnimation(Weapon);
    }

    // Способность завершается после переключения (одноразовая активация)
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UMedComSwitchFireModeAbility::SwitchFireMode(UAbilitySystemComponent* ASC, AWeaponActor* Weapon)
{
    if (!ASC || !Weapon)
    {
        UE_LOG(LogTemp, Error, TEXT("UMedComSwitchFireModeAbility::SwitchFireMode: Недопустимые параметры"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("UMedComSwitchFireModeAbility: Начало переключения. Текущий режим: %s"), 
           *Weapon->GetCurrentFireMode().ToString());
    
    // Просто используем готовый метод Weapon->CycleToNextFireMode() 
    // который делегирует работу в FireModeComponent!
    bool bSuccess = Weapon->CycleToNextFireMode();
    
    UE_LOG(LogTemp, Warning, TEXT("UMedComSwitchFireModeAbility: Режим изменен на: %s. Успех: %s"), 
           *Weapon->GetCurrentFireMode().ToString(), bSuccess ? TEXT("Да") : TEXT("Нет"));
}

void UMedComSwitchFireModeAbility::PlaySwitchAnimation(AWeaponActor* Weapon)
{
    if (!Weapon)
    {
        return;
    }
    
    // Проигрываем анимацию переключения, если она назначена
    if (SwitchModeAnim)
    {
        // Воспроизводим анимацию на персонаже или оружии
        if (ACharacter* Character = Cast<ACharacter>(Weapon->GetOwner()))
        {
            if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
            {
                AnimInstance->Montage_Play(SwitchModeAnim, 1.0f);
            }
        }
    }
}

FGameplayTag UMedComSwitchFireModeAbility::GetFireModeFromASC(UAbilitySystemComponent* ASC) const
{
    if (!ASC)
    {
        return FGameplayTag();
    }
    
    // Проверяем каждый тег и находим соответствующий
    for (const FGameplayTag& Tag : FireModeTags)
    {
        if (ASC->HasMatchingGameplayTag(Tag))
        {
            return Tag;
        }
    }
    
    // Если ни один тег не найден, возвращаем пустой тег
    return FGameplayTag();
}

int32 UMedComSwitchFireModeAbility::GetCurrentFireModeIndex(UAbilitySystemComponent* ASC) const
{
    if (!ASC)
    {
        return 0; // По умолчанию первый режим
    }
    
    // Проверяем каждый тег и находим соответствующий
    for (int32 i = 0; i < FireModeTags.Num(); ++i)
    {
        if (ASC->HasMatchingGameplayTag(FireModeTags[i]))
        {
            return i;
        }
    }
    
    // Если ни один тег не найден, возвращаем 0 (одиночный режим по умолчанию)
    return 0;
}

AWeaponActor* UMedComSwitchFireModeAbility::GetWeaponFromActorInfo() const
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return nullptr;
    }
    
    // Пытаемся получить оружие через MedComCharacter
    AMedComCharacter* Character = Cast<AMedComCharacter>(ActorInfo->AvatarActor.Get());
    if (Character)
    {
        AWeaponActor* Weapon = Character->GetCurrentWeapon();
        if (Weapon)
        {
            return Weapon;
        }
        
        // Если первый способ не сработал, пробуем через PlayerState и EquipmentComponent
        if (AMedComPlayerState* PS = Character->GetPlayerState<AMedComPlayerState>())
        {
            if (UMedComEquipmentComponent* EquipComp = PS->GetEquipmentComponent())
            {
                return EquipComp->GetActiveWeapon();
            }
        }
    }
    
    // Последний вариант - если оружие является OwnerActor
    if (ActorInfo->OwnerActor.IsValid())
    {
        return Cast<AWeaponActor>(ActorInfo->OwnerActor.Get());
    }
    
    return nullptr;
}