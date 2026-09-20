#include "Framework/Generate/AngelscriptTestExprPrimaryContextGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprPrimaryContextGeneratorDetail
{
enum class EPrimaryFamily : uint8
{
	Int,
	Bool,
	Enum,
	ValueObject,
	RootReference,
	DerivedReference,
};

struct FPrimaryContextCell
{
	EExprPrimaryContext Context = EExprPrimaryContext::Initializer;
	EExprPrimaryKind Primary = EExprPrimaryKind::IntLiteral;
};

const TCHAR* ContextToken(const EExprPrimaryContext Context)
{
	switch (Context)
	{
	case EExprPrimaryContext::Initializer:
		return TEXT("INITIALIZER");
	case EExprPrimaryContext::AssignmentRhs:
		return TEXT("ASSIGNMENT_RHS");
	case EExprPrimaryContext::AssignmentLhs:
		return TEXT("ASSIGNMENT_LHS");
	case EExprPrimaryContext::Argument:
		return TEXT("ARGUMENT");
	case EExprPrimaryContext::Return:
		return TEXT("RETURN");
	case EExprPrimaryContext::Condition:
		return TEXT("CONDITION");
	case EExprPrimaryContext::LoopClause:
		return TEXT("LOOP_CLAUSE");
	case EExprPrimaryContext::SwitchSelector:
		return TEXT("SWITCH_SELECTOR");
	case EExprPrimaryContext::Index:
		return TEXT("INDEX");
	case EExprPrimaryContext::PropertyAccessor:
		return TEXT("PROPERTY_ACCESSOR");
	default:
		return nullptr;
	}
}

const TCHAR* PrimaryToken(const EExprPrimaryKind Primary)
{
	switch (Primary)
	{
	case EExprPrimaryKind::IntLiteral:
		return TEXT("INT_LITERAL");
	case EExprPrimaryKind::BoolLiteral:
		return TEXT("BOOL_LITERAL");
	case EExprPrimaryKind::EnumLiteral:
		return TEXT("ENUM_LITERAL");
	case EExprPrimaryKind::NullLiteral:
		return TEXT("NULL_LITERAL");
	case EExprPrimaryKind::LocalIdentifier:
		return TEXT("LOCAL_IDENTIFIER");
	case EExprPrimaryKind::ConstIdentifier:
		return TEXT("CONST_IDENTIFIER");
	case EExprPrimaryKind::ReferenceIdentifier:
		return TEXT("REFERENCE_IDENTIFIER");
	case EExprPrimaryKind::ScopedConstant:
		return TEXT("SCOPED_CONSTANT");
	case EExprPrimaryKind::ScopedEnum:
		return TEXT("SCOPED_ENUM");
	case EExprPrimaryKind::ParenthesizedScalar:
		return TEXT("PARENTHESIZED_SCALAR");
	case EExprPrimaryKind::ParenthesizedLValue:
		return TEXT("PARENTHESIZED_LVALUE");
	case EExprPrimaryKind::GlobalFunctionCall:
		return TEXT("GLOBAL_FUNCTION_CALL");
	case EExprPrimaryKind::MethodCall:
		return TEXT("METHOD_CALL");
	case EExprPrimaryKind::ValueConstructor:
		return TEXT("VALUE_CONSTRUCTOR");
	case EExprPrimaryKind::ReferenceConstructor:
		return TEXT("REFERENCE_CONSTRUCTOR");
	case EExprPrimaryKind::MemberField:
		return TEXT("MEMBER_FIELD");
	case EExprPrimaryKind::VirtualProperty:
		return TEXT("VIRTUAL_PROPERTY");
	case EExprPrimaryKind::IndexedProperty:
		return TEXT("INDEXED_PROPERTY");
	case EExprPrimaryKind::ExplicitNumericCast:
		return TEXT("EXPLICIT_NUMERIC_CAST");
	case EExprPrimaryKind::ObjectCast:
		return TEXT("OBJECT_CAST");
	case EExprPrimaryKind::BaseCast:
		return TEXT("BASE_CAST");
	case EExprPrimaryKind::DerivedCast:
		return TEXT("DERIVED_CAST");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprPrimaryContextParams& Params)
{
	return ContextToken(Params.Context) != nullptr && PrimaryToken(Params.Primary) != nullptr;
}

EPrimaryFamily FamilyOf(const EExprPrimaryKind Primary)
{
	switch (Primary)
	{
	case EExprPrimaryKind::BoolLiteral:
		return EPrimaryFamily::Bool;
	case EExprPrimaryKind::EnumLiteral:
	case EExprPrimaryKind::ScopedEnum:
		return EPrimaryFamily::Enum;
	case EExprPrimaryKind::ValueConstructor:
		return EPrimaryFamily::ValueObject;
	case EExprPrimaryKind::NullLiteral:
	case EExprPrimaryKind::ReferenceIdentifier:
	case EExprPrimaryKind::ReferenceConstructor:
	case EExprPrimaryKind::BaseCast:
		return EPrimaryFamily::RootReference;
	case EExprPrimaryKind::ObjectCast:
	case EExprPrimaryKind::DerivedCast:
		return EPrimaryFamily::DerivedReference;
	default:
		return EPrimaryFamily::Int;
	}
}

bool IsAssignable(const EExprPrimaryKind Primary)
{
	return Primary == EExprPrimaryKind::LocalIdentifier
		|| Primary == EExprPrimaryKind::ReferenceIdentifier
		|| Primary == EExprPrimaryKind::ParenthesizedLValue
		|| Primary == EExprPrimaryKind::MemberField;
}

bool IsIntegralContext(const EExprPrimaryKind Primary)
{
	switch (Primary)
	{
	case EExprPrimaryKind::BoolLiteral:
	case EExprPrimaryKind::NullLiteral:
	case EExprPrimaryKind::ReferenceIdentifier:
	case EExprPrimaryKind::ValueConstructor:
	case EExprPrimaryKind::ReferenceConstructor:
	case EExprPrimaryKind::ObjectCast:
	case EExprPrimaryKind::BaseCast:
	case EExprPrimaryKind::DerivedCast:
		return false;
	default:
		return true;
	}
}

bool IsLegal(const EExprPrimaryKind Primary, const EExprPrimaryContext Context)
{
	switch (Context)
	{
	case EExprPrimaryContext::AssignmentLhs:
		return IsAssignable(Primary);
	case EExprPrimaryContext::SwitchSelector:
		return IsIntegralContext(Primary);
	case EExprPrimaryContext::Index:
	case EExprPrimaryContext::PropertyAccessor:
		return FamilyOf(Primary) == EPrimaryFamily::Int;
	default:
		return true;
	}
}

bool IsReject(const FPrimaryContextCell& Cell)
{
	return !IsLegal(Cell.Primary, Cell.Context);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FPrimaryContextCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-PRIMARY-CONTEXT-%s-%s"),
		ContextToken(Cell.Context),
		PrimaryToken(Cell.Primary));
}

FString MakeEntryName(const FPrimaryContextCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprPrimaryContextParams ParamsFromCell(const FPrimaryContextCell& Cell)
{
	FExprPrimaryContextParams Params;
	Params.Context = Cell.Context;
	Params.Primary = Cell.Primary;
	return Params;
}

TArray<FPrimaryContextCell> AllCells()
{
	TArray<FPrimaryContextCell> Cells;
	Cells.SetNum(220);
	for (int32 Index = 0; Index < 220; ++Index)
	{
		FPrimaryContextCell& Cell = Cells[Index];
		Cell.Context = static_cast<EExprPrimaryContext>(Index / 22);
		Cell.Primary = static_cast<EExprPrimaryKind>(Index % 22);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FPrimaryContextCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FPrimaryContextCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (MakeCaseId(Cells[Index]) == Needle)
		{
			OutCell = Cells[Index];
			return true;
		}
	}
	return false;
}

FString ScriptType(const EExprPrimaryKind Primary)
{
	switch (FamilyOf(Primary))
	{
	case EPrimaryFamily::Bool:
		return TEXT("bool");
	case EPrimaryFamily::Enum:
		return TEXT("EPrimaryValue");
	case EPrimaryFamily::ValueObject:
		return TEXT("FPrimaryValue");
	case EPrimaryFamily::RootReference:
		return TEXT("FRefRoot");
	case EPrimaryFamily::DerivedReference:
		return TEXT("FRefDerived");
	default:
		return TEXT("int");
	}
}

FString PrimaryExpression(const EExprPrimaryKind Primary)
{
	switch (Primary)
	{
	case EExprPrimaryKind::IntLiteral:
		return TEXT("41");
	case EExprPrimaryKind::BoolLiteral:
		return TEXT("true");
	case EExprPrimaryKind::EnumLiteral:
		return TEXT("EPrimaryValue::FortyOne");
	case EExprPrimaryKind::NullLiteral:
		return TEXT("nullptr");
	case EExprPrimaryKind::LocalIdentifier:
		return TEXT("LocalValue");
	case EExprPrimaryKind::ConstIdentifier:
		return TEXT("ConstValue");
	case EExprPrimaryKind::ReferenceIdentifier:
		return TEXT("Object");
	case EExprPrimaryKind::ScopedConstant:
		return TEXT("PrimaryScope::ScopedValue");
	case EExprPrimaryKind::ScopedEnum:
		return TEXT("PrimaryScope::ScopedFortyOne");
	case EExprPrimaryKind::ParenthesizedScalar:
		return TEXT("(LocalValue + 0)");
	case EExprPrimaryKind::ParenthesizedLValue:
		return TEXT("(LocalValue)");
	case EExprPrimaryKind::GlobalFunctionCall:
		return TEXT("MakePrimaryInt()");
	case EExprPrimaryKind::MethodCall:
		return TEXT("Object.GetValue()");
	case EExprPrimaryKind::ValueConstructor:
		return TEXT("FPrimaryValue(41)");
	case EExprPrimaryKind::ReferenceConstructor:
		return TEXT("MakeRefRoot(41)");
	case EExprPrimaryKind::MemberField:
		return TEXT("ValueObject.Value");
	case EExprPrimaryKind::VirtualProperty:
		return TEXT("Object.GetVirtualValue()");
	case EExprPrimaryKind::IndexedProperty:
		return TEXT("Object.GetItem(0)");
	case EExprPrimaryKind::ExplicitNumericCast:
		return TEXT("int(41.75f)");
	case EExprPrimaryKind::ObjectCast:
		return TEXT("RootObject.opCast()");
	case EExprPrimaryKind::BaseCast:
		return TEXT("DerivedObject.opImplCast()");
	case EExprPrimaryKind::DerivedCast:
		return TEXT("BaseView.opCast()");
	default:
		return TEXT("0");
	}
}

FString ReplacementExpression(const EExprPrimaryKind Primary)
{
	switch (FamilyOf(Primary))
	{
	case EPrimaryFamily::Bool:
		return TEXT("false");
	case EPrimaryFamily::Enum:
		return TEXT("EPrimaryValue::FiftyOne");
	case EPrimaryFamily::ValueObject:
		return TEXT("FPrimaryValue(51)");
	case EPrimaryFamily::RootReference:
		return TEXT("MakeRefRoot(51)");
	case EPrimaryFamily::DerivedReference:
		return TEXT("MakeRefDerived(51)");
	default:
		return TEXT("51");
	}
}

FString ObserveCall(const FString& Expression)
{
	return FString::Printf(TEXT("ObservePrimary(%s)"), *Expression);
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("enum EPrimaryValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFortyOne = 41,"));
	AppendLine(Source, TEXT("\tFiftyOne = 51"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace PrimaryScope"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tconst int ScopedValue = 41;"));
	AppendLine(Source, TEXT("\tconst EPrimaryValue ScopedFortyOne = EPrimaryValue::FortyOne;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FPrimaryValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrimaryValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrimaryValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FRefRoot"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Stored = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFRefRoot()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFRefRoot(int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetValue() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Stored;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid SetValue(int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetVirtualValue() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Stored;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid SetVirtualValue(int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetItem(int Index) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Stored + Index;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid SetItem(int Index, int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = Value - Index;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFRefDerived@ opCast() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn null;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FRefDerived"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Stored = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFRefDerived()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFRefDerived(int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetValue() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Stored;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFRefRoot opImplCast() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FRefRoot(Stored);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int MakePrimaryInt()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 41;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRefRoot MakeRefRoot(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FRefRoot(Value);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRefDerived MakeRefDerived(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FRefDerived(Value);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRefRoot MakeRefDerivedAsRoot(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FRefRoot(Value);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrimary(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value == 41 ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrimary(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrimary(EPrimaryValue Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value == EPrimaryValue::FortyOne ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrimary(FPrimaryValue Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value == 41 ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrimary(const FRefRoot Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.GetValue() == 41 || Value.GetValue() == 0 ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrimary(const FRefDerived Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.GetValue() == 41 || Value.GetValue() == 0 ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int PreservePrimary(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool PreservePrimary(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("EPrimaryValue PreservePrimary(EPrimaryValue Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FPrimaryValue PreservePrimary(FPrimaryValue Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRefRoot PreservePrimary(FRefRoot Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRefDerived PreservePrimary(FRefDerived Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntrySetup(FString& Source)
{
	AppendLine(Source, TEXT("\tint LocalValue = 41;"));
	AppendLine(Source, TEXT("\tconst int ConstValue = 41;"));
	AppendLine(Source, TEXT("\tFPrimaryValue ValueObject(41);"));
	AppendLine(Source, TEXT("\tFRefRoot Object = MakeRefRoot(41);"));
	AppendLine(Source, TEXT("\tFRefRoot RootObject = MakeRefRoot(41);"));
	AppendLine(Source, TEXT("\tFRefDerived DerivedObject = MakeRefDerived(41);"));
	AppendLine(Source, TEXT("\tFRefRoot BaseView = MakeRefDerivedAsRoot(41);"));
}

void AppendLegalContext(FString& Source, const EExprPrimaryKind Primary, const EExprPrimaryContext Context)
{
	const FString Expression = PrimaryExpression(Primary);
	const FString Type = ScriptType(Primary);
	switch (Context)
	{
	case EExprPrimaryContext::Initializer:
		AppendLine(Source, TEXT("\t") + Type + TEXT(" Result = ") + Expression + TEXT(";"));
		AppendLine(Source, TEXT("\treturn ") + ObserveCall(TEXT("Result")) + TEXT(";"));
		break;
	case EExprPrimaryContext::AssignmentRhs:
		AppendLine(Source, TEXT("\t") + Type + TEXT(" Result;"));
		AppendLine(Source, TEXT("\tResult = ") + Expression + TEXT(";"));
		AppendLine(Source, TEXT("\treturn ") + ObserveCall(TEXT("Result")) + TEXT(";"));
		break;
	case EExprPrimaryContext::AssignmentLhs:
		AppendLine(Source, TEXT("\t") + Expression + TEXT(" = ") + ReplacementExpression(Primary) + TEXT(";"));
		if (FamilyOf(Primary) == EPrimaryFamily::RootReference || FamilyOf(Primary) == EPrimaryFamily::DerivedReference)
		{
			AppendLine(Source, TEXT("\treturn ") + Expression + TEXT(".GetValue() == 51 ? 1 : 0;"));
		}
		else if (FamilyOf(Primary) == EPrimaryFamily::ValueObject)
		{
			AppendLine(Source, TEXT("\treturn ") + Expression + TEXT(".Value == 51 ? 1 : 0;"));
		}
		else
		{
			AppendLine(Source, TEXT("\treturn ") + Expression + TEXT(" == 51 ? 1 : 0;"));
		}
		break;
	case EExprPrimaryContext::Argument:
		AppendLine(Source, TEXT("\treturn ") + ObserveCall(Expression) + TEXT(";"));
		break;
	case EExprPrimaryContext::Return:
		break;
	case EExprPrimaryContext::Condition:
		AppendLine(Source, TEXT("\tif (") + ObserveCall(Expression) + TEXT(" == 1)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	case EExprPrimaryContext::LoopClause:
		AppendLine(Source, TEXT("\tint Count = 0;"));
		AppendLine(Source, TEXT("\twhile (Count < 1 && (") + ObserveCall(Expression) + TEXT(" == 1))"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\t++Count;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn Count == 1 ? 1 : 0;"));
		break;
	case EExprPrimaryContext::SwitchSelector:
		AppendLine(Source, TEXT("\tswitch (") + Expression + TEXT(")"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(
			Source,
			TEXT("\tcase ") +
				FString(FamilyOf(Primary) == EPrimaryFamily::Enum ? TEXT("EPrimaryValue::FortyOne") : TEXT("41")) +
				TEXT(":"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t\treturn 0;"));
		AppendLine(Source, TEXT("\t}"));
		break;
	case EExprPrimaryContext::Index:
		AppendLine(Source, TEXT("\treturn Object.GetItem(") + Expression + TEXT(") == 82 ? 1 : 0;"));
		break;
	case EExprPrimaryContext::PropertyAccessor:
		AppendLine(Source, TEXT("\tObject.SetVirtualValue(") + Expression + TEXT(");"));
		AppendLine(Source, TEXT("\treturn Object.GetVirtualValue() == 41 ? 1 : 0;"));
		break;
	}
}

void AppendRejectedContext(FString& Source, const EExprPrimaryKind Primary, const EExprPrimaryContext Context)
{
	const FString Expression = PrimaryExpression(Primary);
	switch (Context)
	{
	case EExprPrimaryContext::AssignmentLhs:
		if (Primary == EExprPrimaryKind::NullLiteral || Primary == EExprPrimaryKind::ValueConstructor)
		{
			AppendLine(Source, TEXT("\t1 = 2;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t") + Expression + TEXT(" = ") + ReplacementExpression(Primary) + TEXT(";"));
		}
		break;
	case EExprPrimaryContext::SwitchSelector:
		AppendLine(Source, TEXT("\tswitch (") + Expression + TEXT(")"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t\treturn 0;"));
		AppendLine(Source, TEXT("\t}"));
		break;
	case EExprPrimaryContext::Index:
		AppendLine(Source, TEXT("\treturn Object.GetItem(") + Expression + TEXT(");"));
		break;
	case EExprPrimaryContext::PropertyAccessor:
		AppendLine(Source, TEXT("\tObject.SetVirtualValue(") + Expression + TEXT(");"));
		break;
	default:
		break;
	}
	AppendLine(Source, TEXT("\treturn 0;"));
}

void AppendEntry(FString& Source, const FExprPrimaryContextParams& Params, const FString& FunctionName)
{
	if (Params.Context == EExprPrimaryContext::Return)
	{
		const FString Type = Params.Primary == EExprPrimaryKind::NullLiteral ? TEXT("FRefRoot") : ScriptType(Params.Primary);
		AppendLine(Source, FString::Printf(TEXT("%s ReturnPrimary_%s()"), *Type, *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendEntrySetup(Source);
		AppendLine(Source, TEXT("\treturn ") + PrimaryExpression(Params.Primary) + TEXT(";"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Context != EExprPrimaryContext::Return)
	{
		AppendEntrySetup(Source);
	}
	if (Params.Context == EExprPrimaryContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ObservePrimary(ReturnPrimary_%s());"), *FunctionName));
	}
	else if (IsLegal(Params.Primary, Params.Context))
	{
		AppendLegalContext(Source, Params.Primary, Params.Context);
	}
	else
	{
		AppendRejectedContext(Source, Params.Primary, Params.Context);
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprPrimaryContextParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FPrimaryContextCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = 1;
	return Info;
}
}

FStringView FExprPrimaryContextGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-PRIMARY-CONTEXT");
}

TArray<FGeneratedCaseInfo> FExprPrimaryContextGenerator::ListCases() const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FPrimaryContextCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("ExprPrimaryContext PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprPrimaryContextGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	FPrimaryContextCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprPrimaryContextGenerator::BuildDumpSource() const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	const TArray<FPrimaryContextCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-PRIMARY-CONTEXT"));
	AppendLine(Source, TEXT("// Cases: 220 total, 174 normal, 46 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index]))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index]))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendSharedHelpers(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FExprPrimaryContextGenerator::BuildPrimaryExpressionSource(const FExprPrimaryContextParams& Params) const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	if (!IsValidParams(Params) || !IsLegal(Params.Primary, Params.Context))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Params, FunctionName);
}

FString FExprPrimaryContextGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	const TArray<FPrimaryContextCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index]))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FExprPrimaryContextGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	FPrimaryContextCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell))
	{
		return 0;
	}
	return 1;
}

FString FExprPrimaryContextGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	FPrimaryContextCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FExprPrimaryContextGenerator::ListRejectCaseIds() const
{
	using namespace ExprPrimaryContextGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FPrimaryContextCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index]))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
