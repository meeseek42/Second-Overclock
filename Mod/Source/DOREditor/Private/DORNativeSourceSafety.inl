// Candidate B's equipment window shares the existing local profile journal.
// Schema 2 stays readable; only an explicitly enrolled schema 3 transaction can
// stage vanilla equipment. No caller is enabled by this builder alone.
struct FDORSourceGraph : FDORSafeGraph
{
    FDORSourceGraph(UBlueprint* BP,FName Name):FDORSafeGraph(BP,Name){}
    void Join(UEdGraphPin* Other)
    {
        auto* N=AddNode<UK2Node_IfThenElse>(G,++Serial*160,0);N->AllocateDefaultPins();
        FindPin(N,UEdGraphSchema_K2::PN_Condition)->DefaultValue=TEXT("true");
        Connect(Tail,N->GetExecPin());Connect(Other,N->GetExecPin());Tail=N->GetThenPin();
    }
    UEdGraphPin* And(UEdGraphPin* A,UEdGraphPin* B)
    {return Equal(A,B,TEXT("BooleanAND"),UKismetMathLibrary::StaticClass());}
    UEdGraphPin* EqInt(UEdGraphPin* A,int32 B)
    {auto* N=Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));Connect(A,FindPin(N,TEXT("A")));FindPin(N,TEXT("B"))->DefaultValue=FString::FromInt(B);return N->GetReturnValuePin();}
    UEdGraphPin* StructField(UEdGraphPin* V,UScriptStruct* S,FName Name)
    {return FindPin(Break(V,S),Name);}
    UEdGraphPin* MapFind(UEdGraphPin* Map,UEdGraphPin* Key)
    {
        auto* N=Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"));
        ForcePinType(FindPin(N,TEXT("TargetMap")),MakeUpgradeSelectionMapPinType());
        ForcePinType(FindPin(N,TEXT("Key")),MakeStructPinType(TBaseStructure<FGuid>::Get()));
        ForcePinType(FindPin(N,TEXT("Value")),MakeStructPinType(FItemUpgradeSelection::StaticStruct()));
        Connect(Map,FindPin(N,TEXT("TargetMap")));Connect(Key,FindPin(N,TEXT("Key")));
        return FindPin(N,TEXT("Value"));
    }
    void MapAdd(UEdGraphPin* Map,UEdGraphPin* Key,UEdGraphPin* Value)
    {
        auto* N=Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Add"));
        ForcePinType(FindPin(N,TEXT("TargetMap")),MakeUpgradeSelectionMapPinType());
        ForcePinType(FindPin(N,TEXT("Key")),MakeStructPinType(TBaseStructure<FGuid>::Get()));
        ForcePinType(FindPin(N,TEXT("Value")),MakeStructPinType(FItemUpgradeSelection::StaticStruct()));
        Connect(Map,FindPin(N,TEXT("TargetMap")));Connect(Key,FindPin(N,TEXT("Key")));Connect(Value,FindPin(N,TEXT("Value")));
    }
    void MapRemove(UEdGraphPin* Map,UEdGraphPin* Key)
    {
        auto* N=Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Remove"));
        ForcePinType(FindPin(N,TEXT("TargetMap")),MakeUpgradeSelectionMapPinType());
        ForcePinType(FindPin(N,TEXT("Key")),MakeStructPinType(TBaseStructure<FGuid>::Get()));
        Connect(Map,FindPin(N,TEXT("TargetMap")));Connect(Key,FindPin(N,TEXT("Key")));
    }
    UEdGraphPin* WholeStructSame(UEdGraphPin* A,UEdGraphPin* B)
    {
        auto Wrap=[&](UEdGraphPin* V){auto* N=AddNode<UK2Node_MakeArray>(G,++Serial*160,0);N->NumInputs=1;N->AllocateDefaultPins();auto T=V->PinType;T.ContainerType=EPinContainerType::Array;ForcePinType(N->GetOutputPin(),T);ForcePinType(FindPin(N,TEXT("[0]")),V->PinType);Connect(V,FindPin(N,TEXT("[0]")));return N->GetOutputPin();};
        return SameArray(Wrap(A),Wrap(B));
    }
    UEdGraphPin* SameValue(UEdGraphPin* A,UEdGraphPin* B)
    {
        if(A->PinType.ContainerType==EPinContainerType::Array)return SameArray(A,B);
        const FName Fn=A->PinType.PinCategory==UEdGraphSchema_K2::PC_Boolean?TEXT("EqualEqual_BoolBool"):
            A->PinType.PinCategory==UEdGraphSchema_K2::PC_Int?TEXT("EqualEqual_IntInt"):TEXT("EqualEqual_GuidGuid");
        return Equal(A,B,Fn,Fn==TEXT("EqualEqual_GuidGuid")?UKismetGuidLibrary::StaticClass():UKismetMathLibrary::StaticClass());
    }
    FName Variable(UEdGraphPin* V)
    {return CastChecked<UK2Node_VariableGet>(V->GetOwningNode())->VariableReference.GetMemberName();}
    UEdGraphPin* ExactField(UEdGraphPin* Current,UEdGraphPin* Prior,UEdGraphPin* Staged)
    {
        auto* Out=Cache(TEXT("Field"),Current);
        auto* Changed=Branch(Not(SameValue(Prior,Staged)));
        auto* Own=Branch(SameValue(Current,Staged));Set(Variable(Out),Prior);auto* Restored=Tail;
        Tail=Own->GetElsePin();auto* Benign=Branch(SameValue(Current,Prior));
        Tail=Benign->GetElsePin();Set(TEXT("DOR_SourceConflict"),nullptr,TEXT("true"));auto* Conflict=Tail;
        Tail=Benign->GetThenPin();Join(Conflict);Join(Restored);Join(Changed->GetElsePin());return Out;
    }
};

bool BuildDORNativeSourceSafety(UBlueprint* BP,UClass* JournalClass)
{
    const auto Guid=MakeStructPinType(TBaseStructure<FGuid>::Get());
    const auto GuidArray=MakeStructPinType(TBaseStructure<FGuid>::Get(),true);
    const auto Items=MakeStructPinType(FItemLoadout::StaticStruct());
    const auto Upgrade=MakeStructPinType(FUpgradeLoadout::StaticStruct());
    const auto SelectionArray=MakeStructPinType(FItemUpgradeSelection::StaticStruct(),true);
    const auto Characters=MakeStructPinType(FCharacterSave::StaticStruct(),true);
    // Supported reflected bindings only. The shipping static evidence is in
    // role-aware-native-implementation/equipment-binding-candidates.json.
    for(const FName Name:{FName(TEXT("SavegameID")),FName(TEXT("SelectedLoadout")),FName(TEXT("Loadouts")),
        FName(TEXT("ItemUpgradeLoadouts")),FName(TEXT("RandomWeaponLoadout")),FName(TEXT("RandomItemUpgradeLoadouts"))})
        if(!FindFProperty<FProperty>(FCharacterSave::StaticStruct(),Name))return false;
    auto Saved=[&](const TCHAR* N,const FEdGraphPinType& Type)
    {if(!FindFProperty<FProperty>(JournalClass,N))AddSavedTypedVariable(CastChecked<UBlueprint>(JournalClass->ClassGeneratedBy),N,Type);};
    Saved(TEXT("SourceEquipment"),MakeBoolPinType());Saved(TEXT("EquipmentCharacter"),Guid);
    Saved(TEXT("EquipmentStagingIntent"),MakeBoolPinType());Saved(TEXT("EquipmentEverStaged"),MakeBoolPinType());
    Saved(TEXT("QualificationScopePrepared"),MakeBoolPinType());
    for(const TCHAR* N:{TEXT("QualificationAddedPurchased"),TEXT("QualificationAddedOwned"),TEXT("QualificationAddedForged"),TEXT("QualificationAllowedPurchased"),TEXT("QualificationAllowedOwned"),TEXT("QualificationAllowedForged")})Saved(N,GuidArray);
    Saved(TEXT("NativeProtocolPhase"),MakeIntPinType());Saved(TEXT("NativeRequest"),Guid);Saved(TEXT("NativeEpoch"),Guid);Saved(TEXT("NativeGeneration"),MakeIntPinType());Saved(TEXT("NativeSlot"),MakeIntPinType());Saved(TEXT("NativePlayerKey"),MakeStringPinType());
    Saved(TEXT("NativeBankSchema"),MakeIntPinType());Saved(TEXT("NativeBankIntent"),MakeBoolPinType());
    auto BankStrings=MakeStringPinType();BankStrings.ContainerType=EPinContainerType::Array;
    auto BankBits=MakeBoolPinType();BankBits.ContainerType=EPinContainerType::Array;
    for(const TCHAR* N:{TEXT("NativeBankPaths"),TEXT("NativeBankWeaponPaths"),TEXT("NativeBankRootPaths"),TEXT("NativeBankPriorPaths"),TEXT("NativeBankStagedPaths")})Saved(N,BankStrings);
    for(const TCHAR* N:{TEXT("NativeBankWeaponIds"),TEXT("NativeBankRootIds"),TEXT("NativeBankPriorIds"),TEXT("NativeBankStagedIds")})Saved(N,GuidArray);
    for(const TCHAR* N:{TEXT("NativeBankPriorExists"),TEXT("NativeBankRequired")})Saved(N,BankBits);
    Saved(TEXT("EquipmentPreset"),MakeIntPinType());Saved(TEXT("EquipmentKeys"),GuidArray);
    for(const TCHAR* N:{TEXT("EquipmentPriorItems"),TEXT("EquipmentPriorRandomItems"),TEXT("EquipmentStagedItems")})Saved(N,Items);
    for(const TCHAR* N:{TEXT("EquipmentPriorUpgrades"),TEXT("EquipmentPriorRandomUpgrades"),TEXT("EquipmentStagedUpgrades")})Saved(N,Upgrade);
    auto* JournalBP=CastChecked<UBlueprint>(JournalClass->ClassGeneratedBy);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(JournalBP);FKismetEditorUtilities::CompileBlueprint(JournalBP);
    if(JournalBP->Status==BS_Error)return false;JournalClass=JournalBP->GeneratedClass;
    AddTypedVariable(BP,TEXT("DOR_SourceConflict"),MakeBoolPinType());
    AddTypedVariable(BP,TEXT("DOR_CheckedEquipmentRestoreTransactionId"),MakeStringPinType());
    AddTypedVariable(BP,TEXT("DOR_CheckedEquipmentNoMutationTransactionId"),MakeStringPinType());
    // Exact merge does not touch a field whose prior and staged values agree.
    for(bool Loadout:{true,false})
    {
        const auto Type=Loadout?Items:MakeStructPinType(FItemUpgradeSelection::StaticStruct());
        auto* Struct=Loadout?FItemLoadout::StaticStruct():FItemUpgradeSelection::StaticStruct();
        FDORSourceGraph F(BP,Loadout?TEXT("DOR_SourceMergeItems"):TEXT("DOR_SourceMergeSelection"));
        auto* Current=F.Cache(TEXT("Current"),F.Input(TEXT("Current"),Type));
        auto* Prior=F.Input(TEXT("Prior"),Type);auto* Staged=F.Input(TEXT("Staged"),Type);
        TMap<FName,UEdGraphPin*> Fields;
        const TArray<FName> Names=Loadout?TArray<FName>{TEXT("PrimaryWeapon"),TEXT("SecondaryWeapon"),TEXT("TraversalTool"),TEXT("ClassTool"),TEXT("Armor"),TEXT("Flare"),TEXT("MiningTool"),TEXT("Grenade"),TEXT("iconIndex")}:
            TArray<FName>{TEXT("WeaponID"),TEXT("EquippedUpgrades"),TEXT("PermanentUpgrades"),TEXT("EquippedOverclock"),TEXT("EquippedSkins"),TEXT("OverclockingUnlocked"),TEXT("EquippedSkinColor"),TEXT("EquippedSkinMesh")};
        auto* C=F.Break(Current,Struct);auto* P=F.Break(Prior,Struct);auto* S=F.Break(Staged,Struct);
        for(const auto Name:Names)Fields.Add(Name,F.ExactField(FindPin(C,Name),FindPin(P,Name),FindPin(S,Name)));
        F.Return(F.Patch(Current,Struct,Fields));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceProjectUpgrades"));auto* Current=F.Input(TEXT("Current"),Upgrade);auto* Staged=F.Input(TEXT("Staged"),Upgrade);auto* Keys=F.Input(TEXT("Keys"),GuidArray);
        auto* Map=F.Cache(TEXT("Map"),F.StructField(Current,FUpgradeLoadout::StaticStruct(),TEXT("Loadout")));auto* Input=F.StructField(Staged,FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));
        auto* Loop=F.Each(Keys);auto* Key=FindPin(Loop,TEXT("Array Element"));F.MapAdd(Map,Key,F.MapFind(Input,Key));F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(F.Patch(Current,FUpgradeLoadout::StaticStruct(),{{TEXT("Loadout"),Map}}));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceMergeUpgrades"));
        auto* Current=F.Input(TEXT("Current"),Upgrade);auto* Prior=F.Input(TEXT("Prior"),Upgrade);auto* Staged=F.Input(TEXT("Staged"),Upgrade);
        auto* Keys=F.Input(TEXT("Keys"),GuidArray);
        auto* Map=F.Cache(TEXT("Map"),F.StructField(Current,FUpgradeLoadout::StaticStruct(),TEXT("Loadout")));
        auto* P=F.StructField(Prior,FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));
        auto* S=F.StructField(Staged,FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));
        auto* Loop=F.Each(Keys);auto* Key=FindPin(Loop,TEXT("Array Element"));
        auto* CValue=F.MapFind(Map,Key);auto* CFound=CastChecked<UK2Node_CallFunction>(CValue->GetOwningNode())->GetReturnValuePin();
        auto* PValue=F.MapFind(P,Key);auto* PFound=CastChecked<UK2Node_CallFunction>(PValue->GetOwningNode())->GetReturnValuePin();
        auto* PreviouslyPresent=F.Branch(PFound);auto* Present=F.Branch(CFound);auto* Merge=F.Self(TEXT("DOR_SourceMergeSelection"));
        Connect(CValue,FindPin(Merge,TEXT("Current")));Connect(PValue,FindPin(Merge,TEXT("Prior")));Connect(F.MapFind(S,Key),FindPin(Merge,TEXT("Staged")));
        F.MapAdd(Map,Key,Merge->GetReturnValuePin());auto* Merged=F.Tail;
        F.Tail=Present->GetElsePin();F.Set(TEXT("DOR_SourceConflict"),nullptr,TEXT("true"));F.Join(Merged);auto* PriorDone=F.Tail;
        // A newly inserted key belongs to this request only while its entire
        // typed value still equals the durable staged value.
        F.Tail=PreviouslyPresent->GetElsePin();auto* Inserted=F.Branch(CFound);
        auto* Ours=F.Branch(F.WholeStructSame(CValue,F.MapFind(S,Key)));F.MapRemove(Map,Key);auto* Removed=F.Tail;
        F.Tail=Ours->GetElsePin();F.Set(TEXT("DOR_SourceConflict"),nullptr,TEXT("true"));F.Join(Removed);F.Join(Inserted->GetElsePin());F.Join(PriorDone);
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(F.Patch(Current,FUpgradeLoadout::StaticStruct(),{{TEXT("Loadout"),Map}}));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceFindCharacter"));auto* Array=F.Input(TEXT("Characters"),Characters);auto* Id=F.Input(TEXT("Character"),Guid);
        auto* Count=F.Cache(TEXT("Count"),F.Call(UKismetMathLibrary::StaticClass(),TEXT("MakeLiteralInt"))->GetReturnValuePin());
        // MakeLiteralInt defaults to zero; the unique index starts invalid.
        const FName IndexName(TEXT("DOR_SourceFindCharacter_Index"));AddTypedVariable(BP,IndexName,MakeIntPinType());F.Set(IndexName,nullptr,TEXT("-1"));
        auto* Loop=F.Each(Array);auto* Same=F.Branch(F.SameValue(F.StructField(FindPin(Loop,TEXT("Array Element")),FCharacterSave::StaticStruct(),TEXT("SavegameID")),Id));
        auto* Add=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(Count,FindPin(Add,TEXT("A")));FindPin(Add,TEXT("B"))->DefaultValue=TEXT("1");F.Set(F.Variable(Count),Add->GetReturnValuePin());
        F.Set(IndexName,FindPin(Loop,TEXT("Array Index")));F.Join(Same->GetElsePin());
        F.Tail=FindPin(Loop,TEXT("Completed"));auto* Unique=F.Branch(F.EqInt(Count,1));F.Return(F.Get(IndexName));
        F.Tail=Unique->GetElsePin();auto* Invalid=F.Call(UKismetMathLibrary::StaticClass(),TEXT("MakeLiteralInt"));FindPin(Invalid,TEXT("Value"))->DefaultValue=TEXT("-1");F.Return(Invalid->GetReturnValuePin());F.Compile();
    }
    // Source records/preset arrays must exist; individual equipment map keys
    // may be absent, with their exact absence preserved in the prior map.
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourcePayloadKeys"));auto* Value=F.Input(TEXT("Loadout"),Items);auto* Keys=F.Input(TEXT("Keys"),GuidArray);
        F.Require(F.EqInt(F.Array(TEXT("Array_Length"),Keys)->GetReturnValuePin(),8));auto* Seen=F.Cache(TEXT("Seen"),Keys);F.Array(TEXT("Array_Clear"),Seen);
        auto* Loop=F.Each(Keys);auto* Key=FindPin(Loop,TEXT("Array Element"));auto* Duplicate=F.Array(TEXT("Array_Contains"),Seen);Connect(Key,FindPin(Duplicate,TEXT("ItemToFind")));F.Require(F.Not(Duplicate->GetReturnValuePin()));auto* Valid=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(Key,FindPin(Valid,TEXT("InGuid")));F.Require(Valid->GetReturnValuePin());auto* Add=F.Array(TEXT("Array_Add"),Seen);Connect(Key,FindPin(Add,TEXT("NewItem")));F.Tail=FindPin(Loop,TEXT("Completed"));
        // Native category-default slots remain raw zero; selected-slot binding is
        // checked at original capture. Reject a payload with neither weapon supplied.
        auto WeaponValid=[&](const TCHAR* Name){auto* Valid=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(F.StructField(Value,FItemLoadout::StaticStruct(),Name),FindPin(Valid,TEXT("InGuid")));return Valid->GetReturnValuePin();};
        F.Require(F.Equal(WeaponValid(TEXT("PrimaryWeapon")),WeaponValid(TEXT("SecondaryWeapon")),TEXT("BooleanOR"),UKismetMathLibrary::StaticClass()));
        F.Array(TEXT("Array_Clear"),Seen);
        for(const TCHAR* Name:{TEXT("PrimaryWeapon"),TEXT("SecondaryWeapon"),TEXT("TraversalTool"),TEXT("ClassTool"),TEXT("Armor"),TEXT("Flare"),TEXT("MiningTool"),TEXT("Grenade")})
        {
            auto* Id=F.StructField(Value,FItemLoadout::StaticStruct(),Name);auto* FieldValid=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(Id,FindPin(FieldValid,TEXT("InGuid")));UK2Node_IfThenElse* Populated=nullptr;
            Populated=F.Branch(FieldValid->GetReturnValuePin());
            auto* FieldDuplicate=F.Array(TEXT("Array_Contains"),Seen);Connect(Id,FindPin(FieldDuplicate,TEXT("ItemToFind")));F.Require(F.Not(FieldDuplicate->GetReturnValuePin()));
            auto* Contains=F.Array(TEXT("Array_Contains"),Keys);Connect(Id,FindPin(Contains,TEXT("ItemToFind")));F.Require(Contains->GetReturnValuePin());auto* FieldAdd=F.Array(TEXT("Array_Add"),Seen);Connect(Id,FindPin(FieldAdd,TEXT("NewItem")));if(Populated)F.Join(Populated->GetElsePin());
        }
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    for(bool PriorKeys:{false,true})
    {
        FDORSourceGraph F(BP,PriorKeys?TEXT("DOR_SourcePriorKeys"):TEXT("DOR_SourceHasKeys"));auto* Value=F.Input(TEXT("Upgrades"),Upgrade);auto* Keys=F.Input(TEXT("Keys"),GuidArray);
        auto* All=F.Cache(TEXT("All"),F.EqInt(F.Array(TEXT("Array_Length"),Keys)->GetReturnValuePin(),8));
        auto* Map=F.StructField(Value,FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));auto* Loop=F.Each(Keys);
        auto* FoundValue=F.MapFind(Map,FindPin(Loop,TEXT("Array Element")));auto* Found=CastChecked<UK2Node_CallFunction>(FoundValue->GetOwningNode())->GetReturnValuePin();
        auto* Match=F.SameValue(F.StructField(FoundValue,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID")),FindPin(Loop,TEXT("Array Element")));
        auto* Valid=PriorKeys?F.Equal(F.Not(Found),Match,TEXT("BooleanOR"),UKismetMathLibrary::StaticClass()):F.And(Found,Match);
        F.Set(F.Variable(All),F.And(All,Valid));
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(All);F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceProtocolCanTransition"));auto* Current=F.Input(TEXT("CurrentPhase"),MakeIntPinType());auto* Next=F.Input(TEXT("NextPhase"),MakeIntPinType());auto* Allowed=F.And(F.EqInt(Current,0),F.EqInt(Current,3));
        for(const auto& P:{TPair<int32,int32>(0,0),TPair<int32,int32>(0,3),TPair<int32,int32>(3,3),TPair<int32,int32>(3,0),TPair<int32,int32>(3,4),TPair<int32,int32>(3,5),TPair<int32,int32>(4,4),TPair<int32,int32>(5,5)})Allowed=F.Equal(Allowed,F.And(F.EqInt(Current,P.Key),F.EqInt(Next,P.Value)),TEXT("BooleanOR"),UKismetMathLibrary::StaticClass());F.Return(Allowed);F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceValidateRecord"));auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JournalClass));F.Require(F.IsValid(J));
        F.Require(F.EqInt(F.Field(J,JournalClass,TEXT("Schema")),3));F.Require(F.Field(J,JournalClass,TEXT("SourceEquipment")));
        auto* EquipmentLease=F.Branch(F.Field(J,JournalClass,TEXT("EquipmentStagingIntent")));F.Require(F.Field(J,JournalClass,TEXT("EquipmentEverStaged")));F.Require(F.EqInt(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),0));F.Join(EquipmentLease->GetElsePin());
        F.Require(F.Not(F.Field(J,JournalClass,TEXT("Cleanup"))));F.Require(F.Not(F.Field(J,JournalClass,TEXT("Registration"))));
        auto* ProtocolRange=F.Call(UKismetMathLibrary::StaticClass(),TEXT("InRange_IntInt"));Connect(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),FindPin(ProtocolRange,TEXT("Value")));FindPin(ProtocolRange,TEXT("Min"))->DefaultValue=TEXT("0");FindPin(ProtocolRange,TEXT("Max"))->DefaultValue=TEXT("5");FindPin(ProtocolRange,TEXT("InclusiveMin"))->DefaultValue=TEXT("true");FindPin(ProtocolRange,TEXT("InclusiveMax"))->DefaultValue=TEXT("true");F.Require(ProtocolRange->GetReturnValuePin());
        auto* KnownPhase=F.Self(TEXT("DOR_SourceProtocolCanTransition"));Connect(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),FindPin(KnownPhase,TEXT("CurrentPhase")));Connect(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),FindPin(KnownPhase,TEXT("NextPhase")));F.Require(KnownPhase->GetReturnValuePin());
        auto* Advanced=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),FindPin(Advanced,TEXT("A")));FindPin(Advanced,TEXT("B"))->DefaultValue=TEXT("3");auto* Bound=F.Branch(Advanced->GetReturnValuePin());for(const TCHAR* Name:{TEXT("NativeRequest"),TEXT("NativeEpoch")}){auto* Exists=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(F.Field(J,JournalClass,Name),FindPin(Exists,TEXT("InGuid")));F.Require(Exists->GetReturnValuePin());}auto* Positive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(F.Field(J,JournalClass,TEXT("NativeGeneration")),FindPin(Positive,TEXT("A")));F.Require(Positive->GetReturnValuePin());F.Require(F.Equal(F.EqInt(F.Field(J,JournalClass,TEXT("NativeSlot")),0),F.EqInt(F.Field(J,JournalClass,TEXT("NativeSlot")),1),TEXT("BooleanOR"),UKismetMathLibrary::StaticClass()));F.Require(F.Not(F.Equal(F.Field(J,JournalClass,TEXT("NativePlayerKey")),DORSafeString(F,TEXT("")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass())));F.Join(Bound->GetElsePin());
        auto* CharacterValid=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(F.Field(J,JournalClass,TEXT("EquipmentCharacter")),FindPin(CharacterValid,TEXT("InGuid")));F.Require(CharacterValid->GetReturnValuePin());
        auto* Nonnegative=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(F.Field(J,JournalClass,TEXT("EquipmentPreset")),FindPin(Nonnegative,TEXT("A")));F.Require(Nonnegative->GetReturnValuePin());
        auto* Keys=F.Field(J,JournalClass,TEXT("EquipmentKeys"));F.Require(F.EqInt(F.Array(TEXT("Array_Length"),Keys)->GetReturnValuePin(),8));
        auto* PayloadKeys=F.Self(TEXT("DOR_SourcePayloadKeys"));Connect(F.Field(J,JournalClass,TEXT("EquipmentStagedItems")),FindPin(PayloadKeys,TEXT("Loadout")));Connect(Keys,FindPin(PayloadKeys,TEXT("Keys")));F.Require(PayloadKeys->GetReturnValuePin());
        // Reject duplicate/empty keys independently of map cardinality.
        auto* Seen=F.Cache(TEXT("Seen"),Keys);F.Array(TEXT("Array_Clear"),Seen);
        auto* Loop=F.Each(Keys);auto* Key=FindPin(Loop,TEXT("Array Element"));auto* Contains=F.Array(TEXT("Array_Contains"),Seen);Connect(Key,FindPin(Contains,TEXT("ItemToFind")));F.Require(F.Not(Contains->GetReturnValuePin()));
        auto* Valid=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(Key,FindPin(Valid,TEXT("InGuid")));F.Require(Valid->GetReturnValuePin());
        auto* Add=F.Array(TEXT("Array_Add"),Seen);Connect(Key,FindPin(Add,TEXT("NewItem")));F.Tail=FindPin(Loop,TEXT("Completed"));
        for(const TCHAR* N:{TEXT("EquipmentPriorUpgrades"),TEXT("EquipmentPriorRandomUpgrades"),TEXT("EquipmentStagedUpgrades")})
        {auto* Check=F.Self(FString(N)==TEXT("EquipmentStagedUpgrades")?TEXT("DOR_SourceHasKeys"):TEXT("DOR_SourcePriorKeys"));Connect(Keys,FindPin(Check,TEXT("Keys")));Connect(F.Field(J,JournalClass,N),FindPin(Check,TEXT("Upgrades")));F.Require(Check->GetReturnValuePin());}
        for(const auto& P:{TPair<const TCHAR*,const TCHAR*>(TEXT("QualificationAddedPurchased"),TEXT("PairIds")),TPair<const TCHAR*,const TCHAR*>(TEXT("QualificationAddedOwned"),TEXT("SchematicIds")),TPair<const TCHAR*,const TCHAR*>(TEXT("QualificationAddedForged"),TEXT("SchematicIds")),TPair<const TCHAR*,const TCHAR*>(TEXT("QualificationAllowedPurchased"),TEXT("PairIds")),TPair<const TCHAR*,const TCHAR*>(TEXT("QualificationAllowedOwned"),TEXT("SchematicIds")),TPair<const TCHAR*,const TCHAR*>(TEXT("QualificationAllowedForged"),TEXT("SchematicIds"))})
        {auto* Added=F.Field(J,JournalClass,P.Key);auto* Unprepared=F.Branch(F.Not(F.Field(J,JournalClass,TEXT("QualificationScopePrepared"))));F.Require(F.EqInt(F.Array(TEXT("Array_Length"),Added)->GetReturnValuePin(),0));F.Join(Unprepared->GetElsePin());auto* AddedLoop=F.Each(Added);auto* Has=F.Array(TEXT("Array_Contains"),F.Field(J,JournalClass,P.Value));Connect(FindPin(AddedLoop,TEXT("Array Element")),FindPin(Has,TEXT("ItemToFind")));F.Require(Has->GetReturnValuePin());F.Tail=FindPin(AddedLoop,TEXT("Completed"));}
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceInputsMatch"));auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JournalClass));auto* Loadout=F.Input(TEXT("Loadout"),Items);auto* Selections=F.Input(TEXT("Selections"),SelectionArray);F.Require(F.IsValid(J));auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());F.Require(F.WholeStructSame(Loadout,F.Field(J,JournalClass,TEXT("EquipmentStagedItems"))));F.Require(F.EqInt(F.Array(TEXT("Array_Length"),Selections)->GetReturnValuePin(),8));
        auto* Keys=F.Field(J,JournalClass,TEXT("EquipmentKeys"));auto* Seen=F.Cache(TEXT("Seen"),Keys);F.Array(TEXT("Array_Clear"),Seen);auto* Map=F.StructField(F.Field(J,JournalClass,TEXT("EquipmentStagedUpgrades")),FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));auto* Loop=F.Each(Selections);auto* Entry=FindPin(Loop,TEXT("Array Element"));auto* Key=F.StructField(Entry,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID"));auto* Duplicate=F.Array(TEXT("Array_Contains"),Seen);Connect(Key,FindPin(Duplicate,TEXT("ItemToFind")));F.Require(F.Not(Duplicate->GetReturnValuePin()));auto* Has=F.Array(TEXT("Array_Contains"),Keys);Connect(Key,FindPin(Has,TEXT("ItemToFind")));F.Require(Has->GetReturnValuePin());F.Require(F.WholeStructSame(Entry,F.MapFind(Map,Key)));auto* Add=F.Array(TEXT("Array_Add"),Seen);Connect(Key,FindPin(Add,TEXT("NewItem")));F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    auto GetArray=[&](FDORSourceGraph& F,UEdGraphPin* Array,UEdGraphPin* Index)
    {auto* N=F.Array(TEXT("Array_Get"),Array);Connect(Index,FindPin(N,TEXT("Index")));return FindPin(N,TEXT("Item"));};
    auto HasIndex=[&](FDORSourceGraph& F,UEdGraphPin* Array,UEdGraphPin* Index)
    {auto* N=F.Array(TEXT("Array_IsValidIndex"),Array);Connect(Index,FindPin(N,TEXT("IndexToTest")));return N->GetReturnValuePin();};
    auto Replace=[&](FDORSourceGraph& F,UEdGraphPin* Array,UEdGraphPin* Index,UEdGraphPin* Value)
    {auto* N=F.Array(TEXT("Array_Set"),Array);Connect(Index,FindPin(N,TEXT("Index")));Connect(Value,FindPin(N,TEXT("Item")));return Array;};
    auto Locate=[&](FDORSourceGraph& F,UEdGraphPin* Save,UEdGraphPin* Id,UEdGraphPin*& Array,UEdGraphPin*& Index)
    {
        Array=F.Cache(TEXT("Characters"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("CharacterSaves")));
        auto* Find=F.Self(TEXT("DOR_SourceFindCharacter"));Connect(Array,FindPin(Find,TEXT("Characters")));Connect(Id,FindPin(Find,TEXT("Character")));Index=F.Cache(TEXT("CharacterIndex"),Find->GetReturnValuePin());
        F.Require(HasIndex(F,Array,Index),TEXT("[DOR-SOURCE]|stage=source-gate|condition=missing-or-duplicate-character|mutation=false"));return F.Cache(TEXT("Character"),GetArray(F,Array,Index));
    };
    auto PayloadSame=[&](FDORSourceGraph& F,UEdGraphPin* A,UEdGraphPin* B)
    {
        // Native struct equality is available through Array_Identical without
        // inventing a Blueprint-callable operator for native equipment structs.
        auto Wrap=[&](UEdGraphPin* V){auto* N=AddNode<UK2Node_MakeArray>(F.G,++F.Serial*160,0);N->NumInputs=1;N->AllocateDefaultPins();auto T=V->PinType;T.ContainerType=EPinContainerType::Array;ForcePinType(N->GetOutputPin(),T);ForcePinType(FindPin(N,TEXT("[0]")),V->PinType);Connect(V,FindPin(N,TEXT("[0]")));return N->GetOutputPin();};
        return F.SameArray(Wrap(A),Wrap(B));
    };
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceMergeCharacter"));auto* Current=F.Input(TEXT("Current"),MakeStructPinType(FCharacterSave::StaticStruct()));auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JournalClass));
        auto* Entered=F.Branch(F.Field(J,JournalClass,TEXT("EquipmentStagingIntent")));F.Tail=Entered->GetElsePin();F.Return(Current);F.Tail=Entered->GetThenPin();
        auto* Preset=F.Field(J,JournalClass,TEXT("EquipmentPreset"));auto* LI=F.Cache(TEXT("Items"),F.StructField(Current,FCharacterSave::StaticStruct(),TEXT("Loadouts")));auto* LU=F.Cache(TEXT("Upgrades"),F.StructField(Current,FCharacterSave::StaticStruct(),TEXT("ItemUpgradeLoadouts")));
        auto* Valid=F.Branch(F.And(F.SameValue(F.StructField(Current,FCharacterSave::StaticStruct(),TEXT("SavegameID")),F.Field(J,JournalClass,TEXT("EquipmentCharacter"))),F.And(HasIndex(F,LI,Preset),HasIndex(F,LU,Preset))));
        auto MergeItems=[&](UEdGraphPin* Value,const TCHAR* Prior){auto* M=F.Self(TEXT("DOR_SourceMergeItems"));Connect(Value,FindPin(M,TEXT("Current")));Connect(F.Field(J,JournalClass,Prior),FindPin(M,TEXT("Prior")));Connect(F.Field(J,JournalClass,TEXT("EquipmentStagedItems")),FindPin(M,TEXT("Staged")));return F.Cache(TEXT("MergedItems"),M->GetReturnValuePin());};
        auto MergeUpgrades=[&](UEdGraphPin* Value,const TCHAR* Prior){auto* M=F.Self(TEXT("DOR_SourceMergeUpgrades"));Connect(Value,FindPin(M,TEXT("Current")));Connect(F.Field(J,JournalClass,Prior),FindPin(M,TEXT("Prior")));Connect(F.Field(J,JournalClass,TEXT("EquipmentStagedUpgrades")),FindPin(M,TEXT("Staged")));Connect(F.Field(J,JournalClass,TEXT("EquipmentKeys")),FindPin(M,TEXT("Keys")));return F.Cache(TEXT("MergedUpgrades"),M->GetReturnValuePin());};
        auto* NormalItems=MergeItems(GetArray(F,LI,Preset),TEXT("EquipmentPriorItems"));auto* RandomItems=MergeItems(F.StructField(Current,FCharacterSave::StaticStruct(),TEXT("RandomWeaponLoadout")),TEXT("EquipmentPriorRandomItems"));
        auto* NormalUpgrades=MergeUpgrades(GetArray(F,LU,Preset),TEXT("EquipmentPriorUpgrades"));auto* RandomUpgrades=MergeUpgrades(F.StructField(Current,FCharacterSave::StaticStruct(),TEXT("RandomItemUpgradeLoadouts")),TEXT("EquipmentPriorRandomUpgrades"));
        Replace(F,LI,Preset,NormalItems);Replace(F,LU,Preset,NormalUpgrades);
        F.Return(F.Patch(Current,FCharacterSave::StaticStruct(),{{TEXT("Loadouts"),LI},{TEXT("ItemUpgradeLoadouts"),LU},{TEXT("RandomWeaponLoadout"),RandomItems},{TEXT("RandomItemUpgradeLoadouts"),RandomUpgrades}}));
        F.Tail=Valid->GetElsePin();F.Set(TEXT("DOR_SourceConflict"),nullptr,TEXT("true"));F.Return(Current);F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceStagedReadback"));auto* Token=F.Input(TEXT("TransactionId"),MakeStringPinType());
        auto* J=F.Cache(TEXT("Journal"),F.Get(TEXT("DOR_Journal")));F.Require(F.Get(TEXT("DOR_Active")));
        F.Require(F.Equal(Token,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());
        auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());
        F.Require(F.EqInt(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),0));
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame());F.Require(F.IsValid(Save));UEdGraphPin* Array=nullptr;UEdGraphPin* Index=nullptr;
        auto* C=Locate(F,Save,F.Field(J,JournalClass,TEXT("EquipmentCharacter")),Array,Index);auto* Preset=F.Field(J,JournalClass,TEXT("EquipmentPreset"));
        F.Require(F.SameValue(Preset,F.StructField(C,FCharacterSave::StaticStruct(),TEXT("SelectedLoadout"))));
        auto* LI=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("Loadouts"));auto* LU=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("ItemUpgradeLoadouts"));
        F.Require(HasIndex(F,LI,Preset));F.Require(HasIndex(F,LU,Preset));
        for(auto* Value:{GetArray(F,LI,Preset),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomWeaponLoadout"))})F.Require(PayloadSame(F,Value,F.Field(J,JournalClass,TEXT("EquipmentStagedItems"))));
        auto* ExpectedMap=F.StructField(F.Field(J,JournalClass,TEXT("EquipmentStagedUpgrades")),FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));
        for(auto* Value:{GetArray(F,LU,Preset),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomItemUpgradeLoadouts"))})
        {
            auto* Map=F.StructField(Value,FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));auto* Loop=F.Each(F.Field(J,JournalClass,TEXT("EquipmentKeys")));auto* Key=FindPin(Loop,TEXT("Array Element"));
            auto* Current=F.MapFind(Map,Key);F.Require(CastChecked<UK2Node_CallFunction>(Current->GetOwningNode())->GetReturnValuePin());F.Require(PayloadSame(F,Current,F.MapFind(ExpectedMap,Key)));F.Tail=FindPin(Loop,TEXT("Completed"));
        }
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {FDORSourceGraph F(BP,TEXT("DOR_SourceMarkEquipmentLease"));F.Input(TEXT("TransactionId"),MakeStringPinType());F.Input(TEXT("Acquire"),MakeBoolPinType());F.Input(TEXT("RestoreChecked"),MakeBoolPinType());F.Return();F.Compile();}
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceStage"));auto* Token=F.Input(TEXT("TransactionId"),MakeStringPinType());
        F.Require(F.Get(TEXT("DOR_Active")));F.Require(F.Not(F.Get(TEXT("DOR_Blocked"))));auto* J=F.Cache(TEXT("Journal"),F.Get(TEXT("DOR_Journal")));
        F.Require(F.EqInt(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),0));
        F.Require(F.Equal(Token,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());
        auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame());F.Require(F.IsValid(Save));UEdGraphPin* Array=nullptr;UEdGraphPin* Index=nullptr;
        auto* C=Locate(F,Save,F.Field(J,JournalClass,TEXT("EquipmentCharacter")),Array,Index);auto* Preset=F.Field(J,JournalClass,TEXT("EquipmentPreset"));
        F.Require(F.SameValue(Preset,F.StructField(C,FCharacterSave::StaticStruct(),TEXT("SelectedLoadout"))));
        auto* LI=F.Cache(TEXT("Items"),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("Loadouts")));auto* LU=F.Cache(TEXT("Upgrades"),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("ItemUpgradeLoadouts")));
        F.Require(HasIndex(F,LI,Preset));F.Require(HasIndex(F,LU,Preset));
        for(const auto& P:{TPair<const TCHAR*,UEdGraphPin*>(TEXT("EquipmentPriorItems"),GetArray(F,LI,Preset)),TPair<const TCHAR*,UEdGraphPin*>(TEXT("EquipmentPriorRandomItems"),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomWeaponLoadout")))})
            F.Require(PayloadSame(F,P.Value,F.Field(J,JournalClass,P.Key)),TEXT("[DOR-SOURCE]|stage=stage-rejected|condition=source-changed-since-enrollment|mutation=false"));
        for(const auto& P:{TPair<const TCHAR*,UEdGraphPin*>(TEXT("EquipmentPriorUpgrades"),GetArray(F,LU,Preset)),TPair<const TCHAR*,UEdGraphPin*>(TEXT("EquipmentPriorRandomUpgrades"),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomItemUpgradeLoadouts")))})
        {
            auto* CurrentMap=F.StructField(P.Value,FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));auto* PriorMap=F.StructField(F.Field(J,JournalClass,P.Key),FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));auto* Loop=F.Each(F.Field(J,JournalClass,TEXT("EquipmentKeys")));auto* Key=FindPin(Loop,TEXT("Array Element"));auto* Current=F.MapFind(CurrentMap,Key);auto* Prior=F.MapFind(PriorMap,Key);auto* HasCurrent=CastChecked<UK2Node_CallFunction>(Current->GetOwningNode())->GetReturnValuePin();auto* HasPrior=CastChecked<UK2Node_CallFunction>(Prior->GetOwningNode())->GetReturnValuePin();F.Require(F.SameValue(HasCurrent,HasPrior));auto* Held=F.Branch(HasPrior);F.Require(PayloadSame(F,Current,Prior));F.Join(Held->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));
        }
        auto StageMap=[&](UEdGraphPin* Current){auto* Project=F.Self(TEXT("DOR_SourceProjectUpgrades"));Connect(Current,FindPin(Project,TEXT("Current")));Connect(F.Field(J,JournalClass,TEXT("EquipmentStagedUpgrades")),FindPin(Project,TEXT("Staged")));Connect(F.Field(J,JournalClass,TEXT("EquipmentKeys")),FindPin(Project,TEXT("Keys")));return Project->GetReturnValuePin();};
        auto* Normal=F.Cache(TEXT("NormalStaged"),StageMap(GetArray(F,LU,Preset)));auto* Random=F.Cache(TEXT("RandomStaged"),StageMap(F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomItemUpgradeLoadouts"))));
        auto* Intent=F.Self(TEXT("DOR_SourceMarkEquipmentLease"));Connect(Token,FindPin(Intent,TEXT("TransactionId")));FindPin(Intent,TEXT("Acquire"))->DefaultValue=TEXT("true");F.Require(Intent->GetReturnValuePin());F.Require(F.Equal(F.SaveGame(),Save,TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));F.Require(F.SameArray(Array,F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("CharacterSaves"))),TEXT("[DOR-SOURCE]|stage=stage-rejected|condition=profile-progress-changed-during-WAL-write|barrier=retained"));
        Replace(F,LI,Preset,F.Field(J,JournalClass,TEXT("EquipmentStagedItems")));Replace(F,LU,Preset,Normal);
        auto* Patched=F.Patch(C,FCharacterSave::StaticStruct(),{{TEXT("Loadouts"),LI},{TEXT("ItemUpgradeLoadouts"),LU},{TEXT("RandomWeaponLoadout"),F.Field(J,JournalClass,TEXT("EquipmentStagedItems"))},{TEXT("RandomItemUpgradeLoadouts"),Random}});
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));F.Set(TEXT("DOR_CheckedEquipmentRestoreTransactionId"),nullptr,TEXT(""));Replace(F,Array,Index,Patched);F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("CharacterSaves"),Array);
        auto* Read=F.Self(TEXT("DOR_SourceStagedReadback"));Connect(Token,FindPin(Read,TEXT("TransactionId")));F.Require(Read->GetReturnValuePin(),TEXT("[DOR-SOURCE]|stage=stage-readback-failed|journal=retained|barrier=held"));
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceJournalReadback"));auto* A=F.Input(TEXT("Expected"),MakeObjectPinType(JournalClass));auto* B=F.Input(TEXT("Actual"),MakeObjectPinType(JournalClass));
        auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(B,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());
        for(const TCHAR* N:{TEXT("TransactionId"),TEXT("SourceSlot"),TEXT("SourceAnonymousId")})F.Require(F.Equal(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        for(const TCHAR* N:{TEXT("SourceIndex"),TEXT("EquipmentPreset")})F.Require(F.SameValue(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));
        F.Require(F.SameValue(F.Field(A,JournalClass,TEXT("EquipmentCharacter")),F.Field(B,JournalClass,TEXT("EquipmentCharacter"))));
        F.Require(F.SameArray(F.Field(A,JournalClass,TEXT("EquipmentKeys")),F.Field(B,JournalClass,TEXT("EquipmentKeys"))));
        for(const TCHAR* N:{TEXT("EquipmentPriorItems"),TEXT("EquipmentPriorRandomItems"),TEXT("EquipmentStagedItems"),TEXT("EquipmentPriorUpgrades"),TEXT("EquipmentPriorRandomUpgrades"),TEXT("EquipmentStagedUpgrades")})F.Require(PayloadSame(F,F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));
        // Widening the existing record must not lose its qualification baseline.
        for(const TCHAR* N:{TEXT("BeforeCharacters"),TEXT("BeforeLoadouts"),TEXT("BeforePurchased"),TEXT("BeforeOwned"),TEXT("BeforeForged"),TEXT("PairIds"),TEXT("SchematicIds")})F.Require(F.SameArray(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));
        F.Require(F.SameValue(F.Field(A,JournalClass,TEXT("QualificationScopePrepared")),F.Field(B,JournalClass,TEXT("QualificationScopePrepared"))));
        for(const TCHAR* N:{TEXT("EquipmentStagingIntent"),TEXT("EquipmentEverStaged")})F.Require(F.SameValue(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));
        for(const TCHAR* N:{TEXT("QualificationAddedPurchased"),TEXT("QualificationAddedOwned"),TEXT("QualificationAddedForged"),TEXT("QualificationAllowedPurchased"),TEXT("QualificationAllowedOwned"),TEXT("QualificationAllowedForged")})F.Require(F.SameArray(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));
        for(const TCHAR* N:{TEXT("NativeProtocolPhase"),TEXT("NativeGeneration"),TEXT("NativeSlot"),TEXT("NativeRequest"),TEXT("NativeEpoch")})F.Require(F.SameValue(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));F.Require(F.Equal(F.Field(A,JournalClass,TEXT("NativePlayerKey")),F.Field(B,JournalClass,TEXT("NativePlayerKey")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        for(const TCHAR* N:{TEXT("NativeBankSchema"),TEXT("NativeBankIntent")})F.Require(F.SameValue(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));
        for(const TCHAR* N:{TEXT("NativeBankPaths"),TEXT("NativeBankWeaponPaths"),TEXT("NativeBankRootPaths"),TEXT("NativeBankPriorPaths"),TEXT("NativeBankStagedPaths"),TEXT("NativeBankWeaponIds"),TEXT("NativeBankRootIds"),TEXT("NativeBankPriorIds"),TEXT("NativeBankStagedIds"),TEXT("NativeBankPriorExists"),TEXT("NativeBankRequired")})F.Require(F.SameArray(F.Field(A,JournalClass,N),F.Field(B,JournalClass,N)));
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceMarkEquipmentLease"));auto* Token=F.Input(TEXT("TransactionId"),MakeStringPinType());auto* Acquire=F.Input(TEXT("Acquire"),MakeBoolPinType());auto* Checked=F.Input(TEXT("RestoreChecked"),MakeBoolPinType());auto* J=F.Cache(TEXT("Journal"),F.Get(TEXT("DOR_Journal")));F.Require(F.Get(TEXT("DOR_Active")));F.Require(F.IsValid(J));F.Require(F.Equal(Token,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());F.Require(F.EqInt(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),0));auto* Begin=F.Branch(Acquire);F.Require(F.Not(F.Get(TEXT("DOR_Blocked"))));F.Require(F.Not(F.Field(J,JournalClass,TEXT("EquipmentStagingIntent"))));F.Put(J,JournalClass,TEXT("EquipmentEverStaged"),nullptr,TEXT("true"));F.Set(TEXT("DOR_CheckedEquipmentRestoreTransactionId"),nullptr,TEXT(""));F.Set(TEXT("DOR_CheckedEquipmentNoMutationTransactionId"),nullptr,TEXT(""));auto* Started=F.Tail;F.Tail=Begin->GetElsePin();F.Require(Checked);F.Require(F.Field(J,JournalClass,TEXT("EquipmentStagingIntent")));F.Join(Started);F.Put(J,JournalClass,TEXT("EquipmentStagingIntent"),Acquire);
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));F.Require(DORSafeSave(F,J,F.Get(TEXT("DOR_JournalSlot"))));auto* Read=F.Cache(TEXT("Durable"),DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass));auto* Exact=F.Self(TEXT("DOR_SourceJournalReadback"));Connect(J,FindPin(Exact,TEXT("Expected")));Connect(Read,FindPin(Exact,TEXT("Actual")));F.Require(Exact->GetReturnValuePin());auto* Bound=F.Self(TEXT("DOR_CheckIdentity"));Connect(Read,FindPin(Bound,TEXT("Record")));F.Require(Bound->GetReturnValuePin());F.Set(TEXT("DOR_Journal"),Read);F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceCanEnroll"));auto* Id=F.Input(TEXT("Character"),Guid);auto* Loadout=F.Input(TEXT("Loadout"),Items);auto* Selections=F.Input(TEXT("Selections"),SelectionArray);
        auto* ValidId=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(Id,FindPin(ValidId,TEXT("InGuid")));F.Require(ValidId->GetReturnValuePin());
        F.Require(F.EqInt(F.Array(TEXT("Array_Length"),Selections)->GetReturnValuePin(),8));
        auto* Keys=F.Cache(TEXT("Keys"),F.Get(TEXT("DOR_AllPairIds")));F.Array(TEXT("Array_Clear"),Keys);
        auto* Loop=F.Each(Selections);auto* Key=F.StructField(FindPin(Loop,TEXT("Array Element")),FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID"));
        auto* Seen=F.Array(TEXT("Array_Contains"),Keys);Connect(Key,FindPin(Seen,TEXT("ItemToFind")));F.Require(F.Not(Seen->GetReturnValuePin()));auto* Nonzero=F.Call(UKismetGuidLibrary::StaticClass(),TEXT("IsValid_Guid"));Connect(Key,FindPin(Nonzero,TEXT("InGuid")));F.Require(Nonzero->GetReturnValuePin());auto* Add=F.Array(TEXT("Array_Add"),Keys);Connect(Key,FindPin(Add,TEXT("NewItem")));F.Tail=FindPin(Loop,TEXT("Completed"));
        auto* Payload=F.Self(TEXT("DOR_SourcePayloadKeys"));Connect(Loadout,FindPin(Payload,TEXT("Loadout")));Connect(Keys,FindPin(Payload,TEXT("Keys")));F.Require(Payload->GetReturnValuePin());
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame());F.Require(F.IsValid(Save));UEdGraphPin* Records=nullptr;UEdGraphPin* Index=nullptr;
        auto* C=Locate(F,Save,Id,Records,Index);auto* Preset=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("SelectedLoadout"));auto* LI=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("Loadouts"));auto* LU=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("ItemUpgradeLoadouts"));F.Require(HasIndex(F,LI,Preset));F.Require(HasIndex(F,LU,Preset));
        for(auto* Value:{GetArray(F,LU,Preset),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomItemUpgradeLoadouts"))})
        {auto* Has=F.Self(TEXT("DOR_SourcePriorKeys"));Connect(Keys,FindPin(Has,TEXT("Keys")));Connect(Value,FindPin(Has,TEXT("Upgrades")));F.Require(Has->GetReturnValuePin(),TEXT("[DOR-SOURCE]|stage=preflight-rejected|condition=unsupported-existing-source-keys|mutation=false"));}
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceEnroll"));auto* Id=F.Input(TEXT("Character"),Guid);auto* Loadout=F.Input(TEXT("Loadout"),Items);auto* Selections=F.Input(TEXT("Selections"),SelectionArray);
        auto* Can=F.Self(TEXT("DOR_SourceCanEnroll"));Connect(Id,FindPin(Can,TEXT("Character")));Connect(Loadout,FindPin(Can,TEXT("Loadout")));Connect(Selections,FindPin(Can,TEXT("Selections")));F.Require(Can->GetReturnValuePin());
        F.Require(F.Get(TEXT("DOR_Active")));F.Require(F.Not(F.Get(TEXT("DOR_Blocked"))));auto* J=F.Cache(TEXT("Journal"),F.Get(TEXT("DOR_Journal")));
        auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());F.Require(F.EqInt(F.Field(J,JournalClass,TEXT("Schema")),2));
        F.Require(F.Not(F.Field(J,JournalClass,TEXT("Cleanup"))));F.Require(F.Not(F.Field(J,JournalClass,TEXT("Registration"))));
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame());F.Require(F.IsValid(Save));UEdGraphPin* Array=nullptr;UEdGraphPin* Index=nullptr;auto* C=Locate(F,Save,Id,Array,Index);
        auto* Preset=F.Cache(TEXT("Preset"),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("SelectedLoadout")));
        auto* LI=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("Loadouts"));auto* LU=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("ItemUpgradeLoadouts"));
        F.Require(HasIndex(F,LI,Preset));F.Require(HasIndex(F,LU,Preset));
        auto* Keys=F.Cache(TEXT("Keys"),F.Field(J,JournalClass,TEXT("EquipmentKeys")));F.Array(TEXT("Array_Clear"),Keys);
        auto* Map=F.Cache(TEXT("Map"),F.StructField(GetArray(F,LU,Preset),FUpgradeLoadout::StaticStruct(),TEXT("Loadout")));
        F.Require(F.EqInt(F.Array(TEXT("Array_Length"),Selections)->GetReturnValuePin(),8));
        auto* Loop=F.Each(Selections);auto* Value=FindPin(Loop,TEXT("Array Element"));auto* Key=F.StructField(Value,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID"));
        auto* Contains=F.Array(TEXT("Array_Contains"),Keys);Connect(Key,FindPin(Contains,TEXT("ItemToFind")));F.Require(F.Not(Contains->GetReturnValuePin()));
        auto* Add=F.Array(TEXT("Array_Add"),Keys);Connect(Key,FindPin(Add,TEXT("NewItem")));F.MapAdd(Map,Key,Value);F.Tail=FindPin(Loop,TEXT("Completed"));
        auto* StagedUpgrades=F.Patch(GetArray(F,LU,Preset),FUpgradeLoadout::StaticStruct(),{{TEXT("Loadout"),Map}});
        auto* Random=F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomItemUpgradeLoadouts"));
        for(auto* V:{GetArray(F,LU,Preset),Random}){auto* Has=F.Self(TEXT("DOR_SourcePriorKeys"));Connect(V,FindPin(Has,TEXT("Upgrades")));Connect(Keys,FindPin(Has,TEXT("Keys")));F.Require(Has->GetReturnValuePin(),TEXT("[DOR-SOURCE]|stage=source-gate|condition=unsupported-existing-equipment-key|mutation=false"));}
        // The journal is widened durably before the first staged equipment store.
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));F.Put(J,JournalClass,TEXT("Schema"),nullptr,TEXT("3"));F.Put(J,JournalClass,TEXT("SourceEquipment"),nullptr,TEXT("true"));
        for(const TCHAR* N:{TEXT("EquipmentStagingIntent"),TEXT("EquipmentEverStaged")})F.Put(J,JournalClass,N,nullptr,TEXT("false"));F.Set(TEXT("DOR_CheckedEquipmentNoMutationTransactionId"),nullptr,TEXT(""));
        F.Put(J,JournalClass,TEXT("EquipmentCharacter"),Id);F.Put(J,JournalClass,TEXT("EquipmentPreset"),Preset);F.Put(J,JournalClass,TEXT("EquipmentKeys"),Keys);
        F.Put(J,JournalClass,TEXT("EquipmentPriorItems"),GetArray(F,LI,Preset));F.Put(J,JournalClass,TEXT("EquipmentPriorRandomItems"),F.StructField(C,FCharacterSave::StaticStruct(),TEXT("RandomWeaponLoadout")));F.Put(J,JournalClass,TEXT("EquipmentStagedItems"),Loadout);
        F.Put(J,JournalClass,TEXT("EquipmentPriorUpgrades"),GetArray(F,LU,Preset));F.Put(J,JournalClass,TEXT("EquipmentPriorRandomUpgrades"),Random);F.Put(J,JournalClass,TEXT("EquipmentStagedUpgrades"),StagedUpgrades);
        auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());
        F.Require(DORSafeSave(F,J,F.Get(TEXT("DOR_JournalSlot"))));auto* Read=DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass);
        auto* Exact=F.Self(TEXT("DOR_SourceJournalReadback"));Connect(J,FindPin(Exact,TEXT("Expected")));Connect(Read,FindPin(Exact,TEXT("Actual")));F.Require(Exact->GetReturnValuePin());
        F.Set(TEXT("DOR_Journal"),Read);F.Set(TEXT("DOR_CheckedEquipmentRestoreTransactionId"),nullptr,TEXT(""));F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceRebindProjection"));auto* Token=F.Input(TEXT("TransactionId"),MakeStringPinType());auto* Loadout=F.Input(TEXT("Loadout"),Items);auto* Selections=F.Input(TEXT("Selections"),SelectionArray);
        F.Require(F.Get(TEXT("DOR_Active")));F.Require(F.Not(F.Get(TEXT("DOR_Blocked"))));auto* J=F.Cache(TEXT("Journal"),F.Get(TEXT("DOR_Journal")));F.Require(F.IsValid(J));
        F.Require(F.Equal(Token,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));F.Require(F.Equal(Token,F.Get(TEXT("DOR_CheckedEquipmentRestoreTransactionId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        F.Require(F.Equal(F.EqInt(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),0),F.EqInt(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),3),TEXT("BooleanOR"),UKismetMathLibrary::StaticClass()));
        auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());
        // Fusion changes only the OC selection, never the accepted equipment
        // identities. Rebinding cannot widen the original equipment key scope.
        F.Require(F.WholeStructSame(Loadout,F.Field(J,JournalClass,TEXT("EquipmentStagedItems"))));auto* Can=F.Self(TEXT("DOR_SourceCanEnroll"));Connect(F.Field(J,JournalClass,TEXT("EquipmentCharacter")),FindPin(Can,TEXT("Character")));Connect(Loadout,FindPin(Can,TEXT("Loadout")));Connect(Selections,FindPin(Can,TEXT("Selections")));F.Require(Can->GetReturnValuePin());
        auto* Prior=F.Field(J,JournalClass,TEXT("EquipmentPriorUpgrades"));auto* Map=F.Cache(TEXT("Map"),F.StructField(Prior,FUpgradeLoadout::StaticStruct(),TEXT("Loadout")));auto* Keys=F.Field(J,JournalClass,TEXT("EquipmentKeys"));auto* Loop=F.Each(Selections);auto* Entry=FindPin(Loop,TEXT("Array Element"));auto* Key=F.StructField(Entry,FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID"));auto* Has=F.Array(TEXT("Array_Contains"),Keys);Connect(Key,FindPin(Has,TEXT("ItemToFind")));F.Require(Has->GetReturnValuePin());F.MapAdd(Map,Key,Entry);F.Tail=FindPin(Loop,TEXT("Completed"));
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));F.Put(J,JournalClass,TEXT("EquipmentStagedUpgrades"),F.Patch(Prior,FUpgradeLoadout::StaticStruct(),{{TEXT("Loadout"),Map}}));auto* Check=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Check,TEXT("Record")));F.Require(Check->GetReturnValuePin());F.Require(DORSafeSave(F,J,F.Get(TEXT("DOR_JournalSlot"))));auto* Read=F.Cache(TEXT("Durable"),DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass));auto* Exact=F.Self(TEXT("DOR_SourceJournalReadback"));Connect(J,FindPin(Exact,TEXT("Expected")));Connect(Read,FindPin(Exact,TEXT("Actual")));F.Require(Exact->GetReturnValuePin());
        F.Set(TEXT("DOR_Journal"),Read);F.Set(TEXT("DOR_CheckedEquipmentRestoreTransactionId"),nullptr,TEXT(""));F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceExtendQualificationScope"));auto* Token=F.Input(TEXT("TransactionId"),MakeStringPinType());
        auto* Pairs=F.Input(TEXT("PairIds"),GuidArray);auto* Schematics=F.Input(TEXT("SchematicIds"),GuidArray);
        F.Require(F.Get(TEXT("DOR_Active")));F.Require(F.Not(F.Get(TEXT("DOR_Blocked"))));auto* J=F.Cache(TEXT("Journal"),F.Get(TEXT("DOR_Journal")));
        F.Require(F.Equal(Token,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
        auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());
        auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());
        F.Require(F.Not(F.Field(J,JournalClass,TEXT("QualificationScopePrepared"))));
        F.Require(F.SameValue(F.Array(TEXT("Array_Length"),Pairs)->GetReturnValuePin(),F.Array(TEXT("Array_Length"),Schematics)->GetReturnValuePin()));
        auto* Count=F.Array(TEXT("Array_Length"),Pairs)->GetReturnValuePin();auto* Range=F.Call(UKismetMathLibrary::StaticClass(),TEXT("InRange_IntInt"));Connect(Count,FindPin(Range,TEXT("Value")));FindPin(Range,TEXT("Min"))->DefaultValue=TEXT("1");FindPin(Range,TEXT("Max"))->DefaultValue=TEXT("8");FindPin(Range,TEXT("InclusiveMin"))->DefaultValue=TEXT("true");FindPin(Range,TEXT("InclusiveMax"))->DefaultValue=TEXT("true");F.Require(Range->GetReturnValuePin());
        auto Enlarge=[&](UEdGraphPin* Inputs,const TCHAR* Field,const TCHAR* Known)
        {
            auto* Scope=F.Cache(TEXT("QualificationScope"),F.Field(J,JournalClass,Field));auto* Loop=F.Each(Inputs);auto* Id=FindPin(Loop,TEXT("Array Element"));
            auto* Contains=F.Array(TEXT("Array_Contains"),F.Get(Known));Connect(Id,FindPin(Contains,TEXT("ItemToFind")));F.Require(Contains->GetReturnValuePin(),TEXT("[DOR-SOURCE]|stage=qualification-scope-rejected|condition=unknown-root-or-schematic|mutation=false"));
            auto* Add=F.Array(TEXT("Array_AddUnique"),Scope);Connect(Id,FindPin(Add,TEXT("NewItem")));F.Tail=FindPin(Loop,TEXT("Completed"));return Scope;
        };
        auto* PairScope=Enlarge(Pairs,TEXT("PairIds"),TEXT("DOR_AllPairIds"));auto* SchematicScope=Enlarge(Schematics,TEXT("SchematicIds"),TEXT("DOR_AllSchematicIds"));
        // The original account snapshots stay untouched. This enrolls existing
        // known mission roots, not their account ownership or client save data.
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));F.Put(J,JournalClass,TEXT("PairIds"),PairScope);F.Put(J,JournalClass,TEXT("SchematicIds"),SchematicScope);
        F.Require(DORSafeSave(F,J,F.Get(TEXT("DOR_JournalSlot"))));auto* Read=DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass);
        auto* Exact=F.Self(TEXT("DOR_SourceJournalReadback"));Connect(J,FindPin(Exact,TEXT("Expected")));Connect(Read,FindPin(Exact,TEXT("Actual")));F.Require(Exact->GetReturnValuePin());
        F.Set(TEXT("DOR_Journal"),Read);F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    // Restoration is separate from qualification cleanup. Its checked receipt
    // can end the synchronous source window while the SAME journal stays active.
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceRestore"));auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JournalClass));
        auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());
        auto* Legacy=F.Branch(F.EqInt(F.Field(J,JournalClass,TEXT("Schema")),2));F.Return(nullptr,TEXT("true"));F.Tail=Legacy->GetElsePin();
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());
        auto* Already=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),FindPin(Already,TEXT("A")));FindPin(Already,TEXT("B"))->DefaultValue=TEXT("3");auto* Completed=F.Branch(Already->GetReturnValuePin());F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Tail=Completed->GetElsePin();
        // Enrollment is not a write. A durably closed/no-entry source lease
        // must not replay its old projection over later third-party progress.
        auto* Staged=F.Branch(F.Field(J,JournalClass,TEXT("EquipmentStagingIntent")));F.Tail=Staged->GetElsePin();F.Set(TEXT("DOR_CheckedEquipmentRestoreTransactionId"),F.Field(J,JournalClass,TEXT("TransactionId")));auto* Never=F.Branch(F.Not(F.Field(J,JournalClass,TEXT("EquipmentEverStaged"))));F.Set(TEXT("DOR_CheckedEquipmentNoMutationTransactionId"),F.Field(J,JournalClass,TEXT("TransactionId")));F.Join(Never->GetElsePin());F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Tail=Staged->GetThenPin();
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame());F.Require(F.IsValid(Save));UEdGraphPin* Array=nullptr;UEdGraphPin* Index=nullptr;
        auto* C=Locate(F,Save,F.Field(J,JournalClass,TEXT("EquipmentCharacter")),Array,Index);auto* Preset=F.Field(J,JournalClass,TEXT("EquipmentPreset"));
        F.Set(TEXT("DOR_SourceConflict"),nullptr,TEXT("false"));auto* Merge=F.Self(TEXT("DOR_SourceMergeCharacter"));Connect(C,FindPin(Merge,TEXT("Current")));Connect(J,FindPin(Merge,TEXT("Record")));auto* Patched=F.Cache(TEXT("MergedCharacter"),Merge->GetReturnValuePin());
        F.Require(F.Not(F.Get(TEXT("DOR_SourceConflict"))),TEXT("[DOR-SOURCE]|stage=restore-conflict|condition=third-party-equipment-field|journal=retained|barrier=held"));
        Replace(F,Array,Index,Patched);F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("CharacterSaves"),Array);
        auto* Commit=F.Self(TEXT("DOR_CommitNativeSave"));F.Require(Commit->GetReturnValuePin());
        auto* Live=F.Cache(TEXT("PostCommitSave"),F.SaveGame());F.Require(F.IsValid(Live));
        auto* PostIdentity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(PostIdentity,TEXT("Record")));F.Require(PostIdentity->GetReturnValuePin());
        UEdGraphPin* PostArray=nullptr;UEdGraphPin* PostIndex=nullptr;auto* Post=Locate(F,Live,F.Field(J,JournalClass,TEXT("EquipmentCharacter")),PostArray,PostIndex);
        F.Set(TEXT("DOR_SourceConflict"),nullptr,TEXT("false"));auto* Verify=F.Self(TEXT("DOR_SourceMergeCharacter"));Connect(Post,FindPin(Verify,TEXT("Current")));Connect(J,FindPin(Verify,TEXT("Record")));F.Require(PayloadSame(F,Post,Verify->GetReturnValuePin()));F.Require(F.Not(F.Get(TEXT("DOR_SourceConflict"))));
        // Commit compares the current complete CharacterSaves value with disk,
        // preserving live progression, unrelated maps, mode and index flags.
        auto* Closed=F.Self(TEXT("DOR_SourceMarkEquipmentLease"));Connect(F.Field(J,JournalClass,TEXT("TransactionId")),FindPin(Closed,TEXT("TransactionId")));FindPin(Closed,TEXT("RestoreChecked"))->DefaultValue=TEXT("true");F.Require(Closed->GetReturnValuePin());F.Set(TEXT("DOR_CheckedEquipmentNoMutationTransactionId"),nullptr,TEXT(""));F.Set(TEXT("DOR_CheckedEquipmentRestoreTransactionId"),F.Field(J,JournalClass,TEXT("TransactionId")));
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    for(const TCHAR* N:{TEXT("DOR_QualificationPlanned"),TEXT("DOR_QualificationAdded"),TEXT("DOR_QualificationAllowed")})AddTypedVariable(BP,N,GuidArray);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourcePlanQualification"));auto* Current=F.Input(TEXT("Current"),GuidArray);auto* Before=F.Input(TEXT("Before"),GuidArray);auto* Scope=F.Input(TEXT("Scope"),GuidArray);auto* Required=F.Input(TEXT("Required"),GuidArray);F.Set(TEXT("DOR_QualificationPlanned"),Current);for(const TCHAR* N:{TEXT("DOR_QualificationAdded"),TEXT("DOR_QualificationAllowed")})F.Array(TEXT("Array_Clear"),F.Get(N));auto* Seen=F.Cache(TEXT("Seen"),Scope);F.Array(TEXT("Array_Clear"),Seen);
        auto Has=[&](UEdGraphPin* Array,UEdGraphPin* Id){auto* N=F.Array(TEXT("Array_Contains"),Array);Connect(Id,FindPin(N,TEXT("ItemToFind")));return N->GetReturnValuePin();};auto Add=[&](UEdGraphPin* Array,UEdGraphPin* Id){auto* N=F.Array(TEXT("Array_AddUnique"),Array);Connect(Id,FindPin(N,TEXT("NewItem")));};
        auto* Loop=F.Each(Scope);auto* Id=FindPin(Loop,TEXT("Array Element"));F.Require(F.Not(Has(Seen,Id)));Add(Seen,Id);auto* MissingPrior=F.Branch(F.Not(Has(Before,Id)));Add(F.Get(TEXT("DOR_QualificationAllowed")),Id);F.Join(MissingPrior->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Array(TEXT("Array_Clear"),Seen);Loop=F.Each(Required);Id=FindPin(Loop,TEXT("Array Element"));F.Require(Has(Scope,Id));F.Require(F.Not(Has(Seen,Id)));Add(Seen,Id);auto* MissingCurrent=F.Branch(F.Not(Has(Current,Id)));F.Require(Has(F.Get(TEXT("DOR_QualificationAllowed")),Id),TEXT("[DOR-NATIVE]|stage=qualification-plan-rejected|condition=prior-held-entry-removed-by-other-writer|barrier=retained"));Add(F.Get(TEXT("DOR_QualificationAdded")),Id);Add(F.Get(TEXT("DOR_QualificationPlanned")),Id);F.Join(MissingCurrent->GetElsePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceGrantQualification"));auto* Token=F.Input(TEXT("TransactionId"),MakeStringPinType());auto* Selective=F.Input(TEXT("Selective"),MakeBoolPinType());auto* RequiredPairInput=F.Input(TEXT("RequiredPairs"),GuidArray);auto* RequiredSchemaInput=F.Input(TEXT("RequiredSchematics"),GuidArray);
        F.Require(F.Get(TEXT("DOR_Active")));F.Require(F.Not(F.Get(TEXT("DOR_Blocked"))));auto* J=F.Cache(TEXT("Journal"),F.Get(TEXT("DOR_Journal")));F.Require(F.IsValid(J));F.Require(F.Not(F.Field(J,JournalClass,TEXT("QualificationScopePrepared"))));
        F.Require(F.Equal(Token,F.Field(J,JournalClass,TEXT("TransactionId")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));auto* Identity=F.Self(TEXT("DOR_CheckIdentity"));Connect(J,FindPin(Identity,TEXT("Record")));F.Require(Identity->GetReturnValuePin());auto* Valid=F.Self(TEXT("DOR_SourceValidateRecord"));Connect(J,FindPin(Valid,TEXT("Record")));F.Require(Valid->GetReturnValuePin());F.Require(F.EqInt(F.Field(J,JournalClass,TEXT("NativeProtocolPhase")),0));
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame());F.Require(F.IsValid(Save));auto* SchematicSave=F.Cache(TEXT("Schematic"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")));
        auto* BeforePurchased=F.Cache(TEXT("BeforePurchased"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades")));auto* Purchased=F.Cache(TEXT("Purchased"),BeforePurchased);auto* Owned=F.Cache(TEXT("Owned"),F.StructField(SchematicSave,FSchematicSave::StaticStruct(),TEXT("OwnedSchematics")));auto* Forged=F.Cache(TEXT("Forged"),F.StructField(SchematicSave,FSchematicSave::StaticStruct(),TEXT("ForgedSchematics")));
        auto* RequiredPairs=F.Cache(TEXT("RequiredPairs"),F.Field(J,JournalClass,TEXT("PairIds")));auto* RequiredSchematics=F.Cache(TEXT("RequiredSchematics"),F.Field(J,JournalClass,TEXT("SchematicIds")));auto* Restrict=F.Branch(Selective);F.Set(F.Variable(RequiredPairs),RequiredPairInput);F.Set(F.Variable(RequiredSchematics),RequiredSchemaInput);F.Join(Restrict->GetElsePin());F.Require(F.SameValue(F.Array(TEXT("Array_Length"),RequiredPairs)->GetReturnValuePin(),F.Array(TEXT("Array_Length"),RequiredSchematics)->GetReturnValuePin()));
        auto AddScope=[&](UEdGraphPin* Current,const TCHAR* Scope,const TCHAR* BeforeName,const TCHAR* AddedName,const TCHAR* AllowedName,UEdGraphPin* Required)
        {auto* Plan=F.Self(TEXT("DOR_SourcePlanQualification"));Connect(Current,FindPin(Plan,TEXT("Current")));Connect(F.Field(J,JournalClass,BeforeName),FindPin(Plan,TEXT("Before")));Connect(F.Field(J,JournalClass,Scope),FindPin(Plan,TEXT("Scope")));Connect(Required,FindPin(Plan,TEXT("Required")));F.Require(Plan->GetReturnValuePin());F.Set(CastChecked<UK2Node_VariableGet>(Current->GetOwningNode())->VariableReference.GetMemberName(),F.Get(TEXT("DOR_QualificationPlanned")));return TArray<UEdGraphPin*>{F.Cache(AddedName,F.Get(TEXT("DOR_QualificationAdded"))),F.Cache(AllowedName,F.Get(TEXT("DOR_QualificationAllowed")))};};
        const auto PurchasePlan=AddScope(Purchased,TEXT("PairIds"),TEXT("BeforePurchased"),TEXT("QualificationAddedPurchased"),TEXT("QualificationAllowedPurchased"),RequiredPairs);const auto OwnedPlan=AddScope(Owned,TEXT("SchematicIds"),TEXT("BeforeOwned"),TEXT("QualificationAddedOwned"),TEXT("QualificationAllowedOwned"),RequiredSchematics);const auto ForgedPlan=AddScope(Forged,TEXT("SchematicIds"),TEXT("BeforeForged"),TEXT("QualificationAddedForged"),TEXT("QualificationAllowedForged"),RequiredSchematics);for(const auto& P:{TPair<const TCHAR*,UEdGraphPin*>(TEXT("QualificationAddedPurchased"),PurchasePlan[0]),TPair<const TCHAR*,UEdGraphPin*>(TEXT("QualificationAllowedPurchased"),PurchasePlan[1]),TPair<const TCHAR*,UEdGraphPin*>(TEXT("QualificationAddedOwned"),OwnedPlan[0]),TPair<const TCHAR*,UEdGraphPin*>(TEXT("QualificationAllowedOwned"),OwnedPlan[1]),TPair<const TCHAR*,UEdGraphPin*>(TEXT("QualificationAddedForged"),ForgedPlan[0]),TPair<const TCHAR*,UEdGraphPin*>(TEXT("QualificationAllowedForged"),ForgedPlan[1])})F.Put(J,JournalClass,P.Key,P.Value);F.Put(J,JournalClass,TEXT("QualificationScopePrepared"),nullptr,TEXT("true"));
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("true"));F.Require(DORSafeSave(F,J,F.Get(TEXT("DOR_JournalSlot"))));auto* Read=F.Cache(TEXT("Durable"),DORSafeLoad(F,F.Get(TEXT("DOR_JournalSlot")),JournalClass));F.Require(F.IsValid(Read));auto* Checked=F.Self(TEXT("DOR_SourceJournalReadback"));Connect(J,FindPin(Checked,TEXT("Expected")));Connect(Read,FindPin(Checked,TEXT("Actual")));F.Require(Checked->GetReturnValuePin());
        F.Set(TEXT("DOR_Journal"),Read);auto* StillOwn=F.Self(TEXT("DOR_CheckIdentity"));Connect(Read,FindPin(StillOwn,TEXT("Record")));F.Require(StillOwn->GetReturnValuePin());F.Require(F.Equal(F.SaveGame(),Save,TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));F.Require(F.SameArray(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades")),BeforePurchased));F.Require(F.WholeStructSame(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")),SchematicSave));F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades"),Purchased);F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave"),F.Patch(SchematicSave,FSchematicSave::StaticStruct(),{{TEXT("OwnedSchematics"),Owned},{TEXT("ForgedSchematics"),Forged}}));
        for(const auto& P:{TPair<UEdGraphPin*,UEdGraphPin*>(RequiredPairs,F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades"))),TPair<UEdGraphPin*,UEdGraphPin*>(RequiredSchematics,F.StructField(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")),FSchematicSave::StaticStruct(),TEXT("OwnedSchematics"))),TPair<UEdGraphPin*,UEdGraphPin*>(RequiredSchematics,F.StructField(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")),FSchematicSave::StaticStruct(),TEXT("ForgedSchematics")))})
        {auto* Loop=F.Each(P.Key);auto* Has=F.Array(TEXT("Array_Contains"),P.Value);Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Has,TEXT("ItemToFind")));F.Require(Has->GetReturnValuePin(),TEXT("[DOR-SOURCE]|stage=qualification-readback-failed|journal=retained|barrier=held"));F.Tail=FindPin(Loop,TEXT("Completed"));}
        F.Set(TEXT("DOR_Blocked"),nullptr,TEXT("false"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSourceGraph F(BP,TEXT("DOR_SourceRestoreMembership"));auto* Current=F.Input(TEXT("Current"),GuidArray);auto* Before=F.Input(TEXT("Before"),GuidArray);auto* Ids=F.Input(TEXT("Ids"),GuidArray);auto* Cleanup=F.Input(TEXT("Cleanup"),MakeBoolPinType());auto* J=F.Input(TEXT("Record"),MakeObjectPinType(JournalClass));auto* Role=F.Input(TEXT("Role"),MakeIntPinType());
        auto* New=F.Branch(F.EqInt(F.Field(J,JournalClass,TEXT("Schema")),3));
        auto* Prepared=F.Branch(F.Field(J,JournalClass,TEXT("QualificationScopePrepared")));F.Tail=Prepared->GetElsePin();F.Return(Current);F.Tail=Prepared->GetThenPin();
        auto* Added=F.Cache(TEXT("Added"),F.Field(J,JournalClass,TEXT("QualificationAddedPurchased")));auto* IsOwned=F.Branch(F.EqInt(Role,1));F.Set(F.Variable(Added),F.Field(J,JournalClass,TEXT("QualificationAddedOwned")));F.Join(IsOwned->GetElsePin());auto* IsForged=F.Branch(F.EqInt(Role,2));F.Set(F.Variable(Added),F.Field(J,JournalClass,TEXT("QualificationAddedForged")));F.Join(IsForged->GetElsePin());
        // Scope enrollment alone is not a write. Restore only the 0->1
        // additions durably enrolled for this particular membership array.
        auto* Result=F.Cache(TEXT("Result"),Current);auto* Loop=F.Each(Added);auto* Remove=F.Array(TEXT("Array_RemoveItem"),Result);Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Remove,TEXT("Item")));F.Tail=FindPin(Loop,TEXT("Completed"));F.Return(Result);
        F.Tail=New->GetElsePin();auto* Legacy=F.Self(TEXT("DOR_RestoreMembership"));Connect(Current,FindPin(Legacy,TEXT("Current")));Connect(Before,FindPin(Legacy,TEXT("Before")));Connect(Ids,FindPin(Legacy,TEXT("Ids")));Connect(Cleanup,FindPin(Legacy,TEXT("Cleanup")));F.Return(Legacy->GetReturnValuePin());F.Compile();
    }
    // Extend identity acceptance without changing V2 journal filenames/classes.
    {
        auto* G=FindFunctionGraph(BP,TEXT("DOR_CheckIdentity"));UK2Node_CallFunction* Old=nullptr;
        for(auto* Node:G->Nodes)if(auto* N=Cast<UK2Node_CallFunction>(Node))if(N->FunctionReference.GetMemberName()==TEXT("EqualEqual_IntInt")&&FindPin(N,TEXT("B"))->DefaultValue==TEXT("2"))Old=N;
        if(!Old)return false;auto* Three=AddCall(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"),0,-1000);Connect(FindPin(Old,TEXT("A"))->LinkedTo[0],FindPin(Three,TEXT("A")));FindPin(Three,TEXT("B"))->DefaultValue=TEXT("3");
        auto* Or=AddCall(G,UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"),300,-1000);auto Targets=Old->GetReturnValuePin()->LinkedTo;Old->GetReturnValuePin()->BreakAllPinLinks();Connect(Old->GetReturnValuePin(),FindPin(Or,TEXT("A")));Connect(Three->GetReturnValuePin(),FindPin(Or,TEXT("B")));for(auto* T:Targets)Connect(Or->GetReturnValuePin(),T);
    }
    // Normal and abnormal-startup recovery must resolve wider keys BEFORE the
    // old DOR-only merge, commit, deletion, and final transaction receipt.
    {
        auto* G=FindFunctionGraph(BP,TEXT("DOR_RestoreJournal"));UK2Node_CallFunction* Check=nullptr;UK2Node_VariableSet* Marker=nullptr;
        for(auto* Node:G->Nodes){if(auto* N=Cast<UK2Node_CallFunction>(Node))if(N->FunctionReference.GetMemberName()==TEXT("DOR_ValidateJournal"))Check=N;
            if(auto* N=Cast<UK2Node_VariableSet>(Node))if(N->VariableReference.GetMemberName()==TEXT("DOR_LastError")&&FindPin(N,TEXT("DOR_LastError"))->DefaultValue.Contains(TEXT("[R20]")))Marker=N;}
        if(!Check||!Marker)return false;auto Sources=Marker->GetExecPin()->LinkedTo;Marker->GetExecPin()->BreakAllPinLinks();
        auto* N=AddSelfCall(G,TEXT("DOR_SourceRestore"),0,-1300);Connect(FindPin(Check,TEXT("Record"))->LinkedTo[0],FindPin(N,TEXT("Record")));for(auto* From:Sources)Connect(From,N->GetExecPin());
        auto* B=AddNode<UK2Node_IfThenElse>(G,300,-1300);B->AllocateDefaultPins();Connect(N->GetThenPin(),B->GetExecPin());Connect(N->GetReturnValuePin(),B->GetConditionPin());Connect(B->GetThenPin(),Marker->GetExecPin());
        auto* R=AddNode<UK2Node_FunctionResult>(G,600,-1300);R->FunctionReference=FindFunctionEntry(G)->FunctionReference;R->AllocateDefaultPins();auto* V=R->FindPin(TEXT("ReturnValue"));if(!V){V=R->CreateUserDefinedPin(TEXT("ReturnValue"),MakeBoolPinType(),EGPD_Input);const FName Generated=V->PinName;V->PinName=TEXT("ReturnValue");for(auto& P:R->UserDefinedPins)if(P->PinName==Generated)P->PinName=TEXT("ReturnValue");}V->DefaultValue=TEXT("false");Connect(B->GetElsePin(),FindPin(R,UEdGraphSchema_K2::PN_Execute));
        auto* Record=FindPin(Check,TEXT("Record"))->LinkedTo[0];TArray<UK2Node_CallFunction*> OldEquipment,Membership;
        for(auto* Node:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(Node))
        {
            const auto Name=C->FunctionReference.GetMemberName();
            if(Name==TEXT("DOR_MergeCharacters")||Name==TEXT("DOR_MergeLoadouts"))OldEquipment.Add(C);
            else if(Name==TEXT("DOR_RestoreMembership"))Membership.Add(C);
        }
        if(OldEquipment.Num()!=2||Membership.Num()!=3)return false;
        // Schema 3 already merged exactly its consumed source fields. Applying
        // the old GUID sweep afterward would touch unjournaled presets/maps.
        for(auto* C:OldEquipment)
        {
            auto* Store=C->GetThenPin()->LinkedTo.Num()==1?Cast<UK2Node_VariableSet>(C->GetThenPin()->LinkedTo[0]->GetOwningNode()):nullptr;if(!Store)return false;
            auto Inputs=C->GetExecPin()->LinkedTo;C->GetExecPin()->BreakAllPinLinks();
            auto* Schema=AddExternalVariableGet(G,TEXT("Schema"),JournalClass,0,-1800);Connect(Record,FindPin(Schema,UEdGraphSchema_K2::PN_Self));auto* Two=AddCall(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"),200,-1800);Connect(Schema->GetValuePin(),FindPin(Two,TEXT("A")));FindPin(Two,TEXT("B"))->DefaultValue=TEXT("2");
            auto* Branch=AddNode<UK2Node_IfThenElse>(G,400,-1800);Branch->AllocateDefaultPins();for(auto* Input:Inputs)Connect(Input,Branch->GetExecPin());Connect(Two->GetReturnValuePin(),Branch->GetConditionPin());Connect(Branch->GetThenPin(),C->GetExecPin());for(auto* Target:Store->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo)Connect(Branch->GetElsePin(),Target);
        }
        for(auto* C:Membership)
        {auto* BeforePin=FindPin(C,TEXT("Before"));auto* BeforeGet=BeforePin->LinkedTo.Num()==1?Cast<UK2Node_VariableGet>(BeforePin->LinkedTo[0]->GetOwningNode()):nullptr;if(!BeforeGet)return false;const FName Name=BeforeGet->VariableReference.GetMemberName();const int32 Role=Name==TEXT("BeforePurchased")?0:Name==TEXT("BeforeOwned")?1:Name==TEXT("BeforeForged")?2:-1;if(Role<0)return false;C->FunctionReference.SetSelfMember(TEXT("DOR_SourceRestoreMembership"));C->ReconstructNode();Connect(Record,FindPin(C,TEXT("Record")));FindPin(C,TEXT("Role"))->DefaultValue=FString::FromInt(Role);}
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
}

// Exercise the emitted VM merge functions on synthetic values in an editor
// preview world. No FSDSaveGame/native initializer or disk writer is invoked.
bool ValidateDORNativeSourceMerges(UBlueprint* BP)
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld* World=UWorld::CreateWorld(EWorldType::EditorPreview,false);
    if(!World)return false;
    auto* Object=World->SpawnActor(BP->GeneratedClass);
    auto* Conflict=FindFProperty<FBoolProperty>(BP->GeneratedClass,TEXT("DOR_SourceConflict"));
    bool Passed=Object && Conflict;int32 Checks=0;
    auto Fill=[&](UScriptStruct* S,uint8* Data,int32 Seed)
    {
        int32 Index=0;
        for(TFieldIterator<FProperty> I(S);I;++I)
        {
            auto* P=*I;void* V=P->ContainerPtrToValuePtr<void>(Data);++Index;
            if(auto* B=CastField<FBoolProperty>(P))B->SetPropertyValue(V,(Seed%2)!=0);
            else if(auto* Int=CastField<FIntProperty>(P))Int->SetPropertyValue(V,Seed);
            else if(auto* Guid=CastField<FStructProperty>(P))
            {if(Guid->Struct!=TBaseStructure<FGuid>::Get())return false;*static_cast<FGuid*>(V)=FGuid(Seed,Index,Seed+Index,1);}
            else if(auto* Array=CastField<FArrayProperty>(P))
            {auto* Inner=CastField<FStructProperty>(Array->Inner);if(!Inner||Inner->Struct!=TBaseStructure<FGuid>::Get())return false;
                FScriptArrayHelper H(Array,V);H.Resize(2);*reinterpret_cast<FGuid*>(H.GetRawPtr(0))=FGuid(Seed,Index,1,1);*reinterpret_cast<FGuid*>(H.GetRawPtr(1))=FGuid(Seed,Index,2,1);}
            else return false;
        }
        return true;
    };
    for(bool Items:{true,false})
    {
        auto* S=Items?FItemLoadout::StaticStruct():FItemUpgradeSelection::StaticStruct();
        auto* Fn=BP->GeneratedClass->FindFunctionByName(Items?TEXT("DOR_SourceMergeItems"):TEXT("DOR_SourceMergeSelection"));
        if(!Passed||!Fn){Passed=false;break;}
        FStructOnScope Prior(S),Staged(S),Third(S);
        if(!Fill(S,Prior.GetStructMemory(),11)||!Fill(S,Staged.GetStructMemory(),22)||!Fill(S,Third.GetStructMemory(),33)){Passed=false;break;}
        auto Run=[&](const uint8* Current,const uint8* Expected,bool ExpectedConflict,const uint8* P,const uint8* T)
        {
            FStructOnScope Params(Fn);
            for(const auto& V:{TPair<const TCHAR*,const uint8*>(TEXT("Current"),Current),TPair<const TCHAR*,const uint8*>(TEXT("Prior"),P),TPair<const TCHAR*,const uint8*>(TEXT("Staged"),T)})
            {auto* Property=FindFProperty<FStructProperty>(Fn,V.Key);if(!Property||Property->Struct!=S)return false;Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),V.Value);}
            auto* Result=FindFProperty<FStructProperty>(Fn,TEXT("ReturnValue"));if(!Result||Result->Struct!=S)return false;
            Conflict->SetPropertyValue_InContainer(Object,false);Object->ProcessEvent(Fn,Params.GetStructMemory());++Checks;
            const bool OK=Conflict->GetPropertyValue_InContainer(Object)==ExpectedConflict && S->CompareScriptStruct(Result->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),Expected,0);
            if(!OK)
            {
                UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM failed function=%s case=%d conflict=%d expectedConflict=%d"),*Fn->GetName(),Checks,Conflict->GetPropertyValue_InContainer(Object),ExpectedConflict);
                const void* Actual=Result->ContainerPtrToValuePtr<void>(Params.GetStructMemory());
                for(TFieldIterator<FProperty> I(S);I;++I)if(!I->Identical_InContainer(Actual,Expected))
                {FString A,E;I->ExportText_InContainer(0,A,Actual,Actual,nullptr,PPF_None);I->ExportText_InContainer(0,E,Expected,Expected,nullptr,PPF_None);UE_LOG(LogTemp,Display,TEXT("DOR-SOURCE-VM mismatch field=%s actual=%s expected=%s"),*I->GetName(),*A,*E);}
            }
            return OK;
        };
        Passed=Run(Staged.GetStructMemory(),Prior.GetStructMemory(),false,Prior.GetStructMemory(),Staged.GetStructMemory()) &&
            Run(Prior.GetStructMemory(),Prior.GetStructMemory(),false,Prior.GetStructMemory(),Staged.GetStructMemory());
        for(TFieldIterator<FProperty> I(S);I && Passed;++I)
        {
            auto* Field=*I;
            if(CastField<FBoolProperty>(Field))continue;
            FStructOnScope Current(S),Expected(S),UnchangedStage(S);
            S->CopyScriptStruct(Current.GetStructMemory(),Staged.GetStructMemory());S->CopyScriptStruct(Expected.GetStructMemory(),Prior.GetStructMemory());
            for(auto* V:{Current.GetStructMemory(),Expected.GetStructMemory()})Field->CopyCompleteValue(Field->ContainerPtrToValuePtr<void>(V),Field->ContainerPtrToValuePtr<void>(Third.GetStructMemory()));
            Passed=Run(Current.GetStructMemory(),Expected.GetStructMemory(),true,Prior.GetStructMemory(),Staged.GetStructMemory());
            S->CopyScriptStruct(UnchangedStage.GetStructMemory(),Staged.GetStructMemory());Field->CopyCompleteValue(Field->ContainerPtrToValuePtr<void>(UnchangedStage.GetStructMemory()),Field->ContainerPtrToValuePtr<void>(Prior.GetStructMemory()));
            Passed=Passed && Run(Current.GetStructMemory(),Expected.GetStructMemory(),false,Prior.GetStructMemory(),UnchangedStage.GetStructMemory());
        }
    }
    if(Passed)
    {
        auto* JournalProperty=FindFProperty<FObjectProperty>(BP->GeneratedClass,TEXT("DOR_Journal"));
        auto* J=JournalProperty?NewObject<UObject>(GetTransientPackage(),JournalProperty->PropertyClass):nullptr;
        auto* Fn=BP->GeneratedClass->FindFunctionByName(TEXT("DOR_SourceMergeCharacter"));
        auto* MapProperty=FindFProperty<FMapProperty>(FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));
        Passed=J && Fn && MapProperty;
        if(Passed)
        {
            FStructOnScope PriorItems(FItemLoadout::StaticStruct()),RandomPriorItems(FItemLoadout::StaticStruct()),StagedItems(FItemLoadout::StaticStruct()),OtherItems(FItemLoadout::StaticStruct());
            FStructOnScope PriorUpgrades(FUpgradeLoadout::StaticStruct()),RandomPriorUpgrades(FUpgradeLoadout::StaticStruct()),StagedUpgrades(FUpgradeLoadout::StaticStruct()),CurrentUpgrades(FUpgradeLoadout::StaticStruct()),ExpectedUpgrades(FUpgradeLoadout::StaticStruct()),RandomExpectedUpgrades(FUpgradeLoadout::StaticStruct());
            FStructOnScope Current(FCharacterSave::StaticStruct()),Expected(FCharacterSave::StaticStruct());
            Fill(FItemLoadout::StaticStruct(),PriorItems.GetStructMemory(),11);Fill(FItemLoadout::StaticStruct(),RandomPriorItems.GetStructMemory(),44);Fill(FItemLoadout::StaticStruct(),OtherItems.GetStructMemory(),33);
            const FGuid Character(7,8,9,10);TArray<FGuid> Keys;
            auto AddSelection=[&](uint8* UpgradeData,FGuid Key,int32 Seed)
            {
                FStructOnScope Value(FItemUpgradeSelection::StaticStruct());Fill(FItemUpgradeSelection::StaticStruct(),Value.GetStructMemory(),Seed);
                *FindFProperty<FStructProperty>(FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID"))->ContainerPtrToValuePtr<FGuid>(Value.GetStructMemory())=Key;
                FScriptMapHelper M(MapProperty,MapProperty->ContainerPtrToValuePtr<void>(UpgradeData));M.AddPair(&Key,Value.GetStructMemory());
            };
            for(int32 K=0;K<8;++K){const FGuid Key(1,2,3,K+1);Keys.Add(Key);AddSelection(PriorUpgrades.GetStructMemory(),Key,11);AddSelection(RandomPriorUpgrades.GetStructMemory(),Key,44);AddSelection(StagedUpgrades.GetStructMemory(),Key,22);AddSelection(CurrentUpgrades.GetStructMemory(),Key,22);AddSelection(ExpectedUpgrades.GetStructMemory(),Key,11);AddSelection(RandomExpectedUpgrades.GetStructMemory(),Key,44);}
            auto* Sparse=reinterpret_cast<FItemLoadout*>(StagedItems.GetStructMemory());Sparse->PrimaryWeapon=FGuid();Sparse->SecondaryWeapon=Keys[1];Sparse->iconIndex=22;
            const FGuid Extra(99,98,97,96);AddSelection(PriorUpgrades.GetStructMemory(),Extra,11);AddSelection(StagedUpgrades.GetStructMemory(),Extra,11);
            AddSelection(CurrentUpgrades.GetStructMemory(),Extra,33);AddSelection(ExpectedUpgrades.GetStructMemory(),Extra,33);
            AddSelection(RandomPriorUpgrades.GetStructMemory(),Extra,11);AddSelection(RandomExpectedUpgrades.GetStructMemory(),Extra,33);
            auto JournalStruct=[&](const TCHAR* Name,const void* Data){auto* P=FindFProperty<FStructProperty>(J->GetClass(),Name);P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(J),Data);};
            JournalStruct(TEXT("EquipmentCharacter"),&Character);FindFProperty<FIntProperty>(J->GetClass(),TEXT("EquipmentPreset"))->SetPropertyValue_InContainer(J,0);
            FindFProperty<FArrayProperty>(J->GetClass(),TEXT("EquipmentKeys"))->CopyCompleteValue(FindFProperty<FArrayProperty>(J->GetClass(),TEXT("EquipmentKeys"))->ContainerPtrToValuePtr<void>(J),&Keys);
            JournalStruct(TEXT("EquipmentPriorItems"),PriorItems.GetStructMemory());JournalStruct(TEXT("EquipmentPriorRandomItems"),RandomPriorItems.GetStructMemory());JournalStruct(TEXT("EquipmentStagedItems"),StagedItems.GetStructMemory());
            JournalStruct(TEXT("EquipmentPriorUpgrades"),PriorUpgrades.GetStructMemory());JournalStruct(TEXT("EquipmentPriorRandomUpgrades"),RandomPriorUpgrades.GetStructMemory());JournalStruct(TEXT("EquipmentStagedUpgrades"),StagedUpgrades.GetStructMemory());
            FindFProperty<FIntProperty>(J->GetClass(),TEXT("Schema"))->SetPropertyValue_InContainer(J,3);FindFProperty<FBoolProperty>(J->GetClass(),TEXT("SourceEquipment"))->SetPropertyValue_InContainer(J,true);
            auto CheckRecord=[&](bool ExpectedResult,const TCHAR* Stage){auto* Valid=Object->FindFunction(TEXT("DOR_SourceValidateRecord"));FStructOnScope P(Valid);FindFProperty<FObjectProperty>(Valid,TEXT("Record"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),J);Object->ProcessEvent(Valid,P.GetStructMemory());const bool OK=FindFProperty<FBoolProperty>(Valid,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory())==ExpectedResult;++Checks;Passed=Passed&&OK;if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM record=%s failed"),Stage);};
            CheckRecord(true,TEXT("sparse-schema3-full-distinct-priors"));
            auto InvalidSparse=*Sparse;InvalidSparse.SecondaryWeapon=FGuid();JournalStruct(TEXT("EquipmentStagedItems"),&InvalidSparse);CheckRecord(false,TEXT("schema3-both-weapons-zero-rejected"));InvalidSparse=*Sparse;InvalidSparse.SecondaryWeapon=FGuid(99,98,97,96);JournalStruct(TEXT("EquipmentStagedItems"),&InvalidSparse);CheckRecord(false,TEXT("schema3-populated-weapon-membership-rejected"));InvalidSparse=*Sparse;InvalidSparse.TraversalTool=InvalidSparse.SecondaryWeapon;JournalStruct(TEXT("EquipmentStagedItems"),&InvalidSparse);CheckRecord(false,TEXT("schema3-populated-equipment-duplicate-rejected"));JournalStruct(TEXT("EquipmentStagedItems"),Sparse);
            auto* Read=DuplicateObject<UObject>(J,GetTransientPackage());auto CheckRead=[&](bool ExpectedResult,const TCHAR* Stage){auto* Exact=Object->FindFunction(TEXT("DOR_SourceJournalReadback"));FStructOnScope P(Exact);FindFProperty<FObjectProperty>(Exact,TEXT("Expected"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),J);FindFProperty<FObjectProperty>(Exact,TEXT("Actual"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),Read);Object->ProcessEvent(Exact,P.GetStructMemory());const bool OK=FindFProperty<FBoolProperty>(Exact,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory())==ExpectedResult;++Checks;Passed=Passed&&OK;if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM typed-readback=%s failed"),Stage);};CheckRead(true,TEXT("sparse-zeros-and-eight-selections-exact"));
            auto* ReadItems=FindFProperty<FStructProperty>(Read->GetClass(),TEXT("EquipmentStagedItems"))->ContainerPtrToValuePtr<FItemLoadout>(Read);ReadItems->PrimaryWeapon=Keys[0];CheckRead(false,TEXT("raw-default-primary-fill-readback-rejected"));ReadItems->PrimaryWeapon=FGuid();ReadItems->TraversalTool=Keys[2];CheckRead(false,TEXT("zero-auxiliary-readback-change-rejected"));ReadItems->TraversalTool=FGuid();ReadItems->iconIndex+=1;CheckRead(false,TEXT("icon-readback-change-rejected"));ReadItems->iconIndex-=1;
            auto* ReadUpgrades=FindFProperty<FStructProperty>(Read->GetClass(),TEXT("EquipmentStagedUpgrades"))->ContainerPtrToValuePtr<FUpgradeLoadout>(Read);FScriptMapHelper ReadMap(MapProperty,MapProperty->ContainerPtrToValuePtr<void>(ReadUpgrades));const int32 ReadAt=ReadMap.FindMapIndexWithKey(&Keys[7]);auto* ReadSelection=reinterpret_cast<FItemUpgradeSelection*>(ReadMap.GetValuePtr(ReadAt));const FGuid BeforeOC=ReadSelection->EquippedOverclock;ReadSelection->EquippedOverclock=FGuid(90,91,92,93);CheckRead(false,TEXT("eighth-typed-selection-readback-change-rejected"));ReadSelection->EquippedOverclock=BeforeOC;
            TArray<FItemUpgradeSelection> Inputs;FScriptMapHelper StageMap(MapProperty,MapProperty->ContainerPtrToValuePtr<void>(StagedUpgrades.GetStructMemory()));for(const auto& Key:Keys)Inputs.Add(*reinterpret_cast<FItemUpgradeSelection*>(StageMap.GetValuePtr(StageMap.FindMapIndexWithKey(&Key))));
            auto CheckInputs=[&](const FItemLoadout& L,bool ExpectedResult,const TCHAR* Stage){auto* Exact=Object->FindFunction(TEXT("DOR_SourceInputsMatch"));FStructOnScope P(Exact);FindFProperty<FObjectProperty>(Exact,TEXT("Record"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(),J);for(const auto& V:{TPair<const TCHAR*,const void*>(TEXT("Loadout"),&L),TPair<const TCHAR*,const void*>(TEXT("Selections"),&Inputs)}){auto* Prop=Exact->FindPropertyByName(V.Key);Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(P.GetStructMemory()),V.Value);}Object->ProcessEvent(Exact,P.GetStructMemory());const bool OK=FindFProperty<FBoolProperty>(Exact,TEXT("ReturnValue"))->GetPropertyValue_InContainer(P.GetStructMemory())==ExpectedResult;++Checks;Passed=Passed&&OK;if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM exact-input=%s failed"),Stage);};CheckInputs(*Sparse,true,TEXT("raw-sparse-and-eight-typed-selections"));auto Filled=*Sparse;Filled.TraversalTool=Keys[2];CheckInputs(Filled,false,TEXT("filled-raw-zero-rejected"));Inputs[7].EquippedOverclock=FGuid(80,81,82,83);CheckInputs(*Sparse,false,TEXT("changed-eighth-selection-rejected"));
            FItemLoadout Full;int32 FieldIndex=0;for(const TCHAR* N:{TEXT("PrimaryWeapon"),TEXT("SecondaryWeapon"),TEXT("TraversalTool"),TEXT("ClassTool"),TEXT("Armor"),TEXT("Flare"),TEXT("MiningTool"),TEXT("Grenade")})*FindFProperty<FStructProperty>(FItemLoadout::StaticStruct(),N)->ContainerPtrToValuePtr<FGuid>(&Full)=Keys[FieldIndex++];JournalStruct(TEXT("EquipmentStagedItems"),&Full);CheckRecord(true,TEXT("full-schema3-still-accepted"));JournalStruct(TEXT("EquipmentStagedItems"),StagedItems.GetStructMemory());
            auto CharacterStruct=[&](uint8* Data,const TCHAR* Name,const void* V){auto* P=FindFProperty<FStructProperty>(FCharacterSave::StaticStruct(),Name);P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Data),V);};
            CharacterStruct(Current.GetStructMemory(),TEXT("SavegameID"),&Character);
            FindFProperty<FIntProperty>(FCharacterSave::StaticStruct(),TEXT("XP"))->SetPropertyValue_InContainer(Current.GetStructMemory(),777);
            FindFProperty<FIntProperty>(FCharacterSave::StaticStruct(),TEXT("TimesRetired"))->SetPropertyValue_InContainer(Current.GetStructMemory(),9);
            // A third party selected another preset while the equipment lease
            // was active. Restore the captured keys and preserve that selector.
            FindFProperty<FIntProperty>(FCharacterSave::StaticStruct(),TEXT("SelectedLoadout"))->SetPropertyValue_InContainer(Current.GetStructMemory(),1);
            for(const auto& Pair:{TPair<const TCHAR*,uint8*>(TEXT("Loadouts"),StagedItems.GetStructMemory()),TPair<const TCHAR*,uint8*>(TEXT("ItemUpgradeLoadouts"),CurrentUpgrades.GetStructMemory())})
            {auto* P=FindFProperty<FArrayProperty>(FCharacterSave::StaticStruct(),Pair.Key);FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Current.GetStructMemory()));A.Resize(2);P->Inner->CopyCompleteValue(A.GetRawPtr(0),Pair.Value);P->Inner->CopyCompleteValue(A.GetRawPtr(1),FString(Pair.Key)==TEXT("Loadouts")?OtherItems.GetStructMemory():PriorUpgrades.GetStructMemory());}
            CharacterStruct(Current.GetStructMemory(),TEXT("RandomWeaponLoadout"),StagedItems.GetStructMemory());CharacterStruct(Current.GetStructMemory(),TEXT("RandomItemUpgradeLoadouts"),CurrentUpgrades.GetStructMemory());
            FCharacterSave::StaticStruct()->CopyScriptStruct(Expected.GetStructMemory(),Current.GetStructMemory());
            for(const auto& Pair:{TPair<const TCHAR*,uint8*>(TEXT("Loadouts"),PriorItems.GetStructMemory()),TPair<const TCHAR*,uint8*>(TEXT("ItemUpgradeLoadouts"),ExpectedUpgrades.GetStructMemory())})
            {auto* P=FindFProperty<FArrayProperty>(FCharacterSave::StaticStruct(),Pair.Key);FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Expected.GetStructMemory()));P->Inner->CopyCompleteValue(A.GetRawPtr(0),Pair.Value);}
            CharacterStruct(Expected.GetStructMemory(),TEXT("RandomWeaponLoadout"),RandomPriorItems.GetStructMemory());CharacterStruct(Expected.GetStructMemory(),TEXT("RandomItemUpgradeLoadouts"),RandomExpectedUpgrades.GetStructMemory());
            auto Run=[&](uint8* Input,uint8* Output,bool ExpectedConflict)
            {
                FStructOnScope Params(Fn);auto* C=FindFProperty<FStructProperty>(Fn,TEXT("Current"));C->CopyCompleteValue(C->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),Input);
                FindFProperty<FObjectProperty>(Fn,TEXT("Record"))->SetObjectPropertyValue_InContainer(Params.GetStructMemory(),J);Conflict->SetPropertyValue_InContainer(Object,false);
                Object->ProcessEvent(Fn,Params.GetStructMemory());++Checks;auto* R=FindFProperty<FStructProperty>(Fn,TEXT("ReturnValue"));
                const bool OK=Conflict->GetPropertyValue_InContainer(Object)==ExpectedConflict && FCharacterSave::StaticStruct()->CompareScriptStruct(R->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),Output,0);
                if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM character/map/preset/progress case=%d failed conflict=%d expectedConflict=%d"),Checks,Conflict->GetPropertyValue_InContainer(Object),ExpectedConflict);return OK;
            };
            for(const TCHAR* N:{TEXT("EquipmentStagingIntent"),TEXT("EquipmentEverStaged")})FindFProperty<FBoolProperty>(J->GetClass(),N)->SetPropertyValue_InContainer(J,true);
            Passed=Passed&&Run(Current.GetStructMemory(),Expected.GetStructMemory(),false) && Run(Expected.GetStructMemory(),Expected.GetStructMemory(),false);
            FStructOnScope ForeignCurrent(FCharacterSave::StaticStruct()),ForeignExpected(FCharacterSave::StaticStruct());FCharacterSave::StaticStruct()->CopyScriptStruct(ForeignCurrent.GetStructMemory(),Current.GetStructMemory());FCharacterSave::StaticStruct()->CopyScriptStruct(ForeignExpected.GetStructMemory(),Expected.GetStructMemory());for(auto* Data:{ForeignCurrent.GetStructMemory(),ForeignExpected.GetStructMemory()}){auto* P=FindFProperty<FArrayProperty>(FCharacterSave::StaticStruct(),TEXT("Loadouts"));FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Data));reinterpret_cast<FItemLoadout*>(A.GetRawPtr(0))->TraversalTool=FGuid(70,71,72,73);}Passed=Passed&&Run(ForeignCurrent.GetStructMemory(),ForeignExpected.GetStructMemory(),true);
            FindFProperty<FBoolProperty>(J->GetClass(),TEXT("EquipmentStagingIntent"))->SetPropertyValue_InContainer(J,false);FindFProperty<FBoolProperty>(J->GetClass(),TEXT("EquipmentEverStaged"))->SetPropertyValue_InContainer(J,false);
            Passed=Passed&&Run(Current.GetStructMemory(),Current.GetStructMemory(),false);FindFProperty<FBoolProperty>(J->GetClass(),TEXT("EquipmentEverStaged"))->SetPropertyValue_InContainer(J,true);Passed=Passed&&Run(Current.GetStructMemory(),Current.GetStructMemory(),false);FindFProperty<FBoolProperty>(J->GetClass(),TEXT("EquipmentStagingIntent"))->SetPropertyValue_InContainer(J,true);
            if(Passed)
            {
                FindFProperty<FIntProperty>(J->GetClass(),TEXT("EquipmentPreset"))->SetPropertyValue_InContainer(J,5);
                Passed=Run(Current.GetStructMemory(),Current.GetStructMemory(),true);
            }
        }
    }
    if(Passed)
    {
        auto* Fn=BP->GeneratedClass->FindFunctionByName(TEXT("DOR_SourceMergeUpgrades"));
        auto* MapProperty=FindFProperty<FMapProperty>(FUpgradeLoadout::StaticStruct(),TEXT("Loadout"));
        Passed=Fn && MapProperty;
        if(Passed)
        {
            const FGuid Key(21,22,23,24),Other(31,32,33,34);const TArray<FGuid> Keys={Key};
            FStructOnScope Prior(FUpgradeLoadout::StaticStruct()),Stage(FUpgradeLoadout::StaticStruct()),Inserted(FUpgradeLoadout::StaticStruct()),Restored(FUpgradeLoadout::StaticStruct()),Foreign(FUpgradeLoadout::StaticStruct());
            auto Add=[&](uint8* Data,const FGuid& Id,int32 Seed)
            {FStructOnScope Selection(FItemUpgradeSelection::StaticStruct());Fill(FItemUpgradeSelection::StaticStruct(),Selection.GetStructMemory(),Seed);*FindFProperty<FStructProperty>(FItemUpgradeSelection::StaticStruct(),TEXT("WeaponID"))->ContainerPtrToValuePtr<FGuid>(Selection.GetStructMemory())=Id;FScriptMapHelper M(MapProperty,MapProperty->ContainerPtrToValuePtr<void>(Data));M.AddPair(&Id,Selection.GetStructMemory());};
            Add(Stage.GetStructMemory(),Key,22);Add(Inserted.GetStructMemory(),Key,22);Add(Inserted.GetStructMemory(),Other,33);Add(Restored.GetStructMemory(),Other,33);Add(Foreign.GetStructMemory(),Key,33);Add(Foreign.GetStructMemory(),Other,33);
            auto Run=[&](uint8* Current,uint8* Expected,uint8* Before,bool ExpectedConflict)
            {
                FStructOnScope Params(Fn);
                for(const auto& V:{TPair<const TCHAR*,uint8*>(TEXT("Current"),Current),TPair<const TCHAR*,uint8*>(TEXT("Prior"),Before),TPair<const TCHAR*,uint8*>(TEXT("Staged"),Stage.GetStructMemory())}){auto* P=FindFProperty<FStructProperty>(Fn,V.Key);P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),V.Value);}
                auto* K=FindFProperty<FArrayProperty>(Fn,TEXT("Keys"));K->CopyCompleteValue(K->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),&Keys);Conflict->SetPropertyValue_InContainer(Object,false);Object->ProcessEvent(Fn,Params.GetStructMemory());++Checks;
                auto* R=FindFProperty<FStructProperty>(Fn,TEXT("ReturnValue"));const bool OK=Conflict->GetPropertyValue_InContainer(Object)==ExpectedConflict&&FUpgradeLoadout::StaticStruct()->CompareScriptStruct(R->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),Expected,0);
                if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM absence/insert case=%d failed"),Checks);return OK;
            };
            auto* Project=BP->GeneratedClass->FindFunctionByName(TEXT("DOR_SourceProjectUpgrades"));
            if(!Project)Passed=false;
            else
            {
                FStructOnScope Params(Project);for(const auto& V:{TPair<const TCHAR*,uint8*>(TEXT("Current"),Restored.GetStructMemory()),TPair<const TCHAR*,uint8*>(TEXT("Staged"),Stage.GetStructMemory())}){auto* P=FindFProperty<FStructProperty>(Project,V.Key);P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),V.Value);}
                auto* K=FindFProperty<FArrayProperty>(Project,TEXT("Keys"));K->CopyCompleteValue(K->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),&Keys);Object->ProcessEvent(Project,Params.GetStructMemory());++Checks;auto* R=FindFProperty<FStructProperty>(Project,TEXT("ReturnValue"));Passed=FUpgradeLoadout::StaticStruct()->CompareScriptStruct(R->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),Inserted.GetStructMemory(),0);
                if(!Passed)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM actual-projection-insert case=%d failed"),Checks);
            }
            Passed=Passed&&Run(Inserted.GetStructMemory(),Restored.GetStructMemory(),Prior.GetStructMemory(),false)&&Run(Restored.GetStructMemory(),Restored.GetStructMemory(),Prior.GetStructMemory(),false)&&Run(Foreign.GetStructMemory(),Foreign.GetStructMemory(),Prior.GetStructMemory(),true);
            Add(Prior.GetStructMemory(),Key,11);
            Passed=Passed&&Run(Restored.GetStructMemory(),Restored.GetStructMemory(),Prior.GetStructMemory(),true);
        }
    }
    if(Passed)
    {
        auto* Fn=BP->GeneratedClass->FindFunctionByName(TEXT("DOR_SourceRestoreMembership"));auto* JournalProperty=FindFProperty<FObjectProperty>(BP->GeneratedClass,TEXT("DOR_Journal"));
        if(!Fn||!JournalProperty)Passed=false;
        else
        {
            auto* Journal=NewObject<UObject>(GetTransientPackage(),JournalProperty->PropertyClass);auto* Schema=FindFProperty<FIntProperty>(Journal->GetClass(),TEXT("Schema"));
            const FGuid Held(1,2,3,4),Added(5,6,7,8),Unrelated(9,10,11,12),Earned(13,14,15,16);
            FindFProperty<FBoolProperty>(Journal->GetClass(),TEXT("QualificationScopePrepared"))->SetPropertyValue_InContainer(Journal,true);const TArray<FGuid> Touched={Added};auto* TouchedProperty=FindFProperty<FArrayProperty>(Journal->GetClass(),TEXT("QualificationAddedPurchased"));TouchedProperty->CopyCompleteValue(TouchedProperty->ContainerPtrToValuePtr<void>(Journal),&Touched);
            auto Run=[&](int32 Version,const TArray<FGuid>& Current,const TArray<FGuid>& Expected)
            {
                FStructOnScope Params(Fn);Schema->SetPropertyValue_InContainer(Journal,Version);
                const TArray<FGuid> Before={Held},Ids={Held,Added};
                for(const auto& A:{TPair<const TCHAR*,const TArray<FGuid>*>(TEXT("Current"),&Current),TPair<const TCHAR*,const TArray<FGuid>*>(TEXT("Before"),&Before),TPair<const TCHAR*,const TArray<FGuid>*>(TEXT("Ids"),&Ids)})
                {auto* P=FindFProperty<FArrayProperty>(Fn,A.Key);if(!P)return false;P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),A.Value);}
                FindFProperty<FObjectProperty>(Fn,TEXT("Record"))->SetObjectPropertyValue_InContainer(Params.GetStructMemory(),Journal);Object->ProcessEvent(Fn,Params.GetStructMemory());++Checks;
                auto* Result=FindFProperty<FArrayProperty>(Fn,TEXT("ReturnValue"));const bool OK=Result&&Result->Identical(Result->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),&Expected,0);if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM membership version=%d case=%d failed"),Version,Checks);return OK;
            };
            Passed=Run(3,{Held,Added,Unrelated,Earned},{Held,Unrelated,Earned})&&Run(3,{Held,Unrelated,Earned},{Held,Unrelated,Earned})&&Run(3,{Added,Unrelated,Earned},{Unrelated,Earned})&&Run(2,{Added,Unrelated,Earned},{Unrelated,Earned,Held});
            FindFProperty<FBoolProperty>(Journal->GetClass(),TEXT("QualificationScopePrepared"))->SetPropertyValue_InContainer(Journal,false);
            Passed=Passed&&Run(3,{Held,Added,Unrelated,Earned},{Held,Added,Unrelated,Earned});
        }
    }
    if(Passed)
    {
        auto* Fn=Object->FindFunction(TEXT("DOR_SourcePlanQualification"));const FGuid Held(1,2,3,4),Added(5,6,7,8),RecordedOnly(9,10,11,12),Foreign(13,14,15,16);const TArray<FGuid> Before={Held};auto Equal=[&](const TCHAR* Name,const TArray<FGuid>& Expected){auto* P=FindFProperty<FArrayProperty>(Object->GetClass(),Name);return P->Identical(P->ContainerPtrToValuePtr<void>(Object),&Expected,0);};
        auto Run=[&](const TArray<FGuid>& Current,const TArray<FGuid>& Scope,const TArray<FGuid>& Required,bool Expected,const TCHAR* Stage)
        {FStructOnScope Params(Fn);for(const auto& T:{TPair<const TCHAR*,const TArray<FGuid>*>(TEXT("Current"),&Current),TPair<const TCHAR*,const TArray<FGuid>*>(TEXT("Before"),&Before),TPair<const TCHAR*,const TArray<FGuid>*>(TEXT("Scope"),&Scope),TPair<const TCHAR*,const TArray<FGuid>*>(TEXT("Required"),&Required)}){auto* P=FindFProperty<FArrayProperty>(Fn,T.Key);P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),T.Value);}Object->ProcessEvent(Fn,Params.GetStructMemory());const bool OK=FindFProperty<FBoolProperty>(Fn,TEXT("ReturnValue"))->GetPropertyValue_InContainer(Params.GetStructMemory())==Expected;++Checks;Passed=Passed&&OK;if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM qualification-plan case=%d stage=%s failed"),Checks,Stage);};
        auto Check=[&](bool OK,const TCHAR* Stage){++Checks;Passed=Passed&&OK;if(!OK)UE_LOG(LogTemp,Error,TEXT("DOR-SOURCE-VM qualification-plan case=%d stage=%s failed"),Checks,Stage);};
        Run({Held},{Held,Added,RecordedOnly},{Held,Added},true,TEXT("only-necessary-known-subset-is-planned"));Check(Equal(TEXT("DOR_QualificationPlanned"),{Held,Added})&&Equal(TEXT("DOR_QualificationAdded"),{Added})&&Equal(TEXT("DOR_QualificationAllowed"),{Added,RecordedOnly}),TEXT("recorded-prior-absence-is-distinct-from-actual-initial-grant"));Run({Held},{Held,Added,RecordedOnly},{},true,TEXT("vanilla-original-needs-no-invented-grant"));Check(Equal(TEXT("DOR_QualificationPlanned"),{Held})&&Equal(TEXT("DOR_QualificationAdded"),{})&&Equal(TEXT("DOR_QualificationAllowed"),{Added,RecordedOnly}),TEXT("empty-required-subset-preserves-missing-recorded-root-scope"));Run({Held,Added,Foreign},{Held,Added,RecordedOnly},{Added},true,TEXT("new-third-party-membership-is-not-our-addition"));Check(Equal(TEXT("DOR_QualificationAdded"),{})&&Equal(TEXT("DOR_QualificationPlanned"),{Held,Added,Foreign}),TEXT("already-present-idempotence-and-unrelated-progress"));Run({},{Held,Added},{Held},false,TEXT("prior-held-third-party-removal-is-not-regranted"));Run({Held},{Held,Added,Added},{Added},false,TEXT("duplicate-recorded-scope-rejected"));Run({Held},{Held,Added},{Added,Added},false,TEXT("duplicate-required-subset-rejected"));Run({Held},{Held,Added},{Foreign},false,TEXT("foreign-required-root-rejected"));
    }
    if(Object)World->DestroyActor(Object);
    World->DestroyWorld(false);
    UE_LOG(LogTemp,Display,TEXT("DOR-SOURCE-VM checks=%d passed=%d scope=synthetic-generated-merges-no-save-no-gameplay"),Checks,Passed);
    return Passed;
}
