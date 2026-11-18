// Read-only remote435 primary observations, followed by its same-proxy277.
// The existing authority lock orders evidence; completion retains its old result.
struct FDOR435Graph : FDOR359Graph
{
    FDOR435Graph(UBlueprint* BP,FName Name,UClass* MC):FDOR359Graph(BP,Name,MC){}
    UEdGraphPin* Same(UEdGraphPin* A,UEdGraphPin* B)
    {return Eq(A,B,TEXT("EqualEqual_ObjectObject"));}
    UEdGraphPin* Path(UEdGraphPin* V)
    {auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("GetPathName"));Connect(V,FindPin(N,TEXT("Object")));return N->GetReturnValuePin();}
    void QuietRequire(UEdGraphPin* V)
    {auto* B=Branch(V);Tail=B->GetElsePin();Done();Tail=B->GetThenPin();}
    void Record(UEdGraphPin* Master,UEdGraphPin* Line)
    {auto* N=Call(MasterClass,TEXT("DOR_DiagnosticRecordOnly"));Connect(Master,FindPin(N,UEdGraphSchema_K2::PN_Self));Connect(Line,FindPin(N,TEXT("Message")));auto* Flush=Call(MasterClass,TEXT("DOR_DiagnosticFlush"));Connect(Master,FindPin(Flush,UEdGraphSchema_K2::PN_Self));}
    UEdGraphPin* Add(UEdGraphPin* Line,const TCHAR* Label,UEdGraphPin* Value)
    {return Join(Line,DORSafeConcat(*this,Label,Value));}
    UEdGraphPin* Line(const TCHAR* Stage,bool Owner=false)
    {
        const TCHAR* Prefix=Owner?TEXT("SMGO"):TEXT("SMGS");
        auto GetP=[&](const TCHAR* S){return Get(FName(*FString::Printf(TEXT("%s%s"),Prefix,S)));};
        auto* R=DORSafeConcat(*this,FString::Printf(TEXT("[DOR-SMG]|build=%s|stage=%s|key="),DORResearchBuildTag,Stage),GetP(TEXT("Key")));
        R=Add(R,TEXT("|generation="),Text(GetP(TEXT("Generation")),TEXT("Conv_IntToString")));
        R=Add(R,TEXT("|test="),Text(GetP(TEXT("Test")),TEXT("Conv_IntToString")));
        return Add(R,TEXT("|checkpoint="),Text(GetP(TEXT("Checkpoint")),TEXT("Conv_IntToString")));
    }
};

bool BuildRemote435Diagnostics(UBlueprint* Master,UBlueprint* Proxy)
{
    auto* IC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.InventoryComponent"));
    auto* SC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SaveGameStateComponent"));
    auto* SMG=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.ElectricalSMG"));
    if(!IC||!SC||!SMG)return false;
    for(const TCHAR* N:{TEXT("TurretPlasmaLineEnabled"),TEXT("TurretEMPDischargeEnabled"),TEXT("ElectricfyPlatformsEnabled")})
        if(!FindFProperty<FBoolProperty>(SMG,N))return false;
    const auto PlayerType=MakeObjectPinType(APlayerCharacter::StaticClass());
    const auto ItemType=MakeObjectPinType(AItem::StaticClass());
    FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Float;
    AddTypedVariable(Master,TEXT("SMGObservationProxy"),MakeObjectPinType(AActor::StaticClass()));
    for(const TCHAR* P:{TEXT("SMGS"),TEXT("SMGO")})
    {
        for(const TCHAR* N:{TEXT("Generation"),TEXT("Test"),TEXT("Checkpoint")})AddTypedVariable(Proxy,FName(*FString(P).Append(N)),MakeIntPinType());
        AddTypedVariable(Proxy,FName(*FString(P).Append(TEXT("Key"))),MakeStringPinType());
        AddTypedVariable(Proxy,FName(*FString(P).Append(TEXT("Active"))),MakeBoolPinType());
        AddTypedVariable(Proxy,FName(*FString(P).Append(TEXT("Deadline"))),FloatType);
        AddTypedVariable(Proxy,FName(*FString(P).Append(TEXT("Player"))),PlayerType);
        AddTypedVariable(Proxy,FName(*FString(P).Append(TEXT("Expected"))),ItemType);
    }
    AddTypedVariable(Proxy,TEXT("SMGOwnerMaster"),MakeObjectPinType(Master->GeneratedClass));
    for(const TCHAR* N:{TEXT("SMGBaselineReady"),TEXT("SMGScope")})AddTypedVariable(Proxy,N,MakeBoolPinType());
    AddTypedVariable(Proxy,TEXT("SMGBaselinePlayer"),PlayerType);
    AddTypedVariable(Proxy,TEXT("SMGBaselineActor"),ItemType);
    AddTypedVariable(Proxy,TEXT("SMGBaselineKey"),MakeStringPinType());
    AddTypedVariable(Proxy,TEXT("SMGBaselineGeneration"),MakeIntPinType());
    for(const TCHAR* N:{TEXT("SMGBaselineP40"),TEXT("SMGBaselineRestored"),TEXT("SMGOwnerReceipt")})AddTypedVariable(Proxy,N,MakeStringPinType());
    for(auto* BP:{Master,Proxy}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);}
    auto* EG=FBlueprintEditorUtils::FindEventGraph(Proxy);
    auto Event=[&](const TCHAR* Name,bool Server)
    {
        auto* E=AddCustomEvent(EG,Name,FUNC_Net|FUNC_NetReliable|FUNC_BlueprintCallable|(Server?FUNC_NetServer:FUNC_NetClient),0,26000+EG->Nodes.Num()*20);
        for(const TCHAR* N:{TEXT("Generation"),TEXT("Test"),TEXT("Checkpoint")})E->CreateUserDefinedPin(N,MakeIntPinType(),EGPD_Output);
        E->CreateUserDefinedPin(TEXT("Key"),MakeStringPinType(),EGPD_Output);
        E->CreateUserDefinedPin(TEXT("Player"),PlayerType,EGPD_Output);
        E->CreateUserDefinedPin(TEXT("Expected"),ItemType,EGPD_Output);
        if(Server){E->CreateUserDefinedPin(TEXT("Status"),MakeStringPinType(),EGPD_Output);E->CreateUserDefinedPin(TEXT("Snapshot"),MakeStringPinType(),EGPD_Output);}
        E->ReconstructNode();return E;
    };
    auto* Observe=Event(TEXT("DOR435_ClientReadPrimary"),false);
    auto* Ack=Event(TEXT("DOR435_ServerReadback"),true);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Proxy);FKismetEditorUtilities::CompileBlueprint(Proxy);
    auto Wire=[&](UK2Node_CustomEvent* E,const TCHAR* Name)
    {auto* N=AddSelfCall(EG,Name,500,E->NodePosY);Connect(E->FindPin(UEdGraphSchema_K2::PN_Then),N->GetExecPin());for(auto* P:E->Pins)if(P->Direction==EGPD_Output&&P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec&&N->FindPin(P->PinName))Connect(P,FindPin(N,P->PinName));};
    auto Tuple=[&](FDOR435Graph& F,UK2Node_CallFunction* N,bool Owner)
    {const TCHAR* P=Owner?TEXT("SMGO"):TEXT("SMGS");for(const TCHAR* S:{TEXT("Generation"),TEXT("Test"),TEXT("Checkpoint"),TEXT("Key"),TEXT("Player"),TEXT("Expected")})Connect(F.Get(FName(*FString(P).Append(S))),FindPin(N,S));};
    auto SetTuple=[&](FDOR435Graph& F,bool Owner)
    {const TCHAR* P=Owner?TEXT("SMGO"):TEXT("SMGS");for(const TCHAR* S:{TEXT("Generation"),TEXT("Test"),TEXT("Checkpoint"),TEXT("Key"),TEXT("Player"),TEXT("Expected")})F.Set(FName(*FString(P).Append(S)),F.Input(S,FString(S)==TEXT("Key")?MakeStringPinType():FString(S)==TEXT("Player")?PlayerType:FString(S)==TEXT("Expected")?ItemType:MakeIntPinType()));};
    auto Primary=[&](FDOR435Graph& F,UEdGraphPin* Player)
    {auto* C=F.Cache(TEXT("Inventory"),F.Component(Player,IC));auto* N=F.Call(IC,TEXT("GetItem"));Connect(C,FindPin(N,UEdGraphSchema_K2::PN_Self));ForcePinType(FindPin(N,TEXT("Category")),MakeItemCategoryPinType());FindPin(N,TEXT("Category"))->DefaultValue=UEnum::GetValueAsString(EItemCategory::PrimaryWeapon);return F.Cache(TEXT("Primary"),N->GetReturnValuePin());};
    auto Snapshot=[&](FDOR435Graph& F,UEdGraphPin* Player,UEdGraphPin* Expected)
    {auto* N=F.Self(TEXT("DOR435_Snapshot"));Connect(Player,FindPin(N,TEXT("Player")));Connect(Expected,FindPin(N,TEXT("Expected")));return F.Cache(TEXT("Snapshot"),N->GetReturnValuePin());};
    auto Deadline=[&](FDOR435Graph& F,bool Owner)
    {auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_FloatFloat"));Connect(F.Now(),FindPin(N,TEXT("A")));FindPin(N,TEXT("B"))->DefaultValue=TEXT("5.0");F.Set(Owner?TEXT("SMGODeadline"):TEXT("SMGSDeadline"),N->GetReturnValuePin());};
    auto Expired=[&](FDOR435Graph& F,bool Owner)
    {auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_FloatFloat"));Connect(F.Now(),FindPin(N,TEXT("A")));Connect(F.Get(Owner?TEXT("SMGODeadline"):TEXT("SMGSDeadline")),FindPin(N,TEXT("B")));return N->GetReturnValuePin();};
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_ActorReadback"),Master->GeneratedClass);
        auto* Actor=F.Input(TEXT("Actor"),ItemType);auto* Player=F.Input(TEXT("Player"),PlayerType);
        auto* Result=F.Cache(TEXT("Line"),F.Path(Actor));
        auto* Valid=F.Branch(F.IsValid(Actor));F.Tail=Valid->GetElsePin();F.Return(F.Add(Result,TEXT(";cast="),DORSafeString(F,TEXT("unavailable"))));F.Tail=Valid->GetThenPin();
        auto* C=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(Actor,FindPin(C,TEXT("Object")));
        auto* ClassName=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetClassDisplayName"));Connect(C->GetReturnValuePin(),FindPin(ClassName,TEXT("Class")));Result=F.Add(Result,TEXT(";class="),ClassName->GetReturnValuePin());
        auto* O=F.Call(AActor::StaticClass(),TEXT("GetOwner"));Connect(Actor,FindPin(O,UEdGraphSchema_K2::PN_Self));
        Result=F.Add(Result,TEXT(";owner="),F.Path(O->GetReturnValuePin()));
        Result=F.Add(Result,TEXT(";ownerTarget="),F.Text(F.Same(O->GetReturnValuePin(),Player),TEXT("Conv_BoolToString")));
        Result=F.Add(Result,TEXT(";characterTarget="),F.Text(F.Same(F.Field(Actor,AItem::StaticClass(),TEXT("Character")),Player),TEXT("Conv_BoolToString")));
        auto* A=F.Call(AActor::StaticClass(),TEXT("HasAuthority"));Connect(Actor,FindPin(A,UEdGraphSchema_K2::PN_Self));Result=F.Add(Result,TEXT(";authority="),F.Text(A->GetReturnValuePin(),TEXT("Conv_BoolToString")));
        for(const TCHAR* R:{TEXT("GetLocalRole"),TEXT("GetRemoteRole")}){auto* N=F.Call(AActor::StaticClass(),R);Connect(Actor,FindPin(N,UEdGraphSchema_K2::PN_Self));auto* T=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_ByteToString"));Connect(N->GetReturnValuePin(),FindPin(T,TEXT("InByte")));Result=F.Add(Result,R,T->GetReturnValuePin());}
        Result=F.Cache(TEXT("Context"),Result);auto* Cast=F.Cache(TEXT("SMG"),F.Cast(Actor,SMG));auto* B=F.Branch(F.IsValid(Cast));
        F.Tail=B->GetElsePin();F.Return(F.Add(Result,TEXT(";flags="),DORSafeString(F,TEXT("unavailable-cast"))));F.Tail=B->GetThenPin();
        for(const TCHAR* Flag:{TEXT("TurretPlasmaLineEnabled"),TEXT("TurretEMPDischargeEnabled"),TEXT("ElectricfyPlatformsEnabled")})
        {auto* V=F.Field(Cast,SMG,Flag);ForcePinType(V,MakeBoolPinType());auto* Cached=F.Cache(Flag,V);Result=F.Add(Result,*FString::Printf(TEXT(";%s="),Flag),F.Text(Cached,TEXT("Conv_BoolToString")));}
        F.Return(Result);F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_Selection"),Master->GeneratedClass);auto* Player=F.Input(TEXT("Player"),PlayerType);
        auto* PS=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(Player,FindPin(PS,UEdGraphSchema_K2::PN_Self));auto* C=F.Cache(TEXT("SaveState"),F.Component(PS->GetReturnValuePin(),SC));
        auto* B=F.Branch(F.IsValid(C));F.Tail=B->GetElsePin();F.Return(DORSafeString(F,TEXT("selection=unavailable")));F.Tail=B->GetThenPin();
        auto* PrimaryGuid=F.Cache(TEXT("WeaponID"),F.FieldOf(F.Field(C,SC,TEXT("EquippedLoadout")),FItemLoadout::StaticStruct(),TEXT("PrimaryWeapon")));
        auto* Count=F.Cache(TEXT("Count"),F.Int(0));auto* Selected=F.Cache(TEXT("Selected"),DORSafeString(F,TEXT("none")));auto* Unlocked=F.Cache(TEXT("Unlocked"),F.Eq(F.Int(0),F.Int(1)));
        auto* Loop=F.Each(F.Field(C,SC,TEXT("ItemUpgradeSelections")));auto* E=FindPin(Loop,TEXT("Array Element"));
        F.Branch(F.Eq(F.FieldOf(E,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID")),PrimaryGuid,TEXT("EqualEqual_GuidGuid")));
        auto* Add=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(Count,FindPin(Add,TEXT("A")));FindPin(Add,TEXT("B"))->DefaultValue=TEXT("1");F.Set(Count->PinName,Add->GetReturnValuePin());
        F.Set(Selected->PinName,F.Text(F.FieldOf(E,FItemUpgradeSelection::StaticStruct(),TEXT("EquippedOverclock")),TEXT("Conv_GuidToString")));F.Set(Unlocked->PinName,F.FieldOf(E,FItemUpgradeSelection::StaticStruct(),TEXT("OverclockingUnlocked")));
        F.Tail=FindPin(Loop,TEXT("Completed"));auto* R=DORSafeConcat(F,TEXT("weapon="),F.Text(PrimaryGuid,TEXT("Conv_GuidToString")));R=F.Add(R,TEXT(";selectionCount="),F.Text(Count,TEXT("Conv_IntToString")));R=F.Add(R,TEXT(";selected="),Selected);R=F.Add(R,TEXT(";unlocked="),F.Text(Unlocked,TEXT("Conv_BoolToString")));F.Return(R);F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_Snapshot"),Master->GeneratedClass);auto* P=F.Input(TEXT("Player"),PlayerType);auto* Expected=F.Input(TEXT("Expected"),ItemType);
        auto* A=Primary(F,P);auto* Read=F.Self(TEXT("DOR435_ActorReadback"));Connect(A,FindPin(Read,TEXT("Actor")));Connect(P,FindPin(Read,TEXT("Player")));auto* R=F.Cache(TEXT("Actual"),Read->GetReturnValuePin());
        auto* Inventory=F.Cache(TEXT("InventoryContext"),F.Component(P,IC));auto* Owner=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));Connect(Inventory,FindPin(Owner,UEdGraphSchema_K2::PN_Self));R=F.Add(R,TEXT("|inventoryOwnerTarget="),F.Text(F.And(F.IsValid(Inventory),F.Same(Owner->GetReturnValuePin(),P)),TEXT("Conv_BoolToString")));
        R=F.Add(R,TEXT("|expectedMatch="),F.Text(F.And(F.IsValid(Expected),F.Same(A,Expected)),TEXT("Conv_BoolToString")));
        auto* ER=F.Self(TEXT("DOR435_ActorReadback"));Connect(Expected,FindPin(ER,TEXT("Actor")));Connect(P,FindPin(ER,TEXT("Player")));R=F.Cache(TEXT("ActorPair"),F.Add(R,TEXT("|expected="),ER->GetReturnValuePin()));
        auto* S=F.Self(TEXT("DOR435_Selection"));Connect(P,FindPin(S,TEXT("Player")));F.Return(F.Add(R,TEXT("|targetPrimary="),S->GetReturnValuePin()));F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_BeforeApply"),Master->GeneratedClass);F.Set(TEXT("SMGScope"),nullptr,TEXT("false"));
        // Only manual/research dispatch owns the Master's RemoteCurrent tuple.
        F.QuietRequire(F.Not(F.Get(TEXT("RewardApplyPending"))));F.QuietRequire(F.Not(F.Get(TEXT("ReplayRefreshInFlight"))));F.QuietRequire(F.IsValid(F.Get(TEXT("MasterRef"))));
        for(auto P:{TPair<const TCHAR*,const TCHAR*>(TEXT("RemoteClientGeneration"),TEXT("RemoteCurrentGeneration")),TPair<const TCHAR*,const TCHAR*>(TEXT("RemoteClientTestIndex"),TEXT("RemoteCurrentTestIndex"))})F.QuietRequire(F.Eq(F.Get(P.Key),F.Field(F.Get(TEXT("MasterRef")),Master->GeneratedClass,P.Value)));
        F.QuietRequire(F.Eq(F.Get(TEXT("RemoteRequestPlayerKey")),F.Field(F.Get(TEXT("MasterRef")),Master->GeneratedClass,TEXT("RemoteCurrentPlayerKey")),TEXT("EqualEqual_StrStr")));
        auto* First=F.Branch(F.Eq(F.Get(TEXT("RemoteClientTestIndex")),F.Int(435)));
        F.Set(TEXT("SMGBaselineReady"),nullptr,TEXT("false"));F.Set(TEXT("SMGBaselinePlayer"),F.Get(TEXT("OwningRewardPlayer")));F.Set(TEXT("SMGBaselineKey"),F.Get(TEXT("RemoteRequestPlayerKey")));F.Set(TEXT("SMGBaselineGeneration"),F.Get(TEXT("RemoteClientGeneration")));auto* FT=F.Tail;
        F.Tail=First->GetElsePin();F.QuietRequire(F.Eq(F.Get(TEXT("RemoteClientTestIndex")),F.Int(277)));F.QuietRequire(F.Get(TEXT("SMGBaselineReady")));F.QuietRequire(F.And(F.Same(F.Get(TEXT("OwningRewardPlayer")),F.Get(TEXT("SMGBaselinePlayer"))),F.Eq(F.Get(TEXT("RemoteRequestPlayerKey")),F.Get(TEXT("SMGBaselineKey")),TEXT("EqualEqual_StrStr"))));
        auto* Newer=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(F.Get(TEXT("RemoteClientGeneration")),FindPin(Newer,TEXT("A")));Connect(F.Get(TEXT("SMGBaselineGeneration")),FindPin(Newer,TEXT("B")));F.QuietRequire(Newer->GetReturnValuePin());
        auto* Store=AddVariableSet(F.G,TEXT("SMGScope"),0,0);FindPin(Store,TEXT("SMGScope"))->DefaultValue=TEXT("true");F.Run(Store);Connect(FT,Store->GetExecPin());
        F.Set(TEXT("SMGSPlayer"),F.Get(TEXT("OwningRewardPlayer")));F.Set(TEXT("SMGSKey"),F.Get(TEXT("RemoteRequestPlayerKey")));F.Set(TEXT("SMGSGeneration"),F.Get(TEXT("RemoteClientGeneration")));F.Set(TEXT("SMGSTest"),F.Get(TEXT("RemoteClientTestIndex")));
        F.Set(TEXT("SMGSCheckpoint"),nullptr,TEXT("0"));F.Put(F.Get(TEXT("MasterRef")),Master->GeneratedClass,TEXT("SMGObservationProxy"),SelfPin(F.G,0,700));
        auto* Second=F.Branch(F.Eq(F.Get(TEXT("SMGSTest")),F.Int(277)));F.Record(F.Get(TEXT("MasterRef")),F.Add(F.Line(TEXT("277-authority-entry")),TEXT("|data="),Snapshot(F,F.Get(TEXT("SMGSPlayer")),F.Get(TEXT("SMGBaselineActor")))));F.Tail=Second->GetElsePin();F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_PreRestore"),Master->GeneratedClass);F.QuietRequire(F.Get(TEXT("SMGScope")));F.QuietRequire(F.Eq(F.Get(TEXT("SMGSTest")),F.Int(435)));F.QuietRequire(F.Same(F.Get(TEXT("SMGSPlayer")),F.Field(F.Get(TEXT("MasterRef")),Master->GeneratedClass,TEXT("PresetTransactionPlayer"))));
        F.Set(TEXT("SMGBaselineActor"),Primary(F,F.Get(TEXT("SMGSPlayer"))));F.Set(TEXT("SMGBaselineP40"),Snapshot(F,F.Get(TEXT("SMGSPlayer")),F.Get(TEXT("SMGBaselineActor"))));F.Record(F.Get(TEXT("MasterRef")),F.Add(F.Line(TEXT("435-P40-before-restore")),TEXT("|data="),F.Get(TEXT("SMGBaselineP40"))));F.Compile();
    }
    {
        FDOR435Graph F(Master,TEXT("DOR435_PreRestoreHook"),Master->GeneratedClass);auto* P=F.Cache(TEXT("Proxy"),F.Cast(F.Get(TEXT("SMGObservationProxy")),Proxy->GeneratedClass));F.QuietRequire(F.IsValid(P));auto* N=F.Call(Proxy->GeneratedClass,TEXT("DOR435_PreRestore"));Connect(P,FindPin(N,UEdGraphSchema_K2::PN_Self));F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_ResumeLegacy"),Master->GeneratedClass);
        F.Put(F.Get(TEXT("MasterRef")),Master->GeneratedClass,TEXT("RemoteAuthorityDispatchLocked"),nullptr,TEXT("false"));
        auto* B=F.Branch(F.Get(TEXT("ReplayRefreshInFlight")));F.Self(TEXT("DOR_ServerPublishAuthoritativePair"));F.Done();F.Tail=B->GetElsePin();F.Self(TEXT("DOR_ServerCaptureReplayItems"));F.Self(TEXT("DOR_ServerPublishAuthoritativePair"));F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_CurrentTuple"),Master->GeneratedClass);
        auto* M=F.Get(TEXT("MasterRef"));auto* Match=F.And(F.IsValid(M),F.And(F.Eq(F.Get(TEXT("SMGSGeneration")),F.Get(TEXT("RemoteClientGeneration"))),F.And(F.Eq(F.Get(TEXT("SMGSTest")),F.Get(TEXT("RemoteClientTestIndex"))),F.And(F.Eq(F.Get(TEXT("SMGSKey")),F.Get(TEXT("RemoteRequestPlayerKey")),TEXT("EqualEqual_StrStr")),F.Same(F.Get(TEXT("SMGSPlayer")),F.Get(TEXT("OwningRewardPlayer")))))));
        Match=F.And(Match,F.And(F.Eq(F.Get(TEXT("SMGSGeneration")),F.Field(M,Master->GeneratedClass,TEXT("RemoteCurrentGeneration"))),F.And(F.Eq(F.Get(TEXT("SMGSTest")),F.Field(M,Master->GeneratedClass,TEXT("RemoteCurrentTestIndex"))),F.Eq(F.Get(TEXT("SMGSKey")),F.Field(M,Master->GeneratedClass,TEXT("RemoteCurrentPlayerKey")),TEXT("EqualEqual_StrStr")))));
        F.Return(Match);F.Entry->AddExtraFlags(FUNC_BlueprintPure);F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_FinishObservation"),Master->GeneratedClass);auto* Status=F.Input(TEXT("Status"),MakeStringPinType());auto* Data=F.Input(TEXT("Snapshot"),MakeStringPinType());F.QuietRequire(F.Get(TEXT("SMGSActive")));F.Set(TEXT("SMGSActive"),nullptr,TEXT("false"));
        auto* Current=F.Call(Proxy->GeneratedClass,TEXT("DOR435_CurrentTuple"));Connect(SelfPin(F.G,0,700),FindPin(Current,UEdGraphSchema_K2::PN_Self));auto* Guard=F.Branch(Current->GetReturnValuePin());
        F.Tail=Guard->GetElsePin();F.Set(TEXT("SMGScope"),nullptr,TEXT("false"));F.QuietRequire(F.IsValid(F.Get(TEXT("MasterRef"))));F.Record(F.Get(TEXT("MasterRef")),F.Add(F.Line(TEXT("owner-terminal-report")),TEXT("|status="),DORSafeString(F,TEXT("stale-tuple-ordering-invalid-own-guard-cleared"))));F.Done();F.Tail=Guard->GetThenPin();
        auto* Window=F.Add(F.Add(F.Line(TEXT("owner-terminal-report")),TEXT("|status="),Status),TEXT("|authorityWindowPrimaryMatch="),F.Text(F.And(F.IsValid(F.Get(TEXT("SMGSExpected"))),F.Same(Primary(F,F.Get(TEXT("SMGSPlayer"))),F.Get(TEXT("SMGSExpected")))),TEXT("Conv_BoolToString")));
        Window=F.Add(Window,TEXT("|authorityWindowTransactionInactive="),F.Text(F.Not(F.Field(F.Get(TEXT("MasterRef")),Master->GeneratedClass,TEXT("ExternalPresetTransactionActive"))),TEXT("Conv_BoolToString")));F.Record(F.Get(TEXT("MasterRef")),F.Add(Window,TEXT("|data="),Data));
        F.Set(TEXT("SMGScope"),nullptr,TEXT("false"));F.Put(F.Get(TEXT("MasterRef")),Master->GeneratedClass,TEXT("SMGObservationProxy"),nullptr);F.Self(TEXT("DOR435_ResumeLegacy"));F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_Finalize"),Master->GeneratedClass);auto* Scope=F.Branch(F.Get(TEXT("SMGScope")));F.Tail=Scope->GetElsePin();F.Self(TEXT("DOR435_ResumeLegacy"));F.Done();F.Tail=Scope->GetThenPin();F.QuietRequire(F.Not(F.Get(TEXT("SMGSActive"))));
        F.Set(TEXT("SMGSExpected"),Primary(F,F.Get(TEXT("SMGSPlayer"))));auto* Restored=Snapshot(F,F.Get(TEXT("SMGSPlayer")),F.Get(TEXT("SMGSExpected")));
        Restored=F.Cache(TEXT("BaselineComparison"),F.Add(Restored,TEXT("|baseline435ActorMatch="),F.Text(F.And(F.IsValid(F.Get(TEXT("SMGBaselineActor"))),F.Same(F.Get(TEXT("SMGSExpected")),F.Get(TEXT("SMGBaselineActor")))),TEXT("Conv_BoolToString"))));
        auto* First=F.Branch(F.Eq(F.Get(TEXT("SMGSTest")),F.Int(435)));F.Set(TEXT("SMGBaselineRestored"),Restored);F.Set(TEXT("SMGBaselineReady"),nullptr,TEXT("true"));F.Set(TEXT("SMGBaselineActor"),F.Get(TEXT("SMGSExpected")));F.Set(TEXT("SMGSCheckpoint"),nullptr,TEXT("1"));auto* FT=F.Tail;
        F.Tail=First->GetElsePin();F.Set(TEXT("SMGSCheckpoint"),nullptr,TEXT("2"));auto* Store=AddVariableSet(F.G,TEXT("SMGSActive"),0,0);FindPin(Store,TEXT("SMGSActive"))->DefaultValue=TEXT("true");F.Run(Store);Connect(FT,Store->GetExecPin());
        F.Record(F.Get(TEXT("MasterRef")),F.Add(F.Line(TEXT("authority-after-checked-restore")),TEXT("|data="),Restored));Deadline(F,false);
        auto* N=F.Self(TEXT("DOR435_ClientReadPrimary"));Tuple(F,N,false);F.Timer(TEXT("DOR435_ServerWatch"));F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_ServerWatch"),Master->GeneratedClass);F.QuietRequire(F.Get(TEXT("SMGSActive")));
        auto* Gone=F.Branch(F.Not(F.IsValid(F.Get(TEXT("SMGSPlayer")))));auto* End=F.Self(TEXT("DOR435_FinishObservation"));FindPin(End,TEXT("Status"))->DefaultValue=TEXT("target-disconnected-unknown");F.Done();F.Tail=Gone->GetElsePin();
        auto* Timed=F.Branch(Expired(F,false));auto* Timeout=F.Self(TEXT("DOR435_FinishObservation"));FindPin(Timeout,TEXT("Status"))->DefaultValue=TEXT("host-deadline-unknown");F.Done();F.Tail=Timed->GetElsePin();F.Timer(TEXT("DOR435_ServerWatch"));F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_ReceiveOwner"),Master->GeneratedClass);
        auto* Generation=F.Input(TEXT("Generation"),MakeIntPinType());auto* Test=F.Input(TEXT("Test"),MakeIntPinType());auto* Checkpoint=F.Input(TEXT("Checkpoint"),MakeIntPinType());
        F.QuietRequire(F.Eq(F.And(F.Eq(Test,F.Int(435)),F.Eq(Checkpoint,F.Int(1))),F.And(F.Eq(Test,F.Int(277)),F.Eq(Checkpoint,F.Int(2))),TEXT("BooleanOR")));
        auto* Newer=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Generation,FindPin(Newer,TEXT("A")));Connect(F.Get(TEXT("SMGOGeneration")),FindPin(Newer,TEXT("B")));F.QuietRequire(Newer->GetReturnValuePin());F.QuietRequire(F.Not(F.Get(TEXT("SMGOActive"))));SetTuple(F,true);
        auto* All=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(All,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;Connect(SelfPin(F.G,0,700),FindPin(All,TEXT("WorldContextObject")));F.Set(TEXT("SMGOwnerMaster"),F.Cast(All->GetReturnValuePin(),Master->GeneratedClass));
        F.QuietRequire(F.IsValid(F.Get(TEXT("SMGOwnerMaster"))));auto* Local=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"));Connect(SelfPin(F.G,0,700),FindPin(Local,TEXT("WorldContextObject")));F.QuietRequire(F.Same(F.Get(TEXT("SMGOPlayer")),Local->GetReturnValuePin()));
        F.Set(TEXT("SMGOActive"),nullptr,TEXT("true"));Deadline(F,true);F.Set(TEXT("SMGOwnerReceipt"),Snapshot(F,F.Get(TEXT("SMGOPlayer")),F.Get(TEXT("SMGOExpected"))));F.Record(F.Get(TEXT("SMGOwnerMaster")),F.Add(F.Line(TEXT("owner-receipt"),true),TEXT("|data="),F.Get(TEXT("SMGOwnerReceipt"))));F.Timer(TEXT("DOR435_OwnerWatch"));F.Compile();Wire(Observe,TEXT("DOR435_ReceiveOwner"));
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_OwnerWatch"),Master->GeneratedClass);F.QuietRequire(F.Get(TEXT("SMGOActive")));
        auto* Matched=F.Cache(TEXT("Matched"),F.And(F.IsValid(F.Get(TEXT("SMGOExpected"))),F.Same(Primary(F,F.Get(TEXT("SMGOPlayer"))),F.Get(TEXT("SMGOExpected")))));
        auto* Complete=F.Branch(F.Eq(Matched,Expired(F,true),TEXT("BooleanOR")));auto* Status=F.Cache(TEXT("Status"),DORSafeString(F,TEXT("matched-reference")));auto* State=F.Branch(Matched);auto* MT=F.Tail;F.Tail=State->GetElsePin();F.Set(Status->PinName,DORSafeString(F,TEXT("owner-deadline-unmatched")));
        auto* Stop=AddVariableSet(F.G,TEXT("SMGOActive"),0,0);FindPin(Stop,TEXT("SMGOActive"))->DefaultValue=TEXT("false");F.Run(Stop);Connect(MT,Stop->GetExecPin());
        auto* Data=F.Cache(TEXT("Terminal"),Snapshot(F,F.Get(TEXT("SMGOPlayer")),F.Get(TEXT("SMGOExpected"))));F.Record(F.Get(TEXT("SMGOwnerMaster")),F.Add(F.Add(F.Line(TEXT("owner-terminal"),true),TEXT("|status="),Status),TEXT("|data="),Data));
        auto* N=F.Self(TEXT("DOR435_ServerReadback"));Tuple(F,N,true);Connect(Status,FindPin(N,TEXT("Status")));Connect(F.Add(F.Get(TEXT("SMGOwnerReceipt")),TEXT("|terminal="),Data),FindPin(N,TEXT("Snapshot")));F.Done();F.Tail=Complete->GetElsePin();F.Timer(TEXT("DOR435_OwnerWatch"));F.Compile();
    }
    {
        FDOR435Graph F(Proxy,TEXT("DOR435_AcceptReadback"),Master->GeneratedClass);F.QuietRequire(F.Get(TEXT("SMGSActive")));
        for(const TCHAR* N:{TEXT("Generation"),TEXT("Test"),TEXT("Checkpoint"),TEXT("Key"),TEXT("Player"),TEXT("Expected")})
        {const auto T=FString(N)==TEXT("Key")?MakeStringPinType():FString(N)==TEXT("Player")?PlayerType:FString(N)==TEXT("Expected")?ItemType:MakeIntPinType();auto* A=F.Input(N,T);auto* B=F.Get(FName(*FString(TEXT("SMGS")).Append(N)));F.QuietRequire(FString(N)==TEXT("Key")?F.Eq(A,B,TEXT("EqualEqual_StrStr")):FString(N)==TEXT("Player")||FString(N)==TEXT("Expected")?F.Same(A,B):F.Eq(A,B));}
        auto* N=F.Self(TEXT("DOR435_FinishObservation"));Connect(F.Input(TEXT("Status"),MakeStringPinType()),FindPin(N,TEXT("Status")));Connect(F.Input(TEXT("Snapshot"),MakeStringPinType()),FindPin(N,TEXT("Snapshot")));F.Compile();Wire(Ack,TEXT("DOR435_AcceptReadback"));
    }
    // Insert observations at existing exec boundaries, preserving the old tail.
    auto Insert=[&](UBlueprint* BP,const TCHAR* GraphName,const TCHAR* Before,const TCHAR* Function)
    {auto* G=FindFunctionGraph(BP,GraphName);if(!G)return false;for(auto* Node:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(Node))if(C->FunctionReference.GetMemberName()==Before){auto* Exec=C->GetExecPin();auto Incoming=Exec->LinkedTo;if(Incoming.Num()!=1)return false;Exec->BreakAllPinLinks();auto* N=AddSelfCall(G,Function,0,0);Connect(Incoming[0],N->GetExecPin());Connect(N->GetThenPin(),Exec);return true;}return false;};
    if(!Insert(Proxy,TEXT("DOR_ServerBeginAuthoritativePair"),TEXT("DOR_ApplyRewardCode"),TEXT("DOR435_BeforeApply")))return false;
    // The success P40 schedule is identified by its immediately preceding P40 notify.
    {
        auto* G=FindFunctionGraph(Master,TEXT("DOR_ExecutePair388PresetTransaction"));bool Found=false;
        for(auto* Node:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(Node))if(C->FunctionReference.GetMemberName()==TEXT("DOR_DebugNotify")&&FindPin(C,TEXT("Message"))->DefaultValue.StartsWith(TEXT("[DOR-DIAG][P40]")))
        {auto* Out=C->GetThenPin();auto Next=Out->LinkedTo;if(Next.Num()!=1)return false;Out->BreakAllPinLinks();auto* N=AddSelfCall(G,TEXT("DOR435_PreRestoreHook"),0,0);Connect(Out,N->GetExecPin());Connect(N->GetThenPin(),Next[0]);Found=true;break;}if(!Found)return false;
    }
    {
        auto* G=FindFunctionGraph(Proxy,TEXT("DOR_ServerFinalizeAuthoritativePair"));bool Found=false;
        for(auto* Node:G->Nodes)if(auto* S=Cast<UK2Node_VariableSet>(Node))if(S->VariableReference.GetMemberName()==TEXT("RemoteServerAuthoritySucceeded"))
        {auto* Out=S->FindPin(UEdGraphSchema_K2::PN_Then);Out->BreakAllPinLinks();auto* N=AddSelfCall(G,TEXT("DOR435_Finalize"),0,0);Connect(Out,N->GetExecPin());Found=true;break;}if(!Found)return false;
    }
    for(auto* BP:{Master,Proxy}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    return true;
}
