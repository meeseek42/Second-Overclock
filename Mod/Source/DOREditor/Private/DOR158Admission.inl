UK2Node_FunctionResult* DOR158BoolResult(UEdGraph* G)
{
    auto* R=AddNode<UK2Node_FunctionResult>(G,0,0);R->FunctionReference=FindFunctionEntry(G)->FunctionReference;R->AllocateDefaultPins();if(!R->FindPin(TEXT("ReturnValue"))){auto* P=R->CreateUserDefinedPin(TEXT("ReturnValue"),MakeBoolPinType(),EGPD_Input);const FName Old=P->PinName;P->PinName=TEXT("ReturnValue");for(auto& I:R->UserDefinedPins)if(I->PinName==Old)I->PinName=TEXT("ReturnValue");}FindPin(R,TEXT("ReturnValue"))->DefaultValue=TEXT("false");return R;
}
// Promotion is exact target replicated evidence. Unknown is permissive.
bool BuildDOR158Admission(UBlueprint* Master,UBlueprint* Proxy)
{
    auto* SC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SaveGameStateComponent"));
    auto* Progress=FindObject<UScriptStruct>(nullptr,TEXT("/Script/FSD.CharacterProgress"));if(!SC||!Progress)return false;
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_TargetUnpromoted"),Master->GeneratedClass);
        auto* P=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));auto* Trace=F.Input(TEXT("Trace"),MakeBoolPinType());
        F.QuietRequire(F.IsValid(P));auto ID=DORPlayerIdentity(F,P);F.QuietRequire(ID.Ready);
        auto* PS=F.Cache(TEXT("State"),F.Read(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"),P));F.QuietRequire(F.IsValid(PS));
        auto* C=F.Cache(TEXT("Native"),F.Component(PS,SC));F.QuietRequire(F.IsValid(C));auto* Owner=F.Cache(TEXT("Owner"),F.Read(UActorComponent::StaticClass(),TEXT("GetOwner"),C));F.QuietRequire(F.Same(Owner,PS));
        auto* Character=F.Cache(TEXT("Character"),F.Read(AFSDPlayerState::StaticClass(),TEXT("GetSelectedCharacterID"),PS));F.QuietRequire(F.IsValid(Character));
        auto* Count=F.Cache(TEXT("Matches"),F.Int(0));auto* Retired=F.Cache(TEXT("Retired"),F.Int(-1));auto* Loop=F.Each(F.Field(C,SC,TEXT("CharacterStats")));auto* E=FindPin(Loop,TEXT("Array Element"));auto* Match=F.Branch(F.Same(Character,F.FieldOf(E,Progress,TEXT("characterID"))));F.Set(F.Variable(Count),F.Math(TEXT("Add_IntInt"),Count,F.Int(1)));F.Set(F.Variable(Retired),F.FieldOf(E,Progress,TEXT("TimesRetired")));F.Merge(Match->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));
        auto* Unpromoted=F.Cache(TEXT("Unpromoted"),F.And(F.Eq(Count,F.Int(1)),F.Eq(Retired,F.Int(0))));auto* Observe=F.Branch(Trace);
        FDORNativeProbeLine D(F,TEXT("Promotion"),TEXT("[DOR-ADMISSION]|stage=target-promotion|source=replicated-CharacterStats|unknown=allowed|source-OC-ownership=not-a-gate"));D.Add(TEXT("key"),ID.Key,NAME_None);D.Add(TEXT("player"),F.Path(P),NAME_None);D.Add(TEXT("state"),F.Path(PS),NAME_None);D.Add(TEXT("component"),F.Path(C),NAME_None);D.Add(TEXT("componentOwner"),F.Path(Owner),NAME_None);D.Add(TEXT("character"),F.Path(Character),NAME_None);D.Add(TEXT("matchingStats"),Count,TEXT("Conv_IntToString"));D.Add(TEXT("TimesRetired"),Retired,TEXT("Conv_IntToString"));D.Add(TEXT("unpromoted"),Unpromoted);D.Record();
        // Weapon unlock is independent evidence, captured once per preflight.
        auto* Items=F.Field(C,SC,TEXT("EquippedLoadout"));auto* Selections=F.Each(F.Field(C,SC,TEXT("ItemUpgradeSelections")));auto* S=FindPin(Selections,TEXT("Array Element"));auto* Weapon=F.FieldOf(S,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID"));auto* Relevant=F.Branch(F.Or(F.Eq(Weapon,F.FieldOf(Items,FItemLoadout::StaticStruct(),TEXT("PrimaryWeapon")),TEXT("EqualEqual_GuidGuid")),F.Eq(Weapon,F.FieldOf(Items,FItemLoadout::StaticStruct(),TEXT("SecondaryWeapon")),TEXT("EqualEqual_GuidGuid"))));FDORNativeProbeLine W(F,TEXT("Unlock"),TEXT("[DOR-ADMISSION]|stage=target-weapon-unlock|promotion-gate=false"));W.Add(TEXT("key"),ID.Key,NAME_None);W.Add(TEXT("weapon"),Weapon,TEXT("Conv_GuidToString"));W.Add(TEXT("OverclockingUnlocked"),F.FieldOf(S,FItemUpgradeSelection::StaticStruct(),TEXT("OverclockingUnlocked")));W.Record();F.Merge(Relevant->GetElsePin());F.Tail=FindPin(Selections,TEXT("Completed"));F.Merge(Observe->GetElsePin());F.Return(Unpromoted);F.Compile();
    }
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_TargetForManualKey"),Master->GeneratedClass);F.ResultType=MakeObjectPinType(APlayerCharacter::StaticClass());auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* Found=F.Cache(TEXT("Found"),nullptr,TEXT(""),MakeObjectPinType(APlayerCharacter::StaticClass()));auto* Count=F.Cache(TEXT("Matches"),F.Int(0));auto* Loop=F.Each(DORRoster(F));auto* P=FindPin(Loop,TEXT("Array Element"));auto ID=DORPlayerIdentity(F,P);auto* Match=F.Branch(F.And(ID.Ready,F.Eq(ID.Key,Key,TEXT("EqualEqual_StrStr"))));F.Set(F.Variable(Found),P);F.Set(F.Variable(Count),F.Math(TEXT("Add_IntInt"),Count,F.Int(1)));F.Merge(Match->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));auto* Unique=F.Branch(F.Eq(Count,F.Int(1)));F.Return(Found);F.Tail=Unique->GetElsePin();F.Return();F.Compile();
    }
    // Signature is available to early scheduler graphs; localized body installed
    // after the existing notification RPC gains MessageEn.
    {FDOROrdinaryGraph F(Master,TEXT("DOR_NotifyUnpromoted"),Master->GeneratedClass);F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));F.Input(TEXT("AutomaticSupply"),MakeBoolPinType());F.Done();F.Compile();}
    return Master->Status!=BS_Error;
}

bool InstallDOR158Admission(UBlueprint* Master,UBlueprint* Proxy,UBlueprint* MenuHost)
{
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_NotifyUnpromoted"),Master->GeneratedClass);auto* Player=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));auto* Supply=F.Input(TEXT("AutomaticSupply"),MakeBoolPinType());auto* Ensure=F.Self(TEXT("DOR_OrdinaryEnsureProxy"));Connect(Player,FindPin(Ensure,TEXT("Player")));auto* P=F.Cache(TEXT("Proxy"),Ensure->GetReturnValuePin());F.QuietRequire(F.IsValid(P));F.QuietRequire(F.Same(F.Field(P,Proxy->GeneratedClass,TEXT("OwningRewardPlayer")),Player));auto* Controller=F.Read(APlayerCharacter::StaticClass(),TEXT("GetPlayerController"),Player);F.QuietRequire(F.And(F.IsValid(Controller),F.Same(F.Read(AActor::StaticClass(),TEXT("GetOwner"),P),Controller)));
        auto Message=[&](bool En){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectString"));FindPin(N,TEXT("A"))->DefaultValue=En?TEXT("[Management] Your selected class has not been promoted. Field Resupply was selected automatically; if downed, it will be delivered after revival."):TEXT("[管理层] 当前职业尚未晋升，已自动选择标准战地补给；倒地时将在救起后发放。");FindPin(N,TEXT("B"))->DefaultValue=En?TEXT("[Management] The selected player's current class has not been promoted. Fusion cannot be applied."):TEXT("[管理层] 所选玩家的当前职业尚未晋升，无法应用融合。");Connect(Supply,FindPin(N,TEXT("bPickA")));return N->GetReturnValuePin();};auto* N=F.Call(Proxy->GeneratedClass,TEXT("DOR_ClientPlayerNotification"));Connect(P,FindPin(N,UEdGraphSchema_K2::PN_Self));Connect(Message(false),FindPin(N,TEXT("Message")));Connect(Message(true),FindPin(N,TEXT("MessageEn")));F.Done();F.Compile();
    }
    // Manual preflight precedes enqueue and the explicit-save suppression latch.
    auto Gate=[&](UBlueprint* BP,const TCHAR* Name,UClass* Owner,UEdGraphPin* Player,UEdGraphPin* M,UK2Node_CallFunction* Resolve,bool Notify)
    {
        auto* G=FindFunctionGraph(BP,Name);auto* Out=FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then);auto Next=Out->LinkedTo;Out->BreakAllPinLinks();auto* Q=AddCall(G,Master->GeneratedClass,TEXT("DOR_TargetUnpromoted"),0,0);Connect(M,FindPin(Q,UEdGraphSchema_K2::PN_Self));Connect(Player,FindPin(Q,TEXT("Player")));FindPin(Q,TEXT("Trace"))->DefaultValue=Notify?TEXT("true"):TEXT("false");if(Resolve->GetExecPin()){Connect(Out,Resolve->GetExecPin());Connect(Resolve->GetThenPin(),Q->GetExecPin());}else Connect(Out,Q->GetExecPin());auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);B->AllocateDefaultPins();Connect(Q->GetThenPin(),B->GetExecPin());Connect(Q->GetReturnValuePin(),B->GetConditionPin());for(auto* P:Next)Connect(B->GetElsePin(),P);
        UEdGraphPin* Reject=B->GetThenPin();if(Notify){auto* N=AddCall(G,Master->GeneratedClass,TEXT("DOR_NotifyUnpromoted"),0,0);Connect(M,FindPin(N,UEdGraphSchema_K2::PN_Self));Connect(Player,FindPin(N,TEXT("Player")));Connect(Reject,N->GetExecPin());Reject=N->GetThenPin();}
        auto* Result=DOR158BoolResult(G);Connect(Reject,FindPin(Result,UEdGraphSchema_K2::PN_Execute));
    };
    for(const TCHAR* Name:{TEXT("DOR_NativeSubmitManual"),TEXT("DOR_ManualRewardSaveLatch")})
    {
        auto* G=FindFunctionGraph(MenuHost,Name);auto* E=FindFunctionEntry(G);if(!E->FindPin(TEXT("Player"))) {E->CreateUserDefinedPin(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()),EGPD_Output);E->ReconstructNode();}
        auto* Find=AddCall(G,UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"),0,0);FindPin(Find,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;Connect(SelfPin(G,0,0),FindPin(Find,TEXT("WorldContextObject")));auto* Cast=AddNode<UK2Node_DynamicCast>(G,0,0);Cast->TargetType=Master->GeneratedClass;Cast->SetPurity(true);Cast->AllocateDefaultPins();Connect(Find->GetReturnValuePin(),Cast->GetCastSourcePin());Gate(MenuHost,Name,Master->GeneratedClass,E->FindPin(TEXT("Player")),Cast->GetCastResultPin(),Find,FString(Name)==TEXT("DOR_NativeSubmitManual"));
    }
    // Propagate exact saved target into the latch, never the host's local pawn.
    {auto* G=FindFunctionGraph(MenuHost,TEXT("DOR_SetManual"));for(auto* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("DOR_ManualRewardSaveLatch")){C->ReconstructNode();Connect(FindFunctionEntry(G)->FindPin(TEXT("Player")),FindPin(C,TEXT("Player")));}}
    {
        auto* G=FindFunctionGraph(Master,TEXT("DOR_NativeSubmitChoice"));auto* E=FindFunctionEntry(G);UK2Node_CallFunction* Enqueue=nullptr;for(auto* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("DOR_NativeProducerEnqueue"))Enqueue=C;if(!Enqueue)return false;
        auto* Player=FindPin(Enqueue,TEXT("Player"))->LinkedTo[0];auto* P=FindPin(Enqueue,TEXT("Proxy"))->LinkedTo[0];auto* Before=AddSelfCall(G,TEXT("DOR_TargetUnpromoted"),0,0);Connect(Player,FindPin(Before,TEXT("Player")));FindPin(Before,TEXT("Trace"))->DefaultValue=TEXT("true");auto Prev=Enqueue->GetExecPin()->LinkedTo;Enqueue->GetExecPin()->BreakAllPinLinks();for(auto* V:Prev)Connect(V,Before->GetExecPin());auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);B->AllocateDefaultPins();Connect(Before->GetThenPin(),B->GetExecPin());Connect(Before->GetReturnValuePin(),B->GetConditionPin());Connect(B->GetElsePin(),Enqueue->GetExecPin());
        auto* Supply=AddSelfCall(G,TEXT("DOR_SubmitSupplyChoice"),0,0);Connect(P,FindPin(Supply,TEXT("ProxyActor")));Connect(B->GetThenPin(),Supply->GetExecPin());auto* Accepted=AddNode<UK2Node_IfThenElse>(G,0,0);Accepted->AllocateDefaultPins();Connect(Supply->GetThenPin(),Accepted->GetExecPin());Connect(Supply->GetReturnValuePin(),Accepted->GetConditionPin());auto* Notify=AddSelfCall(G,TEXT("DOR_NotifyUnpromoted"),0,0);Connect(Player,FindPin(Notify,TEXT("Player")));FindPin(Notify,TEXT("AutomaticSupply"))->DefaultValue=TEXT("true");Connect(Accepted->GetThenPin(),Notify->GetExecPin());
        auto* Result=DOR158BoolResult(G);Connect(Supply->GetReturnValuePin(),FindPin(Result,TEXT("ReturnValue")));Connect(Notify->GetThenPin(),FindPin(Result,UEdGraphSchema_K2::PN_Execute));Connect(Accepted->GetElsePin(),FindPin(Result,UEdGraphSchema_K2::PN_Execute));
    }
    for(auto* BP:{Master,MenuHost}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}return true;
}
