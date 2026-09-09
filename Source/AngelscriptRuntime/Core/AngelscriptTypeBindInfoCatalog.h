#pragma once

#include "AngelscriptTypeBindInfoStore.h"

namespace UE::Angelscript::Private { class FAngelscriptBindCollection; }

/** Runs only declaration-bearing provider callbacks into an existing mutable Store. */
class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoCatalog final
{
public:
	static bool RecordRuntimeTypeDeclarations(
		const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic);

	/** Records the catalog subset supported by the current generic image installer. */
	static bool RecordInstallableTypeDeclarations(
		const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic);

	/** Composes a bounded set of provider surfaces with an already-recorded declaration catalog. */
	static bool RecordSelectedProviders(
		const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		TConstArrayView<FName> ProviderNames,
		FAngelscriptTypeBindInfoStore& Store, FString& Diagnostic);
};
