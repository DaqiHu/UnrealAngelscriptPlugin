#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

enum class EAngelscriptCompilationEventType : uint8
{
	Unknown,
	CompileBegin,
	CompileEnd,
	PreprocessProcessChunks,
	PreprocessPostProcessCode,
	CompileModuleAssembly,
	CompileModuleParse,
	CompileModuleGenerateTypes,
	CompileModuleGenerateFunctions,
	CompileModuleLayout,
	CompileModuleCompileCode,
	CompileModuleGlobals,
	CompileClassGenerationHandoff,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCompilationEvent
{
	EAngelscriptCompilationEventType Type = EAngelscriptCompilationEventType::Unknown;
	FName Phase;
	ECompileType CompileType = ECompileType::SoftReloadOnly;
	EAngelscriptCompileCachePolicy CachePolicy =
		EAngelscriptCompileCachePolicy::Default;
	ECompileResult CompileResult = ECompileResult::Error;
	uint64 CompilationRunId = 0;
	bool bSucceeded = false;
	bool bFailed = false;
	int32 ModuleCount = 0;
	int32 CompiledModuleCount = 0;
	int32 FileCount = 0;
	int32 ImportCount = 0;
	int32 ClassCount = 0;
	int32 FunctionCount = 0;
	int32 DiagnosticCount = 0;
	uint32 ThreadId = 0;
	bool bOnGameThread = false;
	bool bLoadedPrecompiledCode = false;
	bool bLoadedIncrementalCache = false;
	bool bJitAvailable = false;
	bool bJitHandoff = false;
	TArray<FString> ModuleNames;
	TArray<FString> FileNames;
	TArray<FString> ImportedModuleNames;
	TArray<FString> Messages;
	FAngelscriptPreprocessorSummary PreprocessorSummary;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FAngelscriptCompilationEventDelegate, const FAngelscriptCompilationEvent&);

struct ANGELSCRIPTRUNTIME_API FAngelscriptCompilationEvents
{
	static bool HasListeners();
	static FDelegateHandle RegisterListener(TFunction<void(const FAngelscriptCompilationEvent&)> Listener);
	static void UnregisterListener(FDelegateHandle Handle);
	static void Broadcast(const FAngelscriptCompilationEvent& Event);
};

/**
 * Thread-local event suppression used by analysis-only compiler hosts.
 *
 * A StaticJITGeneration engine performs a real preprocess/compile pass, but it
 * is not a Runtime compilation lifecycle. Suppressing at the shared event
 * boundary keeps preprocessor and compiler phase events consistent without
 * teaching every producer about engine purpose.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptCompilationEventSuppressionScope
{
public:
	explicit FAngelscriptCompilationEventSuppressionScope(bool bInSuppress);
	~FAngelscriptCompilationEventSuppressionScope();

	FAngelscriptCompilationEventSuppressionScope(
		const FAngelscriptCompilationEventSuppressionScope&) = delete;
	FAngelscriptCompilationEventSuppressionScope& operator=(
		const FAngelscriptCompilationEventSuppressionScope&) = delete;

private:
	bool bSuppress = false;
};
