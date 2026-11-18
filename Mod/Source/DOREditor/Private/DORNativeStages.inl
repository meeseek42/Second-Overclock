// Shared local phases. These do not wait for RPCs, relinquish admission or
// infer terminal settlement from the provisional fields-restored receipt.
bool BuildDORNativeStages(UBlueprint* Master,UBlueprint* Request,UBlueprint* Safety)
{
    auto* JournalClass=FindFProperty<FObjectProperty>(Safety->GeneratedClass,TEXT("DOR_Journal"))->PropertyClass;if(!JournalClass)return false;
    const auto RequestType=MakeObjectPinType(Request->GeneratedClass);FEdGraphPinType Float;Float.PinCategory=UEdGraphSchema_K2::PC_Float;
    for(const TCHAR* N:{TEXT("NoBankMutationCertificate"),TEXT("CancelRequested"),TEXT("StopIssued")})AddTypedVariable(Request,N,MakeBoolPinType());FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Request);FKismetEditorUtilities::CompileBlueprint(Request);
    auto Lease=[&](FDORNativeGraph& F,bool RequireActive=true)
    {
        auto* R=F.Input(TEXT("RequestObject"),RequestType);F.Require(F.IsValid(R));F.Require(F.Get(TEXT("NativeExecutionHeld")));F.Require(F.Same(R,F.Get(TEXT("NativeActiveRequest"))));
        auto* S=F.Cache(TEXT("Safety"),F.Field(R,Request->GeneratedClass,TEXT("LocalSafety")));F.Require(F.IsValid(S));auto* J=F.Cache(TEXT("Journal"),F.Field(S,Safety->GeneratedClass,TEXT("DOR_Journal")));F.Require(F.IsValid(J));auto* Token=F.Cache(TEXT("Token"),F.Field(R,Request->GeneratedClass,TEXT("LocalJournalId")));F.Require(F.Not(F.Eq(Token,F.Str(TEXT("")),TEXT("EqualEqual_StrStr"))));F.Require(F.Eq(Token,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr")));if(RequireActive)F.Require(F.Field(S,Safety->GeneratedClass,TEXT("DOR_Active")));return TArray<UEdGraphPin*>{R,S,J,Token};
    };
    auto SafetyCall=[&](FDORNativeGraph& F,UEdGraphPin* S,UEdGraphPin* Token,const TCHAR* Name){auto* N=F.Call(Safety->GeneratedClass,Name);Connect(S,FindPin(N,UEdGraphSchema_K2::PN_Self));Connect(Token,FindPin(N,TEXT("TransactionId")));return N;};
    auto Tuple=[&](FDORNativeGraph& F,UK2Node_CallFunction* N,UEdGraphPin* R){for(const auto& P:{TPair<const TCHAR*,const TCHAR*>(TEXT("Request"),TEXT("Request")),TPair<const TCHAR*,const TCHAR*>(TEXT("Epoch"),TEXT("Epoch")),TPair<const TCHAR*,const TCHAR*>(TEXT("Generation"),TEXT("Generation")),TPair<const TCHAR*,const TCHAR*>(TEXT("Slot"),TEXT("Slot")),TPair<const TCHAR*,const TCHAR*>(TEXT("PlayerKey"),TEXT("Key"))})Connect(F.Field(R,Request->GeneratedClass,P.Value),FindPin(N,P.Key));};
    {
        // A failed bank acquisition is not a bank restore. This certificate
        // is usable only before any qualification/source/native effect.
        FDORNativeGraph F(Master,TEXT("DOR_NativeCheckNoBankMutation"),Master->GeneratedClass);const auto L=Lease(F);auto* R=L[0];auto* J=L[2];
        F.Require(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("LocalLeaseStep")),F.Int(1)));for(const TCHAR* N:{TEXT("LocalBankCaptured"),TEXT("NativeSubmitted")})F.Require(F.Not(F.Field(R,Request->GeneratedClass,N)));
        for(const TCHAR* N:{TEXT("NativeScopeActive"),TEXT("RuntimeBankActive")})F.Require(F.Not(F.Get(N)));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("Schema")),F.Int(3)));F.Require(F.Field(J,JournalClass,TEXT("SourceEquipment")));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),F.Int(0)));
        for(const TCHAR* N:{TEXT("QualificationScopePrepared"),TEXT("EquipmentStagingIntent"),TEXT("EquipmentEverStaged")})F.Require(F.Not(F.Field(J,JournalClass,N)));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("NativeRequest")),F.Field(R,Request->GeneratedClass,TEXT("Request")),TEXT("EqualEqual_GuidGuid")));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("NativeEpoch")),F.Field(R,Request->GeneratedClass,TEXT("Epoch")),TEXT("EqualEqual_GuidGuid")));
        for(const auto& P:{TPair<const TCHAR*,const TCHAR*>(TEXT("NativeGeneration"),TEXT("Generation")),TPair<const TCHAR*,const TCHAR*>(TEXT("NativeSlot"),TEXT("Slot"))})F.Require(F.Eq(F.Field(J,JournalClass,P.Key),F.Field(R,Request->GeneratedClass,P.Value)));F.Require(F.Eq(F.Field(J,JournalClass,TEXT("NativePlayerKey")),F.Field(R,Request->GeneratedClass,TEXT("Key")),TEXT("EqualEqual_StrStr")));
        F.Put(R,Request->GeneratedClass,TEXT("NoBankMutationCertificate"),nullptr,TEXT("true"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeRestoreLocalFields"),Master->GeneratedClass);const auto L=Lease(F);auto* R=L[0];auto* S=L[1];auto* Token=L[3];
        F.Require(F.Eq(Token,F.Field(S,Safety->GeneratedClass,TEXT("DOR_CheckedEquipmentRestoreTransactionId")),TEXT("EqualEqual_StrStr")),TEXT("[DOR-NATIVE]|stage=fields-restore-rejected|condition=equipment-source-not-checked|barrier=held"));
        auto* BankOK=F.Cache(TEXT("BankOK"),F.Eq(F.Int(0),F.Int(1)));
        auto* Equipment=F.Branch(F.Field(R,Request->GeneratedClass,TEXT("LocalEquipmentOnly")));
        auto* Only=SafetyCall(F,S,Token,TEXT("DOR_SourceEquipmentOnly"));
        F.Set(F.Variable(BankOK),F.And(Only->GetReturnValuePin(),
            F.And(F.Get(TEXT("NativeScopeEquipmentOnly")),
                F.Eq(Token,F.Get(TEXT("NativeScopeJournalId")),TEXT("EqualEqual_StrStr")))));
        auto* EquipmentChecked=F.Tail; F.Tail=Equipment->GetElsePin();
        auto* Acquired=F.Branch(F.Field(R,Request->GeneratedClass,TEXT("LocalBankCaptured")));
        auto* Bank=F.Self(TEXT("DOR_NativeBankRestore"));
        Connect(F.Field(R,Request->GeneratedClass,TEXT("Request")),FindPin(Bank,TEXT("RequestId")));
        F.Set(F.Variable(BankOK),F.And(Bank->GetReturnValuePin(),F.And(F.Get(TEXT("NativeScopeRestored")),F.Not(F.Get(TEXT("NativeScopeActive"))))));
        auto* Restored=F.Tail; F.Tail=Acquired->GetElsePin();
        auto* None=F.Self(TEXT("DOR_NativeCheckNoBankMutation")); Connect(R,FindPin(None,TEXT("RequestObject")));
        F.Set(F.Variable(BankOK),None->GetReturnValuePin()); F.Merge(Restored); F.Merge(EquipmentChecked);
        // A bank conflict must not prolong the independent equipment/account
        // window. Their real receipt survives, but the request remains held.
        auto* Restore=SafetyCall(F,S,Token,TEXT("DOR_SourceRestoreFields"));F.Require(Restore->GetReturnValuePin());F.Require(F.Eq(Token,F.Field(S,Safety->GeneratedClass,TEXT("DOR_CheckedFieldsRestoreTransactionId")),TEXT("EqualEqual_StrStr")));F.Require(F.Eq(F.Field(F.Field(S,Safety->GeneratedClass,TEXT("DOR_Journal")),JournalClass,TEXT("NativeProtocolPhase")),F.Int(3)));F.Require(BankOK,TEXT("[DOR-NATIVE]|stage=fields-restored-bank-unresolved|journal=retained|barrier=held"));
        auto* Close=F.Branch(F.Field(R,Request->GeneratedClass,TEXT("LocalEquipmentOnly")));
        F.Set(TEXT("NativeScopeRestored"),nullptr,TEXT("true"));
        F.Set(TEXT("NativeScopeActive"),nullptr,TEXT("false"));
        F.Merge(Close->GetElsePin());
        F.Put(R,Request->GeneratedClass,TEXT("LocalLeaseStep"),nullptr,TEXT("3"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeBeginLocalRecovery"),Master->GeneratedClass);const auto L=Lease(F);auto* R=L[0];auto* S=L[1];auto* Token=L[3];F.Require(F.Not(F.Field(R,Request->GeneratedClass,TEXT("TerminalLatched"))));F.Require(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("LocalLeaseStep")),F.Int(3)));F.Require(F.Field(R,Request->GeneratedClass,TEXT("OriginalCaptured")));auto* InTime=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_FloatFloat"));Connect(F.RealNow(),FindPin(InTime,TEXT("A")));Connect(F.Field(R,Request->GeneratedClass,TEXT("RecoveryDeadline")),FindPin(InTime,TEXT("B")));F.Require(InTime->GetReturnValuePin());
        F.Require(F.Field(R,Request->GeneratedClass,TEXT("LocalEquipmentOnly")));
        F.Require(F.Get(TEXT("NativeScopeRestored")));
        F.Require(F.Not(F.Get(TEXT("NativeScopeActive"))));
        F.Require(F.Eq(Token,F.Get(TEXT("NativeScopeJournalId")),TEXT("EqualEqual_StrStr")));
        auto* OriginalRoots=F.Field(R,Request->GeneratedClass,TEXT("OriginalRequiredRoots"));
        auto* Registered=F.Self(TEXT("DOR_NativeRequireRegistered"));
        Connect(S,FindPin(Registered,TEXT("Safety"))); Connect(OriginalRoots,FindPin(Registered,TEXT("RequiredRoots")));
        F.Require(Registered->GetReturnValuePin());
        auto* Rebind=SafetyCall(F,S,Token,TEXT("DOR_SourceRebindProjection"));
        Connect(F.Field(R,Request->GeneratedClass,TEXT("OriginalLoadout")),FindPin(Rebind,TEXT("Loadout")));
        Connect(F.Field(R,Request->GeneratedClass,TEXT("OriginalSelections")),FindPin(Rebind,TEXT("Selections")));
        F.Require(Rebind->GetReturnValuePin());
        auto* Only=SafetyCall(F,S,Token,TEXT("DOR_SourceEquipmentOnly"));
        FindPin(Only,TEXT("Reopen"))->DefaultValue=TEXT("true");
        F.Require(Only->GetReturnValuePin());
        F.Set(TEXT("NativeScopeRequired"),OriginalRoots);
        F.Set(TEXT("NativeScopeRecovery"),nullptr,TEXT("true"));
        F.Set(TEXT("NativeScopeRestored"),nullptr,TEXT("false"));
        F.Set(TEXT("NativeScopeActive"),nullptr,TEXT("true"));
        F.Put(R,Request->GeneratedClass,TEXT("LocalLeaseStep"),nullptr,TEXT("2"));
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeFinalizeLocal"),Master->GeneratedClass);const auto L=Lease(F,false);auto* R=L[0];auto* S=L[1];auto* Token=L[3];F.Require(F.Field(R,Request->GeneratedClass,TEXT("TerminalLatched")));auto* Active=F.Branch(F.Field(S,Safety->GeneratedClass,TEXT("DOR_Active")));F.Tail=Active->GetElsePin();F.Return(F.And(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("LocalLeaseStep")),F.Int(4)),F.Eq(Token,F.Field(S,Safety->GeneratedClass,TEXT("DOR_CheckedNativeFinalizeTransactionId")),TEXT("EqualEqual_StrStr"))));F.Tail=Active->GetThenPin();F.Require(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("LocalLeaseStep")),F.Int(3)));
        auto* Equipment=F.Branch(F.Field(R,Request->GeneratedClass,TEXT("LocalEquipmentOnly")));
        F.Require(F.Get(TEXT("NativeScopeEquipmentOnly")));
        F.Require(F.Get(TEXT("NativeScopeRestored")));
        F.Require(F.Not(F.Get(TEXT("NativeScopeActive"))));
        F.Require(F.Not(F.Get(TEXT("RuntimeBankActive"))));
        auto* Only=SafetyCall(F,S,Token,TEXT("DOR_SourceEquipmentOnly")); F.Require(Only->GetReturnValuePin());
        auto* EquipmentClosed=F.Tail; F.Tail=Equipment->GetElsePin();
        auto* Acquired=F.Branch(F.Field(R,Request->GeneratedClass,TEXT("LocalBankCaptured")));F.Require(F.Get(TEXT("NativeScopeRestored")));F.Require(F.Not(F.Get(TEXT("NativeScopeActive"))));auto* Restored=F.Tail;F.Tail=Acquired->GetElsePin();F.Require(F.Field(R,Request->GeneratedClass,TEXT("NoBankMutationCertificate")));F.Require(F.Not(F.Field(R,Request->GeneratedClass,TEXT("NativeSubmitted"))));F.Require(F.Not(F.Get(TEXT("NativeScopeActive"))));F.Require(F.Not(F.Get(TEXT("RuntimeBankActive"))));F.Merge(Restored);F.Merge(EquipmentClosed);auto* Accept=SafetyCall(F,S,Token,TEXT("DOR_SourceAcceptTerminal"));Tuple(F,Accept,R);Connect(F.Field(R,Request->GeneratedClass,TEXT("TerminalSuccess")),FindPin(Accept,TEXT("Success")));F.Require(Accept->GetReturnValuePin());auto* Final=SafetyCall(F,S,Token,TEXT("DOR_SourceFinalize"));Tuple(F,Final,R);F.Require(Final->GetReturnValuePin());F.Require(F.Eq(Token,F.Field(S,Safety->GeneratedClass,TEXT("DOR_CheckedNativeFinalizeTransactionId")),TEXT("EqualEqual_StrStr")));F.Require(F.Not(F.Field(S,Safety->GeneratedClass,TEXT("DOR_Active"))));F.Put(R,Request->GeneratedClass,TEXT("LocalLeaseStep"),nullptr,TEXT("4"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        // Context loss closes reachable local values, not native runtime state.
        // No input setter, inventory rebuild or global outcome is invented.
        FDORNativeGraph F(Master,TEXT("DOR_NativeCloseReachableLocal"),Master->GeneratedClass);const auto L=Lease(F,false);auto* R=L[0];auto* S=L[1];auto* J=L[2];F.Set(TEXT("NativeRecoveryConflict"),nullptr,TEXT("true"));auto* Terminal=F.Branch(F.Field(R,Request->GeneratedClass,TEXT("TerminalLatched")));auto* Final=F.Self(TEXT("DOR_NativeFinalizeLocal"));Connect(R,FindPin(Final,TEXT("RequestObject")));F.Return(Final->GetReturnValuePin());F.Tail=Terminal->GetElsePin();F.Put(R,Request->GeneratedClass,TEXT("CancelRequested"),nullptr,TEXT("true"));F.Require(F.Field(S,Safety->GeneratedClass,TEXT("DOR_Active")));auto* Source=F.Call(Safety->GeneratedClass,TEXT("DOR_SourceRestore"));Connect(S,FindPin(Source,UEdGraphSchema_K2::PN_Self));Connect(J,FindPin(Source,TEXT("Record")));F.Require(Source->GetReturnValuePin());auto* Fields=F.Self(TEXT("DOR_NativeRestoreLocalFields"));Connect(R,FindPin(Fields,TEXT("RequestObject")));F.Require(Fields->GetReturnValuePin());F.Return(nullptr,TEXT("true"));F.Compile();
    }
    // This pure metadata gate is also used by the driver before any durable
    // terminal write. A late cancellation cannot revoke an accepted outcome.
    {
        FDORNativeGraph F(Master,TEXT("DOR_NativeLatchOutcome"),Master->GeneratedClass);auto* R=F.Input(TEXT("RequestObject"),RequestType);auto* Success=F.Input(TEXT("Success"),MakeBoolPinType());auto* At=F.Input(TEXT("AtTime"),Float);auto* Live=F.Input(TEXT("LiveProofs"),MakeBoolPinType());F.Require(F.IsValid(R));F.Require(F.Get(TEXT("NativeExecutionHeld")));F.Require(F.Same(R,F.Get(TEXT("NativeActiveRequest"))));F.Require(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("Epoch")),F.Get(TEXT("NativeEpoch")),TEXT("EqualEqual_GuidGuid")));
        auto* Latched=F.Branch(F.Field(R,Request->GeneratedClass,TEXT("TerminalLatched")));F.Return(F.Eq(Success,F.Field(R,Request->GeneratedClass,TEXT("TerminalSuccess")),TEXT("EqualEqual_BoolBool")));F.Tail=Latched->GetElsePin();auto* AcceptSuccess=F.Branch(Success);for(const TCHAR* N:{TEXT("CancelRequested"),TEXT("StopIssued")})F.Require(F.Not(F.Field(R,Request->GeneratedClass,N)));F.Merge(AcceptSuccess->GetElsePin());F.Require(Live);for(const TCHAR* N:{TEXT("OwnerFieldsRestored"),TEXT("AuthorityFieldsRestored")})F.Require(F.Field(R,Request->GeneratedClass,N));auto* Deadline=F.Cache(TEXT("Deadline"),F.Field(R,Request->GeneratedClass,TEXT("QueuedDeadline")));auto* Recovery=F.Branch(F.Not(Success));F.Set(F.Variable(Deadline),F.Field(R,Request->GeneratedClass,TEXT("RecoveryDeadline")));F.Merge(Recovery->GetElsePin());auto* InTime=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_FloatFloat"));Connect(At,FindPin(InTime,TEXT("A")));Connect(Deadline,FindPin(InTime,TEXT("B")));F.Require(InTime->GetReturnValuePin());F.Put(R,Request->GeneratedClass,TEXT("TerminalSuccess"),Success);F.Put(R,Request->GeneratedClass,TEXT("TerminalLatched"),nullptr,TEXT("true"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    return Master->Status!=BS_Error;
}

bool ValidateDORNativeStages(UBlueprint* Master,UBlueprint* Request,UBlueprint* Safety)
{
    FEditorScriptExecutionGuard Guard;UWorld* World=UWorld::CreateWorld(EWorldType::EditorPreview,false);if(!World)return false;auto* M=World->SpawnActor(Master->GeneratedClass);auto* S=World->SpawnActor(Safety->GeneratedClass);auto* R=NewObject<UObject>(GetTransientPackage(),Request->GeneratedClass);bool Passed=M&&S&&R;int32 Checks=0;
    auto Object=[](UObject* O,const TCHAR* N,UObject* V){FindFProperty<FObjectProperty>(O->GetClass(),N)->SetObjectPropertyValue_InContainer(O,V);};auto Bool=[](UObject* O,const TCHAR* N,bool V){FindFProperty<FBoolProperty>(O->GetClass(),N)->SetPropertyValue_InContainer(O,V);};auto Number=[](UObject* O,const TCHAR* N,int32 V){FindFProperty<FIntProperty>(O->GetClass(),N)->SetPropertyValue_InContainer(O,V);};auto String=[](UObject* O,const TCHAR* N,const TCHAR* V){FindFProperty<FStrProperty>(O->GetClass(),N)->SetPropertyValue_InContainer(O,V);};auto Check=[&](bool V,const TCHAR* Stage){++Checks;Passed=Passed&&V;if(!V)UE_LOG(LogTemp,Error,TEXT("DOR-NATIVE-STAGES-VM case=%d stage=%s failed"),Checks,Stage);};
    if(Passed)
    {
        const FGuid Epoch(11,12,13,14),OtherEpoch(15,16,17,18);auto SetEpoch=[&](const FGuid& Value){*FindFProperty<FStructProperty>(R->GetClass(),TEXT("Epoch"))->ContainerPtrToValuePtr<FGuid>(R)=Value;};*FindFProperty<FStructProperty>(M->GetClass(),TEXT("NativeEpoch"))->ContainerPtrToValuePtr<FGuid>(M)=Epoch;SetEpoch(Epoch);Object(M,TEXT("NativeActiveRequest"),R);Bool(M,TEXT("NativeExecutionHeld"),true);for(const TCHAR* N:{TEXT("OwnerFieldsRestored"),TEXT("AuthorityFieldsRestored")})Bool(R,N,true);FindFProperty<FFloatProperty>(R->GetClass(),TEXT("QueuedDeadline"))->SetPropertyValue_InContainer(R,10.0f);FindFProperty<FFloatProperty>(R->GetClass(),TEXT("RecoveryDeadline"))->SetPropertyValue_InContainer(R,20.0f);
        auto Latch=[&](bool Success,float At,bool Live){auto* Fn=M->FindFunction(TEXT("DOR_NativeLatchOutcome"));FStructOnScope P(Fn);FindFProperty<FObjectProperty>(Fn,TEXT("RequestObject"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),R);FindFProperty<FBoolProperty>(Fn,TEXT("Success"))->SetPropertyValue_InContainer(P.GetStructMemory(),Success);FindFProperty<FBoolProperty>(Fn,TEXT("LiveProofs"))->SetPropertyValue_InContainer(P.GetStructMemory(),Live);FindFProperty<FFloatProperty>(Fn,TEXT("AtTime"))->SetPropertyValue_InContainer(P.GetStructMemory(),At);M->ProcessEvent(Fn,P.GetStructMemory());return FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory());};
        Check(!Latch(true,10.0f,true),TEXT("round-boundary-cannot-accept-success"));Check(!Latch(true,9.0f,false),TEXT("missing-live-proof-cannot-latch"));Bool(R,TEXT("OwnerFieldsRestored"),false);Check(!Latch(true,9.0f,true),TEXT("provisional-owner-receipt-required"));Bool(R,TEXT("OwnerFieldsRestored"),true);SetEpoch(OtherEpoch);Check(!Latch(true,9.0f,true),TEXT("stale-epoch-cannot-latch"));SetEpoch(Epoch);Check(Latch(true,9.0f,true),TEXT("current-proven-within-round-latches-before-durable-deletion"));Check(!Latch(false,15.0f,true),TEXT("late-timeout-cannot-revoke-success"));Check(Latch(true,25.0f,false),TEXT("duplicate-accepted-outcome-does-not-revalidate-or-rollback"));Bool(R,TEXT("TerminalLatched"),false);Check(!Latch(false,20.0f,true),TEXT("recovery-boundary-cannot-accept-cancel"));Check(Latch(false,19.0f,true),TEXT("bounded-recovery-outcome-latch"));Check(!Latch(true,9.0f,true),TEXT("late-success-cannot-revoke-accepted-cancel"));
        Bool(R,TEXT("TerminalLatched"),false);for(const TCHAR* N:{TEXT("CancelRequested"),TEXT("StopIssued")}){Bool(R,N,true);Check(!Latch(true,9.0f,true),TEXT("unlatched-cancel-or-stop-prevents-success-race"));Bool(R,N,false);}Bool(R,TEXT("TerminalLatched"),true);
        auto* JP=FindFProperty<FObjectProperty>(Safety->GeneratedClass,TEXT("DOR_Journal"));auto* J=NewObject<UObject>(GetTransientPackage(),JP->PropertyClass);Object(S,TEXT("DOR_Journal"),J);Object(R,TEXT("LocalSafety"),S);String(J,TEXT("TransactionId"),TEXT("verified-deleted-token"));String(R,TEXT("LocalJournalId"),TEXT("verified-deleted-token"));String(S,TEXT("DOR_CheckedNativeFinalizeTransactionId"),TEXT("verified-deleted-token"));Number(R,TEXT("LocalLeaseStep"),4);Bool(S,TEXT("DOR_Active"),false);
        auto Final=[&](){auto* Fn=M->FindFunction(TEXT("DOR_NativeFinalizeLocal"));FStructOnScope P(Fn);FindFProperty<FObjectProperty>(Fn,TEXT("RequestObject"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),R);M->ProcessEvent(Fn,P.GetStructMemory());return FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory());};Check(Final(),TEXT("lost-final-confirmation-can-repeat-actual-local-final-receipt-without-IO"));String(S,TEXT("DOR_CheckedNativeFinalizeTransactionId"),TEXT("stale-token"));Check(!Final(),TEXT("inactive-without-exact-final-receipt-is-not-finalized"));Bool(R,TEXT("TerminalLatched"),false);String(S,TEXT("DOR_CheckedNativeFinalizeTransactionId"),TEXT("verified-deleted-token"));Check(!Final(),TEXT("deleted-receipt-alone-cannot-invent-global-outcome"));
        Bool(S,TEXT("DOR_Active"),true);String(S,TEXT("DOR_CheckedEquipmentRestoreTransactionId"),TEXT("verified-deleted-token"));for(const int32 Phase:{4,5}){Number(J,TEXT("NativeProtocolPhase"),Phase);auto* Fn=S->FindFunction(TEXT("DOR_SourceRebindProjection"));FStructOnScope P(Fn);FindFProperty<FStrProperty>(Fn,TEXT("TransactionId"))->SetPropertyValue_InContainer(P.GetStructMemory(),TEXT("verified-deleted-token"));S->ProcessEvent(Fn,P.GetStructMemory());Check(!FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory())&&FindFProperty<FIntProperty>(J->GetClass(),TEXT("NativeProtocolPhase"))->GetPropertyValue_InContainer(J)==Phase,TEXT("terminal-WAL-cannot-be-rebound-by-late-recovery-before-profile-getters"));}
        Number(J,TEXT("Schema"),3);Bool(J,TEXT("SourceEquipment"),true);Number(J,TEXT("NativeProtocolPhase"),0);Number(R,TEXT("LocalLeaseStep"),1);const FGuid Id(21,22,23,24);*FindFProperty<FStructProperty>(R->GetClass(),TEXT("Request"))->ContainerPtrToValuePtr<FGuid>(R)=Id;*FindFProperty<FStructProperty>(J->GetClass(),TEXT("NativeRequest"))->ContainerPtrToValuePtr<FGuid>(J)=Id;*FindFProperty<FStructProperty>(J->GetClass(),TEXT("NativeEpoch"))->ContainerPtrToValuePtr<FGuid>(J)=Epoch;
        auto NoBank=[&](){auto* Fn=M->FindFunction(TEXT("DOR_NativeCheckNoBankMutation"));FStructOnScope P(Fn);FindFProperty<FObjectProperty>(Fn,TEXT("RequestObject"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),R);M->ProcessEvent(Fn,P.GetStructMemory());return FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory());};
        Check(NoBank(),TEXT("bound-pristine-partial-lease-certifies-no-bank-write-not-restore"));for(const auto& P:{TPair<UObject*,const TCHAR*>(R,TEXT("LocalBankCaptured")),TPair<UObject*,const TCHAR*>(R,TEXT("NativeSubmitted")),TPair<UObject*,const TCHAR*>(M,TEXT("NativeScopeActive")),TPair<UObject*,const TCHAR*>(M,TEXT("RuntimeBankActive")),TPair<UObject*,const TCHAR*>(J,TEXT("QualificationScopePrepared")),TPair<UObject*,const TCHAR*>(J,TEXT("EquipmentStagingIntent")),TPair<UObject*,const TCHAR*>(J,TEXT("EquipmentEverStaged"))}){Bool(P.Key,P.Value,true);Check(!NoBank(),TEXT("acquired-or-submitted-effect-rejects-no-bank-certificate"));Bool(P.Key,P.Value,false);}Number(J,TEXT("Schema"),2);Check(!NoBank(),TEXT("schema2-is-not-wide-source-cleanup-certificate"));Number(J,TEXT("Schema"),3);Number(J,TEXT("NativeGeneration"),1);Check(!NoBank(),TEXT("foreign-journal-generation-cannot-certify-current-partial"));Number(J,TEXT("NativeGeneration"),0);Number(R,TEXT("LocalLeaseStep"),2);Check(!NoBank(),TEXT("advanced-lease-cannot-relabel-bank-acquisition"));Number(R,TEXT("LocalLeaseStep"),1);String(J,TEXT("TransactionId"),TEXT("foreign-token"));Check(!NoBank(),TEXT("foreign-journal-token-cannot-certify-partial"));
        for(const TCHAR* Flag:{TEXT("DOR_SourceLiveLease"),TEXT("DOR_SourceRetainJournal")}){Bool(S,Flag,true);auto* Fn=S->FindFunction(TEXT("DOR_SourceFinalizeRestoredWAL"));FStructOnScope P(Fn);FindFProperty<FObjectProperty>(Fn,TEXT("Record"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),J);S->ProcessEvent(Fn,P.GetStructMemory());Check(!FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory())&&FindFProperty<FBoolProperty>(S->GetClass(),TEXT("DOR_Active"))->GetPropertyValue_InContainer(S),TEXT("live-or-provisional-WAL-cannot-use-startup-delete-path-before-profile-IO"));Bool(S,Flag,false);}
    }
    if(M)World->DestroyActor(M);if(S)World->DestroyActor(S);World->DestroyWorld(false);UE_LOG(LogTemp,Display,TEXT("DOR-NATIVE-STAGES-VM checks=%d passed=%d scope=emitted-terminal-order-stale-deadline-and-final-receipt-no-profile-IO-no-gameplay"),Checks,Passed);return Passed;
}
