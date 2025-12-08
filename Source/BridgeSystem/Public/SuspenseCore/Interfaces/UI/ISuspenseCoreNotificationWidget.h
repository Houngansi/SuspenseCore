// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_INTERFACES_UI_ISUSPENSECORENOTIFICATIONWIDGET_H
#define SUSPENSECORE_INTERFACES_UI_ISUSPENSECORENOTIFICATIONWIDGET_H

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISuspenseNotificationWidget.generated.h"

// Forward declarations - только базовые UE типы
class USuspenseEventManager;

/**
 * Notification priority levels
 */
UENUM(BlueprintType)
enum class ESuspenseNotificationPriority : uint8
{
    Low         UMETA(DisplayName = "Low"),
    Normal      UMETA(DisplayName = "Normal"),
    High        UMETA(DisplayName = "High"),
    Critical    UMETA(DisplayName = "Critical")
};

/**
 * Notification type for styling
 */
UENUM(BlueprintType)
enum class ESuspenseNotificationType : uint8
{
    Info        UMETA(DisplayName = "Information"),
    Success     UMETA(DisplayName = "Success"),
    Warning     UMETA(DisplayName = "Warning"),
    Error       UMETA(DisplayName = "Error")
};

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseNotificationWidget : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for notification widgets
 * Provides standardized methods for displaying game notifications
 */
class BRIDGESYSTEM_API ISuspenseNotificationWidget
{
    GENERATED_BODY()

public:
    //================================================
    // Notification Display
    //================================================
    
    /**
     * Shows a notification
     * @param Message Notification message
     * @param Duration Display duration (0 = infinite)
     * @param Type Notification type for styling
     * @param Priority Notification priority
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    void ShowNotification(const FText& Message, float Duration, ESuspenseNotificationType Type, ESuspenseNotificationPriority Priority);

    /**
     * Hides the current notification
     * @param bAnimate Whether to animate the hide
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    void HideNotification(bool bAnimate = true);

    /**
     * Checks if notification is currently visible
     * @return true if notification is visible
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    bool IsNotificationVisible() const;

    //================================================
    // Queue Management
    //================================================
    
    /**
     * Adds notification to queue
     * @param Message Notification message
     * @param Duration Display duration
     * @param Type Notification type
     * @param Priority Notification priority
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    void QueueNotification(const FText& Message, float Duration, ESuspenseNotificationType Type, ESuspenseNotificationPriority Priority);

    /**
     * Clears notification queue
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    void ClearNotificationQueue();

    /**
     * Gets number of queued notifications
     * @return Queue size
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    int32 GetQueuedNotificationCount() const;

    //================================================
    // Notification Settings
    //================================================
    
    /**
     * Sets default notification duration
     * @param Duration Default duration in seconds
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    void SetDefaultDuration(float Duration);

    /**
     * Sets whether to stack similar notifications
     * @param bStack Whether to stack
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    void SetStackNotifications(bool bStack);

    /**
     * Sets maximum visible notifications
     * @param MaxCount Maximum count
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Notification")
    void SetMaxVisibleNotifications(int32 MaxCount);

    //================================================
    // Event System Access
    //================================================

    /**
     * Static helper to get event manager
     * @param WorldContextObject Any object with valid world context
     * @return Event manager or nullptr
     */
    static USuspenseEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Helper to broadcast notification event
     * @param Widget Widget broadcasting the event
     * @param Message Notification message
     * @param Duration Display duration
     */
    static void BroadcastNotification(const UObject* Widget, const FString& Message, float Duration);
};

#endif // SUSPENSECORE_INTERFACES_UI_ISUSPENSECORENOTIFICATIONWIDGET_H