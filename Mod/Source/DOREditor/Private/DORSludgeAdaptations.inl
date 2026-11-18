// Typed template edits preserve native operators, owned children and callbacks.
namespace
{
bool DORSetOriginalRootElements(UItemUpgrade* Root, const TSharedPtr<FJsonObject>& Pair)
{
    const TArray<TSharedPtr<FJsonValue>>* References = nullptr;
    if (!Pair->TryGetArrayField(TEXT("rootElementReferences"), References)) return true;
    auto* Array = FindFProperty<FArrayProperty>(Root->GetClass(), TEXT("Elements"));
    auto* Inner = Array ? CastField<FObjectPropertyBase>(Array->Inner) : nullptr;
    if (!Inner) return false;
    FScriptArrayHelper Destination(Array, Array->ContainerPtrToValuePtr<void>(Root));
    for (const auto& Value : *References)
    {
        auto Ref = Value->AsObject();
        auto* Source = LoadObject<UItemUpgrade>(nullptr, *Ref->GetStringField(TEXT("sourceRoot")));
        if (!Source || Ref->GetStringField(TEXT("property")) != TEXT("Elements") || Ref->GetStringField(TEXT("sourceField")) != TEXT("Elements[0]")) return false;
        FScriptArrayHelper Original(Array, Array->ContainerPtrToValuePtr<void>(Source));
        if (Original.Num() != 1) return false;
        UObject* Element = Inner->GetObjectPropertyValue(Original.GetRawPtr(0));
        if (!Element || Element->GetOuter() != Source || !Element->HasAnyFlags(RF_Public) ||
            Element->GetName() != Ref->GetStringField(TEXT("expectedObjectName")) ||
            Element->GetClass()->GetPathName() != Ref->GetStringField(TEXT("expectedClassPath"))) return false;
        bool Present = false;
        for (int32 I=0; I<Destination.Num(); ++I) Present |= Inner->GetObjectPropertyValue(Destination.GetRawPtr(I)) == Element;
        if (!Present) Inner->SetObjectPropertyValue(Destination.GetRawPtr(Destination.AddValue()), Element);
    }
    return true;
}
}

bool DORBuild247TrailMarker()
{
    const FString PackageName = FString(RuntimeRoot) / TEXT("AdaptedProjectiles/GooCannon/PRJ_DOR247_TrailVolatile");
    auto* Parent = LoadObject<UClass>(nullptr, TEXT("/Game/WeaponsNTools/GooCannon/PRJ_BigGooProjectile_Trail.PRJ_BigGooProjectile_Trail_C"));
    auto* Base = LoadObject<UClass>(nullptr, TEXT("/Game/WeaponsNTools/GooCannon/PRJ_BaseGooProjectile.PRJ_BaseGooProjectile_C"));
    auto* ElementClass = LoadObject<UClass>(nullptr, TEXT("/Script/FSD.ProjectileUpgradeElement"));
    auto* Marker = LoadObject<UItemUpgrade>(nullptr, TEXT("/Game/WeaponsNTools/GooCannon/Overclocks/OC_BonusesAndPenalties/Bonus_Volatile_Puddles_GooCannon.Bonus_Volatile_Puddles_GooCannon"));
    auto* ParentEvent = Parent ? Parent->FindFunctionByName(TEXT("OnUpgradeElementAdded")) : nullptr;
    if (!ParentEvent || !Base || !ElementClass || !Marker || !FindFProperty<FBoolProperty>(Base,TEXT("ExplosivePuddles"))) return false;
    DeleteGeneratedFile(PackageName);
    auto* BP = FKismetEditorUtilities::CreateBlueprint(Parent,CreatePackage(*PackageName),TEXT("PRJ_DOR247_TrailVolatile"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass(),TEXT("DORSludgeMarker"));
    if (!BP) return false;
    auto* G = FBlueprintEditorUtils::FindEventGraph(BP);
    auto* Event = AddNode<UK2Node_Event>(G,0,0);
    Event->EventReference.SetExternalMember(TEXT("OnUpgradeElementAdded"),Parent);
    Event->bOverrideFunction = true;
    Event->AllocateDefaultPins();
    auto* CallParent = AddNode<UK2Node_CallParentFunction>(G,240,0);
    CallParent->SetFromFunction(ParentEvent);
    CallParent->AllocateDefaultPins();
    auto* Test = AddCall(G,ElementClass,TEXT("ElementOf"),480,180);
    auto* Branch = AddNode<UK2Node_IfThenElse>(G,720,0);
    Branch->AllocateDefaultPins();
    auto* Set = AddNode<UK2Node_VariableSet>(G,960,0);
    Set->VariableReference.SetExternalMember(TEXT("ExplosivePuddles"),Base);
    Set->AllocateDefaultPins();
    FindPin(Set,TEXT("ExplosivePuddles"))->DefaultValue = TEXT("true");
    FindPin(Test,TEXT("upg"))->DefaultObject = Marker;
    bool Ok = Connect(FindPin(Event,UEdGraphSchema_K2::PN_Then),CallParent->GetExecPin());
    Ok &= Connect(FindPin(Event,TEXT("element")),FindPin(CallParent,TEXT("element")));
    Ok &= Connect(FindPin(Event,TEXT("element")),FindPin(Test,UEdGraphSchema_K2::PN_Self));
    Ok &= Connect(CallParent->GetThenPin(),Branch->GetExecPin());
    Ok &= Connect(Test->GetReturnValuePin(),Branch->GetConditionPin());
    Ok &= Connect(Branch->GetThenPin(),Set->GetExecPin());
    Ok &= Connect(SelfPin(G,720,240),FindPin(Set,UEdGraphSchema_K2::PN_Self));
    if (!Ok) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    FAssetRegistryModule::AssetCreated(BP);
    return BP->Status != BS_Error && SaveBlueprint(BP);
}

bool CreateDORSludgeAdaptations(const TArray<TSharedPtr<FJsonObject>>& Pairs)
{
    bool Need247 = false;
    for (const auto& Pair : Pairs) Need247 |= Pair->GetIntegerField(TEXT("testIndex")) == 247 && Pair->HasField(TEXT("templateLeafClones"));
    if (Need247 && !DORBuild247TrailMarker()) return false;
    for (const auto& Pair : Pairs)
    {
        const TArray<TSharedPtr<FJsonValue>>* Clones = nullptr;
        if (!Pair->TryGetArrayField(TEXT("templateLeafClones"),Clones)) continue;
        for (const auto& Value : *Clones)
        {
            auto Spec = Value->AsObject();
            const FString Path = Spec->GetStringField(TEXT("objectPath"));
            FString PackageName, Name;
            if (!Path.StartsWith(FString(RuntimeRoot)/TEXT("AdaptedLeaves/")) || !Path.Split(TEXT("."),&PackageName,&Name)) return false;
            auto* Source = LoadObject<UItemUpgrade>(nullptr,*Spec->GetStringField(TEXT("sourceObjectPath")));
            if (!Source || Source->GetClass()->GetPathName()!=Spec->GetStringField(TEXT("sourceClassPath"))) return false;
            DeleteGeneratedFile(PackageName);
            auto* Package = CreatePackage(*PackageName);
            auto* Clone = Cast<UItemUpgrade>(StaticDuplicateObject(Source,Package,FName(*Name)));
            if (!Clone) return false;
            Clone->SetFlags(RF_Public|RF_Standalone);
            auto* Id = FindFProperty<FStructProperty>(Clone->GetClass(),TEXT("SavegameID"));
            if (!Id) return false;
            Id->CopyCompleteValue(Id->ContainerPtrToValuePtr<void>(Clone),Id->ContainerPtrToValuePtr<void>(Clone->GetClass()->GetDefaultObject()));
            auto Write = Spec->GetObjectField(TEXT("write"));
            const FString Kind = Write->GetStringField(TEXT("kind"));
            FName Changed;
            if (Kind == TEXT("owned-condition-class-array-slot"))
            {
                if (Source->GetClass()->GetPathName()!=TEXT("/Script/FSD.ProjectileUpgrade")) return false;
                auto* ConditionProperty = FindFProperty<FObjectPropertyBase>(Source->GetClass(),TEXT("Condition"));
                auto* Original = ConditionProperty ? ConditionProperty->GetObjectPropertyValue_InContainer(Source) : nullptr;
                auto* Condition = ConditionProperty ? ConditionProperty->GetObjectPropertyValue_InContainer(Clone) : nullptr;
                if (!Original || !Condition || Original==Condition || Condition->GetOuter()!=Clone || Condition->GetName()!=Write->GetStringField(TEXT("conditionObjectName")) || Condition->GetClass()->GetPathName()!=Write->GetStringField(TEXT("conditionClassPath"))) return false;
                auto* Array = FindFProperty<FArrayProperty>(Condition->GetClass(),TEXT("ProjectileClasses"));
                auto* Inner = Array ? CastField<FClassProperty>(Array->Inner) : nullptr;
                auto* Target = LoadObject<UClass>(nullptr,*Write->GetStringField(TEXT("value")));
                if (!Inner || !Target || !Target->IsChildOf(Inner->MetaClass)) return false;
                FScriptArrayHelper Values(Array,Array->ContainerPtrToValuePtr<void>(Condition));
                FScriptArrayHelper Originals(Array,Array->ContainerPtrToValuePtr<void>(Original));
                const int32 Slot = Write->GetIntegerField(TEXT("index"));
                if (Values.Num()!=Originals.Num() || !Values.IsValidIndex(Slot) || Inner->GetObjectPropertyValue(Values.GetRawPtr(Slot))->GetPathName()!=Write->GetStringField(TEXT("expected"))) return false;
                Inner->SetObjectPropertyValue(Values.GetRawPtr(Slot),Target);
                for (int32 I=0; I<Values.Num(); ++I) if (I!=Slot && !Inner->Identical(Values.GetRawPtr(I),Originals.GetRawPtr(I),0)) return false;
                for (TFieldIterator<FProperty> P(Condition->GetClass()); P; ++P) if (*P!=Array && !P->Identical_InContainer(Condition,Original)) return false;
                Changed = TEXT("Condition");
            }
            else if (Kind == TEXT("required-class"))
            {
                if (Source->GetClass()->GetPathName()!=TEXT("/Script/FSD.DamageUpgrade")) return false;
                auto* P = FindFProperty<FClassProperty>(Clone->GetClass(),TEXT("RequiredClass"));
                auto* Target = LoadObject<UClass>(nullptr,*Write->GetStringField(TEXT("value")));
                if (!P || !Target || !Target->IsChildOf(P->MetaClass) || P->GetObjectPropertyValue_InContainer(Source)->GetPathName()!=Write->GetStringField(TEXT("expected"))) return false;
                P->SetObjectPropertyValue_InContainer(Clone,Target);
                Changed = TEXT("RequiredClass");
            }
            else if (Kind == TEXT("charged-projectile-switch"))
            {
                if (Source->GetClass()->GetPathName()!=TEXT("/Script/FSD.WeaponSwitchProjectileUpgrade")) return false;
                auto* S = FindFProperty<FStructProperty>(Clone->GetClass(),TEXT("Projectile"));
                auto* P = S ? FindFProperty<FClassProperty>(S->Struct,TEXT("ProjectileClass")) : nullptr;
                auto* C = S ? FindFProperty<FEnumProperty>(S->Struct,TEXT("CriteriaType")) : nullptr;
                auto* Target = LoadObject<UClass>(nullptr,*Write->GetStringField(TEXT("value")));
                if (!P || !C || !Target || !Target->IsChildOf(P->MetaClass)) return false;
                void* Struct = S->ContainerPtrToValuePtr<void>(Clone);
                void* Original = S->ContainerPtrToValuePtr<void>(Source);
                if (P->GetObjectPropertyValue_InContainer(Struct)->GetPathName()!=Write->GetStringField(TEXT("expected")) || C->GetUnderlyingProperty()->GetSignedIntPropertyValue(C->ContainerPtrToValuePtr<void>(Struct))!=C->GetEnum()->GetValueByNameString(TEXT("EProjectileSwitchCriteria::CharagedProjectile"))) return false;
                P->SetObjectPropertyValue_InContainer(Struct,Target);
                for (TFieldIterator<FProperty> F(S->Struct); F; ++F) if (*F!=P && !F->Identical_InContainer(Struct,Original)) return false;
                Changed = TEXT("Projectile");
            }
            else if (Kind == TEXT("hurricane-normal-to-charged-switch"))
            {
                const int32 Code=Pair->GetIntegerField(TEXT("testIndex"));
                if ((Code!=338 && Code!=339 && Code!=343) || Source->GetClass()->GetPathName()!=TEXT("/Script/FSD.WeaponSwitchProjectileUpgrade")) return false;
                const FString ExpectedLeaf=Code==338?TEXT("Bonus_MineHeadMissile_MML"):Code==339?TEXT("Bonus_PlasmaBursterMissile_MML"):TEXT("Bonus_ClusterMissile_MML");
                if (Source->GetPathName()!=FString(TEXT("/Game/WeaponsNTools/MicroMissileLauncher/Overclocks/OCBonusesAndPenalties/"))+ExpectedLeaf+TEXT(".")+ExpectedLeaf) return false;
                auto* S=FindFProperty<FStructProperty>(Clone->GetClass(),TEXT("Projectile"));
                auto* P=S?FindFProperty<FClassProperty>(S->Struct,TEXT("ProjectileClass")):nullptr;
                auto* C=S?FindFProperty<FEnumProperty>(S->Struct,TEXT("CriteriaType")):nullptr;
                auto* Target=LoadObject<UClass>(nullptr,*Write->GetStringField(TEXT("value")));
                if (!P || !C || !Target || !Target->IsChildOf(P->MetaClass) || Target->GetPathName()!=Pair->GetObjectField(TEXT("hurricaneChargedCarrier"))->GetStringField(TEXT("objectPath"))) return false;
                void* Struct=S->ContainerPtrToValuePtr<void>(Clone);
                void* Original=S->ContainerPtrToValuePtr<void>(Source);
                if (C->GetUnderlyingProperty()->GetSignedIntPropertyValue(C->ContainerPtrToValuePtr<void>(Original))!=0 || P->GetObjectPropertyValue_InContainer(Original)!=Target->GetSuperClass() || Target->GetSuperClass()->GetPathName()!=Write->GetStringField(TEXT("expected"))) return false;
                const int64 Charged=C->GetEnum()->GetValueByNameString(TEXT("EProjectileSwitchCriteria::CharagedProjectile"));
                if (Charged!=1) return false;
                C->GetUnderlyingProperty()->SetIntPropertyValue(C->ContainerPtrToValuePtr<void>(Struct),Charged);
                P->SetObjectPropertyValue_InContainer(Struct,Target);
                for (TFieldIterator<FProperty> F(S->Struct); F; ++F) if (*F!=P && *F!=C && !F->Identical_InContainer(Struct,Original)) return false;
                Changed=TEXT("Projectile");
            }
            else if (Kind == TEXT("full-lock-auto-component"))
            {
                const int32 Code=Pair->GetIntegerField(TEXT("testIndex"));
                if((Code!=302 && Code!=304 && Code!=309) || Source->GetClass()->GetPathName()!=TEXT("/Script/FSD.AddComponentUpgrade") || Source->GetPathName()!=TEXT("/Game/WeaponsNTools/LockOnRifle/Overclocks/OC_BonusesAndPenalties/OC_Bonus_AutoShoot_LockOnRifle.OC_Bonus_AutoShoot_LockOnRifle"))return false;
                auto* P=FindFProperty<FClassProperty>(Clone->GetClass(),TEXT("ComponentClass"));auto* Target=LoadObject<UClass>(nullptr,*Write->GetStringField(TEXT("value")));
                if(!P || !Target || !Target->IsChildOf(P->MetaClass) || GetPathNameSafe(P->GetObjectPropertyValue_InContainer(Source))!=Write->GetStringField(TEXT("expected")) || Target->GetSuperClass()!=P->GetObjectPropertyValue_InContainer(Source))return false;
                P->SetObjectPropertyValue_InContainer(Clone,Target);Changed=TEXT("ComponentClass");
            }
            else if (Kind == TEXT("cryo-snowball-projectile-class"))
            {
                if(Pair->GetIntegerField(TEXT("testIndex"))!=455 || Pair->GetStringField(TEXT("pairId"))!=TEXT("eeea2c40848d1380") || Source->GetClass()->GetPathName()!=TEXT("/Script/FSD.CryoSprayProjectileUpgrade") || Source->GetPathName()!=TEXT("/Game/WeaponsNTools/Cryospray/Overclocks/OC_BonusesAndPenalties/OC_Bonus_Snowball.OC_Bonus_Snowball"))return false;
                auto* P=FindFProperty<FClassProperty>(Clone->GetClass(),TEXT("ProjectileClass"));auto* Cost=FindFProperty<FFloatProperty>(Clone->GetClass(),TEXT("MaxFireCost"));auto* Arch=FindFProperty<FFloatProperty>(Clone->GetClass(),TEXT("BaseArch"));auto* Target=LoadObject<UClass>(nullptr,*Write->GetStringField(TEXT("value")));
                if(!P || !Cost || !Arch || !Target || !Target->IsChildOf(P->MetaClass) || Cost->GetPropertyValue_InContainer(Source)!=25.f || GetPathNameSafe(P->GetObjectPropertyValue_InContainer(Source))!=Write->GetStringField(TEXT("expected")) || Target->GetPathName()!=Pair->GetObjectField(TEXT("cryoSnowballCarrier"))->GetStringField(TEXT("objectPath")))return false;
                P->SetObjectPropertyValue_InContainer(Clone,Target);Changed=TEXT("ProjectileClass");
                UE_LOG(LogDORGenerateAssets,Display,TEXT("Cryo455 original Snowball clone MaxFireCost=25 BaseArch=%g allOtherFieldsPreserved=true"),Arch->GetPropertyValue_InContainer(Clone));
            }
            else return false;
            for (TFieldIterator<FProperty> P(Clone->GetClass()); P; ++P)
                if (P->GetFName()!=Changed && *P!=Id && !P->Identical_InContainer(Clone,Source)) return false;
            FAssetRegistryModule::AssetCreated(Clone);
            if (!SaveAsset(Package,Clone)) return false;
            UE_LOG(LogDORGenerateAssets,Display,TEXT("Typed adaptation saved %s kind=%s source=%s"),*Path,*Kind,*Source->GetPathName());
        }
    }
    return true;
}
