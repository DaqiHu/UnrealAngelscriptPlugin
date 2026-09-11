/* Maintained Unreal AngelScript fork — family-aware typed AST casts. */
#pragma once

#include "frontend/as_decl.h"
#include "frontend/as_stmt.h"

#include <type_traits>

BEGIN_AS_NAMESPACE

template <typename To, typename From>
	bool isa(const From* Value)
	{
		using RawFrom = std::remove_cv_t<From>;
		static_assert(std::is_base_of_v<asCASTNode, RawFrom>, "AST casts require a typed AST node");
		static_assert(std::is_base_of_v<asCASTNode, To>, "AST casts require a typed AST target");
		return Value != nullptr && To::classof(static_cast<const asCASTNode*>(Value));
	}

	template <typename To, typename From>
	To* dyn_cast(From* Value)
	{
		return isa<To>(Value) ? static_cast<To*>(static_cast<asCASTNode*>(Value)) : nullptr;
	}

	template <typename To, typename From>
	const To* dyn_cast(const From* Value)
	{
		return isa<To>(Value) ? static_cast<const To*>(static_cast<const asCASTNode*>(Value)) : nullptr;
	}

	template <typename To, typename From>
	To* cast(From* Value)
	{
		asASSERT(isa<To>(Value));
		return static_cast<To*>(static_cast<asCASTNode*>(Value));
	}

	template <typename To, typename From>
	const To* cast(const From* Value)
	{
		asASSERT(isa<To>(Value));
		return static_cast<const To*>(static_cast<const asCASTNode*>(Value));
	}

END_AS_NAMESPACE
