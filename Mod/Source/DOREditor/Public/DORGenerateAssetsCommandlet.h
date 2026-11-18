#pragma once

#include "Commandlets/Commandlet.h"
#include "DORGenerateAssetsCommandlet.generated.h"

UCLASS()
class DOREDITOR_API UDORGenerateAssetsCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UDORGenerateAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
