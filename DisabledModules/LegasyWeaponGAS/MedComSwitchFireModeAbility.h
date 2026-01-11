#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Equipment/Base/WeaponActor.h"
#include "MedComSwitchFireModeAbility.generated.h"

class USoundBase;

/**
 * Способность для переключения режимов огня оружия в системе Gameplay Ability System.
 * Реализует только логику смены тегов режима огня и делегирует все расчеты оружию.
 */
UCLASS()
class MEDCOMCORE_API UMedComSwitchFireModeAbility : public UGameplayAbility
{
    GENERATED_BODY()
    
public:
    UMedComSwitchFireModeAbility();
    
    // Основная функция активации способности
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    
protected:
    // Звук переключения режима огня
    UPROPERTY(EditDefaultsOnly, Category = "Audio")
    USoundBase* SwitchSound;
    
    // Доступные режимы огня (теги)
    UPROPERTY(EditDefaultsOnly, Category = "FireMode")
    TArray<FGameplayTag> FireModeTags;
    
    // Отображаемые имена режимов огня (UI)
    UPROPERTY(EditDefaultsOnly, Category = "FireMode")
    TArray<FText> FireModeNames;
    
    // Контейнер тегов для удаления при переключении
    UPROPERTY(EditDefaultsOnly, Category = "FireMode")
    FGameplayTagContainer FireModeTagsToRemove;
    
    // Настройки визуальных эффектов
    UPROPERTY(EditDefaultsOnly, Category = "Visuals")
    bool bPlaySwitchAnimation;
    
    UPROPERTY(EditDefaultsOnly, Category = "Visuals")
    UAnimMontage* SwitchModeAnim;
    
    // Основной метод переключения режима
    void SwitchFireMode(UAbilitySystemComponent* ASC, AWeaponActor* Weapon);
    
    // Получение текущего режима огня из ASC
    FGameplayTag GetFireModeFromASC(UAbilitySystemComponent* ASC) const;
    
    // Получение индекса текущего режима огня
    int32 GetCurrentFireModeIndex(UAbilitySystemComponent* ASC) const;
    
    // Получение текущего оружия
    AWeaponActor* GetWeaponFromActorInfo() const;
    
    // Воспроизведение анимации переключения
    void PlaySwitchAnimation(AWeaponActor* Weapon);
};