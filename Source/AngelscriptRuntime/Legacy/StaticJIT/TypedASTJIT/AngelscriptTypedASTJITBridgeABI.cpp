#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBridgeABI.h"

#if AS_CAN_GENERATE_JIT

#include "StartAngelscriptHeaders.h"
#include "as_callfunc.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptTypedASTJITBridgeABI_Private
{
	static bool TrySpellScalarType(
		const asCDataType& Type,
		const bool bAllowVoid,
		FString& OutType)
	{
		OutType.Reset();
		if (!Type.IsValid() || Type.IsReference()
			|| Type.IsObjectHandle() || Type.GetTypeInfo() != nullptr)
		{
			return false;
		}
		switch (Type.GetTokenType())
		{
		case ttVoid:
			if (bAllowVoid)
			{
				OutType = TEXT("void");
				return true;
			}
			return false;
		case ttBool:
			OutType = TEXT("bool");
			return true;
		case ttInt8:
			OutType = TEXT("i8");
			return true;
		case ttUInt8:
			OutType = TEXT("u8");
			return true;
		case ttInt16:
			OutType = TEXT("i16");
			return true;
		case ttUInt16:
			OutType = TEXT("u16");
			return true;
		case ttInt:
			OutType = TEXT("i32");
			return true;
		case ttUInt:
			OutType = TEXT("u32");
			return true;
		case ttInt64:
			OutType = TEXT("i64");
			return true;
		case ttUInt64:
			OutType = TEXT("u64");
			return true;
		case ttFloat32:
			OutType = TEXT("f32");
			return true;
		case ttFloat64:
			OutType = TEXT("f64");
			return true;
		default:
			return false;
		}
	}
}

FAngelscriptTypedASTJITVMBridgeABIIdentity
FAngelscriptTypedASTJITVMBridgeABIIdentity::FromCanonical(
	const FStringView InCanonical)
{
	FAngelscriptTypedASTJITVMBridgeABIIdentity Result;
	Result.Canonical = FString(InCanonical);
	if (!Result.Canonical.IsEmpty())
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("Angelscript.TypedASTJIT.VMBridgeABI.v1"));
		Writer.WriteString(Result.Canonical);
		Result.Hash = Writer.FinalizeHash();
	}
	return Result;
}

bool FAngelscriptTypedASTJITVMBridgeABIIdentity::IsValid() const
{
	return !Canonical.IsEmpty()
		&& !Hash.IsZero()
		&& FromCanonical(Canonical).Hash == Hash;
}

bool TryBuildAngelscriptTypedASTJITVMBridgeABIIdentity(
	const asIScriptFunction& InScriptFunction,
	FAngelscriptTypedASTJITVMBridgeABIIdentity& OutIdentity,
	FString* OutError)
{
	using namespace AngelscriptTypedASTJITBridgeABI_Private;
	OutIdentity = {};
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	auto Fail = [OutError](const FString& Error)
	{
		if (OutError != nullptr)
		{
			*OutError = Error;
		}
		return false;
	};

	const auto& Function =
		static_cast<const asCScriptFunction&>(InScriptFunction);
	if (Function.funcType != asFUNC_SYSTEM
		|| Function.sysFuncIntf == nullptr)
	{
		return Fail(TEXT(
			"VMBridgeABIRequiresSystemFunction: only an installed system function has a current bridge target."));
	}

	FString ReturnType;
	if (!TrySpellScalarType(Function.returnType, true, ReturnType))
	{
		return Fail(TEXT(
			"VMBridgeABIUnsupportedReturnType: return type is not a reviewed by-value primitive scalar."));
	}
	TArray<FString> Parameters;
	Parameters.Reserve(Function.parameterTypes.GetLength());
	for (asUINT Index = 0; Index < Function.parameterTypes.GetLength(); ++Index)
	{
		FString ParameterType;
		const uint32 Direction = Index < Function.inOutFlags.GetLength()
			? static_cast<uint32>(Function.inOutFlags[Index]) : 0u;
		if (!TrySpellScalarType(
				Function.parameterTypes[Index], false, ParameterType)
			|| Direction != asTM_NONE)
		{
			return Fail(FString::Printf(TEXT(
				"VMBridgeABIUnsupportedParameter: ParameterIndex=%u is not a reviewed by-value primitive scalar."),
				Index));
		}
		Parameters.Add(FString::Printf(
			TEXT("%s:inout=%u"), *ParameterType, Direction));
	}

	FString Receiver = TEXT("none");
	if (Function.objectType != nullptr)
	{
		const FString ReceiverNamespace = UTF8_TO_TCHAR(
			Function.objectType->GetNamespace());
		const FString ReceiverName = UTF8_TO_TCHAR(
			Function.objectType->GetName());
		Receiver = ReceiverNamespace.IsEmpty()
			? ReceiverName
			: ReceiverNamespace + TEXT("::") + ReceiverName;
	}
	OutIdentity = FAngelscriptTypedASTJITVMBridgeABIIdentity::FromCanonical(
		FString::Printf(TEXT(
			"return=%s;params=(%s);receiver=%s;returnOnStack=%u;firstMetadata=%u;hidden=%d;determinesOutput=%d;worldContext=%u;compileOut=%u"),
			*ReturnType,
			*FString::Join(Parameters, TEXT(",")),
			*Receiver,
			Function.DoesReturnOnStack() ? 1u : 0u,
			static_cast<uint32>(
				Function.sysFuncIntf->passFirstParamMetaData),
			static_cast<int32>(Function.hiddenArgumentIndex),
			static_cast<int32>(Function.determinesOutputTypeArgumentIndex),
			Function.traits.GetTrait(asEFuncTrait::asTRAIT_USES_WORLDCONTEXT)
				? 1u : 0u,
			static_cast<uint32>(Function.compileOutType)));
	return OutIdentity.IsValid();
}

#endif // AS_CAN_GENERATE_JIT
