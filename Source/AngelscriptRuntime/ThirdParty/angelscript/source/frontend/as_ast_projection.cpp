#include "frontend/as_ast_projection.h"

#include "frontend/as_ast_verifier.h"
#include "frontend/as_function_modifiers.h"
#include "frontend/as_type_context.h"

BEGIN_AS_NAMESPACE

namespace
	{
		bool AnchorEquals(const asSStableSourceAnchor& Left, const asSStableSourceAnchor& Right)
		{
			return Left.LogicalSourceKey == Right.LogicalSourceKey
				&& Left.ContentRevision == Right.ContentRevision
				&& Left.BeginByteOffset == Right.BeginByteOffset
				&& Left.EndByteOffset == Right.EndByteOffset;
		}

		bool MakeAnchor(
			const asCSourceSnapshot& Snapshot,
			const asCSourceRange& Range,
			asSStableSourceAnchor& Out)
		{
			Out = {};
			if (!Range.IsOrdered()) return false;
			FStringView LogicalSourceKey;
			uint64 Revision = 0;
			if (!Snapshot.TryGetLogicalSourceKey(Range.Begin.FileID, LogicalSourceKey)
				|| !Snapshot.TryGetContentRevision(Range.Begin.FileID, Revision))
			{
				return false;
			}
			Out.LogicalSourceKey = FString(LogicalSourceKey);
			Out.ContentRevision = Revision;
			Out.BeginByteOffset = Range.Begin.ByteOffset;
			Out.EndByteOffset = Range.End.ByteOffset;
			return Out.IsValid();
		}

		bool ProjectTypeSyntax(
			const asCSourceSnapshot& Snapshot,
			const asSTypeSyntax& Syntax,
			asSASTProjectedTypeSyntax& Out)
		{
			Out = {};
			if (!Syntax.IsValid()) return true;
			Out.bPresent = true;
			Out.Spelling = Syntax.GetSpelling();
			Out.NameParts = Syntax.NameParts;
			Out.bGlobal = Syntax.bGlobal;
			Out.bConst = Syntax.bConst;
			Out.bHandle = Syntax.bHandle;
			Out.Reference = Syntax.Reference;
			if (!MakeAnchor(Snapshot, Syntax.Range, Out.Source)) return false;
			Out.ComponentRanges.Reserve(Syntax.ComponentRanges.Num());
			for (const asCSourceRange& Range : Syntax.ComponentRanges)
			{
				asSStableSourceAnchor Anchor;
				if (!MakeAnchor(Snapshot, Range, Anchor)) return false;
				Out.ComponentRanges.Add(MoveTemp(Anchor));
			}
			Out.GenericArguments.Reserve(Syntax.GenericArguments.Num());
			for (const asSTypeSyntax& Argument : Syntax.GenericArguments)
			{
				asSASTProjectedTypeSyntax Projected;
				if (!ProjectTypeSyntax(Snapshot, Argument, Projected)) return false;
				Out.GenericArguments.Add(MoveTemp(Projected));
			}
			Out.ArrayDimensions.Reserve(Syntax.ArrayDimensions.Num());
			for (const asCSourceRange& Dimension : Syntax.ArrayDimensions)
			{
				asSStableSourceAnchor Anchor;
				if (!MakeAnchor(Snapshot, Dimension, Anchor)) return false;
				Out.ArrayDimensions.Add(MoveTemp(Anchor));
			}
			return true;
		}

		bool FlattenAnnotationArguments(
			const asCSourceSnapshot& Snapshot,
			TConstArrayView<asSAnnotationArgument> Arguments,
			uint32 Parent,
			TArray<asSASTProjectedAnnotationArgument>& Out)
		{
			for (const asSAnnotationArgument& Argument : Arguments)
			{
				asSASTProjectedAnnotationArgument Projected;
				Projected.ParentIndex = Parent;
				Projected.Name = Argument.Name;
				Projected.Kind = Argument.Kind;
				Projected.Value = Argument.Value;
				if (!MakeAnchor(Snapshot, Argument.Range, Projected.Source)) return false;
				if (Argument.ValueRange.IsOrdered()
					&& !MakeAnchor(Snapshot, Argument.ValueRange, Projected.ValueSource)) return false;
				const uint32 Index = static_cast<uint32>(Out.Num());
				Out.Add(MoveTemp(Projected));
				if (!FlattenAnnotationArguments(Snapshot, Argument.Arguments, Index, Out)) return false;
			}
			return true;
		}

		bool ProjectAuthoredAccessUse(
			const asCSourceSnapshot& Snapshot,
			const asSAuthoredAccessUse* Use,
			asSASTProjectedAuthoredAccessUse& Out)
		{
			Out = {};
			if (!Use) return true;
			Out.bPresent = true;
			Out.Name = Use->Name;
			return MakeAnchor(Snapshot, Use->KeywordRange, Out.KeywordSource)
				&& MakeAnchor(Snapshot, Use->ColonRange, Out.ColonSource)
				&& MakeAnchor(Snapshot, Use->NameRange, Out.NameSource);
		}

		template <typename T>
		bool ArrayEquals(TConstArrayView<T> Left, TConstArrayView<T> Right)
		{
			if (Left.Num() != Right.Num()) return false;
			for (int32 Index = 0; Index < Left.Num(); ++Index)
				if (!Left[Index].Equals(Right[Index])) return false;
			return true;
		}

		bool AnchorArrayEquals(
			TConstArrayView<asSStableSourceAnchor> Left,
			TConstArrayView<asSStableSourceAnchor> Right)
		{
			if (Left.Num() != Right.Num()) return false;
			for (int32 Index = 0; Index < Left.Num(); ++Index)
				if (!AnchorEquals(Left[Index], Right[Index])) return false;
			return true;
		}

		template <typename T>
		bool SetPayload(asFASTProjectionPayload& Out, T Value)
		{
			Out.Set<T>(MoveTemp(Value));
			return true;
		}

		bool PayloadEquals(const asFASTProjectionPayload& Left, const asFASTProjectionPayload& Right)
		{
			if (Left.GetIndex() != Right.GetIndex()) return false;
			if (Left.TryGet<asSASTEmptyPayload>()) return true;
			if (const auto* A = Left.TryGet<asSASTNamedDeclPayload>())
				return A->Name == Right.TryGet<asSASTNamedDeclPayload>()->Name;
			if (const auto* A = Left.TryGet<asSASTRecordDeclPayload>())
			{
				const auto& B = *Right.TryGet<asSASTRecordDeclPayload>();
				return A->Name == B.Name && A->DeclaredTypeKind == B.DeclaredTypeKind
					&& A->UnresolvedBases == B.UnresolvedBases && A->Modifiers == B.Modifiers;
			}
			if (const auto* A = Left.TryGet<asSASTVarDeclPayload>())
			{
				const auto& B = *Right.TryGet<asSASTVarDeclPayload>();
				return A->Name == B.Name && A->TypeSyntax.Equals(B.TypeSyntax)
					&& A->bHasDeferredInitializer == B.bHasDeferredInitializer
					&& AnchorEquals(A->DeferredInitializerSource, B.DeferredInitializerSource);
			}
			if (const auto* A = Left.TryGet<asSASTEnumConstantDeclPayload>())
			{
				const auto& B = *Right.TryGet<asSASTEnumConstantDeclPayload>();
				return A->Name == B.Name && A->Value == B.Value
					&& A->bExplicitValue == B.bExplicitValue
					&& A->bResolvedValue == B.bResolvedValue
					&& A->bHasDeferredInitializer == B.bHasDeferredInitializer
					&& AnchorEquals(A->DeferredInitializerSource, B.DeferredInitializerSource);
			}
			if (const auto* A = Left.TryGet<asSASTFunctionDeclPayload>())
			{
				const auto& B = *Right.TryGet<asSASTFunctionDeclPayload>();
				return A->Name == B.Name && A->ReturnTypeSyntax.Equals(B.ReturnTypeSyntax)
					&& A->bConstMethod == B.bConstMethod && A->FunctionKind == B.FunctionKind
					&& A->Modifiers == B.Modifiers && A->bHasDeferredBody == B.bHasDeferredBody
					&& AnchorEquals(A->DeferredBodySource, B.DeferredBodySource)
					&& A->LambdaOrdinal == B.LambdaOrdinal
					&& AnchorArrayEquals(A->DeferredStatementSources, B.DeferredStatementSources);
			}
			if (const auto* A = Left.TryGet<asSASTCallableTypeDeclPayload>())
			{
				const auto& B = *Right.TryGet<asSASTCallableTypeDeclPayload>();
				return A->Name == B.Name && A->ReturnTypeSyntax.Equals(B.ReturnTypeSyntax)
					&& A->bMulticast == B.bMulticast;
			}
			if (const auto* A = Left.TryGet<asSASTTypeAliasDeclPayload>())
			{
				const auto& B = *Right.TryGet<asSASTTypeAliasDeclPayload>();
				return A->Name == B.Name && A->TypeSyntax.Equals(B.TypeSyntax);
			}
			if (const auto* A = Left.TryGet<asSASTAccessSpecifierDeclPayload>())
			{
				const auto& B = *Right.TryGet<asSASTAccessSpecifierDeclPayload>();
				return A->Name == B.Name && A->BaseAccess == B.BaseAccess
					&& AnchorEquals(A->KeywordSource, B.KeywordSource)
					&& AnchorEquals(A->NameSource, B.NameSource)
					&& AnchorEquals(A->EqualsSource, B.EqualsSource)
					&& AnchorEquals(A->BaseAccessSource, B.BaseAccessSource)
					&& AnchorEquals(A->SemicolonSource, B.SemicolonSource)
					&& ArrayEquals<asSASTProjectedAccessPermission>(A->Permissions, B.Permissions);
			}
			if (const auto* A = Left.TryGet<asSASTIntegerLiteralPayload>())
				return A->Value == Right.TryGet<asSASTIntegerLiteralPayload>()->Value;
			if (const auto* A = Left.TryGet<asSASTBooleanLiteralPayload>())
				return A->Value == Right.TryGet<asSASTBooleanLiteralPayload>()->Value;
			if (const auto* A = Left.TryGet<asSASTFloatingLiteralPayload>())
				return A->IEEE754Bits == Right.TryGet<asSASTFloatingLiteralPayload>()->IEEE754Bits;
			if (const auto* A = Left.TryGet<asSASTImplicitCastPayload>())
				return A->Conversion == Right.TryGet<asSASTImplicitCastPayload>()->Conversion;
			if (const auto* A = Left.TryGet<asSASTUserConversionPayload>())
				return A->Mode == Right.TryGet<asSASTUserConversionPayload>()->Mode;
			if (const auto* A = Left.TryGet<asSASTUnaryOperatorPayload>())
			{
				const auto& B = *Right.TryGet<asSASTUnaryOperatorPayload>();
				return A->Operator == B.Operator && AnchorEquals(A->OperatorSource, B.OperatorSource);
			}
			if (const auto* A = Left.TryGet<asSASTBinaryOperatorPayload>())
			{
				const auto& B = *Right.TryGet<asSASTBinaryOperatorPayload>();
				return A->Operator == B.Operator && AnchorEquals(A->OperatorSource, B.OperatorSource);
			}
			if (const auto* A = Left.TryGet<asSASTCallExprPayload>())
				return A->ArgumentSourceOrdinals == Right.TryGet<asSASTCallExprPayload>()->ArgumentSourceOrdinals;
			if (const auto* A = Left.TryGet<asSASTMemberExprPayload>())
				return AnchorEquals(A->MemberSource, Right.TryGet<asSASTMemberExprPayload>()->MemberSource);
			if (const auto* A = Left.TryGet<asSASTExplicitCastPayload>())
				return A->TypeSyntax.Equals(Right.TryGet<asSASTExplicitCastPayload>()->TypeSyntax);
			if (const auto* A = Left.TryGet<asSASTConstructExprPayload>())
				return A->TypeSyntax.Equals(Right.TryGet<asSASTConstructExprPayload>()->TypeSyntax);
			if (const auto* A = Left.TryGet<asSASTStringLiteralPayload>())
				return A->Value == Right.TryGet<asSASTStringLiteralPayload>()->Value;
			if (const auto* A = Left.TryGet<asSASTThisExprPayload>())
				return A->bImplicit == Right.TryGet<asSASTThisExprPayload>()->bImplicit;
			if (const auto* A = Left.TryGet<asSASTLambdaExprPayload>())
			{
				const auto& B = *Right.TryGet<asSASTLambdaExprPayload>();
				return A->CaptureCount == B.CaptureCount && A->bCapturesThis == B.bCapturesThis;
			}
			if (const auto* A = Left.TryGet<asSASTInitListExprPayload>())
			{
				const auto& B = *Right.TryGet<asSASTInitListExprPayload>();
				return A->InitializerIdentity == B.InitializerIdentity
					&& A->ElementPatternIndices == B.ElementPatternIndices;
			}
			if (Left.TryGet<asSASTValueInitExprPayload>()) return true;
			if (const auto* A = Left.TryGet<asSASTDeclGroupStmtPayload>())
				return A->DeclarationCount == Right.TryGet<asSASTDeclGroupStmtPayload>()->DeclarationCount;
			if (const auto* A = Left.TryGet<asSASTClassDefaultStmtPayload>())
				return AnchorEquals(A->KeywordSource, Right.TryGet<asSASTClassDefaultStmtPayload>()->KeywordSource);
			if (const auto* A = Left.TryGet<asSASTForStmtPayload>())
			{
				const auto& B = *Right.TryGet<asSASTForStmtPayload>();
				return A->InitializerCount == B.InitializerCount && A->bHasCondition == B.bHasCondition
					&& A->IncrementCount == B.IncrementCount && AnchorEquals(A->HeaderSource, B.HeaderSource);
			}
			if (const auto* A = Left.TryGet<asSASTSwitchStmtPayload>())
			{
				const auto& B = *Right.TryGet<asSASTSwitchStmtPayload>();
				return A->ClauseCount == B.ClauseCount && A->DefaultOrdinal == B.DefaultOrdinal
					&& AnchorEquals(A->HeaderSource, B.HeaderSource);
			}
			if (const auto* A = Left.TryGet<asSASTCaseStmtPayload>())
			{
				const auto& B = *Right.TryGet<asSASTCaseStmtPayload>();
				return A->StatementCount == B.StatementCount && A->ResolvedValueBits == B.ResolvedValueBits
					&& AnchorEquals(A->KeywordSource, B.KeywordSource) && AnchorEquals(A->ColonSource, B.ColonSource);
			}
			if (const auto* A = Left.TryGet<asSASTDefaultStmtPayload>())
			{
				const auto& B = *Right.TryGet<asSASTDefaultStmtPayload>();
				return A->StatementCount == B.StatementCount
					&& AnchorEquals(A->KeywordSource, B.KeywordSource) && AnchorEquals(A->ColonSource, B.ColonSource);
			}
			if (Left.TryGet<asSASTFallthroughStmtPayload>()) return true;
			if (const auto* A = Left.TryGet<asSASTForeachStmtPayload>())
			{
				const auto& B = *Right.TryGet<asSASTForeachStmtPayload>();
				return A->bHasKey == B.bHasKey && AnchorEquals(A->HeaderSource, B.HeaderSource)
					&& AnchorEquals(A->ColonSource, B.ColonSource);
			}
			if (const auto* A = Left.TryGet<asSASTBuiltinTypePayload>())
				return A->Primitive == Right.TryGet<asSASTBuiltinTypePayload>()->Primitive;
			if (const auto* A = Left.TryGet<asSASTNominalTypePayload>())
				return A->DeclarationIdentity == Right.TryGet<asSASTNominalTypePayload>()->DeclarationIdentity;
			if (const auto* A = Left.TryGet<asSASTFunctionTypePayload>())
				return A->PassingModes == Right.TryGet<asSASTFunctionTypePayload>()->PassingModes;
			if (const auto* A = Left.TryGet<asSASTFlagAttrPayload>())
			{
				const auto& B = *Right.TryGet<asSASTFlagAttrPayload>();
				return A->Name == B.Name && A->AllowedTargets == B.AllowedTargets;
			}
			if (const auto* A = Left.TryGet<asSASTStringAttrPayload>())
			{
				const auto& B = *Right.TryGet<asSASTStringAttrPayload>();
				return A->Name == B.Name && A->Value == B.Value && A->AllowedTargets == B.AllowedTargets;
			}
			if (const auto* A = Left.TryGet<asSASTAnnotationAttrPayload>())
			{
				const auto& B = *Right.TryGet<asSASTAnnotationAttrPayload>();
				return A->AnnotationKind == B.AnnotationKind && A->AllowedTargets == B.AllowedTargets
					&& ArrayEquals<asSASTProjectedAnnotationArgument>(A->Arguments, B.Arguments);
			}
			return false;
		}

		const TCHAR* NodeKindName(const asSASTProjectionRecord& Record)
		{
			switch (Record.Family)
			{
			case asEASTNodeFamily::Decl:
				switch (static_cast<asEDeclKind>(Record.Kind))
				{
#define AS_DECL_NODE(Class, Base, Name) case asEDeclKind::Name: return TEXT(#Name);
#include "frontend/as_decl_nodes.def"
#undef AS_DECL_NODE
				case asEDeclKind::ParmVar: return TEXT("ParmVar");
				default: return TEXT("UnknownDecl");
				}
			case asEASTNodeFamily::Stmt:
				switch (static_cast<asEStmtKind>(Record.Kind))
				{
#define AS_STMT_NODE(Class, Base, Name) case asEStmtKind::Name: return TEXT(#Name);
#include "frontend/as_stmt_nodes.def"
#undef AS_STMT_NODE
				default: return TEXT("UnknownStmt");
				}
			case asEASTNodeFamily::Type:
				switch (static_cast<asETypeKind>(Record.Kind))
				{
#define AS_TYPE_NODE(Class, Base, Name) case asETypeKind::Name: return TEXT(#Name);
#include "frontend/as_type_nodes.def"
#undef AS_TYPE_NODE
				default: return TEXT("UnknownType");
				}
			case asEASTNodeFamily::Attr:
				switch (static_cast<asEAttrKind>(Record.Kind))
				{
#define AS_ATTR_NODE(Class, Base, Name) case asEAttrKind::Name: return TEXT(#Name);
#include "frontend/as_attr_nodes.def"
#undef AS_ATTR_NODE
				default: return TEXT("UnknownAttr");
				}
			default: return TEXT("Unknown");
			}
		}

		FString PayloadSummary(const asFASTProjectionPayload& Payload)
		{
			if (const auto* Value = Payload.TryGet<asSASTNamedDeclPayload>()) return FString(TEXT("name=")) + Value->Name;
			if (const auto* Value = Payload.TryGet<asSASTRecordDeclPayload>()) return FString::Printf(TEXT("name=%s type-kind=%u modifiers=%u bases=[%s]"), *Value->Name, static_cast<uint8>(Value->DeclaredTypeKind), static_cast<uint8>(Value->Modifiers), *FString::Join(Value->UnresolvedBases, TEXT(",")));
			if (const auto* Value = Payload.TryGet<asSASTVarDeclPayload>()) return FString::Printf(TEXT("name=%s type=%s deferred-initializer=%u"), *Value->Name, *Value->TypeSyntax.Spelling, Value->bHasDeferredInitializer ? 1 : 0);
			if (const auto* Value = Payload.TryGet<asSASTEnumConstantDeclPayload>()) return FString::Printf(TEXT("name=%s value=%lld explicit=%u resolved=%u deferred=%u"), *Value->Name, Value->Value, Value->bExplicitValue ? 1 : 0, Value->bResolvedValue ? 1 : 0, Value->bHasDeferredInitializer ? 1 : 0);
			if (const auto* Value = Payload.TryGet<asSASTFunctionDeclPayload>()) return FString::Printf(TEXT("name=%s return=%s kind=%u modifiers=%u const=%u lambda-ordinal=%u deferred-statements=%d"), *Value->Name, *Value->ReturnTypeSyntax.Spelling, static_cast<uint8>(Value->FunctionKind), static_cast<uint16>(Value->Modifiers), Value->bConstMethod ? 1 : 0, Value->LambdaOrdinal, Value->DeferredStatementSources.Num());
			if (const auto* Value = Payload.TryGet<asSASTCallableTypeDeclPayload>()) return FString::Printf(TEXT("name=%s return=%s multicast=%u"), *Value->Name, *Value->ReturnTypeSyntax.Spelling, Value->bMulticast ? 1 : 0);
			if (const auto* Value = Payload.TryGet<asSASTTypeAliasDeclPayload>()) return FString::Printf(TEXT("name=%s target=%s"), *Value->Name, *Value->TypeSyntax.Spelling);
			if (const auto* Value = Payload.TryGet<asSASTAccessSpecifierDeclPayload>())
			{
				TArray<FString> Permissions;
				for (const asSASTProjectedAccessPermission& Permission : Value->Permissions)
				{
					TArray<FString> Modifiers;
					for (const asSASTProjectedAccessPermissionModifier& Modifier : Permission.Modifiers)
						Modifiers.Add(FString::FromInt(static_cast<uint8>(Modifier.Kind)));
					Permissions.Add(FString::Printf(TEXT("%u:%s(%s)"), static_cast<uint8>(Permission.Subject),
						*Permission.Name, *FString::Join(Modifiers, TEXT(","))));
				}
				return FString::Printf(TEXT("name=%s base=%u permissions=[%s]"), *Value->Name,
					static_cast<uint8>(Value->BaseAccess), *FString::Join(Permissions, TEXT(",")));
			}
			if (const auto* Value = Payload.TryGet<asSASTIntegerLiteralPayload>()) return FString::Printf(TEXT("value=%lld"), Value->Value);
			if (const auto* Value = Payload.TryGet<asSASTBooleanLiteralPayload>()) return Value->Value ? TEXT("value=true") : TEXT("value=false");
			if (const auto* Value = Payload.TryGet<asSASTFloatingLiteralPayload>()) return FString::Printf(TEXT("ieee754=0x%016llx"), Value->IEEE754Bits);
			if (const auto* Value = Payload.TryGet<asSASTImplicitCastPayload>()) return FString::Printf(TEXT("conversion=%u"), static_cast<uint8>(Value->Conversion));
			if (const auto* Value = Payload.TryGet<asSASTUserConversionPayload>()) return FString::Printf(TEXT("mode=%u"), static_cast<uint8>(Value->Mode));
			if (const auto* Value = Payload.TryGet<asSASTUnaryOperatorPayload>()) return FString::Printf(TEXT("operator=%u"), static_cast<uint8>(Value->Operator));
			if (const auto* Value = Payload.TryGet<asSASTBinaryOperatorPayload>()) return FString::Printf(TEXT("operator=%u"), static_cast<uint8>(Value->Operator));
			if (const auto* Value = Payload.TryGet<asSASTCallExprPayload>())
			{
				TArray<FString> Ordinals;
				for (int32 Ordinal : Value->ArgumentSourceOrdinals) Ordinals.Add(FString::FromInt(Ordinal));
				return FString::Printf(TEXT("argument-source-ordinals=[%s]"), *FString::Join(Ordinals, TEXT(",")));
			}
			if (const auto* Value = Payload.TryGet<asSASTMemberExprPayload>()) return FString::Printf(TEXT("member-source=%u-%u"), Value->MemberSource.BeginByteOffset, Value->MemberSource.EndByteOffset);
			if (const auto* Value = Payload.TryGet<asSASTExplicitCastPayload>()) return FString(TEXT("target=")) + Value->TypeSyntax.Spelling;
			if (const auto* Value = Payload.TryGet<asSASTConstructExprPayload>()) return FString(TEXT("type=")) + Value->TypeSyntax.Spelling;
			if (const auto* Value = Payload.TryGet<asSASTStringLiteralPayload>()) return FString::Printf(TEXT("value=\"%s\""), *Value->Value);
			if (const auto* Value = Payload.TryGet<asSASTThisExprPayload>()) return Value->bImplicit ? TEXT("implicit=true") : TEXT("implicit=false");
			if (const auto* Value = Payload.TryGet<asSASTLambdaExprPayload>()) return FString::Printf(TEXT("captures=%u this=%u"), Value->CaptureCount, Value->bCapturesThis ? 1 : 0);
			if (const auto* Value = Payload.TryGet<asSASTInitListExprPayload>())
			{
				TArray<FString> Patterns;
				for (int32 Pattern : Value->ElementPatternIndices) Patterns.Add(FString::FromInt(Pattern));
				return FString::Printf(TEXT("initializer=%s patterns=[%s]"), *Value->InitializerIdentity.ToHexString(), *FString::Join(Patterns, TEXT(",")));
			}
			if (Payload.TryGet<asSASTValueInitExprPayload>()) return TEXT("value-init");
			if (const auto* Value = Payload.TryGet<asSASTDeclGroupStmtPayload>()) return FString::Printf(TEXT("declarations=%u"), Value->DeclarationCount);
			if (const auto* Value = Payload.TryGet<asSASTClassDefaultStmtPayload>()) return FString::Printf(TEXT("keyword=%u-%u"), Value->KeywordSource.BeginByteOffset, Value->KeywordSource.EndByteOffset);
			if (const auto* Value = Payload.TryGet<asSASTForStmtPayload>()) return FString::Printf(TEXT("initializers=%u condition=%u increments=%u header=%u-%u"), Value->InitializerCount, Value->bHasCondition ? 1 : 0, Value->IncrementCount, Value->HeaderSource.BeginByteOffset, Value->HeaderSource.EndByteOffset);
			if (const auto* Value = Payload.TryGet<asSASTSwitchStmtPayload>()) return FString::Printf(TEXT("clauses=%u default=%u header=%u-%u"), Value->ClauseCount, Value->DefaultOrdinal, Value->HeaderSource.BeginByteOffset, Value->HeaderSource.EndByteOffset);
			if (const auto* Value = Payload.TryGet<asSASTCaseStmtPayload>()) return FString::Printf(TEXT("statements=%u value=0x%016llx"), Value->StatementCount, Value->ResolvedValueBits);
			if (const auto* Value = Payload.TryGet<asSASTDefaultStmtPayload>()) return FString::Printf(TEXT("statements=%u"), Value->StatementCount);
			if (Payload.TryGet<asSASTFallthroughStmtPayload>()) return TEXT("explicit-fallthrough");
			if (const auto* Value = Payload.TryGet<asSASTForeachStmtPayload>()) return FString::Printf(TEXT("key=%u header=%u-%u colon=%u-%u"), Value->bHasKey ? 1 : 0, Value->HeaderSource.BeginByteOffset, Value->HeaderSource.EndByteOffset, Value->ColonSource.BeginByteOffset, Value->ColonSource.EndByteOffset);
			if (const auto* Value = Payload.TryGet<asSASTBuiltinTypePayload>()) return FString::Printf(TEXT("primitive=%u"), static_cast<uint8>(Value->Primitive));
			if (const auto* Value = Payload.TryGet<asSASTNominalTypePayload>()) return FString(TEXT("declaration=")) + Value->DeclarationIdentity.ToHexString();
			if (const auto* Value = Payload.TryGet<asSASTFunctionTypePayload>())
			{
				TArray<FString> Modes;
				for (asEParameterPassingMode Mode : Value->PassingModes) Modes.Add(FString::FromInt(static_cast<uint8>(Mode)));
				return FString::Printf(TEXT("passing-modes=[%s]"), *FString::Join(Modes, TEXT(",")));
			}
			if (const auto* Value = Payload.TryGet<asSASTFlagAttrPayload>()) return FString(TEXT("name=")) + Value->Name;
			if (const auto* Value = Payload.TryGet<asSASTStringAttrPayload>()) return FString::Printf(TEXT("name=%s value=%s"), *Value->Name, *Value->Value);
			if (const auto* Value = Payload.TryGet<asSASTAnnotationAttrPayload>())
			{
				TArray<FString> Arguments;
				for (const auto& Argument : Value->Arguments)
					Arguments.Add(FString::Printf(TEXT("%u:%s=%s@%u"), static_cast<uint8>(Argument.Kind), *Argument.Name, *Argument.Value, Argument.ParentIndex));
				return FString::Printf(TEXT("annotation=%u targets=%u arguments=[%s]"), static_cast<uint8>(Value->AnnotationKind), static_cast<uint8>(Value->AllowedTargets), *FString::Join(Arguments, TEXT(",")));
			}
			return {};
		}

		FString EscapeJson(const FString& Value)
		{
			FString Result;
			Result.Reserve(Value.Len() + 8);
			for (TCHAR Character : Value)
			{
				switch (Character)
				{
				case TEXT('\\'): Result += TEXT("\\\\"); break;
				case TEXT('"'): Result += TEXT("\\\""); break;
				case TEXT('\n'): Result += TEXT("\\n"); break;
				case TEXT('\r'): Result += TEXT("\\r"); break;
				case TEXT('\t'): Result += TEXT("\\t"); break;
				default:
					if (Character < 0x20) Result += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Character));
					else Result.AppendChar(Character);
					break;
				}
			}
			return Result;
		}

		FString AnchorJson(const asSStableSourceAnchor& Anchor)
		{
			return FString::Printf(
				TEXT("{\"source\":\"%s\",\"revision\":%llu,\"begin\":%u,\"end\":%u}"),
				*EscapeJson(Anchor.LogicalSourceKey), Anchor.ContentRevision,
				Anchor.BeginByteOffset, Anchor.EndByteOffset);
		}

		FString StringArrayJson(TConstArrayView<FString> Values)
		{
			FString Result = TEXT("[");
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				if (Index) Result += TEXT(",");
				Result += FString::Printf(TEXT("\"%s\""), *EscapeJson(Values[Index]));
			}
			return Result + TEXT("]");
		}

		FString TypeSyntaxJson(const asSASTProjectedTypeSyntax& Syntax)
		{
			if (!Syntax.bPresent) return TEXT("{\"present\":false}");
			FString Components = TEXT("[");
			for (int32 Index = 0; Index < Syntax.ComponentRanges.Num(); ++Index)
			{
				if (Index) Components += TEXT(",");
				Components += AnchorJson(Syntax.ComponentRanges[Index]);
			}
			Components += TEXT("]");
			FString Arguments = TEXT("[");
			for (int32 Index = 0; Index < Syntax.GenericArguments.Num(); ++Index)
			{
				if (Index) Arguments += TEXT(",");
				Arguments += TypeSyntaxJson(Syntax.GenericArguments[Index]);
			}
			Arguments += TEXT("]");
			FString Dimensions = TEXT("[");
			for (int32 Index = 0; Index < Syntax.ArrayDimensions.Num(); ++Index)
			{
				if (Index) Dimensions += TEXT(",");
				Dimensions += AnchorJson(Syntax.ArrayDimensions[Index]);
			}
			Dimensions += TEXT("]");
			return FString::Printf(
				TEXT("{\"present\":true,\"spelling\":\"%s\",\"parts\":%s,\"source\":%s,\"components\":%s,\"genericArguments\":%s,\"arrayDimensions\":%s,\"global\":%s,\"const\":%s,\"handle\":%s,\"reference\":%u}"),
				*EscapeJson(Syntax.Spelling), *StringArrayJson(Syntax.NameParts), *AnchorJson(Syntax.Source),
				*Components, *Arguments, *Dimensions, Syntax.bGlobal ? TEXT("true") : TEXT("false"),
				Syntax.bConst ? TEXT("true") : TEXT("false"), Syntax.bHandle ? TEXT("true") : TEXT("false"),
				static_cast<uint8>(Syntax.Reference));
		}

		FString PayloadJson(const asFASTProjectionPayload& Payload)
		{
			if (Payload.TryGet<asSASTEmptyPayload>()) return TEXT("{\"type\":\"empty\"}");
			if (const auto* Value = Payload.TryGet<asSASTNamedDeclPayload>())
				return FString::Printf(TEXT("{\"type\":\"named-decl\",\"name\":\"%s\"}"), *EscapeJson(Value->Name));
			if (const auto* Value = Payload.TryGet<asSASTRecordDeclPayload>())
				return FString::Printf(TEXT("{\"type\":\"record-decl\",\"name\":\"%s\",\"declaredTypeKind\":%u,\"modifiers\":%u,\"bases\":%s}"),
					*EscapeJson(Value->Name), static_cast<uint8>(Value->DeclaredTypeKind), static_cast<uint8>(Value->Modifiers), *StringArrayJson(Value->UnresolvedBases));
			if (const auto* Value = Payload.TryGet<asSASTVarDeclPayload>())
				return FString::Printf(TEXT("{\"type\":\"var-decl\",\"name\":\"%s\",\"typeSyntax\":%s,\"hasDeferredInitializer\":%s,\"deferredInitializer\":%s}"),
					*EscapeJson(Value->Name), *TypeSyntaxJson(Value->TypeSyntax),
					Value->bHasDeferredInitializer ? TEXT("true") : TEXT("false"), *AnchorJson(Value->DeferredInitializerSource));
			if (const auto* Value = Payload.TryGet<asSASTEnumConstantDeclPayload>())
				return FString::Printf(TEXT("{\"type\":\"enum-constant\",\"name\":\"%s\",\"value\":%lld,\"explicit\":%s,\"resolved\":%s,\"hasDeferredInitializer\":%s,\"deferredInitializer\":%s}"),
					*EscapeJson(Value->Name), Value->Value, Value->bExplicitValue ? TEXT("true") : TEXT("false"),
					Value->bResolvedValue ? TEXT("true") : TEXT("false"), Value->bHasDeferredInitializer ? TEXT("true") : TEXT("false"),
					*AnchorJson(Value->DeferredInitializerSource));
			if (const auto* Value = Payload.TryGet<asSASTFunctionDeclPayload>())
			{
				FString DeferredStatements = TEXT("[");
				for (int32 Index = 0; Index < Value->DeferredStatementSources.Num(); ++Index)
				{
					if (Index) DeferredStatements += TEXT(",");
					DeferredStatements += AnchorJson(Value->DeferredStatementSources[Index]);
				}
				DeferredStatements += TEXT("]");
				return FString::Printf(TEXT("{\"type\":\"function-decl\",\"name\":\"%s\",\"returnTypeSyntax\":%s,\"constMethod\":%s,\"functionKind\":%u,\"modifiers\":%u,\"hasDeferredBody\":%s,\"deferredBody\":%s,\"lambdaOrdinal\":%u,\"deferredStatements\":%s}"),
					*EscapeJson(Value->Name), *TypeSyntaxJson(Value->ReturnTypeSyntax),
					Value->bConstMethod ? TEXT("true") : TEXT("false"), static_cast<uint8>(Value->FunctionKind),
					static_cast<uint16>(Value->Modifiers), Value->bHasDeferredBody ? TEXT("true") : TEXT("false"),
					*AnchorJson(Value->DeferredBodySource), Value->LambdaOrdinal, *DeferredStatements);
			}
			if (const auto* Value = Payload.TryGet<asSASTCallableTypeDeclPayload>())
				return FString::Printf(TEXT("{\"type\":\"callable-decl\",\"name\":\"%s\",\"returnTypeSyntax\":%s,\"multicast\":%s}"),
					*EscapeJson(Value->Name), *TypeSyntaxJson(Value->ReturnTypeSyntax), Value->bMulticast ? TEXT("true") : TEXT("false"));
			if (const auto* Value = Payload.TryGet<asSASTTypeAliasDeclPayload>())
				return FString::Printf(TEXT("{\"type\":\"type-alias-decl\",\"name\":\"%s\",\"typeSyntax\":%s}"),
					*EscapeJson(Value->Name), *TypeSyntaxJson(Value->TypeSyntax));
			if (const auto* Value = Payload.TryGet<asSASTAccessSpecifierDeclPayload>())
			{
				FString Permissions = TEXT("[");
				for (int32 PermissionIndex = 0; PermissionIndex < Value->Permissions.Num(); ++PermissionIndex)
				{
					if (PermissionIndex) Permissions += TEXT(",");
					const asSASTProjectedAccessPermission& Permission = Value->Permissions[PermissionIndex];
					FString Modifiers = TEXT("[");
					for (int32 ModifierIndex = 0; ModifierIndex < Permission.Modifiers.Num(); ++ModifierIndex)
					{
						if (ModifierIndex) Modifiers += TEXT(",");
						const asSASTProjectedAccessPermissionModifier& Modifier = Permission.Modifiers[ModifierIndex];
						Modifiers += FString::Printf(TEXT("{\"kind\":%u,\"source\":%s}"),
							static_cast<uint8>(Modifier.Kind), *AnchorJson(Modifier.Source));
					}
					Modifiers += TEXT("]");
					Permissions += FString::Printf(
						TEXT("{\"subject\":%u,\"name\":\"%s\",\"subjectSource\":%s,\"source\":%s,\"modifiers\":%s}"),
						static_cast<uint8>(Permission.Subject), *EscapeJson(Permission.Name),
						*AnchorJson(Permission.SubjectSource), *AnchorJson(Permission.Source), *Modifiers);
				}
				Permissions += TEXT("]");
				return FString::Printf(
					TEXT("{\"type\":\"access-specifier-decl\",\"name\":\"%s\",\"baseAccess\":%u,\"keywordSource\":%s,\"nameSource\":%s,\"equalsSource\":%s,\"baseAccessSource\":%s,\"semicolonSource\":%s,\"permissions\":%s}"),
					*EscapeJson(Value->Name), static_cast<uint8>(Value->BaseAccess),
					*AnchorJson(Value->KeywordSource), *AnchorJson(Value->NameSource),
					*AnchorJson(Value->EqualsSource), *AnchorJson(Value->BaseAccessSource),
					*AnchorJson(Value->SemicolonSource), *Permissions);
			}
			if (const auto* Value = Payload.TryGet<asSASTIntegerLiteralPayload>())
				return FString::Printf(TEXT("{\"type\":\"integer-literal\",\"bits\":\"%016llx\"}"), static_cast<uint64>(Value->Value));
			if (const auto* Value = Payload.TryGet<asSASTBooleanLiteralPayload>())
				return Value->Value ? TEXT("{\"type\":\"boolean-literal\",\"value\":true}") : TEXT("{\"type\":\"boolean-literal\",\"value\":false}");
			if (const auto* Value = Payload.TryGet<asSASTFloatingLiteralPayload>())
				return FString::Printf(TEXT("{\"type\":\"floating-literal\",\"ieee754\":\"%016llx\"}"), Value->IEEE754Bits);
			if (const auto* Value = Payload.TryGet<asSASTImplicitCastPayload>())
				return FString::Printf(TEXT("{\"type\":\"implicit-cast\",\"conversion\":%u}"), static_cast<uint8>(Value->Conversion));
			if (const auto* Value = Payload.TryGet<asSASTUserConversionPayload>())
				return FString::Printf(TEXT("{\"type\":\"user-conversion\",\"mode\":%u}"), static_cast<uint8>(Value->Mode));
			if (const auto* Value = Payload.TryGet<asSASTUnaryOperatorPayload>())
				return FString::Printf(TEXT("{\"type\":\"unary-operator\",\"operator\":%u,\"operatorSource\":%s}"),
					static_cast<uint8>(Value->Operator), *AnchorJson(Value->OperatorSource));
			if (const auto* Value = Payload.TryGet<asSASTBinaryOperatorPayload>())
				return FString::Printf(TEXT("{\"type\":\"binary-operator\",\"operator\":%u,\"operatorSource\":%s}"),
					static_cast<uint8>(Value->Operator), *AnchorJson(Value->OperatorSource));
			if (const auto* Value = Payload.TryGet<asSASTCallExprPayload>())
			{
				FString Ordinals = TEXT("[");
				for (int32 Index = 0; Index < Value->ArgumentSourceOrdinals.Num(); ++Index)
				{
					if (Index) Ordinals += TEXT(",");
					Ordinals += FString::FromInt(Value->ArgumentSourceOrdinals[Index]);
				}
				return FString::Printf(TEXT("{\"type\":\"call-expr\",\"argumentSourceOrdinals\":%s]}"), *Ordinals);
			}
			if (const auto* Value = Payload.TryGet<asSASTMemberExprPayload>())
				return FString::Printf(TEXT("{\"type\":\"member-expr\",\"memberSource\":%s}"), *AnchorJson(Value->MemberSource));
			if (const auto* Value = Payload.TryGet<asSASTExplicitCastPayload>())
				return FString::Printf(TEXT("{\"type\":\"explicit-cast\",\"typeSyntax\":%s}"), *TypeSyntaxJson(Value->TypeSyntax));
			if (const auto* Value = Payload.TryGet<asSASTConstructExprPayload>())
				return FString::Printf(TEXT("{\"type\":\"construct-expr\",\"typeSyntax\":%s}"), *TypeSyntaxJson(Value->TypeSyntax));
			if (const auto* Value = Payload.TryGet<asSASTStringLiteralPayload>())
				return FString::Printf(TEXT("{\"type\":\"string-literal\",\"value\":\"%s\"}"), *EscapeJson(Value->Value));
			if (const auto* Value = Payload.TryGet<asSASTThisExprPayload>())
				return Value->bImplicit ? TEXT("{\"type\":\"this-expr\",\"implicit\":true}") : TEXT("{\"type\":\"this-expr\",\"implicit\":false}");
			if (const auto* Value = Payload.TryGet<asSASTLambdaExprPayload>())
				return FString::Printf(TEXT("{\"type\":\"lambda-expr\",\"captureCount\":%u,\"capturesThis\":%s}"), Value->CaptureCount, Value->bCapturesThis ? TEXT("true") : TEXT("false"));
			if (const auto* Value = Payload.TryGet<asSASTInitListExprPayload>())
			{
				FString Patterns = TEXT("[");
				for (int32 Index = 0; Index < Value->ElementPatternIndices.Num(); ++Index)
				{
					if (Index) Patterns += TEXT(",");
					Patterns += FString::FromInt(Value->ElementPatternIndices[Index]);
				}
				return FString::Printf(TEXT("{\"type\":\"init-list-expr\",\"initializerIdentity\":\"%s\",\"elementPatternIndices\":%s]}"),
					*Value->InitializerIdentity.ToHexString(), *Patterns);
			}
			if (Payload.TryGet<asSASTValueInitExprPayload>()) return TEXT("{\"type\":\"value-init-expr\"}");
			if (const auto* Value = Payload.TryGet<asSASTDeclGroupStmtPayload>())
				return FString::Printf(TEXT("{\"type\":\"decl-group-stmt\",\"declarationCount\":%u}"), Value->DeclarationCount);
			if (const auto* Value = Payload.TryGet<asSASTClassDefaultStmtPayload>())
				return FString::Printf(TEXT("{\"type\":\"class-default-stmt\",\"keywordSource\":%s}"), *AnchorJson(Value->KeywordSource));
			if (const auto* Value = Payload.TryGet<asSASTForStmtPayload>())
				return FString::Printf(TEXT("{\"type\":\"for-stmt\",\"initializerCount\":%u,\"hasCondition\":%s,\"incrementCount\":%u,\"headerSource\":%s}"), Value->InitializerCount, Value->bHasCondition ? TEXT("true") : TEXT("false"), Value->IncrementCount, *AnchorJson(Value->HeaderSource));
			if (const auto* Value = Payload.TryGet<asSASTSwitchStmtPayload>())
				return FString::Printf(TEXT("{\"type\":\"switch-stmt\",\"clauseCount\":%u,\"defaultOrdinal\":%u,\"headerSource\":%s}"), Value->ClauseCount, Value->DefaultOrdinal, *AnchorJson(Value->HeaderSource));
			if (const auto* Value = Payload.TryGet<asSASTCaseStmtPayload>())
				return FString::Printf(TEXT("{\"type\":\"case-stmt\",\"statementCount\":%u,\"resolvedValueBits\":\"%016llx\",\"keywordSource\":%s,\"colonSource\":%s}"), Value->StatementCount, Value->ResolvedValueBits, *AnchorJson(Value->KeywordSource), *AnchorJson(Value->ColonSource));
			if (const auto* Value = Payload.TryGet<asSASTDefaultStmtPayload>())
				return FString::Printf(TEXT("{\"type\":\"default-stmt\",\"statementCount\":%u,\"keywordSource\":%s,\"colonSource\":%s}"), Value->StatementCount, *AnchorJson(Value->KeywordSource), *AnchorJson(Value->ColonSource));
			if (Payload.TryGet<asSASTFallthroughStmtPayload>()) return TEXT("{\"type\":\"fallthrough-stmt\"}");
			if (const auto* Value = Payload.TryGet<asSASTForeachStmtPayload>())
				return FString::Printf(TEXT("{\"type\":\"foreach-stmt\",\"hasKey\":%s,\"headerSource\":%s,\"colonSource\":%s}"), Value->bHasKey ? TEXT("true") : TEXT("false"), *AnchorJson(Value->HeaderSource), *AnchorJson(Value->ColonSource));
			if (const auto* Value = Payload.TryGet<asSASTBuiltinTypePayload>())
				return FString::Printf(TEXT("{\"type\":\"builtin-type\",\"primitive\":%u}"), static_cast<uint8>(Value->Primitive));
			if (const auto* Value = Payload.TryGet<asSASTNominalTypePayload>())
				return FString::Printf(TEXT("{\"type\":\"nominal-type\",\"declarationIdentity\":\"%s\"}"), *Value->DeclarationIdentity.ToHexString());
			if (const auto* Value = Payload.TryGet<asSASTFunctionTypePayload>())
			{
				FString Modes = TEXT("[");
				for (int32 Index = 0; Index < Value->PassingModes.Num(); ++Index)
				{
					if (Index) Modes += TEXT(",");
					Modes += FString::FromInt(static_cast<uint8>(Value->PassingModes[Index]));
				}
				return FString::Printf(TEXT("{\"type\":\"function-type\",\"passingModes\":%s]}"), *Modes);
			}
			if (const auto* Value = Payload.TryGet<asSASTFlagAttrPayload>())
				return FString::Printf(TEXT("{\"type\":\"flag-attr\",\"name\":\"%s\",\"targets\":%u}"), *EscapeJson(Value->Name), static_cast<uint8>(Value->AllowedTargets));
			if (const auto* Value = Payload.TryGet<asSASTStringAttrPayload>())
				return FString::Printf(TEXT("{\"type\":\"string-attr\",\"name\":\"%s\",\"value\":\"%s\",\"targets\":%u}"),
					*EscapeJson(Value->Name), *EscapeJson(Value->Value), static_cast<uint8>(Value->AllowedTargets));
			if (const auto* Value = Payload.TryGet<asSASTAnnotationAttrPayload>())
			{
				FString Arguments = TEXT("[");
				for (int32 Index = 0; Index < Value->Arguments.Num(); ++Index)
				{
					if (Index) Arguments += TEXT(",");
					const auto& Argument = Value->Arguments[Index];
					Arguments += FString::Printf(TEXT("{\"parent\":%u,\"name\":\"%s\",\"kind\":%u,\"value\":\"%s\",\"source\":%s,\"valueSource\":%s}"),
						Argument.ParentIndex, *EscapeJson(Argument.Name), static_cast<uint8>(Argument.Kind),
						*EscapeJson(Argument.Value), *AnchorJson(Argument.Source), *AnchorJson(Argument.ValueSource));
				}
				Arguments += TEXT("]");
				return FString::Printf(TEXT("{\"type\":\"annotation-attr\",\"annotationKind\":%u,\"targets\":%u,\"arguments\":%s}"),
					static_cast<uint8>(Value->AnnotationKind), static_cast<uint8>(Value->AllowedTargets), *Arguments);
			}
			return TEXT("{\"type\":\"unknown\"}");
		}

		bool AssignPayload(
			const asCSourceSnapshot& Snapshot,
			const asCASTNode& Node,
			asFASTProjectionPayload& Out)
		{
			switch (Node.GetFamily())
			{
			case asEASTNodeFamily::Decl:
			{
				const asCDecl& Decl = static_cast<const asCDecl&>(Node);
				switch (Decl.GetKind())
				{
				case asEDeclKind::TranslationUnit:
				case asEDeclKind::Recovery: return SetPayload(Out, asSASTEmptyPayload{});
				case asEDeclKind::Namespace:
					return SetPayload(Out, asSASTNamedDeclPayload{static_cast<const asCNamespaceDecl&>(Decl).GetName()});
				case asEDeclKind::Record:
				{
					const auto& Record = static_cast<const asCRecordDecl&>(Decl);
					asSASTRecordDeclPayload Payload;
					Payload.Name = Record.GetName(); Payload.DeclaredTypeKind = Record.GetDeclaredTypeKind();
					Payload.Modifiers = Record.GetModifiers();
					Payload.UnresolvedBases.Append(Record.GetUnresolvedBases());
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEDeclKind::Var:
				case asEDeclKind::ParmVar:
				{
					const auto& Variable = static_cast<const asCVarDecl&>(Decl);
					asSASTVarDeclPayload Payload; Payload.Name = Variable.GetName();
					if (!ProjectTypeSyntax(Snapshot, Variable.GetTypeSyntax(), Payload.TypeSyntax)) return false;
					Payload.bHasDeferredInitializer = Variable.HasDeferredInitializer();
					if (Payload.bHasDeferredInitializer
						&& !MakeAnchor(Snapshot, Variable.GetDeferredInitializerRange(),
							Payload.DeferredInitializerSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEDeclKind::EnumConstant:
				{
					const auto& Constant = static_cast<const asCEnumConstantDecl&>(Decl);
					asSASTEnumConstantDeclPayload Payload;
					Payload.Name = Constant.GetName();
					Payload.Value = Constant.GetValue();
					Payload.bExplicitValue = Constant.HasExplicitValue();
					Payload.bResolvedValue = Constant.HasResolvedValue();
					Payload.bHasDeferredInitializer = Constant.HasDeferredInitializer();
					if (Payload.bHasDeferredInitializer
						&& !MakeAnchor(Snapshot, Constant.GetDeferredInitializerRange(),
							Payload.DeferredInitializerSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEDeclKind::Function:
				{
					const auto& Function = static_cast<const asCFunctionDecl&>(Decl);
					asSASTFunctionDeclPayload Payload; Payload.Name = Function.GetName();
					Payload.bConstMethod = Function.IsConstMethod(); Payload.FunctionKind = Function.GetFunctionKind();
					Payload.Modifiers = Function.GetModifiers(); Payload.bHasDeferredBody = Function.HasDeferredBody();
					if (!ProjectTypeSyntax(Snapshot, Function.GetReturnTypeSyntax(), Payload.ReturnTypeSyntax)) return false;
					if (Payload.bHasDeferredBody && !MakeAnchor(Snapshot, Function.GetDeferredBodyRange(), Payload.DeferredBodySource)) return false;
					Payload.LambdaOrdinal = Function.GetLambdaOrdinal();
					for (const asCSourceRange& Deferred : Function.GetDeferredStatementRanges())
					{
						asSStableSourceAnchor Anchor;
						if (!MakeAnchor(Snapshot, Deferred, Anchor)) return false;
						Payload.DeferredStatementSources.Add(MoveTemp(Anchor));
					}
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEDeclKind::Delegate:
				case asEDeclKind::Event:
				{
					const auto& Callable = static_cast<const asCCallableTypeDecl&>(Decl);
					asSASTCallableTypeDeclPayload Payload; Payload.Name = Callable.GetName(); Payload.bMulticast = Callable.IsMulticast();
					if (!ProjectTypeSyntax(Snapshot, Callable.GetReturnTypeSyntax(), Payload.ReturnTypeSyntax)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEDeclKind::TypeAlias:
				{
					const auto& Alias = static_cast<const asCTypeAliasDecl&>(Decl);
					asSASTTypeAliasDeclPayload Payload; Payload.Name = Alias.GetName();
					if (!ProjectTypeSyntax(Snapshot, Alias.GetTypeSyntax(), Payload.TypeSyntax)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEDeclKind::AccessSpecifier:
				{
					const auto& Access = static_cast<const asCAccessSpecifierDecl&>(Decl);
					const asSAccessSpecifierSourceInfo& Source = Access.GetSourceInfo();
					asSASTAccessSpecifierDeclPayload Payload;
					Payload.Name = Access.GetName();
					Payload.BaseAccess = Access.GetBaseAccess();
					if (!MakeAnchor(Snapshot, Source.KeywordRange, Payload.KeywordSource)
						|| !MakeAnchor(Snapshot, Source.NameRange, Payload.NameSource)
						|| !MakeAnchor(Snapshot, Source.EqualsRange, Payload.EqualsSource)
						|| !MakeAnchor(Snapshot, Source.BaseAccessRange, Payload.BaseAccessSource)
						|| !MakeAnchor(Snapshot, Source.SemicolonRange, Payload.SemicolonSource)) return false;
					for (const asSAuthoredAccessPermission& Permission : Access.GetPermissions())
					{
						asSASTProjectedAccessPermission Projected;
						Projected.Subject = Permission.Subject;
						Projected.Name = Permission.Name;
						if (!MakeAnchor(Snapshot, Permission.SubjectRange, Projected.SubjectSource)
							|| !MakeAnchor(Snapshot, Permission.Range, Projected.Source)) return false;
						for (const asSAccessPermissionModifier& Modifier : Permission.Modifiers)
						{
							asSASTProjectedAccessPermissionModifier ProjectedModifier;
							ProjectedModifier.Kind = Modifier.Kind;
							if (!MakeAnchor(Snapshot, Modifier.Range, ProjectedModifier.Source)) return false;
							Projected.Modifiers.Add(MoveTemp(ProjectedModifier));
						}
						Payload.Permissions.Add(MoveTemp(Projected));
					}
					return SetPayload(Out, MoveTemp(Payload));
				}
				default: return false;
				}
			}
			case asEASTNodeFamily::Stmt:
				switch (static_cast<const asCStmt&>(Node).GetKind())
				{
				case asEStmtKind::IntegerLiteral: return SetPayload(Out, asSASTIntegerLiteralPayload{static_cast<const asCIntegerLiteral&>(Node).GetValue()});
				case asEStmtKind::BooleanLiteral: return SetPayload(Out, asSASTBooleanLiteralPayload{static_cast<const asCBooleanLiteral&>(Node).GetValue()});
				case asEStmtKind::FloatingLiteral:
				{
					asSASTFloatingLiteralPayload Payload;
					const double Value = static_cast<const asCFloatingLiteral&>(Node).GetValue();
					FMemory::Memcpy(&Payload.IEEE754Bits, &Value, sizeof(Value)); return SetPayload(Out, Payload);
				}
				case asEStmtKind::ImplicitCastExpr: return SetPayload(Out, asSASTImplicitCastPayload{static_cast<const asCImplicitCastExpr&>(Node).GetConversionKind()});
				case asEStmtKind::UserConversionExpr: return SetPayload(Out, asSASTUserConversionPayload{static_cast<const asCUserConversionExpr&>(Node).GetMode()});
				case asEStmtKind::UnaryOperator:
				{
					const auto& Unary = static_cast<const asCUnaryOperator&>(Node); asSASTUnaryOperatorPayload Payload; Payload.Operator = Unary.GetOperator();
					if (!MakeAnchor(Snapshot, Unary.GetOperatorRange(), Payload.OperatorSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::BinaryOperator:
				{
					const auto& Binary = static_cast<const asCBinaryOperator&>(Node); asSASTBinaryOperatorPayload Payload; Payload.Operator = Binary.GetOperator();
					if (!MakeAnchor(Snapshot, Binary.GetOperatorRange(), Payload.OperatorSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::CallExpr:
				{
					asSASTCallExprPayload Payload;
					Payload.ArgumentSourceOrdinals.Append(
						static_cast<const asCCallExpr&>(Node).GetArgumentSourceOrdinals());
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::MemberExpr:
				{
					asSASTMemberExprPayload Payload;
					if (!MakeAnchor(Snapshot, static_cast<const asCMemberExpr&>(Node).GetMemberRange(),
						Payload.MemberSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::ExplicitCastExpr:
				{
					asSASTExplicitCastPayload Payload;
					if (!ProjectTypeSyntax(Snapshot, static_cast<const asCExplicitCastExpr&>(Node).GetTypeSyntax(),
						Payload.TypeSyntax)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::ConstructExpr:
				{
					asSASTConstructExprPayload Payload;
					if (!ProjectTypeSyntax(Snapshot, static_cast<const asCConstructExpr&>(Node).GetTypeSyntax(),
						Payload.TypeSyntax)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::StringLiteral:
					return SetPayload(Out, asSASTStringLiteralPayload{
						static_cast<const asCStringLiteral&>(Node).GetValue()});
				case asEStmtKind::ThisExpr:
					return SetPayload(Out, asSASTThisExprPayload{static_cast<const asCThisExpr&>(Node).IsImplicit()});
				case asEStmtKind::LambdaExpr:
				{
					const asCFunctionDecl* Function = static_cast<const asCLambdaExpr&>(Node).GetFunction();
					return SetPayload(Out, asSASTLambdaExprPayload{
						Function ? static_cast<uint32>(Function->GetCaptures().Num()) : 0,
						Function && Function->GetCapturedThisRecord() != nullptr});
				}
				case asEStmtKind::InitListExpr:
				{
					const auto& Init = static_cast<const asCInitListExpr&>(Node);
					asSASTInitListExprPayload Payload;
					Payload.InitializerIdentity = Init.GetInitializerIdentity();
					Payload.ElementPatternIndices.Append(Init.GetElementPatternIndices());
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::ValueInitExpr:
					return SetPayload(Out, asSASTValueInitExprPayload{});
				case asEStmtKind::DeclGroupStmt:
					return SetPayload(Out, asSASTDeclGroupStmtPayload{
						static_cast<uint32>(static_cast<const asCDeclGroupStmt&>(Node).GetDeclarations().Num())});
				case asEStmtKind::ClassDefaultStmt:
				{
					asSASTClassDefaultStmtPayload Payload;
					if (!MakeAnchor(Snapshot, static_cast<const asCClassDefaultStmt&>(Node).GetKeywordRange(),
						Payload.KeywordSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::For:
				{
					const auto& For = static_cast<const asCForStmt&>(Node);
					asSASTForStmtPayload Payload;
					Payload.InitializerCount = static_cast<uint32>(For.GetInitializers().Num());
					Payload.bHasCondition = For.GetCondition() != nullptr;
					Payload.IncrementCount = static_cast<uint32>(For.GetIncrements().Num());
					if (!MakeAnchor(Snapshot, For.GetHeaderRange(), Payload.HeaderSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::Switch:
				{
					const auto& Switch = static_cast<const asCSwitchStmt&>(Node);
					asSASTSwitchStmtPayload Payload;
					Payload.ClauseCount = static_cast<uint32>(Switch.GetClauses().Num());
					for (int32 Index = 0; Index < Switch.GetClauses().Num(); ++Index)
						if (isa<asCDefaultStmt>(Switch.GetClauses()[Index])) Payload.DefaultOrdinal = static_cast<uint32>(Index);
					if (!MakeAnchor(Snapshot, Switch.GetHeaderRange(), Payload.HeaderSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::Case:
				{
					const auto& Case = static_cast<const asCCaseStmt&>(Node);
					asSASTCaseStmtPayload Payload;
					Payload.StatementCount = static_cast<uint32>(Case.GetStatements().Num());
					Payload.ResolvedValueBits = Case.GetResolvedValueBits();
					if (!MakeAnchor(Snapshot, Case.GetKeywordRange(), Payload.KeywordSource)
						|| !MakeAnchor(Snapshot, Case.GetColonRange(), Payload.ColonSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::Default:
				{
					const auto& Default = static_cast<const asCDefaultStmt&>(Node);
					asSASTDefaultStmtPayload Payload;
					Payload.StatementCount = static_cast<uint32>(Default.GetStatements().Num());
					if (!MakeAnchor(Snapshot, Default.GetKeywordRange(), Payload.KeywordSource)
						|| !MakeAnchor(Snapshot, Default.GetColonRange(), Payload.ColonSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::Fallthrough:
					return SetPayload(Out, asSASTFallthroughStmtPayload{});
				case asEStmtKind::Foreach:
				{
					const auto& Foreach = static_cast<const asCForeachStmt&>(Node);
					asSASTForeachStmtPayload Payload;
					Payload.bHasKey = Foreach.GetKeyStatement() != nullptr;
					if (!MakeAnchor(Snapshot, Foreach.GetHeaderRange(), Payload.HeaderSource)
						|| !MakeAnchor(Snapshot, Foreach.GetColonRange(), Payload.ColonSource)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				case asEStmtKind::Compound: case asEStmtKind::If: case asEStmtKind::Return:
				case asEStmtKind::While: case asEStmtKind::Break: case asEStmtKind::Continue:
				case asEStmtKind::DeclStmt: case asEStmtKind::RecoveryStmt: case asEStmtKind::NullStmt:
				case asEStmtKind::Do: case asEStmtKind::DeclRefExpr:
				case asEStmtKind::RecoveryExpr: case asEStmtKind::ParenExpr:
				case asEStmtKind::ConditionalOperator: case asEStmtKind::ArraySubscriptExpr:
				case asEStmtKind::NullPtrLiteral: case asEStmtKind::DefaultArgumentExpr:
					return SetPayload(Out, asSASTEmptyPayload{});
				default: return false;
				}
			case asEASTNodeFamily::Type:
			{
				const asCType& Type = static_cast<const asCType&>(Node);
				if (const auto* Builtin = dyn_cast<asCBuiltinType>(&Type)) return SetPayload(Out, asSASTBuiltinTypePayload{Builtin->GetPrimitive()});
				if (const auto* Nominal = dyn_cast<asCNominalType>(&Type)) return SetPayload(Out, asSASTNominalTypePayload{Nominal->GetDeclarationIdentity()});
				if (const auto* Function = dyn_cast<asCFunctionType>(&Type))
				{
					asSASTFunctionTypePayload Payload; Payload.PassingModes.Append(Function->GetParameterPassingModes()); return SetPayload(Out, MoveTemp(Payload));
				}
				if (isa<asCGenericType>(&Type) || isa<asCErrorType>(&Type)) return SetPayload(Out, asSASTEmptyPayload{});
				return false;
			}
			case asEASTNodeFamily::Attr:
			{
				const asCAttr& Attr = static_cast<const asCAttr&>(Node);
				if (const auto* Flag = dyn_cast<asCFlagAttr>(&Attr)) return SetPayload(Out, asSASTFlagAttrPayload{Flag->GetName(), Flag->GetAllowedTargets()});
				if (const auto* String = dyn_cast<asCStringAttr>(&Attr)) return SetPayload(Out, asSASTStringAttrPayload{String->GetName(), String->GetValue(), String->GetAllowedTargets()});
				if (const auto* Annotation = dyn_cast<asCAnnotationAttr>(&Attr))
				{
					asSASTAnnotationAttrPayload Payload; Payload.AnnotationKind = Annotation->GetAnnotationKind(); Payload.AllowedTargets = Annotation->GetAllowedTargets();
					if (!FlattenAnnotationArguments(Snapshot, Annotation->GetArguments(), asSASTProjectedAnnotationArgument::NoParent, Payload.Arguments)) return false;
					return SetPayload(Out, MoveTemp(Payload));
				}
				return false;
			}
			default: return false;
			}
		}
	}

	bool asSASTProjectedTypeSyntax::Equals(const asSASTProjectedTypeSyntax& Other) const
	{
		return bPresent == Other.bPresent && Spelling == Other.Spelling && NameParts == Other.NameParts
			&& AnchorEquals(Source, Other.Source)
			&& bGlobal == Other.bGlobal && bConst == Other.bConst && bHandle == Other.bHandle
			&& Reference == Other.Reference
			&& [&]()
			{
				if (ComponentRanges.Num() != Other.ComponentRanges.Num()) return false;
				for (int32 Index = 0; Index < ComponentRanges.Num(); ++Index)
					if (!AnchorEquals(ComponentRanges[Index], Other.ComponentRanges[Index])) return false;
				return true;
			}()
			&& [&]()
			{
				if (GenericArguments.Num() != Other.GenericArguments.Num()) return false;
				for (int32 Index = 0; Index < GenericArguments.Num(); ++Index)
					if (!GenericArguments[Index].Equals(Other.GenericArguments[Index])) return false;
				return true;
			}()
			&& [&]()
			{
				if (ArrayDimensions.Num() != Other.ArrayDimensions.Num()) return false;
				for (int32 Index = 0; Index < ArrayDimensions.Num(); ++Index)
					if (!AnchorEquals(ArrayDimensions[Index], Other.ArrayDimensions[Index])) return false;
				return true;
			}();
	}

	bool asSASTProjectedAnnotationArgument::Equals(const asSASTProjectedAnnotationArgument& Other) const
	{
		return ParentIndex == Other.ParentIndex && Name == Other.Name && Kind == Other.Kind
			&& Value == Other.Value && AnchorEquals(Source, Other.Source)
			&& AnchorEquals(ValueSource, Other.ValueSource);
	}

	bool asSASTProjectedAccessPermissionModifier::Equals(
		const asSASTProjectedAccessPermissionModifier& Other) const
	{
		return Kind == Other.Kind && AnchorEquals(Source, Other.Source);
	}

	bool asSASTProjectedAccessPermission::Equals(
		const asSASTProjectedAccessPermission& Other) const
	{
		return Subject == Other.Subject && Name == Other.Name
			&& AnchorEquals(SubjectSource, Other.SubjectSource)
			&& AnchorEquals(Source, Other.Source)
			&& ArrayEquals<asSASTProjectedAccessPermissionModifier>(Modifiers, Other.Modifiers);
	}

	bool asSASTProjectedAuthoredAccessUse::Equals(
		const asSASTProjectedAuthoredAccessUse& Other) const
	{
		return bPresent == Other.bPresent && Name == Other.Name
			&& AnchorEquals(KeywordSource, Other.KeywordSource)
			&& AnchorEquals(ColonSource, Other.ColonSource)
			&& AnchorEquals(NameSource, Other.NameSource);
	}

	bool asSASTProjectedExpressionSemantics::Equals(const asSASTProjectedExpressionSemantics& Other) const
	{
		return bPresent == Other.bPresent && TypeIdentity == Other.TypeIdentity
			&& ValueCategory == Other.ValueCategory && bReadOnly == Other.bReadOnly;
	}

	bool asSASTProjectedDeclarationSemantics::Equals(const asSASTProjectedDeclarationSemantics& Other) const
	{
		return bPresent == Other.bPresent && Access == Other.Access
			&& AuthoredAccess.Equals(Other.AuthoredAccess);
	}

	bool asSASTProjectedSemanticReference::Equals(const asSASTProjectedSemanticReference& Other) const
	{
		return Kind == Other.Kind && Ordinal == Other.Ordinal && TypeIdentity == Other.TypeIdentity
			&& PassingMode == Other.PassingMode;
	}

	bool asSASTProjectedCrossReference::Equals(const asSASTProjectedCrossReference& Other) const
	{
		return Kind == Other.Kind && LocalIndex == Other.LocalIndex && StableIdentity == Other.StableIdentity;
	}

	bool asSASTProjectionRecord::Equals(const asSASTProjectionRecord& Other) const
	{
		return Family == Other.Family && Kind == Other.Kind && AnchorEquals(Source, Other.Source)
			&& Children == Other.Children
			&& ArrayEquals<asSASTProjectedCrossReference>(CrossReferences, Other.CrossReferences)
			&& ArrayEquals<asSASTProjectedSemanticReference>(SemanticReferences, Other.SemanticReferences)
			&& Declaration.Equals(Other.Declaration) && Expression.Equals(Other.Expression)
			&& PayloadEquals(Payload, Other.Payload)
			&& StableIdentity == Other.StableIdentity;
	}

	bool asCASTProjection::Equals(const asCASTProjection& Other) const
	{
		return ArrayEquals<asSASTProjectionRecord>(Records, Other.Records);
	}

	bool asCASTProjection::ValidateIdentities(
		const asCStableIdentityRegistry& Registry,
		FString& OutError) const
	{
		OutError.Reset();
		auto FailIdentity = [&OutError](const TCHAR* Message)
		{
			OutError = Message;
			return false;
		};
		auto HasRole = [&Registry](const asSStableKey& Key, asEStableIdentityRole Expected)
		{
			asEStableIdentityRole Actual = asEStableIdentityRole::Invalid;
			return !Key.IsZero()
				&& Registry.ResolveIdentityRole(Key, Actual) == asETypeIdentityStatus::Succeeded
				&& Actual == Expected;
		};
		auto ParametersEqual = [](TConstArrayView<asSStableParameter> Left, TConstArrayView<asSStableParameter> Right)
		{
			if (Left.Num() != Right.Num()) return false;
			for (int32 Index = 0; Index < Left.Num(); ++Index)
				if (Left[Index].Type != Right[Index].Type || Left[Index].PassingMode != Right[Index].PassingMode) return false;
			return true;
		};
		auto SignatureMatches = [&Registry, &ParametersEqual](
			const asSStableKey& SignatureKey,
			const asSStableKey& ReturnType,
			TConstArrayView<asSStableParameter> Parameters)
		{
			asSStableTypeUseDescriptor Signature;
			return Registry.ResolveTypeUse(SignatureKey, Signature) == asETypeIdentityStatus::Succeeded
				&& Signature.Kind == asETypeUseKind::Callable
				&& Signature.Qualifiers == asETypeUseQualifiers::None
				&& Signature.ReturnType == ReturnType
				&& ParametersEqual(Signature.Parameters, Parameters);
		};
		auto TypeUseDescriptorsEqual = [&ParametersEqual](
			const asSStableTypeUseDescriptor& Left,
			const asSStableTypeUseDescriptor& Right)
		{
			return Left.Kind == Right.Kind && Left.Primitive == Right.Primitive
				&& Left.Declaration == Right.Declaration && Left.Arguments == Right.Arguments
				&& Left.ReturnType == Right.ReturnType
				&& ParametersEqual(Left.Parameters, Right.Parameters)
				&& Left.Qualifiers == Right.Qualifiers;
		};
		auto IsNumericTypeUse = [&Registry](const asSStableTypeUseDescriptor& Type)
		{
			if (Type.Kind == asETypeUseKind::Primitive)
				return Type.Primitive > asEPrimitiveType::Bool
					&& Type.Primitive <= asEPrimitiveType::Float64;
			if (Type.Kind != asETypeUseKind::Nominal || Type.Declaration.IsZero()) return false;
			asSNominalTypeDescriptor Nominal;
			return Registry.ResolveNominalType(Type.Declaration, Nominal) == asETypeIdentityStatus::Succeeded
				&& Nominal.Kind == asETypeDeclKind::Enum;
		};
		auto ValueTypeUsesEqual = [&Registry, &TypeUseDescriptorsEqual](
			const asSStableKey& LeftKey,
			const asSStableKey& RightKey)
		{
			asSStableTypeUseDescriptor Left;
			asSStableTypeUseDescriptor Right;
			if (Registry.ResolveTypeUse(LeftKey, Left) != asETypeIdentityStatus::Succeeded
				|| Registry.ResolveTypeUse(RightKey, Right) != asETypeIdentityStatus::Succeeded)
				return false;
			const uint16 ReferenceBit = static_cast<uint16>(asETypeUseQualifiers::Reference);
			Left.Qualifiers = static_cast<asETypeUseQualifiers>(
				static_cast<uint16>(Left.Qualifiers) & ~ReferenceBit);
			Right.Qualifiers = static_cast<asETypeUseQualifiers>(
				static_cast<uint16>(Right.Qualifiers) & ~ReferenceBit);
			return TypeUseDescriptorsEqual(Left, Right);
		};
		auto TypeUseCanInitialize = [&Registry, &ValueTypeUsesEqual, &IsNumericTypeUse](
			const asSStableKey& TargetKey,
			const asSStableKey& SourceKey)
		{
			asSStableTypeUseDescriptor Target;
			asSStableTypeUseDescriptor Source;
			if (Registry.ResolveTypeUse(TargetKey, Target) != asETypeIdentityStatus::Succeeded
				|| Registry.ResolveTypeUse(SourceKey, Source) != asETypeIdentityStatus::Succeeded)
				return false;
			const uint16 TargetQualifiers = static_cast<uint16>(Target.Qualifiers);
			const uint16 SourceQualifiers = static_cast<uint16>(Source.Qualifiers);
			const uint16 ConstBit = static_cast<uint16>(asETypeUseQualifiers::Const);
			const uint16 ReferenceBit = static_cast<uint16>(asETypeUseQualifiers::Reference);
			const uint16 HandleBit = static_cast<uint16>(asETypeUseQualifiers::Handle);
			if ((TargetQualifiers & HandleBit) != (SourceQualifiers & HandleBit)) return false;
			if ((TargetQualifiers & ReferenceBit) != 0 && (TargetQualifiers & ConstBit) == 0
				&& ((SourceQualifiers & ReferenceBit) == 0 || (SourceQualifiers & ConstBit) != 0))
				return false;
			if (ValueTypeUsesEqual(TargetKey, SourceKey)) return true;
			return (TargetQualifiers & (ReferenceBit | HandleBit)) == 0
				&& IsNumericTypeUse(Target) && IsNumericTypeUse(Source);
		};
		auto GetChild = [this](const asSASTProjectionRecord& Record, int32 Ordinal)
			-> const asSASTProjectionRecord*
		{
			if (!Record.Children.IsValidIndex(Ordinal)) return nullptr;
			const uint32 Index = Record.Children[Ordinal];
			return Index < static_cast<uint32>(Records.Num()) ? &Records[static_cast<int32>(Index)] : nullptr;
		};
		auto IsExpressionRecord = [](const asSASTProjectionRecord* Record)
		{
			return Record && Record->Family == asEASTNodeFamily::Stmt
				&& ((Record->Kind >= static_cast<uint16>(asEStmtKind::IntegerLiteral)
					&& Record->Kind <= static_cast<uint16>(asEStmtKind::ValueInitExpr))
					|| Record->Kind == static_cast<uint16>(asEStmtKind::UserConversionExpr));
		};
		auto IsStatementRecord = [](const asSASTProjectionRecord* Record)
		{
			return Record && Record->Family == asEASTNodeFamily::Stmt;
		};
		auto CountCrossReference = [](const asSASTProjectionRecord& Record, asEASTCrossReferenceKind Kind)
		{
			int32 Count = 0;
			for (const asSASTProjectedCrossReference& Reference : Record.CrossReferences)
				if (Reference.Kind == Kind) ++Count;
			return Count;
		};
		auto ProtocolOwnerIsReachable = [this, &Registry](
			const asSStableKey& ContainerDeclaration,
			const asSStableKey& ProtocolOwner)
		{
			if (ContainerDeclaration.IsZero() || ProtocolOwner.IsZero()) return false;
			TArray<asSStableKey> Pending;
			TArray<asSStableKey> Visited;
			Pending.Add(ContainerDeclaration);
			for (int32 PendingIndex = 0; PendingIndex < Pending.Num(); ++PendingIndex)
			{
				const asSStableKey Current = Pending[PendingIndex];
				if (Current == ProtocolOwner) return true;
				if (Visited.Contains(Current)) continue;
				Visited.Add(Current);
				const asSASTProjectionRecord* ProjectedRecord = nullptr;
				for (const asSASTProjectionRecord& Candidate : Records)
				{
					if (Candidate.Family == asEASTNodeFamily::Decl
						&& Candidate.Kind == static_cast<uint16>(asEDeclKind::Record)
						&& Candidate.StableIdentity == Current)
					{
						ProjectedRecord = &Candidate;
						break;
					}
				}
				if (!ProjectedRecord) continue;
				for (const asSASTProjectedSemanticReference& Reference : ProjectedRecord->SemanticReferences)
				{
					if (Reference.Kind != asEASTSemanticTypeReferenceKind::BaseType) continue;
					asSStableTypeUseDescriptor Base;
					if (Registry.ResolveTypeUse(Reference.TypeIdentity, Base) == asETypeIdentityStatus::Succeeded
						&& Base.Kind == asETypeUseKind::Nominal && !Base.Declaration.IsZero())
						Pending.Add(Base.Declaration);
				}
			}
			return false;
		};

		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			const asSASTProjectionRecord& Record = Records[RecordIndex];
			if (Record.Declaration.bPresent != (Record.Family == asEASTNodeFamily::Decl)
				|| (Record.Declaration.bPresent
					&& Record.Declaration.Access > asEAccessSpecifier::Private))
			{
				return FailIdentity(TEXT("projection declaration semantics are absent or invalid"));
			}
			asEStableIdentityRole Expected = asEStableIdentityRole::Invalid;
			bool bRequired = false;
			if (Record.Family == asEASTNodeFamily::Type)
			{
				Expected = asEStableIdentityRole::TypeUse; bRequired = true;
			}
			else if (Record.Family == asEASTNodeFamily::Decl)
			{
				switch (static_cast<asEDeclKind>(Record.Kind))
				{
				case asEDeclKind::Record: case asEDeclKind::Delegate: case asEDeclKind::Event:
				case asEDeclKind::TypeAlias:
					Expected = asEStableIdentityRole::NominalType; bRequired = true; break;
				case asEDeclKind::Function:
					Expected = asEStableIdentityRole::Function; bRequired = true; break;
				case asEDeclKind::Namespace: case asEDeclKind::Var: case asEDeclKind::ParmVar: case asEDeclKind::EnumConstant:
					Expected = asEStableIdentityRole::Declaration; break;
				default: break;
				}
			}
			if ((bRequired || !Record.StableIdentity.IsZero())
				&& (Expected == asEStableIdentityRole::Invalid || !HasRole(Record.StableIdentity, Expected)))
			{
				return FailIdentity(TEXT("projection node identity is missing, foreign, or has the wrong semantic role"));
			}
			if (Record.Expression.bPresent
				&& (Record.Expression.ValueCategory < asEValueCategory::LValue
					|| Record.Expression.ValueCategory > asEValueCategory::PRValue
					|| !HasRole(Record.Expression.TypeIdentity, asEStableIdentityRole::TypeUse)))
			{
				return FailIdentity(TEXT("projection expression type is foreign or has the wrong semantic role"));
			}
			for (const asSASTProjectedSemanticReference& Reference : Record.SemanticReferences)
			{
				if (Reference.Kind > asEASTSemanticTypeReferenceKind::ForeachIteratorType
					|| Reference.PassingMode > asEParameterPassingMode::InOut
					|| !HasRole(Reference.TypeIdentity, asEStableIdentityRole::TypeUse))
				{
					return FailIdentity(TEXT("projection semantic type reference is foreign or has the wrong role"));
				}
			}
			for (const asSASTProjectedCrossReference& Reference : Record.CrossReferences)
			{
				if (Reference.Kind > asEASTCrossReferenceKind::ConversionFunction)
					return FailIdentity(TEXT("projection cross-reference kind is invalid"));
				const asSASTProjectionRecord* LocalTarget = nullptr;
				if (Reference.LocalIndex != 0)
				{
					const uint32 TargetIndex = Reference.LocalIndex - 1;
					if (TargetIndex >= static_cast<uint32>(Records.Num()))
						return FailIdentity(TEXT("projection cross-reference has an invalid local target"));
					LocalTarget = &Records[static_cast<int32>(TargetIndex)];
					if (!Reference.StableIdentity.IsZero()
						&& Reference.StableIdentity != LocalTarget->StableIdentity)
						return FailIdentity(TEXT("projection cross-reference identity disagrees with its local target"));
				}
				else if (Reference.StableIdentity.IsZero())
					return FailIdentity(TEXT("external projection cross-reference has no durable identity"));

				if (LocalTarget)
				{
					if (Reference.Kind == asEASTCrossReferenceKind::Declaration
						&& LocalTarget->Family != asEASTNodeFamily::Decl)
						return FailIdentity(TEXT("declaration cross-reference does not target a declaration"));
					if (Reference.Kind == asEASTCrossReferenceKind::MemberDeclaration
						&& LocalTarget->Family != asEASTNodeFamily::Decl)
						return FailIdentity(TEXT("member cross-reference does not target a declaration"));
					if ((Reference.Kind == asEASTCrossReferenceKind::ReturnTarget
							|| Reference.Kind == asEASTCrossReferenceKind::LambdaEnclosingFunction)
						&& (LocalTarget->Family != asEASTNodeFamily::Decl
							|| LocalTarget->Kind != static_cast<uint16>(asEDeclKind::Function)))
						return FailIdentity(TEXT("return cross-reference does not target a function"));
					if ((Reference.Kind == asEASTCrossReferenceKind::OperatorFunction
							|| Reference.Kind == asEASTCrossReferenceKind::Constructor
							|| Reference.Kind == asEASTCrossReferenceKind::ForeachBeginFunction
							|| Reference.Kind == asEASTCrossReferenceKind::ForeachEndFunction
							|| Reference.Kind == asEASTCrossReferenceKind::ForeachNextFunction
							|| Reference.Kind == asEASTCrossReferenceKind::ForeachValueFunction
							|| Reference.Kind == asEASTCrossReferenceKind::ForeachKeyFunction)
						&& (LocalTarget->Family != asEASTNodeFamily::Decl
							|| LocalTarget->Kind != static_cast<uint16>(asEDeclKind::Function)))
						return FailIdentity(TEXT("callable cross-reference does not target a function"));
					if (Reference.Kind == asEASTCrossReferenceKind::DefaultParameter
						&& (LocalTarget->Family != asEASTNodeFamily::Decl
							|| LocalTarget->Kind != static_cast<uint16>(asEDeclKind::ParmVar)))
						return FailIdentity(TEXT("default-argument cross-reference does not target a parameter"));
					if (Reference.Kind == asEASTCrossReferenceKind::BreakTarget
						|| Reference.Kind == asEASTCrossReferenceKind::ContinueTarget)
					{
						const bool bLoop = LocalTarget->Family == asEASTNodeFamily::Stmt
							&& (LocalTarget->Kind == static_cast<uint16>(asEStmtKind::While)
								|| LocalTarget->Kind == static_cast<uint16>(asEStmtKind::For)
								|| LocalTarget->Kind == static_cast<uint16>(asEStmtKind::Do)
								|| LocalTarget->Kind == static_cast<uint16>(asEStmtKind::Foreach));
						const bool bSwitch = LocalTarget->Family == asEASTNodeFamily::Stmt
							&& LocalTarget->Kind == static_cast<uint16>(asEStmtKind::Switch);
						if (!bLoop && (Reference.Kind == asEASTCrossReferenceKind::ContinueTarget || !bSwitch))
							return FailIdentity(TEXT("loop cross-reference does not target a loop"));
					}
					if (Reference.Kind == asEASTCrossReferenceKind::SwitchParent
						&& (LocalTarget->Family != asEASTNodeFamily::Stmt
							|| LocalTarget->Kind != static_cast<uint16>(asEStmtKind::Switch)))
						return FailIdentity(TEXT("switch parent cross-reference does not target a switch"));
					if (Reference.Kind == asEASTCrossReferenceKind::FallthroughTarget
						&& (LocalTarget->Family != asEASTNodeFamily::Stmt
							|| (LocalTarget->Kind != static_cast<uint16>(asEStmtKind::Case)
								&& LocalTarget->Kind != static_cast<uint16>(asEStmtKind::Default))))
						return FailIdentity(TEXT("fallthrough cross-reference does not target a clause"));
					if ((Reference.Kind == asEASTCrossReferenceKind::ThisRecord
							|| Reference.Kind == asEASTCrossReferenceKind::LambdaCapturedThis)
						&& (LocalTarget->Family != asEASTNodeFamily::Decl
							|| LocalTarget->Kind != static_cast<uint16>(asEDeclKind::Record)))
						return FailIdentity(TEXT("record cross-reference does not target a record"));
					if (Reference.Kind == asEASTCrossReferenceKind::LambdaCapture
						&& (LocalTarget->Family != asEASTNodeFamily::Decl
							|| (LocalTarget->Kind != static_cast<uint16>(asEDeclKind::Var)
								&& LocalTarget->Kind != static_cast<uint16>(asEDeclKind::ParmVar))))
						return FailIdentity(TEXT("lambda capture cross-reference does not target a variable"));
					if (Reference.Kind == asEASTCrossReferenceKind::CustomAccessSpecifier
						&& (LocalTarget->Family != asEASTNodeFamily::Decl
							|| LocalTarget->Kind != static_cast<uint16>(asEDeclKind::AccessSpecifier)))
						return FailIdentity(TEXT("custom access cross-reference does not target an access specifier"));
					if (Reference.Kind == asEASTCrossReferenceKind::ConversionFunction
						&& (LocalTarget->Family != asEASTNodeFamily::Decl
							|| LocalTarget->Kind != static_cast<uint16>(asEDeclKind::Function)))
						return FailIdentity(TEXT("conversion cross-reference does not target a function"));
				}

				if (Reference.StableIdentity.IsZero()) continue;
				asEStableIdentityRole Actual = asEStableIdentityRole::Invalid;
				const bool bFunctionReference = Reference.Kind == asEASTCrossReferenceKind::ReturnTarget
					|| Reference.Kind == asEASTCrossReferenceKind::OperatorFunction
					|| Reference.Kind == asEASTCrossReferenceKind::Constructor
					|| Reference.Kind == asEASTCrossReferenceKind::LambdaEnclosingFunction
					|| Reference.Kind == asEASTCrossReferenceKind::ForeachBeginFunction
					|| Reference.Kind == asEASTCrossReferenceKind::ForeachEndFunction
					|| Reference.Kind == asEASTCrossReferenceKind::ForeachNextFunction
					|| Reference.Kind == asEASTCrossReferenceKind::ForeachValueFunction
					|| Reference.Kind == asEASTCrossReferenceKind::ForeachKeyFunction
					|| Reference.Kind == asEASTCrossReferenceKind::ConversionFunction;
				const bool bNamedDeclarationReference = Reference.Kind == asEASTCrossReferenceKind::Declaration
					|| Reference.Kind == asEASTCrossReferenceKind::MemberDeclaration
					|| Reference.Kind == asEASTCrossReferenceKind::LambdaCapture;
				const bool bNominalReference = Reference.Kind == asEASTCrossReferenceKind::ThisRecord
					|| Reference.Kind == asEASTCrossReferenceKind::LambdaCapturedThis;
				if (Registry.ResolveIdentityRole(Reference.StableIdentity, Actual) != asETypeIdentityStatus::Succeeded
					|| (bFunctionReference && Actual != asEStableIdentityRole::Function)
					|| (bNominalReference && Actual != asEStableIdentityRole::NominalType)
					|| (Reference.Kind == asEASTCrossReferenceKind::DefaultParameter
						&& Actual != asEStableIdentityRole::Declaration)
					|| (bNamedDeclarationReference
						&& Actual != asEStableIdentityRole::Declaration
						&& Actual != asEStableIdentityRole::Function
						&& Actual != asEStableIdentityRole::NominalType))
				{
					return FailIdentity(TEXT("projection cross-reference identity is foreign or has the wrong role"));
				}
				if (Reference.Kind == asEASTCrossReferenceKind::DefaultParameter)
				{
					asSDeclarationIdentityDescriptor Parameter;
					if (Registry.ResolveDeclaration(Reference.StableIdentity, Parameter)
							!= asETypeIdentityStatus::Succeeded
						|| Parameter.Kind != asEDeclarationIdentityKind::Parameter)
						return FailIdentity(TEXT("default-argument reference does not resolve to a parameter coordinate"));
				}
				if (Reference.Kind == asEASTCrossReferenceKind::Constructor)
				{
					asSFunctionIdentityDescriptor Constructor;
					if (Registry.ResolveFunction(Reference.StableIdentity, Constructor)
							!= asETypeIdentityStatus::Succeeded
						|| Constructor.Kind != asEFunctionIdentityKind::Constructor)
						return FailIdentity(TEXT("constructor reference does not resolve to a constructor identity"));
				}
			}

			if (Record.Family == asEASTNodeFamily::Stmt)
			{
				const asEStmtKind Kind = static_cast<asEStmtKind>(Record.Kind);
				const bool bExpressionKind =
					(Record.Kind >= static_cast<uint16>(asEStmtKind::IntegerLiteral)
						&& Record.Kind <= static_cast<uint16>(asEStmtKind::ValueInitExpr))
					|| Record.Kind == static_cast<uint16>(asEStmtKind::UserConversionExpr);
				if (Record.Expression.bPresent != bExpressionKind)
					return FailIdentity(TEXT("projected expression semantics do not match the statement kind"));
				switch (Kind)
				{
				case asEStmtKind::UserConversionExpr:
				{
					const auto* Payload = Record.TryGetPayload<asSASTUserConversionPayload>();
					const asSASTProjectionRecord* Operand = GetChild(Record, 0);
					asSFunctionIdentityDescriptor Function;
					const asSASTProjectedCrossReference* ConversionRef = nullptr;
					for (const asSASTProjectedCrossReference& Reference : Record.CrossReferences)
						if (Reference.Kind == asEASTCrossReferenceKind::ConversionFunction) ConversionRef = &Reference;
					if (!Payload || !ConversionRef || !Operand || !IsExpressionRecord(Operand)
						|| Record.Children.Num() != 1 || CountCrossReference(Record, asEASTCrossReferenceKind::ConversionFunction) != 1
						|| !Operand->Expression.bPresent || !Record.Expression.bPresent
						|| Registry.ResolveFunction(ConversionRef->StableIdentity, Function) != asETypeIdentityStatus::Succeeded
						|| Function.Kind != asEFunctionIdentityKind::Conversion
						|| Function.ConversionDestination != Record.Expression.TypeIdentity
						|| Function.ReturnType != Record.Expression.TypeIdentity
						|| !Function.Parameters.IsEmpty()
						|| (Payload->Mode == asEUserConversionMode::Implicit
							&& Function.Name != TEXT("opImplConv") && Function.Name != TEXT("opImplCast"))
						|| (Payload->Mode == asEUserConversionMode::Explicit
							&& Function.Name != TEXT("opConv") && Function.Name != TEXT("opCast")))
						return FailIdentity(TEXT("projected user conversion does not authenticate its selected function"));
					break;
				}
				case asEStmtKind::ImplicitCastExpr:
				{
					const auto* Payload = Record.TryGetPayload<asSASTImplicitCastPayload>();
					const asSASTProjectionRecord* Operand = GetChild(Record, 0);
					asSStableTypeUseDescriptor Source;
					asSStableTypeUseDescriptor Target;
					if (!Payload || Record.Children.Num() != 1 || !IsExpressionRecord(Operand)
						|| !Operand->Expression.bPresent || !Record.Expression.bPresent
						|| Registry.ResolveTypeUse(Operand->Expression.TypeIdentity, Source)
							!= asETypeIdentityStatus::Succeeded
						|| Registry.ResolveTypeUse(Record.Expression.TypeIdentity, Target)
							!= asETypeIdentityStatus::Succeeded)
						return FailIdentity(TEXT("projected implicit cast lacks typed source or target semantics"));
					const uint16 SourceQualifiers = static_cast<uint16>(Source.Qualifiers);
					const uint16 TargetQualifiers = static_cast<uint16>(Target.Qualifiers);
					const uint16 ConstBit = static_cast<uint16>(asETypeUseQualifiers::Const);
					const uint16 HandleBit = static_cast<uint16>(asETypeUseQualifiers::Handle);
					const uint16 ReferenceBit = static_cast<uint16>(asETypeUseQualifiers::Reference);
					auto SameUnqualifiedType = [&TypeUseDescriptorsEqual](
						asSStableTypeUseDescriptor Left,
						asSStableTypeUseDescriptor Right)
					{
						Left.Qualifiers = asETypeUseQualifiers::None;
						Right.Qualifiers = asETypeUseQualifiers::None;
						return TypeUseDescriptorsEqual(Left, Right);
					};
					bool bValidConversion = false;
					switch (Payload->Conversion)
					{
					case asEImplicitConversionKind::Numeric:
						bValidConversion = (SourceQualifiers & (HandleBit | ReferenceBit)) == 0
							&& (TargetQualifiers & (HandleBit | ReferenceBit)) == 0
							&& IsNumericTypeUse(Source) && IsNumericTypeUse(Target)
							&& Record.Expression.ValueCategory == asEValueCategory::PRValue;
						break;
					case asEImplicitConversionKind::LValueToRValue:
						bValidConversion = Operand->Expression.ValueCategory == asEValueCategory::LValue
							&& Record.Expression.ValueCategory == asEValueCategory::PRValue
							&& (SourceQualifiers & HandleBit) == (TargetQualifiers & HandleBit)
							&& SameUnqualifiedType(Source, Target);
						break;
					case asEImplicitConversionKind::NullToHandle:
						bValidConversion = Operand->Kind == static_cast<uint16>(asEStmtKind::NullPtrLiteral)
							&& (TargetQualifiers & HandleBit) != 0 && (TargetQualifiers & ReferenceBit) == 0
							&& Record.Expression.ValueCategory == asEValueCategory::PRValue
							&& TypeUseDescriptorsEqual(Source, Target);
						break;
					case asEImplicitConversionKind::ReferenceBinding:
						bValidConversion = Operand->Kind == static_cast<uint16>(asEStmtKind::ThisExpr)
							&& Operand->Expression.ValueCategory == asEValueCategory::PRValue
							&& Record.Expression.ValueCategory == asEValueCategory::LValue
							&& (SourceQualifiers & ~ConstBit) == HandleBit
							&& (TargetQualifiers & ~ConstBit) == ReferenceBit
							&& (SourceQualifiers & ConstBit) == (TargetQualifiers & ConstBit)
							&& SameUnqualifiedType(Source, Target);
						break;
					default: break;
					}
					if (!bValidConversion)
						return FailIdentity(TEXT("projected implicit cast does not authenticate its conversion semantics"));
					break;
				}
				case asEStmtKind::ThisExpr:
				{
					asSStableTypeUseDescriptor ThisType;
					const uint16 ConstBit = static_cast<uint16>(asETypeUseQualifiers::Const);
					const uint16 HandleBit = static_cast<uint16>(asETypeUseQualifiers::Handle);
					if (!Record.TryGetPayload<asSASTThisExprPayload>() || !Record.Children.IsEmpty()
						|| Record.CrossReferences.Num() != 1
						|| Record.CrossReferences[0].Kind != asEASTCrossReferenceKind::ThisRecord
						|| Registry.ResolveTypeUse(Record.Expression.TypeIdentity, ThisType)
							!= asETypeIdentityStatus::Succeeded
						|| ThisType.Kind != asETypeUseKind::Nominal
						|| ThisType.Declaration != Record.CrossReferences[0].StableIdentity
						|| (static_cast<uint16>(ThisType.Qualifiers) & ~(ConstBit | HandleBit)) != 0
						|| (static_cast<uint16>(ThisType.Qualifiers) & HandleBit) == 0
						|| Record.Expression.ValueCategory != asEValueCategory::PRValue
						|| Record.Expression.bReadOnly
							!= ((static_cast<uint16>(ThisType.Qualifiers) & ConstBit) != 0))
						return FailIdentity(TEXT("projected this expression has an invalid typed edge shape"));
					break;
				}
				case asEStmtKind::LambdaExpr:
				{
					const auto* Payload = Record.TryGetPayload<asSASTLambdaExprPayload>();
					const asSASTProjectionRecord* Function = GetChild(Record, 0);
					const auto* FunctionPayload = Function
						? Function->TryGetPayload<asSASTFunctionDeclPayload>() : nullptr;
					if (!Payload || Record.Children.Num() != 1 || !Function
						|| Function->Family != asEASTNodeFamily::Decl
						|| Function->Kind != static_cast<uint16>(asEDeclKind::Function)
						|| !FunctionPayload || FunctionPayload->FunctionKind != asEFunctionKind::Lambda
						|| Payload->CaptureCount != static_cast<uint32>(CountCrossReference(
							*Function, asEASTCrossReferenceKind::LambdaCapture))
						|| Payload->bCapturesThis != (CountCrossReference(
							*Function, asEASTCrossReferenceKind::LambdaCapturedThis) == 1))
						return FailIdentity(TEXT("projected lambda expression does not own its exact lambda function payload"));
					break;
				}
				case asEStmtKind::InitListExpr:
				{
					const auto* Payload = Record.TryGetPayload<asSASTInitListExprPayload>();
					if (!Payload || Payload->ElementPatternIndices.Num() != Record.Children.Num()
						|| !HasRole(Payload->InitializerIdentity, asEStableIdentityRole::Function))
						return FailIdentity(TEXT("projected initializer list has an invalid factory or pattern shape"));
					for (int32 Index = 0; Index < Record.Children.Num(); ++Index)
					{
						if (!IsExpressionRecord(GetChild(Record, Index))
							|| Payload->ElementPatternIndices[Index] < 0)
							return FailIdentity(TEXT("projected initializer list contains an invalid element binding"));
					}
					break;
				}
				case asEStmtKind::ValueInitExpr:
					if (!Record.TryGetPayload<asSASTValueInitExprPayload>() || !Record.Children.IsEmpty())
						return FailIdentity(TEXT("projected value initializer has an invalid payload"));
					break;
				case asEStmtKind::DeclGroupStmt:
				{
					const auto* Payload = Record.TryGetPayload<asSASTDeclGroupStmtPayload>();
					if (!Payload || Payload->DeclarationCount < 2
						|| Payload->DeclarationCount != static_cast<uint32>(Record.Children.Num()))
						return FailIdentity(TEXT("projected declaration group lost its authored declaration count"));
					for (int32 Index = 0; Index < Record.Children.Num(); ++Index)
					{
						const asSASTProjectionRecord* Child = GetChild(Record, Index);
						if (!Child || Child->Family != asEASTNodeFamily::Stmt
							|| (Child->Kind != static_cast<uint16>(asEStmtKind::DeclStmt)
								&& Child->Kind != static_cast<uint16>(asEStmtKind::RecoveryStmt)))
							return FailIdentity(TEXT("projected declaration group contains a non-declaration child"));
					}
					break;
				}
				case asEStmtKind::ClassDefaultStmt:
					if (!Record.TryGetPayload<asSASTClassDefaultStmtPayload>() || Record.Children.Num() != 1
						|| !IsStatementRecord(GetChild(Record, 0)))
						return FailIdentity(TEXT("projected class-default statement lost its authored statement"));
					break;
				case asEStmtKind::For:
				{
					const auto* Payload = Record.TryGetPayload<asSASTForStmtPayload>();
					if (!Payload || Record.Children.Num() != static_cast<int32>(Payload->InitializerCount
						+ Payload->IncrementCount + (Payload->bHasCondition ? 1u : 0u) + 1u))
						return FailIdentity(TEXT("projected for statement phase counts are inconsistent"));
					int32 ChildIndex = 0;
					for (uint32 Index = 0; Index < Payload->InitializerCount; ++Index, ++ChildIndex)
						if (!IsStatementRecord(GetChild(Record, ChildIndex)))
							return FailIdentity(TEXT("projected for initializer is not a statement"));
					if (Payload->bHasCondition && !IsExpressionRecord(GetChild(Record, ChildIndex++)))
						return FailIdentity(TEXT("projected for condition is not an expression"));
					for (uint32 Index = 0; Index < Payload->IncrementCount; ++Index, ++ChildIndex)
						if (!IsExpressionRecord(GetChild(Record, ChildIndex)))
							return FailIdentity(TEXT("projected for increment is not an expression"));
					if (!IsStatementRecord(GetChild(Record, ChildIndex)))
						return FailIdentity(TEXT("projected for body is not a statement"));
					break;
				}
				case asEStmtKind::Switch:
				{
					const auto* Payload = Record.TryGetPayload<asSASTSwitchStmtPayload>();
					if (!Payload || Payload->ClauseCount == 0
						|| Record.Children.Num() != static_cast<int32>(Payload->ClauseCount + 1u)
						|| !IsExpressionRecord(GetChild(Record, 0)))
						return FailIdentity(TEXT("projected switch lost its selector or clauses"));
					uint32 ActualDefault = MAX_uint32;
					TSet<uint64> CaseValues;
					for (uint32 ClauseIndex = 0; ClauseIndex < Payload->ClauseCount; ++ClauseIndex)
					{
						const asSASTProjectionRecord* Clause = GetChild(Record, static_cast<int32>(ClauseIndex + 1));
						if (!Clause || Clause->Family != asEASTNodeFamily::Stmt
							|| (Clause->Kind != static_cast<uint16>(asEStmtKind::Case)
								&& Clause->Kind != static_cast<uint16>(asEStmtKind::Default))
							|| Clause->CrossReferences.Num() != 1
							|| Clause->CrossReferences[0].Kind != asEASTCrossReferenceKind::SwitchParent
							|| Clause->CrossReferences[0].LocalIndex != static_cast<uint32>(RecordIndex + 1))
							return FailIdentity(TEXT("projected switch clause has an invalid parent edge"));
						if (Clause->Kind == static_cast<uint16>(asEStmtKind::Default))
						{
							if (ActualDefault != MAX_uint32)
								return FailIdentity(TEXT("projected switch contains duplicate default clauses"));
							ActualDefault = ClauseIndex;
						}
						else if (const auto* CasePayload = Clause->TryGetPayload<asSASTCaseStmtPayload>())
						{
							if (CaseValues.Contains(CasePayload->ResolvedValueBits))
								return FailIdentity(TEXT("projected switch contains duplicate normalized case values"));
							CaseValues.Add(CasePayload->ResolvedValueBits);
						}
					}
					if (ActualDefault != Payload->DefaultOrdinal)
						return FailIdentity(TEXT("projected switch default ordinal is invalid"));
					break;
				}
				case asEStmtKind::Case:
				{
					const auto* Payload = Record.TryGetPayload<asSASTCaseStmtPayload>();
					if (!Payload || Record.Children.Num() != static_cast<int32>(Payload->StatementCount + 1u)
						|| !IsExpressionRecord(GetChild(Record, 0)))
						return FailIdentity(TEXT("projected case clause lost its label or statement count"));
					for (int32 Index = 1; Index < Record.Children.Num(); ++Index)
						if (!IsStatementRecord(GetChild(Record, Index)))
							return FailIdentity(TEXT("projected case clause contains a non-statement child"));
					break;
				}
				case asEStmtKind::Default:
				{
					const auto* Payload = Record.TryGetPayload<asSASTDefaultStmtPayload>();
					if (!Payload || Record.Children.Num() != static_cast<int32>(Payload->StatementCount))
						return FailIdentity(TEXT("projected default clause lost its statement count"));
					for (int32 Index = 0; Index < Record.Children.Num(); ++Index)
						if (!IsStatementRecord(GetChild(Record, Index)))
							return FailIdentity(TEXT("projected default clause contains a non-statement child"));
					break;
				}
				case asEStmtKind::Fallthrough:
					if (!Record.TryGetPayload<asSASTFallthroughStmtPayload>() || !Record.Children.IsEmpty()
						|| Record.CrossReferences.Num() != 1
						|| Record.CrossReferences[0].Kind != asEASTCrossReferenceKind::FallthroughTarget)
						return FailIdentity(TEXT("projected fallthrough has an invalid target edge"));
					break;
				case asEStmtKind::Foreach:
				{
					const auto* Payload = Record.TryGetPayload<asSASTForeachStmtPayload>();
					const int32 ExpectedChildren = Payload && Payload->bHasKey ? 4 : 3;
					const int32 ExpectedCrossReferences = Payload && Payload->bHasKey ? 5 : 4;
					if (!Payload || Record.Children.Num() != ExpectedChildren
						|| Record.CrossReferences.Num() != ExpectedCrossReferences
						|| Record.SemanticReferences.Num() != 2
						|| Record.SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::ForeachContainerType
						|| Record.SemanticReferences[1].Kind != asEASTSemanticTypeReferenceKind::ForeachIteratorType)
						return FailIdentity(TEXT("projected foreach has an invalid authored or semantic edge count"));
					const asEASTCrossReferenceKind ExpectedKinds[] = {
						asEASTCrossReferenceKind::ForeachBeginFunction,
						asEASTCrossReferenceKind::ForeachEndFunction,
						asEASTCrossReferenceKind::ForeachNextFunction,
						asEASTCrossReferenceKind::ForeachValueFunction,
						asEASTCrossReferenceKind::ForeachKeyFunction};
					const TCHAR* ExpectedNames[] = {TEXT("opForBegin"), TEXT("opForEnd"), TEXT("opForNext"), TEXT("opForValue"), TEXT("opForKey")};
					asSFunctionIdentityDescriptor ProtocolFunctions[5];
					for (int32 Index = 0; Index < ExpectedCrossReferences; ++Index)
					{
						const asSASTProjectedCrossReference& Reference = Record.CrossReferences[Index];
						if (Reference.Kind != ExpectedKinds[Index]
							|| Registry.ResolveFunction(Reference.StableIdentity, ProtocolFunctions[Index]) != asETypeIdentityStatus::Succeeded
							|| ProtocolFunctions[Index].Kind != asEFunctionIdentityKind::Method
							|| ProtocolFunctions[Index].Name != ExpectedNames[Index])
							return FailIdentity(TEXT("projected foreach protocol function is reordered or semantically wrong"));
					}
					const asSASTProjectionRecord* ValueStatement = GetChild(Record, 0);
					const asSASTProjectionRecord* KeyStatement = Payload->bHasKey ? GetChild(Record, 1) : nullptr;
					if (!IsStatementRecord(ValueStatement)
						|| (Payload->bHasKey && !IsStatementRecord(GetChild(Record, 1)))
						|| !IsExpressionRecord(GetChild(Record, Payload->bHasKey ? 2 : 1))
						|| !IsStatementRecord(GetChild(Record, Payload->bHasKey ? 3 : 2)))
						return FailIdentity(TEXT("projected foreach authored children are out of phase order"));
					auto VariableType = [&GetChild](const asSASTProjectionRecord* Statement)
					{
						asSStableKey Result;
						if (!Statement || Statement->Family != asEASTNodeFamily::Stmt
							|| Statement->Kind != static_cast<uint16>(asEStmtKind::DeclStmt)) return Result;
						const asSASTProjectionRecord* Variable = GetChild(*Statement, 0);
						if (!Variable || Variable->Family != asEASTNodeFamily::Decl
							|| Variable->Kind != static_cast<uint16>(asEDeclKind::Var)
							|| Variable->SemanticReferences.Num() != 1
							|| Variable->SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::VariableType)
							return Result;
						return Variable->SemanticReferences[0].TypeIdentity;
					};
					const asSStableKey ValueVariableType = VariableType(ValueStatement);
					const asSStableKey KeyVariableType = VariableType(KeyStatement);
					const asSStableKey ContainerType = Record.SemanticReferences[0].TypeIdentity;
					const asSStableKey IteratorType = Record.SemanticReferences[1].TypeIdentity;
					asSStableTypeUseDescriptor ContainerDescriptor;
					asSStableTypeUseDescriptor EndReturn;
					const bool bContainerResolved = Registry.ResolveTypeUse(ContainerType, ContainerDescriptor)
						== asETypeIdentityStatus::Succeeded;
					const uint16 ContainerQualifiers = bContainerResolved
						? static_cast<uint16>(ContainerDescriptor.Qualifiers) : 0;
					auto ProtocolParameterMatches = [&ValueTypeUsesEqual, &IteratorType](
						const asSFunctionIdentityDescriptor& Function)
					{
						return Function.Parameters.Num() == 1
							&& (Function.Parameters[0].PassingMode == asEParameterPassingMode::Value
								|| Function.Parameters[0].PassingMode == asEParameterPassingMode::In)
							&& ValueTypeUsesEqual(Function.Parameters[0].Type, IteratorType);
					};
					const bool bConstContainer = (ContainerQualifiers
						& static_cast<uint16>(asETypeUseQualifiers::Const)) != 0;
					const bool bOwnersMatch = bContainerResolved
						&& ContainerDescriptor.Kind == asETypeUseKind::Nominal
						&& !ContainerDescriptor.Declaration.IsZero();
					for (int32 Index = 0; Index < ExpectedCrossReferences && bOwnersMatch; ++Index)
					{
						if (!ProtocolOwnerIsReachable(
							ContainerDescriptor.Declaration, ProtocolFunctions[Index].Owner)
							|| (bConstContainer && (static_cast<uint16>(ProtocolFunctions[Index].Modifiers)
								& static_cast<uint16>(asEFunctionIdentityModifiers::ConstMethod)) == 0))
							return FailIdentity(TEXT("projected foreach protocol owner or receiver qualifiers are invalid"));
					}
					if (!bOwnersMatch || ValueVariableType.IsZero()
						|| ProtocolFunctions[0].Parameters.Num() != 0
						|| !ValueTypeUsesEqual(ProtocolFunctions[0].ReturnType, IteratorType)
						|| !ProtocolParameterMatches(ProtocolFunctions[1])
						|| !ProtocolParameterMatches(ProtocolFunctions[2])
						|| !ProtocolParameterMatches(ProtocolFunctions[3])
						|| (Payload->bHasKey && !ProtocolParameterMatches(ProtocolFunctions[4]))
						|| Registry.ResolveTypeUse(ProtocolFunctions[1].ReturnType, EndReturn) != asETypeIdentityStatus::Succeeded
						|| EndReturn.Kind != asETypeUseKind::Primitive || EndReturn.Primitive != asEPrimitiveType::Bool
						|| EndReturn.Qualifiers != asETypeUseQualifiers::None
						|| !TypeUseCanInitialize(ValueVariableType, ProtocolFunctions[3].ReturnType)
						|| (Payload->bHasKey && (KeyVariableType.IsZero()
							|| !TypeUseCanInitialize(KeyVariableType, ProtocolFunctions[4].ReturnType))))
						return FailIdentity(TEXT("projected foreach protocol signatures do not match its semantic variables"));
					break;
				}
				default: break;
				}
			}

			if (Record.Family == asEASTNodeFamily::Type)
			{
				asSStableTypeUseDescriptor Descriptor;
				if (Registry.ResolveTypeUse(Record.StableIdentity, Descriptor) != asETypeIdentityStatus::Succeeded
					|| Descriptor.Qualifiers != asETypeUseQualifiers::None
					|| !Record.Children.IsEmpty() || !Record.CrossReferences.IsEmpty() || Record.Expression.bPresent)
					return FailIdentity(TEXT("projected canonical type has invalid ownership or registry description"));
				switch (static_cast<asETypeKind>(Record.Kind))
				{
				case asETypeKind::Builtin:
				{
					const auto* Payload = Record.TryGetPayload<asSASTBuiltinTypePayload>();
					if (!Payload || Descriptor.Kind != asETypeUseKind::Primitive
						|| Descriptor.Primitive != Payload->Primitive || !Record.SemanticReferences.IsEmpty())
						return FailIdentity(TEXT("projected builtin payload disagrees with its registry description"));
					break;
				}
				case asETypeKind::Nominal:
				{
					const auto* Payload = Record.TryGetPayload<asSASTNominalTypePayload>();
					if (!Payload || Descriptor.Kind != asETypeUseKind::Nominal
						|| Descriptor.Declaration != Payload->DeclarationIdentity
						|| !Descriptor.Arguments.IsEmpty() || !Record.SemanticReferences.IsEmpty()
						|| !HasRole(Payload->DeclarationIdentity, asEStableIdentityRole::NominalType))
						return FailIdentity(TEXT("projected nominal payload disagrees with its registry description"));
					break;
				}
				case asETypeKind::Generic:
				{
					if (!Record.TryGetPayload<asSASTEmptyPayload>()
						|| Descriptor.Kind != asETypeUseKind::Nominal
						|| Record.SemanticReferences.Num() != Descriptor.Arguments.Num() + 1)
						return FailIdentity(TEXT("projected generic type has an invalid semantic edge shape"));
					const asSASTProjectedSemanticReference& Template = Record.SemanticReferences[0];
					asSStableTypeUseDescriptor TemplateDescriptor;
					if (Template.Kind != asEASTSemanticTypeReferenceKind::GenericTemplate
						|| Registry.ResolveTypeUse(Template.TypeIdentity, TemplateDescriptor) != asETypeIdentityStatus::Succeeded
						|| TemplateDescriptor.Kind != asETypeUseKind::Nominal
						|| TemplateDescriptor.Declaration != Descriptor.Declaration
						|| !TemplateDescriptor.Arguments.IsEmpty())
						return FailIdentity(TEXT("projected generic template disagrees with its registry description"));
					for (int32 Index = 0; Index < Descriptor.Arguments.Num(); ++Index)
					{
						const auto& Argument = Record.SemanticReferences[Index + 1];
						if (Argument.Kind != asEASTSemanticTypeReferenceKind::GenericArgument
							|| Argument.Ordinal != static_cast<uint32>(Index)
							|| Argument.TypeIdentity != Descriptor.Arguments[Index])
							return FailIdentity(TEXT("projected generic argument disagrees with its registry description"));
					}
					break;
				}
				case asETypeKind::Function:
				{
					const auto* Payload = Record.TryGetPayload<asSASTFunctionTypePayload>();
					if (!Payload || Descriptor.Kind != asETypeUseKind::Callable
						|| Payload->PassingModes.Num() != Descriptor.Parameters.Num()
						|| Record.SemanticReferences.Num() != Descriptor.Parameters.Num() + 1)
						return FailIdentity(TEXT("projected callable type has an invalid semantic edge shape"));
					if (Record.SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::FunctionReturn
						|| Record.SemanticReferences[0].TypeIdentity != Descriptor.ReturnType)
						return FailIdentity(TEXT("projected callable return type disagrees with its registry description"));
					for (int32 Index = 0; Index < Descriptor.Parameters.Num(); ++Index)
					{
						const auto& Parameter = Record.SemanticReferences[Index + 1];
						if (Parameter.Kind != asEASTSemanticTypeReferenceKind::FunctionParameter
							|| Parameter.Ordinal != static_cast<uint32>(Index)
							|| Parameter.TypeIdentity != Descriptor.Parameters[Index].Type
							|| Parameter.PassingMode != Descriptor.Parameters[Index].PassingMode
							|| Payload->PassingModes[Index] != Descriptor.Parameters[Index].PassingMode)
							return FailIdentity(TEXT("projected callable parameter disagrees with its registry description"));
					}
					break;
				}
				default:
					return FailIdentity(TEXT("projected type kind is not publishable"));
				}
			}
			else if (Record.Family == asEASTNodeFamily::Decl)
			{
				switch (static_cast<asEDeclKind>(Record.Kind))
				{
				case asEDeclKind::Record:
				case asEDeclKind::Delegate:
				case asEDeclKind::Event:
				{
					asSNominalTypeDescriptor Descriptor;
					if (Registry.ResolveNominalType(Record.StableIdentity, Descriptor) != asETypeIdentityStatus::Succeeded)
						return FailIdentity(TEXT("projected nominal declaration cannot be resolved"));
					if (Record.Kind == static_cast<uint16>(asEDeclKind::Record))
					{
						const auto* Payload = Record.TryGetPayload<asSASTRecordDeclPayload>();
						constexpr uint8 ValidModifiers = static_cast<uint8>(asERecordModifiers::Final)
							| static_cast<uint8>(asERecordModifiers::Abstract)
							| static_cast<uint8>(asERecordModifiers::Mixin)
							| static_cast<uint8>(asERecordModifiers::Local);
						const bool bRecordKind = Payload
							&& (Payload->DeclaredTypeKind == asETypeDeclKind::Class
								|| Payload->DeclaredTypeKind == asETypeDeclKind::Struct
								|| Payload->DeclaredTypeKind == asETypeDeclKind::Interface
								|| Payload->DeclaredTypeKind == asETypeDeclKind::Enum);
						if (!Payload || !bRecordKind || Payload->Name != Descriptor.Name
							|| Payload->DeclaredTypeKind != Descriptor.Kind
							|| (static_cast<uint8>(Payload->Modifiers) & ~ValidModifiers) != 0)
							return FailIdentity(TEXT("projected record declaration disagrees with its registry description"));
					}
					else
					{
						const auto* Payload = Record.TryGetPayload<asSASTCallableTypeDeclPayload>();
						const asETypeDeclKind Kind = Record.Kind == static_cast<uint16>(asEDeclKind::Event)
							? asETypeDeclKind::Event : asETypeDeclKind::Delegate;
						asSStableTypeUseDescriptor Signature;
						if (!Payload || Payload->Name != Descriptor.Name || Descriptor.Kind != Kind
							|| Payload->bMulticast != (Kind == asETypeDeclKind::Event))
							return FailIdentity(TEXT("projected callable declaration disagrees with its nominal identity"));
						if (Record.SemanticReferences.Num() != 1
							|| Record.SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::SignatureType
							|| Registry.ResolveTypeUse(Record.SemanticReferences[0].TypeIdentity, Signature)
								!= asETypeIdentityStatus::Succeeded
							|| Signature.Kind != asETypeUseKind::Callable)
							return FailIdentity(TEXT("projected callable declaration has an invalid signature reference"));
					}
					break;
				}
			case asEDeclKind::Function:
				{
					const auto* Payload = Record.TryGetPayload<asSASTFunctionDeclPayload>();
					asSFunctionIdentityDescriptor Descriptor;
					asSStableTypeUseDescriptor ClassDefaultReturn;
					constexpr uint16 ValidModifiers = static_cast<uint16>(asEFunctionModifiers::Final)
						| static_cast<uint16>(asEFunctionModifiers::Override)
						| static_cast<uint16>(asEFunctionModifiers::Property)
						| static_cast<uint16>(asEFunctionModifiers::Explicit)
						| static_cast<uint16>(asEFunctionModifiers::Local)
						| static_cast<uint16>(asEFunctionModifiers::Mixin)
						| static_cast<uint16>(asEFunctionModifiers::AcceptTemporary)
						| static_cast<uint16>(asEFunctionModifiers::ExternalImplicitThis)
						| static_cast<uint16>(asEFunctionModifiers::NoDiscard)
						| static_cast<uint16>(asEFunctionModifiers::AllowDiscard)
						| static_cast<uint16>(asEFunctionModifiers::Generated)
						| static_cast<uint16>(asEFunctionModifiers::Deprecated)
						| static_cast<uint16>(asEFunctionModifiers::Defaults)
						| static_cast<uint16>(asEFunctionModifiers::UnsafeDuringConstruction);
					if (!Payload || Registry.ResolveFunction(Record.StableIdentity, Descriptor) != asETypeIdentityStatus::Succeeded)
						return FailIdentity(TEXT("projected function declaration cannot be resolved"));
					const bool bImplicitFunction = Payload->FunctionKind == asEFunctionKind::Lambda
						|| Payload->FunctionKind == asEFunctionKind::ClassDefault;
					if (Payload->Name != Descriptor.Name
						|| Payload->Name.IsEmpty() != bImplicitFunction
						|| Payload->ReturnTypeSyntax.bPresent == bImplicitFunction
						|| Payload->FunctionKind > asEFunctionKind::Conversion
						|| (static_cast<uint16>(Payload->Modifiers) & ~ValidModifiers) != 0
						|| Payload->bConstMethod != ((static_cast<uint16>(Descriptor.Modifiers)
							& static_cast<uint16>(asEFunctionIdentityModifiers::ConstMethod)) != 0)
						|| Descriptor.DefinitionModifiers != asGetFunctionDefinitionModifiers(Payload->Modifiers)
						|| (Payload->FunctionKind == asEFunctionKind::Constructor
							&& Descriptor.Kind != asEFunctionIdentityKind::Constructor)
						|| (Payload->FunctionKind == asEFunctionKind::Destructor
							&& Descriptor.Kind != asEFunctionIdentityKind::Destructor)
						|| (Payload->FunctionKind == asEFunctionKind::Lambda
							&& Descriptor.Kind != asEFunctionIdentityKind::Lambda)
						|| (Payload->FunctionKind == asEFunctionKind::ClassDefault
							&& Descriptor.Kind != asEFunctionIdentityKind::ClassDefault)
						|| (Payload->FunctionKind == asEFunctionKind::Conversion
							&& Descriptor.Kind != asEFunctionIdentityKind::Conversion)
						|| (Payload->FunctionKind != asEFunctionKind::Constructor
							&& Payload->FunctionKind != asEFunctionKind::Destructor
							&& Payload->FunctionKind != asEFunctionKind::Lambda
							&& Payload->FunctionKind != asEFunctionKind::ClassDefault
							&& Payload->FunctionKind != asEFunctionKind::Conversion
							&& Descriptor.Kind != asEFunctionIdentityKind::Global
							&& Descriptor.Kind != asEFunctionIdentityKind::Method)
						|| Record.SemanticReferences.Num() != 1
						|| Record.SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::SignatureType
						|| !SignatureMatches(Record.SemanticReferences[0].TypeIdentity,
							Descriptor.ReturnType, Descriptor.Parameters))
						return FailIdentity(TEXT("projected function declaration disagrees with its registry description"));
					if (Payload->FunctionKind == asEFunctionKind::ClassDefault
						&& (!Descriptor.Parameters.IsEmpty()
							|| Registry.ResolveTypeUse(Descriptor.ReturnType, ClassDefaultReturn)
								!= asETypeIdentityStatus::Succeeded
							|| ClassDefaultReturn.Kind != asETypeUseKind::Primitive
							|| ClassDefaultReturn.Primitive != asEPrimitiveType::Void
							|| ClassDefaultReturn.Qualifiers != asETypeUseQualifiers::None))
						return FailIdentity(TEXT("projected class-default function is not an implicit void zero-parameter function"));
					if (Payload->FunctionKind == asEFunctionKind::Lambda)
					{
						const int32 EnclosingCount = CountCrossReference(
							Record, asEASTCrossReferenceKind::LambdaEnclosingFunction);
						if (Payload->LambdaOrdinal == MAX_uint32 || Payload->LambdaOrdinal != Descriptor.LambdaOrdinal
							|| EnclosingCount != 1 || Descriptor.EnclosingFunction.IsZero())
							return FailIdentity(TEXT("projected lambda function lacks its enclosing function coordinate"));
						for (const asSASTProjectedCrossReference& Reference : Record.CrossReferences)
							if (Reference.Kind == asEASTCrossReferenceKind::LambdaEnclosingFunction
								&& Reference.StableIdentity != Descriptor.EnclosingFunction)
								return FailIdentity(TEXT("projected lambda enclosing edge disagrees with its identity descriptor"));
					}
					else if (Payload->LambdaOrdinal != MAX_uint32
						|| CountCrossReference(Record, asEASTCrossReferenceKind::LambdaEnclosingFunction) != 0
						|| CountCrossReference(Record, asEASTCrossReferenceKind::LambdaCapture) != 0
						|| CountCrossReference(Record, asEASTCrossReferenceKind::LambdaCapturedThis) != 0)
						return FailIdentity(TEXT("projected non-lambda function carries lambda-only payload"));
					if (Payload->FunctionKind != asEFunctionKind::ClassDefault
						&& !Payload->DeferredStatementSources.IsEmpty())
						return FailIdentity(TEXT("projected non-class-default function carries default statements"));
					break;
				}
				case asEDeclKind::TypeAlias:
				{
					const auto* Payload = Record.TryGetPayload<asSASTTypeAliasDeclPayload>();
					asSNominalTypeDescriptor Descriptor;
					asSStableTypeUseDescriptor Target;
					if (!Payload
						|| Registry.ResolveNominalType(Record.StableIdentity, Descriptor)
							!= asETypeIdentityStatus::Succeeded
						|| Descriptor.Kind != asETypeDeclKind::Typedef
						|| Descriptor.Name != Payload->Name
						|| Record.SemanticReferences.Num() != 1
						|| Record.SemanticReferences[0].Kind != asEASTSemanticTypeReferenceKind::AliasedType
						|| Registry.ResolveTypeUse(Record.SemanticReferences[0].TypeIdentity, Target)
							!= asETypeIdentityStatus::Succeeded
						|| Target.Kind != asETypeUseKind::Primitive
						|| Target.Primitive <= asEPrimitiveType::Void
						|| Target.Primitive > asEPrimitiveType::Float64
						|| Target.Qualifiers != asETypeUseQualifiers::None
						|| Payload->TypeSyntax.bConst || Payload->TypeSyntax.bHandle
						|| Payload->TypeSyntax.Reference != asETypeReferenceDirection::None
						|| !Payload->TypeSyntax.GenericArguments.IsEmpty()
						|| !Payload->TypeSyntax.ArrayDimensions.IsEmpty())
					{
						return FailIdentity(TEXT("projected type alias disagrees with its nominal identity or target type"));
					}
					break;
				}
				case asEDeclKind::Namespace:
				case asEDeclKind::Var:
				case asEDeclKind::ParmVar:
				case asEDeclKind::EnumConstant:
				{
					if (Record.StableIdentity.IsZero()) break;
					asSDeclarationIdentityDescriptor Descriptor;
					if (Registry.ResolveDeclaration(Record.StableIdentity, Descriptor) != asETypeIdentityStatus::Succeeded)
						return FailIdentity(TEXT("projected declaration coordinate cannot be resolved"));
					if (Record.Kind == static_cast<uint16>(asEDeclKind::Namespace))
					{
						const auto* Payload = Record.TryGetPayload<asSASTNamedDeclPayload>();
						if (!Payload || Descriptor.Kind != asEDeclarationIdentityKind::Namespace || Descriptor.Name != Payload->Name)
							return FailIdentity(TEXT("projected namespace disagrees with its registry coordinate"));
					}
					else if (Record.Kind == static_cast<uint16>(asEDeclKind::EnumConstant))
					{
						const auto* Payload = Record.TryGetPayload<asSASTEnumConstantDeclPayload>();
						bool bHasInitializerChild = false;
						for (uint32 ChildIndex : Record.Children)
						{
							if (ChildIndex < static_cast<uint32>(Records.Num())
								&& IsExpressionRecord(&Records[static_cast<int32>(ChildIndex)]))
							{
								bHasInitializerChild = true;
								break;
							}
						}
						if (!Payload || Descriptor.Kind != asEDeclarationIdentityKind::EnumConstant
							|| Descriptor.Name != Payload->Name || !Payload->bResolvedValue
							|| (Payload->bHasDeferredInitializer && !bHasInitializerChild))
							return FailIdentity(TEXT("projected enum constant disagrees with its registry coordinate"));
					}
					else if (Record.Kind == static_cast<uint16>(asEDeclKind::ParmVar))
					{
						if (Descriptor.Kind != asEDeclarationIdentityKind::Parameter)
							return FailIdentity(TEXT("projected parameter has the wrong registry coordinate kind"));
					}
					else
					{
						const auto* Payload = Record.TryGetPayload<asSASTVarDeclPayload>();
						if (!Payload || (Descriptor.Kind != asEDeclarationIdentityKind::GlobalVariable
								&& Descriptor.Kind != asEDeclarationIdentityKind::Property)
							|| Descriptor.Name != Payload->Name)
							return FailIdentity(TEXT("projected variable disagrees with its registry coordinate"));
					}
					break;
				}
				default: break;
				}
			}
		}
		return true;
	}

	bool asCASTProjection::Build(
		const asCASTContext& Context,
		const asCASTNode* Root,
		asCASTProjection& Out,
		FString& OutError)
	{
		Out.Records.Reset(); OutError.Reset();
		if (!Context.IsSealed() || !Context.Owns(Root))
		{
			OutError = TEXT("projection requires a verified, sealed, owned root");
			return false;
		}
		asSASTVerificationResult Verification;
		if (!asCASTVerifier::Verify(Context, Root, Verification))
		{
			OutError = Verification.Diagnostic;
			return false;
		}

		TArray<const asCASTNode*> Ordered;
		const asEASTVisitStatus Visit = asCASTVisitor::Walk(
			Root, [&Ordered](const asCASTNode& Node) { Ordered.Add(&Node); });
		if (Visit != asEASTVisitStatus::Succeeded)
		{
			OutError = TEXT("owning traversal failed while building projection");
			return false;
		}
		TMap<const asCASTNode*, uint32> Indices;
		for (int32 Index = 0; Index < Ordered.Num(); ++Index)
			Indices.Add(Ordered[Index], static_cast<uint32>(Index));

		TArray<asSASTProjectionRecord> Candidate;
		Candidate.Reserve(Ordered.Num());
		asCStableIdentityRegistry& Registry = Context.GetTypeContext()->GetIdentityRegistry();
		for (const asCASTNode* Node : Ordered)
		{
			asSASTProjectionRecord Record;
			Record.Family = Node->GetFamily(); Record.Kind = Node->GetRawKind();
			if (Node->GetFamily() != asEASTNodeFamily::Type
				&& !MakeAnchor(*Context.GetSourceSnapshot(), Node->GetSourceRange(), Record.Source))
			{
				OutError = TEXT("node source range cannot be projected"); return false;
			}
			if (Node->GetFamily() == asEASTNodeFamily::Decl)
			{
				Record.Declaration.bPresent = true;
				const asCDecl& Declaration = static_cast<const asCDecl&>(*Node);
				Record.Declaration.Access = Declaration.GetAccess();
				const asSAuthoredAccessUse* AccessUse = nullptr;
				if (const auto* Variable = dyn_cast<asCVarDecl>(&Declaration))
					AccessUse = Variable->GetAuthoredAccessUse();
				else if (const auto* Function = dyn_cast<asCFunctionDecl>(&Declaration))
					AccessUse = Function->GetAuthoredAccessUse();
				if (!ProjectAuthoredAccessUse(*Context.GetSourceSnapshot(), AccessUse,
					Record.Declaration.AuthoredAccess))
				{
					OutError = TEXT("authored custom access source cannot be projected"); return false;
				}
			}

			TArray<const asCASTNode*> Children;
			if (!asCASTVisitor::CollectOwningChildren(*Node, Children))
			{
				OutError = TEXT("node kind has no owning projection rule"); return false;
			}
			for (const asCASTNode* Child : Children)
			{
				const uint32* Index = Indices.Find(Child);
				if (!Index) { OutError = TEXT("owning child is absent from projection order"); return false; }
				Record.Children.Add(*Index);
			}

			TArray<asSASTCrossReference> CrossReferences;
			asCASTVisitor::CollectCrossReferences(*Node, CrossReferences);
			for (const asSASTCrossReference& Reference : CrossReferences)
			{
				asSASTProjectedCrossReference Projected; Projected.Kind = Reference.Kind;
				if (const uint32* Index = Indices.Find(Reference.Target)) Projected.LocalIndex = *Index + 1;
				if (!Reference.Identity.IsZero()) Projected.StableIdentity = Reference.Identity;
				else if (const auto* Named = dyn_cast<asCNamedDecl>(Reference.Target)) Projected.StableIdentity = Named->GetStableIdentity();
				if (Projected.LocalIndex == 0 && Projected.StableIdentity.IsZero())
				{
					OutError = TEXT("external cross reference has no durable identity"); return false;
				}
				Record.CrossReferences.Add(MoveTemp(Projected));
			}

			TArray<asSASTSemanticTypeReference> SemanticReferences;
			asCASTVisitor::CollectSemanticTypeReferences(*Node, SemanticReferences);
			for (const asSASTSemanticTypeReference& Reference : SemanticReferences)
			{
				asSStableKey TypeIdentity;
				if (Reference.Type.GetStableIdentity(Registry, TypeIdentity) != asETypeIdentityStatus::Succeeded)
				{
					OutError = TEXT("semantic type reference cannot be projected"); return false;
				}
				if (Reference.Kind == asEASTSemanticTypeReferenceKind::ExpressionType)
				{
					const auto& Expression = static_cast<const asCExpr&>(*Node);
					Record.Expression.bPresent = true; Record.Expression.TypeIdentity = TypeIdentity;
					Record.Expression.ValueCategory = Expression.GetValueCategory(); Record.Expression.bReadOnly = Expression.IsReadOnly();
				}
				else
				{
					Record.SemanticReferences.Add({Reference.Kind, Reference.Ordinal, TypeIdentity, Reference.PassingMode});
				}
			}

			if (Node->GetFamily() == asEASTNodeFamily::Type)
				Record.StableIdentity = static_cast<const asCType&>(*Node).GetStableIdentity();
			else if (const auto* Named = dyn_cast<asCNamedDecl>(Node))
				Record.StableIdentity = Named->GetStableIdentity();
			if (!AssignPayload(*Context.GetSourceSnapshot(), *Node, Record.Payload))
			{
				OutError = TEXT("concrete node payload cannot be projected"); return false;
			}
			Candidate.Add(MoveTemp(Record));
		}

		Out.Records = MoveTemp(Candidate);
		if (!Out.ValidateIdentities(Registry, OutError))
		{
			Out.Records.Reset(); return false;
		}
		return true;
	}

	FString asCASTProjection::DumpText() const
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("typed-ast-v7 nodes=%d"), Records.Num()));
		for (int32 Index = 0; Index < Records.Num(); ++Index)
		{
			const asSASTProjectionRecord& Record = Records[Index];
			const FString Access = Record.Declaration.bPresent
				? FString::FromInt(static_cast<uint8>(Record.Declaration.Access)) : TEXT("none");
			Lines.Add(FString::Printf(TEXT("[%d] %s source=%s:%u-%u children=%d semantic=%d cross=%d access=%s identity=%s %s"),
				Index, NodeKindName(Record), *Record.Source.LogicalSourceKey,
				Record.Source.BeginByteOffset, Record.Source.EndByteOffset,
				Record.Children.Num(), Record.SemanticReferences.Num(), Record.CrossReferences.Num(),
				*Access,
				*Record.StableIdentity.ToHexString(), *PayloadSummary(Record.Payload)));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString asCASTProjection::DumpJson() const
	{
		FString Result = TEXT("{\"format\":\"typed-ast-v7\",\"nodes\":[");
		for (int32 Index = 0; Index < Records.Num(); ++Index)
		{
			if (Index) Result += TEXT(",");
			const asSASTProjectionRecord& Record = Records[Index];
			Result += FString::Printf(TEXT("{\"index\":%d,\"family\":%u,\"kind\":\"%s\",\"kindValue\":%u,\"source\":%s,\"identity\":\"%s\",\"payload\":%s,\"declaration\":"),
				Index, static_cast<uint8>(Record.Family), NodeKindName(Record), Record.Kind,
				*AnchorJson(Record.Source), *Record.StableIdentity.ToHexString(), *PayloadJson(Record.Payload));
			if (Record.Declaration.bPresent)
				Result += FString::Printf(TEXT("{\"access\":%u}"), static_cast<uint8>(Record.Declaration.Access));
			else Result += TEXT("null");
			Result += TEXT(",\"children\":[");
			for (int32 Child = 0; Child < Record.Children.Num(); ++Child)
			{
				if (Child) Result += TEXT(",");
				Result += FString::FromInt(static_cast<int32>(Record.Children[Child]));
			}
			Result += TEXT("],\"expression\":");
			if (Record.Expression.bPresent)
			{
				Result += FString::Printf(TEXT("{\"type\":\"%s\",\"valueCategory\":%u,\"readOnly\":%s}"),
					*Record.Expression.TypeIdentity.ToHexString(), static_cast<uint8>(Record.Expression.ValueCategory),
					Record.Expression.bReadOnly ? TEXT("true") : TEXT("false"));
			}
			else Result += TEXT("null");
			Result += TEXT(",\"semanticReferences\":[");
			for (int32 ReferenceIndex = 0; ReferenceIndex < Record.SemanticReferences.Num(); ++ReferenceIndex)
			{
				if (ReferenceIndex) Result += TEXT(",");
				const auto& Reference = Record.SemanticReferences[ReferenceIndex];
				Result += FString::Printf(TEXT("{\"kind\":%u,\"ordinal\":%u,\"type\":\"%s\",\"passingMode\":%u}"),
					static_cast<uint8>(Reference.Kind), Reference.Ordinal,
					*Reference.TypeIdentity.ToHexString(), static_cast<uint8>(Reference.PassingMode));
			}
			Result += TEXT("],\"crossReferences\":[");
			for (int32 ReferenceIndex = 0; ReferenceIndex < Record.CrossReferences.Num(); ++ReferenceIndex)
			{
				if (ReferenceIndex) Result += TEXT(",");
				const auto& Reference = Record.CrossReferences[ReferenceIndex];
				Result += FString::Printf(TEXT("{\"kind\":%u,\"localIndex\":%u,\"identity\":\"%s\"}"),
					static_cast<uint8>(Reference.Kind), Reference.LocalIndex, *Reference.StableIdentity.ToHexString());
			}
			Result += TEXT("]}");
		}
		Result += TEXT("]}");
		return Result;
	}

END_AS_NAMESPACE
