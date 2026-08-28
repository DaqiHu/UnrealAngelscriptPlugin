#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCanonical.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_lifetime.h"
#include "source/as_ast_traversal.h"
#include "source/as_runtime_type_bridge.h"
#include "source/as_scriptfunction.h"
#include "source/as_source_manager.h"
#include "source/as_typeinfo.h"
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
		if (TrySpellCanonicalScalarCppType(Context, QualType, OutType))
		{
			return true;
		}

		const asCType* const Type = Context.GetType(QualType.type);
		if (Type == nullptr
			|| Type->kind != asAST_TYPE_VALUE_OBJECT
			|| !QualType.IsConst()
			|| !QualType.IsReference()
			|| QualType.IsHandle()
			|| QualType.IsAutoHandle()
			|| (Type->stableKey != "FString"
				&& Type->stableKey != "FLinearColor"))
		{
			OutType.Reset();
			return false;
		}

		// The reviewed BorrowedManagedPOD route owns these exact names as part
		// of its public C++ ABI. Resolve them from the sealed, address-free
		// Canonical type key instead of requiring a live asCTypeInfo pointer.
		// No arbitrary user-defined value object is admitted by this path.
		OutType = UTF8_TO_TCHAR(Type->stableKey.AddressOf());
		OutType += TEXT("&");
		return true;
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

	bool IsReviewedManagedInput(const asCDataType& Type)
	{
		const asCTypeInfo* const TypeInfo = Type.GetTypeInfo();
		return TypeInfo != nullptr
			&& TypeInfo->module == nullptr
			&& Type.IsObject()
			&& Type.IsReference()
			&& Type.IsReadOnly()
			&& !Type.IsObjectHandle()
			&& (TypeInfo->name == "FString"
				|| TypeInfo->name == "FLinearColor");
	}

	bool IsReviewedParameterType(const asCDataType& Type)
	{
		return IsByValueScalarOrEnum(Type, false)
			|| IsReviewedManagedInput(Type);
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
			// Keep this eligibility migration behavior-neutral. The legacy canonical
			// walk inspected call arguments but not the separately stored receiver.
			return Edge.role != asAST_EDGE_EXPR_RECEIVER;
		}

	private:
		EAngelscriptTypedASTJITEligibilityPurpose Purpose;
		FAngelscriptTypedASTJITEligibility& OutFailure;
	};

	enum class ECanonicalLexicalCleanupKind : uint8
	{
		None,
		Release,
		Destructor,
	};

	struct FCanonicalLexicalCleanupAction
	{
		ECanonicalLexicalCleanupKind Kind =
			ECanonicalLexicalCleanupKind::None;
		asASTDeclId Target;
		asASTDeclId Destructor;
		asASTStmtId Scope;
	};

	struct FCanonicalLoopCleanupPhase
	{
		FCanonicalLexicalCleanupAction Action;
		asASTStmtId Loop;
	};

	const asCType* ResolveCanonicalLifetimeTypedef(
		const asCASTContext& Context,
		const asCType* NominalType)
	{
		const asCType* Current = NominalType;
		for (asUINT Depth = 0; Current != nullptr && Depth < 8; ++Depth)
		{
			if (Current->kind == asAST_TYPE_PRIMITIVE
				|| Current->kind == asAST_TYPE_ENUM)
			{
				return Current;
			}
			const asCDecl* Alias = nullptr;
			for (asUINT DeclIndex = 1;
				DeclIndex <= Context.GetDeclCount(); ++DeclIndex)
			{
				const asCDecl* const Candidate = Context.GetDecl(
					asASTDeclId(DeclIndex));
				if (Candidate != nullptr
					&& Candidate->kind == asAST_DECL_TYPEDEF
					&& (Candidate->name.Equals(Current->stableKey)
						|| Candidate->stableKey.Equals(Current->stableKey)))
				{
					Alias = Candidate;
					break;
				}
			}
			if (Alias == nullptr || !Alias->type.IsValid())
			{
				return Current;
			}
			const asCType* const Underlying = Context.GetType(Alias->type.type);
			if (Underlying == nullptr || Underlying == Current)
			{
				return Current;
			}
			Current = Underlying;
		}
		return Current;
	}

	asASTDeclId FindCanonicalLifetimeDestructor(
		const asCASTContext& Context,
		const asCType& Type)
	{
		for (asUINT DeclIndex = 1;
			DeclIndex <= Context.GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Destructor = Context.GetDecl(
				asASTDeclId(DeclIndex));
			if (Destructor == nullptr
				|| Destructor->kind != asAST_DECL_DESTRUCTOR)
			{
				continue;
			}
			const asCDecl* const Parent = Context.GetDecl(Destructor->parent);
			if (Parent != nullptr
				&& (Parent->stableKey.Equals(Type.stableKey)
					|| Parent->name.Equals(Type.stableKey)))
			{
				return Destructor->id;
			}
		}
		return {};
	}

	bool TryGetCanonicalLexicalCleanupRequirement(
		const asCASTContext& Context,
		const asASTDeclId Declaration,
		const asASTStmtId Scope,
		FCanonicalLexicalCleanupAction& OutAction)
	{
		const asCDecl* const Variable = Context.GetDecl(Declaration);
		if (Variable == nullptr || Variable->kind != asAST_DECL_VAR
			|| !Variable->type.IsValid() || Variable->type.IsReference()
			|| Variable->inits.GetLength() == 0)
		{
			return false;
		}
		const asCType* const Type = ResolveCanonicalLifetimeTypedef(
			Context, Context.GetType(Variable->type.type));
		if (Type == nullptr)
		{
			return false;
		}

		OutAction.Target = Declaration;
		OutAction.Scope = Scope;
		if (Type->kind == asAST_TYPE_REFERENCE_OBJECT
			|| Type->kind == asAST_TYPE_FUNCDEF)
		{
			OutAction.Kind = ECanonicalLexicalCleanupKind::Release;
			return true;
		}
		if (Type->kind != asAST_TYPE_VALUE_OBJECT
			|| Variable->type.IsHandle())
		{
			return false;
		}
		OutAction.Destructor = FindCanonicalLifetimeDestructor(Context, *Type);
		if (!OutAction.Destructor.IsValid())
		{
			return false;
		}
		OutAction.Kind = ECanonicalLexicalCleanupKind::Destructor;
		return true;
	}

	bool TryReadCanonicalLexicalCleanupAction(
		const asCASTContext& Context,
		const asCASTLifetimeView& LifetimeView,
		const asASTStmtId StatementId,
		FCanonicalLexicalCleanupAction& OutAction)
	{
		const asSASTLifetimeCleanupBinding* const Binding =
			LifetimeView.FindCleanupBinding(StatementId);
		if (Binding == nullptr
			|| Binding->subject.nodeClass != asAST_LIFETIME_NODE_DECL)
		{
			return false;
		}

		OutAction.Target = asASTDeclId(Binding->subject.value);
		if (Binding->actionKind == asAST_LIFETIME_ACTION_RELEASE_REFERENCE)
		{
			OutAction.Kind = ECanonicalLexicalCleanupKind::Release;
			return true;
		}
		const asCDecl* const Destructor =
			Binding->actionTarget.nodeClass == asAST_LIFETIME_NODE_DECL
			? Context.GetDecl(asASTDeclId(Binding->actionTarget.value)) : nullptr;
		if (Binding->actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE
			&& Destructor != nullptr
			&& Destructor->kind == asAST_DECL_DESTRUCTOR)
		{
			OutAction.Kind = ECanonicalLexicalCleanupKind::Destructor;
			OutAction.Destructor = Destructor->id;
			return true;
		}
		return false;
	}

	bool CanonicalCleanupActionsEqual(
		const FCanonicalLexicalCleanupAction& Expected,
		const FCanonicalLexicalCleanupAction& Actual)
	{
		return Expected.Kind == Actual.Kind
			&& Expected.Target == Actual.Target
			&& Expected.Destructor == Actual.Destructor;
	}

	bool CanonicalStatementSubtreeContains(
		const asCASTContext& Context,
		const asASTStmtId Root,
		const asASTStmtId Needle)
	{
		if (!Root.IsValid() || !Needle.IsValid())
		{
			return false;
		}
		TArray<asASTStmtId> Stack;
		Stack.Add(Root);
		TArray<bool> Visited;
		Visited.Init(false, static_cast<int32>(Context.GetStmtCount() + 1));
		while (!Stack.IsEmpty())
		{
			const asASTStmtId CurrentId = Stack.Pop(EAllowShrinking::No);
			if (CurrentId == Needle)
			{
				return true;
			}
			if (!CurrentId.IsValid()
				|| CurrentId.value >= static_cast<asUINT>(Visited.Num())
				|| Visited[CurrentId.value])
			{
				continue;
			}
			Visited[CurrentId.value] = true;
			const asCStmt* const Current = Context.GetStmt(CurrentId);
			for (asUINT ChildIndex = 0;
				Current != nullptr
					&& ChildIndex < Current->children.GetLength(); ++ChildIndex)
			{
				Stack.Add(Current->children[ChildIndex]);
			}
		}
		return false;
	}

	bool ProveCanonicalLifetimeStmt(
		const asCASTContext& Context,
		const asCASTLifetimeView& LifetimeView,
		const asASTStmtId StatementId,
		TArray<FCanonicalLexicalCleanupAction>& Active,
		TArray<FCanonicalLoopCleanupPhase>& ActiveLoopPhases,
		bool& bSawRequiredLifetime,
		int32& RemainingStatements)
	{
		if (!StatementId.IsValid())
		{
			return true;
		}
		if (--RemainingStatements < 0)
		{
			return false;
		}
		const asCStmt* const Statement = Context.GetStmt(StatementId);
		if (Statement == nullptr)
		{
			return false;
		}

		if (Statement->kind == asAST_STMT_FOREACH)
		{
			const asSASTForEachPhases* const Phases =
				LifetimeView.FindForEachPhases(StatementId);
			if (Phases == nullptr)
			{
				return false;
			}
			// A value-object foreach iterator owns a loop-lifetime phase rather
			// than an ordinary trailing Block action. The synthetic init Block's
			// scope spans condition/body/increment; break enters the common phase,
			// continue stays inside it, and return/outer transfers use the active
			// CodeGen loop-frame route. Prove that exact protocol here instead of
			// treating the init Block or fourth child as ordinary lexical cleanup.
			const asCStmt* const Init = Context.GetStmt(Phases->initializer);
			if (Init == nullptr || Init->kind != asAST_STMT_BLOCK
				|| --RemainingStatements < 0)
			{
				return false;
			}

			FCanonicalLexicalCleanupAction PhaseAction;
			const bool bHasLifetimePhase = Phases->cleanup.IsValid();
			if (bHasLifetimePhase
				&& (!TryReadCanonicalLexicalCleanupAction(
					Context, LifetimeView, Phases->cleanup, PhaseAction)
					|| --RemainingStatements < 0))
			{
				return false;
			}

			FCanonicalLexicalCleanupAction IteratorRequirement;
			int32 RequiredIteratorCount = 0;
			for (asUINT ChildIndex = 0;
				ChildIndex < Init->children.GetLength(); ++ChildIndex)
			{
				const asCStmt* const Child = Context.GetStmt(
					Init->children[ChildIndex]);
				FCanonicalLexicalCleanupAction Expected;
				if (Child != nullptr && Child->kind == asAST_STMT_DECL
					&& TryGetCanonicalLexicalCleanupRequirement(
						Context, Child->decl, StatementId, Expected))
				{
					const asCDecl* const Iterator = Context.GetDecl(Child->decl);
					if (Iterator == nullptr
						|| (Iterator->traits & asAST_TRAIT_GENERATED) == 0)
					{
						return false;
					}
					IteratorRequirement = Expected;
					++RequiredIteratorCount;
				}
			}
			if ((bHasLifetimePhase && (RequiredIteratorCount != 1
					|| !CanonicalCleanupActionsEqual(
						IteratorRequirement, PhaseAction)))
				|| (!bHasLifetimePhase && RequiredIteratorCount != 0))
			{
				return false;
			}

			// The init Block is synthetic: traverse its authored declarations and
			// initialization statements without demanding a Block-tail action.
			// The exact phase above is the sole owner of the generated iterator.
			for (asUINT ChildIndex = 0;
				ChildIndex < Init->children.GetLength(); ++ChildIndex)
			{
				FCanonicalLexicalCleanupAction UnexpectedCleanup;
				if (TryReadCanonicalLexicalCleanupAction(
						Context, LifetimeView, Init->children[ChildIndex], UnexpectedCleanup)
					|| !ProveCanonicalLifetimeStmt(
						Context, LifetimeView, Init->children[ChildIndex], Active,
						ActiveLoopPhases, bSawRequiredLifetime,
						RemainingStatements))
				{
					return false;
				}
			}

			if (bHasLifetimePhase)
			{
				bSawRequiredLifetime = true;
				FCanonicalLoopCleanupPhase Phase;
				Phase.Action = IteratorRequirement;
				Phase.Loop = StatementId;
				ActiveLoopPhases.Add(Phase);
			}
			const asASTStmtId ExecutedPhases[2] =
				{ Phases->body, Phases->increment };
			for (asUINT PhaseIndex = 0; PhaseIndex < 2; ++PhaseIndex)
			{
				if (!ProveCanonicalLifetimeStmt(
						Context, LifetimeView, ExecutedPhases[PhaseIndex], Active,
						ActiveLoopPhases, bSawRequiredLifetime,
						RemainingStatements))
				{
					if (bHasLifetimePhase)
						ActiveLoopPhases.Pop(EAllowShrinking::No);
					return false;
				}
			}
			if (bHasLifetimePhase)
				ActiveLoopPhases.Pop(EAllowShrinking::No);
			return true;
		}

		if (Statement->kind == asAST_STMT_BLOCK)
		{
			int32 AuthoredCount = static_cast<int32>(
				Statement->children.GetLength());
			while (AuthoredCount > 0)
			{
				FCanonicalLexicalCleanupAction Action;
				if (!TryReadCanonicalLexicalCleanupAction(
					Context, LifetimeView,
					Statement->children[AuthoredCount - 1], Action))
				{
					break;
				}
				--AuthoredCount;
			}

			TArray<FCanonicalLexicalCleanupAction> BlockLifetimes;
			for (int32 ChildIndex = 0; ChildIndex < AuthoredCount; ++ChildIndex)
			{
				const asCStmt* const Child = Context.GetStmt(
					Statement->children[ChildIndex]);
				FCanonicalLexicalCleanupAction Expected;
				if (Child != nullptr && Child->kind == asAST_STMT_DECL
					&& TryGetCanonicalLexicalCleanupRequirement(
						Context, Child->decl, StatementId, Expected))
				{
					BlockLifetimes.Add(Expected);
					bSawRequiredLifetime = true;
				}
			}

			const int32 CleanupCount = static_cast<int32>(
				Statement->children.GetLength()) - AuthoredCount;
			if (CleanupCount != BlockLifetimes.Num())
			{
				return false;
			}
			for (int32 CleanupIndex = 0;
				CleanupIndex < CleanupCount; ++CleanupIndex)
			{
				FCanonicalLexicalCleanupAction Actual;
				if (!TryReadCanonicalLexicalCleanupAction(
						Context, LifetimeView,
						Statement->children[AuthoredCount + CleanupIndex],
						Actual)
					|| !CanonicalCleanupActionsEqual(
						BlockLifetimes[BlockLifetimes.Num() - CleanupIndex - 1],
						Actual))
				{
					return false;
				}
			}

			const int32 RestoreActiveCount = Active.Num();
			for (int32 ChildIndex = 0; ChildIndex < AuthoredCount; ++ChildIndex)
			{
				const asCStmt* const Child = Context.GetStmt(
					Statement->children[ChildIndex]);
				FCanonicalLexicalCleanupAction Expected;
				if (Child != nullptr && Child->kind == asAST_STMT_DECL
					&& TryGetCanonicalLexicalCleanupRequirement(
						Context, Child->decl, StatementId, Expected))
				{
					Active.Add(Expected);
				}
				if (!ProveCanonicalLifetimeStmt(
					Context, LifetimeView,
					Statement->children[ChildIndex], Active,
					ActiveLoopPhases, bSawRequiredLifetime,
					RemainingStatements))
				{
					Active.SetNum(RestoreActiveCount, EAllowShrinking::No);
					return false;
				}
			}
			Active.SetNum(RestoreActiveCount, EAllowShrinking::No);
			return true;
		}

		const bool bReturn = Statement->kind == asAST_STMT_RETURN;
		const bool bTargetedTransfer =
			Statement->kind == asAST_STMT_BREAK
			|| Statement->kind == asAST_STMT_CONTINUE
			|| Statement->kind == asAST_STMT_FALLTHROUGH;
		if (bReturn || bTargetedTransfer)
		{
			for (int32 PhaseIndex = ActiveLoopPhases.Num() - 1;
				PhaseIndex >= 0; --PhaseIndex)
			{
				const FCanonicalLoopCleanupPhase& Phase =
					ActiveLoopPhases[PhaseIndex];
				if (bReturn)
				{
					// Return exits every active loop frame. The exact phase has
					// already been proven and CodeGen routes all active frames.
					continue;
				}
				if (!Statement->target.IsValid())
				{
					return false;
				}
				const bool bTargetStaysInsidePhase =
					CanonicalStatementSubtreeContains(
						Context, Phase.Loop, Statement->target);
				if (Statement->kind == asAST_STMT_FALLTHROUGH
					&& !bTargetStaysInsidePhase)
				{
					// Fallthrough has no loop-frame cleanup route and therefore
					// cannot escape an active foreach iterator lifetime.
					return false;
				}
				// A contained target remains inside the iterator lifetime.
				// Targeting the foreach itself means break enters its common
				// cleanup label while continue stays live. An outer target makes
				// EmitTransferCleanups walk and execute this exact active phase.
			}

			TArray<FCanonicalLexicalCleanupAction> ExpectedActions;
			for (int32 ActiveIndex = Active.Num() - 1;
				ActiveIndex >= 0; --ActiveIndex)
			{
				const FCanonicalLexicalCleanupAction& Candidate =
					Active[ActiveIndex];
				if (bReturn || !Statement->target.IsValid()
					|| !CanonicalStatementSubtreeContains(
						Context, Candidate.Scope, Statement->target))
				{
					ExpectedActions.Add(Candidate);
				}
			}
			if (ExpectedActions.Num()
				!= static_cast<int32>(Statement->children.GetLength()))
			{
				return false;
			}
			for (int32 CleanupIndex = 0;
				CleanupIndex < ExpectedActions.Num(); ++CleanupIndex)
			{
				FCanonicalLexicalCleanupAction Actual;
				if (!TryReadCanonicalLexicalCleanupAction(
						Context, LifetimeView,
						Statement->children[CleanupIndex], Actual)
					|| !CanonicalCleanupActionsEqual(
						ExpectedActions[CleanupIndex], Actual))
				{
					return false;
				}
			}
			return true;
		}

		FCanonicalLexicalCleanupAction StandaloneCleanup;
		if (TryReadCanonicalLexicalCleanupAction(
			Context, LifetimeView, StatementId, StandaloneCleanup))
		{
			// A lexical action is only complete as a trailing Block action or a
			// direct transfer child. Loop/foreach cleanup phases require their own
			// reviewed lifetime protocol before they can publish a proof.
			return false;
		}

		for (asUINT ChildIndex = 0;
			ChildIndex < Statement->children.GetLength(); ++ChildIndex)
		{
			if (!ProveCanonicalLifetimeStmt(
				Context, LifetimeView, Statement->children[ChildIndex], Active,
				ActiveLoopPhases, bSawRequiredLifetime,
				RemainingStatements))
			{
				return false;
			}
		}
		return true;
	}

	bool ProveCanonicalLifetimeCoverage(
		const asCASTContext& Context,
		const asCASTLifetimeView& LifetimeView,
		const asASTStmtId Body,
		bool& bSawRequiredLifetime)
	{
		TArray<FCanonicalLexicalCleanupAction> Active;
		TArray<FCanonicalLoopCleanupPhase> ActiveLoopPhases;
		int32 RemainingStatements = static_cast<int32>(
			Context.GetStmtCount() + 1);
		return ProveCanonicalLifetimeStmt(
			Context, LifetimeView, Body, Active, ActiveLoopPhases,
			bSawRequiredLifetime,
			RemainingStatements);
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
				const bool bScriptCall = Script != nullptr;
				const bool bBoundNative = Native != nullptr
					&& Native->Route
						!= EAngelscriptTypedASTJITCallEmissionRoute::Direct;
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

				if (Native != nullptr && !bBoundNative
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
				if (!TrySpellCanonicalScalarCppType(
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
					const asUINT FormalIndex =
						Expr->children.GetLength() - EvaluationIndex - 1;
					const asASTExprId OperandId =
						Expr->children[EvaluationIndex];
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
					if (!TrySpellCanonicalCallArgumentCppType(
							Context, Operand->type, OperandType))
					{
						const asCType* const OperandCanonicalType =
							Context.GetType(Operand->type.type);
						return Fail(FString::Printf(TEXT(
							"Canonical call argument has no reviewed C++ spelling: Call=%u Operand=%u Kind=%u Formal=%u Type=%s Quals=0x%08x"),
							Id.value,
							OperandId.value,
							static_cast<uint32>(Operand->kind),
							FormalIndex,
							OperandCanonicalType != nullptr
								? UTF8_TO_TCHAR(
									OperandCanonicalType->stableKey.AddressOf())
								: TEXT("<missing>"),
							Operand->type.quals));
					}
					FormalTypes[FormalIndex] = OperandType;
					FormalTemporaries[FormalIndex] = FString::Printf(
						TEXT("as_ast_e%u_arg%u"), Id.value, FormalIndex);
					Out += FString::Printf(TEXT("%sconst %s %s = %s;\n"),
						*BodyPrefix,
						*OperandType,
						*FormalTemporaries[FormalIndex],
						*OperandText);
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
					Out += CasePrefix + TEXT("default:\n");
					AppendSourcePosition(Stmt->range, Depth + 3, Out);
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

	const bool bHasReviewedManagedInput =
		Shape.ParameterTypes.ContainsByPredicate(
			[](const asCDataType& ParameterType)
			{
				return IsReviewedManagedInput(ParameterType);
			});
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
	for (const asCDataType& ParameterType : Shape.ParameterTypes)
	{
		if (!IsReviewedParameterType(ParameterType))
		{
			return Reject(EFallback::UnsupportedType, {},
				TEXT("Parameter type is neither a by-value scalar/enum nor a reviewed const FString/FLinearColor input."));
		}
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

	bool bSawReleaseAction = false;
	bool bSawDestructorAction = false;
	for (asUINT BindingIndex = 0;
		BindingIndex < LifetimeView.GetCleanupBindingCount(); ++BindingIndex)
	{
		const asSASTLifetimeCleanupBinding* const Binding =
			LifetimeView.GetCleanupBinding(BindingIndex);
		const asCDecl* const Subject = Binding != nullptr
			&& Binding->subject.nodeClass == asAST_LIFETIME_NODE_DECL
			? Context.GetDecl(asASTDeclId(Binding->subject.value)) : nullptr;
		if (Subject == nullptr || Subject->parent != FunctionDecl)
		{
			continue;
		}
		bSawReleaseAction |= Binding->actionKind
			== asAST_LIFETIME_ACTION_RELEASE_REFERENCE;
		bSawDestructorAction |= Binding->actionKind
			== asAST_LIFETIME_ACTION_DESTROY_VALUE;
	}

	bool bSawRequiredLifetime = false;
	const bool bProvenCoverage = ProveCanonicalLifetimeCoverage(
		Context, LifetimeView, Decl->body, bSawRequiredLifetime);

	if (bSawDestructorAction)
	{
		Facts.CleanupPlanState =
			EAngelscriptTypedASTJITCleanupPlanState::ScriptDestructor;
	}
	else if (bSawReleaseAction)
	{
		Facts.CleanupPlanState =
			EAngelscriptTypedASTJITCleanupPlanState::NonEmpty;
	}
	else
	{
		if (bSawRequiredLifetime && !bProvenCoverage)
		{
			// The declaration/type facts require an action, but the sealed tree
			// did not provide the exact normal/transfer plan. Never publish this
			// as VerifiedEmpty; generation must keep the function on fallback.
			return Facts;
		}
		Facts.CleanupPlanState =
			EAngelscriptTypedASTJITCleanupPlanState::VerifiedEmpty;
	}

	Facts.bCleanupPlanCoversAllTransfers = bProvenCoverage;
	return Facts;
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
	FString ReturnType;
	if (!TrySpellAngelscriptTypedASTJITCppType(Shape.ReturnType, ReturnType))
	{
		asCDataType Derived = Shape.ReturnType;
		if (!Derived.IsValid())
		{
			if (const asCDecl* Decl = FindFunctionDecl(
				Context, FunctionDecl, nullptr))
			{
				Derived = ResolveType(Context, Decl->type);
			}
		}
		if (!TrySpellAngelscriptTypedASTJITCppType(Derived, ReturnType))
		{
			ReturnType = TEXT("int32");
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
	const asCDecl* Decl = FindFunctionDecl(Context, FunctionDecl, nullptr);
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
		for (int32 Index = 0; Index < Shape.ParameterTypes.Num(); ++Index)
		{
			FString ParameterType;
			if (!TrySpellAngelscriptTypedASTJITCppType(
				Shape.ParameterTypes[Index], ParameterType))
			{
				return EmissionFail(EFallback::EmitterFailure, {},
					TEXT("Parameter type is outside the reviewed scalar C++ spelling set"));
			}
			asASTDeclId ParamDecl;
			if (const asCDecl* Fn = Context.GetDecl(FunctionDecl))
			{
				asUINT ParamOrdinal = 0;
				for (asUINT Child = 0; Child < Fn->children.GetLength(); ++Child)
				{
					const asCDecl* ChildDecl = Context.GetDecl(Fn->children[Child]);
					if (ChildDecl != nullptr
						&& ChildDecl->kind == asAST_DECL_PARAM)
					{
						if (static_cast<int32>(ParamOrdinal) == Index)
						{
							ParamDecl = ChildDecl->id;
							break;
						}
						++ParamOrdinal;
					}
				}
			}
			++EmittedParameterCount;
			const FString Parameter = FString::Printf(
				TEXT("\tconst %s as_ast_d%u%s\n"),
				*ParameterType,
				ParamDecl.value,
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
