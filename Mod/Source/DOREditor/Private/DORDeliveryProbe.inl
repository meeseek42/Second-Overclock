// Explicit manual delivery diagnostics only. Inactive by default; no startup
// sampling, automatic history read, admission inputs or native message replay.
struct FDORDeliveryLine
{
    FDORConnectionGraph& F; UEdGraphPin* Line;
    FDORDeliveryLine(FDORConnectionGraph& In,const TCHAR* Stage):F(In),Line(F.Join(F.Str(TEXT("[DOR-DELIVERY]|stage=")),F.Str(Stage))){}
    void Add(const TCHAR* N,UEdGraphPin* V){Line=F.Join(F.Join(Line,F.Str(*(FString(TEXT("|"))+N+TEXT("=")))),V);}
    void Bool(const TCHAR* N,UEdGraphPin* V){Add(N,F.Text(V,TEXT("Conv_BoolToString")));}
    void Int(const TCHAR* N,UEdGraphPin* V){Add(N,F.Text(V,TEXT("Conv_IntToString")));}
    void Float(const TCHAR* N,UEdGraphPin* V){Add(N,F.Text(V,TEXT("Conv_FloatToString")));}
    void Object(const TCHAR* N,UEdGraphPin* V){Add(N,F.Path(V));}
    void Record(){Float(TEXT("localSeconds"),F.RealNow());DORNormalReceipt(F,Line,true);}
};

UEdGraphPin* DORDeliverySubsystem(FDORConnectionGraph& F)
{auto* S=AddNode<UK2Node_GetSubsystem>(F.G,0,0);S->Initialize(LoadObject<UClass>(nullptr,TEXT("/Script/FSD.MessagingSubSystem")));S->AllocateDefaultPins();return F.Cache(TEXT("ProbeFreshSubsystem"),S->GetResultPin());}
UEdGraphPin* DORDeliveryStarts(FDORConnectionGraph& F,UEdGraphPin* Text,const FString& Prefix)
{auto* C=F.Call(UKismetStringLibrary::StaticClass(),TEXT("StartsWith"));Connect(Text,FindPin(C,TEXT("SourceString")));FindPin(C,TEXT("InPrefix"))->DefaultValue=Prefix;FindPin(C,TEXT("SearchCase"))->DefaultValue=TEXT("CaseSensitive");return C->GetReturnValuePin();}
UEdGraphPin* DORDeliveryClip(FDORConnectionGraph& F,UEdGraphPin* Text,int32 Limit)
{auto* C=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Left"));Connect(Text,FindPin(C,TEXT("SourceString")));FindPin(C,TEXT("Count"))->DefaultValue=FString::FromInt(Limit);return C->GetReturnValuePin();}
UEdGraphPin* DORDeliverySafePrefix(FDORConnectionGraph& F,UEdGraphPin* Text)
{auto* V=DORDeliveryClip(F,Text,32);for(const TCHAR* From:{TEXT("\r"),TEXT("\n"),TEXT("|")}){auto* C=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Replace"));Connect(V,FindPin(C,TEXT("SourceString")));FindPin(C,TEXT("From"))->DefaultValue=From;FindPin(C,TEXT("To"))->DefaultValue=TEXT("_");V=C->GetReturnValuePin();}return V;}
void DORDeliveryIdentity(FDORConnectionGraph& F,const TCHAR* Stage,bool Deduplicate=false)
{auto* Active=F.Branch(F.Self(TEXT("DOR_ConnectionProbeActive"))->GetReturnValuePin());auto* C=F.Self(TEXT("DOR_ConnectionProbeIdentity"));FindPin(C,TEXT("Stage"))->DefaultValue=Stage;FindPin(C,TEXT("Deduplicate"))->DefaultValue=Deduplicate?TEXT("true"):TEXT("false");F.Merge(Active->GetElsePin());}

bool BuildDORDeliveryProbe(UBlueprint* Safety,UClass* MT)
{
    UClass* SC=Safety->GeneratedClass;auto* MC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.MessagingSubSystem"));
    const auto ST=MakeStringPinType(),BT=MakeBoolPinType(),IT=MakeIntPinType(),OT=MakeObjectPinType(UObject::StaticClass());
    AddTypedVariable(Safety,TEXT("ConnectionProbeBoundSubsystem"),MakeObjectPinType(MC));
    for(const TCHAR* N:{TEXT("ConnectionProbeUntil")})AddTypedVariable(Safety,N,MakeFloatPinType());
    for(const TCHAR* N:{TEXT("ConnectionProbeEntries"),TEXT("ConnectionProbeSequence")})AddTypedVariable(Safety,N,IT);
    AddTypedVariable(Safety,TEXT("ConnectionProbeExhausted"),BT);AddTypedVariable(Safety,TEXT("ConnectionProbeArmed"),BT);
    AddTypedVariable(Safety,TEXT("ConnectionProbeLastIdentity"),ST);
    AddTypedVariable(Safety,TEXT("ConnectionProbeManualExchanges"),MakeStringPinType(true));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Safety);FKismetEditorUtilities::CompileBlueprint(Safety);if(Safety->Status==BS_Error)return false;
    {FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeRowReason"),MT);F.ResultType=ST;F.Input(TEXT("Index"),IT);F.Return(F.Str(TEXT("not-evaluated")));F.Compile();}
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeWindow"),MT);F.VoidResult=true;auto* Seconds=F.Input(TEXT("Seconds"),MakeFloatPinType());F.Set(TEXT("ConnectionProbeArmed"),nullptr,TEXT("true"));F.Set(TEXT("ConnectionProbeUntil"),F.Math(TEXT("Add_FloatFloat"),F.RealNow(),Seconds));F.Set(TEXT("ConnectionProbeEntries"),F.Int(0));F.Set(TEXT("ConnectionProbeExhausted"),nullptr,TEXT("false"));FDORDeliveryLine D(F,TEXT("window-armed"));D.Float(TEXT("seconds"),Seconds);D.Float(TEXT("until"),F.Get(TEXT("ConnectionProbeUntil")));D.Object(TEXT("self"),SelfPin(F.G,0,0));D.Record();F.Done();F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeActive"),MT);F.QuietRequire(F.Get(TEXT("ConnectionProbeArmed")));auto* Time=F.Math(TEXT("Less_FloatFloat"),F.RealNow(),F.Get(TEXT("ConnectionProbeUntil")));F.Return(F.And(Time,F.Greater(F.Int(64),F.Get(TEXT("ConnectionProbeEntries")))));F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeIdentity"),MT);F.VoidResult=true;auto* Stage=F.Input(TEXT("Stage"),ST);auto* Dedup=F.Input(TEXT("Deduplicate"),BT);
        auto* GS=F.Cache(TEXT("FreshGS"),F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"))->GetReturnValuePin());auto* PC=F.LocalPC();
        auto* Host=F.Cache(TEXT("FreshHost"),nullptr,TEXT(""),MakeObjectPinType(AFSDPlayerState::StaticClass()));auto* PS=F.Cache(TEXT("FreshPS"),nullptr,TEXT(""),MakeObjectPinType(AFSDPlayerState::StaticClass()));auto* NC=F.Cache(TEXT("FreshNC"),nullptr,TEXT(""),MakeObjectPinType(UNetConnection::StaticClass()));
        auto* HostListed=F.Cache(TEXT("HostListed"),F.Eq(F.Int(0),F.Int(1)));auto* ValidGS=F.Branch(F.IsValid(GS));F.Set(F.Variable(Host),F.Read(AFSDGameState::StaticClass(),TEXT("GetServerPlayerState"),GS));F.Set(F.Variable(HostListed),F.Contains(F.Field(GS,AGameStateBase::StaticClass(),TEXT("PlayerArray")),Host));F.Merge(ValidGS->GetElsePin());
        auto* ValidPC=F.Branch(F.IsValid(PC));F.Set(F.Variable(PS),F.Read(AFSDPlayerController::StaticClass(),TEXT("GetFSDPlayerState"),PC));auto* Net=F.Self(TEXT("DOR_ConnectionNativeNet"));Connect(PC,FindPin(Net,TEXT("Controller")));F.Set(F.Variable(NC),Net->GetReturnValuePin());F.Merge(ValidPC->GetElsePin());
        auto* NativeLocal=F.Cache(TEXT("NativeLocal"),F.Str(TEXT("unavailable")));auto* NativeHost=F.Cache(TEXT("NativeHost"),F.Str(TEXT("unavailable")));auto* PSOK=F.Branch(F.IsValid(PS));F.Set(F.Variable(NativeLocal),F.NativeKey(PS));F.Merge(PSOK->GetElsePin());auto* HostOK=F.Branch(F.IsValid(Host));F.Set(F.Variable(NativeHost),F.NativeKey(Host));F.Merge(HostOK->GetElsePin());
        auto* Selected=F.Selected(SC);auto* All=F.Call(UGameplayStatics::StaticClass(),TEXT("GetAllActorsOfClass"));FindPin(All,TEXT("ActorClass"))->DefaultObject=SC;
        FDORDeliveryLine D(F,TEXT("identity"));D.Object(TEXT("self"),SelfPin(F.G,0,0));D.Object(TEXT("selected"),Selected);D.Int(TEXT("safetyCount"),F.Count(FindPin(All,TEXT("OutActors"))));
        auto* GI=F.Call(UGameplayStatics::StaticClass(),TEXT("GetGameInstance"));D.Object(TEXT("GI"),GI->GetReturnValuePin());auto* Level=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetOuterObject"));Connect(GS,FindPin(Level,TEXT("Object")));auto* World=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetOuterObject"));Connect(Level->GetReturnValuePin(),FindPin(World,TEXT("Object")));D.Object(TEXT("world"),World->GetReturnValuePin());
        D.Object(TEXT("oldGS"),F.Get(TEXT("ConnectionGS")));D.Object(TEXT("currentGS"),GS);D.Object(TEXT("oldHost"),F.Get(TEXT("ConnectionHost")));D.Object(TEXT("currentHost"),Host);D.Object(TEXT("oldPC"),F.Get(TEXT("ConnectionLocalController")));D.Object(TEXT("currentPC"),PC);D.Object(TEXT("oldPS"),F.Get(TEXT("ConnectionLocalState")));D.Object(TEXT("currentPS"),PS);D.Object(TEXT("oldNC"),F.Get(TEXT("ConnectionLocalNet")));D.Object(TEXT("currentNC"),NC);D.Add(TEXT("localNativeKey"),NativeLocal);D.Add(TEXT("hostNativeKey"),NativeHost);D.Add(TEXT("liveClientExchange"),F.Get(TEXT("ConnectionClientExchange")));D.Add(TEXT("hostWorldEpoch"),F.Get(TEXT("ConnectionEpoch")));
        D.Object(TEXT("boundSubsystem"),F.Get(TEXT("ConnectionProbeBoundSubsystem")));D.Object(TEXT("freshSubsystem"),DORDeliverySubsystem(F));D.Object(TEXT("callbackObject"),SelfPin(F.G,0,0));D.Add(TEXT("callbackFunction"),F.Str(*(SC->GetPathName()+TEXT(":DOR_ConnectionNativeMessage"))));D.Bool(TEXT("bound"),F.Get(TEXT("ConnectionListenerBound")));D.Bool(TEXT("closed"),F.Get(TEXT("ConnectionClosed")));
        // Every production context condition is observed, without changing it.
        D.Bool(TEXT("gate-notClosed"),F.Not(F.Get(TEXT("ConnectionClosed"))));D.Bool(TEXT("gate-selected"),F.Same(SelfPin(F.G,0,0),Selected));D.Bool(TEXT("gate-GS-valid"),F.IsValid(GS));D.Bool(TEXT("gate-GS-same"),F.Same(GS,F.Get(TEXT("ConnectionGS"))));D.Bool(TEXT("gate-host-valid"),F.IsValid(Host));D.Bool(TEXT("gate-host-same"),F.Same(Host,F.Get(TEXT("ConnectionHost"))));D.Bool(TEXT("gate-host-listed"),HostListed);D.Bool(TEXT("gate-PC-valid"),F.IsValid(PC));D.Bool(TEXT("gate-PC-same"),F.Same(PC,F.Get(TEXT("ConnectionLocalController"))));D.Bool(TEXT("gate-PS-valid"),F.IsValid(PS));D.Bool(TEXT("gate-PS-same"),F.Same(PS,F.Get(TEXT("ConnectionLocalState"))));D.Bool(TEXT("gate-NC-same"),F.Same(NC,F.Get(TEXT("ConnectionLocalNet"))));
        auto* Identity=F.Cache(TEXT("Identity"),D.Line);F.QuietRequire(F.Or(F.Not(Dedup),F.Not(F.TextEq(Identity,F.Get(TEXT("ConnectionProbeLastIdentity"))))));F.Set(TEXT("ConnectionProbeLastIdentity"),Identity);D.Line=Identity;D.Add(TEXT("event"),Stage);D.Record();F.Done();F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeMessage"),MT);F.VoidResult=true;auto* Message=F.Input(TEXT("Message"),MakeStructPinType(FFSDChatMessage::StaticStruct()));auto* History=F.Input(TEXT("History"),BT);auto* B=F.Break(Message,FFSDChatMessage::StaticStruct());auto* Text=F.Cache(TEXT("Wire"),FindPin(B,TEXT("Msg")));
        auto* Relevant=F.Cache(TEXT("Relevant"),F.Or(DORDeliveryStarts(F,Text,TEXT("[DOR-HS")),DORDeliveryStarts(F,Text,TEXT("!dor probe "))));auto* Keep=F.Branch(F.Or(F.Not(History),Relevant));
        FDORDeliveryLine D(F,TEXT("native-message"));D.Bool(TEXT("history"),History);D.Int(TEXT("sequence"),F.Get(TEXT("ConnectionProbeSequence")));D.Object(TEXT("self"),SelfPin(F.G,0,0));D.Object(TEXT("selected"),F.Selected(SC));D.Object(TEXT("boundSubsystem"),F.Get(TEXT("ConnectionProbeBoundSubsystem")));D.Object(TEXT("freshSubsystem"),DORDeliverySubsystem(F));D.Bool(TEXT("bound"),F.Get(TEXT("ConnectionListenerBound")));D.Bool(TEXT("closed"),F.Get(TEXT("ConnectionClosed")));
        D.Int(TEXT("length"),F.Len(Text));D.Add(TEXT("kind"),F.Text(FindPin(B,TEXT("MsgType")),TEXT("Conv_ByteToString")));D.Add(TEXT("SenderType"),F.Text(FindPin(B,TEXT("SenderType")),TEXT("Conv_ByteToString")));auto* Sender=F.Call(MC,TEXT("MessageSenderIdAsString"));Connect(Message,FindPin(Sender,TEXT("Msg")));D.Add(TEXT("nativeSender"),Sender->GetReturnValuePin());D.Bool(TEXT("HS3"),DORDeliveryStarts(F,Text,DORHandshakeEnvelopePrefix(TEXT(""))));D.Bool(TEXT("HELLO"),DORDeliveryStarts(F,Text,DORHandshakeEnvelopePrefix(TEXT("HELLO"))));D.Bool(TEXT("exactHELLOVersion"),DORDeliveryStarts(F,Text,DORHandshakeIdentityPrefix(TEXT(""),TEXT("HELLO"))));
        auto* Wire=F.Cache(TEXT("BoundedWire"),DORDeliverySafePrefix(F,Text));auto* Full=F.Branch(Relevant);F.Set(F.Variable(Wire),DORDeliveryClip(F,Text,1024));F.Merge(Full->GetElsePin());D.Add(TEXT("wire"),Wire);
        auto* Line=F.Cache(TEXT("MessageLine"),D.Line);auto* H=F.Branch(History);auto* Age=F.Call(MC,TEXT("MessageAge"));Connect(Message,FindPin(Age,TEXT("Msg")));D.Line=Line;D.Float(TEXT("localMessageAge"),Age->GetReturnValuePin());F.Set(F.Variable(Line),D.Line);F.Merge(H->GetElsePin());D.Line=Line;D.Record();F.Merge(Keep->GetElsePin());F.Done();F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeIngress"),MT);F.VoidResult=true;auto* Message=F.Input(TEXT("Message"),MakeStructPinType(FFSDChatMessage::StaticStruct()));auto* B=F.Break(Message,FFSDChatMessage::StaticStruct());auto* Text=FindPin(B,TEXT("Msg"));F.Set(TEXT("ConnectionProbeSequence"),F.Math(TEXT("Add_IntInt"),F.Get(TEXT("ConnectionProbeSequence")),F.Int(1)));
        // Exact commands reopen a finite observation window on each listener;
        // their read/send actions remain separately native-authorized below.
        auto* Command=F.Branch(F.Or(F.TextEq(Text,F.Str(TEXT("!dor probe snapshot"))),F.TextEq(Text,F.Str(TEXT("!dor probe hello")))));auto* Arm=F.Self(TEXT("DOR_ConnectionProbeWindow"));FindPin(Arm,TEXT("Seconds"))->DefaultValue=TEXT("20.0");F.Merge(Command->GetElsePin());
        auto* Active=F.Branch(F.Self(TEXT("DOR_ConnectionProbeActive"))->GetReturnValuePin());F.Set(TEXT("ConnectionProbeEntries"),F.Math(TEXT("Add_IntInt"),F.Get(TEXT("ConnectionProbeEntries")),F.Int(1)));auto* Log=F.Self(TEXT("DOR_ConnectionProbeMessage"));Connect(Message,FindPin(Log,TEXT("Message")));FindPin(Log,TEXT("History"))->DefaultValue=TEXT("false");F.Done();F.Tail=Active->GetElsePin();auto* Once=F.Branch(F.Not(F.Get(TEXT("ConnectionProbeExhausted"))));F.Set(TEXT("ConnectionProbeExhausted"),nullptr,TEXT("true"));F.Set(TEXT("ConnectionProbeArmed"),nullptr,TEXT("false"));FDORDeliveryLine D(F,TEXT("window-exhausted"));D.Int(TEXT("entries"),F.Get(TEXT("ConnectionProbeEntries")));D.Float(TEXT("until"),F.Get(TEXT("ConnectionProbeUntil")));D.Record();F.Merge(Once->GetElsePin());F.Done();F.Compile();
    }
    // Snapshot authorization is local native identity only; it remains useful
    // when production context selection is false. No history reaches admission.
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeSnapshot"),MT);F.VoidResult=true;DORDeliveryIdentity(F,TEXT("snapshot"));auto* Sub=DORDeliverySubsystem(F);F.QuietRequire(F.IsValid(Sub));auto* Read=F.Call(MC,TEXT("LatestMessages"));Connect(Sub,FindPin(Read,UEdGraphSchema_K2::PN_Self));FindPin(Read,TEXT("Num"))->DefaultValue=TEXT("50");FindPin(Read,TEXT("resetAge"))->DefaultValue=TEXT("false");auto* Messages=F.Cache(TEXT("HistoryMessages"),FindPin(Read,TEXT("OutMessages")));FDORDeliveryLine D(F,TEXT("history-read"));D.Int(TEXT("total"),F.Count(Messages));D.Object(TEXT("subsystem"),Sub);D.Add(TEXT("scope"),F.Str(TEXT("GI-ring-may-contain-other-worlds;resetAge=false;read-only;capacity=50")));D.Record();auto* Loop=F.Each(Messages);auto* Log=F.Self(TEXT("DOR_ConnectionProbeMessage"));Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Log,TEXT("Message")));FindPin(Log,TEXT("History"))->DefaultValue=TEXT("true");F.Tail=FindPin(Loop,TEXT("Completed"));F.Done();F.Compile();
    }
    return Safety->Status!=BS_Error;
}

bool BuildDORDeliverySendProbe(UBlueprint* Safety,UClass* MT)
{
    const auto ST=MakeStringPinType(),IT=MakeIntPinType();
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeRowReason"),MT);F.ResultType=ST;auto* I=F.Input(TEXT("Index"),IT);
        auto Gate=[&](UEdGraphPin* C,const TCHAR* Reason){auto* B=F.Branch(C);F.Tail=B->GetElsePin();F.Return(F.Str(Reason));F.Tail=B->GetThenPin();};
        Gate(F.Self(TEXT("DOR_ConnectionContextValid"))->GetReturnValuePin(),TEXT("context-invalid-see-identity-gates"));Gate(DORLocalHost(F),TEXT("local-not-host"));auto* Bounds=F.Array(TEXT("Array_IsValidIndex"),F.Get(TEXT("ConnectionStates")));Connect(I,FindPin(Bounds,TEXT("IndexToTest")));Gate(Bounds->GetReturnValuePin(),TEXT("row-index-invalid"));
        auto* PS=F.Cache(TEXT("PS"),F.At(F.Get(TEXT("ConnectionStates")),I));Gate(F.IsValid(PS),TEXT("PS-invalid"));Gate(F.Contains(F.Field(F.Get(TEXT("ConnectionGS")),AGameStateBase::StaticClass(),TEXT("PlayerArray")),PS),TEXT("PS-not-in-current-roster"));auto* PC=F.Cache(TEXT("CurrentPC"),F.Cast(F.Read(AActor::StaticClass(),TEXT("GetOwner"),PS),AFSDPlayerController::StaticClass()));Gate(F.IsValid(PC),TEXT("PS-owner-PC-invalid"));Gate(F.Read(AActor::StaticClass(),TEXT("HasAuthority"),PC),TEXT("PC-not-authority"));Gate(F.Not(F.Read(AController::StaticClass(),TEXT("IsLocalPlayerController"),PC)),TEXT("PC-is-local"));Gate(F.Same(F.Field(PC,AController::StaticClass(),TEXT("PlayerState")),PS),TEXT("PC-PlayerState-mismatch"));auto* Net=F.Self(TEXT("DOR_ConnectionNativeNet"));Connect(PC,FindPin(Net,TEXT("Controller")));auto* NC=F.Cache(TEXT("CurrentNC"),Net->GetReturnValuePin());Gate(F.IsValid(NC),TEXT("native-NC-invalid"));auto* Key=F.NativeKey(PS);Gate(DORPlayerKeyValid(F,Key),TEXT("native-key-invalid"));Gate(F.Same(PC,F.At(F.Get(TEXT("ConnectionControllers")),I)),TEXT("PC-replaced"));Gate(F.Same(NC,F.At(F.Get(TEXT("ConnectionNets")),I)),TEXT("NC-replaced"));Gate(F.TextEq(Key,F.At(F.Get(TEXT("ConnectionKeys")),I)),TEXT("key-replaced"));auto* Accepted=F.Branch(F.At(F.Get(TEXT("ConnectionAccepted")),I));auto* E=F.Cache(TEXT("Endpoint"),F.At(F.Get(TEXT("ConnectionEndpoints")),I));Gate(F.IsValid(E),TEXT("accepted-endpoint-invalid"));Gate(F.Same(F.Read(AActor::StaticClass(),TEXT("GetOwner"),E),PC),TEXT("accepted-endpoint-owner-mismatch"));F.Merge(Accepted->GetElsePin());auto* Count=F.Self(TEXT("DOR_ConnectionNativeCount"));Connect(Key,FindPin(Count,TEXT("Key")));Gate(F.Eq(Count->GetReturnValuePin(),F.Int(1)),TEXT("native-key-not-unique"));F.Return(F.Str(TEXT("all-row-gates-valid;explicit-reset-or-endpoint-ended")));F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeSend"),MT);F.VoidResult=true;auto* I=F.Input(TEXT("Index"),IT);auto* Manual=F.Input(TEXT("Manual"),MakeBoolPinType());F.QuietRequire(F.Or(Manual,F.Self(TEXT("DOR_ConnectionProbeActive"))->GetReturnValuePin()));auto* PC=F.Cache(TEXT("PC"),F.At(F.Get(TEXT("ConnectionControllers")),I));auto* NC=F.Cache(TEXT("NC"),F.At(F.Get(TEXT("ConnectionNets")),I));
        FDORDeliveryLine D(F,TEXT("native-Modder-invocation-observation"));D.Bool(TEXT("manual"),Manual);D.Add(TEXT("nativeAPI"),F.Str(TEXT("AFSDPlayerController.Server_NewMessage(SenderType=Modder)")));D.Add(TEXT("exchange"),F.At(F.Get(TEXT("ConnectionExchanges")),I));D.Add(TEXT("wire"),F.Join(F.Str(*DORHandshakeIdentityPrefix(TEXT(""),TEXT("HELLO"))),F.At(F.Get(TEXT("ConnectionExchanges")),I)));D.Int(TEXT("discoveryAge"),F.At(F.Get(TEXT("ConnectionAges")),I));D.Int(TEXT("automaticSendIndex"),F.At(F.Get(TEXT("ConnectionSends")),I));D.Object(TEXT("PC"),PC);D.Object(TEXT("PS"),F.At(F.Get(TEXT("ConnectionStates")),I));D.Object(TEXT("NetConnection"),NC);auto* View=F.Cache(TEXT("ViewTarget"),nullptr,TEXT(""),MakeObjectPinType(AActor::StaticClass()));auto* Travel=F.Cache(TEXT("ServerTravelDone"),F.Eq(F.Int(0),F.Int(1)));auto* HaveNC=F.Branch(F.IsValid(NC));F.Set(F.Variable(View),F.Field(NC,UNetConnection::StaticClass(),TEXT("ViewTarget")));F.Merge(HaveNC->GetElsePin());auto* HavePC=F.Branch(F.IsValid(PC));F.Set(F.Variable(Travel),F.Field(PC,AFSDPlayerController::StaticClass(),TEXT("ServerTravelDone")));F.Merge(HavePC->GetElsePin());D.Bool(TEXT("ViewTargetValid"),F.IsValid(View));D.Object(TEXT("ViewTarget"),View);D.Bool(TEXT("observedServerTravelDone"),Travel);D.Add(TEXT("phase"),F.Str(TEXT("HELLO;native-call-returned-not-delivery-proof")));D.Record();DORDeliveryIdentity(F,TEXT("send-identities"));F.Done();F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeHelloRow"),MT);F.VoidResult=true;auto* I=F.Input(TEXT("Index"),IT);auto* X=F.Cache(TEXT("Exchange"),F.At(F.Get(TEXT("ConnectionExchanges")),I));
        auto Select=[&](UEdGraphPin* C,const TCHAR* Reason){auto* B=F.Branch(C);F.Tail=B->GetElsePin();FDORDeliveryLine D(F,TEXT("manual-tuple-skipped"));D.Int(TEXT("index"),I);D.Add(TEXT("exchange"),X);D.Add(TEXT("reason"),F.Str(Reason));D.Record();F.Return();F.Tail=B->GetThenPin();};
        auto* Valid=F.Self(TEXT("DOR_ConnectionRowValid"));Connect(I,FindPin(Valid,TEXT("Index")));Select(Valid->GetReturnValuePin(),TEXT("current-row-invalid"));Select(F.Not(F.At(F.Get(TEXT("ConnectionAccepted")),I)),TEXT("already-compatible"));Select(F.Not(F.TextEq(X,F.Str(TEXT("")))),TEXT("no-live-exchange"));Select(F.Not(F.Contains(F.Get(TEXT("ConnectionProbeManualExchanges")),X)),TEXT("one-shot-already-used"));
        F.Append(F.Get(TEXT("ConnectionProbeManualExchanges")),X);FDORDeliveryLine D(F,TEXT("manual-tuple-selected"));D.Int(TEXT("index"),I);D.Add(TEXT("exchange"),X);D.Add(TEXT("wire"),F.Join(F.Str(*DORHandshakeIdentityPrefix(TEXT(""),TEXT("HELLO"))),X));D.Record();
        auto* Send=F.Call(AFSDPlayerController::StaticClass(),TEXT("Server_NewMessage"));Connect(F.Get(TEXT("ConnectionLocalController")),FindPin(Send,UEdGraphSchema_K2::PN_Self));Connect(F.Read(APlayerState::StaticClass(),TEXT("GetPlayerName"),F.Get(TEXT("ConnectionLocalState"))),FindPin(Send,TEXT("Sender")));FindPin(Send,TEXT("SenderType"))->DefaultValue=TEXT("Modder");Connect(F.Join(F.Str(*DORHandshakeIdentityPrefix(TEXT(""),TEXT("HELLO"))),X),FindPin(Send,TEXT("Text")));auto* Observe=F.Self(TEXT("DOR_ConnectionProbeSend"));Connect(I,FindPin(Observe,TEXT("Index")));FindPin(Observe,TEXT("Manual"))->DefaultValue=TEXT("true");F.Done();F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionProbeHello"),MT);F.VoidResult=true;auto* Sender=F.Input(TEXT("SenderKey"),ST);
        auto Reject=[&](UEdGraphPin* C,const TCHAR* Reason){auto* B=F.Branch(C);F.Tail=B->GetElsePin();FDORDeliveryLine D(F,TEXT("manual-hello-rejected"));D.Add(TEXT("reason"),F.Str(Reason));D.Add(TEXT("nativeSender"),Sender);D.Record();F.Return();F.Tail=B->GetThenPin();};
        Reject(F.Self(TEXT("DOR_ConnectionContextValid"))->GetReturnValuePin(),TEXT("selected-current-context"));auto* PC=F.LocalPC();Reject(F.IsValid(PC),TEXT("local-PC-invalid"));Reject(F.Read(AActor::StaticClass(),TEXT("HasAuthority"),PC),TEXT("local-PC-not-authority"));auto* HostKey=F.NativeKey(F.Get(TEXT("ConnectionHost")));Reject(F.And(DORPlayerKeyValid(F,HostKey),F.TextEq(Sender,HostKey)),TEXT("native-sender-not-current-host"));
        FDORDeliveryLine Receipt(F,TEXT("manual-hello-command"));Receipt.Add(TEXT("nativeSender"),Sender);Receipt.Int(TEXT("rows"),F.Count(F.Get(TEXT("ConnectionStates"))));Receipt.Record();
        auto* Loop=F.Each(F.Get(TEXT("ConnectionStates")));auto* I=FindPin(Loop,TEXT("Array Index"));
        // Loop-body rejection returns from a tiny per-row function, below.
        auto* Row=F.Self(TEXT("DOR_ConnectionProbeHelloRow"));Connect(I,FindPin(Row,TEXT("Index")));F.Tail=FindPin(Loop,TEXT("Completed"));FDORDeliveryLine Done(F,TEXT("manual-hello-command-complete"));Done.Record();F.Done();F.Compile();
    }
    return Safety->Status!=BS_Error;
}
