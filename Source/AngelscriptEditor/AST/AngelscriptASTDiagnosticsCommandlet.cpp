#include "AST/AngelscriptASTDiagnosticsCommandlet.h"

#include "Core/AngelscriptEngine.h"
#include "Dump/AngelscriptASTDiagnostics.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	bool IsASTDiagnosticsHostSwitch(const FString& Switch)
	{
		FString Key;
		FString Value;
		if (!Switch.Split(TEXT("="), &Key, &Value)) Key = Switch;
		return Key.Equals(TEXT("run"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("buildmachine"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("unattended"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("nopause"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("nosplash"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("stdout"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("fullstdoutlogoutput"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("utf8output"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("nosound"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("nullrhi"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("abslog"), ESearchCase::IgnoreCase);
	}
}

int32 UAngelscriptASTDiagnosticsCommandlet::Main(const FString& Params)
{
	if (!FAngelscriptEngine::IsInitialized())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Angelscript AST diagnostics require an initialized script engine."));
		return 2;
	}

	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine(*Params, Tokens, Switches);
	TArray<FString> Args = MoveTemp(Tokens);
	FString OutputPath;
	for (FString& Switch : Switches)
	{
		FString Key;
		FString Value;
		if (Switch.Split(TEXT("="), &Key, &Value)
			&& Key.Equals(TEXT("output"), ESearchCase::IgnoreCase))
		{
			OutputPath = MoveTemp(Value);
			continue;
		}
		if (IsASTDiagnosticsHostSwitch(Switch)) continue;
		Args.Add(FString(TEXT("--")) + MoveTemp(Switch));
	}

	FString Output;
	FString Error;
	const int32 Status = FAngelscriptASTDiagnostics::Run(
		FAngelscriptEngine::Get().GetScriptEngine(), Args, Output, Error);
	if (Status != asSUCCESS)
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		if (!Output.IsEmpty()) UE_LOG(LogTemp, Error, TEXT("%s"), *Output);
		return 1;
	}
	if (!OutputPath.IsEmpty())
	{
		OutputPath = FPaths::ConvertRelativePathToFull(OutputPath);
		if (!FFileHelper::SaveStringToFile(
			Output,
			*OutputPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogTemp, Error,
				TEXT("Failed to write diagnostic-only AST output to '%s'."), *OutputPath);
			return 3;
		}
		UE_LOG(LogTemp, Display,
			TEXT("Wrote diagnostic-only AST output to '%s'."), *OutputPath);
		return 0;
	}
	UE_LOG(LogTemp, Display, TEXT("%s"), *Output);
	return 0;
}
