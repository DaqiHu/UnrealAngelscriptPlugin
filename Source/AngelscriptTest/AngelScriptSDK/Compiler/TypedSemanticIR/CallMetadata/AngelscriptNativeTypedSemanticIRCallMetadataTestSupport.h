#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_callfunc.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

namespace TypedSemanticIRCallMetadataTestSupport
{
	inline int32 HiddenInvocationCount = 0;
	inline int32 NativeUserDataMarker = 0;

	inline int32 InvokeHidden(
		const int32 Visible,
		const int32 Hidden)
	{
		++HiddenInvocationCount;
		return Visible * 10 + Hidden;
	}

	inline int32 InvokeMetadata(const int32 Value)
	{
		return Value;
	}

	inline void InvokeGeneric(asIScriptGeneric* const Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(Generic->GetArgDWord(0));
		}
	}

	struct FRegisteredCallMetadataFunctions
	{
		int Hidden = asERROR;
		int ScriptFunction = asERROR;
		int ScriptObjectType = asERROR;
		int GenericWithUserData = asERROR;
		int DeterminesOutputType = asERROR;
		asCObjectType* DeterminesBaseType = nullptr;
		asCObjectType* DeterminesDerivedType = nullptr;

		bool HasHiddenFixture() const
		{
			return Hidden >= 0;
		}

		bool HasNativeAbiFixtures() const
		{
			return ScriptFunction >= 0 && ScriptObjectType >= 0
				&& GenericWithUserData >= 0;
		}

		bool HasDeterminesOutputFixture() const
		{
			return DeterminesOutputType >= 0
				&& DeterminesBaseType != nullptr
				&& DeterminesDerivedType != nullptr;
		}
	};

	inline FRegisteredCallMetadataFunctions RegisterCallMetadataFunctions(
		asIScriptEngine& ScriptEngine,
		const bool bRegisterDeterminesOutput = true)
	{
		FRegisteredCallMetadataFunctions Result;
		const ASAutoCaller::FunctionCaller HiddenCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeHidden);
		const ASAutoCaller::FunctionCaller MetadataCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeMetadata);
		Result.Hidden = ScriptEngine.RegisterGlobalFunction(
			"int MetadataHidden(int Visible, int Hidden)",
			asFUNCTION(InvokeHidden),
			asCALL_CDECL,
			*(asFunctionCaller*)&HiddenCaller);
		Result.ScriptFunction = ScriptEngine.RegisterGlobalFunction(
			"int MetadataScriptFunction(int Value)",
			asFUNCTION(InvokeMetadata),
			asCALL_CDECL,
			*(asFunctionCaller*)&MetadataCaller);
		Result.ScriptObjectType = ScriptEngine.RegisterGlobalFunction(
			"int MetadataScriptObjectType(int Value)",
			asFUNCTION(InvokeMetadata),
			asCALL_CDECL,
			*(asFunctionCaller*)&MetadataCaller);
		Result.GenericWithUserData = ScriptEngine.RegisterGlobalFunction(
			"int MetadataGeneric(int Value)",
			asFUNCTION(InvokeGeneric),
			asCALL_GENERIC);

		if (bRegisterDeterminesOutput
			&& ScriptEngine.RegisterObjectType(
				"FDeterminesBase",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0
			&& ScriptEngine.RegisterObjectType(
				"FDeterminesDerived",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0)
		{
			Result.DeterminesBaseType = static_cast<asCObjectType*>(
				ScriptEngine.GetTypeInfoByDecl("FDeterminesBase"));
			Result.DeterminesDerivedType = static_cast<asCObjectType*>(
				ScriptEngine.GetTypeInfoByDecl("FDeterminesDerived"));
			if (Result.DeterminesBaseType != nullptr
				&& Result.DeterminesDerivedType != nullptr)
			{
				// The maintained fork's shadow relation is the same authoritative
				// relation consumed by ApplyDeterminesOutputType(). These are
				// test-only no-count types; no ownership is transferred here.
				Result.DeterminesDerivedType->shadowType =
					Result.DeterminesBaseType;
					Result.DeterminesOutputType =
						ScriptEngine.RegisterGlobalFunction(
						"FDeterminesBase MetadataDeterminesOutput(FDeterminesBase Value)",
						asFUNCTION(InvokeGeneric),
						asCALL_GENERIC);
			}
		}

		asCScriptFunction* const HiddenFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.Hidden));
		asCScriptFunction* const ScriptFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.ScriptFunction));
		asCScriptFunction* const ScriptObjectTypeFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.ScriptObjectType));
		asCScriptFunction* const GenericFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.GenericWithUserData));
		asCScriptFunction* const DeterminesFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.DeterminesOutputType));
		if (HiddenFunction != nullptr)
		{
			HiddenFunction->hiddenArgumentIndex = 1;
			HiddenFunction->hiddenArgumentDefault = "4";
		}
		if (ScriptFunction != nullptr && ScriptFunction->sysFuncIntf != nullptr)
		{
			ScriptFunction->sysFuncIntf->passFirstParamMetaData =
				asEFirstParamMetaData::ScriptFunction;
		}
		if (ScriptObjectTypeFunction != nullptr
			&& ScriptObjectTypeFunction->sysFuncIntf != nullptr)
		{
			ScriptObjectTypeFunction->sysFuncIntf->passFirstParamMetaData =
				asEFirstParamMetaData::ScriptObjectType;
		}
		if (GenericFunction != nullptr)
		{
			GenericFunction->userData = &NativeUserDataMarker;
		}
		if (DeterminesFunction != nullptr)
		{
			DeterminesFunction->determinesOutputTypeArgumentIndex = 0;
		}
		return Result;
	}

	inline const asSTypedSemanticExpression* FindResolvedCall(
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
				return &Expression;
			}
		}
		return nullptr;
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
		if (Context->Prepare(&Function) < 0
			|| Context->SetArgDWord(0, static_cast<asDWORD>(Argument)) < 0)
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
