// Local preferences are deliberately independent of player saves, reservations,
// and the per-save maintenance disable marker.
struct FDOROrdinaryGraph : FDORNativeReceiptGraph
{
    using FDORNativeReceiptGraph::FDORNativeReceiptGraph;
    using FDORNativeReceiptGraph::Cache;
    FEdGraphPinType ResultType=MakeBoolPinType();
    bool VoidResult=false;
    void Return(UEdGraphPin* V=nullptr,const FString& Default=TEXT("false"))
    {
        if(VoidResult){auto* R=AddNode<UK2Node_FunctionResult>(G,++Serial*160,0);R->FunctionReference=Entry->FunctionReference;R->AllocateDefaultPins();Connect(Tail,R->FindPin(UEdGraphSchema_K2::PN_Execute));Tail=nullptr;return;}
        if(V){ResultType=V->PinType;ResultType.bIsReference=false;ResultType.bIsConst=false;}
        auto* R=AddNode<UK2Node_FunctionResult>(G,++Serial*160,0);R->FunctionReference=Entry->FunctionReference;R->AllocateDefaultPins();
        if(!R->FindPin(TEXT("ReturnValue"))){auto* P=R->CreateUserDefinedPin(TEXT("ReturnValue"),ResultType,EGPD_Input);const FName Old=P->PinName;P->PinName=TEXT("ReturnValue");for(auto& I:R->UserDefinedPins)if(I->PinName==Old)I->PinName=TEXT("ReturnValue");}
        for(auto* Node:G->Nodes)if(auto* Result=::Cast<UK2Node_FunctionResult>(Node))if(auto* P=Result->FindPin(TEXT("ReturnValue"))){ForcePinType(P,ResultType);for(auto& I:Result->UserDefinedPins)if(I->PinName==TEXT("ReturnValue"))I->PinType=ResultType;if(ResultType.PinCategory==UEdGraphSchema_K2::PC_Object)P->DefaultValue=TEXT("");else if(ResultType.PinCategory==UEdGraphSchema_K2::PC_Int&&P->DefaultValue==TEXT("false"))P->DefaultValue=TEXT("0");}
        auto* P=R->FindPin(TEXT("ReturnValue"));if(V)Connect(V,P);else P->DefaultValue=ResultType.PinCategory==UEdGraphSchema_K2::PC_Object?TEXT(""):ResultType.PinCategory==UEdGraphSchema_K2::PC_Int&&Default==TEXT("false")?TEXT("0"):Default;
        Connect(Tail,R->FindPin(UEdGraphSchema_K2::PN_Execute));Tail=nullptr;
    }
    void QuietRequire(UEdGraphPin* Condition){auto* B=Branch(Condition);Tail=B->GetElsePin();Return();Tail=B->GetThenPin();}
    void Done(){Return(nullptr,TEXT("true"));}
    UEdGraphPin* Cache(const TCHAR* Label,UEdGraphPin*,const TCHAR*,const FEdGraphPinType& Type)
    {const FName N(*FString::Printf(TEXT("%s_%s_%d"),*G->GetName(),Label,++Serial));AddTypedVariable(BP,N,Type);Set(N,nullptr);return Get(N);}
    UEdGraphPin* Read(UClass* C,const TCHAR* N,UEdGraphPin* O)
    {auto* V=Call(C,N);Connect(O,FindPin(V,UEdGraphSchema_K2::PN_Self));return V->GetReturnValuePin();}
    UEdGraphPin* Math(const TCHAR* N,UEdGraphPin* A,UEdGraphPin* B)
    {auto* V=Call(UKismetMathLibrary::StaticClass(),N);Connect(A,FindPin(V,TEXT("A")));Connect(B,FindPin(V,TEXT("B")));return V->GetReturnValuePin();}
    UEdGraphPin* Or(UEdGraphPin* A,UEdGraphPin* B){return Math(TEXT("BooleanOR"),A,B);}
    UEdGraphPin* PickBool(UEdGraphPin* A,UEdGraphPin* B,UEdGraphPin* PickA){return Or(And(PickA,A),And(Not(PickA),B));}
    UEdGraphPin* Greater(UEdGraphPin* A,UEdGraphPin* B){return Math(TEXT("Greater_IntInt"),A,B);}
    UEdGraphPin* At(UEdGraphPin* A,UEdGraphPin* I)
    {auto* V=Array(TEXT("Array_Get"),A);Connect(I,FindPin(V,TEXT("Index")));return FindPin(V,TEXT("Item"));}
    void Append(UEdGraphPin* A,UEdGraphPin* V)
    {auto* N=Array(TEXT("Array_Add"),A);if(V)Connect(V,FindPin(N,TEXT("NewItem")));}
    UEdGraphPin* Contains(UEdGraphPin* A,UEdGraphPin* V)
    {auto* N=Array(TEXT("Array_Contains"),A);Connect(V,FindPin(N,TEXT("ItemToFind")));return N->GetReturnValuePin();}
    UEdGraphPin* Find(UEdGraphPin* A,UEdGraphPin* V)
    {auto* N=Array(TEXT("Array_Find"),A);Connect(V,FindPin(N,TEXT("ItemToFind")));return N->GetReturnValuePin();}
    UEdGraphPin* Count(UEdGraphPin* A){return Array(TEXT("Array_Length"),A)->GetReturnValuePin();}
    UEdGraphPin* Path(UEdGraphPin* V){auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("GetPathName"));Connect(V,FindPin(N,TEXT("Object")));return N->GetReturnValuePin();}
    UEdGraphPin* New(UClass* C)
    {auto* N=AddNode<UK2Node_GenericCreateObject>(G,0,0);N->AllocateDefaultPins();FindPin(N,TEXT("Class"))->DefaultObject=C;N->ReconstructNode();Connect(SelfPin(G,0,0),FindPin(N,TEXT("Outer")));Run(N);return FindPin(N,UEdGraphSchema_K2::PN_ReturnValue);}
    void Note(const TCHAR* Stage,UEdGraphPin* Key=nullptr)
    {FDORNativeProbeLine D(*this,TEXT("Ordinary"),TEXT("[DOR-ORDINARY]"));D.Add(TEXT("stage"),Str(Stage),NAME_None);if(Key)D.Add(TEXT("key"),Key,NAME_None);if(BP->GeneratedClass==MasterClass)D.Record();else{auto* Find=Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(Find,TEXT("ActorClass"))->DefaultObject=MasterClass;auto* M=Cache(TEXT("DiagnosticMaster"),Cast(Find->GetReturnValuePin(),MasterClass));auto* Have=Branch(IsValid(M));auto* Record=Call(MasterClass,TEXT("DOR_DiagnosticRecordOnly"));Connect(M,FindPin(Record,UEdGraphSchema_K2::PN_Self));Connect(D.Line,FindPin(Record,TEXT("Message")));auto* Good=Tail;Tail=Have->GetElsePin();auto* Log=Call(UKismetSystemLibrary::StaticClass(),TEXT("PrintString"));Connect(D.Line,FindPin(Log,TEXT("InString")));FindPin(Log,TEXT("bPrintToScreen"))->DefaultValue=TEXT("false");Merge(Good);}}
};

UK2Node_CallFunction* DOROrdinaryInsert(UEdGraphPin* Out,UClass* C,const TCHAR* Name,UEdGraphPin* Target=nullptr)
{
    auto* G=Out->GetOwningNode()->GetGraph();auto Next=Out->LinkedTo;Out->BreakAllPinLinks();
    auto* Call=C?AddCall(G,C,Name,0,0):AddSelfCall(G,Name,0,0);
    if(Target)Connect(Target,FindPin(Call,UEdGraphSchema_K2::PN_Self));Connect(Out,Call->GetExecPin());for(auto* P:Next)Connect(Call->GetThenPin(),P);return Call;
}

bool BuildDOROrdinarySettings(UBlueprint* Master,UBlueprint* MenuHost,UWidgetBlueprint* Page,UBlueprint*& Preferences)
{
    Preferences=CreateObjectBlueprint(FString(RuntimeRoot)/TEXT("Blueprints/SG_DOR_LocalPreferences"),TEXT("SG_DOR_LocalPreferences"),USaveGame::StaticClass());
    if(!Preferences)return false;
    for(const TCHAR* N:{TEXT("Enabled"),TEXT("Rewards"),TEXT("Reward2"),TEXT("Reward2Explicit"),TEXT("Wave"),TEXT("DeepDive"),TEXT("HideHighLoad"),TEXT("DetailedDiagnostics")})AddSavedTypedVariable(Preferences,N,MakeBoolPinType());
    for(const TCHAR* N:{TEXT("Progress1"),TEXT("Progress2"),TEXT("DDStage1"),TEXT("DDStage2")})AddSavedTypedVariable(Preferences,N,MakeIntPinType());
    FKismetEditorUtilities::CompileBlueprint(Preferences);if(Preferences->Status==BS_Error)return false;
    auto* Defaults=Preferences->GeneratedClass->GetDefaultObject();
    FindFProperty<FBoolProperty>(Preferences->GeneratedClass,TEXT("DetailedDiagnostics"))->SetPropertyValue_InContainer(Defaults,false);
    for(const TCHAR* N:{TEXT("Enabled"),TEXT("Rewards"),TEXT("Wave")})FindFProperty<FBoolProperty>(Preferences->GeneratedClass,N)->SetPropertyValue_InContainer(Defaults,true);
    for(const TCHAR* N:{TEXT("Progress1"),TEXT("Progress2")})FindFProperty<FIntProperty>(Preferences->GeneratedClass,N)->SetPropertyValue_InContainer(Defaults,100);
    FindFProperty<FIntProperty>(Preferences->GeneratedClass,TEXT("DDStage1"))->SetPropertyValue_InContainer(Defaults,2);
    FindFProperty<FIntProperty>(Preferences->GeneratedClass,TEXT("DDStage2"))->SetPropertyValue_InContainer(Defaults,3);
    for(auto* BP:{Master,MenuHost})AddTypedVariable(BP,TEXT("LocalPreferences"),MakeObjectPinType(Preferences->GeneratedClass));
    AddTypedVariable(Page,TEXT("PreferencesRefreshing"),MakeBoolPinType());
    AddTypedVariable(Master,TEXT("OrdinarySnapshotReady"),MakeBoolPinType());
    for(const TCHAR* N:{TEXT("ActiveModEnabled"),TEXT("ActiveRewards"),TEXT("ActiveReward2"),TEXT("ActiveWave"),TEXT("ActiveHideHighLoad"),TEXT("ManualRewardBlocked")})AddTypedVariable(Master,N,MakeBoolPinType());
    for(const TCHAR* N:{TEXT("ActiveProgress1"),TEXT("ActiveProgress2")})AddTypedVariable(Master,N,MakeIntPinType());
    for(auto* BP:{Master,MenuHost,static_cast<UBlueprint*>(Page)}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);}
    for(auto* BP:{Master,MenuHost})
    {
        FDOROrdinaryGraph F(BP,TEXT("DOR_LoadPreferences"),Master->GeneratedClass);
        auto* Have=F.Branch(F.IsValid(F.Get(TEXT("LocalPreferences"))));F.Return(nullptr,TEXT("true"));F.Tail=Have->GetElsePin();
        auto* Exists=F.Call(UGameplayStatics::StaticClass(),TEXT("DoesSaveGameExist"));FindPin(Exists,TEXT("SlotName"))->DefaultValue=TEXT("Mods/DOR_LocalPreferences");
        auto* Saved=F.Branch(Exists->GetReturnValuePin());auto* Load=F.Call(UGameplayStatics::StaticClass(),TEXT("LoadGameFromSlot"));FindPin(Load,TEXT("SlotName"))->DefaultValue=TEXT("Mods/DOR_LocalPreferences");
        F.Set(TEXT("LocalPreferences"),F.Cast(Load->GetReturnValuePin(),Preferences->GeneratedClass));F.Return(F.IsValid(F.Get(TEXT("LocalPreferences"))));F.Tail=Saved->GetElsePin();
        auto* Create=F.Call(UGameplayStatics::StaticClass(),TEXT("CreateSaveGameObject"));FindPin(Create,TEXT("SaveGameClass"))->DefaultObject=Preferences->GeneratedClass;F.Set(TEXT("LocalPreferences"),F.Cast(Create->GetReturnValuePin(),Preferences->GeneratedClass));F.Return(F.IsValid(F.Get(TEXT("LocalPreferences"))));F.Compile();
    }
    // Read only the local cached object. Optional producers never load or search.
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_DetailedDiagnosticsEnabled"),Master->GeneratedClass);
        auto* P=F.Get(TEXT("LocalPreferences"));F.QuietRequire(F.IsValid(P));
        F.Return(F.Field(P,Preferences->GeneratedClass,TEXT("DetailedDiagnostics")));F.Compile();
    }
    {
        FDOROrdinaryGraph F(MenuHost,TEXT("DOR_SavePreferences"),Master->GeneratedClass);F.QuietRequire(F.IsValid(F.Get(TEXT("LocalPreferences"))));auto* Save=F.Call(UGameplayStatics::StaticClass(),TEXT("SaveGameToSlot"));Connect(F.Get(TEXT("LocalPreferences")),FindPin(Save,TEXT("SaveGameObject")));FindPin(Save,TEXT("SlotName"))->DefaultValue=TEXT("Mods/DOR_LocalPreferences");
        auto* Result=F.Cache(TEXT("Saved"),Save->GetReturnValuePin());auto* Failed=F.Branch(F.Not(Result));F.Note(TEXT("preferences-save-failed"));F.Merge(Failed->GetElsePin());F.Return(Result);F.Compile();
    }
    // Cache desired settings before the cave's readiness waits. UI edits
    // change the separate menu object and take effect in the next cave.
    for(auto* N:TArray<UEdGraphNode*>(FBlueprintEditorUtils::FindEventGraph(Master)->Nodes))if(auto* E=Cast<UK2Node_Event>(N))if(E->EventReference.GetMemberName()==TEXT("ReceiveBeginPlay"))if(E->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num()>0)DOROrdinaryInsert(E->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_LoadPreferences"));
    {
        FDOROrdinaryGraph F(Master,TEXT("DOR_SnapshotOrdinarySettings"),Master->GeneratedClass);auto* Done=F.Branch(F.Get(TEXT("OrdinarySnapshotReady")));F.Return(nullptr,TEXT("true"));F.Tail=Done->GetElsePin();auto* Load=F.Self(TEXT("DOR_LoadPreferences"));F.QuietRequire(Load->GetReturnValuePin());
        auto* P=F.Get(TEXT("LocalPreferences"));for(const auto& N:{TPair<const TCHAR*,const TCHAR*>(TEXT("Enabled"),TEXT("ActiveModEnabled")),TPair<const TCHAR*,const TCHAR*>(TEXT("Rewards"),TEXT("ActiveRewards")),TPair<const TCHAR*,const TCHAR*>(TEXT("Wave"),TEXT("ActiveWave")),TPair<const TCHAR*,const TCHAR*>(TEXT("HideHighLoad"),TEXT("ActiveHideHighLoad"))})F.Set(N.Value,F.Field(P,Preferences->GeneratedClass,N.Key));
        for(int32 I=1;I<=2;++I){auto* Clamp=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Clamp"));Connect(F.Field(P,Preferences->GeneratedClass,*FString::Printf(TEXT("Progress%d"),I)),FindPin(Clamp,TEXT("Value")));FindPin(Clamp,TEXT("Max"))->DefaultValue=TEXT("100");F.Set(*FString::Printf(TEXT("ActiveProgress%d"),I),Clamp->GetReturnValuePin());}
        auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));F.QuietRequire(F.IsValid(GS->GetReturnValuePin()));F.QuietRequire(F.Not(F.Field(GS->GetReturnValuePin(),AFSDGameState::StaticClass(),TEXT("IsOnSpaceRig"))));F.QuietRequire(F.Read(AGameState::StaticClass(),TEXT("HasMatchStarted"),GS->GetReturnValuePin()));auto* Objectives=F.Read(AFSDGameState::StaticClass(),TEXT("GetSecondaryObjectives"),GS->GetReturnValuePin());auto* Empty=F.Branch(F.Eq(F.Count(Objectives),F.Int(0)));auto* GI=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameInstance"));auto* Mission=F.Read(UFSDGameInstance::StaticClass(),TEXT("GetGeneratedMission"),GI->GetReturnValuePin());F.QuietRequire(F.IsValid(Mission));F.QuietRequire(F.Read(UGeneratedMission::StaticClass(),TEXT("IsDeepDive"),Mission));F.Merge(Empty->GetElsePin());auto* Multiple=F.Greater(F.Count(Objectives),F.Int(1));
        F.Set(TEXT("ActiveReward2"),F.PickBool(F.Field(P,Preferences->GeneratedClass,TEXT("Reward2")),Multiple,F.Field(P,Preferences->GeneratedClass,TEXT("Reward2Explicit"))));
        F.Set(TEXT("ActiveProgress2"),F.Math(TEXT("Max"),F.Get(TEXT("ActiveProgress1")),F.Get(TEXT("ActiveProgress2"))));F.Set(TEXT("OrdinarySnapshotReady"),nullptr,TEXT("true"));
        FDORNativeProbeLine D(F,TEXT("Snapshot"),TEXT("[DOR-ORDINARY]|stage=mission-settings-snapshot"));for(const TCHAR* N:{TEXT("ActiveModEnabled"),TEXT("ActiveRewards"),TEXT("ActiveReward2"),TEXT("ActiveWave")})D.Add(N,F.Get(N));for(const TCHAR* N:{TEXT("ActiveProgress1"),TEXT("ActiveProgress2")})D.Add(N,F.Get(N),TEXT("Conv_IntToString"));D.Add(TEXT("twoObjectives"),Multiple);D.Add(TEXT("explicitReward2"),F.Field(P,Preferences->GeneratedClass,TEXT("Reward2Explicit")));D.Record();F.Return(nullptr,TEXT("true"));F.Compile();
    }
    auto* Root=Cast<UVerticalBox>(Page->WidgetTree->FindWidget(TEXT("Root")));auto* Advanced=Cast<UVerticalBox>(Page->WidgetTree->FindWidget(TEXT("AdvancedRoot")));auto* Debug=Cast<UVerticalBox>(Page->WidgetTree->FindWidget(TEXT("DebugRoot")));if(!Root||!Advanced||!Debug)return false;
    auto Check=[&](UVerticalBox* Parent,const TCHAR* Name,const TCHAR* Label,const TCHAR* Tip){auto* C=MakeWidget<UCheckBox>(Page->WidgetTree,Name);Parent->AddChildToVerticalBox(C)->SetPadding(FMargin(10,5));auto* T=MakeWidget<UTextBlock>(Page->WidgetTree,FName(*(FString(Name)+TEXT("Label"))));T->SetText(FText::FromString(Label));if(FString(Name)==TEXT("PreferenceDeepDive"))T->SetAutoWrapText(true);C->AddChild(T);C->SetToolTipText(FText::FromString(Tip));return C;};
    Check(Root,TEXT("PreferenceEnabled"),TEXT("启用Mod"),TEXT("仅空间站可更改。"));Check(Root,TEXT("PreferenceRewards"),TEXT("次要任务抽卡奖励"),TEXT("次要目标达到设定进度时，全队抽取奖励。"));
    for(auto* W:{Page->WidgetTree->FindWidget(TEXT("PreferenceRewards")),Page->WidgetTree->FindWidget(TEXT("PreferenceEnabled"))})Root->ShiftChild(0,W);
    auto* Hint=MakeWidget<UTextBlock>(Page->WidgetTree,TEXT("HostSettingsHint"));Hint->SetText(FText::FromString(TEXT("其余设置仅在你作为房主时生效")));Root->AddChildToVerticalBox(Hint)->SetPadding(FMargin(10,5));Root->ShiftChild(1,Hint);
    Check(Debug,TEXT("PreferenceDetailedDiagnostics"),TEXT("详细诊断"),TEXT("启用额外诊断采集，详细记录写入日志文件。"));
    Check(Advanced,TEXT("PreferenceHideHighLoad"),TEXT("隐藏高负担组合"),TEXT("隐藏可能带来较高性能负担的融合组合。组合名单由AI按机制特征筛选，欢迎前往GitHub提交调整意见。"));
    Check(Advanced,TEXT("PreferenceReward2"),TEXT("启用二次奖励"),TEXT("开启第二次抽卡奖励。"));Check(Advanced,TEXT("PreferenceWave"),TEXT("领奖后触发虫潮"),TEXT("全队领取奖励后，呼叫额外虫潮。"));
    Check(Root,TEXT("PreferenceDeepDive"),TEXT("深潜肉鸽模式 / Deep Dive Roguelike"),TEXT("在指定深潜层数获得融合抽卡。默认第2、3层。\n想提高深潜难度，可搭配CD2自行调整。\nGrants fusion drafts on the selected Deep Dive stages. Defaults to stages 2 and 3.\nFor a tougher Deep Dive, adjust the difficulty with CD2."));
    for(int32 I=1;I<=2;++I){auto* T=MakeWidget<UTextBlock>(Page->WidgetTree,FName(*FString::Printf(TEXT("DDStage%dLabel"),I)));T->SetText(FText::FromString(I==1?TEXT("首次深潜奖励层数 / Deep Dive Reward 1 Stage"):TEXT("二次深潜奖励层数 / Deep Dive Reward 2 Stage")));T->SetAutoWrapText(true);Advanced->AddChildToVerticalBox(T)->SetPadding(FMargin(10,5));auto* S=MakeWidget<USpinBox>(Page->WidgetTree,FName(*FString::Printf(TEXT("PreferenceDDStage%d"),I)));S->SetMinValue(1);S->SetMaxValue(3);S->SetMinSliderValue(1);S->SetMaxSliderValue(3);S->SetDelta(1);S->SetMinFractionalDigits(0);S->SetMaxFractionalDigits(0);S->SetToolTipText(FText::FromString(I==1?TEXT("设置首次深潜融合抽卡的层数。\nSets the stage for the first Deep Dive fusion draft."):TEXT("设置二次深潜融合抽卡的层数。\nSets the stage for the second Deep Dive fusion draft.")));Advanced->AddChildToVerticalBox(S)->SetPadding(FMargin(10,5));}
    for(int32 I=1;I<=2;++I){auto* T=MakeWidget<UTextBlock>(Page->WidgetTree,FName(*FString::Printf(TEXT("Progress%dLabel"),I)));T->SetText(FText::FromString(I==1?TEXT("首次奖励进度 (%)"):TEXT("二次奖励进度 (%)")));Advanced->AddChildToVerticalBox(T)->SetPadding(FMargin(10,5));auto* S=MakeWidget<USpinBox>(Page->WidgetTree,FName(*FString::Printf(TEXT("PreferenceProgress%d"),I)));S->SetMinValue(0);S->SetMaxValue(100);S->SetMinSliderValue(0);S->SetMaxSliderValue(100);S->SetDelta(1);S->SetMinFractionalDigits(0);S->SetMaxFractionalDigits(0);S->SetToolTipText(FText::FromString(I==1?TEXT("设置首次奖励进度。0%为开门抽卡。"):TEXT("设置第二次奖励进度。")));Advanced->AddChildToVerticalBox(S)->SetPadding(FMargin(10,5));}
    FKismetEditorUtilities::CompileBlueprint(Page);

    {
        FDOROrdinaryGraph F(Page,TEXT("DOR_PreferencesTwoObjectives"),Master->GeneratedClass);auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));F.QuietRequire(F.IsValid(GS->GetReturnValuePin()));F.Return(F.Greater(F.Count(F.Read(AFSDGameState::StaticClass(),TEXT("GetSecondaryObjectives"),GS->GetReturnValuePin())),F.Int(1)));F.Compile();
    }
    {
        FDOROrdinaryGraph F(Page,TEXT("DOR_RefreshPreferences"),Master->GeneratedClass);F.QuietRequire(F.IsValid(F.Get(TEXT("Host"))));auto* Load=F.Call(MenuHost->GeneratedClass,TEXT("DOR_LoadPreferences"));Connect(F.Get(TEXT("Host")),FindPin(Load,UEdGraphSchema_K2::PN_Self));F.QuietRequire(Load->GetReturnValuePin());F.Set(TEXT("PreferencesRefreshing"),nullptr,TEXT("true"));auto* P=F.Field(F.Get(TEXT("Host")),MenuHost->GeneratedClass,TEXT("LocalPreferences"));
        for(const TCHAR* N:{TEXT("Enabled"),TEXT("Rewards"),TEXT("Reward2"),TEXT("Wave"),TEXT("DeepDive"),TEXT("HideHighLoad"),TEXT("DetailedDiagnostics")}){UEdGraphPin* Value=F.Field(P,Preferences->GeneratedClass,N);if(FString(N)==TEXT("Reward2")){auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));auto* Multiple=F.Cache(TEXT("Multiple"),F.Eq(F.Int(0),F.Int(1)));auto* Have=F.Branch(F.IsValid(GS->GetReturnValuePin()));F.Set(F.Variable(Multiple),F.Greater(F.Count(F.Read(AFSDGameState::StaticClass(),TEXT("GetSecondaryObjectives"),GS->GetReturnValuePin())),F.Int(1)));F.Merge(Have->GetElsePin());Value=F.PickBool(Value,Multiple,F.Field(P,Preferences->GeneratedClass,TEXT("Reward2Explicit")));}auto* Set=F.Call(UCheckBox::StaticClass(),TEXT("SetIsChecked"));Connect(F.Get(*FString::Printf(TEXT("Preference%s"),N)),FindPin(Set,UEdGraphSchema_K2::PN_Self));Connect(Value,FindPin(Set,TEXT("InIsChecked")));}
        auto* Float=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Conv_IntToFloat"));Connect(F.Field(P,Preferences->GeneratedClass,TEXT("Progress1")),FindPin(Float,TEXT("InInt")));for(const TCHAR* Name:{TEXT("SetMinValue"),TEXT("SetMinSliderValue")}){auto* Set=F.Call(USpinBox::StaticClass(),Name);Connect(F.Get(TEXT("PreferenceProgress2")),FindPin(Set,UEdGraphSchema_K2::PN_Self));Connect(Float->GetReturnValuePin(),FindPin(Set,TEXT("NewValue")));}
        for(int32 I=1;I<=2;++I){auto* Set=F.Call(USpinBox::StaticClass(),TEXT("SetValue"));Connect(F.Get(*FString::Printf(TEXT("PreferenceProgress%d"),I)),FindPin(Set,UEdGraphSchema_K2::PN_Self));auto* Convert=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Conv_IntToFloat"));Connect(F.Field(P,Preferences->GeneratedClass,*FString::Printf(TEXT("Progress%d"),I)),FindPin(Convert,TEXT("InInt")));Connect(Convert->GetReturnValuePin(),FindPin(Set,TEXT("NewValue")));}
        for(int32 I=1;I<=2;++I){auto* Set=F.Call(USpinBox::StaticClass(),TEXT("SetValue"));Connect(F.Get(*FString::Printf(TEXT("PreferenceDDStage%d"),I)),FindPin(Set,UEdGraphSchema_K2::PN_Self));auto* Convert=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Conv_IntToFloat"));Connect(F.Field(P,Preferences->GeneratedClass,*FString::Printf(TEXT("DDStage%d"),I)),FindPin(Convert,TEXT("InInt")));Connect(Convert->GetReturnValuePin(),FindPin(Set,TEXT("NewValue")));}auto* Minimum=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Conv_IntToFloat"));Connect(F.Field(P,Preferences->GeneratedClass,TEXT("DDStage1")),FindPin(Minimum,TEXT("InInt")));for(const TCHAR* Name:{TEXT("SetMinValue"),TEXT("SetMinSliderValue")}){auto* Set=F.Call(USpinBox::StaticClass(),Name);Connect(F.Get(TEXT("PreferenceDDStage2")),FindPin(Set,UEdGraphSchema_K2::PN_Self));Connect(Minimum->GetReturnValuePin(),FindPin(Set,TEXT("NewValue")));}
        for(const TCHAR* N:{TEXT("Enabled"),TEXT("Rewards"),TEXT("Reward2"),TEXT("Wave"),TEXT("Progress1"),TEXT("Progress2"),TEXT("DeepDive"),TEXT("HideHighLoad"),TEXT("DDStage1"),TEXT("DDStage2")}){auto* Set=F.Call(UWidget::StaticClass(),TEXT("SetIsEnabled"));Connect(F.Get(*FString::Printf(TEXT("Preference%s"),N)),FindPin(Set,UEdGraphSchema_K2::PN_Self));UEdGraphPin* Editable=DORLocalHost(F);if(FString(N)==TEXT("Enabled")){auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));Editable=F.And(F.IsValid(GS->GetReturnValuePin()),F.Field(GS->GetReturnValuePin(),AFSDGameState::StaticClass(),TEXT("IsOnSpaceRig")));}Connect(Editable,FindPin(Set,TEXT("bInIsEnabled")));}F.Set(TEXT("PreferencesRefreshing"),nullptr,TEXT("false"));F.Done();F.Compile();
    }
    auto* EG=FBlueprintEditorUtils::FindEventGraph(Page);UEdGraphPin* ConstructTail=nullptr;
    for(auto* N:TArray<UEdGraphNode*>(EG->Nodes))if(auto* E=Cast<UK2Node_Event>(N))if(E->EventReference.GetMemberName()==TEXT("Construct")||E->EventReference.GetMemberName()==TEXT("HubPageOpened")){if(E->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num()==0)continue;auto* C=DOROrdinaryInsert(E->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_RefreshPreferences"));if(E->EventReference.GetMemberName()==TEXT("Construct"))ConstructTail=C->GetThenPin();}
    if(!ConstructTail)return false;
    for(const TCHAR* N:{TEXT("Enabled"),TEXT("Rewards"),TEXT("Reward2"),TEXT("Wave"),TEXT("Progress1"),TEXT("Progress2"),TEXT("DeepDive"),TEXT("HideHighLoad"),TEXT("DDStage1"),TEXT("DDStage2"),TEXT("DetailedDiagnostics")})
    {
        const bool Numeric=FString(N).StartsWith(TEXT("Progress"))||FString(N).StartsWith(TEXT("DDStage"));const bool DD=FString(N).StartsWith(TEXT("DDStage"));const FString Fn=FString(TEXT("DOR_ChangePreference"))+N;
        {FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Float;FDOROrdinaryGraph F(Page,*Fn,Master->GeneratedClass);auto* V=F.Input(TEXT("Value"),Numeric?FloatType:MakeBoolPinType());F.QuietRequire(F.Not(F.Get(TEXT("PreferencesRefreshing"))));if(FString(N)==TEXT("Enabled")){auto* GS=F.Call(UGameFunctionLibrary::StaticClass(),TEXT("GetFSDGameState"));F.QuietRequire(F.IsValid(GS->GetReturnValuePin()));F.QuietRequire(F.Field(GS->GetReturnValuePin(),AFSDGameState::StaticClass(),TEXT("IsOnSpaceRig")));}if(FString(N)!=TEXT("Enabled")&&FString(N)!=TEXT("DetailedDiagnostics"))F.QuietRequire(DORLocalHost(F));F.QuietRequire(F.IsValid(F.Get(TEXT("Host"))));auto* P=F.Field(F.Get(TEXT("Host")),MenuHost->GeneratedClass,TEXT("LocalPreferences"));F.QuietRequire(F.IsValid(P));UEdGraphPin* Value=V;if(Numeric){auto* Round=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Round"));Connect(V,FindPin(Round,TEXT("A")));auto* Clamp=F.Call(UKismetMathLibrary::StaticClass(),TEXT("Clamp"));Connect(Round->GetReturnValuePin(),FindPin(Clamp,TEXT("Value")));FindPin(Clamp,TEXT("Min"))->DefaultValue=DD?TEXT("1"):TEXT("0");FindPin(Clamp,TEXT("Max"))->DefaultValue=DD?TEXT("3"):TEXT("100");Value=Clamp->GetReturnValuePin();}F.Put(P,Preferences->GeneratedClass,N,Value);if(Numeric){const TCHAR* A=DD?TEXT("DDStage1"):TEXT("Progress1");const TCHAR* B=DD?TEXT("DDStage2"):TEXT("Progress2");F.Put(P,Preferences->GeneratedClass,B,F.Math(TEXT("Max"),F.Field(P,Preferences->GeneratedClass,A),F.Field(P,Preferences->GeneratedClass,B)));}if(FString(N)==TEXT("Reward2"))F.Put(P,Preferences->GeneratedClass,TEXT("Reward2Explicit"),nullptr,TEXT("true"));auto* Save=F.Call(MenuHost->GeneratedClass,TEXT("DOR_SavePreferences"));Connect(F.Get(TEXT("Host")),FindPin(Save,UEdGraphSchema_K2::PN_Self));F.Self(TEXT("DOR_RefreshPreferences"));F.Done();F.Compile();}
        UClass* C=Numeric?USpinBox::StaticClass():UCheckBox::StaticClass();auto* Prop=FindFProperty<FMulticastDelegateProperty>(C,Numeric?TEXT("OnValueCommitted"):TEXT("OnCheckStateChanged"));if(!Prop)return false;TSet<UEdGraphNode*> Before;for(auto* Node:EG->Nodes)Before.Add(Node);auto* Bind=AddNode<UK2Node_AssignDelegate>(EG,0,0);Bind->SetFromProperty(Prop,false,C);Bind->AllocateDefaultPins();Bind->PostPlacedNewNode();UK2Node_CustomEvent* Event=nullptr;for(auto* Node:EG->Nodes)if(!Before.Contains(Node))if(auto* E=Cast<UK2Node_CustomEvent>(Node))Event=E;if(!Event)return false;Event->CustomFunctionName=*FString::Printf(TEXT("Changed_Preference%s"),N);Event->ReconstructNode();Connect(AddVariableGet(EG,*FString::Printf(TEXT("Preference%s"),N),0,0)->GetValuePin(),FindPin(Bind,UEdGraphSchema_K2::PN_Self));auto Next=ConstructTail->LinkedTo;ConstructTail->BreakAllPinLinks();Connect(ConstructTail,Bind->GetExecPin());ConstructTail=FindPin(Bind,UEdGraphSchema_K2::PN_Then);for(auto* P:Next)Connect(ConstructTail,P);auto* Change=AddSelfCall(EG,*Fn,0,0);Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then),Change->GetExecPin());Connect(Event->FindPin(Numeric?TEXT("InValue"):TEXT("bIsChecked")),FindPin(Change,TEXT("Value")));
    }
    for(auto* BP:{Master,MenuHost,static_cast<UBlueprint*>(Page)}){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;}
    return SaveBlueprint(Preferences);
}

// Finalize only this local-preference handler once owned observer cleanup exists.
bool BuildDORDetailedDiagnosticsChange(UBlueprint* Master,UBlueprint* MenuHost,UWidgetBlueprint* Page,UBlueprint* Preferences)
{
    FDOROrdinaryGraph F(Page,TEXT("DOR_ChangePreferenceDetailedDiagnostics"),Master->GeneratedClass);
    auto* Value=F.Input(TEXT("Value"),MakeBoolPinType());
    F.QuietRequire(F.Not(F.Get(TEXT("PreferencesRefreshing"))));F.QuietRequire(F.IsValid(F.Get(TEXT("Host"))));
    auto* P=F.Field(F.Get(TEXT("Host")),MenuHost->GeneratedClass,TEXT("LocalPreferences"));F.QuietRequire(F.IsValid(P));
    F.Put(P,Preferences->GeneratedClass,TEXT("DetailedDiagnostics"),Value);
    // One explicit UI-action lookup; never create an actor for this preference.
    auto* Find=F.Call(UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"));FindPin(Find,TEXT("ActorClass"))->DefaultObject=Master->GeneratedClass;
    auto* M=F.Cache(TEXT("Master"),F.Cast(Find->GetReturnValuePin(),Master->GeneratedClass));auto* HaveMaster=F.Branch(F.IsValid(M));
    // Existing load is idempotent and preserves all other desired gameplay fields.
    // A failed load remains fail-closed and must still reach OFF cleanup.
    auto* Load=F.Call(Master->GeneratedClass,TEXT("DOR_LoadPreferences"));Connect(M,FindPin(Load,UEdGraphSchema_K2::PN_Self));
    auto* Live=F.Field(M,Master->GeneratedClass,TEXT("LocalPreferences"));auto* HavePreferences=F.Branch(F.IsValid(Live));
    F.Put(Live,Preferences->GeneratedClass,TEXT("DetailedDiagnostics"),Value);F.Merge(HavePreferences->GetElsePin());
    auto* Off=F.Branch(F.Not(Value));auto* Clear=F.Call(Master->GeneratedClass,TEXT("DOR_SludgeClear"));Connect(M,FindPin(Clear,UEdGraphSchema_K2::PN_Self));
    F.Merge(Off->GetElsePin());F.Merge(HaveMaster->GetElsePin());
    // Persist after the live change and cleanup. Save failure never re-enables it.
    auto* Save=F.Call(MenuHost->GeneratedClass,TEXT("DOR_SavePreferences"));Connect(F.Get(TEXT("Host")),FindPin(Save,UEdGraphSchema_K2::PN_Self));
    F.Self(TEXT("DOR_RefreshPreferences"));F.Done();F.Compile();return Page->Status!=BS_Error;
}
