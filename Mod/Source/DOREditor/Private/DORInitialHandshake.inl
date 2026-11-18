// Initial scheduling for each new/replaced tuple, including late join/reconnect.
// This native load observation is never an admission,
// compatibility, endpoint-operation, reward or save readiness predicate.
bool BuildDORInitialHandshake(UBlueprint* Safety,UClass* MT)
{
    const auto IT=MakeIntPinType(),ST=MakeStringPinType();
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionInitialReadyReason"),MT);F.ResultType=ST;auto* I=F.Input(TEXT("Index"),IT);
        auto Need=[&](UEdGraphPin* C,const TCHAR* Reason){auto* B=F.Branch(C);B->NodeComment=Reason;F.Tail=B->GetElsePin();F.Return(F.Str(Reason));F.Tail=B->GetThenPin();};
        auto* PC=F.Cache(TEXT("PC"),F.At(F.Get(TEXT("ConnectionControllers")),I));auto* PS=F.Cache(TEXT("PS"),F.At(F.Get(TEXT("ConnectionStates")),I));auto* NC=F.Cache(TEXT("NC"),F.At(F.Get(TEXT("ConnectionNets")),I));
        Need(F.IsValid(PC),TEXT("controller-missing"));auto* Pawn=F.Cache(TEXT("CurrentPawn"),F.Cast(F.Read(AController::StaticClass(),TEXT("K2_GetPawn"),PC),APlayerCharacter::StaticClass()));Need(F.IsValid(Pawn),TEXT("native-pawn-missing"));
        Need(F.Field(Pawn,APlayerCharacter::StaticClass(),TEXT("ClientReady")),TEXT("native-client-ready-false"));
        auto* PawnPC=F.Cache(TEXT("PawnPC"),F.Read(APlayerCharacter::StaticClass(),TEXT("GetPlayerController"),Pawn));Need(F.Same(PawnPC,PC),TEXT("pawn-controller-mismatch"));
        auto* PawnPS=F.Cache(TEXT("PawnPS"),F.Read(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"),Pawn));Need(F.Same(PawnPS,PS),TEXT("pawn-player-state-mismatch"));
        // Same native post-arrival condition as OrdinaryArrivalReady, without
        // that consumer's compatibility/replay prerequisites. User accepts
        // waiting until the support-pod Attached state has ended.
        auto* State=F.Cache(TEXT("ArrivalState"),F.Read(APlayerCharacter::StaticClass(),TEXT("GetCurrentState"),Pawn));auto* Attached=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ByteByte"));Connect(State,FindPin(Attached,TEXT("A")));FindPin(Attached,TEXT("B"))->DefaultValue=TEXT("14");Need(F.Not(Attached->GetReturnValuePin()),TEXT("native-attached-arrival-pending"));
        Need(F.IsValid(NC),TEXT("native-connection-missing"));auto* View=F.Cache(TEXT("ViewTarget"),F.Field(NC,UNetConnection::StaticClass(),TEXT("ViewTarget")));Need(F.IsValid(View),TEXT("native-view-target-missing"));
        // A valid cinematic/spectator camera is legitimate; no ViewTarget==Pawn
        // condition or gameplay-class whitelist is used here.
        F.Return(F.Str(TEXT("ready")));F.Compile();
    }
    {
        FDORConnectionGraph F(Safety,TEXT("DOR_ConnectionTryInitialStart"),MT);auto* I=F.Input(TEXT("Index"),IT);auto* Valid=F.Self(TEXT("DOR_ConnectionRowValid"));Connect(I,FindPin(Valid,TEXT("Index")));F.QuietRequire(Valid->GetReturnValuePin());
        // Never read Pawn again for an already-started/accepted connection.
        auto* Started=F.Branch(F.Or(F.At(F.Get(TEXT("ConnectionInitialStarted")),I),F.At(F.Get(TEXT("ConnectionAccepted")),I)));Started->NodeComment=TEXT("DOR163-STARTED-SKIPS-NATIVE-READINESS");F.Return(nullptr,TEXT("true"));F.Tail=Started->GetElsePin();
        auto* Read=F.Self(TEXT("DOR_ConnectionInitialReadyReason"));Connect(I,FindPin(Read,TEXT("Index")));auto* Reason=F.Cache(TEXT("PendingReason"),Read->GetReturnValuePin());auto* Ready=F.Branch(F.TextEq(Reason,F.Str(TEXT("ready"))));Ready->NodeComment=TEXT("DOR163-NATIVE-READY-INITIAL-START");
        F.RowSet(TEXT("ConnectionInitialStarted"),I,F.Eq(F.Int(0),F.Int(0)));F.RowSet(TEXT("ConnectionInitialReasons"),I,Reason);for(const TCHAR* N:{TEXT("ConnectionAges"),TEXT("ConnectionSends")})F.RowSet(N,I,F.Int(0));
        F.Stage(TEXT("initial-start-native-ready"),F.At(F.Get(TEXT("ConnectionKeys")),I),F.At(F.Get(TEXT("ConnectionExchanges")),I),nullptr,F.At(F.Get(TEXT("ConnectionControllers")),I),F.At(F.Get(TEXT("ConnectionStates")),I),F.At(F.Get(TEXT("ConnectionNets")),I));F.Return(nullptr,TEXT("true"));
        F.Tail=Ready->GetElsePin();auto* Changed=F.Branch(F.Not(F.TextEq(Reason,F.At(F.Get(TEXT("ConnectionInitialReasons")),I))));F.RowSet(TEXT("ConnectionInitialReasons"),I,Reason);
        auto* Line=F.Join(F.Str(TEXT("[DOR-CONNECTION]|stage=initial-pending|reason=")),Reason);Line=F.Join(F.Join(Line,F.Str(TEXT("|key="))),F.At(F.Get(TEXT("ConnectionKeys")),I));Line=F.Join(F.Join(Line,F.Str(TEXT("|exchange="))),F.At(F.Get(TEXT("ConnectionExchanges")),I));DORNormalReceipt(F,Line,true);F.Merge(Changed->GetElsePin());F.Return();F.Compile();
    }
    return Safety->Status!=BS_Error;
}
