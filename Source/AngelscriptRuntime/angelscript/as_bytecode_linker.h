/* Maintained Unreal AngelScript fork — transactional bytecode image linker. */
#pragma once

#include "as_bytecode_image.h"
#include "as_execution_snapshot.h"
#include "as_scriptengine.h"

BEGIN_AS_NAMESPACE

ANGELSCRIPTRUNTIME_API asEByteCodeLinkStatus asLinkByteCodeImage(
	asCScriptEngine& Engine,
	const asCByteCodeImage& Image,
	asCExecutableSnapshot*& OutSnapshot);

END_AS_NAMESPACE
