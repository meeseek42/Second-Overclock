// Only scalar run data crosses worlds. Native requests, actors, delegates and
// transaction journals remain owned by the existing per-world paths.
bool BuildDORDeepDiveRun(UBlueprint* Master,UBlueprint* Safety,UBlueprint* Preferences,UBlueprint* Receipt,UBlueprint* Proxy,UBlueprint*& Run)
{
    auto* DC=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.DeepDiveManager"));
    if(!DC)return false;

    Run=CreateObjectBlueprint(FString(RuntimeRoot)/TEXT("Blueprints/SG_DOR_DeepDiveRun"),TEXT("SG_DOR_DeepDiveRun"),USaveGame::StaticClass());
    if(!Run)return false;

    const TCHAR* Slot=TEXT("Mods/DOR_DeepDiveRun");

    const TArray<FName> Strings={TEXT("RunId")};

    const TArray<FName> Ints={TEXT("Schema"),TEXT("EntryStage"),TEXT("DDStage1"),TEXT("DDStage2"),TEXT("Revision")};

    const TArray<FName> Bools={TEXT("ActiveHideHighLoad"),TEXT("ActiveModEnabled"),TEXT("DDMode"),TEXT("ManualRewardBlocked")};

    const TArray<FName> StringArrays={TEXT("DDRosterKeys"),TEXT("ReplayPlayerKeys"),TEXT("RewardedPlayerSlotKeys"),TEXT("IssuedOpportunities"),TEXT("OpportunitySources"),TEXT("OriginalGroupKeys"),TEXT("OpportunityWaveClosed")};

    const TArray<FName> IntArrays={TEXT("DDRosterCaps"),TEXT("ReplayPrimaryCodes"),TEXT("ReplaySecondaryCodes")};

    const TArray<FName> ReceiptStrings={TEXT("Key"),TEXT("Opportunity"),TEXT("Slot"),TEXT("SupplyResult")};

    const TArray<FName> ReceiptInts={TEXT("State"),TEXT("Code"),TEXT("ChoiceIndex")};

    const TArray<FName> ReceiptBools={TEXT("RoundSettled"),TEXT("SupplyAttempted"),TEXT("OfferStored"),TEXT("DDOriginalRecipient")};

    const TArray<FName> OfferStrings={TEXT("OfferedSlots"),TEXT("OfferedWeaponNames"),TEXT("OfferedOverclockNames"),TEXT("OfferedEffects"),TEXT("OfferedCategories")};

    for(auto N:Strings)AddSavedTypedVariable(Run,N,MakeStringPinType());
    for(auto N:Ints)AddSavedTypedVariable(Run,N,MakeIntPinType());
    for(auto N:Bools)AddSavedTypedVariable(Run,N,MakeBoolPinType());

    for(auto N:StringArrays)AddSavedTypedVariable(Run,N,MakeStringPinType(true));
    for(auto N:IntArrays)AddSavedTypedVariable(Run,N,MakeIntPinType(true));

    auto RN=[](FName N){return FName(*(TEXT("Receipt")+N.ToString()));
    };

    for(auto N:ReceiptStrings)AddSavedTypedVariable(Run,RN(N),MakeStringPinType(true));
    for(auto N:ReceiptInts)AddSavedTypedVariable(Run,RN(N),MakeIntPinType(true));
    for(auto N:ReceiptBools)AddSavedTypedVariable(Run,RN(N),MakeBoolPinType(true));

    AddSavedTypedVariable(Run,TEXT("OfferCounts"),MakeIntPinType(true));
    AddSavedTypedVariable(Run,TEXT("OfferedChoiceCodes"),MakeIntPinType(true));
    for(auto N:OfferStrings)AddSavedTypedVariable(Run,N,MakeStringPinType(true));

    AddTypedVariable(Receipt,TEXT("DDOriginalRecipient"),MakeBoolPinType());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Receipt);
    FKismetEditorUtilities::CompileBlueprint(Receipt);
    if(Receipt->Status==BS_Error)return false;
    FKismetEditorUtilities::CompileBlueprint(Run);
    if(Run->Status==BS_Error)return false;
    UClass* RC=Run->GeneratedClass;
    UClass* C=Receipt->GeneratedClass;

    AddTypedVariable(Master,TEXT("DDRun"),MakeObjectPinType(RC));

    for(const TCHAR* N:{TEXT("DDWorldBound"),TEXT("DDLoading"),TEXT("DDMode"),TEXT("DDOpeningLogged"),TEXT("DDPersistenceFailed"),TEXT("DDCloseSaved"),TEXT("DDWorldRejected"),TEXT("DDRunRetired")})AddTypedVariable(Master,N,MakeBoolPinType());
    auto* EndReasonEnum=FindObject<UEnum>(ANY_PACKAGE,TEXT("EEndPlayReason"));
    if(!EndReasonEnum)return false;
    FEdGraphPinType EndReasonType;EndReasonType.PinCategory=UEdGraphSchema_K2::PC_Byte;EndReasonType.PinSubCategoryObject=EndReasonEnum;
    AddTypedVariable(Master,TEXT("DDEndPlayReason"),EndReasonType);

    for(const TCHAR* N:{TEXT("DDEntryStage"),TEXT("DDStage1"),TEXT("DDStage2")})AddTypedVariable(Master,N,MakeIntPinType());

    AddTypedVariable(Master,TEXT("DDRosterKeys"),MakeStringPinType(true));
    AddTypedVariable(Master,TEXT("DDRosterCaps"),MakeIntPinType(true));

    AddTypedVariable(Safety,TEXT("DDCleanupChecked"),MakeBoolPinType());

    for(auto* BP:{Master,Safety}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if(BP->Status==BS_Error)return false;
    }
    auto Mission=[](FDOROrdinaryGraph& F){auto* GI=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameInstance"));
        return F.Cache(TEXT("DDMission"),F.Read(UFSDGameInstance::StaticClass(),TEXT("GetGeneratedMission"),GI->GetReturnValuePin()));
    };

    auto Manager=[&](FDOROrdinaryGraph& F){auto* Get=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetDeepDiveManager"));
        auto* Obj=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("Conv_InterfaceToObject"));
        Connect(Get->GetReturnValuePin(),FindPin(Obj,TEXT("Interface")));
        return F.Cache(TEXT("DDManager"),F.Cast(Obj->GetReturnValuePin(),DC));
    };

    auto ClampStage=[](FDOROrdinaryGraph& F,UEdGraphPin* V){auto* X=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Clamp"));
        Connect(V,FindPin(X,TEXT("Value")));
        FindPin(X,TEXT("Min"))->DefaultValue=TEXT("1");
        FindPin(X,TEXT("Max"))->DefaultValue=TEXT("3");
        return X->GetReturnValuePin();
    };

    auto SetAt=[](FDOROrdinaryGraph& F,UEdGraphPin* A,UEdGraphPin* I,UEdGraphPin* V){auto* Set=F.Array(TEXT("Array_Set"),A);
        Connect(I,FindPin(Set,TEXT("Index")));
        Connect(V,FindPin(Set,TEXT("Item")));
    };

    auto Log=[&](FDOROrdinaryGraph& F,const TCHAR* Stage){FDORNativeProbeLine D(F,TEXT("DeepDive"),TEXT("[DOR-DD]|gameplay=awaiting-test"));
        D.Add(TEXT("event"),F.Str(Stage),NAME_None);
        D.Add(TEXT("run"),F.Field(F.Get(TEXT("DDRun")),RC,TEXT("RunId")),NAME_None);
        D.Add(TEXT("entryStage"),F.Get(TEXT("DDEntryStage")),TEXT("Conv_IntToString"));
        auto* NativeStage=F.Cache(TEXT("NativeStageObservation"),F.Int(-1));
        auto* DM=Manager(F);auto* HaveManager=F.Branch(F.IsValid(DM));
        F.Set(F.Variable(NativeStage),F.Read(DC,TEXT("GetCurrentDeepDiveStage"),DM));
        F.Merge(HaveManager->GetElsePin());
        D.Add(TEXT("nativeStage"),NativeStage,TEXT("Conv_IntToString"));
        D.Add(TEXT("opening"),F.Get(TEXT("StartingDoorsOpen")));
        D.Add(TEXT("startingPod"),F.Path(F.Get(TEXT("StartingPod"))),NAME_None);
        D.Add(TEXT("mode"),F.Get(TEXT("DDMode")));
        D.Add(TEXT("modEnabled"),F.Get(TEXT("ActiveModEnabled")));
        D.Add(TEXT("reward1"),F.Get(TEXT("DDStage1")),TEXT("Conv_IntToString"));
        D.Add(TEXT("reward2"),F.Get(TEXT("DDStage2")),TEXT("Conv_IntToString"));
        D.Add(TEXT("manualBlocked"),F.Get(TEXT("ManualRewardBlocked")));
        D.Add(TEXT("roster"),F.Count(F.Get(TEXT("DDRosterKeys"))),TEXT("Conv_IntToString"));
        D.Add(TEXT("receipts"),F.Count(F.Get(TEXT("RewardReceipts"))),TEXT("Conv_IntToString"));
        D.Record();
        F.Self(TEXT("DOR_DiagnosticFlush"));
    };

    // Write only at meaningful events. Read back the revision and run id, not
    // merely the SaveGameToSlot boolean. Never route through native save APIs.
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDSaveRun"),Master->GeneratedClass);
        F.QuietRequire(F.Not(F.Get(TEXT("DDRunRetired"))));
        F.QuietRequire(DORLocalHost(F));
        F.QuietRequire(F.Get(TEXT("DDWorldBound")));
        F.QuietRequire(F.Not(F.Get(TEXT("DDLoading"))));
        auto* R=F.Cache(TEXT("Run"),F.Get(TEXT("DDRun")));
        F.QuietRequire(F.IsValid(R));

        for(auto N:StringArrays)F.Put(R,RC,N,F.Get(N));
        for(auto N:IntArrays)F.Put(R,RC,N,F.Get(N));
        for(auto N:Bools)F.Put(R,RC,N,F.Get(N));

        F.Put(R,RC,TEXT("EntryStage"),F.Get(TEXT("DDEntryStage")));
        for(const TCHAR* N:{TEXT("DDStage1"),TEXT("DDStage2")})F.Put(R,RC,N,F.Get(N));
        F.Put(R,RC,TEXT("Schema"),F.Int(1));
        F.Put(R,RC,TEXT("Revision"),F.Math(TEXT("Add_IntInt"),F.Field(R,RC,TEXT("Revision")),F.Int(1)));

        for(auto N:ReceiptStrings)F.Array(TEXT("Array_Clear"),F.Field(R,RC,RN(N)));
        for(auto N:ReceiptInts)F.Array(TEXT("Array_Clear"),F.Field(R,RC,RN(N)));
        for(auto N:ReceiptBools)F.Array(TEXT("Array_Clear"),F.Field(R,RC,RN(N)));
        F.Array(TEXT("Array_Clear"),F.Field(R,RC,TEXT("OfferCounts")));
        F.Array(TEXT("Array_Clear"),F.Field(R,RC,TEXT("OfferedChoiceCodes")));
        for(auto N:OfferStrings)F.Array(TEXT("Array_Clear"),F.Field(R,RC,N));

        auto* Loop=F.Each(F.Get(TEXT("RewardReceipts")));
        auto* Item=FindPin(Loop,TEXT("Array Element"));
        for(auto N:ReceiptStrings)F.Append(F.Field(R,RC,RN(N)),F.Field(Item,C,N));
        for(auto N:ReceiptInts)F.Append(F.Field(R,RC,RN(N)),F.Field(Item,C,N));
        for(auto N:ReceiptBools)F.Append(F.Field(R,RC,RN(N)),F.Field(Item,C,N));
        auto* Codes=F.Field(Item,C,TEXT("OfferedChoiceCodes"));
        F.Append(F.Field(R,RC,TEXT("OfferCounts")),F.Count(Codes));
        auto* Offers=F.Each(Codes);
        auto* Index=FindPin(Offers,TEXT("Array Index"));
        F.Append(F.Field(R,RC,TEXT("OfferedChoiceCodes")),FindPin(Offers,TEXT("Array Element")));
        for(auto N:OfferStrings)F.Append(F.Field(R,RC,N),F.At(F.Field(Item,C,N),Index));
        F.Tail=FindPin(Offers,TEXT("Completed"));
        F.Tail=FindPin(Loop,TEXT("Completed"));

        auto* Save=F.Call(UGameplayStatics::StaticClass(),TEXT("SaveGameToSlot"));
        Connect(F.Cast(R,RC),FindPin(Save,TEXT("SaveGameObject")));
        FindPin(Save,TEXT("SlotName"))->DefaultValue=Slot;
        FindPin(Save,TEXT("UserIndex"))->DefaultValue=TEXT("0");
        auto* Saved=F.Cache(TEXT("Saved"),Save->GetReturnValuePin());
        auto* Read=F.Call(UGameplayStatics::StaticClass(),TEXT("LoadGameFromSlot"));
        FindPin(Read,TEXT("SlotName"))->DefaultValue=Slot;
        FindPin(Read,TEXT("UserIndex"))->DefaultValue=TEXT("0");
        auto* Back=F.Cache(TEXT("Readback"),F.Cast(Read->GetReturnValuePin(),RC));
        auto* Okay=F.Cache(TEXT("Okay"),F.Eq(F.Int(0),F.Int(1)));
        auto* Valid=F.Branch(F.IsValid(Back));
        F.Set(F.Variable(Okay),F.And(Saved,F.And(F.Eq(F.Field(R,RC,TEXT("RunId")),F.Field(Back,RC,TEXT("RunId")),TEXT("EqualEqual_StrStr")),F.Eq(F.Field(R,RC,TEXT("Revision")),F.Field(Back,RC,TEXT("Revision"))))));
        F.Merge(Valid->GetElsePin());
        auto* Failed=F.Branch(F.Not(Okay));
        F.Set(TEXT("DDPersistenceFailed"),nullptr,TEXT("true"));
        F.Set(TEXT("ManualRewardBlocked"),nullptr,TEXT("true"));
        Log(F,TEXT("run-write-or-readback-failed-drafts-blocked"));
        F.Merge(Failed->GetElsePin());
        F.Return(Okay);
        F.Compile();

    }
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDRestoreRun"),Master->GeneratedClass);
        auto* R=F.Get(TEXT("DDRun"));
        F.QuietRequire(F.IsValid(R));
        for(auto N:StringArrays)F.Set(N,F.Field(R,RC,N));
        for(auto N:IntArrays)F.Set(N,F.Field(R,RC,N));
        for(auto N:Bools)F.Set(N,F.Field(R,RC,N));
        for(const TCHAR* N:{TEXT("DDStage1"),TEXT("DDStage2")})F.Set(N,F.Field(R,RC,N));
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("RewardReceipts")));
        auto* Offset=F.Cache(TEXT("Offset"),F.Int(0));

        auto* Loop=F.Each(F.Field(R,RC,TEXT("ReceiptKey")));
        auto* I=FindPin(Loop,TEXT("Array Index"));
        auto* Item=F.Cache(TEXT("Receipt"),F.New(C));
        for(auto N:ReceiptStrings)F.Put(Item,C,N,F.At(F.Field(R,RC,RN(N)),I));
        for(auto N:ReceiptInts)F.Put(Item,C,N,F.At(F.Field(R,RC,RN(N)),I));
        for(auto N:ReceiptBools)F.Put(Item,C,N,F.At(F.Field(R,RC,RN(N)),I));

        // An accepted in-flight native operation cannot be resumed from disk.
        // Only successful published slot codes get fresh normal replay tuples.
        auto* Pending=F.Branch(F.Eq(F.Field(Item,C,TEXT("State")),F.Int(2)));
        F.Put(Item,C,TEXT("State"),F.Int(6));
        F.Put(Item,C,TEXT("RoundSettled"),nullptr,TEXT("true"));
        F.Put(Item,C,TEXT("SupplyResult"),F.Str(TEXT("world-travel-unsettled-native-not-reissued")));
        F.Merge(Pending->GetElsePin());
        F.Append(F.Get(TEXT("RewardReceipts")),Item);

        auto* Offers=F.Each(F.Field(R,RC,TEXT("OfferedChoiceCodes")));
        auto* O=FindPin(Offers,TEXT("Array Index"));
        auto* Count=F.At(F.Field(R,RC,TEXT("OfferCounts")),I);
        auto* In=F.Branch(F.And(F.Not(F.Greater(Offset,O)),F.Greater(F.Math(TEXT("Add_IntInt"),Offset,Count),O)));
        F.Append(F.Field(Item,C,TEXT("OfferedChoiceCodes")),FindPin(Offers,TEXT("Array Element")));
        for(auto N:OfferStrings)F.Append(F.Field(Item,C,N),F.At(F.Field(R,RC,N),O));
        F.Merge(In->GetElsePin());
        F.Tail=FindPin(Offers,TEXT("Completed"));
        F.Set(F.Variable(Offset),F.Math(TEXT("Add_IntInt"),Offset,Count));
        F.Tail=FindPin(Loop,TEXT("Completed"));
        F.Done();
        F.Compile();

    }
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDEnsureWorld"),Master->GeneratedClass);
        F.QuietRequire(F.Not(F.Get(TEXT("DDRunRetired"))));
        auto* Bound=F.Branch(F.Get(TEXT("DDWorldBound")));
        F.Return(nullptr,TEXT("true"));
        F.Tail=Bound->GetElsePin();
        F.QuietRequire(DORLocalHost(F));
        F.QuietRequire(F.Not(F.Get(TEXT("DDWorldRejected"))));
        F.QuietRequire(F.Not(F.Get(TEXT("DDLoading"))));
        auto* GS=F.Cache(TEXT("GameState"),F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"))->GetReturnValuePin());
        F.QuietRequire(F.IsValid(GS));
        F.QuietRequire(F.Not(F.Field(GS,AFSDGameState::StaticClass(),TEXT("IsOnSpaceRig"))));
        auto* GM=Mission(F);
        F.QuietRequire(F.IsValid(GM));
        F.QuietRequire(F.Read(UGeneratedMission::StaticClass(),TEXT("IsDeepDive"),GM));
        auto NeedEntry=[&](UEdGraphPin* Condition,UEdGraphPin* State){auto* B=F.Branch(Condition);auto* Good=F.Tail;F.Tail=B->GetElsePin();auto* Note=F.Self(TEXT("DOR_RecordStartingDoorState"));Connect(State,FindPin(Note,TEXT("State")));F.Self(TEXT("DOR_OrdinaryFlushChanged"));F.Return();F.Tail=Good;};
        auto* M=Manager(F);
        NeedEntry(F.IsValid(M),F.Str(TEXT("dd-native-manager-not-ready")));
        auto* Stage=F.Cache(TEXT("Stage"),F.Read(DC,TEXT("GetCurrentDeepDiveStage"),M));
        NeedEntry(F.And(F.Not(F.Greater(F.Int(0),Stage)),F.Not(F.Greater(Stage,F.Int(2)))),F.Join(F.Str(TEXT("dd-native-stage-not-ready:")),F.Text(Stage,TEXT("Conv_IntToString"))));
        F.Set(TEXT("DDLoading"),nullptr,TEXT("true"));
        F.Set(TEXT("DDEntryStage"),Stage);

        auto* Fresh=F.Branch(F.Eq(Stage,F.Int(0)));
        auto* LoadPrefs=F.Self(TEXT("DOR_LoadPreferences"));
        auto* HavePrefs=F.Branch(LoadPrefs->GetReturnValuePin());
        auto* Create=F.Call(UGameplayStatics::StaticClass(),TEXT("CreateSaveGameObject"));
        FindPin(Create,TEXT("SaveGameClass"))->DefaultObject=RC;
        F.Set(TEXT("DDRun"),F.Cast(Create->GetReturnValuePin(),RC));
        auto* P=F.Get(TEXT("LocalPreferences"));
        F.Set(TEXT("ActiveModEnabled"),F.Field(P,Preferences->GeneratedClass,TEXT("Enabled")));
        F.Set(TEXT("DDMode"),F.Field(P,Preferences->GeneratedClass,TEXT("DeepDive")));
        F.Set(TEXT("ActiveHideHighLoad"),F.Field(P,Preferences->GeneratedClass,TEXT("HideHighLoad")));
        F.Set(TEXT("DDStage1"),ClampStage(F,F.Field(P,Preferences->GeneratedClass,TEXT("DDStage1"))));
        F.Set(TEXT("DDStage2"),F.Math(TEXT("Max"),F.Get(TEXT("DDStage1")),ClampStage(F,F.Field(P,Preferences->GeneratedClass,TEXT("DDStage2")))));
        F.Put(F.Get(TEXT("DDRun")),RC,TEXT("RunId"),F.Text(F.Call(UKismetGuidLibrary::StaticClass(),TEXT("NewGuid"))->GetReturnValuePin(),TEXT("Conv_GuidToString")));
        auto* NewPath=F.Tail;
        F.Tail=HavePrefs->GetElsePin();
        F.Set(TEXT("DDLoading"),nullptr,TEXT("false"));
        F.Return();
        F.Tail=Fresh->GetElsePin();

        auto* Load=F.Call(UGameplayStatics::StaticClass(),TEXT("LoadGameFromSlot"));
        FindPin(Load,TEXT("SlotName"))->DefaultValue=Slot;
        FindPin(Load,TEXT("UserIndex"))->DefaultValue=TEXT("0");
        F.Set(TEXT("DDRun"),F.Cast(Load->GetReturnValuePin(),RC));
        auto* Valid=F.Branch(F.IsValid(F.Get(TEXT("DDRun"))));
        auto* Previous=F.Field(F.Get(TEXT("DDRun")),RC,TEXT("EntryStage"));
        auto* Compatible=F.Branch(F.And(F.Eq(F.Field(F.Get(TEXT("DDRun")),RC,TEXT("Schema")),F.Int(1)),F.Or(F.Eq(Previous,Stage),F.Eq(F.Math(TEXT("Add_IntInt"),Previous,F.Int(1)),Stage))));
        F.Self(TEXT("DOR_DDRestoreRun"));
        auto* Restored=F.Tail;

        F.Tail=Compatible->GetElsePin();
        F.Set(TEXT("DDLoading"),nullptr,TEXT("false"));
        F.Note(TEXT("dd-run-load-rejected-stage-or-schema"));
        F.Set(TEXT("DDWorldRejected"),nullptr,TEXT("true"));
        F.Self(TEXT("DOR_DiagnosticFlush"));
        F.Return();
        F.Tail=Valid->GetElsePin();
        F.Set(TEXT("DDLoading"),nullptr,TEXT("false"));
        F.Note(TEXT("dd-run-load-missing-later-stage-no-new-run"));
        F.Set(TEXT("DDWorldRejected"),nullptr,TEXT("true"));
        F.Self(TEXT("DOR_DiagnosticFlush"));
        F.Return();
        F.Tail=Restored;
        F.Merge(NewPath);

        F.Set(TEXT("ActiveWave"),nullptr,TEXT("false"));
        F.Set(TEXT("OrdinarySnapshotReady"),nullptr,TEXT("true"));
        F.Set(TEXT("DDWorldBound"),nullptr,TEXT("true"));
        F.Set(TEXT("DDLoading"),nullptr,TEXT("false"));
        F.Self(TEXT("DOR_DDSaveRun"));
        Log(F,TEXT("world-entry-settings-bound"));
        F.Return(nullptr,TEXT("true"));
        F.Compile();

    }
    // SnapshotOrdinarySettings routes DD to the run adapter, without enabling
    // ordinary objective initialization or its secondary-objective source.
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_IsDeepDiveWorld"),Master->GeneratedClass);
        auto* GM=Mission(F);
        F.QuietRequire(F.IsValid(GM));
        F.Return(F.Read(UGeneratedMission::StaticClass(),TEXT("IsDeepDive"),GM));
        F.Compile();

        auto* G=FindFunctionGraph(Master,TEXT("DOR_SnapshotOrdinarySettings"));
        auto* E=FindFunctionEntry(G);
        auto* Out=E->FindPin(UEdGraphSchema_K2::PN_Then);
        auto Next=Out->LinkedTo;
        Out->BreakAllPinLinks();
        auto* Is=AddSelfCall(G,TEXT("DOR_IsDeepDiveWorld"),0,0);
        Connect(Out,Is->GetExecPin());
        auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);
        B->AllocateDefaultPins();
        Connect(Is->GetThenPin(),B->GetExecPin());
        Connect(Is->GetReturnValuePin(),B->GetConditionPin());
        for(auto* P:Next)Connect(B->GetElsePin(),P);
        auto* Ensure=AddSelfCall(G,TEXT("DOR_DDEnsureWorld"),0,0);
        Connect(B->GetThenPin(),Ensure->GetExecPin());
        auto* Result=AddNode<UK2Node_FunctionResult>(G,0,0);
        Result->FunctionReference=E->FunctionReference;
        Result->AllocateDefaultPins();
        if(!Result->FindPin(TEXT("ReturnValue"))){auto* ReturnPin=Result->CreateUserDefinedPin(TEXT("ReturnValue"),MakeBoolPinType(),EGPD_Input);const FName Old=ReturnPin->PinName;ReturnPin->PinName=TEXT("ReturnValue");for(auto& Info:Result->UserDefinedPins)if(Info->PinName==Old)Info->PinName=TEXT("ReturnValue");}
        Connect(Ensure->GetThenPin(),Result->FindPin(UEdGraphSchema_K2::PN_Execute));
        Connect(Ensure->GetReturnValuePin(),Result->FindPin(TEXT("ReturnValue")));

        auto* Carry=FindFunctionGraph(Master,TEXT("DOR_CheckCarriedManualRewards"));
        DOROrdinaryInsert(FindFunctionEntry(Carry)->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_DDEnsureWorld"));

    }
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDCapabilityObserved"),Master->GeneratedClass);
        F.ResultType=MakeIntPinType();
        auto* P=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));
        auto* Cap=F.Input(TEXT("Capability"),MakeIntPinType());
        auto* DD=F.Branch(F.Get(TEXT("DDWorldBound")));
        auto* DDPath=F.Tail;
        F.Tail=DD->GetElsePin();
        F.Return(Cap);
        F.Tail=DDPath;
        auto ID=DORPlayerIdentity(F,P);
        auto* Ready=F.Branch(ID.Ready);
        F.Tail=Ready->GetElsePin();
        F.Return(Cap);
        F.Tail=Ready->GetThenPin();
        auto* I=F.Cache(TEXT("Index"),F.Find(F.Get(TEXT("DDRosterKeys")),ID.Key));
        auto* Missing=F.Branch(F.Eq(I,F.Int(-1)));
        auto* First=F.Branch(F.Eq(F.Get(TEXT("DDEntryStage")),F.Int(0)));
        F.Append(F.Get(TEXT("DDRosterKeys")),ID.Key);
        F.Append(F.Get(TEXT("DDRosterCaps")),F.Int(0));
        F.Set(F.Variable(I),F.Find(F.Get(TEXT("DDRosterKeys")),ID.Key));
        F.Self(TEXT("DOR_DDSaveRun"));
        auto* Added=F.Tail;
        F.Tail=First->GetElsePin();
        F.Return(F.Int(2));
        F.Tail=Added;
        F.Merge(Missing->GetElsePin());
        auto* Saved=F.Cache(TEXT("SavedCapability"),F.At(F.Get(TEXT("DDRosterCaps")),I));
        auto* Observed=F.Cache(TEXT("ObservedCapability"),Cap);
        // Current connection capability is independent of a temporary Pawn or
        // equipment-readiness delay. Persistent earned-reward records stay separate.
        auto* S=F.Self(TEXT("DOR_GetSafety"));auto* SO=F.Cache(TEXT("CapabilitySafety"),S->GetReturnValuePin());
        auto* HaveSafety=F.Branch(F.IsValid(SO));auto* Connection=F.Call(Safety->GeneratedClass,TEXT("DOR_ConnectionStatus"));Connect(SO,FindPin(Connection,UEdGraphSchema_K2::PN_Self));Connect(F.Read(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"),P),FindPin(Connection,TEXT("PlayerState")));auto* CurrentCap=F.Cache(TEXT("CurrentConnectionCap"),Connection->GetReturnValuePin());auto* Confirmed=F.Branch(F.Or(F.Eq(CurrentCap,F.Int(1)),F.Eq(CurrentCap,F.Int(2))));F.Set(F.Variable(Observed),CurrentCap);F.Merge(Confirmed->GetElsePin());F.Merge(HaveSafety->GetElsePin());
        auto* Settled=F.Branch(F.And(F.Eq(Saved,F.Int(0)),F.And(F.Greater(Observed,F.Int(0)),F.Not(F.Greater(Observed,F.Int(3))))));
        SetAt(F,F.Get(TEXT("DDRosterCaps")),I,Observed);
        F.Set(F.Variable(Saved),Observed);
        F.Self(TEXT("DOR_DDSaveRun"));
        F.Merge(Settled->GetElsePin());
        // Persisted run eligibility cannot authorize a fresh connection or
        // override current disabled/unready/absent operation state.
        auto* NativeReady=F.Branch(F.Eq(Cap,F.Int(1)));
        NativeReady->NodeComment=TEXT("DOR160-CURRENT-CONNECTION-READY");
        auto* Known=F.Branch(F.Greater(Saved,F.Int(0)));
        F.Return(Saved);
        F.Tail=Known->GetElsePin();
        F.Merge(NativeReady->GetElsePin());
        F.Return(Cap);
        F.Compile();

        auto* G=FindFunctionGraph(Master,TEXT("DOR_RewardCapability"));
        for(auto* N:TArray<UEdGraphNode*>(G->Nodes))if(auto* R=Cast<UK2Node_FunctionResult>(N)){auto* Exec=R->FindPin(UEdGraphSchema_K2::PN_Execute);
            if(!Exec||Exec->LinkedTo.Num()==0)continue;
            auto* Value=R->FindPin(TEXT("ReturnValue"));
            auto Prev=Exec->LinkedTo;
            Exec->BreakAllPinLinks();
            auto* Call=AddSelfCall(G,TEXT("DOR_DDCapabilityObserved"),0,0);
            for(auto* Incoming:Prev)Connect(Incoming,Call->GetExecPin());
            Connect(FindFunctionEntry(G)->FindPin(TEXT("Player")),FindPin(Call,TEXT("Player")));
            if(Value->LinkedTo.Num())Connect(Value->LinkedTo[0],FindPin(Call,TEXT("Capability")));
            else FindPin(Call,TEXT("Capability"))->DefaultValue=Value->DefaultValue;
            Value->BreakAllPinLinks();
            Connect(Call->GetReturnValuePin(),Value);
            Connect(Call->GetThenPin(),Exec);
        }
    }
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDCatchupAllowed"),Master->GeneratedClass);
        auto* P=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));
        auto* R=F.Input(TEXT("Receipt"),MakeObjectPinType(C));
        auto* DD=F.Branch(F.Get(TEXT("DDWorldBound")));
        auto* Path=F.Tail;
        F.Tail=DD->GetElsePin();
        F.Return(nullptr,TEXT("true"));
        F.Tail=Path;
        F.QuietRequire(F.IsValid(R));
        auto ID=DORPlayerIdentity(F,P);
        F.QuietRequire(F.Contains(F.Get(TEXT("DDRosterKeys")),ID.Key));
        F.QuietRequire(F.Or(F.Field(R,C,TEXT("OfferStored")),F.And(F.Field(R,C,TEXT("DDOriginalRecipient")),F.Eq(F.Field(R,C,TEXT("State")),F.Int(0)))));
        auto* Key=F.Field(R,C,TEXT("Opportunity"));
        auto* Stage=F.Math(TEXT("Add_IntInt"),F.Get(TEXT("DDEntryStage")),F.Int(1));
        auto* One=F.And(F.Eq(Key,F.Str(TEXT("dd:reward1")),TEXT("EqualEqual_StrStr")),F.Eq(Stage,F.Get(TEXT("DDStage1"))));
        auto* Two=F.And(F.Eq(Key,F.Str(TEXT("dd:reward2")),TEXT("EqualEqual_StrStr")),F.Eq(Stage,F.Get(TEXT("DDStage2"))));
        F.Return(F.Or(One,Two));
        F.Compile();

        auto* G=FindFunctionGraph(Master,TEXT("DOR_OrdinarySchedule"));
        for(auto* N:TArray<UEdGraphNode*>(G->Nodes))if(auto* A=Cast<UK2Node_CallFunction>(N))if(A->FunctionReference.GetMemberName()==TEXT("DOR_OrdinaryArrivalReady")){auto Prev=A->GetExecPin()->LinkedTo;
            A->GetExecPin()->BreakAllPinLinks();
            auto* Call=AddSelfCall(G,TEXT("DOR_DDCatchupAllowed"),0,0);
            for(auto* Incoming:Prev)Connect(Incoming,Call->GetExecPin());
            for(const TCHAR* Name:{TEXT("Player"),TEXT("Receipt")})Connect(FindPin(A,Name)->LinkedTo[0],FindPin(Call,Name));
            auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);
            B->AllocateDefaultPins();
            Connect(Call->GetThenPin(),B->GetExecPin());
            Connect(Call->GetReturnValuePin(),B->GetConditionPin());
            Connect(B->GetThenPin(),A->GetExecPin());
        /* False deliberately ends this foreach body: no new DD catchup or supply. */}
    }
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDMarkOriginalRecipient"),Master->GeneratedClass);
        auto* R=F.Input(TEXT("Receipt"),MakeObjectPinType(C));
        F.QuietRequire(F.Get(TEXT("DDWorldBound")));
        F.QuietRequire(F.IsValid(R));
        F.Put(R,C,TEXT("DDOriginalRecipient"),nullptr,TEXT("true"));
        F.Done();F.Compile();
        auto* G=FindFunctionGraph(Master,TEXT("DOR_OrdinaryBeginRecipient"));
        auto* Call=DOROrdinaryInsert(FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_DDMarkOriginalRecipient"));
        Connect(FindFunctionEntry(G)->FindPin(TEXT("Receipt")),FindPin(Call,TEXT("Receipt")));
    }
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDReconcile"),Master->GeneratedClass);
        F.QuietRequire(DORLocalHost(F));
        auto* Ensure=F.Self(TEXT("DOR_DDEnsureWorld"));
        F.QuietRequire(Ensure->GetReturnValuePin());
        F.QuietRequire(F.Not(F.Get(TEXT("NativeLifecycleClosed"))));
        auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));
        F.QuietRequire(F.Read(AGameState::StaticClass(),TEXT("HasMatchStarted"),GS->GetReturnValuePin()));

        // Register actual connected stable identities even before CAPE/CAPD or
        // native inventory readiness settles. Stage 0 is the sole roster source.
        auto* First=F.Branch(F.Eq(F.Get(TEXT("DDEntryStage")),F.Int(0)));
        auto* Players=F.Each(DORRoster(F));
        auto* Player=FindPin(Players,TEXT("Array Element"));
        auto ID=DORPlayerIdentity(F,Player);
        auto* Add=F.Branch(F.And(ID.Ready,F.Not(F.Contains(F.Get(TEXT("DDRosterKeys")),ID.Key))));
        F.Append(F.Get(TEXT("DDRosterKeys")),ID.Key);
        F.Append(F.Get(TEXT("DDRosterCaps")),F.Int(0));
        F.Self(TEXT("DOR_DDSaveRun"));
        F.Merge(Add->GetElsePin());
        F.Tail=FindPin(Players,TEXT("Completed"));
        F.Merge(First->GetElsePin());

        F.Self(TEXT("DOR_CheckCarriedManualRewards"));
        F.Self(TEXT("DOR_ObserveStartingDoors"));
        auto* Opening=F.Branch(F.And(F.Get(TEXT("StartingDoorsOpen")),F.Not(F.Get(TEXT("DDOpeningLogged")))));
        F.Set(TEXT("DDOpeningLogged"),nullptr,TEXT("true"));
        Log(F,TEXT("opening-latched"));
        auto* Mode=F.Branch(F.And(F.Get(TEXT("DDMode")),F.And(F.Get(TEXT("ActiveModEnabled")),F.Not(F.Get(TEXT("ManualRewardBlocked"))))));
        for(int32 I=1;
        I<=2;
        ++I){auto* Due=F.Branch(F.Eq(F.Math(TEXT("Add_IntInt"),F.Get(TEXT("DDEntryStage")),F.Int(1)),F.Get(*FString::Printf(TEXT("DDStage%d"),I))));
            const FString Key=FString::Printf(TEXT("dd:reward%d"),I);
            auto* New=F.Branch(F.Not(F.Contains(F.Get(TEXT("IssuedOpportunities")),F.Str(*Key))));
            F.Append(F.Get(TEXT("IssuedOpportunities")),F.Str(*Key));
            F.Append(F.Get(TEXT("OpportunitySources")),F.Str(*Key));
            F.Self(TEXT("DOR_DDSaveRun"));
            Log(F,I==1?TEXT("opportunity1-issued"):TEXT("opportunity2-issued"));
            F.Merge(New->GetElsePin());
            F.Merge(Due->GetElsePin());
        }F.Merge(Mode->GetElsePin());
        F.Merge(Opening->GetElsePin());
        F.Self(TEXT("DOR_ReconcilePlayerSessions"));
        F.Self(TEXT("DOR_OrdinaryFlushChanged"));
        F.Done();
        F.Compile();

    }
    // Derive DD slot consumption from checked successful publication, not an
    // attempted native application that may subsequently restore its original.
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDRefreshSuccessfulSlots"),Master->GeneratedClass);
        F.QuietRequire(F.Get(TEXT("DDWorldBound")));
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("RewardedPlayerSlotKeys")));
        auto* Loop=F.Each(F.Get(TEXT("ReplayPlayerKeys")));
        auto* Key=FindPin(Loop,TEXT("Array Element"));auto* I=FindPin(Loop,TEXT("Array Index"));
        for(int32 S=0;S<2;++S){auto* Has=F.Branch(F.Greater(F.At(F.Get(S?TEXT("ReplaySecondaryCodes"):TEXT("ReplayPrimaryCodes")),I),F.Int(0)));F.Append(F.Get(TEXT("RewardedPlayerSlotKeys")),F.Add(Key,TEXT("|"),F.Str(S?TEXT("secondary"):TEXT("primary"))));F.Merge(Has->GetElsePin());}
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Done();F.Compile();
        auto* G=FindFunctionGraph(Master,TEXT("DOR_GenerateCandidatesForActivePlayer"));
        DOROrdinaryInsert(FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_DDRefreshSuccessfulSlots"));
    }
    // The exact auxiliary run slot is independent of native recovery/profile IO.
    for(auto* DeleteOwner:{Safety,Master})
    {
        FDOROrdinaryGraph F(DeleteOwner,TEXT("DOR_DDDeleteRun"),Master->GeneratedClass);
        auto* Branch=F.Input(TEXT("Branch"),MakeStringPinType());
        auto* Existing=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(Existing,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;
        auto* M=F.Cache(TEXT("ExistingMaster"),F.Cast(Existing->GetReturnValuePin(),Master->GeneratedClass));
        auto* HaveMaster=F.Branch(F.IsValid(M));F.Put(M,Master->GeneratedClass,TEXT("DDRunRetired"),nullptr,TEXT("true"));F.Merge(HaveMaster->GetElsePin());
        auto* Exists=F.Call(UGameplayStatics::StaticClass(),TEXT("DoesSaveGameExist"));FindPin(Exists,TEXT("SlotName"))->DefaultValue=Slot;FindPin(Exists,TEXT("UserIndex"))->DefaultValue=TEXT("0");
        auto* Before=F.Cache(TEXT("Existed"),Exists->GetReturnValuePin());
        auto* Deleted=F.Cache(TEXT("Deleted"),F.Not(Before));auto* Have=F.Branch(Before);
        auto* Delete=F.Call(UGameplayStatics::StaticClass(),TEXT("DeleteGameInSlot"));FindPin(Delete,TEXT("SlotName"))->DefaultValue=Slot;FindPin(Delete,TEXT("UserIndex"))->DefaultValue=TEXT("0");F.Set(F.Variable(Deleted),Delete->GetReturnValuePin());F.Merge(Have->GetElsePin());
        auto* ExistsAfter=F.Call(UGameplayStatics::StaticClass(),TEXT("DoesSaveGameExist"));FindPin(ExistsAfter,TEXT("SlotName"))->DefaultValue=Slot;FindPin(ExistsAfter,TEXT("UserIndex"))->DefaultValue=TEXT("0");auto* After=F.Cache(TEXT("Remains"),ExistsAfter->GetReturnValuePin());
        FDORNativeProbeLine D(F,TEXT("DeleteResult"),TEXT("[DOR-DD-CLEANUP]|slot=Mods/DOR_DeepDiveRun|user=0"));D.Add(TEXT("branch"),Branch,NAME_None);D.Add(TEXT("existed"),Before);D.Add(TEXT("deleteReturned"),Deleted);D.Add(TEXT("remains"),After);auto* Level=F.Call(UGameplayStatics::StaticClass(),TEXT("GetCurrentLevelName"));D.Add(TEXT("level"),Level->GetReturnValuePin(),NAME_None);
        auto* HaveRun=F.Branch(F.IsValid(M));auto* R=F.Field(M,Master->GeneratedClass,TEXT("DDRun"));auto* ValidRun=F.Branch(F.IsValid(R));D.Add(TEXT("run"),F.Field(R,RC,TEXT("RunId")),NAME_None);F.Merge(ValidRun->GetElsePin());F.Merge(HaveRun->GetElsePin());
        auto* UGC=LoadObject<UClass>(nullptr,TEXT("/Script/SimpleUGC.UGCBlueprintLibrary"));auto* IO=LoadObject<UClass>(nullptr,TEXT("/Script/SimpleUGC.UGCSettings"));if(!UGC||!IO)return false;
        DORNormalReceipt(F,D.Line,true,true);F.Return(F.And(Deleted,F.Not(After)));F.Compile();
    }
    // Preserve stage travel snapshots and all existing native closure steps.
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DDCloseRunWorld"),Master->GeneratedClass);
        F.QuietRequire(F.Get(TEXT("DDWorldBound")));
        FDORNativeProbeLine D(F,TEXT("ExitReason"),TEXT("[DOR-DD]|stage=world-close-reason"));D.Add(TEXT("reason"),F.Get(TEXT("DDEndPlayReason")),TEXT("Conv_ByteToString"));D.Add(TEXT("run"),F.Field(F.Get(TEXT("DDRun")),RC,TEXT("RunId")),NAME_None);D.Record();
        auto* Quit=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ByteByte"));Connect(F.Get(TEXT("DDEndPlayReason")),FindPin(Quit,TEXT("A")));FindPin(Quit,TEXT("B"))->DefaultValue=FString::FromInt(EndReasonEnum->GetValueByNameString(TEXT("Quit")));
        auto* Quitting=F.Branch(Quit->GetReturnValuePin());F.Set(TEXT("DDRunRetired"),nullptr,TEXT("true"));
        auto* Delete=F.Self(TEXT("DOR_DDDeleteRun"));FindPin(Delete,TEXT("Branch"))->DefaultValue=TEXT("Quit");auto* Deleted=F.Cache(TEXT("Deleted"),Delete->GetReturnValuePin());FDORNativeProbeLine Q(F,TEXT("QuitResult"),TEXT("[DOR-DD]|stage=quit-delete-result"));Q.Add(TEXT("deleted"),Deleted);Q.Record();F.Self(TEXT("DOR_DiagnosticFlush"));F.Done();
        F.Tail=Quitting->GetElsePin();F.QuietRequire(F.Not(F.Get(TEXT("DDRunRetired"))));
        F.QuietRequire(F.Not(F.Get(TEXT("DDCloseSaved"))));
        F.Set(TEXT("DDCloseSaved"),nullptr,TEXT("true"));
        F.Self(TEXT("DOR_DDSaveRun"));
        Log(F,TEXT("world-close-scalar-snapshot"));
        F.Done();
        F.Compile();
        auto* G=FindFunctionGraph(Master,TEXT("DOR_NativeCloseLifecycle"));
        DOROrdinaryInsert(FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_DDCloseRunWorld"));
        bool Wired=false;auto* EG=FBlueprintEditorUtils::FindEventGraph(Master);for(auto* N:TArray<UEdGraphNode*>(EG->Nodes))if(auto* E=Cast<UK2Node_Event>(N))if(E->EventReference.GetMemberName()==TEXT("ReceiveEndPlay")){auto* Out=E->FindPin(UEdGraphSchema_K2::PN_Then);auto Next=Out->LinkedTo;Out->BreakAllPinLinks();auto* Set=AddVariableSet(EG,TEXT("DDEndPlayReason"),0,0);Connect(E->FindPin(TEXT("EndPlayReason")),FindPin(Set,TEXT("DDEndPlayReason")));Connect(Out,Set->GetExecPin());for(auto* P:Next)Connect(FindPin(Set,UEdGraphSchema_K2::PN_Then),P);Wired=true;}if(!Wired)return false;
    }
    {
        FDOROrdinaryGraph F(Safety,TEXT("DOR_DDDispatch"),Master->GeneratedClass);
        F.QuietRequire(DORLocalHost(F));
        auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));
        F.QuietRequire(F.IsValid(GS->GetReturnValuePin()));
        auto* ShouldClean=F.Cache(TEXT("ShouldClean"),F.Field(GS->GetReturnValuePin(),AFSDGameState::StaticClass(),TEXT("IsOnSpaceRig")));
        auto* Cave=F.Branch(F.Not(ShouldClean));
        auto* GM=Mission(F);
        F.QuietRequire(F.IsValid(GM));
        F.Set(F.Variable(ShouldClean),F.Not(F.Read(UGeneratedMission::StaticClass(),TEXT("IsDeepDive"),GM)));
        F.Merge(Cave->GetElsePin());

        auto* Cleanup=F.Branch(ShouldClean);
        F.QuietRequire(F.Not(F.Get(TEXT("DDCleanupChecked"))));
        F.Set(TEXT("DDCleanupChecked"),nullptr,TEXT("true"));
        auto* Delete=F.Self(TEXT("DOR_DDDeleteRun"));FindPin(Delete,TEXT("Branch"))->DefaultValue=TEXT("rig-or-non-dd");F.Cache(TEXT("Deleted"),Delete->GetReturnValuePin());
        F.Done();

        F.Tail=Cleanup->GetElsePin();
        F.Set(TEXT("DDCleanupChecked"),nullptr,TEXT("false"));
        auto* Find=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));
        FindPin(Find,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;
        auto* M=F.Cache(TEXT("Master"),F.Cast(Find->GetReturnValuePin(),Master->GeneratedClass));
        F.QuietRequire(F.IsValid(M));
        auto* Tick=F.Call(Master->GeneratedClass,TEXT("DOR_DDReconcile"));
        Connect(M,FindPin(Tick,UEdGraphSchema_K2::PN_Self));
        F.Done();
        F.Compile();
        auto* G=FindFunctionGraph(Safety,TEXT("DOR_BackupTick"));
        DOROrdinaryInsert(FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_DDDispatch"));

    }
    auto BeforeReturns=[&](const TCHAR* Name){auto* G=FindFunctionGraph(Master,Name);
        for(auto* N:TArray<UEdGraphNode*>(G->Nodes))if(auto* R=Cast<UK2Node_FunctionResult>(N)){auto* Exec=R->FindPin(UEdGraphSchema_K2::PN_Execute);
            auto* Value=R->FindPin(TEXT("ReturnValue"));
            if(!Exec||Exec->LinkedTo.Num()==0||(Value&&Value->LinkedTo.Num()==0&&(Value->DefaultValue==TEXT("false")||Value->DefaultValue==TEXT("0"))))continue;
            auto Prev=Exec->LinkedTo;
            Exec->BreakAllPinLinks();
            auto* Save=AddSelfCall(G,TEXT("DOR_DDSaveRun"),0,0);
            for(auto* P:Prev)Connect(P,Save->GetExecPin());
            Connect(Save->GetThenPin(),Exec);
        }};

    for(const TCHAR* N:{TEXT("DOR_OrdinaryCaptureOffer"),TEXT("DOR_OrdinaryAcceptedFusion"),TEXT("DOR_OrdinaryNativeSettled"),TEXT("DOR_OrdinaryBeginRecipient"),TEXT("DOR_NativeNotifyProducer"),TEXT("DOR_AcceptSupply"),TEXT("DOR_DeliverSupply")})BeforeReturns(N);

    auto* Latch=FindFunctionGraph(Master,TEXT("DOR_LatchManualRewards"));
    DOROrdinaryInsert(FindFunctionEntry(Latch)->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_DDEnsureWorld"));
    for(auto* N:TArray<UEdGraphNode*>(Latch->Nodes))if(auto* V=Cast<UK2Node_VariableSet>(N))if(V->VariableReference.GetMemberName()==TEXT("ManualRewardBlocked"))DOROrdinaryInsert(FindPin(V,UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_DDSaveRun"));

    for(auto* BP:{Master,Safety,Run}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if(BP->Status==BS_Error)return false;
    }
    for(const auto& V:Run->NewVariables){auto* P=RC->FindPropertyByName(V.VarName);
        if(!P||!P->HasAnyPropertyFlags(CPF_SaveGame)||P->HasAnyPropertyFlags(CPF_Net)||CastField<FObjectPropertyBase>(P))return false;
    }
    UE_LOG(LogTemp,Display,TEXT("DOR-DD-RUN generated schema=1 scalar-only=1 slot=Mods/DOR_DeepDiveRun writes=events-only scope=uninterrupted-host gameplay=awaiting-test"));
    return SaveBlueprint(Receipt)&&SaveBlueprint(Run);

}
