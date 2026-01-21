// SuspenseCoreGrenadeProjectile.h
// AAA-quality grenade projectile actor with physics, damage, and effects
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// This actor represents a thrown grenade in the world.
// - Server-authoritative physics and damage
// - Client-predicted visual effects
// - Full GAS integration for damage application
// - Support for all grenade types (Frag, Smoke, Flash, Incendiary, Impact)
//
// NETWORK:
// - Spawned on server, replicated to clients
// - Physics runs on server, clients interpolate
// - Explosion damage calculated on server only
// - Visual/audio effects run on all clients
//
// FLOW:
// 1. Spawned by GrenadeHandler with throw velocity
// 2. Physics simulation (bounce, roll)
// 3. Fuse timer countdown (reduced by cook time)
// 4. Explode → Apply damage via GAS → Spawn effects → Destroy
//
// REFERENCES:
// - Escape from Tarkov: Grenade cooking, fuse mechanics
// - Valley of the Ancient: Projectile spawning pattern
// - GAS Documentation: Area damage application

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreGrenadeProjectile.generated.h"

// Forward declarations
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;
class USoundBase;
class UNiagaraSystem;
class UNiagaraComponent;
class UParticleSystem;
class UCameraShakeBase;
class USuspenseCoreEventBus;

// SSOT forward declaration
struct FSuspenseCoreThrowableAttributeRow;

/**
 * Grenade type enum matching GrenadeHandler
 */
UENUM(BlueprintType)
enum class ESuspenseCoreGrenadeProjectileType : uint8
{
    Fragmentation   UMETA(DisplayName = "Fragmentation"),  // Explosive damage
    Smoke           UMETA(DisplayName = "Smoke"),          // Smoke screen
    Flashbang       UMETA(DisplayName = "Flashbang"),      // Blind/deafen
    Incendiary      UMETA(DisplayName = "Incendiary"),     // Fire damage over time
    Impact          UMETA(DisplayName = "Impact")          // Explodes on impact
};

/**
 * Grenade explosion data for GAS damage application
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreGrenadeExplosionData
{
    GENERATED_BODY()

    /** Explosion location */
    UPROPERTY(BlueprintReadWrite)
    FVector ExplosionLocation = FVector::ZeroVector;

    /** Inner radius - full damage */
    UPROPERTY(BlueprintReadWrite)
    float InnerRadius = 200.0f;

    /** Outer radius - falloff damage */
    UPROPERTY(BlueprintReadWrite)
    float OuterRadius = 500.0f;

    /** Base damage at inner radius */
    UPROPERTY(BlueprintReadWrite)
    float BaseDamage = 250.0f;

    /** Damage falloff exponent (1.0 = linear, 2.0 = quadratic) */
    UPROPERTY(BlueprintReadWrite)
    float DamageFalloff = 1.0f;

    /** Grenade type for effect selection */
    UPROPERTY(BlueprintReadWrite)
    ESuspenseCoreGrenadeProjectileType GrenadeType = ESuspenseCoreGrenadeProjectileType::Fragmentation;

    /** Instigator who threw the grenade */
    UPROPERTY(BlueprintReadWrite)
    TWeakObjectPtr<AActor> Instigator;
};

/**
 * Delegate broadcast when grenade explodes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrenadeExploded, const FSuspenseCoreGrenadeExplosionData&, ExplosionData);

/**
 * ASuspenseCoreGrenadeProjectile
 *
 * Physics-based grenade projectile with GAS damage integration.
 * Supports all Tarkov-style grenade types with proper multiplayer replication.
 *
 * CONFIGURATION:
 * Set via Blueprint or DataTable:
 * - FuseTime: Time until explosion (default 3.5s)
 * - ExplosionRadius: Inner/Outer damage radii
 * - BaseDamage: Damage at epicenter
 * - GrenadeType: Determines effects and damage type
 *
 * USAGE:
 * 1. Spawn via GrenadeHandler::ThrowGrenadeFromEvent()
 * 2. Call InitializeGrenade() with throw parameters
 * 3. Grenade handles physics, timer, explosion automatically
 *
 * @see USuspenseCoreGrenadeHandler
 * @see USuspenseCoreGrenadeThrowAbility
 */
UCLASS(BlueprintType, Blueprintable)
class EQUIPMENTSYSTEM_API ASuspenseCoreGrenadeProjectile : public AActor
{
    GENERATED_BODY()

public:
    ASuspenseCoreGrenadeProjectile();

    //==================================================================
    // Components
    //==================================================================

    /** Root collision sphere */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionComponent;

    /** Visual mesh */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    /** Projectile movement for physics */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    //==================================================================
    // Configuration - Timing
    //==================================================================

    /** Base fuse time in seconds (cook time is subtracted) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Timing")
    float FuseTime = 3.5f;

    /** Minimum fuse time after cooking (safety limit) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Timing")
    float MinFuseTime = 0.5f;

    /** Time before grenade can be picked up (0 = never) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Timing")
    float PickupDelay = 0.0f;

    //==================================================================
    // Configuration - Damage
    //==================================================================

    /** Grenade type */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    ESuspenseCoreGrenadeProjectileType GrenadeType = ESuspenseCoreGrenadeProjectileType::Fragmentation;

    /** Base damage at epicenter */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    float BaseDamage = 250.0f;

    /** Inner radius - full damage zone */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    float InnerRadius = 200.0f;

    /** Outer radius - damage falloff zone */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    float OuterRadius = 500.0f;

    /** Damage falloff exponent (1.0 = linear) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    float DamageFalloff = 1.0f;

    /** Whether explosion damages instigator (self-damage) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    bool bDamageInstigator = true;

    /** Whether explosion goes through walls (false = line trace check) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    bool bIgnoreWalls = false;

    /** GameplayEffect to apply for damage */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    /** GameplayEffect for flashbang (blind/deafen) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    TSubclassOf<UGameplayEffect> FlashbangEffectClass;

    /** GameplayEffect for incendiary (burn DoT) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Damage")
    TSubclassOf<UGameplayEffect> IncendiaryEffectClass;

    //==================================================================
    // Configuration - Physics
    //==================================================================

    /** Bounce coefficient (0.0 = no bounce, 1.0 = full bounce) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Physics")
    float Bounciness = 0.3f;

    /** Friction when rolling */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Physics")
    float Friction = 0.5f;

    /** Grenade mass for physics calculations */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Physics")
    float GrenadeMass = 0.6f;

    //==================================================================
    // Configuration - Effects
    //==================================================================

    /** Explosion sound */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    TObjectPtr<USoundBase> ExplosionSound;

    /** Bounce sound */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    TObjectPtr<USoundBase> BounceSound;

    /** Pin sound (safety lever release) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    TObjectPtr<USoundBase> PinSound;

    /** Explosion particle effect */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    TObjectPtr<UNiagaraSystem> ExplosionEffect;

    /** Smoke effect (for smoke grenades) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    TObjectPtr<UNiagaraSystem> SmokeEffect;

    /** Trail effect while flying */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    TObjectPtr<UNiagaraSystem> TrailEffect;

    /** Camera shake class for explosion */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    TSubclassOf<UCameraShakeBase> ExplosionCameraShake;

    /** Camera shake radius */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grenade|Effects")
    float CameraShakeRadius = 1000.0f;

    //==================================================================
    // Events
    //==================================================================

    /** Broadcast when grenade explodes */
    UPROPERTY(BlueprintAssignable, Category = "Grenade|Events")
    FOnGrenadeExploded OnExploded;

    //==================================================================
    // Initialization
    //==================================================================

    /**
     * Initialize grenade with throw parameters
     * Call immediately after spawning
     *
     * @param InInstigator Actor who threw the grenade
     * @param ThrowVelocity Initial velocity vector
     * @param CookTime Time already cooked (subtracted from fuse)
     * @param InGrenadeID Item ID for data lookup
     */
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void InitializeGrenade(
        AActor* InInstigator,
        const FVector& ThrowVelocity,
        float CookTime = 0.0f,
        FName InGrenadeID = NAME_None);

    /**
     * Initialize grenade from SSOT (Single Source of Truth)
     * Loads all attributes from FSuspenseCoreThrowableAttributeRow
     *
     * @param Attributes Throwable attributes from DataManager
     */
    void InitializeFromSSOT(const FSuspenseCoreThrowableAttributeRow& Attributes);

    //==================================================================
    // Runtime Accessors
    //==================================================================

    /** Get remaining fuse time */
    UFUNCTION(BlueprintPure, Category = "Grenade")
    float GetRemainingFuseTime() const;

    /** Get time since thrown */
    UFUNCTION(BlueprintPure, Category = "Grenade")
    float GetTimeSinceThrown() const;

    /** Check if grenade is armed (fuse active) */
    UFUNCTION(BlueprintPure, Category = "Grenade")
    bool IsArmed() const { return bIsArmed; }

    /** Check if grenade has exploded */
    UFUNCTION(BlueprintPure, Category = "Grenade")
    bool HasExploded() const { return bHasExploded; }

    /** Get grenade type */
    UFUNCTION(BlueprintPure, Category = "Grenade")
    ESuspenseCoreGrenadeProjectileType GetGrenadeType() const { return GrenadeType; }

    //==================================================================
    // Manual Control
    //==================================================================

    /**
     * Force immediate explosion (debug/special case)
     */
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void ForceExplode();

    /**
     * Defuse the grenade (prevent explosion)
     */
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void Defuse();

protected:
    //==================================================================
    // AActor Overrides
    //==================================================================

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    //==================================================================
    // Physics Callbacks
    //==================================================================

    /** Called when grenade hits something */
    UFUNCTION()
    void OnProjectileHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

    /** Called when grenade bounces */
    UFUNCTION()
    void OnProjectileBounce(
        const FHitResult& ImpactResult,
        const FVector& ImpactVelocity);

    //==================================================================
    // Explosion Logic
    //==================================================================

    /** Execute explosion sequence */
    UFUNCTION(BlueprintNativeEvent, Category = "Grenade")
    void Explode();

    /** Apply damage to actors in radius (server only) */
    UFUNCTION(BlueprintCallable, Category = "Grenade")
    void ApplyExplosionDamage();

    /** Spawn explosion effects (all clients) */
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnExplosionEffects();

    /** Calculate damage for target based on distance */
    UFUNCTION(BlueprintPure, Category = "Grenade")
    float CalculateDamageForTarget(AActor* Target, float Distance) const;

    /** Check if target is visible from explosion (not blocked by wall) */
    UFUNCTION(BlueprintPure, Category = "Grenade")
    bool IsTargetVisible(AActor* Target) const;

    //==================================================================
    // Blueprint Events
    //==================================================================

    /** Called when grenade is initialized */
    UFUNCTION(BlueprintImplementableEvent, Category = "Grenade")
    void OnGrenadeInitialized();

    /** Called when grenade becomes armed */
    UFUNCTION(BlueprintImplementableEvent, Category = "Grenade")
    void OnGrenadeArmed();

    /** Called just before explosion */
    UFUNCTION(BlueprintImplementableEvent, Category = "Grenade")
    void OnPreExplosion();

    /** Called after explosion effects */
    UFUNCTION(BlueprintImplementableEvent, Category = "Grenade")
    void OnPostExplosion();

private:
    //==================================================================
    // Runtime State
    //==================================================================

    /** Is the grenade armed (fuse counting down) */
    UPROPERTY(ReplicatedUsing = OnRep_IsArmed)
    bool bIsArmed = false;

    /** Has the grenade exploded */
    UPROPERTY(Replicated)
    bool bHasExploded = false;

    /** Time when grenade was thrown */
    UPROPERTY(Replicated)
    float ThrowTime = 0.0f;

    /** Effective fuse time (after cook reduction) */
    UPROPERTY(Replicated)
    float EffectiveFuseTime = 0.0f;

    /** Item ID for data lookup */
    UPROPERTY()
    FName GrenadeID;

    /** Actor who threw the grenade */
    UPROPERTY(Replicated)
    TWeakObjectPtr<AActor> InstigatorActor;

    /** Trail effect component */
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> TrailEffectComponent;

    /** EventBus reference */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

    //==================================================================
    // Replication
    //==================================================================

    UFUNCTION()
    void OnRep_IsArmed();

    //==================================================================
    // Internal Methods
    //==================================================================

    /** Start the fuse timer */
    void ArmGrenade();

    /** Play sound at grenade location */
    void PlayGrenadeSound(USoundBase* Sound);

    /** Spawn Niagara effect */
    UNiagaraComponent* SpawnEffect(UNiagaraSystem* Effect, const FVector& Location, const FRotator& Rotation);

    /** Get EventBus from game instance */
    USuspenseCoreEventBus* GetEventBus();

    /** Publish explosion event via EventBus */
    void PublishExplosionEvent();
};
