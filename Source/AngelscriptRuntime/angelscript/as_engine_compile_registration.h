#pragma once

#include "as_definitions.h"
#include "as_scriptengine.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCEngineCompileRegistration
{
public:
	explicit asCEngineCompileRegistration(asCScriptEngine& InEngine) : Engine(InEngine) {}
	asERegistrationResult Register(TArray<TUniquePtr<asCDefinitions>> Sets);
	asERegistrationResult Register(
		TArray<TUniquePtr<asCDefinitions>> Sets,
		const TMap<const asCTypeInfo*, int>& PublishedTypeIds,
		const TMap<const asCScriptFunction*, int>& PublishedFunctionIds);

private:
	asCScriptEngine& Engine;
};

END_AS_NAMESPACE
