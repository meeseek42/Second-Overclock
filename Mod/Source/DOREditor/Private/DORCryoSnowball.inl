namespace
{
bool DORSnowballTemplateEqual(UObject* Copy,UObject* Source)
{
    if(!Copy || !Source || Copy==Source || Copy->GetClass()!=Source->GetClass())return false;
    for(TFieldIterator<FProperty> P(Source->GetClass());P;++P)
    {
        if(P->GetFName()==TEXT("DamageBonuses"))
        {
            auto* A=CastField<FArrayProperty>(*P);auto* Inner=A?CastField<FObjectPropertyBase>(A->Inner):nullptr;if(!Inner)return false;
            FScriptArrayHelper X(A,A->ContainerPtrToValuePtr<void>(Copy)),Y(A,A->ContainerPtrToValuePtr<void>(Source));if(X.Num()!=Y.Num() || X.Num()!=1)return false;
            auto* C=Inner->GetObjectPropertyValue(X.GetRawPtr(0));auto* S=Inner->GetObjectPropertyValue(Y.GetRawPtr(0));
            if(!C || !S || C->GetOuter()!=Copy || S->GetOuter()!=Source || C==S || C->GetClass()!=S->GetClass())return false;
            for(TFieldIterator<FProperty> B(S->GetClass());B;++B)if(!B->Identical_InContainer(C,S))return false;
        }
        else if(!P->Identical_InContainer(Copy,Source))return false;
    }
    return true;
}

bool CreateDORCryoSnowball(const TArray<TSharedPtr<FJsonObject>>& Pairs)
{
    const TSharedPtr<FJsonObject>* Selected=nullptr;
    for(const auto& P:Pairs)if(P->GetStringField(TEXT("pairId"))==TEXT("eeea2c40848d1380"))Selected=&P;
    if(!Selected)return true;
    const auto& Pair=*Selected;if(Pair->GetIntegerField(TEXT("testIndex"))!=455 || !Pair->HasField(TEXT("cryoSnowballCarrier")))return false;
    const FString PackageName=FString(RuntimeRoot)/TEXT("AdaptedProjectiles/Cryospray/PRJ_DOR_IcicleSnowball");
    auto* Parent=LoadObject<UClass>(nullptr,TEXT("/Game/WeaponsNTools/Cryospray/PRJ_Cryocannon.PRJ_Cryocannon_C"));
    auto* Snowball=LoadObject<UBlueprintGeneratedClass>(nullptr,TEXT("/Game/WeaponsNTools/Cryospray/PRJ_Crye_SnowBall.PRJ_Crye_SnowBall_C"));
    auto* Damage=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.DamageComponent"));
    auto* Explosion=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.ProjectileExplosion"));
    auto* ExplosionBase=LoadObject<UClass>(nullptr,TEXT("/Script/FSD.ExplosionBaseComponent"));
    auto* ParentImpact=Parent?Parent->FindFunctionByName(TEXT("OnImpacted")):nullptr;
    if(!ParentImpact || !Snowball || !Snowball->SimpleConstructionScript || !Damage || !Explosion || !ExplosionBase)return false;
    UObject* SourceDamage=nullptr;UObject* SourceExplosion=nullptr;
    for(auto* Node:Snowball->SimpleConstructionScript->GetAllNodes())
    {
        if(Node->GetVariableName()==TEXT("Damage"))SourceDamage=Node->ComponentTemplate;
        if(Node->GetVariableName()==TEXT("ProjectileExplosion"))SourceExplosion=Node->ComponentTemplate;
    }
    if(!SourceDamage || SourceDamage->GetClass()!=Damage || !SourceExplosion || SourceExplosion->GetClass()!=Explosion)return false;
    const auto ParentBefore=DORHurricaneDefaults(Parent->GetDefaultObject()),DamageBefore=DORHurricaneDefaults(SourceDamage),ExplosionBefore=DORHurricaneDefaults(SourceExplosion);
    auto* BP=CreateObjectBlueprint(PackageName,TEXT("PRJ_DOR_IcicleSnowball"),Parent);if(!BP || !BP->SimpleConstructionScript)return false;
    auto Copy=[&](UObject* Source,const TCHAR* Name)->USCS_Node*
    {
        auto* Node=BP->SimpleConstructionScript->CreateNode(Source->GetClass(),Name);if(!Node || !Node->ComponentTemplate)return nullptr;
        const FName TemplateName=Node->ComponentTemplate->GetFName();
        Node->ComponentTemplate->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
        Node->ComponentTemplate=Cast<UActorComponent>(StaticDuplicateObject(Source,BP->GeneratedClass,TemplateName));
        if(!Node->ComponentTemplate || Node->ComponentTemplate->GetOuter()!=BP->GeneratedClass || !DORSnowballTemplateEqual(Node->ComponentTemplate,Source))return nullptr;
        BP->SimpleConstructionScript->AddNode(Node);return Node;
    };
    auto* DamageNode=Copy(SourceDamage,TEXT("DORSnowballDamage"));auto* ExplosionNode=Copy(SourceExplosion,TEXT("DORSnowballExplosion"));if(!DamageNode || !ExplosionNode)return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return false;
    AddTypedVariable(BP,TEXT("DORImpactLocation"),MakeStructPinType(TBaseStructure<FVector>::Get()));AddTypedVariable(BP,TEXT("DORImpactNormal"),MakeStructPinType(TBaseStructure<FVector>::Get()));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    auto* G=FBlueprintEditorUtils::FindEventGraph(BP);auto* Event=AddNode<UK2Node_Event>(G,0,0);Event->EventReference.SetExternalMember(TEXT("OnImpacted"),Parent);Event->bOverrideFunction=true;Event->AllocateDefaultPins();
    auto* Hit=FindPin(Event,TEXT("HitResult"));auto* Predicted=FindPin(Event,TEXT("PredictedImpact"));if(!Hit || !Predicted)return false;
    auto* Break=AddCall(G,UGameplayStatics::StaticClass(),TEXT("BreakHitResult"),0,200);if(!Connect(Hit,FindPin(Break,TEXT("Hit"))))return false;
    auto* Location=AddVariableSet(G,TEXT("DORImpactLocation"),200,0);auto* Normal=AddVariableSet(G,TEXT("DORImpactNormal"),400,0);
    auto* Collision=AddExternalVariableGet(G,TEXT("CollisionComponent"),Parent,0,400);Connect(SelfPin(G,0,700),FindPin(Collision,UEdGraphSchema_K2::PN_Self));auto* CollisionLocation=AddCall(G,USceneComponent::StaticClass(),TEXT("K2_GetComponentLocation"),0,500);Connect(Collision->GetValuePin(),FindPin(CollisionLocation,UEdGraphSchema_K2::PN_Self));
    bool Ok=Connect(FindPin(Event,UEdGraphSchema_K2::PN_Then),Location->GetExecPin());Ok&=Connect(CollisionLocation->GetReturnValuePin(),FindPin(Location,TEXT("DORImpactLocation")));Ok&=Connect(FindPin(Location,UEdGraphSchema_K2::PN_Then),Normal->GetExecPin());Ok&=Connect(FindPin(Break,TEXT("Normal")),FindPin(Normal,TEXT("DORImpactNormal")));
    auto* ParentCall=AddNode<UK2Node_CallParentFunction>(G,600,0);ParentCall->SetFromFunction(ParentImpact);ParentCall->AllocateDefaultPins();Ok&=Connect(FindPin(Normal,UEdGraphSchema_K2::PN_Then),ParentCall->GetExecPin());Ok&=Connect(Hit,FindPin(ParentCall,TEXT("HitResult")));Ok&=Connect(Predicted,FindPin(ParentCall,TEXT("PredictedImpact")));
    auto* Authority=AddCall(G,AActor::StaticClass(),TEXT("HasAuthority"),800,200);auto* Branch=AddNode<UK2Node_IfThenElse>(G,800,0);Branch->AllocateDefaultPins();Ok&=Connect(ParentCall->GetThenPin(),Branch->GetExecPin());Ok&=Connect(Authority->GetReturnValuePin(),Branch->GetConditionPin());
    auto* OriginalDamage=AddCall(G,ExplosionBase,TEXT("DamageOnlyFromHit"),1000,0);Ok&=Connect(Branch->GetThenPin(),OriginalDamage->GetExecPin());Ok&=Connect(Hit,FindPin(OriginalDamage,TEXT("HitResult")));Ok&=Connect(AddVariableGet(G,TEXT("DORSnowballDamage"),1000,200)->GetValuePin(),FindPin(OriginalDamage,TEXT("DamageComponent")));Ok&=Connect(AddVariableGet(G,TEXT("DORSnowballExplosion"),1000,300)->GetValuePin(),FindPin(OriginalDamage,UEdGraphSchema_K2::PN_Self));
    auto* Effects=AddCall(G,Explosion,TEXT("SpawnEffects"),1200,0);Ok&=Connect(OriginalDamage->GetThenPin(),Effects->GetExecPin());Ok&=Connect(Branch->GetElsePin(),Effects->GetExecPin());Ok&=Connect(AddVariableGet(G,TEXT("DORSnowballExplosion"),1200,200)->GetValuePin(),FindPin(Effects,UEdGraphSchema_K2::PN_Self));Ok&=Connect(AddVariableGet(G,TEXT("DORImpactLocation"),1200,300)->GetValuePin(),FindPin(Effects,TEXT("Location")));Ok&=Connect(AddVariableGet(G,TEXT("DORImpactNormal"),1200,400)->GetValuePin(),FindPin(Effects,TEXT("Normal")));
    if(!Ok || !SaveBlueprint(BP) || BP->GeneratedClass->GetSuperClass()!=Parent || !DORSnowballTemplateEqual(DamageNode->ComponentTemplate,SourceDamage) || !DORSnowballTemplateEqual(ExplosionNode->ComponentTemplate,SourceExplosion) || !ParentBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(Parent->GetDefaultObject())) || !DamageBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(SourceDamage)) || !ExplosionBefore.OrderIndependentCompareEqual(DORHurricaneDefaults(SourceExplosion)))return false;
    UE_LOG(LogDORGenerateAssets,Display,TEXT("Cryo455 saved child=%s independentSnowballTemplates=2 parentImpactCalls=1 nativeDamageAfterParentAuthorityOnly=true sourceTemplatesUnchanged=true"),*BP->GeneratedClass->GetPathName());return true;
}
}

