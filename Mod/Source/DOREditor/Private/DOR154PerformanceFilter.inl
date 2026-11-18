// This policy changes only new list construction. Native admission/replay and
// stored offers never consult it. Codes are resolved from reviewed pair IDs.
TArray<int32> DORHighLoadCodes;
bool BuildDOR154Filter(UBlueprint* Master,UBlueprint* Host,UWidgetBlueprint* Page,UBlueprint* Preferences,const FString& ManifestFilename,const TArray<TSharedPtr<FJsonObject>>& Pairs)
{
    FString Text;TSharedPtr<FJsonObject> Manifest;
    if(!FFileHelper::LoadFileToString(Text,*ManifestFilename)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest))return false;
    auto Policy=Manifest->GetObjectField(TEXT("policy"))->GetObjectField(TEXT("highLoadFilter"));
    if(Policy->GetBoolField(TEXT("defaultEnabled")))return false;
    DORHighLoadCodes.Empty();
    for(auto V:Policy->GetArrayField(TEXT("exclusions"))){auto E=V->AsObject();const int32 Code=E->GetIntegerField(TEXT("runtimeCode"));if(!Pairs.IsValidIndex(Code-1)||Pairs[Code-1]->GetStringField(TEXT("pairId"))!=E->GetStringField(TEXT("pairId")))return false;DORHighLoadCodes.AddUnique(Code);}
    if(DORHighLoadCodes.Num()!=37)return false;
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_NewChoiceAllowed"),Master->GeneratedClass);
        auto* Code=F.Input(TEXT("Code"),MakeIntPinType());auto* Hide=F.Input(TEXT("HideHighLoad"),MakeBoolPinType());
        // OFF never evaluates the absolute code or the eager membership chain.
        auto* Enabled=F.Branch(Hide);F.Tail=Enabled->GetElsePin();F.Return(nullptr,TEXT("true"));F.Tail=Enabled->GetThenPin();
        auto* Abs=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Abs_Int"));Connect(Code,FindPin(Abs,TEXT("A")));
        UEdGraphPin* Blocked=F.Eq(Abs->GetReturnValuePin(),F.Int(DORHighLoadCodes[0]));
        for(int32 I=1;I<DORHighLoadCodes.Num();++I)Blocked=F.Or(Blocked,F.Eq(Abs->GetReturnValuePin(),F.Int(DORHighLoadCodes[I])));
        F.Return(F.Not(Blocked));F.Compile();
    }
    // Both signed pair contributions and the existing standalone contribution
    // retain their source/base/slot predicates; filtering precedes every append.
    auto* G=FindFunctionGraph(Master,TEXT("DOR_AddCandidatesForSlot"));int32 Gates=0;
    for(auto* N:TArray<UEdGraphNode*>(G->Nodes))if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("Array_AddUnique")){
        auto* Target=FindPin(C,TEXT("TargetArray"));auto* V=Target->LinkedTo.Num()?Cast<UK2Node_VariableGet>(Target->LinkedTo[0]->GetOwningNode()):nullptr;if(!V||V->VariableReference.GetMemberName()!=TEXT("EligibleChoiceCodes"))continue;
        auto Prev=C->GetExecPin()->LinkedTo;C->GetExecPin()->BreakAllPinLinks();auto* Allowed=AddSelfCall(G,TEXT("DOR_NewChoiceAllowed"),0,0);Connect(FindPin(C,TEXT("NewItem"))->LinkedTo[0],FindPin(Allowed,TEXT("Code")));Connect(AddVariableGet(G,TEXT("ActiveHideHighLoad"),0,0)->GetValuePin(),FindPin(Allowed,TEXT("HideHighLoad")));for(auto* P:Prev)Connect(P,Allowed->GetExecPin());auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);B->AllocateDefaultPins();Connect(Allowed->GetThenPin(),B->GetExecPin());Connect(Allowed->GetReturnValuePin(),B->GetConditionPin());Connect(B->GetThenPin(),C->GetExecPin());++Gates;
    }
    if(Gates!=3)return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Master);FKismetEditorUtilities::CompileBlueprint(Master);if(Master->Status==BS_Error)return false;
    {
        FDOROrdinaryGraph F(Page,TEXT("DOR_EffectiveHideHighLoad"),Master->GeneratedClass);
        auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));F.QuietRequire(F.IsValid(GS->GetReturnValuePin()));
        auto* Rig=F.Branch(F.Field(GS->GetReturnValuePin(),AFSDGameState::StaticClass(),TEXT("IsOnSpaceRig")));
        F.QuietRequire(F.IsValid(F.Get(TEXT("Host"))));auto* Load=F.Call(Host->GeneratedClass,TEXT("DOR_LoadPreferences"));Connect(F.Get(TEXT("Host")),FindPin(Load,UEdGraphSchema_K2::PN_Self));F.QuietRequire(Load->GetReturnValuePin());F.Return(F.Field(F.Field(F.Get(TEXT("Host")),Host->GeneratedClass,TEXT("LocalPreferences")),Preferences->GeneratedClass,TEXT("HideHighLoad")));
        F.Tail=Rig->GetElsePin();auto* Find=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(Find,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;auto* M=F.Cache(TEXT("Master"),F.Cast(Find->GetReturnValuePin(),Master->GeneratedClass));F.QuietRequire(F.IsValid(M));F.Return(F.Field(M,Master->GeneratedClass,TEXT("ActiveHideHighLoad")));F.Compile();
    }
    // Execute only the emitted stateless boolean predicate on its CDO. No actor,
    // world, save or native gameplay function is created or simulated here.
    auto* Fn=Master->GeneratedClass->FindFunctionByName(TEXT("DOR_NewChoiceAllowed"));int32 Checks=0;
    auto Allowed=[&](int32 Code,bool Hide){FStructOnScope Params(Fn);FindFProperty<FIntProperty>(Fn,TEXT("Code"))->SetPropertyValue_InContainer(Params.GetStructMemory(),Code);FindFProperty<FBoolProperty>(Fn,TEXT("HideHighLoad"))->SetPropertyValue_InContainer(Params.GetStructMemory(),Hide);Master->GeneratedClass->GetDefaultObject()->ProcessEvent(Fn,Params.GetStructMemory());return FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(Params.GetStructMemory());};
    TArray<int32> Cases={0,1,-1,1001};for(int32 Code:DORHighLoadCodes){Cases.Add(Code);Cases.Add(-Code);}
    for(bool Hide:{false,true})for(int32 Code:Cases){
        if(Allowed(Code,Hide)!=( !Hide||!DORHighLoadCodes.Contains(FMath::Abs(Code))))return false;++Checks;
    }
    TArray<int32> Pool;for(int32 Code:{343,-343,456,-456,1})if(Allowed(Code,true))Pool.Add(Code);if(Pool.Num()!=1||Pool[0]!=1)return false;++Checks;
    Pool.Empty();for(int32 Code:{343,-456})if(Allowed(Code,true))Pool.Add(Code);if(Pool.Num()!=0)return false;++Checks;
    if(FindFProperty<FBoolProperty>(Preferences->GeneratedClass,TEXT("HideHighLoad"))->GetPropertyValue_InContainer(Preferences->GeneratedClass->GetDefaultObject()))return false;
    UE_LOG(LogTemp,Display,TEXT("DOR154-FILTER-VM checks=%d default=false scope=stateless-CDO-predicate-no-world-no-save"),Checks);
    return true;
}

bool BuildDOR154PageLifecycle(UWidgetBlueprint* Page,UClass* MasterClass)
{
    for(bool Open:{false,true}){FDOROrdinaryGraph F(Page,Open?TEXT("DOR154PageOpened"):TEXT("DOR154PageClosed"),MasterClass);F.Set(TEXT("DebugPageOpened"),nullptr,Open?TEXT("true"):TEXT("false"));if(Open)F.Self(TEXT("DOR_BatchProgressOnly"));F.Done();F.Compile();}
    auto* EG=FBlueprintEditorUtils::FindEventGraph(Page);int32 Hooks=0;
    for(auto* N:TArray<UEdGraphNode*>(EG->Nodes))if(auto* E=Cast<UK2Node_Event>(N)){auto Name=E->EventReference.GetMemberName();if(Name==TEXT("HubPageOpened")||Name==TEXT("HubPageClosed")){DOROrdinaryInsert(E->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,Name==TEXT("HubPageOpened")?TEXT("DOR154PageOpened"):TEXT("DOR154PageClosed"));++Hooks;}}
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Page);FKismetEditorUtilities::CompileBlueprint(Page);return Hooks==2&&Page->Status!=BS_Error;
}
