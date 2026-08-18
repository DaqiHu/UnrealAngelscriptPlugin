#pragma once

#include "CoreMinimal.h"

#include "Core/AngelscriptSourceProvider.h"
#include "StaticJIT/AngelscriptJITGeneration.h"

struct FAngelscriptEngine;

namespace AngelscriptEditor::StaticJIT
{
	enum class EAngelscriptJITSourceAuthorityCode : uint8
	{
		Current = 0,
		EngineUnavailable,
		InitialCompileFailed,
		WrongEnginePurpose,
		ProfileMismatch,
		ProjectRootMismatch,
		SourceDiscoveryFailed,
		InventoryMismatch,
		ContentMismatch,
		Unproven,
	};

	/**
	 * Pointer-free identities plus an immutable provider containing the exact
	 * source text/bytes observed after the primary Engine compile.
	 */
	struct ANGELSCRIPTEDITOR_API FAngelscriptJITSourceAuthoritySnapshot
	{
		EAngelscriptJITSourceAuthorityCode Code =
			EAngelscriptJITSourceAuthorityCode::EngineUnavailable;
		FString Detail;
		FAngelscriptHash256 ProfileIdentity;
		FAngelscriptHash256 InventoryIdentity;
		FAngelscriptHash256 ContentIdentity;
		uint32 SourceCount = 0;
		TSharedPtr<IAngelscriptSourceProvider> FrozenSourceProvider;

		bool IsCurrent() const
		{
			return Code == EAngelscriptJITSourceAuthorityCode::Current;
		}
	};

	/** Read-only Editor gate over the already-compiled primary Engine. */
	class ANGELSCRIPTEDITOR_API FAngelscriptJITSourceAuthority final
	{
	public:
		static FAngelscriptJITSourceAuthoritySnapshot Capture(
			FAngelscriptEngine& PrimaryEngine,
			FStringView ProjectFile,
			EAngelscriptJITTargetProfile TargetProfile);
	};
}
