#include "Core/Enemy/StatesAI/MedComEnemyDetectionHelper.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"

// Initialize static variables
TWeakObjectPtr<ACharacter> UMedComEnemyDetectionHelper::CachedPlayerCharacter = nullptr;
double UMedComEnemyDetectionHelper::LastCacheUpdateTime = 0.0;
double UMedComEnemyDetectionHelper::LastLOSCacheClearTime = 0.0;
double UMedComEnemyDetectionHelper::LastSpatialGridUpdateTime = 0.0;
const double UMedComEnemyDetectionHelper::CACHE_UPDATE_INTERVAL = 1.0;
const double UMedComEnemyDetectionHelper::LOS_CACHE_LIFETIME = 0.5;
const double UMedComEnemyDetectionHelper::SPATIAL_GRID_UPDATE_INTERVAL = 2.0;
TMap<uint32, bool> UMedComEnemyDetectionHelper::LOSCache;
TMap<FIntVector, TArray<AMedComEnemyCharacter*>> UMedComEnemyDetectionHelper::SpatialGrid;

bool UMedComEnemyDetectionHelper::IsPlayerDetectable(
    AMedComEnemyCharacter* Enemy,
    float DetectionRadius,
    float DetectionAngle,
    bool bRequireLineOfSight)
{
    // Early authority check
    if (!Enemy || !Enemy->HasAuthority())
    {
        return false;
    }
    
    // Ограничиваем радиус обнаружения максимальным значением
    const float MAX_DETECTION_RADIUS = 50.0f; // 50 метров
    DetectionRadius = FMath::Min(DetectionRadius, MAX_DETECTION_RADIUS);
    
    ACharacter* PlayerCharacter = GetPlayerCharacter(Enemy);
    if (!PlayerCharacter)
    {
        return false;
    }
    
    // First check distance (fast rejection)
    FVector EnemyLocation = Enemy->GetActorLocation();
    FVector PlayerLocation = PlayerCharacter->GetActorLocation();
    float DistanceSquared = FVector::DistSquared(EnemyLocation, PlayerLocation);
    float RadiusSquared = (DetectionRadius * 100.0f) * (DetectionRadius * 100.0f); // переводим в сантиметры
    
    if (DistanceSquared > RadiusSquared)
    {
        return false;
    }
    
    // Check field of view if needed
    bool InFieldOfView = true;
    if (DetectionAngle < 360.0f)
    {
        FVector EnemyForward = Enemy->GetActorForwardVector();
        FVector DirectionToPlayer = (PlayerLocation - EnemyLocation).GetSafeNormal();
        
        float DotProduct = FVector::DotProduct(EnemyForward, DirectionToPlayer);
        float AngleCos = FMath::Cos(FMath::DegreesToRadians(DetectionAngle * 0.5f));
        
        InFieldOfView = (DotProduct >= AngleCos);
    }
    
    if (!InFieldOfView)
    {
        return false;
    }
    
    // ВСЕГДА проверяем линию видимости, игнорируя параметр bRequireLineOfSight
    return HasLineOfSightTo(Enemy, PlayerCharacter);
}

ACharacter* UMedComEnemyDetectionHelper::GetPlayerCharacter(UObject* WorldContext)
{
    if (!WorldContext || !WorldContext->GetWorld())
    {
        return nullptr;
    }
    
    UWorld* World = WorldContext->GetWorld();
    double CurrentTime = World->GetTimeSeconds();
    
    // Check if we need to update cache
    if (!CachedPlayerCharacter.IsValid() || (CurrentTime - LastCacheUpdateTime > CACHE_UPDATE_INTERVAL))
    {
        CachedPlayerCharacter = UGameplayStatics::GetPlayerCharacter(World, 0);
        LastCacheUpdateTime = CurrentTime;
        
        // Also clear LOS cache periodically
        if (CurrentTime - LastLOSCacheClearTime > LOS_CACHE_LIFETIME)
        {
            LOSCache.Empty();
            LastLOSCacheClearTime = CurrentTime;
        }
    }
    
    return CachedPlayerCharacter.Get();
}

bool UMedComEnemyDetectionHelper::HasLineOfSightTo(AMedComEnemyCharacter* Enemy, AActor* Target)
{
    if (!Enemy || !Target || !Enemy->GetWorld())
    {
        return false;
    }
    
    // Generate cache key and check if in cache
    uint32 CacheKey = GenerateLOSCacheKey(Enemy, Target);
    if (LOSCache.Contains(CacheKey))
    {
        return LOSCache[CacheKey];
    }
    
    FVector EnemyEyeLocation = Enemy->GetActorLocation() + FVector(0, 0, 50); // Eye level
    FVector TargetLocation = Target->GetActorLocation() + FVector(0, 0, 50);
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Enemy);
    QueryParams.bTraceComplex = false; // Use simple collision for performance
    
    // Use single trace with visibility channel
    bool Blocked = Enemy->GetWorld()->LineTraceSingleByChannel(
        HitResult,
        EnemyEyeLocation,
        TargetLocation,
        ECC_Visibility,
        QueryParams
    );
    
    bool HasLOS = !Blocked || HitResult.GetActor() == Target;
    
    // Cache the result
    LOSCache.Add(CacheKey, HasLOS);
    
    return HasLOS;
}

void UMedComEnemyDetectionHelper::BatchDetectPlayers(
    const TArray<AMedComEnemyCharacter*>& Enemies,
    float DetectionRadius,
    float DetectionAngle,
    bool bRequireLineOfSight,
    TArray<AMedComEnemyCharacter*>& OutDetectingEnemies)
{
    // Clear output array
    OutDetectingEnemies.Empty();
    
    // Get world from first valid enemy
    UWorld* World = nullptr;
    for (AMedComEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy && Enemy->HasAuthority())
        {
            World = Enemy->GetWorld();
            break;
        }
    }
    
    if (!World || Enemies.Num() == 0)
        return;
        
    // Get player once for all checks
    ACharacter* PlayerCharacter = GetPlayerCharacter(World);
    if (!PlayerCharacter)
        return;
        
    FVector PlayerLocation = PlayerCharacter->GetActorLocation();
    float RadiusSquared = DetectionRadius * DetectionRadius;
    float AngleCos = 0.0f;
    
    if (DetectionAngle < 180.0f)
    {
        AngleCos = FMath::Cos(FMath::DegreesToRadians(DetectionAngle * 0.5f));
    }
    
    // Process all enemies in one pass
    for (AMedComEnemyCharacter* Enemy : Enemies)
    {
        if (!Enemy || !Enemy->HasAuthority())
            continue;
            
        FVector EnemyLocation = Enemy->GetActorLocation();
        float DistanceSquared = FVector::DistSquared(EnemyLocation, PlayerLocation);
        
        // Fast distance check
        if (DistanceSquared > RadiusSquared)
            continue;
            
        // Field of view check if needed
        if (DetectionAngle < 180.0f)
        {
            FVector EnemyForward = Enemy->GetActorForwardVector();
            FVector DirectionToPlayer = (PlayerLocation - EnemyLocation).GetSafeNormal();
            
            float DotProduct = FVector::DotProduct(EnemyForward, DirectionToPlayer);
            
            if (DotProduct < AngleCos)
                continue; // Not in field of view
        }
        
        // Line of sight check if needed
        if (bRequireLineOfSight)
        {
            uint32 CacheKey = GenerateLOSCacheKey(Enemy, PlayerCharacter);
            bool HasLOS = false;
            
            if (LOSCache.Contains(CacheKey))
            {
                HasLOS = LOSCache[CacheKey];
            }
            else
            {
                FVector EnemyEyeLocation = Enemy->GetActorLocation() + FVector(0, 0, 50);
                FVector TargetLocation = PlayerCharacter->GetActorLocation() + FVector(0, 0, 50);
                
                FHitResult HitResult;
                FCollisionQueryParams QueryParams;
                QueryParams.AddIgnoredActor(Enemy);
                QueryParams.bTraceComplex = false;
                
                bool Blocked = World->LineTraceSingleByChannel(
                    HitResult,
                    EnemyEyeLocation,
                    TargetLocation,
                    ECC_Visibility,
                    QueryParams
                );
                
                HasLOS = !Blocked || HitResult.GetActor() == PlayerCharacter;
                LOSCache.Add(CacheKey, HasLOS);
            }
            
            if (!HasLOS)
                continue;
        }
            
        // This enemy detected player
        OutDetectingEnemies.Add(Enemy);
    }
}

uint32 UMedComEnemyDetectionHelper::GenerateLOSCacheKey(const AActor* Source, const AActor* Target)
{
    if (!Source || !Target)
        return 0;
        
    // Create a simple hash from pointers
    uint32 SourceHash = GetTypeHash(Source);
    uint32 TargetHash = GetTypeHash(Target);
    
    return HashCombine(SourceHash, TargetHash);
}

void UMedComEnemyDetectionHelper::GetSpatialBucketData(UWorld* World, TMap<FIntVector, TArray<AMedComEnemyCharacter*>>& OutBuckets, float BucketSize)
{
    double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
    
    // Check if we need to update the grid
    if (ShouldUpdateSpatialGrid(World))
    {
        // Clear old grid
        SpatialGrid.Empty();
        
        // Find all enemies and place them in grid
        for (TActorIterator<AMedComEnemyCharacter> It(World); It; ++It)
        {
            AMedComEnemyCharacter* Enemy = *It;
            if (IsValid(Enemy))
            {
                FIntVector Cell = WorldToGridCell(Enemy->GetActorLocation(), BucketSize);
                
                if (!SpatialGrid.Contains(Cell))
                {
                    SpatialGrid.Add(Cell, TArray<AMedComEnemyCharacter*>());
                }
                
                SpatialGrid[Cell].Add(Enemy);
            }
        }
        
        LastSpatialGridUpdateTime = CurrentTime;
    }
    
    // Return the current grid
    OutBuckets = SpatialGrid;
}

bool UMedComEnemyDetectionHelper::ShouldUpdateSpatialGrid(UWorld* World)
{
    if (!World)
        return false;
        
    double CurrentTime = World->GetTimeSeconds();
    return (CurrentTime - LastSpatialGridUpdateTime > SPATIAL_GRID_UPDATE_INTERVAL);
}

FIntVector UMedComEnemyDetectionHelper::WorldToGridCell(const FVector& WorldPos, float BucketSize)
{
    return FIntVector(
        FMath::Floor(WorldPos.X / BucketSize),
        FMath::Floor(WorldPos.Y / BucketSize),
        FMath::Floor(WorldPos.Z / BucketSize)
    );
}

void UMedComEnemyDetectionHelper::ResetDetectionCache()
{
    CachedPlayerCharacter = nullptr;
    LOSCache.Empty();
    LastCacheUpdateTime = 0.0;
    LastLOSCacheClearTime = 0.0;
}