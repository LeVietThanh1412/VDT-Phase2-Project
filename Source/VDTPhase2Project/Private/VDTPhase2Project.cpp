// Copyright Epic Games, Inc. All Rights Reserved.

#include "VDTPhase2Project.h"
#include "Modules/ModuleManager.h"
#if WITH_EDITOR
#include "Navigation/Editor/NavalTerrainEditorPreview.h"
#include "Navigation/Editor/NavalCookActorPackages.h"
#endif

class FVDTPhase2ProjectModule : public FDefaultGameModuleImpl {
public:
  void StartupModule() override {
    FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
    NavalTerrainEditorPreview::Startup();
    NavalCookActorPackages::Startup();
#endif
  }
  void ShutdownModule() override {
#if WITH_EDITOR
    NavalTerrainEditorPreview::Shutdown();
    NavalCookActorPackages::Shutdown();
#endif
    FDefaultGameModuleImpl::ShutdownModule();
  }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FVDTPhase2ProjectModule, VDTPhase2Project,
                              "VDTPhase2Project");
