/* Maintained Unreal AngelScript fork — concrete typed expression leaves. */
#pragma once

#include "frontend/as_stmt.h"
#include "frontend/as_type_identity.h"

BEGIN_AS_NAMESPACE

class asCScriptFunction;

class ANGELSCRIPTRUNTIME_API asCBooleanLiteral final : public asCExpr
	{
	public:
		asCBooleanLiteral(bool Value, asCSourceRange Range) : asCExpr(asEStmtKind::BooleanLiteral, MoveTemp(Range)), Value(Value) {}
		bool GetValue() const { return Value; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::BooleanLiteral); }
	private:
		bool Value = false;
	};

	class ANGELSCRIPTRUNTIME_API asCFloatingLiteral final : public asCExpr
	{
	public:
		asCFloatingLiteral(double Value, asCSourceRange Range) : asCExpr(asEStmtKind::FloatingLiteral, MoveTemp(Range)), Value(Value) {}
		double GetValue() const { return Value; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::FloatingLiteral); }
	private:
		double Value = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCParenExpr final : public asCExpr
	{
	public:
		asCParenExpr(asCExpr* SubExpr, asCSourceRange Range) : asCExpr(asEStmtKind::ParenExpr, MoveTemp(Range)), SubExpr(SubExpr) {}
		asCExpr* GetSubExpr() const { return SubExpr; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ParenExpr); }
	private:
		asCExpr* SubExpr = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCUnaryOperator final : public asCExpr
	{
	public:
		asCUnaryOperator(asEUnaryOperator Operator, asCExpr* Operand, asCSourceRange OperatorRange, asCSourceRange Range)
			: asCExpr(asEStmtKind::UnaryOperator, MoveTemp(Range)), Operand(Operand), OperatorRange(OperatorRange), Operator(Operator) {}
		asEUnaryOperator GetOperator() const { return Operator; }
		asCExpr* GetOperand() const { return Operand; }
		const asCSourceRange& GetOperatorRange() const { return OperatorRange; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::UnaryOperator); }
	private:
		asCExpr* Operand = nullptr;
		asCSourceRange OperatorRange;
		asEUnaryOperator Operator;
	};

	class ANGELSCRIPTRUNTIME_API asCBinaryOperator final : public asCExpr
	{
	public:
		asCBinaryOperator(asEBinaryOperator Operator, asCExpr* Left, asCExpr* Right, asCSourceRange OperatorRange, asCSourceRange Range)
			: asCExpr(asEStmtKind::BinaryOperator, MoveTemp(Range)), Left(Left), Right(Right), OperatorRange(OperatorRange), Operator(Operator) {}
		asEBinaryOperator GetOperator() const { return Operator; }
		asCExpr* GetLeft() const { return Left; }
		asCExpr* GetRight() const { return Right; }
		const asCSourceRange& GetOperatorRange() const { return OperatorRange; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::BinaryOperator); }
	private:
		asCExpr* Left = nullptr;
		asCExpr* Right = nullptr;
		asCSourceRange OperatorRange;
		asEBinaryOperator Operator;
	};

	class ANGELSCRIPTRUNTIME_API asCConditionalOperator final : public asCExpr
	{
	public:
		asCConditionalOperator(asCExpr* Condition, asCExpr* TrueExpr, asCExpr* FalseExpr, asCSourceRange Range)
			: asCExpr(asEStmtKind::ConditionalOperator, MoveTemp(Range)), Condition(Condition), TrueExpr(TrueExpr), FalseExpr(FalseExpr) {}
		asCExpr* GetCondition() const { return Condition; }
		asCExpr* GetTrueExpr() const { return TrueExpr; }
		asCExpr* GetFalseExpr() const { return FalseExpr; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ConditionalOperator); }
	private:
		asCExpr* Condition = nullptr;
		asCExpr* TrueExpr = nullptr;
		asCExpr* FalseExpr = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCIntegerLiteral final : public asCExpr
	{
	public:
		asCIntegerLiteral(int64 Value, asCSourceRange Range) : asCExpr(asEStmtKind::IntegerLiteral, MoveTemp(Range)), Value(Value) {}
		int64 GetValue() const { return Value; }
		uint64 GetUnsignedValue() const { return static_cast<uint64>(Value); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::IntegerLiteral); }
	private:
		int64 Value = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCDeclRefExpr final : public asCExpr
	{
	public:
		asCDeclRefExpr(asCDecl* Decl, asCSourceRange Range) : asCExpr(asEStmtKind::DeclRefExpr, MoveTemp(Range)), Decl(Decl) {}
		asCDeclRefExpr(asCScriptFunction* ExternalFunction, asSStableKey ExternalIdentity, asCSourceRange Range)
			: asCExpr(asEStmtKind::DeclRefExpr, MoveTemp(Range)), ExternalFunction(ExternalFunction), ExternalIdentity(MoveTemp(ExternalIdentity)) {}
		asCDecl* GetDecl() const { return Decl; }
		asCScriptFunction* GetExternalFunction() const { return ExternalFunction; }
		const asSStableKey& GetExternalIdentity() const { return ExternalIdentity; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::DeclRefExpr); }
	private:
		asCDecl* Decl = nullptr;
		asCScriptFunction* ExternalFunction = nullptr;
		asSStableKey ExternalIdentity;
	};

	class ANGELSCRIPTRUNTIME_API asCCallExpr final : public asCExpr
	{
	public:
		asCCallExpr(asCExpr* Callee, asCSourceRange Range) : asCExpr(asEStmtKind::CallExpr, MoveTemp(Range)), Callee(Callee) {}
		bool AddArgument(asCExpr* Argument) { if (!CanMutate() || !Argument) return false; Arguments.Add(Argument); return true; }
		asCExpr* GetCallee() const { return Callee; }
		TConstArrayView<asCExpr*> GetArguments() const { return MakeArrayView(Arguments); }
		bool SetArgumentSourceOrdinals(TArray<int32> Ordinals) { if (!CanMutate() || Ordinals.Num() != Arguments.Num()) return false; ArgumentSourceOrdinals = MoveTemp(Ordinals); return true; }
		TConstArrayView<int32> GetArgumentSourceOrdinals() const { return MakeArrayView(ArgumentSourceOrdinals); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::CallExpr); }
	private:
		asCExpr* Callee = nullptr;
		TArray<asCExpr*> Arguments;
		TArray<int32> ArgumentSourceOrdinals;
	};


	class ANGELSCRIPTRUNTIME_API asCMemberExpr final : public asCExpr
	{
	public:
		asCMemberExpr(asCExpr* Base, asCNamedDecl* Member, asCSourceRange MemberRange, asCSourceRange Range)
			: asCExpr(asEStmtKind::MemberExpr, MoveTemp(Range)), Base(Base), Member(Member), MemberRange(MemberRange) {}
		asCExpr* GetBase() const { return Base; }
		asCNamedDecl* GetMember() const { return Member; }
		const asCSourceRange& GetMemberRange() const { return MemberRange; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::MemberExpr); }
	private:
		asCExpr* Base = nullptr;
		asCNamedDecl* Member = nullptr;
		asCSourceRange MemberRange;
	};

	class ANGELSCRIPTRUNTIME_API asCArraySubscriptExpr final : public asCExpr
	{
	public:
		asCArraySubscriptExpr(asCExpr* Base, TArray<asCExpr*> Arguments, asCFunctionDecl* OperatorFunction, asCSourceRange Range)
			: asCExpr(asEStmtKind::ArraySubscriptExpr, MoveTemp(Range)), Base(Base), Arguments(MoveTemp(Arguments)), OperatorFunction(OperatorFunction) {}
		asCExpr* GetBase() const { return Base; }
		TConstArrayView<asCExpr*> GetArguments() const { return MakeArrayView(Arguments); }
		asCFunctionDecl* GetOperatorFunction() const { return OperatorFunction; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ArraySubscriptExpr); }
	private:
		asCExpr* Base = nullptr;
		TArray<asCExpr*> Arguments;
		asCFunctionDecl* OperatorFunction = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCExplicitCastExpr final : public asCExpr
	{
	public:
		asCExplicitCastExpr(asCExpr* Operand, asSTypeSyntax TypeSyntax, asCSourceRange Range)
			: asCExpr(asEStmtKind::ExplicitCastExpr, MoveTemp(Range)), Operand(Operand), TypeSyntax(MoveTemp(TypeSyntax)) {}
		asCExpr* GetOperand() const { return Operand; }
		const asSTypeSyntax& GetTypeSyntax() const { return TypeSyntax; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ExplicitCastExpr); }
	private:
		asCExpr* Operand = nullptr;
		asSTypeSyntax TypeSyntax;
	};

	class ANGELSCRIPTRUNTIME_API asCConstructExpr final : public asCExpr
	{
	public:
		asCConstructExpr(asSTypeSyntax TypeSyntax, TArray<asCExpr*> Arguments, asCFunctionDecl* Constructor, asCSourceRange Range)
			: asCExpr(asEStmtKind::ConstructExpr, MoveTemp(Range)), TypeSyntax(MoveTemp(TypeSyntax)), Arguments(MoveTemp(Arguments)), Constructor(Constructor) {}
		const asSTypeSyntax& GetTypeSyntax() const { return TypeSyntax; }
		TConstArrayView<asCExpr*> GetArguments() const { return MakeArrayView(Arguments); }
		asCFunctionDecl* GetConstructor() const { return Constructor; }
		bool SetArgumentSourceOrdinals(TArray<int32> Ordinals) { if (!CanMutate() || Ordinals.Num() != Arguments.Num()) return false; ArgumentSourceOrdinals = MoveTemp(Ordinals); return true; }
		TConstArrayView<int32> GetArgumentSourceOrdinals() const { return MakeArrayView(ArgumentSourceOrdinals); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ConstructExpr); }
	private:
		asSTypeSyntax TypeSyntax;
		TArray<asCExpr*> Arguments;
		TArray<int32> ArgumentSourceOrdinals;
		asCFunctionDecl* Constructor = nullptr;
	};

	class ANGELSCRIPTRUNTIME_API asCStringLiteral final : public asCExpr
	{
	public:
		asCStringLiteral(FString Value, asCSourceRange Range) : asCExpr(asEStmtKind::StringLiteral, MoveTemp(Range)), Value(MoveTemp(Value)) {}
		const FString& GetValue() const { return Value; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::StringLiteral); }
	private:
		FString Value;
	};

	class ANGELSCRIPTRUNTIME_API asCNullPtrLiteral final : public asCExpr
	{
	public:
		explicit asCNullPtrLiteral(asCSourceRange Range) : asCExpr(asEStmtKind::NullPtrLiteral, MoveTemp(Range)) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::NullPtrLiteral); }
	};

	class ANGELSCRIPTRUNTIME_API asCDefaultArgumentExpr final : public asCExpr
	{
	public:
		asCDefaultArgumentExpr(asCVarDecl* Parameter, asCSourceRange Range)
			: asCExpr(asEStmtKind::DefaultArgumentExpr, MoveTemp(Range)), Parameter(Parameter) {}
		asCVarDecl* GetParameter() const { return Parameter; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::DefaultArgumentExpr); }
	private:
		asCVarDecl* Parameter = nullptr;
	};


	class ANGELSCRIPTRUNTIME_API asCThisExpr final : public asCExpr
	{
	public:
		asCThisExpr(asCRecordDecl* Record, bool bImplicit, asCSourceRange Range)
			: asCExpr(asEStmtKind::ThisExpr, MoveTemp(Range)), Record(Record), bImplicit(bImplicit) {}
		asCRecordDecl* GetRecord() const { return Record; }
		bool IsImplicit() const { return bImplicit; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ThisExpr); }
	private:
		asCRecordDecl* Record = nullptr;
		bool bImplicit = false;
	};


	class ANGELSCRIPTRUNTIME_API asCInitListExpr final : public asCExpr
	{
	public:
		asCInitListExpr(TArray<asCExpr*> Elements, asCSourceRange Range)
			: asCExpr(asEStmtKind::InitListExpr, MoveTemp(Range)), Elements(MoveTemp(Elements)) {}
		TConstArrayView<asCExpr*> GetElements() const { return MakeArrayView(Elements); }
		const asSStableKey& GetInitializerIdentity() const { return InitializerIdentity; }
		TConstArrayView<int32> GetElementPatternIndices() const { return MakeArrayView(ElementPatternIndices); }
		bool SetElement(int32 Index, asCExpr* Value) { if (!CanMutate() || !Value || !Elements.IsValidIndex(Index)) return false; Elements[Index] = Value; return true; }
		bool SetInitializerBinding(asSStableKey Function, TArray<int32> Indices)
		{
			if (!CanMutate() || Function.IsZero() || Indices.Num() != Elements.Num()) return false;
			InitializerIdentity = MoveTemp(Function); ElementPatternIndices = MoveTemp(Indices); return true;
		}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::InitListExpr); }
	private:
		TArray<asCExpr*> Elements;
		asSStableKey InitializerIdentity;
		TArray<int32> ElementPatternIndices;
	};

	class ANGELSCRIPTRUNTIME_API asCValueInitExpr final : public asCExpr
	{
	public:
		explicit asCValueInitExpr(asCSourceRange Range) : asCExpr(asEStmtKind::ValueInitExpr, MoveTemp(Range)) {}
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ValueInitExpr); }
	};

	class ANGELSCRIPTRUNTIME_API asCImplicitCastExpr final : public asCExpr
	{
	public:
		asCImplicitCastExpr(asCExpr* InOperand, asEImplicitConversionKind InKind, asCSourceRange Range)
			: asCExpr(asEStmtKind::ImplicitCastExpr, MoveTemp(Range)), Operand(InOperand), ConversionKind(InKind) {}
		asCExpr* GetOperand() const { return Operand; }
		asEImplicitConversionKind GetConversionKind() const { return ConversionKind; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::ImplicitCastExpr); }
	private:
		asCExpr* Operand = nullptr;
		asEImplicitConversionKind ConversionKind = asEImplicitConversionKind::None;
	};

	class ANGELSCRIPTRUNTIME_API asCUserConversionExpr final : public asCExpr
	{
	public:
		asCUserConversionExpr(asCExpr* InOperand, asCFunctionDecl* InFunction,
			asEUserConversionMode InMode, asCSourceRange Range)
			: asCExpr(asEStmtKind::UserConversionExpr, MoveTemp(Range))
			, Operand(InOperand), Function(InFunction), Mode(InMode) {}
		asCExpr* GetOperand() const { return Operand; }
		asCFunctionDecl* GetConversionFunction() const { return Function; }
		asEUserConversionMode GetMode() const { return Mode; }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::UserConversionExpr); }
	private:
		asCExpr* Operand = nullptr;
		asCFunctionDecl* Function = nullptr;
		asEUserConversionMode Mode = asEUserConversionMode::Implicit;
	};

	class ANGELSCRIPTRUNTIME_API asCRecoveryExpr final : public asCExpr
	{
	public:
		asCRecoveryExpr(asCType* ErrorType, asCSourceRange Range) : asCExpr(asEStmtKind::RecoveryExpr, MoveTemp(Range))
		{
			SetExpressionSemantics(ErrorType, asEValueCategory::PRValue);
			MarkInvalid();
		}
		explicit asCRecoveryExpr(asCSourceRange Range) : asCExpr(asEStmtKind::RecoveryExpr, MoveTemp(Range)) { MarkInvalid(); }
		static bool classof(const asCASTNode* Node) { return Node && Node->GetFamily() == asEASTNodeFamily::Stmt && Node->GetRawKind() == static_cast<uint16>(asEStmtKind::RecoveryExpr); }
	};

END_AS_NAMESPACE
