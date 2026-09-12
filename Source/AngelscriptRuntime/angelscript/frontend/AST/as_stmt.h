/* Maintained Unreal AngelScript fork — concrete typed statement hierarchy. */
#pragma once

#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_type.h"

#include "Containers/Array.h"

BEGIN_AS_NAMESPACE

inline bool asIsExpressionStmtKind(asEStmtKind Kind)
	{
		return (Kind >= asEStmtKind::IntegerLiteral && Kind <= asEStmtKind::ValueInitExpr)
			|| Kind == asEStmtKind::UserConversionExpr;
	}

	class ANGELSCRIPTRUNTIME_API asCStmt : public asCASTNode
	{
	public:
		asEStmtKind GetKind() const { return static_cast<asEStmtKind>(GetRawKind()); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt; }
	protected:
		asCStmt(asEStmtKind Kind, asCSourceRange Range) : asCASTNode(asEASTNodeFamily::Stmt, static_cast<uint16>(Kind), MoveTemp(Range)) {}
	};

	class ANGELSCRIPTRUNTIME_API asCValueStmt : public asCStmt
	{
	public:
		static bool classof(const asCASTNode* Node)
		{
			return Node && Node->GetFamily() == asEASTNodeFamily::Stmt
				&& asIsExpressionStmtKind(static_cast<asEStmtKind>(Node->GetRawKind()));
		}
	protected:
		asCValueStmt(asEStmtKind Kind, asCSourceRange Range) : asCStmt(Kind, MoveTemp(Range)) {}
	};

	class ANGELSCRIPTRUNTIME_API asCExpr : public asCValueStmt
	{
	public:
		asCType* GetExpressionType() const
		{
			return reinterpret_cast<asCType*>(ExpressionSemantics & ~SemanticBitsMask);
		}
		asEValueCategory GetValueCategory() const
		{
			return static_cast<asEValueCategory>(ExpressionSemantics & ValueCategoryMask);
		}
		asCQualType GetExpressionQualType() const
		{
			asCQualType Result;
			const uint8 Qualifiers = static_cast<uint8>((GetSubclassBits() << 1)
				| (IsReadOnly() ? static_cast<uint8>(asETypeQualifiers::Const) : 0));
			asCQualType::TryCreateRaw(GetExpressionType(), Qualifiers, Result);
			return Result;
		}
		bool SetExpressionQualType(asCQualType Type, asEValueCategory Category)
		{
			if (!Type.IsValid() || !SetExpressionSemantics(Type.GetType(), Category)) return false;
			const uint8 NonConstQualifiers = static_cast<uint8>(Type.GetQualifiers())
				& ~static_cast<uint8>(asETypeQualifiers::Const);
			if (!SetSubclassBits(static_cast<uint8>(NonConstQualifiers >> 1))) return false;
			return SetReadOnly(Type.IsConst());
		}
		bool IsReadOnly() const { return (ExpressionSemantics & ReadOnlyMask) != 0; }
		bool SetReadOnly(bool Value) { if (!CanMutate()) return false; ExpressionSemantics = Value ? ExpressionSemantics | ReadOnlyMask : ExpressionSemantics & ~ReadOnlyMask; return true; }
		bool SetExpressionSemantics(asCType* Type, asEValueCategory Category)
		{
			if (!CanMutate() || !Type || Category == asEValueCategory::Invalid) return false;
			const asPWORD TypeBits = reinterpret_cast<asPWORD>(Type);
			const asPWORD CategoryBits = static_cast<asPWORD>(Category);
			if ((TypeBits & SemanticBitsMask) != 0 || CategoryBits > ValueCategoryMask) return false;
			ExpressionSemantics = TypeBits | CategoryBits | (ExpressionSemantics & ReadOnlyMask);
			return SetSubclassBits(0);
		}
		static bool classof(const asCASTNode* Node)
		{
			return Node && Node->GetFamily() == asEASTNodeFamily::Stmt
				&& asIsExpressionStmtKind(static_cast<asEStmtKind>(Node->GetRawKind()));
		}
	protected:
		asCExpr(asEStmtKind Kind, asCSourceRange Range) : asCValueStmt(Kind, MoveTemp(Range)) {}

	private:
		// Like Clang's pointer-like semantic handles, the type pointer owns the
		// low category bits. AST types are naturally pointer-aligned, so this
		// keeps semantic facts node-owned without widening every expression.
		static constexpr asPWORD ValueCategoryMask = 0x3;
		static constexpr asPWORD ReadOnlyMask = 0x4;
		static constexpr asPWORD SemanticBitsMask = 0x7;
		asPWORD ExpressionSemantics = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCCompoundStmt final : public asCStmt
	{
	public:
		explicit asCCompoundStmt(asCSourceRange Range) : asCStmt(asEStmtKind::Compound, MoveTemp(Range)) {}
		bool AddStatement(asCStmt* Statement) { if (!CanMutate() || !Statement) return false; Statements.Add(Statement); return true; }
		TConstArrayView<asCStmt*> GetStatements() const { return MakeArrayView(Statements); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Compound); }
	private:
		TArray<asCStmt*> Statements;
	};

	class ANGELSCRIPTRUNTIME_API asCIfStmt final : public asCStmt
	{
	public:
		asCIfStmt(asCExpr* Condition, asCStmt* Then, asCStmt* Else, asCSourceRange Range)
			: asCStmt(asEStmtKind::If, MoveTemp(Range)), Condition(Condition), Then(Then), Else(Else) {}
		asCExpr* GetCondition() const { return Condition; }
		asCStmt* GetThen() const { return Then; }
		asCStmt* GetElse() const { return Else; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::If); }
	private:
		asCExpr* Condition = nullptr;
		asCStmt* Then = nullptr;
		asCStmt* Else = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCReturnStmt final : public asCStmt
	{
	public:
		asCReturnStmt(asCExpr* Value, asCSourceRange Range, asCFunctionDecl* InTarget = nullptr)
			: asCStmt(asEStmtKind::Return, MoveTemp(Range)), Value(Value), Target(InTarget) {}
		asCExpr* GetValue() const { return Value; }
		asCFunctionDecl* GetTarget() const { return Target; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Return); }
	private:
		asCExpr* Value = nullptr;
		asCFunctionDecl* Target = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCWhileStmt final : public asCStmt
	{
	public:
		asCWhileStmt(asCExpr* InCondition, asCStmt* InBody, asCSourceRange Range)
			: asCStmt(asEStmtKind::While, MoveTemp(Range)), Condition(InCondition), Body(InBody) {}
		asCExpr* GetCondition() const { return Condition; }
		asCStmt* GetBody() const { return Body; }
		bool SetBody(asCStmt* InBody) { if (!CanMutate() || Body || !InBody) return false; Body = InBody; return true; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::While); }
	private:
		asCExpr* Condition = nullptr;
		asCStmt* Body = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCBreakStmt final : public asCStmt
	{
	public:
		asCBreakStmt(asCStmt* InTarget, asCSourceRange Range) : asCStmt(asEStmtKind::Break, MoveTemp(Range)), Target(InTarget) {}
		asCStmt* GetTarget() const { return Target; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Break); }
	private:
		asCStmt* Target = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCContinueStmt final : public asCStmt
	{
	public:
		asCContinueStmt(asCStmt* InTarget, asCSourceRange Range) : asCStmt(asEStmtKind::Continue, MoveTemp(Range)), Target(InTarget) {}
		asCStmt* GetTarget() const { return Target; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Continue); }
	private:
		asCStmt* Target = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCDeclStmt final : public asCStmt
	{
	public:
		asCDeclStmt(asCVarDecl* InDeclaration, asCExpr* InInitializer, asCSourceRange Range)
			: asCStmt(asEStmtKind::DeclStmt, MoveTemp(Range)), Declaration(InDeclaration), Initializer(InInitializer) {}
		asCVarDecl* GetDeclaration() const { return Declaration; }
		asCExpr* GetInitializer() const { return Initializer; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::DeclStmt); }
	private:
		asCVarDecl* Declaration = nullptr;
		asCExpr* Initializer = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCNullStmt final : public asCStmt
	{
	public:
		explicit asCNullStmt(asCSourceRange Range) : asCStmt(asEStmtKind::NullStmt, MoveTemp(Range)) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::NullStmt); }
	};

	class ANGELSCRIPTRUNTIME_API asCForStmt final : public asCStmt
	{
	public:
		asCForStmt(TArray<asCStmt*> InInitializers, asCExpr* Condition, TArray<asCExpr*> Increments,
			asCStmt* Body, asCSourceRange HeaderRange, asCSourceRange Range)
			: asCStmt(asEStmtKind::For, MoveTemp(Range)), Initializers(MoveTemp(InInitializers)),
			  Increments(MoveTemp(Increments)), HeaderRange(MoveTemp(HeaderRange)), Condition(Condition), Body(Body) {}
		TConstArrayView<asCStmt*> GetInitializers() const { return MakeArrayView(Initializers); }
		asCExpr* GetCondition() const { return Condition; }
		TConstArrayView<asCExpr*> GetIncrements() const { return MakeArrayView(Increments); }
		asCStmt* GetBody() const { return Body; }
		const asCSourceRange& GetHeaderRange() const { return HeaderRange; }
		bool SetBody(asCStmt* InBody) { if (!CanMutate() || Body || !InBody) return false; Body = InBody; return true; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::For); }
	private:
		TArray<asCStmt*> Initializers;
		TArray<asCExpr*> Increments;
		asCSourceRange HeaderRange;
		asCExpr* Condition = nullptr;
		asCStmt* Body = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCDoStmt final : public asCStmt
	{
	public:
		asCDoStmt(asCStmt* Body, asCExpr* Condition, asCSourceRange Range)
			: asCStmt(asEStmtKind::Do, MoveTemp(Range)), Body(Body), Condition(Condition) {}
		asCStmt* GetBody() const { return Body; }
		asCExpr* GetCondition() const { return Condition; }
		bool SetBody(asCStmt* Value) { if (!CanMutate() || Body || !Value) return false; Body = Value; return true; }
		bool SetCondition(asCExpr* Value) { if (!CanMutate() || Condition || !Value) return false; Condition = Value; return true; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Do); }
	private:
		asCStmt* Body = nullptr;
		asCExpr* Condition = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCRecoveryStmt final : public asCStmt
	{
	public:
		explicit asCRecoveryStmt(asCSourceRange Range) : asCStmt(asEStmtKind::RecoveryStmt, MoveTemp(Range)) { MarkInvalid(); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::RecoveryStmt); }
	};

	class ANGELSCRIPTRUNTIME_API asCDeclGroupStmt final : public asCStmt
	{
	public:
		asCDeclGroupStmt(TArray<asCStmt*> InDeclarations, asCSourceRange Range)
			: asCStmt(asEStmtKind::DeclGroupStmt, MoveTemp(Range)), Declarations(MoveTemp(InDeclarations)) {}
		TConstArrayView<asCStmt*> GetDeclarations() const { return MakeArrayView(Declarations); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::DeclGroupStmt); }
	private:
		TArray<asCStmt*> Declarations;
	};

	class ANGELSCRIPTRUNTIME_API asCClassDefaultStmt final : public asCStmt
	{
	public:
		asCClassDefaultStmt(asCStmt* InStatement, asCSourceRange InKeywordRange, asCSourceRange Range)
			: asCStmt(asEStmtKind::ClassDefaultStmt, MoveTemp(Range)), Statement(InStatement), KeywordRange(MoveTemp(InKeywordRange)) {}
		asCStmt* GetStatement() const { return Statement; }
		const asCSourceRange& GetKeywordRange() const { return KeywordRange; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ClassDefaultStmt); }
	private:
		asCStmt* Statement = nullptr;
		asCSourceRange KeywordRange;
	};

	class ANGELSCRIPTRUNTIME_API asCSwitchStmt final : public asCStmt
	{
	public:
		asCSwitchStmt(asCExpr* InSelector, asCSourceRange InHeaderRange, asCSourceRange Range)
			: asCStmt(asEStmtKind::Switch, MoveTemp(Range)), Selector(InSelector), HeaderRange(MoveTemp(InHeaderRange)) {}
		asCExpr* GetSelector() const { return Selector; }
		TConstArrayView<asCSwitchClause*> GetClauses() const { return MakeArrayView(Clauses); }
		const asCSourceRange& GetHeaderRange() const { return HeaderRange; }
		bool AddClause(asCSwitchClause* Clause) { if (!CanMutate() || !Clause) return false; Clauses.Add(Clause); return true; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Switch); }
	private:
		asCExpr* Selector = nullptr;
		TArray<asCSwitchClause*> Clauses;
		asCSourceRange HeaderRange;
	};

	class ANGELSCRIPTRUNTIME_API asCSwitchClause : public asCStmt
	{
	public:
		asCSwitchStmt* GetParentSwitch() const { return ParentSwitch; }
		TConstArrayView<asCStmt*> GetStatements() const { return MakeArrayView(Statements); }
		const asCSourceRange& GetKeywordRange() const { return KeywordRange; }
		const asCSourceRange& GetColonRange() const { return ColonRange; }
		bool AddStatement(asCStmt* Statement) { if (!CanMutate() || !Statement) return false; Statements.Add(Statement); return true; }
		static bool classof(const asCASTNode* Node)
		{
			return Node && Node->GetFamily() == asEASTNodeFamily::Stmt
				&& (Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Case)
					|| Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Default));
		}
	protected:
		asCSwitchClause(asEStmtKind Kind, asCSwitchStmt* InParentSwitch, asCSourceRange InKeywordRange,
			asCSourceRange InColonRange, asCSourceRange Range)
			: asCStmt(Kind, MoveTemp(Range)), ParentSwitch(InParentSwitch),
			  KeywordRange(MoveTemp(InKeywordRange)), ColonRange(MoveTemp(InColonRange)) {}
	private:
		asCSwitchStmt* ParentSwitch = nullptr;
		TArray<asCStmt*> Statements;
		asCSourceRange KeywordRange;
		asCSourceRange ColonRange;
	};

	class ANGELSCRIPTRUNTIME_API asCCaseStmt final : public asCSwitchClause
	{
	public:
		asCCaseStmt(asCSwitchStmt* ParentSwitch, asCExpr* InLabel, asCSourceRange KeywordRange,
			asCSourceRange ColonRange, asCSourceRange Range)
			: asCSwitchClause(asEStmtKind::Case, ParentSwitch, MoveTemp(KeywordRange), MoveTemp(ColonRange), MoveTemp(Range)), Label(InLabel) {}
		asCExpr* GetLabel() const { return Label; }
		bool HasResolvedValue() const { return bResolvedValue; }
		uint64 GetResolvedValueBits() const { return ResolvedValueBits; }
		bool SetResolvedValueBits(uint64 Value) { if (!CanMutate() || bResolvedValue) return false; ResolvedValueBits = Value; bResolvedValue = true; return true; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Case); }
	private:
		asCExpr* Label = nullptr;
		uint64 ResolvedValueBits = 0;
		bool bResolvedValue = false;
	};

	class ANGELSCRIPTRUNTIME_API asCDefaultStmt final : public asCSwitchClause
	{
	public:
		asCDefaultStmt(asCSwitchStmt* ParentSwitch, asCSourceRange KeywordRange,
			asCSourceRange ColonRange, asCSourceRange Range)
			: asCSwitchClause(asEStmtKind::Default, ParentSwitch, MoveTemp(KeywordRange), MoveTemp(ColonRange), MoveTemp(Range)) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Default); }
	};

	class ANGELSCRIPTRUNTIME_API asCFallthroughStmt final : public asCStmt
	{
	public:
		asCFallthroughStmt(asCSwitchClause* InTarget, asCSourceRange Range)
			: asCStmt(asEStmtKind::Fallthrough, MoveTemp(Range)), Target(InTarget) {}
		asCSwitchClause* GetTarget() const { return Target; }
		bool SetTarget(asCSwitchClause* Value) { if (!CanMutate() || Target || !Value) return false; Target = Value; return true; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Fallthrough); }
	private:
		asCSwitchClause* Target = nullptr;
	};

	struct asSForeachProtocol
	{
		asCFunctionDecl* BeginFunction = nullptr;
		asCFunctionDecl* EndFunction = nullptr;
		asCFunctionDecl* NextFunction = nullptr;
		asCFunctionDecl* ValueFunction = nullptr;
		asCFunctionDecl* KeyFunction = nullptr;
		asCQualType ContainerType;
		asCQualType IteratorType;
	};

	class ANGELSCRIPTRUNTIME_API asCForeachStmt final : public asCStmt
	{
	public:
		asCForeachStmt(asCDeclStmt* InValue, asCDeclStmt* InKey, asCExpr* InRangeExpression,
			asSForeachProtocol InProtocol, asCSourceRange InHeaderRange, asCSourceRange InColonRange,
			asCSourceRange Range)
			: asCStmt(asEStmtKind::Foreach, MoveTemp(Range)), Protocol(MoveTemp(InProtocol)),
			  HeaderRange(MoveTemp(InHeaderRange)), ColonRange(MoveTemp(InColonRange)), Value(InValue),
			  Key(InKey), RangeExpression(InRangeExpression) {}
		asCDeclStmt* GetValueStatement() const { return Value; }
		asCDeclStmt* GetKeyStatement() const { return Key; }
		asCExpr* GetRangeExpression() const { return RangeExpression; }
		asCStmt* GetBody() const { return Body; }
		const asSForeachProtocol& GetProtocol() const { return Protocol; }
		const asCSourceRange& GetHeaderRange() const { return HeaderRange; }
		const asCSourceRange& GetColonRange() const { return ColonRange; }
		bool SetBody(asCStmt* InBody) { if (!CanMutate() || Body || !InBody) return false; Body = InBody; return true; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::Foreach); }
	private:
		asSForeachProtocol Protocol;
		asCSourceRange HeaderRange;
		asCSourceRange ColonRange;
		asCDeclStmt* Value = nullptr;
		asCDeclStmt* Key = nullptr;
		asCExpr* RangeExpression = nullptr;
		asCStmt* Body = nullptr;
	};

END_AS_NAMESPACE
