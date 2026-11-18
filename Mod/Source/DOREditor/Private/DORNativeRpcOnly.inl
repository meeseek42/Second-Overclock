// Run after send instrumentation: the only replicated interval is the synchronous
// reliable owner RPC. Same-PlayerState placement retains native loadout FIFO.
bool InstallDORNativeRpcOnly(UBlueprint* Master,UBlueprint* Context)
{
    const TMap<FName,FName> Bodies={
        {TEXT("DOR_NativeClientArm"),TEXT("DOR_NativeAcceptArm")},
        {TEXT("DOR_NativeClientPrepare"),TEXT("DOR_NativePrepareOwner")},
        {TEXT("DOR_NativeClientObserve"),TEXT("DOR_NativeBeginOwnerObservation")},
        {TEXT("DOR_NativeClientCleanup"),TEXT("DOR_NativeCleanupOwner")},
        {TEXT("DOR_NativeClientFinalize"),TEXT("DOR_NativeFinalizeOwner")},
        {TEXT("DOR_NativeClientStop"),TEXT("DOR_NativeStopOwner")},
        {TEXT("DOR_NativeClientReleaseIdle"),TEXT("DOR_NativeReleaseIdleOwner")},
        {TEXT("DOR_NativeClientLegacyApply"),TEXT("DOR_NativeLegacyApplyOwner")}};
    TArray<UK2Node_CallFunction*> Sends;TSet<FName> Found;
    for(auto* BP:{Master,Context})for(auto* G:BP->FunctionGraphs)for(auto* Node:G->Nodes)
        if(auto* Call=Cast<UK2Node_CallFunction>(Node))
        {
            const auto Name=Call->FunctionReference.GetMemberName();
            if(!Name.ToString().StartsWith(TEXT("DOR_NativeClient")))continue;
            if(!Bodies.Contains(Name)||!Call->GetExecPin()||!Call->GetExecPin()->LinkedTo.Num())return false;
            Sends.Add(Call);Found.Add(Name);
        }
    if(Found.Num()!=Bodies.Num())return false;
    {
        FDORNativeGraph F(Context,TEXT("DOR_NativeTransportSendReceipt"),Master->GeneratedClass);
        auto* Endpoint=F.Input(TEXT("Endpoint"),MakeStringPinType());auto* Route=F.Input(TEXT("Route"),MakeStringPinType());
        auto* M=F.Get(TEXT("NativeMaster"));F.QuietRequire(F.IsValid(M));
        FDORRewardReceipt D(F.G,TEXT("transport-send-returned"));D.Add(TEXT("endpoint"),Endpoint);D.Add(TEXT("route"),Route);
        D.Path(TEXT("context"),SelfPin(F.G,0,700));D.Add(TEXT("replicatingAfterSend"),F.Field(SelfPin(F.G,0,700),UActorComponent::StaticClass(),TEXT("bReplicates")));
        for(const auto& T:DORNativeTuplePins()){auto* V=F.Get(FName(*(FString(TEXT("Native"))+T.Key.ToString())));D.Add(*T.Key.ToString(),T.Key==TEXT("Epoch")||T.Key==TEXT("Request")?F.Text(V,TEXT("Conv_GuidToString")):V);}
        auto* Record=F.Call(Master->GeneratedClass,TEXT("DOR_DiagnosticRecordOnly"));Connect(M,FindPin(Record,UEdGraphSchema_K2::PN_Self));Connect(D.Line,FindPin(Record,TEXT("Message")));F.Done();F.Compile();
    }
    for(const auto& Pair:Bodies)
    {
        const FName SendName(*(Pair.Key.ToString().Replace(TEXT("Client"),TEXT("Send"))));
        auto* Fn=Context->GeneratedClass->FindFunctionByName(Pair.Key);if(!Fn||!(Fn->FunctionFlags&FUNC_NetReliable)||!(Fn->FunctionFlags&FUNC_NetClient)||(Fn->FunctionFlags&FUNC_NetMulticast))return false;
        FDORNativeGraph F(Context,SendName,Master->GeneratedClass);TMap<FName,UEdGraphPin*> Args;
        for(TFieldIterator<FProperty> It(Fn);It&&(It->PropertyFlags&CPF_Parm);++It)
        {
            if(It->PropertyFlags&CPF_ReturnParm)return false;
            FEdGraphPinType Type;if(!GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(*It,Type))return false;
            Type.bIsReference=false;Type.bIsConst=false;
            Args.Add(It->GetFName(),F.Cache(*It->GetName(),F.Input(It->GetFName(),Type)));
        }
        auto* Self=SelfPin(F.G,0,700);auto* Owner=F.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));
        auto* State=F.Cache(TEXT("OwnerState"),F.Cast(Owner->GetReturnValuePin(),AFSDPlayerState::StaticClass()));F.Require(F.IsValid(State));
        auto* Authority=F.Call(AActor::StaticClass(),TEXT("HasAuthority"));Connect(State,FindPin(Authority,UEdGraphSchema_K2::PN_Self));F.Require(Authority->GetReturnValuePin());
        F.Require(F.Same(F.Component(State,Context->GeneratedClass),Self));
        F.Require(F.Not(F.Field(Self,UActorComponent::StaticClass(),TEXT("bReplicates"))));
        auto* Local=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerController"));Connect(Self,FindPin(Local,TEXT("WorldContextObject")));
        auto* PC=F.Cache(TEXT("LocalController"),Local->GetReturnValuePin());
        auto* LocalState=F.Field(PC,AController::StaticClass(),TEXT("PlayerState"));
        auto* OwnerController=F.Call(AActor::StaticClass(),TEXT("GetOwner"));Connect(State,FindPin(OwnerController,UEdGraphSchema_K2::PN_Self));
        auto* TargetPC=F.Cache(TEXT("TargetController"),F.Cast(OwnerController->GetReturnValuePin(),APlayerController::StaticClass()));F.Require(F.IsValid(TargetPC));
        F.Require(F.Same(F.Field(TargetPC,AController::StaticClass(),TEXT("PlayerState")),State));
        auto* IsLocal=F.Call(AController::StaticClass(),TEXT("IsLocalController"));Connect(TargetPC,FindPin(IsLocal,UEdGraphSchema_K2::PN_Self));
        auto* LocalRoute=F.Cache(TEXT("LocalRoute"),F.And(IsLocal->GetReturnValuePin(),F.And(F.Same(TargetPC,PC),F.Same(State,LocalState))));
        auto WireArgs=[&](UK2Node_CallFunction* N){for(const auto& A:Args)if(auto* P=N->FindPin(A.Key))Connect(A.Value,P);};
        auto Replicate=[&](bool Value){auto* N=F.Call(UActorComponent::StaticClass(),TEXT("SetIsReplicated"));FindPin(N,TEXT("ShouldReplicate"))->DefaultValue=Value?TEXT("true"):TEXT("false");};
        auto Receipt=[&](const TCHAR* Route){auto* N=F.Self(TEXT("DOR_NativeTransportSendReceipt"));FindPin(N,TEXT("Endpoint"))->DefaultValue=Pair.Key.ToString();FindPin(N,TEXT("Route"))->DefaultValue=Route;};
        auto* LocalBranch=F.Branch(LocalRoute);Replicate(false);auto* Body=F.Self(Pair.Value);WireArgs(Body);Receipt(TEXT("authority-local"));F.Done();
        F.Tail=LocalBranch->GetElsePin();F.Require(F.Not(IsLocal->GetReturnValuePin()));
        Replicate(true);auto* RPC=F.Self(Pair.Key);WireArgs(RPC);Replicate(false);Receipt(TEXT("rpc-only"));F.Done();F.Compile();
    }
    for(auto* Call:Sends)
    {
        const FName Name(*(Call->FunctionReference.GetMemberName().ToString().Replace(TEXT("Client"),TEXT("Send"))));
        Call->FunctionReference.SetExternalMember(Name,Context->GeneratedClass);Call->ReconstructNode();
    }
    for(auto* BP:{Context,Master}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    auto* CDO=Cast<UActorComponent>(Context->GeneratedClass->GetDefaultObject());if(!CDO||CDO->GetIsReplicated())return false;
    UE_LOG(LogTemp,Display,TEXT("DOR-RPC-ONLY endpoints=%d callsites=%d defaultReplicated=false samePlayerState=true"),Found.Num(),Sends.Num());return true;
}
