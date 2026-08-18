#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace TypedSemanticIRCallRewriteTestSupport
{
	inline int32 NormalInvocationCount = 0;
	inline int32 ErasedInvocationCount = 0;
	inline int32 ReplacedInvocationCount = 0;
	inline int32 MethodChainInvocationCount = 0;

	inline int32 InvokeNormal(const int32 Value)
	{
		++NormalInvocationCount;
		return Value + 3;
	}

	inline void InvokeErased(const int32)
	{
		++ErasedInvocationCount;
	}

	inline int32 InvokeReplaced(const int32 First, const int32)
	{
		++ReplacedInvocationCount;
		return First + 1000;
	}

	struct FChainProbe
	{
		int32 Value = 0;

		FChainProbe& Chain(const int32)
		{
			++MethodChainInvocationCount;
			return *this;
		}
	};

	inline void ConstructChainProbe(FChainProbe* const Self)
	{
		new (Self) FChainProbe();
	}

	struct FRegisteredRewriteFunctions
	{
		int Normal = asERROR;
		int Erased = asERROR;
		int Replaced = asERROR;
		int MethodChain = asERROR;

		bool IsValid() const
		{
			return Normal >= 0 && Erased >= 0 && Replaced >= 0
				&& MethodChain >= 0;
		}
	};

	inline FRegisteredRewriteFunctions RegisterRewriteFunctions(
		asIScriptEngine& ScriptEngine)
	{
		FRegisteredRewriteFunctions Result;
		const ASAutoCaller::FunctionCaller NormalCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeNormal);
		const ASAutoCaller::FunctionCaller ErasedCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeErased);
		const ASAutoCaller::FunctionCaller ReplacedCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeReplaced);
		Result.Normal = ScriptEngine.RegisterGlobalFunction(
			"int RewriteNormal(int Value)",
			asFUNCTION(InvokeNormal),
			asCALL_CDECL,
			*(asFunctionCaller*)&NormalCaller);
		Result.Erased = ScriptEngine.RegisterGlobalFunction(
			"void RewriteErased(int Value)",
			asFUNCTION(InvokeErased),
			asCALL_CDECL,
			*(asFunctionCaller*)&ErasedCaller);
		Result.Replaced = ScriptEngine.RegisterGlobalFunction(
			"int RewriteReplaced(int First, int Ignored)",
			asFUNCTION(InvokeReplaced),
			asCALL_CDECL,
			*(asFunctionCaller*)&ReplacedCaller);

		const ASAutoCaller::FunctionCaller ConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructChainProbe);
		const ASAutoCaller::FunctionCaller MethodCaller =
			ASAutoCaller::MakeFunctionCaller(&FChainProbe::Chain);
		if (ScriptEngine.RegisterObjectType(
				"FCallRewriteChainProbe",
				sizeof(FChainProbe),
				asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FChainProbe>()
					| asOBJ_APP_CLASS_ALLINTS) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FCallRewriteChainProbe",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ConstructChainProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ConstructorCaller) >= 0
			&& ScriptEngine.RegisterObjectProperty(
				"FCallRewriteChainProbe",
				"int Value",
				asOFFSET(FChainProbe, Value)) >= 0)
		{
			Result.MethodChain = ScriptEngine.RegisterObjectMethod(
				"FCallRewriteChainProbe",
				"FCallRewriteChainProbe& Chain(int Ignored)",
				asMETHODPR(FChainProbe, Chain, (int32), FChainProbe&),
				asCALL_THISCALL,
				*(asFunctionCaller*)&MethodCaller);
		}

		asCScriptFunction* const ErasedFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.Erased));
		asCScriptFunction* const ReplacedFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.Replaced));
		asCScriptFunction* const MethodChainFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.MethodChain));
		if (ErasedFunction != nullptr)
		{
			ErasedFunction->compileOutType =
				asECompileOutType::CompileOutEntirely;
		}
		if (ReplacedFunction != nullptr)
		{
			ReplacedFunction->compileOutType =
				asECompileOutType::ReplaceWithFirstParam;
		}
		if (MethodChainFunction != nullptr)
		{
			MethodChainFunction->compileOutType =
				asECompileOutType::CompileOutAsMethodChain;
		}
		return Result;
	}

	inline const asSTypedSemanticExpression* FindCallRewrite(
		const asCTypedSemanticFunction& HIR,
		const asETypedSemanticCompileOutKind CompileOutKind)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				HIR.expressions[Index];
			if (Expression.kind
					== asETypedSemanticExpressionKind::CallRewrite
				&& Expression.callRewriteKind == CompileOutKind)
			{
				return &Expression;
			}
		}
		return nullptr;
	}

	inline bool HasResolvedCallTo(
		const asCTypedSemanticFunction& HIR,
		const int TargetFunctionId)
	{
		for (asUINT Index = 0; Index < HIR.expressions.GetLength(); ++Index)
		{
			const asSTypedSemanticExpression& Expression =
				HIR.expressions[Index];
			if (Expression.kind
					== asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.resolvedFunctionId == TargetFunctionId)
			{
				return true;
			}
		}
		return false;
	}

	inline int ExecuteGlobalIntWithInt(
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Function,
		const int32 Argument,
		int32& OutValue)
	{
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (Context == nullptr)
		{
			return asERROR;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		if (Context->Prepare(&Function) < 0)
		{
			return asERROR;
		}
		if (Function.GetParamCount() != 0
			&& Context->SetArgDWord(0, static_cast<asDWORD>(Argument)) < 0)
		{
			return asERROR;
		}
		const int State = Context->Execute();
		if (State == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}
		return State;
	}
}
