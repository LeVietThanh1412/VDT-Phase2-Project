#include "Navigation/Editor/NavalCookActorPackages.h"
#if WITH_EDITOR
#include "HAL/FileManager.h"
#include "GameFramework/Actor.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace NavalCookActorPackages {
namespace {
FDelegateHandle Loaded;
bool bPreparing = false;
// Package roots alone do not keep exports with a different Outer alive. Keep
// actors too, until the cooker has moved them into the generated persistent cell.
TArray<TStrongObjectPtr<UPackage>> Packages;
TArray<TStrongObjectPtr<AActor>> Actors;
}
void Startup() {
  if (!IsRunningCommandlet()) return;
  Loaded = FCoreUObjectDelegates::OnEndLoadPackage.AddLambda(
      [](const FEndLoadPackageContext& Context) {
        if (!IsInGameThread() || !IsRunningCookCommandlet() || bPreparing || !Packages.IsEmpty()) return;
        for (UPackage* Package : Context.LoadedPackages) {
          if (Package->GetName() != TEXT("/Game/Levels/MainMapLevel")) continue;
          TGuardValue<bool> Guard(bPreparing, true);
          const FString Root = FPackageName::LongPackageNameToFilename(
              TEXT("/Game/__ExternalActors__/Levels/MainMapLevel"));
          TArray<FString> Files;
          IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.uasset"), true, false);
          for (const FString& File : Files) {
            FString Name;
            if (!FPackageName::TryConvertFilenameToLongPackageName(File, Name)) continue;
            UPackage* ActorPackage = LoadPackage(nullptr, *Name, LOAD_None);
            if (!ActorPackage) {
              UE_LOG(LogTemp, Error, TEXT("Unable to load external actor package for cook: %s"), *Name);
              continue;
            }
            Packages.Emplace(ActorPackage);
            TArray<UObject*> Objects;
            GetObjectsWithPackage(ActorPackage, Objects);
            for (UObject* Object : Objects)
              if (auto* Actor = Cast<AActor>(Object)) Actors.Emplace(Actor);
          }
          UE_LOG(LogTemp, Display, TEXT("Retained %d MainMapLevel external actors for World Partition cooking."), Actors.Num());
        }
      });
}
void Shutdown() {
  FCoreUObjectDelegates::OnEndLoadPackage.Remove(Loaded);
  Actors.Empty();
  Packages.Empty();
}
}
#endif
