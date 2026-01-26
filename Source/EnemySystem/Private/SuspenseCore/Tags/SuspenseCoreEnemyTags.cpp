#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"

namespace SuspenseCoreEnemyTags
{
    namespace State
    {
        UE_DEFINE_GAMEPLAY_TAG(Idle, "State.Enemy.Idle");
        UE_DEFINE_GAMEPLAY_TAG(Patrol, "State.Enemy.Patrol");
        UE_DEFINE_GAMEPLAY_TAG(Alert, "State.Enemy.Alert");
        UE_DEFINE_GAMEPLAY_TAG(Chase, "State.Enemy.Chase");
        UE_DEFINE_GAMEPLAY_TAG(Attack, "State.Enemy.Attack");
        UE_DEFINE_GAMEPLAY_TAG(Cover, "State.Enemy.Cover");
        UE_DEFINE_GAMEPLAY_TAG(Retreat, "State.Enemy.Retreat");
        UE_DEFINE_GAMEPLAY_TAG(Death, "State.Enemy.Death");
    }

    namespace Event
    {
        UE_DEFINE_GAMEPLAY_TAG(PlayerDetected, "Event.Enemy.PlayerDetected");
        UE_DEFINE_GAMEPLAY_TAG(PlayerLost, "Event.Enemy.PlayerLost");
        UE_DEFINE_GAMEPLAY_TAG(DamageTaken, "Event.Enemy.DamageTaken");
        UE_DEFINE_GAMEPLAY_TAG(ReachedDestination, "Event.Enemy.ReachedDestination");
        UE_DEFINE_GAMEPLAY_TAG(TargetInRange, "Event.Enemy.TargetInRange");
        UE_DEFINE_GAMEPLAY_TAG(TargetOutOfRange, "Event.Enemy.TargetOutOfRange");
        UE_DEFINE_GAMEPLAY_TAG(LowHealth, "Event.Enemy.LowHealth");
        UE_DEFINE_GAMEPLAY_TAG(AmmoEmpty, "Event.Enemy.AmmoEmpty");
        UE_DEFINE_GAMEPLAY_TAG(PatrolComplete, "Event.Enemy.PatrolComplete");
        UE_DEFINE_GAMEPLAY_TAG(IdleTimeout, "Event.Enemy.IdleTimeout");
        UE_DEFINE_GAMEPLAY_TAG(Died, "Event.Enemy.Died");
    }

    namespace Type
    {
        UE_DEFINE_GAMEPLAY_TAG(Scav, "Type.Enemy.Scav");
        UE_DEFINE_GAMEPLAY_TAG(PMC, "Type.Enemy.PMC");
        UE_DEFINE_GAMEPLAY_TAG(Boss, "Type.Enemy.Boss");
        UE_DEFINE_GAMEPLAY_TAG(Raider, "Type.Enemy.Raider");
    }
}
