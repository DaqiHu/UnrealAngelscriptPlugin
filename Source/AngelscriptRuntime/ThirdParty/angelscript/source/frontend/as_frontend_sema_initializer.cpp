#include "frontend/as_sema.h"
#include "frontend/as_ast_cast.h"

BEGIN_AS_NAMESPACE
asCQualType asCSema::ResolveAutoVariableType(const asSTypeSyntax& Syntax, asCExpr* Initializer)
	{
		if (!Initializer || Initializer->IsInvalid() || Syntax.NameParts.Num() != 1 || Syntax.NameParts[0] != TEXT("auto")
			|| Syntax.bGlobal || !Syntax.GenericArguments.IsEmpty() || !Syntax.ArrayDimensions.IsEmpty()
			|| Syntax.Reference != asETypeReferenceDirection::None) return {};
		const asCQualType Actual = Initializer->GetExpressionQualType();
		if (!Actual.IsValid() || (Syntax.bHandle && !Actual.IsHandle())) return {};
		if (const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Actual.GetType()))
			if (Builtin->GetPrimitive() == asEPrimitiveType::Void) return {};
		uint8 Qualifiers = static_cast<uint8>(Actual.GetQualifiers()) & ~static_cast<uint8>(asETypeQualifiers::Reference);
		if (!Actual.IsHandle()) Qualifiers &= ~static_cast<uint8>(asETypeQualifiers::Const);
		if (Syntax.bConst) Qualifiers |= static_cast<uint8>(asETypeQualifiers::Const);
		asCQualType Result; asCQualType::TryCreateRaw(Actual.GetType(), Qualifiers, Result); return Result;
	}

	asCInitListExpr* asCSema::ActOnInitList(TArray<asCExpr*> Elements, asCSourceRange Range)
	{
		return Context.Create<asCInitListExpr>(MoveTemp(Elements), Range);
	}

	asCValueInitExpr* asCSema::ActOnValueInit(asCSourceRange Range)
	{
		return Context.Create<asCValueInitExpr>(Range);
	}

	asCExpr* asCSema::ConvertInitList(asCInitListExpr& List, asCQualType Target)
	{
		asSListInitialization Contract;
		if (!Target.IsValid() || Target.IsReference() || Target.IsHandle() || !ListInitializationLookup
			|| !ListInitializationLookup(Target, Contract) || Contract.Factory.IsZero()) return nullptr;
		TMap<const asSResolvedListPattern*, int32> PatternIndices;
		TMap<const asSResolvedListPattern*, int32> RepetitionCounts;
		TFunction<bool(const asSResolvedListPattern&, int32)> IndexPattern;
		IndexPattern = [&](const asSResolvedListPattern& Pattern, int32 Depth)
		{
			if (Depth > 64 || PatternIndices.Num() >= 4096) return false;
			PatternIndices.Add(&Pattern, PatternIndices.Num());
			for (const auto& Child : Pattern.Children) if (!IndexPattern(Child, Depth + 1)) return false;
			return true;
		};
		if (!IndexPattern(Contract.Pattern, 0)) return nullptr;
		TFunction<bool(asCInitListExpr&, const asSResolvedListPattern&, int32)> BindList;
		TFunction<bool(asCInitListExpr&, int32, const asSResolvedListPattern&, int32)> BindElement;
		BindElement = [&](asCInitListExpr& Group, int32 Index, const asSResolvedListPattern& Pattern, int32 Depth)
		{
			if (Depth > 64 || !Group.GetElements().IsValidIndex(Index)) return false;
			asCExpr* Element = Group.GetElements()[Index];
			if (Pattern.Kind == asEMetadataListPatternKind::List)
			{
				asCInitListExpr* Nested = dyn_cast<asCInitListExpr>(Element);
				return Nested && BindList(*Nested, Pattern, Depth + 1);
			}
			if (Pattern.Kind != asEMetadataListPatternKind::Value || !Pattern.ValueType.IsValid()) return false;
			if (auto* Omitted = dyn_cast<asCValueInitExpr>(Element))
			{
				if (Pattern.ValueType.IsReference()) return false;
				if (auto* Builtin = dyn_cast<asCBuiltinType>(Pattern.ValueType.GetType()))
					if (Builtin->GetPrimitive() == asEPrimitiveType::Void) return false;
				Omitted->SetExpressionQualType(Pattern.ValueType, asEValueCategory::PRValue);
				return true;
			}
			asCExpr* Converted = ConvertTo(Element, Pattern.ValueType);
			return Converted && Group.SetElement(Index, Converted);
		};
		BindList = [&](asCInitListExpr& Group, const asSResolvedListPattern& Pattern, int32 Depth)
		{
			if (Depth > 64 || Pattern.Kind != asEMetadataListPatternKind::List) return false;
			TArray<int32> ElementIndices;
			int32 ElementIndex = 0;
			for (int32 ChildIndex = 0; ChildIndex < Pattern.Children.Num(); ++ChildIndex)
			{
				const auto& Child = Pattern.Children[ChildIndex];
				if (Child.Kind == asEMetadataListPatternKind::Repeat || Child.Kind == asEMetadataListPatternKind::RepeatSame)
				{
					if (Child.Children.Num() != 1 || ChildIndex + 1 != Pattern.Children.Num()) return false;
					const int32 Count = Group.GetElements().Num() - ElementIndex;
					if (Child.Kind == asEMetadataListPatternKind::RepeatSame)
					{
						if (const int32* Previous = RepetitionCounts.Find(&Child)) { if (*Previous != Count) return false; }
						else RepetitionCounts.Add(&Child, Count);
					}
					while (ElementIndex < Group.GetElements().Num())
					{
						if (!BindElement(Group, ElementIndex++, Child.Children[0], Depth + 1)) return false;
						ElementIndices.Add(PatternIndices.FindChecked(&Child.Children[0]));
					}
				}
				else
				{
					if (!BindElement(Group, ElementIndex++, Child, Depth + 1)) return false;
					ElementIndices.Add(PatternIndices.FindChecked(&Child));
				}
			}
			if (ElementIndex != Group.GetElements().Num()) return false;
			Group.SetExpressionQualType(Target, asEValueCategory::PRValue);
			return Group.SetInitializerBinding(Contract.Factory, MoveTemp(ElementIndices));
		};
		if (!BindList(List, Contract.Pattern, 0))
		{
			ReportBodyError(List.GetSourceRange(), TEXT("initializer-does-not-match-list-factory-pattern"));
			return nullptr;
		}
		return &List;
	}
END_AS_NAMESPACE
