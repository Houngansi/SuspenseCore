// SuspenseCoreInventoryTypes.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.
//
// Implementation of FSuspenseCoreReplicatedItem delta replication callbacks.
// These callbacks invoke delegates bound by InventoryComponent to handle
// the actual replication logic without circular module dependencies.

#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"

//==================================================================
// FSuspenseCoreReplicatedItem - Delta Replication Callbacks
// These invoke delegates on the owning FSuspenseCoreReplicatedInventory
//==================================================================

void FSuspenseCoreReplicatedItem::PreReplicatedRemove(const FSuspenseCoreReplicatedInventory& InArraySerializer)
{
	// Invoke delegate if bound - InventoryComponent handles the actual logic
	if (InArraySerializer.OnPreRemoveDelegate.IsBound())
	{
		InArraySerializer.OnPreRemoveDelegate.Execute(*this, InArraySerializer);
	}
}

void FSuspenseCoreReplicatedItem::PostReplicatedAdd(const FSuspenseCoreReplicatedInventory& InArraySerializer)
{
	// Invoke delegate if bound - InventoryComponent handles the actual logic
	if (InArraySerializer.OnPostAddDelegate.IsBound())
	{
		InArraySerializer.OnPostAddDelegate.Execute(*this, InArraySerializer);
	}
}

void FSuspenseCoreReplicatedItem::PostReplicatedChange(const FSuspenseCoreReplicatedInventory& InArraySerializer)
{
	// Invoke delegate if bound - InventoryComponent handles the actual logic
	if (InArraySerializer.OnPostChangeDelegate.IsBound())
	{
		InArraySerializer.OnPostChangeDelegate.Execute(*this, InArraySerializer);
	}
}
