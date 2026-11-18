// 244's native weapon callback queries viewport-local saved selections. Rebuild
// only its count from the real owner's live roots after native initialization.
bool InstallDORSludgeCount(UBlueprint* Master,UBlueprint* Request,UBlueprint* Context)
{
    auto* Goo=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.GooGun"));
    auto* Gear=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.UpgradableGearComponent"));
    auto* CountUpgrade=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.GooGunProjectileUpgrade"));
    auto* Standard=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.StandardItemUpgrade"));
    auto* Values=LoadObject<UScriptStruct>(nullptr,TEXT("/Script/FSD.UpgradeValues"));
    auto* Big=LoadObject<UClass>(nullptr,TEXT("/Game/WeaponsNTools/GooCannon/PRJ_BigGooProjectile.PRJ_BigGooProjectile_C"));
    if(!Goo||!Gear||!CountUpgrade||!Standard||!Values||!Big)return false;
    {
        FDORNativeReadinessGraph F(Master,TEXT("DOR_SludgeRepairCount"),Master->GeneratedClass);
        auto* R=F.Input(TEXT("RequestObject"),MakeObjectPinType(Request->GeneratedClass));
        auto* W=F.Input(TEXT("Weapon"),MakeObjectPinType(AItem::StaticClass()));
        auto* Phase=F.Input(TEXT("Phase"),MakeIntPinType());
        auto* Stage=F.Input(TEXT("Stage"),MakeStringPinType());
        F.Require(F.IsValid(R));F.Require(F.Eq(Phase,F.Int(7)));
        F.Require(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("Code")),F.Int(244)));
        F.Require(F.Eq(F.Field(R,Request->GeneratedClass,TEXT("Operation")),F.Int(0)));
        FDORNativeProbeLine D(F,TEXT("CountReceipt"),TEXT("[DOR-SLUDGE-COUNT]|source=live-owner-equipped-roots|baseline=original-Big-query-BaseValue-only"));
        for(const auto& T:DORNativeTuplePins())D.Add(T.Key.ToString(),F.Field(R,Request->GeneratedClass,T.Key),T.Key==TEXT("Key")?FName(NAME_None):T.Key==TEXT("Epoch")||T.Key==TEXT("Request")?FName(TEXT("Conv_GuidToString")):FName(TEXT("Conv_IntToString")));
        D.Add(TEXT("stage"),Stage,NAME_None);D.Add(TEXT("localAuthority"),DORLocalHost(F));
        D.Add(TEXT("requestObject"),D.Path(R),NAME_None);D.Add(TEXT("acceptedRoot"),D.Path(F.Field(R,Request->GeneratedClass,TEXT("AcceptedRoot"))),NAME_None);D.Add(TEXT("weapon"),D.Path(W),NAME_None);
        auto Need=[&](UEdGraphPin* Condition,const TCHAR* Reason){auto* B=F.Branch(Condition);F.Tail=B->GetElsePin();D.Add(TEXT("write"),F.Str(TEXT("skipped")),NAME_None);D.Add(TEXT("reason"),F.Str(Reason),NAME_None);D.Record();F.Return();F.Tail=B->GetThenPin();};
        Need(F.Not(F.Get(TEXT("NativeLegacySourceOpen"))),TEXT("source-window-open"));
        Need(F.IsValid(W),TEXT("weapon-unavailable"));
        auto* G=F.Cache(TEXT("GooWeapon"),F.Cast(W,Goo));Need(F.IsValid(G),TEXT("weapon-not-goo"));
        auto* Before=F.Cache(TEXT("BeforeCount"),F.Field(G,Goo,TEXT("ChargedShotCount")));D.Add(TEXT("preCount"),Before,TEXT("Conv_IntToString"));
        auto Read=[&](UClass* C,const TCHAR* Name,UEdGraphPin* Object){auto* N=F.Call(C,Name);Connect(Object,FindPin(N,UEdGraphSchema_K2::PN_Self));return N->GetReturnValuePin();};
        auto* Owner=F.Cache(TEXT("WeaponOwner"),F.Cast(Read(AActor::StaticClass(),TEXT("GetOwner"),W),APlayerCharacter::StaticClass()));D.Add(TEXT("owner"),D.Path(Owner),NAME_None);Need(F.IsValid(Owner),TEXT("owner-unavailable"));
        auto* PS=F.Cache(TEXT("OwnerState"),Read(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"),Owner));D.Add(TEXT("ownerState"),D.Path(PS),NAME_None);Need(F.And(F.IsValid(PS),F.Same(PS,F.Field(R,Request->GeneratedClass,TEXT("TargetState")))),TEXT("owner-state-mismatch"));
        auto* Upgradable=F.Cache(TEXT("ActualUpgradable"),F.Field(W,AItem::StaticClass(),TEXT("UpgradableItem")));Need(F.IsValid(Upgradable),TEXT("upgradable-unavailable"));Need(F.Field(Upgradable,Gear,TEXT("bUpgradesAddedToItem")),TEXT("native-upgrades-not-ready"));
        auto* ClassCall=F.Call(UGameplayStatics::StaticClass(),TEXT("GetObjectClass"));Connect(W,FindPin(ClassCall,TEXT("Object")));
        auto* ClassCast=AddNode<UK2Node_ClassDynamicCast>(F.G,0,200);ClassCast->TargetType=AActor::StaticClass();ClassCast->SetPurity(true);ClassCast->AllocateDefaultPins();ForcePinType(ClassCast->GetCastSourcePin(),MakeClassPinType(UObject::StaticClass()));Connect(ClassCall->GetReturnValuePin(),ClassCast->GetCastSourcePin());
        auto* Class=F.Cache(TEXT("ActualWeaponClass"),ClassCast->GetCastResultPin());D.Add(TEXT("weaponClass"),D.ClassPath(Class),NAME_None);
        auto* Equipped=F.Call(UUpgradableGearComponent::StaticClass(),TEXT("GetEquippedUpgrades"));Connect(Class,FindPin(Equipped,TEXT("itemClass")));Connect(PS,FindPin(Equipped,TEXT("Player")));auto* Roots=F.Cache(TEXT("LiveRoots"),Equipped->GetReturnValuePin());
        D.Add(TEXT("equippedRootCount"),F.Array(TEXT("Array_Length"),Roots)->GetReturnValuePin(),TEXT("Conv_IntToString"));
        auto* RootCount=F.Cache(TEXT("AcceptedRootOccurrences"),F.Int(0));
        auto* OriginalCount=F.Cache(TEXT("OriginalCountLeafOccurrences"),F.Int(0));
        auto* OrdinaryCount=F.Cache(TEXT("OrdinaryCountLeafOccurrences"),F.Int(0));
        auto* Sum=F.Cache(TEXT("FlatSum"),F.Float(0));auto* Factor=F.Cache(TEXT("FactorProduct"),F.Float(1));
        auto Increment=[&](UEdGraphPin* Value){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(Value,FindPin(N,TEXT("A")));FindPin(N,TEXT("B"))->DefaultValue=TEXT("1");F.Set(F.Variable(Value),N->GetReturnValuePin());};
        auto Accumulate=[&](UEdGraphPin* Candidate,UEdGraphPin* Root){
            auto* Leaf=F.Cache(TEXT("CountLeaf"),F.Cast(Candidate,CountUpgrade));auto* IsCount=F.Branch(F.IsValid(Leaf));
            auto* Enum=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ByteByte"));Connect(F.Field(Leaf,CountUpgrade,TEXT("upgradeType")),FindPin(Enum,TEXT("A")));FindPin(Enum,TEXT("B"))->DefaultValue=TEXT("1");auto* Fragment=F.Branch(Enum->GetReturnValuePin());
            auto* Amount=F.Cache(TEXT("LeafAmount"),F.Field(Leaf,Standard,TEXT("amount")));auto* Percent=F.Cache(TEXT("LeafPercentage"),F.Field(Leaf,Standard,TEXT("IsPercentage")));auto* Path=F.Cache(TEXT("CountLeafPath"),D.Path(Leaf));
            D.Add(TEXT("countRoot"),D.Path(Root),NAME_None);D.Add(TEXT("countLeaf"),Path,NAME_None);D.Add(TEXT("amount"),Amount,TEXT("Conv_FloatToString"));D.Add(TEXT("percentage"),Percent);
            auto* Original=F.Branch(F.Eq(Path,F.Str(TEXT("/Game/WeaponsNTools/GooCannon/Overclocks/OC_BonusesAndPenalties/Bonus_FragmentAmount_GooCannon.Bonus_FragmentAmount_GooCannon")),TEXT("EqualEqual_StrStr")));Increment(OriginalCount);F.Merge(Original->GetElsePin());
            auto* Ordinary=F.Branch(F.Eq(Path,F.Str(TEXT("/Game/WeaponsNTools/GooCannon/Upgrades/UPG_Goo_B_FragmentAmount.UPG_Goo_B_FragmentAmount")),TEXT("EqualEqual_StrStr")));Increment(OrdinaryCount);F.Merge(Ordinary->GetElsePin());
            auto* Percentage=F.Branch(Percent);auto* Multiply=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Multiply_FloatFloat"));Connect(Factor,FindPin(Multiply,TEXT("A")));Connect(Amount,FindPin(Multiply,TEXT("B")));F.Set(F.Variable(Factor),Multiply->GetReturnValuePin());auto* PercentageDone=F.Tail;
            F.Tail=Percentage->GetElsePin();auto* Add=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_FloatFloat"));Connect(Sum,FindPin(Add,TEXT("A")));Connect(Amount,FindPin(Add,TEXT("B")));F.Set(F.Variable(Sum),Add->GetReturnValuePin());F.Merge(PercentageDone);F.Merge(Fragment->GetElsePin());F.Merge(IsCount->GetElsePin());
        };
        auto* Loop=F.Each(Roots);auto* Root=FindPin(Loop,TEXT("Array Element"));auto* Accepted=F.Branch(F.Same(Root,F.Field(R,Request->GeneratedClass,TEXT("AcceptedRoot"))));Increment(RootCount);F.Merge(Accepted->GetElsePin());
        auto* Combined=F.Cache(TEXT("CombinedRoot"),F.Cast(Root,UCombinedUpgrade::StaticClass()));auto* IsCombined=F.Branch(F.IsValid(Combined));auto* Children=F.Call(UCombinedUpgrade::StaticClass(),TEXT("GetUpgrades"));Connect(Combined,FindPin(Children,UEdGraphSchema_K2::PN_Self));auto* ChildLoop=F.Each(Children->GetReturnValuePin());Accumulate(FindPin(ChildLoop,TEXT("Array Element")),Root);F.Tail=FindPin(ChildLoop,TEXT("Completed"));auto* CombinedDone=F.Tail;
        F.Tail=IsCombined->GetElsePin();Accumulate(Root,Root);F.Merge(CombinedDone);F.Tail=FindPin(Loop,TEXT("Completed"));
        D.Add(TEXT("acceptedRootOccurrences"),RootCount,TEXT("Conv_IntToString"));D.Add(TEXT("originalCountLeafOccurrences"),OriginalCount,TEXT("Conv_IntToString"));D.Add(TEXT("ordinaryCountLeafOccurrences"),OrdinaryCount,TEXT("Conv_IntToString"));D.Add(TEXT("flatSum"),Sum,TEXT("Conv_FloatToString"));D.Add(TEXT("factorProduct"),Factor,TEXT("Conv_FloatToString"));
        Need(F.And(F.IsValid(F.Field(R,Request->GeneratedClass,TEXT("AcceptedRoot"))),F.Eq(RootCount,F.Int(1))),TEXT("accepted-live-root-not-exactly-once"));
        // Only BaseValue is equipment-independent. CurrentValue uses the wrong
        // selection domain and must never participate in this calculation.
        auto* Query=F.Call(CountUpgrade,TEXT("GetUpgradedValue"));Connect(PS,FindPin(Query,TEXT("Player")));Connect(Class,FindPin(Query,TEXT("Item")));FindPin(Query,TEXT("upgradedItem"))->DefaultObject=Big;FindPin(Query,TEXT("NewUpgradeType"))->DefaultValue=TEXT("EGooGunProjectileUpgrades::FragmentCount");auto* Base=F.Cache(TEXT("OriginalBigBase"),F.FieldOf(Query->GetReturnValuePin(),Values,TEXT("BaseValue")));
        auto* Add=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_FloatFloat"));Connect(Base,FindPin(Add,TEXT("A")));Connect(Sum,FindPin(Add,TEXT("B")));auto* Multiply=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Multiply_FloatFloat"));Connect(Add->GetReturnValuePin(),FindPin(Multiply,TEXT("A")));Connect(Factor,FindPin(Multiply,TEXT("B")));auto* Trunc=F.Call(UKismetMathLibrary::StaticClass(),TEXT("FTrunc"));Connect(Multiply->GetReturnValuePin(),FindPin(Trunc,TEXT("A")));auto* Computed=F.Cache(TEXT("ComputedCount"),Trunc->GetReturnValuePin());
        F.Put(G,Goo,TEXT("ChargedShotCount"),Computed);D.Add(TEXT("base"),Base,TEXT("Conv_FloatToString"));D.Add(TEXT("computed"),Computed,TEXT("Conv_IntToString"));D.Add(TEXT("postCount"),F.Field(G,Goo,TEXT("ChargedShotCount")),TEXT("Conv_IntToString"));D.Add(TEXT("write"),F.Str(TEXT("applied")),NAME_None);D.Record();F.Return(nullptr,TEXT("true"));F.Compile();
    }
    // The settled sample is itself inserted after stable native proof. Insert
    // independently before it; neither its once flag nor any return gates repair.
    for(auto* BP:{Master,Context}){
        const TCHAR* Name=BP==Master?TEXT("DOR_NativeAdvanceSettlement"):TEXT("DOR_NativePollOwner");auto* Graph=FindFunctionGraph(BP,Name);if(!Graph)return false;UK2Node_CallFunction* Settled=nullptr;
        for(auto* N:Graph->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("DOR_SludgeSettled"))Settled=C;
        if(!Settled)return false;const auto Links=Settled->GetExecPin()->LinkedTo;Settled->GetExecPin()->BreakAllPinLinks();auto* Repair=AddCall(Graph,Master->GeneratedClass,TEXT("DOR_SludgeRepairCount"),0,0);
        for(const TCHAR* Pin:{TEXT("self"),TEXT("RequestObject"),TEXT("Weapon"),TEXT("Phase"),TEXT("Stage")}){auto* From=FindPin(Settled,Pin);auto* To=FindPin(Repair,Pin);To->DefaultValue=From->DefaultValue;To->DefaultObject=From->DefaultObject;for(auto* Source:From->LinkedTo)Connect(Source,To);}
        for(auto* P:Links)Connect(P,Repair->GetExecPin());Connect(Repair->GetThenPin(),Settled->GetExecPin());FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;
    }
    return Master->Status!=BS_Error;
}
