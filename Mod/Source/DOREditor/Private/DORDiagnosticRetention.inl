// Normalize individual records before forwarding to the bounded normal sink.
// BatchCapture is installed at entry and sees the original text.
struct FDORDiagnosticGraph : FDORSafeGraph
{
    using FDORSafeGraph::FDORSafeGraph;
    FName Variable(UEdGraphPin* P){return CastChecked<UK2Node_VariableGet>(P->GetOwningNode())->VariableReference.GetMemberName();}
    UEdGraphPin* Str(const FString& V){auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralString"));FindPin(N,TEXT("Value"))->DefaultValue=V;return N->GetReturnValuePin();}
    UEdGraphPin* Int(int32 V){auto* N=Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralInt"));FindPin(N,TEXT("Value"))->DefaultValue=FString::FromInt(V);return N->GetReturnValuePin();}
    UEdGraphPin* Bin(FName Name,UEdGraphPin* A,UEdGraphPin* B,UClass* C=UKismetMathLibrary::StaticClass()){auto* N=Call(C,Name);Connect(A,FindPin(N,TEXT("A")));Connect(B,FindPin(N,TEXT("B")));return N->GetReturnValuePin();}
    UEdGraphPin* Join(UEdGraphPin* A,UEdGraphPin* B){return Bin(TEXT("Concat_StrStr"),A,B,UKismetStringLibrary::StaticClass());}
    UEdGraphPin* Len(UEdGraphPin* V){auto* N=Call(UKismetStringLibrary::StaticClass(),TEXT("Len"));Connect(V,FindPin(N,TEXT("S")));return N->GetReturnValuePin();}
    UEdGraphPin* Contains(UEdGraphPin* V,const TCHAR* Match,bool Prefix=false){auto* N=Call(UKismetStringLibrary::StaticClass(),Prefix?TEXT("StartsWith"):TEXT("Contains"));Connect(V,FindPin(N,Prefix?TEXT("SourceString"):TEXT("SearchIn")));FindPin(N,Prefix?TEXT("InPrefix"):TEXT("Substring"))->DefaultValue=Match;return N->GetReturnValuePin();}
};

bool BuildDORDiagnosticRetention(UBlueprint* BP)
{
    constexpr int32 Budget=1048576;
    {
        FDORDiagnosticGraph F(BP,TEXT("DOR_DiagnosticOversizedSummary"));auto* Message=F.Input(TEXT("Message"),MakeStringPinType());auto* Length=F.Cache(TEXT("Length"),F.Len(Message));auto* Normal=F.Branch(F.Bin(TEXT("LessEqual_IntInt"),Length,F.Int(Budget-8192)));F.Return(Message);F.Tail=Normal->GetElsePin();
        auto* CountText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(Length,FindPin(CountText,TEXT("InInt")));auto* Summary=F.Cache(TEXT("Summary"),F.Join(F.Str(TEXT("[DOR-DIAGNOSTIC-WARNING]|event=OVERSIZED_RECORD|policy=whole-scalar-and-typed-difference-fields|component-lists-omitted|originalChars=")),CountText->GetReturnValuePin()));auto* Count=F.Cache(TEXT("SelectedCount"),F.Int(0));
        auto* Parse=F.Call(UKismetStringLibrary::StaticClass(),TEXT("ParseIntoArray"));Connect(Message,FindPin(Parse,TEXT("SourceString")));FindPin(Parse,TEXT("Delimiter"))->DefaultValue=TEXT("|");auto* Tokens=F.Cache(TEXT("Tokens"),Parse->GetReturnValuePin());auto* Loop=F.Each(Tokens);auto* Token=FindPin(Loop,TEXT("Array Element"));
        auto* Selected=F.Contains(Token,TEXT("Expected"));Selected=F.Bin(TEXT("BooleanOR"),Selected,F.Contains(Token,TEXT("Actual")));
        for(const TCHAR* Key:{TEXT("stage="),TEXT("gate="),TEXT("passed="),TEXT("detail="),TEXT("key="),TEXT("Epoch="),TEXT("Request="),TEXT("Generation="),TEXT("Code="),TEXT("Slot="),TEXT("Operation="),TEXT("Phase="),TEXT("LocalLeaseStep="),TEXT("LegacyResult"),TEXT("bReload"),TEXT("expected277"),TEXT("incoming"),TEXT("context"),TEXT("target"),TEXT("untouched"),TEXT("newActor="),TEXT("host"),TEXT("acceptedWeapon"),TEXT("acceptedRoot"),TEXT("currentTarget"),TEXT("originalTarget"),TEXT("originalUntouched"),TEXT("legacy"),TEXT("CancelRequested="),TEXT("TerminalLatched="),TEXT("now="),TEXT("QueuedDeadline="),TEXT("RecoveryDeadline="),TEXT("status="),TEXT("result="),TEXT("event="),TEXT("compareWeapon="),TEXT("missingOrDuplicate"),TEXT("duplicateExpected"),TEXT("actualKeyCount="),TEXT("expectedKeyCount=")})Selected=F.Bin(TEXT("BooleanOR"),Selected,F.Contains(Token,Key,true));
        auto* Total=F.Bin(TEXT("Add_IntInt"),F.Len(Summary),F.Bin(TEXT("Add_IntInt"),F.Len(Token),F.Int(1)));auto* Fits=F.Branch(F.Bin(TEXT("BooleanAND"),Selected,F.Bin(TEXT("BooleanAND"),F.Bin(TEXT("LessEqual_IntInt"),F.Len(Token),F.Int(8192)),F.Bin(TEXT("LessEqual_IntInt"),Total,F.Int(Budget-8192)))));F.Set(F.Variable(Summary),F.Join(F.Join(Summary,F.Str(TEXT("|"))),Token));F.Set(F.Variable(Count),F.Bin(TEXT("Add_IntInt"),Count,F.Int(1)));F.Tail=FindPin(Loop,TEXT("Completed"));
        auto* Omitted=F.Bin(TEXT("Subtract_IntInt"),F.Array(TEXT("Array_Length"),Tokens)->GetReturnValuePin(),Count);auto* OmittedText=F.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));Connect(Omitted,FindPin(OmittedText,TEXT("InInt")));F.Return(F.Join(F.Join(Summary,F.Str(TEXT("|omittedFieldCount="))),OmittedText->GetReturnValuePin()));F.Compile();
    }
    {
        FDORDiagnosticGraph F(BP,TEXT("DOR_DiagnosticRetain"));auto* Message=F.Input(TEXT("Message"),MakeStringPinType());auto* Normalize=F.Self(TEXT("DOR_DiagnosticOversizedSummary"));Connect(Message,FindPin(Normalize,TEXT("Message")));auto* Record=F.Cache(TEXT("Record"),Normalize->GetReturnValuePin());auto* Queue=F.Self(TEXT("DOR_NormalRecord"));Connect(Record,FindPin(Queue,TEXT("Message")));F.Set(TEXT("LastDiagnosticLine"),Record);F.Return(nullptr,TEXT("true"));F.Compile();
    }
    return BP->Status!=BS_Error;
}

