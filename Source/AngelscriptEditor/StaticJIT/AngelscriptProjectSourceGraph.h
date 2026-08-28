#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptStaticJITCaptureProfile.h"

class asIScriptModule;
struct FAngelscriptEngine;
struct FAngelscriptStaticJITGenerationSnapshot;
struct IAngelscriptSourceProvider;

namespace AngelscriptEditor::StaticJIT
{
	struct FAngelscriptProjectSourceGraphCompileRequest
	{
		FString ProjectFile;
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		EAngelscriptStaticJITCaptureProfile CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::Bytecode;
		/**
		 * 0 = discard AST after CodeGen, 1 = retain sealed snapshot. A
		 * VerifiedCanonicalAST request must select retention so its immutable
		 * generation snapshot can own V1 AST leases.
		 */
		int32 ASTRetentionPolicy = 0;
		/** Optional immutable authority captured from the primary Editor Engine. */
		TSharedPtr<IAngelscriptSourceProvider> SourceProviderOverride;
	};

	/**
	 * Synchronous view owned by one restricted generation Engine. Raw pointers
	 * are valid only for the duration of the consumer call.
	 */
	struct FAngelscriptProjectSourceGraphCompileView
	{
		FAngelscriptEngine& Engine;
		FStringView ProjectFile;
		FStringView ProjectDirectory;
		TConstArrayView<asIScriptModule*> Modules;
		TSharedRef<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot;
	};

	using FAngelscriptProjectSourceGraphConsumer = TFunction<bool(
		const FAngelscriptProjectSourceGraphCompileView&,
		FString*)>;

	/**
	 * Shared Editor orchestration for complete project source compilation.
	 * The consumer must freeze pointer-free output synchronously before this
	 * call destroys the generation Engine.
	 */
	class ANGELSCRIPTEDITOR_API FAngelscriptProjectSourceGraph final
	{
	public:
		static bool Compile(
			const FAngelscriptProjectSourceGraphCompileRequest& Request,
			FAngelscriptProjectSourceGraphConsumer Consumer,
			FString* OutError = nullptr);
	};
}
