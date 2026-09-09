#pragma once

#include "AngelscriptTypeBindInfoStore.h"

namespace UE::Angelscript::Private { class FAngelscriptBindCollection; }

/**
 * Serial GameThread capture. Provider modules and reflected objects must outlive
 * all Engines using the returned snapshot's native addresses and UE recipes.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptTypeBindInfoRecorder
{
public:
	static bool Capture(const UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FAngelscriptBindRecordingPolicy Policy, TSharedPtr<const FAngelscriptTypeBindInfoStore>& OutStore,
		FString& OutDiagnostic);
};
