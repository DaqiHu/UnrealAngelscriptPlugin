#pragma once
#include "frontend/as_ast_fwd.h"
#include "frontend/as_type_identity.h"

BEGIN_AS_NAMESPACE
inline asEFunctionDefinitionModifiers asGetFunctionDefinitionModifiers(asEFunctionModifiers Input)
	{
		asEFunctionDefinitionModifiers Result = asEFunctionDefinitionModifiers::None;
		struct FMapping { asEFunctionModifiers Syntax; asEFunctionDefinitionModifiers Definition; };
		static constexpr FMapping Mappings[] = {
			{asEFunctionModifiers::Final, asEFunctionDefinitionModifiers::Final},
			{asEFunctionModifiers::Override, asEFunctionDefinitionModifiers::Override},
			{asEFunctionModifiers::Property, asEFunctionDefinitionModifiers::Property},
			{asEFunctionModifiers::Explicit, asEFunctionDefinitionModifiers::Explicit},
			{asEFunctionModifiers::Mixin, asEFunctionDefinitionModifiers::Mixin},
			{asEFunctionModifiers::Local, asEFunctionDefinitionModifiers::Local},
			{asEFunctionModifiers::AcceptTemporary, asEFunctionDefinitionModifiers::AcceptTemporary},
			{asEFunctionModifiers::ExternalImplicitThis, asEFunctionDefinitionModifiers::ExternalImplicitThis},
			{asEFunctionModifiers::NoDiscard, asEFunctionDefinitionModifiers::NoDiscard},
			{asEFunctionModifiers::AllowDiscard, asEFunctionDefinitionModifiers::AllowDiscard},
			{asEFunctionModifiers::Generated, asEFunctionDefinitionModifiers::Generated},
			{asEFunctionModifiers::Deprecated, asEFunctionDefinitionModifiers::Deprecated},
			{asEFunctionModifiers::Defaults, asEFunctionDefinitionModifiers::Defaults},
			{asEFunctionModifiers::UnsafeDuringConstruction, asEFunctionDefinitionModifiers::UnsafeDuringConstruction},
		};
		for (const auto& Mapping : Mappings)
			if ((uint16(Input) & uint16(Mapping.Syntax)) != 0) Result = Result | Mapping.Definition;
		return Result;
	}
END_AS_NAMESPACE
