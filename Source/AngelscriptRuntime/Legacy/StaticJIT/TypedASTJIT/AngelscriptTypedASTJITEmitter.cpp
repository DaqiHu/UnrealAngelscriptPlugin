#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"

#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"

namespace
{
	FString EscapeTypedASTCppString(FStringView Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('\\'):
				Result += TEXT("\\\\");
				break;
			case TEXT('"'):
				Result += TEXT("\\\"");
				break;
			case TEXT('\n'):
				Result += TEXT("\\n");
				break;
			case TEXT('\r'):
				Result += TEXT("\\r");
				break;
			case TEXT('\t'):
				Result += TEXT("\\t");
				break;
			default:
				Result.AppendChar(Character);
				break;
			}
		}
		return Result;
	}
}

bool TrySpellAngelscriptTypedASTJITCppType(
	const asCDataType& Type,
	FString& OutType)
{
	return TrySpellAngelscriptStaticJITBodyCppType(Type, OutType);
}

bool EmitAngelscriptTypedASTJITBridgeCallSiteRows(
	const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls,
	FString& OutDefinitions,
	FString* OutError)
{
	OutDefinitions.Reset();
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	auto Fail = [OutError](FString Detail)
	{
		if (OutError != nullptr)
		{
			*OutError = MoveTemp(Detail);
		}
		return false;
	};

	TMap<FString, const FAngelscriptTypedASTJITNativeCallEmissionPlan*>
		EmittedBridgeCallSites;
	for (const FAngelscriptTypedASTJITNativeCallEmissionPlan& NativeCall
		: NativeCalls)
	{
		if (NativeCall.Route
				!= EAngelscriptTypedASTJITCallEmissionRoute::Bridge
			&& NativeCall.Route
				!= EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding)
		{
			continue;
		}
		if (const FAngelscriptTypedASTJITNativeCallEmissionPlan* const* Existing =
			EmittedBridgeCallSites.Find(NativeCall.CallSiteSymbol))
		{
			if ((*Existing)->CanonicalDeclaration
					!= NativeCall.CanonicalDeclaration
				|| (*Existing)->RegisteredTarget
					!= NativeCall.RegisteredTarget
				|| !((*Existing)->StableTargetKey
					== NativeCall.StableTargetKey)
				|| !((*Existing)->ExpectedABI == NativeCall.ExpectedABI)
				|| (*Existing)->ReferenceSlotIndex
					!= NativeCall.ReferenceSlotIndex)
			{
				return Fail(TEXT(
					"Bridge call-site identifier aliases different immutable metadata"));
			}
			continue;
		}
		EmittedBridgeCallSites.Add(NativeCall.CallSiteSymbol, &NativeCall);
		OutDefinitions += FString::Printf(TEXT(
			"static const FAngelscriptTypedASTJITBoundCallSite %s = {\n"
			"\tTEXT(\"%s\"),\n"
			"\tTEXT(\"%s\"),\n"
			"\tFAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(\"%s\"))) }},\n"
			"\tFAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(\"%s\"))) },\n"
			"\t%d\n"
			"};\n\n"),
			*NativeCall.CallSiteSymbol,
			*EscapeTypedASTCppString(NativeCall.CanonicalDeclaration),
			*EscapeTypedASTCppString(NativeCall.RegisteredTarget),
			*NativeCall.StableTargetKey.Hash.ToHexString(),
			*NativeCall.ExpectedABI.ToHexString(),
			NativeCall.ReferenceSlotIndex);
	}
	return true;
}
