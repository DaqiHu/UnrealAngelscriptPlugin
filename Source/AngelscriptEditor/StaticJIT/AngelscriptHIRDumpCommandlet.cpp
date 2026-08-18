#include "StaticJIT/AngelscriptHIRDumpCommandlet.h"

#include "StaticJIT/AngelscriptHIRDumpCommand.h"

#include "Misc/Paths.h"

int32 UAngelscriptHIRDumpCommandlet::Main(const FString& Params)
{
	using namespace AngelscriptEditor::StaticJIT;
	FAngelscriptHIRDumpRequest Request;
	FString Error;
	if (!FAngelscriptHIRDumpCommand::Parse(
		Params, FPaths::GetProjectFilePath(), Request, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return static_cast<int32>(
			EAngelscriptHIRDumpExitCode::InvalidArguments);
	}

	const FAngelscriptHIRDumpResult Result =
		FAngelscriptHIRDumpCommand::Execute(Request);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Result.Error);
		return static_cast<int32>(Result.ExitCode);
	}
	UE_LOG(LogTemp, Display,
		TEXT("AngelscriptHIRDump Profile=%s Graph=%s CompleteFunctions=%d SelectedFunctions=%d Text=%s Json=%s"),
		FAngelscriptJITTargetProfiles::ToString(Result.TargetProfile),
		*Result.CompleteGraphIdentity.ToHexString(),
		Result.CompleteFunctionCount,
		Result.SelectedFunctions.Num(),
		Result.TextOutputPath.IsEmpty() ? TEXT("<disabled>")
			: *Result.TextOutputPath,
		Result.JsonOutputPath.IsEmpty() ? TEXT("<disabled>")
			: *Result.JsonOutputPath);
	return static_cast<int32>(EAngelscriptHIRDumpExitCode::Success);
}
