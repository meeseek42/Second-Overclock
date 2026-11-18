// Editor-only graph builders. All emitted operations use native game/engine
// functions; no DOREditor runtime DLL is required by the packaged mod.
struct FDORSafeGraph
{
    UBlueprint* BP;
    UEdGraph* G;
    UK2Node_FunctionEntry* Entry;
    UEdGraphPin* Tail;
    int32 Serial = 0;
    explicit FDORSafeGraph(UBlueprint* InBP, const FName Name) : BP(InBP)
    {
        G = FindFunctionGraph(BP, Name);
        if(!G) for(const auto& I:BP->ImplementedInterfaces) for(auto* Graph:I.Graphs) if(Graph->GetFName()==Name) G=Graph;
        if (G) Entry = ClearFunctionBody(G);
        else
        {
            G = FBlueprintEditorUtils::CreateNewGraph(BP, Name,
                UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
            FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, G, true, nullptr);
            Entry = FindFunctionEntry(G);
        }
        Tail = Entry->FindPin(UEdGraphSchema_K2::PN_Then);
    }
    UEdGraphPin* Input(const FName N, const FEdGraphPinType& T)
    {
        if (auto* P = Entry->FindPin(N)) return P;
        return AddFunctionInput(G, N, T);
    }
    void Run(UK2Node* N)
    {
        Connect(Tail, N->FindPin(UEdGraphSchema_K2::PN_Execute));
        Tail = N->FindPin(UEdGraphSchema_K2::PN_Then);
    }
    UK2Node_CallFunction* Call(UClass* C, const FName N)
    {
        auto* R = AddCall(G, C, N, ++Serial * 160, 0);
        if (R->FindPin(UEdGraphSchema_K2::PN_Execute)) Run(R);
        return R;
    }
    UK2Node_CallFunction* Self(const FName N)
    {
        auto* R = AddSelfCall(G, N, ++Serial * 160, 0); Run(R); return R;
    }
    UEdGraphPin* Get(const FName N)
    {
        auto* P=AddVariableGet(G, N, ++Serial * 160, 100)->GetValuePin();
        for(const auto& V:BP->NewVariables) if(V.VarName==N) ForcePinType(P,V.VarType);
        return P;
    }
    void Set(const FName N, UEdGraphPin* V, const FString& Default = TEXT(""))
    {
        auto* R = AddVariableSet(G, N, ++Serial * 160, 0);
        for(const auto& Var:BP->NewVariables) if(Var.VarName==N) ForcePinType(FindPin(R,N),Var.VarType);
        if (V) Connect(V, FindPin(R, N)); else FindPin(R, N)->DefaultValue = Default;
        Run(R);
    }
    UEdGraphPin* Cache(const TCHAR* Label, UEdGraphPin* V)
    {
        const FName N(*FString::Printf(TEXT("%s_%s_%d"), *G->GetName(), Label, ++Serial));
        auto T = V->PinType; T.bIsReference = false; T.bIsConst = false;
        AddTypedVariable(BP, N, T); Set(N, V); return Get(N);
    }
    UEdGraphPin* Field(UEdGraphPin* Obj, UClass* C, const FName N)
    {
        auto* R = AddExternalVariableGet(G, N, C, ++Serial * 160, 100);
        Connect(Obj, FindPin(R, UEdGraphSchema_K2::PN_Self)); return R->GetValuePin();
    }
    void Put(UEdGraphPin* Obj, UClass* C, const FName N, UEdGraphPin* V,
             const FString& Default = TEXT(""))
    {
        auto* R = AddExternalVariableSet(G, N, C, ++Serial * 160, 0);
        Connect(Obj, FindPin(R, UEdGraphSchema_K2::PN_Self));
        if (V) Connect(V, FindPin(R, N)); else FindPin(R, N)->DefaultValue = Default;
        Run(R);
    }
    UK2Node_BreakStruct* Break(UEdGraphPin* V, UScriptStruct* S)
    {
        auto* R = AddNode<UK2Node_BreakStruct>(G, ++Serial * 160, 180);
        R->StructType = S; R->bMadeAfterOverridePinRemoval = true; R->AllocateDefaultPins();
        for (auto* P : R->Pins) if (P->Direction == EGPD_Input && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
        { Connect(V, P); break; }
        return R;
    }
    UEdGraphPin* Patch(UEdGraphPin* V, UScriptStruct* S,
                      const TMap<FName, UEdGraphPin*>& Fields)
    {
        auto* Copy = Cache(TEXT("StructCopy"), V);
        auto* R = AddNode<UK2Node_SetFieldsInStruct>(G, ++Serial * 160, 0);
        R->StructType = S; R->bMadeAfterOverridePinRemoval = true; R->AllocateDefaultPins();
        for (auto& F : R->ShowPinForProperties) F.bShowPin = Fields.Contains(F.PropertyName);
        R->ReconstructNode(); Connect(Copy, FindPin(R, TEXT("StructRef")));
        for (const auto& F : Fields) Connect(F.Value, FindPin(R, F.Key));
        Run(R); return FindPin(R, TEXT("StructOut"));
    }
    UK2Node_IfThenElse* Branch(UEdGraphPin* Condition)
    {
        auto* R = AddNode<UK2Node_IfThenElse>(G, ++Serial * 160, 0); R->AllocateDefaultPins();
        Connect(Tail, R->GetExecPin()); Connect(Condition, R->GetConditionPin());
        Tail = R->GetThenPin(); return R;
    }
    void Return(UEdGraphPin* V = nullptr, const FString& Default = TEXT("false"))
    {
        auto* R = AddNode<UK2Node_FunctionResult>(G, ++Serial * 160, 0);
        R->FunctionReference = Entry->FunctionReference; R->AllocateDefaultPins();
        auto Type=V ? V->PinType : MakeBoolPinType();
        Type.bIsReference=false; Type.bIsConst=false;
        auto* P=R->FindPin(TEXT("ReturnValue"));
        if(!P)
        {
            P=R->CreateUserDefinedPin(TEXT("ReturnValue"),Type, EGPD_Input);
            // CreateUserDefinedPin makes names unique across the function;
            // secondary return nodes must instead share the primary signature.
            const FName Generated=P->PinName; P->PinName=TEXT("ReturnValue");
            for(auto& Info:R->UserDefinedPins) if(Info->PinName==Generated) Info->PinName=TEXT("ReturnValue");
        }
        if (V) Connect(V, P); else P->DefaultValue = Default;
        Connect(Tail, R->FindPin(UEdGraphSchema_K2::PN_Execute)); Tail = nullptr;
    }
    void Require(UEdGraphPin* Condition,const FString& FailureMessage=TEXT(""))
    {
        auto* B = Branch(Condition); Tail = B->GetElsePin();
        if(!FailureMessage.IsEmpty())Print(FailureMessage);
        else Print(FString::Printf(
            TEXT("[DOR]|event=REQUIRE|result=failed|code=DOR-E-REQUIRE-UNSPECIFIED|function=%s"),
            *G->GetName()));
        Return(); Tail = B->GetThenPin();
    }
    UK2Node_MacroInstance* Each(UEdGraphPin* Array)
    {
        auto* R = AddNode<UK2Node_MacroInstance>(G, ++Serial * 160, 0);
        R->SetMacroGraph(FindStandardMacroGraph(TEXT("ForEachLoop"))); R->AllocateDefaultPins();
        ForcePinType(FindPin(R, TEXT("Array")), Array->PinType);
        auto T = Array->PinType; T.ContainerType = EPinContainerType::None;
        T.bIsReference=false; T.bIsConst=false; R->ResolvedWildcardType=T;
        ForcePinType(FindPin(R, TEXT("Array Element")), T);
        Connect(Array, FindPin(R, TEXT("Array"))); Connect(Tail, FindPin(R, TEXT("Exec")));
        Tail = FindPin(R, TEXT("LoopBody")); return R;
    }
    UK2Node_CallArrayFunction* Array(const FName N, UEdGraphPin* V)
    {
        auto* R = AddArrayCall(G, N, ++Serial * 160, 0);
        ForcePinType(FindPin(R, TEXT("TargetArray")), V->PinType);
        auto T = V->PinType; T.ContainerType = EPinContainerType::None;
        for (const FName F : {FName(TEXT("Item")), FName(TEXT("NewItem")), FName(TEXT("ItemToFind"))})
            if (auto* P = R->FindPin(F)) ForcePinType(P, T);
        Connect(V, FindPin(R, TEXT("TargetArray")));
        if (R->FindPin(UEdGraphSchema_K2::PN_Execute)) Run(R);
        return R;
    }
    UEdGraphPin* Equal(UEdGraphPin* A, UEdGraphPin* B, const FName Fn, UClass* C)
    {
        auto* R = Call(C, Fn); Connect(A, FindPin(R, TEXT("A"))); Connect(B, FindPin(R, TEXT("B")));
        return R->GetReturnValuePin();
    }
    UEdGraphPin* IsValid(UEdGraphPin* V)
    { auto* R=Call(UKismetSystemLibrary::StaticClass(), TEXT("IsValid")); Connect(V,FindPin(R,TEXT("Object"))); return R->GetReturnValuePin(); }
    UEdGraphPin* Not(UEdGraphPin* V)
    { auto* R=Call(UKismetMathLibrary::StaticClass(),TEXT("Not_PreBool")); Connect(V,FindPin(R,TEXT("A"))); return R->GetReturnValuePin(); }
    void Print(const FString& Message)
    {
        auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("PrintString"));
        FindPin(N,TEXT("InString"))->DefaultValue=Message;
        FindPin(N,TEXT("Duration"))->DefaultValue=TEXT("12");
    }
    UEdGraphPin* SameArray(UEdGraphPin* A,UEdGraphPin* B)
    {
        auto* N=AddArrayCall(G,TEXT("Array_Identical"),++Serial*160,0);
        ForcePinType(FindPin(N,TEXT("ArrayA")),A->PinType); ForcePinType(FindPin(N,TEXT("ArrayB")),A->PinType);
        Connect(A,FindPin(N,TEXT("ArrayA"))); Connect(B,FindPin(N,TEXT("ArrayB"))); return N->GetReturnValuePin();
    }
    UEdGraphPin* SamePersistentCharacters(UEdGraphPin* A,UEdGraphPin* B)
    {
        auto* N=Self(TEXT("DOR_CharactersPersistentEqual"));
        Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();
    }
    UEdGraphPin* SaveGame()
    {
        auto* R = Call(UGameFunctionLibrary::StaticClass(), TEXT("GetFSDSaveGame"));
        Connect(SelfPin(G, 0, 700), FindPin(R, TEXT("WorldContextObject"))); return R->GetReturnValuePin();
    }
    // Validate every authored graph even when its owner compile is deferred.
    void ValidateReturnPins()
    {
        TArray<UK2Node_FunctionResult*> Results; G->GetNodesOfClass(Results);
        for(auto* R:Results) for(auto* P:R->Pins)
        {
            auto* Other=Results[0]->FindPin(P->PinName);
            if(!Other || Other->PinType!=P->PinType)
                UE_LOG(LogTemp,Warning,TEXT("DOR-RETURN %s %s type=%s obj=%s ref=%d const=%d container=%d first=%s firstObj=%s firstRef=%d firstConst=%d"),
                    *G->GetName(),*P->PinName.ToString(),*P->PinType.PinCategory.ToString(),*GetNameSafe(P->PinType.PinSubCategoryObject.Get()),P->PinType.bIsReference,P->PinType.bIsConst,int32(P->PinType.ContainerType),
                    Other?*Other->PinType.PinCategory.ToString():TEXT("missing"),Other?*GetNameSafe(Other->PinType.PinSubCategoryObject.Get()):TEXT("missing"),Other?Other->PinType.bIsReference:0,Other?Other->PinType.bIsConst:0);
        }
    }
    void Compile()
    {
        ValidateReturnPins();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
    }
};

// These helpers merge into the current structures. They never copy progression,
// cosmetics, weapon unlocks, resources, or an entire historical CharacterSave.
bool BuildDORSelectiveRestore(UBlueprint* BP)
{
    const auto Guid = MakeStructPinType(TBaseStructure<FGuid>::Get());
    const auto GuidArray = MakeStructPinType(TBaseStructure<FGuid>::Get(), true);
    const auto Selection = MakeStructPinType(FItemUpgradeSelection::StaticStruct());
    const auto Loadout = MakeStructPinType(FUpgradeLoadout::StaticStruct());
    const auto Loadouts = MakeStructPinType(FUpgradeLoadout::StaticStruct(), true);
    {
        FDORSafeGraph F(BP, TEXT("DOR_FilterOwnedGuids"));
        auto* Current = F.Input(TEXT("Current"), GuidArray);
        auto* Remove = F.Input(TEXT("Remove"), GuidArray);
        auto* Result = F.Cache(TEXT("Result"), Current);
        auto* Loop = F.Each(Remove);
        auto* Node = F.Array(TEXT("Array_RemoveItem"), Result);
        Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Node,TEXT("Item")));
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(Result); F.Compile();
    }
    {
        FDORSafeGraph F(BP, TEXT("DOR_MergeSelection"));
        auto* Current=F.Input(TEXT("Current"),Selection);
        auto* Before=F.Input(TEXT("Before"),Selection);
        auto* Ids=F.Input(TEXT("Ids"),GuidArray);
        auto* Cleanup=F.Input(TEXT("Cleanup"),MakeBoolPinType());
        auto* C=F.Break(Current,FItemUpgradeSelection::StaticStruct());
        auto* B=F.Break(Before,FItemUpgradeSelection::StaticStruct());
        auto* Has=F.Array(TEXT("Array_Contains"),Ids);
        Connect(FindPin(C,TEXT("EquippedOverclock")),FindPin(Has,TEXT("ItemToFind")));
        auto* Mode=F.Branch(Cleanup);
        // Cleanup removes only whitelisted DOR GUIDs, keeping vanilla tiers/skins.
        auto* Filter=F.Self(TEXT("DOR_FilterOwnedGuids"));
        Connect(FindPin(C,TEXT("EquippedUpgrades")),FindPin(Filter,TEXT("Current")));
        Connect(Ids,FindPin(Filter,TEXT("Remove")));
        auto* Filtered=F.Cache(TEXT("Filtered"),Filter->GetReturnValuePin());
        auto* Permanent=F.Self(TEXT("DOR_FilterOwnedGuids"));
        Connect(FindPin(C,TEXT("PermanentUpgrades")),FindPin(Permanent,TEXT("Current"))); Connect(Ids,FindPin(Permanent,TEXT("Remove")));
        auto* OC=F.Cache(TEXT("OC"),FindPin(C,TEXT("EquippedOverclock")));
        auto* OwnedOC=F.Branch(Has->GetReturnValuePin());
        auto* Empty=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("Parse_StringToGuid"));
        FindPin(Empty,TEXT("GuidString"))->DefaultValue=TEXT("00000000000000000000000000000000");
        F.Set(CastChecked<UK2Node_VariableGet>(OC->GetOwningNode())->VariableReference.GetMemberName(),FindPin(Empty,TEXT("OutGuid")));
        auto* OCTail=F.Tail; F.Tail=OwnedOC->GetElsePin();
        auto* Clean=F.Patch(Current,FItemUpgradeSelection::StaticStruct(),{
            {TEXT("EquippedOverclock"),OC}, {TEXT("PermanentUpgrades"),Permanent->GetReturnValuePin()},
            {TEXT("EquippedUpgrades"),Filtered}});
        // Patch starts with a struct cache, join before that cache executes.
        auto* CopyNode=F.G->Nodes.FindByPredicate([&](UEdGraphNode* N){return N->FindPin(UEdGraphSchema_K2::PN_Execute) && OwnedOC->GetElsePin()->LinkedTo.Contains(N->FindPin(UEdGraphSchema_K2::PN_Execute));});
        if(CopyNode) Connect(OCTail,(*CopyNode)->FindPin(UEdGraphSchema_K2::PN_Execute));
        F.Return(Clean);
        F.Tail=Mode->GetElsePin();
        auto* Branch=F.Branch(Has->GetReturnValuePin());
        F.Tail=Branch->GetElsePin(); F.Return(Current); F.Tail=Branch->GetThenPin();
        auto* Restored=F.Patch(Current,FItemUpgradeSelection::StaticStruct(),{
            {TEXT("EquippedOverclock"),FindPin(B,TEXT("EquippedOverclock"))},
            {TEXT("EquippedUpgrades"),FindPin(B,TEXT("EquippedUpgrades"))}});
        F.Return(Restored); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_MergeLoadout"));
        auto* Current=F.Input(TEXT("Current"),Loadout);
        auto* Before=F.Input(TEXT("Before"),Loadout);
        auto* Ids=F.Input(TEXT("Ids"),GuidArray);
        auto* Cleanup=F.Input(TEXT("Cleanup"),MakeBoolPinType());
        auto* C=F.Break(Current,FUpgradeLoadout::StaticStruct());
        auto* B=F.Break(Before,FUpgradeLoadout::StaticStruct());
        auto* Map=F.Cache(TEXT("Map"),FindPin(C,TEXT("Loadout")));
        auto* Keys=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Keys"));
        ForcePinType(FindPin(Keys,TEXT("TargetMap")),MakeUpgradeSelectionMapPinType());
        ForcePinType(FindPin(Keys,TEXT("Keys")),GuidArray); Connect(Map,FindPin(Keys,TEXT("TargetMap")));
        auto* Loop=F.Each(FindPin(Keys,TEXT("Keys")));
        auto Find=[&](UEdGraphPin* M){ auto* N=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"));
            ForcePinType(FindPin(N,TEXT("TargetMap")),MakeUpgradeSelectionMapPinType());
            ForcePinType(FindPin(N,TEXT("Key")),Guid); ForcePinType(FindPin(N,TEXT("Value")),Selection);
            Connect(M,FindPin(N,TEXT("TargetMap"))); Connect(FindPin(Loop,TEXT("Array Element")),FindPin(N,TEXT("Key"))); return N; };
        auto* CV=Find(Map); auto* BV=Find(FindPin(B,TEXT("Loadout")));
        auto* Merge=F.Self(TEXT("DOR_MergeSelection"));
        Connect(FindPin(CV,TEXT("Value")),FindPin(Merge,TEXT("Current")));
        Connect(FindPin(BV,TEXT("Value")),FindPin(Merge,TEXT("Before")));
        auto* Missing=F.Not(BV->GetReturnValuePin());
        auto* CleanMissing=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));
        Connect(Cleanup,FindPin(CleanMissing,TEXT("A"))); Connect(Missing,FindPin(CleanMissing,TEXT("B")));
        Connect(Ids,FindPin(Merge,TEXT("Ids"))); Connect(CleanMissing->GetReturnValuePin(),FindPin(Merge,TEXT("Cleanup")));
        auto* Add=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Add"));
        ForcePinType(FindPin(Add,TEXT("TargetMap")),MakeUpgradeSelectionMapPinType());
        ForcePinType(FindPin(Add,TEXT("Key")),Guid); ForcePinType(FindPin(Add,TEXT("Value")),Selection);
        Connect(Map,FindPin(Add,TEXT("TargetMap"))); Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Add,TEXT("Key")));
        Connect(Merge->GetReturnValuePin(),FindPin(Add,TEXT("Value")));
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(F.Patch(Current,FUpgradeLoadout::StaticStruct(),{{TEXT("Loadout"),Map}})); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_MergeLoadouts"));
        auto* Current=F.Input(TEXT("Current"),Loadouts); auto* Before=F.Input(TEXT("Before"),Loadouts);
        auto* Ids=F.Input(TEXT("Ids"),GuidArray); auto* Cleanup=F.Input(TEXT("Cleanup"),MakeBoolPinType());
        auto* Out=F.Cache(TEXT("Result"),Current); auto* Loop=F.Each(Current);
        auto* OldValue=F.Cache(TEXT("OldValue"),FindPin(Loop,TEXT("Array Element")));
        auto* HasOld=F.Array(TEXT("Array_IsValidIndex"),Before); Connect(FindPin(Loop,TEXT("Array Index")),FindPin(HasOld,TEXT("IndexToTest")));
        auto* Present=F.Branch(HasOld->GetReturnValuePin());
        auto* Old=F.Array(TEXT("Array_Get"),Before); Connect(FindPin(Loop,TEXT("Array Index")),FindPin(Old,TEXT("Index")));
        F.Set(CastChecked<UK2Node_VariableGet>(OldValue->GetOwningNode())->VariableReference.GetMemberName(),FindPin(Old,TEXT("Item")));
        auto* Merge=F.Self(TEXT("DOR_MergeLoadout"));
        Connect(Present->GetElsePin(),Merge->GetExecPin());
        Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Merge,TEXT("Current")));
        Connect(OldValue,FindPin(Merge,TEXT("Before")));
        auto* CleanMissing=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));
        Connect(Cleanup,FindPin(CleanMissing,TEXT("A"))); Connect(F.Not(HasOld->GetReturnValuePin()),FindPin(CleanMissing,TEXT("B")));
        Connect(Ids,FindPin(Merge,TEXT("Ids"))); Connect(CleanMissing->GetReturnValuePin(),FindPin(Merge,TEXT("Cleanup")));
        auto* Set=F.Array(TEXT("Array_Set"),Out); Connect(FindPin(Loop,TEXT("Array Index")),FindPin(Set,TEXT("Index")));
        Connect(Merge->GetReturnValuePin(),FindPin(Set,TEXT("Item")));
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(Out); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_MergeCharacters"));
        const auto Characters=MakeStructPinType(FCharacterSave::StaticStruct(),true);
        auto* Current=F.Input(TEXT("Current"),Characters); auto* Before=F.Input(TEXT("Before"),Characters);
        auto* Ids=F.Input(TEXT("Ids"),GuidArray); auto* Cleanup=F.Input(TEXT("Cleanup"),MakeBoolPinType());
        auto* Out=F.Cache(TEXT("Result"),Current); auto* Loop=F.Each(Current);
        auto* Character=F.Cache(TEXT("Character"),FindPin(Loop,TEXT("Array Element")));
        auto* CB=F.Break(Character,FCharacterSave::StaticStruct());
        // Identity match, not array position: other characters may have been added.
        auto* Old=F.Cache(TEXT("OldCharacter"),Character);
        const FName FoundName(TEXT("DOR_MergeCharacters_Found")); AddTypedVariable(BP,FoundName,MakeBoolPinType()); F.Set(FoundName,nullptr,TEXT("false"));
        auto* OldLoop=F.Each(Before); auto* OB=F.Break(FindPin(OldLoop,TEXT("Array Element")),FCharacterSave::StaticStruct());
        auto* Same=F.Equal(FindPin(CB,TEXT("SavegameID")),FindPin(OB,TEXT("SavegameID")),
            TEXT("EqualEqual_GuidGuid"),UKismetGuidLibrary::StaticClass());
        F.Branch(Same);
        auto* OldVar=Cast<UK2Node_VariableGet>(Old->GetOwningNode());
        F.Set(OldVar->VariableReference.GetMemberName(),FindPin(OldLoop,TEXT("Array Element")));
        F.Set(FoundName,nullptr,TEXT("true"));
        F.Tail=FindPin(OldLoop,TEXT("Completed"));
        auto* CleanMissing=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));
        Connect(Cleanup,FindPin(CleanMissing,TEXT("A"))); Connect(F.Not(F.Get(FoundName)),FindPin(CleanMissing,TEXT("B")));
        auto* OldBreak=F.Break(Old,FCharacterSave::StaticStruct());
        auto* Merge=F.Self(TEXT("DOR_MergeLoadouts"));
        Connect(FindPin(CB,TEXT("ItemUpgradeLoadouts")),FindPin(Merge,TEXT("Current")));
        Connect(FindPin(OldBreak,TEXT("ItemUpgradeLoadouts")),FindPin(Merge,TEXT("Before")));
        Connect(Ids,FindPin(Merge,TEXT("Ids"))); Connect(CleanMissing->GetReturnValuePin(),FindPin(Merge,TEXT("Cleanup")));
        auto* UpgradeOut=F.Cache(TEXT("UpgradeOut"),Merge->GetReturnValuePin());
        auto* Random=F.Self(TEXT("DOR_MergeLoadout"));
        Connect(FindPin(CB,TEXT("RandomItemUpgradeLoadouts")),FindPin(Random,TEXT("Current")));
        Connect(FindPin(OldBreak,TEXT("RandomItemUpgradeLoadouts")),FindPin(Random,TEXT("Before")));
        Connect(Ids,FindPin(Random,TEXT("Ids"))); Connect(CleanMissing->GetReturnValuePin(),FindPin(Random,TEXT("Cleanup")));
        auto* NewCharacter=F.Patch(Character,FCharacterSave::StaticStruct(),{
            {TEXT("ItemUpgradeLoadouts"),UpgradeOut}, {TEXT("RandomItemUpgradeLoadouts"),Random->GetReturnValuePin()}});
        auto* Set=F.Array(TEXT("Array_Set"),Out); Connect(FindPin(Loop,TEXT("Array Index")),FindPin(Set,TEXT("Index")));
        Connect(NewCharacter,FindPin(Set,TEXT("Item")));
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(Out); F.Compile();
    }
    return BP->Status != BS_Error;
}

UBlueprint* CreateDORSafetyJournal()
{
    auto* BP=CreateObjectBlueprint(FString(RuntimeRoot)/TEXT("Blueprints/SG_DOR_SafetyJournalV2"),
        TEXT("SG_DOR_SafetyJournalV2"),USaveGame::StaticClass());
    if(!BP) return nullptr;
    for(const TCHAR* N : {TEXT("SourceSlot"),TEXT("SourceAnonymousId"),TEXT("TransactionId")})
        AddSavedTypedVariable(BP,N,MakeStringPinType());
    AddSavedTypedVariable(BP,TEXT("SourceIndex"),MakeIntPinType());
    AddSavedTypedVariable(BP,TEXT("Schema"),MakeIntPinType());
    AddSavedTypedVariable(BP,TEXT("Disabled"),MakeBoolPinType());
    AddSavedTypedVariable(BP,TEXT("BeforeCharacters"),MakeStructPinType(FCharacterSave::StaticStruct(),true));
    AddSavedTypedVariable(BP,TEXT("BeforeLoadouts"),MakeStructPinType(FUpgradeLoadout::StaticStruct(),true));
    AddSavedTypedVariable(BP,TEXT("Cleanup"),MakeBoolPinType());
    AddSavedTypedVariable(BP,TEXT("Registration"),MakeBoolPinType());
    for(const TCHAR* N : {TEXT("PairIds"),TEXT("SchematicIds"),TEXT("BeforePurchased"),TEXT("BeforeOwned"),TEXT("BeforeForged")})
        AddSavedTypedVariable(BP,N,MakeStructPinType(TBaseStructure<FGuid>::Get(),true));
    FKismetEditorUtilities::CompileBlueprint(BP); return BP;
}

UEdGraphPin* DORSafeLoad(FDORSafeGraph& F, UEdGraphPin* Slot, UClass* Class)
{
    auto* N=F.Call(UGameplayStatics::StaticClass(),TEXT("LoadGameFromSlot"));
    Connect(Slot,FindPin(N,TEXT("SlotName"))); FindPin(N,TEXT("UserIndex"))->DefaultValue=TEXT("0");
    auto* CastNode=AddNode<UK2Node_DynamicCast>(F.G,++F.Serial*160,0);
    CastNode->TargetType=Class; CastNode->SetPurity(true); CastNode->AllocateDefaultPins();
    ForcePinType(CastNode->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass()));
    Connect(N->GetReturnValuePin(),CastNode->GetCastSourcePin());
    return F.Cache(TEXT("Loaded"),CastNode->GetCastResultPin());
}
UEdGraphPin* DORSafeSave(FDORSafeGraph& F, UEdGraphPin* Obj, UEdGraphPin* Slot)
{
    auto* N=F.Call(UGameplayStatics::StaticClass(),TEXT("SaveGameToSlot"));
    Connect(Obj,FindPin(N,TEXT("SaveGameObject"))); Connect(Slot,FindPin(N,TEXT("SlotName")));
    FindPin(N,TEXT("UserIndex"))->DefaultValue=TEXT("0"); return N->GetReturnValuePin();
}
UEdGraphPin* DORSafeExists(FDORSafeGraph& F, UEdGraphPin* Slot)
{
    auto* N=F.Call(UGameplayStatics::StaticClass(),TEXT("DoesSaveGameExist"));
    Connect(Slot,FindPin(N,TEXT("SlotName"))); FindPin(N,TEXT("UserIndex"))->DefaultValue=TEXT("0");
    return N->GetReturnValuePin();
}
UEdGraphPin* DORSafeString(FDORSafeGraph& F,const FString& Value)
{
    auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));
    FindPin(N,TEXT("A"))->DefaultValue=Value; return N->GetReturnValuePin();
}
UEdGraphPin* DORSafeConcat(FDORSafeGraph& F,const FString& Prefix,UEdGraphPin* Value)
{
    auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));
    FindPin(N,TEXT("A"))->DefaultValue=Prefix; Connect(Value,FindPin(N,TEXT("B"))); return N->GetReturnValuePin();
}

bool BuildDORPersistenceComparison(UBlueprint* BP)
{
    const auto Characters=MakeStructPinType(FCharacterSave::StaticStruct(),true);
    {
        FDORSafeGraph F(BP,TEXT("DOR_NormalizeCharactersForComparison"));
        auto* Input=F.Input(TEXT("Characters"),Characters);auto* Out=F.Cache(TEXT("CharactersCopy"),Input);
        auto* MinusOne=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Subtract_IntInt"));
        FindPin(MinusOne,TEXT("B"))->DefaultValue=TEXT("1");
        auto* Loop=F.Each(Input);auto* Character=F.Cache(TEXT("CharacterCopy"),FindPin(Loop,TEXT("Array Element")));
        auto* CB=F.Break(Character,FCharacterSave::StaticStruct());auto* Vanity=F.Cache(TEXT("VanityCopy"),FindPin(CB,TEXT("Vanity")));
        auto* VB=F.Break(Vanity,FCharacterVanitySave::StaticStruct());auto* Loadouts=FindPin(VB,TEXT("Loadouts"));
        auto* Copies=F.Cache(TEXT("LoadoutsCopy"),Loadouts);auto* Inner=F.Each(Loadouts);
        // Patch copies rather than reconstructing known fields: native values
        // outside this confirmed transient member must remain fully comparable.
        auto* Loadout=F.Patch(FindPin(Inner,TEXT("Array Element")),FCharacterVanityLoadout::StaticStruct(),{{TEXT("MedicalGownIndex"),MinusOne->GetReturnValuePin()}});
        auto* Set=F.Array(TEXT("Array_Set"),Copies);Connect(FindPin(Inner,TEXT("Array Index")),FindPin(Set,TEXT("Index")));Connect(Loadout,FindPin(Set,TEXT("Item")));
        F.Tail=FindPin(Inner,TEXT("Completed"));
        auto* Random=F.Patch(FindPin(VB,TEXT("RandomVanityLoadout")),FCharacterVanityLoadout::StaticStruct(),{{TEXT("MedicalGownIndex"),MinusOne->GetReturnValuePin()}});
        auto* NormalizedVanity=F.Patch(Vanity,FCharacterVanitySave::StaticStruct(),{{TEXT("Loadouts"),Copies},{TEXT("RandomVanityLoadout"),Random}});
        auto* NormalizedCharacter=F.Patch(Character,FCharacterSave::StaticStruct(),{{TEXT("Vanity"),NormalizedVanity}});
        Set=F.Array(TEXT("Array_Set"),Out);Connect(FindPin(Loop,TEXT("Array Index")),FindPin(Set,TEXT("Index")));Connect(NormalizedCharacter,FindPin(Set,TEXT("Item")));
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(Out);F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_CharactersPersistentEqual"));auto* A=F.Input(TEXT("A"),Characters);auto* B=F.Input(TEXT("B"),Characters);
        auto* NA=F.Self(TEXT("DOR_NormalizeCharactersForComparison"));Connect(A,FindPin(NA,TEXT("Characters")));auto* AC=F.Cache(TEXT("NormalizedA"),NA->GetReturnValuePin());
        auto* NB=F.Self(TEXT("DOR_NormalizeCharactersForComparison"));Connect(B,FindPin(NB,TEXT("Characters")));auto* BC=F.Cache(TEXT("NormalizedB"),NB->GetReturnValuePin());
        F.Return(F.SameArray(AC,BC));F.Compile();
    }
    return BP->Status!=BS_Error;
}

bool BuildDORSafetyActor(UBlueprint* BP,UClass* JournalClass,
    const TArray<TSharedPtr<FJsonObject>>& Pairs)
{
    const auto GuidArray=MakeStructPinType(TBaseStructure<FGuid>::Get(),true);
    for(const TCHAR* N:{TEXT("DOR_Ready"),TEXT("DOR_Blocked"),TEXT("DOR_Disabled"),TEXT("DOR_Active")})
        AddTypedVariable(BP,N,MakeBoolPinType());
    for(const TCHAR* N:{TEXT("DOR_SourceSlot"),TEXT("DOR_AnonymousId"),TEXT("DOR_JournalSlot"),TEXT("DOR_BackupSlot"),TEXT("DOR_SettingsSlot"),TEXT("DOR_LastError")})
        AddTypedVariable(BP,N,MakeStringPinType());
    AddTypedVariable(BP,TEXT("DOR_SourceIndex"),MakeIntPinType());
    AddTypedVariable(BP,TEXT("DOR_SourceLiveObject"),MakeObjectPinType(UFSDSaveGame::StaticClass()));
    AddTypedVariable(BP,TEXT("DOR_Journal"),MakeObjectPinType(JournalClass));
    AddTypedVariable(BP,TEXT("DOR_AllPairIds"),GuidArray);
    AddTypedVariable(BP,TEXT("DOR_BaselinePairIds"),GuidArray);
    AddTypedVariable(BP,TEXT("DOR_AllSchematicIds"),GuidArray);
    if(!BuildDORSelectiveRestore(BP)) return false;
    if(!BuildDORPersistenceComparison(BP)) return false;
    {
        // GetSlotLoadedFrom can be empty while the live FSD save object is
        // already usable (observed after the 2026 game update). Every safety
        // path must share this resolver; otherwise backup succeeds by fallback
        // while later journal identity checks reject the exact same profile.
        FDORSafeGraph F(BP,TEXT("DOR_ResolveSourceSlot"));
        auto* Save=F.Input(TEXT("Save"),MakeObjectPinType(UFSDSaveGame::StaticClass()));
        auto* Valid=F.Branch(F.IsValid(Save));
        auto* LoadedCall=F.Call(UFSDSaveGame::StaticClass(),TEXT("GetSlotLoadedFrom"));Connect(Save,FindPin(LoadedCall,UEdGraphSchema_K2::PN_Self));
        auto* Loaded=F.Cache(TEXT("Loaded"),LoadedCall->GetReturnValuePin());
        auto* LoadedLen=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Len"));Connect(Loaded,FindPin(LoadedLen,TEXT("S")));
        auto* HasLoaded=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(LoadedLen->GetReturnValuePin(),FindPin(HasLoaded,TEXT("A")));
        auto* LoadedBranch=F.Branch(HasLoaded->GetReturnValuePin());F.Return(Loaded);
        F.Tail=LoadedBranch->GetElsePin();
        auto* Derived=F.Call(UFSDSaveGame::StaticClass(),TEXT("GetSaveSlotName"));Connect(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")),FindPin(Derived,TEXT("NewUserIdx")));
        F.Return(Derived->GetReturnValuePin());
        F.Tail=Valid->GetElsePin();F.Return(DORSafeString(F,TEXT("")));F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_CheckIdentity"));
        auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JournalClass));
        F.Require(F.IsValid(J)); auto* Save=F.Cache(TEXT("Save"),F.SaveGame()); F.Require(F.IsValid(Save));
        F.Require(F.Equal(Save,F.Get(TEXT("DOR_SourceLiveObject")),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));
        auto* Slot=F.Self(TEXT("DOR_ResolveSourceSlot"));Connect(Save,FindPin(Slot,TEXT("Save")));
        F.Require(F.Equal(Slot->GetReturnValuePin(),F.Field(J,JournalClass,TEXT("SourceSlot")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        F.Require(F.Equal(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Field(J,JournalClass,TEXT("SourceAnonymousId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        F.Require(F.Equal(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Field(J,JournalClass,TEXT("SourceIndex")),TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));
        auto* Schema=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));
        Connect(F.Field(J,JournalClass,TEXT("Schema")),FindPin(Schema,TEXT("A"))); FindPin(Schema,TEXT("B"))->DefaultValue=TEXT("2");
        F.Return(Schema->GetReturnValuePin()); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_CommitNativeSave"));
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame()); F.Require(F.IsValid(Save));
        F.Require(F.Equal(Save,F.Get(TEXT("DOR_SourceLiveObject")),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));
        // SaveToDisk is a fire-and-forget instance call. Immediate read-back can
        // therefore observe the previous file and strand a valid registration
        // journal. SaveSlotToDisk is the native synchronous writer: it accepts
        // the already identity-checked slot/index and reports the disk result.
        auto* Slot=F.Self(TEXT("DOR_ResolveSourceSlot"));Connect(Save,FindPin(Slot,TEXT("Save")));
        F.Require(F.Equal(Slot->GetReturnValuePin(),F.Get(TEXT("DOR_SourceSlot")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        auto* Write=F.Call(UFSDSaveGame::StaticClass(),TEXT("SaveSlotToDisk"));
        Connect(Save,FindPin(Write,TEXT("SaveSlot")));Connect(F.Get(TEXT("DOR_SourceSlot")),FindPin(Write,TEXT("slotName")));
        Connect(F.Get(TEXT("DOR_SourceIndex")),FindPin(Write,TEXT("NewUserIdx")));
        F.Require(Write->GetReturnValuePin(),TEXT("[DOR-SAVE] 原生同步保存返回失败；事务文件已保留，禁止继续写入或卸载。"));
        auto* Read=DORSafeLoad(F,F.Get(TEXT("DOR_SourceSlot")),UFSDSaveGame::StaticClass()); F.Require(F.IsValid(Read),TEXT("[DOR-SAVE] 原生主存档保存后无法回读，已阻止后续 DOR 写入；请保留 pak 和 DOR_TransactionV2 恢复文件并反馈日志。"));
        F.Require(F.Equal(F.SaveGame(),Save,TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));
        F.Require(F.Equal(F.Field(Read,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Get(TEXT("DOR_SourceIndex")),TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));
        F.Require(F.Equal(F.Field(Read,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Get(TEXT("DOR_AnonymousId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        // Read-back verifies the specific fields written and never restores a
        // complete save object. Equality uses native reflected struct equality.
        for(const TCHAR* Name:{TEXT("CharacterSaves"),TEXT("ItemUpgradeLoadouts"),TEXT("PurchasedItemUpgrades")})
        {
            auto* A=F.Field(Save,UFSDSaveGame::StaticClass(),Name); auto* B=F.Field(Read,UFSDSaveGame::StaticClass(),Name);
            F.Require(FString(Name)==TEXT("CharacterSaves")?F.SamePersistentCharacters(A,B):F.SameArray(A,B));
        }
        auto* AS=F.Break(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")),FSchematicSave::StaticStruct());
        auto* BS=F.Break(F.Field(Read,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")),FSchematicSave::StaticStruct());
        for(const TCHAR* Name:{TEXT("OwnedSchematics"),TEXT("ForgedSchematics")})F.Require(F.SameArray(FindPin(AS,Name),FindPin(BS,Name)));
        F.Require(F.Equal(FindPin(AS,TEXT("PendingReward")),FindPin(BS,TEXT("PendingReward")),TEXT("EqualEqual_GuidGuid"),UKismetGuidLibrary::StaticClass()));
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    // Conservative registration list used by uninstall; never infer ownership
    // from a numeric range or remove unrelated/native IDs.
    {
        FDORSafeGraph F(BP,TEXT("DOR_LoadOwnedIds"));
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("DOR_AllPairIds")));
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("DOR_BaselinePairIds")));
        for(const auto& P:Pairs)
        {
            const FString Id=P->GetStringField(TEXT("pairId"));
            const uint64 N=FCString::Strtoui64(*Id.Left(16),nullptr,16);
            const FGuid Value(uint32(N>>32),uint32(N),FCrc::StrCrc32(*(Id+TEXT("|DOR|A"))),FCrc::StrCrc32(*(Id+TEXT("|DOR|B"))));
            auto* Parse=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("Parse_StringToGuid"));
            FindPin(Parse,TEXT("GuidString"))->DefaultValue=Value.ToString();
            auto* Add=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("DOR_AllPairIds"))); Connect(FindPin(Parse,TEXT("OutGuid")),FindPin(Add,TEXT("NewItem")));
            if(DORIsSupportedPairTestIndex(P->GetIntegerField(TEXT("testIndex"))))
            {auto* Baseline=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("DOR_BaselinePairIds")));Connect(FindPin(Parse,TEXT("OutGuid")),FindPin(Baseline,TEXT("NewItem")));}
        }
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("DOR_AllSchematicIds")));
        for(const FGuid Id:{FGuid(0xD64B7266,0xC5D8D016,0x53434845,0x4D413030),
             FGuid(0xCBDB1B90,0xF7DA184B,0x53434845,0x4D413335),
             FGuid(0xA2EDC4FA,0x1D61B86F,0x53434845,0x4D413338),
             FGuid(0xCF632122,0x30811CEB,0x53434845,0x4D413338),
             FGuid(0xDB0DC6F2,0xB8B4F8ED,0x53434845,0x4D413434),
             FGuid(0xC13A775E,0x9AF6BE89,0x53434845,0x4D413237),
             FGuid(0x8D77DBB8,0xF0914479,0x53434845,0x4D413539),
            FGuid(0x79C731E7,0xDD86051C,0x53434845,0x4D413435)})
        {
            auto* Parse=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("Parse_StringToGuid"));
            FindPin(Parse,TEXT("GuidString"))->DefaultValue=Id.ToString();
            auto* Add=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("DOR_AllSchematicIds")));
            Connect(FindPin(Parse,TEXT("OutGuid")),FindPin(Add,TEXT("NewItem")));
        }
        for(const FDORPreparedPairSpec& Spec:DORCommonPreparedSpecs(false))
        {
            if(!DORIsSupportedPairTestIndex(Spec.TestIndex)) continue;
            auto* Parse=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("Parse_StringToGuid"));
            FindPin(Parse,TEXT("GuidString"))->DefaultValue=Spec.SchematicGuid.ToString();
            auto* Add=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("DOR_AllSchematicIds")));
            Connect(FindPin(Parse,TEXT("OutGuid")),FindPin(Add,TEXT("NewItem")));
        }
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    // Membership rollback is restricted to IDs owned by this transaction. Native
    // unlocks earned while the transaction was open are never overwritten.
    {
        FDORSafeGraph F(BP,TEXT("DOR_RestoreMembership"));
        auto* Current=F.Input(TEXT("Current"),GuidArray); auto* Before=F.Input(TEXT("Before"),GuidArray);
        auto* Ids=F.Input(TEXT("Ids"),GuidArray); auto* Cleanup=F.Input(TEXT("Cleanup"),MakeBoolPinType());
        auto* Filter=F.Self(TEXT("DOR_FilterOwnedGuids"));
        Connect(Current,FindPin(Filter,TEXT("Current"))); Connect(Ids,FindPin(Filter,TEXT("Remove")));
        auto* Out=F.Cache(TEXT("Result"),Filter->GetReturnValuePin());
        auto* B=F.Branch(Cleanup); F.Return(Out); F.Tail=B->GetElsePin();
        auto* Loop=F.Each(Ids); auto* Has=F.Array(TEXT("Array_Contains"),Before);
        Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Has,TEXT("ItemToFind")));
        F.Branch(Has->GetReturnValuePin()); auto* Add=F.Array(TEXT("Array_AddUnique"),Out);
        Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Add,TEXT("NewItem")));
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(Out); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_ValidateJournal"));
        auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JournalClass));
        auto* Identity=F.Self(TEXT("DOR_CheckIdentity")); Connect(J,FindPin(Identity,TEXT("Record"))); F.Require(Identity->GetReturnValuePin());
        auto* Len=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Len")); Connect(F.Field(J,JournalClass,TEXT("TransactionId")),FindPin(Len,TEXT("S")));
        auto* Positive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt")); Connect(Len->GetReturnValuePin(),FindPin(Positive,TEXT("A")));
        F.Require(Positive->GetReturnValuePin());
        const TCHAR* Retired300SchematicGuid=TEXT("D64B7266C5D8D016534348454D413030");
        for(const auto& Names:{TPair<FName,FName>(TEXT("PairIds"),TEXT("DOR_AllPairIds")),TPair<FName,FName>(TEXT("SchematicIds"),TEXT("DOR_AllSchematicIds"))})
        {
            auto* Ids=F.Field(J,JournalClass,Names.Key); auto* Length=F.Array(TEXT("Array_Length"),Ids);
            auto* NonEmpty=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt")); Connect(Length->GetReturnValuePin(),FindPin(NonEmpty,TEXT("A"))); F.Require(NonEmpty->GetReturnValuePin());
            auto* Loop=F.Each(Ids); auto* Has=F.Array(TEXT("Array_Contains"),F.Get(Names.Value));
            auto* Element=FindPin(Loop,TEXT("Array Element"));
            Connect(Element,FindPin(Has,TEXT("ItemToFind")));
            auto* Accepted=Has->GetReturnValuePin();
            if(Names.Key==TEXT("SchematicIds"))
            {
                auto* Retired=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("Parse_StringToGuid"));
                FindPin(Retired,TEXT("GuidString"))->DefaultValue=Retired300SchematicGuid;
                auto* IsRetired=F.Equal(Element,FindPin(Retired,TEXT("OutGuid")),TEXT("EqualEqual_GuidGuid"),UKismetGuidLibrary::StaticClass());
                auto* AllowRetired=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));
                Connect(Accepted,FindPin(AllowRetired,TEXT("A"))); Connect(IsRetired,FindPin(AllowRetired,TEXT("B")));
                Accepted=AllowRetired->GetReturnValuePin();
            }
            F.Require(Accepted);
            F.Tail=FindPin(Loop,TEXT("Completed"));
        }
        auto* Count=F.Array(TEXT("Array_Length"),F.Field(J,JournalClass,TEXT("BeforeCharacters")));
        auto* Any=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt")); Connect(Count->GetReturnValuePin(),FindPin(Any,TEXT("A"))); F.Require(Any->GetReturnValuePin());
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_WriteDisabledMarker"));
        F.Set(TEXT("DOR_Disabled"),nullptr,TEXT("true"));
        auto* Create=F.Call(UGameplayStatics::StaticClass(),TEXT("CreateSaveGameObject"));FindPin(Create,TEXT("SaveGameClass"))->DefaultObject=JournalClass;
        auto* CastNode=AddNode<UK2Node_DynamicCast>(F.G,0,0);CastNode->TargetType=JournalClass;CastNode->SetPurity(true);CastNode->AllocateDefaultPins();
        ForcePinType(CastNode->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass()));Connect(Create->GetReturnValuePin(),CastNode->GetCastSourcePin());
        auto* Marker=F.Cache(TEXT("Marker"),CastNode->GetCastResultPin());F.Require(F.IsValid(Marker));
        F.Put(Marker,JournalClass,TEXT("Schema"),nullptr,TEXT("2"));F.Put(Marker,JournalClass,TEXT("Disabled"),nullptr,TEXT("true"));
        F.Put(Marker,JournalClass,TEXT("SourceSlot"),F.Get(TEXT("DOR_SourceSlot")));F.Put(Marker,JournalClass,TEXT("SourceIndex"),F.Get(TEXT("DOR_SourceIndex")));
        F.Put(Marker,JournalClass,TEXT("SourceAnonymousId"),F.Get(TEXT("DOR_AnonymousId")));
        F.Require(DORSafeSave(F,Marker,F.Get(TEXT("DOR_SettingsSlot"))));auto* Read=DORSafeLoad(F,F.Get(TEXT("DOR_SettingsSlot")),JournalClass);
        auto* Check=F.Self(TEXT("DOR_CheckIdentity"));Connect(Read,FindPin(Check,TEXT("Record")));F.Require(Check->GetReturnValuePin());F.Require(F.Field(Read,JournalClass,TEXT("Disabled")));
        auto* Manual=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Replace"));Connect(F.Get(TEXT("DOR_BackupSlot")),FindPin(Manual,TEXT("SourceString")));
        FindPin(Manual,TEXT("From"))->DefaultValue=TEXT("Mods/DOR_FirstInstallBackup_");FindPin(Manual,TEXT("To"))->DefaultValue=TEXT("Mods/DOR_Manual_");
        auto* Exists=F.Branch(DORSafeExists(F,Manual->GetReturnValuePin()));
        auto* Delete=F.Call(UGameplayStatics::StaticClass(),TEXT("DeleteGameInSlot"));Connect(Manual->GetReturnValuePin(),FindPin(Delete,TEXT("SlotName")));F.Require(Delete->GetReturnValuePin());F.Require(F.Not(DORSafeExists(F,Manual->GetReturnValuePin())));
        F.Return(nullptr,TEXT("true"));F.Tail=Exists->GetElsePin();F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_RestoreJournal"));
        // Failure leaves both the journal and the write barrier in place.
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("恢复失败 [R10]：事务文件无法加载、校验或不属于当前存档；已保留文件并阻止后续写入。"));
        auto* J=DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass);
        auto* Check=F.Self(TEXT("DOR_ValidateJournal")); Connect(J,FindPin(Check,TEXT("Record"))); F.Require(Check->GetReturnValuePin(),TEXT("[DOR-SAVE] 恢复日志损坏或不属于当前存档，已阻止 DOR 写入。不会猜测恢复，也不会删除恢复文件。"));
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("恢复失败 [R20]：当前主存档对象不可用；事务文件已保留。"));
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame()); F.Require(F.IsValid(Save));
        auto* Cleanup=F.Field(J,JournalClass,TEXT("Cleanup"));
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("恢复失败 [R30]：清理事务的停用标记未能完成保存与身份回读；事务文件已保留。"));
        auto* CleanupBranch=F.Branch(Cleanup);
        auto* Disable=F.Self(TEXT("DOR_WriteDisabledMarker"));F.Require(Disable->GetReturnValuePin());auto* DisabledTail=F.Tail;
        F.Tail=CleanupBranch->GetElsePin();
        auto* MergeJoin=AddVariableSet(F.G,TEXT("DOR_Active"),0,0);FindPin(MergeJoin,TEXT("DOR_Active"))->DefaultValue=TEXT("true");F.Run(MergeJoin);Connect(DisabledTail,MergeJoin->GetExecPin());
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("恢复失败 [R40]：定向装备或本模组 GUID 清理未完成；事务文件已保留。"));
        for(const auto& Names:{TPair<FName,FName>(TEXT("CharacterSaves"),TEXT("BeforeCharacters")),TPair<FName,FName>(TEXT("ItemUpgradeLoadouts"),TEXT("BeforeLoadouts"))})
        {
            auto* Merge=F.Self(Names.Key==TEXT("CharacterSaves")?TEXT("DOR_MergeCharacters"):TEXT("DOR_MergeLoadouts"));
            Connect(F.Field(Save,UFSDSaveGame::StaticClass(),Names.Key),FindPin(Merge,TEXT("Current")));
            Connect(F.Field(J,JournalClass,Names.Value),FindPin(Merge,TEXT("Before")));
            Connect(F.Field(J,JournalClass,TEXT("PairIds")),FindPin(Merge,TEXT("Ids"))); Connect(Cleanup,FindPin(Merge,TEXT("Cleanup")));
            F.Put(Save,UFSDSaveGame::StaticClass(),Names.Key,Merge->GetReturnValuePin());
        }
        auto* Purchased=F.Self(TEXT("DOR_RestoreMembership"));
        Connect(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades")),FindPin(Purchased,TEXT("Current")));
        Connect(F.Field(J,JournalClass,TEXT("BeforePurchased")),FindPin(Purchased,TEXT("Before")));
        Connect(F.Field(J,JournalClass,TEXT("PairIds")),FindPin(Purchased,TEXT("Ids"))); Connect(Cleanup,FindPin(Purchased,TEXT("Cleanup")));
        F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades"),Purchased->GetReturnValuePin());
        auto* S=F.Cache(TEXT("Schematic"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")));
        auto* SB=F.Break(S,FSchematicSave::StaticStruct()); TMap<FName,UEdGraphPin*> Fields;
        for(const auto& Names:{TPair<FName,FName>(TEXT("OwnedSchematics"),TEXT("BeforeOwned")),TPair<FName,FName>(TEXT("ForgedSchematics"),TEXT("BeforeForged"))})
        {
            auto* Merge=F.Self(TEXT("DOR_RestoreMembership")); Connect(FindPin(SB,Names.Key),FindPin(Merge,TEXT("Current")));
            Connect(F.Field(J,JournalClass,Names.Value),FindPin(Merge,TEXT("Before")));
            Connect(F.Field(J,JournalClass,TEXT("SchematicIds")),FindPin(Merge,TEXT("Ids"))); Connect(Cleanup,FindPin(Merge,TEXT("Cleanup")));
            Fields.Add(Names.Key,F.Cache(*Names.Key.ToString(),Merge->GetReturnValuePin()));
        }
        auto* Pending=F.Cache(TEXT("PendingReward"),FindPin(SB,TEXT("PendingReward")));
        auto* HasPending=F.Array(TEXT("Array_Contains"),F.Field(J,JournalClass,TEXT("SchematicIds")));Connect(Pending,FindPin(HasPending,TEXT("ItemToFind")));
        auto* ClearPending=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));Connect(Cleanup,FindPin(ClearPending,TEXT("A")));Connect(HasPending->GetReturnValuePin(),FindPin(ClearPending,TEXT("B")));
        auto* PendingBranch=F.Branch(ClearPending->GetReturnValuePin());
        auto* Empty=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("Parse_StringToGuid"));FindPin(Empty,TEXT("GuidString"))->DefaultValue=TEXT("00000000000000000000000000000000");
        F.Set(CastChecked<UK2Node_VariableGet>(Pending->GetOwningNode())->VariableReference.GetMemberName(),FindPin(Empty,TEXT("OutGuid")));auto* PendingTail=F.Tail;F.Tail=PendingBranch->GetElsePin();
        Fields.Add(TEXT("PendingReward"),Pending);auto* Patched=F.Patch(S,FSchematicSave::StaticStruct(),Fields);
        if(PendingBranch->GetElsePin()->LinkedTo.Num())Connect(PendingTail,PendingBranch->GetElsePin()->LinkedTo[0]);
        F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave"),Patched);
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("恢复失败 [R50]：主存档同步保存或字段回读未通过；事务文件已保留，禁止卸载。"));
        auto* Commit=F.Self(TEXT("DOR_CommitNativeSave")); F.Require(Commit->GetReturnValuePin());
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("恢复失败 [R60]：主存档已回读，但事务文件未能删除或删除后仍存在；请保留 pak。"));
        auto* Delete=F.Call(UGameplayStatics::StaticClass(),TEXT("DeleteGameInSlot"));
        Connect(F.Get(TEXT("DOR_JournalSlot")),FindPin(Delete,TEXT("SlotName"))); F.Require(Delete->GetReturnValuePin());
        F.Require(F.Not(DORSafeExists(F,F.Get(TEXT("DOR_JournalSlot")))));
        F.Set(TEXT("DOR_Active"),nullptr,TEXT("false")); F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false")); F.Set(TEXT("DOR_LastError"),nullptr,TEXT(""));
        F.Print(TEXT("[DOR-SAVE] 定向恢复、保存回读和事务文件清理均已完成；其他角色进度保留。"));
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_Initialize"));
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame()); F.Require(F.IsValid(Save));
        auto* Bound=F.Branch(F.IsValid(F.Get(TEXT("DOR_SourceLiveObject"))));
        F.Require(F.Equal(Save,F.Get(TEXT("DOR_SourceLiveObject")),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));
        auto* SameObject=F.Tail;
        F.Tail=Bound->GetElsePin();
        F.Set(TEXT("DOR_SourceLiveObject"),Save);
        auto* JoinBound=AddNode<UK2Node_IfThenElse>(F.G,0,0); JoinBound->AllocateDefaultPins();
        FindPin(JoinBound,UEdGraphSchema_K2::PN_Condition)->DefaultValue=TEXT("true");
        F.Run(JoinBound); Connect(SameObject,JoinBound->GetExecPin());
        auto* SlotCall=F.Self(TEXT("DOR_ResolveSourceSlot"));Connect(Save,FindPin(SlotCall,TEXT("Save")));
        auto* Slot=F.Cache(TEXT("Slot"),SlotCall->GetReturnValuePin());
        auto* Already=F.Branch(F.Get(TEXT("DOR_Ready")));
        // A profile switch is not a licence to replay a journal on the new save.
        F.Require(F.Equal(Slot,F.Get(TEXT("DOR_SourceSlot")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        F.Require(F.Equal(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Get(TEXT("DOR_AnonymousId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        F.Require(F.Equal(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Get(TEXT("DOR_SourceIndex")),TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));
        F.Return(F.Not(F.Get(TEXT("DOR_Blocked")))); F.Tail=Already->GetElsePin();
        // A transient backup write/read failure may be retried, but only while
        // the live save still has the exact same slot and identity. A profile
        // switch can never reuse the partially initialized service.
        auto* SourceEmpty=F.Equal(F.Get(TEXT("DOR_SourceSlot")),DORSafeString(F,TEXT("")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
        auto* SameSlot=F.Equal(Slot,F.Get(TEXT("DOR_SourceSlot")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
        auto* SameAnonymous=F.Equal(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Get(TEXT("DOR_AnonymousId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
        auto* SameIndex=F.Equal(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Get(TEXT("DOR_SourceIndex")),TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass());
        auto* SameA=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));Connect(SameSlot,FindPin(SameA,TEXT("A")));Connect(SameAnonymous,FindPin(SameA,TEXT("B")));
        auto* SameB=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));Connect(SameA->GetReturnValuePin(),FindPin(SameB,TEXT("A")));Connect(SameIndex,FindPin(SameB,TEXT("B")));
        auto* RetrySafe=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(SourceEmpty,FindPin(RetrySafe,TEXT("A")));Connect(SameB->GetReturnValuePin(),FindPin(RetrySafe,TEXT("B")));
        F.Require(RetrySafe->GetReturnValuePin());
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));
        auto* Len=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Len")); Connect(Slot,FindPin(Len,TEXT("S")));
        auto* HasSlot=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt")); Connect(Len->GetReturnValuePin(),FindPin(HasSlot,TEXT("A"))); F.Require(HasSlot->GetReturnValuePin());
        F.Set(TEXT("DOR_SourceSlot"),Slot); F.Set(TEXT("DOR_AnonymousId"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")));
        F.Set(TEXT("DOR_SourceIndex"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")));
        UEdGraphPin* SafeName=Slot;
        for(const TCHAR* Bad:{TEXT("/"),TEXT("\\"),TEXT(":"),TEXT("*"),TEXT("?"),TEXT("\""),TEXT("<"),TEXT(">"),TEXT("|")})
        {
            auto* Replace=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Replace")); Connect(SafeName,FindPin(Replace,TEXT("SourceString")));
            FindPin(Replace,TEXT("From"))->DefaultValue=Bad; FindPin(Replace,TEXT("To"))->DefaultValue=TEXT("_"); SafeName=Replace->GetReturnValuePin();
        }
        SafeName=F.Cache(TEXT("SafeName"),SafeName);
        // All DOR auxiliary saves live under the game's existing mod-save
        // folder. They never belong beside pak files or native player slots.
        F.Set(TEXT("DOR_JournalSlot"),DORSafeConcat(F,TEXT("Mods/DOR_TransactionV2_"),SafeName));
        F.Set(TEXT("DOR_BackupSlot"),DORSafeConcat(F,TEXT("Mods/DOR_FirstInstallBackup_"),SafeName));
        F.Set(TEXT("DOR_SettingsSlot"),DORSafeConcat(F,TEXT("Mods/DOR_Disabled_"),SafeName));
        F.Self(TEXT("DOR_LoadOwnedIds"));
        auto* Exists=F.Branch(DORSafeExists(F,F.Get(TEXT("DOR_BackupSlot"))));
        auto* Backup=DORSafeLoad(F,F.Get(TEXT("DOR_BackupSlot")),UFSDSaveGame::StaticClass()); F.Require(F.IsValid(Backup),TEXT("[DOR-SAVE] 已有首次备份无法读取；禁止覆盖，已阻止 DOR 写入。"));
        F.Require(F.Equal(F.Field(Backup,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Get(TEXT("DOR_AnonymousId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        F.Require(F.Equal(F.Field(Backup,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Get(TEXT("DOR_SourceIndex")),TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));
        auto* ExistingTail=F.Tail; F.Tail=Exists->GetElsePin();
        F.Require(DORSafeSave(F,Save,F.Get(TEXT("DOR_BackupSlot"))),TEXT("[DOR-SAVE] 首次备份保存失败，尚未允许 DOR 修改存档。请检查磁盘空间和目录权限。"));
        auto* NewBackup=DORSafeLoad(F,F.Get(TEXT("DOR_BackupSlot")),UFSDSaveGame::StaticClass()); F.Require(F.IsValid(NewBackup));
        for(const TCHAR* Name:{TEXT("CharacterSaves"),TEXT("ItemUpgradeLoadouts"),TEXT("PurchasedItemUpgrades")})
        {
            auto* A=F.Field(Save,UFSDSaveGame::StaticClass(),Name);auto* B=F.Field(NewBackup,UFSDSaveGame::StaticClass(),Name);
            F.Require(FString(Name)==TEXT("CharacterSaves")?F.SamePersistentCharacters(A,B):F.SameArray(A,B));
        }
        F.Print(TEXT("[DOR-SAVE] 首次完整存档对象备份已保存并回读验证：Saved/SaveGames/Mods/DOR_FirstInstallBackup_*.sav；不会自动覆盖。"));
        auto* Join=F.Self(TEXT("DOR_LoadOwnedIds")); Connect(ExistingTail,FindPin(Join,UEdGraphSchema_K2::PN_Execute));
        // An identity-less legacy journal cannot be safely guessed or discarded.
        auto* LegacyRoot=DORSafeExists(F,DORSafeString(F,TEXT("DOR_PresetRecovery_v1")));
        auto* LegacyMods=DORSafeExists(F,DORSafeString(F,TEXT("Mods/DOR_PresetRecovery_v1")));
        auto* AnyLegacy=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(LegacyRoot,FindPin(AnyLegacy,TEXT("A")));Connect(LegacyMods,FindPin(AnyLegacy,TEXT("B")));
        F.Require(F.Not(AnyLegacy->GetReturnValuePin()),TEXT("[DOR-SAVE] 检测到无存档身份的旧版 DOR_PresetRecovery_v1。已阻止 DOR 写入；请保留文件，先核查来源，不自动套用或删除。"));
        auto* Pending=F.Branch(DORSafeExists(F,F.Get(TEXT("DOR_JournalSlot"))));
        auto* Restore=F.Self(TEXT("DOR_RestoreJournal")); F.Require(Restore->GetReturnValuePin()); auto* RestoredTail=F.Tail;
        F.Tail=Pending->GetElsePin(); F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));
        auto* SetReady=AddVariableSet(F.G,TEXT("DOR_Ready"),0,0); FindPin(SetReady,TEXT("DOR_Ready"))->DefaultValue=TEXT("true");
        F.Run(SetReady); Connect(RestoredTail,SetReady->GetExecPin());
        auto* Disabled=F.Branch(DORSafeExists(F,F.Get(TEXT("DOR_SettingsSlot"))));
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));
        auto* Marker=DORSafeLoad(F,F.Get(TEXT("DOR_SettingsSlot")),JournalClass); auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));
        Connect(Marker,FindPin(Identity,TEXT("Record"))); F.Require(Identity->GetReturnValuePin());
        F.Set(TEXT("DOR_Disabled"),F.Field(Marker,JournalClass,TEXT("Disabled"))); F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false")); F.Return(nullptr,TEXT("true"));
        F.Tail=Disabled->GetElsePin(); F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_BackupTick"));
        // This core tick also carries capability-ready retry. Completed
        // backups stay idle; initialization is not repeated after readiness.
        auto* B=F.Branch(F.Get(TEXT("DOR_Ready")));
        F.Tail=B->GetElsePin();F.Self(TEXT("DOR_Initialize"));F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_CanWrite")); auto* Init=F.Self(TEXT("DOR_Initialize"));
        F.Require(Init->GetReturnValuePin()); F.Require(F.Not(F.Get(TEXT("DOR_Blocked"))));
        F.Return(F.Not(F.Get(TEXT("DOR_Disabled")))); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_Prepare"));
        auto* Pair=F.Input(TEXT("PairId"),MakeStructPinType(TBaseStructure<FGuid>::Get()));
        auto* Schematic=F.Input(TEXT("SchematicId"),MakeStructPinType(TBaseStructure<FGuid>::Get()));
        auto* Cleanup=F.Input(TEXT("Cleanup"),MakeBoolPinType());
        auto* Registration=F.Input(TEXT("Registration"),MakeBoolPinType());
        auto* Init=F.Self(TEXT("DOR_Initialize")); F.Require(Init->GetReturnValuePin());
        F.Require(F.Not(F.Get(TEXT("DOR_Blocked")))); F.Require(F.Not(F.Get(TEXT("DOR_Active"))));
        auto* CleanMode=F.Branch(Cleanup); auto* CleanTail=F.Tail; F.Tail=CleanMode->GetElsePin();
        F.Require(F.Not(F.Get(TEXT("DOR_Disabled"))));
        auto* Lock=AddVariableSet(F.G,TEXT("DOR_Blocked"),0,0); FindPin(Lock,TEXT("DOR_Blocked"))->DefaultValue=TEXT("true");
        F.Run(Lock); Connect(CleanTail,Lock->GetExecPin());
        F.Require(F.Not(DORSafeExists(F,F.Get(TEXT("DOR_JournalSlot")))));
        auto* Create=F.Call(UGameplayStatics::StaticClass(),TEXT("CreateSaveGameObject")); FindPin(Create,TEXT("SaveGameClass"))->DefaultObject=JournalClass;
        auto* CastNode=AddNode<UK2Node_DynamicCast>(F.G,0,0); CastNode->TargetType=JournalClass; CastNode->SetPurity(true); CastNode->AllocateDefaultPins();
        ForcePinType(CastNode->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass()));
        Connect(Create->GetReturnValuePin(),CastNode->GetCastSourcePin()); auto* J=F.Cache(TEXT("Journal"),CastNode->GetCastResultPin()); F.Require(F.IsValid(J));
        F.Put(J,JournalClass,TEXT("Schema"),nullptr,TEXT("2")); F.Put(J,JournalClass,TEXT("Cleanup"),Cleanup);
        F.Put(J,JournalClass,TEXT("Registration"),Registration);
        F.Put(J,JournalClass,TEXT("SourceSlot"),F.Get(TEXT("DOR_SourceSlot"))); F.Put(J,JournalClass,TEXT("SourceIndex"),F.Get(TEXT("DOR_SourceIndex")));
        F.Put(J,JournalClass,TEXT("SourceAnonymousId"),F.Get(TEXT("DOR_AnonymousId")));
        auto* NewGuid=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("NewGuid")); auto* Str=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("Conv_GuidToString"));
        Connect(NewGuid->GetReturnValuePin(),FindPin(Str,TEXT("InGuid"))); F.Put(J,JournalClass,TEXT("TransactionId"),Str->GetReturnValuePin());
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame()); F.Require(F.IsValid(Save));
        for(const auto& Names:{TPair<FName,FName>(TEXT("BeforeCharacters"),TEXT("CharacterSaves")),TPair<FName,FName>(TEXT("BeforeLoadouts"),TEXT("ItemUpgradeLoadouts")),TPair<FName,FName>(TEXT("BeforePurchased"),TEXT("PurchasedItemUpgrades"))})
            F.Put(J,JournalClass,Names.Key,F.Field(Save,UFSDSaveGame::StaticClass(),Names.Value));
        auto* SB=F.Break(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")),FSchematicSave::StaticStruct());
        F.Put(J,JournalClass,TEXT("BeforeOwned"),FindPin(SB,TEXT("OwnedSchematics"))); F.Put(J,JournalClass,TEXT("BeforeForged"),FindPin(SB,TEXT("ForgedSchematics")));
        auto* Mode=F.Branch(Cleanup);
        F.Put(J,JournalClass,TEXT("PairIds"),F.Get(TEXT("DOR_AllPairIds"))); F.Put(J,JournalClass,TEXT("SchematicIds"),F.Get(TEXT("DOR_AllSchematicIds"))); auto* ModeTail=F.Tail;
        F.Tail=Mode->GetElsePin();
        auto* RegisterMode=F.Branch(Registration);
        F.Put(J,JournalClass,TEXT("PairIds"),F.Get(TEXT("DOR_BaselinePairIds"))); F.Put(J,JournalClass,TEXT("SchematicIds"),F.Get(TEXT("DOR_AllSchematicIds"))); auto* RegistrationTail=F.Tail;
        F.Tail=RegisterMode->GetElsePin();
        for(const auto& Item:{TPair<FName,UEdGraphPin*>(TEXT("PairIds"),Pair),TPair<FName,UEdGraphPin*>(TEXT("SchematicIds"),Schematic)})
        {
            auto* One=AddNode<UK2Node_MakeArray>(F.G,0,0); One->NumInputs=1; One->AllocateDefaultPins();
            ForcePinType(One->GetOutputPin(),GuidArray); ForcePinType(FindPin(One,TEXT("[0]")),Pair->PinType); Connect(Item.Value,FindPin(One,TEXT("[0]")));
            F.Put(J,JournalClass,Item.Key,One->GetOutputPin());
        }
        auto* Validate=F.Self(TEXT("DOR_ValidateJournal")); Connect(ModeTail,FindPin(Validate,UEdGraphSchema_K2::PN_Execute));
        Connect(RegistrationTail,Validate->GetExecPin());
        Connect(J,FindPin(Validate,TEXT("Record"))); F.Require(Validate->GetReturnValuePin());
        F.Require(DORSafeSave(F,J,F.Get(TEXT("DOR_JournalSlot")))); auto* Read=DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass); F.Require(F.IsValid(Read));
        auto* Identity=F.Self(TEXT("DOR_ValidateJournal")); Connect(Read,FindPin(Identity,TEXT("Record"))); F.Require(Identity->GetReturnValuePin());
        for(const TCHAR* N:{TEXT("BeforeCharacters"),TEXT("BeforeLoadouts"),TEXT("BeforePurchased"),TEXT("BeforeOwned"),TEXT("BeforeForged"),TEXT("PairIds"),TEXT("SchematicIds")})
        {
            auto* A=F.Field(J,JournalClass,N);auto* B=F.Field(Read,JournalClass,N);
            F.Require(FString(N)==TEXT("BeforeCharacters")?F.SamePersistentCharacters(A,B):F.SameArray(A,B));
        }
        F.Set(TEXT("DOR_Journal"),Read); F.Set(TEXT("DOR_Active"),nullptr,TEXT("true")); F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_PrepareRegistration"));
        auto* Prepare=F.Self(TEXT("DOR_Prepare"));FindPin(Prepare,TEXT("Registration"))->DefaultValue=TEXT("true");F.Return(Prepare->GetReturnValuePin());F.Compile();
    }
    {
        // Space-rig preregistration is runtime-confirmed and cannot be replaced
        // by the disproven Alpha39 mission-only registration. Persist only its
        // three owned qualification IDs, with write-ahead rollback on failure.
        FDORSafeGraph F(BP,TEXT("DOR_CommitRegistration"));F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));
        F.Require(F.Get(TEXT("DOR_Active")));auto* J=DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass);
        auto* Check=F.Self(TEXT("DOR_ValidateJournal"));Connect(J,FindPin(Check,TEXT("Record")));F.Require(Check->GetReturnValuePin());
        F.Require(F.Field(J,JournalClass,TEXT("Registration")));F.Require(F.Not(F.Field(J,JournalClass,TEXT("Cleanup"))));
        auto* Commit=F.Self(TEXT("DOR_CommitNativeSave"));F.Require(Commit->GetReturnValuePin());
        auto* Delete=F.Call(UGameplayStatics::StaticClass(),TEXT("DeleteGameInSlot"));Connect(F.Get(TEXT("DOR_JournalSlot")),FindPin(Delete,TEXT("SlotName")));F.Require(Delete->GetReturnValuePin());
        F.Require(F.Not(DORSafeExists(F,F.Get(TEXT("DOR_JournalSlot")))));
        F.Set(TEXT("DOR_Active"),nullptr,TEXT("false"));F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_RestoreIfActive"));
        auto* Active=F.Branch(F.Get(TEXT("DOR_Active")));
        auto* Restore=F.Self(TEXT("DOR_RestoreJournal")); F.Return(Restore->GetReturnValuePin());
        F.Tail=Active->GetElsePin(); F.Return(F.Not(F.Get(TEXT("DOR_Blocked")))); F.Compile();
    }
    {
        FDORSafeGraph F(BP,TEXT("DOR_PrepareUninstall"));
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("清理失败：存档安全服务尚未完成初始化，请留在空间站等待备份就绪后重试。"));
        auto* Init=F.Self(TEXT("DOR_Initialize")); F.Require(Init->GetReturnValuePin());
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("清理失败：当前关卡未识别为空间站；请返回空间站后重试。"));
        auto* Level=F.Call(UGameplayStatics::StaticClass(),TEXT("GetCurrentLevelName"));
        auto* Rig=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Contains"));
        Connect(Level->GetReturnValuePin(),FindPin(Rig,TEXT("SearchIn"))); FindPin(Rig,TEXT("Substring"))->DefaultValue=TEXT("Spacerig");
        FindPin(Rig,TEXT("bUseCase"))->DefaultValue=TEXT("false"); F.Require(Rig->GetReturnValuePin());
        // A checked journal is a recovery instruction, not a reason to deadlock
        // the cleanup button. Resume it first; corruption/identity mismatch still
        // fails closed inside DOR_RestoreJournal and never deletes the journal.
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("清理失败：已有事务未能安全恢复；请保留 pak 与事务文件。"));
        auto* Resume=F.Self(TEXT("DOR_RestoreIfActive"));F.Require(Resume->GetReturnValuePin());F.Require(F.Not(F.Get(TEXT("DOR_Active"))));
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("清理失败：清理事务文件未能完成写入、回读或身份校验；没有修改主存档。"));
        auto* Prepare=F.Self(TEXT("DOR_Prepare")); FindPin(Prepare,TEXT("Cleanup"))->DefaultValue=TEXT("true"); F.Require(Prepare->GetReturnValuePin());
        F.Set(TEXT("DOR_Disabled"),nullptr,TEXT("true")); F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));
        // Recovery itself writes the minimal marker and deletes only the DOR
        // manual settings, so interruption here is resumable on next launch.
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT("清理失败：定向恢复、停用标记、主存档回读或事务删除未全部完成；请保留 pak 与事务文件。"));
        auto* Restore=F.Self(TEXT("DOR_RestoreJournal")); F.Require(Restore->GetReturnValuePin());
        F.Set(TEXT("DOR_LastError"),nullptr,TEXT(""));
        F.Print(TEXT("[DOR-SAVE] 本机当前存档清理成功，DOR 已停用。请退出游戏后手动删除 DOR pak；首次备份保留。其他玩家须各自清理本机。"));
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    return BP->Status != BS_Error;
}

bool BuildDORSafetyAccess(UBlueprint* BP,UClass* SafetyClass)
{
    AddTypedVariable(BP,TEXT("DOR_SafetyRef"),MakeObjectPinType(SafetyClass));
    FDORSafeGraph F(BP,TEXT("DOR_GetSafety"));
    // Always select the current world's first live Safety; a cached duplicate
    // must not split protocol and operation state between coordinators.
    auto* Find=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass")); FindPin(Find,TEXT("ActorClass"))->DefaultObject=SafetyClass;
    auto* CastNode=AddNode<UK2Node_DynamicCast>(F.G,0,0); CastNode->TargetType=SafetyClass; CastNode->SetPurity(true); CastNode->AllocateDefaultPins();
    ForcePinType(CastNode->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass())); Connect(Find->GetReturnValuePin(),CastNode->GetCastSourcePin());
    F.Set(TEXT("DOR_SafetyRef"),CastNode->GetCastResultPin());
    auto* Found=F.Branch(F.IsValid(F.Get(TEXT("DOR_SafetyRef")))); F.Return(F.Get(TEXT("DOR_SafetyRef"))); F.Tail=Found->GetElsePin();
    auto* Spawn=AddNode<UK2Node_SpawnActorFromClass>(F.G,0,0); Spawn->AllocateDefaultPins(); Spawn->GetClassPin()->DefaultObject=SafetyClass;
    auto* Transform=F.Call(UKismetMathLibrary::StaticClass(),TEXT("MakeTransform"));
    FindPin(Transform,TEXT("Scale"))->DefaultValue=TEXT("1,1,1"); Connect(Transform->GetReturnValuePin(),FindPin(Spawn,TEXT("SpawnTransform")));
    ForcePinType(Spawn->GetResultPin(),MakeObjectPinType(SafetyClass)); F.Run(Spawn); F.Set(TEXT("DOR_SafetyRef"),Spawn->GetResultPin());
    F.Return(F.Get(TEXT("DOR_SafetyRef"))); F.Compile(); return BP->Status!=BS_Error;
}

UK2Node_CallFunction* DORSafetyCall(UEdGraph* G,UClass* SafetyClass,const FName Function,UEdGraphPin* Exec)
{
    auto* Get=AddSelfCall(G,TEXT("DOR_GetSafety"),0,0); Connect(Exec,Get->GetExecPin());
    auto* Call=AddCall(G,SafetyClass,Function,260,0); Connect(Get->GetThenPin(),Call->GetExecPin());
    Connect(Get->GetReturnValuePin(),FindPin(Call,UEdGraphSchema_K2::PN_Self)); return Call;
}

bool IntegrateDORSafety(UBlueprint* Master,UBlueprint* InitCave,UBlueprint* InitRig,UBlueprint* Guard,UBlueprint* Safety)
{
    UClass* C=Safety->GeneratedClass;
    for(auto* BP:{Master,InitCave,InitRig,Guard}) if(!BuildDORSafetyAccess(BP,C)) return false;
    // Do not run the identity-less v1 recovery guard. New guard only bootstraps
    // the per-profile safety service and therefore never restores whole records.
    {
        auto* G=FBlueprintEditorUtils::FindEventGraph(Guard); G->Nodes.Empty();
        auto* E=AddNode<UK2Node_Event>(G,0,0); E->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"),AActor::StaticClass());
        E->bOverrideFunction=true; E->AllocateDefaultPins();
        auto* Delay=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("Delay"),0,0); FindPin(Delay,TEXT("Duration"))->DefaultValue=TEXT("0.5");
        Connect(E->FindPin(UEdGraphSchema_K2::PN_Then),Delay->GetExecPin()); DORSafetyCall(G,C,TEXT("DOR_Initialize"),Delay->GetThenPin());
    }
    // Both clients and hosts initialize before cave bootstrap continues.
    {
        auto* G=FBlueprintEditorUtils::FindEventGraph(InitCave); TArray<UK2Node_Event*> Events; G->GetNodesOfClass(Events);
        for(auto* E:Events) if(E->EventReference.GetMemberName()==TEXT("ReceiveBeginPlay"))
        {
            auto* P=E->FindPin(UEdGraphSchema_K2::PN_Then); auto Old=P->LinkedTo; P->BreakAllPinLinks();
            auto* Call=DORSafetyCall(G,C,TEXT("DOR_Initialize"),P); for(auto* To:Old) Connect(Call->GetThenPin(),To);
        }
    }
    // The journal is prepared BEFORE the earliest temporary qualification write.
    auto* Apply=FindFunctionGraph(Master,TEXT("DOR_ApplyPair"));
    if(!Apply) return false;
    UClass* Savable=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SavableDataAsset"));
    int32 PreparedRoutes=0;
    for(auto* Graph:Master->FunctionGraphs) for(auto* Node:TArray<UEdGraphNode*>(Graph->Nodes))
    {
        auto* Call=Cast<UK2Node_CallFunction>(Node);
        if(!Call || Call->FunctionReference.GetMemberName()!=TEXT("DOR_BeginFusionTransientEligibility")) continue;
        auto In=Call->GetExecPin()->LinkedTo; Call->GetExecPin()->BreakAllPinLinks();
        auto* SafetyCall=DORSafetyCall(Graph,C,TEXT("DOR_Prepare"),In.Num()?In[0]:nullptr);
        auto* GetSafety=CastChecked<UK2Node_CallFunction>(SafetyCall->GetExecPin()->LinkedTo[0]->GetOwningNode());
        for(int32 I=1;I<In.Num();++I) Connect(In[I],GetSafety->GetExecPin());
        for(const auto& Pair:{TPair<FName,FName>(TEXT("PairUpgrade"),TEXT("PairId")),TPair<FName,FName>(TEXT("PairSchematic"),TEXT("SchematicId"))})
        {
            auto* Id=AddCall(Graph,Savable,TEXT("GetSavegameID"),0,0);
            auto* Input=FindPin(Call,Pair.Key); for(auto* From:Input->LinkedTo) Connect(From,FindPin(Id,UEdGraphSchema_K2::PN_Self));
            Connect(Id->GetReturnValuePin(),FindPin(SafetyCall,Pair.Value));
        }
        auto* B=AddNode<UK2Node_IfThenElse>(Graph,0,0); B->AllocateDefaultPins(); Connect(SafetyCall->GetThenPin(),B->GetExecPin());
        Connect(SafetyCall->GetReturnValuePin(),B->GetConditionPin()); Connect(B->GetThenPin(),Call->GetExecPin());
        auto* Failed=AddSelfCall(Graph,TEXT("DOR_DebugNotify"),0,0);
        FindPin(Failed,TEXT("Message"))->DefaultValue=TEXT("[DOR-SAVE] 写入已阻止：备份/存档身份/恢复日志未通过，或已执行卸载准备；没有开始新的融合事务。");
        Connect(B->GetElsePin(),Failed->GetExecPin()); ++PreparedRoutes;
    }
    // Seven legacy call sites plus one table-driven GF-01 call site.
    const int32 ExpectedPreparedRoutes=8;
    if(PreparedRoutes!=ExpectedPreparedRoutes) {UE_LOG(LogTemp,Error,TEXT("DOR safety expected %d prepared routes, got %d"),ExpectedPreparedRoutes,PreparedRoutes);return false;}
    // Eliminate the old mutable stage journal. The V2 write-ahead record remains
    // immutable until checked recovery + native save read-back + checked delete.
    {
        FDORSafeGraph F(Master,TEXT("DOR_WritePair388RecoveryStage"));
        F.Compile();
    }
    {
        FDORSafeGraph F(Master,TEXT("DOR_CleanupPair388PersistentUnlock"));
        auto* S=F.Self(TEXT("DOR_GetSafety"));auto* Clean=F.Call(C,TEXT("DOR_PrepareUninstall"));
        Connect(S->GetReturnValuePin(),FindPin(Clean,UEdGraphSchema_K2::PN_Self));
        auto* B=F.Branch(Clean->GetReturnValuePin());
        auto* Ok=F.Self(TEXT("DOR_DebugNotify"));FindPin(Ok,TEXT("Message"))->DefaultValue=TEXT("[DOR-SAVE] 定向清理并校验成功；退出游戏后可手动删除 DOR pak。");
        F.Tail=B->GetElsePin();auto* Fail=F.Self(TEXT("DOR_DebugNotify"));FindPin(Fail,TEXT("Message"))->DefaultValue=TEXT("[DOR-SAVE] 尚未清理成功。请在空间站使用 ModHub 清理，并处理任何恢复错误。");F.Compile();
    }
    // Bypass obsolete journal construction but retain accepted transaction flow,
    // readonly refresh probe, and the existing live-weapon rebuild implementation.
    for(const FName Name:{FName(TEXT("DOR_ApplyPair388PresetTransaction")),FName(TEXT("DOR_ExecutePair388PresetTransaction"))})
    {
        auto* G=FindFunctionGraph(Master,Name); if(!G) return false;
        UK2Node_CallFunction* Create=nullptr; UEdGraphPin* Next=nullptr;
        for(auto* N:G->Nodes)
        {
            if(auto* Call=Cast<UK2Node_CallFunction>(N))
            {
                if(Call->FunctionReference.GetMemberName()==TEXT("CreateSaveGameObject")) Create=Call;
                if(Name==TEXT("DOR_ApplyPair388PresetTransaction") && Call->FunctionReference.GetMemberName()==TEXT("DOR_DebugNotify") &&
                    FindPin(Call,TEXT("Message"))->DefaultValue.Contains(TEXT("[Q01]"))) Next=Call->GetExecPin();
            }
            if(Name==TEXT("DOR_ExecutePair388PresetTransaction")) if(auto* Notify=Cast<UK2Node_CallFunction>(N))
                if(Notify->FunctionReference.GetMemberName()==TEXT("DOR_DebugNotify") &&
                    FindPin(Notify,TEXT("Message"))->DefaultValue.Contains(TEXT("[P05]"))) Next=Notify->GetExecPin();
        }
        if(!Create || !Next) return false;
        // Execute used to jump back to PresetTransactionPlayer here.  That node is
        // before the snapshot/create-journal chain, so SnapshotLoadouts -> Player
        // formed a closed exec cycle and the runtime died immediately after Q10.
        // P05 is the first mutation-authorized node after the obsolete journal.
        auto Sources=Create->GetExecPin()->LinkedTo; Create->GetExecPin()->BreakAllPinLinks(); Next->BreakAllPinLinks();
        for(auto* From:Sources) Connect(From,Next);
    }
    // Qualification unwind retains runtime bank removal, but no longer restores
    // the complete Purchased/SchematicSave snapshots.
    {
        auto* G=FindFunctionGraph(Master,TEXT("DOR_EndFusionTransientEligibility"));
        UEdGraphPin* Remove=nullptr; for(auto* N:G->Nodes) if(auto* Call=Cast<UK2Node_CallFunction>(N))
            if(Call->FunctionReference.GetMemberName()==TEXT("Map_Remove")) Remove=Call->GetExecPin();
        if(!Remove) return false; Remove->BreakAllPinLinks();
        auto* Start=FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then); Start->BreakAllPinLinks();
        auto* Restore=DORSafetyCall(G,C,TEXT("DOR_RestoreIfActive"),Start);
        auto* B=AddNode<UK2Node_IfThenElse>(G,0,0); B->AllocateDefaultPins(); Connect(Restore->GetThenPin(),B->GetExecPin());
        Connect(Restore->GetReturnValuePin(),B->GetConditionPin());
        // A missing bank object when recovery happened at startup is harmless;
        // existing graph still only removes the current mission pair.
        auto* Active=AddVariableGet(G,TEXT("TransientFusionEligibilityActive"),0,0);
        auto* ActiveB=AddNode<UK2Node_IfThenElse>(G,0,0); ActiveB->AllocateDefaultPins(); Connect(B->GetThenPin(),ActiveB->GetExecPin());
        Connect(Active->GetValuePin(),ActiveB->GetConditionPin()); Connect(ActiveB->GetThenPin(),Remove);
        for(auto* N:TArray<UEdGraphNode*>(G->Nodes)) if(auto* Set=Cast<UK2Node_VariableSet>(N))
            if(Set->VariableReference.GetMemberName()==TEXT("PurchasedItemUpgrades") || Set->VariableReference.GetMemberName()==TEXT("SchematicSave"))
            {Set->BreakAllNodeLinks(); G->RemoveNode(Set);}
    }
    {
        auto* G=FindFunctionGraph(Master,TEXT("DOR_RestorePair388ExternalPreset"));
        auto* Start=FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then); Start->BreakAllPinLinks();
        UK2Node_CallFunction* Notify=nullptr;
        for(auto* N:G->Nodes) if(auto* Call=Cast<UK2Node_CallFunction>(N))
            if(Call->FunctionReference.GetMemberName()==TEXT("DOR_DebugNotify") && FindPin(Call,TEXT("Message"))->DefaultValue.Contains(TEXT("[R85]"))) Notify=Call;
        if(!Notify) return false; Notify->GetExecPin()->BreakAllPinLinks();
        auto* End=AddSelfCall(G,TEXT("DOR_EndFusionTransientEligibility"),0,0); Connect(Start,End->GetExecPin());
        auto* Restore=DORSafetyCall(G,C,TEXT("DOR_RestoreIfActive"),End->GetThenPin());
        auto* B=AddNode<UK2Node_IfThenElse>(G,0,0); B->AllocateDefaultPins(); Connect(Restore->GetThenPin(),B->GetExecPin());
        Connect(Restore->GetReturnValuePin(),B->GetConditionPin()); Connect(B->GetThenPin(),Notify->GetExecPin());
        FindPin(Notify,TEXT("Message"))->DefaultValue=TEXT("[DOR-DIAG][R85] identity-bound=true selective-restore=true save-readback=true journal-delete-verified=true");
        auto* Fail=AddSelfCall(G,TEXT("DOR_DebugNotify"),0,0); FindPin(Fail,TEXT("Message"))->DefaultValue=TEXT("[DOR-SAVE] 恢复/保存/清理未完成：保留恢复文件并阻止后续写入。请勿卸载。不会报告融合成功。"); Connect(B->GetElsePin(),Fail->GetExecPin());
        auto* FailedFlag=AddVariableSet(G,TEXT("TransientApplySucceeded"),0,0);
        FindPin(FailedFlag,TEXT("TransientApplySucceeded"))->DefaultValue=TEXT("false"); Connect(Fail->GetThenPin(),FailedFlag->GetExecPin());
        for(auto* N:TArray<UEdGraphNode*>(G->Nodes))
        {
            if(auto* Set=Cast<UK2Node_VariableSet>(N)) if(Set->VariableReference.GetMemberName()==TEXT("CharacterSaves") || Set->VariableReference.GetMemberName()==TEXT("ItemUpgradeLoadouts")) {Set->BreakAllNodeLinks();G->RemoveNode(Set);}
            if(auto* Call=Cast<UK2Node_CallFunction>(N)) if(Call->FunctionReference.GetMemberName()==TEXT("SaveToDisk")) {Call->BreakAllNodeLinks();G->RemoveNode(Call);}
        }
    }
    // No reachable legacy journal writer/deleter survives in the master.
    // The old next-frame context-expired branch must unwind qualification too.
    for(auto* G:Master->FunctionGraphs) for(auto* N:TArray<UEdGraphNode*>(G->Nodes)) if(auto* Call=Cast<UK2Node_CallFunction>(N))
    {
        const FName Name=Call->FunctionReference.GetMemberName(); auto* Slot=Call->FindPin(TEXT("SlotName"));
        if(!Slot || Slot->DefaultValue!=TEXT("DOR_PresetRecovery_v1")) continue;
        if(Name==TEXT("DeleteGameInSlot") && G->GetFName()==TEXT("DOR_ContinuePair388PresetTransaction"))
        {
            auto* End=AddSelfCall(G,TEXT("DOR_EndFusionTransientEligibility"),0,0);
            for(auto* From:Call->GetExecPin()->LinkedTo) Connect(From,End->GetExecPin());
        }
        if(Name==TEXT("SaveGameToSlot") || Name==TEXT("DeleteGameInSlot")) {Call->BreakAllNodeLinks();G->RemoveNode(Call);}
    }
    for(auto* BP:{Master,InitCave,InitRig,Guard})
    {FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    return true;
}
