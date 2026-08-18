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
	enum class EAngelscriptProjectSourceGraphRequestKind : uint8
	{
		StaticJITArtifact = 0,
		DeveloperHIRDump,
	};

	struct FAngelscriptProjectSourceGraphCompileRequest
	{
		EAngelscriptProjectSourceGraphRequestKind RequestKind =
			EAngelscriptProjectSourceGraphRequestKind::StaticJITArtifact;
		FString ProjectFile;
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		EAngelscriptStaticJITCaptureProfile CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::Bytecode;
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
	 * Request kind changes diagnostics/cache scratch ownership only; it never
	 * selects a Static backend. The consumer must freeze pointer-free output
	 * synchronously before this call destroys the generation Engine.
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
