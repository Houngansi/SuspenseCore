// SuspenseCoreRepNode_AlwaysRelevant.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_AlwaysRelevant.h"

USuspenseCoreRepNode_AlwaysRelevant::USuspenseCoreRepNode_AlwaysRelevant()
{
	bRequiresPrepareForReplicationCall = false;
}

void USuspenseCoreRepNode_AlwaysRelevant::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	ReplicationActorList.ConditionalAdd(ActorInfo.Actor);
}

bool USuspenseCoreRepNode_AlwaysRelevant::NotifyRemoveNetworkActor(
	const FNewReplicatedActorInfo& ActorInfo,
	bool bWarnIfNotFound)
{
	return ReplicationActorList.RemoveFast(ActorInfo.Actor);
}

void USuspenseCoreRepNode_AlwaysRelevant::GatherActorListsForConnection(
	const FConnectionGatherActorListParameters& Params)
{
	// Add all always-relevant actors to the replication list
	Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorList);
}
