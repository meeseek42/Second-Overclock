// Native FText histories resolve on each receiver before the ordinary chat
// history append. Registration is finite, peer-local, and independent of saves.
const TCHAR* DOR154NoticeNamespace=TEXT("DOR.FusionNotice.v1");
TMap<FString,FString> DOR154NoticeSources;
FText DOR154MakeNoticeText(const FString& Key){return FText::ChangeKey(DOR154NoticeNamespace,Key,FText::FromString(DOR154NoticeSources.FindChecked(Key)));}
UEdGraphPin* DOR154NoticeText(FDOROrdinaryGraph& F,const FString& Key)
{
    auto* Literal=F.Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralText"));
    FindPin(Literal,TEXT("Value"))->DefaultTextValue=DOR154MakeNoticeText(Key);
    return Literal->GetReturnValuePin();
}
bool BuildDOR154NoticeRegistration(UBlueprint* Cave,UBlueprint* Rig,const FString& ManifestFilename)
{
    FString Json;TSharedPtr<FJsonObject> Data;if(!FFileHelper::LoadFileToString(Json,*(FPaths::GetPath(ManifestFilename)/TEXT("display-overrides.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Data))return false;
    TArray<FPolyglotTextData> Rows;DOR154NoticeSources.Empty();
    auto Add=[&](const FString& Key,const FString& Zh,const FString& En){FPolyglotTextData P(ELocalizedTextSourceCategory::Game,DOR154NoticeNamespace,Key,En,TEXT("en"));for(const TCHAR* Culture:{TEXT("zh"),TEXT("zh-Hans"),TEXT("zh-Hant")})P.AddLocalizedString(Culture,Zh);Rows.Add(P);DOR154NoticeSources.Add(Key,En);};
    Add(TEXT("Format"),TEXT("[管理层] {0}的{1}已融合：{2}＋{3}。"),TEXT("[Management] {0}'s {1} now uses {2} + {3}."));
    Add(TEXT("SupplyDelivered"),TEXT("[管理层] {0}的标准战地补给已发放。"),TEXT("[Management] Field Resupply delivered to {0}."));
    Add(TEXT("SupplyPending"),TEXT("[管理层] {0}已选择标准战地补给，将在救起后发放。"),TEXT("[Management] {0} selected Field Resupply; delivery is pending revival."));
    Add(TEXT("SupplyTimeoutDelivered"),TEXT("[管理层] {0}选择超时，标准战地补给已发放。"),TEXT("[Management] {0} timed out; Field Resupply delivered."));
    Add(TEXT("SupplyTimeoutPending"),TEXT("[管理层] {0}选择超时，标准战地补给将在救起后发放。"),TEXT("[Management] {0} timed out; Field Resupply is pending revival."));
    Add(TEXT("SupplyUnpromotedDelivered"),TEXT("[管理层] {0}的当前职业尚未晋升，已自动发放标准战地补给。"),TEXT("[Management] {0}'s current class is unpromoted; automatic Field Resupply delivered."));
    Add(TEXT("SupplyUnpromotedPending"),TEXT("[管理层] {0}的当前职业尚未晋升，已自动选择标准战地补给，将在救起后发放。"),TEXT("[Management] {0}'s current class is unpromoted; automatic Field Resupply is pending revival."));
    Add(TEXT("UnpromotedManual"),TEXT("[管理层] {0}的当前职业尚未晋升，无法应用融合。"),TEXT("[Management] {0}'s current class is unpromoted; fusion cannot be applied."));
    for(auto V:Data->GetArrayField(TEXT("weapons"))){auto W=V->AsObject();Add(W->GetStringField(TEXT("weaponIdObjectPath")),W->GetStringField(TEXT("nameZh")),W->GetStringField(TEXT("nameEn")));}
    for(auto V:Data->GetArrayField(TEXT("overclocks"))){auto O=V->AsObject();Add(O->GetStringField(TEXT("overclockId")),O->GetStringField(TEXT("nameZh")),O->GetStringField(TEXT("nameEn")));}
    auto* ST=FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.PolyglotTextData"));if(!ST||Rows.Num()!=192)return false;
    FEdGraphPinType TextType;TextType.PinCategory=UEdGraphSchema_K2::PC_Text;
    for(auto* Init:{Cave,Rig})
    {
        AddTypedVariable(Init,TEXT("FusionNoticeTextData"),MakeStructPinType(ST,true));AddTypedVariable(Init,TEXT("FusionNoticeTextsReady"),MakeBoolPinType());
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Init);FKismetEditorUtilities::CompileBlueprint(Init);if(Init->Status==BS_Error)return false;
        {FDOROrdinaryGraph F(Init,TEXT("DOR_RegisterFusionNoticeTexts"),nullptr);auto* Ready=F.Branch(F.Get(TEXT("FusionNoticeTextsReady")));F.Done();F.Tail=Ready->GetElsePin();auto* Loop=F.Each(F.Get(TEXT("FusionNoticeTextData")));auto* Text=F.Call(UKismetTextLibrary::StaticClass(),TEXT("PolyglotDataToText"));Connect(FindPin(Loop,TEXT("Array Element")),FindPin(Text,TEXT("PolyglotData")));F.Cache(TEXT("RegisteredText"),Text->GetReturnValuePin());F.Tail=FindPin(Loop,TEXT("Completed"));F.Set(TEXT("FusionNoticeTextsReady"),nullptr,TEXT("true"));F.Done();F.Compile();}
        auto* P=FindFProperty<FArrayProperty>(Init->GeneratedClass,TEXT("FusionNoticeTextData"));FScriptArrayHelper Values(P,P->ContainerPtrToValuePtr<void>(Init->GeneratedClass->GetDefaultObject()));Values.Resize(Rows.Num());for(int32 I=0;I<Rows.Num();++I)*reinterpret_cast<FPolyglotTextData*>(Values.GetRawPtr(I))=Rows[I];
        auto* EG=FBlueprintEditorUtils::FindEventGraph(Init);int32 Hooks=0;for(auto* N:TArray<UEdGraphNode*>(EG->Nodes))if(auto* E=Cast<UK2Node_Event>(N))if(E->EventReference.GetMemberName()==TEXT("ReceiveBeginPlay")){DOROrdinaryInsert(E->FindPin(UEdGraphSchema_K2::PN_Then),nullptr,TEXT("DOR_RegisterFusionNoticeTexts"));++Hooks;}if(Hooks!=1)return false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Init);FKismetEditorUtilities::CompileBlueprint(Init);if(Init->Status==BS_Error)return false;
    }
    UE_LOG(LogTemp,Display,TEXT("DOR154-FUSION-TEXT entries=%d peers=cave+rig scope=finite-local-polyglot-native-FText-history"),Rows.Num());return true;
}
