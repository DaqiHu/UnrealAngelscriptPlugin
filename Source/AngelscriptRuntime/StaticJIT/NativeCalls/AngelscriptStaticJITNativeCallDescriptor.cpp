#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"

#if AS_CAN_GENERATE_JIT

#include "StartAngelscriptHeaders.h"
#include "source/as_callfunc.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptStaticJITNativeCallDescriptor_Private
{
	static bool TrySpellScalarABIType(
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

	static constexpr uint8 KnownRouteFlags =
		static_cast<uint8>(
			EAngelscriptStaticJITNativeCallRouteFlags::DirectCall)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallRouteFlags::RequiresProcessEvent)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallRouteFlags::MaySuspend)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall);

	static constexpr uint8 KnownLifetimeFlags =
		static_cast<uint8>(
			EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallLifetimeFlags::BorrowedInputReferences)
		| static_cast<uint8>(
			EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueObjectArguments);

	static constexpr EAngelscriptStaticJITNativeCallLifetimeFlags
		RequiredScalarLifetime =
		EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly
		| EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
		| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture;

	static const TCHAR* LexToString(
		const EAngelscriptStaticJITNativeCallLinkage Linkage)
	{
		switch (Linkage)
		{
		case EAngelscriptStaticJITNativeCallLinkage::None:
			return TEXT("None");
		case EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol:
			return TEXT("ExportedSymbol");
		case EAngelscriptStaticJITNativeCallLinkage::HeaderInline:
			return TEXT("HeaderInline");
		case EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk:
			return TEXT("ExportedRuntimeThunk");
		case EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable:
			return TEXT("ExportedRuntimeCallable");
		case EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate:
			return TEXT("ProviderPrivate");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* LexToString(
		const EAngelscriptStaticJITNativeCallABIDomain Domain)
	{
		switch (Domain)
		{
		case EAngelscriptStaticJITNativeCallABIDomain::None:
			return TEXT("None");
		case EAngelscriptStaticJITNativeCallABIDomain::Scalar:
			return TEXT("Scalar");
		case EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD:
			return TEXT("BorrowedManagedPOD");
		case EAngelscriptStaticJITNativeCallABIDomain::ManagedValuesDeferred:
			return TEXT("ManagedValuesDeferred");
		case EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge:
			return TEXT("VMScalarBridge");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* LexToString(
		const EAngelscriptStaticJITNativeCallVisibility Visibility)
	{
		switch (Visibility)
		{
		case EAngelscriptStaticJITNativeCallVisibility::Unspecified:
			return TEXT("Unspecified");
		case EAngelscriptStaticJITNativeCallVisibility::External:
			return TEXT("External");
		case EAngelscriptStaticJITNativeCallVisibility::HeaderInline:
			return TEXT("HeaderInline");
		case EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate:
			return TEXT("ProviderPrivate");
		case EAngelscriptStaticJITNativeCallVisibility::Internal:
			return TEXT("Internal");
		default:
			return TEXT("Unknown");
		}
	}

	static FAngelscriptStaticJITNativeCallValidation Fail(
		const EAngelscriptStaticJITNativeCallValidationCode Code,
		const FString& Detail)
	{
		FAngelscriptStaticJITNativeCallValidation Result;
		Result.Code = Code;
		Result.Detail = Detail;
		return Result;
	}

	static bool ContainsUnsafeText(const FString& Value)
	{
		return Value.Contains(TEXT("\n"))
			|| Value.Contains(TEXT("\r"))
			|| Value.Contains(TEXT("\""));
	}

	static bool IsReviewedCppTypeCharacter(const TCHAR Character)
	{
		return FChar::IsAlnum(Character)
			|| FChar::IsWhitespace(Character)
			|| Character == TEXT('_')
			|| Character == TEXT(':')
			|| Character == TEXT('<')
			|| Character == TEXT('>')
			|| Character == TEXT(',')
			|| Character == TEXT('*')
			|| Character == TEXT('&')
			|| Character == TEXT('(')
			|| Character == TEXT(')')
			|| Character == TEXT('[')
			|| Character == TEXT(']');
	}

	static bool TryParseReviewedCppCallableSignature(
		const FString& Input,
		FAngelscriptStaticJITNativeCallCppSignature& OutSignature,
		FString& OutError)
	{
		constexpr int32 MaxSignatureLength = 4096;
		constexpr int32 MaxNestingDepth = 64;
		constexpr int32 MaxParameterCount = 256;

		OutSignature = {};
		OutError.Reset();
		const FString Signature = Input.TrimStartAndEnd();
		if (Signature.IsEmpty() || Signature.Len() > MaxSignatureLength)
		{
			OutError = TEXT(
				"the reviewed callable type is empty or exceeds the bounded length");
			return false;
		}

		for (const TCHAR Character : Signature)
		{
			if (!IsReviewedCppTypeCharacter(Character))
			{
				OutError = FString::Printf(TEXT(
					"the reviewed callable type contains unsupported character U+%04x"),
					static_cast<uint32>(Character));
				return false;
			}
		}

		int32 AngleDepth = 0;
		int32 SquareDepth = 0;
		int32 ParenthesisDepth = 0;
		int32 OuterOpen = INDEX_NONE;
		int32 OuterClose = INDEX_NONE;
		for (int32 Index = 0; Index < Signature.Len(); ++Index)
		{
			const TCHAR Character = Signature[Index];
			switch (Character)
			{
			case TEXT('<'):
				++AngleDepth;
				break;
			case TEXT('>'):
				if (--AngleDepth < 0)
				{
					OutError = TEXT(
						"the reviewed callable type has an unmatched '>'");
					return false;
				}
				break;
			case TEXT('['):
				++SquareDepth;
				break;
			case TEXT(']'):
				if (--SquareDepth < 0)
				{
					OutError = TEXT(
						"the reviewed callable type has an unmatched ']'");
					return false;
				}
				break;
			case TEXT('('):
				if (OuterOpen == INDEX_NONE
					&& AngleDepth == 0
					&& SquareDepth == 0
					&& ParenthesisDepth == 0)
				{
					OuterOpen = Index;
				}
				++ParenthesisDepth;
				break;
			case TEXT(')'):
				if (--ParenthesisDepth < 0)
				{
					OutError = TEXT(
						"the reviewed callable type has an unmatched ')'");
					return false;
				}
				if (OuterOpen != INDEX_NONE && ParenthesisDepth == 0)
				{
					OuterClose = Index;
					if (!Signature.Mid(Index + 1).TrimStartAndEnd().IsEmpty())
					{
						OutError = TEXT(
							"the reviewed callable type has tokens after its outer parameter list");
						return false;
					}
					Index = Signature.Len();
				}
				break;
			default:
				break;
			}

			if (AngleDepth > MaxNestingDepth
				|| SquareDepth > MaxNestingDepth
				|| ParenthesisDepth > MaxNestingDepth)
			{
				OutError = TEXT(
					"the reviewed callable type exceeds the bounded nesting depth");
				return false;
			}
		}

		if (OuterOpen == INDEX_NONE || OuterClose == INDEX_NONE
			|| AngleDepth != 0 || SquareDepth != 0 || ParenthesisDepth != 0)
		{
			OutError = TEXT(
				"the reviewed callable type must be one balanced return-type(parameter-types...) form");
			return false;
		}

		OutSignature.ReturnType =
			Signature.Left(OuterOpen).TrimStartAndEnd();
		if (OutSignature.ReturnType.IsEmpty())
		{
			OutError = TEXT(
				"the reviewed callable type has no return type");
			return false;
		}

		auto AddParameter = [&](const int32 Begin, const int32 End)
		{
			FString Parameter = Signature.Mid(Begin, End - Begin)
				.TrimStartAndEnd();
			if (Parameter.IsEmpty() || Parameter == TEXT("void"))
			{
				OutError = TEXT(
					"the reviewed callable type has an empty or void parameter slot");
				return false;
			}
			if (OutSignature.ParameterTypes.Num() >= MaxParameterCount)
			{
				OutError = TEXT(
					"the reviewed callable type exceeds the bounded parameter count");
				return false;
			}
			OutSignature.ParameterTypes.Add(MoveTemp(Parameter));
			return true;
		};

		const int32 ParametersBegin = OuterOpen + 1;
		if (ParametersBegin != OuterClose)
		{
			AngleDepth = 0;
			SquareDepth = 0;
			ParenthesisDepth = 0;
			int32 ParameterBegin = ParametersBegin;
			for (int32 Index = ParametersBegin; Index < OuterClose; ++Index)
			{
				switch (Signature[Index])
				{
				case TEXT('<'): ++AngleDepth; break;
				case TEXT('>'): --AngleDepth; break;
				case TEXT('['): ++SquareDepth; break;
				case TEXT(']'): --SquareDepth; break;
				case TEXT('('): ++ParenthesisDepth; break;
				case TEXT(')'): --ParenthesisDepth; break;
				case TEXT(','):
					if (AngleDepth == 0 && SquareDepth == 0
						&& ParenthesisDepth == 0)
					{
						if (!AddParameter(ParameterBegin, Index))
						{
							return false;
						}
						ParameterBegin = Index + 1;
					}
					break;
				default:
					break;
				}
			}
			if (!AddParameter(ParameterBegin, OuterClose))
			{
				return false;
			}
		}

		return OutSignature.IsValid();
	}
}

FAngelscriptStaticJITNativeABIIdentity
FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
	const FStringView InCanonical)
{
	FAngelscriptStaticJITNativeABIIdentity Result;
	Result.Canonical = FString(InCanonical);
	if (!Result.Canonical.IsEmpty())
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("Angelscript.StaticJIT.NativeABI.v1"));
		Writer.WriteString(Result.Canonical);
		Result.Hash = Writer.FinalizeHash();
	}
	return Result;
}

bool FAngelscriptStaticJITNativeABIIdentity::IsValid() const
{
	return !Canonical.IsEmpty()
		&& !Hash.IsZero()
		&& FromCanonical(Canonical).Hash == Hash;
}

bool TryBuildAngelscriptStaticJITNativeABIIdentity(
	const asIScriptFunction& InScriptFunction,
	FAngelscriptStaticJITNativeABIIdentity& OutIdentity,
	FString* OutError)
{
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
			"NativeABIRequiresSystemFunction: only an installed system function has a registered native ABI."));
	}

	auto FormatType = [](const asCDataType& Type)
	{
		const asCString Formatted = Type.Format(nullptr, true, false);
		return FString(UTF8_TO_TCHAR(Formatted.AddressOf()));
	};
	const FString ReturnType = FormatType(Function.returnType);
	if (ReturnType.IsEmpty())
	{
		return Fail(TEXT(
			"NativeABIInvalidReturnType: registered return type has no canonical spelling."));
	}

	TArray<FString> Parameters;
	TArray<FString> Defaults;
	uint32 ParameterStackDWords = 0;
	Parameters.Reserve(Function.parameterTypes.GetLength());
	Defaults.Reserve(Function.parameterTypes.GetLength());
	for (asUINT Index = 0;
		Index < Function.parameterTypes.GetLength(); ++Index)
	{
		const int32 ParameterStackSize =
			Function.parameterTypes[Index].GetSizeOnStackDWords();
		if (ParameterStackSize < 0
			|| ParameterStackDWords
				> MAX_uint32 - static_cast<uint32>(ParameterStackSize))
		{
			return Fail(FString::Printf(TEXT(
				"NativeABIInvalidParameterStackSize: ParameterIndex=%u has an invalid canonical stack size."),
				Index));
		}
		ParameterStackDWords += static_cast<uint32>(ParameterStackSize);
		const FString Type = FormatType(Function.parameterTypes[Index]);
		if (Type.IsEmpty())
		{
			return Fail(FString::Printf(TEXT(
				"NativeABIInvalidParameterType: ParameterIndex=%u has no canonical spelling."),
				Index));
		}
		const uint32 InOut = Index < Function.inOutFlags.GetLength()
			? static_cast<uint32>(Function.inOutFlags[Index]) : 0u;
		Parameters.Add(FString::Printf(
			TEXT("%s:inout=%u"), *Type, InOut));
		Defaults.Add(Index < Function.defaultArgs.GetLength()
			&& Function.defaultArgs[Index] != nullptr
				? FString(UTF8_TO_TCHAR(
					Function.defaultArgs[Index]->AddressOf()))
				: FString(TEXT("<none>")));
	}

	FString Receiver = TEXT("none");
	if (Function.objectType != nullptr)
	{
		Receiver = FString::Printf(TEXT("%s::%s"),
			UTF8_TO_TCHAR(Function.objectType->GetNamespace()),
			UTF8_TO_TCHAR(Function.objectType->GetName()));
	}
	const asSSystemFunctionInterface& Interface = *Function.sysFuncIntf;
	// sysFuncIntf::paramSize is only populated for asCALL_GENERIC when
	// PrepareEngine runs. Derive the same value from the already-authoritative
	// registered parameter layout so one installed ABI has one identity both
	// before and after that preparation boundary.
	const FString Canonical = FString::Printf(TEXT(
		"callconv=%u;return=%s;params=(%s);defaults=(%s);receiver=%s;returnOnStack=%u;firstMetadata=%u;hidden=%d;determinesOutput=%d;traits=%u;compileOut=%u;paramSize=%u;returnSize=%u"),
		static_cast<uint32>(Interface.callConv), *ReturnType,
		*FString::Join(Parameters, TEXT(",")),
		*FString::Join(Defaults, TEXT(",")), *Receiver,
		Function.DoesReturnOnStack() ? 1u : 0u,
		static_cast<uint32>(Interface.passFirstParamMetaData),
		static_cast<int32>(Function.hiddenArgumentIndex),
		static_cast<int32>(Function.determinesOutputTypeArgumentIndex),
		static_cast<uint32>(Function.traits.traits),
		static_cast<uint32>(Function.compileOutType),
		ParameterStackDWords,
		static_cast<uint32>(Interface.scriptReturnSize));
	OutIdentity = FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
		Canonical);
	return OutIdentity.IsValid();
}

bool TryBuildAngelscriptStaticJITScalarABIIdentity(
	const asIScriptFunction& InScriptFunction,
	FAngelscriptStaticJITScalarABIIdentity& OutIdentity,
	FString* OutError)
{
	using namespace AngelscriptStaticJITNativeCallDescriptor_Private;
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

	const auto& ScriptFunction =
		static_cast<const asCScriptFunction&>(InScriptFunction);
	if (ScriptFunction.funcType != asFUNC_SYSTEM
		|| ScriptFunction.sysFuncIntf == nullptr)
	{
		return Fail(TEXT(
			"NativeScalarABIRequiresSystemFunction: only a registered system function owns a native ABI."));
	}
	if (ScriptFunction.DoesReturnOnStack()
		|| ScriptFunction.sysFuncIntf->passFirstParamMetaData
			!= asEFirstParamMetaData::None)
	{
		return Fail(TEXT(
			"NativeScalarABIUnsupportedHiddenState: return-in-memory and hidden metadata parameters are outside the scalar direct-call contract."));
	}

	FString CallingConvention;
	bool bHasReceiver = false;
	switch (ScriptFunction.sysFuncIntf->callConv)
	{
	case ICC_CDECL:
		CallingConvention = TEXT("cdecl");
		break;
	case ICC_STDCALL:
		CallingConvention = TEXT("stdcall");
		break;
	case ICC_THISCALL:
		CallingConvention = TEXT("thiscall");
		bHasReceiver = true;
		break;
	default:
		return Fail(FString::Printf(TEXT(
			"NativeScalarABIUnsupportedCallingConvention: InternalCallConv=%d."),
			static_cast<int32>(ScriptFunction.sysFuncIntf->callConv)));
	}

	FString ReturnType;
	if (!TrySpellScalarABIType(
		ScriptFunction.returnType, true, ReturnType))
	{
		return Fail(TEXT(
			"NativeScalarABIUnsupportedReturnType: return type is not a by-value reviewed primitive scalar."));
	}
	TArray<FString> Parameters;
	if (bHasReceiver)
	{
		Parameters.Add(TEXT("ptr"));
	}
	for (asUINT Index = 0;
		Index < ScriptFunction.parameterTypes.GetLength(); ++Index)
	{
		FString Parameter;
		if (!TrySpellScalarABIType(
			ScriptFunction.parameterTypes[Index], false, Parameter)
			|| (Index < ScriptFunction.inOutFlags.GetLength()
				&& ScriptFunction.inOutFlags[Index] != asTM_NONE))
		{
			return Fail(FString::Printf(TEXT(
				"NativeScalarABIUnsupportedParameter: ParameterIndex=%u is not a by-value reviewed primitive scalar."),
				Index));
		}
		Parameters.Add(MoveTemp(Parameter));
	}

	OutIdentity = FAngelscriptStaticJITScalarABIIdentity::FromCanonical(
		FString::Printf(TEXT("%s:%s(%s)"),
			*CallingConvention, *ReturnType,
			*FString::Join(Parameters, TEXT(","))));
	return OutIdentity.IsValid();
}

FAngelscriptStaticJITScalarABIIdentity
FAngelscriptStaticJITScalarABIIdentity::FromCanonical(
	const FStringView InCanonical)
{
	FAngelscriptStaticJITScalarABIIdentity Result;
	Result.Canonical = FString(InCanonical);
	if (!Result.Canonical.IsEmpty())
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("Angelscript.StaticJIT.NativeScalarABI.v1"));
		Writer.WriteString(Result.Canonical);
		Result.Hash = Writer.FinalizeHash();
	}
	return Result;
}

bool FAngelscriptStaticJITScalarABIIdentity::IsValid() const
{
	return !Canonical.IsEmpty()
		&& !Hash.IsZero()
		&& FromCanonical(Canonical).Hash == Hash;
}

bool FAngelscriptStaticJITNativeCallDescriptor::IsAbsent() const
{
	return Linkage == EAngelscriptStaticJITNativeCallLinkage::None
		&& Visibility
			== EAngelscriptStaticJITNativeCallVisibility::Unspecified
		&& Symbol.IsEmpty()
		&& Include.IsEmpty()
		&& OwningModule.IsNone()
		&& ApiMacro.IsEmpty()
		&& CppCallableSignature.IsEmpty()
		&& NativeABI.Canonical.IsEmpty()
		&& NativeABI.Hash.IsZero()
		&& ABIDomain == EAngelscriptStaticJITNativeCallABIDomain::None
		&& CompileOutPolicy
			== EAngelscriptStaticJITNativeCallCompileOutPolicy::None
		&& DefaultArgumentPolicy
			== EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::None
		&& DefaultArgumentCount == 0
		&& WorldContextPolicy
			== EAngelscriptStaticJITNativeCallWorldContextPolicy::None
		&& ExceptionPolicy
			== EAngelscriptStaticJITNativeCallExceptionPolicy::Unspecified
		&& ScalarABI.Canonical.IsEmpty()
		&& ScalarABI.Hash.IsZero()
		&& RouteFlags == EAngelscriptStaticJITNativeCallRouteFlags::None
		&& LifetimeFlags
			== EAngelscriptStaticJITNativeCallLifetimeFlags::None;
}

FString FAngelscriptStaticJITNativeCallDescriptor::DumpNormalized() const
{
	using namespace AngelscriptStaticJITNativeCallDescriptor_Private;
	return FString::Printf(TEXT(
		"linkage=%s visibility=%s symbol=%s include=%s module=%s api=%s cpp=%s nativeAbi=%s nativeAbiHash=%s domain=%s compileOut=%u defaults=%u:%u world=%u exception=%u scalarAbi=%s scalarAbiHash=%s route=0x%02x lifetime=0x%02x"),
		LexToString(Linkage), LexToString(Visibility), *Symbol, *Include,
		*OwningModule.ToString(), *ApiMacro, *CppCallableSignature,
		*NativeABI.Canonical, *NativeABI.Hash.ToHexString(),
		LexToString(ABIDomain), static_cast<uint8>(CompileOutPolicy),
		static_cast<uint8>(DefaultArgumentPolicy), DefaultArgumentCount,
		static_cast<uint8>(WorldContextPolicy),
		static_cast<uint8>(ExceptionPolicy), *ScalarABI.Canonical,
		*ScalarABI.Hash.ToHexString(), static_cast<uint8>(RouteFlags),
		static_cast<uint8>(LifetimeFlags));
}

FAngelscriptStaticJITNativeCallValidation
ValidateAngelscriptStaticJITNativeCallDescriptor(
	const FAngelscriptStaticJITNativeCallDescriptor& Descriptor,
	const FAngelscriptStaticJITNativeCallValidationContext& Context)
{
	using namespace AngelscriptStaticJITNativeCallDescriptor_Private;

	if (Descriptor.IsAbsent())
	{
		FAngelscriptStaticJITNativeCallValidation Result;
		Result.bMetadataValid = true;
		Result.Code =
			EAngelscriptStaticJITNativeCallValidationCode::Absent;
		Result.Detail = TEXT(
			"ExternalCallMetadataAbsent: legacy native forms are non-external by default.");
		return Result;
	}

	switch (Descriptor.Linkage)
	{
	case EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol:
	case EAngelscriptStaticJITNativeCallLinkage::HeaderInline:
	case EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk:
	case EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable:
	case EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate:
		break;
	case EAngelscriptStaticJITNativeCallLinkage::None:
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidAbsentMetadata,
			TEXT("InvalidAbsentMetadata: a None linkage descriptor must carry no other fields."));
	default:
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownLinkage,
			TEXT("UnknownLinkage: the descriptor linkage value is not recognized."));
	}

	switch (Descriptor.Visibility)
	{
	case EAngelscriptStaticJITNativeCallVisibility::External:
	case EAngelscriptStaticJITNativeCallVisibility::HeaderInline:
	case EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate:
		break;
	case EAngelscriptStaticJITNativeCallVisibility::Internal:
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InternalLinkage,
			TEXT("InternalLinkage: a generated external module cannot name an internal target."));
	case EAngelscriptStaticJITNativeCallVisibility::Unspecified:
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidVisibility,
			TEXT("InvalidVisibility: non-absent metadata requires explicit visibility."));
	default:
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownVisibility,
			TEXT("UnknownVisibility: the descriptor visibility value is not recognized."));
	}

	const bool bVisibilityMatches =
		(Descriptor.Linkage
				== EAngelscriptStaticJITNativeCallLinkage::HeaderInline
			&& Descriptor.Visibility
				== EAngelscriptStaticJITNativeCallVisibility::HeaderInline)
		|| (Descriptor.Linkage
				== EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate
			&& Descriptor.Visibility
				== EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate)
		|| ((Descriptor.Linkage
				== EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol
				|| Descriptor.Linkage
					== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk
				|| Descriptor.Linkage
					== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable)
			&& Descriptor.Visibility
				== EAngelscriptStaticJITNativeCallVisibility::External);
	if (!bVisibilityMatches)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidVisibility,
			TEXT("InvalidVisibility: visibility contradicts the selected linkage class."));
	}

	if (Descriptor.Symbol.IsEmpty())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::MissingSymbol,
			TEXT("MissingSymbol: non-absent native-call metadata needs an exact callable symbol."));
	}
	if (ContainsUnsafeText(Descriptor.Symbol)
		|| Descriptor.Symbol.Contains(TEXT("("))
		|| Descriptor.Symbol.Contains(TEXT(")")))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidSymbol,
			TEXT("InvalidSymbol: callable spelling must be a declaration-owned C++ name, not generated code."));
	}

	const bool bDirectLinkage = Descriptor.Linkage
		!= EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
	if (bDirectLinkage && Descriptor.Include.IsEmpty())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::MissingInclude,
			TEXT("MissingInclude: a direct external target needs an includable declaration."));
	}
	if (ContainsUnsafeText(Descriptor.Include))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidInclude,
			TEXT("InvalidInclude: include paths cannot contain quotes or line breaks."));
	}
	if (Descriptor.OwningModule.IsNone())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::MissingOwningModule,
			TEXT("MissingOwningModule: native-call metadata needs an owning UE module."));
	}
	if (!Context.KnownModules.Contains(Descriptor.OwningModule))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownOwningModule,
			FString::Printf(TEXT(
				"UnknownOwningModule: %s is not in the frozen generation module set."),
				*Descriptor.OwningModule.ToString()));
	}
	if (bDirectLinkage
		&& !Context.PublicDependencyModules.Contains(Descriptor.OwningModule))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::PrivateDependency,
			FString::Printf(TEXT(
				"PrivateDependency: generated consumers cannot publicly depend on %s."),
				*Descriptor.OwningModule.ToString()));
	}

	if (Descriptor.Linkage
		== EAngelscriptStaticJITNativeCallLinkage::HeaderInline)
	{
		if (!Descriptor.ApiMacro.IsEmpty())
		{
			return Fail(
				EAngelscriptStaticJITNativeCallValidationCode::UnexpectedApiMacro,
				TEXT("UnexpectedApiMacro: a complete header-inline definition does not use DLL export as its proof."));
		}
	}
	else if (Descriptor.Linkage
		== EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol)
	{
		if (Descriptor.ApiMacro.IsEmpty())
		{
			return Fail(
				EAngelscriptStaticJITNativeCallValidationCode::MissingApiMacro,
				TEXT("MissingApiMacro: an out-of-line external symbol needs its owning module's API declaration."));
		}
	}
	else if (Descriptor.Linkage
			== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk
		|| Descriptor.Linkage
			== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable)
	{
		if (Descriptor.ApiMacro.IsEmpty())
		{
			return Fail(
				EAngelscriptStaticJITNativeCallValidationCode::MissingApiMacro,
				TEXT("MissingApiMacro: a Runtime callable must be declared ANGELSCRIPTRUNTIME_API."));
		}
		if (Descriptor.OwningModule != FName(TEXT("AngelscriptRuntime"))
			|| Descriptor.ApiMacro != TEXT("ANGELSCRIPTRUNTIME_API"))
		{
			return Fail(
				EAngelscriptStaticJITNativeCallValidationCode::InvalidApiMacro,
				TEXT("InvalidApiMacro: exported Runtime callables must belong to AngelscriptRuntime and use ANGELSCRIPTRUNTIME_API."));
		}
	}
	else if (!Descriptor.ApiMacro.IsEmpty() || !Descriptor.Include.IsEmpty())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnexpectedApiMacro,
			TEXT("UnexpectedApiMacro: provider-private targets cannot advertise a generated-consumer declaration."));
	}

	if (Descriptor.CppCallableSignature.IsEmpty())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::MissingCppCallableSignature,
			TEXT("MissingCppCallableSignature: a reviewed target needs its exact public C++ function type."));
	}
	FAngelscriptStaticJITNativeCallCppSignature ReviewedCppSignature;
	FString ReviewedCppSignatureError;
	if (ContainsUnsafeText(Descriptor.CppCallableSignature)
		|| !TryParseReviewedCppCallableSignature(
			Descriptor.CppCallableSignature,
			ReviewedCppSignature,
			ReviewedCppSignatureError))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidCppCallableSignature,
			FString::Printf(TEXT(
				"InvalidCppCallableSignature: %s."),
				ReviewedCppSignatureError.IsEmpty()
					? TEXT("callable type cannot contain quotes or line breaks")
					: *ReviewedCppSignatureError));
	}
	if (Descriptor.NativeABI.Canonical.IsEmpty()
		|| Descriptor.NativeABI.Hash.IsZero())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::MissingNativeABI,
			TEXT("MissingNativeABI: descriptor needs the exact installed system-function ABI identity."));
	}
	if (!Descriptor.NativeABI.IsValid())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidNativeABI,
			TEXT("InvalidNativeABI: canonical registered ABI and identity hash disagree."));
	}
	if (!Context.ExpectedNativeABI.IsValid()
		|| !(Descriptor.NativeABI == Context.ExpectedNativeABI))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::NativeABIMismatch,
			TEXT("NativeABIMismatch: descriptor does not match the compiler-authoritative installed function ABI."));
	}

	switch (Descriptor.ABIDomain)
	{
	case EAngelscriptStaticJITNativeCallABIDomain::Scalar:
	case EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD:
	case EAngelscriptStaticJITNativeCallABIDomain::ManagedValuesDeferred:
	case EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge:
		break;
	case EAngelscriptStaticJITNativeCallABIDomain::None:
	default:
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownABIDomain,
			TEXT("UnknownABIDomain: non-absent metadata needs a reviewed typed ABI domain."));
	}
	if (Descriptor.CompileOutPolicy
			!= EAngelscriptStaticJITNativeCallCompileOutPolicy::None
		&& Descriptor.CompileOutPolicy
			!= EAngelscriptStaticJITNativeCallCompileOutPolicy::IfNoLog)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidCallSemantics,
			TEXT("InvalidCallSemantics: compile-out policy is not recognized."));
	}
	if (Descriptor.DefaultArgumentPolicy
			== EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::None
		&& Descriptor.DefaultArgumentCount != 0)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidCallSemantics,
			TEXT("InvalidCallSemantics: default argument count requires CallerMaterializes policy."));
	}
	if (Descriptor.DefaultArgumentPolicy
			== EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::CallerMaterializes
		&& Descriptor.DefaultArgumentCount == 0)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidCallSemantics,
			TEXT("InvalidCallSemantics: CallerMaterializes requires at least one registered default."));
	}
	if (Descriptor.DefaultArgumentPolicy
			!= EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::None
		&& Descriptor.DefaultArgumentPolicy
			!= EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::CallerMaterializes)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidCallSemantics,
			TEXT("InvalidCallSemantics: default argument policy is not recognized."));
	}
	if ((Descriptor.WorldContextPolicy
			!= EAngelscriptStaticJITNativeCallWorldContextPolicy::None
		&& Descriptor.WorldContextPolicy
			!= EAngelscriptStaticJITNativeCallWorldContextPolicy::RuntimeCurrentWorld)
		|| (Descriptor.ExceptionPolicy
			!= EAngelscriptStaticJITNativeCallExceptionPolicy::DoesNotSetScriptException
		&& Descriptor.ExceptionPolicy
			!= EAngelscriptStaticJITNativeCallExceptionPolicy::MaySetScriptException))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidCallSemantics,
			TEXT("InvalidCallSemantics: world-context and exception behavior must be explicit reviewed values."));
	}
	if ((static_cast<uint8>(Descriptor.RouteFlags) & ~KnownRouteFlags) != 0)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownRouteFlags,
			TEXT("UnknownRouteFlags: unreviewed route bits fail closed."));
	}
	if ((static_cast<uint8>(Descriptor.LifetimeFlags)
			& ~KnownLifetimeFlags) != 0)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownLifetimeFlags,
			TEXT("UnknownLifetimeFlags: unreviewed lifetime bits fail closed."));
	}

	if (Descriptor.ABIDomain
		== EAngelscriptStaticJITNativeCallABIDomain::VMScalarBridge)
	{
		const bool bOnlyScalarBridgeRoute =
			EnumHasAnyFlags(
				Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge)
			&& !EnumHasAnyFlags(
				Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::DirectCall
					| EAngelscriptStaticJITNativeCallRouteFlags::RequiresProcessEvent
					| EAngelscriptStaticJITNativeCallRouteFlags::MaySuspend
					| EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall);
		if (!Descriptor.ScalarABI.Canonical.IsEmpty()
			|| !Descriptor.ScalarABI.Hash.IsZero()
			|| bDirectLinkage
			|| !bOnlyScalarBridgeRoute
			|| Descriptor.CompileOutPolicy
				!= EAngelscriptStaticJITNativeCallCompileOutPolicy::None
			|| Descriptor.DefaultArgumentPolicy
				!= EAngelscriptStaticJITNativeCallDefaultArgumentPolicy::None
			|| Descriptor.DefaultArgumentCount != 0
			|| Descriptor.WorldContextPolicy
				!= EAngelscriptStaticJITNativeCallWorldContextPolicy::None
			|| !EnumHasAllFlags(
				Descriptor.LifetimeFlags, RequiredScalarLifetime))
		{
			return Fail(
				EAngelscriptStaticJITNativeCallValidationCode::UnsafeRouting,
				TEXT("UnsafeRouting: a VM scalar bridge must be provider-private, carry no native scalar ABI, and use only reviewed scalar VM marshalling without defaults, WorldContext, suspend, ProcessEvent, or direct dispatch."));
		}

		FAngelscriptStaticJITNativeCallValidation Result;
		Result.bMetadataValid = true;
		Result.bScalarBridgeCallable = true;
		Result.ReviewedCppSignature = ReviewedCppSignature;
		Result.Code = EAngelscriptStaticJITNativeCallValidationCode::Valid;
		Result.Detail = Descriptor.DumpNormalized();
		return Result;
	}

	if (Descriptor.ABIDomain
		== EAngelscriptStaticJITNativeCallABIDomain::ManagedValuesDeferred)
	{
		if (!Descriptor.ScalarABI.Canonical.IsEmpty()
			|| !Descriptor.ScalarABI.Hash.IsZero()
			|| !EnumHasAnyFlags(Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::DirectCall)
			|| EnumHasAnyFlags(Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge
					| EAngelscriptStaticJITNativeCallRouteFlags::RequiresProcessEvent
					| EAngelscriptStaticJITNativeCallRouteFlags::MaySuspend
					| EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall)
			|| !EnumHasAllFlags(Descriptor.LifetimeFlags,
				EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
					| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture))
		{
			return Fail(
				EAngelscriptStaticJITNativeCallValidationCode::UnsafeLifetime,
				TEXT("UnsafeLifetime: deferred managed ABI metadata may prove linkage only; it cannot claim scalar routing or retained/Engine-local state."));
		}
		FAngelscriptStaticJITNativeCallValidation Result;
		Result.bMetadataValid = true;
		Result.ReviewedCppSignature = ReviewedCppSignature;
		Result.Code =
			EAngelscriptStaticJITNativeCallValidationCode::TypedABIUnavailable;
		Result.Detail = TEXT(
			"TypedABIUnavailable: external linkage is proven, but managed value/reference lowering remains fail-closed.");
		return Result;
	}
	if (Descriptor.ABIDomain
		== EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD)
	{
		const EAngelscriptStaticJITNativeCallLifetimeFlags RequiredManagedLifetime =
			EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture
			| EAngelscriptStaticJITNativeCallLifetimeFlags::BorrowedInputReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueObjectArguments;
		if (!Descriptor.ScalarABI.Canonical.IsEmpty()
			|| !Descriptor.ScalarABI.Hash.IsZero()
			|| !bDirectLinkage
			|| !EnumHasAnyFlags(Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::DirectCall)
			|| EnumHasAnyFlags(Descriptor.RouteFlags,
				EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge
					| EAngelscriptStaticJITNativeCallRouteFlags::RequiresProcessEvent
					| EAngelscriptStaticJITNativeCallRouteFlags::MaySuspend
					| EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall)
			|| !EnumHasAllFlags(
				Descriptor.LifetimeFlags, RequiredManagedLifetime))
		{
			return Fail(
				EAngelscriptStaticJITNativeCallValidationCode::UnsafeLifetime,
				TEXT("UnsafeLifetime: borrowed managed/POD direct calls require exact direct linkage, borrowed inputs, cleanup-free by-value objects, and no retained or Engine-local state."));
		}

		FAngelscriptStaticJITNativeCallValidation Result;
		Result.bMetadataValid = true;
		Result.bDirectCallable = true;
		Result.ReviewedCppSignature = ReviewedCppSignature;
		Result.Code = EAngelscriptStaticJITNativeCallValidationCode::Valid;
		Result.Detail = Descriptor.DumpNormalized();
		return Result;
	}

	if (Descriptor.ScalarABI.Canonical.IsEmpty()
		|| Descriptor.ScalarABI.Hash.IsZero())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::MissingScalarABI,
			TEXT("MissingScalarABI: native-call metadata needs a normalized scalar ABI identity."));
	}
	if (!Descriptor.ScalarABI.IsValid())
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::InvalidScalarABI,
			TEXT("InvalidScalarABI: canonical scalar ABI text and identity hash disagree."));
	}
	if (!Context.ExpectedScalarABI.IsValid()
		|| !(Descriptor.ScalarABI == Context.ExpectedScalarABI))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::ScalarABIMismatch,
			TEXT("ScalarABIMismatch: descriptor ABI does not match the compiler-authoritative resolved call ABI."));
	}

	if ((static_cast<uint8>(Descriptor.RouteFlags) & ~KnownRouteFlags) != 0)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownRouteFlags,
			TEXT("UnknownRouteFlags: unreviewed route bits fail closed."));
	}
	if ((static_cast<uint8>(Descriptor.LifetimeFlags)
			& ~KnownLifetimeFlags) != 0)
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnknownLifetimeFlags,
			TEXT("UnknownLifetimeFlags: unreviewed lifetime bits fail closed."));
	}

	const bool bDirectRequested = EnumHasAnyFlags(
		Descriptor.RouteFlags,
		EAngelscriptStaticJITNativeCallRouteFlags::DirectCall);
	const bool bBridgeRequested = EnumHasAnyFlags(
		Descriptor.RouteFlags,
		EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge);
	const bool bCurrentNativeRequested = EnumHasAnyFlags(
		Descriptor.RouteFlags,
		EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall);
	const bool bProviderPrivate = Descriptor.Linkage
		== EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
	const int32 SelectedRouteCount =
		(bDirectRequested ? 1 : 0)
		+ (bBridgeRequested ? 1 : 0)
		+ (bCurrentNativeRequested ? 1 : 0);
	if (bDirectRequested != bDirectLinkage
		|| SelectedRouteCount != 1
		|| (bCurrentNativeRequested && !bProviderPrivate)
		|| (bCurrentNativeRequested
			&& Descriptor.ExceptionPolicy
				!= EAngelscriptStaticJITNativeCallExceptionPolicy::
					DoesNotSetScriptException)
		|| EnumHasAnyFlags(
			Descriptor.RouteFlags,
			EAngelscriptStaticJITNativeCallRouteFlags::RequiresProcessEvent
				| EAngelscriptStaticJITNativeCallRouteFlags::MaySuspend))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnsafeRouting,
			TEXT("UnsafeRouting: scalar AOT needs exactly one linkage-consistent direct, current-native, or VM route; provider-private current-native calls must not set script exceptions or bypass ProcessEvent/suspend."));
	}
	if ((bDirectRequested || bBridgeRequested)
		&& !EnumHasAllFlags(Descriptor.LifetimeFlags, RequiredScalarLifetime))
	{
		return Fail(
			EAngelscriptStaticJITNativeCallValidationCode::UnsafeLifetime,
			TEXT("UnsafeLifetime: callable scalar metadata must prove by-value arguments, no retained references, and no Engine-local capture."));
	}

	FAngelscriptStaticJITNativeCallValidation Result;
	Result.bMetadataValid = true;
	Result.bDirectCallable = bDirectRequested;
	Result.bCurrentNativeCallable = bCurrentNativeRequested;
	Result.bScalarBridgeCallable = bBridgeRequested;
	Result.ReviewedCppSignature = ReviewedCppSignature;
	Result.Code = EAngelscriptStaticJITNativeCallValidationCode::Valid;
	Result.Detail = Descriptor.DumpNormalized();
	return Result;
}

#endif // AS_CAN_GENERATE_JIT
