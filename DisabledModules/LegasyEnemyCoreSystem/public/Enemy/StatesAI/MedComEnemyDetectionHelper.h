#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MedComEnemyDetectionHelper.generated.h"

class AMedComEnemyCharacter;
class ACharacter;

/**
 * Helper class for enemy detection logic to avoid code duplication
 * Optimized version with caching and batch processing
 */
UCLASS()
class MEDCOMCORE_API UMedComEnemyDetectionHelper : public UObject
{
    GENERATED_BODY()
    
public:
    /**
     * Check if player is detectable by enemy - SERVER ONLY
     * @param Enemy - The enemy
     * @param DetectionRadius - Maximum detection distance
     * @param DetectionAngle - Field of view angle (in degrees)
     * @param bRequireLineOfSight - Whether direct line of sight is required
     * @return True if player is detectable
     */
    static bool IsPlayerDetectable(
        AMedComEnemyCharacter* Enemy,
        float DetectionRadius,
        float DetectionAngle,
        bool bRequireLineOfSight);
        
    /**
     * Find player character with caching for optimization
     * @param WorldContext - Object to get world from
     * @return Player character or nullptr if not found
     */
    static ACharacter* GetPlayerCharacter(UObject* WorldContext);
    
    /**
     * Check line of sight to target
     * @param Enemy - Enemy performing check
     * @param Target - Target actor to check
     * @return True if target is visible
     */
    static bool HasLineOfSightTo(AMedComEnemyCharacter* Enemy, AActor* Target);
    
    /**
     * Batch detection for group of enemies - OPTIMIZED METHOD
     * @param Enemies - Array of enemies to check
     * @param DetectionRadius - Detection radius
     * @param DetectionAngle - Field of view angle
     * @param bRequireLineOfSight - Whether line of sight is required
     * @param OutDetectingEnemies - Output array of enemies that detected player
     */
    static void BatchDetectPlayers(
        const TArray<AMedComEnemyCharacter*>& Enemies,
        float DetectionRadius,
        float DetectionAngle,
        bool bRequireLineOfSight,
        TArray<AMedComEnemyCharacter*>& OutDetectingEnemies);
        
    /** Get spatial grid data that contains enemies - for spatial partitioning */
    static void GetSpatialBucketData(UWorld* World, TMap<FIntVector, TArray<AMedComEnemyCharacter*>>& OutBuckets, 
                                     float BucketSize = 1000.0f);
                                     
    /** Check if spatial grid needs update */
    static bool ShouldUpdateSpatialGrid(UWorld* World);
    
    /** Reset the detection cache to force refresh */
    static void ResetDetectionCache();
        
private:
    // Cache for player character reference
    static TWeakObjectPtr<ACharacter> CachedPlayerCharacter;
    
    // Last time cache was updated
    static double LastCacheUpdateTime;
    
    // Last time spatial grid was updated
    static double LastSpatialGridUpdateTime;
    
    // Cache update interval (seconds)
    static const double CACHE_UPDATE_INTERVAL;
    
    // Spatial grid update interval
    static const double SPATIAL_GRID_UPDATE_INTERVAL;
    
    // Cache for line of sight results
    static TMap<uint32, bool> LOSCache;
    
    // Last time LOS cache was cleared
    static double LastLOSCacheClearTime;
    
    // LOS cache lifetime
    static const double LOS_CACHE_LIFETIME;
    
    // Generate hash for LOS caching
    static uint32 GenerateLOSCacheKey(const AActor* Source, const AActor* Target);
    
    // Spatial grid data for enemies
    static TMap<FIntVector, TArray<AMedComEnemyCharacter*>> SpatialGrid;
    
    // Convert world position to grid cell
    static FIntVector WorldToGridCell(const FVector& WorldPos, float BucketSize);
};