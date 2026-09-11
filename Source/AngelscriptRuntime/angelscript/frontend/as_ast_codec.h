/* Maintained Unreal AngelScript fork — bounded versioned typed AST projection codec. */
#pragma once

#include "frontend/as_ast_projection.h"

BEGIN_AS_NAMESPACE

enum class asEASTCodecStatus : uint8
	{
		Succeeded,
		Malformed,
		IncompatibleVersion,
		UnsupportedVersion,
		BudgetExceeded,
		InvalidIndex,
		InvalidPayload,
	};

	struct asSASTCodecBudgets
	{
		uint32 MaxNodes = 4096;
		uint32 MaxEdges = 16384;
		uint32 MaxBytes = 4 * 1024 * 1024;
		uint32 MaxTypeSyntaxDepth = 32;
	};

	class ANGELSCRIPTRUNTIME_API asCASTCodec
	{
	public:
		static constexpr uint16 CurrentVersion = 9;
		static asEASTCodecStatus Encode(const asCASTProjection& Projection, TArray<uint8>& OutBytes);
		static asEASTCodecStatus Decode(TConstArrayView<uint8> Bytes, asCASTProjection& OutProjection, const asSASTCodecBudgets& Budgets = {});
	};

END_AS_NAMESPACE
