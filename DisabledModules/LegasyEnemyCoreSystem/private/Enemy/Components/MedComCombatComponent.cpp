#include "Core/Enemy/Components/MedComCombatComponent.h"
#include "Core/Enemy/Components/MedComWeaponHandlerComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UMedComCombatComponent::UMedComCombatComponent() : WeaponHandler()
{
    PrimaryComponentTick.bCanEverTick = false; // Можно включить, если нужно
    AttackCooldown = 0.5f;
    EffectiveRange = 800.0f;
    AttackCooldownRemaining = 0.0f;
    
    // Значения уже определены в заголовочном файле
    // CloseRange = 200.0f;
    // MediumRange = 500.0f;
}

void UMedComCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    // Ищем UMedComWeaponHandlerComponent на владельце
    if (AActor* MyOwner = GetOwner())
    {
        WeaponHandler = MyOwner->FindComponentByClass<UMedComWeaponHandlerComponent>();
        UE_LOG(LogTemp, Warning, TEXT("CombatComponent: Found WeaponHandler: %s"), 
              WeaponHandler ? TEXT("YES") : TEXT("NO"));
    }
}

bool UMedComCombatComponent::CanAttack(AActor* Target, float& OutDistance) const
{
    if (!Target) return false;

    // Проверка дистанции
    FVector MyLocation = GetOwner()->GetActorLocation();
    FVector TargetLocation = Target->GetActorLocation();
    OutDistance = FVector::Dist(MyLocation, TargetLocation);

    // Проверяем, что дистанция в пределах EffectiveRange
    bool bInRange = (OutDistance <= EffectiveRange);

    // Проверяем, что КД не активен
    bool bCooldownReady = (AttackCooldownRemaining <= 0.0f);

    UE_LOG(LogTemp, Warning, TEXT("CanAttack check: Distance=%.1f, InRange=%s, CooldownReady=%s"),
           OutDistance,
           bInRange ? TEXT("YES") : TEXT("NO"),
           bCooldownReady ? TEXT("YES") : TEXT("NO"));

    return bInRange && bCooldownReady;
}

bool UMedComCombatComponent::PerformAttack(AActor* Target)
{
    if (!WeaponHandler || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("PerformAttack: No WeaponHandler or not authority"));
        return false;
    }

    float Distance;
    if (!CanAttack(Target, Distance))
    {
        return false;
    }

    // Перед атакой проверяем, нужна ли перезарядка
    if (WeaponHandler->NeedsReload())
    {
        UE_LOG(LogTemp, Warning, TEXT("PerformAttack: Weapon needs reload, attempting..."));
        bool bReloaded = WeaponHandler->TryReloadWeapon();
        UE_LOG(LogTemp, Warning, TEXT("Reload attempt result: %s"), bReloaded ? TEXT("SUCCESS") : TEXT("FAILED"));
        
        // Если перезарядка успешна, ставим небольшой КД и возвращаем true
        if (bReloaded)
        {
            AttackCooldownRemaining = 1.5f; // Перезарядка занимает больше времени
            return true;
        }
        // Если перезарядка не удалась, продолжаем попытку атаки (может быть другое оружие?)
    }

    // Выбираем подходящий режим огня в зависимости от дистанции
    // Передаем Distance из MedComCombatComponent в WeaponHandler
    WeaponHandler->SelectAppropriateFireMode(Distance);
    UE_LOG(LogTemp, Warning, TEXT("Selected fire mode based on distance: %.1f"), Distance);
    
    // Стандартная логика атаки
    if (Distance < 150.0f)
    {
        // Ближняя атака (если реализована)
        return false;
    }
    else
    {
        // Стреляем
        bool bFired = WeaponHandler->TryFireWeapon();
        UE_LOG(LogTemp, Warning, TEXT("Fire attempt result: %s"), bFired ? TEXT("SUCCESS") : TEXT("FAILED"));
        
        if (bFired)
        {
            // Ставим КД
            AttackCooldownRemaining = AttackCooldown;
            return true;
        }
        else
        {
            // Если выстрел не удался, проверяем, можно ли перезарядить
            if (WeaponHandler->NeedsReload())
            {
                UE_LOG(LogTemp, Warning, TEXT("Fire failed, trying reload"));
                bool bReloaded = WeaponHandler->TryReloadWeapon();
                
                if (bReloaded)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Reload after failed fire: SUCCESS"));
                    AttackCooldownRemaining = 1.5f; // КД после перезарядки
                    return true;
                }
            }
        }
    }

    return false;
}

void UMedComCombatComponent::UpdateCombatState(float DeltaTime)
{
    // Обновляем КД
    if (AttackCooldownRemaining > 0.0f)
    {
        AttackCooldownRemaining -= DeltaTime;
    }
}