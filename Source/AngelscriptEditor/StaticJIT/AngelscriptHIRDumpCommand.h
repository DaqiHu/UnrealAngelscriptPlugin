#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/AngelscriptJITGeneration.h"

namespace AngelscriptEditor::StaticJIT
{
	enum class EAngelscriptHIRDumpRequestKind : uint8
	{
		DeveloperHIRDump = 0,
	};

	enum class EAngelscriptHIRDumpFormat : uint8
	{
		Text = 0,
		Json,
		Both,
	};

	enum class EAngelscriptHIRDumpExitCode : int32
	{
		Success = 0,
		ExecutionFailure = 1,
		InvalidArguments = 2,
	};

	enum class EAngelscriptHIRDumpCaptureState : uint8
	{
		Missing = 0,
		Verified,
	};

	struct ANGELSCRIPTEDITOR_API FAngelscriptHIRDumpRequest
	{
		EAngelscriptHIRDumpRequestKind RequestKind =
			EAngelscriptHIRDumpRequestKind::DeveloperHIRDump;
		FString ProjectFile;
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		FString OutputDirectory;
		FString StableModuleFilter;
		FString StableFunctionFilter;
		EAngelscriptHIRDumpFormat Format = EAngelscriptHIRDumpFormat::Both;
	};

	/** Pointer-free result row for one selected function. */
	struct ANGELSCRIPTEDITOR_API FAngelscriptHIRDumpFunctionResult
	{
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptStableFunctionKey FunctionKey;
		FString CanonicalDeclaration;
		EAngelscriptHIRDumpCaptureState CaptureState =
			EAngelscriptHIRDumpCaptureState::Missing;
	};

	/**
	 * Developer snapshot result. Deliberately contains no BackendId, Provider,
	 * fallback, entry, or execution-counter fields.
	 */
	struct ANGELSCRIPTEDITOR_API FAngelscriptHIRDumpResult
	{
		EAngelscriptHIRDumpRequestKind RequestKind =
			EAngelscriptHIRDumpRequestKind::DeveloperHIRDump;
		EAngelscriptHIRDumpExitCode ExitCode =
			EAngelscriptHIRDumpExitCode::ExecutionFailure;
		FString Error;
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		EAngelscriptHIRDumpFormat Format = EAngelscriptHIRDumpFormat::Both;
		FString OutputDirectory;
		FString TextOutputPath;
		FString JsonOutputPath;
		FString StableModuleFilter;
		FString StableFunctionFilter;
		FAngelscriptHash256 CompleteGraphIdentity;
		FAngelscriptArtifactProfileKey ArtifactProfile;
		uint32 SourceCompilePassCount = 0;
		int32 CompleteModuleCount = 0;
		int32 CompleteFunctionCount = 0;
		int32 CapturedFunctionCount = 0;
		int32 MissingFunctionCount = 0;
		int32 InvalidFunctionCount = 0;
		TArray<FAngelscriptHIRDumpFunctionResult> SelectedFunctions;

		bool IsSuccess() const
		{
			return ExitCode == EAngelscriptHIRDumpExitCode::Success;
		}
	};

	/** Testable service used by the dedicated UObject commandlet. */
	class ANGELSCRIPTEDITOR_API FAngelscriptHIRDumpCommand final
	{
	public:
		static bool Parse(
			FStringView Params,
			FStringView DefaultProjectFile,
			FAngelscriptHIRDumpRequest& OutRequest,
			FString& OutError);

		static FAngelscriptHIRDumpResult Execute(
			const FAngelscriptHIRDumpRequest& Request);
	};
}
