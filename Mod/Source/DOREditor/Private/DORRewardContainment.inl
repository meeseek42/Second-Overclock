// The native reward catalog is process-local. Each world observes its own
// live objects, and verifies inherited mappings before admitting mission work.
bool BuildDORRewardContainment(UBlueprint* Safety, UBlueprint* Rig, UBlueprint* Guard,
    const TArray<TSharedPtr<FJsonObject>>& Manifest)
{
    using FGraph = FDORBankJournalGraph;
    auto* RootClass = LoadObject<UClass>(nullptr,TEXT("/Script/FSD.OverclockUpgrade"));
    auto* SettingsClass = LoadObject<UClass>(nullptr,TEXT("/Script/FSD.SchematicSettings"));
    auto* BankClass = UOverclockBank::StaticClass();
    auto* UserSettingsClass = LoadObject<UClass>(nullptr,TEXT("/Script/FSD.FSDGameUserSettings"));
    auto* SchematicClass = USchematic::StaticClass();
    auto* ItemClass = UOverclockShematicItem::StaticClass();
    auto* UpgradeCategory = LoadObject<UClass>(nullptr, TEXT("/Script/FSD.ItemUpgradeCategory"));
    auto* SchematicCategory = LoadObject<UClass>(nullptr, TEXT("/Script/FSD.SchematicCategory"));
    if (!RootClass || !SettingsClass || !UserSettingsClass || !UpgradeCategory || !SchematicCategory) return false;
    auto* Change = FindFProperty<FMulticastDelegateProperty>(UserSettingsClass, TEXT("OnSettingsChanged"));
    if (!Change) return false;

    const auto Guid = MakeStructPinType(TBaseStructure<FGuid>::Get());
    const auto Guids = MakeStructPinType(TBaseStructure<FGuid>::Get(), true);
    const auto Roots = MakeObjectPinType(RootClass, true);
    const auto SettingsType = MakeObjectPinType(SettingsClass);
    const auto Banks = MakeObjectPinType(BankClass, true);
    auto BankMap = MakeOverclockBankMapPinType(RootClass, SchematicClass);
    auto CatalogMap = BankMap;
    CatalogMap.PinCategory = Guid.PinCategory;
    CatalogMap.PinSubCategoryObject = Guid.PinSubCategoryObject;
    auto CatalogSet = MakeObjectPinType(SchematicClass);
    CatalogSet.ContainerType = EPinContainerType::Set;

    auto Ints=MakeIntPinType(); Ints.ContainerType=EPinContainerType::Array;
    auto Strings=MakeStringPinType(); Strings.ContainerType=EPinContainerType::Array;
    const TMap<FName, FEdGraphPinType> Fields = {
        {TEXT("DOR_RewardRoots"), Roots},
        {TEXT("DOR_RewardSchematics"), MakeObjectPinType(SchematicClass, true)},
        {TEXT("DOR_RewardWeaponPaths"), Strings},
        {TEXT("DOR_RewardWeapons"), MakeObjectPinType(UItemID::StaticClass(), true)},
        {TEXT("DOR_RewardWeaponIndices"), Ints},
        {TEXT("DOR_RewardTestIndices"), Ints},
        {TEXT("DOR_RewardShellPaths"), Strings},
        {TEXT("DOR_RewardUpgradeCategoryPaths"), Strings},
        {TEXT("DOR_RewardSchematicCategoryPaths"), Strings},
        {TEXT("DOR_RewardCarrierCategoryOverrides"), Strings},
        {TEXT("DOR_RewardUpgradeCategories"), MakeObjectPinType(UpgradeCategory, true)},
        {TEXT("DOR_RewardSchematicCategories"), MakeObjectPinType(SchematicCategory, true)},
        {TEXT("DOR_RewardPairIds"), Guids}, {TEXT("DOR_RewardSchematicIds"), Guids},
        {TEXT("DOR_RewardSave"), MakeObjectPinType(UFSDSaveGame::StaticClass())},
        {TEXT("DOR_RewardData"), MakeObjectPinType(UGameData::StaticClass())},
        {TEXT("DOR_RewardSettings"), SettingsType}, {TEXT("DOR_RewardBanks"), Banks},
        {TEXT("DOR_RewardSlot"), MakeStringPinType()}, {TEXT("DOR_RewardAnon"), MakeStringPinType()},
        {TEXT("DOR_RewardIndex"), MakeIntPinType()}, {TEXT("DOR_RewardReady"), MakeBoolPinType()},
        {TEXT("DOR_RewardObserved"), MakeBoolPinType()},
        {TEXT("DOR_RewardWaitingSafety"), MakeBoolPinType()},
        {TEXT("DOR_RewardMissing"), MakeBoolPinType()},
        {TEXT("DOR_RewardFirstMissingRow"), MakeIntPinType()},
        {TEXT("DOR_RewardLastReceipt"), MakeStringPinType()},
        {TEXT("DOR_RewardPurchased"), Guids}, {TEXT("DOR_RewardOwned"), Guids}, {TEXT("DOR_RewardForged"), Guids},
        {TEXT("DOR_RewardUserSettings"), MakeObjectPinType(UserSettingsClass)}
    };
    for (const auto& Field : Fields) if (!AddTypedVariable(Safety, Field.Key, Field.Value)) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Safety);
    FKismetEditorUtilities::CompileBlueprint(Safety);

    auto MapOp = [&](FGraph& F, const TCHAR* Op, UEdGraphPin* Map, UEdGraphPin* Key, UEdGraphPin* Value=nullptr)
    {
        auto* N = F.Call(UBlueprintMapLibrary::StaticClass(), Op);
        ForcePinType(FindPin(N,TEXT("TargetMap")), Map->PinType);
        ForcePinType(FindPin(N,TEXT("Key")), Key->PinType);
        Connect(Map,FindPin(N,TEXT("TargetMap")));
        Connect(Key,FindPin(N,TEXT("Key")));
        if (auto* Pin=N->FindPin(TEXT("Value")))
        {
            ForcePinType(Pin,MakeObjectPinType(SchematicClass));
            if(Value) Connect(Value,Pin);
        }
        return N;
    };
    auto BankField = [&](FGraph& F, UEdGraphPin* Bank)
    {
        auto* Map = F.Field(Bank,BankClass,TEXT("Overclocks"));
        ForcePinType(Map,BankMap);
        return Map;
    };
    auto ResolveBank = [&](FGraph& F, UEdGraphPin* Settings, UEdGraphPin* Weapon)
    {
        auto* Bank=F.Call(SettingsClass,TEXT("GetOverclocksForItem"));
        Connect(Settings,FindPin(Bank,UEdGraphSchema_K2::PN_Self));
        Connect(Weapon,FindPin(Bank,TEXT("Item")));
        return Bank->GetReturnValuePin();
    };
    auto Data = [](FGraph& F){return F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameData"))->GetReturnValuePin();};
    auto Settings = [&](FGraph& F, UEdGraphPin* DataObject){return F.Field(DataObject,UGameData::StaticClass(),TEXT("SchematicSettings"));};
    auto Slot = [](FGraph& F, UEdGraphPin* Save)
    {
        auto* N=F.Self(TEXT("DOR_ResolveSourceSlot"));
        Connect(Save,FindPin(N,TEXT("Save")));
        return N->GetReturnValuePin();
    };
    auto Append = [](FGraph& F, UEdGraphPin* Array, UEdGraphPin* Value, bool Unique=false)
    {
        auto* N=F.Array(Unique?TEXT("Array_AddUnique"):TEXT("Array_Add"),Array);
        Connect(Value,FindPin(N,TEXT("NewItem")));
    };
    auto Contains = [](FGraph& F, UEdGraphPin* Array, UEdGraphPin* Value)
    {
        auto* N=F.Array(TEXT("Array_Contains"),Array);
        Connect(Value,FindPin(N,TEXT("ItemToFind")));
        return N->GetReturnValuePin();
    };
    auto Quiet = [](FGraph& F, UEdGraphPin* Condition)
    {
        auto* B=F.Branch(Condition);
        F.Tail=B->GetElsePin(); F.Return();
        F.Tail=B->GetThenPin();
    };
    auto Number = [](FGraph& F, UEdGraphPin* Value)
    {
        auto* Call=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));
        Connect(Value,FindPin(Call,TEXT("InInt")));
        return Call->GetReturnValuePin();
    };
    auto BoolText = [](FGraph& F, UEdGraphPin* Value)
    {
        auto* Call=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectString"));
        FindPin(Call,TEXT("A"))->DefaultValue=TEXT("true");
        FindPin(Call,TEXT("B"))->DefaultValue=TEXT("false");
        Connect(Value,FindPin(Call,TEXT("bPickA")));
        return Call->GetReturnValuePin();
    };
    auto Concat = [](FGraph& F, UEdGraphPin* Prefix, const TCHAR* Label, UEdGraphPin* Value)
    {
        auto* Call=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));
        Connect(Prefix,FindPin(Call,TEXT("A")));
        Connect(DORSafeConcat(F,Label,Value),FindPin(Call,TEXT("B")));
        return Call->GetReturnValuePin();
    };
    // The normal writer belongs to this world-local actor but does not need
    // the save safety initialization, backup, or WAL to have succeeded.
    {
        FGraph F(Safety,TEXT("DOR_RewardReceipt"));
        auto* Stage=F.Input(TEXT("Stage"),MakeStringPinType());
        auto* Row=F.Input(TEXT("Row"),MakeIntPinType());
        auto* SlotName=F.Input(TEXT("Slot"),MakeStringPinType());
        auto* Detail=F.Input(TEXT("Detail"),MakeStringPinType());
        auto* Level=F.Call(UGameplayStatics::StaticClass(),TEXT("GetCurrentLevelName"));
        auto* Message=DORSafeConcat(F,TEXT("[DOR]|event=REWARD_REGISTRATION|stage="),Stage);
        Message=Concat(F,Message,TEXT("|world="),Level->GetReturnValuePin());
        Message=Concat(F,Message,TEXT("|slot="),SlotName);
        Message=Concat(F,Message,TEXT("|row="),Number(F,Row));
        Message=Concat(F,Message,TEXT("|ready="),BoolText(F,F.Get(TEXT("DOR_Ready"))));
        Message=Concat(F,Message,TEXT("|blocked="),BoolText(F,F.Get(TEXT("DOR_Blocked"))));
        Message=Concat(F,Message,TEXT("|active="),BoolText(F,F.Get(TEXT("DOR_Active"))));
        Message=Concat(F,Message,TEXT("|disabled="),BoolText(F,F.Get(TEXT("DOR_Disabled"))));
        auto* Line=F.Cache(TEXT("Receipt"),Concat(F,Message,TEXT("|detail="),Detail));
        Quiet(F,F.Not(F.SameText(Line,F.Get(TEXT("DOR_RewardLastReceipt")))));
        F.Set(TEXT("DOR_RewardLastReceipt"),Line);
        auto* Record=F.Self(TEXT("DOR_NormalRecord"));
        Connect(Line,FindPin(Record,TEXT("Message")));
        auto* Flush=F.Self(TEXT("DOR_NormalFlush"));
        FindPin(Flush,TEXT("Force"))->DefaultValue=TEXT("true");
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    auto Receipt = [&](FGraph& F, const TCHAR* Stage, UEdGraphPin* I=nullptr,
        UEdGraphPin* Detail=nullptr, UEdGraphPin* ResolvedSlot=nullptr)
    {
        auto* Call=F.Self(TEXT("DOR_RewardReceipt"));
        FindPin(Call,TEXT("Stage"))->DefaultValue=Stage;
        if(I) Connect(F.At(F.Get(TEXT("DOR_RewardTestIndices")),I),FindPin(Call,TEXT("Row")));
        else FindPin(Call,TEXT("Row"))->DefaultValue=TEXT("-1");
        Connect(ResolvedSlot?ResolvedSlot:F.Get(TEXT("DOR_RewardSlot")),FindPin(Call,TEXT("Slot")));
        if(Detail) Connect(Detail,FindPin(Call,TEXT("Detail")));
    };
    auto Require = [&](FGraph& F, UEdGraphPin* Condition, const TCHAR* Stage,
        UEdGraphPin* I=nullptr, UEdGraphPin* ResolvedSlot=nullptr)
    {
        auto* B=F.Branch(Condition);
        F.Tail=B->GetElsePin(); Receipt(F,Stage,I,nullptr,ResolvedSlot); F.Return();
        F.Tail=B->GetThenPin();
    };
    auto MembershipDetail = [&](FGraph& F, UEdGraphPin* I, UEdGraphPin* Bank)
    {
        auto* Save=F.Get(TEXT("DOR_RewardSave"));
        auto* SS=F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave"));
        auto* Pair=F.At(F.Get(TEXT("DOR_RewardPairIds")),I);
        auto* Schema=F.At(F.Get(TEXT("DOR_RewardSchematicIds")),I);
        auto* Text=DORSafeConcat(F,TEXT("purchased="),BoolText(F,
            Contains(F,F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades")),Pair)));
        Text=Concat(F,Text,TEXT("|owned="),BoolText(F,
            Contains(F,F.StructField(SS,FSchematicSave::StaticStruct(),TEXT("OwnedSchematics")),Schema)));
        Text=Concat(F,Text,TEXT("|forged="),BoolText(F,
            Contains(F,F.StructField(SS,FSchematicSave::StaticStruct(),TEXT("ForgedSchematics")),Schema)));
        Text=Concat(F,Text,TEXT("|settings="),F.Path(F.Get(TEXT("DOR_RewardSettings"))));
        return Concat(F,Text,TEXT("|bank="),F.Path(Bank));
    };
    auto Log = [](FGraph& F, const TCHAR* Message)
    {
        F.Print(Message);
        auto* N=CastChecked<UK2Node_CallFunction>(F.Tail->GetOwningNode());
        FindPin(N,TEXT("bPrintToScreen"))->DefaultValue=TEXT("false");
    };
    auto Delegate = [&](FGraph& F, UEdGraphPin* Target, bool Remove)
    {
        auto* N=Remove
            ? static_cast<UK2Node_BaseMCDelegate*>(AddNode<UK2Node_RemoveDelegate>(F.G,0,0))
            : static_cast<UK2Node_BaseMCDelegate*>(AddNode<UK2Node_AddDelegate>(F.G,0,0));
        N->SetFromProperty(Change,false,UserSettingsClass); N->AllocateDefaultPins();
        Connect(Target,FindPin(N,UEdGraphSchema_K2::PN_Self));
        auto* D=AddNode<UK2Node_CreateDelegate>(F.G,0,0);
        D->AllocateDefaultPins(); D->SetFunction(TEXT("DOR_RewardSettingsChanged"));
        Connect(SelfPin(F.G,0,0),D->GetObjectInPin());
        Connect(D->GetDelegateOutPin(),N->GetDelegatePin()); F.Run(N);
    };
    {
        FGraph F(Safety,TEXT("DOR_RewardSameProfile"));
        auto* Save=F.Cache(TEXT("Live"),F.SaveGame());
        Quiet(F,F.And(F.IsValid(Save),F.Same(Save,F.Get(TEXT("DOR_RewardSave")))));
        Quiet(F,F.SameText(Slot(F,Save),F.Get(TEXT("DOR_RewardSlot"))));
        Quiet(F,F.SameText(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Get(TEXT("DOR_RewardAnon"))));
        F.Return(F.SameValue(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Get(TEXT("DOR_RewardIndex"))));
        F.Compile();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardContextCurrent"));
        Quiet(F,F.Get(TEXT("DOR_RewardObserved")));
        Quiet(F,F.Self(TEXT("DOR_RewardSameProfile"))->GetReturnValuePin());
        auto* Live=F.Cache(TEXT("Data"),Data(F));
        Quiet(F,F.And(F.IsValid(Live),F.Same(Live,F.Get(TEXT("DOR_RewardData")))));
        auto* Schematics=F.Cache(TEXT("Settings"),Settings(F,Live));
        Quiet(F,F.And(F.IsValid(Schematics),F.Same(Schematics,F.Get(TEXT("DOR_RewardSettings")))));
        // Compare only the distinct relevant bank objects on the existing 1s tick.
        auto* Loop=F.Each(F.Get(TEXT("DOR_RewardWeapons")));
        auto* I=FindPin(Loop,TEXT("Array Index"));
        auto* Bank=ResolveBank(F,Schematics,FindPin(Loop,TEXT("Array Element")));
        Quiet(F,F.Same(Bank,F.At(F.Get(TEXT("DOR_RewardBanks")),I)));
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(nullptr,TEXT("true")); F.ValidateReturnPins();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardEvict"));
        auto* Schematics=F.Input(TEXT("Settings"),SettingsType);
        auto* BankList=F.Input(TEXT("Banks"),Banks);
        auto* Zero=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"))->GetReturnValuePin();
        auto* BankRemoved=F.Cache(TEXT("BankRemoved"),Zero);
        auto* MapRemoved=F.Cache(TEXT("MapRemoved"),Zero);
        auto* SetRemoved=F.Cache(TEXT("SetRemoved"),Zero);
        auto CountRemoval=[&](UEdGraphPin* Count,UEdGraphPin* Removed)
        {
            auto* Changed=F.Branch(Removed);
            auto* Next=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));
            Connect(Count,FindPin(Next,TEXT("A")));
            FindPin(Next,TEXT("B"))->DefaultValue=TEXT("1");
            F.Set(F.Variable(Count),Next->GetReturnValuePin());
            F.Join(Changed->GetElsePin());
        };
        auto* Loop=F.Each(F.Get(TEXT("DOR_RewardRoots")));
        auto* Root=FindPin(Loop,TEXT("Array Element"));
        auto* I=FindPin(Loop,TEXT("Array Index"));
        auto* Schema=F.At(F.Get(TEXT("DOR_RewardSchematics")),I);
        auto* WeaponIndex=F.At(F.Get(TEXT("DOR_RewardWeaponIndices")),I);
        auto* Bank=F.At(BankList,WeaponIndex);
        auto* ValidBank=F.Branch(F.IsValid(Bank));
        auto* Map=BankField(F,Bank);
        auto* Found=MapOp(F,TEXT("Map_Find"),Map,Root);
        auto* Exact=F.Branch(F.And(Found->GetReturnValuePin(),F.Same(FindPin(Found,TEXT("Value")),Schema)));
        CountRemoval(BankRemoved,MapOp(F,TEXT("Map_Remove"),Map,Root)->GetReturnValuePin());
        F.Join(Exact->GetElsePin()); F.Join(ValidBank->GetElsePin());

        auto* ValidSettings=F.Branch(F.IsValid(Schematics));
        auto* Catalog=F.Field(Schematics,SettingsClass,TEXT("AllSchematicsMap"));
        ForcePinType(Catalog,CatalogMap);
        auto* Id=F.At(F.Get(TEXT("DOR_RewardSchematicIds")),I);
        auto* Cached=MapOp(F,TEXT("Map_Find"),Catalog,Id);
        auto* Own=F.Branch(F.And(Cached->GetReturnValuePin(),F.Same(FindPin(Cached,TEXT("Value")),Schema)));
        CountRemoval(MapRemoved,MapOp(F,TEXT("Map_Remove"),Catalog,Id)->GetReturnValuePin());
        F.Join(Own->GetElsePin());
        // The pointer set is independent even when another schematic replaced the GUID value.
        auto* Set=F.Field(Schematics,SettingsClass,TEXT("AllSchematics"));
        ForcePinType(Set,CatalogSet);
        auto* Remove=F.Call(UBlueprintSetLibrary::StaticClass(),TEXT("Set_Remove"));
        ForcePinType(FindPin(Remove,TEXT("TargetSet")),CatalogSet);
        ForcePinType(FindPin(Remove,TEXT("Item")),MakeObjectPinType(SchematicClass));
        Connect(Set,FindPin(Remove,TEXT("TargetSet")));
        Connect(Schema,FindPin(Remove,TEXT("Item")));
        CountRemoval(SetRemoved,Remove->GetReturnValuePin());
        F.Join(ValidSettings->GetElsePin());
        F.Tail=FindPin(Loop,TEXT("Completed"));
        auto* Message=DORSafeString(F,TEXT("[DOR-REWARD]|stage=eviction"));
        for(const auto& Entry : {TPair<const TCHAR*,UEdGraphPin*>(TEXT("|bank="),BankRemoved),
            TPair<const TCHAR*,UEdGraphPin*>(TEXT("|map="),MapRemoved),
            TPair<const TCHAR*,UEdGraphPin*>(TEXT("|set="),SetRemoved)})
        {
            auto* Text=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));
            Connect(Entry.Value,FindPin(Text,TEXT("InInt")));
            auto* Label=DORSafeConcat(F,Entry.Key,Text->GetReturnValuePin());
            auto* AppendText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));
            Connect(Message,FindPin(AppendText,TEXT("A"))); Connect(Label,FindPin(AppendText,TEXT("B")));
            Message=AppendText->GetReturnValuePin();
        }
        Log(F,TEXT(""));
        Connect(Message,FindPin(F.Tail->GetOwningNode(),TEXT("InString")));
        F.Return(nullptr,TEXT("true")); F.ValidateReturnPins();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardLoadWeapons"));
        auto* Weapons=F.Get(TEXT("DOR_RewardWeapons"));
        F.Array(TEXT("Array_Clear"),Weapons);
        auto* Loop=F.Each(F.Get(TEXT("DOR_RewardWeaponPaths")));
        auto* Path=FindPin(Loop,TEXT("Array Element"));
        auto* Weapon=F.Cache(TEXT("Weapon"),F.Asset(Path,UItemID::StaticClass()));
        auto* Exact=F.Branch(F.And(F.IsValid(Weapon),F.SameText(F.Path(Weapon),Path)));
        Append(F,Weapons,Weapon);
        auto* Loaded=F.Tail; F.Tail=Exact->GetElsePin();
        // Keep indices aligned so a failure cannot hide other exact entries.
        F.Set(F.Variable(Weapon),nullptr);
        Append(F,Weapons,Weapon); F.Join(Loaded);
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardWithdraw"));
        F.Set(TEXT("DOR_RewardReady"),nullptr,TEXT("false"));
        auto* Tracked=F.Branch(F.Get(TEXT("DOR_RewardObserved")));
        auto* Old=F.Self(TEXT("DOR_RewardEvict"));
        Connect(F.Get(TEXT("DOR_RewardSettings")),FindPin(Old,TEXT("Settings")));
        Connect(F.Get(TEXT("DOR_RewardBanks")),FindPin(Old,TEXT("Banks")));
        F.Join(Tracked->GetElsePin());
        auto* Live=F.Cache(TEXT("Data"),Data(F)); Quiet(F,F.IsValid(Live));
        auto* Schematics=F.Cache(TEXT("Settings"),Settings(F,Live)); Quiet(F,F.IsValid(Schematics));
        auto* NeedWeapons=F.Branch(F.EqInt(F.Array(TEXT("Array_Length"),F.Get(TEXT("DOR_RewardWeapons")))->GetReturnValuePin(),0));
        F.Self(TEXT("DOR_RewardLoadWeapons")); F.Join(NeedWeapons->GetElsePin());
        auto* Current=F.Cache(TEXT("Banks"),F.Get(TEXT("DOR_RewardBanks")));
        F.Array(TEXT("Array_Clear"),Current);
        auto* Loop=F.Each(F.Get(TEXT("DOR_RewardWeapons")));
        Append(F,Current,ResolveBank(F,Schematics,FindPin(Loop,TEXT("Array Element"))));
        F.Tail=FindPin(Loop,TEXT("Completed"));
        auto* Already=F.Branch(F.And(F.Same(Schematics,F.Get(TEXT("DOR_RewardSettings"))),
            F.And(F.Get(TEXT("DOR_RewardObserved")),F.SameArray(Current,F.Get(TEXT("DOR_RewardBanks"))))));
        F.Return(nullptr,TEXT("true")); F.Tail=Already->GetElsePin();
        auto* Active=F.Self(TEXT("DOR_RewardEvict"));
        Connect(Schematics,FindPin(Active,TEXT("Settings")));
        Connect(Current,FindPin(Active,TEXT("Banks")));
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardSettingsChanged"));
        auto* Observed=F.Branch(F.Get(TEXT("DOR_RewardObserved")));
        auto* Changed=F.Branch(F.Not(F.Self(TEXT("DOR_RewardContextCurrent"))->GetReturnValuePin()));
        F.Self(TEXT("DOR_RewardWithdraw"));
        F.Set(TEXT("DOR_RewardObserved"),nullptr,TEXT("false"));
        Log(F,TEXT("[DOR-REWARD]|stage=settings-change-retracted"));
        // All branches end without a return property for the native delegate.
        F.Join(Changed->GetElsePin()); F.Join(Observed->GetElsePin()); F.Compile();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardUnbindSettings"));
        auto* Bound=F.Get(TEXT("DOR_RewardUserSettings"));
        Quiet(F,F.IsValid(Bound)); Delegate(F,Bound,true);
        F.Set(TEXT("DOR_RewardUserSettings"),nullptr);
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardBindSettings"));
        auto* Current=F.Cache(TEXT("Settings"),F.Call(UserSettingsClass,TEXT("GetFSDGameUserSettings"))->GetReturnValuePin());
        Quiet(F,F.IsValid(Current));
        auto* Same=F.Branch(F.Same(Current,F.Get(TEXT("DOR_RewardUserSettings"))));
        F.Return(nullptr,TEXT("true")); F.Tail=Same->GetElsePin();
        F.Self(TEXT("DOR_RewardUnbindSettings")); Delegate(F,Current,false);
        F.Set(TEXT("DOR_RewardUserSettings"),Current);
        F.Return(nullptr,TEXT("true")); F.ValidateReturnPins();
    }
    // A save pointer change invalidates Safety's in-memory binding as well.
    // A live mission lease must keep its original binding and its WAL.
    {
        FGraph F(Safety,TEXT("DOR_RewardReleaseOldSource"));
        auto* Bound=F.Get(TEXT("DOR_SourceLiveObject"));
        auto* Current=F.Cache(TEXT("Save"),F.SaveGame());
        Quiet(F,F.IsValid(Current));
        auto* SameObject=F.Branch(F.Same(Current,Bound));
        auto* SameSlot=F.SameText(Slot(F,Current),F.Get(TEXT("DOR_SourceSlot")));
        auto* SameAnon=F.SameText(F.Field(Current,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Get(TEXT("DOR_AnonymousId")));
        auto* SameIndex=F.SameValue(F.Field(Current,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Get(TEXT("DOR_SourceIndex")));
        auto* SameIdentity=F.Branch(F.And(SameSlot,F.And(SameAnon,SameIndex)));
        F.Return(nullptr,TEXT("true")); F.Tail=SameIdentity->GetElsePin();
        F.Join(SameObject->GetElsePin());
        Quiet(F,F.Not(F.Get(TEXT("DOR_SourceLiveLease"))));
        for (const TCHAR* Name : {TEXT("DOR_Ready"),TEXT("DOR_Blocked"),TEXT("DOR_Active"),TEXT("DOR_Disabled")})
            F.Set(Name,nullptr,TEXT("false"));
        F.Set(TEXT("DOR_SourceSlot"),nullptr,TEXT(""));
        F.Set(TEXT("DOR_Journal"),nullptr);
        F.Set(TEXT("DOR_SourceLiveObject"),Current);
        F.Return(nullptr,TEXT("true")); F.ValidateReturnPins();
    }
    // Resolve exact live banks and assemble a proposed save in scratch arrays.
    // This graph never mutates native save fields, native banks, or the catalog.
    {
        FGraph F(Safety,TEXT("DOR_RewardPreflight"));
        auto* RigEntry=F.Input(TEXT("RigEntry"),MakeBoolPinType());
        auto* Save=F.Get(TEXT("DOR_RewardSave"));
        auto* SchematicSave=F.Cache(TEXT("SchematicSave"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")));
        F.Set(TEXT("DOR_RewardPurchased"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades")));
        F.Set(TEXT("DOR_RewardOwned"),F.StructField(SchematicSave,FSchematicSave::StaticStruct(),TEXT("OwnedSchematics")));
        F.Set(TEXT("DOR_RewardForged"),F.StructField(SchematicSave,FSchematicSave::StaticStruct(),TEXT("ForgedSchematics")));
        F.Set(TEXT("DOR_RewardMissing"),nullptr,TEXT("false"));
        F.Set(TEXT("DOR_RewardFirstMissingRow"),nullptr,TEXT("-1"));
        for(const TCHAR* N:{TEXT("DOR_RewardUpgradeCategories"),TEXT("DOR_RewardSchematicCategories")})
            F.Array(TEXT("Array_Clear"),F.Get(N));
        auto* Loop=F.Each(F.Get(TEXT("DOR_RewardRoots")));
        auto* I=FindPin(Loop,TEXT("Array Index"));
        auto* Root=FindPin(Loop,TEXT("Array Element"));
        auto* Schema=F.At(F.Get(TEXT("DOR_RewardSchematics")),I);
        auto* PairId=F.At(F.Get(TEXT("DOR_RewardPairIds")),I);
        auto* SchemaId=F.At(F.Get(TEXT("DOR_RewardSchematicIds")),I);
        auto* WeaponIndex=F.At(F.Get(TEXT("DOR_RewardWeaponIndices")),I);
        auto* Weapon=F.At(F.Get(TEXT("DOR_RewardWeapons")),WeaponIndex);
        auto* Bank=F.At(F.Get(TEXT("DOR_RewardBanks")),WeaponIndex);
        Require(F,F.And(F.IsValid(Root),F.And(F.IsValid(Schema),F.IsValid(Weapon))),TEXT("preflight:asset-invalid"),I);
        Require(F,F.And(F.SameValue(F.Id(Root),PairId),F.SameValue(F.Id(Schema),SchemaId)),TEXT("preflight:identity-mismatch"),I);
        Require(F,F.IsValid(Bank),TEXT("preflight:bank-missing"),I);
        Require(F,F.Same(F.Field(Bank,BankClass,TEXT("OwningItem")),Weapon),TEXT("preflight:bank-weapon-mismatch"),I);
        auto* Item=F.Call(SchematicClass,TEXT("GetSchematicItem"));
        Connect(Schema,FindPin(Item,UEdGraphSchema_K2::PN_Self));
        auto* Link=AddNode<UK2Node_DynamicCast>(F.G,++F.Serial*160,0);
        Link->TargetType=ItemClass; Link->SetPurity(true); Link->AllocateDefaultPins();
        ForcePinType(Link->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass()));
        Connect(Item->GetReturnValuePin(),Link->GetCastSourcePin());
        Require(F,F.IsValid(Link->GetCastResultPin()),TEXT("preflight:schematic-item-invalid"),I);
        // Rig may supply the previously generated metadata after save commit.
        // A mission may only adopt mappings and links already present.
        auto* ExactRoot=F.Same(F.Field(Link->GetCastResultPin(),ItemClass,TEXT("Overclock")),Root);
        Require(F,ExactRoot,TEXT("preflight:schematic-root-mismatch"),I);
        auto* RigMetadata=F.Branch(RigEntry);
        auto* ShellPath=F.At(F.Get(TEXT("DOR_RewardShellPaths")),I);
        auto* Upgrade=F.Cache(TEXT("Upgrade"),F.Asset(F.At(F.Get(TEXT("DOR_RewardUpgradeCategoryPaths")),I),UpgradeCategory));
        auto* Category=F.Cache(TEXT("Category"),F.Asset(F.At(F.Get(TEXT("DOR_RewardSchematicCategoryPaths")),I),SchematicCategory));
        Require(F,F.And(F.IsValid(Upgrade),F.IsValid(Category)),TEXT("preflight:category-invalid"),I);
        auto* Common=F.Branch(F.Not(F.SameText(ShellPath,DORSafeString(F,TEXT("")))));
        auto* Shell=F.Cache(TEXT("Carrier"),F.Asset(ShellPath,RootClass));
        Require(F,F.IsValid(Shell),TEXT("preflight:carrier-invalid"),I);
        auto* CarrierCategory=F.Cache(TEXT("CarrierCategory"),Category);
        auto* Override=F.At(F.Get(TEXT("DOR_RewardCarrierCategoryOverrides")),I);
        auto* HasOverride=F.Branch(F.Not(F.SameText(Override,DORSafeString(F,TEXT("")))));
        F.Set(F.Variable(CarrierCategory),F.Asset(Override,SchematicCategory));
        F.Join(HasOverride->GetElsePin());
        Require(F,F.And(F.Same(F.Field(Shell,UItemUpgrade::StaticClass(),TEXT("Category")),Upgrade),
            F.And(F.IsValid(CarrierCategory),F.Same(F.Field(Shell,RootClass,TEXT("SchematicCategory")),CarrierCategory))),
            TEXT("preflight:carrier-category-mismatch"),I);
        F.Join(Common->GetElsePin());
        Append(F,F.Get(TEXT("DOR_RewardUpgradeCategories")),Upgrade);
        Append(F,F.Get(TEXT("DOR_RewardSchematicCategories")),Category);
        F.Join(RigMetadata->GetElsePin());
        auto* Map=MapOp(F,TEXT("Map_Find"),BankField(F,Bank),Root);
        auto* Installed=F.Branch(Map->GetReturnValuePin());
        Require(F,F.Same(FindPin(Map,TEXT("Value")),Schema),TEXT("preflight:foreign-bank-value"),I);
        Require(F,F.Same(F.Field(Link->GetCastResultPin(),ItemClass,TEXT("OwningItem")),Weapon),
            TEXT("preflight:installed-weapon-mismatch"),I);
        F.Join(Installed->GetElsePin());
        auto* Mission=F.Branch(F.Not(RigEntry));
        auto* Mapped=F.Branch(Map->GetReturnValuePin());
        F.Tail=Mapped->GetElsePin();
        Receipt(F,TEXT("adopt:mapping-missing"),I,MembershipDetail(F,I,Bank));
        F.Return(); F.Tail=Mapped->GetThenPin();
        F.Join(Mission->GetElsePin());
        for (const auto& Membership : {TPair<FName,UEdGraphPin*>(TEXT("DOR_RewardPurchased"),PairId),
            TPair<FName,UEdGraphPin*>(TEXT("DOR_RewardOwned"),SchemaId),
            TPair<FName,UEdGraphPin*>(TEXT("DOR_RewardForged"),SchemaId)})
        {
            auto* Scratch=F.Get(Membership.Key);
            auto* Missing=F.Branch(F.Not(Contains(F,Scratch,Membership.Value)));
            auto* First=F.Branch(F.Not(F.Get(TEXT("DOR_RewardMissing"))));
            F.Set(TEXT("DOR_RewardFirstMissingRow"),I);
            F.Join(First->GetElsePin());
            F.Set(TEXT("DOR_RewardMissing"),nullptr,TEXT("true"));
            Append(F,Scratch,Membership.Value,true);
            F.Join(Missing->GetElsePin());
        }
        F.Tail=FindPin(Loop,TEXT("Completed"));
        F.Return(nullptr,TEXT("true")); F.ValidateReturnPins();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardRegister"));
        auto* I=F.Get(TEXT("DOR_RewardFirstMissingRow"));
        auto* Prepare=F.Self(TEXT("DOR_PrepareRegistration"));
        Require(F,Prepare->GetReturnValuePin(),TEXT("prepare:registration-rejected"),I);
        Require(F,F.Self(TEXT("DOR_RewardSameProfile"))->GetReturnValuePin(),TEXT("prepare:source-profile-changed"),I);
        auto* Save=F.Get(TEXT("DOR_RewardSave"));
        auto* Schema=F.Patch(F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")),FSchematicSave::StaticStruct(),
            {{TEXT("OwnedSchematics"),F.Get(TEXT("DOR_RewardOwned"))},{TEXT("ForgedSchematics"),F.Get(TEXT("DOR_RewardForged"))}});
        F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades"),F.Get(TEXT("DOR_RewardPurchased")));
        F.Put(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave"),Schema);
        Require(F,F.Self(TEXT("DOR_CommitRegistration"))->GetReturnValuePin(),TEXT("commit:checked-save-rejected"),I);
        F.Return(nullptr,TEXT("true")); F.ValidateReturnPins();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardPublish"));
        auto* Loop=F.Each(F.Get(TEXT("DOR_RewardRoots")));
        auto* I=FindPin(Loop,TEXT("Array Index")); auto* Root=FindPin(Loop,TEXT("Array Element"));
        auto* Schema=F.At(F.Get(TEXT("DOR_RewardSchematics")),I);
        auto* WeaponIndex=F.At(F.Get(TEXT("DOR_RewardWeaponIndices")),I);
        auto* Weapon=F.At(F.Get(TEXT("DOR_RewardWeapons")),WeaponIndex);
        auto* Bank=F.At(F.Get(TEXT("DOR_RewardBanks")),WeaponIndex);
        auto* Item=F.Call(SchematicClass,TEXT("GetSchematicItem"));
        Connect(Schema,FindPin(Item,UEdGraphSchema_K2::PN_Self));
        const auto SetObject = [&](UEdGraphPin* Object, const TCHAR* Property, UEdGraphPin* Value)
        {
            auto* N=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("SetObjectPropertyByName"));
            Connect(Object,FindPin(N,TEXT("Object")));
            Connect(Value,FindPin(N,TEXT("Value")));
            FindPin(N,TEXT("PropertyName"))->DefaultValue=Property;
        };
        auto* Map=BankField(F,Bank);
        auto* Found=MapOp(F,TEXT("Map_Find"),Map,Root);
        auto* Present=F.Branch(Found->GetReturnValuePin());
        Require(F,F.Same(FindPin(Found,TEXT("Value")),Schema),TEXT("publish:foreign-bank-value"),I);
        auto* Existing=F.Tail; F.Tail=Present->GetElsePin();
        auto* Category=F.At(F.Get(TEXT("DOR_RewardSchematicCategories")),I);
        SetObject(Root,TEXT("Category"),F.At(F.Get(TEXT("DOR_RewardUpgradeCategories")),I));
        SetObject(Root,TEXT("SchematicCategory"),Category);
        SetObject(Schema,TEXT("Category"),Category);
        SetObject(Item->GetReturnValuePin(),TEXT("OwningItem"),Weapon);
        SetObject(Item->GetReturnValuePin(),TEXT("Overclock"),Root);
        MapOp(F,TEXT("Map_Add"),Map,Root,Schema);
        auto* Check=MapOp(F,TEXT("Map_Find"),Map,Root);
        Require(F,F.And(Check->GetReturnValuePin(),F.Same(FindPin(Check,TEXT("Value")),Schema)),
            TEXT("publish:map-readback-mismatch"),I);
        F.Join(Existing);
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardQualifyAndPublish"));
        auto* RigEntry=F.Input(TEXT("RigEntry"),MakeBoolPinType());
        Require(F,F.Self(TEXT("DOR_RewardReleaseOldSource"))->GetReturnValuePin(),TEXT("qualify:release-old-source"));
        auto* Init=F.Self(TEXT("DOR_Initialize"));
        F.Set(TEXT("DOR_RewardWaitingSafety"),F.Not(F.Get(TEXT("DOR_Ready"))));
        Require(F,Init->GetReturnValuePin(),TEXT("qualify:safety-initialize"));
        Require(F,F.Not(F.Get(TEXT("DOR_Disabled"))),TEXT("qualify:disabled"));
        Require(F,F.Not(F.Get(TEXT("DOR_Active"))),TEXT("qualify:transaction-active"));
        auto* Preflight=F.Self(TEXT("DOR_RewardPreflight"));
        Connect(RigEntry,FindPin(Preflight,TEXT("RigEntry"))); Quiet(F,Preflight->GetReturnValuePin());
        auto* Missing=F.Branch(F.Get(TEXT("DOR_RewardMissing")));
        auto* Registration=F.Branch(RigEntry);
        F.Tail=Registration->GetElsePin();
        auto* I=F.Get(TEXT("DOR_RewardFirstMissingRow"));
        auto* Bank=F.At(F.Get(TEXT("DOR_RewardBanks")),F.At(F.Get(TEXT("DOR_RewardWeaponIndices")),I));
        Receipt(F,TEXT("adopt:membership-missing"),I,MembershipDetail(F,I,Bank));
        F.Return(); F.Tail=Registration->GetThenPin();
        // Inherited entries can still be present after travel. Withhold them
        // while any permanent membership is missing from this live profile.
        F.Self(TEXT("DOR_RewardWithdraw"));
        auto* Commit=F.Self(TEXT("DOR_RewardRegister"));
        Quiet(F,Commit->GetReturnValuePin());
        F.Join(Missing->GetElsePin());
        Require(F,F.Self(TEXT("DOR_RewardContextCurrent"))->GetReturnValuePin(),TEXT("qualify:context-changed"));
        auto* RigOnly=F.Branch(RigEntry);
        Quiet(F,F.Self(TEXT("DOR_RewardPublish"))->GetReturnValuePin());
        F.Join(RigOnly->GetElsePin());
        F.Return(nullptr,TEXT("true")); F.Compile();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardRefresh"));
        auto* Unresolved=DORSafeString(F,TEXT(""));
        Require(F,F.Self(TEXT("DOR_RewardBindSettings"))->GetReturnValuePin(),TEXT("resolve:bind-user-settings"),nullptr,Unresolved);
        auto* Current=F.Self(TEXT("DOR_RewardContextCurrent"));
        auto* SafetyBecameReady=F.And(F.Get(TEXT("DOR_RewardWaitingSafety")),F.Get(TEXT("DOR_Ready")));
        auto* Same=F.Branch(F.And(Current->GetReturnValuePin(),F.Not(SafetyBecameReady)));
        F.Return(F.Get(TEXT("DOR_RewardReady"))); F.Tail=Same->GetElsePin();
        auto* PreviouslyObserved=F.Branch(F.Get(TEXT("DOR_RewardObserved")));
        F.Self(TEXT("DOR_RewardWithdraw"));
        F.Set(TEXT("DOR_RewardObserved"),nullptr,TEXT("false"));
        Log(F,TEXT("[DOR-REWARD]|stage=lifecycle-change-retracted"));
        F.Join(PreviouslyObserved->GetElsePin());
        auto* Save=F.Cache(TEXT("Save"),F.SaveGame());
        Require(F,F.IsValid(Save),TEXT("resolve:live-save"),nullptr,Unresolved);
        auto* CurrentSlot=F.Cache(TEXT("Slot"),Slot(F,Save));
        Require(F,F.Not(F.SameText(CurrentSlot,Unresolved)),TEXT("resolve:source-slot"),nullptr,CurrentSlot);
        auto* Live=F.Cache(TEXT("Data"),Data(F));
        Require(F,F.IsValid(Live),TEXT("resolve:game-data"),nullptr,CurrentSlot);
        auto* Schematics=F.Cache(TEXT("Settings"),Settings(F,Live));
        Require(F,F.IsValid(Schematics),TEXT("resolve:schematic-settings"),nullptr,CurrentSlot);
        F.Set(TEXT("DOR_RewardLastReceipt"),nullptr,TEXT(""));
        F.Set(TEXT("DOR_RewardSave"),Save);
        F.Set(TEXT("DOR_RewardSlot"),CurrentSlot);
        F.Set(TEXT("DOR_RewardAnon"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")));
        F.Set(TEXT("DOR_RewardIndex"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("Index")));
        F.Set(TEXT("DOR_RewardData"),Live); F.Set(TEXT("DOR_RewardSettings"),Schematics);
        F.Self(TEXT("DOR_RewardLoadWeapons"));
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("DOR_RewardBanks")));
        auto* Loop=F.Each(F.Get(TEXT("DOR_RewardWeapons")));
        Append(F,F.Get(TEXT("DOR_RewardBanks")),ResolveBank(F,Schematics,FindPin(Loop,TEXT("Array Element"))));
        F.Tail=FindPin(Loop,TEXT("Completed"));
        F.Set(TEXT("DOR_RewardObserved"),nullptr,TEXT("true"));
        auto* Level=F.Call(UGameplayStatics::StaticClass(),TEXT("GetCurrentLevelName"));
        auto* RigLevel=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Contains"));
        Connect(Level->GetReturnValuePin(),FindPin(RigLevel,TEXT("SearchIn")));
        FindPin(RigLevel,TEXT("Substring"))->DefaultValue=TEXT("Spacerig");
        FindPin(RigLevel,TEXT("bUseCase"))->DefaultValue=TEXT("false");
        auto* Check=F.Self(TEXT("DOR_RewardQualifyAndPublish"));
        Connect(RigLevel->GetReturnValuePin(),FindPin(Check,TEXT("RigEntry")));
        auto* Ready=F.Branch(Check->GetReturnValuePin());
        F.Set(TEXT("DOR_RewardReady"),nullptr,TEXT("true"));
        auto* InRig=F.Branch(RigLevel->GetReturnValuePin());
        auto* Detail=Concat(F,DORSafeConcat(F,TEXT("commit="),BoolText(F,F.Get(TEXT("DOR_RewardMissing")))),
            TEXT("|rows="),Number(F,F.Array(TEXT("Array_Length"),F.Get(TEXT("DOR_RewardRoots")))->GetReturnValuePin()));
        Receipt(F,TEXT("publish:verified"),nullptr,Detail);
        auto* Published=F.Tail; F.Tail=InRig->GetElsePin();
        Receipt(F,TEXT("adopt:verified"));
        F.Join(Published);
        F.Return(nullptr,TEXT("true")); F.Tail=Ready->GetElsePin();
        F.Self(TEXT("DOR_RewardWithdraw"));
        Log(F,TEXT("[DOR-REWARD]|stage=verification-failed-retracted"));
        F.Return(); F.ValidateReturnPins();
    }
    {
        FGraph F(Safety,TEXT("DOR_RewardVerifyRoots"));
        auto* Required=F.Input(TEXT("RequiredRoots"),Roots);
        auto* Live=F.Cache(TEXT("Live"),F.SaveGame());
        Quiet(F,F.And(F.IsValid(Live),F.Same(Live,F.Get(TEXT("DOR_SourceLiveObject")))));
        Quiet(F,F.SameText(Slot(F,Live),F.Get(TEXT("DOR_SourceSlot"))));
        Quiet(F,F.SameText(F.Field(Live,UFSDSaveGame::StaticClass(),TEXT("AnonymousID")),F.Get(TEXT("DOR_AnonymousId"))));
        Quiet(F,F.SameValue(F.Field(Live,UFSDSaveGame::StaticClass(),TEXT("Index")),F.Get(TEXT("DOR_SourceIndex"))));
        auto* Empty=F.Branch(F.EqInt(F.Array(TEXT("Array_Length"),Required)->GetReturnValuePin(),0));
        F.Return(nullptr,TEXT("true")); F.Tail=Empty->GetElsePin();
        Quiet(F,F.Get(TEXT("DOR_RewardReady")));
        Quiet(F,F.Self(TEXT("DOR_RewardContextCurrent"))->GetReturnValuePin());
        auto* Save=F.Get(TEXT("DOR_RewardSave"));
        auto* SS=F.Cache(TEXT("SchematicSave"),F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("SchematicSave")));
        auto* Loop=F.Each(Required); auto* Root=FindPin(Loop,TEXT("Array Element"));
        auto* Find=F.Array(TEXT("Array_Find"),F.Get(TEXT("DOR_RewardRoots")));
        Connect(Root,FindPin(Find,TEXT("ItemToFind")));
        auto* I=Find->GetReturnValuePin();
        Quiet(F,F.Not(F.EqInt(I,-1)));
        auto* PairId=F.At(F.Get(TEXT("DOR_RewardPairIds")),I);
        auto* SchemaId=F.At(F.Get(TEXT("DOR_RewardSchematicIds")),I);
        auto* Schema=F.At(F.Get(TEXT("DOR_RewardSchematics")),I);
        auto* Bank=F.At(F.Get(TEXT("DOR_RewardBanks")),F.At(F.Get(TEXT("DOR_RewardWeaponIndices")),I));
        Quiet(F,F.And(F.IsValid(Bank),F.And(F.SameValue(F.Id(Root),PairId),F.SameValue(F.Id(Schema),SchemaId))));
        auto* Found=MapOp(F,TEXT("Map_Find"),BankField(F,Bank),Root);
        Quiet(F,F.And(Found->GetReturnValuePin(),F.Same(FindPin(Found,TEXT("Value")),Schema)));
        Quiet(F,Contains(F,F.Field(Save,UFSDSaveGame::StaticClass(),TEXT("PurchasedItemUpgrades")),PairId));
        Quiet(F,Contains(F,F.StructField(SS,FSchematicSave::StaticStruct(),TEXT("OwnedSchematics")),SchemaId));
        Quiet(F,Contains(F,F.StructField(SS,FSchematicSave::StaticStruct(),TEXT("ForgedSchematics")),SchemaId));
        F.Tail=FindPin(Loop,TEXT("Completed")); F.Return(nullptr,TEXT("true")); F.Compile();
    }

    auto Insert = [](UEdGraph* Graph, UEdGraphPin* Out, const TCHAR* Name)
    {
        const auto Next=Out->LinkedTo; Out->BreakAllPinLinks();
        auto* Call=AddSelfCall(Graph,Name,0,-1200);
        Connect(Out,Call->GetExecPin());
        for(auto* P:Next) Connect(Call->GetThenPin(),P);
    };
    auto* Tick=FindFunctionGraph(Safety,TEXT("DOR_BackupTick"));
    Insert(Tick,FindFunctionEntry(Tick)->FindPin(UEdGraphSchema_K2::PN_Then),TEXT("DOR_RewardRefresh"));
    auto* Cleanup=FindFunctionGraph(Safety,TEXT("DOR_RestoreJournal"));
    for(auto* Node:TArray<UEdGraphNode*>(Cleanup->Nodes))
        if(auto* Call=Cast<UK2Node_CallFunction>(Node))
            if(Call->FunctionReference.GetMemberName()==TEXT("DOR_WriteDisabledMarker"))
            {
                auto* In=Call->GetExecPin();
                if(In->LinkedTo.Num()!=1)return false;
                Insert(Cleanup,In->LinkedTo[0],TEXT("DOR_RewardWithdraw"));
            }
    auto* Recovery=FBlueprintEditorUtils::FindEventGraph(Guard);
    for(auto* Node:TArray<UEdGraphNode*>(Recovery->Nodes))
        if(auto* Call=Cast<UK2Node_CallFunction>(Node))
            if(Call->FunctionReference.GetMemberName()==TEXT("DOR_Initialize"))
            {
                auto* Refresh=AddCall(Recovery,Safety->GeneratedClass,TEXT("DOR_RewardRefresh"),0,0);
                Connect(Call->GetThenPin(),Refresh->GetExecPin());
                Connect(FindPin(Call,UEdGraphSchema_K2::PN_Self)->LinkedTo[0],FindPin(Refresh,UEdGraphSchema_K2::PN_Self));
            }
    auto* Events=FBlueprintEditorUtils::FindEventGraph(Safety);
    auto* End=AddNode<UK2Node_Event>(Events,0,0);
    End->EventReference.SetExternalMember(TEXT("ReceiveEndPlay"),AActor::StaticClass());
    End->bOverrideFunction=true; End->AllocateDefaultPins();
    Insert(Events,End->FindPin(UEdGraphSchema_K2::PN_Then),TEXT("DOR_RewardUnbindSettings"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Rig); FKismetEditorUtilities::CompileBlueprint(Rig);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Guard); FKismetEditorUtilities::CompileBlueprint(Guard);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Safety); FKismetEditorUtilities::CompileBlueprint(Safety);
    if(Safety->Status==BS_Error || Rig->Status==BS_Error || Guard->Status==BS_Error)return false;

    // Build the exact descriptor columns from the same admitted manifest and
    // generated GUIDs used by installed-journal recovery.
    TMap<FName,TArray<UObject*>> Objects;
    TMap<FName,TArray<FString>> Paths;
    TArray<int32> WeaponIndices, TestIndices;
    TArray<FGuid> PairIds, SchematicIds;
    TSet<FGuid> SeenPairs, SeenSchematics;
    const auto CommonSpecs=DORCommonPreparedSpecs(false);
    // These fusions deliberately display as Unstable. Their exact stock
    // carriers have a different category and must retain that identity.
    struct FCarrierCategoryOverride { int32 Index; const TCHAR* Shell; const TCHAR* Category; };
    const TArray<FCarrierCategoryOverride> CarrierOverrides = {
        {309,TEXT("/Game/WeaponsNTools/LockOnRifle/Overclocks/OC_LockOnRifle_ArmorBreaking_C.OC_LockOnRifle_ArmorBreaking_C"),TEXT("SCAT_OC_Clean")},
        {406,TEXT("/Game/WeaponsNTools/Revolver/Overclocks/OC_Revolver_LuckyBullets_C.OC_Revolver_LuckyBullets_C"),TEXT("SCAT_OC_Clean")},
        {455,TEXT("/Game/WeaponsNTools/Cryospray/Overclocks/OC_Cryospray_Icicle_U.OC_Cryospray_Icicle_U"),TEXT("SCAT_OC_Balanced")}
    };
    int32 Legacy=0;
    int32 OverridesUsed=0;
    for(const auto& Row:Manifest)
    {
        const int32 Index=Row->GetIntegerField(TEXT("testIndex"));
        if(!DORSupportedPairTestIndices().Contains(Index))continue;
        const FString RootPath=Row->GetStringField(TEXT("pairUpgradeObjectPath"));
        FString SchemaPath=RootPath; SchemaPath.ReplaceInline(TEXT("/Pairs/"),TEXT("/Schematics/")); SchemaPath.ReplaceInline(TEXT("DORPair_"),TEXT("DORSchematic_"));
        auto* Root=LoadObject<UObject>(nullptr,*RootPath);
        auto* Schema=LoadObject<USchematic>(nullptr,*SchemaPath);
        const FString WeaponPath=Row->GetStringField(TEXT("weaponIdObjectPath"));
        if(!Root || !Root->IsA(RootClass) || !Schema || WeaponPath.IsEmpty())
        {
            UE_LOG(LogTemp,Error,TEXT("DOR-REWARD descriptor assets missing for index=%d root=%s schematic=%s"),Index,*RootPath,*SchemaPath);
            return false;
        }
        auto* RootId=FindFProperty<FStructProperty>(Root->GetClass(),TEXT("SavegameID"));
        auto* SchemaId=FindFProperty<FStructProperty>(Schema->GetClass(),TEXT("SavegameID"));
        if(!RootId || !SchemaId)return false;
        const FGuid R=*RootId->ContainerPtrToValuePtr<FGuid>(Root);
        const FGuid S=*SchemaId->ContainerPtrToValuePtr<FGuid>(Schema);
        const FString PairId=Row->GetStringField(TEXT("pairId"));
        const uint64 NumericId=FCString::Strtoui64(*PairId.Left(16),nullptr,16);
        const FGuid ExpectedRoot(static_cast<uint32>(NumericId>>32),static_cast<uint32>(NumericId),
            FCrc::StrCrc32(*(PairId+TEXT("|DOR|A"))),FCrc::StrCrc32(*(PairId+TEXT("|DOR|B"))));
        if(!R.IsValid() || R!=ExpectedRoot || !S.IsValid() || SeenPairs.Contains(R) || SeenSchematics.Contains(S))
        {
            UE_LOG(LogTemp,Error,TEXT("DOR-REWARD descriptor IDs invalid index=%d root=%s expected=%s schematic=%s"),
                Index,*R.ToString(),*ExpectedRoot.ToString(),*S.ToString());
            return false;
        }
        SeenPairs.Add(R); SeenSchematics.Add(S); PairIds.Add(R); SchematicIds.Add(S);
        FString ShellPath, UpgradePath, CategoryPath;
        if(const auto* Spec=CommonSpecs.FindByPredicate([&](const FDORPreparedPairSpec& P){return P.TestIndex==Index;}))
        {
            if(Spec->PairPath!=RootPath || Spec->SchematicPath!=SchemaPath || (Spec->PairGuid.IsValid() && Spec->PairGuid!=R) ||
                Spec->SchematicGuid!=S || Spec->WeaponIdPath!=WeaponPath)
            {
                UE_LOG(LogTemp,Error,TEXT("DOR-REWARD descriptor mismatch index=%d root=%s expected=%s schematic=%s expected=%s"),
                    Index,*R.ToString(),*Spec->PairGuid.ToString(),*S.ToString(),*Spec->SchematicGuid.ToString());
                return false;
            }
            ShellPath=Spec->NativeShellPath;
            UpgradePath=Spec->UpgradeCategoryPath; CategoryPath=Spec->SchematicCategoryPath;
        }
        else
        {
            if(Index!=277 && Index!=435 && Index!=438 && Index!=444 && Index!=359)return false;
            const TCHAR* Upgrade=Index==277?TEXT("UPC_RateOfFire"):
                Index==359?TEXT("UPC_Special_ReloadEffect_Enemies"):TEXT("UPC_Electricity");
            const TCHAR* Category=Index==277?TEXT("SCAT_OC_Balanced"):TEXT("SCAT_OC_Unstable");
            UpgradePath=FString::Printf(TEXT("/Game/WeaponsNTools/_UpgradeCategories/%s.%s"),Upgrade,Upgrade);
            CategoryPath=FString::Printf(TEXT("/Game/GameElements/Schematics/Categories/%s.%s"),Category,Category);
            ++Legacy;
        }
        if(UpgradePath.IsEmpty() || CategoryPath.IsEmpty())return false;
        FString CarrierCategoryOverride;
        if(const auto* Override=CarrierOverrides.FindByPredicate([Index](const FCarrierCategoryOverride& P){return P.Index==Index;}))
        {
            if(ShellPath!=Override->Shell || CategoryPath!=TEXT("/Game/GameElements/Schematics/Categories/SCAT_OC_Unstable.SCAT_OC_Unstable"))
            {
                UE_LOG(LogTemp,Error,TEXT("DOR-REWARD carrier category contract changed for index=%d"),Index);
                return false;
            }
            CarrierCategoryOverride=FString::Printf(TEXT("/Game/GameElements/Schematics/Categories/%s.%s"),Override->Category,Override->Category);
            ++OverridesUsed;
        }
        Objects.FindOrAdd(TEXT("DOR_RewardRoots")).Add(Root);
        Objects.FindOrAdd(TEXT("DOR_RewardSchematics")).Add(Schema);
        Paths.FindOrAdd(TEXT("DOR_RewardShellPaths")).Add(ShellPath);
        Paths.FindOrAdd(TEXT("DOR_RewardUpgradeCategoryPaths")).Add(UpgradePath);
        Paths.FindOrAdd(TEXT("DOR_RewardSchematicCategoryPaths")).Add(CategoryPath);
        Paths.FindOrAdd(TEXT("DOR_RewardCarrierCategoryOverrides")).Add(CarrierCategoryOverride);
        WeaponIndices.Add(Paths.FindOrAdd(TEXT("DOR_RewardWeaponPaths")).AddUnique(WeaponPath));
        TestIndices.Add(Index);
    }
    if(PairIds.Num()!=453 || Legacy!=5 || OverridesUsed!=CarrierOverrides.Num())return false;
    auto* Defaults=Safety->GeneratedClass->GetDefaultObject();
    for(const auto& Column:Objects)
    {
        auto* P=FindFProperty<FArrayProperty>(Safety->GeneratedClass,Column.Key);
        FScriptArrayHelper Array(P,P->ContainerPtrToValuePtr<void>(Defaults));
        auto* Inner=CastFieldChecked<FObjectProperty>(P->Inner);
        Array.Resize(Column.Value.Num());
        for(int32 I=0;I<Column.Value.Num();++I)Inner->SetObjectPropertyValue(Array.GetRawPtr(I),Column.Value[I]);
    }
    auto Write=[&](const TCHAR* Name,const auto& Values)
    {
        auto* P=FindFProperty<FArrayProperty>(Safety->GeneratedClass,Name);
        P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Defaults),&Values);
    };
    Write(TEXT("DOR_RewardPairIds"),PairIds); Write(TEXT("DOR_RewardSchematicIds"),SchematicIds);
    Write(TEXT("DOR_RewardWeaponIndices"),WeaponIndices);
    Write(TEXT("DOR_RewardTestIndices"),TestIndices);
    for(const auto& Column:Paths)Write(*Column.Key.ToString(),Column.Value);
    UE_LOG(LogTemp,Display,TEXT("DOR-REWARD preflight descriptors=%d legacy=%d distinctWeapons=%d"),PairIds.Num(),Legacy,Paths[TEXT("DOR_RewardWeaponPaths")].Num());
    return true;
}

