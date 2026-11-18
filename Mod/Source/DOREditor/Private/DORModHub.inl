// Compile-time interface declarations matching the user-supplied ModHub.pak.
// They live outside RuntimeRoot and must NEVER be shipped in the DOR pak.
FEdGraphPinType DORInterfaceType(UClass* C,bool Array=false)
{
    auto T=MakeObjectPinType(C,Array); T.PinCategory=UEdGraphSchema_K2::PC_Interface; return T;
}
UK2Node_FunctionResult* DORNamedResult(FDORSafeGraph& F,const TMap<FName,FEdGraphPinType>& Outputs)
{
    auto* R=AddNode<UK2Node_FunctionResult>(F.G,0,0); R->FunctionReference=F.Entry->FunctionReference; R->AllocateDefaultPins();
    for(const auto& O:Outputs) if(!R->FindPin(O.Key))
    {
        auto* P=R->CreateUserDefinedPin(O.Key,O.Value,EGPD_Input); const FName Old=P->PinName; P->PinName=O.Key;
        for(auto& Info:R->UserDefinedPins) if(Info->PinName==Old) Info->PinName=O.Key;
    }
    Connect(F.Tail,R->FindPin(UEdGraphSchema_K2::PN_Execute)); F.Tail=nullptr; return R;
}
UBlueprint* DORHubInterface(const TCHAR* Name)
{
    const FString Path=FString(TEXT("/Game/_ModHub/"))+Name; DeleteGeneratedFile(Path);
    auto* P=CreatePackage(*Path);
    return FKismetEditorUtilities::CreateBlueprint(UInterface::StaticClass(),P,Name,BPTYPE_Interface,
        UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass(),TEXT("DORHubCompileStub"));
}
UEdGraphPin* DORCastPin(FDORSafeGraph& F,UEdGraphPin* Obj,UClass* C)
{
    auto* N=AddNode<UK2Node_DynamicCast>(F.G,0,0); N->TargetType=C; N->SetPurity(true); N->AllocateDefaultPins();
    ForcePinType(N->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass())); Connect(Obj,N->GetCastSourcePin()); return N->GetCastResultPin();
}
UEdGraphPin* DORLocalHost(FDORSafeGraph& F)
{
    auto* PC=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerController"));
    auto* Authority=F.Call(AActor::StaticClass(),TEXT("HasAuthority")); Connect(PC->GetReturnValuePin(),FindPin(Authority,UEdGraphSchema_K2::PN_Self));
    return Authority->GetReturnValuePin();
}
UEdGraphPin* DORRoster(FDORSafeGraph& F)
{
    auto* N=F.Call(UActorFunctionLibrary::StaticClass(),TEXT("GetAllPlayerCharacters"));
    Connect(SelfPin(F.G,0,0),FindPin(N,TEXT("WorldContext"))); return N->GetReturnValuePin();
}
FDORStablePlayerKeyPins DORPlayerIdentity(FDORSafeGraph& F,UEdGraphPin* Player)
{
    auto Result=AddDORStableIdentityCall(F.G,Player,++F.Serial*160,100);
    F.Run(Result.Call);
    return Result;
}
UEdGraphPin* DORPlayerKeyValid(FDORSafeGraph& F,UEdGraphPin* Key)
{
    auto* Missing=F.Equal(Key,DORSafeString(F,TEXT("")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
    auto* Empty=F.Equal(Key,DORSafeString(F,TEXT("net-v1:")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
    auto* Invalid=F.Equal(Key,DORSafeString(F,TEXT("net-v1:INVALID")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
    auto* EmptyOrInvalid=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(Empty,FindPin(EmptyOrInvalid,TEXT("A")));Connect(Invalid,FindPin(EmptyOrInvalid,TEXT("B")));
    auto* Bad=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(Missing,FindPin(Bad,TEXT("A")));Connect(EmptyOrInvalid->GetReturnValuePin(),FindPin(Bad,TEXT("B")));
    return F.Not(Bad->GetReturnValuePin());
}
UEdGraphPin* DORPlayerLabel(FDORSafeGraph& F,UEdGraphPin* Player)
{
    auto* Name=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerName")); Connect(Player,FindPin(Name,UEdGraphSchema_K2::PN_Self));
    auto* PS=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState")); Connect(Player,FindPin(PS,UEdGraphSchema_K2::PN_Self));
    auto* ID=F.Field(PS->GetReturnValuePin(),APlayerState::StaticClass(),TEXT("PlayerId"));
    auto* Str=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString")); Connect(ID,FindPin(Str,TEXT("InInt")));
    auto* Suffix=DORSafeConcat(F,TEXT("#"),Str->GetReturnValuePin());
    auto* Join=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr")); Connect(Name->GetReturnValuePin(),FindPin(Join,TEXT("A"))); Connect(Suffix,FindPin(Join,TEXT("B")));
    return Join->GetReturnValuePin();
}
void DORSetStatus(FDORSafeGraph& F,const FString& Text)
{
    auto* Set=F.Call(UTextBlock::StaticClass(),TEXT("SetText")); Connect(F.Get(TEXT("Status")),FindPin(Set,UEdGraphSchema_K2::PN_Self));
    FindPin(Set,TEXT("InText"))->DefaultTextValue=FText::FromString(Text);
}

bool BuildDORModHub(UBlueprint* Safety,UBlueprint* Master,UBlueprint*& MenuHost,UWidgetBlueprint*& Page,UBlueprint*& Settings,const TArray<TSharedPtr<FJsonObject>>& ManifestPairs)
{
    struct FMenuOption
    {
        int32 TestIndex;
        FString ClassKey;
        FString Slot;
        FString WeaponIdentity;
        FString Label;
    };
    const TMap<int32,FString> LegacyLabels = {
        {385,TEXT("385 | 侦察兵 · DRAK · 废热回导 + 护盾协能")},
        {388,TEXT("388 | 侦察兵 · DRAK · 恒温电导 + 废热回导")},
        {435,TEXT("435 | 工程师 · “百万”伏特微型冲锋枪 · 导体附加 + 炮台电磁爆炸")},
        {444,TEXT("444 | 工程师 · “百万”伏特微型冲锋枪 · 精心保养 + 炮塔电磁爆炸")},
        {438,TEXT("438 | 工程师 · “百万”伏特微型冲锋枪 · 电磁反馈 + 精心保养")},
        {277,TEXT("277 | 工程师 · 心石聚能炮 · 自动光控 + 聚能过载")},
        {359,TEXT("359 | 钻机手 · 苏巴特120半自动手枪 · 爆破装填 + 全自动射击")}
    };
    const TMap<FString,FString> CanonicalProfessionLabels = {
        {TEXT("scout"),TEXT("侦察兵")},
        {TEXT("engineer"),TEXT("工程师")},
        {TEXT("gunner"),TEXT("枪手")},
        {TEXT("driller"),TEXT("钻机手")}
    };
    TMap<int32,FString> Labels=LegacyLabels;
    for(const FDORGF01PairSpec& Spec:GF01CandidateSpecs)
    {
        if(Labels.Contains(Spec.TestIndex))return false;
        Labels.Add(Spec.TestIndex,Spec.OptionLabel);
    }
    for(const FDORPreparedPairSpec& Spec:GF02Specs)
    {
        if(Spec.TestIndex==438)continue;
        if(Labels.Contains(Spec.TestIndex))return false;
        Labels.Add(Spec.TestIndex,Spec.OptionLabel);
    }
    for(const FDORPreparedPairSpec& Spec:GF04Specs)
    {
        if(Spec.TestIndex==359){if(Labels.FindChecked(359)!=Spec.OptionLabel)return false;continue;}
        if(Labels.Contains(Spec.TestIndex))return false;
        Labels.Add(Spec.TestIndex,Spec.OptionLabel);
    }
    for(const FDORPreparedPairSpec& Spec:([&](){auto Specs=GF03Specs;Specs.Append(RemainingSpecs);return Specs;})())
    {
        if(Labels.Contains(Spec.TestIndex))return false;
        Labels.Add(Spec.TestIndex,Spec.OptionLabel);
    }
    TArray<FMenuOption> MenuOptions;
    TSet<int32> SeenMenuIndices;
    for(const TSharedPtr<FJsonObject>& Pair:ManifestPairs)
    {
        const int32 Index=Pair->GetIntegerField(TEXT("testIndex"));
        if(!DORIsSupportedPairTestIndex(Index))continue;
        const FString* Label=Labels.Find(Index);
        const FString ClassKey=Pair->GetStringField(TEXT("classKey"));
        const FString WeaponIdentity=Pair->GetStringField(TEXT("weaponIdObjectPath"));
        if(!Label || WeaponIdentity.IsEmpty() || SeenMenuIndices.Contains(Index))return false;
        SeenMenuIndices.Add(Index);
        FString MenuLabel=*Label;
        const FString* CanonicalClass=CanonicalProfessionLabels.Find(ClassKey);
        const int32 ProfessionSeparator=Label->Find(TEXT(" · "));
        if(CanonicalClass && ProfessionSeparator!=INDEX_NONE)
            MenuLabel=FString::Printf(TEXT("%d | %s"),Index,**CanonicalClass)+Label->Mid(ProfessionSeparator);
        MenuOptions.Add({Index,ClassKey,Pair->GetStringField(TEXT("slot")),WeaponIdentity,MenuLabel});
    }
    if(MenuOptions.Num()!=DORSupportedPairTestIndices().Num())return false;
    MenuOptions.Sort([](const FMenuOption& A,const FMenuOption& B)
    {
        if(A.ClassKey!=B.ClassKey)return A.ClassKey<B.ClassKey;
        if(A.Slot!=B.Slot)return A.Slot<B.Slot;
        if(A.WeaponIdentity!=B.WeaponIdentity)return A.WeaponIdentity<B.WeaponIdentity;
        return A.TestIndex<B.TestIndex;
    });
    UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=start"));
    UClass* InventoryComponentClass=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.InventoryComponent"));
    if(!InventoryComponentClass)return false;
    auto* PageI=DORHubInterface(TEXT("IHubPageWidget")); auto* ModI=DORHubInterface(TEXT("IHubMod"));
    if(!PageI || !ModI)return false;
    FEdGraphPinType TextType; TextType.PinCategory=UEdGraphSchema_K2::PC_Text;
    {FDORSafeGraph F(PageI,TEXT("GetPageInfo")); DORNamedResult(F,{{TEXT("PageName"),TextType}});F.Compile();}
    {FDORSafeGraph F(PageI,TEXT("GetContainerMode"));DORNamedResult(F,{{TEXT("bStretchToContainer"),MakeBoolPinType()}});F.Compile();}
    {FDORSafeGraph F(PageI,TEXT("HubPageOpened"));F.Compile();}
    {FDORSafeGraph F(PageI,TEXT("HubPageClosed"));F.Compile();}
    {FDORSafeGraph F(ModI,TEXT("GetModInfo"));DORNamedResult(F,{{TEXT("ModName"),TextType},{TEXT("ModAuthor"),TextType},{TEXT("ModVersion"),TextType}});F.Compile();}
    {FDORSafeGraph F(ModI,TEXT("GetModPages"));DORNamedResult(F,{{TEXT("HubPages"),DORInterfaceType(PageI->GeneratedClass,true)}});F.Compile();}
    if(!SaveBlueprint(PageI)||!SaveBlueprint(ModI))return false;
    UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=interfaces-ready"));
    Settings=CreateObjectBlueprint(FString(RuntimeRoot)/TEXT("Blueprints/SG_DOR_ManualSettings"),TEXT("SG_DOR_ManualSettings"),USaveGame::StaticClass());
    // Keys/Pairs and KeyN/PairN are retained so existing files deserialize
    // without attribution. IdPairN is the Alpha75 single-reservation field.
    // Alpha76 writes two slot-specific values under the same stable player key;
    // an old 277 migrates to secondary and old 385/388 migrate to primary.
    // Unknown legacy values are never guessed or auto-applied.
    AddSavedTypedVariable(Settings,TEXT("Keys"),MakeStringPinType(true)); AddSavedTypedVariable(Settings,TEXT("Pairs"),MakeIntPinType(true));
    for(int32 Slot=0;Slot<4;++Slot)
    {
        AddSavedTypedVariable(Settings,FName(*FString::Printf(TEXT("Key%d"),Slot)),MakeStringPinType());
        AddSavedTypedVariable(Settings,FName(*FString::Printf(TEXT("Pair%d"),Slot)),MakeIntPinType());
        AddSavedTypedVariable(Settings,FName(*FString::Printf(TEXT("IdKey%d"),Slot)),MakeStringPinType());
        AddSavedTypedVariable(Settings,FName(*FString::Printf(TEXT("IdPair%d"),Slot)),MakeIntPinType());
        AddSavedTypedVariable(Settings,FName(*FString::Printf(TEXT("IdPrimaryPair%d"),Slot)),MakeIntPinType());
        AddSavedTypedVariable(Settings,FName(*FString::Printf(TEXT("IdSecondaryPair%d"),Slot)),MakeIntPinType());
    }
    FKismetEditorUtilities::CompileBlueprint(Settings);
    MenuHost=CreateActorBlueprint(FString(RuntimeRoot)/TEXT("Blueprints/BP_DOR_ModHub"),TEXT("BP_DOR_ModHub"),AActor::StaticClass(),false,false);
    const FString PagePath=FString(RuntimeRoot)/TEXT("UI/WBP_DOR_ModHub"); DeleteGeneratedFile(PagePath);
    Page=Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UUserWidget::StaticClass(),CreatePackage(*PagePath),TEXT("WBP_DOR_ModHub"),BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("DORHub")));
    if(!MenuHost||!Page||!Settings)return false;
    FBlueprintEditorUtils::ImplementNewInterface(MenuHost,ModI->GeneratedClass->GetFName());
    FBlueprintEditorUtils::ImplementNewInterface(Page,PageI->GeneratedClass->GetFName());
    if(!BuildDORSafetyAccess(MenuHost,Safety->GeneratedClass)||!BuildDORSafetyAccess(Page,Safety->GeneratedClass))return false;
    AddTypedVariable(MenuHost,TEXT("Page"),MakeObjectPinType(Page->GeneratedClass));
    AddTypedVariable(MenuHost,TEXT("DebugPage"),MakeObjectPinType(Page->GeneratedClass));
    AddTypedVariable(MenuHost,TEXT("MaintenancePage"),MakeObjectPinType(Page->GeneratedClass));
    AddTypedVariable(MenuHost,TEXT("AdvancedPage"),MakeObjectPinType(Page->GeneratedClass));
    AddTypedVariable(MenuHost,TEXT("Settings"),MakeObjectPinType(Settings->GeneratedClass));
    AddTypedVariable(MenuHost,TEXT("SettingsSlot"),MakeStringPinType());
    AddTypedVariable(MenuHost,TEXT("AttemptedPlayers"),MakeStringPinType(true));
    AddTypedVariable(MenuHost,TEXT("ConsumedPlayers"),MakeStringPinType(true));
    auto ManualRevisions=MakeStringPinType();ManualRevisions.ContainerType=EPinContainerType::Map;ManualRevisions.PinValueType.TerminalCategory=UEdGraphSchema_K2::PC_Int;
    AddTypedVariable(MenuHost,TEXT("ManualRevisions"),ManualRevisions);
    AddTypedVariable(MenuHost,TEXT("PendingKey"),MakeStringPinType());
    AddTypedVariable(MenuHost,TEXT("PendingPlayer"),MakeObjectPinType(APlayerCharacter::StaticClass()));
    AddTypedVariable(MenuHost,TEXT("PendingPair"),MakeIntPinType());
    AddTypedVariable(MenuHost,TEXT("PendingSlot"),MakeStringPinType());
    AddTypedVariable(MenuHost,TEXT("PendingIsLocal"),MakeBoolPinType());
    AddTypedVariable(MenuHost,TEXT("PendingRemoteBaseline"),MakeIntPinType());
    AddTypedVariable(MenuHost,TEXT("PendingSettleTicks"),MakeIntPinType());
    AddTypedVariable(MenuHost,TEXT("MissionSettleTicks"),MakeIntPinType());
    AddTypedVariable(MenuHost,TEXT("LocalPrimaryProbeInventory"),MakeObjectPinType(InventoryComponentClass));
    AddTypedVariable(MenuHost,TEXT("LocalPrimaryProbeItem"),MakeObjectPinType(AItem::StaticClass()));
    AddTypedVariable(MenuHost,TEXT("MissionSettingsReady"),MakeBoolPinType());
    AddTypedVariable(MenuHost,TEXT("RigEntryClearAttempted"),MakeBoolPinType());
    AddTypedVariable(MenuHost,TEXT("LegacySettingsWarningShown"),MakeBoolPinType());
    AddTypedVariable(Page,TEXT("Host"),MakeObjectPinType(MenuHost->GeneratedClass));
    AddTypedVariable(Page,TEXT("PageMode"),MakeIntPinType());
    AddTypedVariable(Page,TEXT("PageName"),TextType);
    AddTypedVariable(Page,TEXT("RosterPlayers"),MakeObjectPinType(APlayerCharacter::StaticClass(),true));
    // Keep the numeric ids separate from localized/display labels.  Reading the
    // visible string back from ComboBoxString proved nondeterministic in the live
    // ModHub widget ("none" could return the previously selected 385 label).
    AddTypedVariable(Page,TEXT("PrimaryPairOptionIds"),MakeIntPinType(true));
    AddTypedVariable(Page,TEXT("SecondaryPairOptionIds"),MakeIntPinType(true));
    AddTypedVariable(Page,TEXT("PairChoicesPlayer"),MakeObjectPinType(APlayerCharacter::StaticClass()));
    AddTypedVariable(Page,TEXT("PairChoicesDirty"),MakeBoolPinType());
    FKismetEditorUtilities::CompileBlueprint(MenuHost); FKismetEditorUtilities::CompileBlueprint(Page);
    if(!BuildDORStableIdentityResolver(MenuHost))return false;
    UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=types-ready"));
    // ModHub supplies the viewport height.  Keep the full page inside one
    // vertical scroll box so compact windows can still reach both selectors
    // and the Apply button without changing any of their bindings.
    auto* Shell=MakeWidget<UVerticalBox>(Page->WidgetTree,TEXT("PageShell"));Page->WidgetTree->RootWidget=Shell;
    auto* Switcher=MakeWidget<UWidgetSwitcher>(Page->WidgetTree,TEXT("Sections"));Shell->AddChildToVerticalBox(Switcher)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Scroll=MakeWidget<UScrollBox>(Page->WidgetTree,TEXT("PageScroll"));Switcher->AddChild(Scroll);
    Scroll->SetOrientation(Orient_Vertical);
    Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    Scroll->SetAlwaysShowScrollbar(true);
    Scroll->AlwaysShowScrollbarTrack=true;
    Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
    Scroll->SetAllowOverscroll(false);
    auto* Root=MakeWidget<UVerticalBox>(Page->WidgetTree,TEXT("Root")); Scroll->AddChild(Root);
    auto* FusionRoot=Root;
    auto Section=[&](const TCHAR* ScrollName,const TCHAR* RootName){auto* S=MakeWidget<UScrollBox>(Page->WidgetTree,ScrollName);S->SetAlwaysShowScrollbar(true);S->SetAllowOverscroll(false);Switcher->AddChild(S);auto* R=MakeWidget<UVerticalBox>(Page->WidgetTree,RootName);S->AddChild(R);return R;};
    auto* DebugRoot=Section(TEXT("DebugScroll"),TEXT("DebugRoot"));auto* MaintenanceRoot=Section(TEXT("MaintenanceScroll"),TEXT("MaintenanceRoot"));
    Section(TEXT("AdvancedScroll"),TEXT("AdvancedRoot"));
    auto Text=[&](const TCHAR* Name,const FString& Value,int32 Size=18)
    {
        auto* W=MakeWidget<UTextBlock>(Page->WidgetTree,Name); W->SetText(FText::FromString(Value)); W->SetAutoWrapText(true);
        auto Font=W->Font; Font.Size=Size; W->SetFont(Font); Root->AddChildToVerticalBox(W)->SetPadding(FMargin(10,8)); return W;
    };
    Root=DebugRoot;
    Text(TEXT("DebugTitle"),TEXT("DOR · 调试检查"),26);
    Text(TEXT("DebugInfo"),TEXT("检查当前目标的装备应用与事务收尾；结构通过不代表所有武器效果已验证。"));
    Root=FusionRoot;
    Text(TEXT("Title"),TEXT("DOR · 融合预约"),26);
    Root=MaintenanceRoot;
    Text(TEXT("MaintenanceTitle"),TEXT("DOR · 存档维护"),26);
    Text(TEXT("SafetyInfo"),TEXT("首次启动自动备份保留在 Saved/SaveGames/Mods/DOR_FirstInstallBackup_*.sav。\n清理仅针对本机当前存档，不触碰经验、晋升或其他模组的标记。请在空间站执行；成功后退出游戏，再手动删除 DOR pak。"));
    auto* Confirm=MakeWidget<UCheckBox>(Page->WidgetTree,TEXT("ConfirmCleanup")); Root->AddChildToVerticalBox(Confirm)->SetPadding(FMargin(10,4));
    auto* ConfirmText=MakeWidget<UTextBlock>(Page->WidgetTree,TEXT("ConfirmText")); ConfirmText->SetText(FText::FromString(TEXT("我确认清理本机当前档，并停用本档 DOR"))); Confirm->AddChild(ConfirmText);
    auto Button=[&](const TCHAR* Name,const FString& Value)
    {
        auto* W=MakeWidget<UButton>(Page->WidgetTree,Name); Root->AddChildToVerticalBox(W)->SetPadding(FMargin(10,5));
        auto* Label=MakeWidget<UTextBlock>(Page->WidgetTree,FName(*(FString(Name)+TEXT("Label")))); Label->SetText(FText::FromString(Value)); W->AddChild(Label); return W;
    };
    Button(TEXT("Clean"),TEXT("清理存档 / 准备卸载"));
    Root=FusionRoot;
    Text(TEXT("ManualInfo"),TEXT("手动配装后，本局后续奖励与补领关闭；深潜关闭至整次结束。"));
    Text(TEXT("PlayerLabel"),TEXT("1. 选择玩家（下框显示玩家名与连接编号）"));
    auto* Players=MakeWidget<UComboBoxString>(Page->WidgetTree,TEXT("Players")); Root->AddChildToVerticalBox(Players)->SetPadding(FMargin(10,5));
    Text(TEXT("PrimaryPairLabel"),TEXT("2. 主武器融合（默认：无）"));
    auto* PrimaryPairs=MakeWidget<UComboBoxString>(Page->WidgetTree,TEXT("PrimaryPairs")); Root->AddChildToVerticalBox(PrimaryPairs)->SetPadding(FMargin(10,5));
    PrimaryPairs->AddOption(TEXT("0 | 无 / 不自动应用")); PrimaryPairs->SetSelectedIndex(0);
    Text(TEXT("SecondaryPairLabel"),TEXT("3. 次武器融合（默认：无）"));
    auto* SecondaryPairs=MakeWidget<UComboBoxString>(Page->WidgetTree,TEXT("SecondaryPairs")); Root->AddChildToVerticalBox(SecondaryPairs)->SetPadding(FMargin(10,5));
    SecondaryPairs->AddOption(TEXT("0 | 无 / 不自动应用")); SecondaryPairs->SetSelectedIndex(0);
    Button(TEXT("Refresh"),TEXT("刷新当前玩家")); Button(TEXT("Apply"),TEXT("保存所选玩家的两项预约"));
    Root=Shell;
    Text(TEXT("Status"),TEXT("等待操作。"));
    FKismetEditorUtilities::CompileBlueprint(Page);
    UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=widgets-ready"));
    {
        FDORSafeGraph F(MenuHost,TEXT("GetModInfo")); auto* R=DORNamedResult(F,{{TEXT("ModName"),TextType},{TEXT("ModAuthor"),TextType},{TEXT("ModVersion"),TextType}});
        FindPin(R,TEXT("ModName"))->DefaultTextValue=FText::FromString(TEXT("Dual Overclock Reward / 融合超频"));
        FindPin(R,TEXT("ModAuthor"))->DefaultTextValue=FText::FromString(TEXT("DOR")); FindPin(R,TEXT("ModVersion"))->DefaultTextValue=FText::FromString(DORResearchBuildTag);F.Compile();
    }
    {FDORSafeGraph F(Page,TEXT("GetPageInfo"));auto* Name=F.Get(TEXT("PageName"));auto* R=DORNamedResult(F,{{TEXT("PageName"),TextType}});Connect(Name,FindPin(R,TEXT("PageName")));F.Compile();}
    {FDORSafeGraph F(Page,TEXT("GetContainerMode"));auto* R=DORNamedResult(F,{{TEXT("bStretchToContainer"),MakeBoolPinType()}});FindPin(R,TEXT("bStretchToContainer"))->DefaultValue=TEXT("true");F.Compile();}
    {FDORSafeGraph F(Page,TEXT("DOR_ApplyPageMode"));auto* C=F.Call(UWidgetSwitcher::StaticClass(),TEXT("SetActiveWidgetIndex"));Connect(F.Get(TEXT("Sections")),FindPin(C,UEdGraphSchema_K2::PN_Self));Connect(F.Get(TEXT("PageMode")),FindPin(C,TEXT("Index")));F.Return(nullptr,TEXT("true"));F.Compile();}
    {
        FDORSafeGraph F(MenuHost,TEXT("GetModPages"));
        auto EmptyPages=[&](){auto* Empty=AddNode<UK2Node_MakeArray>(F.G,0,0);Empty->NumInputs=0;Empty->AllocateDefaultPins();ForcePinType(Empty->GetOutputPin(),DORInterfaceType(PageI->GeneratedClass,true));auto* R=DORNamedResult(F,{{TEXT("HubPages"),DORInterfaceType(PageI->GeneratedClass,true)}});Connect(Empty->GetOutputPin(),FindPin(R,TEXT("HubPages")));};
        // ModHub can enumerate page providers while a remote controller is being
        // admitted. GetPlayerController(0) is not guaranteed to be local during
        // that window; passing it to CreateWidget emits a fatal PIE/runtime error
        // and can tear down the pending Steam connection. Return no page until a
        // valid local player controller exists. ModHub will query again locally.
        auto* PC=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerController"));auto* ValidPC=F.Branch(F.IsValid(PC->GetReturnValuePin()));auto* ValidTail=F.Tail;F.Tail=ValidPC->GetElsePin();EmptyPages();F.Tail=ValidTail;
        auto* LocalPC=F.Call(AController::StaticClass(),TEXT("IsLocalPlayerController"));Connect(PC->GetReturnValuePin(),FindPin(LocalPC,UEdGraphSchema_K2::PN_Self));auto* LocalBranch=F.Branch(LocalPC->GetReturnValuePin());auto* LocalTail=F.Tail;F.Tail=LocalBranch->GetElsePin();EmptyPages();F.Tail=LocalTail;
        auto* Hosts=F.Call(UGameplayStatics::StaticClass(),TEXT("GetAllActorsOfClass"));Connect(SelfPin(F.G,0,0),FindPin(Hosts,TEXT("WorldContextObject")));FindPin(Hosts,TEXT("ActorClass"))->DefaultObject=MenuHost->GeneratedClass;
        auto* FirstHost=F.Array(TEXT("Array_Get"),FindPin(Hosts,TEXT("OutActors")));FindPin(FirstHost,TEXT("Index"))->DefaultValue=TEXT("0");
        auto* FirstOnly=F.Branch(F.Equal(SelfPin(F.G,0,0),FindPin(FirstHost,TEXT("Item")),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));auto* FirstOnlyTail=F.Tail;F.Tail=FirstOnly->GetElsePin();EmptyPages();F.Tail=FirstOnlyTail;
        const TCHAR* CacheNames[]={TEXT("Page"),TEXT("DebugPage"),TEXT("MaintenancePage"),TEXT("AdvancedPage")};
        const TCHAR* PageNames[]={TEXT("Settings"),TEXT("Debug"),TEXT("Maintenance"),TEXT("Advanced")};
        for(int32 Mode=0;Mode<4;++Mode)
        {
            auto* B=F.Branch(F.IsValid(F.Get(CacheNames[Mode])));auto* Ready=F.Tail;F.Tail=B->GetElsePin();auto* Create=F.Call(UWidgetBlueprintLibrary::StaticClass(),TEXT("Create"));
            FindPin(Create,TEXT("WidgetType"))->DefaultObject=Page->GeneratedClass;Connect(PC->GetReturnValuePin(),FindPin(Create,TEXT("OwningPlayer")));
            F.Set(CacheNames[Mode],DORCastPin(F,Create->GetReturnValuePin(),Page->GeneratedClass));
            auto* SetHost=AddExternalVariableSet(F.G,TEXT("Host"),Page->GeneratedClass,0,0);Connect(F.Get(CacheNames[Mode]),FindPin(SetHost,UEdGraphSchema_K2::PN_Self));
            Connect(SelfPin(F.G,0,0),FindPin(SetHost,TEXT("Host")));F.Run(SetHost);Connect(Ready,SetHost->GetExecPin());
            auto* SetMode=AddExternalVariableSet(F.G,TEXT("PageMode"),Page->GeneratedClass,0,0);Connect(F.Get(CacheNames[Mode]),FindPin(SetMode,UEdGraphSchema_K2::PN_Self));FindPin(SetMode,TEXT("PageMode"))->DefaultValue=FString::FromInt(Mode);F.Run(SetMode);
            auto* SetName=AddExternalVariableSet(F.G,TEXT("PageName"),Page->GeneratedClass,0,0);Connect(F.Get(CacheNames[Mode]),FindPin(SetName,UEdGraphSchema_K2::PN_Self));FindPin(SetName,TEXT("PageName"))->DefaultTextValue=FText::FromString(PageNames[Mode]);F.Run(SetName);
        }
        auto* Arr=AddNode<UK2Node_MakeArray>(F.G,0,0);Arr->NumInputs=4;Arr->AllocateDefaultPins();
        ForcePinType(Arr->GetOutputPin(),DORInterfaceType(PageI->GeneratedClass,true));
        for(int32 Mode=0;Mode<4;++Mode){auto* Input=FindPin(Arr,FName(*FString::Printf(TEXT("[%d]"),Mode)));ForcePinType(Input,DORInterfaceType(PageI->GeneratedClass));Connect(DORCastPin(F,F.Get(CacheNames[Mode]),PageI->GeneratedClass),Input);}
        auto* R=DORNamedResult(F,{{TEXT("HubPages"),DORInterfaceType(PageI->GeneratedClass,true)}});Connect(Arr->GetOutputPin(),FindPin(R,TEXT("HubPages")));F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_WarnLegacyManual"));
        F.Require(F.Not(F.Get(TEXT("LegacySettingsWarningShown"))));
        F.Require(F.IsValid(F.Get(TEXT("Settings"))));
        auto Or=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();};
        auto* LegacyKeysLength=F.Array(TEXT("Array_Length"),F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,TEXT("Keys")));
        auto* LegacyPairsLength=F.Array(TEXT("Array_Length"),F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,TEXT("Pairs")));
        auto* LegacyKeysPresent=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(LegacyKeysLength->GetReturnValuePin(),FindPin(LegacyKeysPresent,TEXT("A")));
        auto* LegacyPairsPresent=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(LegacyPairsLength->GetReturnValuePin(),FindPin(LegacyPairsPresent,TEXT("A")));
        UEdGraphPin* HasLegacy=Or(LegacyKeysPresent->GetReturnValuePin(),LegacyPairsPresent->GetReturnValuePin());
        for(int32 SlotIndex=0;SlotIndex<4;++SlotIndex)
        {
            const FName KeyName(*FString::Printf(TEXT("Key%d"),SlotIndex));
            auto* NonEmpty=F.Not(F.Equal(F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName),DORSafeString(F,TEXT("")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass()));
            HasLegacy=Or(HasLegacy,NonEmpty);
        }
        F.Require(HasLegacy);
        auto* Print=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("PrintString"));
        FindPin(Print,TEXT("InString"))->DefaultValue=TEXT("[DOR-MENU][A75] legacy-name-settings-preserved=true auto-apply=false action=reselect-player-setting");
        FindPin(Print,TEXT("bPrintToScreen"))->DefaultValue=TEXT("true");FindPin(Print,TEXT("bPrintToLog"))->DefaultValue=TEXT("true");FindPin(Print,TEXT("Duration"))->DefaultValue=TEXT("12");
        F.Set(TEXT("LegacySettingsWarningShown"),nullptr,TEXT("true"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_LoadManual"));auto* S=F.Self(TEXT("DOR_GetSafety"));auto* Init=F.Call(Safety->GeneratedClass,TEXT("DOR_Initialize"));
        Connect(S->GetReturnValuePin(),FindPin(Init,UEdGraphSchema_K2::PN_Self));F.Require(Init->GetReturnValuePin());
        auto* Slot=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Replace"));Connect(F.Field(S->GetReturnValuePin(),Safety->GeneratedClass,TEXT("DOR_BackupSlot")),FindPin(Slot,TEXT("SourceString")));
        FindPin(Slot,TEXT("From"))->DefaultValue=TEXT("Mods/DOR_FirstInstallBackup_");FindPin(Slot,TEXT("To"))->DefaultValue=TEXT("Mods/DOR_Manual_");F.Set(TEXT("SettingsSlot"),Slot->GetReturnValuePin());
        // Always reload the manual-settings slot.  The legacy arrays are not
        // validated or read: Alpha68/69 demonstrated that their VM mutation could
        // leave a stale 385 behind while the UI reported success.  Missing scalar
        // fields in an older file deserialize to their empty/zero defaults.
        auto* Exists=F.Branch(DORSafeExists(F,F.Get(TEXT("SettingsSlot"))));
        F.Set(TEXT("Settings"),DORSafeLoad(F,F.Get(TEXT("SettingsSlot")),Settings->GeneratedClass));F.Require(F.IsValid(F.Get(TEXT("Settings"))));
        F.Self(TEXT("DOR_WarnLegacyManual"));F.Return(nullptr,TEXT("true"));F.Tail=Exists->GetElsePin();
        auto* Create=F.Call(UGameplayStatics::StaticClass(),TEXT("CreateSaveGameObject"));FindPin(Create,TEXT("SaveGameClass"))->DefaultObject=Settings->GeneratedClass;
        F.Set(TEXT("Settings"),DORCastPin(F,Create->GetReturnValuePin(),Settings->GeneratedClass));F.Return(F.IsValid(F.Get(TEXT("Settings"))));F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_IsCurrentPlayerKey"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());F.Require(DORPlayerKeyValid(F,Key),TEXT("[DOR]|event=MANUAL_KEY|result=blocked|stage=is-current-input-invalid"));auto* Loop=F.Each(DORRoster(F));auto* Player=FindPin(Loop,TEXT("Array Element"));F.Branch(F.IsValid(Player));
        auto Identity=DORPlayerIdentity(F,Player);F.Branch(Identity.Ready);auto* Match=F.Equal(Identity.Key,Key,TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());auto* Found=F.Branch(Match);F.Return(nullptr,TEXT("true"));F.Tail=FindPin(Loop,TEXT("Completed"));F.Return();F.Compile();
    }
    {
        // Fixed scalar slots deliberately avoid every array getter/mutator. Only
        // clearing both slots removes the player key. A slot-specific consume
        // therefore cannot erase the other reservation.
        FDORSafeGraph F(MenuHost,TEXT("DOR_WriteManualFlat"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* PrimaryPair=F.Input(TEXT("PrimaryPair"),MakeIntPinType());auto* SecondaryPair=F.Input(TEXT("SecondaryPair"),MakeIntPinType());F.Require(DORPlayerKeyValid(F,Key),TEXT("[DOR]|event=MANUAL_KEY|result=blocked|stage=write-input-invalid"));
        auto EqualZero=[&](UEdGraphPin* Value){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));Connect(Value,FindPin(N,TEXT("A")));FindPin(N,TEXT("B"))->DefaultValue=TEXT("0");return N->GetReturnValuePin();};
        auto* BothZero=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));Connect(EqualZero(PrimaryPair),FindPin(BothZero,TEXT("A")));Connect(EqualZero(SecondaryPair),FindPin(BothZero,TEXT("B")));auto* ClearMode=F.Branch(BothZero->GetReturnValuePin());
        for(int32 SlotIndex=0;SlotIndex<4;++SlotIndex)
        {
            const FName KeyName(*FString::Printf(TEXT("IdKey%d"),SlotIndex));
            const FName PairName(*FString::Printf(TEXT("IdPair%d"),SlotIndex));
            const FName PrimaryName(*FString::Printf(TEXT("IdPrimaryPair%d"),SlotIndex));
            const FName SecondaryName(*FString::Printf(TEXT("IdSecondaryPair%d"),SlotIndex));
            auto* Match=F.Equal(F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName),Key,TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
            auto* Found=F.Branch(Match);F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,PairName,nullptr,TEXT("0"));F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,PrimaryName,nullptr,TEXT("0"));F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,SecondaryName,nullptr,TEXT("0"));F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName,DORSafeString(F,TEXT("")));F.Return(nullptr,TEXT("true"));F.Tail=Found->GetElsePin();
        }
        F.Return(nullptr,TEXT("true"));F.Tail=ClearMode->GetElsePin();
        for(int32 SlotIndex=0;SlotIndex<4;++SlotIndex)
        {
            const FName KeyName(*FString::Printf(TEXT("IdKey%d"),SlotIndex));
            const FName PairName(*FString::Printf(TEXT("IdPair%d"),SlotIndex));
            const FName PrimaryName(*FString::Printf(TEXT("IdPrimaryPair%d"),SlotIndex));
            const FName SecondaryName(*FString::Printf(TEXT("IdSecondaryPair%d"),SlotIndex));
            auto* Existing=F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName);
            auto* Match=F.Equal(Existing,Key,TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
            auto* Empty=F.Equal(Existing,DORSafeString(F,TEXT("")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
            auto* ExistingCurrent=F.Self(TEXT("DOR_IsCurrentPlayerKey"));Connect(Existing,FindPin(ExistingCurrent,TEXT("Key")));
            auto* MatchOrEmpty=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(Match,FindPin(MatchOrEmpty,TEXT("A")));Connect(Empty,FindPin(MatchOrEmpty,TEXT("B")));
            auto* Available=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(MatchOrEmpty->GetReturnValuePin(),FindPin(Available,TEXT("A")));Connect(F.Not(ExistingCurrent->GetReturnValuePin()),FindPin(Available,TEXT("B")));
            auto* Use=F.Branch(Available->GetReturnValuePin());F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName,Key);F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,PairName,nullptr,TEXT("0"));F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,PrimaryName,PrimaryPair);F.Put(F.Get(TEXT("Settings")),Settings->GeneratedClass,SecondaryName,SecondaryPair);F.Return(nullptr,TEXT("true"));F.Tail=Use->GetElsePin();
        }
        F.Return();F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_ReadManualPrimary"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());
        auto* ValidKey=F.Branch(DORPlayerKeyValid(F,Key));auto* ValidTail=F.Tail;F.Tail=ValidKey->GetElsePin();
        F.Print(TEXT("[DOR]|event=MANUAL_KEY|result=blocked|stage=read-input-invalid"));
        auto* InvalidZero=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));FindPin(InvalidZero,TEXT("A"))->DefaultValue=TEXT("0");FindPin(InvalidZero,TEXT("B"))->DefaultValue=TEXT("0");F.Return(InvalidZero->GetReturnValuePin());F.Tail=ValidTail;
        for(int32 SlotIndex=0;SlotIndex<4;++SlotIndex)
        {
            const FName KeyName(*FString::Printf(TEXT("IdKey%d"),SlotIndex));
            const FName PairName(*FString::Printf(TEXT("IdPair%d"),SlotIndex));const FName PrimaryName(*FString::Printf(TEXT("IdPrimaryPair%d"),SlotIndex));
            auto* Match=F.Equal(F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName),Key,TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
            auto* Found=F.Branch(Match);auto* Current=F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,PrimaryName);auto* Positive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Current,FindPin(Positive,TEXT("A")));auto* CurrentBranch=F.Branch(Positive->GetReturnValuePin());F.Return(Current);F.Tail=CurrentBranch->GetElsePin();
            auto* Legacy=F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,PairName);auto EqualLiteral=[&](int32 Value){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));Connect(Legacy,FindPin(N,TEXT("A")));FindPin(N,TEXT("B"))->DefaultValue=FString::FromInt(Value);return N->GetReturnValuePin();};auto* Known=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(EqualLiteral(385),FindPin(Known,TEXT("A")));Connect(EqualLiteral(388),FindPin(Known,TEXT("B")));auto* Migrate=F.Branch(Known->GetReturnValuePin());F.Return(Legacy);F.Tail=Migrate->GetElsePin();F.Return(Current);F.Tail=Found->GetElsePin();
        }
        auto* Zero=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));FindPin(Zero,TEXT("A"))->DefaultValue=TEXT("0");FindPin(Zero,TEXT("B"))->DefaultValue=TEXT("0");F.Return(Zero->GetReturnValuePin());F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_ReadManualSecondary"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());
        auto* ValidKey=F.Branch(DORPlayerKeyValid(F,Key));auto* ValidTail=F.Tail;F.Tail=ValidKey->GetElsePin();F.Print(TEXT("[DOR]|event=MANUAL_KEY|result=blocked|stage=read-secondary-input-invalid"));auto* InvalidZero=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));FindPin(InvalidZero,TEXT("A"))->DefaultValue=TEXT("0");FindPin(InvalidZero,TEXT("B"))->DefaultValue=TEXT("0");F.Return(InvalidZero->GetReturnValuePin());F.Tail=ValidTail;
        for(int32 SlotIndex=0;SlotIndex<4;++SlotIndex)
        {
            const FName KeyName(*FString::Printf(TEXT("IdKey%d"),SlotIndex));const FName PairName(*FString::Printf(TEXT("IdPair%d"),SlotIndex));const FName SecondaryName(*FString::Printf(TEXT("IdSecondaryPair%d"),SlotIndex));
            auto* Match=F.Equal(F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName),Key,TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());auto* Found=F.Branch(Match);auto* Current=F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,SecondaryName);auto* Positive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Current,FindPin(Positive,TEXT("A")));auto* CurrentBranch=F.Branch(Positive->GetReturnValuePin());F.Return(Current);F.Tail=CurrentBranch->GetElsePin();auto* Legacy=F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,PairName);auto* Is277=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));Connect(Legacy,FindPin(Is277,TEXT("A")));FindPin(Is277,TEXT("B"))->DefaultValue=TEXT("277");auto* Known=F.Branch(Is277->GetReturnValuePin());F.Return(Legacy);F.Tail=Known->GetElsePin();F.Return(Current);F.Tail=Found->GetElsePin();
        }
        auto* Zero=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));FindPin(Zero,TEXT("A"))->DefaultValue=TEXT("0");FindPin(Zero,TEXT("B"))->DefaultValue=TEXT("0");F.Return(Zero->GetReturnValuePin());F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_ReadPlayerManualPrimary"));auto* Player=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));auto Zero=[&](){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));FindPin(N,TEXT("A"))->DefaultValue=TEXT("0");FindPin(N,TEXT("B"))->DefaultValue=TEXT("0");return N->GetReturnValuePin();};auto* Valid=F.Branch(F.IsValid(Player));auto* ValidTail=F.Tail;F.Tail=Valid->GetElsePin();F.Return(Zero());F.Tail=ValidTail;auto Identity=DORPlayerIdentity(F,Player);auto* Ready=F.Branch(Identity.Ready);auto* ReadyTail=F.Tail;F.Tail=Ready->GetElsePin();F.Return(Zero());F.Tail=ReadyTail;auto* Read=F.Self(TEXT("DOR_ReadManualPrimary"));Connect(Identity.Key,FindPin(Read,TEXT("Key")));F.Return(Read->GetReturnValuePin());F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_ReadPlayerManualSecondary"));auto* Player=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));auto Zero=[&](){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));FindPin(N,TEXT("A"))->DefaultValue=TEXT("0");FindPin(N,TEXT("B"))->DefaultValue=TEXT("0");return N->GetReturnValuePin();};auto* Valid=F.Branch(F.IsValid(Player));auto* ValidTail=F.Tail;F.Tail=Valid->GetElsePin();F.Return(Zero());F.Tail=ValidTail;auto Identity=DORPlayerIdentity(F,Player);auto* Ready=F.Branch(Identity.Ready);auto* ReadyTail=F.Tail;F.Tail=Ready->GetElsePin();F.Return(Zero());F.Tail=ReadyTail;auto* Read=F.Self(TEXT("DOR_ReadManualSecondary"));Connect(Identity.Key,FindPin(Read,TEXT("Key")));F.Return(Read->GetReturnValuePin());F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_ReadPlayerDisplayedCode"));auto* Player=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));auto* Slot=F.Input(TEXT("Slot"),MakeIntPinType());auto Zero=[&](){auto* N=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralInt"));return N->GetReturnValuePin();};auto Need=[&](UEdGraphPin* Good){auto* B=F.Branch(Good);F.Tail=B->GetElsePin();F.Return(Zero());F.Tail=B->GetThenPin();};auto* Valid=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("IsValid"));Connect(Player,FindPin(Valid,TEXT("Object")));Need(Valid->GetReturnValuePin());auto* State=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(Player,FindPin(State,UEdGraphSchema_K2::PN_Self));auto* PS=DORCastPin(F,State->GetReturnValuePin(),AFSDPlayerState::StaticClass());Valid=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("IsValid"));Connect(PS,FindPin(Valid,TEXT("Object")));Need(Valid->GetReturnValuePin());auto* Rig=F.Branch(F.Field(PS,AFSDPlayerState::StaticClass(),TEXT("IsOnSpaceRig")));auto* Load=F.Self(TEXT("DOR_LoadManual"));Need(Load->GetReturnValuePin());auto* Secondary=F.Branch(F.Equal(Slot,Zero(),TEXT("NotEqual_IntInt"),UKismetMathLibrary::StaticClass()));auto* Read=F.Self(TEXT("DOR_ReadPlayerManualSecondary"));Connect(Player,FindPin(Read,TEXT("Player")));F.Return(Read->GetReturnValuePin());F.Tail=Secondary->GetElsePin();Read=F.Self(TEXT("DOR_ReadPlayerManualPrimary"));Connect(Player,FindPin(Read,TEXT("Player")));F.Return(Read->GetReturnValuePin());
        F.Tail=Rig->GetElsePin();auto* Find=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(Find,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;auto* M=DORCastPin(F,Find->GetReturnValuePin(),Master->GeneratedClass);Valid=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("IsValid"));Connect(M,FindPin(Valid,TEXT("Object")));Need(Valid->GetReturnValuePin());auto* Current=F.Call(Master->GeneratedClass,TEXT("DOR_NativeCurrentSelectionCode"));Connect(M,FindPin(Current,UEdGraphSchema_K2::PN_Self));Connect(Player,FindPin(Current,TEXT("Player")));Connect(Slot,FindPin(Current,TEXT("Slot")));F.Return(Current->GetReturnValuePin());F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_SaveManualFlat"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* PrimaryPair=F.Input(TEXT("PrimaryPair"),MakeIntPinType());auto* SecondaryPair=F.Input(TEXT("SecondaryPair"),MakeIntPinType());
        F.Require(DORPlayerKeyValid(F,Key),TEXT("[DOR]|event=MANUAL_KEY|result=blocked|stage=save-input-invalid"));F.Require(DORLocalHost(F));auto* Load=F.Self(TEXT("DOR_LoadManual"));F.Require(Load->GetReturnValuePin());
        auto* S=F.Self(TEXT("DOR_GetSafety"));auto* Allow=F.Call(Safety->GeneratedClass,TEXT("DOR_CanWrite"));Connect(S->GetReturnValuePin(),FindPin(Allow,UEdGraphSchema_K2::PN_Self));F.Require(Allow->GetReturnValuePin());
        auto* StableKey=F.Cache(TEXT("Key"),Key);auto* Write=F.Self(TEXT("DOR_WriteManualFlat"));Connect(StableKey,FindPin(Write,TEXT("Key")));Connect(PrimaryPair,FindPin(Write,TEXT("PrimaryPair")));Connect(SecondaryPair,FindPin(Write,TEXT("SecondaryPair")));F.Require(Write->GetReturnValuePin());
        F.Require(DORSafeSave(F,F.Get(TEXT("Settings")),F.Get(TEXT("SettingsSlot"))));auto* Read=DORSafeLoad(F,F.Get(TEXT("SettingsSlot")),Settings->GeneratedClass);F.Require(F.IsValid(Read));F.Set(TEXT("Settings"),Read);
        auto* ReadPrimaryCall=F.Self(TEXT("DOR_ReadManualPrimary"));Connect(StableKey,FindPin(ReadPrimaryCall,TEXT("Key")));auto* ReadPrimary=F.Cache(TEXT("ReadPrimary"),ReadPrimaryCall->GetReturnValuePin());auto* ReadSecondaryCall=F.Self(TEXT("DOR_ReadManualSecondary"));Connect(StableKey,FindPin(ReadSecondaryCall,TEXT("Key")));auto* ReadSecondary=F.Cache(TEXT("ReadSecondary"),ReadSecondaryCall->GetReturnValuePin());
        auto ToText=[&](UEdGraphPin* Value){auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(Value,FindPin(N,TEXT("InInt")));return N->GetReturnValuePin();};auto* RequestedMessage=DORSafeConcat(F,TEXT("[DOR-MENU][A76] dual-slot-disk-save requested-primary="),ToText(PrimaryPair));auto* RequestedSecondary=DORSafeConcat(F,TEXT(" secondary="),ToText(SecondaryPair));auto* Requested=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(RequestedMessage,FindPin(Requested,TEXT("A")));Connect(RequestedSecondary,FindPin(Requested,TEXT("B")));auto* ReadMessage=DORSafeConcat(F,TEXT(" readback-primary="),ToText(ReadPrimary));auto* ReadSecondaryMessage=DORSafeConcat(F,TEXT(" secondary="),ToText(ReadSecondary));auto* ReadCombined=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(ReadMessage,FindPin(ReadCombined,TEXT("A")));Connect(ReadSecondaryMessage,FindPin(ReadCombined,TEXT("B")));auto* SaveMessage=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(Requested->GetReturnValuePin(),FindPin(SaveMessage,TEXT("A")));Connect(ReadCombined->GetReturnValuePin(),FindPin(SaveMessage,TEXT("B")));
        auto* Print=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("PrintString"));Connect(SaveMessage->GetReturnValuePin(),FindPin(Print,TEXT("InString")));FindPin(Print,TEXT("bPrintToScreen"))->DefaultValue=TEXT("true");FindPin(Print,TEXT("bPrintToLog"))->DefaultValue=TEXT("true");FindPin(Print,TEXT("Duration"))->DefaultValue=TEXT("12");
        F.Require(F.Equal(ReadPrimary,PrimaryPair,TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));F.Require(F.Equal(ReadSecondary,SecondaryPair,TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));F.Set(TEXT("MissionSettingsReady"),nullptr,TEXT("false"));
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_ManualRevision"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* Slot=F.Input(TEXT("SlotName"),MakeStringPinType());
        auto* Token=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(DORSafeConcat(F,TEXT(""),Key),FindPin(Token,TEXT("A")));Connect(DORSafeConcat(F,TEXT("|"),Slot),FindPin(Token,TEXT("B")));
        auto* Find=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"));ForcePinType(FindPin(Find,TEXT("TargetMap")),ManualRevisions);ForcePinType(FindPin(Find,TEXT("Key")),MakeStringPinType());ForcePinType(FindPin(Find,TEXT("Value")),MakeIntPinType());Connect(F.Get(TEXT("ManualRevisions")),FindPin(Find,TEXT("TargetMap")));Connect(Token->GetReturnValuePin(),FindPin(Find,TEXT("Key")));F.Return(FindPin(Find,TEXT("Value")));F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_ManualRevisionMatches"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* Slot=F.Input(TEXT("SlotName"),MakeStringPinType());auto* Expected=F.Input(TEXT("ExpectedRevision"),MakeIntPinType());auto* Read=F.Self(TEXT("DOR_ManualRevision"));Connect(Key,FindPin(Read,TEXT("Key")));Connect(Slot,FindPin(Read,TEXT("SlotName")));F.Return(F.Equal(Read->GetReturnValuePin(),Expected,TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));F.Compile();
    }
    {
        // Only an explicit, successfully persisted user save calls this helper.
        FDORSafeGraph F(MenuHost,TEXT("DOR_RearmManual"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());
        for(const TCHAR* Slot:{TEXT("primary"),TEXT("secondary")})
        {
            auto* Join=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(Key,FindPin(Join,TEXT("A")));FindPin(Join,TEXT("B"))->DefaultValue=FString(TEXT("|"))+Slot;auto* Token=F.Cache(Slot,Join->GetReturnValuePin());
            auto* Read=F.Self(TEXT("DOR_ManualRevision"));Connect(Key,FindPin(Read,TEXT("Key")));FindPin(Read,TEXT("SlotName"))->DefaultValue=Slot;auto* Next=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(Read->GetReturnValuePin(),FindPin(Next,TEXT("A")));FindPin(Next,TEXT("B"))->DefaultValue=TEXT("1");
            auto* Store=F.Call(UBlueprintMapLibrary::StaticClass(),TEXT("Map_Add"));ForcePinType(FindPin(Store,TEXT("TargetMap")),ManualRevisions);ForcePinType(FindPin(Store,TEXT("Key")),MakeStringPinType());ForcePinType(FindPin(Store,TEXT("Value")),MakeIntPinType());Connect(F.Get(TEXT("ManualRevisions")),FindPin(Store,TEXT("TargetMap")));Connect(Token,FindPin(Store,TEXT("Key")));Connect(Next->GetReturnValuePin(),FindPin(Store,TEXT("Value")));
            for(const TCHAR* List:{TEXT("AttemptedPlayers"),TEXT("ConsumedPlayers")}){auto* Remove=F.Array(TEXT("Array_RemoveItem"),F.Get(List));Connect(Token,FindPin(Remove,TEXT("Item")));}
        }
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSafeGraph F(MenuHost,TEXT("DOR_SetManual"));auto* Player=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));auto* PrimaryPair=F.Input(TEXT("PrimaryPair"),MakeIntPinType());auto* SecondaryPair=F.Input(TEXT("SecondaryPair"),MakeIntPinType());
        F.Require(F.IsValid(Player));auto Identity=DORPlayerIdentity(F,Player);F.Require(Identity.Ready,TEXT("[DOR]|event=MANUAL_KEY|result=deferred|stage=set-identity-not-ready"));auto* Key=F.Cache(TEXT("StableKey"),Identity.Key);auto* Save=F.Self(TEXT("DOR_SaveManualFlat"));Connect(Key,FindPin(Save,TEXT("Key")));Connect(PrimaryPair,FindPin(Save,TEXT("PrimaryPair")));Connect(SecondaryPair,FindPin(Save,TEXT("SecondaryPair")));F.Require(Save->GetReturnValuePin());auto* Rearm=F.Self(TEXT("DOR_RearmManual"));Connect(Key,FindPin(Rearm,TEXT("Key")));
        auto* FindMaster=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(FindMaster,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;auto* M=DORCastPin(F,FindMaster->GetReturnValuePin(),Master->GeneratedClass);auto* HaveMaster=F.Branch(F.IsValid(M));
        for(const auto& Slot:{TPair<const TCHAR*,UEdGraphPin*>(TEXT("primary"),PrimaryPair),TPair<const TCHAR*,UEdGraphPin*>(TEXT("secondary"),SecondaryPair)})
        {
            auto* Read=F.Self(TEXT("DOR_ManualRevision"));Connect(Key,FindPin(Read,TEXT("Key")));FindPin(Read,TEXT("SlotName"))->DefaultValue=Slot.Key;auto ToText=[&](UEdGraphPin* V){auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(V,FindPin(N,TEXT("InInt")));return N->GetReturnValuePin();};auto* Line=DORSafeConcat(F,TEXT("[DOR-MANUAL]|stage=explicit-save|result=saved-rearmed|key="),Key);auto Append=[&](const TCHAR* Label,UEdGraphPin* V){auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(Line,FindPin(N,TEXT("A")));Connect(DORSafeConcat(F,Label,V),FindPin(N,TEXT("B")));Line=N->GetReturnValuePin();};Append(TEXT("|slot="),DORSafeString(F,Slot.Key));Append(TEXT("|code="),ToText(Slot.Value));Append(TEXT("|revision="),ToText(Read->GetReturnValuePin()));auto* Record=F.Call(Master->GeneratedClass,TEXT("DOR_DiagnosticRecordOnly"));Connect(M,FindPin(Record,UEdGraphSchema_K2::PN_Self));Connect(Line,FindPin(Record,TEXT("Message")));
        }
        F.Return(nullptr,TEXT("true"));F.Tail=HaveMaster->GetElsePin();F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        // Do not erase a newer menu choice made after this mission dispatched.
        FDORSafeGraph F(MenuHost,TEXT("DOR_ConsumeManualIfUnchanged"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* SlotName=F.Input(TEXT("SlotName"),MakeStringPinType());auto* Expected=F.Input(TEXT("Expected"),MakeIntPinType());F.Require(DORPlayerKeyValid(F,Key),TEXT("[DOR]|event=MANUAL_KEY|result=blocked|stage=consume-input-invalid"));
        auto* Load=F.Self(TEXT("DOR_LoadManual"));F.Require(Load->GetReturnValuePin());auto* PrimaryCall=F.Self(TEXT("DOR_ReadManualPrimary"));Connect(Key,FindPin(PrimaryCall,TEXT("Key")));auto* Primary=F.Cache(TEXT("Primary"),PrimaryCall->GetReturnValuePin());auto* SecondaryCall=F.Self(TEXT("DOR_ReadManualSecondary"));Connect(Key,FindPin(SecondaryCall,TEXT("Key")));auto* Secondary=F.Cache(TEXT("Secondary"),SecondaryCall->GetReturnValuePin());auto* IsPrimary=F.Equal(SlotName,DORSafeString(F,TEXT("primary")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());auto* IsSecondary=F.Equal(SlotName,DORSafeString(F,TEXT("secondary")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());auto* ValidSlot=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(IsPrimary,FindPin(ValidSlot,TEXT("A")));Connect(IsSecondary,FindPin(ValidSlot,TEXT("B")));F.Require(ValidSlot->GetReturnValuePin());auto* Current=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Connect(Primary,FindPin(Current,TEXT("A")));Connect(Secondary,FindPin(Current,TEXT("B")));Connect(IsPrimary,FindPin(Current,TEXT("bPickA")));auto* Same=F.Branch(F.Equal(Current->GetReturnValuePin(),Expected,TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));auto* NextPrimary=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));FindPin(NextPrimary,TEXT("A"))->DefaultValue=TEXT("0");Connect(Primary,FindPin(NextPrimary,TEXT("B")));Connect(IsPrimary,FindPin(NextPrimary,TEXT("bPickA")));auto* NextSecondary=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Connect(Secondary,FindPin(NextSecondary,TEXT("A")));FindPin(NextSecondary,TEXT("B"))->DefaultValue=TEXT("0");Connect(IsPrimary,FindPin(NextSecondary,TEXT("bPickA")));auto* Clear=F.Self(TEXT("DOR_SaveManualFlat"));Connect(Key,FindPin(Clear,TEXT("Key")));Connect(NextPrimary->GetReturnValuePin(),FindPin(Clear,TEXT("PrimaryPair")));Connect(NextSecondary->GetReturnValuePin(),FindPin(Clear,TEXT("SecondaryPair")));F.Return(Clear->GetReturnValuePin());F.Tail=Same->GetElsePin();F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        // Existing disk write/readback validation is shared with normal menu saves.
        FDORSafeGraph F(MenuHost,TEXT("DOR_ClearRigSlot"));auto* Key=F.Input(TEXT("Key"),MakeStringPinType());auto* SlotIndex=F.Input(TEXT("SlotIndex"),MakeIntPinType());
        auto* Valid=F.Branch(DORPlayerKeyValid(F,Key));auto* ValidTail=F.Tail;F.Tail=Valid->GetElsePin();F.Return(nullptr,TEXT("true"));F.Tail=ValidTail;
        auto* Primary=F.Self(TEXT("DOR_ReadManualPrimary"));Connect(Key,FindPin(Primary,TEXT("Key")));
        auto* Secondary=F.Self(TEXT("DOR_ReadManualSecondary"));Connect(Key,FindPin(Secondary,TEXT("Key")));
        auto* PrimaryPositive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Primary->GetReturnValuePin(),FindPin(PrimaryPositive,TEXT("A")));
        auto* SecondaryPositive=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Secondary->GetReturnValuePin(),FindPin(SecondaryPositive,TEXT("A")));
        auto* Either=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(PrimaryPositive->GetReturnValuePin(),FindPin(Either,TEXT("A")));Connect(SecondaryPositive->GetReturnValuePin(),FindPin(Either,TEXT("B")));
        auto* HasPair=F.Branch(Either->GetReturnValuePin());auto* PairTail=F.Tail;F.Tail=HasPair->GetElsePin();F.Return(nullptr,TEXT("true"));F.Tail=PairTail;
        auto* Clear=F.Self(TEXT("DOR_SaveManualFlat"));Connect(Key,FindPin(Clear,TEXT("Key")));FindPin(Clear,TEXT("PrimaryPair"))->DefaultValue=TEXT("0");FindPin(Clear,TEXT("SecondaryPair"))->DefaultValue=TEXT("0");
        auto* SlotText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(SlotIndex,FindPin(SlotText,TEXT("InInt")));
        auto Log=[&](const TCHAR* Result){auto* Prefix=DORSafeConcat(F,TEXT("[DOR-MENU][A88] rig-entry-clear slot="),SlotText->GetReturnValuePin());auto* Message=DORSafeConcat(F,TEXT(" result="),DORSafeString(F,Result));auto* Combined=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(Prefix,FindPin(Combined,TEXT("A")));Connect(Message,FindPin(Combined,TEXT("B")));auto* Print=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("PrintString"));Connect(Combined->GetReturnValuePin(),FindPin(Print,TEXT("InString")));FindPin(Print,TEXT("bPrintToScreen"))->DefaultValue=TEXT("false");FindPin(Print,TEXT("bPrintToLog"))->DefaultValue=TEXT("true");};
        auto* Saved=F.Branch(Clear->GetReturnValuePin());Log(TEXT("cleared"));F.Return(nullptr,TEXT("true"));F.Tail=Saved->GetElsePin();Log(TEXT("failed stage=save-or-readback"));F.Return();F.Compile();
    }
    {
        // The Rig actor tries once before its page is exposed. A later choice in
        // this same world cannot be erased by the repeating mission timer.
        FDORSafeGraph F(MenuHost,TEXT("DOR_ClearOnRigEntry"));auto* Attempted=F.Branch(F.Get(TEXT("RigEntryClearAttempted")));F.Return(nullptr,TEXT("true"));F.Tail=Attempted->GetElsePin();
        auto* Level=F.Call(UGameplayStatics::StaticClass(),TEXT("GetCurrentLevelName"));auto* Rig=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Contains"));Connect(Level->GetReturnValuePin(),FindPin(Rig,TEXT("SearchIn")));FindPin(Rig,TEXT("Substring"))->DefaultValue=TEXT("Spacerig");FindPin(Rig,TEXT("bUseCase"))->DefaultValue=TEXT("false");
        auto* IsRig=F.Branch(Rig->GetReturnValuePin());auto* RigTail=F.Tail;F.Tail=IsRig->GetElsePin();F.Return();F.Tail=RigTail;
        auto* PC=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerController"));F.Require(F.IsValid(PC->GetReturnValuePin()));
        auto* Local=F.Call(AController::StaticClass(),TEXT("IsLocalPlayerController"));Connect(PC->GetReturnValuePin(),FindPin(Local,UEdGraphSchema_K2::PN_Self));F.Require(Local->GetReturnValuePin());
        auto* Authority=F.Call(AActor::StaticClass(),TEXT("HasAuthority"));Connect(PC->GetReturnValuePin(),FindPin(Authority,UEdGraphSchema_K2::PN_Self));F.Require(Authority->GetReturnValuePin());
        auto* All=F.Call(UGameplayStatics::StaticClass(),TEXT("GetAllActorsOfClass"));Connect(SelfPin(F.G,0,0),FindPin(All,TEXT("WorldContextObject")));FindPin(All,TEXT("ActorClass"))->DefaultObject=MenuHost->GeneratedClass;
        auto* First=F.Array(TEXT("Array_Get"),FindPin(All,TEXT("OutActors")));FindPin(First,TEXT("Index"))->DefaultValue=TEXT("0");F.Require(F.IsValid(FindPin(First,TEXT("Item"))));
        auto* FirstHost=F.Branch(F.Equal(SelfPin(F.G,0,0),FindPin(First,TEXT("Item")),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));auto* FirstTail=F.Tail;F.Tail=FirstHost->GetElsePin();F.Set(TEXT("RigEntryClearAttempted"),nullptr,TEXT("true"));F.Return();F.Tail=FirstTail;
        F.Set(TEXT("RigEntryClearAttempted"),nullptr,TEXT("true"));auto* Load=F.Self(TEXT("DOR_LoadManual"));auto* Loaded=F.Branch(Load->GetReturnValuePin());auto* LoadedTail=F.Tail;F.Tail=Loaded->GetElsePin();F.Print(TEXT("[DOR-MENU][A88] rig-entry-clear result=failed stage=load"));F.Return();F.Tail=LoadedTail;
        for(int32 SlotIndex=0;SlotIndex<4;++SlotIndex)
        {
            const FName KeyName(*FString::Printf(TEXT("IdKey%d"),SlotIndex));auto* Key=F.Cache(TEXT("RigEntryKey"),F.Field(F.Get(TEXT("Settings")),Settings->GeneratedClass,KeyName));
            auto* Clear=F.Self(TEXT("DOR_ClearRigSlot"));Connect(Key,FindPin(Clear,TEXT("Key")));FindPin(Clear,TEXT("SlotIndex"))->DefaultValue=FString::FromInt(SlotIndex);
        }
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        // One verified disk snapshot per mission is enough.  DOR_LoadManual itself
        // deliberately bypasses actor-local cache so the first mission read cannot
        // inherit a stale value from a different ModHub page/instance.
        FDORSafeGraph F(MenuHost,TEXT("DOR_EnsureMissionSettings"));auto* Ready=F.Branch(F.Get(TEXT("MissionSettingsReady")));
        F.Return(nullptr,TEXT("true"));F.Tail=Ready->GetElsePin();auto* Load=F.Self(TEXT("DOR_LoadManual"));F.Require(Load->GetReturnValuePin());
        F.Set(TEXT("MissionSettingsReady"),nullptr,TEXT("true"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        // The local bridge resolves player zero again. Admit a primary reservation
        // only when that exact character owns a primary item.
        FDORSafeGraph F(MenuHost,TEXT("DOR_LocalPrimaryReady"));
        auto* Player=F.Input(TEXT("Player"),MakeObjectPinType(APlayerCharacter::StaticClass()));
        auto* Token=F.Input(TEXT("Token"),MakeStringPinType());
        auto* Pair=F.Input(TEXT("Pair"),MakeIntPinType());
        auto* MasterActor=F.Input(TEXT("MasterActor"),MakeObjectPinType(Master->GeneratedClass));
        auto* Character=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"));
        auto* Pawn=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerPawn"));
        auto* Matches=F.Equal(Player,Character->GetReturnValuePin(),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass());
        F.Set(TEXT("LocalPrimaryProbeInventory"),nullptr);
        F.Set(TEXT("LocalPrimaryProbeItem"),nullptr);
        auto* CharacterValid=F.Branch(F.IsValid(Character->GetReturnValuePin()));
        auto* Component=F.Call(AActor::StaticClass(),TEXT("GetComponentByClass"));
        Connect(Character->GetReturnValuePin(),FindPin(Component,UEdGraphSchema_K2::PN_Self));
        FindPin(Component,TEXT("ComponentClass"))->DefaultObject=InventoryComponentClass;
        auto* Inventory=DORCastPin(F,Component->GetReturnValuePin(),InventoryComponentClass);
        F.Set(TEXT("LocalPrimaryProbeInventory"),Inventory);
        auto* InventoryValid=F.Branch(F.IsValid(Inventory));
        auto* Item=F.Call(InventoryComponentClass,TEXT("GetItem"));
        Connect(Inventory,FindPin(Item,UEdGraphSchema_K2::PN_Self));
        ForcePinType(FindPin(Item,TEXT("Category")),MakeItemCategoryPinType());
        FindPin(Item,TEXT("Category"))->DefaultValue=UEnum::GetValueAsString(EItemCategory::PrimaryWeapon);
        F.Set(TEXT("LocalPrimaryProbeItem"),Item->GetReturnValuePin());
        auto* ItemValid=F.IsValid(F.Get(TEXT("LocalPrimaryProbeItem")));
        auto* Ready=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));
        Connect(Matches,FindPin(Ready,TEXT("A")));Connect(ItemValid,FindPin(Ready,TEXT("B")));
        auto Record=[&](const TCHAR* Stage,bool Flush){
            auto ToString=[&](UEdGraphPin* Value,const TCHAR* Function,const TCHAR* Pin){auto* N=F.Call(UKismetStringLibrary::StaticClass(),Function);Connect(Value,FindPin(N,Pin));return N->GetReturnValuePin();};
            auto Name=[&](UEdGraphPin* Value){auto* N=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetObjectName"));Connect(Value,FindPin(N,TEXT("Object")));return N->GetReturnValuePin();};
            auto Append=[&](UEdGraphPin* Base,const TCHAR* Key,UEdGraphPin* Value){auto* Part=DORSafeConcat(F,Key,Value);auto* Join=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(Base,FindPin(Join,TEXT("A")));Connect(Part,FindPin(Join,TEXT("B")));return Join->GetReturnValuePin();};
            UEdGraphPin* Message=DORSafeConcat(F,TEXT("[DOR-DIAG][NP87] stage="),DORSafeString(F,Stage));
            Message=Append(Message,TEXT("|pair="),ToString(Pair,TEXT("Conv_IntToString"),TEXT("InInt")));
            Message=Append(Message,TEXT("|tick="),ToString(F.Get(TEXT("MissionSettleTicks")),TEXT("Conv_IntToString"),TEXT("InInt")));
            Message=Append(Message,TEXT("|roster="),Name(Player));
            Message=Append(Message,TEXT("|localCharacter="),Name(Character->GetReturnValuePin()));
            Message=Append(Message,TEXT("|localPawn="),Name(Pawn->GetReturnValuePin()));
            Message=Append(Message,TEXT("|inventory="),Name(F.Get(TEXT("LocalPrimaryProbeInventory"))));
            Message=Append(Message,TEXT("|primaryItem="),Name(F.Get(TEXT("LocalPrimaryProbeItem"))));
            Message=Append(Message,TEXT("|playerMatches="),ToString(Matches,TEXT("Conv_BoolToString"),TEXT("InBool")));
            Message=Append(Message,TEXT("|itemValid="),ToString(ItemValid,TEXT("Conv_BoolToString"),TEXT("InBool")));
            auto* RecordCall=F.Call(Master->GeneratedClass,TEXT("DOR_DiagnosticRecordOnly"));Connect(MasterActor,FindPin(RecordCall,UEdGraphSchema_K2::PN_Self));Connect(Message,FindPin(RecordCall,TEXT("Message")));
            if(Flush){auto* FlushCall=F.Call(Master->GeneratedClass,TEXT("DOR_DiagnosticFlush"));Connect(MasterActor,FindPin(FlushCall,UEdGraphSchema_K2::PN_Self));}
        };
        auto* Admitted=F.Branch(Ready->GetReturnValuePin());
        Connect(CharacterValid->GetElsePin(),Admitted->GetExecPin());
        Connect(InventoryValid->GetElsePin(),Admitted->GetExecPin());
        Record(TEXT("ready"),true);F.Return(nullptr,TEXT("true"));
        F.Tail=Admitted->GetElsePin();auto* FirstTick=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));
        Connect(F.Get(TEXT("MissionSettleTicks")),FindPin(FirstTick,TEXT("A")));FindPin(FirstTick,TEXT("B"))->DefaultValue=TEXT("10");
        auto* First=F.Branch(FirstTick->GetReturnValuePin());Record(TEXT("wait-start"),false);F.Return(nullptr,TEXT("false"));
        F.Tail=First->GetElsePin();auto* Timeout=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));
        Connect(F.Get(TEXT("MissionSettleTicks")),FindPin(Timeout,TEXT("A")));FindPin(Timeout,TEXT("B"))->DefaultValue=TEXT("30");
        auto* Terminal=F.Branch(Timeout->GetReturnValuePin());
        auto* Mark=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("AttemptedPlayers")));Connect(Token,FindPin(Mark,TEXT("NewItem")));
        Record(TEXT("terminal-missing"),true);F.Return(nullptr,TEXT("false"));
        F.Tail=Terminal->GetElsePin();F.Return(nullptr,TEXT("false"));F.Compile();
    }
    {
        // The consume helper returns true when the saved value was changed by
        // the player after dispatch. Report the actual slot value separately.
        FDORSafeGraph F(MenuHost,TEXT("DOR_ManualConsumeReadback"));
        auto* Key=F.Input(TEXT("Key"),MakeStringPinType());
        auto* SlotName=F.Input(TEXT("SlotName"),MakeStringPinType());
        auto* Expected=F.Input(TEXT("Expected"),MakeIntPinType());
        auto* ConsumeSucceeded=F.Input(TEXT("ConsumeSucceeded"),MakeBoolPinType());
        auto* Consumed=F.Branch(ConsumeSucceeded);
        auto* AfterConsumed=F.Tail;F.Tail=Consumed->GetElsePin();F.Return(DORSafeString(F,TEXT("failed stage=consume")));F.Tail=AfterConsumed;
        auto* Load=F.Self(TEXT("DOR_LoadManual"));auto* Loaded=F.Branch(Load->GetReturnValuePin());
        auto* AfterLoaded=F.Tail;F.Tail=Loaded->GetElsePin();F.Return(DORSafeString(F,TEXT("failed stage=readback")));F.Tail=AfterLoaded;
        auto* IsPrimary=F.Equal(SlotName,DORSafeString(F,TEXT("primary")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
        auto* Primary=F.Self(TEXT("DOR_ReadManualPrimary"));Connect(Key,FindPin(Primary,TEXT("Key")));
        auto* Secondary=F.Self(TEXT("DOR_ReadManualSecondary"));Connect(Key,FindPin(Secondary,TEXT("Key")));
        auto* Select=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Connect(Primary->GetReturnValuePin(),FindPin(Select,TEXT("A")));Connect(Secondary->GetReturnValuePin(),FindPin(Select,TEXT("B")));Connect(IsPrimary,FindPin(Select,TEXT("bPickA")));
        auto* Actual=F.Cache(TEXT("Actual"),Select->GetReturnValuePin());
        auto* AsText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(Actual,FindPin(AsText,TEXT("InInt")));
        auto* Cleared=F.Branch(F.Equal(Actual,Expected,TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass()));
        auto* StillExpected=F.Tail;F.Tail=Cleared->GetElsePin();
        auto* Zero=F.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_IntInt"));Connect(Actual,FindPin(Zero,TEXT("A")));FindPin(Zero,TEXT("B"))->DefaultValue=TEXT("0");
        auto* IsZero=F.Branch(Zero->GetReturnValuePin());F.Return(DORSafeConcat(F,TEXT("cleared readback="),AsText->GetReturnValuePin()));
        F.Tail=IsZero->GetElsePin();F.Return(DORSafeConcat(F,TEXT("preserved-newer-choice readback="),AsText->GetReturnValuePin()));
        F.Tail=StillExpected;F.Return(DORSafeConcat(F,TEXT("unchanged readback="),AsText->GetReturnValuePin()));F.Compile();
    }
    {
        UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=pump-start"));
        FDORSafeGraph F(MenuHost,TEXT("DOR_PumpManual"));F.Require(DORLocalHost(F));
        auto* Level=F.Call(UGameplayStatics::StaticClass(),TEXT("GetCurrentLevelName"));auto* Rig=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Contains"));
        Connect(Level->GetReturnValuePin(),FindPin(Rig,TEXT("SearchIn")));FindPin(Rig,TEXT("Substring"))->DefaultValue=TEXT("Spacerig");FindPin(Rig,TEXT("bUseCase"))->DefaultValue=TEXT("false");
        auto* RigBranch=F.Branch(Rig->GetReturnValuePin());F.Self(TEXT("DOR_ClearOnRigEntry"));F.Set(TEXT("MissionSettleTicks"),nullptr,TEXT("0"));F.Set(TEXT("MissionSettingsReady"),nullptr,TEXT("false"));
        F.Array(TEXT("Array_Clear"),F.Get(TEXT("AttemptedPlayers")));F.Array(TEXT("Array_Clear"),F.Get(TEXT("ConsumedPlayers")));F.Set(TEXT("PendingKey"),nullptr,TEXT(""));F.Set(TEXT("PendingPlayer"),nullptr);F.Set(TEXT("PendingPair"),nullptr,TEXT("0"));F.Set(TEXT("PendingSlot"),nullptr,TEXT(""));F.Set(TEXT("PendingIsLocal"),nullptr,TEXT("false"));F.Set(TEXT("PendingRemoteBaseline"),nullptr,TEXT("0"));F.Set(TEXT("PendingSettleTicks"),nullptr,TEXT("0"));F.Return();F.Tail=RigBranch->GetElsePin();
        // Only the first live ModHub actor is allowed to pump.  This makes mission
        // auto-apply idempotent even if a travel/spawn race temporarily duplicates
        // the optional ModHub actor.
        auto* All=F.Call(UGameplayStatics::StaticClass(),TEXT("GetAllActorsOfClass"));Connect(SelfPin(F.G,0,0),FindPin(All,TEXT("WorldContextObject")));FindPin(All,TEXT("ActorClass"))->DefaultObject=MenuHost->GeneratedClass;
        auto* First=F.Array(TEXT("Array_Get"),FindPin(All,TEXT("OutActors")));FindPin(First,TEXT("Index"))->DefaultValue=TEXT("0");F.Require(F.IsValid(FindPin(First,TEXT("Item"))));
        F.Require(F.Equal(SelfPin(F.G,0,0),FindPin(First,TEXT("Item")),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()));
        // The ModHub actor appears before mission actors have finished settling. Alpha65
        // dispatched after its first one-second tick, so the deferred fusion continuation
        // could be lost when the early master/weapon instance was replaced. Wait ten ticks;
        // explicit chat commands remain immediate and bypass this admission delay.
        auto* NextTick=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(F.Get(TEXT("MissionSettleTicks")),FindPin(NextTick,TEXT("A")));FindPin(NextTick,TEXT("B"))->DefaultValue=TEXT("1");F.Set(TEXT("MissionSettleTicks"),NextTick->GetReturnValuePin());
        auto* Settled=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(F.Get(TEXT("MissionSettleTicks")),FindPin(Settled,TEXT("A")));FindPin(Settled,TEXT("B"))->DefaultValue=TEXT("10");F.Require(Settled->GetReturnValuePin());
        auto* Load=F.Self(TEXT("DOR_EnsureMissionSettings"));F.Require(Load->GetReturnValuePin());auto* S=F.Self(TEXT("DOR_GetSafety"));auto* Allow=F.Call(Safety->GeneratedClass,TEXT("DOR_CanWrite"));Connect(S->GetReturnValuePin(),FindPin(Allow,UEdGraphSchema_K2::PN_Self));
        auto* Find=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(Find,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;auto* M=F.Cache(TEXT("Master"),DORCastPin(F,Find->GetReturnValuePin(),Master->GeneratedClass));F.Require(F.IsValid(M));
        // Keep both pending slots queued throughout construction and recovery.
        auto* AuthorityHeld=F.Branch(F.Field(M,Master->GeneratedClass,TEXT("RemoteAuthorityDispatchLocked")));F.Return();F.Tail=AuthorityHeld->GetElsePin();
        // A one-shot is consumed only after the asynchronous fusion transaction
        // reports success. Local work settles through TransientApplySucceeded;
        // remote work requires a newer matching server/client result generation.
        auto* PendingEmpty=F.Equal(F.Get(TEXT("PendingKey")),DORSafeString(F,TEXT("")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());auto* Pending=F.Branch(F.Not(PendingEmpty));
        auto* NextPendingTick=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));Connect(F.Get(TEXT("PendingSettleTicks")),FindPin(NextPendingTick,TEXT("A")));FindPin(NextPendingTick,TEXT("B"))->DefaultValue=TEXT("1");F.Set(TEXT("PendingSettleTicks"),NextPendingTick->GetReturnValuePin());
        auto* StillActive=F.Branch(F.Field(M,Master->GeneratedClass,TEXT("ExternalPresetTransactionActive")));F.Return();F.Tail=StillActive->GetElsePin();
        auto And=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();};
        auto Or=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();};
        auto* RemoteTestMatches=F.Equal(F.Field(M,Master->GeneratedClass,TEXT("RemoteLastTestIndex")),F.Get(TEXT("PendingPair")),TEXT("EqualEqual_IntInt"),UKismetMathLibrary::StaticClass());
        auto* RemotePlayerMatches=F.Equal(F.Field(M,Master->GeneratedClass,TEXT("RemoteLastPlayerKey")),F.Get(TEXT("PendingKey")),TEXT("EqualEqual_StrStr"),UKismetStringLibrary::StaticClass());
        auto* RemoteGenerationNew=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(F.Field(M,Master->GeneratedClass,TEXT("RemoteLastGeneration")),FindPin(RemoteGenerationNew,TEXT("A")));Connect(F.Get(TEXT("PendingRemoteBaseline")),FindPin(RemoteGenerationNew,TEXT("B")));
        auto* RemoteKnown=And(RemotePlayerMatches,And(RemoteTestMatches,RemoteGenerationNew->GetReturnValuePin()));auto* NotLocal=F.Not(F.Get(TEXT("PendingIsLocal")));
        auto* LocalSuccess=And(F.Get(TEXT("PendingIsLocal")),F.Field(M,Master->GeneratedClass,TEXT("TransientApplySucceeded")));auto* RemoteSuccess=And(NotLocal,And(RemoteKnown,F.Field(M,Master->GeneratedClass,TEXT("RemoteLastSucceeded"))));
        auto* Success=F.Branch(Or(LocalSuccess,RemoteSuccess));
        UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=pump-correlation-ready"));
        // A consumed one-shot and a mission-local applied fusion are separate
        // records. Persist the latter only after the actual apply has reached a
        // successful terminal result, so reconnect can create a fresh proxy.
        auto* PersistApplied=F.Call(Master->GeneratedClass,TEXT("DOR_PersistReplayCode"));Connect(M,FindPin(PersistApplied,UEdGraphSchema_K2::PN_Self));Connect(F.Get(TEXT("PendingPlayer")),FindPin(PersistApplied,TEXT("Player")));Connect(F.Get(TEXT("PendingPair")),FindPin(PersistApplied,TEXT("ChoiceCode")));Connect(F.Get(TEXT("PendingSlot")),FindPin(PersistApplied,TEXT("SlotName")));
        auto* Consume=F.Self(TEXT("DOR_ConsumeManualIfUnchanged"));Connect(F.Get(TEXT("PendingKey")),FindPin(Consume,TEXT("Key")));Connect(F.Get(TEXT("PendingSlot")),FindPin(Consume,TEXT("SlotName")));Connect(F.Get(TEXT("PendingPair")),FindPin(Consume,TEXT("Expected")));
        auto* Readback=F.Self(TEXT("DOR_ManualConsumeReadback"));Connect(F.Get(TEXT("PendingKey")),FindPin(Readback,TEXT("Key")));Connect(F.Get(TEXT("PendingSlot")),FindPin(Readback,TEXT("SlotName")));Connect(F.Get(TEXT("PendingPair")),FindPin(Readback,TEXT("Expected")));Connect(Consume->GetReturnValuePin(),FindPin(Readback,TEXT("ConsumeSucceeded")));
        auto* PairText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(F.Get(TEXT("PendingPair")),FindPin(PairText,TEXT("InInt")));
        auto* Message=DORSafeConcat(F,TEXT("[DOR-MENU][A87] one-shot-after-success slot="),F.Get(TEXT("PendingSlot")));
        auto* PairTail=DORSafeConcat(F,TEXT(" pair="),PairText->GetReturnValuePin());
        auto* WithPair=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(Message,FindPin(WithPair,TEXT("A")));Connect(PairTail,FindPin(WithPair,TEXT("B")));
        auto* ResultTail=DORSafeConcat(F,TEXT(" result="),Readback->GetReturnValuePin());
        auto* Full=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(WithPair->GetReturnValuePin(),FindPin(Full,TEXT("A")));Connect(ResultTail,FindPin(Full,TEXT("B")));
        auto* ConsumeOutcomeNotify=F.Call(Master->GeneratedClass,TEXT("DOR_DebugNotify"));Connect(M,FindPin(ConsumeOutcomeNotify,UEdGraphSchema_K2::PN_Self));Connect(Full->GetReturnValuePin(),FindPin(ConsumeOutcomeNotify,TEXT("Message")));
        F.Require(Consume->GetReturnValuePin());
        auto* ConsumedTokenTail=DORSafeConcat(F,TEXT("|"),F.Get(TEXT("PendingSlot")));auto* ConsumedToken=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(F.Get(TEXT("PendingKey")),FindPin(ConsumedToken,TEXT("A")));Connect(ConsumedTokenTail,FindPin(ConsumedToken,TEXT("B")));auto* Claim=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("ConsumedPlayers")));Connect(ConsumedToken->GetReturnValuePin(),FindPin(Claim,TEXT("NewItem")));
        auto ClearPending=[&](){F.Set(TEXT("PendingKey"),nullptr,TEXT(""));F.Set(TEXT("PendingPlayer"),nullptr);F.Set(TEXT("PendingPair"),nullptr,TEXT("0"));F.Set(TEXT("PendingSlot"),nullptr,TEXT(""));F.Set(TEXT("PendingIsLocal"),nullptr,TEXT("false"));F.Set(TEXT("PendingRemoteBaseline"),nullptr,TEXT("0"));F.Set(TEXT("PendingSettleTicks"),nullptr,TEXT("0"));};
        ClearPending();F.Return(nullptr,TEXT("true"));F.Tail=Success->GetElsePin();
        UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=pump-success-ready"));
        auto* LocalWaitDone=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(F.Get(TEXT("PendingSettleTicks")),FindPin(LocalWaitDone,TEXT("A")));FindPin(LocalWaitDone,TEXT("B"))->DefaultValue=TEXT("3");
        auto* RemoteTimedOut=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(F.Get(TEXT("PendingSettleTicks")),FindPin(RemoteTimedOut,TEXT("A")));FindPin(RemoteTimedOut,TEXT("B"))->DefaultValue=TEXT("20");
        auto* Resolved=F.Branch(Or(And(F.Get(TEXT("PendingIsLocal")),LocalWaitDone->GetReturnValuePin()),And(NotLocal,Or(RemoteKnown,RemoteTimedOut->GetReturnValuePin()))));
        auto* RetainedPairText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(F.Get(TEXT("PendingPair")),FindPin(RetainedPairText,TEXT("InInt")));auto* RetainedMessage=DORSafeConcat(F,TEXT("[DOR-MENU][A76] apply-not-confirmed; setting-retained slot="),F.Get(TEXT("PendingSlot")));auto* RetainedPairTail=DORSafeConcat(F,TEXT(" pair="),RetainedPairText->GetReturnValuePin());auto* RetainedFull=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(RetainedMessage,FindPin(RetainedFull,TEXT("A")));Connect(RetainedPairTail,FindPin(RetainedFull,TEXT("B")));auto* RetainedNotify=F.Call(Master->GeneratedClass,TEXT("DOR_DebugNotify"));Connect(M,FindPin(RetainedNotify,UEdGraphSchema_K2::PN_Self));Connect(RetainedFull->GetReturnValuePin(),FindPin(RetainedNotify,TEXT("Message")));ClearPending();F.Return();F.Tail=Resolved->GetElsePin();F.Return();F.Tail=Pending->GetElsePin();
        UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=pump-settlement-ready"));
        F.Require(Allow->GetReturnValuePin());F.Require(F.Not(F.Field(M,Master->GeneratedClass,TEXT("ExternalPresetTransactionActive"))));
        auto* Loop=F.Each(DORRoster(F));auto* Player=FindPin(Loop,TEXT("Array Element"));F.Branch(F.IsValid(Player));
        auto Identity=DORPlayerIdentity(F,Player);F.Branch(Identity.Ready);auto* Key=F.Cache(TEXT("Key"),Identity.Key);auto MakeToken=[&](const TCHAR* Slot){auto* Tail=DORSafeConcat(F,TEXT("|"),DORSafeString(F,Slot));auto* Token=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(Key,FindPin(Token,TEXT("A")));Connect(Tail,FindPin(Token,TEXT("B")));return Token->GetReturnValuePin();};auto* PrimaryToken=MakeToken(TEXT("primary"));auto* SecondaryToken=MakeToken(TEXT("secondary"));
        auto Seen=[&](UEdGraphPin* Token){auto* Attempted=F.Array(TEXT("Array_Contains"),F.Get(TEXT("AttemptedPlayers")));Connect(Token,FindPin(Attempted,TEXT("ItemToFind")));auto* Consumed=F.Array(TEXT("Array_Contains"),F.Get(TEXT("ConsumedPlayers")));Connect(Token,FindPin(Consumed,TEXT("ItemToFind")));auto* Result=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(Attempted->GetReturnValuePin(),FindPin(Result,TEXT("A")));Connect(Consumed->GetReturnValuePin(),FindPin(Result,TEXT("B")));return Result->GetReturnValuePin();};auto* PrimarySeen=Seen(PrimaryToken);auto* SecondarySeen=Seen(SecondaryToken);
        auto* ReadPrimary=F.Self(TEXT("DOR_ReadManualPrimary"));Connect(Key,FindPin(ReadPrimary,TEXT("Key")));auto* Primary=F.Cache(TEXT("Primary"),ReadPrimary->GetReturnValuePin());auto* ReadSecondary=F.Self(TEXT("DOR_ReadManualSecondary"));Connect(Key,FindPin(ReadSecondary,TEXT("Key")));auto* Secondary=F.Cache(TEXT("Secondary"),ReadSecondary->GetReturnValuePin());auto Positive=[&](UEdGraphPin* Value){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Value,FindPin(N,TEXT("A")));return N->GetReturnValuePin();};auto* PrimaryAvailable=And(Positive(Primary),F.Not(PrimarySeen));auto* SecondaryAvailable=And(Positive(Secondary),F.Not(SecondarySeen));F.Branch(Or(PrimaryAvailable,SecondaryAvailable));
        auto* PairSelect=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Connect(Primary,FindPin(PairSelect,TEXT("A")));Connect(Secondary,FindPin(PairSelect,TEXT("B")));Connect(PrimaryAvailable,FindPin(PairSelect,TEXT("bPickA")));auto* Pair=F.Cache(TEXT("Pair"),PairSelect->GetReturnValuePin());auto* SlotSelect=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectString"));FindPin(SlotSelect,TEXT("A"))->DefaultValue=TEXT("primary");FindPin(SlotSelect,TEXT("B"))->DefaultValue=TEXT("secondary");Connect(PrimaryAvailable,FindPin(SlotSelect,TEXT("bPickA")));auto* SelectedSlot=F.Cache(TEXT("Slot"),SlotSelect->GetReturnValuePin());auto* TokenSelect=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectString"));Connect(PrimaryToken,FindPin(TokenSelect,TEXT("A")));Connect(SecondaryToken,FindPin(TokenSelect,TEXT("B")));Connect(PrimaryAvailable,FindPin(TokenSelect,TEXT("bPickA")));auto* SelectedToken=TokenSelect->GetReturnValuePin();
        auto* Local=F.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerPawn"));auto* IsLocal=F.Equal(Player,Local->GetReturnValuePin(),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass());
        auto* Compatible=F.Array(TEXT("Array_Contains"),F.Field(M,Master->GeneratedClass,TEXT("HandshakeCompatibleNetIds")));Connect(Key,FindPin(Compatible,TEXT("ItemToFind")));
        auto* Ready=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(IsLocal,FindPin(Ready,TEXT("A")));Connect(Compatible->GetReturnValuePin(),FindPin(Ready,TEXT("B")));F.Branch(Ready->GetReturnValuePin());
        auto* Probe=F.Branch(And(IsLocal,PrimaryAvailable));
        auto* Check=F.Self(TEXT("DOR_LocalPrimaryReady"));Connect(Player,FindPin(Check,TEXT("Player")));Connect(SelectedToken,FindPin(Check,TEXT("Token")));Connect(Pair,FindPin(Check,TEXT("Pair")));Connect(M,FindPin(Check,TEXT("MasterActor")));
        auto* ProbeReady=F.Branch(Check->GetReturnValuePin());auto* ProbePassed=F.Tail;
        F.Tail=ProbeReady->GetElsePin();F.Return();
        F.Tail=Probe->GetElsePin();
        // Record one pending asynchronous result. The setting remains on disk
        // during the apply and is reset only by the success branch above.
        auto* Mark=F.Array(TEXT("Array_AddUnique"),F.Get(TEXT("AttemptedPlayers")));Connect(SelectedToken,FindPin(Mark,TEXT("NewItem")));
        Connect(ProbePassed,Mark->GetExecPin());
        F.Set(TEXT("PendingKey"),Key);F.Set(TEXT("PendingPlayer"),Player);F.Set(TEXT("PendingPair"),Pair);F.Set(TEXT("PendingSlot"),SelectedSlot);F.Set(TEXT("PendingIsLocal"),IsLocal);F.Set(TEXT("PendingRemoteBaseline"),F.Field(M,Master->GeneratedClass,TEXT("RemoteRequestGeneration")));F.Set(TEXT("PendingSettleTicks"),nullptr,TEXT("0"));
        auto* PStr=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(Pair,FindPin(PStr,TEXT("InInt")));
        auto* SlotLog=DORSafeConcat(F,TEXT("[DOR-MENU][A76] one-shot-dispatched-awaiting-result slot="),SelectedSlot);auto* PairLogTail=DORSafeConcat(F,TEXT(" pair="),PStr->GetReturnValuePin());auto* PairLog=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(SlotLog,FindPin(PairLog,TEXT("A")));Connect(PairLogTail,FindPin(PairLog,TEXT("B")));auto* Notify=F.Call(Master->GeneratedClass,TEXT("DOR_DebugNotify"));Connect(M,FindPin(Notify,UEdGraphSchema_K2::PN_Self));Connect(PairLog->GetReturnValuePin(),FindPin(Notify,TEXT("Message")));
        // Keep the selected roster object as the identity anchor. Re-encoding it as
        // a numeric chat command made the parser enumerate the unordered roster a
        // second time and could silently redirect host/client assignments.
        auto* LocalDispatch=F.Branch(IsLocal);
        auto* RunLocal=F.Call(Master->GeneratedClass,TEXT("DOR_DebugSetIndex"));Connect(M,FindPin(RunLocal,UEdGraphSchema_K2::PN_Self));Connect(Pair,FindPin(RunLocal,TEXT("TestIndex")));F.Return(nullptr,TEXT("true"));
        F.Tail=LocalDispatch->GetElsePin();auto* RunRemote=F.Call(Master->GeneratedClass,TEXT("DOR_DispatchRemoteResearch"));Connect(M,FindPin(RunRemote,UEdGraphSchema_K2::PN_Self));Connect(Player,FindPin(RunRemote,TEXT("Player")));Connect(Pair,FindPin(RunRemote,TEXT("TestIndex")));FindPin(RunRemote,TEXT("Action"))->DefaultValue=TEXT("0");FindPin(RunRemote,TEXT("Argument"))->DefaultValue=TEXT("");F.Return(nullptr,TEXT("true"));
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Return();F.Compile();
        UE_LOG(LogDORGenerateAssets, Display, TEXT("DOR ModHub build phase=pump-compiled"));
    }
    {
        // Timers must target a void/no-argument function, not a bool-returning
        // helper (UE rejects delegates with any parameter storage).
        FDORSafeGraph F(MenuHost,TEXT("DOR_ManualTick"));F.Self(TEXT("DOR_PumpManual"));F.Compile();
    }
    {
        // A consumed setting has already been reset to zero on disk, so keep a
        // mission-local roster latch to suppress the secondary reward for that
        // same mission.  Otherwise inspect only the Alpha70+ scalar slots.
        FDORSafeGraph F(MenuHost,TEXT("DOR_HasAnyManualPair"));auto* ClaimedLength=F.Array(TEXT("Array_Length"),F.Get(TEXT("ConsumedPlayers")));
        auto* AnyClaimed=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(ClaimedLength->GetReturnValuePin(),FindPin(AnyClaimed,TEXT("A")));auto* Claimed=F.Branch(AnyClaimed->GetReturnValuePin());F.Return(nullptr,TEXT("true"));F.Tail=Claimed->GetElsePin();
        auto* Load=F.Self(TEXT("DOR_LoadManual"));F.Require(Load->GetReturnValuePin());
        auto* Loop=F.Each(DORRoster(F));auto* Player=FindPin(Loop,TEXT("Array Element"));F.Branch(F.IsValid(Player));
        auto Identity=DORPlayerIdentity(F,Player);F.Branch(Identity.Ready);auto* Primary=F.Self(TEXT("DOR_ReadManualPrimary"));Connect(Identity.Key,FindPin(Primary,TEXT("Key")));auto* Secondary=F.Self(TEXT("DOR_ReadManualSecondary"));Connect(Identity.Key,FindPin(Secondary,TEXT("Key")));
        auto Positive=[&](UEdGraphPin* Value){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Greater_IntInt"));Connect(Value,FindPin(N,TEXT("A")));return N->GetReturnValuePin();};auto* Any=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(Positive(Primary->GetReturnValuePin()),FindPin(Any,TEXT("A")));Connect(Positive(Secondary->GetReturnValuePin()),FindPin(Any,TEXT("B")));
        F.Branch(Any->GetReturnValuePin());F.Return(nullptr,TEXT("true"));
        F.Tail=FindPin(Loop,TEXT("Completed"));F.Return();F.Compile();
    }
    // Default/all-zero settings retain the original secondary-objective reward.
    // Any non-zero manual assignment selects direct mission-entry mode globally,
    // so completion cannot start a second fusion-card workflow.
    {
        auto* G=FindFunctionGraph(Master,TEXT("DOR_HandleSecondaryCompleted"));if(!G)return false;
        auto* Start=FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then);auto Old=Start->LinkedTo;Start->BreakAllPinLinks();
        auto* Find=AddCall(G,UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"),0,0);FindPin(Find,TEXT("ActorClass"))->DefaultObject=MenuHost->GeneratedClass;
        auto* CastNode=AddNode<UK2Node_DynamicCast>(G,0,0);CastNode->TargetType=MenuHost->GeneratedClass;CastNode->SetPurity(true);CastNode->AllocateDefaultPins();
        ForcePinType(CastNode->GetCastSourcePin(),MakeObjectPinType(UObject::StaticClass()));Connect(Find->GetReturnValuePin(),CastNode->GetCastSourcePin());
        auto* Valid=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("IsValid"),0,0);Connect(CastNode->GetCastResultPin(),FindPin(Valid,TEXT("Object")));
        auto* ValidBranch=AddNode<UK2Node_IfThenElse>(G,0,0);ValidBranch->AllocateDefaultPins();Connect(Start,Find->GetExecPin());Connect(Find->GetThenPin(),ValidBranch->GetExecPin());Connect(Valid->GetReturnValuePin(),ValidBranch->GetConditionPin());
        auto* Manual=AddCall(G,MenuHost->GeneratedClass,TEXT("DOR_HasAnyManualPair"),0,0);Connect(CastNode->GetCastResultPin(),FindPin(Manual,UEdGraphSchema_K2::PN_Self));Connect(ValidBranch->GetThenPin(),Manual->GetExecPin());
        auto* ManualBranch=AddNode<UK2Node_IfThenElse>(G,0,0);ManualBranch->AllocateDefaultPins();FDORRewardReceipt D(G,TEXT("manual-suppression-decision"));D.Path(TEXT("menuHost"),CastNode->GetCastResultPin());D.Add(TEXT("suppressed"),Manual->GetReturnValuePin());D.Add(TEXT("currentRound"),AddVariableGet(G,TEXT("CurrentRewardObjectiveKey"),0,0)->GetValuePin());Connect(D.Record(Manual->GetThenPin(),true)->GetThenPin(),ManualBranch->GetExecPin());Connect(Manual->GetReturnValuePin(),ManualBranch->GetConditionPin());
        auto* Skip=AddSelfCall(G,TEXT("DOR_DebugNotify"),0,0);FindPin(Skip,TEXT("Message"))->DefaultValue=TEXT("[DOR-MENU] 已启用房主手动融合，本轮次要任务不再触发 DOR 融合选卡。");Connect(ManualBranch->GetThenPin(),Skip->GetExecPin());
        for(auto* To:Old){Connect(ValidBranch->GetElsePin(),To);Connect(ManualBranch->GetElsePin(),To);}
    }
    {
        // Rebuild the choices at widget construction time. Design-time combo
        // options are not trusted to survive every ModHub widget reconstruction.
        if(!FindFProperty<FStrProperty>(UComboBoxString::StaticClass(),TEXT("SelectedOption")))return false;
        FDORSafeGraph F(Page,TEXT("DOR_ResetPairOptions"));
        for(const auto& Slot : {TPair<const TCHAR*,const TCHAR*>(TEXT("PrimaryPairs"),TEXT("PrimaryPairOptionIds")),TPair<const TCHAR*,const TCHAR*>(TEXT("SecondaryPairs"),TEXT("SecondaryPairOptionIds"))})
        {
            auto* Clear=F.Call(UComboBoxString::StaticClass(),TEXT("ClearOptions"));Connect(F.Get(Slot.Key),FindPin(Clear,UEdGraphSchema_K2::PN_Self));
            // ClearOptions can leave the cached label after Slate selection content is cleared.
            auto* Reset=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("SetStringPropertyByName"));Connect(F.Get(Slot.Key),FindPin(Reset,TEXT("Object")));FindPin(Reset,TEXT("PropertyName"))->DefaultValue=TEXT("SelectedOption");FindPin(Reset,TEXT("Value"))->DefaultValue=TEXT("");
            F.Array(TEXT("Array_Clear"),F.Get(Slot.Value));auto* Add=F.Call(UComboBoxString::StaticClass(),TEXT("AddOption"));Connect(F.Get(Slot.Key),FindPin(Add,UEdGraphSchema_K2::PN_Self));FindPin(Add,TEXT("Option"))->DefaultValue=TEXT("0 | 无 / 不自动应用");auto* AddNoneId=F.Array(TEXT("Array_Add"),F.Get(Slot.Value));FindPin(AddNoneId,TEXT("NewItem"))->DefaultValue=TEXT("0");auto* Select=F.Call(UComboBoxString::StaticClass(),TEXT("SetSelectedIndex"));Connect(F.Get(Slot.Key),FindPin(Select,UEdGraphSchema_K2::PN_Self));FindPin(Select,TEXT("Index"))->DefaultValue=TEXT("0");
        }
        F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSafeGraph F(Page,TEXT("DOR_SelectSavedChoices"));F.Require(F.IsValid(F.Get(TEXT("Host"))));auto* Id=F.Call(UComboBoxString::StaticClass(),TEXT("GetSelectedIndex"));Connect(F.Get(TEXT("Players")),FindPin(Id,UEdGraphSchema_K2::PN_Self));auto* Valid=F.Array(TEXT("Array_IsValidIndex"),F.Get(TEXT("RosterPlayers")));Connect(Id->GetReturnValuePin(),FindPin(Valid,TEXT("IndexToTest")));F.Require(Valid->GetReturnValuePin());auto* GetPlayer=F.Array(TEXT("Array_Get"),F.Get(TEXT("RosterPlayers")));Connect(Id->GetReturnValuePin(),FindPin(GetPlayer,TEXT("Index")));auto* Player=FindPin(GetPlayer,TEXT("Item"));F.Require(F.IsValid(Player));
        auto SelectSaved=[&](int32 Slot,const TCHAR* Options,const TCHAR* Combo){auto* Read=F.Call(MenuHost->GeneratedClass,TEXT("DOR_ReadPlayerDisplayedCode"));Connect(F.Get(TEXT("Host")),FindPin(Read,UEdGraphSchema_K2::PN_Self));Connect(Player,FindPin(Read,TEXT("Player")));FindPin(Read,TEXT("Slot"))->DefaultValue=FString::FromInt(Slot);auto* Find=F.Array(TEXT("Array_Find"),F.Get(Options));Connect(Read->GetReturnValuePin(),FindPin(Find,TEXT("ItemToFind")));auto* ValidIndex=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(Find->GetReturnValuePin(),FindPin(ValidIndex,TEXT("A")));auto* SelectIndex=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Connect(Find->GetReturnValuePin(),FindPin(SelectIndex,TEXT("A")));FindPin(SelectIndex,TEXT("B"))->DefaultValue=TEXT("0");Connect(ValidIndex->GetReturnValuePin(),FindPin(SelectIndex,TEXT("bPickA")));auto* Set=F.Call(UComboBoxString::StaticClass(),TEXT("SetSelectedIndex"));Connect(F.Get(Combo),FindPin(Set,UEdGraphSchema_K2::PN_Self));Connect(SelectIndex->GetReturnValuePin(),FindPin(Set,TEXT("Index")));};
        SelectSaved(0,TEXT("PrimaryPairOptionIds"),TEXT("PrimaryPairs"));SelectSaved(1,TEXT("SecondaryPairOptionIds"),TEXT("SecondaryPairs"));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        // The menu is driven only by replicated career identity. It accepts either
        // the selected pawn class or CharacterID name and never inspects weapons.
        FDORSafeGraph F(Page,TEXT("DOR_RefreshPairOptions"));
        DORSetStatus(F,TEXT("组合筛选失败 [M30]：所选玩家已离线或名单已变化，请刷新当前玩家。"));
        auto* Id=F.Call(UComboBoxString::StaticClass(),TEXT("GetSelectedIndex"));Connect(F.Get(TEXT("Players")),FindPin(Id,UEdGraphSchema_K2::PN_Self));
        auto* Valid=F.Array(TEXT("Array_IsValidIndex"),F.Get(TEXT("RosterPlayers")));Connect(Id->GetReturnValuePin(),FindPin(Valid,TEXT("IndexToTest")));auto* HaveSelection=F.Branch(Valid->GetReturnValuePin());F.Tail=HaveSelection->GetElsePin();F.Return(nullptr,TEXT("true"));F.Tail=HaveSelection->GetThenPin();
        auto* GetPlayer=F.Array(TEXT("Array_Get"),F.Get(TEXT("RosterPlayers")));Connect(Id->GetReturnValuePin(),FindPin(GetPlayer,TEXT("Index")));auto* Player=F.Cache(TEXT("Player"),FindPin(GetPlayer,TEXT("Item")));F.Require(F.IsValid(Player));
        auto* Dirty=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));Connect(F.Get(TEXT("PairChoicesDirty")),FindPin(Dirty,TEXT("A")));Connect(F.Equal(Player,F.Get(TEXT("PairChoicesPlayer")),TEXT("EqualEqual_ObjectObject"),UKismetMathLibrary::StaticClass()),FindPin(Dirty,TEXT("B")));auto* Editing=F.Branch(Dirty->GetReturnValuePin());F.Return(nullptr,TEXT("true"));F.Tail=Editing->GetElsePin();auto* Reset=F.Self(TEXT("DOR_ResetPairOptions"));F.Require(Reset->GetReturnValuePin());F.Set(TEXT("PairChoicesPlayer"),Player);F.Set(TEXT("PairChoicesDirty"),nullptr,TEXT("false"));
        auto* GetPS=F.Call(APlayerCharacter::StaticClass(),TEXT("GetPlayerState"));Connect(Player,FindPin(GetPS,UEdGraphSchema_K2::PN_Self));
        auto* PS=DORCastPin(F,GetPS->GetReturnValuePin(),AFSDPlayerState::StaticClass());F.Require(F.IsValid(PS));
        auto* SelectedClass=F.Call(AFSDPlayerState::StaticClass(),TEXT("GetSelectedCharacter"));Connect(PS,FindPin(SelectedClass,UEdGraphSchema_K2::PN_Self));
        auto* ClassName=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetClassDisplayName"));Connect(SelectedClass->GetReturnValuePin(),FindPin(ClassName,TEXT("Class")));
        auto* SelectedId=F.Call(AFSDPlayerState::StaticClass(),TEXT("GetSelectedCharacterID"));Connect(PS,FindPin(SelectedId,UEdGraphSchema_K2::PN_Self));
        auto* IdName=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetObjectName"));Connect(SelectedId->GetReturnValuePin(),FindPin(IdName,TEXT("Object")));
        auto Contains=[&](UEdGraphPin* Value,const TCHAR* Token){auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Contains"));Connect(Value,FindPin(N,TEXT("SearchIn")));FindPin(N,TEXT("Substring"))->DefaultValue=Token;FindPin(N,TEXT("bUseCase"))->DefaultValue=TEXT("false");return N->GetReturnValuePin();};
        auto Or=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=F.Call(UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();};
        auto* ScoutClass=Or(Contains(ClassName->GetReturnValuePin(),TEXT("Navigator")),Contains(ClassName->GetReturnValuePin(),TEXT("Scout")));
        auto* IsScout=Or(ScoutClass,Contains(IdName->GetReturnValuePin(),TEXT("Scout")));
        auto* IsEngineer=Or(Contains(ClassName->GetReturnValuePin(),TEXT("Engineer")),Contains(IdName->GetReturnValuePin(),TEXT("Engineer")));
        auto* IsDriller=Or(Contains(ClassName->GetReturnValuePin(),TEXT("Driller")),Contains(IdName->GetReturnValuePin(),TEXT("Driller")));
        auto* IsGunner=Or(Contains(ClassName->GetReturnValuePin(),TEXT("Gunner")),Contains(IdName->GetReturnValuePin(),TEXT("Gunner")));
        auto AddClassOptions=[&](const FString& ClassKey)
        {
            for(const FMenuOption& Option:MenuOptions)
            {
                if(Option.ClassKey!=ClassKey)continue;
                const TCHAR* Combo=Option.Slot==TEXT("primary") ? TEXT("PrimaryPairs") : TEXT("SecondaryPairs");
                const TCHAR* Ids=Option.Slot==TEXT("primary") ? TEXT("PrimaryPairOptionIds") : TEXT("SecondaryPairOptionIds");
                auto* Add=F.Call(UComboBoxString::StaticClass(),TEXT("AddOption"));
                Connect(F.Get(Combo),FindPin(Add,UEdGraphSchema_K2::PN_Self));
                FindPin(Add,TEXT("Option"))->DefaultValue=Option.Label;
                auto* AddId=F.Array(TEXT("Array_Add"),F.Get(Ids));
                FindPin(AddId,TEXT("NewItem"))->DefaultValue=FString::FromInt(Option.TestIndex);
            }
        };
        auto* Scout=F.Branch(IsScout);
        AddClassOptions(TEXT("scout"));
        auto* ScoutTail=F.Tail;F.Tail=Scout->GetElsePin();auto* Engineer=F.Branch(IsEngineer);Connect(ScoutTail,Engineer->GetExecPin());
        AddClassOptions(TEXT("engineer"));
        auto* EngineerTail=F.Tail;F.Tail=Engineer->GetElsePin();auto* Driller=F.Branch(IsDriller);Connect(EngineerTail,Driller->GetExecPin());
        AddClassOptions(TEXT("driller"));
        auto* DrillerTail=F.Tail;F.Tail=Driller->GetElsePin();auto* Gunner=F.Branch(IsGunner);Connect(DrillerTail,Gunner->GetExecPin());
        AddClassOptions(TEXT("gunner"));
        auto* GunnerTail=F.Tail;F.Tail=Gunner->GetElsePin();
        auto AppendLiteral=[&](UEdGraphPin* A,const TCHAR* B){auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(A,FindPin(N,TEXT("A")));FindPin(N,TEXT("B"))->DefaultValue=B;return N->GetReturnValuePin();};
        auto AppendValue=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();};
        const FString SummarySuffix=FString::Printf(TEXT("。当前已开放%d个组合。"),DORSupportedPairTestIndices().Num());
        auto* Summary=DORSafeConcat(F,TEXT("职业及槽位筛选：角色类="),ClassName->GetReturnValuePin());Summary=AppendLiteral(Summary,TEXT("，角色ID="));Summary=AppendValue(Summary,IdName->GetReturnValuePin());Summary=AppendLiteral(Summary,*SummarySuffix);
        auto* SummaryText=F.Call(UKismetTextLibrary::StaticClass(),TEXT("Conv_StringToText"));Connect(Summary,FindPin(SummaryText,TEXT("InString")));
        auto* Done=F.Call(UTextBlock::StaticClass(),TEXT("SetText"));Connect(F.Get(TEXT("Status")),FindPin(Done,UEdGraphSchema_K2::PN_Self));Connect(SummaryText->GetReturnValuePin(),FindPin(Done,TEXT("InText")));Connect(GunnerTail,Done->GetExecPin());auto* Saved=F.Self(TEXT("DOR_SelectSavedChoices"));F.Return(Saved->GetReturnValuePin());F.Compile();
    }
    {
        FDORSafeGraph F(Page,TEXT("DOR_RefreshRoster"));F.Set(TEXT("RosterPlayers"),DORRoster(F));
        auto* Clear=F.Call(UComboBoxString::StaticClass(),TEXT("ClearOptions"));Connect(F.Get(TEXT("Players")),FindPin(Clear,UEdGraphSchema_K2::PN_Self));
        auto* Loop=F.Each(F.Get(TEXT("RosterPlayers")));auto* Add=F.Call(UComboBoxString::StaticClass(),TEXT("AddOption"));
        Connect(F.Get(TEXT("Players")),FindPin(Add,UEdGraphSchema_K2::PN_Self));Connect(DORPlayerLabel(F,FindPin(Loop,TEXT("Array Element"))),FindPin(Add,TEXT("Option")));
        F.Tail=FindPin(Loop,TEXT("Completed"));auto* Select=F.Call(UComboBoxString::StaticClass(),TEXT("SetSelectedIndex"));Connect(F.Get(TEXT("Players")),FindPin(Select,UEdGraphSchema_K2::PN_Self));
        auto* OldIndex=F.Array(TEXT("Array_Find"),F.Get(TEXT("RosterPlayers")));Connect(F.Get(TEXT("PairChoicesPlayer")),FindPin(OldIndex,TEXT("ItemToFind")));auto* Found=F.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_IntInt"));Connect(OldIndex->GetReturnValuePin(),FindPin(Found,TEXT("A")));auto* KeepIndex=F.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Connect(OldIndex->GetReturnValuePin(),FindPin(KeepIndex,TEXT("A")));FindPin(KeepIndex,TEXT("B"))->DefaultValue=TEXT("0");Connect(Found->GetReturnValuePin(),FindPin(KeepIndex,TEXT("bPickA")));Connect(KeepIndex->GetReturnValuePin(),FindPin(Select,TEXT("Index")));
        auto* Enabled=F.Call(UWidget::StaticClass(),TEXT("SetIsEnabled"));Connect(F.Get(TEXT("Apply")),FindPin(Enabled,UEdGraphSchema_K2::PN_Self));Connect(DORLocalHost(F),FindPin(Enabled,TEXT("bInIsEnabled")));auto* PairRefresh=F.Self(TEXT("DOR_RefreshPairOptions"));F.Return(PairRefresh->GetReturnValuePin());F.Compile();
    }
    {
        FDORSafeGraph F(Page,TEXT("DOR_SaveChoice"));DORSetStatus(F,TEXT("设置未提交 [M10]：当前实例不是房主权威端。"));F.Require(DORLocalHost(F));
        DORSetStatus(F,TEXT("设置未提交 [M20]：ModHub 控制器尚未就绪，请关闭页面后重新打开。"));F.Require(F.IsValid(F.Get(TEXT("Host"))));
        auto* Id=F.Call(UComboBoxString::StaticClass(),TEXT("GetSelectedIndex"));Connect(F.Get(TEXT("Players")),FindPin(Id,UEdGraphSchema_K2::PN_Self));
        DORSetStatus(F,TEXT("设置未提交 [M30]：所选玩家已离线或名单已变化，请刷新当前玩家。"));auto* Valid=F.Array(TEXT("Array_IsValidIndex"),F.Get(TEXT("RosterPlayers")));Connect(Id->GetReturnValuePin(),FindPin(Valid,TEXT("IndexToTest")));F.Require(Valid->GetReturnValuePin());
        auto* Player=F.Array(TEXT("Array_Get"),F.Get(TEXT("RosterPlayers")));Connect(Id->GetReturnValuePin(),FindPin(Player,TEXT("Index")));F.Require(F.IsValid(FindPin(Player,TEXT("Item"))));
        auto* S=F.Self(TEXT("DOR_GetSafety"));DORSetStatus(F,TEXT("设置未提交 [M50]：存档安全服务尚未初始化或当前存档身份不匹配。"));auto* Init=F.Call(Safety->GeneratedClass,TEXT("DOR_Initialize"));Connect(S->GetReturnValuePin(),FindPin(Init,UEdGraphSchema_K2::PN_Self));F.Require(Init->GetReturnValuePin());
        DORSetStatus(F,TEXT("设置未提交 [M51]：仍有未完成或异常的存档事务，已阻止写入。"));F.Require(F.Not(F.Field(S->GetReturnValuePin(),Safety->GeneratedClass,TEXT("DOR_Blocked"))));
        DORSetStatus(F,TEXT("设置未提交 [M52]：本档已执行“清理存档 / 准备卸载”，DOR_Disabled 标记正在停用本模组。若要继续使用，请退出游戏后手动删除对应 DOR_Disabled_*.sav，再重新启动。"));F.Require(F.Not(F.Field(S->GetReturnValuePin(),Safety->GeneratedClass,TEXT("DOR_Disabled"))));
        auto ReadChoice=[&](const TCHAR* Combo,const TCHAR* Options,const TCHAR* CacheName){auto* ChoiceIndex=F.Call(UComboBoxString::StaticClass(),TEXT("GetSelectedIndex"));Connect(F.Get(Combo),FindPin(ChoiceIndex,UEdGraphSchema_K2::PN_Self));auto* PairIndexValid=F.Array(TEXT("Array_IsValidIndex"),F.Get(Options));Connect(ChoiceIndex->GetReturnValuePin(),FindPin(PairIndexValid,TEXT("IndexToTest")));F.Require(PairIndexValid->GetReturnValuePin());auto* PairId=F.Array(TEXT("Array_Get"),F.Get(Options));Connect(ChoiceIndex->GetReturnValuePin(),FindPin(PairId,TEXT("Index")));return F.Cache(CacheName,FindPin(PairId,TEXT("Item")));};
        DORSetStatus(F,TEXT("设置未提交 [M55]：主武器或次武器组合索引无效，请刷新后重新选择。"));auto* SelectedPrimary=ReadChoice(TEXT("PrimaryPairs"),TEXT("PrimaryPairOptionIds"),TEXT("SelectedPrimary"));auto* SelectedSecondary=ReadChoice(TEXT("SecondaryPairs"),TEXT("SecondaryPairOptionIds"),TEXT("SelectedSecondary"));
        DORSetStatus(F,TEXT("设置未提交 [M60]：双槽手动设置文件保存或回读失败。"));auto* Set=F.Call(MenuHost->GeneratedClass,TEXT("DOR_SetManual"));Connect(F.Get(TEXT("Host")),FindPin(Set,UEdGraphSchema_K2::PN_Self));Connect(FindPin(Player,TEXT("Item")),FindPin(Set,TEXT("Player")));Connect(SelectedPrimary,FindPin(Set,TEXT("PrimaryPair")));Connect(SelectedSecondary,FindPin(Set,TEXT("SecondaryPair")));F.Require(Set->GetReturnValuePin());
        F.Set(TEXT("PairChoicesDirty"),nullptr,TEXT("false"));
        auto ToText=[&](UEdGraphPin* Value){auto* N=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(Value,FindPin(N,TEXT("InInt")));return N->GetReturnValuePin();};auto* PrimaryMessage=DORSafeConcat(F,TEXT("已保存并回读校验：主武器="),ToText(SelectedPrimary));auto* SecondaryMessage=DORSafeConcat(F,TEXT("，次武器="),ToText(SelectedSecondary));auto* SavedMessage=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));Connect(PrimaryMessage,FindPin(SavedMessage,TEXT("A")));Connect(SecondaryMessage,FindPin(SavedMessage,TEXT("B")));auto* SavedText=F.Call(UKismetTextLibrary::StaticClass(),TEXT("Conv_StringToText"));Connect(SavedMessage->GetReturnValuePin(),FindPin(SavedText,TEXT("InString")));auto* SavedStatus=F.Call(UTextBlock::StaticClass(),TEXT("SetText"));Connect(F.Get(TEXT("Status")),FindPin(SavedStatus,UEdGraphSchema_K2::PN_Self));Connect(SavedText->GetReturnValuePin(),FindPin(SavedStatus,TEXT("InText")));F.Return(nullptr,TEXT("true"));F.Compile();
    }
    {
        FDORSafeGraph F(Page,TEXT("DOR_CleanupClicked"));DORSetStatus(F,TEXT("尚未清理成功：须在空间站勾选确认，且备份/事务检查通过。若出现恢复错误，请保留 pak 和恢复文件。"));
        auto* Check=F.Call(UCheckBox::StaticClass(),TEXT("IsChecked"));Connect(F.Get(TEXT("ConfirmCleanup")),FindPin(Check,UEdGraphSchema_K2::PN_Self));F.Require(Check->GetReturnValuePin());
        auto* S=F.Self(TEXT("DOR_GetSafety"));auto* Clean=F.Call(Safety->GeneratedClass,TEXT("DOR_PrepareUninstall"));Connect(S->GetReturnValuePin(),FindPin(Clean,UEdGraphSchema_K2::PN_Self));
        auto* Result=F.Branch(Clean->GetReturnValuePin());
        DORSetStatus(F,TEXT("清理和保存回读均成功，本机当前档 DOR 已停用。请退出游戏，然后手动删除 DOR pak。首次备份保留；其他玩家需在各自客户端清理。"));F.Return(nullptr,TEXT("true"));
        F.Tail=Result->GetElsePin();auto* ErrorText=F.Call(UKismetTextLibrary::StaticClass(),TEXT("Conv_StringToText"));Connect(F.Field(S->GetReturnValuePin(),Safety->GeneratedClass,TEXT("DOR_LastError")),FindPin(ErrorText,TEXT("InString")));
        auto* SetError=F.Call(UTextBlock::StaticClass(),TEXT("SetText"));Connect(F.Get(TEXT("Status")),FindPin(SetError,UEdGraphSchema_K2::PN_Self));Connect(ErrorText->GetReturnValuePin(),FindPin(SetError,TEXT("InText")));F.Return();F.Compile();
    }
    {
        auto* G=FBlueprintEditorUtils::FindEventGraph(Page);auto* E=AddNode<UK2Node_Event>(G,0,0);E->EventReference.SetExternalMember(TEXT("Construct"),UUserWidget::StaticClass());E->bOverrideFunction=true;E->AllocateDefaultPins();
        // ModHub caches page widgets. This interface event fires on each open.
        auto* Opened=AddNode<UK2Node_Event>(G,0,0);Opened->EventReference.SetExternalMember(TEXT("HubPageOpened"),PageI->GeneratedClass);Opened->bOverrideFunction=true;Opened->AllocateDefaultPins();
        auto* ModeOnOpen=AddSelfCall(G,TEXT("DOR_ApplyPageMode"),0,0);Connect(Opened->FindPin(UEdGraphSchema_K2::PN_Then),ModeOnOpen->GetExecPin());
        auto* RefreshOnOpen=AddSelfCall(G,TEXT("DOR_RefreshRoster"),0,0);Connect(ModeOnOpen->GetThenPin(),RefreshOnOpen->GetExecPin());
        auto* ModeOnConstruct=AddSelfCall(G,TEXT("DOR_ApplyPageMode"),0,0);Connect(E->FindPin(UEdGraphSchema_K2::PN_Then),ModeOnConstruct->GetExecPin());
        UEdGraphPin* Tail=ModeOnConstruct->GetThenPin();
        auto* Delegate=FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(),TEXT("OnClicked"));
        for(const auto& B:{TPair<FName,FName>(TEXT("Refresh"),TEXT("DOR_RefreshRoster")),TPair<FName,FName>(TEXT("Apply"),TEXT("DOR_SaveChoice")),TPair<FName,FName>(TEXT("Clean"),TEXT("DOR_CleanupClicked"))})
        {
            TSet<UEdGraphNode*> Old;for(auto* N:G->Nodes)Old.Add(N);auto* Assign=AddNode<UK2Node_AssignDelegate>(G,0,0);Assign->SetFromProperty(Delegate,false,UButton::StaticClass());Assign->AllocateDefaultPins();Assign->PostPlacedNewNode();
            UK2Node_CustomEvent* Click=nullptr;for(auto* N:G->Nodes)if(!Old.Contains(N))if(auto* C=Cast<UK2Node_CustomEvent>(N))Click=C;if(!Click)return false;
            Click->CustomFunctionName=FName(*(FString(TEXT("Click_"))+B.Key.ToString()));Click->ReconstructNode();
            auto* Var=AddVariableGet(G,B.Key,0,0);Connect(Var->GetValuePin(),FindPin(Assign,UEdGraphSchema_K2::PN_Self));Connect(Tail,Assign->GetExecPin());Tail=FindPin(Assign,UEdGraphSchema_K2::PN_Then);
            auto* Run=AddSelfCall(G,B.Value,0,0);Connect(Click->FindPin(UEdGraphSchema_K2::PN_Then),Run->GetExecPin());
        }
        auto* SelectionDelegate=FindFProperty<FMulticastDelegateProperty>(UComboBoxString::StaticClass(),TEXT("OnSelectionChanged"));if(!SelectionDelegate)return false;
        TSet<UEdGraphNode*> Old;for(auto* N:G->Nodes)Old.Add(N);auto* AssignSelection=AddNode<UK2Node_AssignDelegate>(G,0,0);AssignSelection->SetFromProperty(SelectionDelegate,false,UComboBoxString::StaticClass());AssignSelection->AllocateDefaultPins();AssignSelection->PostPlacedNewNode();
        UK2Node_CustomEvent* Changed=nullptr;for(auto* N:G->Nodes)if(!Old.Contains(N))if(auto* C=Cast<UK2Node_CustomEvent>(N))Changed=C;if(!Changed)return false;Changed->CustomFunctionName=TEXT("Changed_PlayerSelection");Changed->ReconstructNode();
        auto* PlayersVar=AddVariableGet(G,TEXT("Players"),0,0);Connect(PlayersVar->GetValuePin(),FindPin(AssignSelection,UEdGraphSchema_K2::PN_Self));Connect(Tail,AssignSelection->GetExecPin());Tail=FindPin(AssignSelection,UEdGraphSchema_K2::PN_Then);
        auto* RefreshPairs=AddSelfCall(G,TEXT("DOR_RefreshPairOptions"),0,0);Connect(Changed->FindPin(UEdGraphSchema_K2::PN_Then),RefreshPairs->GetExecPin());
        for(const TCHAR* Combo:{TEXT("PrimaryPairs"),TEXT("SecondaryPairs")})
        {
            TSet<UEdGraphNode*> Existing;for(auto* N:G->Nodes)Existing.Add(N);auto* Assign=AddNode<UK2Node_AssignDelegate>(G,0,0);Assign->SetFromProperty(SelectionDelegate,false,UComboBoxString::StaticClass());Assign->AllocateDefaultPins();Assign->PostPlacedNewNode();UK2Node_CustomEvent* Draft=nullptr;for(auto* N:G->Nodes)if(!Existing.Contains(N))if(auto* C=Cast<UK2Node_CustomEvent>(N))Draft=C;if(!Draft)return false;Draft->CustomFunctionName=FName(*(FString(TEXT("Changed_Draft_"))+Combo));Draft->ReconstructNode();auto* SelectionType=Draft->FindPin(TEXT("SelectionType"));auto* Enum=SelectionType?Cast<UEnum>(SelectionType->PinType.PinSubCategoryObject.Get()):nullptr;if(!Enum)return false;int64 Direct=-1;for(int32 I=0;I<Enum->NumEnums();++I)if(Enum->GetNameStringByIndex(I)==TEXT("Direct"))Direct=Enum->GetValueByIndex(I);if(Direct<0)return false;
            Connect(AddVariableGet(G,Combo,0,0)->GetValuePin(),FindPin(Assign,UEdGraphSchema_K2::PN_Self));Connect(Tail,Assign->GetExecPin());Tail=FindPin(Assign,UEdGraphSchema_K2::PN_Then);auto* User=AddCall(G,UKismetMathLibrary::StaticClass(),TEXT("NotEqual_ByteByte"),0,0);Connect(SelectionType,FindPin(User,TEXT("A")));FindPin(User,TEXT("B"))->DefaultValue=FString::Printf(TEXT("%lld"),Direct);auto* Branch=AddNode<UK2Node_IfThenElse>(G,0,0);Branch->AllocateDefaultPins();Connect(User->GetReturnValuePin(),Branch->GetConditionPin());Connect(Draft->FindPin(UEdGraphSchema_K2::PN_Then),Branch->GetExecPin());auto* Dirty=AddVariableSet(G,TEXT("PairChoicesDirty"),0,0);FindPin(Dirty,TEXT("PairChoicesDirty"))->DefaultValue=TEXT("true");Connect(Branch->GetThenPin(),Dirty->GetExecPin());
        }
        auto* Reset=AddSelfCall(G,TEXT("DOR_ResetPairOptions"),0,0);Connect(Tail,Reset->GetExecPin());
        auto* Refresh=AddSelfCall(G,TEXT("DOR_RefreshRoster"),0,0);Connect(Reset->GetThenPin(),Refresh->GetExecPin());
    }
    {
        auto* G=FBlueprintEditorUtils::FindEventGraph(MenuHost);auto* E=AddNode<UK2Node_Event>(G,0,0);E->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"),AActor::StaticClass());E->bOverrideFunction=true;E->AllocateDefaultPins();
        auto* Timer=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("K2_SetTimer"),0,0);FindPin(Timer,TEXT("FunctionName"))->DefaultValue=TEXT("DOR_ManualTick");FindPin(Timer,TEXT("Time"))->DefaultValue=TEXT("1.0");FindPin(Timer,TEXT("bLooping"))->DefaultValue=TEXT("true");
        auto* Clear=AddSelfCall(G,TEXT("DOR_ClearOnRigEntry"),0,0);
        Connect(SelfPin(G,0,0),FindPin(Timer,TEXT("Object")));Connect(E->FindPin(UEdGraphSchema_K2::PN_Then),Clear->GetExecPin());Connect(Clear->GetThenPin(),Timer->GetExecPin());
    }
    // Soft-load only the optional UI. Core safety/fusion still loads without
    // ModHub.pak; no copies of ModHub interfaces are included in this package.
    {
        auto* G=FBlueprintEditorUtils::FindEventGraph(Safety);auto* E=AddNode<UK2Node_Event>(G,0,0);E->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"),AActor::StaticClass());E->bOverrideFunction=true;E->AllocateDefaultPins();
        auto* Load=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("LoadClassAsset_Blocking"),0,0);FindPin(Load,TEXT("AssetClass"))->DefaultValue=FString(RuntimeRoot)+TEXT("/Blueprints/BP_DOR_ModHub.BP_DOR_ModHub_C");
        auto* Timer=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("K2_SetTimer"),0,0);FindPin(Timer,TEXT("FunctionName"))->DefaultValue=TEXT("DOR_BackupTick");FindPin(Timer,TEXT("Time"))->DefaultValue=TEXT("1.0");FindPin(Timer,TEXT("bLooping"))->DefaultValue=TEXT("true");
        Connect(SelfPin(G,0,0),FindPin(Timer,TEXT("Object")));Connect(E->FindPin(UEdGraphSchema_K2::PN_Then),Timer->GetExecPin());Connect(Timer->GetThenPin(),Load->GetExecPin());
        auto* Valid=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("IsValidClass"),0,0);Connect(Load->GetReturnValuePin(),FindPin(Valid,TEXT("Class")));
        auto* B=AddNode<UK2Node_IfThenElse>(G,0,0);B->AllocateDefaultPins();Connect(Load->GetThenPin(),B->GetExecPin());Connect(Valid->GetReturnValuePin(),B->GetConditionPin());
        auto* ClassCast=AddNode<UK2Node_ClassDynamicCast>(G,0,0);ClassCast->TargetType=AActor::StaticClass();ClassCast->SetPurity(true);ClassCast->AllocateDefaultPins();
        ForcePinType(ClassCast->GetCastSourcePin(),MakeClassPinType(UObject::StaticClass()));Connect(Load->GetReturnValuePin(),ClassCast->GetCastSourcePin());
        auto* Existing=AddCall(G,UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"),0,0);Connect(SelfPin(G,0,0),FindPin(Existing,TEXT("WorldContextObject")));Connect(ClassCast->GetCastResultPin(),FindPin(Existing,TEXT("ActorClass")));Connect(B->GetThenPin(),Existing->GetExecPin());
        auto* ExistingValid=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("IsValid"),0,0);Connect(Existing->GetReturnValuePin(),FindPin(ExistingValid,TEXT("Object")));
        auto* Missing=AddCall(G,UKismetMathLibrary::StaticClass(),TEXT("Not_PreBool"),0,0);Connect(ExistingValid->GetReturnValuePin(),FindPin(Missing,TEXT("A")));
        auto* SpawnBranch=AddNode<UK2Node_IfThenElse>(G,0,0);SpawnBranch->AllocateDefaultPins();Connect(Existing->GetThenPin(),SpawnBranch->GetExecPin());Connect(Missing->GetReturnValuePin(),SpawnBranch->GetConditionPin());
        auto* Spawn=AddNode<UK2Node_SpawnActorFromClass>(G,0,0);Spawn->AllocateDefaultPins();Connect(ClassCast->GetCastResultPin(),Spawn->GetClassPin());Connect(B->GetThenPin(),Spawn->GetExecPin());
        Spawn->GetExecPin()->BreakAllPinLinks();Connect(SpawnBranch->GetThenPin(),Spawn->GetExecPin());
        auto* T=AddCall(G,UKismetMathLibrary::StaticClass(),TEXT("MakeTransform"),0,0);FindPin(T,TEXT("Scale"))->DefaultValue=TEXT("1,1,1");Connect(T->GetReturnValuePin(),FindPin(Spawn,TEXT("SpawnTransform")));
    }
    {
        auto* G=FindFunctionGraph(MenuHost,TEXT("GetModPages"));
        if(!G)for(const auto& I:MenuHost->ImplementedInterfaces)for(auto* Graph:I.Graphs)if(Graph->GetFName()==TEXT("GetModPages"))G=Graph;
        if(!G)return false;
        auto* Start=FindFunctionEntry(G)->FindPin(UEdGraphSchema_K2::PN_Then);auto Old=Start->LinkedTo;Start->BreakAllPinLinks();
        auto* Clear=AddSelfCall(G,TEXT("DOR_ClearOnRigEntry"),0,0);Connect(Start,Clear->GetExecPin());for(auto* Next:Old)Connect(Clear->GetThenPin(),Next);
    }
    for(auto* BP:{static_cast<UBlueprint*>(Page),MenuHost,Safety}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    return true;
}

// Inspect the finished graphs, not merely the historical builder source. A
// future builder must not silently reintroduce a direct disk-write escape hatch.
bool AuditDORSaveSafety(UBlueprint* Master,UBlueprint* Rig,UBlueprint* Guard,
    UBlueprint* Safety,UBlueprint* MenuHost,UBlueprint* Page,UBlueprint* ManualSettings,
    const FString& ManifestHash)
{
    int32 ForbiddenWrites=0,PreparedRoutes=0,NativeWindowPrepares=0,NativeLegacyFallbackPrepares=0,NativeCommitWriters=0,UncheckedIO=0;
    int32 CharacterPatches=0,SourceCharacterPatches=0,ForbiddenCharacterFields=0,InvalidTimers=0;
    int32 RegistrationPrepares=0,RegistrationCommits=0,PresetResumeChecks=0,InvalidPresetResume=0;
    int32 LocalPageControllerChecks=0,PageWidgetCreates=0;
    int32 FinalLoadoutIdentityGuids=0,FinalWeaponIdentityChecks=0;
    int32 AlreadySelectedSuccessSets=0;
    int32 ManualLocalObjectDispatches=0,ManualRemoteObjectDispatches=0,ManualTextDispatches=0;
    int32 ManualRemotePlayerLinks=0,RemoteDispatchPlayerInputs=0,RemoteDispatchIndexInputs=0;
    int32 DualSlotSavedFields=0,DualSlotReaderGraphs=0,DualSlotWriterInputs=0;
    int32 DualSlotSetterInputs=0,SlotBoundConsumeInputs=0,PageDualSlotSetCalls=0;
    for(int32 SlotIndex=0;SlotIndex<4;++SlotIndex)
    {
        for(const TCHAR* Prefix:{TEXT("IdPrimaryPair"),TEXT("IdSecondaryPair")})
        {
            const FName Field(*FString::Printf(TEXT("%s%d"),Prefix,SlotIndex));
            if(ManualSettings && ManualSettings->GeneratedClass &&
               ManualSettings->GeneratedClass->FindPropertyByName(Field)) ++DualSlotSavedFields;
        }
    }
    for(auto* BP:{Master,Rig,Guard,Safety,MenuHost,Page})
    {
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for(auto* G:Graphs) for(auto* N:G->Nodes)
        {
            if(auto* Call=Cast<UK2Node_CallFunction>(N))
            {
                const FName Name=Call->FunctionReference.GetMemberName();
                const bool DiskWrite=Name==TEXT("SaveToDisk") || Name==TEXT("SaveSlotToDisk") || Name==TEXT("SaveGameToSlot") || Name==TEXT("DeleteGameInSlot");
                // The auxiliary DD run slot is not a native profile/journal.
                // Permit only this exact SaveGame class, graph, slot and user.
                bool AllowedDeepDiveWrite=false;
                if(BP==Master && G->GetFName()==TEXT("DOR_DDSaveRun") && Name==TEXT("SaveGameToSlot"))
                {
                    auto* ObjectPin=FindPin(Call,TEXT("SaveGameObject"));auto* SlotPin=FindPin(Call,TEXT("SlotName"));auto* UserPin=FindPin(Call,TEXT("UserIndex"));
                    auto* Source=ObjectPin&&ObjectPin->LinkedTo.Num()==1?Cast<UK2Node_DynamicCast>(ObjectPin->LinkedTo[0]->GetOwningNode()):nullptr;
                    UClass* Class=Source?Source->TargetType.Get():nullptr;
                    AllowedDeepDiveWrite=Class&&Class->IsChildOf(USaveGame::StaticClass())&&Class->GetOutermost()->GetName()==FString(RuntimeRoot)/TEXT("Blueprints/SG_DOR_DeepDiveRun")&&Class->GetName()==TEXT("SG_DOR_DeepDiveRun_C")&&SlotPin&&SlotPin->LinkedTo.Num()==0&&SlotPin->DefaultValue==TEXT("Mods/DOR_DeepDiveRun")&&UserPin&&UserPin->LinkedTo.Num()==0&&UserPin->DefaultValue==TEXT("0")&&Call->GetReturnValuePin()->LinkedTo.Num()>0;
                }
                if(BP==Master && G->GetFName()==TEXT("DOR_DDDeleteRun") && Name==TEXT("DeleteGameInSlot"))
                {
                    auto ExactSlot=[](UK2Node_CallFunction* C){auto* S=FindPin(C,TEXT("SlotName"));auto* U=FindPin(C,TEXT("UserIndex"));return S&&!S->LinkedTo.Num()&&S->DefaultValue==TEXT("Mods/DOR_DeepDiveRun")&&U&&!U->LinkedTo.Num()&&U->DefaultValue==TEXT("0");};
                    int32 CheckedExists=0;for(auto* Node:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(Node))if(C->FunctionReference.GetMemberName()==TEXT("DoesSaveGameExist")&&ExactSlot(C)&&C->GetReturnValuePin()->LinkedTo.Num())++CheckedExists;
                    AllowedDeepDiveWrite=ExactSlot(Call)&&Call->GetReturnValuePin()->LinkedTo.Num()>0&&CheckedExists==2;
                }
                if((BP==Master || BP==Rig || BP==Guard) && DiskWrite && !AllowedDeepDiveWrite)
                {++ForbiddenWrites; UE_LOG(LogTemp,Error,TEXT("DOR-SAFETY-AUDIT forbidden writer %s.%s: %s"),*BP->GetName(),*G->GetName(),*Name.ToString());}
                if(BP==Master && Name==TEXT("DOR_Prepare")){++PreparedRoutes;if(G->GetFName()==TEXT("DOR_NativeOpenWindow"))++NativeWindowPrepares;if(G->GetFName()==TEXT("DOR_NativeLegacyPrepare"))++NativeLegacyFallbackPrepares;}
                if(BP==Master && Name==TEXT("DOR_NativeLegacyPrepare"))++PreparedRoutes;
                if(BP==Rig && Name==TEXT("DOR_PrepareRegistration"))++RegistrationPrepares;
                if(BP==Rig && Name==TEXT("DOR_CommitRegistration"))++RegistrationCommits;
                if(BP==Safety && Name==TEXT("SaveSlotToDisk"))
                {++NativeCommitWriters; if(G->GetFName()!=TEXT("DOR_CommitNativeSave"))++ForbiddenWrites;}
                if(BP==MenuHost && G->GetFName()==TEXT("GetModPages"))
                {
                    if(Name==TEXT("IsLocalPlayerController"))++LocalPageControllerChecks;
                    if(Name==TEXT("Create"))++PageWidgetCreates;
                }
                if(BP==MenuHost && G->GetFName()==TEXT("DOR_PumpManual"))
                {
                    if(Name==TEXT("DOR_DebugSetIndex"))++ManualLocalObjectDispatches;
                    if(Name==TEXT("DOR_DispatchRemoteResearch"))
                    {
                        ++ManualRemoteObjectDispatches;
                        auto* Target=FindPin(Call,TEXT("Player"));
                        if(Target && Target->LinkedTo.Num()==1)++ManualRemotePlayerLinks;
                    }
                    if(Name==TEXT("DOR_ParseResearchCommand"))++ManualTextDispatches;
                }
                if(BP==Page && G->GetFName()==TEXT("DOR_SaveChoice") &&
                   Name==TEXT("DOR_SetManual") && Call->FindPin(TEXT("PrimaryPair")) &&
                   Call->FindPin(TEXT("SecondaryPair"))) ++PageDualSlotSetCalls;
                const bool FusionCycle=BP==Master &&
                    (G->GetFName()==TEXT("DOR_ForcePrimaryFusionWeaponCycle") ||
                     G->GetFName()==TEXT("DOR_ForceSecondaryFusionWeaponCycle"));
                if(FusionCycle && Name==TEXT("GetSavegameID"))++FinalLoadoutIdentityGuids;
                if(FusionCycle && Name==TEXT("EqualEqual_ClassClass"))++FinalWeaponIdentityChecks;
                if((Name==TEXT("SaveSlotToDisk") || Name==TEXT("SaveGameToSlot") || Name==TEXT("DeleteGameInSlot")) &&
                   (!Call->GetReturnValuePin() || Call->GetReturnValuePin()->LinkedTo.Num()==0)) ++UncheckedIO;
                if(Name==TEXT("K2_SetTimer"))
                {
                    const FString Fn=FindPin(Call,TEXT("FunctionName"))->DefaultValue;
                    if(Fn.StartsWith(TEXT("DOR_")))
                    {
                        UClass* TimerOwnerClass=BP->GeneratedClass;
                        auto* ObjectPin=FindPin(Call,TEXT("Object"));
                        UEdGraphPin* LinkedObject=ObjectPin && ObjectPin->LinkedTo.Num()==1
                            ? ObjectPin->LinkedTo[0] : nullptr;
                        if(LinkedObject)
                        {
                            if(auto* Spawn=Cast<UK2Node_SpawnActorFromClass>(LinkedObject->GetOwningNode()))
                            {
                                if(auto* SpawnClass=Cast<UClass>(Spawn->GetClassPin()->DefaultObject))
                                    TimerOwnerClass=SpawnClass;
                            }
                            else if(auto* CastNode=Cast<UK2Node_DynamicCast>(LinkedObject->GetOwningNode()))
                            {
                                if(CastNode->TargetType) TimerOwnerClass=CastNode->TargetType;
                            }
                            else if(auto* PinClass=Cast<UClass>(LinkedObject->PinType.PinSubCategoryObject.Get()))
                            {
                                TimerOwnerClass=PinClass;
                            }
                        }
                        auto* Function=TimerOwnerClass
                            ? TimerOwnerClass->FindFunctionByName(FName(*Fn)) : nullptr;
                        if(!Function || Function->ParmsSize!=0)
                        {
                            ++InvalidTimers;
                            UE_LOG(LogTemp,Error,
                                TEXT("DOR-SAFETY-AUDIT invalid timer %s.%s: function=%s owner=%s parms=%d"),
                                *BP->GetName(),*G->GetName(),*Fn,*GetNameSafe(TimerOwnerClass),
                                Function ? Function->ParmsSize : -1);
                        }
                    }
                }
                if(BP==Master && G->GetFName()==TEXT("DOR_ExecutePair388PresetTransaction") &&
                    Name==TEXT("DOR_DebugNotify") && FindPin(Call,TEXT("Message"))->DefaultValue.Contains(TEXT("[P05]")))
                {
                    ++PresetResumeChecks; auto Links=Call->GetExecPin()->LinkedTo;
                    auto* Resume=Links.Num()==1 ? Cast<UK2Node_VariableSet>(Links[0]->GetOwningNode()) : nullptr;
                    if(!Resume || Resume->VariableReference.GetMemberName()!=TEXT("ExternalUpgradeLoadoutsSnapshot")) ++InvalidPresetResume;
                }
            }
            if(auto* FunctionEntry=Cast<UK2Node_FunctionEntry>(N)) if(BP==Master &&
                G->GetFName()==TEXT("DOR_DispatchRemoteResearch"))
            {
                if(FunctionEntry->FindPin(TEXT("Player")))++RemoteDispatchPlayerInputs;
                if(FunctionEntry->FindPin(TEXT("PlayerIndex")))++RemoteDispatchIndexInputs;
            }
            if(auto* FunctionEntry=Cast<UK2Node_FunctionEntry>(N)) if(BP==MenuHost)
            {
                const FName GraphName=G->GetFName();
                if(GraphName==TEXT("DOR_ReadManualPrimary") ||
                   GraphName==TEXT("DOR_ReadManualSecondary")) ++DualSlotReaderGraphs;
                if(GraphName==TEXT("DOR_WriteManualFlat") ||
                   GraphName==TEXT("DOR_SaveManualFlat"))
                {
                    if(FunctionEntry->FindPin(TEXT("PrimaryPair"))) ++DualSlotWriterInputs;
                    if(FunctionEntry->FindPin(TEXT("SecondaryPair"))) ++DualSlotWriterInputs;
                }
                if(GraphName==TEXT("DOR_SetManual"))
                {
                    if(FunctionEntry->FindPin(TEXT("PrimaryPair"))) ++DualSlotSetterInputs;
                    if(FunctionEntry->FindPin(TEXT("SecondaryPair"))) ++DualSlotSetterInputs;
                }
                if(GraphName==TEXT("DOR_ConsumeManualIfUnchanged") &&
                   FunctionEntry->FindPin(TEXT("SlotName"))) ++SlotBoundConsumeInputs;
            }
            if(auto* Patch=Cast<UK2Node_SetFieldsInStruct>(N))
            {
                const bool Comparison=BP==Safety && G->GetFName()==TEXT("DOR_NormalizeCharactersForComparison");
                if(Comparison)
                {
                    // The exception is a private struct-copy reference plus an
                    // exact member set, never permission to patch live Vanity.
                    auto* Ref=FindPin(Patch,TEXT("StructRef"));
                    auto* Copy=Ref && Ref->LinkedTo.Num()==1?Cast<UK2Node_VariableGet>(Ref->LinkedTo[0]->GetOwningNode()):nullptr;
                    const bool PrivateCopy=Copy && Copy->VariableReference.GetMemberName().ToString().StartsWith(TEXT("DOR_NormalizeCharactersForComparison_StructCopy_"));
                    TSet<FName> Allowed;
                    if(Patch->StructType==FCharacterSave::StaticStruct())Allowed={TEXT("Vanity")};
                    else if(Patch->StructType==FCharacterVanitySave::StaticStruct())Allowed={TEXT("Loadouts"),TEXT("RandomVanityLoadout")};
                    else if(Patch->StructType==FCharacterVanityLoadout::StaticStruct())Allowed={TEXT("MedicalGownIndex")};
                    int32 Shown=0;for(const auto& P:Patch->ShowPinForProperties)if(P.bShowPin){++Shown;if(!Allowed.Contains(P.PropertyName))++ForbiddenCharacterFields;}
                    if(!PrivateCopy || Allowed.Num()==0 || Shown!=Allowed.Num())++ForbiddenCharacterFields;
                }
                if(Patch->StructType==FCharacterSave::StaticStruct())
                {
                    ++CharacterPatches;
                    const bool SourceEquipment=BP==Safety && (G->GetFName()==TEXT("DOR_SourceStage") || G->GetFName()==TEXT("DOR_SourceMergeCharacter"));
                    if(SourceEquipment)++SourceCharacterPatches;
                    if(!Comparison)for(const auto& P:Patch->ShowPinForProperties) if(P.bShowPin &&
                        P.PropertyName!=TEXT("ItemUpgradeLoadouts") && P.PropertyName!=TEXT("RandomItemUpgradeLoadouts") &&
                        !(SourceEquipment && (P.PropertyName==TEXT("Loadouts") || P.PropertyName==TEXT("RandomWeaponLoadout")))) ++ForbiddenCharacterFields;
                }
            }
            if(auto* Set=Cast<UK2Node_VariableSet>(N)) if(BP==Master &&
                G->GetFName()==TEXT("DOR_ApplyPair") &&
                Set->VariableReference.GetMemberName()==TEXT("TransientApplySucceeded"))
            {
                auto* Value=FindPin(Set,TEXT("TransientApplySucceeded"));
                if(Value && Value->DefaultValue==TEXT("true"))++AlreadySelectedSuccessSets;
            }
        }
    }
    const int32 RawPreparedRoutes=PreparedRoutes;
    PreparedRoutes-=NativeWindowPrepares+NativeLegacyFallbackPrepares;
    int32 AdmittedGF01=0;
    for(const FDORGF01PairSpec& Spec:GF01CandidateSpecs)
        if(DORIsSupportedPairTestIndex(Spec.TestIndex)) ++AdmittedGF01;
    PreparedRoutes += AdmittedGF01-1 + GF02Specs.Num()-1 + GF04Specs.Num()-1 + GF03Specs.Num() + RemainingSpecs.Num() + MigratedPreparedSpecs.Num();
    auto* ModInterface=LoadObject<UClass>(nullptr,TEXT("/Game/_ModHub/IHubMod.IHubMod_C"));
    auto* PageInterface=LoadObject<UClass>(nullptr,TEXT("/Game/_ModHub/IHubPageWidget.IHubPageWidget_C"));
    auto HasInterface=[](UBlueprint* BP,UClass* Expected)
    {return Expected && BP->ImplementedInterfaces.ContainsByPredicate([&](const FBPInterfaceDescription& I){return I.Interface==Expected;});};
    // Retain actual per-node write prohibitions and named ABI.
    // Whole-graph census and physical adjacency are not safety proofs.
    const bool Pass=ForbiddenWrites==0 &&
        UncheckedIO==0 && ForbiddenCharacterFields==0 && InvalidTimers==0 &&
        ManualTextDispatches==0 && RemoteDispatchPlayerInputs==1 && RemoteDispatchIndexInputs==0 &&
        DualSlotSavedFields==8 && DualSlotReaderGraphs==2 && DualSlotWriterInputs==4 &&
        DualSlotSetterInputs==2 && SlotBoundConsumeInputs==1 &&
        HasInterface(MenuHost,ModInterface) && HasInterface(Page,PageInterface);
    auto Report=MakeShared<FJsonObject>(); Report->SetNumberField(TEXT("schema"),2);
    Report->SetStringField(TEXT("build"),DORResearchBuildTag);
    Report->SetNumberField(TEXT("admittedRoutes"),DORSupportedPairTestIndices().Num());
    Report->SetNumberField(TEXT("gf01Members"),GF01CandidateSpecs.Num()+1);
    Report->SetNumberField(TEXT("gf02Members"),GF02Specs.Num());
    Report->SetNumberField(TEXT("gf04Members"),GF04Specs.Num());
    Report->SetNumberField(TEXT("gf03Members"),GF03Specs.Num());
    Report->SetNumberField(TEXT("remainingPreparedMembers"),RemainingSpecs.Num());
    Report->SetNumberField(TEXT("migratedPreparedMembers"),MigratedPreparedSpecs.Num());
    Report->SetStringField(TEXT("manifestHash"),ManifestHash);
    Report->SetStringField(TEXT("generatedAtUtc"),FDateTime::UtcNow().ToIso8601());
    Report->SetStringField(TEXT("generatorSource"),TEXT("DORGenerateAssetsCommandlet.cpp+DORModHub.inl"));
    Report->SetBoolField(TEXT("passed"),Pass); Report->SetNumberField(TEXT("forbiddenWrites"),ForbiddenWrites);
    Report->SetNumberField(TEXT("preparedRoutes"),PreparedRoutes); Report->SetNumberField(TEXT("nativeCommitWriters"),NativeCommitWriters);
    Report->SetNumberField(TEXT("legacyFallbackPrepares"),NativeLegacyFallbackPrepares);
    Report->SetNumberField(TEXT("uncheckedIO"),UncheckedIO); Report->SetNumberField(TEXT("characterPatches"),CharacterPatches);
    Report->SetNumberField(TEXT("sourceEquipmentCharacterPatches"),SourceCharacterPatches);
    Report->SetNumberField(TEXT("forbiddenCharacterFields"),ForbiddenCharacterFields); Report->SetNumberField(TEXT("invalidTimers"),InvalidTimers);
    Report->SetNumberField(TEXT("registrationPrepares"),RegistrationPrepares); Report->SetNumberField(TEXT("registrationCommits"),RegistrationCommits);
    Report->SetNumberField(TEXT("presetResumeChecks"),PresetResumeChecks); Report->SetNumberField(TEXT("invalidPresetResume"),InvalidPresetResume);
    Report->SetNumberField(TEXT("localPageControllerChecks"),LocalPageControllerChecks); Report->SetNumberField(TEXT("pageWidgetCreates"),PageWidgetCreates);
    Report->SetNumberField(TEXT("finalLoadoutIdentityGuids"),FinalLoadoutIdentityGuids); Report->SetNumberField(TEXT("finalWeaponIdentityChecks"),FinalWeaponIdentityChecks);
    Report->SetNumberField(TEXT("alreadySelectedSuccessSets"),AlreadySelectedSuccessSets);
    Report->SetNumberField(TEXT("manualLocalObjectDispatches"),ManualLocalObjectDispatches); Report->SetNumberField(TEXT("manualRemoteObjectDispatches"),ManualRemoteObjectDispatches);
    Report->SetNumberField(TEXT("manualTextDispatches"),ManualTextDispatches); Report->SetNumberField(TEXT("manualRemotePlayerLinks"),ManualRemotePlayerLinks);
    Report->SetNumberField(TEXT("remoteDispatchPlayerInputs"),RemoteDispatchPlayerInputs); Report->SetNumberField(TEXT("remoteDispatchIndexInputs"),RemoteDispatchIndexInputs);
    Report->SetNumberField(TEXT("dualSlotSavedFields"),DualSlotSavedFields); Report->SetNumberField(TEXT("dualSlotReaderGraphs"),DualSlotReaderGraphs);
    Report->SetNumberField(TEXT("dualSlotWriterInputs"),DualSlotWriterInputs); Report->SetNumberField(TEXT("dualSlotSetterInputs"),DualSlotSetterInputs);
    Report->SetNumberField(TEXT("slotBoundConsumeInputs"),SlotBoundConsumeInputs); Report->SetNumberField(TEXT("pageDualSlotSetCalls"),PageDualSlotSetCalls);
    Report->SetStringField(TEXT("scope"),TEXT("generated-editor-graphs; not a live-game save recovery test"));
    FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
    if(!FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("DORGraphAudit.json")))) return false;
    if(!Pass) {UE_LOG(LogTemp,Error,TEXT("DOR-SAFETY-AUDIT failed: %s"),*Json);}
    else {UE_LOG(LogTemp,Display,TEXT("DOR-SAFETY-AUDIT passed: %s"),*Json);}
    return Pass;
}
