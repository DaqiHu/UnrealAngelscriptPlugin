#pragma once

#include "as_module_definition_set.h"
#include "as_scriptengine.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCEngineCompileRegistration
{
public:
	explicit asCEngineCompileRegistration(asCScriptEngine& InEngine) : Engine(InEngine) {}
	asEMetadataRegistrationResult Register(TArray<TUniquePtr<asCModuleDefinitionSet>> Sets);
	asEMetadataRegistrationResult Register(
		TArray<TUniquePtr<asCModuleDefinitionSet>> Sets,
		const TMap<const asCTypeInfo*, int>& PublishedTypeIds,
		const TMap<const asCScriptFunction*, int>& PublishedFunctionIds);

private:
	asCScriptEngine& Engine;
};

END_AS_NAMESPACE
