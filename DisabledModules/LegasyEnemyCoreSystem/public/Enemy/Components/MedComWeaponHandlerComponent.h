#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "MedComWeaponHandlerComponent.generated.h"

class UAbilitySystemComponent;
class AWeaponActor;
class UMedComWeaponAttributeSet;

/** Режимы огня */
UENUM(BlueprintType)
enum class EFireMode : uint8
{
    Single UMETA(DisplayName="Single"),
    Burst  UMETA(DisplayName="Burst"),
    Auto   UMETA(DisplayName="Auto")
};

UCLASS(ClassGroup=(MedCom), meta=(BlueprintSpawnableComponent))
class MEDCOMCORE_API UMedComWeaponHandlerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMedComWeaponHandlerComponent();

    /** Попытка выстрелить из текущего оружия (активация способности) */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
    bool TryFireWeapon();

    /** Попытка перезарядить текущее оружие (активация способности) */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
    bool TryReloadWeapon();

    /** Циклически переключить режим огня */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
    void CycleFireMode();

    /** Установить конкретный режим огня */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
    void SetFireMode(EFireMode NewMode);

    /** Проверка, нужна ли перезарядка */
    UFUNCTION(BlueprintPure, Category = "Weapon|State")
    bool NeedsReload() const;

    /** Вернуть текущий режим огня */
    UFUNCTION(BlueprintPure, Category = "Weapon|State")
    EFireMode GetCurrentFireMode() const { return CurrentFireMode; }

    /**
     * Пример ИИ-логики: выбор режима огня на основе дистанции до цели
     * (можно вызывать из Behavior Tree или из State-машины)
     */
    
    UFUNCTION(BlueprintCallable, Category = "Weapon|AI")
    void SelectAppropriateFireMode(float DistanceToTarget);

    UFUNCTION(BlueprintCallable, Category = "Weapon|State")
    void SetCurrentWeapon(AWeaponActor* NewWeapon);

    UPROPERTY()
    UMedComWeaponAttributeSet* CurrentWeaponAttributeSet;

    // В публичной секции (или protected, если предпочитаете):
    UFUNCTION(BlueprintCallable, Category = "Weapon|State")
    void UpdateFireModeTags();

    /** Инициализация ссылок на ASC и оружие */
    void InitializeWeaponHandler();

    // Получение текущего оружия
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    AWeaponActor* GetCurrentWeapon() const { return CurrentWeapon; }
protected:
    virtual void BeginPlay() override;
    
    /** ASC владельца (персонажа) */
    UPROPERTY()
    UAbilitySystemComponent* OwnerASC;

    /** Текущее активное оружие персонажа */
    UPROPERTY()
    AWeaponActor* CurrentWeapon;

    /** Текущий режим огня */
    UPROPERTY(Replicated)
    EFireMode CurrentFireMode;
    // Настройки дистанций для режимов огня
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|FireModes")
    float PreferredBurstFireDistance = 10.0f; // Максимальная дистанция для режима очередей (было 5.0f)
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|FireModes")
    float PreferredAutoFireDistance = 6.0f; // Максимальная дистанция для автоматического режима (было 2.0f)
    
    /** Пороговые дистанции для разных режимов (пример) */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|FireModes")
    float PreferredSingleFireDistance = 1000.0f;
    
    /** Хендлы способностей, полученные из AbilityInitializer */
    UPROPERTY()
    TMap<FName, FGameplayAbilitySpecHandle> AbilityHandles;

    // Репликация
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
