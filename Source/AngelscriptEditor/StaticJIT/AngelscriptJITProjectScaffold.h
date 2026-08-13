#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProvider.h"

namespace AngelscriptEditor::StaticJIT
{
	struct ANGELSCRIPTEDITOR_API FAngelscriptJITProjectScaffoldInspection
	{
		FString ProjectFile;
		FString ModuleDirectory;
		FAngelscriptJITProviderId ProviderId;
		bool bDescriptorCurrent = false;
		bool bRuntimeDependencyCurrent = false;
		bool bModuleSourceCurrent = false;
		bool bGeneratedLayoutCurrent = false;
		bool bTargetDiscoverable = false;
		TArray<FString> Differences;

		bool IsCurrent() const
		{
			return Differences.IsEmpty()
				&& bDescriptorCurrent
				&& bRuntimeDependencyCurrent
				&& bModuleSourceCurrent
				&& bGeneratedLayoutCurrent
				&& bTargetDiscoverable;
		}

		FString Summary() const;
	};

	struct ANGELSCRIPTEDITOR_API FAngelscriptJITProjectScaffoldResult
	{
		bool bSuccess = false;
		bool bChanged = false;
		bool bRequiresFullBuild = false;
		FString Error;
		FString ProjectFile;
		FString ModuleDirectory;
		FAngelscriptJITProviderId ProviderId;
		TArray<FString> WrittenFiles;
		TArray<FString> UnchangedFiles;
		TArray<FString> RemovedFiles;
	};

	/**
	 * Editor-owned service for the fixed project Source/AngelscriptJIT shell.
	 * It structurally updates the project descriptor and never discovers AS
	 * source, generates provider artifacts, or depends on either test module.
	 */
	class ANGELSCRIPTEDITOR_API FAngelscriptJITProjectScaffold
	{
	public:
		static constexpr uint32 ScaffoldRevision = 2;
		static constexpr const TCHAR* ModuleName = TEXT("AngelscriptJIT");

		static FAngelscriptJITProviderId BuildProviderId(
			FStringView ProjectFile);

		static FAngelscriptJITProjectScaffoldInspection Inspect(
			FStringView ProjectFile);

		static FAngelscriptJITProjectScaffoldResult Scaffold(
			FStringView ProjectFile);
	};
}
