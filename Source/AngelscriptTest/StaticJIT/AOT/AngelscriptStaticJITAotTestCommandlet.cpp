#include "StaticJIT/AOT/AngelscriptStaticJITAotTestCommandlet.h"

#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"

#include "Misc/Parse.h"

int32 UAngelscriptTestJITCommandlet::Main(const FString& Params)
{
	FString ModeText;
	FParse::Value(*Params, TEXT("Mode="), ModeText);
	if (ModeText.IsEmpty())
	{
		ModeText = TEXT("Verify");
	}

	const bool bGenerate = ModeText.Equals(TEXT("Generate"), ESearchCase::IgnoreCase);
	const bool bVerify = ModeText.Equals(TEXT("Verify"), ESearchCase::IgnoreCase);
	if (!bGenerate && !bVerify)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid AngelscriptTestJIT Mode='%s'. Expected Generate or Verify."), *ModeText);
		return 2;
	}

	const AngelscriptStaticJITAotGeneration::FStaticJITAotGenerationResult Result = AngelscriptStaticJITAotGeneration::Run(
		bGenerate
			? AngelscriptStaticJITAotGeneration::EStaticJITAotGenerationMode::Generate
			: AngelscriptStaticJITAotGeneration::EStaticJITAotGenerationMode::Verify);

	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Result.Error);
		return 1;
	}

	for (const FString& Filename : Result.WrittenFiles)
	{
		UE_LOG(LogTemp, Display, TEXT("AngelscriptTestJIT wrote %s"), *Filename);
	}
	for (const FString& Filename : Result.UnchangedFiles)
	{
		UE_LOG(LogTemp, Display, TEXT("AngelscriptTestJIT unchanged %s"), *Filename);
	}
	for (const FString& Filename : Result.RemovedFiles)
	{
		UE_LOG(LogTemp, Display, TEXT("AngelscriptTestJIT removed %s"), *Filename);
	}
	UE_LOG(LogTemp, Display,
		TEXT("AngelscriptTestJIT ProviderId=%s ProviderGeneration=%s"),
		*Result.ProviderId, *Result.ProviderGeneration);
	for (const FString& Module : Result.GeneratedModules)
	{
		UE_LOG(LogTemp, Display, TEXT("AngelscriptTestJIT module %s"), *Module);
	}
	for (const FString& FunctionKey : Result.GeneratedFunctionKeys)
	{
		UE_LOG(LogTemp, Display,
			TEXT("AngelscriptTestJIT function %s"), *FunctionKey);
	}

	return 0;
}
