#pragma once
#include "as_metadata_list_pattern.h"
#include "frontend/AST/as_type.h"
#include "Templates/Function.h"

BEGIN_AS_NAMESPACE
struct asSResolvedListPattern
	{
		asEMetadataListPatternKind Kind = asEMetadataListPatternKind::List;
		asCQualType ValueType;
		TArray<asSResolvedListPattern> Children;
	};
	struct asSListInitialization
	{
		asSStableKey Factory;
		asSResolvedListPattern Pattern;
	};
	using asFListInitializationLookup = TFunction<bool(asCQualType, asSListInitialization&)>;
END_AS_NAMESPACE
