#include "DOREditor.h"

#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"

namespace
{
// Authoring process only. Cook also recompiles the generated Blueprint; the
// shipped game uses ordinary multicast bytecode and its unchanged native flags.
TArray<FMulticastDelegateProperty*> DOR165AddedCallable;
}

IMPLEMENT_MODULE(FDOREditorModule, DOREditor)

void FDOREditorModule::StartupModule()
{
    if(!IsRunningCommandlet()||!FParse::Param(FCommandLine::Get(),TEXT("DOR165AmmoAuthoring")))return;
    UClass* C=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.ClipBasedItemAggregator"));check(C);
    for(const TCHAR* Name:{TEXT("OnMaxAmmoCapacityChanged"),TEXT("OnTotalAmountChanged"),TEXT("OnClipAmountChanged"),TEXT("OnClipAndTotalChanged")})
    {
        auto* P=FindFProperty<FMulticastDelegateProperty>(C,Name);check(P);
        if(!P->HasAnyPropertyFlags(CPF_BlueprintCallable)){P->SetPropertyFlags(CPF_BlueprintCallable);DOR165AddedCallable.Add(P);}
    }
    UE_LOG(LogTemp,Display,TEXT("DOR165 authoring-only delegate exposure members=4 added=%d runtime-module=false"),DOR165AddedCallable.Num());
}

void FDOREditorModule::ShutdownModule()
{
    for(auto* P:DOR165AddedCallable)P->ClearPropertyFlags(CPF_BlueprintCallable);
    if(DOR165AddedCallable.Num())UE_LOG(LogTemp,Display,TEXT("DOR165 authoring-only delegate exposure restored=%d"),DOR165AddedCallable.Num());
    DOR165AddedCallable.Empty();
}

