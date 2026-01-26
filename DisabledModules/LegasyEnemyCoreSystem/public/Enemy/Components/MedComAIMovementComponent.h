#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MedComAIMovementComponent.generated.h"

class AMedComEnemyCharacter;
class AAIController;
class UNavigationSystemV1;
class ACharacter;

#define CM_TO_M_CONVERSION 100.0f
#define M_TO_CM_CONVERSION 100.0f

UENUM(BlueprintType)
enum class EMedComAIMovementType : uint8
{
    Forward,
    Backward,
    Left,
    Right,
    Random
};

USTRUCT(BlueprintType)
struct FMedComRepositioningParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float MinTargetDistance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float MaxTargetDistance = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float MinEnemyDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float BurstFireDistance = 12.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float AutoFireDistance = 5.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float MeleeAttackRange = 2.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
    float RepositionDistance = 250.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float MaxRepositionTime = 1.5f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MEDCOMCORE_API UMedComAIMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    UMedComAIMovementComponent();

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void InitializeParams(const FMedComRepositioningParams& Params);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    FMedComRepositioningParams RepositioningParams;

    UPROPERTY(BlueprintReadWrite, Category = "AI|Movement")
    float RepositionCooldown = 2.0f;
    
    UPROPERTY(BlueprintReadWrite, Category = "AI|Movement")
    float LastRepositionTime = 0.0f;
    
    UPROPERTY(BlueprintReadWrite, Category = "AI|Movement")
    float RepositionTimer = 0.0f;
    
    UPROPERTY(BlueprintReadWrite, Category = "AI|Movement")
    bool bRepositioning = false;
    
    UPROPERTY(BlueprintReadWrite, Category = "AI|Movement")
    FVector TargetPosition = FVector::ZeroVector;
    
    UPROPERTY(BlueprintReadWrite, Category = "AI|Combat")
    int32 ShotsSinceReposition = 0;

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    bool NeedsRepositioning(float DistanceToTarget) const;
    
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    FVector CalculateRepositionTarget(ACharacter* TargetPlayer, float CurrentDistance, FGameplayTag CurrentFireMode);
    
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    FVector CalculateAttackPosition(const ACharacter* TargetPlayer) const;
    
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void RepositionForAttack(const ACharacter* TargetPlayer);
    
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool HasClearLineOfFire(const ACharacter* Target) const;
    
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    bool UpdateRepositioning(ACharacter* Target, float DeltaTime, float& InOutRepositionTimer);
    
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void StartRepositioning(ACharacter* Target, float CurrentDistance, FGameplayTag CurrentFireMode);
    
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool IsInAttackRange(const ACharacter* Target, float MinDistance, float MaxDistance) const;
    
    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    void RotateTowardsTarget(const ACharacter* Target, float DeltaTime, float RotationSpeed = 5.0f);

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool HasClearLineOfSight(const AActor* Target) const;
    
protected:
    virtual void BeginPlay() override;
    
    UPROPERTY()
    AMedComEnemyCharacter* OwnerEnemy;
    
    UPROPERTY()
    AAIController* AIController;

    FVector FindPositionAwayFromOtherEnemies(const FVector& BasePosition, float MinDistance) const;
    
    bool IsTooCloseToOtherEnemies(float MinDistance) const;
    
    FVector ProjectPointToNavigation(UNavigationSystemV1* NavSys, const FVector& Point) const;
    
    bool CanSeeTargetFromPosition(const ACharacter* Target, const FVector& Position) const;

private:
    static constexpr float DEFAULT_NAV_EXTENT = 500.0f;
    static constexpr float EYE_HEIGHT_OFFSET = 80.0f;
    FVector LastKnownPlayerPosition = FVector::ZeroVector;
    float LastPlayerSightingTime = 0.0f;
    // Период памяти (секунд), в течение которого бот помнит последнюю позицию игрока
    float MemoryDuration = 3.0f;
    void UpdateCache();
};
