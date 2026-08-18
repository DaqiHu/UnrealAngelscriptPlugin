#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace TypedSemanticIRExternalImplicitThisTestSupport
{
	struct FExecutionObservation
	{
		int State = asERROR;
		int32 ReturnValue = 0;
		FString Exception;
	};

	inline FExecutionObservation ExecuteGlobalInt(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function)
	{
		FExecutionObservation Observation;
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return Observation;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return Observation;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int PrepareResult = Context->Prepare(Function);
		if (PrepareResult < 0)
		{
			Observation.State = PrepareResult;
			return Observation;
		}

		Observation.State = Context->Execute();
		if (Observation.State == asEXECUTION_FINISHED)
		{
			Observation.ReturnValue = static_cast<int32>(Context->GetReturnDWord());
		}
		else if (Observation.State == asEXECUTION_EXCEPTION)
		{
			Observation.Exception = UTF8_TO_TCHAR(
				Context->GetExceptionString() != nullptr
					? Context->GetExceptionString()
					: "");
		}
		return Observation;
	}

	inline TArray<asDWORD> CopyBytecode(asIScriptFunction* Function)
	{
		TArray<asDWORD> Copy;
		if (Function == nullptr)
		{
			return Copy;
		}

		asUINT WordCount = 0;
		const asDWORD* const Words = Function->GetByteCode(&WordCount);
		if (Words != nullptr && WordCount != 0)
		{
			Copy.Append(Words, static_cast<int32>(WordCount));
		}
		return Copy;
	}

	inline bool HasEngineLocalPointerOperand(const asBYTE Opcode)
	{
		switch (Opcode)
		{
		case asBC_ALLOC:
		case asBC_REFCPY:
		case asBC_RefCpyV:
		case asBC_OBJTYPE:
		case asBC_FinConstruct:
		case asBC_DestructScript:
		case asBC_CopyScript:
		case asBC_JitEntry:
		case asBC_CALLSYS:
		case asBC_Thiscall1:
		case asBC_FuncPtr:
		case asBC_PGA:
		case asBC_PshGPtr:
		case asBC_LDG:
		case asBC_PshG4:
		case asBC_LdGRdR4:
		case asBC_CpyGtoV4:
		case asBC_CpyVtoG4:
		case asBC_SetG4:
		case asBC_FREE:
			return true;
		default:
			return false;
		}
	}

	inline TArray<asDWORD> CopyNormalizedBytecode(asIScriptFunction* Function)
	{
		TArray<asDWORD> Result = CopyBytecode(Function);
		for (asUINT WordIndex = 0;
			 WordIndex < static_cast<asUINT>(Result.Num());)
		{
			const asBYTE Opcode =
				*reinterpret_cast<const asBYTE*>(&Result[WordIndex]);
			if (Opcode > asBC_MAXBYTECODE)
			{
				Result.Reset();
				return Result;
			}

			const int32 InstructionWords = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionWords <= 0
				|| WordIndex + static_cast<asUINT>(InstructionWords)
					> static_cast<asUINT>(Result.Num()))
			{
				Result.Reset();
				return Result;
			}

			if (HasEngineLocalPointerOperand(Opcode))
			{
				if (InstructionWords < 1 + AS_PTR_SIZE)
				{
					Result.Reset();
					return Result;
				}
				for (int32 PointerWord = 1;
					 PointerWord <= AS_PTR_SIZE;
					 ++PointerWord)
				{
					Result[WordIndex + static_cast<asUINT>(PointerWord)] = 0;
				}
			}
			WordIndex += static_cast<asUINT>(InstructionWords);
		}
		return Result;
	}

	inline FString DescribeBytecode(asIScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return TEXT("<null function>");
		}

		asUINT WordCount = 0;
		const asDWORD* const Words = Function->GetByteCode(&WordCount);
		if (Words == nullptr || WordCount == 0)
		{
			return TEXT("<empty bytecode>");
		}

		TArray<FString> Instructions;
		for (asUINT WordIndex = 0; WordIndex < WordCount;)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(&Words[WordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:<invalid opcode=%u word=0x%08x>"),
					WordIndex,
					static_cast<uint32>(static_cast<asBYTE>(Opcode)),
					Words[WordIndex]));
				break;
			}

			const int32 InstructionWords = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionWords <= 0
				|| WordIndex + static_cast<asUINT>(InstructionWords) > WordCount)
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:%hs<invalid size=%d>"),
					WordIndex,
					asBCInfo[Opcode].name,
					InstructionWords));
				break;
			}

			TArray<FString> EncodedWords;
			for (int32 Offset = 0; Offset < InstructionWords; ++Offset)
			{
				EncodedWords.Add(FString::Printf(
					TEXT("%08x"),
					Words[WordIndex + static_cast<asUINT>(Offset)]));
			}
			Instructions.Add(FString::Printf(
				TEXT("%u:%hs[%s]"),
				WordIndex,
				asBCInfo[Opcode].name,
				*FString::Join(EncodedWords, TEXT(","))));
			WordIndex += static_cast<asUINT>(InstructionWords);
		}
		return FString::Join(Instructions, TEXT("; "));
	}

	inline const asSTypedSemanticSymbol* FindSymbol(
		const asCTypedSemanticFunction& HIR,
		const asETypedSemanticSymbolKind Kind,
		const char* Name)
	{
		for (asUINT Index = 0; Index < HIR.symbols.GetLength(); ++Index)
		{
			const asSTypedSemanticSymbol& Symbol = HIR.symbols[Index];
			if (Symbol.kind == Kind && Symbol.name == Name)
			{
				return &Symbol;
			}
		}
		return nullptr;
	}

	inline const asSTypedSemanticExpression* FindResolvedCall(
		const asCTypedSemanticFunction& HIR,
		const int TargetFunctionId)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression = HIR.expressions[Index];
			if (Expression.kind == asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.resolvedFunctionId == TargetFunctionId)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	inline bool ExpressionAliasesReceiver(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticExpressionId ExpressionId,
		const asTypedSemanticSymbolId ReceiverSymbol)
	{
		if (!ExpressionId.IsValid()
			|| ExpressionId.value >= HIR.expressions.GetLength())
		{
			return false;
		}

		const asSTypedSemanticExpression& Expression =
			HIR.expressions[ExpressionId.value];
		if (Expression.kind == asETypedSemanticExpressionKind::Symbol)
		{
			return Expression.symbol == ReceiverSymbol;
		}
		return Expression.kind == asETypedSemanticExpressionKind::Unsupported
			&& Expression.unsupportedKind
				== asETypedSemanticUnsupportedKind::ObjectAccess
			&& Expression.operands.GetLength() == 1
			&& ExpressionAliasesReceiver(
				HIR,
				Expression.operands[0],
				ReceiverSymbol);
	}
}
