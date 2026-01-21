// SuspenseCoreGrenadeProjectile.cpp
// AAA-quality grenade projectile actor implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Actors/SuspenseCoreGrenadeProjectile.h"
#include "SuspenseCore/Subsystems/SuspenseCoreThrowableAssetPreloader.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
#include "Particles/ParticleSystemComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "SuspenseCore/Effects/Weapon/SuspenseCoreDamageEffect.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrenadeProjectile, Log, All);

#define GRENADE_PROJECTILE_LOG(Verbosity, Format, ...) \
    UE_LOG(LogGrenadeProjectile, Verbosity, TEXT("[%s] " Format), *GetName(), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

ASuspenseCoreGrenadeProjectile::ASuspenseCoreGrenadeProjectile()
{
    // Enable ticking for fuse countdown
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Replication setup
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicatingMovement(true);

    // Create collision component (root)
    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    CollisionComponent->InitSphereRadius(5.0f);

    // Configure collision for bouncing off walls/floors
    // Using explicit settings instead of named profile for reliability
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);  // Block everything (bounce off walls)
    CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);  // Overlap with pawns (for damage)
    CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);  // Ignore camera

    CollisionComponent->SetNotifyRigidBodyCollision(true);
    CollisionComponent->SetGenerateOverlapEvents(true);
    RootComponent = CollisionComponent;

    // Create visual mesh
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(CollisionComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create projectile movement component
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = CollisionComponent;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = true;
    ProjectileMovement->Bounciness = 0.3f;
    ProjectileMovement->Friction = 0.5f;
    ProjectileMovement->ProjectileGravityScale = 1.0f;
    ProjectileMovement->InitialSpeed = 0.0f;  // Set via InitializeGrenade
    ProjectileMovement->MaxSpeed = 5000.0f;

    // Default configuration
    FuseTime = 3.5f;
    MinFuseTime = 0.5f;
    BaseDamage = 250.0f;
    InnerRadius = 200.0f;
    OuterRadius = 500.0f;
    DamageFalloff = 1.0f;
    bDamageInstigator = true;
    bIgnoreWalls = false;
    Bounciness = 0.3f;
    Friction = 0.5f;
    GrenadeMass = 0.6f;
    CameraShakeRadius = 1000.0f;
    GrenadeType = ESuspenseCoreGrenadeProjectileType::Fragmentation;

    // Initialize runtime state
    bIsArmed = false;
    bHasExploded = false;
    ThrowTime = 0.0f;
    EffectiveFuseTime = 0.0f;
}

//==================================================================
// Lifecycle
//==================================================================

void ASuspenseCoreGrenadeProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Bind hit callback for impact grenades
    if (CollisionComponent)
    {
        CollisionComponent->OnComponentHit.AddDynamic(this, &ASuspenseCoreGrenadeProjectile::OnProjectileHit);
    }

    // Bind bounce callback for sound effects
    if (ProjectileMovement)
    {
        ProjectileMovement->OnProjectileBounce.AddDynamic(this, &ASuspenseCoreGrenadeProjectile::OnProjectileBounce);
        ProjectileMovement->Bounciness = Bounciness;
        ProjectileMovement->Friction = Friction;
    }

    // Get EventBus reference
    EventBus = GetEventBus();

    GRENADE_PROJECTILE_LOG(Log, TEXT("BeginPlay: Type=%d, FuseTime=%.2f"),
        static_cast<int32>(GrenadeType), FuseTime);
}

void ASuspenseCoreGrenadeProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Fuse countdown (server only for authority)
    if (HasAuthority() && bIsArmed && !bHasExploded)
    {
        float RemainingTime = GetRemainingFuseTime();

        if (RemainingTime <= 0.0f)
        {
            GRENADE_PROJECTILE_LOG(Log, TEXT("Fuse expired - exploding"));
            Explode();
        }
    }
}

void ASuspenseCoreGrenadeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASuspenseCoreGrenadeProjectile, bIsArmed);
    DOREPLIFETIME(ASuspenseCoreGrenadeProjectile, bHasExploded);
    DOREPLIFETIME(ASuspenseCoreGrenadeProjectile, ThrowTime);
    DOREPLIFETIME(ASuspenseCoreGrenadeProjectile, EffectiveFuseTime);
    DOREPLIFETIME(ASuspenseCoreGrenadeProjectile, InstigatorActor);
}

//==================================================================
// Initialization
//==================================================================

void ASuspenseCoreGrenadeProjectile::InitializeGrenade(
    AActor* InInstigator,
    const FVector& ThrowVelocity,
    float CookTime,
    FName InGrenadeID)
{
    InstigatorActor = InInstigator;
    GrenadeID = InGrenadeID;
    ThrowTime = GetWorld()->GetTimeSeconds();

    // Calculate effective fuse time (reduced by cooking)
    EffectiveFuseTime = FMath::Max(FuseTime - CookTime, MinFuseTime);

    // Set throw velocity
    if (ProjectileMovement)
    {
        ProjectileMovement->Velocity = ThrowVelocity;
    }

    // Spawn trail effect
    if (TrailEffect)
    {
        TrailEffectComponent = SpawnEffect(TrailEffect, GetActorLocation(), GetActorRotation());
        if (TrailEffectComponent)
        {
            TrailEffectComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
        }
    }

    // Play pin sound (safety lever release)
    PlayGrenadeSound(PinSound);

    // Arm the grenade
    ArmGrenade();

    // Notify blueprint
    OnGrenadeInitialized();

    GRENADE_PROJECTILE_LOG(Log, TEXT("Initialized: Instigator=%s, Velocity=%s, CookTime=%.2f, EffectiveFuse=%.2f"),
        *GetNameSafe(InInstigator),
        *ThrowVelocity.ToString(),
        CookTime,
        EffectiveFuseTime);
}

void ASuspenseCoreGrenadeProjectile::InitializeFromSSOT(const FSuspenseCoreThrowableAttributeRow& Attributes)
{
    GRENADE_PROJECTILE_LOG(Log, TEXT("InitializeFromSSOT: Loading from %s"), *Attributes.ThrowableID.ToString());

    // ═══════════════════════════════════════════════════════════════════
    // TRY TO USE PRELOADED ASSETS (AAA-quality: no microfreezes)
    // ═══════════════════════════════════════════════════════════════════
    FSuspenseCoreThrowableAssetCache PreloadedCache;
    bool bUsePreloaded = false;

    if (USuspenseCoreThrowableAssetPreloader* Preloader = USuspenseCoreThrowableAssetPreloader::Get(this))
    {
        bUsePreloaded = Preloader->GetPreloadedAssets(Attributes.ThrowableID, PreloadedCache);
        if (bUsePreloaded)
        {
            GRENADE_PROJECTILE_LOG(Log, TEXT("  Using preloaded assets (zero-hitch)"));
        }
        else
        {
            GRENADE_PROJECTILE_LOG(Warning, TEXT("  Assets not preloaded - will use sync load (may cause hitch)"));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // TIMING
    // ═══════════════════════════════════════════════════════════════════
    FuseTime = Attributes.FuseTime;

    // ═══════════════════════════════════════════════════════════════════
    // DAMAGE
    // ═══════════════════════════════════════════════════════════════════
    BaseDamage = Attributes.ExplosionDamage;
    InnerRadius = Attributes.InnerRadius * 100.0f;  // Meters to cm
    OuterRadius = Attributes.ExplosionRadius * 100.0f;  // Meters to cm
    DamageFalloff = Attributes.DamageFalloff;

    // ═══════════════════════════════════════════════════════════════════
    // PHYSICS
    // ═══════════════════════════════════════════════════════════════════
    Bounciness = 1.0f - Attributes.BounceFriction;  // Convert friction to bounciness
    Friction = Attributes.BounceFriction;

    if (ProjectileMovement)
    {
        ProjectileMovement->Bounciness = Bounciness;
        ProjectileMovement->Friction = Friction;
    }

    // ═══════════════════════════════════════════════════════════════════
    // VFX - Use preloaded or fallback to sync load
    // ═══════════════════════════════════════════════════════════════════

    // Explosion Effect
    if (bUsePreloaded && PreloadedCache.ExplosionEffect)
    {
        ExplosionEffect = PreloadedCache.ExplosionEffect;
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Using preloaded Niagara explosion: %s"), *GetNameSafe(ExplosionEffect));
    }
    else if (!Attributes.ExplosionEffect.IsNull())
    {
        ExplosionEffect = Attributes.ExplosionEffect.LoadSynchronous();
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Sync loaded Niagara explosion: %s"), *GetNameSafe(ExplosionEffect));
    }
    else if (!Attributes.ExplosionEffectLegacy.IsNull())
    {
        // Store legacy particle system - will handle in Multicast_SpawnExplosionEffects
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Will use Cascade explosion (legacy)"));
    }

    // Smoke Effect
    if (bUsePreloaded && PreloadedCache.SmokeEffect)
    {
        SmokeEffect = PreloadedCache.SmokeEffect;
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Using preloaded Niagara smoke: %s"), *GetNameSafe(SmokeEffect));
    }
    else if (!Attributes.SmokeEffect.IsNull())
    {
        SmokeEffect = Attributes.SmokeEffect.LoadSynchronous();
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Sync loaded Niagara smoke: %s"), *GetNameSafe(SmokeEffect));
    }

    // Trail Effect
    if (bUsePreloaded && PreloadedCache.TrailEffect)
    {
        TrailEffect = PreloadedCache.TrailEffect;
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Using preloaded Niagara trail: %s"), *GetNameSafe(TrailEffect));
    }
    else if (!Attributes.TrailEffect.IsNull())
    {
        TrailEffect = Attributes.TrailEffect.LoadSynchronous();
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Sync loaded Niagara trail: %s"), *GetNameSafe(TrailEffect));
    }

    // ═══════════════════════════════════════════════════════════════════
    // AUDIO - Use preloaded or fallback to sync load
    // ═══════════════════════════════════════════════════════════════════
    if (bUsePreloaded && PreloadedCache.ExplosionSound)
    {
        ExplosionSound = PreloadedCache.ExplosionSound;
    }
    else if (!Attributes.ExplosionSound.IsNull())
    {
        ExplosionSound = Attributes.ExplosionSound.LoadSynchronous();
    }

    if (bUsePreloaded && PreloadedCache.PinPullSound)
    {
        PinSound = PreloadedCache.PinPullSound;
    }
    else if (!Attributes.PinPullSound.IsNull())
    {
        PinSound = Attributes.PinPullSound.LoadSynchronous();
    }

    if (bUsePreloaded && PreloadedCache.BounceSound)
    {
        BounceSound = PreloadedCache.BounceSound;
    }
    else if (!Attributes.BounceSound.IsNull())
    {
        BounceSound = Attributes.BounceSound.LoadSynchronous();
    }

    // ═══════════════════════════════════════════════════════════════════
    // CAMERA SHAKE - Use preloaded or fallback to sync load
    // ═══════════════════════════════════════════════════════════════════
    if (bUsePreloaded && PreloadedCache.ExplosionCameraShake)
    {
        ExplosionCameraShake = PreloadedCache.ExplosionCameraShake;
    }
    else if (!Attributes.ExplosionCameraShake.IsNull())
    {
        ExplosionCameraShake = Attributes.ExplosionCameraShake.LoadSynchronous();
    }
    CameraShakeRadius = Attributes.GetEffectiveCameraShakeRadius();

    // ═══════════════════════════════════════════════════════════════════
    // DAMAGE SYSTEM - Use preloaded or fallback to sync load
    // ═══════════════════════════════════════════════════════════════════
    bDamageInstigator = Attributes.bDamageSelf;

    if (bUsePreloaded && PreloadedCache.DamageEffectClass)
    {
        DamageEffectClass = PreloadedCache.DamageEffectClass;
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Using preloaded DamageEffect: %s"), *GetNameSafe(DamageEffectClass));
    }
    else if (!Attributes.DamageEffectClass.IsNull())
    {
        DamageEffectClass = Attributes.DamageEffectClass.LoadSynchronous();
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Sync loaded DamageEffect: %s"), *GetNameSafe(DamageEffectClass));
    }

    if (bUsePreloaded && PreloadedCache.FlashbangEffectClass)
    {
        FlashbangEffectClass = PreloadedCache.FlashbangEffectClass;
    }
    else if (!Attributes.FlashbangEffectClass.IsNull())
    {
        FlashbangEffectClass = Attributes.FlashbangEffectClass.LoadSynchronous();
    }

    if (bUsePreloaded && PreloadedCache.IncendiaryEffectClass)
    {
        IncendiaryEffectClass = PreloadedCache.IncendiaryEffectClass;
    }
    else if (!Attributes.IncendiaryEffectClass.IsNull())
    {
        IncendiaryEffectClass = Attributes.IncendiaryEffectClass.LoadSynchronous();
    }

    // ═══════════════════════════════════════════════════════════════════
    // GRENADE TYPE
    // ═══════════════════════════════════════════════════════════════════
    if (Attributes.IsFragGrenade())
    {
        GrenadeType = ESuspenseCoreGrenadeProjectileType::Fragmentation;
    }
    else if (Attributes.IsSmokeGrenade())
    {
        GrenadeType = ESuspenseCoreGrenadeProjectileType::Smoke;
    }
    else if (Attributes.IsFlashbang())
    {
        GrenadeType = ESuspenseCoreGrenadeProjectileType::Flashbang;
    }
    else if (Attributes.IsIncendiary())
    {
        GrenadeType = ESuspenseCoreGrenadeProjectileType::Incendiary;
    }
    else if (Attributes.IsLauncherRound())
    {
        GrenadeType = ESuspenseCoreGrenadeProjectileType::Impact;
    }

    GRENADE_PROJECTILE_LOG(Log, TEXT("SSOT loaded: Type=%d, Damage=%.0f, Radius=%.0f-%.0f, Fuse=%.2f, Preloaded=%s"),
        static_cast<int32>(GrenadeType), BaseDamage, InnerRadius, OuterRadius, FuseTime,
        bUsePreloaded ? TEXT("YES") : TEXT("NO"));
}

//==================================================================
// Runtime Accessors
//==================================================================

float ASuspenseCoreGrenadeProjectile::GetRemainingFuseTime() const
{
    if (!bIsArmed || bHasExploded)
    {
        return 0.0f;
    }

    float TimeSinceThrow = GetWorld()->GetTimeSeconds() - ThrowTime;
    return FMath::Max(EffectiveFuseTime - TimeSinceThrow, 0.0f);
}

float ASuspenseCoreGrenadeProjectile::GetTimeSinceThrown() const
{
    return GetWorld()->GetTimeSeconds() - ThrowTime;
}

//==================================================================
// Manual Control
//==================================================================

void ASuspenseCoreGrenadeProjectile::ForceExplode()
{
    if (!bHasExploded && HasAuthority())
    {
        GRENADE_PROJECTILE_LOG(Log, TEXT("ForceExplode called"));
        Explode();
    }
}

void ASuspenseCoreGrenadeProjectile::Defuse()
{
    if (!bHasExploded)
    {
        bIsArmed = false;
        GRENADE_PROJECTILE_LOG(Log, TEXT("Grenade defused"));

        // Stop trail effect
        if (TrailEffectComponent)
        {
            TrailEffectComponent->DestroyComponent();
            TrailEffectComponent = nullptr;
        }
    }
}

//==================================================================
// Physics Callbacks
//==================================================================

void ASuspenseCoreGrenadeProjectile::OnProjectileHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    // Impact grenades explode on hit
    if (GrenadeType == ESuspenseCoreGrenadeProjectileType::Impact && HasAuthority() && !bHasExploded)
    {
        // Small delay to prevent immediate explosion
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ASuspenseCoreGrenadeProjectile::ForceExplode, 0.05f, false);
    }
}

void ASuspenseCoreGrenadeProjectile::OnProjectileBounce(
    const FHitResult& ImpactResult,
    const FVector& ImpactVelocity)
{
    // Play bounce sound
    if (ImpactVelocity.Size() > 100.0f)  // Only for significant impacts
    {
        PlayGrenadeSound(BounceSound);
    }

    GRENADE_PROJECTILE_LOG(Verbose, TEXT("Bounced: Surface=%s, Velocity=%.0f"),
        *GetNameSafe(ImpactResult.GetActor()),
        ImpactVelocity.Size());
}

//==================================================================
// Explosion
//==================================================================

void ASuspenseCoreGrenadeProjectile::Explode_Implementation()
{
    if (bHasExploded)
    {
        return;
    }

    bHasExploded = true;

    // Notify pre-explosion
    OnPreExplosion();

    // Stop trail effect
    if (TrailEffectComponent)
    {
        TrailEffectComponent->DestroyComponent();
        TrailEffectComponent = nullptr;
    }

    // Apply damage (server only)
    if (HasAuthority())
    {
        ApplyExplosionDamage();
    }

    // Spawn effects (all clients)
    Multicast_SpawnExplosionEffects();

    // Publish EventBus event
    PublishExplosionEvent();

    // Broadcast delegate
    FSuspenseCoreGrenadeExplosionData ExplosionData;
    ExplosionData.ExplosionLocation = GetActorLocation();
    ExplosionData.InnerRadius = InnerRadius;
    ExplosionData.OuterRadius = OuterRadius;
    ExplosionData.BaseDamage = BaseDamage;
    ExplosionData.DamageFalloff = DamageFalloff;
    ExplosionData.GrenadeType = GrenadeType;
    ExplosionData.Instigator = InstigatorActor;

    OnExploded.Broadcast(ExplosionData);

    // Notify post-explosion
    OnPostExplosion();

    GRENADE_PROJECTILE_LOG(Log, TEXT("Exploded at %s"), *GetActorLocation().ToString());

    // Destroy after short delay (allows effects to play)
    SetLifeSpan(0.1f);
}

void ASuspenseCoreGrenadeProjectile::ApplyExplosionDamage()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FVector ExplosionLocation = GetActorLocation();

    // Find all actors in radius
    TArray<FOverlapResult> Overlaps;
    FCollisionShape CollisionShape;
    CollisionShape.SetSphere(OuterRadius);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    if (!bDamageInstigator && InstigatorActor.IsValid())
    {
        QueryParams.AddIgnoredActor(InstigatorActor.Get());
    }

    World->OverlapMultiByChannel(
        Overlaps,
        ExplosionLocation,
        FQuat::Identity,
        ECC_Pawn,
        CollisionShape,
        QueryParams);

    GRENADE_PROJECTILE_LOG(Log, TEXT("ApplyExplosionDamage: Found %d potential targets"), Overlaps.Num());

    // Process each target
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* TargetActor = Overlap.GetActor();
        if (!TargetActor)
        {
            continue;
        }

        // Check visibility (not blocked by wall)
        if (!bIgnoreWalls && !IsTargetVisible(TargetActor))
        {
            GRENADE_PROJECTILE_LOG(Verbose, TEXT("  Target %s blocked by wall"), *TargetActor->GetName());
            continue;
        }

        // Calculate distance
        float Distance = FVector::Dist(ExplosionLocation, TargetActor->GetActorLocation());

        // Calculate damage
        float Damage = CalculateDamageForTarget(TargetActor, Distance);
        if (Damage <= 0.0f)
        {
            continue;
        }

        // Apply damage via GAS if available
        UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
        if (TargetASC)
        {
            // Use configured damage effect or default to USuspenseCoreDamageEffect
            TSubclassOf<UGameplayEffect> EffectToApply = DamageEffectClass;
            if (!EffectToApply)
            {
                EffectToApply = USuspenseCoreDamageEffect::StaticClass();
            }

            // Create effect context
            FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
            EffectContext.AddSourceObject(this);
            EffectContext.AddInstigator(InstigatorActor.Get(), this);
            EffectContext.AddHitResult(FHitResult());

            // Create effect spec
            FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
                EffectToApply, 1.0f, EffectContext);

            if (SpecHandle.IsValid())
            {
                // Set damage value via SetByCaller (POSITIVE - PostGameplayEffectExecute handles IncomingDamage)
                SpecHandle.Data->SetSetByCallerMagnitude(
                    SuspenseCoreTags::Data::Damage, Damage);

                // Apply effect
                TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

                GRENADE_PROJECTILE_LOG(Log, TEXT("  Applied %.0f damage to %s (Distance=%.0f)"),
                    Damage, *TargetActor->GetName(), Distance);
            }
        }
        else
        {
            // Fallback: Apply damage via standard AActor::TakeDamage
            FDamageEvent DamageEvent;
            TargetActor->TakeDamage(Damage, DamageEvent, nullptr, InstigatorActor.Get());

            GRENADE_PROJECTILE_LOG(Log, TEXT("  Applied %.0f damage to %s (via TakeDamage)"),
                Damage, *TargetActor->GetName());
        }

        // Apply special effects based on grenade type
        if (TargetASC)
        {
            switch (GrenadeType)
            {
                case ESuspenseCoreGrenadeProjectileType::Flashbang:
                    if (FlashbangEffectClass)
                    {
                        FGameplayEffectContextHandle FlashContext = TargetASC->MakeEffectContext();
                        FlashContext.AddSourceObject(this);
                        FGameplayEffectSpecHandle FlashSpec = TargetASC->MakeOutgoingSpec(
                            FlashbangEffectClass, 1.0f, FlashContext);
                        if (FlashSpec.IsValid())
                        {
                            TargetASC->ApplyGameplayEffectSpecToSelf(*FlashSpec.Data.Get());
                        }
                    }
                    break;

                case ESuspenseCoreGrenadeProjectileType::Incendiary:
                    if (IncendiaryEffectClass)
                    {
                        FGameplayEffectContextHandle FireContext = TargetASC->MakeEffectContext();
                        FireContext.AddSourceObject(this);
                        FGameplayEffectSpecHandle FireSpec = TargetASC->MakeOutgoingSpec(
                            IncendiaryEffectClass, 1.0f, FireContext);
                        if (FireSpec.IsValid())
                        {
                            TargetASC->ApplyGameplayEffectSpecToSelf(*FireSpec.Data.Get());
                        }
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

void ASuspenseCoreGrenadeProjectile::Multicast_SpawnExplosionEffects_Implementation()
{
    FVector ExplosionLocation = GetActorLocation();
    FRotator ExplosionRotation = FRotator::ZeroRotator;

    // Play explosion sound
    PlayGrenadeSound(ExplosionSound);

    // Spawn explosion effect
    if (ExplosionEffect)
    {
        SpawnEffect(ExplosionEffect, ExplosionLocation, ExplosionRotation);
    }

    // Spawn smoke effect (for smoke grenades)
    if (GrenadeType == ESuspenseCoreGrenadeProjectileType::Smoke && SmokeEffect)
    {
        SpawnEffect(SmokeEffect, ExplosionLocation, ExplosionRotation);
    }

    // Camera shake - uses Blueprint-configured ExplosionCameraShake class
    // This allows using SuspenseCoreExplosionCameraShake from PlayerCore via Blueprint
    // without creating circular module dependencies
    UWorld* World = GetWorld();
    if (World && ExplosionCameraShake)
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (PC && PC->IsLocalController())
            {
                APawn* Pawn = PC->GetPawn();
                if (Pawn)
                {
                    float Distance = FVector::Dist(ExplosionLocation, Pawn->GetActorLocation());

                    // Only shake if within camera shake radius
                    if (Distance <= CameraShakeRadius)
                    {
                        // Calculate scale based on distance (1.0 at center, 0.0 at edge)
                        float DistanceScale = 1.0f - FMath::Clamp(
                            Distance / CameraShakeRadius,
                            0.0f, 1.0f);

                        // Apply shake via PlayerCameraManager
                        if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
                        {
                            CameraManager->StartCameraShake(
                                ExplosionCameraShake,
                                DistanceScale,
                                ECameraShakePlaySpace::World,
                                FRotator::ZeroRotator);
                        }
                    }
                }
            }
        }
    }

    GRENADE_PROJECTILE_LOG(Verbose, TEXT("Spawned explosion effects at %s"), *ExplosionLocation.ToString());
}

float ASuspenseCoreGrenadeProjectile::CalculateDamageForTarget(AActor* Target, float Distance) const
{
    if (!Target || Distance > OuterRadius)
    {
        return 0.0f;
    }

    // Full damage within inner radius
    if (Distance <= InnerRadius)
    {
        return BaseDamage;
    }

    // Falloff damage between inner and outer radius
    float FalloffRange = OuterRadius - InnerRadius;
    float DistanceInFalloff = Distance - InnerRadius;
    float FalloffFactor = 1.0f - FMath::Pow(DistanceInFalloff / FalloffRange, DamageFalloff);

    return BaseDamage * FMath::Max(FalloffFactor, 0.0f);
}

bool ASuspenseCoreGrenadeProjectile::IsTargetVisible(AActor* Target) const
{
    if (!Target)
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    FVector Start = GetActorLocation();
    FVector End = Target->GetActorLocation();

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(Target);

    // Line trace to check for blocking geometry
    bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility,
        QueryParams);

    // If nothing was hit, target is visible
    return !bHit;
}

//==================================================================
// Replication
//==================================================================

void ASuspenseCoreGrenadeProjectile::OnRep_IsArmed()
{
    if (bIsArmed)
    {
        OnGrenadeArmed();
    }
}

//==================================================================
// Internal Methods
//==================================================================

void ASuspenseCoreGrenadeProjectile::ArmGrenade()
{
    bIsArmed = true;
    OnGrenadeArmed();

    GRENADE_PROJECTILE_LOG(Log, TEXT("Grenade armed, EffectiveFuseTime=%.2f"), EffectiveFuseTime);
}

void ASuspenseCoreGrenadeProjectile::PlayGrenadeSound(USoundBase* Sound)
{
    if (!Sound)
    {
        return;
    }

    UGameplayStatics::PlaySoundAtLocation(
        this,
        Sound,
        GetActorLocation(),
        GetActorRotation(),
        1.0f,
        1.0f,
        0.0f,
        nullptr,
        nullptr,
        this);
}

UNiagaraComponent* ASuspenseCoreGrenadeProjectile::SpawnEffect(
    UNiagaraSystem* Effect,
    const FVector& Location,
    const FRotator& Rotation)
{
    if (!Effect)
    {
        return nullptr;
    }

    return UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(),
        Effect,
        Location,
        Rotation,
        FVector(1.0f),
        true,
        true,
        ENCPoolMethod::AutoRelease);
}

USuspenseCoreEventBus* ASuspenseCoreGrenadeProjectile::GetEventBus()
{
    // Use cached EventBus if valid
    if (EventBus.IsValid())
    {
        return EventBus.Get();
    }

    // Get EventBus through EventManager (project standard pattern)
    if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
    {
        USuspenseCoreEventBus* FoundEventBus = Manager->GetEventBus();
        if (FoundEventBus)
        {
            EventBus = FoundEventBus;
            return FoundEventBus;
        }
    }

    return nullptr;
}

void ASuspenseCoreGrenadeProjectile::PublishExplosionEvent()
{
    if (!EventBus.IsValid())
    {
        // Try to get EventBus again
        EventBus = GetEventBus();
        if (!EventBus.IsValid())
        {
            return;
        }
    }

    FSuspenseCoreEventData EventData;
    EventData.Source = InstigatorActor.Get();
    EventData.Timestamp = FPlatformTime::Seconds();

    EventData.StringPayload.Add(TEXT("GrenadeID"), GrenadeID.ToString());
    EventData.VectorPayload.Add(TEXT("ExplosionLocation"), GetActorLocation());
    EventData.FloatPayload.Add(TEXT("Damage"), BaseDamage);
    EventData.FloatPayload.Add(TEXT("InnerRadius"), InnerRadius);
    EventData.FloatPayload.Add(TEXT("OuterRadius"), OuterRadius);
    EventData.IntPayload.Add(TEXT("GrenadeType"), static_cast<int32>(GrenadeType));

    // Note: Using a generic tag here - you may want to add a specific tag
    // for grenade explosions in SuspenseCoreGameplayTags.h
    static const FGameplayTag ExplosionTag = FGameplayTag::RequestGameplayTag(
        FName("SuspenseCore.Event.Throwable.Exploded"), false);

    if (ExplosionTag.IsValid())
    {
        EventBus->Publish(ExplosionTag, EventData);
        GRENADE_PROJECTILE_LOG(Verbose, TEXT("Published explosion event via EventBus"));
    }
}

#undef GRENADE_PROJECTILE_LOG
