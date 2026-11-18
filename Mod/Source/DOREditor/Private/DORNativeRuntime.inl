// One transport per target PlayerState; request metadata is separate from the
// selected execution context. Queuing never captures mission or profile data.
struct FDORNativeGraph : FDOR91Graph
{
    FDORNativeGraph(UBlueprint* BP,FName Name,UClass* Master):FDOR91Graph(BP,Name,Master){}
    FName Variable(UEdGraphPin* V)
    {return CastChecked<UK2Node_VariableGet>(V->GetOwningNode())->VariableReference.GetMemberName();}
    UEdGraphPin* Text(UEdGraphPin* V,FName Conversion)
    {if(Conversion!=TEXT("Conv_FloatToString")&&Conversion!=TEXT("Conv_ByteToString")&&Conversion!=TEXT("Conv_VectorToString"))return FDOR91Graph::Text(V,Conversion);auto* N=Call(UKismetStringLibrary::StaticClass(),Conversion);Connect(V,FindPin(N,Conversion==TEXT("Conv_FloatToString")?TEXT("InFloat"):Conversion==TEXT("Conv_VectorToString")?TEXT("InVec"):TEXT("InByte")));return N->GetReturnValuePin();}
    UEdGraphPin* RealNow()
    {
        auto* Time=Call(UGameplayStatics::StaticClass(),TEXT("GetAccurateRealTime"));
        auto* Seconds=Call(UKismetMathLibrary::StaticClass(),TEXT("Conv_IntToFloat"));
        Connect(FindPin(Time,TEXT("Seconds")),FindPin(Seconds,TEXT("InInt")));
        auto* Add=Call(UKismetMathLibrary::StaticClass(),TEXT("Add_FloatFloat"));
        Connect(Seconds->GetReturnValuePin(),FindPin(Add,TEXT("A")));
        Connect(FindPin(Time,TEXT("PartialSeconds")),FindPin(Add,TEXT("B")));return Add->GetReturnValuePin();
    }
    void QuietObjectRequire(UEdGraphPin* Condition,UEdGraphPin* Null)
    {auto* B=Branch(Condition);Tail=B->GetElsePin();Return(Null);Tail=B->GetThenPin();}
    UEdGraphPin* Str(const TCHAR* Value)
    {auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralString"));FindPin(N,TEXT("Value"))->DefaultValue=Value;return N->GetReturnValuePin();}
    UEdGraphPin* Float(float Value)
    {auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralFloat"));FindPin(N,TEXT("Value"))->DefaultValue=FString::SanitizeFloat(Value);return N->GetReturnValuePin();}
};

// Carrier-driven construction excludes the distinct 435 pilot adapter.
UEdGraphPin* DORNativeParameterized(FDORNativeGraph& F,UEdGraphPin* R,UClass* RequestClass)
{
    return F.Eq(F.Eq(F.Eq(F.Field(R,RequestClass,TEXT("NativePreparedRemaining")),F.Field(R,RequestClass,TEXT("NativePreparedGF01")),TEXT("BooleanOR")),
        F.Field(R,RequestClass,TEXT("NativePreparedGF02")),TEXT("BooleanOR")),
        F.Eq(F.Field(R,RequestClass,TEXT("NativePreparedGF03")),
        F.Field(R,RequestClass,TEXT("NativePreparedGF04")),TEXT("BooleanOR")),TEXT("BooleanOR"));
}

const TArray<TPair<FName,FEdGraphPinType>>& DORNativeTuplePins()
{
    static const TArray<TPair<FName,FEdGraphPinType>> Pins={
        TPair<FName,FEdGraphPinType>(TEXT("Key"),MakeStringPinType()),
        TPair<FName,FEdGraphPinType>(TEXT("Epoch"),MakeStructPinType(TBaseStructure<FGuid>::Get())),
        TPair<FName,FEdGraphPinType>(TEXT("Generation"),MakeIntPinType()),
        TPair<FName,FEdGraphPinType>(TEXT("Request"),MakeStructPinType(TBaseStructure<FGuid>::Get())),
        TPair<FName,FEdGraphPinType>(TEXT("Slot"),MakeIntPinType()),TPair<FName,FEdGraphPinType>(TEXT("Code"),MakeIntPinType()),
        TPair<FName,FEdGraphPinType>(TEXT("Operation"),MakeIntPinType())};
    return Pins;
}

void DORNativeArmReceipt(FDORNativeGraph& F,UBlueprint* Master,const TCHAR* Stage,const TCHAR* Gate)
{
    auto* M=F.Cache(TEXT("ArmDiagnosticMaster"),F.Get(TEXT("NativeMaster")));auto* Missing=F.Branch(F.Not(F.IsValid(M)));auto* Find=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(Find,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;Connect(SelfPin(F.G,0,0),FindPin(Find,TEXT("WorldContextObject")));F.Set(F.Variable(M),F.Cast(Find->GetReturnValuePin(),Master->GeneratedClass));F.Merge(Missing->GetElsePin());auto* Have=F.Branch(F.IsValid(M));
    FDORRewardReceipt D(F.G,Stage);D.Add(TEXT("gate"),F.Str(Gate));D.Path(TEXT("context"),SelfPin(F.G,0,700));D.Path(TEXT("master"),M);auto* Owner=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));D.Path(TEXT("contextOwner"),Owner->GetReturnValuePin());D.Add(TEXT("ownerPhase"),F.Get(TEXT("NativeOwnerPhase")));D.Add(TEXT("observePhase"),F.Get(TEXT("NativeObservePhase")));D.Add(TEXT("bound"),F.Get(TEXT("NativeOwnerBound")));D.Add(TEXT("lifecycleReady"),F.Get(TEXT("NativeLifecycleReady")));
    for(const auto& T:DORNativeTuplePins()){auto* V=F.Input(T.Key,T.Value);D.Add(*(FString(TEXT("incoming"))+T.Key.ToString()),T.Key==TEXT("Epoch")||T.Key==TEXT("Request")?F.Text(V,TEXT("Conv_GuidToString")):V);}
    auto* Local=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"));Connect(SelfPin(F.G,0,0),FindPin(Local,TEXT("WorldContextObject")));D.Path(TEXT("localPawn"),Local->GetReturnValuePin());auto* P=F.Cast(Local->GetReturnValuePin(),APlayerCharacter::StaticClass());auto* ValidLocal=F.Branch(F.IsValid(P));auto* PS=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(P,FindPin(PS,UEdGraphSchema_K2::PN_Self));D.Path(TEXT("localState"),PS->GetReturnValuePin());auto* SC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SaveGameStateComponent"));auto* C=F.Component(PS->GetReturnValuePin(),SC);D.Path(TEXT("native"),C);auto* HaveNative=F.Branch(F.IsValid(C));auto* NativeOwner=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));Connect(C,FindPin(NativeOwner,UEdGraphSchema_K2::PN_Self));D.Path(TEXT("nativeOwner"),NativeOwner->GetReturnValuePin());F.Merge(HaveNative->GetElsePin());F.Merge(ValidLocal->GetElsePin());
    auto* Record=F.Call(Master->GeneratedClass,TEXT("DOR_DiagnosticRecordOnly"));Connect(M,FindPin(Record,UEdGraphSchema_K2::PN_Self));Connect(D.Line,FindPin(Record,TEXT("Message")));F.Merge(Have->GetElsePin());
}
void DORNativeHostArmRequire(FDORNativeGraph& F,UBlueprint* Master,UBlueprint* Request,UEdGraphPin* Condition,const TCHAR* Gate,bool ValueReturn=false)
{
    auto* B=F.Branch(Condition);auto* Passed=F.Tail;F.Tail=B->GetElsePin();auto* First=F.Branch(F.Not(F.Get(TEXT("NativeArmTraceRefused"))));F.Set(TEXT("NativeArmTraceRefused"),nullptr,TEXT("true"));DORNativeArmReceipt(F,Master,TEXT("incoming-arm-first-refusal"),Gate);F.Merge(First->GetElsePin());if(ValueReturn)F.Return();else F.Done();F.Tail=Passed;
}

bool BuildDORNativeTransport(UBlueprint* Master,UBlueprint*& Request,UBlueprint*& Context)
{
    Request=CreateObjectBlueprint(TEXT("/Game/DualOverclockReward/Runtime/BP_DOR_NativeRequest"),TEXT("BP_DOR_NativeRequest"),UObject::StaticClass());
    Context=CreateObjectBlueprint(TEXT("/Game/DualOverclockReward/Runtime/BP_DOR_NativeContext"),TEXT("BP_DOR_NativeContext"),UActorComponent::StaticClass());
    if(!Request||!Context)return false;
    const auto PS=MakeObjectPinType(AFSDPlayerState::StaticClass()),Player=MakeObjectPinType(APlayerCharacter::StaticClass());
    const auto Items=MakeStructPinType(FItemLoadout::StaticStruct()),Selections=MakeStructPinType(FItemUpgradeSelection::StaticStruct(),true);
    const auto Guid=MakeStructPinType(TBaseStructure<FGuid>::Get());
    FEdGraphPinType Float;Float.PinCategory=UEdGraphSchema_K2::PC_Float;
    for(const auto& P:DORNativeTuplePins())AddTypedVariable(Request,P.Key,P.Value);
    for(const TCHAR* N:{TEXT("Origin"),TEXT("OriginGeneration"),TEXT("Status"),TEXT("Phase")})AddTypedVariable(Request,N,MakeIntPinType());
    AddTypedVariable(Request,TEXT("Reason"),MakeStringPinType());
    for(const TCHAR* N:{TEXT("HostArmRefusalRecorded"),TEXT("ArmIssued"),TEXT("PrepareIssued")})AddTypedVariable(Request,N,MakeBoolPinType());
    AddTypedVariable(Request,TEXT("PreMutationCancelled"),MakeBoolPinType());
    AddTypedVariable(Request,TEXT("TerminalLatched"),MakeBoolPinType());AddTypedVariable(Request,TEXT("NoHostWindowCertificate"),MakeBoolPinType());
    for(const TCHAR* N:{TEXT("DispatchedAt"),TEXT("QueuedDeadline"),TEXT("StageDeadline"),TEXT("MutationDeadline"),TEXT("RecoveryDeadline")})AddTypedVariable(Request,N,Float);
    AddTypedVariable(Request,TEXT("TargetState"),PS);AddTypedVariable(Request,TEXT("TargetPlayer"),Player);
    AddTypedVariable(Request,TEXT("Character"),MakeObjectPinType(UPlayerCharacterID::StaticClass()));
    AddTypedVariable(Request,TEXT("PlayerClass"),MakeClassPinType(UObject::StaticClass()));
    AddTypedVariable(Request,TEXT("Proxy"),MakeObjectPinType(AActor::StaticClass()));
    for(const auto& P:DORNativeTuplePins())AddTypedVariable(Context,FName(*(FString(TEXT("Native"))+P.Key.ToString())),P.Value);
    for(const TCHAR* N:{TEXT("NativeLifecycleReady"),TEXT("NativeTransportReady"),TEXT("NativePreparationStopped"),TEXT("NativeOwnerRestoreChecked"),TEXT("NativeOwnerBound")})AddTypedVariable(Context,N,MakeBoolPinType());
    for(const TCHAR* N:{TEXT("NativeServerPhase"),TEXT("NativeOwnerPhase"),TEXT("NativeLastGeneration")})AddTypedVariable(Context,N,MakeIntPinType());
    AddTypedVariable(Context,TEXT("NativeArmTraceTuple"),MakeStringPinType());AddTypedVariable(Context,TEXT("NativeArmTraceRefused"),MakeBoolPinType());
    AddTypedVariable(Context,TEXT("NativeOwnerDeadline"),Float);
    AddTypedVariable(Context,TEXT("NativeObservePhase"),MakeIntPinType());AddTypedVariable(Context,TEXT("NativeProbeFirstPhase"),MakeIntPinType());AddTypedVariable(Context,TEXT("NativeExpectedActor"),MakeObjectPinType(AItem::StaticClass()));
    for(const TCHAR* N:{TEXT("NativeObservationReported"),TEXT("NativeReferencePendingReported"),TEXT("NativeDeadlineRequested")})AddTypedVariable(Context,N,MakeBoolPinType());
    AddTypedVariable(Context,TEXT("NativeMaster"),MakeObjectPinType(Master->GeneratedClass));
    for(auto* BP:{Request,Context}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);}
    if(Request->Status==BS_Error||Context->Status==BS_Error)return false;
    AddTypedVariable(Context,TEXT("NativeActiveRequest"),MakeObjectPinType(Request->GeneratedClass));
    AddTypedVariable(Context,TEXT("NativePendingHostArm"),MakeObjectPinType(Request->GeneratedClass));
    AddTypedVariable(Master,TEXT("NativeActiveRequest"),MakeObjectPinType(Request->GeneratedClass));
    AddTypedVariable(Master,TEXT("NativeRequests"),MakeObjectPinType(Request->GeneratedClass,true));
    AddTypedVariable(Master,TEXT("NativeCancelledRequests"),MakeObjectPinType(Request->GeneratedClass,true));
    AddTypedVariable(Master,TEXT("NativeOutstandingRequests"),MakeObjectPinType(Request->GeneratedClass,true));
    AddTypedVariable(Master,TEXT("NativeOwnerCandidates"),MakeObjectPinType(Request->GeneratedClass,true));
    AddTypedVariable(Master,TEXT("NativeRoundStartedAt"),Float);AddTypedVariable(Master,TEXT("NativeRoundDeadline"),Float);
    AddTypedVariable(Master,TEXT("NativeEpoch"),Guid);AddTypedVariable(Master,TEXT("NativeNextGeneration"),MakeIntPinType());
    AddTypedVariable(Master,TEXT("NativeExecutionHeld"),MakeBoolPinType());
    AddTypedVariable(Master,TEXT("NativeDriverEnabled"),MakeBoolPinType());
    AddTypedVariable(Master,TEXT("NativeRecoveryConflict"),MakeBoolPinType());
    AddTypedVariable(Master,TEXT("NativePumpRunning"),MakeBoolPinType());
    AddTypedVariable(Master,TEXT("NativeSelectedRequest"),MakeObjectPinType(Request->GeneratedClass));
    AddTypedVariable(Master,TEXT("NativeLastEnqueuedRequest"),MakeObjectPinType(Request->GeneratedClass));
    for(auto* BP:{Master,Context}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);}
    UClass* SC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SaveGameStateComponent"));if(!SC)return false;
    auto Owner=[&](FDORNativeGraph& F){auto* N=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));return N->GetReturnValuePin();};
    auto Authority=[&](FDORNativeGraph& F,UEdGraphPin* A){auto* N=F.Call(AActor::StaticClass(),TEXT("HasAuthority"));Connect(A,FindPin(N,UEdGraphSchema_K2::PN_Self));return N->GetReturnValuePin();};
    auto BindTuple=[&](FDORNativeGraph& F,UK2Node_CallFunction* N)
    {for(const auto& P:DORNativeTuplePins())Connect(F.Get(FName(*(FString(TEXT("Native"))+P.Key.ToString()))),FindPin(N,P.Key));};
    auto TupleInputs=[&](FDORNativeGraph& F){for(const auto& P:DORNativeTuplePins())F.Input(P.Key,P.Value);};
    auto EqualTuple=[&](FDORNativeGraph& F,UEdGraphPin* J)
    {
        UEdGraphPin* Result=nullptr;
        for(const auto& P:DORNativeTuplePins())
        {
            auto* A=F.Input(P.Key,P.Value);auto* B=J?F.Field(J,Request->GeneratedClass,P.Key):F.Get(FName(*(FString(TEXT("Native"))+P.Key.ToString())));
            const FName Fn=P.Key==TEXT("Key")?TEXT("EqualEqual_StrStr"):P.Key==TEXT("Epoch")||P.Key==TEXT("Request")?TEXT("EqualEqual_GuidGuid"):TEXT("EqualEqual_IntInt");
            auto* Same=F.Eq(A,B,Fn);Result=Result?F.And(Result,Same):Same;
        }
        return Result;
    };
    // These endpoints deliberately fail closed until the shared execution
    // builder installs the native phase bodies. The old producers are not
    // routed here by the transport builder alone.
    for(const TCHAR* N:{TEXT("DOR_NativeTransportReady"),TEXT("DOR_NativePreparationFinished"),TEXT("DOR_NativePreparationStopped"),TEXT("DOR_NativeOwnerObserved"),TEXT("DOR_NativeOwnerCleaned")})
    {FDORNativeGraph F(Context,N,Master->GeneratedClass);TupleInputs(F);F.Return();F.Compile();}
    {
        FDORNativeGraph F(Context,TEXT("DOR_NativeTupleMatches"),Master->GeneratedClass);TupleInputs(F);F.Return(EqualTuple(F,nullptr));F.Compile();
    }
    {
        FDORNativeGraph F(Context,TEXT("DOR_NativeServerEnvelope"),Master->GeneratedClass);TupleInputs(F);auto* PreCaptureCancellation=F.Input(TEXT("AllowPreCaptureCancellation"),MakeBoolPinType());PreCaptureCancellation->DefaultValue=TEXT("false");for(auto& Pin:F.Entry->UserDefinedPins)if(Pin->PinName==TEXT("AllowPreCaptureCancellation"))Pin->PinDefaultValue=TEXT("false");
        F.Require(Authority(F,Owner(F)),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=not-authority"));
        auto* J=F.Get(TEXT("NativeActiveRequest"));auto* M=F.Get(TEXT("NativeMaster"));
        F.Require(F.And(F.IsValid(J),F.IsValid(M)),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=no-active-context"));
        auto* Admitted=F.Array(TEXT("Array_Contains"),F.Field(M,Master->GeneratedClass,TEXT("NativeOutstandingRequests")));Connect(J,FindPin(Admitted,TEXT("ItemToFind")));F.Require(Admitted->GetReturnValuePin(),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=not-common-admitted"));
        // Independent owner prep has its own PlayerState context. Only host
        // construction receipts require ownership of the shared local window.
        auto* LateTerminal=F.And(F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Status")),F.Int(6)),F.And(F.Field(J,Request->GeneratedClass,TEXT("TerminalLatched")),F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Phase")),F.Int(8))));auto* Constructing=F.Branch(F.Eq(F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Status")),F.Int(2)),F.And(LateTerminal,F.Not(F.Field(J,Request->GeneratedClass,TEXT("NoHostWindowCertificate")))),TEXT("BooleanOR")));
        F.Require(F.Same(J,F.Field(M,Master->GeneratedClass,TEXT("NativeActiveRequest"))),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=wrong-admitted-request"));
        F.Require(F.Field(M,Master->GeneratedClass,TEXT("NativeExecutionHeld")),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=execution-not-held"));F.Merge(Constructing->GetElsePin());
        auto* NormalReceipt=F.Eq(F.Eq(F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Status")),F.Int(1)),F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Status")),F.Int(2)),TEXT("BooleanOR")),LateTerminal,TEXT("BooleanOR"));auto* PreCaptureLate=F.And(PreCaptureCancellation,F.And(F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Status")),F.Int(6)),F.And(F.Not(F.Field(J,Request->GeneratedClass,TEXT("TerminalLatched"))),F.Eq(F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Phase")),F.Int(2)),F.Eq(F.Field(J,Request->GeneratedClass,TEXT("Phase")),F.Int(91)),TEXT("BooleanOR")))));F.Require(F.Eq(NormalReceipt,PreCaptureLate,TEXT("BooleanOR")),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=request-already-settled"));
        F.Require(F.And(EqualTuple(F,J),EqualTuple(F,nullptr)),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=stale-key-epoch-generation-request-slot-code"));
        F.Require(F.Same(Owner(F),F.Field(J,Request->GeneratedClass,TEXT("TargetState"))),TEXT("[DOR-NATIVE]|stage=receipt-rejected|condition=wrong-playerstate-instance"));
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    auto* EG=FBlueprintEditorUtils::FindEventGraph(Context);
    auto* Begin=AddNode<UK2Node_Event>(EG,0,0);Begin->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"),UActorComponent::StaticClass());Begin->bOverrideFunction=true;Begin->AllocateDefaultPins();
    auto* Ready=AddVariableSet(EG,TEXT("NativeLifecycleReady"),250,0);FindPin(Ready,TEXT("NativeLifecycleReady"))->DefaultValue=TEXT("true");Connect(Begin->FindPin(UEdGraphSchema_K2::PN_Then),Ready->GetExecPin());
    auto Event=[&](const TCHAR* N,bool Server,const TArray<TPair<FName,FEdGraphPinType>>& Extra)
    {
        auto* E=AddCustomEvent(EG,N,FUNC_Net|FUNC_NetReliable|FUNC_BlueprintCallable|(Server?FUNC_NetServer:FUNC_NetClient),0,500+EG->Nodes.Num()*30);
        for(const auto& P:DORNativeTuplePins())E->CreateUserDefinedPin(P.Key,P.Value,EGPD_Output);
        for(const auto& P:Extra)E->CreateUserDefinedPin(P.Key,P.Value,EGPD_Output);E->ReconstructNode();return E;
    };
    AddTypedVariable(Context,TEXT("NativeDebugBatchRun"),MakeStringPinType());AddTypedVariable(Context,TEXT("NativeDebugBatchCase"),MakeStringPinType());
    auto* Arm=Event(TEXT("DOR_NativeClientArm"),false,{TPair<FName,FEdGraphPinType>(TEXT("RemainingDuration"),Float),TPair<FName,FEdGraphPinType>(TEXT("DebugBatchRun"),MakeStringPinType()),TPair<FName,FEdGraphPinType>(TEXT("DebugBatchCase"),MakeStringPinType())});
    auto* Transport=Event(TEXT("DOR_NativeServerTransportReady"),true,{});
    auto ExtraPin=[](const TCHAR* N,const FEdGraphPinType& T){return TPair<FName,FEdGraphPinType>(N,T);};
    auto* Prepared=Event(TEXT("DOR_NativeServerPrepFinished"),true,{ExtraPin(TEXT("OriginalLoadout"),Items),ExtraPin(TEXT("OriginalSelections"),Selections),ExtraPin(TEXT("Loadout"),Items),ExtraPin(TEXT("Selections"),Selections),ExtraPin(TEXT("Accepted"),MakeBoolPinType()),ExtraPin(TEXT("JournalToken"),MakeStringPinType()),ExtraPin(TEXT("NativeSubmitted"),MakeBoolPinType()),ExtraPin(TEXT("NoLocalResources"),MakeBoolPinType()),ExtraPin(TEXT("LegacyPrepared"),MakeBoolPinType())});
    auto* Stop=Event(TEXT("DOR_NativeServerPrepStopped"),true,{ExtraPin(TEXT("Loadout"),Items),ExtraPin(TEXT("Selections"),Selections),ExtraPin(TEXT("EquipmentChecked"),MakeBoolPinType()),ExtraPin(TEXT("NativeSubmitted"),MakeBoolPinType()),ExtraPin(TEXT("OriginalUnchanged"),MakeBoolPinType()),ExtraPin(TEXT("JournalToken"),MakeStringPinType())});
    auto* Observed=Event(TEXT("DOR_NativeServerOwnerObserved"),true,{ExtraPin(TEXT("Phase"),MakeIntPinType()),ExtraPin(TEXT("Passed"),MakeBoolPinType()),ExtraPin(TEXT("ReferencePending"),MakeBoolPinType())});
    auto* Cleaned=Event(TEXT("DOR_NativeServerOwnerCleaned"),true,{ExtraPin(TEXT("Phase"),MakeIntPinType()),ExtraPin(TEXT("Passed"),MakeBoolPinType()),ExtraPin(TEXT("JournalToken"),MakeStringPinType())});
    // Owner commands are declared here so the execution builder can install
    // their bodies without introducing another RPC transport or coordinator.
    Event(TEXT("DOR_NativeClientPrepare"),false,{ExtraPin(TEXT("RemainingDuration"),Float)});
    Event(TEXT("DOR_NativeClientStop"),false,{ExtraPin(TEXT("RemainingDuration"),Float),ExtraPin(TEXT("AuthorityNativeSubmitted"),MakeBoolPinType())});
    Event(TEXT("DOR_NativeClientCleanup"),false,{ExtraPin(TEXT("Phase"),MakeIntPinType())});
    Event(TEXT("DOR_NativeClientFinalize"),false,{ExtraPin(TEXT("Success"),MakeBoolPinType())});
    Event(TEXT("DOR_NativeServerOwnerDeadline"),true,{});
    Event(TEXT("DOR_NativeClientObserve"),false,{ExtraPin(TEXT("Phase"),MakeIntPinType()),ExtraPin(TEXT("Expected"),MakeObjectPinType(AItem::StaticClass())),ExtraPin(TEXT("Loadout"),Items),ExtraPin(TEXT("Selections"),Selections)});
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context);FKismetEditorUtilities::CompileBlueprint(Context);
    auto Wire=[&](UK2Node_CustomEvent* E,const TCHAR* N)
    {auto* C=AddSelfCall(EG,N,600,E->NodePosY);Connect(E->FindPin(UEdGraphSchema_K2::PN_Then),C->GetExecPin());for(auto* P:E->Pins)if(P->Direction==EGPD_Output&&P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec&&C->FindPin(P->PinName))Connect(P,FindPin(C,P->PinName));};
    {
        FDORNativeGraph F(Context,TEXT("DOR_NativeAcceptArm"),Master->GeneratedClass);TupleInputs(F);auto* Remaining=F.Input(TEXT("RemainingDuration"),Float);
        auto* IncomingToken=F.Cache(TEXT("IncomingToken"),F.Str(TEXT("")));for(const auto& T:DORNativeTuplePins()){auto* V=F.Input(T.Key,T.Value);F.Set(F.Variable(IncomingToken),F.Join(F.Join(IncomingToken,F.Str(TEXT("|"))),T.Key==TEXT("Key")?V:F.Text(V,T.Key==TEXT("Epoch")||T.Key==TEXT("Request")?TEXT("Conv_GuidToString"):TEXT("Conv_IntToString"))));}auto* NewIncoming=F.Branch(F.Not(F.Eq(IncomingToken,F.Get(TEXT("NativeArmTraceTuple")),TEXT("EqualEqual_StrStr"))));F.Set(TEXT("NativeArmTraceTuple"),IncomingToken);F.Set(TEXT("NativeArmTraceRefused"),nullptr,TEXT("false"));DORNativeArmReceipt(F,Master,TEXT("incoming-arm"),TEXT("entry"));F.Merge(NewIncoming->GetElsePin());
        const TCHAR* Gates[]={TEXT("accept-duration"),TEXT("accept-lifecycle-ready"),TEXT("accept-replicating-context"),TEXT("accept-owner-idle"),TEXT("accept-local-player"),TEXT("accept-owner-playerstate"),TEXT("accept-registered-context"),TEXT("accept-native-component"),TEXT("accept-native-owner"),TEXT("accept-not-space-rig"),TEXT("accept-master"),TEXT("accept-ready-stable-key"),TEXT("accept-fresh-epoch-or-generation")};int32 Gate=0;auto Require=[&](UEdGraphPin* Condition){DORNativeHostArmRequire(F,Master,Request,Condition,Gates[Gate++]);};
        auto* Duration=F.Call(UKismetMathLibrary::StaticClass(),TEXT("InRange_FloatFloat"));Connect(Remaining,FindPin(Duration,TEXT("Value")));FindPin(Duration,TEXT("Min"))->DefaultValue=TEXT("0");FindPin(Duration,TEXT("Max"))->DefaultValue=TEXT("10");FindPin(Duration,TEXT("InclusiveMin"))->DefaultValue=TEXT("false");FindPin(Duration,TEXT("InclusiveMax"))->DefaultValue=TEXT("true");Require(Duration->GetReturnValuePin());
        Require(F.Get(TEXT("NativeLifecycleReady")));
        auto* Replicating=F.Field(SelfPin(F.G,0,700),UActorComponent::StaticClass(),TEXT("bReplicates"));
        auto* LocalAuthority=Authority(F,Owner(F));
        Require(F.Eq(F.And(LocalAuthority,F.Not(Replicating)),F.And(F.Not(LocalAuthority),Replicating),TEXT("BooleanOR")));
        Require(F.Eq(F.Get(TEXT("NativeOwnerPhase")),F.Int(0)));
        auto* Local=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"));Connect(SelfPin(F.G,0,700),FindPin(Local,TEXT("WorldContextObject")));
        auto* P=F.Cast(Local->GetReturnValuePin(),APlayerCharacter::StaticClass());Require(F.IsValid(P));
        auto* State=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(P,FindPin(State,UEdGraphSchema_K2::PN_Self));
        auto* PSValue=F.Cache(TEXT("State"),State->GetReturnValuePin());Require(F.Same(Owner(F),PSValue));
        Require(F.Same(F.Component(PSValue,Context->GeneratedClass),SelfPin(F.G,0,700)));
        auto* Native=F.Component(PSValue,SC);Require(F.IsValid(Native));
        auto* NativeOwner=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));Connect(Native,FindPin(NativeOwner,UEdGraphSchema_K2::PN_Self));Require(F.Same(PSValue,NativeOwner->GetReturnValuePin()));
        Require(F.Not(F.Field(PSValue,AFSDPlayerState::StaticClass(),TEXT("IsOnSpaceRig"))));
        auto* FindMaster=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(FindMaster,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;Connect(SelfPin(F.G,0,700),FindPin(FindMaster,TEXT("WorldContextObject")));
        F.Set(TEXT("NativeMaster"),F.Cast(FindMaster->GetReturnValuePin(),Master->GeneratedClass));Require(F.IsValid(F.Get(TEXT("NativeMaster"))));
        auto* Identity=F.Call(Master->GeneratedClass,TEXT("DOR_ResolveStablePlayerKey"));Connect(F.Get(TEXT("NativeMaster")),FindPin(Identity,UEdGraphSchema_K2::PN_Self));Connect(P,FindPin(Identity,TEXT("Player")));
        Require(F.And(FindPin(Identity,TEXT("Ready")),F.Eq(FindPin(Identity,TEXT("Key")),F.Input(TEXT("Key"),MakeStringPinType()),TEXT("EqualEqual_StrStr"))));
        auto* Fresh=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(F.Input(TEXT("Generation"),MakeIntPinType()),FindPin(Fresh,TEXT("A")));Connect(F.Get(TEXT("NativeLastGeneration")),FindPin(Fresh,TEXT("B")));
        // A replaced Master starts a new epoch, but an old request may not
        // replace an outstanding owner context. The idle gate above is shared.
        auto* DifferentEpoch=F.Not(F.Eq(F.Get(TEXT("NativeEpoch")),F.Input(TEXT("Epoch"),Guid),TEXT("EqualEqual_GuidGuid")));
        Require(F.Eq(DifferentEpoch,Fresh->GetReturnValuePin(),TEXT("BooleanOR")));
        for(const auto& Pin:DORNativeTuplePins())F.Set(FName(*(FString(TEXT("Native"))+Pin.Key.ToString())),F.Input(Pin.Key,Pin.Value));
        // Accepted new tuple only: never clear a protected predecessor on ingress.
        F.Set(TEXT("NativeObservePhase"),nullptr,TEXT("0"));F.Set(TEXT("NativeExpectedActor"),nullptr);
        F.Set(TEXT("NativeObservationReported"),nullptr,TEXT("false"));F.Set(TEXT("NativeReferencePendingReported"),nullptr,TEXT("false"));F.Set(TEXT("NativeProbeFirstPhase"),nullptr,TEXT("0"));F.Set(TEXT("NativeDeadlineRequested"),nullptr,TEXT("false"));
        F.Set(TEXT("NativeLastGeneration"),F.Input(TEXT("Generation"),MakeIntPinType()));F.Set(TEXT("NativeOwnerPhase"),nullptr,TEXT("1"));F.Set(TEXT("NativeOwnerBound"),nullptr,TEXT("false"));
        F.Set(TEXT("NativeDebugBatchRun"),F.Input(TEXT("DebugBatchRun"),MakeStringPinType()));F.Set(TEXT("NativeDebugBatchCase"),F.Input(TEXT("DebugBatchCase"),MakeStringPinType()));
        auto* Deadline=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_FloatFloat"));Connect(F.RealNow(),FindPin(Deadline,TEXT("A")));Connect(Remaining,FindPin(Deadline,TEXT("B")));F.Set(TEXT("NativeOwnerDeadline"),Deadline->GetReturnValuePin());
        auto* Ack=F.Self(TEXT("DOR_NativeServerTransportReady"));BindTuple(F,Ack);F.Done();F.Compile();Wire(Arm,TEXT("DOR_NativeAcceptArm"));
    }
    {
        FDORNativeGraph F(Context,TEXT("DOR_NativeAcceptReady"),Master->GeneratedClass);TupleInputs(F);
        auto* Check=F.Self(TEXT("DOR_NativeServerEnvelope"));for(const auto& P:DORNativeTuplePins())Connect(F.Input(P.Key,P.Value),FindPin(Check,P.Key));F.QuietRequire(Check->GetReturnValuePin());
        F.QuietRequire(F.Eq(F.Get(TEXT("NativeServerPhase")),F.Int(1)));
        F.QuietRequire(F.Not(F.Get(TEXT("NativeTransportReady"))));F.Set(TEXT("NativeTransportReady"),nullptr,TEXT("true"));
        auto* Next=F.Self(TEXT("DOR_NativeTransportReady"));BindTuple(F,Next);F.Done();F.Compile();Wire(Transport,TEXT("DOR_NativeAcceptReady"));
    }
    for(const auto& E:{TPair<UK2Node_CustomEvent*,const TCHAR*>(Prepared,TEXT("DOR_NativePreparationFinished")),TPair<UK2Node_CustomEvent*,const TCHAR*>(Stop,TEXT("DOR_NativePreparationStopped")),TPair<UK2Node_CustomEvent*,const TCHAR*>(Observed,TEXT("DOR_NativeOwnerObserved")),TPair<UK2Node_CustomEvent*,const TCHAR*>(Cleaned,TEXT("DOR_NativeOwnerCleaned"))})
    {
        FDORNativeGraph F(Context,E.Value,Master->GeneratedClass);TupleInputs(F);
        for(auto* P:E.Key->Pins)if(P->Direction==EGPD_Output&&P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec&&P->PinName!=TEXT("OutputDelegate"))F.Input(P->PinName,P->PinType);
        auto* Check=F.Self(TEXT("DOR_NativeServerEnvelope"));for(const auto& P:DORNativeTuplePins())Connect(F.Input(P.Key,P.Value),FindPin(Check,P.Key));F.Require(Check->GetReturnValuePin());
        // No receipt is minted by an incomplete execution adapter.
        F.Return();F.Compile();Wire(E.Key,E.Value);
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeGetTransport"),Master->GeneratedClass);auto* State=F.Input(TEXT("PlayerState"),PS);
        auto* Result=F.Cache(TEXT("Context"),F.Component(State,Context->GeneratedClass));
        AddTypedVariable(Master,TEXT("NativeNullTransport"),MakeObjectPinType(Context->GeneratedClass));F.Set(TEXT("NativeNullTransport"),nullptr);auto* Null=F.Get(TEXT("NativeNullTransport"));
        F.QuietObjectRequire(F.And(F.IsValid(State),Authority(F,State)),Null);
        auto* Existing=F.Branch(F.IsValid(Result));F.Return(Result);F.Tail=Existing->GetElsePin();
        auto* Add=AddNode<UK2Node_AddComponentByClass>(F.G,++F.Serial*160,0);Add->AllocateDefaultPins();FindPin(Add,TEXT("Class"))->DefaultObject=Context->GeneratedClass;Add->ReconstructNode();
        Connect(State,FindPin(Add,UEdGraphSchema_K2::PN_Self));F.Run(Add);
        F.Set(F.Variable(Result),F.Cast(FindPin(Add,UEdGraphSchema_K2::PN_ReturnValue),Context->GeneratedClass));F.QuietObjectRequire(F.IsValid(Result),Null);
        auto* Replicate=F.Call(UActorComponent::StaticClass(),TEXT("SetIsReplicated"));Connect(Result,FindPin(Replicate,UEdGraphSchema_K2::PN_Self));FindPin(Replicate,TEXT("ShouldReplicate"))->DefaultValue=TEXT("false");
        F.Put(Result,Context->GeneratedClass,TEXT("NativeMaster"),SelfPin(F.G,0,700));F.Return(Result);F.Compile();
    }
    for(auto* BP:{Request,Context,Master}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    auto* CDO=Cast<UActorComponent>(Context->GeneratedClass->GetDefaultObject());if(!CDO)return false;
    CDO->SetIsReplicated(false);CDO->PrimaryComponentTick.bTickEvenWhenPaused=true;
    return true;
}

bool BuildDORNativeAdmission(UBlueprint* Master,UBlueprint* Request)
{
    const auto RequestType=MakeObjectPinType(Request->GeneratedClass),PlayerType=MakeObjectPinType(APlayerCharacter::StaticClass());
    FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Float;
    // Status: 0 queued, 1 selected, 2 executing, 3 success, 4 checked cancel,
    // 5 cancelled before mutation, 6 unresolved recovery (barrier retained).
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeEarlierPlayerPending"),Master->GeneratedClass);auto* R=F.Input(TEXT("RequestObject"),RequestType);F.Require(F.IsValid(R));
        auto* Result=F.Cache(TEXT("Pending"),F.Get(TEXT("NativeRecoveryConflict")));auto* False=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));FindPin(False,TEXT("A"))->DefaultValue=TEXT("0");FindPin(False,TEXT("B"))->DefaultValue=TEXT("1");F.Set(F.Variable(Result),False->GetReturnValuePin());
        auto* Loop=F.Each(F.Get(TEXT("NativeOutstandingRequests")));auto* Earlier=FindPin(Loop,TEXT("Array Element"));auto* Valid=F.Branch(F.IsValid(Earlier));
        auto* First=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));Connect(F.Field(Earlier,Request->GeneratedClass,TEXT("Generation")),FindPin(First,TEXT("A")));Connect(F.Field(R,Request->GeneratedClass,TEXT("Generation")),FindPin(First,TEXT("B")));
        auto* Unsettled=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));Connect(F.Field(Earlier,Request->GeneratedClass,TEXT("Status")),FindPin(Unsettled,TEXT("A")));FindPin(Unsettled,TEXT("B"))->DefaultValue=TEXT("3");
        auto* IdlePending=F.And(F.Eq(F.Field(Earlier,Request->GeneratedClass,TEXT("Status")),F.Int(5)),F.Not(F.Field(Earlier,Request->GeneratedClass,TEXT("PreMutationCancelled"))));auto* Pending=F.Eq(Unsettled->GetReturnValuePin(),F.Eq(F.Eq(F.Field(Earlier,Request->GeneratedClass,TEXT("Status")),F.Int(6)),IdlePending,TEXT("BooleanOR")),TEXT("BooleanOR"));
        auto* SamePlayer=F.Same(F.Field(Earlier,Request->GeneratedClass,TEXT("TargetState")),F.Field(R,Request->GeneratedClass,TEXT("TargetState")));
        auto* Block=F.Branch(F.And(First->GetReturnValuePin(),F.And(SamePlayer,Pending)));F.Set(F.Variable(Result),nullptr,TEXT("true"));F.Merge(Block->GetElsePin());F.Merge(Valid->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(Result);F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeAdmitOwnersMetadata"),Master->GeneratedClass);auto* At=F.Input(TEXT("AtTime"),FloatType);
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("NativeOwnerCandidates")));auto* Loop=F.Each(F.Get(TEXT("NativeRequests")));auto* R=FindPin(Loop,TEXT("Array Element"));auto* Valid=F.Branch(F.IsValid(R));auto* Queued=F.Branch(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("Status")),F.Int(0)));
        auto* Expired=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_FloatFloat"));Connect(At,FindPin(Expired,TEXT("A")));Connect(F.Field(R,Request->GeneratedClass,TEXT("QueuedDeadline")),FindPin(Expired,TEXT("B")));
        auto* Cancel=F.Branch(F.Eq(Expired->GetReturnValuePin(),F.Get(TEXT("NativeRecoveryConflict")),TEXT("BooleanOR")));F.Put(R,Request->GeneratedClass,TEXT("Status"),nullptr,TEXT("5"));F.Put(R,Request->GeneratedClass,TEXT("Reason"),nullptr,TEXT("round-deadline-before-preparation"));auto* Add=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("NativeCancelledRequests")));Connect(R,FindPin(Add,TEXT("NewItem")));auto* Cancelled=F.Tail;
        F.Tail=Cancel->GetElsePin();auto* Earlier=F.Self(TEXT("DOR_NativeEarlierPlayerPending"));Connect(R,FindPin(Earlier,TEXT("RequestObject")));auto* Free=F.Branch(F.Not(Earlier->GetReturnValuePin()));F.Put(R,Request->GeneratedClass,TEXT("Status"),nullptr,TEXT("1"));F.Put(R,Request->GeneratedClass,TEXT("Phase"),nullptr,TEXT("1"));Add=F.Array(TEXT("Array_Add"),F.Get(TEXT("NativeOwnerCandidates")));Connect(R,FindPin(Add,TEXT("NewItem")));F.Merge(Free->GetElsePin());F.Merge(Cancelled);F.Merge(Queued->GetElsePin());F.Merge(Valid->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(F.Get(TEXT("NativeOwnerCandidates")));F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeSelectReadyMetadata"),Master->GeneratedClass);auto* At=F.Input(TEXT("AtTime"),FloatType);auto* Held=F.Input(TEXT("ExecutionHeld"),MakeBoolPinType());
        F.Set(TEXT("NativeSelectedRequest"),nullptr);auto* Loop=F.Each(F.Get(TEXT("NativeRequests")));auto* R=FindPin(Loop,TEXT("Array Element"));auto* Valid=F.Branch(F.IsValid(R));auto* Ready=F.Branch(F.And(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("Status")),F.Int(1)),F.Eq(F.Field(R,Request->GeneratedClass,TEXT("Phase")),F.Int(3))));
        auto* Expired=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_FloatFloat"));Connect(At,FindPin(Expired,TEXT("A")));Connect(F.Field(R,Request->GeneratedClass,TEXT("QueuedDeadline")),FindPin(Expired,TEXT("B")));auto* Stop=F.Branch(F.Eq(Expired->GetReturnValuePin(),F.Get(TEXT("NativeRecoveryConflict")),TEXT("BooleanOR")));
        // Preparation may already own an account journal: expiry requests stop,
        // not a false before-mutation settlement or an unsafe resource unlock.
        F.Put(R,Request->GeneratedClass,TEXT("Phase"),nullptr,TEXT("91"));F.Put(R,Request->GeneratedClass,TEXT("Reason"),nullptr,TEXT("round-deadline-owner-stop-required"));auto* Stopped=F.Tail;
        F.Tail=Stop->GetElsePin();auto* Earlier=F.Self(TEXT("DOR_NativeEarlierPlayerPending"));Connect(R,FindPin(Earlier,TEXT("RequestObject")));auto* Select=F.Branch(F.And(F.Not(Held),F.And(F.Not(F.IsValid(F.Get(TEXT("NativeSelectedRequest")))),F.Not(Earlier->GetReturnValuePin()))));F.Set(TEXT("NativeSelectedRequest"),R);F.Put(R,Request->GeneratedClass,TEXT("Status"),nullptr,TEXT("2"));F.Put(R,Request->GeneratedClass,TEXT("Phase"),nullptr,TEXT("4"));F.Merge(Select->GetElsePin());F.Merge(Stopped);F.Merge(Ready->GetElsePin());F.Merge(Valid->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(F.Get(TEXT("NativeSelectedRequest")));F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeRequestCurrent"),Master->GeneratedClass);auto* R=F.Input(TEXT("RequestObject"),RequestType);
        F.Require(F.IsValid(R));auto* P=F.Field(R,Request->GeneratedClass,TEXT("TargetPlayer"));auto* PS=F.Field(R,Request->GeneratedClass,TEXT("TargetState"));
        F.Require(F.And(F.IsValid(P),F.IsValid(PS)));F.Require(F.Eq(F.Get(TEXT("NativeEpoch")),F.Field(R,Request->GeneratedClass,TEXT("Epoch")),TEXT("EqualEqual_GuidGuid")));
        auto* State=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(P,FindPin(State,UEdGraphSchema_K2::PN_Self));F.Require(F.Same(State->GetReturnValuePin(),PS));
        auto* Character=F.Call(AFSDPlayerState::StaticClass(),TEXT("GetSelectedCharacterID"));Connect(PS,FindPin(Character,UEdGraphSchema_K2::PN_Self));F.Require(F.Same(Character->GetReturnValuePin(),F.Field(R,Request->GeneratedClass,TEXT("Character"))));
        auto* Class=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(P,FindPin(Class,TEXT("Object")));F.Require(F.Eq(Class->GetReturnValuePin(),F.Field(R,Request->GeneratedClass,TEXT("PlayerClass")),TEXT("EqualEqual_ClassClass")));
        F.Require(F.Not(F.Field(PS,AFSDPlayerState::StaticClass(),TEXT("IsOnSpaceRig"))));
        auto* Key=F.Self(TEXT("DOR_ResolveStablePlayerKey"));Connect(P,FindPin(Key,TEXT("Player")));F.Require(F.And(FindPin(Key,TEXT("Ready")),F.Eq(FindPin(Key,TEXT("Key")),F.Field(R,Request->GeneratedClass,TEXT("Key")),TEXT("EqualEqual_StrStr"))));
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeHasRoundWork"),Master->GeneratedClass);auto* Work=F.Cache(TEXT("Work"),F.Get(TEXT("NativeExecutionHeld")));auto* Loop=F.Each(F.Get(TEXT("NativeOutstandingRequests")));auto* R=FindPin(Loop,TEXT("Array Element"));auto* Valid=F.Branch(F.IsValid(R));auto* Pending=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));Connect(F.Field(R,Request->GeneratedClass,TEXT("Status")),FindPin(Pending,TEXT("A")));FindPin(Pending,TEXT("B"))->DefaultValue=TEXT("3");auto* Active=F.Branch(Pending->GetReturnValuePin());F.Set(F.Variable(Work),nullptr,TEXT("true"));F.Merge(Active->GetElsePin());F.Merge(Valid->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(Work);F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeEnqueue"),Master->GeneratedClass);auto* P=F.Input(TEXT("Player"),PlayerType);auto* Proxy=F.Input(TEXT("Proxy"),MakeObjectPinType(AActor::StaticClass()));
        auto* Code=F.Input(TEXT("Code"),MakeIntPinType());auto* Slot=F.Input(TEXT("Slot"),MakeIntPinType());auto* Origin=F.Input(TEXT("Origin"),MakeIntPinType());auto* OriginGeneration=F.Input(TEXT("OriginGeneration"),MakeIntPinType());
        F.Require(DORLocalHost(F),TEXT("[DOR-NATIVE]|stage=enqueue-rejected|condition=authority-only"));
        F.Require(F.IsValid(P));F.Require(F.Not(F.Get(TEXT("NativeRecoveryConflict"))),TEXT("[DOR-NATIVE]|stage=enqueue-rejected|condition=shared-recovery-barrier"));
        F.Require(F.Eq(F.Eq(Slot,F.Int(0)),F.Eq(Slot,F.Int(1)),TEXT("BooleanOR")),TEXT("[DOR-NATIVE]|stage=enqueue-rejected|condition=unsupported-slot"));
        auto* Capacity=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));Connect(F.Array(TEXT("Array_Length"),F.Get(TEXT("NativeRequests")))->GetReturnValuePin(),FindPin(Capacity,TEXT("A")));FindPin(Capacity,TEXT("B"))->DefaultValue=TEXT("32");F.Require(Capacity->GetReturnValuePin(),TEXT("[DOR-NATIVE]|stage=enqueue-rejected|condition=queue-capacity"));
        auto* Counter=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));Connect(F.Get(TEXT("NativeNextGeneration")),FindPin(Counter,TEXT("A")));FindPin(Counter,TEXT("B"))->DefaultValue=TEXT("2147483646");F.Require(Counter->GetReturnValuePin(),TEXT("[DOR-NATIVE]|stage=enqueue-rejected|condition=generation-exhausted"));
        auto* State=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(P,FindPin(State,UEdGraphSchema_K2::PN_Self));auto* PS=F.Cache(TEXT("TargetState"),State->GetReturnValuePin());F.Require(F.IsValid(PS));F.Require(F.Not(F.Field(PS,AFSDPlayerState::StaticClass(),TEXT("IsOnSpaceRig"))));
        auto* Identity=F.Self(TEXT("DOR_ResolveStablePlayerKey"));Connect(P,FindPin(Identity,TEXT("Player")));F.Require(FindPin(Identity,TEXT("Ready")));auto* Key=F.Cache(TEXT("Key"),FindPin(Identity,TEXT("Key")));
        auto* IsEpoch=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(F.Get(TEXT("NativeEpoch")),FindPin(IsEpoch,TEXT("InGuid")));auto* NewEpoch=F.Branch(F.Not(IsEpoch->GetReturnValuePin()));F.Set(TEXT("NativeEpoch"),F.Call(UKismetGuidLibrary::StaticClass(),TEXT("NewGuid"))->GetReturnValuePin());auto* Created=F.Tail;F.Tail=NewEpoch->GetElsePin();F.Merge(Created);
        auto* Next=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(F.Get(TEXT("NativeNextGeneration")),FindPin(Next,TEXT("A")));FindPin(Next,TEXT("B"))->DefaultValue=TEXT("1");F.Set(TEXT("NativeNextGeneration"),Next->GetReturnValuePin());
        auto* Construct=AddNode<UK2Node_GenericCreateObject>(F.G,++F.Serial*160,0);Construct->AllocateDefaultPins();Construct->GetClassPin()->DefaultObject=Request->GeneratedClass;Construct->ReconstructNode();Connect(SelfPin(F.G,0,700),Construct->GetOuterPin());F.Run(Construct);auto* R=F.Cache(TEXT("Request"),Construct->GetResultPin());F.Require(F.IsValid(R));
        F.Put(R,Request->GeneratedClass,TEXT("Key"),Key);F.Put(R,Request->GeneratedClass,TEXT("Epoch"),F.Get(TEXT("NativeEpoch")));F.Put(R,Request->GeneratedClass,TEXT("Generation"),F.Get(TEXT("NativeNextGeneration")));F.Put(R,Request->GeneratedClass,TEXT("Request"),F.Call(UKismetGuidLibrary::StaticClass(),TEXT("NewGuid"))->GetReturnValuePin());
        F.Put(R,Request->GeneratedClass,TEXT("TargetState"),PS);F.Put(R,Request->GeneratedClass,TEXT("TargetPlayer"),P);F.Put(R,Request->GeneratedClass,TEXT("Proxy"),Proxy);F.Put(R,Request->GeneratedClass,TEXT("Code"),Code);F.Put(R,Request->GeneratedClass,TEXT("Slot"),Slot);F.Put(R,Request->GeneratedClass,TEXT("Origin"),Origin);F.Put(R,Request->GeneratedClass,TEXT("OriginGeneration"),OriginGeneration);
        auto* ClearOperation=F.Branch(F.Eq(Origin,F.Int(15)));F.Put(R,Request->GeneratedClass,TEXT("Operation"),nullptr,TEXT("1"));F.Merge(ClearOperation->GetElsePin());
        auto* Character=F.Call(AFSDPlayerState::StaticClass(),TEXT("GetSelectedCharacterID"));Connect(PS,FindPin(Character,UEdGraphSchema_K2::PN_Self));F.Put(R,Request->GeneratedClass,TEXT("Character"),Character->GetReturnValuePin());auto* Class=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(P,FindPin(Class,TEXT("Object")));F.Put(R,Request->GeneratedClass,TEXT("PlayerClass"),Class->GetReturnValuePin());
        auto* Start=F.Branch(F.Not(F.Self(TEXT("DOR_NativeHasRoundWork"))->GetReturnValuePin()));
        F.Set(TEXT("NativeRoundStartedAt"),F.RealNow());auto* Deadline=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_FloatFloat"));Connect(F.Get(TEXT("NativeRoundStartedAt")),FindPin(Deadline,TEXT("A")));FindPin(Deadline,TEXT("B"))->DefaultValue=TEXT("10.0");F.Set(TEXT("NativeRoundDeadline"),Deadline->GetReturnValuePin());F.Merge(Start->GetElsePin());
        auto* InTime=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_FloatFloat"));Connect(F.RealNow(),FindPin(InTime,TEXT("A")));Connect(F.Get(TEXT("NativeRoundDeadline")),FindPin(InTime,TEXT("B")));F.Require(InTime->GetReturnValuePin(),TEXT("[DOR-NATIVE]|stage=enqueue-rejected|condition=round-absolute-deadline"));
        F.Put(R,Request->GeneratedClass,TEXT("DispatchedAt"),F.RealNow());F.Put(R,Request->GeneratedClass,TEXT("QueuedDeadline"),F.Get(TEXT("NativeRoundDeadline")));
        auto* Add=F.Array(TEXT("Array_Add"),F.Get(TEXT("NativeRequests")));Connect(R,FindPin(Add,TEXT("NewItem")));Add=F.Array(TEXT("Array_Add"),F.Get(TEXT("NativeOutstandingRequests")));Connect(R,FindPin(Add,TEXT("NewItem")));F.Set(TEXT("NativeLastEnqueuedRequest"),R);F.Return(nullptr,TEXT("true"));F.Compile();
    }
    // Cancellation runs while another request owns the execution window too.
    // Selection only touches metadata; originals are captured by execution.
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeSelectMetadata"),Master->GeneratedClass);auto* At=F.Input(TEXT("AtTime"),FloatType);auto* Held=F.Input(TEXT("ExecutionHeld"),MakeBoolPinType());auto* Blocked=F.Input(TEXT("Blocked"),MakeBoolPinType());
        F.Set(TEXT("NativeSelectedRequest"),nullptr);auto* Queue=F.Cache(TEXT("Queue"),F.Get(TEXT("NativeRequests")));auto* Remaining=F.Cache(TEXT("Remaining"),Queue);F.Array(TEXT("Array_Clear"),Remaining);
        auto* Loop=F.Each(Queue);auto* R=FindPin(Loop,TEXT("Array Element"));auto* Valid=F.Branch(F.IsValid(R));auto* Queued=F.Branch(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("Status")),F.Int(0)));
        auto* Expired=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_FloatFloat"));Connect(At,FindPin(Expired,TEXT("A")));Connect(F.Field(R,Request->GeneratedClass,TEXT("QueuedDeadline")),FindPin(Expired,TEXT("B")));
        auto* Cancel=F.Branch(F.Eq(Blocked,Expired->GetReturnValuePin(),TEXT("BooleanOR")));F.Put(R,Request->GeneratedClass,TEXT("Status"),nullptr,TEXT("5"));F.Put(R,Request->GeneratedClass,TEXT("Reason"),nullptr,TEXT("queue-deadline-or-shared-recovery-barrier"));
        // Retain cancelled metadata until its exact producer receives settlement.
        // Removing it from the wait queue alone would strand reward/menu waiters.
        auto* Pending=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("NativeCancelledRequests")));Connect(R,FindPin(Pending,TEXT("NewItem")));auto* Cancelled=F.Tail;
        F.Tail=Cancel->GetElsePin();auto* Select=F.Branch(F.And(F.Not(Held),F.Not(F.IsValid(F.Get(TEXT("NativeSelectedRequest"))))));F.Put(R,Request->GeneratedClass,TEXT("Status"),nullptr,TEXT("1"));F.Set(TEXT("NativeSelectedRequest"),R);auto* Selected=F.Tail;
        F.Tail=Select->GetElsePin();auto* Add=F.Array(TEXT("Array_Add"),Remaining);Connect(R,FindPin(Add,TEXT("NewItem")));F.Merge(Selected);F.Merge(Cancelled);F.Merge(Queued->GetElsePin());F.Merge(Valid->GetElsePin());
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Set(TEXT("NativeRequests"),Remaining);F.Return(F.Get(TEXT("NativeSelectedRequest")));F.Compile();
    }
    for(auto* BP:{Request,Master}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    return true;
}

bool ValidateDORNativeAdmission(UBlueprint* Master,UBlueprint* Request,UBlueprint* Context)
{
    FEditorScriptExecutionGuard Guard;
    UWorld* World=UWorld::CreateWorld(EWorldType::EditorPreview,false);if(!World)return false;
    auto* M=World->SpawnActor(Master->GeneratedClass);auto* PS=World->SpawnActor<AFSDPlayerState>();
    bool Passed=M&&PS;int32 Checks=0;
    auto Object=[&](UObject* O,const TCHAR* N,UObject* V){auto* P=FindFProperty<FObjectProperty>(O->GetClass(),N);if(!P){Passed=false;return;}P->SetObjectPropertyValue_InContainer(O,V);};
    auto Number=[&](UObject* O,const TCHAR* N,int32 V){auto* P=FindFProperty<FIntProperty>(O->GetClass(),N);if(!P){Passed=false;return;}P->SetPropertyValue_InContainer(O,V);};
    auto NumberOf=[&](UObject* O,const TCHAR* N){auto* P=FindFProperty<FIntProperty>(O->GetClass(),N);if(!P){Passed=false;return -1;}return P->GetPropertyValue_InContainer(O);};
    auto Bool=[&](UObject* O,const TCHAR* N,bool V){auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N);if(!P){Passed=false;return;}P->SetPropertyValue_InContainer(O,V);};
    auto Check=[&](bool V,const TCHAR* Stage){++Checks;Passed=Passed&&V;if(!V)UE_LOG(LogTemp,Error,TEXT("DOR-NATIVE-VM case=%d stage=%s failed"),Checks,Stage);};
    UActorComponent* Transport=nullptr;
    if(Passed)
    {
        auto* Fn=M->FindFunction(TEXT("DOR_NativeGetTransport"));if(!Fn)Passed=false;
        if(Passed)
        {
            FStructOnScope P(Fn);auto* State=FindFProperty<FObjectProperty>(Fn,TEXT("PlayerState"));auto* Result=FindFProperty<FObjectProperty>(Fn,TEXT("ReturnValue"));
            if(!State||!Result)Passed=false;
            else
            {
                State->SetObjectPropertyValue_InContainer(P.GetStructMemory(),PS);M->ProcessEvent(Fn,P.GetStructMemory());Transport=Cast<UActorComponent>(Result->GetObjectPropertyValue_InContainer(P.GetStructMemory()));
                Check(Transport&&Transport->GetOwner()==PS&&!Transport->GetIsReplicated()&&PS->GetComponentByClass(TSubclassOf<UActorComponent>(Context->GeneratedClass.Get()))==Transport,TEXT("standard-add-component-registered-owner-rpc-only-idle"));
                M->ProcessEvent(Fn,P.GetStructMemory());Check(Result->GetObjectPropertyValue_InContainer(P.GetStructMemory())==Transport,TEXT("transport-retained-not-recreated"));
            }
        }
    }
    auto* R=NewObject<UObject>(GetTransientPackage(),Request->GeneratedClass);
    if(Passed&&R&&Transport)
    {
        Object(M,TEXT("NativeActiveRequest"),R);Bool(M,TEXT("NativeExecutionHeld"),true);
        Object(Transport,TEXT("NativeActiveRequest"),R);Object(Transport,TEXT("NativeMaster"),M);Object(R,TEXT("TargetState"),PS);Number(R,TEXT("Status"),2);
        auto* Outstanding=FindFProperty<FArrayProperty>(M->GetClass(),TEXT("NativeOutstandingRequests"));FScriptArrayHelper Admitted(Outstanding,Outstanding->ContainerPtrToValuePtr<void>(M));Admitted.Resize(1);CastFieldChecked<FObjectProperty>(Outstanding->Inner)->SetObjectPropertyValue(Admitted.GetRawPtr(0),R);
        auto* Tuple=Transport->FindFunction(TEXT("DOR_NativeTupleMatches"));auto* Envelope=Transport->FindFunction(TEXT("DOR_NativeServerEnvelope"));
        if(!Tuple||!Envelope)Passed=false;
        auto Populate=[&](UFunction* Fn,FStructOnScope& Parameters)
        {
            for(const auto& T:DORNativeTuplePins())
            {
                auto* P=FindFProperty<FProperty>(Fn,T.Key);auto* Server=FindFProperty<FProperty>(R->GetClass(),T.Key);auto* Local=FindFProperty<FProperty>(Transport->GetClass(),FName(*(FString(TEXT("Native"))+T.Key.ToString())));
                if(!P||!Server||!Local){Passed=false;return;}
                FStructOnScope Scratch(Fn);void* Value=P->ContainerPtrToValuePtr<void>(Scratch.GetStructMemory());
                if(auto* S=CastField<FStrProperty>(P))S->SetPropertyValue(Value,TEXT("net-v1:synthetic"));
                else if(auto* I=CastField<FIntProperty>(P))I->SetPropertyValue(Value,T.Key==TEXT("Generation")?9:T.Key==TEXT("Code")?435:0);
                else if(auto* G=CastField<FStructProperty>(P))*static_cast<FGuid*>(Value)=T.Key==TEXT("Epoch")?FGuid(1,2,3,4):FGuid(5,6,7,8);
                else {Passed=false;return;}
                P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()),Value);Server->CopyCompleteValue(Server->ContainerPtrToValuePtr<void>(R),Value);Local->CopyCompleteValue(Local->ContainerPtrToValuePtr<void>(Transport),Value);
            }
        };
        auto Invoke=[&](UFunction* Fn,FStructOnScope& P){Transport->ProcessEvent(Fn,P.GetStructMemory());auto* B=FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"));return B&&B->GetPropertyValue_InContainer(P.GetStructMemory());};
        if(Passed)
        {
            FStructOnScope P(Tuple);Populate(Tuple,P);Check(Invoke(Tuple,P),TEXT("exact-request-tuple"));
            for(const auto& T:DORNativeTuplePins())
            {
                Populate(Tuple,P);auto* Field=FindFProperty<FProperty>(Tuple,T.Key);void* Value=Field->ContainerPtrToValuePtr<void>(P.GetStructMemory());
                if(auto* S=CastField<FStrProperty>(Field))S->SetPropertyValue(Value,TEXT("different-player"));
                else if(auto* I=CastField<FIntProperty>(Field))I->SetPropertyValue(Value,I->GetPropertyValue(Value)+1);
                else *static_cast<FGuid*>(Value)=FGuid(99,98,97,96);
                Check(!Invoke(Tuple,P),*FString::Printf(TEXT("reject-stale-%s"),*T.Key.ToString()));
            }
            FStructOnScope E(Envelope);Populate(Envelope,E);Check(Invoke(Envelope,E),TEXT("active-admitted-envelope"));
            Object(M,TEXT("NativeActiveRequest"),nullptr);Check(!Invoke(Envelope,E),TEXT("reject-different-active-request"));Object(M,TEXT("NativeActiveRequest"),R);
            Bool(M,TEXT("NativeExecutionHeld"),false);Check(!Invoke(Envelope,E),TEXT("reject-released-execution"));Bool(M,TEXT("NativeExecutionHeld"),true);
            Number(R,TEXT("Status"),3);Check(!Invoke(Envelope,E),TEXT("reject-settled-request"));Number(R,TEXT("Status"),2);
            Object(R,TEXT("TargetState"),nullptr);Check(!Invoke(Envelope,E),TEXT("reject-different-playerstate"));Object(R,TEXT("TargetState"),PS);
            Number(R,TEXT("Status"),1);Object(M,TEXT("NativeActiveRequest"),nullptr);Bool(M,TEXT("NativeExecutionHeld"),false);Check(Invoke(Envelope,E),TEXT("owner-preparation-receipt-does-not-hold-host-window"));Number(R,TEXT("Status"),2);Object(M,TEXT("NativeActiveRequest"),R);Bool(M,TEXT("NativeExecutionHeld"),true);
        }
    }
    if(Passed)
    {
        auto* Queue=FindFProperty<FArrayProperty>(M->GetClass(),TEXT("NativeRequests"));auto* Inner=Queue?CastField<FObjectProperty>(Queue->Inner):nullptr;
        auto* Select=M->FindFunction(TEXT("DOR_NativeSelectMetadata"));if(!Queue||!Inner||!Select)Passed=false;
        if(Passed)
        {
            auto Make=[&](int32 SlotValue,int32 CodeValue,float Deadline)
            {auto* Q=NewObject<UObject>(GetTransientPackage(),Request->GeneratedClass);Number(Q,TEXT("Slot"),SlotValue);Number(Q,TEXT("Code"),CodeValue);FindFProperty<FFloatProperty>(Q->GetClass(),TEXT("QueuedDeadline"))->SetPropertyValue_InContainer(Q,Deadline);return Q;};
            auto* A=Make(0,435,50);auto* B=Make(1,277,60);auto* C=Make(0,435,60);auto* Expired=Make(1,277,5);
            Object(A,TEXT("TargetState"),PS);Object(B,TEXT("TargetState"),PS);
            auto PutQueue=[&](std::initializer_list<UObject*> Entries){FScriptArrayHelper H(Queue,Queue->ContainerPtrToValuePtr<void>(M));H.Resize(static_cast<int32>(Entries.size()));int32 I=0;for(auto* Q:Entries)Inner->SetObjectPropertyValue(H.GetRawPtr(I++),Q);};
            auto Length=[&](){FScriptArrayHelper H(Queue,Queue->ContainerPtrToValuePtr<void>(M));return H.Num();};
            auto Run=[&](float At,bool Held,bool Blocked)
            {FStructOnScope P(Select);FindFProperty<FFloatProperty>(Select,TEXT("AtTime"))->SetPropertyValue_InContainer(P.GetStructMemory(),At);FindFProperty<FBoolProperty>(Select,TEXT("ExecutionHeld"))->SetPropertyValue_InContainer(P.GetStructMemory(),Held);FindFProperty<FBoolProperty>(Select,TEXT("Blocked"))->SetPropertyValue_InContainer(P.GetStructMemory(),Blocked);M->ProcessEvent(Select,P.GetStructMemory());return FindFProperty<FObjectProperty>(Select,TEXT("ReturnValue"))->GetObjectPropertyValue_InContainer(P.GetStructMemory());};
            PutQueue({A,B,C,Expired});Check(!Run(10,true,false)&&Length()==3&&NumberOf(Expired,TEXT("Status"))==5&&NumberOf(A,TEXT("Status"))==0,TEXT("held-window-still-cancels-expired-wait"));
            Check(Run(11,false,false)==A&&Length()==2&&NumberOf(A,TEXT("Status"))==1,TEXT("fifo-first-client-primary"));
            Check(Run(12,false,false)==B&&Length()==1&&NumberOf(B,TEXT("Status"))==1,TEXT("same-player-next-secondary-metadata"));
            Check(Run(13,false,false)==C&&Length()==0,TEXT("next-client-after-both-slots"));
            PutQueue({A});Check(!Run(14,false,false)&&Length()==0,TEXT("already-selected-reference-cannot-reexecute"));
            Number(A,TEXT("Status"),0);Number(B,TEXT("Status"),0);PutQueue({A,B});Check(!Run(15,true,true)&&Length()==0&&NumberOf(A,TEXT("Status"))==5&&NumberOf(B,TEXT("Status"))==5,TEXT("conflict-barrier-settles-all-waiters"));
            auto* Exact=Make(0,435,20);PutQueue({Exact,nullptr});Check(!Run(20,false,false)&&Length()==0&&NumberOf(Exact,TEXT("Status"))==5,TEXT("exact-deadline-and-null-reference"));
            auto* PS2=World->SpawnActor<AFSDPlayerState>();auto* First=Make(0,435,30);auto* Second=Make(1,277,30);auto* Independent=Make(0,435,30);
            Object(First,TEXT("TargetState"),PS);Object(Second,TEXT("TargetState"),PS);Object(Independent,TEXT("TargetState"),PS2);Number(First,TEXT("Generation"),1);Number(Second,TEXT("Generation"),2);Number(Independent,TEXT("Generation"),3);
            auto* O=FindFProperty<FArrayProperty>(M->GetClass(),TEXT("NativeOutstandingRequests"));FScriptArrayHelper Outstanding(O,O->ContainerPtrToValuePtr<void>(M));Outstanding.Resize(3);auto* OI=CastFieldChecked<FObjectProperty>(O->Inner);OI->SetObjectPropertyValue(Outstanding.GetRawPtr(0),First);OI->SetObjectPropertyValue(Outstanding.GetRawPtr(1),Second);OI->SetObjectPropertyValue(Outstanding.GetRawPtr(2),Independent);PutQueue({First,Second,Independent});
            auto Owners=[&](float At){auto* Fn=M->FindFunction(TEXT("DOR_NativeAdmitOwnersMetadata"));FStructOnScope P(Fn);FindFProperty<FFloatProperty>(Fn,TEXT("AtTime"))->SetPropertyValue_InContainer(P.GetStructMemory(),At);M->ProcessEvent(Fn,P.GetStructMemory());auto* A=FindFProperty<FArrayProperty>(Fn,TEXT("ReturnValue"));FScriptArrayHelper H(A,A->ContainerPtrToValuePtr<void>(P.GetStructMemory()));return H.Num();};
            auto SelectReady=[&](float At,bool Held){auto* Fn=M->FindFunction(TEXT("DOR_NativeSelectReadyMetadata"));FStructOnScope P(Fn);FindFProperty<FFloatProperty>(Fn,TEXT("AtTime"))->SetPropertyValue_InContainer(P.GetStructMemory(),At);FindFProperty<FBoolProperty>(Fn,TEXT("ExecutionHeld"))->SetPropertyValue_InContainer(P.GetStructMemory(),Held);M->ProcessEvent(Fn,P.GetStructMemory());return FindFProperty<FObjectProperty>(Fn,TEXT("ReturnValue"))->GetObjectPropertyValue_InContainer(P.GetStructMemory());};
            Check(Owners(21)==2&&NumberOf(First,TEXT("Status"))==1&&NumberOf(Independent,TEXT("Status"))==1&&NumberOf(Second,TEXT("Status"))==0,TEXT("concurrent-independent-owners-and-same-player-slot-dependency"));
            Number(Independent,TEXT("Phase"),3);Check(!SelectReady(22,true)&&NumberOf(Independent,TEXT("Status"))==1,TEXT("ready-owner-does-not-overwrite-held-host-window"));
            Check(SelectReady(22,false)==Independent&&NumberOf(Independent,TEXT("Status"))==2,TEXT("ready-player-bypasses-unready-different-player"));
            Number(First,TEXT("Status"),3);Check(Owners(23)==1&&NumberOf(Second,TEXT("Status"))==1,TEXT("next-slot-admitted-only-after-earlier-settles"));
            Number(Second,TEXT("Phase"),3);Check(!SelectReady(30,false)&&NumberOf(Second,TEXT("Status"))==1&&NumberOf(Second,TEXT("Phase"))==91,TEXT("absolute-deadline-stops-prepared-owner-without-false-cleanup-receipt"));
        }
    }
    World->DestroyWorld(false);UE_LOG(LogTemp,Display,TEXT("DOR-NATIVE-VM checks=%d passed=%d scope=generated-metadata-tuple-standard-component-no-save-no-multiplayer"),Checks,Passed);return Passed;
}
