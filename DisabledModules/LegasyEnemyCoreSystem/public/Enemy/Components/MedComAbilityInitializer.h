#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "MedComAbilityInitializer.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * Компонент для выдачи способностей владельцу (обычно вызывается на сервере при PossessedBy).
 * Хранит ссылки на TSubclassOf и выдаёт их через ASC.
 */
UCLASS(ClassGroup=(MedCom), meta=(BlueprintSpawnableComponent))
class MEDCOMCORE_API UMedComAbilityInitializer : public UActorComponent
{
    GENERATED_BODY()

public:
    UMedComAbilityInitializer();

    /** Способность одиночной стрельбы */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Weapon")
    TSubclassOf<UGameplayAbility> FireAbility;

    /** Способность стрельбы очередями */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Weapon")
    TSubclassOf<UGameplayAbility> BurstFireAbility;

    /** Способность автоматической стрельбы */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Weapon")
    TSubclassOf<UGameplayAbility> AutoFireAbility;

    /** Способность перезарядки */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Weapon")
    TSubclassOf<UGameplayAbility> ReloadAbility;

    /** Способность смены режима огня (необязательно) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Weapon")
    TSubclassOf<UGameplayAbility> SwitchFireModeAbility;

    /** Дополнительные способности */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Other")
    TArray<TSubclassOf<UGameplayAbility>> AdditionalAbilities;

    /** Инициализировать и выдать способности владельцу */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    void InitializeAbilities();

    /** Вернуть уже выданные хендлы (для других компонентов) */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    TMap<FName, FGameplayAbilitySpecHandle> GetAbilityHandles() const { return AbilityHandles; }

protected:
    virtual void BeginPlay() override;

    /** ASC владельца */
    UPROPERTY()
    UAbilitySystemComponent* OwnerASC;

    /** Хендлы выданных способностей */
    UPROPERTY()
    TMap<FName, FGameplayAbilitySpecHandle> AbilityHandles;

    // Выдача способности с возможностью добавления тегов стрельбы
    void GrantAbility(const FString& KeyName, TSubclassOf<UGameplayAbility> AbilityClass, bool bAddFireTags = false);
};