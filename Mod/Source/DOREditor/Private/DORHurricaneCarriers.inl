// Charged-only original carriers inherit all contact logic; only Buck flight
// defaults differ. The native launcher still initializes and upgrades each actor.
namespace
{
TMap<FName,FString> DORHurricaneDefaults(UObject* Object)
{
    TMap<FName,FString> Result;
    for (TFieldIterator<FProperty> P(Object->GetClass()); P; ++P)
    {
        FString Value;
        P->ExportText_InContainer(0,Value,Object,Object,Object,PPF_None);
        Result.Add(P->GetFName(),Value);
    }
    return Result;
}

bool CreateDORHurricaneCarriers(const TArray<TSharedPtr<FJsonObject>>& Pairs)
{
    const FString NativeRoot=TEXT("/Game/WeaponsNTools/MicroMissileLauncher/");
    auto* Moving=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.Projectile"));
    auto* Movement=Moving?FindFProperty<FObjectPropertyBase>(Moving,TEXT("MovementComponent")):nullptr;
    auto* Buck=LoadObject<UClass>(nullptr,*(NativeRoot+TEXT("PRJ_MicroMissile_Buck.PRJ_MicroMissile_Buck_C")));
    auto* BuckCDO=Buck?Buck->GetDefaultObject():nullptr;
    auto* BuckMove=Movement&&BuckCDO?Movement->GetObjectPropertyValue_InContainer(BuckCDO):nullptr;
    if (!BuckMove || BuckMove->GetClass()->GetPathName()!=TEXT("/Script/FSD.FSDProjectileMovementComponent")) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=Buck-movement class=%s movement=%s"),*GetPathNameSafe(Buck),*GetPathNameSafe(BuckMove)); return false; }
    const TSet<FName> Allowed={TEXT("InitialSpeed"),TEXT("MaxSpeed"),TEXT("AccelerationCurve"),TEXT("bIsHomingProjectile"),TEXT("HomingAccelerationMagnitude")};
    const auto LoadedBuckDefaults=DORHurricaneDefaults(BuckMove);
    for (const FName Name:Allowed) UE_LOG(LogDORGenerateAssets,Display,TEXT("Hurricane Buck default %s=%s"),*Name.ToString(),*LoadedBuckDefaults[Name]);
    auto* Initial=FindFProperty<FFloatProperty>(BuckMove->GetClass(),TEXT("InitialSpeed"));
    auto* Maximum=FindFProperty<FFloatProperty>(BuckMove->GetClass(),TEXT("MaxSpeed"));
    auto* Homing=FindFProperty<FBoolProperty>(BuckMove->GetClass(),TEXT("bIsHomingProjectile"));
    auto* Strength=FindFProperty<FFloatProperty>(BuckMove->GetClass(),TEXT("HomingAccelerationMagnitude"));
    auto* Curve=FindFProperty<FObjectPropertyBase>(BuckMove->GetClass(),TEXT("AccelerationCurve"));
    if (!Initial || !Maximum || !Homing || !Strength || !Curve || Initial->GetPropertyValue_InContainer(BuckMove)!=2000.f ||
        Maximum->GetPropertyValue_InContainer(BuckMove)!=2000.f || Homing->GetPropertyValue_InContainer(BuckMove) ||
        Strength->GetPropertyValue_InContainer(BuckMove)!=0.f || GetPathNameSafe(Curve->GetObjectPropertyValue_InContainer(BuckMove))!=NativeRoot+TEXT("CRV_MML_InstantAcceleration.CRV_MML_InstantAcceleration")) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=Buck-five-source-defaults")); return false; }
    const auto BuckBefore=DORHurricaneDefaults(BuckCDO), BuckMoveBefore=DORHurricaneDefaults(BuckMove);
    for (const auto& Pair:Pairs)
    {
        const TSharedPtr<FJsonObject>* Spec=nullptr;
        if (!Pair->TryGetObjectField(TEXT("hurricaneChargedCarrier"),Spec)) continue;
        const int32 Code=Pair->GetIntegerField(TEXT("testIndex"));
        if ((Code!=338 && Code!=339 && Code!=343) || Pair->GetStringField(TEXT("pairId"))!=(Code==338?TEXT("5feabfc9c453cabe"):Code==339?TEXT("d3741858aa76e2bc"):TEXT("72eab4f1424c13af"))) return false;
        const FString ParentName=Code==338?TEXT("PRJ_MicroMissile_Mine_Head"):Code==339?TEXT("PRJ_MicroMissile_PlasmaBurster"):TEXT("PRJ_MicroMissile_Cluster");
        const FString Name=Code==338?TEXT("PRJ_DOR338_ChargedMine"):Code==339?TEXT("PRJ_DOR339_ChargedPlasma"):TEXT("PRJ_DOR343_ChargedCluster");
        const FString PackageName=FString(RuntimeRoot)/TEXT("AdaptedProjectiles/MicroMissileLauncher")/Name;
        const FString ParentPath=NativeRoot+ParentName+TEXT(".")+ParentName+TEXT("_C");
        if ((*Spec)->GetStringField(TEXT("objectPath"))!=PackageName+TEXT(".")+Name+TEXT("_C") ||
            (*Spec)->GetStringField(TEXT("parentClassPath"))!=ParentPath || (*Spec)->GetStringField(TEXT("sourceOfOverrides"))!=Buck->GetPathName()) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=keyed-descriptor code=%d parent=%s"),Code,*ParentPath); return false; }
        auto* Parent=LoadObject<UClass>(nullptr,*ParentPath);
        auto* ParentCDO=Parent?Parent->GetDefaultObject():nullptr;
        auto* ParentMove=ParentCDO?Movement->GetObjectPropertyValue_InContainer(ParentCDO):nullptr;
        if (!ParentMove) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=original-parent-movement code=%d parent=%s"),Code,*GetPathNameSafe(Parent)); return false; }
        const auto ParentBefore=DORHurricaneDefaults(ParentCDO), ParentMoveBefore=DORHurricaneDefaults(ParentMove);
        DeleteGeneratedFile(PackageName);
        auto* BP=FKismetEditorUtilities::CreateBlueprint(Parent,CreatePackage(*PackageName),FName(*Name),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass(),TEXT("DORHurricaneCarrier"));
        if (!BP) return false;
        FKismetEditorUtilities::CompileBlueprint(BP);
        auto* ChildCDO=BP->GeneratedClass->GetDefaultObject();
        auto* ChildMove=Movement->GetObjectPropertyValue_InContainer(ChildCDO);
        if (!ChildMove || ChildMove==ParentMove || ChildMove==BuckMove || ChildMove->GetOuter()!=ChildCDO || ChildMove->GetArchetype()!=ParentMove) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=child-template-ownership code=%d child=%s movement=%s outer=%s archetype=%s expected=%s"),Code,*GetPathNameSafe(ChildCDO),*GetPathNameSafe(ChildMove),*GetPathNameSafe(ChildMove?ChildMove->GetOuter():nullptr),*GetPathNameSafe(ChildMove?ChildMove->GetArchetype():nullptr),*GetPathNameSafe(ParentMove)); return false; }
        const auto ChildBefore=DORHurricaneDefaults(ChildCDO), ChildMoveBefore=DORHurricaneDefaults(ChildMove);
        for (const FName Field:Allowed)
        {
            auto* Property=FindFProperty<FProperty>(BuckMove->GetClass(),Field);
            if (!Property || FindFProperty<FProperty>(ChildMove->GetClass(),Field)!=Property) return false;
            Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(ChildMove),Property->ContainerPtrToValuePtr<void>(BuckMove));
        }
        FAssetRegistryModule::AssetCreated(BP);
        if (!SaveBlueprint(BP)) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=save code=%d"),Code); return false; }
        ChildCDO=BP->GeneratedClass->GetDefaultObject();
        ChildMove=Movement->GetObjectPropertyValue_InContainer(ChildCDO);
        if (!ChildMove || ChildMove->GetOuter()!=ChildCDO || ChildMove->GetArchetype()!=ParentMove || BP->GeneratedClass->GetSuperClass()!=Parent) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=saved-template-ownership code=%d"),Code); return false; }
        const auto ChildAfter=DORHurricaneDefaults(ChildCDO), ChildMoveAfter=DORHurricaneDefaults(ChildMove);
        if (ChildBefore.OrderIndependentCompareEqual(ChildAfter)==false ||
            !ParentBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(ParentCDO)) || !ParentMoveBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(ParentMove)) ||
            !BuckBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(BuckCDO)) || !BuckMoveBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(BuckMove))) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=unchanged-defaults code=%d"),Code); return false; }
        for (const auto& Field:ChildMoveBefore)
            if (ChildMoveAfter[Field.Key]!=(Allowed.Contains(Field.Key)?BuckMoveBefore[Field.Key]:Field.Value)) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane feasibility failed stage=saved-field code=%d field=%s before=%s after=%s expected=%s"),Code,*Field.Key.ToString(),*Field.Value,*ChildMoveAfter[Field.Key],*(Allowed.Contains(Field.Key)?BuckMoveBefore[Field.Key]:Field.Value)); return false; }
        UE_LOG(LogDORGenerateAssets,Display,TEXT("Hurricane carrier saved code=%d class=%s parent=%s movement=%s archetype=%s exactBuckFlightFields=5 parentAndBuckUnchanged=true"),Code,*BP->GeneratedClass->GetPathName(),*ParentPath,*ChildMove->GetPathName(),*ParentMove->GetPathName());
    }
    return true;
}

// A fresh editor process reloads the real saved children. Uncooked export tables
// infer native default-subobject archetypes rather than serializing TemplateIndex.
bool DORVerifyHurricaneCarriersReload()
{
    const FString NativeRoot=TEXT("/Game/WeaponsNTools/MicroMissileLauncher/");
    auto* Moving=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.Projectile"));
    auto* Movement=Moving?FindFProperty<FObjectPropertyBase>(Moving,TEXT("MovementComponent")):nullptr;
    auto* Buck=LoadObject<UClass>(nullptr,*(NativeRoot+TEXT("PRJ_MicroMissile_Buck.PRJ_MicroMissile_Buck_C")));
    auto* BuckMove=Movement&&Buck?Movement->GetObjectPropertyValue_InContainer(Buck->GetDefaultObject()):nullptr;
    if (!BuckMove) return false;
    const TSet<FName> Allowed={TEXT("InitialSpeed"),TEXT("MaxSpeed"),TEXT("AccelerationCurve"),TEXT("bIsHomingProjectile"),TEXT("HomingAccelerationMagnitude")};
    const auto BuckBefore=DORHurricaneDefaults(Buck->GetDefaultObject()), BuckMoveBefore=DORHurricaneDefaults(BuckMove);
    auto Equivalent=[](FProperty* P,UObject* Child,UObject* Parent)
    {
        if (P->Identical_InContainer(Child,Parent)) return true;
        auto* Object=CastField<FObjectPropertyBase>(P);
        auto* A=Object?Object->GetObjectPropertyValue_InContainer(Child):nullptr;
        auto* B=Object?Object->GetObjectPropertyValue_InContainer(Parent):nullptr;
        return A && B && A->GetClass()==B->GetClass() && A->GetArchetype()==B;
    };
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const int32 Code:{338,339,343})
    {
        const FString ParentName=Code==338?TEXT("PRJ_MicroMissile_Mine_Head"):Code==339?TEXT("PRJ_MicroMissile_PlasmaBurster"):TEXT("PRJ_MicroMissile_Cluster");
        const FString Name=Code==338?TEXT("PRJ_DOR338_ChargedMine"):Code==339?TEXT("PRJ_DOR339_ChargedPlasma"):TEXT("PRJ_DOR343_ChargedCluster");
        auto* Parent=LoadObject<UClass>(nullptr,*(NativeRoot+ParentName+TEXT(".")+ParentName+TEXT("_C")));
        auto* ParentCDO=Parent?Parent->GetDefaultObject():nullptr;
        auto* ParentMove=ParentCDO?Movement->GetObjectPropertyValue_InContainer(ParentCDO):nullptr;
        if (!ParentMove) return false;
        const auto ParentBefore=DORHurricaneDefaults(ParentCDO), ParentMoveBefore=DORHurricaneDefaults(ParentMove);
        const FString ClassPath=FString(RuntimeRoot)/TEXT("AdaptedProjectiles/MicroMissileLauncher")/Name+TEXT(".")+Name+TEXT("_C");
        auto* Child=LoadObject<UClass>(nullptr,*ClassPath);
        auto* CDO=Child?Child->GetDefaultObject():nullptr;
        auto* Move=CDO?Movement->GetObjectPropertyValue_InContainer(CDO):nullptr;
        if (!Child || Child->GetSuperClass()!=Parent || !Move || Move->GetOuter()!=CDO || Move->GetArchetype()!=ParentMove)
        { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane reload failed stage=class-template-ownership code=%d child=%s movement=%s archetype=%s expected=%s"),Code,*GetPathNameSafe(Child),*GetPathNameSafe(Move),*GetPathNameSafe(Move?Move->GetArchetype():nullptr),*GetPathNameSafe(ParentMove)); return false; }
        for (TFieldIterator<FProperty> P(Move->GetClass()); P; ++P)
        {
            UObject* Expected=Allowed.Contains(P->GetFName())?BuckMove:ParentMove;
            if (!Equivalent(*P,Move,Expected)) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane reload failed stage=movement-default code=%d field=%s"),Code,*P->GetName()); return false; }
        }
        for (TFieldIterator<FProperty> P(Parent); P; ++P)
            if (!Equivalent(*P,CDO,ParentCDO)) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane reload failed stage=inherited-actor-default code=%d field=%s"),Code,*P->GetName()); return false; }
        for (const FName Function:{FName(TEXT("OnInitialized")),FName(TEXT("OnImpacted")),FName(TEXT("OnUpgradeElementAdded")),FName(TEXT("OnPenetrated")),FName(TEXT("ReceiveBeginPlay"))})
            if (Child->FindFunctionByName(Function)!=Parent->FindFunctionByName(Function)) { UE_LOG(LogDORGenerateAssets,Error,TEXT("Hurricane reload failed stage=inherited-function code=%d function=%s"),Code,*Function.ToString()); return false; }
        if (!ParentBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(ParentCDO)) || !ParentMoveBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(ParentMove)) ||
            !BuckBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(Buck->GetDefaultObject())) || !BuckMoveBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(BuckMove))) return false;
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("code"),Code);Row->SetStringField(TEXT("class"),Child->GetPathName());Row->SetStringField(TEXT("parent"),Parent->GetPathName());Row->SetStringField(TEXT("movement"),Move->GetPathName());Row->SetStringField(TEXT("movementOwner"),Move->GetOuter()->GetPathName());Row->SetStringField(TEXT("movementArchetype"),Move->GetArchetype()->GetPathName());Row->SetNumberField(TEXT("exactBuckFields"),5);Row->SetBoolField(TEXT("otherDefaultsAndFunctionsInherited"),true);Row->SetBoolField(TEXT("parentAndBuckUnchanged"),true);Rows.Add(MakeShared<FJsonValueObject>(Row));
        UE_LOG(LogDORGenerateAssets,Display,TEXT("Hurricane fresh reload passed code=%d movement=%s archetype=%s exactBuckFields=5 otherDefaultsAndFunctionsInherited=true parentAndBuckUnchanged=true"),Code,*Move->GetPathName(),*ParentMove->GetPathName());
    }
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("schema"),TEXT("dor.hurricane.fresh-editor-reload.v1"));Receipt->SetArrayField(TEXT("cases"),Rows);Receipt->SetBoolField(TEXT("gameplayProof"),false);
    FString Json;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));
    return FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("DORHurricaneReadback.json")));
}
}
