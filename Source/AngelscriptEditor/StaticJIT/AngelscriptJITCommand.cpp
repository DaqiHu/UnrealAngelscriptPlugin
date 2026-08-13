#include "StaticJIT/AngelscriptJITCommand.h"

#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace AngelscriptEditor::StaticJIT
{
	namespace
	{
		TArray<EAngelscriptJITTargetProfile> ConcreteProfiles()
		{
			return {
				EAngelscriptJITTargetProfile::EditorDevelopment,
				EAngelscriptJITTargetProfile::GameDevelopment,
				EAngelscriptJITTargetProfile::GameShipping,
			};
		}

		bool ParseMode(
			const FStringView Value,
			EAngelscriptJITCommandMode& OutMode)
		{
			OutMode = EAngelscriptJITCommandMode::Invalid;
			if (Value.Equals(TEXT("Scaffold"), ESearchCase::IgnoreCase))
			{
				OutMode = EAngelscriptJITCommandMode::Scaffold;
			}
			else if (Value.Equals(TEXT("Generate"), ESearchCase::IgnoreCase))
			{
				OutMode = EAngelscriptJITCommandMode::Generate;
			}
			else if (Value.Equals(TEXT("Verify"), ESearchCase::IgnoreCase))
			{
				OutMode = EAngelscriptJITCommandMode::Verify;
			}
			return OutMode != EAngelscriptJITCommandMode::Invalid;
		}

		FString DefaultTemporaryRoot(const FString& ProjectFile)
		{
			return FPaths::Combine(
				FPaths::GetPath(ProjectFile),
				TEXT("Saved/Angelscript/JITVerify"));
		}
	}

	bool FAngelscriptJITCommand::Parse(
		const FStringView Params,
		const FStringView DefaultProjectFile,
		FAngelscriptJITCommandRequest& OutRequest,
		FString& OutError)
	{
		OutRequest = {};
		OutError.Reset();
		const FString ParamsString(Params);
		FString ModeText;
		FParse::Value(*ParamsString, TEXT("Mode="), ModeText);
		if (!ParseMode(ModeText, OutRequest.Mode))
		{
			OutError =
				TEXT("AngelscriptJIT requires -Mode=Scaffold|Generate|Verify.");
			return false;
		}

		FParse::Value(*ParamsString, TEXT("Project="), OutRequest.ProjectFile);
		if (OutRequest.ProjectFile.IsEmpty())
		{
			FParse::Value(
				*ParamsString, TEXT("ProjectFile="), OutRequest.ProjectFile);
		}
		if (OutRequest.ProjectFile.IsEmpty())
		{
			OutRequest.ProjectFile = FString(DefaultProjectFile);
		}
		OutRequest.ProjectFile.TrimStartAndEndInline();
		if (OutRequest.ProjectFile.IsEmpty())
		{
			OutError = TEXT("AngelscriptJIT requires a project descriptor.");
			return false;
		}
		OutRequest.ProjectFile = FPaths::ConvertRelativePathToFull(
			OutRequest.ProjectFile);
		FPaths::NormalizeFilename(OutRequest.ProjectFile);

		FParse::Value(
			*ParamsString, TEXT("TempRoot="), OutRequest.TemporaryRoot);
		if (OutRequest.TemporaryRoot.IsEmpty())
		{
			OutRequest.TemporaryRoot =
				DefaultTemporaryRoot(OutRequest.ProjectFile);
		}
		OutRequest.TemporaryRoot = FPaths::ConvertRelativePathToFull(
			OutRequest.TemporaryRoot);
		FPaths::NormalizeDirectoryName(OutRequest.TemporaryRoot);

		if (OutRequest.Mode == EAngelscriptJITCommandMode::Scaffold)
		{
			return true;
		}

		FString ProfileText;
		FParse::Value(*ParamsString, TEXT("Profile="), ProfileText);
		if (ProfileText.IsEmpty())
		{
			ProfileText = TEXT("EditorDevelopment");
		}
		EAngelscriptJITTargetProfile Profile =
			EAngelscriptJITTargetProfile::Invalid;
		if (!FAngelscriptJITTargetProfiles::TryParse(ProfileText, Profile))
		{
			OutError = FString::Printf(
				TEXT("Invalid AngelscriptJIT Profile='%s'."), *ProfileText);
			return false;
		}
		OutRequest.TargetProfiles = Profile
			== EAngelscriptJITTargetProfile::All
			? ConcreteProfiles()
			: TArray<EAngelscriptJITTargetProfile>{Profile};
		return true;
	}

	FAngelscriptJITCommandResult FAngelscriptJITCommand::Execute(
		const FAngelscriptJITCommandRequest& Request,
		FAngelscriptJITProjectArtifactBuilder ArtifactBuilder)
	{
		FAngelscriptJITCommandResult Result;
		if (Request.Mode == EAngelscriptJITCommandMode::Invalid
			|| Request.ProjectFile.IsEmpty())
		{
			Result.ExitCode = EAngelscriptJITCommandExitCode::InvalidArguments;
			Result.Error = TEXT("AngelscriptJIT command request is incomplete.");
			return Result;
		}

		if (Request.Mode == EAngelscriptJITCommandMode::Scaffold)
		{
			Result.Scaffold = FAngelscriptJITProjectScaffold::Scaffold(
				Request.ProjectFile);
			Result.ExitCode = Result.Scaffold.bSuccess
				? EAngelscriptJITCommandExitCode::Success
				: EAngelscriptJITCommandExitCode::ExecutionFailure;
			Result.Error = Result.Scaffold.Error;
			Result.bRequiresFullBuild = Result.Scaffold.bRequiresFullBuild;
			return Result;
		}

		if (Request.TargetProfiles.IsEmpty())
		{
			Result.ExitCode = EAngelscriptJITCommandExitCode::InvalidArguments;
			Result.Error = TEXT("Generate/Verify requires a target profile.");
			return Result;
		}
		const FAngelscriptJITProjectScaffoldInspection Inspection =
			FAngelscriptJITProjectScaffold::Inspect(Request.ProjectFile);
		if (!Inspection.IsCurrent())
		{
			Result.ExitCode = EAngelscriptJITCommandExitCode::ScaffoldRequired;
			Result.Error = TEXT("AngelscriptJIT project scaffold is missing or stale: ")
				+ Inspection.Summary();
			return Result;
		}
		if (!ArtifactBuilder)
		{
			ArtifactBuilder = [](
				const FStringView ProjectFile,
				const EAngelscriptJITTargetProfile Profile,
				FAngelscriptJITGenerationOutput& OutOutput,
				FString* OutError)
			{
				return FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
					ProjectFile, Profile, OutOutput, OutError);
			};
		}

		for (const EAngelscriptJITTargetProfile Profile :
			Request.TargetProfiles)
		{
			if (!FAngelscriptJITTargetProfiles::GetTraits(Profile).IsSet())
			{
				Result.ExitCode =
					EAngelscriptJITCommandExitCode::InvalidArguments;
				Result.Error = TEXT("Command request contains a non-concrete profile.");
				return Result;
			}
			FAngelscriptJITGenerationOutput RuntimeOutput;
			FString GenerationError;
			if (!ArtifactBuilder(Request.ProjectFile, Profile,
				RuntimeOutput, &GenerationError))
			{
				Result.Error = GenerationError;
				return Result;
			}
			if (RuntimeOutput.TargetProfile != Profile
				|| RuntimeOutput.ProviderId != Inspection.ProviderId)
			{
				Result.Error =
					TEXT("Generated artifacts do not belong to the requested project/profile domain.");
				return Result;
			}

			FAngelscriptJITGenerationOutput ProjectOutput;
			if (!FAngelscriptJITProjectGeneration::PrepareProjectProfileOutput(
				RuntimeOutput, ProjectOutput, &GenerationError))
			{
				Result.Error = GenerationError;
				return Result;
			}
			FAngelscriptJITCommandProfileResult& ProfileResult =
				Result.Profiles.AddDefaulted_GetRef();
			ProfileResult.TargetProfile = Profile;
			ProfileResult.OutputDirectory =
				FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
					Request.ProjectFile, Profile);
			ProfileResult.ProjectGeneration = Request.Mode
				== EAngelscriptJITCommandMode::Generate
				? FAngelscriptJITProjectGeneration::Generate(
					ProfileResult.OutputDirectory, ProjectOutput)
				: FAngelscriptJITProjectGeneration::Verify(
					ProfileResult.OutputDirectory,
					Request.TemporaryRoot,
					ProjectOutput);
			ProfileResult.bRequiresFullBuild =
				ProfileResult.ProjectGeneration.HasModuleSourceSetChange();
			Result.bRequiresFullBuild |= ProfileResult.bRequiresFullBuild;
			if (!ProfileResult.ProjectGeneration.bSuccess)
			{
				Result.Error = ProfileResult.ProjectGeneration.Error;
				return Result;
			}
		}

		if (Request.Mode == EAngelscriptJITCommandMode::Generate
			&& Result.bRequiresFullBuild)
		{
			// __has_include() does not establish a dependency when a profile
			// provider does not exist during the scaffold build. Adding/removing
			// module sources therefore also invalidates the stable selector TU so
			// the required full build cannot silently retain its old null view.
			const FString SelectorSource = FPaths::Combine(
				FPaths::GetPath(Request.ProjectFile),
				TEXT("Source/AngelscriptJIT/Generated/Provider.generated.cpp"));
			if (!IFileManager::Get().FileExists(*SelectorSource))
			{
				Result.Error = TEXT("Generated Provider selector source is missing.");
				return Result;
			}
			IFileManager::Get().SetTimeStamp(
				*SelectorSource, FDateTime::UtcNow());
		}

		Result.ExitCode = EAngelscriptJITCommandExitCode::Success;
		return Result;
	}
}
