// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "SuspenseTooltipManager.generated.h"

// Forward declarations
class USuspenseItemTooltipWidget;
class UUserWidget;
class APlayerController;
class USuspenseEventManager;

/**
 * Configuration structure for tooltip behavior
 * Allows fine-tuning of tooltip system per project needs
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FTooltipConfiguration
{
    GENERATED_BODY()

    /** Default tooltip widget class when none specified */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
    TSubclassOf<USuspenseItemTooltipWidget> DefaultTooltipClass;

    /** Z-order for tooltip display (higher = on top) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip", meta = (ClampMin = "100", ClampMax = "10000"))
    int32 TooltipZOrder = 1000;

    /** Maximum tooltip widgets to pool per class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxPooledTooltipsPerClass = 3;

    /** Whether to allow multiple tooltip classes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
    bool bAllowMultipleTooltipClasses = true;

    /** Whether to log detailed tooltip operations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip|Debug")
    bool bEnableDetailedLogging = false;

    FTooltipConfiguration()
    {
        DefaultTooltipClass = nullptr;
        TooltipZOrder = 1000;
        MaxPooledTooltipsPerClass = 3;
        bAllowMultipleTooltipClasses = true;
        bEnableDetailedLogging = false;
    }
};

/**
 * Pool structure for managing tooltip widgets of a specific class
 * Tracks available and in-use widgets for efficient memory management
 */
USTRUCT()
struct UISYSTEM_API FTooltipPool
{
    GENERATED_BODY()

    /** Available widgets ready to be used */
    UPROPERTY()
    TArray<USuspenseItemTooltipWidget*> AvailableWidgets;

    /** Widgets currently being displayed */
    UPROPERTY()
    TArray<USuspenseItemTooltipWidget*> InUseWidgets;

    /** Maximum number of widgets to keep in pool */
    int32 MaxPoolSize = 3;

    /** Total number of widgets created for this class (for statistics) */
    int32 TotalCreated = 0;

    FTooltipPool()
    {
        MaxPoolSize = 3;
        TotalCreated = 0;
    }

    /** Get total widget count (available + in use) */
    int32 GetTotalCount() const { return AvailableWidgets.Num() + InUseWidgets.Num(); }

    /** Check if pool has available widgets */
    bool HasAvailable() const { return AvailableWidgets.Num() > 0; }

    /** Check if pool is at capacity */
    bool IsAtCapacity() const { return AvailableWidgets.Num() >= MaxPoolSize; }
};

/**
 * Enhanced Centralized Tooltip Management System
 *
 * Key Features:
 * - Support for multiple tooltip widget classes
 * - Per-class object pooling for performance
 * - Complete decoupling through EventDelegateManager
 * - No hardcoded paths - all configuration through Blueprint
 * - Smart memory management with configurable pool sizes
 *
 * Architecture:
 * - Slots can specify custom tooltip classes
 * - Falls back to default class when none specified
 * - Maintains separate pools for each tooltip class
 * - Integrates with existing event system
 */
UCLASS(BlueprintType, Blueprintable, Config = Game)
class UISYSTEM_API USuspenseTooltipManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ========================================
    // Subsystem Lifecycle
    // ========================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ========================================
    // Configuration Management
    // ========================================

    /**
     * Update tooltip configuration at runtime
     * Allows dynamic adjustment of tooltip behavior
     * @param NewConfig New configuration to apply
     */
    UFUNCTION(BlueprintCallable, Category = "Tooltip|Config")
    void UpdateConfiguration(const FTooltipConfiguration& NewConfig);

    /**
     * Get current tooltip configuration
     * @return Active configuration structure
     */
    UFUNCTION(BlueprintPure, Category = "Tooltip|Config")
    const FTooltipConfiguration& GetConfiguration() const { return Configuration; }

    /**
     * Set default tooltip widget class
     * This is used when slots don't specify a custom class
     * @param InTooltipClass Default class to use
     */
    UFUNCTION(BlueprintCallable, Category = "Tooltip|Config")
    void SetDefaultTooltipClass(TSubclassOf<USuspenseItemTooltipWidget> InTooltipClass);

    /**
     * Register a new tooltip class for pooling
     * Pre-creates pool for better performance
     * @param TooltipClass Class to register
     * @param PoolSize Number of widgets to pre-create
     */
    UFUNCTION(BlueprintCallable, Category = "Tooltip|Config")
    void RegisterTooltipClass(TSubclassOf<USuspenseItemTooltipWidget> TooltipClass, int32 PoolSize = 3);

    // ========================================
    // Tooltip Control
    // ========================================

    /**
     * Force hide any active tooltip
     * Useful for UI state changes, scene transitions, etc.
     */
    UFUNCTION(BlueprintCallable, Category = "Tooltip|Control")
    void ForceHideTooltip();

    /**
     * Check if any tooltip is currently visible
     * @return True if a tooltip is being displayed
     */
    UFUNCTION(BlueprintPure, Category = "Tooltip|Control")
    bool IsTooltipActive() const;

    /**
     * Get the class of currently active tooltip
     * @return Class of active tooltip or nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Tooltip|Control")
    UClass* GetActiveTooltipClass() const { return ActiveTooltipClass; }

    // ========================================
    // Statistics and Debug
    // ========================================

    /**
     * Get total number of tooltip widgets across all pools
     * @return Total widget count
     */
    UFUNCTION(BlueprintPure, Category = "Tooltip|Stats")
    int32 GetTotalTooltipCount() const;

    /**
     * Get number of active (non-pooled) tooltips
     * @return Active tooltip count
     */
    UFUNCTION(BlueprintPure, Category = "Tooltip|Stats")
    int32 GetActiveTooltipCount() const;

    /**
     * Get pool statistics for specific tooltip class
     * @param TooltipClass Class to check
     * @param OutPoolSize Current pool size
     * @param OutInUse Number currently in use
     */
    UFUNCTION(BlueprintCallable, Category = "Tooltip|Stats")
    void GetPoolStats(TSubclassOf<USuspenseItemTooltipWidget> TooltipClass, int32& OutPoolSize, int32& OutInUse) const;

    /**
     * Clear all tooltip pools
     * Destroys all pooled widgets to free memory
     */
    UFUNCTION(BlueprintCallable, Category = "Tooltip|Debug")
    void ClearAllPools();

    /**
     * Log current tooltip system state
     * Useful for debugging tooltip issues
     */
    UFUNCTION(BlueprintCallable, Category = "Tooltip|Debug")
    void LogTooltipSystemState() const;

protected:
    // ========================================
    // Configuration
    // ========================================

    /** Active tooltip configuration */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    FTooltipConfiguration Configuration;

private:
    // ========================================
    // Pool Management
    // ========================================

    /** Tooltip pools organized by widget class */
    UPROPERTY()
    TMap<UClass*, FTooltipPool> TooltipPools;

    /** Currently active tooltip widget */
    UPROPERTY()
    USuspenseItemTooltipWidget* ActiveTooltip;

    /** Class of the currently active tooltip */
    UPROPERTY()
    UClass* ActiveTooltipClass;

    /** Widget that currently owns the active tooltip */
    UPROPERTY()
    TWeakObjectPtr<UUserWidget> CurrentSourceWidget;

    /** Cached reference to event manager */
    UPROPERTY()
    USuspenseCoreEventManager* CachedEventManager;

    /** Event subscription handles for cleanup */
    FDelegateHandle TooltipRequestHandle;
    FDelegateHandle TooltipHideHandle;
    FDelegateHandle TooltipUpdateHandle;

    // ========================================
    // Event Handlers
    // ========================================

    /**
     * Handle tooltip request from EventDelegateManager
     * Determines which tooltip class to use based on item data
     */
    void OnTooltipRequested(const FItemUIData& ItemData, const FVector2D& ScreenPosition);

    /**
     * Handle tooltip hide request
     */
    void OnTooltipHideRequested();

    /**
     * Handle tooltip position update
     */
    void OnTooltipUpdatePosition(const FVector2D& ScreenPosition);

    // ========================================
    // Internal Management
    // ========================================

    /**
     * Process tooltip display request with specific class
     * @param SourceWidget Widget requesting tooltip (optional)
     * @param ItemData Data to display
     * @param ScreenPosition Initial position
     * @param TooltipClass Class to use (will use default if null)
     */
    void ProcessTooltipRequest(
        UUserWidget* SourceWidget,
        const FItemUIData& ItemData,
        const FVector2D& ScreenPosition,
        TSubclassOf<UUserWidget> TooltipClass);

    /**
     * Process tooltip hide request
     * @param SourceWidget Widget requesting hide (optional)
     */
    void ProcessTooltipHide(UUserWidget* SourceWidget);

    /**
     * Acquire tooltip widget from pool or create new
     * @param TooltipClass Class of tooltip to acquire
     * @return Tooltip widget ready for use
     */
    USuspenseItemTooltipWidget* AcquireTooltipWidget(TSubclassOf<UUserWidget> TooltipClass);

    /**
     * Return tooltip widget to pool
     * @param Tooltip Widget to return
     * @param TooltipClass Class of the tooltip
     */
    void ReleaseTooltipWidget(USuspenseItemTooltipWidget* Tooltip, UClass* TooltipClass);

    /**
     * Create new tooltip widget instance
     * @param TooltipClass Class to instantiate
     * @return New tooltip widget or nullptr on failure
     */
    USuspenseItemTooltipWidget* CreateTooltipWidget(TSubclassOf<UUserWidget> TooltipClass);

    /**
     * Get or create pool for tooltip class
     * @param TooltipClass Class to get pool for
     * @return Pool structure reference
     */
    FTooltipPool& GetOrCreatePool(UClass* TooltipClass);

    /**
     * Clean up specific tooltip pool
     * @param TooltipClass Class pool to clean
     */
    void CleanupTooltipPool(UClass* TooltipClass);

    /**
     * Clean up all tooltip pools
     */
    void CleanupAllPools();

    /**
     * Get player controller for UI operations
     */
    APlayerController* GetOwningPlayerController() const;

    /**
     * Get event manager subsystem
     */
    USuspenseEventManager* GetEventManager() const;

    /**
     * Determine tooltip class from item data
     * @param ItemData Item data that might contain preferred class
     * @return Tooltip class to use
     */
    TSubclassOf<UUserWidget> DetermineTooltipClass(const FItemUIData& ItemData) const;

    /**
     * Log message with verbose level control
     */
    void LogVerbose(const FString& Message) const;
};
