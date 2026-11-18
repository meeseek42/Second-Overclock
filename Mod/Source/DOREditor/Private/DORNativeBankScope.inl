// One shared execution owns all accepted roots. These are runtime resolver
// entries, not save eligibility. The account journal remains independently held.
bool BuildDORNativeBankScope(UBlueprint* Master,UBlueprint* Request,UBlueprint* Safety)
{
    UClass* RootClass=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.OverclockUpgrade"));
    if(!RootClass)return false;
    UClass* JournalClass=FindFProperty<FObjectProperty>(Safety->GeneratedClass,TEXT("DOR_Journal"))->PropertyClass;
    const auto Banks=MakeObjectPinType(UOverclockBank::StaticClass(),true),Roots=MakeObjectPinType(RootClass,true),Values=MakeObjectPinType(USchematic::StaticClass(),true);
    const auto MapType=MakeOverclockBankMapPinType(RootClass,USchematic::StaticClass());
    auto Bits=MakeBoolPinType();Bits.ContainerType=EPinContainerType::Array;
    for(const auto& P:{TPair<FName,FEdGraphPinType>(TEXT("NativeScopeBanks"),Banks),TPair<FName,FEdGraphPinType>(TEXT("NativeScopeRoots"),Roots),TPair<FName,FEdGraphPinType>(TEXT("NativeScopePrior"),Values),TPair<FName,FEdGraphPinType>(TEXT("NativeScopeInstalled"),Values),TPair<FName,FEdGraphPinType>(TEXT("NativeScopeExists"),Bits)})AddTypedVariable(Master,P.Key,P.Value);
    for(const TCHAR* N:{TEXT("NativeScopeActive"),TEXT("NativeScopeConflict"),TEXT("NativeScopeRestored"),TEXT("NativeScopeEquipmentOnly")})AddTypedVariable(Master,N,MakeBoolPinType());
    AddTypedVariable(Master,TEXT("NativeScopeRequest"),MakeStructPinType(TBaseStructure<FGuid>::Get()));
    AddTypedVariable(Master,TEXT("NativeScopeJournalId"),MakeStringPinType());
    AddTypedVariable(Master,TEXT("NativeScopeSafety"),MakeObjectPinType(Safety->GeneratedClass));
    AddTypedVariable(Master,TEXT("NativeScopeRequired"),Roots);AddTypedVariable(Master,TEXT("NativeScopeRecovery"),MakeBoolPinType());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Master);FKismetEditorUtilities::CompileBlueprint(Master);
    auto At=[](FDORNativeGraph& F,UEdGraphPin* A,UEdGraphPin* Index){auto* N=F.Array(TEXT("Array_Get"),A);Connect(Index,FindPin(N,TEXT("Index")));return FindPin(N,TEXT("Item"));};
    auto Map=[&](FDORNativeGraph& F,UEdGraphPin* Bank){auto* M=F.Field(Bank,UOverclockBank::StaticClass(),TEXT("Overclocks"));ForcePinType(M,MapType);return M;};
    auto Find=[&](FDORNativeGraph& F,UEdGraphPin* Bank,UEdGraphPin* Root)
    {auto* N=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"));ForcePinType(FindPin(N,TEXT("TargetMap")),MapType);ForcePinType(FindPin(N,TEXT("Key")),MakeObjectPinType(RootClass));ForcePinType(FindPin(N,TEXT("Value")),MakeObjectPinType(USchematic::StaticClass()));Connect(Map(F,Bank),FindPin(N,TEXT("TargetMap")));Connect(Root,FindPin(N,TEXT("Key")));return N;};
    auto Store=[&](FDORNativeGraph& F,UEdGraphPin* Bank,UEdGraphPin* Root,UEdGraphPin* Value)
    {auto* N=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Add"));ForcePinType(FindPin(N,TEXT("TargetMap")),MapType);ForcePinType(FindPin(N,TEXT("Key")),MakeObjectPinType(RootClass));ForcePinType(FindPin(N,TEXT("Value")),MakeObjectPinType(USchematic::StaticClass()));Connect(Map(F,Bank),FindPin(N,TEXT("TargetMap")));Connect(Root,FindPin(N,TEXT("Key")));Connect(Value,FindPin(N,TEXT("Value")));};
    auto Remove=[&](FDORNativeGraph& F,UEdGraphPin* Bank,UEdGraphPin* Root)
    {auto* N=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Remove"));ForcePinType(FindPin(N,TEXT("TargetMap")),MapType);ForcePinType(FindPin(N,TEXT("Key")),MakeObjectPinType(RootClass));Connect(Map(F,Bank),FindPin(N,TEXT("TargetMap")));Connect(Root,FindPin(N,TEXT("Key")));};
    auto Match=[&](FDORNativeGraph& F)
    {auto* Token=F.Input(TEXT("RequestId"),MakeStructPinType(TBaseStructure<FGuid>::Get()));F.Require(F.Get(TEXT("NativeExecutionHeld")));auto* Active=F.Get(TEXT("NativeActiveRequest"));F.Require(F.IsValid(Active));F.Require(F.Eq(Token,F.Field(Active,Request->GeneratedClass,TEXT("Request")),TEXT("EqualEqual_GuidGuid")));return Token;};
    auto PriorMatches=[&](FDORNativeGraph& F,UEdGraphPin* Found,UEdGraphPin* Value,UEdGraphPin* Had,UEdGraphPin* Prior)
    {return F.Eq(F.And(Had,F.And(Found,F.Same(Value,Prior))),F.And(F.Not(Had),F.Not(Found)),TEXT("BooleanOR"));};
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeRequireRegistered"),Master->GeneratedClass);
        auto* Service=F.Input(TEXT("Safety"),MakeObjectPinType(Safety->GeneratedClass));
        auto* Required=F.Input(TEXT("RequiredRoots"),Roots);
        F.Require(F.IsValid(Service));
        auto* Verified=F.Call(Safety->GeneratedClass,TEXT("DOR_RewardVerifyRoots"));
        Connect(Service,FindPin(Verified,UEdGraphSchema_K2::PN_Self));
        Connect(Required,FindPin(Verified,TEXT("RequiredRoots")));
        auto* Passed=F.Branch(Verified->GetReturnValuePin());
        F.Return(nullptr,TEXT("true")); F.Tail=Passed->GetElsePin();
        auto* Published=F.Branch(F.Field(Service,Safety->GeneratedClass,TEXT("DOR_RewardReady")));
        auto* Withdraw=F.Call(Safety->GeneratedClass,TEXT("DOR_RewardWithdraw"));
        Connect(Service,FindPin(Withdraw,UEdGraphSchema_K2::PN_Self));
        F.Merge(Published->GetElsePin());
        auto* Notice=F.Self(TEXT("DOR_DebugNotify"));
        FindPin(Notice,TEXT("Message"))->DefaultValue=TEXT("DOR：当前存档的融合超频未完成登记，请回空间站检查。 Fusion overclocks are not registered for this save. Return to the Space Rig.");
        F.Return(); F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeBankCapture"),Master->GeneratedClass);auto* Token=Match(F);
        auto* BankInput=F.Input(TEXT("Banks"),Banks);auto* RootInput=F.Input(TEXT("Roots"),Roots);auto* ValueInput=F.Input(TEXT("Installed"),Values);
        auto* Service=F.Input(TEXT("Safety"),MakeObjectPinType(Safety->GeneratedClass));auto* JournalId=F.Input(TEXT("JournalId"),MakeStringPinType());
        F.Require(F.Not(F.Get(TEXT("NativeScopeActive"))));F.Require(F.Not(F.Get(TEXT("RuntimeBankActive"))));F.Require(F.IsValid(Service));
        F.Require(F.And(F.Field(Service,Safety->GeneratedClass,TEXT("DOR_Active")),F.Not(F.Field(Service,Safety->GeneratedClass,TEXT("DOR_Blocked")))));
        auto* J=F.Field(Service,Safety->GeneratedClass,TEXT("DOR_Journal"));F.Require(F.IsValid(J));F.Require(F.Eq(JournalId,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr")));
        auto* Count=F.Cache(TEXT("Count"),F.Array(TEXT("Array_Length"),RootInput)->GetReturnValuePin());
        auto* Positive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Count,FindPin(Positive,TEXT("A")));F.Require(F.Eq(Positive->GetReturnValuePin(),F.Eq(F.Field(F.Get(TEXT("NativeActiveRequest")),Request->GeneratedClass,TEXT("Operation")),F.Int(1)),TEXT("BooleanOR")));
        auto* Empty=F.Branch(F.Eq(Count,F.Int(0)));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("Schema")),F.Int(3)));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("NativeRequest")),Token,TEXT("EqualEqual_GuidGuid")));F.Merge(Empty->GetElsePin());
        auto* Bound=F.Call(UKismetMathLibrary::StaticClass(),TEXT("LessEqual_IntInt"));Connect(Count,FindPin(Bound,TEXT("A")));FindPin(Bound,TEXT("B"))->DefaultValue=TEXT("8");F.Require(Bound->GetReturnValuePin());
        for(auto* A:{BankInput,ValueInput})F.Require(F.Eq(Count,F.Array(TEXT("Array_Length"),A)->GetReturnValuePin()));
        auto* Seen=F.Cache(TEXT("Seen"),RootInput);F.Array(TEXT("Array_Clear"),Seen);
        auto* Prior=F.Cache(TEXT("Prior"),ValueInput);F.Array(TEXT("Array_Clear"),Prior);
        auto* Had=F.Cache(TEXT("Exists"),F.Get(TEXT("NativeScopeExists")));F.Array(TEXT("Array_Clear"),Had);
        auto* Loop=F.Each(RootInput);auto* Root=FindPin(Loop,TEXT("Array Element"));auto* Index=FindPin(Loop,TEXT("Array Index"));auto* Bank=At(F,BankInput,Index);auto* Value=At(F,ValueInput,Index);
        F.Require(F.And(F.IsValid(Root),F.And(F.IsValid(Bank),F.IsValid(Value))));auto* Duplicate=F.Array(TEXT("Array_Contains"),Seen);Connect(Root,FindPin(Duplicate,TEXT("ItemToFind")));F.Require(F.Not(Duplicate->GetReturnValuePin()));auto* AddRoot=F.Array(TEXT("Array_Add"),Seen);Connect(Root,FindPin(AddRoot,TEXT("NewItem")));
        auto* Existing=Find(F,Bank,Root);auto* AddPrior=F.Array(TEXT("Array_Add"),Prior);Connect(FindPin(Existing,TEXT("Value")),FindPin(AddPrior,TEXT("NewItem")));auto* AddHad=F.Array(TEXT("Array_Add"),Had);Connect(Existing->GetReturnValuePin(),FindPin(AddHad,TEXT("NewItem")));
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Set(TEXT("NativeScopeBanks"),BankInput);F.Set(TEXT("NativeScopeRoots"),RootInput);F.Set(TEXT("NativeScopeInstalled"),ValueInput);F.Set(TEXT("NativeScopePrior"),Prior);F.Set(TEXT("NativeScopeExists"),Had);
        F.Set(TEXT("NativeScopeSafety"),Service);F.Set(TEXT("NativeScopeJournalId"),JournalId);F.Set(TEXT("NativeScopeRequest"),Token);F.Set(TEXT("NativeScopeRequired"),RootInput);F.Set(TEXT("NativeScopeRecovery"),nullptr,TEXT("false"));F.Set(TEXT("NativeScopeConflict"),nullptr,TEXT("false"));F.Set(TEXT("NativeScopeRestored"),nullptr,TEXT("false"));F.Set(TEXT("NativeScopeActive"),nullptr,TEXT("true"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {FDORNativeGraph F(Master,TEXT("DOR_NativeBankStage"),Master->GeneratedClass);F.Input(TEXT("RequestId"),MakeStructPinType(TBaseStructure<FGuid>::Get()));F.Return();F.Compile();}
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeBankRearmOriginal"),Master->GeneratedClass);auto* Token=Match(F);auto* Required=F.Input(TEXT("RequiredRoots"),Roots);
        F.Require(F.Eq(Token,F.Get(TEXT("NativeScopeRequest")),TEXT("EqualEqual_GuidGuid")));F.Require(F.Not(F.Get(TEXT("NativeScopeActive"))));F.Require(F.Get(TEXT("NativeScopeRestored")));F.Require(F.Not(F.Get(TEXT("NativeScopeConflict"))));F.Require(F.Not(F.Get(TEXT("NativeScopeRecovery"))));
        auto* Service=F.Get(TEXT("NativeScopeSafety"));F.Require(F.IsValid(Service));auto* J=F.Field(Service,Safety->GeneratedClass,TEXT("DOR_Journal"));F.Require(F.And(F.Field(Service,Safety->GeneratedClass,TEXT("DOR_Active")),F.Not(F.Field(Service,Safety->GeneratedClass,TEXT("DOR_Blocked")))));F.Require(F.IsValid(J));F.Require(F.Eq(F.Get(TEXT("NativeScopeJournalId")),F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr")));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),F.Int(0)));
        auto* Seen=F.Cache(TEXT("Seen"),Required);F.Array(TEXT("Array_Clear"),Seen);auto* Loop=F.Each(Required);auto* Root=FindPin(Loop,TEXT("Array Element"));auto* Known=F.Array(TEXT("Array_Contains"),F.Get(TEXT("NativeScopeRoots")));Connect(Root,FindPin(Known,TEXT("ItemToFind")));F.Require(Known->GetReturnValuePin());auto* Duplicate=F.Array(TEXT("Array_Contains"),Seen);Connect(Root,FindPin(Duplicate,TEXT("ItemToFind")));F.Require(F.Not(Duplicate->GetReturnValuePin()));auto* Add=F.Array(TEXT("Array_Add"),Seen);Connect(Root,FindPin(Add,TEXT("NewItem")));F.Tail=FindPin(Loop,TEXT("Completed"));
        // Never recapture priors after provisional restore. The old lease owns
        // only the original-required subset, and staging rechecks those priors.
        F.Set(TEXT("NativeScopeRequired"),Required);F.Set(TEXT("NativeScopeRecovery"),nullptr,TEXT("true"));F.Set(TEXT("NativeScopeRestored"),nullptr,TEXT("false"));F.Set(TEXT("NativeScopeActive"),nullptr,TEXT("true"));auto* Stage=F.Self(TEXT("DOR_NativeBankStage"));Connect(Token,FindPin(Stage,TEXT("RequestId")));F.Return(Stage->GetReturnValuePin());F.Compile();
    }
    for(bool Restore:{false,true})
    {
        FDORNativeGraph F(Master,Restore?TEXT("DOR_NativeBankRestore"):TEXT("DOR_NativeBankStage"),Master->GeneratedClass);auto* Token=Match(F);
        F.Require(F.Eq(Token,F.Get(TEXT("NativeScopeRequest")),TEXT("EqualEqual_GuidGuid")));
        if(Restore){auto* Active=F.Branch(F.Get(TEXT("NativeScopeActive")));F.Tail=Active->GetElsePin();F.Return(F.Get(TEXT("NativeScopeRestored")));F.Tail=Active->GetThenPin();}
        else F.Require(F.Get(TEXT("NativeScopeActive")));
        F.Set(TEXT("NativeScopeConflict"),nullptr,TEXT("false"));
        auto* RootList=F.Get(TEXT("NativeScopeRoots"));auto* Loop=F.Each(RootList);auto* Root=FindPin(Loop,TEXT("Array Element"));auto* Index=FindPin(Loop,TEXT("Array Index"));auto* Bank=At(F,F.Get(TEXT("NativeScopeBanks")),Index);auto* Prior=At(F,F.Get(TEXT("NativeScopePrior")),Index);auto* Had=At(F,F.Get(TEXT("NativeScopeExists")),Index);auto* Installed=At(F,F.Get(TEXT("NativeScopeInstalled")),Index);
        // A prior==installed entry was never changed by this transaction. Leave
        // third-party changes there alone, exactly like unchanged save fields.
        auto* Required=F.Array(TEXT("Array_Contains"),F.Get(TEXT("NativeScopeRequired")));Connect(Root,FindPin(Required,TEXT("ItemToFind")));auto* Changed=F.Branch(F.And(Required->GetReturnValuePin(),F.Not(F.And(Had,F.Same(Prior,Installed)))));auto* Valid=F.Branch(F.IsValid(Bank));auto* Current=Find(F,Bank,Root);
        auto* Exact=PriorMatches(F,Current->GetReturnValuePin(),FindPin(Current,TEXT("Value")),Had,Prior);
        if(Restore)Exact=F.Eq(Exact,F.And(Current->GetReturnValuePin(),F.Same(FindPin(Current,TEXT("Value")),Installed)),TEXT("BooleanOR"));
        auto* Safe=F.Branch(Exact);auto* OK=F.Tail;F.Tail=Safe->GetElsePin();F.Set(TEXT("NativeScopeConflict"),nullptr,TEXT("true"));F.Merge(OK);auto* Existing=F.Tail;F.Tail=Valid->GetElsePin();F.Set(TEXT("NativeScopeConflict"),nullptr,TEXT("true"));F.Merge(Existing);F.Merge(Changed->GetElsePin());
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Require(F.Not(F.Get(TEXT("NativeScopeConflict"))),TEXT("[DOR-NATIVE]|stage=bank-conflict|condition=all-root-preflight|barrier=retained"));
        Loop=F.Each(RootList);Root=FindPin(Loop,TEXT("Array Element"));Index=FindPin(Loop,TEXT("Array Index"));Bank=At(F,F.Get(TEXT("NativeScopeBanks")),Index);Prior=At(F,F.Get(TEXT("NativeScopePrior")),Index);Had=At(F,F.Get(TEXT("NativeScopeExists")),Index);Installed=At(F,F.Get(TEXT("NativeScopeInstalled")),Index);
        Required=F.Array(TEXT("Array_Contains"),F.Get(TEXT("NativeScopeRequired")));Connect(Root,FindPin(Required,TEXT("ItemToFind")));Changed=F.Branch(F.And(Required->GetReturnValuePin(),F.Not(F.And(Had,F.Same(Prior,Installed)))));
        if(Restore)
        {auto* Exists=F.Branch(Had);Store(F,Bank,Root,Prior);auto* Added=F.Tail;F.Tail=Exists->GetElsePin();Remove(F,Bank,Root);F.Merge(Added);}
        else Store(F,Bank,Root,Installed);
        F.Merge(Changed->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));
        Loop=F.Each(RootList);Root=FindPin(Loop,TEXT("Array Element"));Index=FindPin(Loop,TEXT("Array Index"));Bank=At(F,F.Get(TEXT("NativeScopeBanks")),Index);Prior=At(F,F.Get(TEXT("NativeScopePrior")),Index);Had=At(F,F.Get(TEXT("NativeScopeExists")),Index);Installed=At(F,F.Get(TEXT("NativeScopeInstalled")),Index);Required=F.Array(TEXT("Array_Contains"),F.Get(TEXT("NativeScopeRequired")));Connect(Root,FindPin(Required,TEXT("ItemToFind")));Changed=F.Branch(F.And(Required->GetReturnValuePin(),F.Not(F.And(Had,F.Same(Prior,Installed)))));Current=Find(F,Bank,Root);
        F.Require(Restore?PriorMatches(F,Current->GetReturnValuePin(),FindPin(Current,TEXT("Value")),Had,Prior):F.And(Current->GetReturnValuePin(),F.Same(FindPin(Current,TEXT("Value")),Installed)),TEXT("[DOR-NATIVE]|stage=bank-readback-failed|barrier=retained"));F.Merge(Changed->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));
        if(Restore){F.Set(TEXT("NativeScopeRestored"),nullptr,TEXT("true"));F.Set(TEXT("NativeScopeActive"),nullptr,TEXT("false"));}
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeBankReady"),Master->GeneratedClass);auto* Token=Match(F);F.Require(F.Get(TEXT("NativeScopeActive")));F.Require(F.Not(F.Get(TEXT("NativeScopeConflict"))));F.Require(F.Eq(Token,F.Get(TEXT("NativeScopeRequest")),TEXT("EqualEqual_GuidGuid")));
        auto* Equipment=F.Branch(F.Get(TEXT("NativeScopeEquipmentOnly")));
        auto* Verified=F.Self(TEXT("DOR_NativeRequireRegistered"));
        Connect(F.Get(TEXT("NativeScopeSafety")),FindPin(Verified,TEXT("Safety")));
        Connect(F.Get(TEXT("NativeScopeRequired")),FindPin(Verified,TEXT("RequiredRoots")));
        F.Return(Verified->GetReturnValuePin()); F.Tail=Equipment->GetElsePin();
        auto* Count=F.Array(TEXT("Array_Length"),F.Get(TEXT("NativeScopeRoots")))->GetReturnValuePin();auto* EmptyClear=F.And(F.Eq(Count,F.Int(0)),F.Eq(F.Field(F.Get(TEXT("NativeActiveRequest")),Request->GeneratedClass,TEXT("Operation")),F.Int(1)));auto* Positive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Count,FindPin(Positive,TEXT("A")));F.Require(F.Eq(Positive->GetReturnValuePin(),EmptyClear,TEXT("BooleanOR")));for(const TCHAR* N:{TEXT("NativeScopeBanks"),TEXT("NativeScopeInstalled")})F.Require(F.Eq(Count,F.Array(TEXT("Array_Length"),F.Get(N))->GetReturnValuePin()));
        auto* RequiredCount=F.Array(TEXT("Array_Length"),F.Get(TEXT("NativeScopeRequired")))->GetReturnValuePin();auto* Nonempty=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(RequiredCount,FindPin(Nonempty,TEXT("A")));F.Require(F.Eq(Nonempty->GetReturnValuePin(),F.Eq(F.Get(TEXT("NativeScopeRecovery")),EmptyClear,TEXT("BooleanOR")),TEXT("BooleanOR")));
        auto* Loop=F.Each(F.Get(TEXT("NativeScopeRoots")));auto* Root=FindPin(Loop,TEXT("Array Element"));auto* Index=FindPin(Loop,TEXT("Array Index"));auto* Required=F.Array(TEXT("Array_Contains"),F.Get(TEXT("NativeScopeRequired")));Connect(Root,FindPin(Required,TEXT("ItemToFind")));auto* Needed=F.Branch(Required->GetReturnValuePin());auto* Bank=At(F,F.Get(TEXT("NativeScopeBanks")),Index);F.Require(F.IsValid(Bank));auto* Current=Find(F,Bank,Root);F.Require(F.And(Current->GetReturnValuePin(),F.Same(FindPin(Current,TEXT("Value")),At(F,F.Get(TEXT("NativeScopeInstalled")),Index))));F.Merge(Needed->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    return Master->Status!=BS_Error;
}

// Stage4 keeps generic diagnostics out of the HUD. This particular refusal
// must still reach the local player through the native message path.
bool InstallDORRewardRegistrationNotice(UBlueprint* Master)
{
    auto* Graph=FindFunctionGraph(Master,TEXT("DOR_NativeRequireRegistered"));
    if(!Graph)return false;
    UK2Node_CallFunction* Record=nullptr;
    for(auto* Node:Graph->Nodes)
    {
        auto* Call=Cast<UK2Node_CallFunction>(Node);
        if(!Call || Call->FunctionReference.GetMemberName()!=TEXT("DOR_DebugNotify"))continue;
        if(Record)return false;
        Record=Call;
    }
    if(!Record || Record->GetThenPin()->LinkedTo.Num()!=1)return false;
    auto* Continue=Record->GetThenPin()->LinkedTo[0];
    if(!Cast<UK2Node_FunctionResult>(Continue->GetOwningNode()))return false;

    auto* State=AddCall(Graph,UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"),0,0);
    auto* Valid=AddCall(Graph,UKismetSystemLibrary::StaticClass(),TEXT("IsValid"),160,0);
    auto* HasState=AddNode<UK2Node_IfThenElse>(Graph,320,0);
    HasState->AllocateDefaultPins();
    auto* Post=AddCall(Graph,AFSDGameState::StaticClass(),TEXT("PostGameMessage"),480,0);
    FindPin(Post,TEXT("Msg"))->DefaultValue=FindPin(Record,TEXT("Message"))->DefaultValue;
    Record->GetThenPin()->BreakAllPinLinks();
    return Connect(SelfPin(Graph,0,0),FindPin(State,TEXT("WorldContextObject"))) &&
        Connect(State->GetReturnValuePin(),FindPin(Valid,TEXT("Object"))) &&
        Connect(Valid->GetReturnValuePin(),HasState->GetConditionPin()) &&
        Connect(Record->GetThenPin(),HasState->GetExecPin()) &&
        Connect(HasState->GetThenPin(),Post->GetExecPin()) &&
        Connect(HasState->GetElsePin(),Continue) &&
        Connect(State->GetReturnValuePin(),FindPin(Post,UEdGraphSchema_K2::PN_Self)) &&
        Connect(Post->GetThenPin(),Continue);
}
bool ValidateDORNativeBankScope(UBlueprint* Master,UBlueprint* Request,UBlueprint* Safety)
{
    FEditorScriptExecutionGuard Guard;
    UWorld* World=UWorld::CreateWorld(EWorldType::EditorPreview,false);if(!World)return false;
    auto* M=World->SpawnActor(Master->GeneratedClass);auto* S=World->SpawnActor(Safety->GeneratedClass);
    auto* R=NewObject<UObject>(GetTransientPackage(),Request->GeneratedClass);
    auto* RootClass=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.OverclockUpgrade"));
    auto* Bank=NewObject<UOverclockBank>();auto* Bank2=NewObject<UOverclockBank>();
    auto* Root=NewObject<UObject>(GetTransientPackage(),RootClass);auto* Root2=NewObject<UObject>(GetTransientPackage(),RootClass);auto* OtherRoot=NewObject<UObject>(GetTransientPackage(),RootClass);
    auto* Prior=NewObject<USchematic>();auto* Installed=NewObject<USchematic>();auto* Other=NewObject<USchematic>();
    bool Passed=M&&S&&R&&Root&&Root2&&OtherRoot;int32 Checks=0;const FGuid Token(1,2,3,4);
    auto Object=[](UObject* O,const TCHAR* N,UObject* V){FindFProperty<FObjectProperty>(O->GetClass(),N)->SetObjectPropertyValue_InContainer(O,V);};
    auto Bool=[](UObject* O,const TCHAR* N,bool V){FindFProperty<FBoolProperty>(O->GetClass(),N)->SetPropertyValue_InContainer(O,V);};
    auto Check=[&](bool V,const TCHAR* Stage){++Checks;Passed=Passed&&V;if(!V)UE_LOG(LogTemp,Error,TEXT("DOR-NATIVE-BANK-VM case=%d stage=%s failed"),Checks,Stage);};
    auto* JournalProperty=FindFProperty<FObjectProperty>(Safety->GeneratedClass,TEXT("DOR_Journal"));
    auto* J=JournalProperty?NewObject<UObject>(GetTransientPackage(),JournalProperty->PropertyClass):nullptr;
    if(Passed&&J)
    {
        Object(M,TEXT("NativeActiveRequest"),R);Bool(M,TEXT("NativeExecutionHeld"),true);Object(S,TEXT("DOR_Journal"),J);Bool(S,TEXT("DOR_Active"),true);
        *FindFProperty<FStructProperty>(R->GetClass(),TEXT("Request"))->ContainerPtrToValuePtr<FGuid>(R)=Token;
        FindFProperty<FStrProperty>(J->GetClass(),TEXT("TransactionId"))->SetPropertyValue_InContainer(J,TEXT("synthetic-bank"));
        Bank->Overclocks.Add(reinterpret_cast<UOverclockUpgrade*>(Root),Prior);
        auto Invoke=[&](const TCHAR* Name,std::initializer_list<UObject*> RootValues,std::initializer_list<UObject*> BankValues,std::initializer_list<UObject*> ValueValues)
        {
            auto* Fn=M->FindFunction(Name);FStructOnScope Params(Fn);*FindFProperty<FStructProperty>(Fn,TEXT("RequestId"))->ContainerPtrToValuePtr<FGuid>(Params.GetStructMemory())=Token;
            if(FString(Name)==TEXT("DOR_NativeBankCapture"))
            {
                for(const auto& Pair:{TPair<const TCHAR*,std::initializer_list<UObject*>>(TEXT("Roots"),RootValues),TPair<const TCHAR*,std::initializer_list<UObject*>>(TEXT("Banks"),BankValues),TPair<const TCHAR*,std::initializer_list<UObject*>>(TEXT("Installed"),ValueValues)})
                {auto* P=FindFProperty<FArrayProperty>(Fn,Pair.Key);auto* Inner=CastFieldChecked<FObjectProperty>(P->Inner);FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Params.GetStructMemory()));A.Resize(static_cast<int32>(Pair.Value.size()));int32 I=0;for(auto* V:Pair.Value)Inner->SetObjectPropertyValue(A.GetRawPtr(I++),V);}
                FindFProperty<FObjectProperty>(Fn,TEXT("Safety"))->SetObjectPropertyValue_InContainer(Params.GetStructMemory(),S);FindFProperty<FStrProperty>(Fn,TEXT("JournalId"))->SetPropertyValue_InContainer(Params.GetStructMemory(),TEXT("synthetic-bank"));
            }
            if(FString(Name)==TEXT("DOR_NativeBankRearmOriginal"))
            {auto* P=FindFProperty<FArrayProperty>(Fn,TEXT("RequiredRoots"));auto* Inner=CastFieldChecked<FObjectProperty>(P->Inner);FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Params.GetStructMemory()));A.Resize(static_cast<int32>(RootValues.size()));int32 I=0;for(auto* V:RootValues)Inner->SetObjectPropertyValue(A.GetRawPtr(I++),V);}
            M->ProcessEvent(Fn,Params.GetStructMemory());return FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(Params.GetStructMemory());
        };
        auto Present=[&](UOverclockBank* B,UObject* Key,USchematic* Value){auto* V=B->Overclocks.Find(reinterpret_cast<UOverclockUpgrade*>(Key));return V&&*V==Value;};
        Check(Invoke(TEXT("DOR_NativeBankCapture"),{Root,Root2},{Bank,Bank2},{Installed,Installed})&&Present(Bank,Root,Prior)&&Bank2->Overclocks.Num()==0,TEXT("capture-all-priors-before-any-mutation"));
        Check(Invoke(TEXT("DOR_NativeBankStage"),{},{},{})&&Present(Bank,Root,Installed)&&Present(Bank2,Root2,Installed),TEXT("two-root-two-bank-stage-and-readback"));
        Bank->Overclocks.Add(reinterpret_cast<UOverclockUpgrade*>(Root),Other);Bank2->Overclocks.Add(reinterpret_cast<UOverclockUpgrade*>(OtherRoot),Other);
        Check(!Invoke(TEXT("DOR_NativeBankRestore"),{},{},{})&&Present(Bank,Root,Other)&&Present(Bank2,Root2,Installed),TEXT("conflict-preflight-prevents-partial-restore"));
        Check(FindFProperty<FBoolProperty>(M->GetClass(),TEXT("NativeScopeActive"))->GetPropertyValue_InContainer(M)&&FindFProperty<FBoolProperty>(M->GetClass(),TEXT("NativeScopeConflict"))->GetPropertyValue_InContainer(M),TEXT("conflict-retains-scope-barrier"));
        Bank->Overclocks.Add(reinterpret_cast<UOverclockUpgrade*>(Root),Installed);
        Check(Invoke(TEXT("DOR_NativeBankRestore"),{},{},{})&&Present(Bank,Root,Prior)&&!Bank2->Overclocks.Contains(reinterpret_cast<UOverclockUpgrade*>(Root2))&&Present(Bank2,OtherRoot,Other),TEXT("restore-exact-held-absent-and-unrelated-priors"));
        Check(Invoke(TEXT("DOR_NativeBankRestore"),{},{},{}),TEXT("restore-idempotent-actual-receipt"));
        Bank->Overclocks.Add(reinterpret_cast<UOverclockUpgrade*>(Root),Other);
        Check(Invoke(TEXT("DOR_NativeBankRearmOriginal"),{Root2},{},{})&&Present(Bank,Root,Other)&&Present(Bank2,Root2,Installed)&&Invoke(TEXT("DOR_NativeBankReady"),{},{},{}),TEXT("same-prior-recovery-only-original-required-root"));
        Check(!Invoke(TEXT("DOR_NativeBankRearmOriginal"),{Root2},{},{})&&Present(Bank2,Root2,Installed),TEXT("repeated-recovery-enrollment-rejected"));
        Check(Invoke(TEXT("DOR_NativeBankRestore"),{},{},{})&&Present(Bank,Root,Other)&&!Bank2->Overclocks.Contains(reinterpret_cast<UOverclockUpgrade*>(Root2)),TEXT("recovery-does-not-touch-accepted-only-foreign-entry"));
        Bank->Overclocks.Add(reinterpret_cast<UOverclockUpgrade*>(Root),Installed);
        Check(Invoke(TEXT("DOR_NativeBankCapture"),{Root},{Bank},{Installed})&&Invoke(TEXT("DOR_NativeBankStage"),{},{},{}),TEXT("already-held-installed-root-is-unchanged"));
        Bank->Overclocks.Remove(reinterpret_cast<UOverclockUpgrade*>(Root));
        Check(Invoke(TEXT("DOR_NativeBankRestore"),{},{},{})&&!Bank->Overclocks.Contains(reinterpret_cast<UOverclockUpgrade*>(Root)),TEXT("unchanged-entry-third-party-removal-preserved"));
        Check(!Invoke(TEXT("DOR_NativeBankCapture"),{Root,Root},{Bank,Bank},{Installed,Installed})&&!FindFProperty<FBoolProperty>(M->GetClass(),TEXT("NativeScopeActive"))->GetPropertyValue_InContainer(M),TEXT("duplicate-root-rejected-before-mutation"));
        Check(!Invoke(TEXT("DOR_NativeBankCapture"),{Root,Root2},{Bank},{Installed,Installed}),TEXT("mismatched-input-cardinality-rejected"));
        Bank->Overclocks.Add(reinterpret_cast<UOverclockUpgrade*>(Root),Prior);
        Check(Invoke(TEXT("DOR_NativeBankCapture"),{Root},{Bank},{Installed})&&Invoke(TEXT("DOR_NativeBankStage"),{},{},{})&&Invoke(TEXT("DOR_NativeBankRestore"),{},{},{})&&Invoke(TEXT("DOR_NativeBankRearmOriginal"),{},{},{})&&Present(Bank,Root,Prior)&&Invoke(TEXT("DOR_NativeBankReady"),{},{},{}),TEXT("vanilla-original-recovery-needs-no-accepted-root-registration"));
        Check(Invoke(TEXT("DOR_NativeBankRestore"),{},{},{})&&Present(Bank,Root,Prior),TEXT("empty-original-scope-restores-without-writing"));
        Check(!Invoke(TEXT("DOR_NativeBankCapture"),{},{},{}),TEXT("fusion-adapter-cannot-invent-empty-bank-readiness"));FindFProperty<FIntProperty>(R->GetClass(),TEXT("Operation"))->SetPropertyValue_InContainer(R,1);Check(!Invoke(TEXT("DOR_NativeBankCapture"),{},{},{}),TEXT("clear-empty-scope-requires-bound-schema3-journal"));FindFProperty<FIntProperty>(J->GetClass(),TEXT("Schema"))->SetPropertyValue_InContainer(J,3);*FindFProperty<FStructProperty>(J->GetClass(),TEXT("NativeRequest"))->ContainerPtrToValuePtr<FGuid>(J)=Token;
        Check(Invoke(TEXT("DOR_NativeBankCapture"),{},{},{})&&Invoke(TEXT("DOR_NativeBankReady"),{},{},{})&&Present(Bank,Root,Prior),TEXT("bound-clear-zero-bank-scope-has-no-asset-mutation"));Check(Invoke(TEXT("DOR_NativeBankStage"),{},{},{})&&Invoke(TEXT("DOR_NativeBankRestore"),{},{},{})&&Present(Bank,Root,Prior),TEXT("empty-clear-selective-bank-closure-idempotent-no-write"));
    }
    else Passed=false;
    World->DestroyWorld(false);UE_LOG(LogTemp,Display,TEXT("DOR-NATIVE-BANK-VM checks=%d passed=%d scope=generated-multi-root-memory-bank-no-save-no-gameplay"),Checks,Passed);return Passed;
}
