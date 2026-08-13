#include "StaticJIT/AngelscriptJITCommandlet.h"

#include "StaticJIT/AngelscriptJITCommand.h"

#include "Misc/Paths.h"

int32 UAngelscriptJITCommandlet::Main(const FString& Params)
{
	using namespace AngelscriptEditor::StaticJIT;
	FAngelscriptJITCommandRequest Request;
	FString Error;
	if (!FAngelscriptJITCommand::Parse(
		Params, FPaths::GetProjectFilePath(), Request, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return static_cast<int32>(
			EAngelscriptJITCommandExitCode::InvalidArguments);
	}

	const FAngelscriptJITCommandResult Result =
		FAngelscriptJITCommand::Execute(Request);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Result.Error);
		return static_cast<int32>(Result.ExitCode);
	}
	if (Request.Mode == EAngelscriptJITCommandMode::Scaffold)
	{
		UE_LOG(LogTemp, Display,
			TEXT("AngelscriptJIT scaffold %s at %s. RequiresFullBuild=%s."),
			Result.Scaffold.bChanged ? TEXT("updated") : TEXT("is current"),
			*Result.Scaffold.ModuleDirectory,
			Result.Scaffold.bRequiresFullBuild ? TEXT("true") : TEXT("false"));
	}
	for (const FAngelscriptJITCommandProfileResult& Profile : Result.Profiles)
	{
		UE_LOG(LogTemp, Display,
			TEXT("AngelscriptJIT %s current at %s. RequiresFullBuild=%s."),
			FAngelscriptJITTargetProfiles::ToString(Profile.TargetProfile),
			*Profile.OutputDirectory,
			Profile.bRequiresFullBuild ? TEXT("true") : TEXT("false"));
		if (Request.Mode == EAngelscriptJITCommandMode::Verify
			&& !Profile.ProjectGeneration.Report.IsEmpty())
		{
			UE_LOG(LogTemp, Display, TEXT("%s"),
				*Profile.ProjectGeneration.Report);
		}
	}
	return static_cast<int32>(EAngelscriptJITCommandExitCode::Success);
}
