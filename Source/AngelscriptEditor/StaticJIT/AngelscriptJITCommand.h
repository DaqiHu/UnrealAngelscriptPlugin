#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProjectGeneration.h"
#include "StaticJIT/AngelscriptJITProjectScaffold.h"

namespace AngelscriptEditor::StaticJIT
{
	enum class EAngelscriptJITCommandMode : uint8
	{
		Invalid = 0,
		Scaffold,
		Generate,
		Verify,
	};

	enum class EAngelscriptJITCommandExitCode : int32
	{
		Success = 0,
		ExecutionFailure = 1,
		InvalidArguments = 2,
		ScaffoldRequired = 3,
	};

	struct ANGELSCRIPTEDITOR_API FAngelscriptJITCommandRequest
	{
		EAngelscriptJITCommandMode Mode = EAngelscriptJITCommandMode::Invalid;
		FString ProjectFile;
		FString TemporaryRoot;
		TArray<EAngelscriptJITTargetProfile> TargetProfiles;
	};

	struct ANGELSCRIPTEDITOR_API FAngelscriptJITCommandProfileResult
	{
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		FString OutputDirectory;
		FAngelscriptJITProjectGenerationResult ProjectGeneration;
		bool bRequiresFullBuild = false;
	};

	struct ANGELSCRIPTEDITOR_API FAngelscriptJITCommandResult
	{
		EAngelscriptJITCommandExitCode ExitCode =
			EAngelscriptJITCommandExitCode::ExecutionFailure;
		FString Error;
		FAngelscriptJITProjectScaffoldResult Scaffold;
		TArray<FAngelscriptJITCommandProfileResult> Profiles;
		bool bRequiresFullBuild = false;

		bool IsSuccess() const
		{
			return ExitCode == EAngelscriptJITCommandExitCode::Success;
		}
	};

	using FAngelscriptJITProjectArtifactBuilder = TFunction<bool(
		FStringView,
		EAngelscriptJITTargetProfile,
		FAngelscriptJITGenerationOutput&,
		FString*)>;

	/** Testable Editor command service used by UAngelscriptJITCommandlet. */
	class ANGELSCRIPTEDITOR_API FAngelscriptJITCommand
	{
	public:
		static bool Parse(
			FStringView Params,
			FStringView DefaultProjectFile,
			FAngelscriptJITCommandRequest& OutRequest,
			FString& OutError);

		static FAngelscriptJITCommandResult Execute(
			const FAngelscriptJITCommandRequest& Request,
			FAngelscriptJITProjectArtifactBuilder ArtifactBuilder = {});
	};
}
