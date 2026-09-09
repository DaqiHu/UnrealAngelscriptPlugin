#include "Bind_FText.h"

#include "AngelscriptEngine.h"

#include "Helper_GetTypeInfo.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	template <typename InternalType, typename ExternalType>
	bool AddPrimitiveFormatOrderedArgument(FFormatOrderedArguments& Arguments, const void* Address)
	{
		const InternalType Value = *static_cast<const ExternalType*>(Address);
		Arguments.Emplace(FFormatArgumentValue(Value));
		return true;
	}

}

void FAngelscriptFTextBinds::ConstructDefault(FText* Address)
{
	new (Address) FText();
}

void FAngelscriptFTextBinds::ConstructCopy(FText* Address, const FText& Other)
{
	new (Address) FText(Other);
}

void FAngelscriptFTextBinds::Destroy(FText& Text)
{
	Text.~FText();
}

void FAngelscriptFTextBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FText*>(Address)->ToString();
}

FText FAngelscriptFTextBinds::AsCultureInvariant(const FString& Value)
{
	return FText::AsCultureInvariant(Value);
}

FText FAngelscriptFTextBinds::AsDate(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle)
{
	return FText::AsDate(DateTime, DateStyle);
}

FText FAngelscriptFTextBinds::AsDateTime(
	const FDateTime& DateTime,
	const EDateTimeStyle::Type DateStyle,
	const EDateTimeStyle::Type TimeStyle)
{
	return FText::AsDateTime(DateTime, DateStyle, TimeStyle);
}

FText FAngelscriptFTextBinds::AsTime(const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle)
{
	return FText::AsTime(DateTime, TimeStyle);
}

FText FAngelscriptFTextBinds::AsTimespan(const FTimespan& Timespan)
{
	return FText::AsTimespan(Timespan);
}

FText FAngelscriptFTextBinds::AsMemory(const uint64 NumBytes)
{
	return FText::AsMemory(NumBytes);
}

bool FAngelscriptFTextBinds::TryAddFormatArgument(
	FFormatOrderedArguments& Arguments, const void* Address, const int32 TypeId, FString& Diagnostic)
{
	Diagnostic.Reset();
	if (!Address)
	{
		Diagnostic = TEXT("Invalid argument type passed to FText::Format: value address is null"); return false;
	}
	switch (TypeId & asTYPEID_MASK_SEQNBR)
	{
	case asTYPEID_INT8: return AddPrimitiveFormatOrderedArgument<int32, int8>(Arguments, Address);
	case asTYPEID_INT16: return AddPrimitiveFormatOrderedArgument<int32, int16>(Arguments, Address);
	case asTYPEID_INT32: return AddPrimitiveFormatOrderedArgument<int32, int32>(Arguments, Address);
	case asTYPEID_INT64: return AddPrimitiveFormatOrderedArgument<int64, int64>(Arguments, Address);
	case asTYPEID_UINT8: return AddPrimitiveFormatOrderedArgument<uint32, uint8>(Arguments, Address);
	case asTYPEID_UINT16: return AddPrimitiveFormatOrderedArgument<uint32, uint16>(Arguments, Address);
	case asTYPEID_UINT32: return AddPrimitiveFormatOrderedArgument<uint32, uint32>(Arguments, Address);
	case asTYPEID_UINT64: return AddPrimitiveFormatOrderedArgument<uint64, uint64>(Arguments, Address);
	case asTYPEID_FLOAT32: return AddPrimitiveFormatOrderedArgument<float, float>(Arguments, Address);
	case asTYPEID_FLOAT64: return AddPrimitiveFormatOrderedArgument<double, double>(Arguments, Address);
	default: break;
	}
	if (TypeId <= 0)
	{
		Diagnostic = FString::Printf(TEXT("Invalid argument type passed to FText::Format: type id %d"), TypeId); return false;
	}
	asITypeInfo* TypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);
	if (!TypeInfo)
	{
		Diagnostic = FString::Printf(TEXT("Invalid argument type passed to FText::Format: type id %d"), TypeId); return false;
	}
	if ((TypeInfo->GetFlags() & asOBJ_ENUM) != 0)
	{
		Arguments.Emplace(FFormatArgumentValue(*static_cast<const uint8*>(Address))); return true;
	}
	if (TGetStaticTypeInfo<FText>::IsForEngine(FAngelscriptEngine::Get().GetScriptEngine(), TypeInfo))
	{
		Arguments.Emplace(FFormatArgumentValue(*static_cast<const FText*>(Address))); return true;
	}
	Diagnostic = FString::Printf(TEXT("Invalid argument type passed to FText::Format: %s"), ANSI_TO_TCHAR(TypeInfo->GetName()));
	return false;
}

void FAngelscriptFTextBinds::GenericTextFormat(asIScriptGeneric* Generic)
{
	const FText& Format = *static_cast<FText*>(Generic->GetArgAddress(0));

	bool bSuccess = true;
	FFormatOrderedArguments Arguments;
	for (int32 Index = 1; bSuccess && Index < Generic->GetArgCount(); ++Index)
	{
		FString Diagnostic;
		bSuccess &= TryAddFormatArgument(Arguments, Generic->GetArgAddress(Index), Generic->GetArgTypeId(Index), Diagnostic);
		if (!bSuccess) FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Diagnostic));
	}

	const FText Result = bSuccess ? FText::Format(Format, Arguments) : FText();
	new (Generic->GetAddressOfReturnLocation()) FText(Result);
}

FText FAngelscriptFTextBinds::NamedTextFormat(
	const FText& Format,
	const TMap<FString, FFormatArgumentValue>& Arguments)
{
	FFormatNamedArguments NamedArguments;
	NamedArguments.Reserve(Arguments.Num());
	for (const TPair<FString, FFormatArgumentValue>& Argument : Arguments)
	{
		NamedArguments.Add(Argument.Key, Argument.Value);
	}
	return FText::Format(Format, NamedArguments);
}

FText FAngelscriptFTextBinds::OrderedTextFormat(
	const FText& Format,
	const TArray<FFormatArgumentValue>& Arguments)
{
	return FText::Format(Format, Arguments);
}

void FAngelscriptFTextBinds::GetFormatPatternParameters(
	const FText& Format,
	TArray<FString>& ParameterNames)
{
	FText::GetFormatPatternParameters(Format, ParameterNames);
}

FText FAngelscriptFTextBinds::MakeLocalizableText(
	const FString& Namespace,
	const FString& Key,
	const FString& Text)
{
	return FText::AsLocalizable_Advanced(*Namespace, *Key, Text);
}
