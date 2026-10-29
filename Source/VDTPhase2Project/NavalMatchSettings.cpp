#include "NavalMatchSettings.h"

bool UNavalMatchSettings::SetBattlefieldSeed(int32 Seed)
{if(Seed<0)return false;BattlefieldSeed=Seed;return true;}

bool UNavalMatchSettings::Configure(int32 Blue,int32 Red,const TArray<EShipHullClass>& Classes,EShipAISkill Skill,ENavalBattleMode BattleMode)
{
    if(Blue<1 || Blue>3 || Red<1 || Red>3 || Classes.Num()!=6 || (Skill!=EShipAISkill::Easy && Skill!=EShipAISkill::Normal && Skill!=EShipAISkill::Hard))return false;
    for(const auto Class:Classes)if(Class!=EShipHullClass::Small && Class!=EShipHullClass::Large && Class!=EShipHullClass::Medium)return false;
    if(BattleMode!=ENavalBattleMode::RealTime && BattleMode!=ENavalBattleMode::TurnBased)return false;
    BlueCount=Blue;RedCount=Red;HullClasses=Classes;Difficulty=Skill;Mode=BattleMode;bExplicitSelection=true;
    return true;
}
