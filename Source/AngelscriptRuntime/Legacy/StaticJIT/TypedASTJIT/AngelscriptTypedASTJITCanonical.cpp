#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "source/as_ast_lifetime.h"
#include "source/as_ast_traversal.h"
#include "source/as_runtime_type_bridge.h"
#include "as_scriptfunction.h"
#include "frontend/Basic/as_source_manager.h"
#include "as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#if AS_CAN_GENERATE_JIT

namespace AngelscriptTypedASTJITCanonical_Private
{
	using EFallback = EAngelscriptTypedASTJITFallbackReason;

	const TCHAR* NativeCallSiteRouteName(
		const EAngelscriptJITNativeCallSiteRoute Route)
	{
		switch (Route)
		{
		case EAngelscriptJITNativeCallSiteRoute::DirectExported:
			return TEXT("DirectExported");
		case EAngelscriptJITNativeCallSiteRoute::DirectInline:
			return TEXT("DirectInline");
		case EAngelscriptJITNativeCallSiteRoute::RuntimeThunk:
			return TEXT("RuntimeThunk");
		case EAngelscriptJITNativeCallSiteRoute::Bridge:
			return TEXT("Bridge");
		case EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding:
			return TEXT("CurrentNativeBinding");
		default:
			return TEXT("Invalid");
		}
	}

	FString OneLineNativeCallComment(FString Value)
	{
		Value.ReplaceInline(TEXT("\r"), TEXT(" "));
		Value.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Value;
	}

	asSSourceSpan MakeSpan(
		const asCASTContext& Context,
		const asCSourceRange& Range)
	{
		asSSourceSpan Span;
		if (!Range.begin.IsValid())
		{
			return Span;
		}
		asSSourceLineColumn LineColumn;
		if (Context.GetSourceManager().GetLineColumn(Range.begin, LineColumn))
		{
			Span.row = static_cast<asUINT>(LineColumn.row);
			Span.column = static_cast<asUINT>(LineColumn.column);
		}
		if (const char* Key =
			Context.GetSourceManager().GetLogicalKey(Range.begin.fileID))
		{
			Span.section = Key;
		}
		return Span;
	}

	FAngelscriptTypedASTJITEligibility Reject(
		const EFallback Reason,
		const asSSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITEligibility Result;
		Result.Reason = Reason;
		Result.SourceSpan = Span;
		Result.Detail = Detail;
		return Result;
	}

	FAngelscriptTypedASTJITAnalysis AnalysisFail(
		const EFallback Reason,
		const asSSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITAnalysis Result;
		Result.FailureReason = Reason;
		Result.FailureSpan = Span;
		Result.FailureDetail = Detail;
		return Result;
	}

	FAngelscriptTypedASTJITEmission EmissionFail(
		const EFallback Reason,
		const asSSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITEmission Result;
		Result.FailureReason = Reason;
		Result.FailureSpan = Span;
		Result.FailureDetail = Detail;
		return Result;
	}

	FAngelscriptTypedASTJITDependencyAnalysis DependencyFail(
		const asSSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITDependencyAnalysis Result;
		Result.FailureReason = EFallback::SemanticDependencyMismatch;
		Result.FailureSpan = Span;
		Result.FailureDetail = Detail;
		return Result;
	}

	bool HasExactReference(
		const FAngelscriptTypedASTJITSemanticUseManifest& Manifest,
		const FAngelscriptArtifactReference& Reference)
	{
		return Manifest.RequiredReferences.Contains(Reference);
	}

	const FAngelscriptTypedASTJITGlobalDependency*
	FindExactCanonicalGlobalDependency(
		const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
			Dependencies,
		const asCDecl& Target)
	{
		if (Target.stableKey.GetLength() == 0)
		{
			return nullptr;
		}
		const FString StableKey = UTF8_TO_TCHAR(Target.stableKey.AddressOf());
		const FAngelscriptTypedASTJITGlobalDependency* Match = nullptr;
		for (const FAngelscriptTypedASTJITGlobalDependency& Candidate
			: Dependencies)
		{
			if (Candidate.CanonicalASTDeclKey != StableKey)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = &Candidate;
		}
		return Match;
	}

	bool AddCanonicalSemanticUse(
		FAngelscriptTypedASTJITDependencyAnalysis& Result,
		const EAngelscriptTypedASTJITSemanticUseKind UseKind,
		const EAngelscriptArtifactSemanticDependencyKind RequiredKind,
		const FAngelscriptArtifactReference& Reference,
		const asSSourceSpan& Span,
		const int32 Coordinate,
		const FStringView Target)
	{
		if (!Reference.IsValid()
			|| !HasExactReference(Result.Manifest, Reference))
		{
			Result = DependencyFail(Span, FString::Printf(TEXT(
				"Use=%s RequiredKind=%u StableMapping=Missing Target=%s"),
				LexToString(UseKind), static_cast<uint32>(RequiredKind),
				Reference.StableKey.IsZero()
					? TEXT("<unmapped>")
					: *Reference.StableKey.ToHexString()));
			return false;
		}

		if (RequiredKind
			!= EAngelscriptArtifactSemanticDependencyKind::Invalid)
		{
			int32 ExactCount = 0;
			for (const FAngelscriptArtifactSemanticDependency& Dependency
				: Result.Manifest.ArtifactDependencies)
			{
				ExactCount += Dependency.Target == Reference
					&& Dependency.Kind == RequiredKind ? 1 : 0;
			}
			if (ExactCount != 1)
			{
				Result = DependencyFail(Span, FString::Printf(TEXT(
					"Use=%s RequiredKind=%u ExactCount=%d Target=%s"),
					LexToString(UseKind), static_cast<uint32>(RequiredKind),
					ExactCount, *Reference.StableKey.ToHexString()));
				return false;
			}
		}

		const bool bExists = Result.Manifest.Uses.ContainsByPredicate(
			[&](const FAngelscriptTypedASTJITSemanticUse& Existing)
			{
				return Existing.Kind == UseKind
					&& Existing.RequiredDependencyKind == RequiredKind
					&& Existing.Reference == Reference
					&& Existing.EngineLocalCoordinate == Coordinate;
			});
		if (!bExists)
		{
			FAngelscriptTypedASTJITSemanticUse& Use =
				Result.Manifest.Uses.Emplace_GetRef();
			Use.Kind = UseKind;
			Use.RequiredDependencyKind = RequiredKind;
			Use.Reference = Reference;
			Use.SourceSpan = Span;
			Use.EngineLocalCoordinate = Coordinate;
			Use.Target = Target;
		}
		return true;
	}

	bool InitializeCanonicalDependencyManifest(
		const FAngelscriptStaticJITCompiledFunctionView& Function,
		const FAngelscriptStaticJITCompiledSourceGraph& Graph,
		const asSSourceSpan& RootSpan,
		FAngelscriptTypedASTJITDependencyAnalysis& Result)
	{
		if (Function.FunctionKey.Hash.IsZero()
			|| Function.ModuleKey.Hash.IsZero())
		{
			Result = DependencyFail(RootSpan, TEXT(
				"Use=FunctionIdentity StableMapping=Missing: canonical dependency analysis requires exact function and module keys."));
			return false;
		}
		for (const FAngelscriptStaticJITDependencyView& Dependency
			: Graph.Dependencies)
		{
			if (Dependency.OwnerFunctionKey != Function.FunctionKey)
				continue;
			if (!Dependency.Reference.IsValid())
			{
				Result = DependencyFail(RootSpan,
					TEXT("Use=CompilerReference StableMapping=Invalid"));
				return false;
			}
			Result.Manifest.RequiredReferences.AddUnique(Dependency.Reference);
		}
		Result.Manifest.RequiredReferences.Sort();

		for (const FAngelscriptStaticJITSemanticDependencyView& Dependency
			: Graph.SemanticDependencies)
		{
			if (Dependency.OwnerFunctionKey != Function.FunctionKey)
				continue;
			if (Dependency.Kind
					== EAngelscriptArtifactSemanticDependencyKind::Invalid
				|| !Dependency.Reference.IsValid()
				|| !HasExactReference(Result.Manifest, Dependency.Reference))
			{
				Result = DependencyFail(RootSpan, TEXT(
					"Use=CompilerArtifactDependency StableMapping=InvalidOrMissingReference"));
				return false;
			}
			FAngelscriptArtifactSemanticDependency& Copy =
				Result.Manifest.ArtifactDependencies.Emplace_GetRef();
			Copy.Kind = Dependency.Kind;
			Copy.Target = Dependency.Reference;
			Copy.ExpectedContentOrValue = Dependency.ExpectedContentOrValue;
		}
		Result.Manifest.ArtifactDependencies.Sort();
		return true;
	}

	asCDataType ResolveType(
		const asCASTContext& Context,
		const asCQualType& QualType)
	{
		asCRuntimeTypeBridge Bridge(nullptr);
		return Bridge.Resolve(Context, QualType);
	}

	struct FCanonicalScalarCppType
	{
		FString Spelling;
		int32 SizeInBytes = 0;
		bool bInteger = false;
		bool bUnsigned = false;
	};

	bool IsIntegerToken(const eTokenType Token)
	{
		switch (Token)
		{
		case ttInt8:
		case ttUInt8:
		case ttInt16:
		case ttUInt16:
		case ttInt:
		case ttUInt:
		case ttInt64:
		case ttUInt64:
			return true;
		default:
			return false;
		}
	}

	bool IsExactScriptEnumType(
		const asCASTContext& Context,
		const asCType& Type)
	{
		if (Type.kind != asAST_TYPE_ENUM || Type.stableKey.GetLength() == 0)
		{
			return false;
		}
		asUINT Matches = 0;
		for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
			if (Decl != nullptr
				&& Decl->kind == asAST_DECL_ENUM
				&& Decl->stableKey.Equals(Type.stableKey))
			{
				++Matches;
			}
		}
		return Matches == 1;
	}

	bool IsCanonicalVoidType(
		const asCASTContext& Context,
		const asCQualType& QualType)
	{
		const asCType* const Type = Context.GetType(QualType.type);
		return Type != nullptr && Type->kind == asAST_TYPE_VOID;
	}

	bool TryDescribeCanonicalScalarCppType(
		const asCASTContext& Context,
		const asCQualType& QualType,
		FCanonicalScalarCppType& OutType)
	{
		OutType = {};
		const asCDataType RuntimeType = ResolveType(Context, QualType);
		if (TrySpellAngelscriptTypedASTJITCppType(
			RuntimeType, OutType.Spelling))
		{
			OutType.SizeInBytes = RuntimeType.GetSizeInMemoryBytes();
			OutType.bInteger = RuntimeType.IsEnumType()
				|| IsIntegerToken(RuntimeType.GetTokenType());
			OutType.bUnsigned = OutType.bInteger
				&& !RuntimeType.IsEnumType()
				&& RuntimeType.IsUnsignedType();
			return true;
		}

		const asCType* const CanonicalType = Context.GetType(QualType.type);
		if (CanonicalType == nullptr
			|| QualType.IsHandle()
			|| QualType.IsAutoHandle()
			|| QualType.IsReference()
			|| !IsExactScriptEnumType(Context, *CanonicalType))
		{
			return false;
		}

		// Script enums in this maintained fork use the same one-byte signed
		// storage established by asCBuilder. Keep the script-only semantic name
		// in the AST while emitting the reviewed ABI carrier.
		OutType.Spelling = TEXT("int8");
		OutType.SizeInBytes = 1;
		OutType.bInteger = true;
		return true;
	}

	bool TrySpellCanonicalScalarCppType(
		const asCASTContext& Context,
		const asCQualType& QualType,
		FString& OutType)
	{
		FCanonicalScalarCppType Description;
		if (!TryDescribeCanonicalScalarCppType(
			Context, QualType, Description))
		{
			OutType.Reset();
			return false;
		}
		OutType = MoveTemp(Description.Spelling);
		return true;
	}

	bool TrySpellCanonicalCallArgumentCppType(
		const asCASTContext& Context,
		const asCQualType& QualType,
		FString& OutType)
	{
		const asCType* const Type = Context.GetType(QualType.type);
		if (Type != nullptr
			&& Type->kind == asAST_TYPE_VALUE_OBJECT
			&& QualType.IsConst()
			&& QualType.IsReference()
			&& !QualType.IsHandle()
			&& !QualType.IsAutoHandle()
			&& (Type->stableKey == "FString"
				|| Type->stableKey == "FLinearColor"))
		{
			// The reviewed BorrowedManagedPOD route owns these exact names as
			// part of its public C++ ABI. Resolve the spelling from the sealed,
			// address-free Canonical type key before consulting any Runtime
			// coordinate. No arbitrary user-defined value object is admitted.
			OutType = UTF8_TO_TCHAR(Type->stableKey.AddressOf());
			OutType += TEXT("&");
			return true;
		}

		return TrySpellCanonicalScalarCppType(Context, QualType, OutType);
	}

	bool IsByValueScalarOrEnum(const asCDataType& Type, const bool bAllowVoid)
	{
		if (!Type.IsValid() || Type.IsReference() || Type.IsObjectHandle())
		{
			return false;
		}
		if (bAllowVoid && Type.GetTokenType() == ttVoid)
		{
			return true;
		}
		if (Type.IsEnumType())
		{
			const int32 Size = Type.GetSizeInMemoryBytes();
			return Size == 1 || Size == 2 || Size == 4 || Size == 8;
		}
		if (Type.GetTypeInfo() != nullptr)
		{
			return false;
		}
		switch (Type.GetTokenType())
		{
		case ttBool:
		case ttInt8:
		case ttUInt8:
		case ttInt16:
		case ttUInt16:
		case ttInt:
		case ttUInt:
		case ttInt64:
		case ttUInt64:
		case ttFloat32:
		case ttFloat64:
			return true;
		default:
			return false;
		}
	}

	bool IsStructurallyBorrowedManagedInput(const asCDataType& Type)
	{
		const asCTypeInfo* const TypeInfo = Type.GetTypeInfo();
		return TypeInfo != nullptr
			&& TypeInfo->module == nullptr
			&& Type.IsObject()
			&& Type.IsReference()
			&& Type.IsReadOnly()
			&& !Type.IsObjectHandle();
	}

	const asCDecl* FindFunctionDecl(
		const asCASTContext& Context,
		asASTDeclId FunctionDecl,
		const char* Name)
	{
		if (FunctionDecl.IsValid())
		{
			return Context.GetDecl(FunctionDecl);
		}
		if (Name != nullptr && *Name != 0)
		{
			const asCDecl* Match = nullptr;
			asUINT MatchCount = 0;
			for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
			{
				const asCDecl* Decl = Context.GetDecl(asASTDeclId(Index));
				if (Decl != nullptr
					&& (Decl->kind == asAST_DECL_FUNCTION
						|| Decl->kind == asAST_DECL_METHOD)
					&& Decl->name.Equals(Name))
				{
					++MatchCount;
					Match = Decl;
				}
			}
			if (MatchCount == 1)
			{
				return Match;
			}
			return nullptr;
		}
		return nullptr;
	}

	EFallback UnsupportedExprReason(const asEASTExprKind Kind)
	{
		switch (Kind)
		{
		case asAST_EXPR_ERROR:
			return EFallback::UnsupportedExpression;
		case asAST_EXPR_INDEX:
		case asAST_EXPR_MEMBER_REF:
			return EFallback::UnsupportedExpression;
		case asAST_EXPR_STRING_LITERAL:
		case asAST_EXPR_NULL_LITERAL:
			return EFallback::UnsupportedType;
		default:
			return EFallback::None;
		}
	}

	class FEligibilityTraversalVisitor final : public asIASTConstVisitor
	{
	public:
		FEligibilityTraversalVisitor(
			const EAngelscriptTypedASTJITEligibilityPurpose InPurpose,
			FAngelscriptTypedASTJITEligibility& InFailure)
			: Purpose(InPurpose)
			, OutFailure(InFailure)
		{
		}

		bool EnterNode(
			const asCASTContext& Context,
			const asSASTNodeRef& Node,
			const asSASTTraversalEdge*,
			asUINT) override
		{
			if (Node.nodeClass == asAST_NODE_STMT)
			{
				const asCStmt* Stmt = Context.GetStmt(asASTStmtId(Node.value, Node.snapshotOwner));
				if (Stmt == nullptr)
				{
					OutFailure = Reject(EFallback::UnsupportedStatement, {},
						TEXT("Canonical statement ID is dangling."));
					return false;
				}
				switch (Stmt->kind)
				{
				case asAST_STMT_BLOCK:
				case asAST_STMT_DECL:
				case asAST_STMT_EXPR:
				case asAST_STMT_IF:
				case asAST_STMT_FOR:
				case asAST_STMT_FOREACH:
				case asAST_STMT_WHILE:
				case asAST_STMT_DO_WHILE:
				case asAST_STMT_SWITCH:
				case asAST_STMT_CASE:
				case asAST_STMT_BREAK:
				case asAST_STMT_CONTINUE:
				case asAST_STMT_RETURN:
				case asAST_STMT_FALLTHROUGH:
					return true;
				case asAST_STMT_ERROR:
					OutFailure = Reject(EFallback::UnsupportedStatement,
						MakeSpan(Context, Stmt->range),
						TEXT("Sealed AST contains an unsupported statement."));
					return false;
				default:
					OutFailure = Reject(EFallback::UnsupportedStatement,
						MakeSpan(Context, Stmt->range),
						TEXT("Statement kind is outside the reviewed scalar profile."));
					return false;
				}
			}

			if (Node.nodeClass != asAST_NODE_EXPR)
				return true;
			const asCExpr* Expr = Context.GetExpr(asASTExprId(Node.value, Node.snapshotOwner));
			if (Expr == nullptr)
			{
				OutFailure = Reject(EFallback::UnsupportedExpression, {},
					TEXT("Canonical expression ID is dangling."));
				return false;
			}
			const EFallback ExprReason = UnsupportedExprReason(Expr->kind);
			if (ExprReason != EFallback::None)
			{
				OutFailure = Reject(ExprReason, MakeSpan(Context, Expr->range),
					TEXT("Canonical expression is outside the reviewed scalar profile."));
				return false;
			}
			if (Expr->kind == asAST_EXPR_CONSTRUCT
				|| Expr->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
				|| Expr->kind == asAST_EXPR_CLEANUP)
			{
				FString ScalarType;
				bool bHasScalarValueType = TrySpellCanonicalScalarCppType(
					Context, Expr->type, ScalarType);
				if (!bHasScalarValueType
					&& Expr->kind == asAST_EXPR_CLEANUP
					&& Expr->children.GetLength() == 1)
				{
					const asCExpr* const Value =
						Context.GetExpr(Expr->children[0]);
					bHasScalarValueType = Value != nullptr
						&& TrySpellCanonicalScalarCppType(
							Context, Value->type, ScalarType);
				}
				if (Expr->children.GetLength() != 1
					|| !bHasScalarValueType)
				{
					FString ChildKinds;
					for (asUINT ChildIndex = 0;
						ChildIndex < Expr->children.GetLength(); ++ChildIndex)
					{
						const asCExpr* const Child =
							Context.GetExpr(Expr->children[ChildIndex]);
						ChildKinds += FString::Printf(
							TEXT("%s%u:%u"),
							ChildIndex == 0 ? TEXT("") : TEXT(","),
							Expr->children[ChildIndex].value,
							Child != nullptr
								? static_cast<uint32>(Child->kind)
								: MAX_uint32);
					}
					OutFailure = Reject(
						EFallback::UnsupportedLifetime,
						MakeSpan(Context, Expr->range),
						FString::Printf(TEXT(
							"Only one-child scalar lifetime wrappers are transparent in the canonical TypedASTJIT profile. Kind=%u Children=%u [%s] ScalarType=%s"),
							static_cast<uint32>(Expr->kind),
							Expr->children.GetLength(),
							*ChildKinds,
							*ScalarType));
					return false;
				}
			}
			if (Expr->kind == asAST_EXPR_CALL)
			{
				if (Expr->receiver.IsValid())
				{
					OutFailure = Reject(
						EFallback::UnsupportedReceiver,
						MakeSpan(Context, Expr->range),
						TEXT("Canonical call receivers require explicit receiver evaluation, object-identity, null-state, and ABI lowering before TypedASTJIT closure emission."));
					return false;
				}
				if (const asCDecl* Callee = Context.GetDecl(Expr->resolvedDecl))
				{
					if (Callee->kind == asAST_DECL_IMPORT)
					{
						OutFailure = Reject(
							EFallback::UnsupportedImportedRoute,
							MakeSpan(Context, Expr->range),
							TEXT("ImportedBindingRouteUnavailable: canonical import slots stay mutable."));
						return false;
					}
				}
				if (Purpose == EAngelscriptTypedASTJITEligibilityPurpose::StandaloneRoot)
				{
					OutFailure = Reject(EFallback::UnsupportedCall,
						MakeSpan(Context, Expr->range),
						TEXT("Resolved calls require whole-root closure planning."));
					return false;
				}
			}
			if (Expr->kind == asAST_EXPR_DECL_REF)
			{
				if (const asCDecl* Target = Context.GetDecl(Expr->resolvedDecl))
				{
					if (Target->kind == asAST_DECL_VAR)
					{
						const asCDecl* Parent = Context.GetDecl(Target->parent);
						if (Parent != nullptr
							&& (Parent->kind == asAST_DECL_CLASS
								|| Parent->kind == asAST_DECL_INTERFACE))
						{
							OutFailure = Reject(
								EFallback::UnsupportedReceiver,
								MakeSpan(Context, Expr->range),
								TEXT("The reviewed instance slice preserves object identity but does not read native receiver field storage."));
							return false;
						}
						if (Parent != nullptr
							&& Parent->kind == asAST_DECL_TRANSLATION_UNIT
							&& !Target->hasConstantValue)
						{
							OutFailure = Reject(
								EFallback::UnsupportedGlobalStorage,
								MakeSpan(Context, Expr->range),
								TEXT("The initial TypedASTJIT profile does not read or mutate runtime global storage."));
							return false;
						}
					}
				}
			}
			return true;
		}

		bool VisitEdge(const asCASTContext&, const asSASTTraversalEdge&, asUINT) override { return true; }
		bool LeaveNode(const asCASTContext&, const asSASTNodeRef&, const asSASTTraversalEdge*, asUINT) override { return true; }
		bool ShouldTraverseEdge(const asCASTContext&, const asSASTTraversalEdge& Edge, asUINT) override
		{
			// Receiver-bearing calls fail closed in EnterNode. Do not independently
			// reinterpret receiver expressions as ordinary call arguments here.
			return Edge.role != asAST_EDGE_EXPR_RECEIVER;
		}

	private:
		EAngelscriptTypedASTJITEligibilityPurpose Purpose;
		FAngelscriptTypedASTJITEligibility& OutFailure;
	};

	bool LifetimeNodeBelongsToFunction(
		const asCASTContext& Context,
		const asSASTLifetimeNodeId& Node,
		const asASTDeclId FunctionDecl)
	{
		if (!Node.IsValid())
		{
			return false;
		}
		if (Node.nodeClass == asAST_LIFETIME_NODE_STMT)
		{
			const asCStmt* const Statement = Context.GetStmt(
				asASTStmtId(Node.value));
			return Statement != nullptr && Statement->owner == FunctionDecl;
		}
		if (Node.nodeClass == asAST_LIFETIME_NODE_DECL)
		{
			asASTDeclId Current(Node.value);
			for (asUINT Guard = 0;
				Current.IsValid() && Guard <= Context.GetDeclCount(); ++Guard)
			{
				if (Current == FunctionDecl)
				{
					return true;
				}
				const asCDecl* const Declaration = Context.GetDecl(Current);
				if (Declaration == nullptr || Declaration->parent == Current)
				{
					break;
				}
				Current = Declaration->parent;
			}
		}
		return false;
	}

	bool WriteCanonicalLifetimeTypeIdentity(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const asCASTContext& Context,
		const asCQualType& Type,
		const asCDecl* const Storage)
	{
		if (!Type.IsValid())
		{
			Writer.WriteString(TEXT("<none>"));
			Writer.WriteUInt32(0);
			return Storage == nullptr;
		}
		const asCType* const CanonicalType = Context.GetType(Type.type);
		if (CanonicalType == nullptr || CanonicalType->stableKey.GetLength() == 0)
		{
			return false;
		}
		Writer.WriteString(UTF8_TO_TCHAR(CanonicalType->stableKey.AddressOf()));
		Writer.WriteUInt32(Type.quals);
		if (Storage != nullptr)
		{
			Writer.WriteUInt32(static_cast<uint32>(
				FMath::Max(Storage->byteSize, 0)));
			Writer.WriteUInt32(Storage->byteAlignment);
		}
		return true;
	}

	bool WriteCanonicalLifetimeNodeStableIdentity(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const asCASTContext& Context,
		const asSASTLifetimeNodeId& Node)
	{
		Writer.WriteUInt8(static_cast<uint8>(Node.nodeClass));
		switch (Node.nodeClass)
		{
		case asAST_LIFETIME_NODE_DECL:
		{
			const asCDecl* const Declaration = Context.GetDecl(
				asASTDeclId(Node.value));
			if (Declaration == nullptr || Declaration->stableKey.GetLength() == 0)
			{
				return false;
			}
			Writer.WriteUInt8(static_cast<uint8>(Declaration->kind));
			Writer.WriteString(UTF8_TO_TCHAR(
				Declaration->stableKey.AddressOf()));
			return WriteCanonicalLifetimeTypeIdentity(
				Writer, Context, Declaration->type, nullptr);
		}
		case asAST_LIFETIME_NODE_STMT:
		{
			const asCStmt* const Statement = Context.GetStmt(
				asASTStmtId(Node.value));
			const asCDecl* const Owner = Statement != nullptr
				? Context.GetDecl(Statement->owner) : nullptr;
			if (Statement == nullptr || Owner == nullptr
				|| Owner->stableKey.GetLength() == 0)
			{
				return false;
			}
			Writer.WriteUInt8(static_cast<uint8>(Statement->kind));
			Writer.WriteString(UTF8_TO_TCHAR(Owner->stableKey.AddressOf()));
			return true;
		}
		case asAST_LIFETIME_NODE_EXPR:
		{
			const asCExpr* const Expression = Context.GetExpr(
				asASTExprId(Node.value));
			if (Expression == nullptr)
			{
				return false;
			}
			Writer.WriteUInt8(static_cast<uint8>(Expression->kind));
			Writer.WriteUInt8(static_cast<uint8>(Expression->valueCategory));
			return WriteCanonicalLifetimeTypeIdentity(
				Writer, Context, Expression->type, nullptr);
		}
		default:
			return false;
		}
	}

	bool WriteCanonicalAggregatePlanShape(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const asCASTContext& Context,
		const asCASTLifetimeView& LifetimeView,
		const asSASTLifetimeAggregatePlan& Plan,
		const asASTDeclId FunctionDecl,
		const asUINT Depth)
	{
		if (Depth > LifetimeView.GetAggregatePlanCount()
			|| Plan.semanticOwner != FunctionDecl
			|| Plan.actionKind != asAST_LIFETIME_ACTION_DESTROY_ELEMENT
			|| Plan.elementCommits.GetLength() == 0)
		{
			return false;
		}

		const asCExpr* const Aggregate = Context.GetExpr(Plan.aggregate);
		if (Aggregate == nullptr
			|| !WriteCanonicalLifetimeNodeStableIdentity(
				Writer, Context,
				asSASTLifetimeNodeId::FromExpr(Plan.aggregate)))
		{
			return false;
		}

		Writer.WriteUInt32(Plan.elementCommits.GetLength());
		for (asUINT ElementOrdinal = 0;
			ElementOrdinal < Plan.elementCommits.GetLength(); ++ElementOrdinal)
		{
			const asASTExprId ElementId = Plan.elementCommits[ElementOrdinal];
			if (!WriteCanonicalLifetimeNodeStableIdentity(
				Writer, Context,
				asSASTLifetimeNodeId::FromExpr(ElementId)))
			{
				return false;
			}

			const asSASTLifetimeAggregatePlan* const ChildPlan =
				LifetimeView.FindAggregatePlan(ElementId);
			Writer.WriteBool(ChildPlan != nullptr);
			if (ChildPlan == nullptr)
			{
				continue;
			}
			if (ChildPlan->parentAggregate != Plan.aggregate
				|| ChildPlan->semanticOwner != FunctionDecl)
			{
				return false;
			}
			Writer.WriteUInt32(ElementOrdinal);
			if (!WriteCanonicalAggregatePlanShape(
				Writer, Context, LifetimeView, *ChildPlan,
				FunctionDecl, Depth + 1))
			{
				return false;
			}
		}

		Writer.WriteBool(Plan.parentAggregate.IsValid());
		if (!Plan.parentAggregate.IsValid())
		{
			return true;
		}

		const asSASTLifetimeAggregatePlan* const ParentPlan =
			LifetimeView.FindAggregatePlan(Plan.parentAggregate);
		if (ParentPlan == nullptr || ParentPlan->semanticOwner != FunctionDecl
			|| !WriteCanonicalLifetimeNodeStableIdentity(
				Writer, Context,
				asSASTLifetimeNodeId::FromExpr(ParentPlan->aggregate)))
		{
			return false;
		}

		asUINT ChildOrdinal = 0;
		asUINT MatchCount = 0;
		for (asUINT ElementOrdinal = 0;
			ElementOrdinal < ParentPlan->elementCommits.GetLength();
			++ElementOrdinal)
		{
			if (ParentPlan->elementCommits[ElementOrdinal] == Plan.aggregate)
			{
				ChildOrdinal = ElementOrdinal;
				++MatchCount;
			}
		}
		if (MatchCount != 1)
		{
			return false;
		}
		Writer.WriteUInt32(ParentPlan->elementCommits.GetLength());
		Writer.WriteUInt32(ChildOrdinal);
		return true;
	}

	bool AddCanonicalLifetimeActionFlag(
		const asEASTLifetimeActionKind ActionKind,
		EAngelscriptTypedASTJITLifetimeActionFlags& Flags)
	{
		using EFlags = EAngelscriptTypedASTJITLifetimeActionFlags;
		switch (ActionKind)
		{
		case asAST_LIFETIME_ACTION_DESTROY_VALUE:
			Flags |= EFlags::DestroyValue;
			return true;
		case asAST_LIFETIME_ACTION_RELEASE_REFERENCE:
			Flags |= EFlags::ReleaseReference;
			return true;
		case asAST_LIFETIME_ACTION_COMMIT_DEFERRED_OUT:
			Flags |= EFlags::CommitDeferredOut;
			return true;
		case asAST_LIFETIME_ACTION_DESTROY_ELEMENT:
			Flags |= EFlags::DestroyElement;
			return true;
		default:
			return false;
		}
	}

	bool AddCanonicalLifetimeExitPlanFlag(
		const asEASTLifetimeExitPlanKind PlanKind,
		EAngelscriptTypedASTJITLifetimeExitPlanFlags& Flags)
	{
		using EFlags = EAngelscriptTypedASTJITLifetimeExitPlanFlags;
		switch (PlanKind)
		{
		case asAST_LIFETIME_EXIT_PLAN_NORMAL:
			Flags |= EFlags::Normal;
			return true;
		case asAST_LIFETIME_EXIT_PLAN_TRANSFER:
			Flags |= EFlags::Transfer;
			return true;
		case asAST_LIFETIME_EXIT_PLAN_FOREACH:
			Flags |= EFlags::ForEach;
			return true;
		case asAST_LIFETIME_EXIT_PLAN_FULL_EXPRESSION:
			Flags |= EFlags::FullExpression;
			return true;
		default:
			return false;
		}
	}

	bool BuildCanonicalLifetimeSummary(
		const asCASTContext& Context,
		const asASTDeclId FunctionDecl,
		const asCASTLifetimeView& LifetimeView,
		FAngelscriptTypedASTJITCanonicalLifetimeFacts& OutFacts)
	{
		const asCDecl* const Function = Context.GetDecl(FunctionDecl);
		if (Function == nullptr || Function->stableKey.GetLength() == 0)
		{
			return false;
		}

		TArray<FAngelscriptHash256> RecordStableKeys;
		RecordStableKeys.SetNum(LifetimeView.GetRecordCount());
		TArray<asUINT> FunctionRecordIndices;
		FAngelscriptArtifactCanonicalWriter ABIWriter(
			TEXT("Angelscript.TypedASTJIT.CanonicalLifetimeABI.v3"));
		ABIWriter.WriteUInt32(LifetimeView.GetProtocolRevision());

		bool bHasPartialConstruction = false;
		bool bSawDestroyValue = false;
		for (asUINT RecordIndex = 0;
			RecordIndex < LifetimeView.GetRecordCount(); ++RecordIndex)
		{
			const asSASTLifetimeViewRecord* const ViewRecord =
				LifetimeView.GetRecord(RecordIndex);
			if (ViewRecord == nullptr
				|| !LifetimeNodeBelongsToFunction(
					Context, ViewRecord->record.semanticRegion, FunctionDecl))
			{
				continue;
			}

			const asSASTLifetimeRecord& Record = ViewRecord->record;
			FAngelscriptArtifactCanonicalWriter RecordWriter(
				TEXT("Angelscript.TypedASTJIT.CanonicalLifetimeRecord.v2"));
			RecordWriter.WriteString(UTF8_TO_TCHAR(
				Function->stableKey.AddressOf()));
			RecordWriter.WriteUInt8(static_cast<uint8>(Record.subjectKind));
			RecordWriter.WriteUInt8(static_cast<uint8>(Record.actionKind));
			RecordWriter.WriteUInt8(static_cast<uint8>(Record.phase));
			RecordWriter.WriteUInt32(Record.supportedExitMask);
			RecordWriter.WriteUInt32(Record.constructionStep);
			RecordWriter.WriteUInt32(Record.order);
			RecordWriter.WriteBool(Record.completeObjectCommit);
			RecordWriter.WriteUInt32(ViewRecord->activationOrdinal);
			RecordWriter.WriteUInt32(ViewRecord->cleanupOrdinal);
			if (!WriteCanonicalLifetimeNodeStableIdentity(
					RecordWriter, Context, Record.subject)
				|| !WriteCanonicalLifetimeNodeStableIdentity(
					RecordWriter, Context, Record.actionTarget)
				|| !WriteCanonicalLifetimeNodeStableIdentity(
					RecordWriter, Context, Record.activationPoint)
				|| !WriteCanonicalLifetimeNodeStableIdentity(
					RecordWriter, Context, Record.semanticRegion)
				|| !AddCanonicalLifetimeActionFlag(
					Record.actionKind, OutFacts.LifetimeSummary.ActionFlags))
			{
				return false;
			}
			FunctionRecordIndices.Add(RecordIndex);
			OutFacts.LifetimeSummary.SupportedExitMask |=
				Record.supportedExitMask;

			ABIWriter.WriteUInt8(static_cast<uint8>(Record.actionKind));
			const asCDecl* Storage = Record.subject.nodeClass
				== asAST_LIFETIME_NODE_DECL
				? Context.GetDecl(asASTDeclId(Record.subject.value)) : nullptr;
			asCQualType StorageType = Storage != nullptr
				? Storage->type : asCQualType();
			if (Record.subjectKind == asAST_LIFETIME_SUBJECT_TEMPORARY)
			{
				const asCExpr* const Materialize =
					Record.subject.nodeClass == asAST_LIFETIME_NODE_EXPR
					? Context.GetExpr(
						asASTExprId(Record.subject.value)) : nullptr;
				const asCDecl* const Destructor =
					Record.actionTarget.nodeClass == asAST_LIFETIME_NODE_DECL
					? Context.GetDecl(
						asASTDeclId(Record.actionTarget.value)) : nullptr;
				const asCDecl* const DestructorOwner = Destructor != nullptr
					? Context.GetDecl(Destructor->parent) : nullptr;
				const asCType* const MaterializedType = Materialize != nullptr
					&& Materialize->type.IsValid()
					? Context.GetType(Materialize->type.type) : nullptr;
				const asCStmt* const SemanticRegion =
					Record.semanticRegion.nodeClass == asAST_LIFETIME_NODE_STMT
					? Context.GetStmt(
						asASTStmtId(Record.semanticRegion.value)) : nullptr;
				if (Materialize == nullptr
					|| Materialize->kind
						!= asAST_EXPR_MATERIALIZE_TEMPORARY
					|| Materialize->valueCategory != asAST_VALUE_XVALUE
					|| Materialize->type.IsReference()
					|| Materialize->type.IsHandle()
					|| Record.activationPoint != Record.subject
					|| Record.actionKind
						!= asAST_LIFETIME_ACTION_DESTROY_VALUE
					|| Record.phase != asAST_LIFETIME_PHASE_FULL_EXPRESSION
					|| Record.supportedExitMask
						!= (asAST_LIFETIME_EXIT_NORMAL
							| asAST_LIFETIME_EXIT_EXCEPTION)
					|| Destructor == nullptr
					|| Destructor->kind != asAST_DECL_DESTRUCTOR
					|| DestructorOwner == nullptr
					|| DestructorOwner->kind != asAST_DECL_CLASS
					|| MaterializedType == nullptr
					|| MaterializedType->kind != asAST_TYPE_VALUE_OBJECT
					|| !MaterializedType->stableKey.Equals(
						DestructorOwner->stableKey)
					|| SemanticRegion == nullptr
					|| SemanticRegion->kind != asAST_STMT_EXPR
					|| SemanticRegion->owner != FunctionDecl)
				{
					return false;
				}
				Storage = DestructorOwner;
				StorageType = Materialize->type;
			}
			if (Record.subjectKind
				== asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT)
			{
				const asSASTLifetimeAggregatePlan* const AggregatePlan =
					LifetimeView.FindAggregatePlan(
						asASTExprId(Record.subject.value));
				const asCDecl* const ElementDestructor =
					Record.actionTarget.nodeClass == asAST_LIFETIME_NODE_DECL
					? Context.GetDecl(
						asASTDeclId(Record.actionTarget.value)) : nullptr;
				Storage = ElementDestructor != nullptr
					? Context.GetDecl(ElementDestructor->parent) : nullptr;
				StorageType = Storage != nullptr
					? Storage->type : asCQualType();
				if (AggregatePlan == nullptr
					|| AggregatePlan->semanticOwner != FunctionDecl
					|| AggregatePlan->actionKind != Record.actionKind
					|| AggregatePlan->actionTarget != Record.actionTarget
					|| AggregatePlan->elementCommits.GetLength() == 0
					|| !WriteCanonicalAggregatePlanShape(
						RecordWriter, Context, LifetimeView,
						*AggregatePlan, FunctionDecl, 0)
					|| !WriteCanonicalAggregatePlanShape(
						ABIWriter, Context, LifetimeView,
						*AggregatePlan, FunctionDecl, 0))
				{
					return false;
				}
			}
			if (Storage == nullptr
				|| !WriteCanonicalLifetimeTypeIdentity(
					ABIWriter, Context, StorageType, Storage)
				|| !WriteCanonicalLifetimeNodeStableIdentity(
					ABIWriter, Context, Record.actionTarget))
			{
				return false;
			}
			RecordStableKeys[RecordIndex] = RecordWriter.FinalizeHash();

			bSawDestroyValue |= Record.actionKind
				== asAST_LIFETIME_ACTION_DESTROY_VALUE;
			bHasPartialConstruction |=
				Record.subjectKind == asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT
				|| Record.subjectKind == asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT
				|| Record.actionKind == asAST_LIFETIME_ACTION_COMMIT_DEFERRED_OUT
				|| Record.actionKind == asAST_LIFETIME_ACTION_DESTROY_ELEMENT
				|| Record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE
				|| Record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER
				|| Record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING
				|| Record.phase == asAST_LIFETIME_PHASE_COMPLETE_OBJECT
				|| Record.phase == asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT;
		}

		FAngelscriptArtifactCanonicalWriter StableWriter(
			TEXT("Angelscript.TypedASTJIT.CanonicalLifetimeSummary.v2"));
		StableWriter.WriteUInt32(LifetimeView.GetProtocolRevision());
		StableWriter.WriteString(UTF8_TO_TCHAR(
			Function->stableKey.AddressOf()));
		StableWriter.WriteUInt32(static_cast<uint32>(
			FunctionRecordIndices.Num()));
		for (const asUINT RecordIndex : FunctionRecordIndices)
		{
			StableWriter.WriteHash(RecordStableKeys[RecordIndex]);
		}

		for (asUINT PlanIndex = 0;
			PlanIndex < LifetimeView.GetExitPlanCount(); ++PlanIndex)
		{
			const asSASTLifetimeExitPlan* const Plan =
				LifetimeView.GetExitPlan(PlanIndex);
			const asCStmt* const StructuralOwner = Plan != nullptr
				? Context.GetStmt(Plan->structuralOwner) : nullptr;
			if (StructuralOwner == nullptr
				|| StructuralOwner->owner != FunctionDecl)
			{
				continue;
			}
			if (!AddCanonicalLifetimeExitPlanFlag(
				Plan->kind, OutFacts.LifetimeSummary.ExitPlanFlags))
			{
				return false;
			}
			StableWriter.WriteUInt8(static_cast<uint8>(Plan->kind));
			StableWriter.WriteUInt32(Plan->exitMask);
			StableWriter.WriteUInt32(Plan->recordIndices.GetLength());
			for (asUINT OrderedIndex = 0;
				OrderedIndex < Plan->recordIndices.GetLength(); ++OrderedIndex)
			{
				const asUINT RecordIndex = Plan->recordIndices[OrderedIndex];
				if (RecordIndex >= static_cast<asUINT>(RecordStableKeys.Num())
					|| RecordStableKeys[RecordIndex].IsZero())
				{
					return false;
				}
				StableWriter.WriteHash(RecordStableKeys[RecordIndex]);
			}
			++OutFacts.LifetimeSummary.ExitPlanCount;
		}

		OutFacts.LifetimeSummary.ProtocolRevision =
			LifetimeView.GetProtocolRevision();
		OutFacts.LifetimeSummary.RecordCount = static_cast<uint32>(
			FunctionRecordIndices.Num());
		StableWriter.WriteUInt32(static_cast<uint32>(
			OutFacts.LifetimeSummary.ActionFlags));
		StableWriter.WriteUInt32(static_cast<uint32>(
			OutFacts.LifetimeSummary.ExitPlanFlags));
		StableWriter.WriteUInt32(
			OutFacts.LifetimeSummary.SupportedExitMask);
		StableWriter.WriteUInt32(
			OutFacts.LifetimeSummary.ExitPlanCount);
		OutFacts.LifetimeSummary.StableKey = StableWriter.FinalizeHash();
		ABIWriter.WriteUInt32(static_cast<uint32>(
			OutFacts.LifetimeSummary.ActionFlags));
		ABIWriter.WriteUInt32(static_cast<uint32>(
			OutFacts.LifetimeSummary.ExitPlanFlags));
		ABIWriter.WriteUInt32(
			OutFacts.LifetimeSummary.SupportedExitMask);
		ABIWriter.WriteUInt32(
			OutFacts.LifetimeSummary.RecordCount);
		ABIWriter.WriteUInt32(
			OutFacts.LifetimeSummary.ExitPlanCount);
		OutFacts.LifetimeSummary.ABIKey = ABIWriter.FinalizeHash();
		OutFacts.LifetimeSummary.bNativeObjectFrameABIAvailable =
			OutFacts.LifetimeSummary.RecordCount == 0;

		if (!OutFacts.LifetimeSummary.IsAuthenticated())
		{
			return false;
		}
		if (bHasPartialConstruction)
		{
			OutFacts.CleanupPlanState =
				EAngelscriptTypedASTJITCleanupPlanState::PartialConstruction;
		}
		else if (bSawDestroyValue)
		{
			OutFacts.CleanupPlanState =
				EAngelscriptTypedASTJITCleanupPlanState::ScriptDestructor;
		}
		else if (OutFacts.LifetimeSummary.RecordCount != 0)
		{
			OutFacts.CleanupPlanState =
				EAngelscriptTypedASTJITCleanupPlanState::NonEmpty;
		}
		else
		{
			OutFacts.CleanupPlanState =
				EAngelscriptTypedASTJITCleanupPlanState::VerifiedEmpty;
		}
		OutFacts.bHasExceptionCleanup =
			OutFacts.LifetimeSummary.RecordCount != 0
			&& (OutFacts.LifetimeSummary.SupportedExitMask
				& asAST_LIFETIME_EXIT_EXCEPTION) != 0;
		// A verifier-authenticated construction plan is semantically complete even
		// when the current native backend cannot represent its committed-prefix
		// cursor or exception exit.  Backend capability is reported separately by
		// bNativeObjectFrameABIAvailable; treating this protocol as incomplete would
		// misclassify a precise per-function fallback as InvalidCleanupPlan.
		OutFacts.bCleanupPlanCoversAllTransfers = true;
		return true;
	}

	bool WalkStmt(
		const asCASTContext& Context,
		asASTStmtId StmtId,
		const EAngelscriptTypedASTJITEligibilityPurpose Purpose,
		FAngelscriptTypedASTJITEligibility& OutFailure)
	{
		if (!StmtId.IsValid())
			return true;
		FEligibilityTraversalVisitor Visitor(Purpose, OutFailure);
		asSASTTraversalOptions Options;
		Options.validateEdgeTargets = false;
		Options.maxNodes = Context.GetStmtCount() + Context.GetExprCount() + 1;
		Options.maxDepth = Options.maxNodes;
		asSASTTraversalResult Result;
		const int Status = asCASTTraverse(Context, asSASTNodeRef::FromStmt(StmtId), Visitor, Options, Result);
		if (Status == asAST_TRAVERSAL_OK)
			return true;
		if (Status == asAST_TRAVERSAL_CALLBACK_ABORTED)
			return false;
		const bool bExpressionFailure = Result.offendingNode.nodeClass == asAST_NODE_EXPR;
		OutFailure = Reject(
			bExpressionFailure ? EFallback::UnsupportedExpression : EFallback::UnsupportedStatement,
			{},
			bExpressionFailure
				? TEXT("Canonical expression ID is dangling or its graph is malformed.")
				: TEXT("Canonical statement ID is dangling or its graph is malformed."));
		return false;
	}

	void CollectCallsFromExpr(
		const asCASTContext& Context,
		asASTExprId ExprId,
		TArray<asASTExprId>& OutCalls)
	{
		if (!ExprId.IsValid())
		{
			return;
		}
		const asCExpr* Expr = Context.GetExpr(ExprId);
		if (Expr == nullptr)
		{
			return;
		}
		if (Expr->kind == asAST_EXPR_CALL)
		{
			OutCalls.Add(ExprId);
		}
		for (asUINT Index = 0; Index < Expr->children.GetLength(); ++Index)
		{
			CollectCallsFromExpr(Context, Expr->children[Index], OutCalls);
		}
	}

	void CollectCallsFromStmt(
		const asCASTContext& Context,
		asASTStmtId StmtId,
		TArray<asASTExprId>& OutCalls)
	{
		if (!StmtId.IsValid())
		{
			return;
		}
		const asCStmt* Stmt = Context.GetStmt(StmtId);
		if (Stmt == nullptr)
		{
			return;
		}
		CollectCallsFromExpr(Context, Stmt->expr, OutCalls);
		for (asUINT Index = 0; Index < Stmt->children.GetLength(); ++Index)
		{
			CollectCallsFromStmt(Context, Stmt->children[Index], OutCalls);
		}
	}

	void CollectDeclRefsFromExpr(
		const asCASTContext& Context,
		const asASTExprId ExprId,
		TArray<asASTExprId>& OutDeclRefs)
	{
		if (!ExprId.IsValid())
		{
			return;
		}
		const asCExpr* const Expr = Context.GetExpr(ExprId);
		if (Expr == nullptr)
		{
			return;
		}
		if (Expr->kind == asAST_EXPR_DECL_REF)
		{
			OutDeclRefs.Add(ExprId);
		}
		if (Expr->receiver.IsValid())
		{
			CollectDeclRefsFromExpr(Context, Expr->receiver, OutDeclRefs);
		}
		for (asUINT Index = 0; Index < Expr->children.GetLength(); ++Index)
		{
			CollectDeclRefsFromExpr(
				Context, Expr->children[Index], OutDeclRefs);
		}
	}

	void CollectDeclRefsFromStmt(
		const asCASTContext& Context,
		const asASTStmtId StmtId,
		TArray<asASTExprId>& OutDeclRefs)
	{
		if (!StmtId.IsValid())
		{
			return;
		}
		const asCStmt* const Stmt = Context.GetStmt(StmtId);
		if (Stmt == nullptr)
		{
			return;
		}
		CollectDeclRefsFromExpr(Context, Stmt->expr, OutDeclRefs);
		for (asUINT Index = 0; Index < Stmt->children.GetLength(); ++Index)
		{
			CollectDeclRefsFromStmt(
				Context, Stmt->children[Index], OutDeclRefs);
		}
	}

	bool StmtHasLoop(
		const asCASTContext& Context,
		asASTStmtId StmtId)
	{
		if (!StmtId.IsValid())
		{
			return false;
		}
		const asCStmt* Stmt = Context.GetStmt(StmtId);
		if (Stmt == nullptr)
		{
			return false;
		}
		if (Stmt->kind == asAST_STMT_FOR
			|| Stmt->kind == asAST_STMT_FOREACH
			|| Stmt->kind == asAST_STMT_WHILE
			|| Stmt->kind == asAST_STMT_DO_WHILE)
		{
			return true;
		}
		for (asUINT Index = 0; Index < Stmt->children.GetLength(); ++Index)
		{
			if (StmtHasLoop(Context, Stmt->children[Index]))
			{
				return true;
			}
		}
		return false;
	}

	FString MakeIndent(const int32 Depth)
	{
		FString Result;
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Result += TEXT("\t");
		}
		return Result;
	}

	class FCanonicalScalarEmitter
	{
	public:
		FCanonicalScalarEmitter(
			const asCASTContext& InContext,
			const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
				InNativeCalls,
			const TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
				InScriptCalls,
			const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
				InGlobalDependencies,
			const bool bInEmitFramePosition,
			const FString& InReturnCppType)
			: Context(InContext)
			, NativeCalls(InNativeCalls)
			, ScriptCalls(InScriptCalls)
			, GlobalDependencies(InGlobalDependencies)
			, bEmitFramePosition(bInEmitFramePosition)
			, ReturnCppType(InReturnCppType)
		{
			bRequiresExecutionState = !ScriptCalls.IsEmpty()
				|| NativeCalls.ContainsByPredicate(
					[](const FAngelscriptTypedASTJITNativeCallEmissionPlan& Plan)
					{
						return Plan.Route
							!= EAngelscriptTypedASTJITCallEmissionRoute::Direct;
					});
		}

		bool Fail(const FString& Detail)
		{
			FailureDetail = Detail;
			return false;
		}

		void AppendSourcePosition(
			const asCSourceRange& Range,
			const int32 Depth,
			FString& Out) const
		{
			if (!bEmitFramePosition)
			{
				return;
			}
			// Match the VM/BytecodeJIT diagnostic contract: a failing operation is
			// attributed to its owning source statement, not an arbitrary operand
			// subexpression. This also keeps columns stable when lowering introduces
			// temporary expressions that never existed in source.
			const asCSourceRange& DiagnosticRange = CurrentStatementRange != nullptr
				? *CurrentStatementRange : Range;
			const asSSourceSpan Span =
				MakeSpan(Context, DiagnosticRange);
			if (Span.row == 0)
			{
				return;
			}
			Out += FString::Printf(
				TEXT("%sTypedFrame.SetSourcePosition(%u, %u);\n"),
				*MakeIndent(Depth), Span.row, Span.column);
		}

		void AppendExceptionReturn(const int32 Depth, FString& Out) const
		{
			if (!bRequiresExecutionState)
			{
				return;
			}
			const FString Prefix = MakeIndent(Depth);
			Out += Prefix + TEXT("if (UNLIKELY(Execution.bExceptionThrown))\n")
				+ Prefix + TEXT("{\n")
				+ Prefix + (ReturnCppType == TEXT("void")
					? TEXT("\treturn;\n")
					: TEXT("\treturn {};\n"))
				+ Prefix + TEXT("}\n");
		}

		const FString& GetFailureDetail() const
		{
			return FailureDetail;
		}

		bool RequiresScalarOps() const
		{
			return bRequiresScalarOps;
		}

		bool RequiresExecutionState() const
		{
			return bRequiresExecutionState;
		}

		const TArray<FString>& GetRequiredIncludes() const
		{
			return RequiredIncludes;
		}

		const TArray<FAngelscriptTypedASTJITNativeCallSiteMetadata>&
		GetNativeCallSites() const
		{
			return NativeCallSites;
		}

		bool EmitExpression(asASTExprId Id, const int32 Depth, FString& Out)
		{
			const asCExpr* Expr = Context.GetExpr(Id);
			if (Expr == nullptr)
			{
				return Fail(TEXT("Canonical expression ID is dangling."));
			}
			FString CppType;
			switch (Expr->kind)
			{
			case asAST_EXPR_INTEGER_LITERAL:
			{
				if (!TrySpellCanonicalScalarCppType(
					Context, Expr->type, CppType))
				{
					return Fail(TEXT("Literal type has no reviewed C++ spelling"));
				}
				const FString Literal = UTF8_TO_TCHAR(Expr->literal.AddressOf());
				if (Literal.IsEmpty())
				{
					bRequiresScalarOps = true;
					Out = FString::Printf(TEXT(
						"AngelscriptTypedASTJIT::FromCanonicalBits<%s>(UINT64_C(%llu))"),
						*CppType,
						static_cast<unsigned long long>(Expr->literalBits));
					return true;
				}
				Out = FString::Printf(TEXT("%s(%s)"), *CppType, *Literal);
				return true;
			}
			case asAST_EXPR_BOOL_LITERAL:
			case asAST_EXPR_FLOAT_LITERAL:
				if (!TrySpellCanonicalScalarCppType(
					Context, Expr->type, CppType))
				{
					return Fail(TEXT("Literal type has no reviewed C++ spelling"));
				}
				bRequiresScalarOps = true;
				Out = FString::Printf(TEXT(
					"AngelscriptTypedASTJIT::FromCanonicalBits<%s>(UINT64_C(%llu))"),
					*CppType,
					static_cast<unsigned long long>(Expr->literalBits));
				return true;
			case asAST_EXPR_DECL_REF:
				if (const asCDecl* const Target =
					Context.GetDecl(Expr->resolvedDecl))
				{
					const asCDecl* const Parent =
						Context.GetDecl(Target->parent);
					if (Target->kind == asAST_DECL_VAR
						&& Parent != nullptr
						&& (Parent->kind == asAST_DECL_CLASS
							|| Parent->kind == asAST_DECL_INTERFACE))
					{
						return Fail(TEXT(
							"Canonical instance-field access requires native receiver storage lowering"));
					}
					if (Target->kind == asAST_DECL_VAR
						&& Parent != nullptr
						&& Parent->kind == asAST_DECL_ENUM
						&& Target->hasConstantValue)
					{
						FCanonicalScalarCppType EnumCarrier;
						if (!TryDescribeCanonicalScalarCppType(
							Context, Expr->type, EnumCarrier)
							|| !EnumCarrier.bInteger)
						{
							return Fail(TEXT(
								"Canonical enum constant has no reviewed integral C++ carrier"));
						}
						Out = FString::Printf(TEXT(
							"%s(%lld)"),
							*EnumCarrier.Spelling,
							static_cast<long long>(static_cast<asINT64>(
								Target->constantValue)));
						return true;
					}
					if (Target->kind == asAST_DECL_VAR
						&& Parent != nullptr
						&& Parent->kind == asAST_DECL_TRANSLATION_UNIT
						&& Target->hasConstantValue)
					{
						const FAngelscriptTypedASTJITGlobalDependency* const
							Dependency = FindExactCanonicalGlobalDependency(
								GlobalDependencies, *Target);
						FString DependencyError;
						if (Dependency == nullptr
							|| !ValidateAngelscriptTypedASTJITFoldedGlobalDependency(
								GlobalDependencies,
								Dependency->EngineLocalGlobalPropertyId,
								&DependencyError))
						{
							return Fail(Dependency == nullptr
								? TEXT("Canonical folded global has no unique exact hard-value dependency")
								: DependencyError);
						}
						if (!TrySpellCanonicalScalarCppType(
							Context, Expr->type, CppType))
						{
							return Fail(TEXT(
								"Canonical folded global type has no reviewed C++ spelling"));
						}
						bRequiresScalarOps = true;
						Out = FString::Printf(TEXT(
							"AngelscriptTypedASTJIT::FromCanonicalBits<%s>(UINT64_C(%llu))"),
							*CppType,
							static_cast<unsigned long long>(
								Target->constantValue));
						return true;
					}
				}
				Out = FString::Printf(
					TEXT("as_ast_d%u"), Expr->resolvedDecl.value);
				return true;
			case asAST_EXPR_CONSTRUCT:
			case asAST_EXPR_MATERIALIZE_TEMPORARY:
			case asAST_EXPR_CLEANUP:
				if (Expr->children.GetLength() != 1)
				{
					return Fail(TEXT(
						"Canonical scalar lifetime wrapper must own exactly one value"));
				}
				return EmitExpression(Expr->children[0], Depth, Out);
			case asAST_EXPR_UNARY:
			{
				FString Inner;
				if (Expr->children.GetLength() != 1
					|| !EmitExpression(Expr->children[0], Depth, Inner))
				{
					return false;
				}
				const FString Operator =
					UTF8_TO_TCHAR(Expr->literal.AddressOf());
				FCanonicalScalarCppType OperandType;
				if (!TryDescribeCanonicalScalarCppType(
					Context, Expr->type, OperandType))
				{
					return Fail(TEXT(
						"Canonical unary result has no reviewed C++ spelling"));
				}

				const TCHAR* MutationHelper = nullptr;
				if (OperandType.bInteger)
				{
					if (Operator == TEXT("pre++"))
					{
						MutationHelper = TEXT("WrapPreIncrement");
					}
					else if (Operator == TEXT("post++"))
					{
						MutationHelper = TEXT("WrapPostIncrement");
					}
					else if (Operator == TEXT("pre--"))
					{
						MutationHelper = TEXT("WrapPreDecrement");
					}
					else if (Operator == TEXT("post--"))
					{
						MutationHelper = TEXT("WrapPostDecrement");
					}
				}
				if (MutationHelper != nullptr)
				{
					bRequiresScalarOps = true;
					Out = FString::Printf(TEXT(
						"AngelscriptTypedASTJIT::%s<%s>(%s)"),
						MutationHelper, *OperandType.Spelling, *Inner);
					return true;
				}
				if (Operator == TEXT("pre++"))
				{
					Out = FString::Printf(TEXT("(++%s)"), *Inner);
					return true;
				}
				if (Operator == TEXT("post++"))
				{
					Out = FString::Printf(TEXT("(%s++)"), *Inner);
					return true;
				}
				if (Operator == TEXT("pre--"))
				{
					Out = FString::Printf(TEXT("(--%s)"), *Inner);
					return true;
				}
				if (Operator == TEXT("post--"))
				{
					Out = FString::Printf(TEXT("(%s--)"), *Inner);
					return true;
				}
				if (OperandType.bInteger
					&& (Operator == TEXT("-") || Operator == TEXT("~")))
				{
					bRequiresScalarOps = true;
					Out = FString::Printf(TEXT(
						"AngelscriptTypedASTJIT::%s<%s>(%s)"),
						Operator == TEXT("-") ? TEXT("WrapNegate")
							: TEXT("BitwiseNot"),
						*OperandType.Spelling, *Inner);
					return true;
				}
				Out = FString::Printf(
					TEXT("(%s%s)"), *Operator, *Inner);
				return true;
			}
			case asAST_EXPR_BINARY:
			case asAST_EXPR_LOGICAL:
			{
				FString Left;
				FString Right;
				if (Expr->children.GetLength() != 2
					|| !EmitExpression(Expr->children[0], Depth, Left)
					|| !EmitExpression(Expr->children[1], Depth, Right))
				{
					return false;
				}
				const FString Operator =
					UTF8_TO_TCHAR(Expr->literal.AddressOf());
				if (Expr->kind == asAST_EXPR_LOGICAL)
				{
					Out = FString::Printf(TEXT("(%s %s %s)"),
						*Left, *Operator, *Right);
					return true;
				}

				const asCExpr* const LeftExpr =
					Context.GetExpr(Expr->children[0]);
				const asCExpr* const RightExpr =
					Context.GetExpr(Expr->children[1]);
				FCanonicalScalarCppType ResultType;
				FCanonicalScalarCppType LeftType;
				FCanonicalScalarCppType RightType;
				if (LeftExpr == nullptr || RightExpr == nullptr
					|| !TryDescribeCanonicalScalarCppType(
						Context, Expr->type, ResultType)
					|| !TryDescribeCanonicalScalarCppType(
						Context, LeftExpr->type, LeftType)
					|| !TryDescribeCanonicalScalarCppType(
						Context, RightExpr->type, RightType))
				{
					return Fail(TEXT(
						"Canonical binary operand or result has no reviewed C++ spelling"));
				}

				const FString LeftTemporary = FString::Printf(
					TEXT("as_ast_e%u_left"), Id.value);
				const FString RightTemporary = FString::Printf(
					TEXT("as_ast_e%u_right"), Id.value);
				FString Operation;
				bool bOperationCanRaise = false;
				if (Operator == TEXT("**"))
				{
					if (ResultType.bInteger)
					{
						return Fail(TEXT(
							"Canonical integer power is outside the reviewed scalar profile"));
					}
					bRequiresScalarOps = true;
					bRequiresExecutionState = true;
					bOperationCanRaise = true;
					Operation = FString::Printf(TEXT(
						"AngelscriptTypedASTJIT::CheckedPower<%s>(Execution, %s, %s)"),
						*ResultType.Spelling,
						*LeftTemporary, *RightTemporary);
				}
				else
				{
					const TCHAR* Helper = nullptr;
					if (ResultType.bInteger && Operator == TEXT("+"))
					{
						Helper = TEXT("WrapAdd");
					}
					else if (ResultType.bInteger && Operator == TEXT("-"))
					{
						Helper = TEXT("WrapSubtract");
					}
					else if (ResultType.bInteger && Operator == TEXT("*"))
					{
						Helper = TEXT("WrapMultiply");
					}
					else if (ResultType.bInteger && Operator == TEXT("/"))
					{
						Helper = TEXT("CheckedDivide");
					}
					else if (ResultType.bInteger && Operator == TEXT("%"))
					{
						Helper = TEXT("CheckedRemainder");
					}
					else if (ResultType.bInteger && Operator == TEXT("<<"))
					{
						Helper = TEXT("ShiftLeft");
					}
					else if (ResultType.bInteger && Operator == TEXT(">>"))
					{
						Helper = TEXT("LogicalShiftRight");
					}
					else if (ResultType.bInteger && Operator == TEXT(">>>"))
					{
						Helper = TEXT("ArithmeticShiftRight");
					}

					if (Helper != nullptr)
					{
						bRequiresScalarOps = true;
						const bool bChecked = Operator == TEXT("/")
							|| Operator == TEXT("%");
						bRequiresExecutionState |= bChecked;
						bOperationCanRaise |= bChecked;
						Operation = bChecked
							? FString::Printf(TEXT(
								"AngelscriptTypedASTJIT::%s<%s>(Execution, %s, %s)"),
								Helper, *ResultType.Spelling,
								*LeftTemporary, *RightTemporary)
							: FString::Printf(TEXT(
								"AngelscriptTypedASTJIT::%s<%s>(%s, %s)"),
								Helper, *ResultType.Spelling,
								*LeftTemporary, *RightTemporary);
					}
					else
					{
						Operation = FString::Printf(TEXT("(%s %s %s)"),
							*LeftTemporary, *Operator, *RightTemporary);
					}
				}

				const FString Prefix = MakeIndent(Depth);
				const FString BodyPrefix = MakeIndent(Depth + 1);
				Out = FString::Printf(TEXT(
					"([&]() -> %s\n%s{\n"),
					*ResultType.Spelling, *Prefix);
				if (bOperationCanRaise)
				{
					AppendSourcePosition(Expr->range, Depth + 1, Out);
				}
				Out += FString::Printf(TEXT("%sconst %s %s = %s;\n"),
					*BodyPrefix, *LeftType.Spelling,
					*LeftTemporary, *Left);
				if (bRequiresExecutionState)
				{
					Out += BodyPrefix
						+ TEXT("if (UNLIKELY(Execution.bExceptionThrown))\n")
						+ BodyPrefix + TEXT("{\n")
						+ BodyPrefix + TEXT("\treturn ")
						+ ResultType.Spelling + TEXT("{};\n")
						+ BodyPrefix + TEXT("}\n");
				}
				Out += FString::Printf(TEXT("%sconst %s %s = %s;\n"),
					*BodyPrefix, *RightType.Spelling,
					*RightTemporary, *Right);
				if (bRequiresExecutionState)
				{
					Out += BodyPrefix
						+ TEXT("if (UNLIKELY(Execution.bExceptionThrown))\n")
						+ BodyPrefix + TEXT("{\n")
						+ BodyPrefix + TEXT("\treturn ")
						+ ResultType.Spelling + TEXT("{};\n")
						+ BodyPrefix + TEXT("}\n");
				}
				Out += FString::Printf(TEXT("%sreturn %s;\n%s}())"),
					*BodyPrefix, *Operation, *Prefix);
				return true;
			}
			case asAST_EXPR_CONVERSION:
			{
				FString Inner;
				const asCExpr* const Source =
					Expr->children.GetLength() == 1
						? Context.GetExpr(Expr->children[0]) : nullptr;
				FCanonicalScalarCppType SourceType;
				FCanonicalScalarCppType DestinationType;
				if (Expr->children.GetLength() != 1
					|| !EmitExpression(Expr->children[0], Depth, Inner)
					|| Source == nullptr
					|| !TryDescribeCanonicalScalarCppType(
						Context, Source->type, SourceType)
					|| !TryDescribeCanonicalScalarCppType(
						Context, Expr->type, DestinationType))
				{
					return false;
				}
				if (SourceType.bInteger
					&& DestinationType.bInteger
					&& SourceType.SizeInBytes > DestinationType.SizeInBytes)
				{
					bRequiresScalarOps = true;
					Out = FString::Printf(TEXT(
						"AngelscriptTypedASTJIT::WrapNarrow<%s>(%s)"),
						*DestinationType.Spelling, *Inner);
				}
				else if (SourceType.bInteger
					&& DestinationType.bInteger
					&& SourceType.bUnsigned != DestinationType.bUnsigned)
				{
					bRequiresScalarOps = true;
					Out = FString::Printf(TEXT(
						"AngelscriptTypedASTJIT::ConvertInteger<%s>(%s)"),
						*DestinationType.Spelling, *Inner);
				}
				else
				{
					Out = FString::Printf(
						TEXT("static_cast<%s>(%s)"),
						*DestinationType.Spelling, *Inner);
				}
				return true;
			}
			case asAST_EXPR_ASSIGN:
			{
				FString Left;
				FString Right;
				if (Expr->children.GetLength() != 2
					|| !EmitExpression(Expr->children[0], Depth, Left)
					|| !EmitExpression(Expr->children[1], Depth, Right)
					|| !TrySpellCanonicalScalarCppType(
						Context, Expr->type, CppType))
				{
					return false;
				}
				const FString Operator = UTF8_TO_TCHAR(
					Expr->literal.AddressOf() != nullptr
						? Expr->literal.AddressOf() : "=");
				const TCHAR* Helper = nullptr;
				if (Operator == TEXT("="))
				{
					// Plain assignment follows the same one-value/one-target protocol
					// as a compound mutation below.
				}
				else if (Operator == TEXT("+="))
				{
					Helper = TEXT("WrapAdd");
				}
				else if (Operator == TEXT("-="))
				{
					Helper = TEXT("WrapSubtract");
				}
				else if (Operator == TEXT("*="))
				{
					Helper = TEXT("WrapMultiply");
				}
				if (Operator != TEXT("=") && Helper == nullptr)
				{
					return Fail(FString::Printf(TEXT(
						"Canonical compound assignment operator %s has no reviewed scalar helper."),
						*Operator));
				}

				// Preserve the same mutation protocol as the former typed-semantic
				// emitter: evaluate the RHS once, materialize the lvalue once, then
				// perform one wrapping store and return the stored value.
				bRequiresScalarOps |= Helper != nullptr;
				const FString Prefix = MakeIndent(Depth);
				const FString BodyPrefix = MakeIndent(Depth + 1);
				const FString ValueTemporary = FString::Printf(
					TEXT("as_ast_e%u_value"), Id.value);
				const FString TargetTemporary = FString::Printf(
					TEXT("as_ast_e%u_target"), Id.value);
				Out = FString::Printf(
					TEXT("([&]() -> %s\n%s{\n")
					TEXT("%sconst %s %s = %s;\n"),
					*CppType, *Prefix,
					*BodyPrefix, *CppType, *ValueTemporary, *Right);
				if (bRequiresExecutionState)
				{
					Out += BodyPrefix
						+ TEXT("if (UNLIKELY(Execution.bExceptionThrown))\n")
						+ BodyPrefix + TEXT("{\n")
						+ BodyPrefix + TEXT("\treturn ") + CppType
						+ TEXT("{};\n") + BodyPrefix + TEXT("}\n");
				}
				Out += FString::Printf(
					TEXT("%s%s& %s = %s;\n"),
					*BodyPrefix, *CppType, *TargetTemporary, *Left);
				if (Operator == TEXT("="))
				{
					Out += FString::Printf(TEXT("%s%s = %s;\n"),
						*BodyPrefix, *TargetTemporary, *ValueTemporary);
				}
				else
				{
					Out += FString::Printf(
						TEXT("%s%s = AngelscriptTypedASTJIT::%s<%s>(%s, %s);\n"),
						*BodyPrefix, *TargetTemporary, Helper, *CppType,
						*TargetTemporary, *ValueTemporary);
				}
				Out += FString::Printf(TEXT("%sreturn %s;\n%s}())"),
					*BodyPrefix, *TargetTemporary, *Prefix);
				return true;
			}
			case asAST_EXPR_SEQUENCE:
			{
				if (Expr->children.GetLength() == 0)
				{
					return Fail(TEXT("Sequence expression is empty."));
				}
				Out = TEXT("(");
				for (asUINT Index = 0; Index < Expr->children.GetLength(); ++Index)
				{
					FString Part;
					if (!EmitExpression(Expr->children[Index], Depth, Part))
					{
						return false;
					}
					if (Index != 0)
					{
						Out += TEXT(", ");
					}
					Out += Part;
				}
				Out += TEXT(")");
				return true;
			}
			case asAST_EXPR_CALL:
			{
				const FAngelscriptTypedASTJITNativeCallEmissionPlan* Native =
					NativeCalls.FindByPredicate(
						[Id](const FAngelscriptTypedASTJITNativeCallEmissionPlan& Plan)
						{
							return Plan.Expression == Id;
						});
				const FAngelscriptTypedASTJITScriptCallEmissionPlan* Script =
					ScriptCalls.FindByPredicate(
						[Id](const FAngelscriptTypedASTJITScriptCallEmissionPlan& Plan)
						{
							return Plan.Expression == Id;
						});
				if ((Native == nullptr) == (Script == nullptr))
				{
					return Fail(TEXT(
						"Resolved call requires exactly one exact native or direct script-helper emission plan"));
				}
				if (Expr->callDispatch != asAST_CALL_DISPATCH_DIRECT)
				{
					return Fail(FString::Printf(TEXT(
						"Canonical call emission only supports a sealed direct dispatch: Call=%u Dispatch=%u"),
						Id.value, static_cast<uint32>(Expr->callDispatch)));
				}
				if (Expr->callArguments.GetLength()
					!= Expr->children.GetLength())
				{
					return Fail(FString::Printf(TEXT(
						"Canonical call emission requires one sealed formal relation per argument: Call=%u Arguments=%u Relations=%u"),
						Id.value,
						Expr->children.GetLength(),
						Expr->callArguments.GetLength()));
				}
				const bool bScriptCall = Script != nullptr;
				const bool bBoundNative = Native != nullptr
					&& Native->Route
						!= EAngelscriptTypedASTJITCallEmissionRoute::Direct;
				const bool bDirectNative = Native != nullptr && !bBoundNative;
				if (bDirectNative
					&& (Native->ReviewedCppReturnType.IsEmpty()
						|| Native->ReviewedCppParameterTypes.Num()
							!= static_cast<int32>(Expr->children.GetLength())))
				{
					return Fail(FString::Printf(TEXT(
						"Canonical direct native call requires one validated reviewed C++ formal per sealed argument: Call=%u Arguments=%u ReviewedFormals=%d Return=%s"),
						Id.value,
						Expr->children.GetLength(),
						Native->ReviewedCppParameterTypes.Num(),
						Native->ReviewedCppReturnType.IsEmpty()
							? TEXT("<missing>")
							: *Native->ReviewedCppReturnType));
				}
				if (bBoundNative)
				{
					RequiredIncludes.AddUnique(TEXT(
						"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"));
				}
				else if (Native != nullptr)
				{
					RequiredIncludes.AddUnique(Native->Include);
				}
				else
				{
					RequiredIncludes.AddUnique(TEXT("StaticJIT/StaticJITHeader.h"));
				}

				if (bDirectNative
					&& Expr->children.GetLength() == 0)
				{
					if (!RecordNativeCallSite(Id, *Native, Native->Symbol))
					{
						return false;
					}
					Out = Native->Symbol + TEXT("()");
					return true;
				}

				FString ReturnType;
				if (bDirectNative)
				{
					ReturnType = Native->ReviewedCppReturnType;
				}
				else if (!TrySpellCanonicalScalarCppType(
					Context, Expr->type, ReturnType))
				{
					return Fail(TEXT(
						"Canonical call return type has no reviewed C++ spelling"));
				}
				const FString Prefix = MakeIndent(Depth);
				const FString BodyPrefix = MakeIndent(Depth + 1);
				Out = FString::Printf(TEXT("([&]() -> %s\n%s{\n"),
					*ReturnType, *Prefix);
				TArray<FString> FormalTemporaries;
				TArray<FString> FormalTypes;
				FormalTemporaries.SetNum(Expr->children.GetLength());
				FormalTypes.SetNum(Expr->children.GetLength());
				for (asUINT EvaluationIndex = 0;
					EvaluationIndex < Expr->children.GetLength();
					++EvaluationIndex)
				{
					const asSASTCallArgument& Argument =
						Expr->callArguments[EvaluationIndex];
					const asASTExprId OperandId =
						Expr->children[EvaluationIndex];
					const asUINT FormalIndex = Argument.formalIndex;
					if (Argument.expression != OperandId)
					{
						return Fail(FString::Printf(TEXT(
							"Canonical call formal relation does not own its stored argument: Call=%u Evaluation=%u Child=%u Relation=%u"),
							Id.value, EvaluationIndex, OperandId.value,
							Argument.expression.value));
					}
					if (FormalIndex >= Expr->children.GetLength())
					{
						return Fail(FString::Printf(TEXT(
							"Canonical call formal relation is out of range: Call=%u Evaluation=%u Formal=%u Count=%u"),
							Id.value, EvaluationIndex, FormalIndex,
							Expr->children.GetLength()));
					}
					if (!FormalTemporaries[FormalIndex].IsEmpty())
					{
						return Fail(FString::Printf(TEXT(
							"Canonical call formal relation is duplicated: Call=%u Evaluation=%u Formal=%u"),
							Id.value, EvaluationIndex, FormalIndex));
					}
					const asCExpr* const Operand = Context.GetExpr(OperandId);
					FString OperandText;
					FString OperandType;
					if (Operand == nullptr)
					{
						return Fail(FString::Printf(TEXT(
							"Canonical call argument is dangling: Call=%u Operand=%u Formal=%u"),
							Id.value, OperandId.value, FormalIndex));
					}
					if (!EmitExpression(OperandId, Depth + 1, OperandText))
					{
						return false;
					}
					if (bDirectNative)
					{
						OperandType =
							Native->ReviewedCppParameterTypes[FormalIndex];
					}
					else if (!TrySpellCanonicalCallArgumentCppType(
						Context, Argument.formalType, OperandType))
					{
						const asCType* const FormalCanonicalType =
							Context.GetType(Argument.formalType.type);
						return Fail(FString::Printf(TEXT(
							"Canonical call formal type has no reviewed C++ spelling: Call=%u Operand=%u Kind=%u Formal=%u Type=%s Quals=0x%08x"),
							Id.value,
							OperandId.value,
							static_cast<uint32>(Operand->kind),
							FormalIndex,
							FormalCanonicalType != nullptr
								? UTF8_TO_TCHAR(
									FormalCanonicalType->stableKey.AddressOf())
								: TEXT("<missing>"),
							Argument.formalType.quals));
					}
					FormalTypes[FormalIndex] = OperandType;
					FormalTemporaries[FormalIndex] = FString::Printf(
						TEXT("as_ast_e%u_arg%u"), Id.value, FormalIndex);
					Out += FString::Printf(TEXT("%s%s%s %s = %s;\n"),
						*BodyPrefix,
						bDirectNative ? TEXT("") : TEXT("const "),
						*OperandType,
						*FormalTemporaries[FormalIndex],
						*OperandText);
				}
				for (int32 FormalIndex = 0;
					FormalIndex < FormalTemporaries.Num(); ++FormalIndex)
				{
					if (FormalTemporaries[FormalIndex].IsEmpty())
					{
						return Fail(FString::Printf(TEXT(
							"Canonical call formal relation is incomplete: Call=%u Formal=%d"),
							Id.value, FormalIndex));
					}
				}

				FString Invocation;
				FString EmittedCppCallee;
				if (bScriptCall)
				{
					EmittedCppCallee = Script->Symbol;
					Invocation = Script->Symbol + TEXT("(Execution");
					if (!FormalTemporaries.IsEmpty())
					{
						Invocation += TEXT(", ")
							+ FString::Join(FormalTemporaries, TEXT(", "));
					}
					Invocation += TEXT(")");
				}
				else if (!bBoundNative)
				{
					EmittedCppCallee = Native->Symbol;
					Invocation = Native->Symbol + TEXT("(")
						+ FString::Join(FormalTemporaries, TEXT(", "))
						+ TEXT(")");
					if (!RecordNativeCallSite(Id, *Native, EmittedCppCallee))
					{
						return false;
					}
				}
				else
				{
					TArray<FString> TemplateTypes;
					TemplateTypes.Add(ReturnType);
					TemplateTypes.Append(FormalTypes);
					const TCHAR* const BoundInvocation = Native->Route
						== EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding
						? TEXT("InvokeBoundNative") : TEXT("InvokeBound");
					EmittedCppCallee = FString::Printf(
						TEXT("AngelscriptTypedASTJIT::%s<%s>"),
						BoundInvocation,
						*FString::Join(TemplateTypes, TEXT(", ")));
					Invocation = EmittedCppCallee + TEXT("(Execution, ")
						+ Native->CallSiteSymbol;
					if (!FormalTemporaries.IsEmpty())
					{
						Invocation += TEXT(", ")
							+ FString::Join(FormalTemporaries, TEXT(", "));
					}
					Invocation += TEXT(")");
					if (!RecordNativeCallSite(Id, *Native, EmittedCppCallee))
					{
						return false;
					}
				}

				if (IsCanonicalVoidType(Context, Expr->type))
				{
					Out += BodyPrefix + Invocation + TEXT(";\n");
					if (bScriptCall || bBoundNative)
					{
						Out += BodyPrefix
							+ TEXT("if (UNLIKELY(Execution.bExceptionThrown))\n")
							+ BodyPrefix + TEXT("{\n")
							+ BodyPrefix + TEXT("\treturn;\n")
							+ BodyPrefix + TEXT("}\n");
					}
					Out += BodyPrefix + TEXT("return;\n") + Prefix + TEXT("}())");
					return true;
				}

				Out += FString::Printf(TEXT("%sconst %s as_ast_e%u_result = %s;\n"),
					*BodyPrefix, *ReturnType, Id.value, *Invocation);
				if (bScriptCall || bBoundNative)
				{
					Out += BodyPrefix
						+ TEXT("if (UNLIKELY(Execution.bExceptionThrown))\n")
						+ BodyPrefix + TEXT("{\n")
						+ BodyPrefix + TEXT("\treturn ") + ReturnType
						+ TEXT("{};\n") + BodyPrefix + TEXT("}\n");
				}
				Out += FString::Printf(TEXT("%sreturn as_ast_e%u_result;\n%s}())"),
					*BodyPrefix, Id.value, *Prefix);
				return true;
			}
			case asAST_EXPR_CONDITIONAL:
			{
				FString Cond;
				FString ThenExpr;
				FString ElseExpr;
				if (Expr->children.GetLength() != 3
					|| !EmitExpression(Expr->children[0], Depth, Cond)
					|| !EmitExpression(Expr->children[1], Depth, ThenExpr)
					|| !EmitExpression(Expr->children[2], Depth, ElseExpr))
				{
					return false;
				}
				Out = FString::Printf(
					TEXT("(%s ? %s : %s)"), *Cond, *ThenExpr, *ElseExpr);
				return true;
			}
			default:
				return Fail(TEXT("Expression kind is outside the reviewed scalar profile."));
			}
		}

		bool EmitStatement(asASTStmtId Id, const int32 Depth, FString& Out)
		{
			const asCStmt* Stmt = Context.GetStmt(Id);
			if (Stmt == nullptr)
			{
				return Fail(TEXT("Canonical statement ID is dangling."));
			}
			TGuardValue<const asCSourceRange*> StatementRangeGuard(
				CurrentStatementRange, &Stmt->range);
			const FString Prefix = MakeIndent(Depth);
			switch (Stmt->kind)
			{
			case asAST_STMT_BLOCK:
				Out += Prefix + TEXT("{\n");
				for (asUINT Index = 0; Index < Stmt->children.GetLength(); ++Index)
				{
					if (!EmitStatement(Stmt->children[Index], Depth + 1, Out))
					{
						return false;
					}
				}
				Out += Prefix + TEXT("}\n");
				return true;
			case asAST_STMT_EXPR:
			{
				const FString BodyPrefix = MakeIndent(Depth + 1);
				Out += Prefix + TEXT("{\n");
				// Position hooks are executable statements. Keep them inside the
				// statement's structural block so an expression used as an if/loop
				// body remains one valid C++ statement.
				AppendSourcePosition(Stmt->range, Depth + 1, Out);
				FString Value;
				if (!EmitExpression(Stmt->expr, Depth + 1, Value))
				{
					return false;
				}
				Out += BodyPrefix + Value + TEXT(";\n");
				AppendExceptionReturn(Depth + 1, Out);
				Out += Prefix + TEXT("}\n");
				return true;
			}
			case asAST_STMT_RETURN:
			{
				const FString BodyPrefix = MakeIndent(Depth + 1);
				if (bEmitFramePosition)
				{
					Out += Prefix + TEXT("{\n");
					AppendSourcePosition(Stmt->range, Depth + 1, Out);
				}
				const FString& ReturnPrefix = bEmitFramePosition
					? BodyPrefix : Prefix;
				if (!Stmt->expr.IsValid())
				{
					Out += ReturnPrefix + TEXT("return;\n");
					if (bEmitFramePosition)
					{
						Out += Prefix + TEXT("}\n");
					}
					return true;
				}
				FString Value;
				if (!EmitExpression(Stmt->expr, Depth, Value))
				{
					return false;
				}
				Out += ReturnPrefix + TEXT("return ") + Value + TEXT(";\n");
				if (bEmitFramePosition)
				{
					Out += Prefix + TEXT("}\n");
				}
				return true;
			}
			case asAST_STMT_IF:
			{
				const FString BodyPrefix = MakeIndent(Depth + 1);
				Out += Prefix + TEXT("{\n");
				AppendSourcePosition(Stmt->range, Depth + 1, Out);
				FString Cond;
				if (!EmitExpression(Stmt->expr, Depth + 1, Cond))
				{
					return false;
				}
				FString Condition = Cond;
				if (bRequiresExecutionState)
				{
					Condition = FString::Printf(
						TEXT("as_ast_stmt_%u_condition"), Id.value);
					Out += BodyPrefix + TEXT("const bool ") + Condition
						+ TEXT(" = ") + Cond + TEXT(";\n");
					AppendExceptionReturn(Depth + 1, Out);
				}
				Out += BodyPrefix + TEXT("if (") + Condition + TEXT(")\n");
				if (Stmt->children.GetLength() > 0
					&& !EmitStatement(Stmt->children[0], Depth + 1, Out))
				{
					return false;
				}
				if (Stmt->children.GetLength() > 1)
				{
					Out += BodyPrefix + TEXT("else\n");
					if (!EmitStatement(Stmt->children[1], Depth + 1, Out))
					{
						return false;
					}
				}
				Out += Prefix + TEXT("}\n");
				return true;
			}
			case asAST_STMT_WHILE:
			case asAST_STMT_DO_WHILE:
			{
				const FString BodyPrefix = MakeIndent(Depth + 1);
				Out += Prefix + TEXT("{\n");
				AppendSourcePosition(Stmt->range, Depth + 1, Out);
				FString Cond;
				if (!EmitExpression(Stmt->expr, Depth + 1, Cond))
				{
					return false;
				}
				if (Stmt->kind == asAST_STMT_WHILE)
				{
					Out += BodyPrefix + TEXT("while (") + Cond + TEXT(")\n");
					if (Stmt->children.GetLength() > 0
						&& !EmitStatement(Stmt->children[0], Depth + 1, Out))
					{
						return false;
					}
					AppendExceptionReturn(Depth + 1, Out);
					Out += Prefix + TEXT("}\n");
					return true;
				}
				Out += BodyPrefix + TEXT("do\n");
				if (Stmt->children.GetLength() > 0
					&& !EmitStatement(Stmt->children[0], Depth + 1, Out))
				{
					return false;
				}
				Out += BodyPrefix + TEXT("while (") + Cond + TEXT(");\n");
				AppendExceptionReturn(Depth + 1, Out);
				Out += Prefix + TEXT("}\n");
				return true;
			}
			case asAST_STMT_FOR:
			{
				if (Stmt->children.GetLength() < 2
					|| Stmt->children.GetLength() > 3)
				{
					return Fail(TEXT(
						"Canonical ForStmt must retain [initializer, body, optional increment]"));
				}
				FString Cond;
				if (Stmt->expr.IsValid()
					&& !EmitExpression(Stmt->expr, Depth, Cond))
				{
					return false;
				}
				FString Increment;
				if (Stmt->children.GetLength() == 3)
				{
					const asCStmt* const IncrementStmt =
						Context.GetStmt(Stmt->children[2]);
					if (IncrementStmt == nullptr
						|| IncrementStmt->kind != asAST_STMT_EXPR
						|| !IncrementStmt->expr.IsValid()
						|| !EmitExpression(
							IncrementStmt->expr, Depth + 1, Increment))
					{
						return Fail(TEXT(
							"Canonical ForStmt increment phase must be an expression statement"));
					}
				}

				Out += Prefix + TEXT("{\n");
				AppendSourcePosition(Stmt->range, Depth + 1, Out);
				if (Stmt->children[0].IsValid())
				{
					const asCStmt* const Initializer =
						Context.GetStmt(Stmt->children[0]);
					if (Initializer == nullptr)
					{
						return Fail(TEXT(
							"Canonical ForStmt initializer is dangling"));
					}
					// Parser/Sema may retain the initializer phase as a synthetic
					// BlockStmt. The C++ loop scope above already represents that
					// lifetime, so emitting the synthetic braces would destroy its
					// local declarations before the condition and increment use them.
					if (Initializer->kind == asAST_STMT_BLOCK)
					{
						for (asUINT InitializerIndex = 0;
							InitializerIndex < Initializer->children.GetLength();
							++InitializerIndex)
						{
							if (!EmitStatement(
								Initializer->children[InitializerIndex],
								Depth + 1, Out))
							{
								return false;
							}
						}
					}
					else if (!EmitStatement(
						Stmt->children[0], Depth + 1, Out))
					{
						return false;
					}
				}
				const FString LoopPrefix = MakeIndent(Depth + 1);
				Out += LoopPrefix + TEXT("for ( ; ") + Cond
					+ TEXT(" ; ") + Increment + TEXT(" )\n");
				if (!EmitStatement(Stmt->children[1], Depth + 1, Out))
				{
					return false;
				}
				AppendExceptionReturn(Depth + 1, Out);
				Out += Prefix + TEXT("}\n");
				return true;
			}
			case asAST_STMT_BREAK:
				if (bEmitFramePosition)
				{
					Out += Prefix + TEXT("{\n");
					AppendSourcePosition(Stmt->range, Depth + 1, Out);
					Out += MakeIndent(Depth + 1) + TEXT("break;\n")
						+ Prefix + TEXT("}\n");
				}
				else
				{
					Out += Prefix + TEXT("break;\n");
				}
				return true;
			case asAST_STMT_CONTINUE:
				if (bEmitFramePosition)
				{
					Out += Prefix + TEXT("{\n");
					AppendSourcePosition(Stmt->range, Depth + 1, Out);
					Out += MakeIndent(Depth + 1) + TEXT("continue;\n")
						+ Prefix + TEXT("}\n");
				}
				else
				{
					Out += Prefix + TEXT("continue;\n");
				}
				return true;
			case asAST_STMT_FALLTHROUGH:
				return true;
			case asAST_STMT_SWITCH:
			{
				const bool bHasInvalidEnumValueEdge =
					Stmt->safePointRole
						== asAST_SAFEPOINT_SWITCH_INVALID_VALUE;
				if (bHasInvalidEnumValueEdge)
				{
					bRequiresExecutionState = true;
				}
				const FString BodyPrefix = MakeIndent(Depth + 1);
				Out += Prefix + TEXT("{\n");
				AppendSourcePosition(Stmt->range, Depth + 1, Out);
				FString Cond;
				if (!EmitExpression(Stmt->expr, Depth + 1, Cond))
				{
					return false;
				}
				FString Condition = Cond;
				if (bRequiresExecutionState)
				{
					Condition = FString::Printf(
						TEXT("as_ast_stmt_%u_condition"), Id.value);
					Out += BodyPrefix + TEXT("const auto ") + Condition
						+ TEXT(" = ") + Cond + TEXT(";\n");
					AppendExceptionReturn(Depth + 1, Out);
				}
				Out += BodyPrefix + TEXT("switch (") + Condition + TEXT(")\n")
					+ BodyPrefix + TEXT("{\n");
				for (asUINT Index = 0; Index < Stmt->children.GetLength(); ++Index)
				{
					if (!EmitStatement(Stmt->children[Index], Depth + 2, Out))
					{
						return false;
					}
				}
				if (bHasInvalidEnumValueEdge)
				{
					const FString CasePrefix = MakeIndent(Depth + 2);
					const FString FailurePrefix = MakeIndent(Depth + 3);
					// The VM compiler appends the synthesized invalid-enum throw
					// after compiling all case bodies. Its current source position is
					// therefore the last top-level case statement that emitted a
					// LineInstr, not the switch token itself. Derive that exact
					// diagnostic position from the sealed Canonical children so the
					// generated native route publishes the same failure payload
					// without consulting legacy parser/compiler nodes.
					const asCSourceRange* InvalidValueRange = &Stmt->range;
					for (asUINT CaseIndex = Stmt->children.GetLength();
						CaseIndex > 0; --CaseIndex)
					{
						const asCStmt* const CaseStmt = Context.GetStmt(
							Stmt->children[CaseIndex - 1]);
						if (CaseStmt == nullptr
							|| CaseStmt->kind != asAST_STMT_CASE
							|| CaseStmt->children.GetLength() == 0)
						{
							continue;
						}
						const asCStmt* const TailStatement = Context.GetStmt(
							CaseStmt->children[
								CaseStmt->children.GetLength() - 1]);
						if (TailStatement != nullptr)
						{
							InvalidValueRange = &TailStatement->range;
							break;
						}
					}
					Out += CasePrefix + TEXT("default:\n");
					{
						TGuardValue<const asCSourceRange*> FailureRangeGuard(
							CurrentStatementRange, InvalidValueRange);
						AppendSourcePosition(*InvalidValueRange, Depth + 3, Out);
					}
					Out += FailurePrefix
						+ TEXT("FStaticJITFunction::SetSwitchValueInvalidException(Execution);\n")
						+ FailurePrefix
						+ (ReturnCppType == TEXT("void")
							? TEXT("return;\n") : TEXT("return {};\n"));
				}
				Out += BodyPrefix + TEXT("}\n");
				AppendExceptionReturn(Depth + 1, Out);
				Out += Prefix + TEXT("}\n");
				return true;
			}
			case asAST_STMT_CASE:
			{
				if (Stmt->expr.IsValid())
				{
					FString Value;
					if (!EmitExpression(Stmt->expr, Depth, Value))
					{
						return false;
					}
					Out += Prefix + TEXT("case ") + Value + TEXT(":\n");
				}
				else
				{
					Out += Prefix + TEXT("default:\n");
				}
				for (asUINT Index = 0; Index < Stmt->children.GetLength(); ++Index)
				{
					if (!EmitStatement(Stmt->children[Index], Depth + 1, Out))
					{
						return false;
					}
				}
				return true;
			}
			case asAST_STMT_DECL:
			{
				const asCDecl* Decl = Context.GetDecl(Stmt->decl);
				if (Decl == nullptr)
				{
					return Fail(TEXT("Declaration statement is missing its decl."));
				}
				FString CppType;
				if (!TrySpellCanonicalScalarCppType(
					Context, Decl->type, CppType))
				{
					return Fail(TEXT("Local declaration type is outside the reviewed scalar set"));
				}
				Out += FString::Printf(
					TEXT("%s%s as_ast_d%u;\n"),
					*Prefix, *CppType, Decl->id.value);
				return true;
			}
			default:
				return Fail(TEXT("Statement kind is outside the reviewed scalar profile."));
			}
		}

	private:
		bool RecordNativeCallSite(
			const asASTExprId Expression,
			const FAngelscriptTypedASTJITNativeCallEmissionPlan& Plan,
			const FString& EmittedCppCallee)
		{
			FAngelscriptTypedASTJITNativeCallSiteMetadata Candidate;
			Candidate.Expression = Expression;
			Candidate.Route = Plan.Route
				== EAngelscriptTypedASTJITCallEmissionRoute::Bridge
					? EAngelscriptJITNativeCallSiteRoute::Bridge
					: Plan.NativeCallSiteRoute;
			Candidate.CanonicalDeclaration = Plan.CanonicalDeclaration;
			Candidate.EmittedCppCallee = EmittedCppCallee;
			Candidate.RegisteredTarget = Plan.RegisteredTarget.IsEmpty()
				? Plan.Symbol : Plan.RegisteredTarget;
			if (Candidate.Route == EAngelscriptJITNativeCallSiteRoute::Bridge
				|| Candidate.Route
					== EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding)
			{
				Candidate.RuntimeDllCore = Candidate.Route
					== EAngelscriptJITNativeCallSiteRoute::Bridge
						? TEXT("AngelscriptTypedASTJIT::InvokeBoundViaVM")
						: TEXT("AngelscriptTypedASTJIT::ResolveBoundNative");
				Candidate.MetadataRowSymbol = Plan.CallSiteSymbol;
				Candidate.StableTargetKey = Plan.StableTargetKey.Hash;
				Candidate.ExpectedAbi = Plan.ExpectedABI;
				Candidate.ReferenceSlotIndex = Plan.ReferenceSlotIndex;
			}
			NativeCallSites.Add(MoveTemp(Candidate));
			return true;
		}

		const asCASTContext& Context;
		TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls;
		TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan> ScriptCalls;
		TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
			GlobalDependencies;
		TArray<FString> RequiredIncludes;
		TArray<FAngelscriptTypedASTJITNativeCallSiteMetadata> NativeCallSites;
		FString FailureDetail;
		bool bRequiresScalarOps = false;
		bool bRequiresExecutionState = false;
		bool bEmitFramePosition = false;
		FString ReturnCppType;
		const asCSourceRange* CurrentStatementRange = nullptr;
	};
}

asASTDeclId FindAngelscriptTypedASTJITCanonicalFunctionDecl(
	const asCASTContext& Context,
	const char* Name)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	const asCDecl* Decl = FindFunctionDecl(Context, asASTDeclId(), Name);
	return Decl != nullptr ? Decl->id : asASTDeclId();
}

bool FunctionHasTypedASTJITCanonicalBody(
	const FAngelscriptStaticJITCompiledFunctionView& Function)
{
	if (Function.SealedAST == nullptr
		|| !Function.SealedAST->IsPublishable()
		|| Function.CanonicalFunctionDeclValue == 0)
	{
		return false;
	}
	return Function.SealedAST->GetDecl(
		asASTDeclId(Function.CanonicalFunctionDeclValue)) != nullptr;
}

FAngelscriptTypedASTJITEligibility
EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(
	const FAngelscriptTypedASTJITEligibilityInput& Input)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	if (Input.SealedAST == nullptr || !Input.SealedAST->IsPublishable())
	{
		return Reject(EFallback::MissingCanonicalAST, {},
			TEXT("No same-compilation sealed canonical AST is available."));
	}

	const asCDecl* FunctionDecl = FindFunctionDecl(
		*Input.SealedAST,
		Input.CanonicalFunctionDecl,
		nullptr);
	if (FunctionDecl == nullptr)
	{
		return Reject(EFallback::InvalidCanonicalAST, {},
			TEXT("CanonicalFunctionDecl / DeclId 0 must not bind the first function or walk the whole module."));
	}
	if (!Input.FrozenBodySignatureFailure.IsEmpty())
	{
		return Reject(
			EFallback::UnsupportedSignature,
			MakeSpan(*Input.SealedAST, FunctionDecl->range),
			Input.FrozenBodySignatureFailure);
	}
	if (FunctionDecl->kind == asAST_DECL_CONSTRUCTOR
		|| FunctionDecl->kind == asAST_DECL_DESTRUCTOR)
	{
		const FAngelscriptTypedASTJITCanonicalLifetimeFacts LifetimeFacts =
			AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
				*Input.SealedAST, Input.CanonicalFunctionDecl);
		if (!LifetimeFacts.LifetimeSummary.IsAuthenticated()
			|| LifetimeFacts.CleanupPlanState
				== EAngelscriptTypedASTJITCleanupPlanState::Unverified
			|| !LifetimeFacts.bCleanupPlanCoversAllTransfers)
		{
			return Reject(
				EFallback::InvalidCleanupPlan,
				MakeSpan(*Input.SealedAST, FunctionDecl->range),
				TEXT("CanonicalLifetimeProtocolAuthenticationFailed: the exact constructor/destructor has no complete authenticated lifetime summary."));
		}
		if (!LifetimeFacts.LifetimeSummary.bNativeObjectFrameABIAvailable)
		{
			const TCHAR* const Prefix = LifetimeFacts.CleanupPlanState
				== EAngelscriptTypedASTJITCleanupPlanState::PartialConstruction
				? TEXT("AuthenticatedConstructorPartialConstructionRequiresNativeObjectFrameABI")
				: TEXT("AuthenticatedConstructorDestructorLifetimeRequiresNativeObjectFrameABI");
			return Reject(
				EFallback::UnsupportedLifetime,
				MakeSpan(*Input.SealedAST, FunctionDecl->range),
				FString::Printf(TEXT(
					"%s: StableKey=%s ABIKey=%s Records=%u ExitPlans=%u ActionFlags=0x%08x ExitPlanFlags=0x%08x"),
					Prefix,
					*LifetimeFacts.LifetimeSummary.StableKey.ToHexString(),
					*LifetimeFacts.LifetimeSummary.ABIKey.ToHexString(),
					LifetimeFacts.LifetimeSummary.RecordCount,
					LifetimeFacts.LifetimeSummary.ExitPlanCount,
					static_cast<uint32>(
						LifetimeFacts.LifetimeSummary.ActionFlags),
					static_cast<uint32>(
						LifetimeFacts.LifetimeSummary.ExitPlanFlags)));
		}
	}
	if (FunctionDecl != nullptr
		&& (FunctionDecl->kind == asAST_DECL_CONSTRUCTOR
			|| FunctionDecl->kind == asAST_DECL_DESTRUCTOR
			|| FunctionDecl->kind == asAST_DECL_PROPERTY
			|| FunctionDecl->kind == asAST_DECL_MIXIN))
	{
		return Reject(EFallback::UnsupportedFunctionKind,
			MakeSpan(*Input.SealedAST, FunctionDecl->range),
			TEXT("The scalar profile accepts ordinary global functions and final non-virtual instance methods only."));
	}

	FAngelscriptTypedASTJITFunctionShape Shape = Input.Shape;
	if (FunctionDecl != nullptr && !Shape.ReturnType.IsValid())
	{
		Shape.ReturnType = ResolveType(*Input.SealedAST, FunctionDecl->type);
	}
	if (FunctionDecl != nullptr
		&& FunctionDecl->kind == asAST_DECL_METHOD
		&& Shape.InvocationKind == EAngelscriptTypedASTJITInvocationKind::Global)
	{
		Shape.InvocationKind = EAngelscriptTypedASTJITInvocationKind::InstanceMethod;
	}
	if (Shape.bHasFrozenBodySignature
		&& (Shape.FrozenBodyReturnCppType.IsEmpty()
			|| Shape.FrozenBodyParameterCppTypes.Num()
				!= Shape.ParameterTypes.Num()
			|| Shape.FrozenBodyParameterABIs.Num()
				!= Shape.ParameterTypes.Num()
			|| Shape.FrozenBodyParameterCppTypes.ContainsByPredicate(
				[](const FString& Type)
				{
					return Type.IsEmpty();
				})))
	{
		return Reject(EFallback::UnsupportedSignature,
			MakeSpan(*Input.SealedAST, FunctionDecl->range),
			TEXT("TypedASTJITFrozenBodySignatureMalformed: frozen return/parameter C++ ABI fields are incomplete."));
	}

	constexpr asDWORD KnownTraitBits =
		asTRAIT_CONSTRUCTOR | asTRAIT_DESTRUCTOR | asTRAIT_CONST
		| asTRAIT_PRIVATE | asTRAIT_PROTECTED | asTRAIT_FINAL
		| asTRAIT_OVERRIDE | asTRAIT_SHARED | asTRAIT_EXTERNAL
		| asTRAIT_PROPERTY | asTRAIT_IMPLICITCONSTRUCTOR | asTRAIT_MIXIN
		| asTRAIT_LOCAL | asTRAIT_NODISCARD | asTRAIT_DEPRECATED
		| asTRAIT_GENERIC_TEMPLATE_FUNCTION | asTRAIT_USES_WORLDCONTEXT
		| asTRAIT_ACCEPT_TEMPORARY_OBJECT | asTRAIT_GENERATED_FUNCTION
		| asTRAIT_NOT_CALLABLE | asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS
		| asTRAIT_EXTERNAL_IMPLICIT_THIS | asTRAIT_ALLOWDISCARD
		| asTRAIT_EDITOR_ONLY | asTRAIT_EXPLICIT
		| asTRAIT_UNSAFE_DURING_CONSTRUCTION | asTRAIT_DEFAULTS_ONLY;
	constexpr asDWORD UnsupportedTraitBits =
		asTRAIT_CONSTRUCTOR | asTRAIT_DESTRUCTOR | asTRAIT_OVERRIDE
		| asTRAIT_EXTERNAL | asTRAIT_IMPLICITCONSTRUCTOR | asTRAIT_MIXIN
		| asTRAIT_LOCAL | asTRAIT_GENERIC_TEMPLATE_FUNCTION
		| asTRAIT_USES_WORLDCONTEXT | asTRAIT_ACCEPT_TEMPORARY_OBJECT
		| asTRAIT_GENERATED_FUNCTION | asTRAIT_NOT_CALLABLE
		| asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS
		| asTRAIT_EXTERNAL_IMPLICIT_THIS
		| asTRAIT_UNSAFE_DURING_CONSTRUCTION | asTRAIT_DEFAULTS_ONLY;
	const asDWORD UnknownTraitBits = Input.RawTraitBits & ~KnownTraitBits;
	const asDWORD RejectedTraitBits = Input.RawTraitBits & UnsupportedTraitBits;
	if (UnknownTraitBits != 0 || RejectedTraitBits != 0
		|| Input.bVirtualOrNonFinal)
	{
		return Reject(EFallback::UnsupportedFunctionTrait, {},
			FString::Printf(TEXT(
				"RawTraits=0x%08x Unknown=0x%08x Unsupported=0x%08x VirtualOrNonFinal=%s"),
				Input.RawTraitBits, UnknownTraitBits, RejectedTraitBits,
				Input.bVirtualOrNonFinal ? TEXT("true") : TEXT("false")));
	}
	if (Input.UFunctionFlags != EAngelscriptTypedASTJITUFunctionFlags::None)
	{
		return Reject(EFallback::UnsupportedUFunctionFlags, {},
			TEXT("Resolved UFunction flags remain outside the typed native surface."));
	}

	const bool bGlobalInvocation =
		Shape.InvocationKind == EAngelscriptTypedASTJITInvocationKind::Global;
	const bool bFinalInstanceInvocation =
		Shape.InvocationKind == EAngelscriptTypedASTJITInvocationKind::InstanceMethod;
	if (Shape.InvocationKind == EAngelscriptTypedASTJITInvocationKind::GlobalInitializer)
	{
		return Reject(EFallback::UnsupportedGlobalInitializer, {},
			TEXT("The initial TypedASTJIT profile does not emit global initializer bodies."));
	}
	if (!bGlobalInvocation && !bFinalInstanceInvocation)
	{
		return Reject(EFallback::UnsupportedFunctionKind, {},
			TEXT("The scalar profile accepts ordinary global functions and final non-virtual instance methods only."));
	}
	if (bGlobalInvocation
		&& (Shape.ReceiverKind != EAngelscriptTypedASTJITReceiverKind::None
			|| Shape.ReceiverParameterIndex != INDEX_NONE
			|| Shape.bHasNativeObjectSlot))
	{
		return Reject(EFallback::UnsupportedReceiver, {},
			TEXT("A global scalar root must not carry an effective receiver."));
	}
	if (bFinalInstanceInvocation
		&& (Shape.ReceiverKind != EAngelscriptTypedASTJITReceiverKind::NativeObjectThis
			|| Shape.ReceiverParameterIndex != INDEX_NONE
			|| !Shape.bHasNativeObjectSlot))
	{
		return Reject(EFallback::UnsupportedReceiver, {},
			TEXT("A final instance scalar root requires the exact native object-this slot."));
	}

	const bool bHasReviewedManagedInput = Shape.bHasFrozenBodySignature
		&& Shape.FrozenBodyParameterABIs.Contains(
			EAngelscriptTypedASTJITBodyParameterABI::BorrowedValueAddress);
	if ((EnumHasAnyFlags(Input.SignatureFlags,
			EAngelscriptTypedASTJITSignatureFlags::Reference)
			&& !bHasReviewedManagedInput)
		|| EnumHasAnyFlags(Input.SignatureFlags,
			EAngelscriptTypedASTJITSignatureFlags::OutParameter
			| EAngelscriptTypedASTJITSignatureFlags::ReturnOnStack)
		|| Shape.bReturnsOnStack)
	{
		return Reject(EFallback::UnsupportedSignature, {},
			TEXT("Canonical signature remains outside the scalar slice."));
	}
	if (EnumHasAnyFlags(Input.SignatureFlags,
		EAngelscriptTypedASTJITSignatureFlags::Object
		| EAngelscriptTypedASTJITSignatureFlags::Container
		| EAngelscriptTypedASTJITSignatureFlags::Delegate)
		|| (EnumHasAnyFlags(Input.SignatureFlags,
				EAngelscriptTypedASTJITSignatureFlags::Struct)
			&& !bHasReviewedManagedInput))
	{
		return Reject(EFallback::UnsupportedType, {},
			TEXT("Canonical signature type remains outside the scalar slice."));
	}
	if (Shape.ReturnType.IsValid()
		&& !IsByValueScalarOrEnum(Shape.ReturnType, true))
	{
		return Reject(EFallback::UnsupportedType, {},
			TEXT("Return type is not a by-value scalar or enum."));
	}
	for (int32 ParameterIndex = 0;
		ParameterIndex < Shape.ParameterTypes.Num(); ++ParameterIndex)
	{
		const asCDataType& ParameterType =
			Shape.ParameterTypes[ParameterIndex];
		const bool bReviewedParameter = Shape.bHasFrozenBodySignature
			? (Shape.FrozenBodyParameterABIs[ParameterIndex]
					== EAngelscriptTypedASTJITBodyParameterABI::ScalarValue
					? IsByValueScalarOrEnum(ParameterType, false)
					: IsStructurallyBorrowedManagedInput(ParameterType))
			: IsByValueScalarOrEnum(ParameterType, false);
		if (!bReviewedParameter)
		{
			return Reject(EFallback::UnsupportedType, {},
				FString::Printf(TEXT(
					"TypedASTJITFrozenBodySignatureParameterMismatch: formal %d is not compatible with its frozen scalar/borrowed body ABI."),
					ParameterIndex));
		}
	}

	const FAngelscriptTypedASTJITCanonicalLifetimeFacts LifetimeFacts =
		AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
			*Input.SealedAST, Input.CanonicalFunctionDecl);
	if (!LifetimeFacts.LifetimeSummary.IsAuthenticated()
		|| LifetimeFacts.CleanupPlanState
			== EAngelscriptTypedASTJITCleanupPlanState::Unverified
		|| !LifetimeFacts.bCleanupPlanCoversAllTransfers)
	{
		return Reject(
			EFallback::InvalidCleanupPlan,
			MakeSpan(*Input.SealedAST, FunctionDecl->range),
			TEXT("CanonicalLifetimeProtocolAuthenticationFailed: the exact function has no complete authenticated lifetime summary."));
	}
	if (!LifetimeFacts.LifetimeSummary.bNativeObjectFrameABIAvailable)
	{
		const bool bRequiresAggregateElementFrame =
			EnumHasAnyFlags(
				LifetimeFacts.LifetimeSummary.ActionFlags,
				EAngelscriptTypedASTJITLifetimeActionFlags::DestroyElement);
		const bool bRequiresFullExpressionTemporaryFrame =
			EnumHasAnyFlags(
				LifetimeFacts.LifetimeSummary.ExitPlanFlags,
				EAngelscriptTypedASTJITLifetimeExitPlanFlags::FullExpression);
		const TCHAR* const Prefix = bRequiresFullExpressionTemporaryFrame
			? TEXT("AuthenticatedFullExpressionTemporaryRequiresNativeObjectFrameABI")
			: bRequiresAggregateElementFrame
				? TEXT("AuthenticatedAggregatePartialConstructionRequiresNativeElementFrameABI")
				: TEXT("AuthenticatedLifetimeRequiresNativeObjectFrameABI");
		return Reject(
			EFallback::UnsupportedLifetime,
			MakeSpan(*Input.SealedAST, FunctionDecl->range),
			FString::Printf(TEXT(
				"%s: StableKey=%s ABIKey=%s Records=%u ExitPlans=%u ActionFlags=0x%08x ExitPlanFlags=0x%08x"),
				Prefix,
				*LifetimeFacts.LifetimeSummary.StableKey.ToHexString(),
				*LifetimeFacts.LifetimeSummary.ABIKey.ToHexString(),
				LifetimeFacts.LifetimeSummary.RecordCount,
				LifetimeFacts.LifetimeSummary.ExitPlanCount,
				static_cast<uint32>(
					LifetimeFacts.LifetimeSummary.ActionFlags),
				static_cast<uint32>(
					LifetimeFacts.LifetimeSummary.ExitPlanFlags)));
	}

	FAngelscriptTypedASTJITEligibility WalkFailure;
	if (!WalkStmt(*Input.SealedAST, FunctionDecl->body, Input.Purpose, WalkFailure))
	{
		return WalkFailure;
	}

	FAngelscriptTypedASTJITEligibility Accepted;
	Accepted.bEligible = true;
	Accepted.Reason = EFallback::None;
	Accepted.Detail = TEXT("Sealed canonical AST");
	if (FunctionDecl != nullptr)
	{
		Accepted.SourceSpan = MakeSpan(*Input.SealedAST, FunctionDecl->range);
	}
	return Accepted;
}

FAngelscriptTypedASTJITAnalysis AnalyzeTypedASTJITFunction(
	const asCASTContext& Context,
	const asASTDeclId FunctionDecl,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls,
	const TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
		ScriptCalls,
	const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		GlobalDependencies)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	(void)NativeCalls;
	(void)ScriptCalls;
	(void)GlobalDependencies;
	if (!Context.IsPublishable())
	{
		return AnalysisFail(EFallback::InvalidCanonicalAST, {},
			TEXT("Canonical AST must be sealed before TypedASTJIT analysis."));
	}
	FAngelscriptTypedASTJITEligibilityInput Input;
	Input.bIsUFunctionRoot = true;
	Input.Purpose = EAngelscriptTypedASTJITEligibilityPurpose::RootCallClosure;
	Input.SealedAST = &Context;
	Input.CanonicalFunctionDecl = FunctionDecl;
	Input.Shape = Shape;
	const FAngelscriptTypedASTJITEligibility Eligibility =
		EvaluateAngelscriptTypedASTJITEligibilityFromCanonical(Input);
	if (!Eligibility.bEligible)
	{
		return AnalysisFail(
			Eligibility.Reason, Eligibility.SourceSpan, Eligibility.Detail);
	}
	FAngelscriptTypedASTJITAnalysis Result;
	Result.bEligible = true;
	return Result;
}

FAngelscriptTypedASTJITCanonicalLifetimeFacts
AnalyzeAngelscriptTypedASTJITCanonicalLifetimeFacts(
	const asCASTContext& Context,
	const asASTDeclId FunctionDecl)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	FAngelscriptTypedASTJITCanonicalLifetimeFacts Facts;
	if (!Context.IsPublishable())
	{
		return Facts;
	}

	const asCDecl* const Decl = FindFunctionDecl(
		Context, FunctionDecl, nullptr);
	if (Decl == nullptr || !Decl->body.IsValid())
	{
		return Facts;
	}

	asCASTLifetimeView LifetimeView;
	if (asCASTBuildLifetimeView(Context, LifetimeView) != asAST_VERIFY_OK)
	{
		return Facts;
	}
	return BuildCanonicalLifetimeSummary(
		Context, FunctionDecl, LifetimeView, Facts)
		? Facts : FAngelscriptTypedASTJITCanonicalLifetimeFacts{};
}

FAngelscriptTypedASTJITEmission EmitTypedASTJITFunction(
	const asCASTContext& Context,
	const asASTDeclId FunctionDecl,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const FAngelscriptTypedASTJITEmitOptions& Options)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	using ECapability = EAngelscriptTypedASTJITDirectCapability;
	constexpr ECapability ApprovedEmissionCapabilities =
		ECapability::FramePosition | ECapability::RecursionBudget;
	const FAngelscriptTypedASTJITGenerationCapabilityProfile* const
		InstrumentationProfile = Options.InstrumentationProfile;
	if (InstrumentationProfile != nullptr)
	{
		if (!InstrumentationProfile->IsValid())
		{
			return EmissionFail(
				EFallback::EmitterFailure,
				{},
				TEXT("InvalidInstrumentationProfile: generated runtime hooks require one valid immutable capability profile"));
		}
		const uint16 UnsupportedBits = static_cast<uint16>(
			InstrumentationProfile->AvailableCapabilities)
			& ~static_cast<uint16>(ApprovedEmissionCapabilities);
		if (UnsupportedBits != 0)
		{
			return EmissionFail(
				EFallback::EmitterFailure,
				{},
				FString::Printf(TEXT(
					"UnsupportedInstrumentationCapabilities: Profile=%s Unsupported=0x%04x"),
					*LexToString(
						InstrumentationProfile->AvailableCapabilities),
					UnsupportedBits));
		}
	}
	const FAngelscriptTypedASTJITAnalysis Analysis =
		AnalyzeTypedASTJITFunction(
			Context, FunctionDecl, Shape,
			Options.NativeCalls, Options.ScriptCalls,
			Options.GlobalDependencies);
	if (!Analysis.bEligible)
	{
		return EmissionFail(
			Analysis.FailureReason, Analysis.FailureSpan, Analysis.FailureDetail);
	}
	if (Shape.SymbolName.IsEmpty())
	{
		return EmissionFail(EFallback::UnsupportedSignature, {},
			TEXT("The resolved C++ symbol name is empty"));
	}
	const asCDecl* const Decl = FindFunctionDecl(
		Context, FunctionDecl, nullptr);
	if (Decl == nullptr)
	{
		return EmissionFail(EFallback::InvalidCanonicalAST, {},
			TEXT("The exact Canonical function declaration is missing"));
	}
	FString CanonicalReturnType;
	if (!TrySpellCanonicalScalarCppType(
			Context, Decl->type, CanonicalReturnType))
	{
		return EmissionFail(EFallback::EmitterFailure, {},
			TEXT("Canonical return type is outside the reviewed scalar C++ spelling set"));
	}
	FString ReturnType;
	if (Shape.bHasFrozenBodySignature)
	{
		if (Shape.FrozenBodyReturnCppType != CanonicalReturnType)
		{
			return EmissionFail(EFallback::EmitterFailure, {},
				FString::Printf(TEXT(
					"TypedASTJITFrozenBodySignatureReturnMismatch: Canonical=%s EntryPlan=%s"),
					*CanonicalReturnType,
					*Shape.FrozenBodyReturnCppType));
		}
		ReturnType = Shape.FrozenBodyReturnCppType;
	}
	else if (!TrySpellAngelscriptTypedASTJITCppType(
		Shape.ReturnType, ReturnType))
	{
		// Provider-independent scalar inspection may omit Runtime type
		// coordinates for script enums. Its only fallback is the sealed
		// Canonical scalar spelling; production always takes the frozen branch.
		ReturnType = CanonicalReturnType;
		if (ReturnType.IsEmpty())
		{
			return EmissionFail(EFallback::EmitterFailure, {},
				TEXT("Return type has no exact reviewed C++ spelling"));
		}
	}

	const bool bEmitFramePosition = InstrumentationProfile != nullptr
		&& EnumHasAnyFlags(
			InstrumentationProfile->AvailableCapabilities,
			ECapability::FramePosition);
	FCanonicalScalarEmitter Emitter(
		Context, Options.NativeCalls, Options.ScriptCalls,
		Options.GlobalDependencies, bEmitFramePosition, ReturnType);
	// Emit the body into a staging buffer before spelling the function ABI. Some
	// source operations (checked divide/remainder/power) discover their need for
	// FScriptExecution while lowering. Freezing the signature first would publish
	// a body that references Execution without declaring the hidden parameter.
	FString EmittedBody;
	if (Decl != nullptr && Decl->body.IsValid())
	{
		const asCStmt* Body = Context.GetStmt(Decl->body);
		if (Body != nullptr && Body->kind == asAST_STMT_BLOCK)
		{
			for (asUINT Index = 0; Index < Body->children.GetLength(); ++Index)
			{
				if (!Emitter.EmitStatement(
					Body->children[Index], 1, EmittedBody))
				{
					return EmissionFail(EFallback::EmitterFailure, {},
						Emitter.GetFailureDetail());
				}
			}
		}
		else if (!Emitter.EmitStatement(Decl->body, 1, EmittedBody))
		{
			return EmissionFail(EFallback::EmitterFailure, {},
				Emitter.GetFailureDetail());
		}
	}
	FAngelscriptTypedASTJITEmission Candidate;
	if (InstrumentationProfile != nullptr)
	{
		Candidate.InstrumentationProfileIdentityHash =
			InstrumentationProfile->IdentityHash;
		Candidate.bRequiresInstrumentationFrame = EnumHasAnyFlags(
			InstrumentationProfile->AvailableCapabilities,
			ECapability::FramePosition);
	}
	Candidate.bRequiresExecutionState = Emitter.RequiresExecutionState();
	Candidate.RequiredIncludes.Add(TEXT("CoreMinimal.h"));
	if (Candidate.bRequiresExecutionState)
	{
		Candidate.RequiredIncludes.Add(TEXT(
			"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"));
	}
	if (Candidate.bRequiresInstrumentationFrame)
	{
		Candidate.RequiredIncludes.Add(TEXT("StaticJIT/StaticJITHeader.h"));
	}
	const FString FunctionStart = FString::Printf(
		TEXT("static %s %s(\n"),
		*ReturnType,
		*Shape.SymbolName);
	Candidate.Declaration = FunctionStart;
	if (Options.bEmitBridgeCallSiteRows)
	{
		FString BridgeRows;
		FString BridgeRowsError;
		if (!EmitAngelscriptTypedASTJITBridgeCallSiteRows(
				Options.NativeCalls, BridgeRows, &BridgeRowsError))
		{
			return EmissionFail(
				EFallback::EmitterFailure, {}, BridgeRowsError);
		}
		Candidate.Definition += MoveTemp(BridgeRows);
	}
	Candidate.Definition += FunctionStart;
	const bool bHasHiddenParameters = Candidate.bRequiresExecutionState
		|| Candidate.bRequiresInstrumentationFrame;
	const int32 TotalParameterCount = Shape.ParameterTypes.Num()
		+ (Candidate.bRequiresExecutionState ? 1 : 0)
		+ (Candidate.bRequiresInstrumentationFrame ? 1 : 0);
	int32 EmittedParameterCount = 0;
	auto AppendHiddenParameter = [&](const TCHAR* Parameter)
	{
		++EmittedParameterCount;
		const FString Line = FString::Printf(TEXT("\t%s%s\n"),
			Parameter,
			EmittedParameterCount == TotalParameterCount
				? TEXT(")") : TEXT(","));
		Candidate.Declaration += Line;
		Candidate.Definition += Line;
	};
	if (Candidate.bRequiresExecutionState)
	{
		AppendHiddenParameter(TEXT("FScriptExecution& Execution"));
	}
	if (Candidate.bRequiresInstrumentationFrame)
	{
		AppendHiddenParameter(
			TEXT("FScopeTypedASTJITExecutionFrame& TypedFrame"));
	}
	if (Shape.ParameterTypes.IsEmpty())
	{
		if (bHasHiddenParameters)
		{
			Candidate.Declaration.RemoveFromEnd(TEXT(")\n"));
			Candidate.Declaration += TEXT(");\n");
			Candidate.Definition += TEXT("{\n");
		}
		else
		{
			Candidate.Declaration.RemoveFromEnd(TEXT("\n"));
			Candidate.Declaration += TEXT(");\n");
			Candidate.Definition.RemoveFromEnd(TEXT("\n"));
			Candidate.Definition += TEXT(")\n{\n");
		}
	}
	else
	{
		const asUINT CanonicalFormalCount =
			Context.GetFormalDeclCount(FunctionDecl);
		if (CanonicalFormalCount != static_cast<asUINT>(
			Shape.ParameterTypes.Num()))
		{
			return EmissionFail(EFallback::EmitterFailure, {},
				FString::Printf(TEXT(
					"Canonical formal count does not match the Runtime entry ABI: Canonical=%u Runtime=%d"),
					CanonicalFormalCount, Shape.ParameterTypes.Num()));
		}
		for (int32 Index = 0; Index < Shape.ParameterTypes.Num(); ++Index)
		{
			FString ParameterType;
			if (Shape.bHasFrozenBodySignature)
			{
				ParameterType = Shape.FrozenBodyParameterCppTypes[Index];
			}
			else if (!TrySpellAngelscriptTypedASTJITCppType(
				Shape.ParameterTypes[Index], ParameterType))
			{
				return EmissionFail(EFallback::EmitterFailure, {},
					TEXT("Parameter type is outside the reviewed scalar C++ spelling set"));
			}
			const asCDecl* const ParamDecl = Context.GetFormalDecl(
				FunctionDecl, static_cast<asUINT>(Index));
			if (ParamDecl == nullptr)
			{
				return EmissionFail(EFallback::EmitterFailure, {},
					FString::Printf(TEXT(
						"Canonical formal slot %d is missing or ambiguous"), Index));
			}
			FString CanonicalParameterType;
			if (!TrySpellCanonicalCallArgumentCppType(
					Context, ParamDecl->type, CanonicalParameterType)
				|| CanonicalParameterType != ParameterType)
			{
				return EmissionFail(EFallback::EmitterFailure, {},
					FString::Printf(TEXT(
						"Canonical formal slot %d type does not match the Runtime entry ABI"),
						Index));
			}
			++EmittedParameterCount;
			const FString Parameter = FString::Printf(
				TEXT("\tconst %s as_ast_d%u%s\n"),
				*ParameterType,
				ParamDecl->id.value,
				EmittedParameterCount == TotalParameterCount
					? TEXT(")") : TEXT(","));
			Candidate.Declaration += Parameter;
			Candidate.Definition += Parameter;
		}
		Candidate.Declaration.RemoveFromEnd(TEXT(")\n"));
		Candidate.Declaration += TEXT(");\n");
		Candidate.Definition += TEXT("{\n");
	}

	Candidate.Definition += MoveTemp(EmittedBody);
	if (ReturnType != TEXT("void"))
	{
		Candidate.Definition += TEXT(
			"\t// Canonical Sema proves source-level return completeness; this "
			"fallback keeps C++ structurally total when it cannot prove enum "
			"switch exhaustiveness.\n");
		Candidate.Definition += TEXT("\treturn ") + ReturnType + TEXT("{};\n");
	}
	if (Emitter.RequiresScalarOps())
	{
		Candidate.RequiredIncludes.Add(
			TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"));
	}
	for (const FString& Include : Emitter.GetRequiredIncludes())
	{
		Candidate.RequiredIncludes.AddUnique(Include);
	}
	Candidate.NativeCallSites = Emitter.GetNativeCallSites();
	Candidate.NativeCallSites.Sort([](
		const FAngelscriptTypedASTJITNativeCallSiteMetadata& Left,
		const FAngelscriptTypedASTJITNativeCallSiteMetadata& Right)
	{
		return Left.Expression.value < Right.Expression.value;
	});
	FString NativeCallComments;
	for (int32 CallSiteIndex = 0;
		CallSiteIndex < Candidate.NativeCallSites.Num();
		++CallSiteIndex)
	{
		const FAngelscriptTypedASTJITNativeCallSiteMetadata& CallSite =
			Candidate.NativeCallSites[CallSiteIndex];
		NativeCallComments += FString::Printf(TEXT(
			"/*\n"
			" * AngelScript Native Call Site %d\n"
			" * AS Declaration      : %s\n"
			" * Emitted C++ Callee  : %s\n"
			" * Route               : %s\n"
			" * Effective Target    : %s\n"
			" * Runtime DLL Core    : %s\n"
			" * Metadata Row        : %s\n"
			" * Reference Slot      : %d\n"
			" */\n"),
			CallSiteIndex,
			*OneLineNativeCallComment(CallSite.CanonicalDeclaration),
			*OneLineNativeCallComment(CallSite.EmittedCppCallee),
			NativeCallSiteRouteName(CallSite.Route),
			*OneLineNativeCallComment(CallSite.RegisteredTarget),
			CallSite.RuntimeDllCore.IsEmpty()
				? TEXT("<none>") : *CallSite.RuntimeDllCore,
			CallSite.MetadataRowSymbol.IsEmpty()
				? TEXT("<none>") : *CallSite.MetadataRowSymbol,
			CallSite.ReferenceSlotIndex);
	}
	Candidate.Definition = NativeCallComments + Candidate.Definition;
	Candidate.Definition += TEXT("}\n");
	Candidate.bSuccess = true;
	return Candidate;
}

const TCHAR* LexToString(
	const EAngelscriptTypedASTJITSemanticUseKind Kind)
{
	switch (Kind)
	{
	case EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature:
		return TEXT("ScriptCallSignature");
	case EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent:
		return TEXT("ScriptCallContent");
	case EAngelscriptTypedASTJITSemanticUseKind::NativeCall:
		return TEXT("NativeCall");
	case EAngelscriptTypedASTJITSemanticUseKind::ImportedBindingSlot:
		return TEXT("ImportedBindingSlot");
	case EAngelscriptTypedASTJITSemanticUseKind::ReceiverType:
		return TEXT("ReceiverType");
	case EAngelscriptTypedASTJITSemanticUseKind::ParameterType:
		return TEXT("ParameterType");
	case EAngelscriptTypedASTJITSemanticUseKind::ReturnType:
		return TEXT("ReturnType");
	case EAngelscriptTypedASTJITSemanticUseKind::ExpressionType:
		return TEXT("ExpressionType");
	case EAngelscriptTypedASTJITSemanticUseKind::HiddenArgumentType:
		return TEXT("HiddenArgumentType");
	case EAngelscriptTypedASTJITSemanticUseKind::FoldedGlobalHardValue:
		return TEXT("FoldedGlobalHardValue");
	case EAngelscriptTypedASTJITSemanticUseKind::MutableGlobalStorage:
		return TEXT("MutableGlobalStorage");
	case EAngelscriptTypedASTJITSemanticUseKind::ProviderRoute:
		return TEXT("ProviderRoute");
	default:
		return TEXT("Invalid");
	}
}

FAngelscriptTypedASTJITDependencyAnalysis
AnalyzeAngelscriptTypedASTJITDependencies(
	const asCASTContext& Context,
	const asASTDeclId FunctionDecl,
	const FAngelscriptTypedASTJITFunctionShape& Shape,
	const FAngelscriptStaticJITCompiledFunctionView& Function,
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	const TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan>
		NativeCalls,
	const TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan>
		ScriptCalls,
	const TConstArrayView<FAngelscriptTypedASTJITGlobalDependency>
		GlobalDependencies)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	(void)GlobalDependencies;
	FAngelscriptTypedASTJITDependencyAnalysis Result;
	const asCDecl* const Decl = FindFunctionDecl(
		Context, FunctionDecl, nullptr);
	const asSSourceSpan RootSpan = Decl != nullptr
		? MakeSpan(Context, Decl->range)
		: asSSourceSpan{};
	const FAngelscriptTypedASTJITAnalysis Analysis =
		AnalyzeTypedASTJITFunction(
			Context, FunctionDecl, Shape, NativeCalls, ScriptCalls,
			GlobalDependencies);
	if (!Analysis.bEligible)
	{
		Result.FailureReason = Analysis.FailureReason;
		Result.FailureSpan = Analysis.FailureSpan;
		Result.FailureDetail = Analysis.FailureDetail;
		return Result;
	}
	if (!InitializeCanonicalDependencyManifest(
		Function, Graph, RootSpan, Result))
	{
		return Result;
	}
	TArray<asASTExprId> Calls;
	CollectAngelscriptTypedASTJITCanonicalCallExprs(
		Context, FunctionDecl, Calls);
	for (const asASTExprId CallId : Calls)
	{
		const FAngelscriptTypedASTJITNativeCallEmissionPlan* const Native =
			NativeCalls.FindByPredicate(
			[CallId](const FAngelscriptTypedASTJITNativeCallEmissionPlan& Plan)
			{
				return Plan.Expression == CallId;
			});
		const FAngelscriptTypedASTJITScriptCallEmissionPlan* const Script =
			ScriptCalls.FindByPredicate(
			[CallId](const FAngelscriptTypedASTJITScriptCallEmissionPlan& Plan)
			{
				return Plan.Expression == CallId;
			});
		if ((Native == nullptr) == (Script == nullptr))
		{
			Result.FailureReason = EFallback::UnsupportedCall;
			if (const asCExpr* Expr = Context.GetExpr(CallId))
			{
				Result.FailureSpan = MakeSpan(Context, Expr->range);
			}
			Result.FailureDetail = TEXT(
				"Resolved call requires exactly one exact native or direct script-helper emission plan");
			return Result;
		}

		const asCExpr* const Expr = Context.GetExpr(CallId);
		const asSSourceSpan Span = Expr != nullptr
			? MakeSpan(Context, Expr->range)
			: RootSpan;
		if (Script != nullptr)
		{
			if (!AddCanonicalSemanticUse(
				Result,
				EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature,
				EAngelscriptArtifactSemanticDependencyKind::Signature,
				Script->TargetReference,
				Span,
				Script->EngineLocalTargetFunctionId,
				Script->Symbol))
			{
				return Result;
			}

			const bool bSelfRecursion =
				Script->TargetReference.Kind
					== EAngelscriptArtifactReferenceKind::ScriptFunction
				&& Script->TargetReference.StableKey
					== Function.FunctionKey.Hash;
			if (!bSelfRecursion)
			{
				const bool bHasFunctionContent =
					Result.Manifest.ArtifactDependencies.ContainsByPredicate(
						[Script](const FAngelscriptArtifactSemanticDependency& Dependency)
						{
							return Dependency.Kind
									== EAngelscriptArtifactSemanticDependencyKind::FunctionContent
								&& Dependency.Target == Script->TargetReference;
						});
				if (!bHasFunctionContent)
				{
					FAngelscriptArtifactSemanticDependency& Synthesized =
						Result.Manifest.ArtifactDependencies.Emplace_GetRef();
					Synthesized.Kind =
						EAngelscriptArtifactSemanticDependencyKind::FunctionContent;
					Synthesized.Target = Script->TargetReference;
					for (const FAngelscriptStaticJITCompiledFunctionView& Candidate
						: Graph.Functions)
					{
						if (Candidate.FunctionKey.Hash
								== Script->TargetReference.StableKey
							&& !Candidate.ExecutionHash.IsZero())
						{
							Synthesized.ExpectedContentOrValue =
								Candidate.ExecutionHash;
							break;
						}
					}
					Result.Manifest.ArtifactDependencies.Sort();
				}
				if (!AddCanonicalSemanticUse(
					Result,
					EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent,
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Script->TargetReference,
					Span,
					Script->EngineLocalTargetFunctionId,
					Script->Symbol))
				{
					return Result;
				}
			}
			if (!AddCanonicalSemanticUse(
				Result,
				EAngelscriptTypedASTJITSemanticUseKind::ProviderRoute,
				EAngelscriptArtifactSemanticDependencyKind::Invalid,
				Script->TargetReference,
				Span,
				Script->EngineLocalTargetFunctionId,
				Script->Symbol))
			{
				return Result;
			}
			continue;
		}

		if (Native->TargetReference.IsValid())
		{
			Result.Manifest.RequiredReferences.AddUnique(
				Native->TargetReference);
			Result.Manifest.RequiredReferences.Sort();
			const bool bHasEnvironmentAbi =
				Result.Manifest.ArtifactDependencies.ContainsByPredicate(
					[Native](const FAngelscriptArtifactSemanticDependency& Dependency)
					{
						return Dependency.Kind
								== EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi
							&& Dependency.Target == Native->TargetReference;
					});
			if (!bHasEnvironmentAbi)
			{
				FAngelscriptArtifactSemanticDependency& Synthesized =
					Result.Manifest.ArtifactDependencies.Emplace_GetRef();
				Synthesized.Kind =
					EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi;
				Synthesized.Target = Native->TargetReference;
				Result.Manifest.ArtifactDependencies.Sort();
			}
		}
		if (!AddCanonicalSemanticUse(
			Result,
			EAngelscriptTypedASTJITSemanticUseKind::NativeCall,
			EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi,
			Native->TargetReference,
			Span,
			Native->EngineLocalTargetFunctionId,
			Native->CanonicalDeclaration))
		{
			return Result;
		}
		if (!Native->Include.IsEmpty())
		{
			Result.Manifest.RequiredIncludes.AddUnique(Native->Include);
		}
		if (!AddCanonicalSemanticUse(
			Result,
			EAngelscriptTypedASTJITSemanticUseKind::ProviderRoute,
			EAngelscriptArtifactSemanticDependencyKind::Invalid,
			Native->TargetReference,
			Span,
			Native->EngineLocalTargetFunctionId,
			Native->RegisteredTarget))
		{
			return Result;
		}
	}

	TArray<asASTExprId> DeclRefs;
	if (Decl != nullptr)
	{
		CollectDeclRefsFromStmt(Context, Decl->body, DeclRefs);
	}
	for (const asASTExprId DeclRefId : DeclRefs)
	{
		const asCExpr* const Expr = Context.GetExpr(DeclRefId);
		const asCDecl* const Target = Expr != nullptr
			? Context.GetDecl(Expr->resolvedDecl) : nullptr;
		const asCDecl* const Parent = Target != nullptr
			? Context.GetDecl(Target->parent) : nullptr;
		if (Target == nullptr
			|| Target->kind != asAST_DECL_VAR
			|| Parent == nullptr
			|| Parent->kind != asAST_DECL_TRANSLATION_UNIT
			|| !Target->hasConstantValue)
		{
			continue;
		}

		const FAngelscriptTypedASTJITGlobalDependency* const Dependency =
			FindExactCanonicalGlobalDependency(GlobalDependencies, *Target);
		FString DependencyError;
		if (Dependency == nullptr
			|| !ValidateAngelscriptTypedASTJITFoldedGlobalDependency(
				GlobalDependencies,
				Dependency->EngineLocalGlobalPropertyId,
				&DependencyError))
		{
			Result = DependencyFail(
				MakeSpan(Context, Expr->range),
				Dependency == nullptr
					? TEXT("Use=FoldedGlobalHardValue StableMapping=Missing")
					: DependencyError);
			return Result;
		}
		if (!AddCanonicalSemanticUse(
			Result,
			EAngelscriptTypedASTJITSemanticUseKind::FoldedGlobalHardValue,
			EAngelscriptArtifactSemanticDependencyKind::HardValue,
			Dependency->Reference,
			MakeSpan(Context, Expr->range),
			Dependency->EngineLocalGlobalPropertyId,
			UTF8_TO_TCHAR(Target->stableKey.AddressOf())))
		{
			return Result;
		}
	}
	Result.Manifest.RequiredIncludes.Sort();
	Result.bValid = true;
	Result.FailureReason = EFallback::None;
	return Result;
}

bool AngelscriptTypedASTJITCanonicalHasLoopBackedge(
	const asCASTContext& Context,
	const asASTDeclId FunctionDecl)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	const asCDecl* Decl = FindFunctionDecl(Context, FunctionDecl, nullptr);
	return Decl != nullptr && StmtHasLoop(Context, Decl->body);
}

void CollectAngelscriptTypedASTJITCanonicalCallExprs(
	const asCASTContext& Context,
	const asASTDeclId FunctionDecl,
	TArray<asASTExprId>& OutCalls)
{
	using namespace AngelscriptTypedASTJITCanonical_Private;
	OutCalls.Reset();
	const asCDecl* Decl = FindFunctionDecl(Context, FunctionDecl, nullptr);
	if (Decl != nullptr)
	{
		CollectCallsFromStmt(Context, Decl->body, OutCalls);
	}
}

#endif
