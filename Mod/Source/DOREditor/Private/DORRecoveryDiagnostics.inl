// Finite observation of the existing recovery invocation. No diagnostic call
// participates in save validation, journal publication, or resource release.
bool BuildDORRecoveryDiagnostics(UBlueprint* Safety,UBlueprint* Master,UBlueprint* Request,UBlueprint* Context,UBlueprint* Proxy,UWidgetBlueprint* Widget)
{
    auto* JC=FindFProperty<FObjectProperty>(Safety->GeneratedClass,TEXT("DOR_Journal"))->PropertyClass;
    auto* UGC=LoadObject<UClass>(nullptr,TEXT("/Script/SimpleUGC.UGCBlueprintLibrary"));
    auto* Settings=LoadObject<UClass>(nullptr,TEXT("/Script/SimpleUGC.UGCSettings"));if(!UGC||!Settings)return false;
    const auto Characters=MakeStructPinType(FCharacterSave::StaticStruct(),true);
    AddTypedVariable(Safety,TEXT("DOR_RecoveryLoadedSave"),MakeObjectPinType(UFSDSaveGame::StaticClass()));
    for(const TCHAR* N:{TEXT("DOR_RecoveryId"),TEXT("DOR_RecoveryOrigin"),TEXT("DOR_RecoveryCaller"),TEXT("DOR_RecoveryReceiptKey")})AddTypedVariable(Safety,N,MakeStringPinType());
    for(const TCHAR* N:{TEXT("DOR_RecoveryCommitEntered"),TEXT("DOR_RecoveryWriteAttempted"),TEXT("DOR_RecoveryWriteResult"),TEXT("DOR_RecoveryLoadValid")})AddTypedVariable(Safety,N,MakeBoolPinType());
    for(const TCHAR* N:{TEXT("DOR_RecoveryPrewrite"),TEXT("DOR_RecoveryPostwrite"),TEXT("DOR_RecoveryLoaded")})AddTypedVariable(Safety,N,Characters);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Safety);FKismetEditorUtilities::CompileBlueprint(Safety);
    auto Before=[](UEdGraph* G,UEdGraphPin* In,const TCHAR* Name){auto Old=In->LinkedTo;In->BreakAllPinLinks();auto* C=AddSelfCall(G,Name,0,0);for(auto* P:Old)Connect(P,C->GetExecPin());Connect(C->GetThenPin(),In);return C;};
    auto After=[](UEdGraph* G,UEdGraphPin* Out,const TCHAR* Name){auto Old=Out->LinkedTo;Out->BreakAllPinLinks();auto* C=AddSelfCall(G,Name,0,0);Connect(Out,C->GetExecPin());for(auto* P:Old)Connect(C->GetThenPin(),P);return C;};
    auto World=[](FDORNativeGraph& F){auto* GS=F.Call(UGameplayStatics::StaticClass(),TEXT("GetGameState"));auto* L=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetOuterObject"));Connect(GS->GetReturnValuePin(),FindPin(L,TEXT("Object")));auto* W=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetOuterObject"));Connect(L->GetReturnValuePin(),FindPin(W,TEXT("Object")));return W->GetReturnValuePin();};
    {
        FDORBatchGraph F(Safety,TEXT("DOR_RecoveryBegin"),Master->GeneratedClass);auto* Id=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("NewGuid"));F.Set(TEXT("DOR_RecoveryId"),F.GuidText(Id->GetReturnValuePin()));F.Set(TEXT("DOR_RecoveryOrigin"),F.Str(TEXT("journal-load-not-completed")));F.Set(TEXT("DOR_RecoveryLoadedSave"),nullptr);
        for(const TCHAR* N:{TEXT("DOR_RecoveryCommitEntered"),TEXT("DOR_RecoveryWriteAttempted"),TEXT("DOR_RecoveryWriteResult"),TEXT("DOR_RecoveryLoadValid")})F.Set(N,nullptr,TEXT("false"));
        for(const TCHAR* N:{TEXT("DOR_RecoveryPrewrite"),TEXT("DOR_RecoveryPostwrite"),TEXT("DOR_RecoveryLoaded")})F.Array(TEXT("Array_Clear"),F.Get(N));F.Done();F.Compile();
    }
    {
        FDORBatchGraph F(Safety,TEXT("DOR_RecoveryLoadedOrigin"),Master->GeneratedClass);auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JC));FDORNativeProbeLine D(F,TEXT("Origin"),TEXT("loaded-before-validation-and-publication|endPlayReason=unavailable"));D.Add(TEXT("journalValid"),F.IsValid(J));auto* Have=F.Branch(F.IsValid(J));
        for(const TCHAR* N:{TEXT("TransactionId"),TEXT("SourceSlot"),TEXT("SourceAnonymousId"),TEXT("NativePlayerKey")})D.Add(N,F.Field(J,JC,N),NAME_None);
        for(const TCHAR* N:{TEXT("Schema"),TEXT("SourceIndex"),TEXT("NativeProtocolPhase"),TEXT("NativeGeneration"),TEXT("NativeSlot")})D.Add(N,F.Field(J,JC,N),TEXT("Conv_IntToString"));
        for(const TCHAR* N:{TEXT("Registration"),TEXT("Cleanup"),TEXT("SourceEquipment")})D.Add(N,F.Field(J,JC,N));
        for(const TCHAR* N:{TEXT("NativeRequest"),TEXT("NativeEpoch"),TEXT("EquipmentCharacter")})D.Add(N,F.Field(J,JC,N),TEXT("Conv_GuidToString"));F.Merge(Have->GetElsePin());F.Set(TEXT("DOR_RecoveryOrigin"),D.Line);F.Done();F.Compile();
    }
    // Scope is exactly CharacterSaves and its reflected Blueprint fields. A
    // first difference returns immediately; no complete profile is serialized.
    {
        FDORBatchGraph F(Safety,TEXT("DOR_RecoveryCharacterDifference"),Master->GeneratedClass);auto* A=F.Input(TEXT("A"),Characters);auto* B=F.Input(TEXT("B"),Characters);auto* NativeEqual=F.Branch(F.SameArray(A,B));F.Return(F.Str(TEXT("equal")));F.Tail=NativeEqual->GetElsePin();
        auto ScalarText=[&](UEdGraphPin* P)->UEdGraphPin*{
            const auto& T=P->PinType;if(T.PinCategory==UEdGraphSchema_K2::PC_String)return P;
            if(T.PinCategory==UEdGraphSchema_K2::PC_Boolean)return F.Text(P,TEXT("Conv_BoolToString"));
            if(T.PinCategory==UEdGraphSchema_K2::PC_Int)return F.Number(P);
            if(T.PinCategory==UEdGraphSchema_K2::PC_Float)return F.Text(P,TEXT("Conv_FloatToString"));
            if(T.PinCategory==UEdGraphSchema_K2::PC_Byte){auto* C=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Conv_ByteToInt"));Connect(P,FindPin(C,TEXT("InByte")));return F.Number(C->GetReturnValuePin());}
            if(T.PinCategory==UEdGraphSchema_K2::PC_Struct&&T.PinSubCategoryObject==TBaseStructure<FGuid>::Get())return F.GuidText(P);
            FDORNativeProbeLine D(F,TEXT("Unused"),TEXT(""));
            if(T.PinCategory==UEdGraphSchema_K2::PC_Object)return D.Path(P);
            if(T.PinCategory==UEdGraphSchema_K2::PC_Class)return D.ClassPath(P);
            return F.Str(TEXT("value-unavailable-through-shipped-BP"));
        };
        auto One=[&](UEdGraphPin* P){auto* N=AddNode<UK2Node_MakeArray>(F.G,++F.Serial*160,0);N->NumInputs=1;N->AllocateDefaultPins();auto T=P->PinType;T.ContainerType=EPinContainerType::Array;T.bIsReference=false;T.bIsConst=false;ForcePinType(N->GetOutputPin(),T);ForcePinType(FindPin(N,TEXT("[0]")),P->PinType);Connect(P,FindPin(N,TEXT("[0]")));return N->GetOutputPin();};
        auto Difference=[&](UEdGraphPin* Equal,UEdGraphPin* Path,UEdGraphPin* AV,UEdGraphPin* BV){auto* Same=F.Branch(Equal);auto* Good=F.Tail;F.Tail=Same->GetElsePin();F.Return(F.Add(F.Add(Path,TEXT("|A="),AV),TEXT("|B="),BV));F.Tail=Good;};
        TFunction<void(UEdGraphPin*,UEdGraphPin*,UEdGraphPin*,int32)> Visit;
        Visit=[&](UEdGraphPin* X,UEdGraphPin* Y,UEdGraphPin* Path,int32 Depth){
            if(Depth>14){F.Return(F.Join(Path,F.Str(TEXT("|limitation=depth-limit"))));return;}
            auto T=X->PinType;
            if(T.ContainerType==EPinContainerType::Array){
                auto* XL=F.Array(TEXT("Array_Length"),X)->GetReturnValuePin();auto* YL=F.Array(TEXT("Array_Length"),Y)->GetReturnValuePin();Difference(F.Eq(XL,YL),F.Join(Path,F.Str(TEXT(".length"))),F.Number(XL),F.Number(YL));
                auto* Loop=F.Each(X);auto* Index=FindPin(Loop,TEXT("Array Index"));auto* Get=F.Array(TEXT("Array_Get"),Y);Connect(Index,FindPin(Get,TEXT("Index")));auto* E=FindPin(Loop,TEXT("Array Element"));auto* P=F.Join(F.Add(Path,TEXT("["),F.Number(Index)),F.Str(TEXT("]")));
                if(T.PinSubCategoryObject==FCharacterSave::StaticStruct())P=F.Add(P,TEXT("{SavegameID="),F.Join(F.GuidText(F.FieldOf(E,FCharacterSave::StaticStruct(),TEXT("SavegameID"))),F.Str(TEXT("}"))));
                Visit(E,FindPin(Get,TEXT("Item")),P,Depth+1);F.Tail=FindPin(Loop,TEXT("Completed"));return;
            }
            if(T.ContainerType==EPinContainerType::Map){
                auto KeyType=T;KeyType.ContainerType=EPinContainerType::None;auto ValueType=MakeStringPinType();ValueType.PinCategory=T.PinValueType.TerminalCategory;ValueType.PinSubCategory=T.PinValueType.TerminalSubCategory;ValueType.PinSubCategoryObject=T.PinValueType.TerminalSubCategoryObject;ValueType.ContainerType=EPinContainerType::None;
                auto Keys=[&](UEdGraphPin* M){auto* N=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Keys"));ForcePinType(FindPin(N,TEXT("TargetMap")),T);auto KT=KeyType;KT.ContainerType=EPinContainerType::Array;ForcePinType(FindPin(N,TEXT("Keys")),KT);Connect(M,FindPin(N,TEXT("TargetMap")));return FindPin(N,TEXT("Keys"));};
                auto* XK=F.Cache(TEXT("KeysA"),Keys(X));auto* YK=F.Cache(TEXT("KeysB"),Keys(Y));auto* XL=F.Array(TEXT("Array_Length"),XK)->GetReturnValuePin();auto* YL=F.Array(TEXT("Array_Length"),YK)->GetReturnValuePin();Difference(F.Eq(XL,YL),F.Join(Path,F.Str(TEXT(".mapLength"))),F.Number(XL),F.Number(YL));
                auto* Loop=F.Each(XK);auto* Key=FindPin(Loop,TEXT("Array Element"));auto Find=[&](UEdGraphPin* M){auto* N=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"));ForcePinType(FindPin(N,TEXT("TargetMap")),T);ForcePinType(FindPin(N,TEXT("Key")),KeyType);ForcePinType(FindPin(N,TEXT("Value")),ValueType);Connect(M,FindPin(N,TEXT("TargetMap")));Connect(Key,FindPin(N,TEXT("Key")));return N;};auto* XV=Find(X);auto* YV=Find(Y);auto* P=F.Join(F.Add(Path,TEXT("{key="),ScalarText(Key)),F.Str(TEXT("}")));Difference(YV->GetReturnValuePin(),P,F.Str(TEXT("present")),F.Str(TEXT("missing")));Visit(FindPin(XV,TEXT("Value")),FindPin(YV,TEXT("Value")),P,Depth+1);F.Tail=FindPin(Loop,TEXT("Completed"));return;
            }
            if(T.PinCategory==UEdGraphSchema_K2::PC_Struct&&T.PinSubCategoryObject!=TBaseStructure<FGuid>::Get()){
                auto* NativeSame=F.Branch(F.SameArray(One(X),One(Y)));auto* SameTail=F.Tail;F.Tail=NativeSame->GetElsePin();auto* S=CastChecked<UScriptStruct>(T.PinSubCategoryObject.Get());auto* XB=F.Break(X,S);auto* YB=F.Break(Y,S);
                for(TFieldIterator<FProperty> It(S);It;++It){auto* XP=XB->FindPin(It->GetFName());auto* YP=YB->FindPin(It->GetFName());if(!XP||!YP){F.Return(F.Join(Path,F.Str(TEXT("|limitation=field-not-exposed:")+It->GetName())));return;}Visit(XP,YP,F.Join(Path,F.Str(TEXT(".")+It->GetName())),Depth+1);}F.Return(F.Join(Path,F.Str(TEXT("|limitation=native-struct-diff-with-no-exposed-field-difference"))));F.Tail=SameTail;return;
            }
            Difference(F.SameArray(One(X),One(Y)),Path,ScalarText(X),ScalarText(Y));
        };
        Visit(A,B,F.Str(TEXT("CharacterSaves")),0);F.Return(F.Str(TEXT("limitation=native-array-diff-with-no-exposed-field-difference")));F.Compile();
    }
    auto Normalized=[&](FDORBatchGraph& F,UEdGraphPin* CharactersPin){auto* N=F.Self(TEXT("DOR_NormalizeCharactersForComparison"));Connect(CharactersPin,FindPin(N,TEXT("Characters")));return F.Cache(TEXT("NormalizedComparisonCopy"),N->GetReturnValuePin());};
    auto AddComparison=[&](FDORBatchGraph& F,FDORNativeProbeLine& D,const FString& Label,UEdGraphPin* A,UEdGraphPin* B){
        auto* AC=Normalized(F,A);auto* BC=Normalized(F,B);D.Add(Label+TEXT(".nativeEqual"),F.SameArray(AC,BC));
        auto* Diff=F.Self(TEXT("DOR_RecoveryCharacterDifference"));Connect(AC,FindPin(Diff,TEXT("A")));Connect(BC,FindPin(Diff,TEXT("B")));D.Add(Label+TEXT(".firstDifference"),Diff->GetReturnValuePin(),NAME_None);
    };
    {
        FDORBatchGraph F(Safety,TEXT("DOR_PrepareCharactersReceipt"),Master->GeneratedClass);auto* A=F.Input(TEXT("A"),Characters);auto* B=F.Input(TEXT("B"),Characters);auto* Equal=F.Input(TEXT("NormalizedEqual"),MakeBoolPinType());auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JC));
        FDORNativeProbeLine D(F,TEXT("PrepareComparison"),TEXT("[DOR-PREPARE]|event=existing-beforecharacters-readback|normalizationPolicy=MedicalGownIndex-only-v1|scope=BeforeCharacters-comparison-only"));
        D.Add(TEXT("build"),F.Str(DORResearchBuildTag),NAME_None);D.Add(TEXT("TransactionId"),F.Field(J,JC,TEXT("TransactionId")),NAME_None);D.Add(TEXT("Registration"),F.Field(J,JC,TEXT("Registration")));D.Add(TEXT("Cleanup"),F.Field(J,JC,TEXT("Cleanup")));D.Add(TEXT("normalizedGateResult"),Equal);D.Add(TEXT("rawNativeEqual"),F.SameArray(A,B));
        auto* Raw=F.Self(TEXT("DOR_RecoveryCharacterDifference"));Connect(A,FindPin(Raw,TEXT("A")));Connect(B,FindPin(Raw,TEXT("B")));D.Add(TEXT("rawFirstDifference"),Raw->GetReturnValuePin(),NAME_None);AddComparison(F,D,TEXT("normalizedBeforeCharacters"),A,B);
        auto* Failed=F.Branch(F.Not(Equal));D.Add(TEXT("failedPredicate"),F.Str(TEXT("DOR_Prepare|DOR_CharactersPersistentEqual(BeforeCharacters,BeforeCharacters)")),NAME_None);F.Merge(Failed->GetElsePin());
        DORNormalReceipt(F,D.Line,true,true);F.Done();F.Compile();
    }
    // Observe this existing comparison before its unchanged Require branch.
    auto* Prepare=FindFunctionGraph(Safety,TEXT("DOR_Prepare"));UK2Node_CallFunction* PrepareComparison=nullptr;
    for(auto* N:Prepare->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("DOR_CharactersPersistentEqual"))PrepareComparison=C;
    if(!PrepareComparison)return false;
    {
        auto* A=FindPin(PrepareComparison,TEXT("A"))->LinkedTo[0];auto* B=FindPin(PrepareComparison,TEXT("B"))->LinkedTo[0];auto* J=FindPin(A->GetOwningNode(),UEdGraphSchema_K2::PN_Self)->LinkedTo[0];
        auto* Receipt=After(Prepare,PrepareComparison->GetThenPin(),TEXT("DOR_PrepareCharactersReceipt"));Connect(A,FindPin(Receipt,TEXT("A")));Connect(B,FindPin(Receipt,TEXT("B")));Connect(J,FindPin(Receipt,TEXT("Record")));Connect(PrepareComparison->GetReturnValuePin(),FindPin(Receipt,TEXT("NormalizedEqual")));
    }
    {
        FDORBatchGraph F(Safety,TEXT("DOR_RecoveryReceipt"),Master->GeneratedClass);auto* Result=F.Input(TEXT("Result"),MakeBoolPinType());auto* Key=F.Cache(TEXT("ReceiptKey"),F.Join(F.Join(F.Get(TEXT("DOR_RecoveryOrigin")),F.Get(TEXT("DOR_InitFailedGate"))),F.Text(Result,TEXT("Conv_BoolToString"))));F.QuietRequire(F.Not(F.Eq(Key,F.Get(TEXT("DOR_RecoveryReceiptKey")),TEXT("EqualEqual_StrStr"))));FDORNativeProbeLine D(F,TEXT("Receipt"),TEXT("[DOR-RECOVERY]|event=existing-restore-return|save-protection=unchanged"));D.Add(TEXT("build"),F.Str(DORResearchBuildTag),NAME_None);D.Add(TEXT("capture"),F.Get(TEXT("DOR_RecoveryId")),NAME_None);D.Add(TEXT("result"),Result);D.Add(TEXT("callerFunction"),F.Get(TEXT("DOR_RecoveryCaller")),NAME_None);D.Add(TEXT("service"),D.Path(SelfPin(F.G,0,0)),NAME_None);D.Add(TEXT("world"),D.Path(World(F)),NAME_None);D.Add(TEXT("loadedOrigin"),F.Get(TEXT("DOR_RecoveryOrigin")),NAME_None);
        for(const TCHAR* N:{TEXT("DOR_InitFailedGate"),TEXT("DOR_LastError"),TEXT("DOR_SourceSlot"),TEXT("DOR_AnonymousId"),TEXT("DOR_JournalSlot")})D.Add(N,F.Get(N),NAME_None);D.Add(TEXT("retainedIndex"),F.Get(TEXT("DOR_SourceIndex")),TEXT("Conv_IntToString"));
        for(const TCHAR* N:{TEXT("DOR_Ready"),TEXT("DOR_Active"),TEXT("DOR_Blocked"),TEXT("DOR_RecoveryCommitEntered"),TEXT("DOR_RecoveryWriteAttempted"),TEXT("DOR_RecoveryWriteResult"),TEXT("DOR_RecoveryLoadValid")})D.Add(N,F.Get(N));
        auto* Save=F.Get(TEXT("DOR_InitLiveSave"));D.Add(TEXT("liveSave"),D.Path(Save),NAME_None);auto* Have=F.Branch(F.IsValid(Save));D.Add(TEXT("liveAnonymousID"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),NAME_None);D.Add(TEXT("liveIndex"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")),TEXT("Conv_IntToString"));D.Add(TEXT("rawSlot"),F.Get(TEXT("DOR_InitRawSlot")),NAME_None);D.Add(TEXT("resolvedSlot"),F.Get(TEXT("DOR_InitResolvedSlot")),NAME_None);F.Merge(Have->GetElsePin());
        for(const TCHAR* N:{TEXT("DOR_RecoveryPrewrite"),TEXT("DOR_RecoveryPostwrite"),TEXT("DOR_RecoveryLoaded")})D.Add(FString(N)+TEXT(".length"),F.Array(TEXT("Array_Length"),F.Get(N))->GetReturnValuePin(),TEXT("Conv_IntToString"));
        auto* LoadedSave=F.Get(TEXT("DOR_RecoveryLoadedSave"));auto* LoadedIdentity=F.Branch(F.IsValid(LoadedSave));D.Add(TEXT("loadedSave"),D.Path(LoadedSave),NAME_None);D.Add(TEXT("loadedAnonymousID"),F.Field(LoadedSave,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),NAME_None);D.Add(TEXT("loadedIndex"),F.Field(LoadedSave,UFSDSaveGame::StaticClass(),TEXT("Index")),TEXT("Conv_IntToString"));auto* RawLoaded=F.Call(UFSDSaveGame::StaticClass(),TEXT("GetSlotLoadedFrom"));Connect(LoadedSave,FindPin(RawLoaded,UEdGraphSchema_K2::PN_Self));D.Add(TEXT("loadedRawSlot"),RawLoaded->GetReturnValuePin(),NAME_None);F.Merge(LoadedIdentity->GetElsePin());
        auto* Compared=F.Branch(F.And(F.Get(TEXT("DOR_RecoveryWriteAttempted")),F.Get(TEXT("DOR_RecoveryLoadValid"))));
        for(const auto& Pair:{TPair<const TCHAR*,const TCHAR*>(TEXT("Prewrite"),TEXT("Postwrite")),TPair<const TCHAR*,const TCHAR*>(TEXT("Prewrite"),TEXT("Loaded")),TPair<const TCHAR*,const TCHAR*>(TEXT("Postwrite"),TEXT("Loaded"))}){auto* C=F.Self(TEXT("DOR_RecoveryCharacterDifference"));Connect(F.Get(FName(*(FString(TEXT("DOR_Recovery"))+Pair.Key))),FindPin(C,TEXT("A")));Connect(F.Get(FName(*(FString(TEXT("DOR_Recovery"))+Pair.Value))),FindPin(C,TEXT("B")));D.Add(FString(Pair.Key)+TEXT("Vs")+Pair.Value,C->GetReturnValuePin(),NAME_None);}
        D.Add(TEXT("normalizationPolicy"),F.Str(TEXT("MedicalGownIndex-only-v1")),NAME_None);
        for(const TCHAR* Stage:{TEXT("Prewrite"),TEXT("Postwrite")})AddComparison(F,D,FString(TEXT("normalized"))+Stage+TEXT("VsLoaded"),F.Get(FName(*(FString(TEXT("DOR_Recovery"))+Stage))),F.Get(TEXT("DOR_RecoveryLoaded")));
        F.Merge(Compared->GetElsePin());
        DORNormalReceipt(F,D.Line,true,true);F.Set(TEXT("DOR_RecoveryReceiptKey"),Key);F.Done();F.Compile();
    }
    // Observe already-computed writer/load results and preserve original gates.
    auto* Commit=FindFunctionGraph(Safety,TEXT("DOR_CommitNativeSave"));UK2Node_CallFunction* Write=nullptr;UK2Node_VariableSet* Loaded=nullptr;
    for(auto* N:Commit->Nodes){if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("SaveSlotToDisk"))Write=C;if(auto* S=Cast<UK2Node_VariableSet>(N))if(S->VariableReference.GetMemberName().ToString().StartsWith(TEXT("DOR_CommitNativeSave_Loaded_")))Loaded=S;}if(!Write||!Loaded)return false;
    auto* SavePin=FindPin(Write,TEXT("SaveSlot"))->LinkedTo[0];
    for(const auto& Stage:{TPair<const TCHAR*,const TCHAR*>(TEXT("DOR_RecoveryCapturePrewrite"),TEXT("DOR_RecoveryPrewrite")),TPair<const TCHAR*,const TCHAR*>(TEXT("DOR_RecoveryCapturePostwrite"),TEXT("DOR_RecoveryPostwrite"))}){FDORBatchGraph F(Safety,Stage.Key,Master->GeneratedClass);auto* Save=F.Input(TEXT("Save"),MakeObjectPinType(UFSDSaveGame::StaticClass()));if(FString(Stage.Value)==TEXT("DOR_RecoveryPrewrite"))F.Set(TEXT("DOR_RecoveryWriteAttempted"),nullptr,TEXT("true"));else{auto* Result=F.Input(TEXT("WriteResult"),MakeBoolPinType());F.Set(TEXT("DOR_RecoveryWriteResult"),Result);}F.Set(Stage.Value,F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("CharacterSaves")));F.Done();F.Compile();}
    {auto* C=Before(Commit,Write->GetExecPin(),TEXT("DOR_RecoveryCapturePrewrite"));Connect(SavePin,FindPin(C,TEXT("Save")));C=After(Commit,Write->GetThenPin(),TEXT("DOR_RecoveryCapturePostwrite"));Connect(SavePin,FindPin(C,TEXT("Save")));Connect(Write->GetReturnValuePin(),FindPin(C,TEXT("WriteResult")));}
    {FDORBatchGraph F(Safety,TEXT("DOR_RecoveryCaptureLoaded"),Master->GeneratedClass);auto* Save=F.Input(TEXT("Save"),MakeObjectPinType(UFSDSaveGame::StaticClass()));F.Set(TEXT("DOR_RecoveryLoadedSave"),Save);F.Set(TEXT("DOR_RecoveryLoadValid"),F.IsValid(Save));auto* Have=F.Branch(F.IsValid(Save));F.Set(TEXT("DOR_RecoveryLoaded"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("CharacterSaves")));F.Merge(Have->GetElsePin());F.Done();F.Compile();auto* C=After(Commit,FindPin(Loaded,UEdGraphSchema_K2::PN_Then),TEXT("DOR_RecoveryCaptureLoaded"));Connect(AddVariableGet(Commit,Loaded->VariableReference.GetMemberName(),0,0)->GetValuePin(),FindPin(C,TEXT("Save")));}
    auto* Restore=FindFunctionGraph(Safety,TEXT("DOR_RestoreJournal"));Before(Restore,FindFunctionEntry(Restore)->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo[0],TEXT("DOR_RecoveryBegin"));
    for(auto* N:TArray<UEdGraphNode*>(Restore->Nodes))if(auto* C=Cast<UK2Node_CallFunction>(N)){if(C->FunctionReference.GetMemberName()==TEXT("DOR_ValidateJournal")){auto* O=Before(Restore,C->GetExecPin(),TEXT("DOR_RecoveryLoadedOrigin"));Connect(FindPin(C,TEXT("Record"))->LinkedTo[0],FindPin(O,TEXT("Record")));}if(C->FunctionReference.GetMemberName()==TEXT("DOR_CommitNativeSave")){auto Old=C->GetExecPin()->LinkedTo;C->GetExecPin()->BreakAllPinLinks();auto* S=AddVariableSet(Restore,TEXT("DOR_RecoveryCommitEntered"),0,0);FindPin(S,TEXT("DOR_RecoveryCommitEntered"))->DefaultValue=TEXT("true");for(auto* P:Old)Connect(P,S->GetExecPin());Connect(FindPin(S,UEdGraphSchema_K2::PN_Then),C->GetExecPin());}}
    for(auto* N:TArray<UEdGraphNode*>(Restore->Nodes))if(auto* R=Cast<UK2Node_FunctionResult>(N)){auto* C=Before(Restore,FindPin(R,UEdGraphSchema_K2::PN_Execute),TEXT("DOR_RecoveryReceipt"));auto* V=FindPin(R,TEXT("ReturnValue"));if(V->LinkedTo.Num())Connect(V->LinkedTo[0],FindPin(C,TEXT("Result")));else FindPin(C,TEXT("Result"))->DefaultValue=V->DefaultValue;}
    for(auto* G:Safety->FunctionGraphs)for(auto* N:TArray<UEdGraphNode*>(G->Nodes))if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("DOR_RestoreJournal")){auto Old=C->GetExecPin()->LinkedTo;C->GetExecPin()->BreakAllPinLinks();auto* S=AddVariableSet(G,TEXT("DOR_RecoveryCaller"),0,0);FindPin(S,TEXT("DOR_RecoveryCaller"))->DefaultValue=G->GetName();for(auto* P:Old)Connect(P,S->GetExecPin());Connect(FindPin(S,UEdGraphSchema_K2::PN_Then),C->GetExecPin());}
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Safety);FKismetEditorUtilities::CompileBlueprint(Safety);return Safety->Status!=BS_Error;
}
