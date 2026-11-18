// Pair359-only authority experiment. The owner retains its local V2 journal;
// the server only operates on the target's transient native mission state.
struct FDOR359Graph : FDORSafeGraph
{
    UClass* MasterClass;
    FDOR359Graph(UBlueprint* BP, FName Name, UClass* MC)
        : FDORSafeGraph(BP, Name), MasterClass(MC) {}
    void Done()
    {
        auto* R=AddNode<UK2Node_FunctionResult>(G,++Serial*160,0);
        R->FunctionReference=Entry->FunctionReference; R->AllocateDefaultPins();
        Connect(Tail,R->FindPin(UEdGraphSchema_K2::PN_Execute)); Tail=nullptr;
    }
    UEdGraphPin* Int(int32 V)
    {
        auto* N=Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));
        FindPin(N,TEXT("A"))->DefaultValue=FString::FromInt(V);
        FindPin(N,TEXT("B"))->DefaultValue=TEXT("0"); return N->GetReturnValuePin();
    }
    UEdGraphPin* Eq(UEdGraphPin* A, UEdGraphPin* B, const FName Fn=TEXT("EqualEqual_IntInt"))
    {return Equal(A,B,Fn,Fn==TEXT("EqualEqual_StrStr")?UKismetStringLibrary::StaticClass():Fn==TEXT("EqualEqual_GuidGuid")?UKismetGuidLibrary::StaticClass():UKismetMathLibrary::StaticClass());}
    UEdGraphPin* And(UEdGraphPin* A, UEdGraphPin* B)
    {return Eq(A,B,TEXT("BooleanAND"));}
    UEdGraphPin* Text(UEdGraphPin* P, const FName Conversion)
    {auto* N=Call(Conversion==TEXT("Conv_GuidToString")?UKismetGuidLibrary::StaticClass():UKismetStringLibrary::StaticClass(),Conversion);Connect(P,N->FindPin(TEXT("InInt"))?FindPin(N,TEXT("InInt")):N->FindPin(TEXT("InGuid"))?FindPin(N,TEXT("InGuid")):FindPin(N,TEXT("InBool")));return N->GetReturnValuePin();}
    UEdGraphPin* Join(UEdGraphPin* A, UEdGraphPin* B)
    {auto* N=Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();}
    void Log(const FString& Stage, UEdGraphPin* Detail=nullptr)
    {
        auto* Line=DORSafeConcat(*this,FString::Printf(TEXT("[DOR-359]|build=%s|test=359|slot=secondary|stage=%s|player="),DORResearchBuildTag,*Stage),Get(TEXT("Probe359Key")));
        Line=Join(Line,DORSafeConcat(*this,TEXT("|generation="),Text(Get(TEXT("Probe359Generation")),TEXT("Conv_IntToString"))));
        Line=Join(Line,DORSafeConcat(*this,TEXT("|phase="),Text(Get(TEXT("Probe359Phase")),TEXT("Conv_IntToString"))));
        if(Detail)Line=Join(Line,DORSafeConcat(*this,TEXT("|data="),Detail));
        auto* N=Call(MasterClass,TEXT("DOR_DebugNotify"));Connect(Get(TEXT("Probe359Master")),FindPin(N,UEdGraphSchema_K2::PN_Self));Connect(Line,FindPin(N,TEXT("Message")));
        auto* Flush=Call(MasterClass,TEXT("DOR_DiagnosticFlush"));Connect(Get(TEXT("Probe359Master")),FindPin(Flush,UEdGraphSchema_K2::PN_Self));
    }
    void Check(UEdGraphPin* V, const TCHAR* Reason, const TCHAR* Failure=nullptr)
    {
        auto* B=Branch(V);Tail=B->GetElsePin();Log(Reason);
        if(Failure){auto* N=Self(Failure);if(N->FindPin(TEXT("Reason")))FindPin(N,TEXT("Reason"))->DefaultValue=Reason;}
        Done();Tail=B->GetThenPin();
    }
    UEdGraphPin* Now()
    {auto* N=Call(UGameplayStatics::StaticClass(),TEXT("GetTimeSeconds"));Connect(SelfPin(G,0,700),FindPin(N,TEXT("WorldContextObject")));return N->GetReturnValuePin();}
    void Deadline()
    {auto* N=Call(UKismetMathLibrary::StaticClass(),TEXT("Add_FloatFloat"));Connect(Now(),FindPin(N,TEXT("A")));FindPin(N,TEXT("B"))->DefaultValue=TEXT("5.0");Set(TEXT("Probe359Deadline"),N->GetReturnValuePin());}
    void Timer(const TCHAR* Name)
    {auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("K2_SetTimer"));Connect(SelfPin(G,0,700),FindPin(N,TEXT("Object")));FindPin(N,TEXT("FunctionName"))->DefaultValue=Name;FindPin(N,TEXT("Time"))->DefaultValue=TEXT("0.1");FindPin(N,TEXT("bLooping"))->DefaultValue=TEXT("false");}
    UEdGraphPin* Cast(UEdGraphPin* V,UClass* C)
    {auto* N=AddNode<UK2Node_DynamicCast>(G,++Serial*160,200);N->TargetType=C;N->SetPurity(true);N->AllocateDefaultPins();ForcePinType(N->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass()));Connect(V,N->GetCastSourcePin());return N->GetCastResultPin();}
    UEdGraphPin* Component(UEdGraphPin* P,UClass* C)
    {auto* N=Call(AActor::StaticClass(),TEXT("GetComponentByClass"));FindPin(N,TEXT("ComponentClass"))->DefaultObject=C;Connect(P,FindPin(N,UEdGraphSchema_K2::PN_Self));return Cast(N->GetReturnValuePin(),C);}
    UEdGraphPin* Item(UClass* IC,EItemCategory Cat)
    {auto* N=Call(IC,TEXT("GetItem"));Connect(Get(TEXT("Probe359Inventory")),FindPin(N,UEdGraphSchema_K2::PN_Self));ForcePinType(FindPin(N,TEXT("Category")),MakeItemCategoryPinType());FindPin(N,TEXT("Category"))->DefaultValue=UEnum::GetValueAsString(Cat);return N->GetReturnValuePin();}
    UEdGraphPin* Load(const TCHAR* Path,UClass* C)
    {auto* P=Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeSoftObjectPath"));FindPin(P,TEXT("PathString"))->DefaultValue=Path;auto* R=Call(UKismetSystemLibrary::StaticClass(),TEXT("Conv_SoftObjPathToSoftObjRef"));Connect(P->GetReturnValuePin(),FindPin(R,TEXT("SoftObjectPath")));auto* L=Call(UKismetSystemLibrary::StaticClass(),TEXT("LoadAsset_Blocking"));Connect(R->GetReturnValuePin(),FindPin(L,TEXT("Asset")));return Cast(L->GetReturnValuePin(),C);}
    UEdGraphPin* Guid(UEdGraphPin* Obj,UClass* C)
    {auto* N=Call(C,TEXT("GetSavegameID"));Connect(Obj,FindPin(N,UEdGraphSchema_K2::PN_Self));ForcePinType(N->GetReturnValuePin(),MakeStructPinType(TBaseStructure<FGuid>::Get()));return N->GetReturnValuePin();}
    UEdGraphPin* FieldOf(UEdGraphPin* Struct,UScriptStruct* S,const FName Name)
    {return FindPin(Break(Struct,S),Name);}
    void Tuple(UK2Node_CallFunction* N,int32 Phase)
    {Connect(Get(TEXT("Probe359Generation")),FindPin(N,TEXT("Generation")));Connect(Get(TEXT("Probe359Key")),FindPin(N,TEXT("Key")));if(N->FindPin(TEXT("Phase")))FindPin(N,TEXT("Phase"))->DefaultValue=FString::FromInt(Phase);}
    UEdGraphPin* ReferenceFields(UEdGraphPin* Actor,UEdGraphPin* Player)
    {auto* N=Self(TEXT("DOR92_ReferenceFields"));Connect(Actor,FindPin(N,TEXT("Actor")));Connect(Player,FindPin(N,TEXT("Player")));return N->GetReturnValuePin();}
};

bool BuildRemote359AuthorityProbe(UBlueprint* Master,UBlueprint* Proxy)
{
    UClass* IC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.InventoryComponent"));
    UClass* SC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SaveGameStateComponent"));
    UClass* SA=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SavableDataAsset"));
    if(!IC||!SC||!SA)return false;
    const auto PlayerType=MakeObjectPinType(APlayerCharacter::StaticClass());
    const auto ItemType=MakeObjectPinType(AItem::StaticClass());
    const auto LoadoutType=MakeStructPinType(FItemLoadout::StaticStruct());
    const auto SelectionsType=MakeStructPinType(FItemUpgradeSelection::StaticStruct(),true);
    const auto GuidType=MakeStructPinType(TBaseStructure<FGuid>::Get());
    const auto FloatType=[](){FEdGraphPinType T;T.PinCategory=UEdGraphSchema_K2::PC_Float;return T;}();
    AddTypedVariable(Master,TEXT("Probe359OwnerActive"),MakeBoolPinType());
    AddTypedVariable(Master,TEXT("Probe359Proxy"),MakeObjectPinType(AActor::StaticClass()));
    for(const TCHAR* N:{TEXT("Probe359Generation"),TEXT("Probe359Phase")})AddTypedVariable(Proxy,N,MakeIntPinType());
    for(const TCHAR* N:{TEXT("Probe359Key"),TEXT("Probe359Failure")})AddTypedVariable(Proxy,N,MakeStringPinType());
    AddTypedVariable(Proxy,TEXT("Probe359Deadline"),FloatType);
    AddTypedVariable(Proxy,TEXT("Probe359Master"),MakeObjectPinType(Master->GeneratedClass));
    AddTypedVariable(Proxy,TEXT("Probe359Player"),PlayerType);
    AddTypedVariable(Proxy,TEXT("Probe359Inventory"),MakeObjectPinType(IC));
    AddTypedVariable(Proxy,TEXT("Probe359SaveState"),MakeObjectPinType(SC));
    for(const TCHAR* N:{TEXT("Probe359PrePrimary"),TEXT("Probe359PreSecondary"),TEXT("Probe359Temporary"),TEXT("Probe359Expected"),TEXT("Probe359Final"),TEXT("Probe359RollbackPre"),TEXT("Probe359RollbackTemporary")})AddTypedVariable(Proxy,N,ItemType);
    AddTypedVariable(Proxy,TEXT("Probe359PreExplosiveCount"),MakeIntPinType());
    for(const TCHAR* N:{TEXT("Probe359OriginalLoadout"),TEXT("Probe359FinalLoadout"),TEXT("Probe359RequestedLoadout")})AddTypedVariable(Proxy,N,LoadoutType);
    for(const TCHAR* N:{TEXT("Probe359OriginalSelections"),TEXT("Probe359FinalSelections")})AddTypedVariable(Proxy,N,SelectionsType);
    for(const TCHAR* N:{TEXT("Probe359PistolGuid"),TEXT("Probe359AlternateGuid"),TEXT("Probe359PairGuid")})AddTypedVariable(Proxy,N,GuidType);
    AddTypedVariable(Proxy,TEXT("Probe359PistolClass"),MakeClassPinType(AItem::StaticClass()));
    AddTypedVariable(Proxy,TEXT("Probe359AlternateClass"),MakeClassPinType(AItem::StaticClass()));
    AddTypedVariable(Proxy,TEXT("Probe359ExplosiveClass"),MakeClassPinType(UActorComponent::StaticClass()));
    AddTypedVariable(Proxy,TEXT("Probe359ConstructionPassed"),MakeBoolPinType());
    for(const TCHAR* N:{TEXT("Probe359RetrySent"),TEXT("Probe359RetryRequested"),TEXT("Probe359RetryReceived")})AddTypedVariable(Proxy,N,MakeBoolPinType());
    AddTypedVariable(Proxy,TEXT("Probe359ReplayBaseline"),ItemType);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Master);FKismetEditorUtilities::CompileBlueprint(Master);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Proxy);FKismetEditorUtilities::CompileBlueprint(Proxy);
    auto* EG=FBlueprintEditorUtils::FindEventGraph(Proxy);
    auto Event=[&](const TCHAR* Name,bool Server,const TArray<TPair<FName,FEdGraphPinType>>& Extra)
    {
        auto* E=AddCustomEvent(EG,Name,FUNC_Net|FUNC_NetReliable|FUNC_BlueprintCallable|(Server?FUNC_NetServer:FUNC_NetClient),0,18000+EG->Nodes.Num()*20);
        E->CreateUserDefinedPin(TEXT("Generation"),MakeIntPinType(),EGPD_Output);
        E->CreateUserDefinedPin(TEXT("Key"),MakeStringPinType(),EGPD_Output);
        for(const auto& P:Extra)E->CreateUserDefinedPin(P.Key,P.Value,EGPD_Output);
        E->ReconstructNode();return E;
    };
    auto Pin=[](const TCHAR* Name,const FEdGraphPinType& Type){return TPair<FName,FEdGraphPinType>(FName(Name),Type);};
    auto* Begin=Event(TEXT("DOR359_ClientBegin"),false,{Pin(TEXT("Player"),PlayerType)});
    auto* Prepared=Event(TEXT("DOR359_ServerPrepared"),true,{Pin(TEXT("Phase"),MakeIntPinType()),Pin(TEXT("Loadout"),LoadoutType),Pin(TEXT("Selections"),SelectionsType)});
    auto* Observe=Event(TEXT("DOR359_ClientObserve"),false,{Pin(TEXT("Phase"),MakeIntPinType()),Pin(TEXT("Expected"),ItemType)});
    auto* RetryRequest=Event(TEXT("DOR359_ServerReferenceRetry"),true,{Pin(TEXT("Phase"),MakeIntPinType())});
    auto* RetryReceive=Event(TEXT("DOR359_ClientReferenceRetry"),false,{Pin(TEXT("Phase"),MakeIntPinType()),Pin(TEXT("Expected"),ItemType)});
    auto* Ack=Event(TEXT("DOR359_ServerAck"),true,{Pin(TEXT("Phase"),MakeIntPinType()),Pin(TEXT("Succeeded"),MakeBoolPinType())});
    auto* Cleanup=Event(TEXT("DOR359_ClientCleanup"),false,{Pin(TEXT("Phase"),MakeIntPinType()),Pin(TEXT("Succeeded"),MakeBoolPinType())});
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Proxy);FKismetEditorUtilities::CompileBlueprint(Proxy);
    auto Wire=[&](UK2Node_CustomEvent* E,const TCHAR* Function)
    {auto* N=AddSelfCall(EG,Function,500,E->NodePosY);Connect(E->FindPin(UEdGraphSchema_K2::PN_Then),N->GetExecPin());for(auto* P:E->Pins)if(P->Direction==EGPD_Output&&P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec&&N->FindPin(P->PinName))Connect(P,FindPin(N,P->PinName));};
    auto SameGuid=[&](FDOR359Graph& F,UEdGraphPin* A,UEdGraphPin* B){return F.Eq(A,B,TEXT("EqualEqual_GuidGuid"));};
    auto SameObject=[&](FDOR359Graph& F,UEdGraphPin* A,UEdGraphPin* B){return F.Eq(A,B,TEXT("EqualEqual_ObjectObject"));};
    {
        FDOR359Graph F(Proxy,TEXT("DOR92_ReferenceFields"),Master->GeneratedClass);auto* A=F.Input(TEXT("Actor"),ItemType);auto* P=F.Input(TEXT("Player"),PlayerType);
        auto* Path=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetPathName"));Connect(A,FindPin(Path,TEXT("Object")));auto* R=F.Cache(TEXT("Description"),DORSafeConcat(F,TEXT("path="),Path->GetReturnValuePin()));
        R=F.Join(R,DORSafeConcat(F,TEXT(";valid="),F.Text(F.IsValid(A),TEXT("Conv_BoolToString"))));auto* Valid=F.Branch(F.IsValid(A));F.Tail=Valid->GetElsePin();F.Return(R);F.Tail=Valid->GetThenPin();
        auto* C=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(A,FindPin(C,TEXT("Object")));auto* CN=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetClassDisplayName"));Connect(C->GetReturnValuePin(),FindPin(CN,TEXT("Class")));R=F.Join(R,DORSafeConcat(F,TEXT(";class="),CN->GetReturnValuePin()));
        auto* O=F.Call(AActor::StaticClass(),TEXT("GetOwner"));Connect(A,FindPin(O,UEdGraphSchema_K2::PN_Self));R=F.Join(R,DORSafeConcat(F,TEXT(";ownerTarget="),F.Text(SameObject(F,O->GetReturnValuePin(),P),TEXT("Conv_BoolToString"))));R=F.Join(R,DORSafeConcat(F,TEXT(";characterTarget="),F.Text(SameObject(F,F.Field(A,AItem::StaticClass(),TEXT("Character")),P),TEXT("Conv_BoolToString"))));
        for(const TCHAR* Name:{TEXT("GetLocalRole"),TEXT("GetRemoteRole")}){auto* N=F.Call(AActor::StaticClass(),Name);Connect(A,FindPin(N,UEdGraphSchema_K2::PN_Self));auto* T=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_ByteToString"));Connect(N->GetReturnValuePin(),FindPin(T,TEXT("InByte")));R=F.Join(R,DORSafeConcat(F,FString::Printf(TEXT(";%s="),Name),T->GetReturnValuePin()));}
        for(const TCHAR* Name:{TEXT("bReplicates"),TEXT("bOnlyRelevantToOwner"),TEXT("bNetUseOwnerRelevancy")})R=F.Join(R,DORSafeConcat(F,FString::Printf(TEXT(";%s="),Name),F.Text(F.Field(A,AActor::StaticClass(),Name),TEXT("Conv_BoolToString"))));
        auto* D=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_ByteToString"));Connect(F.Field(A,AActor::StaticClass(),TEXT("NetDormancy")),FindPin(D,TEXT("InByte")));R=F.Join(R,DORSafeConcat(F,TEXT(";NetDormancy="),D->GetReturnValuePin()));F.Return(R);F.Compile();
    }
    auto CurrentLoadout=[&](FDOR359Graph& F){return F.Field(F.Get(TEXT("Probe359SaveState")),SC,TEXT("EquippedLoadout"));};
    auto CurrentSelections=[&](FDOR359Graph& F){return F.Field(F.Get(TEXT("Probe359SaveState")),SC,TEXT("ItemUpgradeSelections"));};
    auto NativeSet=[&](FDOR359Graph& F,UEdGraphPin* Loadout,UEdGraphPin* Selections)
    {auto* N=F.Call(SC,TEXT("Server_SetLoadout"));Connect(F.Get(TEXT("Probe359SaveState")),FindPin(N,UEdGraphSchema_K2::PN_Self));Connect(Loadout,FindPin(N,TEXT("Loadout")));ForcePinType(FindPin(N,TEXT("weaponLoadouts")),SelectionsType);Connect(Selections,FindPin(N,TEXT("weaponLoadouts")));};
    auto Refresh=[&](FDOR359Graph& F)
    {auto* N=F.Call(IC,TEXT("UpdateFromSaveGameInSlot"));Connect(F.Get(TEXT("Probe359Inventory")),FindPin(N,UEdGraphSchema_K2::PN_Self));ForcePinType(FindPin(N,TEXT("Category")),MakeItemCategoryPinType());FindPin(N,TEXT("Category"))->DefaultValue=UEnum::GetValueAsString(EItemCategory::SecondaryWeapon);};
    auto InitNative=[&](FDOR359Graph& F,UEdGraphPin* Player)
    {
        F.Set(TEXT("Probe359Player"),Player);
        F.Set(TEXT("Probe359Inventory"),F.Component(Player,IC));
        auto* PS=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(Player,FindPin(PS,UEdGraphSchema_K2::PN_Self));
        F.Set(TEXT("Probe359SaveState"),F.Component(PS->GetReturnValuePin(),SC));
        F.Check(F.And(F.IsValid(F.Get(TEXT("Probe359Inventory"))),F.IsValid(F.Get(TEXT("Probe359SaveState")))),TEXT("invalid-native-context"));
        auto* Pistol=F.Load(TEXT("/Game/WeaponsNTools/Pistol/ID_Pistol.ID_Pistol"),UItemID::StaticClass());
        auto* Alternate=F.Load(TEXT("/Game/WeaponsNTools/ChargeBlaster/ID_ChargeBlaster.ID_ChargeBlaster"),UItemID::StaticClass());
        auto* Pair=F.Load(TEXT("/Game/DualOverclockReward/Pairs/Pistol/DORPair_359_8d77dbb8f0914479.DORPair_359_8d77dbb8f0914479"),UItemUpgrade::StaticClass());
        F.Check(F.And(F.IsValid(Pistol),F.And(F.IsValid(Alternate),F.IsValid(Pair))),TEXT("invalid-probe-assets"));
        F.Set(TEXT("Probe359PistolGuid"),F.Guid(Pistol,SA));F.Set(TEXT("Probe359AlternateGuid"),F.Guid(Alternate,SA));F.Set(TEXT("Probe359PairGuid"),F.Guid(Pair,SA));
        for(auto P:{TPair<UEdGraphPin*,const TCHAR*>(Pistol,TEXT("Probe359PistolClass")),TPair<UEdGraphPin*,const TCHAR*>(Alternate,TEXT("Probe359AlternateClass"))})
        {auto* N=F.Call(UItemID::StaticClass(),TEXT("GetItemClass"));Connect(P.Key,FindPin(N,UEdGraphSchema_K2::PN_Self));F.Set(P.Value,N->GetReturnValuePin());}
        auto* CP=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeSoftClassPath"));FindPin(CP,TEXT("PathString"))->DefaultValue=TEXT("/Game/WeaponsNTools/Pistol/Overclocks/OC_BonusesAndPenalties/BP_ExplosiveReloadComponent.BP_ExplosiveReloadComponent_C");
        auto* CR=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("Conv_SoftClassPathToSoftClassRef"));Connect(CP->GetReturnValuePin(),FindPin(CR,TEXT("SoftClassPath")));
        auto* CL=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("LoadClassAsset_Blocking"));Connect(CR->GetReturnValuePin(),FindPin(CL,TEXT("AssetClass")));
        auto* CC=AddNode<UK2Node_ClassDynamicCast>(F.G,++F.Serial*160,200);CC->TargetType=UActorComponent::StaticClass();CC->SetPurity(true);CC->AllocateDefaultPins();ForcePinType(CC->GetCastSourcePin(),MakeClassPinType(UObject::StaticClass()));ForcePinType(CL->GetReturnValuePin(),MakeClassPinType(UObject::StaticClass()));Connect(CL->GetReturnValuePin(),CC->GetCastSourcePin());F.Set(TEXT("Probe359ExplosiveClass"),CC->GetCastResultPin());
        auto* CV=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("IsValidClass"));Connect(F.Get(TEXT("Probe359ExplosiveClass")),FindPin(CV,TEXT("Class")));F.Check(CV->GetReturnValuePin(),TEXT("invalid-explosive-component-class"));
    };
    auto LiveCount=[&](FDOR359Graph& F,UEdGraphPin* Actor)
    {auto* C=F.Call(AActor::StaticClass(),TEXT("K2_GetComponentsByClass"));Connect(Actor,FindPin(C,UEdGraphSchema_K2::PN_Self));Connect(F.Get(TEXT("Probe359ExplosiveClass")),FindPin(C,TEXT("ComponentClass")));auto* N=F.Array(TEXT("Array_Length"),C->GetReturnValuePin());return N->GetReturnValuePin();};
    auto Snapshot=[&](FDOR359Graph& F)
    {F.Set(TEXT("Probe359OriginalLoadout"),CurrentLoadout(F));F.Set(TEXT("Probe359OriginalSelections"),CurrentSelections(F));F.Set(TEXT("Probe359PrePrimary"),F.Item(IC,EItemCategory::PrimaryWeapon));F.Set(TEXT("Probe359PreSecondary"),F.Item(IC,EItemCategory::SecondaryWeapon));F.Set(TEXT("Probe359PreExplosiveCount"),LiveCount(F,F.Get(TEXT("Probe359PreSecondary"))));F.Log(TEXT("pre-explosive-component-count"),F.Text(F.Get(TEXT("Probe359PreExplosiveCount")),TEXT("Conv_IntToString")));};
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_OriginalRuntimeProof"),Master->GeneratedClass);
        auto* Context=F.Branch(F.And(F.IsValid(F.Get(TEXT("Probe359Inventory"))),F.IsValid(F.Get(TEXT("Probe359SaveState")))));F.Tail=Context->GetElsePin();F.Return();F.Tail=Context->GetThenPin();
        auto* A=F.Item(IC,EItemCategory::SecondaryWeapon);auto* C=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(A,FindPin(C,TEXT("Object")));
        auto* Valid=F.Branch(F.IsValid(A));F.Tail=Valid->GetElsePin();F.Return();F.Tail=Valid->GetThenPin();
        auto* O=F.Call(AActor::StaticClass(),TEXT("GetOwner"));Connect(A,FindPin(O,UEdGraphSchema_K2::PN_Self));
        auto* Proof=F.And(F.IsValid(A),F.Eq(C->GetReturnValuePin(),F.Get(TEXT("Probe359PistolClass")),TEXT("EqualEqual_ClassClass")));
        Proof=F.And(Proof,F.And(SameObject(F,O->GetReturnValuePin(),F.Get(TEXT("Probe359Player"))),SameObject(F,F.Field(A,AItem::StaticClass(),TEXT("Character")),F.Get(TEXT("Probe359Player")))));
        Proof=F.And(Proof,SameGuid(F,F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),F.Get(TEXT("Probe359PistolGuid"))));
        Proof=F.And(Proof,F.SameArray(CurrentSelections(F),F.Get(TEXT("Probe359OriginalSelections"))));
        Proof=F.And(Proof,F.Eq(LiveCount(F,A),F.Get(TEXT("Probe359PreExplosiveCount"))));
        UEdGraphPin* Untouched=nullptr;for(const TCHAR* N:{TEXT("PrimaryWeapon"),TEXT("TraversalTool"),TEXT("ClassTool"),TEXT("Armor"),TEXT("Flare"),TEXT("MiningTool"),TEXT("Grenade")})
        {auto* Eq=SameGuid(F,F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),N),F.FieldOf(F.Get(TEXT("Probe359OriginalLoadout")),FItemLoadout::StaticStruct(),N));Untouched=Untouched?F.And(Untouched,Eq):Eq;}
        Proof=F.And(Proof,F.And(Untouched,F.Eq(F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("iconIndex")),F.FieldOf(F.Get(TEXT("Probe359OriginalLoadout")),FItemLoadout::StaticStruct(),TEXT("iconIndex")))));
        F.Return(F.And(Proof,SameObject(F,F.Item(IC,EItemCategory::PrimaryWeapon),F.Get(TEXT("Probe359PrePrimary")))));F.Entry->AddExtraFlags(FUNC_BlueprintPure);F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_MatchesMessage"),Master->GeneratedClass);
        auto* Generation=F.Input(TEXT("Generation"),MakeIntPinType());auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());
        F.Return(F.And(F.Eq(Generation,F.Get(TEXT("Probe359Generation"))),F.And(F.Eq(Key,F.Get(TEXT("Probe359Key")),TEXT("EqualEqual_StrStr")),F.Eq(Phase,F.Get(TEXT("Probe359Phase"))))));
        F.Entry->AddExtraFlags(FUNC_BlueprintPure);F.Compile();
    }
    auto Correlate=[&](FDOR359Graph& F,bool Server)
    {
        auto* Generation=F.Input(TEXT("Generation"),MakeIntPinType());auto* Key=F.Input(TEXT("Key"),MakeStringPinType());
        if(Server)
        {
            auto* Match=F.Call(Proxy->GeneratedClass,TEXT("DOR359_MatchesMessage"));Connect(SelfPin(F.G,0,700),FindPin(Match,UEdGraphSchema_K2::PN_Self));Connect(Generation,FindPin(Match,TEXT("Generation")));Connect(Key,FindPin(Match,TEXT("Key")));Connect(F.Input(TEXT("Phase"),MakeIntPinType()),FindPin(Match,TEXT("Phase")));
            F.Check(Match->GetReturnValuePin(),TEXT("ignored-stale-or-wrong-phase-message"));
            auto* Pending=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(F.Get(TEXT("Probe359Phase")),FindPin(Pending,TEXT("A")));auto* NotIncomplete=F.Not(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(99)));F.Check(F.And(Pending->GetReturnValuePin(),NotIncomplete),TEXT("ignored-terminal-message"));
        }
        else F.Check(F.And(F.Eq(Generation,F.Get(TEXT("Probe359Generation"))),F.Eq(Key,F.Get(TEXT("Probe359Key")),TEXT("EqualEqual_StrStr"))),TEXT("ignored-stale-tuple"));
        if(Server)
        {
            auto* P=F.Get(TEXT("Probe359Player"));F.Check(F.IsValid(P),TEXT("target-disconnected"),TEXT("DOR359_Abort"));
            auto* ID=F.Call(Master->GeneratedClass,TEXT("DOR_ResolveStablePlayerKey"));Connect(F.Get(TEXT("Probe359Master")),FindPin(ID,UEdGraphSchema_K2::PN_Self));Connect(P,FindPin(ID,TEXT("Player")));
            F.Check(F.And(FindPin(ID,TEXT("Ready")),F.Eq(FindPin(ID,TEXT("Key")),Key,TEXT("EqualEqual_StrStr"))),TEXT("target-identity-changed"),TEXT("DOR359_Abort"));
            auto* Authority=F.Call(AActor::StaticClass(),TEXT("HasAuthority"));Connect(P,FindPin(Authority,UEdGraphSchema_K2::PN_Self));F.Check(Authority->GetReturnValuePin(),TEXT("target-not-authority"),TEXT("DOR359_Abort"));
            auto* Controller=F.Call(APawn::StaticClass(),TEXT("GetController"));Connect(P,FindPin(Controller,UEdGraphSchema_K2::PN_Self));auto* Owner=F.Call(AActor::StaticClass(),TEXT("GetOwner"));
            F.Check(SameObject(F,Controller->GetReturnValuePin(),Owner->GetReturnValuePin()),TEXT("proxy-owner-mismatch"),TEXT("DOR359_Abort"));
        }
    };
    auto UntouchedLoadout=[&](FDOR359Graph& F,UEdGraphPin* A,UEdGraphPin* B)
    {
        UEdGraphPin* All=nullptr;
        for(const TCHAR* N:{TEXT("PrimaryWeapon"),TEXT("TraversalTool"),TEXT("ClassTool"),TEXT("Armor"),TEXT("Flare"),TEXT("MiningTool"),TEXT("Grenade")})
        {auto* Equal=SameGuid(F,F.FieldOf(A,FItemLoadout::StaticStruct(),N),F.FieldOf(B,FItemLoadout::StaticStruct(),N));All=All?F.And(All,Equal):Equal;}
        return F.And(All,F.Eq(F.FieldOf(A,FItemLoadout::StaticStruct(),TEXT("iconIndex")),F.FieldOf(B,FItemLoadout::StaticStruct(),TEXT("iconIndex"))));
    };
    auto ActorEvidence=[&](FDOR359Graph& F,UEdGraphPin* Actor,const TCHAR* Stage)
    {
        auto* Name=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetPathName"));Connect(Actor,FindPin(Name,TEXT("Object")));
        auto* Class=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(Actor,FindPin(Class,TEXT("Object")));
        auto* ClassName=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetClassDisplayName"));Connect(Class->GetReturnValuePin(),FindPin(ClassName,TEXT("Class")));
        F.Log(Stage,F.Join(DORSafeConcat(F,TEXT("actor="),Name->GetReturnValuePin()),DORSafeConcat(F,TEXT("|class="),ClassName->GetReturnValuePin())));
        return Class->GetReturnValuePin();
    };
    auto FreshActor=[&](FDOR359Graph& F,UEdGraphPin* Actor,UEdGraphPin* Class,bool Final,const TCHAR* Failure)
    {
        F.Check(F.IsValid(Actor),TEXT("invalid-actor"),Failure);
        auto* Observed=ActorEvidence(F,Actor,TEXT("actor-readback"));
        F.Check(F.Eq(Observed,Class,TEXT("EqualEqual_ClassClass")),TEXT("wrong-actor-class"),Failure);
        F.Check(F.Not(SameObject(F,Actor,F.Get(TEXT("Probe359PreSecondary")))),TEXT("old-actor-retained"),Failure);
        if(Final)F.Check(F.Not(SameObject(F,Actor,F.Get(TEXT("Probe359Temporary")))),TEXT("temporary-actor-retained"),Failure);
        F.Check(SameObject(F,F.Item(IC,EItemCategory::PrimaryWeapon),F.Get(TEXT("Probe359PrePrimary"))),TEXT("untouched-primary-actor-changed"),Failure);
    };
    auto ComponentEvidence=[&](FDOR359Graph& F,UEdGraphPin* Actor,const TCHAR* Failure)
    {
        auto* C=F.Call(AActor::StaticClass(),TEXT("K2_GetComponentsByClass"));Connect(Actor,FindPin(C,UEdGraphSchema_K2::PN_Self));Connect(F.Get(TEXT("Probe359ExplosiveClass")),FindPin(C,TEXT("ComponentClass")));
        auto* Count=F.Array(TEXT("Array_Length"),C->GetReturnValuePin());
        F.Log(TEXT("live-component-count"),F.Text(Count->GetReturnValuePin(),TEXT("Conv_IntToString")));
        F.Check(F.Eq(Count->GetReturnValuePin(),F.Int(1)),TEXT("wrong-explosive-component-count"),Failure);
    };
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_ReferenceDiagnostic"),Master->GeneratedClass);
        auto Describe=[&](UEdGraphPin* A,const TCHAR* Label)
        {auto* P=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetPathName"));Connect(A,FindPin(P,TEXT("Object")));auto* C=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(A,FindPin(C,TEXT("Object")));auto* N=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetClassDisplayName"));Connect(C->GetReturnValuePin(),FindPin(N,TEXT("Class")));return F.Join(DORSafeConcat(F,Label,P->GetReturnValuePin()),F.Join(DORSafeConcat(F,TEXT(";valid="),F.Text(F.IsValid(A),TEXT("Conv_BoolToString"))),DORSafeConcat(F,TEXT(";class="),N->GetReturnValuePin())));};
        auto* A=F.Cache(TEXT("Actual"),F.Item(IC,EItemCategory::SecondaryWeapon));auto* E=F.Get(TEXT("Probe359Expected"));auto* R=F.Join(Describe(E,TEXT("expected=")),Describe(A,TEXT("|actual=")));
        R=F.Join(R,DORSafeConcat(F,TEXT("|referenceMatch="),F.Text(F.And(F.IsValid(E),SameObject(F,A,E)),TEXT("Conv_BoolToString"))));
        auto* O=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));Connect(F.Get(TEXT("Probe359Inventory")),FindPin(O,UEdGraphSchema_K2::PN_Self));
        R=F.Join(R,DORSafeConcat(F,TEXT("|targetValid="),F.Text(F.IsValid(F.Get(TEXT("Probe359Player"))),TEXT("Conv_BoolToString"))));
        R=F.Join(R,DORSafeConcat(F,TEXT("|inventoryTarget="),F.Text(SameObject(F,O->GetReturnValuePin(),F.Get(TEXT("Probe359Player"))),TEXT("Conv_BoolToString"))));
        R=F.Join(R,DORSafeConcat(F,TEXT("|actualCharacterTarget="),F.Text(SameObject(F,F.Field(A,AItem::StaticClass(),TEXT("Character")),F.Get(TEXT("Probe359Player"))),TEXT("Conv_BoolToString"))));F.Log(TEXT("owner-reference-checkpoint"),R);F.Compile();
    }
    // Filter only the target weapon; all other selection entries must be identical.
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_UntouchedSelections"),Master->GeneratedClass);
        auto* A=F.Input(TEXT("Selections"),SelectionsType);
        AddTypedVariable(Proxy,TEXT("Probe359FilteredSelections"),SelectionsType);
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("Probe359FilteredSelections")));
        auto* Loop=F.Each(A);auto* Element=FindPin(Loop,TEXT("Array Element"));
        F.Branch(F.Not(SameGuid(F,F.FieldOf(Element,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID")),F.Get(TEXT("Probe359PistolGuid")))));
        auto* Add=F.Array(TEXT("Array_Add"),F.Get(TEXT("Probe359FilteredSelections")));Connect(Element,FindPin(Add,TEXT("NewItem")));
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(F.Get(TEXT("Probe359FilteredSelections")));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_SelectedPair"),Master->GeneratedClass);
        auto* A=F.Input(TEXT("Selections"),SelectionsType);
        AddTypedVariable(Proxy,TEXT("Probe359MatchingSelections"),MakeIntPinType());AddTypedVariable(Proxy,TEXT("Probe359WeaponSelectionCount"),MakeIntPinType());F.Set(TEXT("Probe359MatchingSelections"),nullptr,TEXT("0"));F.Set(TEXT("Probe359WeaponSelectionCount"),nullptr,TEXT("0"));
        auto* Loop=F.Each(A);auto* E=FindPin(Loop,TEXT("Array Element"));
        auto* IsWeapon=SameGuid(F,F.FieldOf(E,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID")),F.Get(TEXT("Probe359PistolGuid")));
        auto* IsRoot=SameGuid(F,F.FieldOf(E,FItemUpgradeSelection::StaticStruct(),TEXT("EquippedOverclock")),F.Get(TEXT("Probe359PairGuid")));
        F.Branch(IsWeapon);auto* Weapons=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(F.Get(TEXT("Probe359WeaponSelectionCount")),FindPin(Weapons,TEXT("A")));FindPin(Weapons,TEXT("B"))->DefaultValue=TEXT("1");F.Set(TEXT("Probe359WeaponSelectionCount"),Weapons->GetReturnValuePin());
        F.Branch(IsRoot);auto* Inc=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(F.Get(TEXT("Probe359MatchingSelections")),FindPin(Inc,TEXT("A")));FindPin(Inc,TEXT("B"))->DefaultValue=TEXT("1");F.Set(TEXT("Probe359MatchingSelections"),Inc->GetReturnValuePin());
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(F.And(F.Eq(F.Get(TEXT("Probe359MatchingSelections")),F.Int(1)),F.Eq(F.Get(TEXT("Probe359WeaponSelectionCount")),F.Int(1))));F.Compile();
    }
    auto SelectedPair=[&](FDOR359Graph& F,UEdGraphPin* A){auto* N=F.Self(TEXT("DOR359_SelectedPair"));Connect(A,FindPin(N,TEXT("Selections")));return N->GetReturnValuePin();};
    auto UntouchedSelections=[&](FDOR359Graph& F,UEdGraphPin* A,UEdGraphPin* B)
    {auto* X=F.Self(TEXT("DOR359_UntouchedSelections"));Connect(A,FindPin(X,TEXT("Selections")));auto* AX=F.Cache(TEXT("UntouchedA"),X->GetReturnValuePin());auto* Y=F.Self(TEXT("DOR359_UntouchedSelections"));Connect(B,FindPin(Y,TEXT("Selections")));return F.SameArray(AX,Y->GetReturnValuePin());};
    // Declare signatures first because event wrappers and cleanup functions form a cycle.
    for(const TCHAR* N:{TEXT("DOR359_Abort"),TEXT("DOR359_Start"),TEXT("DOR359_AuthorityStep"),TEXT("DOR359_ServerWatch"),TEXT("DOR359_OwnerWatch"),TEXT("DOR359_SendPrepared"),TEXT("DOR359_Rollback"),TEXT("DOR359_Finish"),TEXT("DOR359_FirstReferenceSend"),TEXT("DOR359_RequestReferenceRetry")})
    {FDOR359Graph F(Proxy,N,Master->GeneratedClass);if(FString(N)==TEXT("DOR359_Abort"))F.Input(TEXT("Reason"),MakeStringPinType());F.Compile();}
    auto ReplayPhase=[&](FDOR359Graph& F){return F.Eq(F.Eq(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(2)),F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(4)),TEXT("BooleanOR")),F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(91)),TEXT("BooleanOR"));};
    auto ReplayUnexpired=[&](FDOR359Graph& F){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_FloatFloat"));Connect(F.Now(),FindPin(N,TEXT("A")));Connect(F.Get(TEXT("Probe359Deadline")),FindPin(N,TEXT("B")));return N->GetReturnValuePin();};
    auto OwnCanonical=[&](FDOR359Graph& F,UEdGraphPin* A,UEdGraphPin* Guid){auto* O=F.Call(AActor::StaticClass(),TEXT("GetOwner"));Connect(A,FindPin(O,UEdGraphSchema_K2::PN_Self));auto* ID=F.Field(A,AItem::StaticClass(),TEXT("ItemID"));return F.And(F.IsValid(A),F.And(F.IsValid(ID),F.And(SameGuid(F,F.Guid(ID,SA),Guid),F.And(SameObject(F,O->GetReturnValuePin(),F.Get(TEXT("Probe359Player"))),SameObject(F,F.Field(A,AItem::StaticClass(),TEXT("Character")),F.Get(TEXT("Probe359Player")))))));};
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_FirstReferenceSend"),Master->GeneratedClass);F.Set(TEXT("Probe359RetrySent"),nullptr,TEXT("false"));F.Deadline();auto* E=F.Get(TEXT("Probe359Expected"));F.Log(TEXT("first-reference-send"),F.Join(F.ReferenceFields(E,F.Get(TEXT("Probe359Player"))),DORSafeConcat(F,TEXT("|nativeSlotEqual="),F.Text(SameObject(F,E,F.Item(IC,EItemCategory::SecondaryWeapon)),TEXT("Conv_BoolToString")))));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_RequestReferenceRetry"),Master->GeneratedClass);auto Quiet=[&](UEdGraphPin* V){auto* B=F.Branch(V);F.Tail=B->GetElsePin();F.Done();F.Tail=B->GetThenPin();};
        auto* Null=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ObjectObject"));Connect(F.Get(TEXT("Probe359Expected")),FindPin(Null,TEXT("A")));Quiet(F.And(ReplayPhase(F),F.And(ReplayUnexpired(F),F.And(F.Not(F.Get(TEXT("Probe359RetryRequested"))),Null->GetReturnValuePin()))));
        auto* A=F.Cache(TEXT("Replacement"),F.Item(IC,EItemCategory::SecondaryWeapon));Quiet(F.IsValid(A));Quiet(F.Not(SameObject(F,A,F.Get(TEXT("Probe359PreSecondary")))));Quiet(F.Not(SameObject(F,A,F.Get(TEXT("Probe359ReplayBaseline")))));
        auto* Temp=F.Branch(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(2)));Quiet(OwnCanonical(F,A,F.Get(TEXT("Probe359AlternateGuid"))));auto* C=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(A,FindPin(C,TEXT("Object")));Quiet(F.Eq(C->GetReturnValuePin(),F.Get(TEXT("Probe359AlternateClass")),TEXT("EqualEqual_ClassClass")));auto* T=F.Tail;F.Tail=Temp->GetElsePin();Quiet(OwnCanonical(F,A,F.Get(TEXT("Probe359PistolGuid"))));auto* PC=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(A,FindPin(PC,TEXT("Object")));Quiet(F.Eq(PC->GetReturnValuePin(),F.Get(TEXT("Probe359PistolClass")),TEXT("EqualEqual_ClassClass")));auto* Request=F.Self(TEXT("DOR359_ServerReferenceRetry"));Connect(T,Request->GetExecPin());F.Tuple(Request,0);Connect(F.Get(TEXT("Probe359Phase")),FindPin(Request,TEXT("Phase")));F.Set(TEXT("Probe359RetryRequested"),nullptr,TEXT("true"));F.Log(TEXT("reference-retry-requested-replacement-trigger-only"),F.ReferenceFields(A,F.Get(TEXT("Probe359Player"))));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_ReplayReferenceAuthority"),Master->GeneratedClass);auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());auto* Generation=F.Input(TEXT("Generation"),MakeIntPinType());auto* Key=F.Input(TEXT("Key"),MakeStringPinType());
        F.Check(F.And(F.Eq(Generation,F.Get(TEXT("Probe359Generation"))),F.Eq(Key,F.Get(TEXT("Probe359Key")),TEXT("EqualEqual_StrStr"))),TEXT("reference-retry-rejected-tuple"));F.Check(F.And(ReplayPhase(F),F.Eq(Phase,F.Get(TEXT("Probe359Phase")))),TEXT("reference-retry-rejected-phase"));F.Check(ReplayUnexpired(F),TEXT("reference-retry-rejected-original-deadline"));F.Check(F.Not(F.Get(TEXT("Probe359RetrySent"))),TEXT("reference-retry-rejected-used"));
        auto* P=F.Get(TEXT("Probe359Player"));F.Check(F.IsValid(P),TEXT("reference-retry-rejected-target"));auto* ID=F.Call(Master->GeneratedClass,TEXT("DOR_ResolveStablePlayerKey"));Connect(F.Get(TEXT("Probe359Master")),FindPin(ID,UEdGraphSchema_K2::PN_Self));Connect(P,FindPin(ID,TEXT("Player")));F.Check(F.And(FindPin(ID,TEXT("Ready")),F.Eq(FindPin(ID,TEXT("Key")),Key,TEXT("EqualEqual_StrStr"))),TEXT("reference-retry-rejected-identity"));auto* C=F.Call(APawn::StaticClass(),TEXT("GetController"));Connect(P,FindPin(C,UEdGraphSchema_K2::PN_Self));auto* O=F.Call(AActor::StaticClass(),TEXT("GetOwner"));F.Check(SameObject(F,C->GetReturnValuePin(),O->GetReturnValuePin()),TEXT("reference-retry-rejected-proxy-owner"));
        auto* E=F.Get(TEXT("Probe359Expected"));F.Check(F.And(F.IsValid(E),SameObject(F,E,F.Item(IC,EItemCategory::SecondaryWeapon))),TEXT("reference-retry-rejected-stored-native-slot"));
        auto* Recovery=F.Branch(F.Eq(Phase,F.Int(91)));auto* OriginalProof=F.Call(Proxy->GeneratedClass,TEXT("DOR359_OriginalRuntimeProof"));Connect(SelfPin(F.G,0,700),FindPin(OriginalProof,UEdGraphSchema_K2::PN_Self));F.Check(OriginalProof->GetReturnValuePin(),TEXT("reference-retry-rejected-original-proof"));auto* RT=F.Tail;F.Tail=Recovery->GetElsePin();F.Check(F.SameArray(CurrentSelections(F),F.Get(TEXT("Probe359FinalSelections"))),TEXT("reference-retry-rejected-selections"));F.Check(UntouchedLoadout(F,CurrentLoadout(F),F.Get(TEXT("Probe359OriginalLoadout"))),TEXT("reference-retry-rejected-untouched"));F.Check(SelectedPair(F,F.Cache(TEXT("RetryLiveSelections"),CurrentSelections(F))),TEXT("reference-retry-rejected-root"));F.Check(SameGuid(F,F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),F.FieldOf(F.Get(TEXT("Probe359RequestedLoadout")),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon"))),TEXT("reference-retry-rejected-requested-loadout"));auto* Temp=F.Branch(F.Eq(Phase,F.Int(2)));F.Check(OwnCanonical(F,E,F.Get(TEXT("Probe359AlternateGuid"))),TEXT("reference-retry-rejected-temp-ownership"));FreshActor(F,E,F.Get(TEXT("Probe359AlternateClass")),false,nullptr);auto* TT=F.Tail;F.Tail=Temp->GetElsePin();F.Check(OwnCanonical(F,E,F.Get(TEXT("Probe359PistolGuid"))),TEXT("reference-retry-rejected-final-ownership"));FreshActor(F,E,F.Get(TEXT("Probe359PistolClass")),true,nullptr);ComponentEvidence(F,E,nullptr);
        auto* Sent=AddVariableSet(F.G,TEXT("Probe359RetrySent"),0,0);FindPin(Sent,TEXT("Probe359RetrySent"))->DefaultValue=TEXT("true");F.Run(Sent);Connect(RT,Sent->GetExecPin());Connect(TT,Sent->GetExecPin());F.Log(TEXT("reference-retry-sent-same-stored-object"),F.ReferenceFields(E,P));auto* N=F.Self(TEXT("DOR359_ClientReferenceRetry"));F.Tuple(N,0);Connect(Phase,FindPin(N,TEXT("Phase")));Connect(E,FindPin(N,TEXT("Expected")));F.Compile();Wire(RetryRequest,TEXT("DOR359_ReplayReferenceAuthority"));
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_ReplayReferenceOwner"),Master->GeneratedClass);auto* E=F.Input(TEXT("Expected"),ItemType);auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());F.Log(TEXT("raw-retry-reference-before-filter"),F.Join(DORSafeConcat(F,TEXT("messagePhase="),F.Text(Phase,TEXT("Conv_IntToString"))),F.ReferenceFields(E,F.Get(TEXT("Probe359Player")))));Correlate(F,false);F.Check(F.And(ReplayPhase(F),F.Eq(Phase,F.Get(TEXT("Probe359Phase")))),TEXT("reference-replay-rejected-phase"));F.Check(ReplayUnexpired(F),TEXT("reference-replay-rejected-original-deadline"));F.Check(F.And(F.Get(TEXT("Probe359RetryRequested")),F.Not(F.Get(TEXT("Probe359RetryReceived")))),TEXT("reference-replay-rejected-unrequested-or-used"));auto* Null=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ObjectObject"));Connect(F.Get(TEXT("Probe359Expected")),FindPin(Null,TEXT("A")));F.Check(Null->GetReturnValuePin(),TEXT("reference-replay-rejected-nonnull"));F.Set(TEXT("Probe359RetryReceived"),nullptr,TEXT("true"));F.Set(TEXT("Probe359Expected"),E);F.Log(TEXT("reference-replay-accepted-no-stage-reset"));F.Compile();Wire(RetryReceive,TEXT("DOR359_ReplayReferenceOwner"));
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_Finish"),Master->GeneratedClass);
        F.Log(TEXT("terminal-probe-no-consumption"),F.Get(TEXT("Probe359Failure")));
        // Settlement is deliberately false even when construction passed: this is
        // a non-consuming probe, and native gameplay still needs observation.
        auto* M=F.Get(TEXT("Probe359Master"));
        F.Put(M,Master->GeneratedClass,TEXT("RemoteLastSucceeded"),nullptr,TEXT("false"));
        F.Put(M,Master->GeneratedClass,TEXT("RemoteLastTestIndex"),nullptr,TEXT("359"));
        F.Put(M,Master->GeneratedClass,TEXT("RemoteLastGeneration"),F.Get(TEXT("Probe359Generation")));
        F.Put(M,Master->GeneratedClass,TEXT("RemoteLastPlayerKey"),F.Get(TEXT("Probe359Key")));
        F.Put(M,Master->GeneratedClass,TEXT("RemoteCurrentStage"),nullptr,TEXT("probe-settled-no-consumption"));
        F.Put(M,Master->GeneratedClass,TEXT("RemoteAuthorityDispatchLocked"),nullptr,TEXT("false"));
        F.Set(TEXT("PendingReplayRecord"),nullptr,TEXT("false"));F.Set(TEXT("Probe359Phase"),nullptr,TEXT("0"));
        auto* N=F.Self(TEXT("DOR359_ClientCleanup"));F.Tuple(N,0);FindPin(N,TEXT("Succeeded"))->DefaultValue=TEXT("false");F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_Abort"),Master->GeneratedClass);
        auto* Reason=F.Input(TEXT("Reason"),MakeStringPinType());
        auto* Earlier=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));Connect(F.Get(TEXT("Probe359Phase")),FindPin(Earlier,TEXT("A")));FindPin(Earlier,TEXT("B"))->DefaultValue=TEXT("90");F.Check(Earlier->GetReturnValuePin(),TEXT("ignored-duplicate-abort"));
        F.Set(TEXT("Probe359Failure"),Reason);F.Set(TEXT("Probe359ConstructionPassed"),nullptr,TEXT("false"));F.Set(TEXT("Probe359Phase"),nullptr,TEXT("90"));F.Deadline();F.Log(TEXT("owner-cleanup-requested"),Reason);
        auto* N=F.Self(TEXT("DOR359_ClientCleanup"));F.Tuple(N,90);FindPin(N,TEXT("Succeeded"))->DefaultValue=TEXT("false");F.Timer(TEXT("DOR359_ServerWatch"));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_Start"),Master->GeneratedClass);
        F.Set(TEXT("Probe359Master"),F.Get(TEXT("MasterRef")));
        F.Check(F.Eq(F.Get(TEXT("RemoteTestIndex")),F.Int(359)),TEXT("wrong-test-start"));
        F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(0)),TEXT("ignored-duplicate-start"));
        auto* Newer=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(F.Get(TEXT("RemoteGeneration")),FindPin(Newer,TEXT("A")));Connect(F.Get(TEXT("Probe359Generation")),FindPin(Newer,TEXT("B")));F.Check(Newer->GetReturnValuePin(),TEXT("ignored-stale-server-start"));
        F.Check(F.Not(F.Field(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("RemoteAuthorityDispatchLocked"))),TEXT("authority-busy-before-owner-mutation"));
        F.Check(F.Not(F.Field(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("ExternalPresetTransactionActive"))),TEXT("host-local-transaction-busy"));
        F.Set(TEXT("Probe359Key"),F.Get(TEXT("RemoteRequestPlayerKey")));F.Set(TEXT("Probe359Generation"),F.Get(TEXT("RemoteGeneration")));
        InitNative(F,F.Get(TEXT("OwningRewardPlayer")));
        auto* InventoryOwner=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));Connect(F.Get(TEXT("Probe359Inventory")),FindPin(InventoryOwner,UEdGraphSchema_K2::PN_Self));F.Check(SameObject(F,InventoryOwner->GetReturnValuePin(),F.Get(TEXT("Probe359Player"))),TEXT("inventory-owner-mismatch"));
        auto* Authority=F.Call(AActor::StaticClass(),TEXT("HasAuthority"));Connect(InventoryOwner->GetReturnValuePin(),FindPin(Authority,UEdGraphSchema_K2::PN_Self));F.Check(Authority->GetReturnValuePin(),TEXT("inventory-owner-not-authority"));
        auto* Role=F.Call(AActor::StaticClass(),TEXT("GetLocalRole"));Connect(InventoryOwner->GetReturnValuePin(),FindPin(Role,UEdGraphSchema_K2::PN_Self));auto* RoleText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_ByteToString"));Connect(Role->GetReturnValuePin(),FindPin(RoleText,TEXT("InByte")));F.Log(TEXT("inventory-owner-authority-confirmed-net-role"),RoleText->GetReturnValuePin());
        auto* ID=F.Call(Master->GeneratedClass,TEXT("DOR_ResolveStablePlayerKey"));Connect(F.Get(TEXT("Probe359Master")),FindPin(ID,UEdGraphSchema_K2::PN_Self));Connect(F.Get(TEXT("Probe359Player")),FindPin(ID,TEXT("Player")));F.Check(F.And(FindPin(ID,TEXT("Ready")),F.Eq(FindPin(ID,TEXT("Key")),F.Get(TEXT("Probe359Key")),TEXT("EqualEqual_StrStr"))),TEXT("start-identity-mismatch"));
        auto* C=F.Call(APawn::StaticClass(),TEXT("GetController"));Connect(F.Get(TEXT("Probe359Player")),FindPin(C,UEdGraphSchema_K2::PN_Self));auto* O=F.Call(AActor::StaticClass(),TEXT("GetOwner"));F.Check(SameObject(F,C->GetReturnValuePin(),O->GetReturnValuePin()),TEXT("start-owner-mismatch"));
        F.Check(SameGuid(F,F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),F.Get(TEXT("Probe359PistolGuid"))),TEXT("start-secondary-not-pistol"));
        Snapshot(F);F.Check(F.IsValid(F.Get(TEXT("Probe359PreSecondary"))),TEXT("start-original-actor-invalid"));
        F.Put(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("RemoteAuthorityDispatchLocked"),nullptr,TEXT("true"));
        F.Set(TEXT("PendingReplayRecord"),nullptr,TEXT("false"));F.Set(TEXT("Probe359ConstructionPassed"),nullptr,TEXT("false"));F.Set(TEXT("Probe359Failure"),nullptr,TEXT("pending"));F.Set(TEXT("Probe359Phase"),nullptr,TEXT("1"));F.Deadline();F.Log(TEXT("authority-lock-acquired-before-owner-prep"));
        ActorEvidence(F,F.Get(TEXT("Probe359PrePrimary")),TEXT("server-pre-primary"));ActorEvidence(F,F.Get(TEXT("Probe359PreSecondary")),TEXT("server-pre-secondary"));
        auto* N=F.Self(TEXT("DOR359_ClientBegin"));F.Tuple(N,1);Connect(F.Get(TEXT("Probe359Player")),FindPin(N,TEXT("Player")));F.Timer(TEXT("DOR359_ServerWatch"));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_BeginOwner"),Master->GeneratedClass);
        auto* Generation=F.Input(TEXT("Generation"),MakeIntPinType());auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* P=F.Input(TEXT("Player"),PlayerType);
        auto* All=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(All,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;Connect(SelfPin(F.G,0,700),FindPin(All,TEXT("WorldContextObject")));F.Set(TEXT("Probe359Master"),F.Cast(All->GetReturnValuePin(),Master->GeneratedClass));F.Check(F.IsValid(F.Get(TEXT("Probe359Master"))),TEXT("owner-master-missing"));
        auto* Local=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"));Connect(SelfPin(F.G,0,700),FindPin(Local,TEXT("WorldContextObject")));F.Check(SameObject(F,P,Local->GetReturnValuePin()),TEXT("owner-local-player-mismatch"));
        F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(0)),TEXT("owner-already-pending"));F.Check(F.Not(F.Field(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("ExternalPresetTransactionActive"))),TEXT("owner-transaction-busy"));
        auto* Newer=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Generation,FindPin(Newer,TEXT("A")));Connect(F.Get(TEXT("Probe359Generation")),FindPin(Newer,TEXT("B")));F.Check(Newer->GetReturnValuePin(),TEXT("ignored-stale-owner-begin"));
        F.Set(TEXT("Probe359Generation"),Generation);F.Set(TEXT("Probe359Key"),Key);F.Set(TEXT("RemoteRequestPlayerKey"),Key);F.Set(TEXT("Probe359Phase"),nullptr,TEXT("1"));
        InitNative(F,P);Snapshot(F);F.Set(TEXT("Probe359ReplayBaseline"),F.Get(TEXT("Probe359PreSecondary")));F.Deadline();F.Log(TEXT("owner-original-snapshot"));
        ActorEvidence(F,F.Get(TEXT("Probe359PrePrimary")),TEXT("owner-pre-primary"));ActorEvidence(F,F.Get(TEXT("Probe359PreSecondary")),TEXT("owner-pre-secondary"));
        F.Put(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("Probe359Proxy"),SelfPin(F.G,0,700));F.Put(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("Probe359OwnerActive"),nullptr,TEXT("true"));
        auto* N=F.Self(TEXT("DOR_ClientApplyRemotePair"));FindPin(N,TEXT("TestIndex"))->DefaultValue=TEXT("359");Connect(Generation,FindPin(N,TEXT("Generation")));F.Timer(TEXT("DOR359_OwnerWatch"));F.Compile();Wire(Begin,TEXT("DOR359_BeginOwner"));
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_SendPrepared"),Master->GeneratedClass);
        F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(1)),TEXT("ignored-wrong-preparation-phase"));
        F.Set(TEXT("Probe359FinalLoadout"),CurrentLoadout(F));F.Set(TEXT("Probe359FinalSelections"),CurrentSelections(F));
        F.Check(UntouchedLoadout(F,CurrentLoadout(F),F.Get(TEXT("Probe359OriginalLoadout"))),TEXT("owner-prep-untouched-loadout-changed"));
        F.Check(UntouchedSelections(F,CurrentSelections(F),F.Get(TEXT("Probe359OriginalSelections"))),TEXT("owner-prep-untouched-selections-changed"));
        auto* RootMatched=SelectedPair(F,CurrentSelections(F));F.Log(TEXT("owner-prepared-selected-root-only"),F.Text(RootMatched,TEXT("Conv_BoolToString")));F.Check(RootMatched,TEXT("owner-prepared-root-mismatch"));
        auto* N=F.Self(TEXT("DOR359_ServerPrepared"));F.Tuple(N,1);Connect(F.Get(TEXT("Probe359FinalLoadout")),FindPin(N,TEXT("Loadout")));Connect(F.Get(TEXT("Probe359FinalSelections")),FindPin(N,TEXT("Selections")));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_PreparedAuthority"),Master->GeneratedClass);Correlate(F,true);
        auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());auto* L=F.Input(TEXT("Loadout"),LoadoutType);auto* S=F.Input(TEXT("Selections"),SelectionsType);
        F.Check(F.And(F.Eq(Phase,F.Int(1)),F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(1))),TEXT("ignored-wrong-prepared-phase"));
        F.Check(UntouchedLoadout(F,L,F.Get(TEXT("Probe359OriginalLoadout"))),TEXT("owner-untouched-loadout-mismatch"),TEXT("DOR359_Abort"));
        F.Check(UntouchedSelections(F,S,F.Get(TEXT("Probe359OriginalSelections"))),TEXT("owner-untouched-selections-mismatch"),TEXT("DOR359_Abort"));
        F.Check(SameGuid(F,F.FieldOf(L,FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),F.Get(TEXT("Probe359PistolGuid"))),TEXT("prepared-secondary-not-pistol"),TEXT("DOR359_Abort"));
        F.Check(SelectedPair(F,S),TEXT("prepared-selected-root-rejected"),TEXT("DOR359_Abort"));
        F.Set(TEXT("Probe359FinalLoadout"),L);F.Set(TEXT("Probe359FinalSelections"),S);F.Set(TEXT("Probe359Phase"),nullptr,TEXT("2"));
        F.Self(TEXT("DOR359_AuthorityStep"));F.Compile();Wire(Prepared,TEXT("DOR359_PreparedAuthority"));
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_AuthorityStep"),Master->GeneratedClass);
        auto* Temp=F.Branch(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(2)));
        F.Set(TEXT("Probe359RequestedLoadout"),F.Patch(F.Get(TEXT("Probe359FinalLoadout")),FItemLoadout::StaticStruct(),{{TEXT("SecondaryWeapon"),F.Get(TEXT("Probe359AlternateGuid"))}}));auto* TempTail=F.Tail;
        F.Tail=Temp->GetElsePin();F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(4)),TEXT("ignored-wrong-construction-phase"));F.Set(TEXT("Probe359RequestedLoadout"),F.Get(TEXT("Probe359FinalLoadout")));auto* FinalTail=F.Tail;
        auto* Before=F.Self(TEXT("DOR359_SelectedPair"));Connect(F.Get(TEXT("Probe359FinalSelections")),FindPin(Before,TEXT("Selections")));Connect(TempTail,Before->GetExecPin());Connect(FinalTail,Before->GetExecPin());F.Tail=Before->GetThenPin();
        F.Log(TEXT("server-before-setter-selected-root-only"),F.Text(F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),TEXT("Conv_GuidToString")));
        NativeSet(F,F.Get(TEXT("Probe359RequestedLoadout")),F.Get(TEXT("Probe359FinalSelections")));
        F.Log(TEXT("server-after-setter"),F.Text(F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),TEXT("Conv_GuidToString")));
        F.Check(SameGuid(F,F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),F.FieldOf(F.Get(TEXT("Probe359RequestedLoadout")),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon"))),TEXT("native-setter-guid-rejected"),TEXT("DOR359_Abort"));
        F.Check(F.SameArray(CurrentSelections(F),F.Get(TEXT("Probe359FinalSelections"))),TEXT("native-setter-selections-rejected"),TEXT("DOR359_Abort"));
        F.Check(UntouchedLoadout(F,CurrentLoadout(F),F.Get(TEXT("Probe359OriginalLoadout"))),TEXT("native-setter-untouched-loadout-changed"),TEXT("DOR359_Abort"));
        auto* Auth=F.Call(AActor::StaticClass(),TEXT("HasAuthority"));Connect(F.Get(TEXT("Probe359Player")),FindPin(Auth,UEdGraphSchema_K2::PN_Self));F.Check(Auth->GetReturnValuePin(),TEXT("refresh-not-authority"),TEXT("DOR359_Abort"));
        Refresh(F);F.Set(TEXT("Probe359Expected"),F.Item(IC,EItemCategory::SecondaryWeapon));
        F.Log(TEXT("server-after-authority-refresh"),F.Text(F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),TEXT("Conv_GuidToString")));
        F.Check(F.SameArray(CurrentSelections(F),F.Get(TEXT("Probe359FinalSelections"))),TEXT("refresh-selected-state-changed"),TEXT("DOR359_Abort"));
        auto* T=F.Branch(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(2)));
        FreshActor(F,F.Get(TEXT("Probe359Expected")),F.Get(TEXT("Probe359AlternateClass")),false,TEXT("DOR359_Abort"));F.Set(TEXT("Probe359Temporary"),F.Get(TEXT("Probe359Expected")));auto* TempPass=F.Tail;
        F.Tail=T->GetElsePin();FreshActor(F,F.Get(TEXT("Probe359Expected")),F.Get(TEXT("Probe359PistolClass")),true,TEXT("DOR359_Abort"));ComponentEvidence(F,F.Get(TEXT("Probe359Expected")),TEXT("DOR359_Abort"));F.Check(SelectedPair(F,CurrentSelections(F)),TEXT("final-selected-root-mismatch"),TEXT("DOR359_Abort"));F.Set(TEXT("Probe359Final"),F.Get(TEXT("Probe359Expected")));
        auto* First=F.Self(TEXT("DOR359_FirstReferenceSend"));Connect(TempPass,First->GetExecPin());auto* N=F.Self(TEXT("DOR359_ClientObserve"));F.Tuple(N,0);Connect(F.Get(TEXT("Probe359Phase")),FindPin(N,TEXT("Phase")));Connect(F.Get(TEXT("Probe359Expected")),FindPin(N,TEXT("Expected")));F.Timer(TEXT("DOR359_ServerWatch"));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_ObserveOwner"),Master->GeneratedClass);
        auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());auto* Expected=F.Input(TEXT("Expected"),ItemType);
        F.Log(TEXT("raw-first-reference-before-filter"),F.Join(DORSafeConcat(F,TEXT("messagePhase="),F.Text(Phase,TEXT("Conv_IntToString"))),F.ReferenceFields(Expected,F.Get(TEXT("Probe359Player")))));Correlate(F,false);
        auto* Temp=F.Eq(Phase,F.Int(2));auto* Final=F.Eq(Phase,F.Int(4));auto* Rollback=F.Eq(Phase,F.Int(91));auto* Or=F.Eq(Temp,Final,TEXT("BooleanOR"));
        F.Check(F.Eq(Or,Rollback,TEXT("BooleanOR")),TEXT("ignored-unknown-observation-phase"));
        // Reliable messages on this proxy are ordered; accept only the next phase.
        auto* T=F.Branch(Temp);F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(1)),TEXT("ignored-temp-out-of-order"));auto* TP=F.Tail;
        F.Tail=T->GetElsePin();auto* Fin=F.Branch(Final);F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(3)),TEXT("ignored-final-out-of-order"));auto* FP=F.Tail;
        F.Tail=Fin->GetElsePin();F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(90)),TEXT("ignored-rollback-out-of-order"));
        auto* Store=AddVariableSet(F.G,TEXT("Probe359Expected"),0,0);Connect(Expected,FindPin(Store,TEXT("Probe359Expected")));F.Run(Store);Connect(TP,Store->GetExecPin());Connect(FP,Store->GetExecPin());
        F.Set(TEXT("Probe359Phase"),Phase);F.Set(TEXT("Probe359RetryRequested"),nullptr,TEXT("false"));F.Set(TEXT("Probe359RetryReceived"),nullptr,TEXT("false"));F.Deadline();F.Log(TEXT("owner-expected-reference-received"));F.Self(TEXT("DOR359_ReferenceDiagnostic"));F.Timer(TEXT("DOR359_OwnerWatch"));F.Compile();Wire(Observe,TEXT("DOR359_ObserveOwner"));
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_CleanupOwner"),Master->GeneratedClass);Correlate(F,false);
        auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());auto* Succeeded=F.Input(TEXT("Succeeded"),MakeBoolPinType());
        auto* End=F.Branch(F.Eq(Phase,F.Int(0)));F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(6),TEXT("EqualEqual_IntInt")),TEXT("ignored-unfinished-terminal-cleanup"));F.Put(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("Probe359OwnerActive"),nullptr,TEXT("false"));F.Put(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("Probe359Proxy"),nullptr);F.Set(TEXT("Probe359Phase"),nullptr,TEXT("0"));F.Log(TEXT("owner-terminal"));F.Done();F.Tail=End->GetElsePin();
        auto* Success=F.Eq(Phase,F.Int(5));auto* Failure=F.Eq(Phase,F.Int(90));F.Check(F.Eq(Success,Failure,TEXT("BooleanOR")),TEXT("ignored-unknown-cleanup-phase"));
        auto* Valid=F.Branch(Success);F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(5)),TEXT("ignored-success-cleanup-phase"));auto* ST=F.Tail;F.Tail=Valid->GetElsePin();
        auto* NotTerminal=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));Connect(F.Get(TEXT("Probe359Phase")),FindPin(NotTerminal,TEXT("A")));FindPin(NotTerminal,TEXT("B"))->DefaultValue=TEXT("90");F.Check(NotTerminal->GetReturnValuePin(),TEXT("ignored-duplicate-failure-cleanup"));F.Set(TEXT("Probe359ReplayBaseline"),F.Item(IC,EItemCategory::SecondaryWeapon));
        auto* Set=AddVariableSet(F.G,TEXT("Probe359Phase"),0,0);Connect(Phase,FindPin(Set,TEXT("Probe359Phase")));F.Run(Set);Connect(ST,Set->GetExecPin());
        F.Put(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("TransientApplySucceeded"),Succeeded);
        F.Log(TEXT("owner-checked-restore-requested"));
        auto* Restore=F.Call(Master->GeneratedClass,TEXT("DOR_SchedulePair388PresetRestore"));Connect(F.Get(TEXT("Probe359Master")),FindPin(Restore,UEdGraphSchema_K2::PN_Self));FindPin(Restore,TEXT("RebuildCurrent"))->DefaultValue=TEXT("false");
        F.Deadline();F.Timer(TEXT("DOR359_OwnerWatch"));F.Compile();Wire(Cleanup,TEXT("DOR359_CleanupOwner"));
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_OwnerWatch"),Master->GeneratedClass);
        auto* Phase=F.Get(TEXT("Probe359Phase"));auto* Pending=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Phase,FindPin(Pending,TEXT("A")));F.Check(Pending->GetReturnValuePin(),TEXT("owner-watch-inactive"));
        auto* Completed=F.Branch(F.Eq(Phase,F.Int(6)));F.Done();F.Tail=Completed->GetElsePin();
        auto* Active=F.Field(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("ExternalPresetTransactionActive"));
        auto* SuccessRestore=F.Eq(Phase,F.Int(5));auto* FailureRestore=F.Eq(Phase,F.Int(90));auto* Restore=F.Branch(F.Eq(SuccessRestore,FailureRestore,TEXT("BooleanOR")));
        auto* Wait=F.Branch(Active);auto* WaitTail=F.Tail;F.Tail=Wait->GetElsePin();
        F.Log(TEXT("owner-restore-settled"),F.Text(F.Field(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("TransientApplySucceeded")),TEXT("Conv_BoolToString")));
        auto* N=F.Self(TEXT("DOR359_ServerAck"));F.Tuple(N,0);Connect(Phase,FindPin(N,TEXT("Phase")));Connect(F.Not(Active),FindPin(N,TEXT("Succeeded")));
        // Success also requires the existing checked restore to retain true.
        FindPin(N,TEXT("Succeeded"))->BreakAllPinLinks();auto* RestoreOk=F.Eq(F.Not(SuccessRestore),F.Field(F.Get(TEXT("Probe359Master")),Master->GeneratedClass,TEXT("TransientApplySucceeded")),TEXT("BooleanOR"));Connect(RestoreOk,FindPin(N,TEXT("Succeeded")));
        auto* Final=F.Branch(SuccessRestore);F.Set(TEXT("Probe359Phase"),nullptr,TEXT("6"));F.Done();F.Tail=Final->GetElsePin();F.Done();
        F.Tail=Restore->GetElsePin();auto* Preparation=F.Branch(F.Eq(Phase,F.Int(1)));
        auto* PrepDone=F.Branch(F.Not(Active));auto* Cancel=F.Self(TEXT("DOR359_ServerAck"));F.Tuple(Cancel,1);FindPin(Cancel,TEXT("Succeeded"))->DefaultValue=TEXT("false");F.Set(TEXT("Probe359Phase"),nullptr,TEXT("3"));F.Done();F.Tail=PrepDone->GetElsePin();auto* PrepWait=F.Tail;
        F.Tail=Preparation->GetElsePin();auto* AwaitNext=F.Branch(F.Eq(Phase,F.Int(3)));auto* NextWait=F.Tail;F.Tail=AwaitNext->GetElsePin();
        auto* Match=F.Branch(F.And(F.IsValid(F.Get(TEXT("Probe359Expected"))),SameObject(F,F.Item(IC,EItemCategory::SecondaryWeapon),F.Get(TEXT("Probe359Expected")))));auto* ReferencePending=Match->GetElsePin();
        F.Log(TEXT("owner-observed-authority-reference"));
        auto* Rollback=F.Branch(F.Eq(Phase,F.Int(91)));
        F.Check(F.Not(Active),TEXT("rollback-owner-restore-incomplete"));auto* RollbackClass=ActorEvidence(F,F.Get(TEXT("Probe359Expected")),TEXT("owner-rollback-actor"));F.Check(F.Eq(RollbackClass,F.Get(TEXT("Probe359PistolClass")),TEXT("EqualEqual_ClassClass")),TEXT("owner-rollback-class-mismatch"));F.Check(SameObject(F,F.Item(IC,EItemCategory::PrimaryWeapon),F.Get(TEXT("Probe359PrePrimary"))),TEXT("owner-rollback-primary-changed"));auto* RollbackCount=LiveCount(F,F.Get(TEXT("Probe359Expected")));F.Log(TEXT("owner-rollback-live-component-count"),F.Text(RollbackCount,TEXT("Conv_IntToString")));F.Check(F.Eq(RollbackCount,F.Get(TEXT("Probe359PreExplosiveCount"))),TEXT("owner-rollback-component-not-restored"));
        auto* OwnerProof=F.Call(Proxy->GeneratedClass,TEXT("DOR359_OriginalRuntimeProof"));Connect(SelfPin(F.G,0,700),FindPin(OwnerProof,UEdGraphSchema_K2::PN_Self));F.Check(OwnerProof->GetReturnValuePin(),TEXT("owner-rollback-native-proof-incomplete"));
        auto* RA=F.Self(TEXT("DOR359_ServerAck"));F.Tuple(RA,91);FindPin(RA,TEXT("Succeeded"))->DefaultValue=TEXT("true");F.Set(TEXT("Probe359Phase"),nullptr,TEXT("6"));F.Done();
        F.Tail=Rollback->GetElsePin();auto* Temp=F.Branch(F.Eq(Phase,F.Int(2)));
        FreshActor(F,F.Get(TEXT("Probe359Expected")),F.Get(TEXT("Probe359AlternateClass")),false,nullptr);F.Set(TEXT("Probe359Temporary"),F.Get(TEXT("Probe359Expected")));auto* TA=F.Self(TEXT("DOR359_ServerAck"));F.Tuple(TA,2);FindPin(TA,TEXT("Succeeded"))->DefaultValue=TEXT("true");F.Set(TEXT("Probe359Phase"),nullptr,TEXT("3"));F.Deadline();F.Timer(TEXT("DOR359_OwnerWatch"));F.Done();
        F.Tail=Temp->GetElsePin();F.Check(F.Eq(Phase,F.Int(4)),TEXT("ignored-owner-watch-phase"));FreshActor(F,F.Get(TEXT("Probe359Expected")),F.Get(TEXT("Probe359PistolClass")),true,nullptr);ComponentEvidence(F,F.Get(TEXT("Probe359Expected")),nullptr);
        auto* OwnerRoot=SelectedPair(F,CurrentSelections(F));F.Log(TEXT("owner-final-selected-root-only"),F.Text(OwnerRoot,TEXT("Conv_BoolToString")));F.Check(OwnerRoot,TEXT("owner-final-selected-root-mismatch"));
        F.Check(UntouchedSelections(F,CurrentSelections(F),F.Get(TEXT("Probe359OriginalSelections"))),TEXT("owner-final-untouched-selections-changed"));
        auto* FA=F.Self(TEXT("DOR359_ServerAck"));F.Tuple(FA,4);FindPin(FA,TEXT("Succeeded"))->DefaultValue=TEXT("true");F.Set(TEXT("Probe359Phase"),nullptr,TEXT("5"));F.Deadline();F.Timer(TEXT("DOR359_OwnerWatch"));F.Done();
        F.Tail=ReferencePending;F.Self(TEXT("DOR359_RequestReferenceRetry"));auto* Timeout=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_FloatFloat"));Connect(F.Now(),FindPin(Timeout,TEXT("A")));Connect(F.Get(TEXT("Probe359Deadline")),FindPin(Timeout,TEXT("B")));auto* Expired=F.Branch(Timeout->GetReturnValuePin());for(auto* T:{WaitTail,PrepWait,NextWait})Connect(T,Expired->GetExecPin());
        F.Log(TEXT("owner-deadline-reference-or-restore"));F.Self(TEXT("DOR359_ReferenceDiagnostic"));auto* Fail=F.Self(TEXT("DOR359_ServerAck"));F.Tuple(Fail,0);Connect(F.Get(TEXT("Probe359Phase")),FindPin(Fail,TEXT("Phase")));FindPin(Fail,TEXT("Succeeded"))->DefaultValue=TEXT("false");F.Done();F.Tail=Expired->GetElsePin();
        F.Timer(TEXT("DOR359_OwnerWatch"));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_AcknowledgeAuthority"),Master->GeneratedClass);Correlate(F,true);
        auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());auto* Success=F.Input(TEXT("Succeeded"),MakeBoolPinType());
        F.Check(F.Eq(Phase,F.Get(TEXT("Probe359Phase"))),TEXT("ignored-wrong-ack-phase"));
        auto* Restore=F.Branch(F.Eq(Phase,F.Int(90)));F.Log(TEXT("owner-cleanup-ack"));F.Timer(TEXT("DOR359_Rollback"));F.Done();F.Tail=Restore->GetElsePin();
        auto* Result=F.Branch(Success);F.Tail=Result->GetElsePin();auto* Abort=F.Self(TEXT("DOR359_Abort"));FindPin(Abort,TEXT("Reason"))->DefaultValue=TEXT("owner-phase-failed");F.Done();F.Tail=Result->GetThenPin();
        auto* Rollback=F.Branch(F.Eq(Phase,F.Int(91)));auto* Proof=F.Call(Proxy->GeneratedClass,TEXT("DOR359_OriginalRuntimeProof"));Connect(SelfPin(F.G,0,700),FindPin(Proof,UEdGraphSchema_K2::PN_Self));F.Check(Proof->GetReturnValuePin(),TEXT("rollback-current-proof-lost-lock-retained"));F.Check(SameObject(F,F.Item(IC,EItemCategory::SecondaryWeapon),F.Get(TEXT("Probe359Expected"))),TEXT("rollback-expected-reference-changed-lock-retained"));F.Self(TEXT("DOR359_Finish"));F.Done();F.Tail=Rollback->GetElsePin();
        auto* CheckedRestore=F.Branch(F.Eq(Phase,F.Int(5)));F.Set(TEXT("Probe359Failure"),nullptr,TEXT("construction-and-checked-owner-restore-passed-gameplay-unconfirmed"));F.Self(TEXT("DOR359_Finish"));F.Done();F.Tail=CheckedRestore->GetElsePin();
        auto* Temp=F.Branch(F.Eq(Phase,F.Int(2)));F.Set(TEXT("Probe359Phase"),nullptr,TEXT("4"));F.Self(TEXT("DOR359_AuthorityStep"));F.Done();F.Tail=Temp->GetElsePin();
        F.Check(F.Eq(Phase,F.Int(4)),TEXT("ignored-unsupported-success-phase"));F.Set(TEXT("Probe359ConstructionPassed"),nullptr,TEXT("true"));F.Set(TEXT("Probe359Phase"),nullptr,TEXT("5"));F.Deadline();F.Log(TEXT("both-peers-constructed-awaiting-checked-owner-restore"));auto* N=F.Self(TEXT("DOR359_ClientCleanup"));F.Tuple(N,5);FindPin(N,TEXT("Succeeded"))->DefaultValue=TEXT("true");F.Timer(TEXT("DOR359_ServerWatch"));F.Compile();Wire(Ack,TEXT("DOR359_AcknowledgeAuthority"));
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_Rollback"),Master->GeneratedClass);
        F.Check(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(90)),TEXT("ignored-wrong-rollback-phase"));
        F.Check(F.And(F.IsValid(F.Get(TEXT("Probe359Inventory"))),F.IsValid(F.Get(TEXT("Probe359SaveState")))),TEXT("rollback-context-lost-lock-retained"));
        F.Set(TEXT("Probe359Phase"),nullptr,TEXT("91"));F.Deadline();F.Timer(TEXT("DOR359_ServerWatch"));F.Log(TEXT("authority-rollback-start"));
        F.Set(TEXT("Probe359RollbackPre"),F.Item(IC,EItemCategory::SecondaryWeapon));
        // Always try the original payload first. Already-alternate/native-no-op
        // outcomes must never prevent this attempt; identity is not recovery proof.
        NativeSet(F,F.Get(TEXT("Probe359OriginalLoadout")),F.Get(TEXT("Probe359OriginalSelections")));Refresh(F);
        auto* FirstProof=F.Call(Proxy->GeneratedClass,TEXT("DOR359_OriginalRuntimeProof"));Connect(SelfPin(F.G,0,700),FindPin(FirstProof,UEdGraphSchema_K2::PN_Self));
        auto* Original=F.Branch(FirstProof->GetReturnValuePin());auto* AlreadyRestored=F.Tail;F.Tail=Original->GetElsePin();
        auto* TemporaryLoadout=F.Patch(F.Get(TEXT("Probe359OriginalLoadout")),FItemLoadout::StaticStruct(),{{TEXT("SecondaryWeapon"),F.Get(TEXT("Probe359AlternateGuid"))}});
        NativeSet(F,TemporaryLoadout,F.Get(TEXT("Probe359OriginalSelections")));Refresh(F);F.Set(TEXT("Probe359RollbackTemporary"),F.Item(IC,EItemCategory::SecondaryWeapon));
        NativeSet(F,F.Get(TEXT("Probe359OriginalLoadout")),F.Get(TEXT("Probe359OriginalSelections")));Refresh(F);
        auto* Store=AddVariableSet(F.G,TEXT("Probe359Expected"),0,0);Connect(F.Item(IC,EItemCategory::SecondaryWeapon),FindPin(Store,TEXT("Probe359Expected")));F.Run(Store);Connect(AlreadyRestored,Store->GetExecPin());
        F.Check(F.SameArray(CurrentSelections(F),F.Get(TEXT("Probe359OriginalSelections"))),TEXT("rollback-selection-readback-failed-lock-retained"));
        F.Check(SameGuid(F,F.FieldOf(CurrentLoadout(F),FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),F.Get(TEXT("Probe359PistolGuid"))),TEXT("rollback-guid-failed-lock-retained"));
        F.Check(F.IsValid(F.Get(TEXT("Probe359Expected"))),TEXT("rollback-actor-invalid-lock-retained"));auto* Class=ActorEvidence(F,F.Get(TEXT("Probe359Expected")),TEXT("server-rollback-actor"));F.Check(F.Eq(Class,F.Get(TEXT("Probe359PistolClass")),TEXT("EqualEqual_ClassClass")),TEXT("rollback-class-failed-lock-retained"));
        auto* Proof=F.Call(Proxy->GeneratedClass,TEXT("DOR359_OriginalRuntimeProof"));Connect(SelfPin(F.G,0,700),FindPin(Proof,UEdGraphSchema_K2::PN_Self));F.Check(Proof->GetReturnValuePin(),TEXT("rollback-original-runtime-proof-failed-lock-retained"));
        auto* Count=LiveCount(F,F.Get(TEXT("Probe359Expected")));F.Log(TEXT("server-rollback-live-component-count"),F.Text(Count,TEXT("Conv_IntToString")));F.Check(F.Eq(Count,F.Get(TEXT("Probe359PreExplosiveCount"))),TEXT("rollback-component-not-restored-lock-retained"));
        F.Check(UntouchedLoadout(F,CurrentLoadout(F),F.Get(TEXT("Probe359OriginalLoadout"))),TEXT("rollback-untouched-loadout-changed-lock-retained"));
        F.Check(SameObject(F,F.Item(IC,EItemCategory::PrimaryWeapon),F.Get(TEXT("Probe359PrePrimary"))),TEXT("rollback-primary-changed-lock-retained"));
        F.Self(TEXT("DOR359_FirstReferenceSend"));auto* N=F.Self(TEXT("DOR359_ClientObserve"));F.Tuple(N,91);Connect(F.Get(TEXT("Probe359Expected")),FindPin(N,TEXT("Expected")));F.Timer(TEXT("DOR359_ServerWatch"));F.Compile();
    }
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_ServerWatch"),Master->GeneratedClass);
        auto* Inactive=F.Branch(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(0)));F.Done();F.Tail=Inactive->GetElsePin();auto* Terminal=F.Branch(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(99)));F.Done();F.Tail=Terminal->GetElsePin();
        auto* Time=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_FloatFloat"));Connect(F.Now(),FindPin(Time,TEXT("A")));Connect(F.Get(TEXT("Probe359Deadline")),FindPin(Time,TEXT("B")));auto* Expired=F.Branch(Time->GetReturnValuePin());
        auto* Cleaning=F.Branch(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(90)));F.Log(TEXT("owner-cleanup-deadline-authority-rollback-still-required"));F.Timer(TEXT("DOR359_Rollback"));F.Done();F.Tail=Cleaning->GetElsePin();
        auto* Rollback=F.Branch(F.Eq(F.Get(TEXT("Probe359Phase")),F.Int(91)));F.Log(TEXT("recovery-incomplete-lock-retained"),F.Get(TEXT("Probe359Failure")));F.Set(TEXT("Probe359Phase"),nullptr,TEXT("99"));F.Done();F.Tail=Rollback->GetElsePin();
        auto* Abort=F.Self(TEXT("DOR359_Abort"));FindPin(Abort,TEXT("Reason"))->DefaultValue=TEXT("authority-stage-deadline");F.Done();F.Tail=Expired->GetElsePin();F.Timer(TEXT("DOR359_ServerWatch"));F.Compile();
    }
    // Handoff from the protected owner preparation, after native legal equip and
    // pair resolution, without returning into the synchronous old cycle result.
    {
        FDOR359Graph F(Master,TEXT("DOR359_OwnerPrepared"),Master->GeneratedClass);
        auto* P=F.Cast(F.Get(TEXT("Probe359Proxy")),Proxy->GeneratedClass);auto* PreparedCall=F.Call(Proxy->GeneratedClass,TEXT("DOR359_SendPrepared"));Connect(P,FindPin(PreparedCall,UEdGraphSchema_K2::PN_Self));F.Compile();
        auto* G=FindFunctionGraph(Master,TEXT("DOR_ExecutePair388PresetTransaction"));
        UK2Node_CallFunction* Before=nullptr;for(auto* Node:G->Nodes)if(auto* N=Cast<UK2Node_CallFunction>(Node))if(N->FunctionReference.GetMemberName()==TEXT("DOR_WritePair388RecoveryStage")&&FindPin(N,TEXT("Stage"))->DefaultValue==TEXT("60"))Before=N;
        if(!Before)return false;auto Targets=Before->GetThenPin()->LinkedTo;Before->GetThenPin()->BreakAllPinLinks();
        auto* Active=AddVariableGet(G,TEXT("Probe359OwnerActive"),0,0);auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);B->AllocateDefaultPins();Connect(Before->GetThenPin(),B->GetExecPin());Connect(Active->GetValuePin(),B->GetConditionPin());auto* Prep=AddSelfCall(G,TEXT("DOR359_OwnerPrepared"),0,0);Connect(B->GetThenPin(),Prep->GetExecPin());for(auto* T:Targets)Connect(B->GetElsePin(),T);
        // Remote failures restore the local journal but never attempt authority-only
        // reconstruction on the owner. Correlated server rollback owns that work.
        auto* SG=FindFunctionGraph(Master,TEXT("DOR_SchedulePair388PresetRestore"));auto* E=FindFunctionEntry(SG);UK2Node_VariableSet* Store=nullptr;for(auto* Node:SG->Nodes)if(auto* N=Cast<UK2Node_VariableSet>(Node))if(N->VariableReference.GetMemberName()==TEXT("Pair388DeferredRestoreRebuild"))Store=N;
        if(!Store)return false;FindPin(Store,TEXT("Pair388DeferredRestoreRebuild"))->BreakAllPinLinks();auto* Not=AddCall(SG,UKismetMathLibrary::StaticClass(),TEXT("Not_PreBool"),0,0);auto* PA=AddVariableGet(SG,TEXT("Probe359OwnerActive"),0,0);Connect(PA->GetValuePin(),FindPin(Not,TEXT("A")));auto* Both=AddCall(SG,UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"),0,0);Connect(Not->GetReturnValuePin(),FindPin(Both,TEXT("A")));Connect(FindPin(E,TEXT("RebuildCurrent")),FindPin(Both,TEXT("B")));Connect(Both->GetReturnValuePin(),FindPin(Store,TEXT("Pair388DeferredRestoreRebuild")));
    }
    // Route every server send (including initial ProxyReady) through acquisition.
    {
        FDOR359Graph F(Proxy,TEXT("DOR359_ServerSendPair"),Master->GeneratedClass);
        auto* Test=F.Input(TEXT("TestIndex"),MakeIntPinType());auto* Generation=F.Input(TEXT("Generation"),MakeIntPinType());auto* B=F.Branch(F.Eq(Test,F.Int(359)));F.Check(F.And(F.Eq(Test,F.Get(TEXT("RemoteTestIndex"))),F.Eq(Generation,F.Get(TEXT("RemoteGeneration")))),TEXT("ignored-stale-dispatch-tuple"));F.Self(TEXT("DOR359_Start"));F.Done();F.Tail=B->GetElsePin();auto* SendCall=F.Self(TEXT("DOR_ClientApplyRemotePair"));Connect(Test,FindPin(SendCall,TEXT("TestIndex")));Connect(Generation,FindPin(SendCall,TEXT("Generation")));F.Compile();
        for(auto* BP:{Master,Proxy})
        {
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        for(auto* G:Graphs)for(auto* Node:TArray<UEdGraphNode*>(G->Nodes))if(auto* N=Cast<UK2Node_CallFunction>(Node))
        {
            if(G->GetFName()==TEXT("DOR359_ServerSendPair")||G->GetFName()==TEXT("DOR359_BeginOwner"))continue;
            if(N->FunctionReference.GetMemberName()!=TEXT("DOR_ClientApplyRemotePair"))continue;
            auto* Replacement=AddCall(G,Proxy->GeneratedClass,TEXT("DOR359_ServerSendPair"),N->NodePosX,N->NodePosY);
            for(auto* P:N->Pins){auto* R=Replacement->FindPin(P->PinName);if(!R)continue;R->DefaultValue=P->DefaultValue;R->DefaultObject=P->DefaultObject;auto Links=P->LinkedTo;P->BreakAllPinLinks();for(auto* L:Links)Connect(L,R);}N->BreakAllNodeLinks();G->RemoveNode(N);
        }
        }
    }
    // The old delayed completion/full server replay cannot settle this probe.
    {
        auto* G=FindFunctionGraph(Proxy,TEXT("DOR_ClientFinalizeRemoteApply"));auto* E=FindFunctionEntry(G);auto* Start=E->FindPin(UEdGraphSchema_K2::PN_Then);auto Targets=Start->LinkedTo;Start->BreakAllPinLinks();auto* Phase=AddVariableGet(G,TEXT("Probe359Phase"),0,0);auto* Zero=AddCall(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"),0,0);Connect(Phase->GetValuePin(),FindPin(Zero,TEXT("A")));auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);B->AllocateDefaultPins();Connect(Start,B->GetExecPin());Connect(Zero->GetReturnValuePin(),B->GetConditionPin());for(auto* T:Targets)Connect(B->GetThenPin(),T);
        auto* Events=FBlueprintEditorUtils::FindEventGraph(Proxy);
        for(auto* Node:TArray<UEdGraphNode*>(Events->Nodes))if(auto* EventNode=Cast<UK2Node_CustomEvent>(Node))if(EventNode->CustomFunctionName==TEXT("DOR_ServerReportRemoteApply"))
        {
            auto* EventStart=EventNode->FindPin(UEdGraphSchema_K2::PN_Then);auto Old=EventStart->LinkedTo;EventStart->BreakAllPinLinks();
            auto* Current=AddVariableGet(Events,TEXT("Probe359Phase"),0,0);auto* Inactive=AddCall(Events,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"),0,0);Connect(Current->GetValuePin(),FindPin(Inactive,TEXT("A")));auto* Allowed=AddNode<UK2Node_IfThenElse>(Events,0,0);Allowed->AllocateDefaultPins();Connect(EventStart,Allowed->GetExecPin());Connect(Inactive->GetReturnValuePin(),Allowed->GetConditionPin());for(auto* Target:Old)Connect(Allowed->GetThenPin(),Target);
        }
        // A new dispatch may not replace the immutable proxy tuple, and local
        // debug apply may not open the host's journal while authority is held.
        for(const TCHAR* Name:{TEXT("DOR_DispatchRemoteResearch"),TEXT("DOR_DebugSetIndex")})
        {
            auto* GuardGraph=FindFunctionGraph(Master,Name);auto* Entry=FindFunctionEntry(GuardGraph);auto* GuardStart=Entry->FindPin(UEdGraphSchema_K2::PN_Then);auto Old=GuardStart->LinkedTo;GuardStart->BreakAllPinLinks();
            auto* Locked=AddVariableGet(GuardGraph,TEXT("RemoteAuthorityDispatchLocked"),0,0);auto* Held=AddNode<UK2Node_IfThenElse>(GuardGraph,0,0);Held->AllocateDefaultPins();Connect(GuardStart,Held->GetExecPin());Connect(Locked->GetValuePin(),Held->GetConditionPin());for(auto* Target:Old)Connect(Held->GetElsePin(),Target);
            auto* Notify=AddSelfCall(GuardGraph,TEXT("DOR_DebugNotify"),0,0);FindPin(Notify,TEXT("Message"))->DefaultValue=TEXT("[DOR-359]|stage=dispatch-held|reason=authority-construction-or-recovery-pending");Connect(Held->GetThenPin(),Notify->GetExecPin());
        }
    }
    for(auto* BP:{Master,Proxy}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    return true;
}

// Execute only read-only emitted Blueprint predicates in the editor VM. Native
// setters, refresh, timers, journals, and diagnostic save functions are not run.
bool AuditRemote359Predicates(UBlueprint* Master,UBlueprint* Proxy)
{
    TSharedPtr<FJsonObject> Root=MakeShared<FJsonObject>();auto* Report=Root.Get();
    Report->SetStringField(TEXT("build"),DORResearchBuildTag);
    Report->SetStringField(TEXT("routeIdentity"),DORRuntimeRouteIdentity());
    Report->SetStringField(TEXT("scope"),TEXT("emitted-blueprint-readonly-predicates-not-native-gameplay"));
    auto InvokeBool=[](UObject* Object,const FName FunctionName,int32 Test,const FString* Key=nullptr,int32 Phase=0)
    {
        UFunction* Fn=Object->FindFunction(FunctionName);if(!Fn)return false;
        FStructOnScope Params(Fn);void* Memory=Params.GetStructMemory();
        auto* Integer=FindFProperty<FIntProperty>(Fn,Key?TEXT("Generation"):TEXT("TestIndex"));if(!Integer)return false;Integer->SetPropertyValue_InContainer(Memory,Test);
        if(Key){auto* String=FindFProperty<FStrProperty>(Fn,TEXT("Key"));auto* PhaseProperty=FindFProperty<FIntProperty>(Fn,TEXT("Phase"));if(!String||!PhaseProperty)return false;String->SetPropertyValue_InContainer(Memory,*Key);PhaseProperty->SetPropertyValue_InContainer(Memory,Phase);}
        Object->ProcessEvent(Fn,Memory);auto* Result=FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"));return Result&&Result->GetPropertyValue_InContainer(Memory);
    };
    UObject* MC=Master->GeneratedClass->GetDefaultObject();UObject* PC=Proxy->GeneratedClass->GetDefaultObject();
    bool Passed=true;TArray<TSharedPtr<FJsonValue>> Admitted;
    for(int32 Test=1;Test<=456;++Test)
    {
        bool Actual=InvokeBool(MC,TEXT("DOR_IsRuntimePairAdmitted"),Test);Passed&=Actual==DORIsSupportedPairTestIndex(Test);
        if(Actual)Admitted.Add(MakeShared<FJsonValueNumber>(Test));
    }
    for(int32 Test:{-1,0,457,MAX_int32})Passed&=!InvokeBool(MC,TEXT("DOR_IsRuntimePairAdmitted"),Test);
    Report->SetArrayField(TEXT("admittedTestIndices"),Admitted);Report->SetNumberField(TEXT("admittedCount"),Admitted.Num());
    auto* Generation=FindFProperty<FIntProperty>(Proxy->GeneratedClass,TEXT("Probe359Generation"));auto* KeyProperty=FindFProperty<FStrProperty>(Proxy->GeneratedClass,TEXT("Probe359Key"));auto* PhaseProperty=FindFProperty<FIntProperty>(Proxy->GeneratedClass,TEXT("Probe359Phase"));
    if(!Generation||!KeyProperty||!PhaseProperty)return false;
    const int32 SavedGeneration=Generation->GetPropertyValue_InContainer(PC),SavedPhase=PhaseProperty->GetPropertyValue_InContainer(PC);const FString SavedKey=KeyProperty->GetPropertyValue_InContainer(PC);
    Generation->SetPropertyValue_InContainer(PC,7);KeyProperty->SetPropertyValue_InContainer(PC,TEXT("editor-probe-target"));PhaseProperty->SetPropertyValue_InContainer(PC,2);
    const FString GoodKey=TEXT("editor-probe-target"),OtherKey=TEXT("another-player");
    const bool Good=InvokeBool(PC,TEXT("DOR359_MatchesMessage"),7,&GoodKey,2);
    const bool Stale=!InvokeBool(PC,TEXT("DOR359_MatchesMessage"),6,&GoodKey,2);
    const bool WrongPlayer=!InvokeBool(PC,TEXT("DOR359_MatchesMessage"),7,&OtherKey,2);
    const bool WrongPhase=!InvokeBool(PC,TEXT("DOR359_MatchesMessage"),7,&GoodKey,4);
    Generation->SetPropertyValue_InContainer(PC,SavedGeneration);KeyProperty->SetPropertyValue_InContainer(PC,SavedKey);PhaseProperty->SetPropertyValue_InContainer(PC,SavedPhase);
    Passed&=Good&&Stale&&WrongPlayer&&WrongPhase;Report->SetBoolField(TEXT("currentMessageAccepted"),Good);Report->SetBoolField(TEXT("staleGenerationRejected"),Stale);Report->SetBoolField(TEXT("wrongPlayerRejected"),WrongPlayer);Report->SetBoolField(TEXT("wrongPhaseRejected"),WrongPhase);
    Report->SetBoolField(TEXT("passed"),Passed);FString Json;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
    if(!FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("DORRemote359Audit.json"))))return false;
    UE_LOG(LogDORGenerateAssets,Display,TEXT("DOR359 emitted predicate audit passed=%d admitted=%d stale=%d wrongPlayer=%d wrongPhase=%d"),Passed,Admitted.Num(),Stale,WrongPlayer,WrongPhase);
    return Passed;
}
