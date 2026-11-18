bool DORPublicPreparedClosureVerified=false;

// Byte closure before any public-prepared contract or manifest is parsed.
// Node verifies SHA256; UE4.27 Core's maintained FSHA1 verifies the companion
// byte digest here without introducing a crypto module/platform dependency.
bool DORCheckPreparedMember(const FString& Root, const TSharedPtr<FJsonObject>& Member, FString* OutPath = nullptr)
{
    FString Relative, Expected;
    double Size = -1;
    if (!Member.IsValid() || !Member->TryGetStringField(TEXT("path"),Relative) ||
        !Member->TryGetStringField(TEXT("sha1"),Expected) || Expected.Len()!=40 ||
        !Member->TryGetNumberField(TEXT("bytes"),Size) || Relative.IsEmpty() ||
        !FPaths::IsRelative(Relative) || Relative.Contains(TEXT("..")) || Relative.Contains(TEXT("\\"))) return false;
    const FString Filename=FPaths::ConvertRelativePathToFull(Root/Relative);
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes,*Filename) || Bytes.Num()!=Size) return false;
    uint8 Digest[20];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);
    if (!BytesToHex(Digest,20).Equals(Expected,ESearchCase::IgnoreCase)) return false;
    if (OutPath) *OutPath=Filename;
    return true;
}

bool DORVerifyPreparedInputs(const FString& ReceiptFilename, const FString& PublicRoot,
    const TArray<FString>& RequiredFiles)
{
    FString Text;
    TSharedPtr<FJsonObject> Receipt;
    if (!FFileHelper::LoadFileToString(Text,*ReceiptFilename) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Receipt) || !Receipt.IsValid() ||
        Receipt->GetStringField(TEXT("schema"))!=TEXT("dor.public-prepared.v1") || !Receipt->GetBoolField(TEXT("complete"))) return false;
    const FString Root=FPaths::GetPath(FPaths::GetPath(FPaths::ConvertRelativePathToFull(ReceiptFilename)));
    TSet<FString> Verified;
    for (const auto& Value:Receipt->GetArrayField(TEXT("members")))
    {
        FString Filename;
        if (!DORCheckPreparedMember(Root,Value->AsObject(),&Filename) || Verified.Contains(Filename)) return false;
        Verified.Add(Filename);
    }
    for (const FString& Filename:RequiredFiles)
        if (!Verified.Contains(FPaths::ConvertRelativePathToFull(Filename))) return false;
    for (const auto& Value:Receipt->GetArrayField(TEXT("authoringInputs")))
        if (!DORCheckPreparedMember(PublicRoot,Value->AsObject())) return false;
    // The source receipt is itself an authored member; verify all prepared
    // original companions again before UObject loaded-object semantic checks.
    const FString SourceReceipt=Root/TEXT("source-receipt.json");
    if (!Verified.Contains(FPaths::ConvertRelativePathToFull(SourceReceipt))) return false;
    TSharedPtr<FJsonObject> Native;
    if (!FFileHelper::LoadFileToString(Text,*SourceReceipt) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Native) || !Native.IsValid() ||
        Native->GetStringField(TEXT("schema"))!=TEXT("dor.native-source-receipt.v2") || !Native->GetBoolField(TEXT("complete"))) return false;
    for (const auto& Value:Native->GetArrayField(TEXT("members")))
        if (!DORCheckPreparedMember(Root,Value->AsObject())) return false;
    DORPublicPreparedClosureVerified=true;
    UE_LOG(LogDORGenerateAssets,Display,TEXT("Public prepared byte closure verified; loaded native semantic guards remain required."));
    return true;
}
