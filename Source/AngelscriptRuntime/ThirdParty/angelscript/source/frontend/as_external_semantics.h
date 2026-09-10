/* Transient read-only views of frozen host callables. Metadata remains the owner. */
#pragma once

#include "as_metadata_image.h"
#include "frontend/as_type_identity.h"
#include <memory>

BEGIN_AS_NAMESPACE

class asCFunctionType;
class asCScriptFunction;

struct asSExternalCallable
	{
		asSStableKey Key;
		asCFunctionType* Signature = nullptr;
		asCScriptFunction* Function = nullptr;
		asSStableKey OwnerKey;
		asCMetadataImage* Image = nullptr;
		FString QualifiedName;
		bool bConstMethod = false;
		bool HasTypedDefaults = false;
	};

END_AS_NAMESPACE
