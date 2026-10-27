#include "NavalMatchSettings.h"

bool UNavalMatchSettings::Configure(int32 Blue,int32 Red,const TArray<EShipHullClass>& Classes,EShipAISkill Skill,ENavalBattleMode BattleMode,int32 InPlayerTeam)
{
    if(Blue<1 || Blue>3 || Red<1 || Red>3 || Classes.Num()!=6 || (Skill!=EShipAISkill::Easy && Skill!=EShipAISkill::Normal && Skill!=EShipAISkill::Hard))return false;
    for(const auto Class:Classes)if(Class!=EShipHullClass::Small && Class!=EShipHullClass::Large)return false;
    if(BattleMode!=ENavalBattleMode::RealTime && BattleMode!=ENavalBattleMode::TurnBased)return false;
    if(InPlayerTeam<0 || InPlayerTeam>1)return false;
    BlueCount=Blue;RedCount=Red;HullClasses=Classes;Difficulty=Skill;Mode=BattleMode;PlayerTeam=InPlayerTeam;bExplicitSelection=true;
    return true;
}
