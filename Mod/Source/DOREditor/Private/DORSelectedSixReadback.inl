// Saved real classes/CDOs only. This commandlet never constructs a world or Pawn.
bool DORVerifySelectedSixReload()
{
    if(!DORVerifyHurricaneCarriersReload())return false;
    auto* Parent=LoadObject<UClass>(nullptr,TEXT("/Game/WeaponsNTools/LockOnRifle/BP_LockOnRifleAutoFireComponent.BP_LockOnRifleAutoFireComponent_C"));
    auto* Lock=LoadObject<UClass>(nullptr,TEXT("/Game/DualOverclockReward/Components/LockOnRifle/BP_DOR_FullLockAuto.BP_DOR_FullLockAuto_C"));
    auto* Powder=LoadObject<UClass>(nullptr,TEXT("/Game/DualOverclockReward/Components/SawedOffShotgun/BP_DOR_SpecialPowderDouble.BP_DOR_SpecialPowderDouble_C"));
    if(!Parent || !Lock || !Powder || Lock->GetSuperClass()!=Parent || Powder->GetSuperClass()!=UActorComponent::StaticClass())return false;
    auto* Delay=FindFProperty<FFloatProperty>(Parent,TEXT("FireRate"));if(!Delay || Delay->GetPropertyValue_InContainer(Lock->GetDefaultObject())!=Delay->GetPropertyValue_InContainer(Parent->GetDefaultObject()) || Delay->GetPropertyValue_InContainer(Lock->GetDefaultObject())!=.15f)return false;
    if(Lock->FindFunctionByName(TEXT("ReceiveBeginPlay"))!=Parent->FindFunctionByName(TEXT("ReceiveBeginPlay")) || Lock->FindFunctionByName(TEXT("OnEqipped_Event_0"))!=Parent->FindFunctionByName(TEXT("OnEqipped_Event_0")))return false;
    for(auto* Class:{Lock,Powder}){auto* CDO=Cast<UActorComponent>(Class->GetDefaultObject());if(!CDO || CDO->GetIsReplicated() || CDO->PrimaryComponentTick.bCanEverTick || CDO->PrimaryComponentTick.bStartWithTickEnabled)return false;}
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(int32 Code:{302,304,309}){
        const FString Name=FString::Printf(TEXT("DOR%d_FullLockAuto"),Code),Path=FString(RuntimeRoot)/TEXT("AdaptedLeaves/LockOnRifle")/Name+TEXT(".")+Name;
        auto* Leaf=LoadObject<UItemUpgrade>(nullptr,*Path);auto* Class=Leaf?FindFProperty<FClassProperty>(Leaf->GetClass(),TEXT("ComponentClass")):nullptr;auto* ServerOnly=Leaf?FindFProperty<FBoolProperty>(Leaf->GetClass(),TEXT("ServerOnly")):nullptr;
        if(!Class || !ServerOnly || Class->GetObjectPropertyValue_InContainer(Leaf)!=Lock || ServerOnly->GetPropertyValue_InContainer(Leaf))return false;
    }
    auto* Moving=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.Projectile"));auto* Base=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.ProjectileBase"));auto* Move=Moving?FindFProperty<FObjectPropertyBase>(Moving,TEXT("MovementComponent")):nullptr;
    for(const TCHAR* Name:{TEXT("PRJ_MicroMissile_Cluster"),TEXT("BP_ClusterFragment_MicroMissileLauncher")}){
        const FString Path=FString(TEXT("/Game/WeaponsNTools/MicroMissileLauncher/"))+Name+TEXT(".")+Name+TEXT("_C");auto* C=LoadObject<UClass>(nullptr,*Path);auto* CDO=C?C->GetDefaultObject():nullptr;auto* M=Move&&CDO?Move->GetObjectPropertyValue_InContainer(CDO):nullptr;if(!M || !Base)return false;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("originalClass"),Path);
        for(const TCHAR* Field:{TEXT("InitialSpeed"),TEXT("MaxSpeed")}){auto* P=FindFProperty<FFloatProperty>(M->GetClass(),Field);if(!P)return false;Row->SetNumberField(Field,P->GetPropertyValue_InContainer(M));}
        auto* Velocity=FindFProperty<FFloatProperty>(Base,TEXT("VelocityMultiplier"));if(!Velocity)return false;Row->SetNumberField(TEXT("VelocityMultiplier"),Velocity->GetPropertyValue_InContainer(CDO));Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("schema"),TEXT("dor.selected-six.fresh-reload.v1"));Receipt->SetStringField(TEXT("build"),DORResearchBuildTag);Receipt->SetBoolField(TEXT("lockParentBeginEquipInherited"),true);Receipt->SetNumberField(TEXT("inheritedDefaultDelay"),.15);Receipt->SetBoolField(TEXT("helperTickAndReplicationDisabled"),true);Receipt->SetArrayField(TEXT("nativeOriginalFlightDefaults"),Rows);Receipt->SetBoolField(TEXT("gameplayProof"),false);
    FString Json;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));return FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("DORSelectedSixReadback.json")));
}
