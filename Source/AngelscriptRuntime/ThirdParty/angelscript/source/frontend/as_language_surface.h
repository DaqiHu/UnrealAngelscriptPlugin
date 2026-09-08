/* Permanent source-language boundaries; unrelated host mechanisms remain available. */
#pragma once

#include "frontend/as_source_manager.h"

BEGIN_AS_NAMESPACE

enum class asERemovedLanguageFeature : uint8
{
	ModuleSharing,
	FuncdefDeclaration,
	AnonymousFunction,
	ScriptException,
	ScriptCoroutine,
	UserTemplate,
	VirtualProperty,
};

// Allocated in the existing declaration producer range, shared by body diagnostics.
inline constexpr asUINT asRemovedLanguageFeatureDiagnostic = 3008;
ANGELSCRIPTRUNTIME_API const TCHAR* asRemovedLanguageFeatureName(asERemovedLanguageFeature Feature);

END_AS_NAMESPACE
