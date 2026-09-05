#include "as_restore.h"
#include "as_texts.h"

BEGIN_AS_NAMESPACE

asCReader::asCReader(asCModule* _module, asIBinaryStream* _stream,
	asCScriptEngine* _engine,
	const asSBuildArtifactRestoreContext* _restoreContext)
	: module(_module), stream(_stream), engine(_engine),
	  buildArtifactRestoreContext(_restoreContext), error(false), bytesRead(0),
	  validatingFunctionArtifact(false), functionArtifactValidationStage(asFUNCTION_ARTIFACT_STAGE_NONE),
	  aggregateRestoreStage(0),
	  functionArtifactStackNeeded(0), functionArtifactObjVariablesOnHeap(0),
	  functionArtifactRuntimeStateOffset(asUINT(-1)),
	  functionArtifactStackNeededOffset(asUINT(-1)),
	  functionArtifactObjVariablesOnHeapOffset(asUINT(-1)),
	  functionArtifactObjectVariableCountOffset(asUINT(-1)),
	  functionArtifactFirstObjectVariableTypeOffset(asUINT(-1)),
	  functionArtifactFirstObjectVariablePositionOffset(asUINT(-1)),
	  lastCompositeProp(0)
{
}

asCReader::~asCReader()
{
	if( engine != 0 && engine->stringFactory != 0 )
		for( asUINT n = 0; n < usedStringConstants.GetLength(); ++n )
			engine->stringFactory->ReleaseStringConstant(usedStringConstants[n]);
	usedStringConstants.SetLength(0);
}

int asCReader::ValidateFunctionArtifact(asUINT expectedSize, asSFunctionArtifactValidationDiagnostics *diagnostics)
{
	(void)expectedSize;
	if( diagnostics )
	{
		diagnostics->result = asNOT_SUPPORTED;
		diagnostics->hadError = true;
	}
	return asNOT_SUPPORTED;
}

int asCReader::RestoreFunctionArtifactDetached(asUINT expectedSize,
	asCScriptFunction **outFunction,
	asSFunctionArtifactValidationDiagnostics *diagnostics)
{
	(void)expectedSize;
	if( outFunction )
		*outFunction = 0;
	if( diagnostics )
	{
		diagnostics->result = asNOT_SUPPORTED;
		diagnostics->hadError = true;
	}
	return asNOT_SUPPORTED;
}

int asCReader::CommitFunctionArtifactToExisting(asCScriptFunction *artifact,
	asCScriptFunction *target)
{
	(void)artifact;
	(void)target;
	return asNOT_SUPPORTED;
}

int asCReader::RestoreGlobalFunctionArtifact(asUINT expectedSize,
	asCScriptFunction **outFunction,
	asSFunctionArtifactValidationDiagnostics *diagnostics)
{
	(void)expectedSize;
	if( outFunction )
		*outFunction = 0;
	if( diagnostics )
	{
		diagnostics->result = asNOT_SUPPORTED;
		diagnostics->hadError = true;
	}
	return asNOT_SUPPORTED;
}

END_AS_NAMESPACE
