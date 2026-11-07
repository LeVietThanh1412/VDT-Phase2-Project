#include "NavalBattlefieldSubsystem.h"
#include "NavalSectorSubsystem.h"
#include "NavalMatchSettings.h"
#include "NavalBattleUI.h"
#include "ShipCombatComponent.h"
#include "ShipEnemyNavigationComponent.h"
#include "ShipMovementComponent.h"
#include "SeaNavigationMap.h"
#include "CesiumGlobeAnchorComponent.h"
#include "WaterBodyComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Private/NavalWaterQueries.h"

bool UNavalBattlefieldSubsystem::FindMissionGoal(const FNavalSpawnAssignment& A,FVector2D Desired,bool Defender,FVector2D& Out,FVector2D Threat,float MinimumThreatDistance) const
{
    const auto Map=NavigationChartForShip(A.Ship);const auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    if(!Map.IsValid() || !IsValid(A.Ship))return false;
    const auto Position=Grid->WorldToChart(A.Ship->GetActorLocation());
    const int32 Start=Map->CellAt(Position);
    if(!Map->IsWater(Position) || !Map->ConnectedRegions.IsValidIndex(Start))return false;
    TArray<TPair<double,FVector2D>> Candidates;
    for(int32 I=0;I<Map->Cells.Num();++I)
    {
        if(!Map->Cells[I] || Map->ConnectedRegions[I]!=Map->ConnectedRegions[Start])continue;
        const auto P=Map->Center(I);
        if(!Grid->IsInsideField(P,A.HullRadiusM+30) || FVector2D::Distance(Position,P)<160)continue;
        if(Defender && (P.X>14700 || FVector2D::Distance(P,A.DefensiveAnchorEN)>DefensiveLeashM))continue;
        if(MinimumThreatDistance>0 && FVector2D::Distance(P,Threat)<MinimumThreatDistance)continue;
        bool Occupied=false;
        for(const auto& Other:Assignments)
        {
            if(Other.Ship==A.Ship || !IsValid(Other.Ship))continue;
            const auto* W=Other.Ship->FindComponentByClass<UShipWeaponComponent>();if(!W || W->bIsDestroyed)continue;
            if(FVector2D::Distance(P,Grid->WorldToChart(Other.Ship->GetActorLocation()))<A.HullRadiusM+Other.HullRadiusM+45){Occupied=true;break;}
        }
        if(!Occupied)Candidates.Emplace(FVector2D::Distance(P,Desired)+FVector2D::Distance(Position,P)*.15,P);
    }
    Candidates.Sort([](const auto& L,const auto& R){return L.Key<R.Key;});
    for(int32 I=0;I<FMath::Min(12,Candidates.Num());++I)
    {
        const auto Route=Map->FindPath(Position,Candidates[I].Value,75000,20);if(!Route.bSuccess || Route.Points.Num()<2)continue;
        bool Safe=true;
        for(const auto& P:Route.Points)
            if(!Grid->IsInsideField(P) || (Defender && (P.X>14750 || FVector2D::Distance(P,A.DefensiveAnchorEN)>DefensiveLeashM+150))){Safe=false;break;}
        if(Safe){Out=Candidates[I].Value;return true;}
    }
    return false;
}
void UNavalBattlefieldSubsystem::UpdateDefenders()
{
    const double Now=GetWorld()->GetTimeSeconds();if(Now-LastMissionUpdateAt<.5)return;LastMissionUpdateAt=Now;
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    for(auto& A:Assignments)
    {
        if(A.Team!=0 || !IsValid(A.Ship) || A.Ship->IsPlayerControlled())continue;
        auto* W=A.Ship->FindComponentByClass<UShipWeaponComponent>();auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();
        if(!W || !C || !N || W->bIsDestroyed || W->Team()!=0){A.MissionState=TEXT("Defender unavailable");continue;}
        if(!UNavalBattleSubsystem::CanShipAct(A.Ship))continue;
        const auto State=C->TacticalState;
        if(State==EShipTacticalState::Retreat || State==EShipTacticalState::Escape || State==EShipTacticalState::BackwardRetreat || State==EShipTacticalState::ReturnToPatrol)
        {A.MissionState=TEXT("Shared retreat / returning to defense");continue;}
        if(A.bDefensiveIntercept && !N->HasTacticalDestination())A.bDefensiveIntercept=false;
        if(A.bDefensiveIntercept && N->NavigationState==EShipNavigationState::Failed)
        {
            N->ReturnToPatrol();A.bDefensiveIntercept=false;++A.DefensiveReturns;A.MissionRetryAt=Now+30;
            A.MissionState=TEXT("Interception blocked: returning to defense");continue;
        }
        const auto Position=Grid->WorldToChart(A.Ship->GetActorLocation());
        const bool TargetValid=C->bEnableCombat && C->IsHostile(C->Target);
        const auto Enemy=TargetValid?Grid->WorldToChart(C->Target->GetActorLocation()):FVector2D::ZeroVector;
        const bool Intercept=Now>=A.MissionRetryAt && TargetValid && Enemy.X<=14750 && FVector2D::Distance(Enemy,A.DefensiveAnchorEN)<=DefensiveLeashM+250 &&
            FVector2D::Distance(Position,A.DefensiveAnchorEN)<=DefensiveLeashM+100 && W->CurrentHealth>W->MaxHealth*.4;
        if(!Intercept)
        {
            if(A.bDefensiveIntercept)
            {N->ReturnToPatrol();A.bDefensiveIntercept=false;++A.DefensiveReturns;A.LastMissionPlanAt=Now;}
            A.MissionState=N->PatrolCircuitActors.IsEmpty()?TEXT("Eastern island defensive patrol"):TEXT("Closed island defense circuit");continue;
        }
        if(Now-A.LastMissionPlanAt<5)continue;
        A.LastMissionPlanAt=Now;
        // Seek a safe firing position on the defensive side of the threat.
        // The actual gun still uses the shared faction/LOS/ballistic pipeline.
        FVector2D Goal;const auto TowardDefense=(A.DefensiveAnchorEN-Enemy).GetSafeNormal();
        const float StandOff=FMath::Max(C->MinimumRetreatDistanceM+150,C->EngagementRadiusM*.6f);
        if(FindMissionGoal(A,Enemy+TowardDefense*StandOff,true,Goal,Enemy,C->MinimumRetreatDistanceM+80))
        {
            const bool SameGoal=A.bDefensiveIntercept && N->HasTacticalDestination() &&
                FVector2D::Distance(Grid->WorldToChart(N->GeographicToWorld(N->CurrentFinalDestination)),Goal)<100;
            if(SameGoal){A.MissionState=TEXT("Following bounded interception route");continue;}
            if(N->BeginTacticalMoveTo(N->WorldToGeographic(Grid->ChartToWorld(Goal))))
            {
                if(!A.bDefensiveIntercept)++A.InterceptionsStarted;
                A.bDefensiveIntercept=true;A.MissionState=TEXT("Bounded defensive interception");
            }
        }
        else if(!A.bDefensiveIntercept)A.MissionState=TEXT("Patrolling: no safe interception route");
    }
}
bool UNavalBattlefieldSubsystem::SetAssaultGoal(FNavalSpawnAssignment& A,FVector2D Goal)
{
    auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* Ocean=NavalWaterQueries::FindOcean(GetWorld());
    if(!N || !Ocean || !IsValid(N->PatrolActorB))return false;
    const auto Map=NavigationChartForShip(A.Ship);const auto P=Grid->WorldToChart(A.Ship->GetActorLocation());
    if(!Map->FindPath(P,Goal,75000,20).bSuccess)return false;
    FVector World=Grid->ChartToWorld(Goal),Water,Normal,Velocity;float Depth=0;
    Ocean->GetWaterSurfaceInfoAtLocation(World,Water,Normal,Velocity,Depth,false);World.Z=Water.Z;
    N->PatrolActorB->GetRootComponent()->SetMobility(EComponentMobility::Movable);N->PatrolActorB->SetActorLocation(World);
    if(auto* Anchor=N->PatrolActorB->FindComponentByClass<UCesiumGlobeAnchorComponent>())Anchor->Sync();
    A.MissionGoalEN=Goal;A.bHoldingForLeader=false;A.FormationWaitSeconds=0;A.MissionRouteRetries=0;
    N->bStartAtPatrolB=true;N->StartPatrol();return true;
}
bool UNavalBattlefieldSubsystem::AdvanceAssault(FNavalSpawnAssignment& A)
{
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();
    const int32 Next=FMath::Min(3,A.AssaultPhase+1);
    auto Zone=[&](FName Name)
    {const auto* Index=Grid->ZoneRepresentatives().Find(Name);return Index?Grid->Sectors[*Index].AnchorEN:Grid->CenterEN;};
    const auto Crossing=Zone(TEXT("CENTER_CROSSING")),Core=Zone(TEXT("BLUE_DEFENSE_CORE"));
    FVector2D Desired=Next==1?Crossing:Core;
    const auto Back=(A.SpawnEN-Desired).GetSafeNormal();const FVector2D Side(-Back.Y,Back.X);
    if(Next<3)Desired+=Back*FormationSpacingM*A.Slot+Side*(A.Slot==0?0:A.Slot%2?40:-40);
    FVector2D Threat=FVector2D::ZeroVector;float MinThreat=0;
    if(Next>=3)
    {
        FVector2D Contact=GetWorld()->GetTimeSeconds()-ScoutContactAt<15?ScoutContactEN:Core;
        if(C && C->IsHostile(C->Target)){Contact=Grid->WorldToChart(C->Target->GetActorLocation());Threat=Contact;MinThreat=C->MinimumRetreatDistanceM+80;}
        const float Range=C?FMath::Max(C->MinimumRetreatDistanceM+150,C->EngagementRadiusM*.6f):700;
        const double Angle=FMath::DegreesToRadians(double((ActualSeed%360)+A.Slot*65+A.AssaultGoalsReached*70));
        Desired=Contact+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Range;
    }
    FVector2D Goal;
    if(!FindMissionGoal(A,Desired,false,Goal,Threat,MinThreat) || !SetAssaultGoal(A,Goal))return false;
    A.AssaultPhase=Next;++A.AssaultGoalsReached;
    A.MissionState=Next==1?TEXT("Assault: central crossing"):Next==2?TEXT("Assault: Blue contact area"):TEXT("Attack: safe flank and reposition");return true;
}
void UNavalBattlefieldSubsystem::UpdateAssault()
{
    const auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    if(!Settings || !Settings->bEnableRedAssault)return;
    const double Now=GetWorld()->GetTimeSeconds();if(Now-LastAssaultUpdateAt<.5)return;
    const float Dt=FMath::Min(1.,Now-LastAssaultUpdateAt);LastAssaultUpdateAt=Now;
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    bool AnyActive=false;for(const auto& A:Assignments)if(A.Team==1 && IsValid(A.Ship) && UNavalBattleSubsystem::CanShipAct(A.Ship)){AnyActive=true;break;}
    if(!AnyActive)return;
    // Share genuine LOS sightings, including visible ships that were not the
    // gun's nearest selected target. Never sample a hidden enemy's current pose.
    for(const auto& Red:Assignments)if(Red.Team==1 && IsValid(Red.Ship))
    {
        const auto* RW=Red.Ship->FindComponentByClass<UShipWeaponComponent>();const auto* C=Red.Ship->FindComponentByClass<UShipCombatComponent>();
        if(!RW || RW->bIsDestroyed || !C || !C->bEnableCombat)continue;
        for(const auto& Blue:Assignments)if(Blue.Team==0 && IsValid(Blue.Ship) && C->CanObserveHostile(Blue.Ship))
        {
            auto& Contact=ObservedContacts.FindOrAdd(Blue.Ship);Contact.SeenAt=Now;Contact.Position=Grid->WorldToChart(Blue.Ship->GetActorLocation());
            auto* H=UShipCombatComponent::PhysicalHull(Blue.Ship);
            Contact.Velocity=H?Grid->WorldToChart(Blue.Ship->GetActorLocation()+H->GetPhysicsLinearVelocity())-Contact.Position:FVector2D::ZeroVector;
            Contact.HeadingDegrees=H?H->GetComponentRotation().Yaw:0;
        }
    }
    const FObservedContact* LastLiveContact=nullptr;
    for(auto I=ObservedContacts.CreateIterator();I;++I)
    {
        const auto* Ship=I.Key().Get();const auto* W=Ship?Ship->FindComponentByClass<UShipWeaponComponent>():nullptr;
        if(!W || W->bIsDestroyed || Now-I.Value().SeenAt>FMath::Max(ContactMemoryLifetimeSeconds,LostContactSearchDelaySeconds+30)){I.RemoveCurrent();continue;}
        if(!LastLiveContact || I.Value().SeenAt>LastLiveContact->SeenAt)LastLiveContact=&I.Value();
    }
    if(!bAssaultInitialized)
    {
        for(auto& A:Assignments)if(A.Team==1 && IsValid(A.Ship))
        {
            if(auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>())A.BaseCruiseSpeedMps=N->CruiseSpeedMps;
            if(auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>())C->SetCombatRole(EShipEnemyRole::Fighter);
            A.MissionGoalEN=A.TargetEN;A.MissionState=TEXT("Convoy staging");
            if(A.Slot==0)AssaultLeader=A.Ship;
        }
        bAssaultInitialized=true;
    }
    auto Usable=[&](const FNavalSpawnAssignment& A)
    {
        if(A.Team!=1 || !IsValid(A.Ship))return false;
        const auto* W=A.Ship->FindComponentByClass<UShipWeaponComponent>();const auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();const auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();
        return W && !W->bIsDestroyed && W->Team()==1 && C && N && A.MissionRouteRetries<3 && N->NavigationState!=EShipNavigationState::Failed && N->RecoveryAttemptCount<2 &&
            C->TacticalState!=EShipTacticalState::Retreat && C->TacticalState!=EShipTacticalState::Escape && C->TacticalState!=EShipTacticalState::BackwardRetreat;
    };
    auto* Leader=Assignments.FindByPredicate([&](const FNavalSpawnAssignment& A){return A.Ship==AssaultLeader.Get() && Usable(A);});
    if(!Leader)
    {
        FNavalSpawnAssignment* Best=nullptr;
        for(auto& A:Assignments)if(Usable(A) && (!Best || A.AssaultPhase>Best->AssaultPhase || (A.AssaultPhase==Best->AssaultPhase && A.Slot<Best->Slot)))Best=&A;
        if(Best)
        {if(AssaultLeader.Get()!=Best->Ship){AssaultLeader=Best->Ship;++AssaultLeaderChanges;}Leader=Best;}
        else AssaultLeader.Reset();
    }
    if(Leader && UNavalBattleSubsystem::CanShipAct(Leader->Ship))
        if(const auto* C=Leader->Ship->FindComponentByClass<UShipCombatComponent>();C && C->IsHostile(C->Target) && C->bTargetVisible)
        {ScoutContactEN=Grid->WorldToChart(C->Target->GetActorLocation());ScoutContactAt=Now;}
    for(auto& A:Assignments)
    {
        if(A.Team!=1 || !IsValid(A.Ship))continue;
        auto* W=A.Ship->FindComponentByClass<UShipWeaponComponent>();auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();
        if(!W || W->bIsDestroyed || W->Team()!=1 || !C || !N){A.MissionState=TEXT("Assault ship unavailable");continue;}
        if(!UNavalBattleSubsystem::CanShipAct(A.Ship))continue;
        const bool IsLeader=Leader && Leader->Ship==A.Ship;
        const auto Position=Grid->WorldToChart(A.Ship->GetActorLocation());
        const bool Retreat=C->TacticalState==EShipTacticalState::Retreat || C->TacticalState==EShipTacticalState::Escape || C->TacticalState==EShipTacticalState::BackwardRetreat || C->TacticalState==EShipTacticalState::ReturnToPatrol;
        if(Retreat){A.MissionState=TEXT("Shared retreat; assault objective retained");continue;}
        // Exhausting a local retry window must not retire a surviving escort.
        // Reconsider a fresh, validated objective after the existing search
        // delay; keep each window bounded instead of searching every Tick.
        if(A.MissionRouteRetries>=3 && !A.bSearchingCircuit && !N->HasTacticalDestination() &&
            (N->NavigationState==EShipNavigationState::Failed ||
            N->NavigationState==EShipNavigationState::Idle || N->NavigationState==EShipNavigationState::Arrived))
        {
            A.MissionState=TEXT("Assault route cooldown");
            if(Now<A.MissionRetryAt+FMath::Max(15.f,LostContactSearchDelaySeconds))continue;
            A.MissionRouteRetries=0;A.bHoldingForLeader=false;
            if(!AdvanceAssault(A))
            {
                A.MissionRouteRetries=3;A.MissionRetryAt=Now;N->StopNavigation();
                A.MissionState=TEXT("No safe assault alternative; waiting for next retry window");
            }
            continue;
        }
        const auto* Movement=A.Ship->FindComponentByClass<UShipMovementComponent>();
        const auto* Circuit=Assignments.FindByPredicate([](const FNavalSpawnAssignment& B){return B.Team==0 && B.PatrolCircuitEN.Num()>=3;});
        if(C->bEnableCombat && Movement && Movement->HullClass==EShipHullClass::Small && Circuit && A.AssaultPhase>=2 &&
            (A.bSearchingCircuit || (LastLiveContact && Now-LastLiveContact->SeenAt>=LostContactSearchDelaySeconds)))
        {
            const auto& Points=Circuit->PatrolCircuitEN;
            if(!A.bSearchingCircuit)
            {
                const auto Predicted=LastLiveContact->Position+(LastLiveContact->Velocity*FMath::Min(45.,Now-LastLiveContact->SeenAt)).GetClampedToMaxSize(600.);
                double Best=DBL_MAX;
                for(int32 I=0;I<Points.Num();++I)
                    if(const double D=FVector2D::Distance(Points[I],Predicted);D<Best){Best=D;A.SearchCircuitIndex=I;}
                A.bSearchingCircuit=true;A.PendingSearchIndex=A.SearchCircuitIndex;A.MissionRouteRetries=0;
            }
            if(A.bSearchingCircuit)
            {
                // The Small scout independently searches the known corridor;
                // heavy escorts cannot be ordered into its disconnected passage.
                N->CruiseSpeedMps=FMath::Max(1.f,A.BaseCruiseSpeedMps);
                if(A.PendingSearchIndex!=INDEX_NONE)
                {
                    if(A.MissionRouteRetries>=3)
                    {N->StopNavigation();A.MissionState=TEXT("Scout search unavailable after bounded retries");continue;}
                    if(Now>=A.MissionRetryAt)
                    {
                        const int32 Retries=A.MissionRouteRetries;
                        if(SetAssaultGoal(A,Points[A.PendingSearchIndex]))
                        {A.SearchCircuitIndex=A.PendingSearchIndex;A.PendingSearchIndex=INDEX_NONE;A.MissionRouteRetries=Retries;}
                        else{A.MissionRetryAt=Now+15;++A.MissionRouteRetries;N->StopNavigation();}
                    }
                    A.MissionState=TEXT("Scout: planning last-known island search");continue;
                }
                auto* H=UShipCombatComponent::PhysicalHull(A.Ship);
                const bool At=H && FVector2D::Distance(Position,A.MissionGoalEN)<=N->ArrivalRadiusM &&
                    FVector::VectorPlaneProject(H->GetPhysicsLinearVelocity(),H->GetUpVector()).Size()<150;
                if(At)
                {
                    ++A.SearchGoalsReached;A.PendingSearchIndex=(A.SearchCircuitIndex+Points.Num()-1)%Points.Num();A.MissionRouteRetries=0;
                    if(!LastLiveContact && A.SearchGoalsReached%Points.Num()==0)
                    {A.bSearchingCircuit=false;A.PendingSearchIndex=INDEX_NONE;A.MissionRetryAt=Now+LostContactSearchDelaySeconds;AdvanceAssault(A);A.MissionState=TEXT("Scout search exhausted; returning to accessible assault area");}
                }
                else if(N->NavigationState==EShipNavigationState::Failed)
                {
                    ++A.MissionRouteRetries;A.PendingSearchIndex=A.SearchCircuitIndex;A.MissionRetryAt=Now+15;N->StopNavigation();
                }
                A.MissionState=A.MissionRouteRetries>=3?TEXT("Scout search unavailable after bounded retries"):
                    C->Target && C->bTargetVisible?TEXT("Scout: contact on island search route"):TEXT("Scout: searching last-known island corridor");
                continue;
            }
        }
        if(!A.bAssaultReleased)
        {
            A.FormationWaitSeconds+=Dt;
            const FNavalSpawnAssignment* Predecessor=Leader;
            for(const auto& Other:Assignments)
                if(Other.Slot<A.Slot && Usable(Other) && (!Predecessor || Other.Slot>Predecessor->Slot))Predecessor=&Other;
            const bool Timed=Predecessor && Predecessor->bAssaultReleased && Now-Predecessor->AssaultReleasedAt>=FormationDepartureIntervalSeconds;
            const bool Moving=Predecessor && FVector2D::Distance(Grid->WorldToChart(Predecessor->Ship->GetActorLocation()),Predecessor->SpawnEN)>=80;
            // A stalled predecessor cannot deadlock the convoy; timing still applies to fallback release.
            if(!IsLeader && Predecessor && (!Timed || (!Moving && A.FormationWaitSeconds<20)))
            {A.MissionState=TEXT("Convoy: awaiting sequential safe departure");continue;}
            A.bAssaultReleased=true;A.AssaultReleasedAt=Now;A.FormationWaitSeconds=0;N->bStartAtPatrolB=true;N->StartPatrol();
        }
        N->CruiseSpeedMps=FMath::Max(1.f,A.BaseCruiseSpeedMps);
        A.FormationDistanceM=Leader?FVector2D::Distance(Position,Grid->WorldToChart(Leader->Ship->GetActorLocation())):0;
        if(Leader && !IsLeader)
        {
            const auto LeadPosition=Grid->WorldToChart(Leader->Ship->GetActorLocation());
            const auto Direction=(Leader->MissionGoalEN-LeadPosition).GetSafeNormal();
            const bool Ahead=FVector2D::DotProduct(Position-LeadPosition,Direction)>-FormationSpacingM*.4f;
            if(A.FormationDistanceM<FormationSpacingM*.85f || (Ahead && A.FormationDistanceM<FormationSpacingM*2.5f))
                N->CruiseSpeedMps=1.5f;
            else if(A.FormationDistanceM<FormationSpacingM*1.4f)N->CruiseSpeedMps=FMath::Min(N->CruiseSpeedMps,FMath::Max(2.f,Leader->BaseCruiseSpeedMps*.65f));
        }
        else if(IsLeader)
        {
            for(const auto& Follower:Assignments)if(Follower.Team==1 && Follower.Ship!=A.Ship && Usable(Follower) &&
                FVector2D::Distance(Position,Grid->WorldToChart(Follower.Ship->GetActorLocation()))>FormationSpacingM*2.5f)
                N->CruiseSpeedMps=FMath::Min(N->CruiseSpeedMps,Follower.BaseCruiseSpeedMps*.8f);
        }
        auto* Hull=UShipCombatComponent::PhysicalHull(A.Ship);
        const bool AtGoal=FVector2D::Distance(Position,A.MissionGoalEN)<=N->ArrivalRadiusM && Hull && Hull->GetPhysicsLinearVelocity().Size()<150;
        if(AtGoal)
        {
            if(A.MissionRouteRetries>=3){A.MissionState=TEXT("Assault route unavailable after bounded retries");continue;}
            A.FormationWaitSeconds+=Dt;
            if(!IsLeader && Leader && A.AssaultPhase>=Leader->AssaultPhase && A.FormationWaitSeconds<20)
            {
                if(!A.bHoldingForLeader){N->StopNavigation();A.bHoldingForLeader=true;}
                A.MissionState=TEXT("Convoy: reserving phase spacing");continue;
            }
            if(Now<A.MissionRetryAt)continue;
            if(AdvanceAssault(A))continue;
            N->StopNavigation();A.bHoldingForLeader=true;A.MissionRetryAt=Now+15;++A.MissionRouteRetries;
            A.MissionState=TEXT("No safe next assault route; bounded retry");
            if(A.MissionRouteRetries>=3)A.MissionState=TEXT("Assault route unavailable after bounded retries");
        }
        else if(N->NavigationState==EShipNavigationState::Failed && Now>=A.MissionRetryAt && A.MissionRouteRetries<3)
        {
            ++A.MissionRouteRetries;A.MissionRetryAt=Now+15;
            const int32 Retries=A.MissionRouteRetries;
            FVector2D Goal;if(FindMissionGoal(A,A.MissionGoalEN,false,Goal))SetAssaultGoal(A,Goal);
            A.MissionRouteRetries=Retries;
            A.MissionState=TEXT("Recovering independent assault route");
        }
        else if(A.bHoldingForLeader && Leader && (IsLeader || A.AssaultPhase<Leader->AssaultPhase || A.FormationWaitSeconds>=20))
        {A.bHoldingForLeader=false;N->bStartAtPatrolB=true;N->StartPatrol();}
        else if(!AtGoal)A.MissionState=IsLeader?TEXT("Scout leading assault"):TEXT("Following independent convoy route");
    }
}
