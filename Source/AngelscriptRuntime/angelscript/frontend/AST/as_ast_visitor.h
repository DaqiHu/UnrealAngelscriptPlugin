/* Maintained Unreal AngelScript fork — deterministic typed AST traversal. */
#pragma once

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_context.h"
#include "frontend/AST/as_attr.h"
#include "frontend/AST/as_type.h"

#include "Containers/Set.h"
#include "Templates/Function.h"

BEGIN_AS_NAMESPACE

enum class asEASTVisitStatus : uint8
	{
		Succeeded,
		NullRoot,
		UnhandledKind,
		OwningCycle,
		DuplicateOwningEdge,
	};

	// Canonical type objects are context-owned semantic nodes. References to
	// them are never owning AST edges, and may therefore be shared freely by
	// otherwise-independent declaration/body fragments.
	enum class asEASTSemanticTypeReferenceKind : uint8
	{
		ExpressionType,
		VariableType,
		SignatureType,
		BaseType,
		GenericTemplate,
		GenericArgument,
		FunctionReturn,
		FunctionParameter,
		AliasedType,
		ForeachContainerType,
		ForeachIteratorType,
	};

	struct asSASTSemanticTypeReference
	{
		asEASTSemanticTypeReferenceKind Kind = asEASTSemanticTypeReferenceKind::ExpressionType;
		asCQualType Type;
		uint32 Ordinal = 0;
		asEParameterPassingMode PassingMode = asEParameterPassingMode::Value;
	};

	enum class asEASTCrossReferenceKind : uint8
	{
		Declaration,
		ReturnTarget,
		BreakTarget,
		ContinueTarget,
		MemberDeclaration,
		OperatorFunction,
		Constructor,
		DefaultParameter,
		SwitchParent,
		FallthroughTarget,
		ForeachBeginFunction,
		ForeachEndFunction,
		ForeachNextFunction,
		ForeachValueFunction,
		ForeachKeyFunction,
		ThisRecord,
		ReservedAnonymousEnclosingFunction,
		ReservedAnonymousCapture,
		ReservedAnonymousCapturedThis,
		CustomAccessSpecifier,
		ConversionFunction,
	};

	struct asSASTCrossReference
	{
		asEASTCrossReferenceKind Kind = asEASTCrossReferenceKind::Declaration;
		const asCASTNode* Target = nullptr;
		asSStableKey Identity;
	};

	class ANGELSCRIPTRUNTIME_API asCASTVisitor
	{
	public:
		template <typename Callback>
		static asEASTVisitStatus Walk(const asCASTNode* Root, Callback&& Visit)
		{
			if (!Root) return asEASTVisitStatus::NullRoot;
			TSet<const asCASTNode*> Visited;
			TSet<const asCASTNode*> Active;
			return WalkNode(Root, TFunctionRef<void(const asCASTNode&)>(Visit), Visited, Active);
		}

		static bool CollectOwningChildren(const asCASTNode& Node, TArray<const asCASTNode*>& OutChildren);
		static bool CollectSemanticTypeReferences(
			const asCASTNode& Node,
			TArray<asSASTSemanticTypeReference>& OutReferences);
		static bool CollectCrossReferences(
			const asCASTNode& Node,
			TArray<asSASTCrossReference>& OutReferences);
		static const asCASTNode* GetCrossReference(const asCASTNode& Node);

	private:
		static asEASTVisitStatus WalkNode(
			const asCASTNode* Node,
			TFunctionRef<void(const asCASTNode&)> Visit,
			TSet<const asCASTNode*>& Visited,
			TSet<const asCASTNode*>& Active)
		{
			if (Active.Contains(Node)) return asEASTVisitStatus::OwningCycle;
			if (Visited.Contains(Node)) return asEASTVisitStatus::DuplicateOwningEdge;
			Active.Add(Node);
			Visited.Add(Node);
			Visit(*Node);
			TArray<const asCASTNode*> Children;
			if (!CollectOwningChildren(*Node, Children)) return asEASTVisitStatus::UnhandledKind;
			for (const asCASTNode* Child : Children)
			{
				if (!Child) continue;
				const asEASTVisitStatus Status = WalkNode(Child, Visit, Visited, Active);
				if (Status != asEASTVisitStatus::Succeeded) return Status;
			}
			Active.Remove(Node);
			return asEASTVisitStatus::Succeeded;
		}
	};

	inline bool asCASTVisitor::CollectOwningChildren(const asCASTNode& Node, TArray<const asCASTNode*>& Out)
	{
		Out.Reset();
		switch (Node.GetFamily())
		{
		case asEASTNodeFamily::Decl:
		{
			const asCDecl& Decl = static_cast<const asCDecl&>(Node);
			for (asCAttr* Attr : Decl.GetAttrs()) Out.Add(Attr);
			const asCDeclContext* Context = nullptr;
			switch (Decl.GetKind())
			{
			case asEDeclKind::TranslationUnit: Context = static_cast<const asCTranslationUnitDecl*>(&Decl); break;
			case asEDeclKind::Namespace: Context = static_cast<const asCNamespaceDecl*>(&Decl); break;
			case asEDeclKind::Record: Context = static_cast<const asCRecordDecl*>(&Decl); break;
			case asEDeclKind::Function: Context = static_cast<const asCFunctionDecl*>(&Decl); break;
			case asEDeclKind::Delegate:
			case asEDeclKind::Event: Context = static_cast<const asCCallableTypeDecl*>(&Decl); break;
			case asEDeclKind::EnumConstant:
			case asEDeclKind::Var:
			case asEDeclKind::ParmVar:
			case asEDeclKind::TypeAlias:
			case asEDeclKind::AccessSpecifier:
			case asEDeclKind::Recovery: break;
			default: return false;
			}
			if (Context) for (asCDecl* Child : Context->GetDecls()) Out.Add(Child);
			if (const auto* Variable = dyn_cast<asCVarDecl>(&Decl))
			{
				if (asCExpr* Initializer = Variable->GetInitializer()) Out.Add(Initializer);
			}
			if (const auto* Constant = dyn_cast<asCEnumConstantDecl>(&Decl))
			{
				if (asCExpr* Initializer = Constant->GetInitializer()) Out.Add(Initializer);
			}
			if (Decl.GetKind() == asEDeclKind::Function)
			{
				if (asCStmt* Body = static_cast<const asCFunctionDecl&>(Decl).GetBody()) Out.Add(Body);
			}
			return true;
		}
		case asEASTNodeFamily::Stmt:
		{
			const asCStmt& Stmt = static_cast<const asCStmt&>(Node);
			switch (Stmt.GetKind())
			{
			case asEStmtKind::Compound:
				for (asCStmt* Child : static_cast<const asCCompoundStmt&>(Stmt).GetStatements()) Out.Add(Child);
				return true;
			case asEStmtKind::If:
			{
				const asCIfStmt& If = static_cast<const asCIfStmt&>(Stmt);
				Out.Add(If.GetCondition()); Out.Add(If.GetThen()); if (If.GetElse()) Out.Add(If.GetElse());
				return true;
			}
			case asEStmtKind::Return:
				if (asCExpr* Value = static_cast<const asCReturnStmt&>(Stmt).GetValue()) Out.Add(Value);
				return true;
			case asEStmtKind::While:
			{
				const asCWhileStmt& While = static_cast<const asCWhileStmt&>(Stmt);
				Out.Add(While.GetCondition()); Out.Add(While.GetBody());
				return true;
			}
			case asEStmtKind::For:
			{
				const asCForStmt& For = static_cast<const asCForStmt&>(Stmt);
				for (asCStmt* Initializer : For.GetInitializers()) Out.Add(Initializer);
				if (For.GetCondition()) Out.Add(For.GetCondition());
				for (asCExpr* Increment : For.GetIncrements()) Out.Add(Increment);
				Out.Add(For.GetBody());
				return true;
			}
			case asEStmtKind::Do:
			{
				const asCDoStmt& Do = static_cast<const asCDoStmt&>(Stmt);
				Out.Add(Do.GetBody()); Out.Add(Do.GetCondition());
				return true;
			}
			case asEStmtKind::DeclStmt:
			{
				const asCDeclStmt& DeclStatement = static_cast<const asCDeclStmt&>(Stmt);
				Out.Add(DeclStatement.GetDeclaration());
				if (DeclStatement.GetInitializer()) Out.Add(DeclStatement.GetInitializer());
				return true;
			}
			case asEStmtKind::CallExpr:
			{
				const asCCallExpr& Call = static_cast<const asCCallExpr&>(Stmt);
				Out.Add(Call.GetCallee());
				for (asCExpr* Argument : Call.GetArguments()) Out.Add(Argument);
				return true;
			}
			case asEStmtKind::ImplicitCastExpr:
				Out.Add(static_cast<const asCImplicitCastExpr&>(Stmt).GetOperand()); return true;
			case asEStmtKind::UserConversionExpr:
				Out.Add(static_cast<const asCUserConversionExpr&>(Stmt).GetOperand()); return true;
			case asEStmtKind::ParenExpr:
				Out.Add(static_cast<const asCParenExpr&>(Stmt).GetSubExpr()); return true;
			case asEStmtKind::UnaryOperator:
				Out.Add(static_cast<const asCUnaryOperator&>(Stmt).GetOperand()); return true;
			case asEStmtKind::BinaryOperator:
			{
				const asCBinaryOperator& Binary = static_cast<const asCBinaryOperator&>(Stmt);
				Out.Add(Binary.GetLeft()); Out.Add(Binary.GetRight()); return true;
			}
			case asEStmtKind::ConditionalOperator:
			{
				const asCConditionalOperator& Conditional = static_cast<const asCConditionalOperator&>(Stmt);
				Out.Add(Conditional.GetCondition()); Out.Add(Conditional.GetTrueExpr()); Out.Add(Conditional.GetFalseExpr());
				return true;
			}
			case asEStmtKind::MemberExpr:
				Out.Add(static_cast<const asCMemberExpr&>(Stmt).GetBase()); return true;
			case asEStmtKind::ArraySubscriptExpr:
			{
				const asCArraySubscriptExpr& Subscript = static_cast<const asCArraySubscriptExpr&>(Stmt);
				Out.Add(Subscript.GetBase());
				for (asCExpr* Argument : Subscript.GetArguments()) Out.Add(Argument);
				return true;
			}
			case asEStmtKind::ExplicitCastExpr:
				Out.Add(static_cast<const asCExplicitCastExpr&>(Stmt).GetOperand()); return true;
			case asEStmtKind::ConstructExpr:
				for (asCExpr* Argument : static_cast<const asCConstructExpr&>(Stmt).GetArguments()) Out.Add(Argument);
				return true;
			case asEStmtKind::InitListExpr:
				for (asCExpr* Element : static_cast<const asCInitListExpr&>(Stmt).GetElements()) Out.Add(Element);
				return true;
			case asEStmtKind::DeclGroupStmt:
				for (asCStmt* Declaration : static_cast<const asCDeclGroupStmt&>(Stmt).GetDeclarations()) Out.Add(Declaration);
				return true;
			case asEStmtKind::ClassDefaultStmt:
				Out.Add(static_cast<const asCClassDefaultStmt&>(Stmt).GetStatement()); return true;
			case asEStmtKind::Switch:
			{
				const asCSwitchStmt& Switch = static_cast<const asCSwitchStmt&>(Stmt);
				Out.Add(Switch.GetSelector());
				for (asCSwitchClause* Clause : Switch.GetClauses()) Out.Add(Clause);
				return true;
			}
			case asEStmtKind::Case:
			{
				const asCCaseStmt& Case = static_cast<const asCCaseStmt&>(Stmt);
				Out.Add(Case.GetLabel());
				for (asCStmt* Statement : Case.GetStatements()) Out.Add(Statement);
				return true;
			}
			case asEStmtKind::Default:
				for (asCStmt* Statement : static_cast<const asCDefaultStmt&>(Stmt).GetStatements()) Out.Add(Statement);
				return true;
			case asEStmtKind::Foreach:
			{
				const asCForeachStmt& Foreach = static_cast<const asCForeachStmt&>(Stmt);
				Out.Add(Foreach.GetValueStatement());
				if (Foreach.GetKeyStatement()) Out.Add(Foreach.GetKeyStatement());
				Out.Add(Foreach.GetRangeExpression());
				Out.Add(Foreach.GetBody());
				return true;
			}
			case asEStmtKind::Break:
			case asEStmtKind::Continue:
			case asEStmtKind::Fallthrough:
			case asEStmtKind::RecoveryStmt:
			case asEStmtKind::NullStmt:
			case asEStmtKind::IntegerLiteral:
			case asEStmtKind::DeclRefExpr:
			case asEStmtKind::RecoveryExpr:
			case asEStmtKind::BooleanLiteral:
			case asEStmtKind::FloatingLiteral:
			case asEStmtKind::StringLiteral:
			case asEStmtKind::NullPtrLiteral:
			case asEStmtKind::DefaultArgumentExpr:
			case asEStmtKind::ThisExpr:
			case asEStmtKind::ValueInitExpr:
				return true;
			default: return false;
			}
		}
		case asEASTNodeFamily::Type:
		{
			const asETypeKind Kind = static_cast<const asCType&>(Node).GetKind();
			return Kind >= asETypeKind::Builtin && Kind <= asETypeKind::Error;
		}
		case asEASTNodeFamily::Attr:
			return Node.GetRawKind() == static_cast<uint16>(asEAttrKind::Flag)
				|| Node.GetRawKind() == static_cast<uint16>(asEAttrKind::String)
				|| Node.GetRawKind() == static_cast<uint16>(asEAttrKind::Annotation);
		default: return false;
		}
	}

	inline bool asCASTVisitor::CollectSemanticTypeReferences(
		const asCASTNode& Node,
		TArray<asSASTSemanticTypeReference>& Out)
	{
		Out.Reset();
		auto FromType = [](asCType* Type)
		{
			asCQualType Result;
			if (Type) asCQualType::TryCreate(Type, asETypeQualifiers::None, Result);
			return Result;
		};
		switch (Node.GetFamily())
		{
		case asEASTNodeFamily::Decl:
		{
			const asCDecl& Decl = static_cast<const asCDecl&>(Node);
			if (const auto* Record = dyn_cast<asCRecordDecl>(&Decl))
			{
				uint32 Ordinal = 0;
				for (asCType* Base : Record->GetResolvedBases())
					Out.Add({asEASTSemanticTypeReferenceKind::BaseType, FromType(Base), Ordinal++});
			}
			else if (const auto* Variable = dyn_cast<asCVarDecl>(&Decl))
				Out.Add({asEASTSemanticTypeReferenceKind::VariableType, Variable->GetResolvedQualType()});
			else if (const auto* Function = dyn_cast<asCFunctionDecl>(&Decl))
				Out.Add({asEASTSemanticTypeReferenceKind::SignatureType, FromType(Function->GetResolvedSignature())});
			else if (const auto* Callable = dyn_cast<asCCallableTypeDecl>(&Decl))
				Out.Add({asEASTSemanticTypeReferenceKind::SignatureType, FromType(Callable->GetResolvedSignature())});
			else if (const auto* Alias = dyn_cast<asCTypeAliasDecl>(&Decl))
				Out.Add({asEASTSemanticTypeReferenceKind::AliasedType, Alias->GetResolvedQualType()});
			return true;
		}
		case asEASTNodeFamily::Stmt:
			if (const auto* Expression = dyn_cast<asCExpr>(&Node))
				Out.Add({asEASTSemanticTypeReferenceKind::ExpressionType, Expression->GetExpressionQualType()});
			else if (const auto* Foreach = dyn_cast<asCForeachStmt>(&Node))
			{
				Out.Add({asEASTSemanticTypeReferenceKind::ForeachContainerType, Foreach->GetProtocol().ContainerType});
				Out.Add({asEASTSemanticTypeReferenceKind::ForeachIteratorType, Foreach->GetProtocol().IteratorType});
			}
			return true;
		case asEASTNodeFamily::Type:
		{
			const asCType& Type = static_cast<const asCType&>(Node);
			if (const auto* Generic = dyn_cast<asCGenericType>(&Type))
			{
				Out.Add({asEASTSemanticTypeReferenceKind::GenericTemplate, FromType(Generic->GetTemplateType())});
				for (int32 Index = 0; Index < Generic->GetArguments().Num(); ++Index)
					Out.Add({asEASTSemanticTypeReferenceKind::GenericArgument, Generic->GetArguments()[Index], static_cast<uint32>(Index)});
			}
			else if (const auto* Function = dyn_cast<asCFunctionType>(&Type))
			{
				Out.Add({asEASTSemanticTypeReferenceKind::FunctionReturn, Function->GetReturnType()});
				for (int32 Index = 0; Index < Function->GetParameterTypes().Num(); ++Index)
				{
					const asEParameterPassingMode Mode = Index < Function->GetParameterPassingModes().Num()
						? Function->GetParameterPassingModes()[Index] : asEParameterPassingMode::Value;
					Out.Add({asEASTSemanticTypeReferenceKind::FunctionParameter,
						Function->GetParameterTypes()[Index], static_cast<uint32>(Index), Mode});
				}
			}
			return true;
		}
		case asEASTNodeFamily::Attr: return true;
		default: return false;
		}
	}

	inline bool asCASTVisitor::CollectCrossReferences(
		const asCASTNode& Node,
		TArray<asSASTCrossReference>& Out)
	{
		Out.Reset();
		if (Node.GetFamily() == asEASTNodeFamily::Decl)
		{
			const asSAuthoredAccessUse* AccessUse = nullptr;
			if (const auto* Variable = dyn_cast<asCVarDecl>(&Node))
				AccessUse = Variable->GetAuthoredAccessUse();
			const asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(&Node);
			if (Function) AccessUse = Function->GetAuthoredAccessUse();
			if (AccessUse && AccessUse->Resolved)
				Out.Add({asEASTCrossReferenceKind::CustomAccessSpecifier, AccessUse->Resolved});
			return true;
		}
		if (Node.GetFamily() != asEASTNodeFamily::Stmt) return true;
		switch (static_cast<asEStmtKind>(Node.GetRawKind()))
		{
		case asEStmtKind::DeclRefExpr:
		{
			const auto& Reference = static_cast<const asCDeclRefExpr&>(Node);
			if (Reference.GetDecl()) Out.Add({asEASTCrossReferenceKind::Declaration, Reference.GetDecl()});
			else Out.Add({asEASTCrossReferenceKind::Declaration, nullptr, Reference.GetExternalIdentity()});
			break;
		}
		case asEStmtKind::Return:
			if (const asCFunctionDecl* Target = static_cast<const asCReturnStmt&>(Node).GetTarget())
				Out.Add({asEASTCrossReferenceKind::ReturnTarget, Target});
			break;
		case asEStmtKind::Break:
			Out.Add({asEASTCrossReferenceKind::BreakTarget,
				static_cast<const asCBreakStmt&>(Node).GetTarget()}); break;
		case asEStmtKind::Continue:
			Out.Add({asEASTCrossReferenceKind::ContinueTarget,
				static_cast<const asCContinueStmt&>(Node).GetTarget()}); break;
		case asEStmtKind::MemberExpr:
			Out.Add({asEASTCrossReferenceKind::MemberDeclaration,
				static_cast<const asCMemberExpr&>(Node).GetMember()}); break;
		case asEStmtKind::ArraySubscriptExpr:
			if (const asCFunctionDecl* Function = static_cast<const asCArraySubscriptExpr&>(Node).GetOperatorFunction())
				Out.Add({asEASTCrossReferenceKind::OperatorFunction, Function});
			break;
		case asEStmtKind::ConstructExpr:
			if (const asCFunctionDecl* Constructor = static_cast<const asCConstructExpr&>(Node).GetConstructor())
				Out.Add({asEASTCrossReferenceKind::Constructor, Constructor});
			break;
		case asEStmtKind::UserConversionExpr:
			Out.Add({asEASTCrossReferenceKind::ConversionFunction,
				static_cast<const asCUserConversionExpr&>(Node).GetConversionFunction()});
			break;
		case asEStmtKind::DefaultArgumentExpr:
			Out.Add({asEASTCrossReferenceKind::DefaultParameter,
				static_cast<const asCDefaultArgumentExpr&>(Node).GetParameter()}); break;
		case asEStmtKind::ThisExpr:
			Out.Add({asEASTCrossReferenceKind::ThisRecord,
				static_cast<const asCThisExpr&>(Node).GetRecord()}); break;
		case asEStmtKind::Case:
		case asEStmtKind::Default:
			Out.Add({asEASTCrossReferenceKind::SwitchParent,
				static_cast<const asCSwitchClause&>(Node).GetParentSwitch()}); break;
		case asEStmtKind::Fallthrough:
			Out.Add({asEASTCrossReferenceKind::FallthroughTarget,
				static_cast<const asCFallthroughStmt&>(Node).GetTarget()}); break;
		case asEStmtKind::Foreach:
		{
			const asSForeachProtocol& Protocol = static_cast<const asCForeachStmt&>(Node).GetProtocol();
			Out.Add({asEASTCrossReferenceKind::ForeachBeginFunction, Protocol.BeginFunction});
			Out.Add({asEASTCrossReferenceKind::ForeachEndFunction, Protocol.EndFunction});
			Out.Add({asEASTCrossReferenceKind::ForeachNextFunction, Protocol.NextFunction});
			Out.Add({asEASTCrossReferenceKind::ForeachValueFunction, Protocol.ValueFunction});
			if (Protocol.KeyFunction)
				Out.Add({asEASTCrossReferenceKind::ForeachKeyFunction, Protocol.KeyFunction});
			break;
		}
		default: break;
		}
		return true;
	}

	inline const asCASTNode* asCASTVisitor::GetCrossReference(const asCASTNode& Node)
	{
		TArray<asSASTCrossReference> References;
		return CollectCrossReferences(Node, References) && !References.IsEmpty()
			? References[0].Target : nullptr;
	}

END_AS_NAMESPACE
