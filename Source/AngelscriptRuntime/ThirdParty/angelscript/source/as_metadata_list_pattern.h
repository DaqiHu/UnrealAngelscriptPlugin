#pragma once
#include "as_datatype.h"
#include "Containers/Array.h"

BEGIN_AS_NAMESPACE
class asCScriptFunction;
enum class asEMetadataListPatternKind : asBYTE { List, Repeat, RepeatSame, Value };

// Host-owned list-factory contract, not generated source or executable storage.
// A Value uses either an actual concrete type or an explicit template argument
// slot. Repetition is terminal within its containing list, matching AS patterns.
struct asSMetadataListPattern
{
	asEMetadataListPatternKind Kind = asEMetadataListPatternKind::List;
	asCDataType ValueType;
	asUINT TemplateParameterIndex = ~asUINT(0);
	TArray<asSMetadataListPattern> Children;
};
struct asSMetadataListInitializer
{
	asCScriptFunction* Factory = nullptr;
	asSMetadataListPattern Pattern;
};
END_AS_NAMESPACE
