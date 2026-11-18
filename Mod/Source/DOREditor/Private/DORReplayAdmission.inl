// Admission deferral is not a terminal replay outcome and must not capture a baseline.
bool BuildDORReplayAdmission(UBlueprint* Master,UBlueprint* Proxy)
{
    const auto MC=Master->GeneratedClass,PC=Proxy->GeneratedClass;
    for(const TCHAR* Name:{TEXT("ReplayAdmissionPrimaryState"),TEXT("ReplayAdmissionSecondaryState")})AddTypedVariable(Proxy,Name,MakeStringPinType());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Proxy);FKismetEditorUtilities::CompileBlueprint(Proxy);if(Proxy->Status==BS_Error)return false;
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_ReplayAdmissionReady"),MC);
        auto* Player=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));auto* ProxyActor=F.Input(TEXT("ProxyActor"),MakeObjectPinType(AActor::StaticClass()));auto* Code=F.Input(TEXT("Code"),MakeIntPinType());
        auto* P=F.Cache(TEXT("Proxy"),F.Cast(ProxyActor,PC));F.QuietRequire(F.IsValid(P));F.QuietRequire(F.IsValid(Player));
        auto* Allowed=F.Self(TEXT("DOR_NewGameplayTargetAllowed"));Connect(Player,FindPin(Allowed,TEXT("Player")));auto* Ready=F.Cache(TEXT("Ready"),Allowed->GetReturnValuePin());
        auto* Capability=F.Self(TEXT("DOR_RewardCapability"));Connect(Player,FindPin(Capability,TEXT("Player")));auto* Cap=F.Cache(TEXT("Capability"),Capability->GetReturnValuePin());
        auto* Map=F.Get(TEXT("NativeKnownSlots"));auto* Find=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"));ForcePinType(FindPin(Find,TEXT("TargetMap")),Map->PinType);ForcePinType(FindPin(Find,TEXT("Key")),MakeIntPinType());ForcePinType(FindPin(Find,TEXT("Value")),MakeIntPinType());Connect(Map,FindPin(Find,TEXT("TargetMap")));Connect(Code,FindPin(Find,TEXT("Key")));F.QuietRequire(Find->GetReturnValuePin());auto* Slot=F.Cache(TEXT("Slot"),FindPin(Find,TEXT("Value")));
        auto* State=F.Cache(TEXT("State"),F.Join(F.Join(F.Text(Code,TEXT("Conv_IntToString")),F.Str(TEXT("|"))),F.Join(F.Text(Cap,TEXT("Conv_IntToString")),F.Text(Ready,TEXT("Conv_BoolToString")))));
        auto* Last=F.Cache(TEXT("Last"),F.Field(P,PC,TEXT("ReplayAdmissionPrimaryState")));auto* Secondary=F.Branch(F.Eq(Slot,F.Int(1)));F.Set(F.Variable(Last),F.Field(P,PC,TEXT("ReplayAdmissionSecondaryState")));F.Merge(Secondary->GetElsePin());auto* Changed=F.Branch(F.Not(F.Eq(State,Last,TEXT("EqualEqual_StrStr"))));
        auto* StoreSecondary=F.Branch(F.Eq(Slot,F.Int(1)));F.Put(P,PC,TEXT("ReplayAdmissionSecondaryState"),State);auto* SecondaryTail=F.Tail;F.Tail=StoreSecondary->GetElsePin();F.Put(P,PC,TEXT("ReplayAdmissionPrimaryState"),State);F.Merge(SecondaryTail);
        FDORNativeProbeLine D(F,TEXT("Admission"),TEXT("[DOR-REPLAY-ADMISSION]|stage=current-world-readiness|unadmitted-capture=false"));D.Add(TEXT("key"),F.Field(P,PC,TEXT("ReplayPlayerKey")),NAME_None);D.Add(TEXT("slot"),Slot,TEXT("Conv_IntToString"));D.Add(TEXT("code"),Code,TEXT("Conv_IntToString"));D.Add(TEXT("capability"),Cap,TEXT("Conv_IntToString"));D.Add(TEXT("admittedReady"),Ready);D.Record();F.Merge(Changed->GetElsePin());F.Return(Ready);F.Compile();
    }
    {
        auto* G=FindFunctionGraph(Master,TEXT("DOR_NativeReplayWatchGate"));auto* E=FindFunctionEntry(G);int32 Hooks=0;
        for(auto* N:TArray<UEdGraphNode*>(G->Nodes))if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("GetPlayerState")){
            auto* In=C->GetExecPin();if(!In){for(auto* Consumer:C->GetReturnValuePin()->LinkedTo)if(auto* Set=Cast<UK2Node_VariableSet>(Consumer->GetOwningNode()))In=Set->GetExecPin();}if(!In)return false;
            auto Previous=In->LinkedTo;In->BreakAllPinLinks();auto* Gate=AddSelfCall(G,TEXT("DOR_ReplayAdmissionReady"),0,0);for(auto* P:Previous)Connect(P,Gate->GetExecPin());for(const TCHAR* Pin:{TEXT("Player"),TEXT("ProxyActor"),TEXT("Code")})Connect(E->FindPin(Pin),FindPin(Gate,Pin));auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);B->AllocateDefaultPins();Connect(Gate->GetThenPin(),B->GetExecPin());Connect(Gate->GetReturnValuePin(),B->GetConditionPin());Connect(B->GetThenPin(),In);++Hooks;
        }if(Hooks!=1)return false;
    }
    {
        FDOROrdinaryGraph F(Proxy,TEXT("DOR_DeferUnadmittedReplay"),MC);F.Set(TEXT("ReplayRefreshInFlight"),nullptr,TEXT("false"));F.Set(TEXT("NativeReplayRequest"),nullptr,TEXT(""));F.Set(TEXT("PendingReplaySlot"),F.Str(TEXT("")));
        auto* Timer=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("K2_SetTimer"));FindPin(Timer,TEXT("FunctionName"))->DefaultValue=TEXT("DOR_ServerReconcileReplay");FindPin(Timer,TEXT("Time"))->DefaultValue=TEXT("1.0");FindPin(Timer,TEXT("bLooping"))->DefaultValue=TEXT("true");FindPin(Timer,TEXT("InitialStartDelay"))->DefaultValue=TEXT("1.0");F.Done();F.Compile();
        auto* G=FindFunctionGraph(Proxy,TEXT("DOR_NativeDispatchReplay"));int32 Replaced=0;for(auto* N:TArray<UEdGraphNode*>(G->Nodes))if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("DOR_ServerFinalizeReplayRequest")){auto Before=C->GetExecPin()->LinkedTo,After=C->GetThenPin()->LinkedTo;C->GetExecPin()->BreakAllPinLinks();C->GetThenPin()->BreakAllPinLinks();auto* Defer=AddSelfCall(G,TEXT("DOR_DeferUnadmittedReplay"),0,0);for(auto* P:Before)Connect(P,Defer->GetExecPin());for(auto* P:After)Connect(Defer->GetThenPin(),P);FBlueprintEditorUtils::RemoveNode(Proxy,C,true);++Replaced;}if(Replaced!=1)return false;
    }
    for(auto* BP:{Master,Proxy}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    UE_LOG(LogTemp,Display,TEXT("DOR-REPLAY-ADMISSION readiness=current-producer-admission before=watch-consumption unadmitted=existing-timer-no-capture"));return true;
}
