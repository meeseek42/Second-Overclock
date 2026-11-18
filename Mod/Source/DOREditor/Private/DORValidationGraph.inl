// Editor-only bounded walks. A checked edge must dominate the actual effect,
// rather than occupy a particular physical position in the graph.
namespace
{
bool DORAuditReach(UEdGraphPin* Start, UEdGraphNode* Target, UEdGraphPin* Omit=nullptr)
{
    if(!Start || !Target || Start->GetOwningNode()->GetGraph()!=Target->GetGraph()) return false;
    TArray<UEdGraphPin*> Pending{Start}; TSet<UEdGraphPin*> Seen;
    while(Pending.Num())
    {
        auto* At=Pending.Pop(); if(!At || At==Omit || Seen.Contains(At)) continue;
        Seen.Add(At);
        if(At->Direction==EGPD_Output)
        {for(auto* To:At->LinkedTo) Pending.Add(To); continue;}
        auto* Node=At->GetOwningNode(); if(Node==Target) return true;
        for(auto* Out:Node->Pins) if(Out->Direction==EGPD_Output &&
            Out->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec) Pending.Add(Out);
    }
    return false;
}

bool DORAuditDominates(UEdGraphPin* Edge, UEdGraphNode* Target)
{
    if(!Edge || !Target) return false;
    bool Reachable=false;
    for(auto* N:Target->GetGraph()->Nodes)
        if(Cast<UK2Node_FunctionEntry>(N) || Cast<UK2Node_Event>(N) || Cast<UK2Node_CustomEvent>(N))
        {
            auto* Start=N->FindPin(UEdGraphSchema_K2::PN_Then);
            Reachable=Reachable || DORAuditReach(Start,Target);
            if(DORAuditReach(Start,Target,Edge)) return false;
        }
    return Reachable && DORAuditReach(Edge,Target);
}

bool DORAuditReachable(UEdGraphNode* Target)
{
    if(!Target) return false;
    for(auto* N:Target->GetGraph()->Nodes)
        if(Cast<UK2Node_FunctionEntry>(N) || Cast<UK2Node_Event>(N) || Cast<UK2Node_CustomEvent>(N))
            if(DORAuditReach(N->FindPin(UEdGraphSchema_K2::PN_Then),Target)) return true;
    return false;
}

UEdGraphPin* DORAuditSource(UEdGraphPin* Pin)
{
    return Pin && Pin->Direction==EGPD_Input && Pin->LinkedTo.Num()==1 ? Pin->LinkedTo[0] : Pin;
}

bool DORAuditSameOperand(UEdGraphPin* A, UEdGraphPin* B)
{
    A=DORAuditSource(A); B=DORAuditSource(B);
    if(!A || !B) return false;
    if(A==B) return true;
    auto* AV=Cast<UK2Node_VariableGet>(A->GetOwningNode());
    auto* BV=Cast<UK2Node_VariableGet>(B->GetOwningNode());
    if(!AV || !BV || AV->GetGraph()!=BV->GetGraph() ||
       AV->VariableReference.GetMemberName()!=BV->VariableReference.GetMemberName()) return false;
    auto* AS=AV->FindPin(UEdGraphSchema_K2::PN_Self); auto* BS=BV->FindPin(UEdGraphSchema_K2::PN_Self);
    return (!AS || !AS->LinkedTo.Num()) && (!BS || !BS->LinkedTo.Num());
}

bool DORAuditLiteral(UEdGraphPin* P,const TCHAR* Value)
{return P && P->LinkedTo.Num()==0 && P->DefaultValue==Value;}

TArray<UK2Node_CallFunction*> DORAuditCalls(UEdGraph* G,FName Name)
{
    TArray<UK2Node_CallFunction*> Result;
    if(G) for(auto* N:G->Nodes) if(auto* C=Cast<UK2Node_CallFunction>(N))
        if(C->FunctionReference.GetMemberName()==Name) Result.Add(C);
    return Result;
}

bool DORAuditCurrentSelfCall(UK2Node_CallFunction* Call)
{
    auto* BP=Call ? Cast<UBlueprint>(Call->GetGraph()->GetOuter()) : nullptr;
    auto* Fn=Call ? Call->GetTargetFunction() : nullptr;
    if(!BP || !BP->GeneratedClass || !Fn || !(BP->GeneratedClass->IsChildOf(Fn->GetOuterUClass()) ||
        (BP->SkeletonGeneratedClass && BP->SkeletonGeneratedClass->IsChildOf(Fn->GetOuterUClass())))) return false;
    auto* Self=Call->FindPin(UEdGraphSchema_K2::PN_Self);
    if(!Self || !Self->LinkedTo.Num()) return true;
    auto* Source=DORAuditSource(Self);return Source && Cast<UK2Node_Self>(Source->GetOwningNode());
}

UK2Node_IfThenElse* DORAuditCheckedGate(UK2Node_CallFunction* Call,UEdGraphNode* Effect)
{
    if(!Call || !Effect) return nullptr;
    for(auto* N:Effect->GetGraph()->Nodes) if(auto* Gate=Cast<UK2Node_IfThenElse>(N))
        if(DORAuditSameOperand(Gate->GetConditionPin(),Call->GetReturnValuePin()) &&
           (!Call->GetExecPin() || DORAuditDominates(Call->GetThenPin(),Gate)) &&
           DORAuditDominates(Gate->GetThenPin(),Effect)) return Gate;
    return nullptr;
}

bool DORAuditNativeEffect(UEdGraphNode* N)
{
    auto* C=Cast<UK2Node_CallFunction>(N); if(!C) return false;
    const FName Name=C->FunctionReference.GetMemberName();
    return Name==TEXT("EquipUpgrade") || Name==TEXT("RefreshLoadoutFromSave") ||
        Name==TEXT("Server_SetLoadout") || Name==TEXT("UpdateFromSaveGameInSlot");
}

// Every successor must encounter the close call before any native effect or
// return. Observations and cache nodes may sit between the effect and close.
bool DORAuditClosePaths(UEdGraphPin* Start)
{
    if(!Start) return false;
    TArray<TPair<UEdGraphPin*,bool>> Pending{TPair<UEdGraphPin*,bool>(Start,false)};TMap<UEdGraphPin*,uint8> State;bool Found=false;
    while(Pending.Num())
    {
        const auto Visit=Pending.Pop();auto* At=Visit.Key;if(!At)return false;
        if(Visit.Value) {State.Add(At,2);continue;}
        if(State.FindRef(At)==1)return false;if(State.FindRef(At)==2)continue;
        State.Add(At,1);Pending.Add(TPair<UEdGraphPin*,bool>(At,true));
        if(At->Direction==EGPD_Output)
        {if(!At->LinkedTo.Num()) return false; for(auto* To:At->LinkedTo) Pending.Add(TPair<UEdGraphPin*,bool>(To,false)); continue;}
        auto* N=At->GetOwningNode();
        if(auto* C=Cast<UK2Node_CallFunction>(N))
            if(C->FunctionReference.GetMemberName()==TEXT("DOR_NativeLegacyAfterCall"))
            {
                if(!DORAuditCurrentSelfCall(C)) return false;
                bool Checked=false;
                for(auto* B:N->GetGraph()->Nodes) if(auto* Gate=Cast<UK2Node_IfThenElse>(B))
                    if(DORAuditSameOperand(Gate->GetConditionPin(),C->GetReturnValuePin()) &&
                       DORAuditDominates(C->GetThenPin(),Gate))
                        for(auto* Fault:DORAuditCalls(N->GetGraph(),TEXT("DOR_NativeLegacyFault")))
                            Checked=Checked || (DORAuditCurrentSelfCall(Fault) && DORAuditReach(Gate->GetElsePin(),Fault));
                if(!Checked) return false; Found=true; continue;
            }
        if(DORAuditNativeEffect(N) || Cast<UK2Node_FunctionResult>(N)) return false;
        bool HasOutput=false;
        for(auto* Out:N->Pins) if(Out->Direction==EGPD_Output &&
            Out->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)
        {Pending.Add(TPair<UEdGraphPin*,bool>(Out,false)); HasOutput=true;}
        if(!HasOutput) return false;
    }
    return Found;
}

bool DORAuditLegacySite(UK2Node_CallFunction* Effect)
{
    if(!Effect || !Effect->GetExecPin() || !Effect->GetExecPin()->LinkedTo.Num()) return false;
    const FName Name=Effect->FunctionReference.GetMemberName();
    const int32 Kind=Name==TEXT("EquipUpgrade") ? 0 : Name==TEXT("UpdateFromSaveGameInSlot") ? 2 : 1;
    auto* Receiver=Effect->FindPin(Kind==0 ? TEXT("PlayerState") : UEdGraphSchema_K2::PN_Self);
    bool Before=false;
    for(auto* C:DORAuditCalls(Effect->GetGraph(),TEXT("DOR_NativeLegacyBeforeCall")))
        Before=Before || (DORAuditCurrentSelfCall(C) && DORAuditSameOperand(Receiver,C->FindPin(TEXT("Receiver"))) &&
            DORAuditLiteral(C->FindPin(TEXT("ReceiverKind")),*FString::FromInt(Kind)) &&
            DORAuditLiteral(C->FindPin(TEXT("ConsumesSource")),
                Name==TEXT("EquipUpgrade") || Name==TEXT("RefreshLoadoutFromSave") ? TEXT("true") : TEXT("false")) &&
            DORAuditCheckedGate(C,Effect));
    const bool Close=DORAuditClosePaths(Effect->GetThenPin());
    if(!Before || !Close) UE_LOG(LogTemp,Display,TEXT("DOR-LEGACY-GRAPH condition graph=%s effect=%s before=%d close=%d"),*Effect->GetGraph()->GetName(),*Name.ToString(),Before,Close);
    return Before && Close;
}

bool DORAuditGraphScratch(UK2Node_VariableSet* Set)
{
    if(!Set) return false;auto* Self=Set->FindPin(UEdGraphSchema_K2::PN_Self);
    if(Self && Self->LinkedTo.Num()) return false;
    auto* BP=Cast<UBlueprint>(Set->GetGraph()->GetOuter());if(!BP || !BP->GeneratedClass) return false;
    const FName Field=Set->VariableReference.GetMemberName();
    auto* Property=BP->GeneratedClass->FindPropertyByName(Field);if(Property && Property->HasAnyPropertyFlags(CPF_SaveGame)) return false;
    bool Declared=Property!=nullptr;for(const auto& V:BP->NewVariables) if(V.VarName==Field) {if(V.PropertyFlags&CPF_SaveGame)return false;Declared=true;}if(!Declared)return false;
    auto* Original=Cast<UBlueprint>(BP->GeneratedClass->ClassGeneratedBy);if(!Original)Original=BP;
    TArray<UEdGraph*> Graphs;Original->GetAllGraphs(Graphs);
    for(auto* G:Graphs) if(G->GetFName()!=Set->GetGraph()->GetFName()) for(auto* N:G->Nodes)
        if(auto* V=Cast<UK2Node_Variable>(N)) if(V->VariableReference.GetMemberName()==Field) return false;
    return true;
}

bool DORAuditBankIntent(UEdGraph* G)
{
    auto* Entry=G ? FindFunctionEntry(G) : nullptr; if(!Entry) return false;
    bool EffectFound=false;
    for(auto* N:G->Nodes)
    {
        auto* C=Cast<UK2Node_CallFunction>(N);
        auto* Set=Cast<UK2Node_VariableSet>(N);
        const bool Effect=(Set && !DORAuditGraphScratch(Set)) || (C &&
            (C->FunctionReference.GetMemberName()==TEXT("Map_Add") ||
             C->FunctionReference.GetMemberName()==TEXT("Map_Remove")));
        if(!Effect) continue; EffectFound=true; bool Guarded=false;
        for(auto* Intent:DORAuditCalls(G,TEXT("DOR_NativePersistBankIntent")))
            Guarded=Guarded || (DORAuditCurrentSelfCall(Intent) && DORAuditSameOperand(Intent->FindPin(TEXT("RequestId")),
                Entry->FindPin(TEXT("RequestId"))) && DORAuditCheckedGate(Intent,N));
        if(!Guarded) return false;
    }
    return EffectFound;
}

UEdGraph* DORAuditTransientGraph(UEdGraph* G)
{
    if(!G) return nullptr;
    auto* Original=Cast<UBlueprint>(G->GetOuter()); if(!Original) return nullptr;
    auto* Owner=NewObject<UBlueprint>(GetTransientPackage());Owner->GeneratedClass=Original->GeneratedClass;
    Owner->SkeletonGeneratedClass=Original->SkeletonGeneratedClass;Owner->ParentClass=Original->ParentClass;
    Owner->NewVariables=Original->NewVariables;
    auto* Copy=DuplicateObject<UEdGraph>(G,Owner);
    if(Copy) {Copy->SetFlags(RF_Transient); Copy->ClearFlags(RF_Public|RF_Standalone);}
    return Copy;
}

bool DORAuditInsertObservation(UEdGraphPin* From,UEdGraphPin* Message=nullptr)
{
    if(!From || !From->LinkedTo.Num()) return false;
    auto* G=From->GetOwningNode()->GetGraph(); if(G->GetOutermost()!=GetTransientPackage()) return false;
    const auto Next=From->LinkedTo; From->BreakAllPinLinks();
    auto* Observer=AddCall(G,UKismetSystemLibrary::StaticClass(),TEXT("PrintString"),0,0);
    if(!Observer || !Connect(From,Observer->GetExecPin())) return false;
    if(Message && !Connect(Message,Observer->FindPin(TEXT("InString")))) return false;
    for(auto* To:Next) if(!Connect(Observer->GetThenPin(),To)) return false;
    return true;
}
}
