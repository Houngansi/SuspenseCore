// ISuspenseAttachmentProvider.h
// Equipment Attachment Provider Interface
// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_INTERFACES_EQUIPMENT_ISUSPENSECOREATTACHMENTPROVIDER_H
#define SUSPENSECORE_INTERFACES_EQUIPMENT_ISUSPENSECOREATTACHMENTPROVIDER_H

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "ISuspenseAttachmentProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseAttachmentProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseAttachmentProvider
 *
 * Interface for managing equipment visual attachment to characters.
 * Handles socket attachment, transforms, and visibility.
 */
class BRIDGESYSTEM_API ISuspenseAttachmentProvider
{
    GENERATED_BODY()

public:
    /** Attach equipment to socket */
    virtual bool AttachToSocket(int32 SlotIndex, const FName& SocketName, const FTransform& Offset = FTransform::Identity) = 0;

    /** Detach equipment from current socket */
    virtual bool Detach(int32 SlotIndex) = 0;

    /** Get current attachment state */
    virtual FEquipmentAttachmentState GetAttachmentState(int32 SlotIndex) const = 0;

    /** Get all attachment states */
    virtual TArray<FEquipmentAttachmentState> GetAllAttachmentStates() const = 0;

    /** Set attachment visibility */
    virtual void SetAttachmentVisibility(int32 SlotIndex, bool bVisible) = 0;

    /** Update attachment transform */
    virtual void UpdateAttachmentTransform(int32 SlotIndex, const FTransform& NewTransform) = 0;

    /** Check if slot is currently attached */
    virtual bool IsAttached(int32 SlotIndex) const = 0;

    /** Get socket name for slot */
    virtual FName GetSocketForSlot(int32 SlotIndex) const = 0;

    /** Set socket for slot */
    virtual void SetSocketForSlot(int32 SlotIndex, const FName& SocketName) = 0;
};


#endif // SUSPENSECORE_INTERFACES_EQUIPMENT_ISUSPENSECOREATTACHMENTPROVIDER_H