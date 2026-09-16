#ifndef AS_BUILDER_CALLBACKS_H
#define AS_BUILDER_CALLBACKS_H

#include "frontend/Compile/as_builder_stages.h"
#include "as_compile_output.h"

#include "Containers/StringView.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asIBuilderCallbacks
{
public:
	virtual ~asIBuilderCallbacks() = default;

	virtual bool OnBeforeStage(asEBuilderStage Stage)
	{
		(void)Stage;
		return true;
	}

	virtual bool OnAfterStage(asEBuilderStage Stage)
	{
		(void)Stage;
		return true;
	}

	virtual bool OnDeclarationsReady(const asCDefinitionCompileOutput& Definitions)
	{
		(void)Definitions;
		return true;
	}

	virtual void OnBuildFinished(
		bool bSucceeded,
		asEBuilderStage Stage,
		FStringView Error,
		const asCCompileOutput& Output)
	{
		(void)bSucceeded;
		(void)Stage;
		(void)Error;
		(void)Output;
	}
};

END_AS_NAMESPACE

#endif
