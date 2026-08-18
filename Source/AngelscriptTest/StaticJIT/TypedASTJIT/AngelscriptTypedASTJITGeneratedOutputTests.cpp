#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "Core/AngelscriptEngine.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITProviderEmitter.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_typeinfo.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITGeneratedOutputTests,
	"Angelscript.TestModule.StaticJIT.GeneratedOutput.TypedASTJIT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ECapability = EAngelscriptTypedASTJITDirectCapability;

	static FAngelscriptTypedASTJITGenerationCapabilityProfile MakeProfile(
		const ECapability Capabilities)
	{
		FAngelscriptTypedASTJITGenerationCapabilityProfile Profile;
		FString Error;
		check(BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
			Capabilities, Profile, &Error));
		check(Error.IsEmpty());
		return Profile;
	}

	static asSTypedSemanticSourceSpan MakeSpan()
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = "typed_ast_scalar.as";
		Span.sourceLength = 1;
		return Span;
	}

	static asTypedSemanticSymbolId AddSymbol(
		asCTypedSemanticFunction& Function,
		const asETypedSemanticSymbolKind Kind,
		const char* Name,
		const asCDataType& Type =
			asCDataType::CreatePrimitive(ttInt, false))
	{
		asSTypedSemanticSymbol Symbol;
		Symbol.kind = Kind;
		Symbol.name = Name;
		Symbol.type = Type;
		Symbol.span = MakeSpan();
		return Function.AddSymbol(Symbol);
	}

	static asTypedSemanticExpressionId AddRead(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticSymbolId Symbol)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Symbol;
		Expression.type = Function.symbols[Symbol.value].type;
		Expression.span = MakeSpan();
		Expression.symbol = Symbol;
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticExpressionId AddLiteral(
		asCTypedSemanticFunction& Function,
		const char* Value,
		const asCDataType& Type =
			asCDataType::CreatePrimitive(ttInt, false))
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Literal;
		Expression.type = Type;
		Expression.span = MakeSpan();
		Expression.literal = Value;
		return Function.AddExpression(Expression);
	}

	static void AddEvaluationStep(
		asSTypedSemanticExpression& Expression,
		const asETypedSemanticEvaluationRole Role,
		const asTypedSemanticExpressionId Input,
		const int32 Formal = INDEX_NONE,
		const int32 SourceOrdinal = INDEX_NONE)
	{
		asSTypedSemanticEvaluationStep Step;
		Step.role = Role;
		Step.expression = Input;
		Step.formalParameterIndex = Formal;
		Step.sourceArgumentOrdinal = SourceOrdinal;
		Expression.evaluationSteps.PushLast(Step);
	}

	static asTypedSemanticExpressionId AddConversion(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId Value,
		const asCDataType& TargetType)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Conversion;
		Expression.type = TargetType;
		Expression.span = MakeSpan();
		Expression.operands.PushLast(Value);
		AddEvaluationStep(
			Expression,
			asETypedSemanticEvaluationRole::Operand,
			Value);
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticExpressionId AddUnary(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId Operand,
		const char* Operator,
		const asCDataType& Type)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Unary;
		Expression.type = Type;
		Expression.span = MakeSpan();
		Expression.operands.PushLast(Operand);
		Expression.operatorToken = Operator;
		AddEvaluationStep(
			Expression,
			asETypedSemanticEvaluationRole::Operand,
			Operand);
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticExpressionId AddTypedBinary(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId Left,
		const asTypedSemanticExpressionId Right,
		const char* Operator,
		const asCDataType& Type)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Binary;
		Expression.type = Type;
		Expression.span = MakeSpan();
		Expression.operands.PushLast(Left);
		Expression.operands.PushLast(Right);
		Expression.operatorToken = Operator;
		AddEvaluationStep(
			Expression,
			asETypedSemanticEvaluationRole::Operand,
			Left);
		AddEvaluationStep(
			Expression,
			asETypedSemanticEvaluationRole::Operand,
			Right);
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticExpressionId AddMutation(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId Target,
		const asTypedSemanticExpressionId Value,
		const char* Operator,
		const bool bReturnsOldValue)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Assignment;
		Expression.type = Function.expressions[Target.value].type;
		Expression.span = MakeSpan();
		Expression.operatorToken = Operator;
		Expression.mutationTarget = Target;
		Expression.mutationValue = Value;
		Expression.mutationReturnsOldValue = bReturnsOldValue;
		if (Value.IsValid())
		{
			AddEvaluationStep(
				Expression,
				asETypedSemanticEvaluationRole::MutationValue,
				Value);
		}
		AddEvaluationStep(
			Expression,
			asETypedSemanticEvaluationRole::MutationTarget,
			Target);
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticExpressionId AddBinary(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId Left,
		const asTypedSemanticExpressionId Right,
		const char* Operator,
		const bool bBoolean = false)
	{
		asSTypedSemanticExpression Expression;
		Expression.kind = asETypedSemanticExpressionKind::Binary;
		Expression.type = asCDataType::CreatePrimitive(
			bBoolean ? ttBool : ttInt,
			false);
		Expression.span = MakeSpan();
		Expression.operands.PushLast(Left);
		Expression.operands.PushLast(Right);
		Expression.operatorToken = Operator;
		AddEvaluationStep(
			Expression,
			asETypedSemanticEvaluationRole::Operand,
			Left);
		AddEvaluationStep(
			Expression,
			asETypedSemanticEvaluationRole::Operand,
			Right);
		return Function.AddExpression(Expression);
	}

	static asTypedSemanticStatementId AddReturn(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId Value)
	{
		asSTypedSemanticStatement Statement;
		Statement.kind = asETypedSemanticStatementKind::Return;
		Statement.span = MakeSpan();
		Statement.safePointRole = asETypedSemanticSafePointRole::Return;
		Statement.valueExpression = Value;
		Statement.exitedScopeCount = 1;
		return Function.AddStatement(Statement);
	}

	static void MarkVerifiedEmptyCleanup(
		asCTypedSemanticFunction& Function)
	{
		asSTypedSemanticCleanupPlan EmptyCleanup;
		Function.header.universalCleanupPlan =
			Function.AddCleanupPlan(EmptyCleanup);
		Function.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::VerifiedEmpty;
		Function.header.cleanupPlanCoversAllTransfers = true;
		for (asUINT StatementIndex = 0;
			StatementIndex < Function.statements.GetLength();
			++StatementIndex)
		{
			asSTypedSemanticStatement& Statement =
				Function.statements[StatementIndex];
			if (Statement.kind == asETypedSemanticStatementKind::Return
				|| Statement.kind == asETypedSemanticStatementKind::Break
				|| Statement.kind == asETypedSemanticStatementKind::Continue)
			{
				Statement.cleanupPlan =
					Function.header.universalCleanupPlan;
			}
		}
	}

	static FAngelscriptHash256 RepeatedByteHash(const uint8 Value)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (uint8& Byte : Bytes)
		{
			Byte = Value;
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static asCTypedSemanticFunction MakeFoldedGlobalFunction(
		const asCDataType& Type,
		const asQWORD LiteralBits)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int ReadFoldedGlobal()";

		asSTypedSemanticExpression Folded;
		Folded.kind =
			asETypedSemanticExpressionKind::FoldedGlobalConstant;
		Folded.type = Type;
		Folded.span = MakeSpan();
		Folded.resolvedGlobalPropertyId = 23;
		Folded.literalBits = LiteralBits;

		const asTypedSemanticStatementId ReturnId = AddReturn(
			Function, Function.AddExpression(Folded));
		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static FAngelscriptTypedASTJITGlobalDependency
	MakeFoldedGlobalDependency()
	{
		FAngelscriptTypedASTJITGlobalDependency Dependency;
		Dependency.EngineLocalGlobalPropertyId = 23;
		Dependency.Kind =
			EAngelscriptArtifactSemanticDependencyKind::HardValue;
		Dependency.Reference.Kind =
			EAngelscriptArtifactReferenceKind::ScriptGlobal;
		Dependency.Reference.StableKey = RepeatedByteHash(0x41);
		Dependency.Reference.ExpectedAbi = RepeatedByteHash(0x42);
		Dependency.ExpectedContentOrValue = RepeatedByteHash(0x43);
		return Dependency;
	}

	static asTypedSemanticStatementId AddExpressionStatement(
		asCTypedSemanticFunction& Function,
		const asTypedSemanticExpressionId Expression,
		const asETypedSemanticLoopPhase LoopPhase =
			asETypedSemanticLoopPhase::None)
	{
		asSTypedSemanticStatement Statement;
		Statement.kind = asETypedSemanticStatementKind::Expression;
		Statement.span = MakeSpan();
		Statement.loopPhase = LoopPhase;
		Statement.valueExpression = Expression;
		return Function.AddStatement(Statement);
	}

	static asTypedSemanticStatementId AddBlock(
		asCTypedSemanticFunction& Function,
		std::initializer_list<asTypedSemanticStatementId> Children,
		const asETypedSemanticLoopPhase LoopPhase =
			asETypedSemanticLoopPhase::None,
		const asETypedSemanticSafePointRole SafePointRole =
			asETypedSemanticSafePointRole::None)
	{
		asSTypedSemanticStatement Statement;
		Statement.kind = asETypedSemanticStatementKind::Block;
		Statement.span = MakeSpan();
		Statement.loopPhase = LoopPhase;
		Statement.safePointRole = SafePointRole;
		for (const asTypedSemanticStatementId Child : Children)
		{
			Statement.children.PushLast(Child);
		}
		return Function.AddStatement(Statement);
	}

	static asTypedSemanticStatementId AddTransfer(
		asCTypedSemanticFunction& Function,
		const asETypedSemanticStatementKind Kind,
		const asTypedSemanticStatementId Target)
	{
		asSTypedSemanticStatement Statement;
		Statement.kind = Kind;
		Statement.span = MakeSpan();
		Statement.safePointRole =
			asETypedSemanticSafePointRole::Transfer;
		Statement.targetStatement = Target;
		Statement.exitedScopeCount = 1;
		return Function.AddStatement(Statement);
	}

	static asCTypedSemanticFunction MakeScalarBranch()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int SemanticScalarBranch(int A, int B)";

		const asTypedSemanticSymbolId A = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "A");
		const asTypedSemanticSymbolId B = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "B");
		const asTypedSemanticSymbolId Sum = AddSymbol(
			Function, asETypedSemanticSymbolKind::Local, "Sum");

		const asTypedSemanticExpressionId Add = AddBinary(
			Function, AddRead(Function, A), AddRead(Function, B), "+");
		const asTypedSemanticExpressionId Greater = AddBinary(
			Function, AddRead(Function, Sum), AddLiteral(Function, "10"), ">", true);
		const asTypedSemanticExpressionId Multiply = AddBinary(
			Function, AddRead(Function, Sum), AddLiteral(Function, "2"), "*");
		const asTypedSemanticExpressionId Subtract = AddBinary(
			Function, AddRead(Function, Sum), AddLiteral(Function, "1"), "-");

		asSTypedSemanticStatement Local;
		Local.kind = asETypedSemanticStatementKind::LocalDeclaration;
		Local.span = MakeSpan();
		Local.symbol = Sum;
		Local.initializerExpression = Add;
		const asTypedSemanticStatementId LocalId = Function.AddStatement(Local);

		const asTypedSemanticStatementId ThenReturn = AddReturn(Function, Multiply);
		asSTypedSemanticStatement Then;
		Then.kind = asETypedSemanticStatementKind::Block;
		Then.span = MakeSpan();
		Then.children.PushLast(ThenReturn);
		const asTypedSemanticStatementId ThenId = Function.AddStatement(Then);

		asSTypedSemanticStatement If;
		If.kind = asETypedSemanticStatementKind::If;
		If.span = MakeSpan();
		If.conditionExpression = Greater;
		If.thenStatement = ThenId;
		const asTypedSemanticStatementId IfId = Function.AddStatement(If);

		const asTypedSemanticStatementId FinalReturn = AddReturn(Function, Subtract);
		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(LocalId);
		Root.children.PushLast(IfId);
		Root.children.PushLast(FinalReturn);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeShortCircuit()
	{
		asCTypedSemanticFunction Function;
		Function.declaration =
			"bool SemanticShortCircuit(bool Left, bool Right)";
		const asCDataType BoolType =
			asCDataType::CreatePrimitive(ttBool, false);

		const asTypedSemanticSymbolId Left = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Left", BoolType);
		const asTypedSemanticSymbolId Right = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Right", BoolType);

		asSTypedSemanticExpression ShortCircuit;
		ShortCircuit.kind = asETypedSemanticExpressionKind::ShortCircuit;
		ShortCircuit.type = BoolType;
		ShortCircuit.span = MakeSpan();
		ShortCircuit.operands.PushLast(AddRead(Function, Left));
		ShortCircuit.operands.PushLast(AddRead(Function, Right));
		ShortCircuit.operatorToken = "&&";
		const asTypedSemanticStatementId ReturnId = AddReturn(
			Function, Function.AddExpression(ShortCircuit));

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakePrimitiveIdentity(
		const asCDataType& Type)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "identity";
		const asTypedSemanticSymbolId Value = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Value", Type);
		const asTypedSemanticStatementId ReturnId = AddReturn(
			Function, AddRead(Function, Value));

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeScalarConversionAndLiteral()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int64 ScalarConversion(int Value)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asCDataType Int8Type =
			asCDataType::CreatePrimitive(ttInt8, false);
		const asCDataType Int64Type =
			asCDataType::CreatePrimitive(ttInt64, false);

		const asTypedSemanticSymbolId Value = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Value", IntType);
		const asTypedSemanticSymbolId Tiny = AddSymbol(
			Function, asETypedSemanticSymbolKind::Local, "Tiny", Int8Type);
		const asTypedSemanticSymbolId Wide = AddSymbol(
			Function, asETypedSemanticSymbolKind::Local, "Wide", Int64Type);

		asSTypedSemanticStatement TinyLocal;
		TinyLocal.kind = asETypedSemanticStatementKind::LocalDeclaration;
		TinyLocal.span = MakeSpan();
		TinyLocal.symbol = Tiny;
		TinyLocal.initializerExpression =
			AddLiteral(Function, "7", Int8Type);
		const asTypedSemanticStatementId TinyLocalId =
			Function.AddStatement(TinyLocal);

		asSTypedSemanticStatement WideLocal;
		WideLocal.kind = asETypedSemanticStatementKind::LocalDeclaration;
		WideLocal.span = MakeSpan();
		WideLocal.symbol = Wide;
		WideLocal.initializerExpression = AddConversion(
			Function, AddRead(Function, Value), Int64Type);
		const asTypedSemanticStatementId WideLocalId =
			Function.AddStatement(WideLocal);

		const asTypedSemanticStatementId ReturnId = AddReturn(
			Function, AddRead(Function, Wide));
		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(TinyLocalId);
		Root.children.PushLast(WideLocalId);
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeScalarOperators()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int ScalarOperators(int Left, int Right)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asTypedSemanticSymbolId Left = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Left", IntType);
		const asTypedSemanticSymbolId Right = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Right", IntType);

		const asTypedSemanticExpressionId Negated = AddUnary(
			Function, AddRead(Function, Left), "-", IntType);
		const asTypedSemanticExpressionId Inverted = AddUnary(
			Function, AddRead(Function, Right), "~", IntType);
		const asTypedSemanticExpressionId Product = AddTypedBinary(
			Function, Negated, Inverted, "*", IntType);
		const asTypedSemanticExpressionId LogicalShifted = AddTypedBinary(
			Function,
			AddRead(Function, Left),
			AddLiteral(Function, "33"),
			">>",
			IntType);
		const asTypedSemanticExpressionId Difference = AddTypedBinary(
			Function, Product, LogicalShifted, "-", IntType);
		const asTypedSemanticExpressionId LeftShifted = AddTypedBinary(
			Function,
			AddRead(Function, Right),
			AddLiteral(Function, "33"),
			"<<",
			IntType);
		const asTypedSemanticExpressionId ArithmeticShifted = AddTypedBinary(
			Function,
			LeftShifted,
			AddLiteral(Function, "34"),
			">>>",
			IntType);
		const asTypedSemanticExpressionId Sum = AddTypedBinary(
			Function, Difference, ArithmeticShifted, "+", IntType);
		const asTypedSemanticStatementId ReturnId = AddReturn(Function, Sum);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeCheckedIntegerDivision()
	{
		asCTypedSemanticFunction Function;
		Function.declaration =
			"int CheckedIntegerDivision(int Numerator, int Denominator)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asTypedSemanticSymbolId Numerator = AddSymbol(
			Function,
			asETypedSemanticSymbolKind::Parameter,
			"Numerator",
			IntType);
		const asTypedSemanticSymbolId Denominator = AddSymbol(
			Function,
			asETypedSemanticSymbolKind::Parameter,
			"Denominator",
			IntType);
		const asTypedSemanticExpressionId Quotient = AddTypedBinary(
			Function,
			AddRead(Function, Numerator),
			AddRead(Function, Denominator),
			"/",
			IntType);
		const asTypedSemanticStatementId ReturnId = AddReturn(
			Function,
			Quotient);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeScalarMutations()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int ScalarMutations(int Input)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asTypedSemanticSymbolId Input = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Input", IntType);
		const asTypedSemanticSymbolId Value = AddSymbol(
			Function, asETypedSemanticSymbolKind::Local, "Value", IntType);
		const asTypedSemanticSymbolId PrefixResult = AddSymbol(
			Function, asETypedSemanticSymbolKind::Local, "PrefixResult", IntType);

		asSTypedSemanticStatement ValueLocal;
		ValueLocal.kind = asETypedSemanticStatementKind::LocalDeclaration;
		ValueLocal.span = MakeSpan();
		ValueLocal.symbol = Value;
		ValueLocal.initializerExpression = AddLiteral(Function, "1");
		const asTypedSemanticStatementId ValueLocalId =
			Function.AddStatement(ValueLocal);

		const asTypedSemanticExpressionId AssignValue =
			AddRead(Function, Input);
		const asTypedSemanticExpressionId AssignTarget =
			AddRead(Function, Value);
		const asTypedSemanticExpressionId Assign = AddMutation(
			Function,
			AssignTarget,
			AssignValue,
			"=",
			false);
		asSTypedSemanticStatement AssignStatement;
		AssignStatement.kind = asETypedSemanticStatementKind::Expression;
		AssignStatement.span = MakeSpan();
		AssignStatement.valueExpression = Assign;
		const asTypedSemanticStatementId AssignStatementId =
			Function.AddStatement(AssignStatement);

		const asTypedSemanticExpressionId CompoundValue =
			AddLiteral(Function, "3");
		const asTypedSemanticExpressionId CompoundTarget =
			AddRead(Function, Value);
		const asTypedSemanticExpressionId Compound = AddMutation(
			Function,
			CompoundTarget,
			CompoundValue,
			"+=",
			false);
		asSTypedSemanticStatement CompoundStatement;
		CompoundStatement.kind = asETypedSemanticStatementKind::Expression;
		CompoundStatement.span = MakeSpan();
		CompoundStatement.valueExpression = Compound;
		const asTypedSemanticStatementId CompoundStatementId =
			Function.AddStatement(CompoundStatement);

		const asTypedSemanticExpressionId PrefixTarget =
			AddRead(Function, Value);
		const asTypedSemanticExpressionId Prefix = AddMutation(
			Function,
			PrefixTarget,
			asTypedSemanticExpressionId(),
			"++",
			false);
		asSTypedSemanticStatement PrefixLocal;
		PrefixLocal.kind = asETypedSemanticStatementKind::LocalDeclaration;
		PrefixLocal.span = MakeSpan();
		PrefixLocal.symbol = PrefixResult;
		PrefixLocal.initializerExpression = Prefix;
		const asTypedSemanticStatementId PrefixLocalId =
			Function.AddStatement(PrefixLocal);

		const asTypedSemanticExpressionId PostfixTarget =
			AddRead(Function, Value);
		const asTypedSemanticExpressionId Postfix = AddMutation(
			Function,
			PostfixTarget,
			asTypedSemanticExpressionId(),
			"++",
			true);
		const asTypedSemanticStatementId ReturnId = AddReturn(Function, Postfix);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ValueLocalId);
		Root.children.PushLast(AssignStatementId);
		Root.children.PushLast(CompoundStatementId);
		Root.children.PushLast(PrefixLocalId);
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeScalarPower(
		const asCDataType& LeftType,
		const asCDataType& RightType,
		const asCDataType& ResultType)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "ScalarPower";
		const asTypedSemanticSymbolId Left = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Left", LeftType);
		const asTypedSemanticSymbolId Right = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Right", RightType);
		const asTypedSemanticExpressionId Power = AddTypedBinary(
			Function,
			AddRead(Function, Left),
			AddRead(Function, Right),
			"**",
			ResultType);
		const asTypedSemanticStatementId ReturnId =
			AddReturn(Function, Power);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeOrderedNativeCall()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int OrderedNativeCall(int A, int B, int C)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asTypedSemanticSymbolId A = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "A", IntType);
		const asTypedSemanticSymbolId B = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "B", IntType);
		const asTypedSemanticSymbolId C = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "C", IntType);

		asSTypedSemanticExpression Call;
		Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
		Call.type = IntType;
		Call.span = MakeSpan();
		Call.callTargetKind =
			asETypedSemanticCallTargetKind::SystemFunction;
		Call.resolvedFunctionId = 42;
		Call.targetProvenance.canonicalDeclaration =
			"int OrderedNative(int,int,int)";
		Call.targetProvenance.bodyAvailability =
			asETypedSemanticBodyAvailability::RegisteredNative;
		Call.targetProvenance.bodyOwnership =
			asETypedSemanticBodyOwnership::NativeRuntime;
		Call.operands.PushLast(AddRead(Function, A));
		Call.operands.PushLast(AddRead(Function, B));
		Call.operands.PushLast(AddRead(Function, C));
		Call.formalParameterIndices.PushLast(0);
		Call.formalParameterIndices.PushLast(1);
		Call.formalParameterIndices.PushLast(2);
		Call.sourceArgumentOrdinals.PushLast(0);
		Call.sourceArgumentOrdinals.PushLast(1);
		Call.sourceArgumentOrdinals.PushLast(2);
		Call.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::SourcePositional);
		Call.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::SourcePositional);
		Call.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::SourcePositional);
		for (int32 ArgumentIndex = 0; ArgumentIndex < 3; ++ArgumentIndex)
		{
			asSTypedSemanticArgumentProvenance Provenance;
			Provenance.origin =
				asETypedSemanticArgumentOrigin::SourcePositional;
			Provenance.sourceArgumentOrdinal = ArgumentIndex;
			Provenance.processedSpan = MakeSpan();
			Call.argumentProvenance.PushLast(Provenance);
		}
		Call.evaluationSequence.PushLast(2);
		Call.evaluationSequence.PushLast(1);
		Call.evaluationSequence.PushLast(0);
		AddEvaluationStep(Call,
			asETypedSemanticEvaluationRole::Argument,
			Call.operands[2], 2, 2);
		AddEvaluationStep(Call,
			asETypedSemanticEvaluationRole::Argument,
			Call.operands[1], 1, 1);
		AddEvaluationStep(Call,
			asETypedSemanticEvaluationRole::Argument,
			Call.operands[0], 0, 0);
		Call.nativeCall.isSystemFunction = true;
		Call.nativeCall.callConvention = 2;
		Call.nativeCall.hasFunctionCaller = true;
		const asTypedSemanticExpressionId CallId =
			Function.AddExpression(Call);
		const asTypedSemanticStatementId ReturnId =
			AddReturn(Function, CallId);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeRepeatedOrderedNativeCalls()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int OrderedNativeCall(int A, int B, int C)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asTypedSemanticSymbolId A = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "A", IntType);
		const asTypedSemanticSymbolId B = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "B", IntType);
		const asTypedSemanticSymbolId C = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "C", IntType);

		auto AddCall = [&]()
		{
			asSTypedSemanticExpression Call;
			Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
			Call.type = IntType;
			Call.span = MakeSpan();
			Call.callTargetKind =
				asETypedSemanticCallTargetKind::SystemFunction;
			Call.resolvedFunctionId = 42;
			Call.targetProvenance.canonicalDeclaration =
				"int OrderedNative(int,int,int)";
			Call.targetProvenance.bodyAvailability =
				asETypedSemanticBodyAvailability::RegisteredNative;
			Call.targetProvenance.bodyOwnership =
				asETypedSemanticBodyOwnership::NativeRuntime;
			Call.operands.PushLast(AddRead(Function, A));
			Call.operands.PushLast(AddRead(Function, B));
			Call.operands.PushLast(AddRead(Function, C));
			Call.formalParameterIndices.PushLast(0);
			Call.formalParameterIndices.PushLast(1);
			Call.formalParameterIndices.PushLast(2);
			Call.sourceArgumentOrdinals.PushLast(0);
			Call.sourceArgumentOrdinals.PushLast(1);
			Call.sourceArgumentOrdinals.PushLast(2);
			Call.argumentOrigins.PushLast(
				asETypedSemanticArgumentOrigin::SourcePositional);
			Call.argumentOrigins.PushLast(
				asETypedSemanticArgumentOrigin::SourcePositional);
			Call.argumentOrigins.PushLast(
				asETypedSemanticArgumentOrigin::SourcePositional);
			for (int32 ArgumentIndex = 0; ArgumentIndex < 3; ++ArgumentIndex)
			{
				asSTypedSemanticArgumentProvenance Provenance;
				Provenance.origin =
					asETypedSemanticArgumentOrigin::SourcePositional;
				Provenance.sourceArgumentOrdinal = ArgumentIndex;
				Provenance.processedSpan = MakeSpan();
				Call.argumentProvenance.PushLast(Provenance);
			}
			Call.evaluationSequence.PushLast(2);
			Call.evaluationSequence.PushLast(1);
			Call.evaluationSequence.PushLast(0);
			AddEvaluationStep(Call,
				asETypedSemanticEvaluationRole::Argument,
				Call.operands[2], 2, 2);
			AddEvaluationStep(Call,
				asETypedSemanticEvaluationRole::Argument,
				Call.operands[1], 1, 1);
			AddEvaluationStep(Call,
				asETypedSemanticEvaluationRole::Argument,
				Call.operands[0], 0, 0);
			Call.nativeCall.isSystemFunction = true;
			Call.nativeCall.callConvention = 2;
			Call.nativeCall.hasFunctionCaller = true;
			return Function.AddExpression(Call);
		};

		const asTypedSemanticExpressionId FirstCall = AddCall();
		const asTypedSemanticExpressionId SecondCall = AddCall();
		const asTypedSemanticExpressionId Sum = AddTypedBinary(
			Function, FirstCall, SecondCall, "+", IntType);
		const asTypedSemanticStatementId ReturnId = AddReturn(Function, Sum);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeStructuredControlFlow()
	{
		asCTypedSemanticFunction Function;
		Function.declaration =
			"int StructuredControlFlow(int Limit, int Selector)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);

		const asTypedSemanticSymbolId Limit = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Limit", IntType);
		const asTypedSemanticSymbolId Selector = AddSymbol(
			Function, asETypedSemanticSymbolKind::Parameter, "Selector", IntType);
		const asTypedSemanticSymbolId Sum = AddSymbol(
			Function, asETypedSemanticSymbolKind::Local, "Sum", IntType);
		const asTypedSemanticSymbolId Index = AddSymbol(
			Function, asETypedSemanticSymbolKind::Local, "Index", IntType);

		asSTypedSemanticStatement SumLocal;
		SumLocal.kind = asETypedSemanticStatementKind::LocalDeclaration;
		SumLocal.span = MakeSpan();
		SumLocal.symbol = Sum;
		SumLocal.initializerExpression = AddLiteral(Function, "0", IntType);
		const asTypedSemanticStatementId SumLocalId =
			Function.AddStatement(SumLocal);

		asSTypedSemanticStatement For;
		For.kind = asETypedSemanticStatementKind::For;
		For.span = MakeSpan();
		For.loopPhase = asETypedSemanticLoopPhase::Condition;
		For.safePointRole = asETypedSemanticSafePointRole::LoopBackedge;
		const asTypedSemanticStatementId ForId = Function.AddStatement(For);

		asSTypedSemanticStatement IndexLocal;
		IndexLocal.kind = asETypedSemanticStatementKind::LocalDeclaration;
		IndexLocal.span = MakeSpan();
		IndexLocal.loopPhase = asETypedSemanticLoopPhase::Initializer;
		IndexLocal.symbol = Index;
		IndexLocal.initializerExpression = AddLiteral(Function, "0", IntType);
		const asTypedSemanticStatementId IndexLocalId =
			Function.AddStatement(IndexLocal);

		const asTypedSemanticExpressionId ForCondition = AddBinary(
			Function,
			AddRead(Function, Index),
			AddRead(Function, Limit),
			"<",
			true);
		const asTypedSemanticExpressionId Increment = AddMutation(
			Function,
			AddRead(Function, Index),
			asTypedSemanticExpressionId(),
			"++",
			false);
		const asTypedSemanticStatementId IncrementExpression =
			AddExpressionStatement(
				Function, Increment, asETypedSemanticLoopPhase::Increment);
		const asTypedSemanticStatementId IncrementBlock = AddBlock(
			Function,
			{IncrementExpression},
			asETypedSemanticLoopPhase::Increment);

		const asTypedSemanticStatementId ContinueId = AddTransfer(
			Function, asETypedSemanticStatementKind::Continue, ForId);
		const asTypedSemanticStatementId ContinueBlock = AddBlock(
			Function, {ContinueId});
		asSTypedSemanticStatement ContinueIf;
		ContinueIf.kind = asETypedSemanticStatementKind::If;
		ContinueIf.span = MakeSpan();
		ContinueIf.conditionExpression = AddBinary(
			Function,
			AddRead(Function, Index),
			AddLiteral(Function, "1", IntType),
			"==",
			true);
		ContinueIf.thenStatement = ContinueBlock;
		const asTypedSemanticStatementId ContinueIfId =
			Function.AddStatement(ContinueIf);

		const asTypedSemanticStatementId AddIndexId = AddExpressionStatement(
			Function,
			AddMutation(
				Function,
				AddRead(Function, Sum),
				AddRead(Function, Index),
				"+=",
				false));

		const asTypedSemanticStatementId BreakForId = AddTransfer(
			Function, asETypedSemanticStatementKind::Break, ForId);
		const asTypedSemanticStatementId BreakForBlock = AddBlock(
			Function, {BreakForId});
		asSTypedSemanticStatement BreakForIf;
		BreakForIf.kind = asETypedSemanticStatementKind::If;
		BreakForIf.span = MakeSpan();
		BreakForIf.conditionExpression = AddBinary(
			Function,
			AddRead(Function, Sum),
			AddLiteral(Function, "10", IntType),
			">",
			true);
		BreakForIf.thenStatement = BreakForBlock;
		const asTypedSemanticStatementId BreakForIfId =
			Function.AddStatement(BreakForIf);

		const asTypedSemanticStatementId ForBody = AddBlock(
			Function,
			{ContinueIfId, AddIndexId, BreakForIfId},
			asETypedSemanticLoopPhase::Body,
			asETypedSemanticSafePointRole::LoopEntry);
		Function.statements[ForId.value].initializerStatement = IndexLocalId;
		Function.statements[ForId.value].conditionExpression = ForCondition;
		Function.statements[ForId.value].incrementStatement = IncrementBlock;
		Function.statements[ForId.value].incrementExpressions.PushLast(Increment);
		Function.statements[ForId.value].bodyStatement = ForBody;

		asSTypedSemanticStatement While;
		While.kind = asETypedSemanticStatementKind::While;
		While.span = MakeSpan();
		While.loopPhase = asETypedSemanticLoopPhase::Condition;
		While.safePointRole = asETypedSemanticSafePointRole::LoopBackedge;
		While.conditionExpression = AddBinary(
			Function,
			AddRead(Function, Sum),
			AddLiteral(Function, "15", IntType),
			"<",
			true);
		const asTypedSemanticStatementId WhileId = Function.AddStatement(While);
		const asTypedSemanticStatementId WhileBody = AddBlock(
			Function,
			{AddExpressionStatement(
				Function,
				AddMutation(
					Function,
					AddRead(Function, Sum),
					asTypedSemanticExpressionId(),
					"++",
					false))},
			asETypedSemanticLoopPhase::Body,
			asETypedSemanticSafePointRole::LoopEntry);
		Function.statements[WhileId.value].bodyStatement = WhileBody;

		asSTypedSemanticStatement DoWhile;
		DoWhile.kind = asETypedSemanticStatementKind::DoWhile;
		DoWhile.span = MakeSpan();
		DoWhile.loopPhase = asETypedSemanticLoopPhase::Condition;
		DoWhile.safePointRole = asETypedSemanticSafePointRole::LoopBackedge;
		DoWhile.conditionExpression = AddBinary(
			Function,
			AddRead(Function, Sum),
			AddLiteral(Function, "12", IntType),
			">",
			true);
		const asTypedSemanticStatementId DoWhileId =
			Function.AddStatement(DoWhile);
		const asTypedSemanticStatementId DoWhileBody = AddBlock(
			Function,
			{AddExpressionStatement(
				Function,
				AddMutation(
					Function,
					AddRead(Function, Sum),
					asTypedSemanticExpressionId(),
					"--",
					false))},
			asETypedSemanticLoopPhase::Body,
			asETypedSemanticSafePointRole::LoopEntry);
		Function.statements[DoWhileId.value].bodyStatement = DoWhileBody;

		asSTypedSemanticStatement Switch;
		Switch.kind = asETypedSemanticStatementKind::Switch;
		Switch.span = MakeSpan();
		Switch.safePointRole = asETypedSemanticSafePointRole::Statement;
		Switch.conditionExpression = AddRead(Function, Selector);
		Switch.switchNormalization =
			asETypedSemanticSwitchNormalization::SignedInt32;
		Switch.switchHasDefault = true;
		Switch.switchIsExhaustive = true;
		const asTypedSemanticStatementId SwitchId =
			Function.AddStatement(Switch);

		const asTypedSemanticStatementId SwitchBreak = AddTransfer(
			Function, asETypedSemanticStatementKind::Break, SwitchId);
		const asTypedSemanticStatementId CaseZeroBody = AddBlock(
			Function,
			{AddExpressionStatement(
				Function,
				AddMutation(
					Function,
					AddRead(Function, Sum),
					AddLiteral(Function, "100", IntType),
					"+=",
					false)),
			SwitchBreak});
		asSTypedSemanticStatement CaseZero;
		CaseZero.kind = asETypedSemanticStatementKind::Case;
		CaseZero.span = MakeSpan();
		CaseZero.safePointRole = asETypedSemanticSafePointRole::Statement;
		CaseZero.valueExpression = AddLiteral(Function, "0", IntType);
		CaseZero.caseConstantValue = 0;
		CaseZero.bodyStatement = CaseZeroBody;
		const asTypedSemanticStatementId CaseZeroId =
			Function.AddStatement(CaseZero);

		const asTypedSemanticStatementId CaseOneBody = AddBlock(
			Function,
			{AddExpressionStatement(
				Function,
				AddMutation(
					Function,
					AddRead(Function, Sum),
					AddLiteral(Function, "10", IntType),
					"+=",
					false))});
		asSTypedSemanticStatement CaseOne;
		CaseOne.kind = asETypedSemanticStatementKind::Case;
		CaseOne.span = MakeSpan();
		CaseOne.safePointRole = asETypedSemanticSafePointRole::Statement;
		CaseOne.valueExpression = AddLiteral(Function, "1", IntType);
		CaseOne.caseConstantValue = 1;
		CaseOne.bodyStatement = CaseOneBody;
		CaseOne.fallsThrough = true;
		CaseOne.hasExplicitFallthrough = true;
		const asTypedSemanticStatementId CaseOneId =
			Function.AddStatement(CaseOne);

		const asTypedSemanticStatementId DefaultBody = AddBlock(
			Function,
			{AddExpressionStatement(
				Function,
				AddMutation(
					Function,
					AddRead(Function, Sum),
					AddLiteral(Function, "1", IntType),
					"+=",
					false))});
		asSTypedSemanticStatement DefaultCase;
		DefaultCase.kind = asETypedSemanticStatementKind::Case;
		DefaultCase.span = MakeSpan();
		DefaultCase.safePointRole = asETypedSemanticSafePointRole::Statement;
		DefaultCase.isDefaultCase = true;
		DefaultCase.bodyStatement = DefaultBody;
		const asTypedSemanticStatementId DefaultId =
			Function.AddStatement(DefaultCase);

		Function.statements[SwitchId.value].children.PushLast(CaseZeroId);
		Function.statements[SwitchId.value].children.PushLast(CaseOneId);
		Function.statements[SwitchId.value].children.PushLast(DefaultId);

		const asTypedSemanticStatementId ReturnId = AddReturn(
			Function, AddRead(Function, Sum));
		Function.rootStatement = AddBlock(
			Function,
			{SumLocalId, ForId, WhileId, DoWhileId, SwitchId, ReturnId});
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeExhaustiveEnumSwitch(
		const asCDataType& EnumType,
		const asCDataType& RawType)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int ExhaustiveEnumSwitch(int RawMode)";
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asTypedSemanticSymbolId RawMode = AddSymbol(
			Function,
			asETypedSemanticSymbolKind::Parameter,
			"RawMode",
			RawType);
		const asTypedSemanticSymbolId Mode = AddSymbol(
			Function,
			asETypedSemanticSymbolKind::Local,
			"Mode",
			EnumType);
		asSTypedSemanticStatement ModeLocal;
		ModeLocal.kind = asETypedSemanticStatementKind::LocalDeclaration;
		ModeLocal.span = MakeSpan();
		ModeLocal.symbol = Mode;
		ModeLocal.initializerExpression = AddConversion(
			Function,
			AddRead(Function, RawMode),
			EnumType);
		const asTypedSemanticStatementId ModeLocalId =
			Function.AddStatement(ModeLocal);

		asSTypedSemanticStatement Switch;
		Switch.kind = asETypedSemanticStatementKind::Switch;
		Switch.span = MakeSpan();
		Switch.safePointRole =
			asETypedSemanticSafePointRole::SwitchInvalidValue;
		Switch.conditionExpression = AddRead(Function, Mode);
		Switch.switchNormalization =
			asETypedSemanticSwitchNormalization::SignedInt32;
		Switch.switchIsExhaustive = true;
		Switch.switchHasInvalidEnumValueEdge = true;
		const asTypedSemanticStatementId SwitchId =
			Function.AddStatement(Switch);

		asSTypedSemanticStatement FirstCase;
		FirstCase.kind = asETypedSemanticStatementKind::Case;
		FirstCase.span = MakeSpan();
		FirstCase.safePointRole =
			asETypedSemanticSafePointRole::Statement;
		FirstCase.valueExpression = AddLiteral(Function, "0", IntType);
		FirstCase.caseConstantValue = 0;
		FirstCase.bodyStatement = AddBlock(
			Function,
			{AddReturn(Function, AddLiteral(Function, "10", IntType))});
		const asTypedSemanticStatementId FirstCaseId =
			Function.AddStatement(FirstCase);

		asSTypedSemanticStatement SecondCase;
		SecondCase.kind = asETypedSemanticStatementKind::Case;
		SecondCase.span = MakeSpan();
		SecondCase.safePointRole =
			asETypedSemanticSafePointRole::Statement;
		SecondCase.valueExpression = AddLiteral(Function, "1", IntType);
		SecondCase.caseConstantValue = 1;
		SecondCase.bodyStatement = AddBlock(
			Function,
			{AddReturn(Function, AddLiteral(Function, "20", IntType))});
		const asTypedSemanticStatementId SecondCaseId =
			Function.AddStatement(SecondCase);

		Function.statements[SwitchId.value].children.PushLast(FirstCaseId);
		Function.statements[SwitchId.value].children.PushLast(SecondCaseId);
		const asTypedSemanticStatementId FinalReturn = AddReturn(
			Function, AddLiteral(Function, "0", IntType));
		Function.rootStatement = AddBlock(
			Function, {ModeLocalId, SwitchId, FinalReturn});
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_SemanticScalarBranch");
		Shape.ParameterTypes.Add(asCDataType::CreatePrimitive(ttInt, false));
		Shape.ParameterTypes.Add(asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape
	MakeStructuredControlFlowShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName =
			TEXT("AS_Test_Semantic_StructuredControlFlow");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeExhaustiveEnumShape(
		const asCDataType& RawType)
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_ExhaustiveEnumSwitch");
		Shape.ParameterTypes.Add(RawType);
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeShortCircuitShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_SemanticShortCircuit");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttBool, false));
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttBool, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttBool, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakePrimitiveIdentityShape(
		const asCDataType& Type)
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_PrimitiveIdentity");
		Shape.ParameterTypes.Add(Type);
		Shape.ReturnType = Type;
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeScalarConversionShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_ScalarConversion");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt64, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeScalarOperatorsShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_ScalarOperators");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape
	MakeCheckedIntegerDivisionShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_CheckedIntegerDivision");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeScalarMutationsShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_ScalarMutations");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeScalarPowerShape(
		const FString& SymbolName,
		const asCDataType& LeftType,
		const asCDataType& RightType,
		const asCDataType& ResultType)
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = SymbolName;
		Shape.ParameterTypes.Add(LeftType);
		Shape.ParameterTypes.Add(RightType);
		Shape.ReturnType = ResultType;
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeOrderedNativeCallShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_OrderedNativeCall");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Shape;
	}

	static FString ExpectedDeclaration()
	{
		return
			TEXT("static int32 AS_Test_Semantic_SemanticScalarBranch(\n")
			TEXT("\tconst int32 as_sem_s0,\n")
			TEXT("\tconst int32 as_sem_s1);\n");
	}

	static FString ExpectedDefinition()
	{
		return
			TEXT("static int32 AS_Test_Semantic_SemanticScalarBranch(\n")
			TEXT("\tconst int32 as_sem_s0,\n")
			TEXT("\tconst int32 as_sem_s1)\n")
			TEXT("{\n")
			TEXT("\tconst int32 as_sem_s2 = ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e2_left = as_sem_s0;\n")
			TEXT("\t\tconst int32 as_sem_e2_right = as_sem_s1;\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::WrapAdd<int32>(")
			TEXT("as_sem_e2_left, as_sem_e2_right);\n")
			TEXT("\t}());\n")
			TEXT("\tif (([&]() -> bool\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e5_left = as_sem_s2;\n")
			TEXT("\t\tconst int32 as_sem_e5_right = int32(10);\n")
			TEXT("\t\treturn (as_sem_e5_left > as_sem_e5_right);\n")
			TEXT("\t}()))\n")
			TEXT("\t{\n")
			TEXT("\t\treturn ([&]() -> int32\n")
			TEXT("\t\t{\n")
			TEXT("\t\t\tconst int32 as_sem_e8_left = as_sem_s2;\n")
			TEXT("\t\t\tconst int32 as_sem_e8_right = int32(2);\n")
			TEXT("\t\t\treturn AngelscriptTypedASTJIT::WrapMultiply<int32>(")
			TEXT("as_sem_e8_left, as_sem_e8_right);\n")
			TEXT("\t\t}());\n")
			TEXT("\t}\n")
			TEXT("\treturn ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e11_left = as_sem_s2;\n")
			TEXT("\t\tconst int32 as_sem_e11_right = int32(1);\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::WrapSubtract<int32>(")
			TEXT("as_sem_e11_left, as_sem_e11_right);\n")
			TEXT("\t}());\n")
			TEXT("}\n");
	}

	static FString ExpectedShortCircuitDeclaration()
	{
		return
			TEXT("static bool AS_Test_Semantic_SemanticShortCircuit(\n")
			TEXT("\tconst bool as_sem_s0,\n")
			TEXT("\tconst bool as_sem_s1);\n");
	}

	static FString ExpectedShortCircuitDefinition()
	{
		return
			TEXT("static bool AS_Test_Semantic_SemanticShortCircuit(\n")
			TEXT("\tconst bool as_sem_s0,\n")
			TEXT("\tconst bool as_sem_s1)\n")
			TEXT("{\n")
			TEXT("\treturn (as_sem_s0 && as_sem_s1);\n")
			TEXT("}\n");
	}

	static void AssertNoPartialOutput(
		FAutomationTestBase& Test,
		const FAngelscriptTypedASTJITEmission& Emission)
	{
		Test.TestEqual(TEXT("failure declaration"), Emission.Declaration, FString());
		Test.TestEqual(TEXT("failure definition"), Emission.Definition, FString());
		Test.TestEqual(TEXT("failure include count"), Emission.RequiredIncludes.Num(), 0);
	}

public:
	TEST_METHOD(FoldedGlobalConstantRequiresHardValuePlanAndEmitsExactBits)
	{
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asCTypedSemanticFunction Function =
			MakeFoldedGlobalFunction(IntType, 42);

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_ReadFoldedGlobal");
		Shape.ReturnType = IntType;
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;

		const FAngelscriptTypedASTJITEmission MissingDependency =
			EmitTypedASTJITFunction(Function, Shape, {});
		ASSERT_THAT(IsFalse(MissingDependency.bSuccess));
		ASSERT_THAT(IsTrue(MissingDependency.FailureReason
			== EAngelscriptTypedASTJITFallbackReason::
				SemanticDependencyMismatch));
		AssertNoPartialOutput(*TestRunner, MissingDependency);

		TArray<FAngelscriptTypedASTJITGlobalDependency> Dependencies;
		Dependencies.Add(MakeFoldedGlobalDependency());
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.GlobalDependencies = Dependencies;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, Options);
		ASSERT_THAT(IsTrue(Emission.bSuccess, *Emission.FailureDetail));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT(
			"return AngelscriptTypedASTJIT::FromCanonicalBits<int32>(UINT64_C(42));"))));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT("int32(42)"))));
	}

	TEST_METHOD(ZeroParameterBodyClosesTheTypedCppSignature)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int ZeroParameterBody()";
		const asTypedSemanticStatementId ReturnId = AddReturn(
			Function, AddLiteral(Function, "7"));
		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("AS_Test_Semantic_ZeroParameterBody");
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		Shape.InvocationKind = asETypedSemanticInvocationKind::Global;
		Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, {});

		ASSERT_THAT(IsTrue(Emission.bSuccess));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"static int32 AS_Test_Semantic_ZeroParameterBody();\n")),
			Emission.Declaration));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"static int32 AS_Test_Semantic_ZeroParameterBody()\n"
				"{\n"
				"\treturn int32(7);\n"
				"}\n")),
			Emission.Definition));
	}

	TEST_METHOD(ScalarHIRAnalyzesAndEmitsTheFrozenGolden)
	{
		const asCTypedSemanticFunction Function = MakeScalarBranch();
		const FAngelscriptTypedASTJITFunctionShape Shape = MakeShape();
		const FAngelscriptTypedASTJITAnalysis Analysis =
			AnalyzeTypedASTJITFunction(Function, Shape);
		ASSERT_THAT(IsTrue(Analysis.bEligible));
		ASSERT_THAT(AreEqual(3, Analysis.OrderedSymbols.Num()));
		ASSERT_THAT(AreEqual(6, Analysis.OrderedStatements.Num()));

		const FAngelscriptTypedASTJITEmission First =
			EmitTypedASTJITFunction(Function, Shape, {});
		const FAngelscriptTypedASTJITEmission Second =
			EmitTypedASTJITFunction(Function, Shape, {});
		ASSERT_THAT(IsTrue(First.bSuccess));
		ASSERT_THAT(AreEqual(ExpectedDeclaration(), First.Declaration));
		ASSERT_THAT(AreEqual(ExpectedDefinition(), First.Definition));
		ASSERT_THAT(AreEqual(First.Definition, Second.Definition));
		ASSERT_THAT(AreEqual(2, First.RequiredIncludes.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("CoreMinimal.h")), First.RequiredIncludes[0]));
		ASSERT_THAT(AreEqual(
			FString(TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h")),
			First.RequiredIncludes[1]));

		const FString Combined = First.Declaration + First.Definition;
		const TCHAR* Forbidden[] = {
			TEXT("GetByteCode"),
			TEXT("FAngelscriptBytecode"),
			TEXT("FStaticJITContext"),
			TEXT("FAngelscriptJITExecutionContext"),
			TEXT("FAngelscriptJITProvider"),
			TEXT("ProviderPrivate"),
		};
		for (const TCHAR* Token : Forbidden)
		{
			ASSERT_THAT(IsFalse(Combined.Contains(Token, ESearchCase::IgnoreCase)));
		}
	}

	TEST_METHOD(VerifiedStructuredControlFlowEmitsNativeCppStructure)
	{
		const asCTypedSemanticFunction Function =
			MakeStructuredControlFlow();
		const asSTypedSemanticVerificationResult Verification =
			VerifyTypedSemanticFunction(Function);
		ASSERT_THAT(IsTrue(
			Verification.IsValid(),
			UTF8_TO_TCHAR(Verification.detail.AddressOf())));

		const FAngelscriptTypedASTJITFunctionShape Shape =
			MakeStructuredControlFlowShape();
		const FAngelscriptTypedASTJITAnalysis Analysis =
			AnalyzeTypedASTJITFunction(Function, Shape);
		ASSERT_THAT(IsTrue(Analysis.bEligible, *Analysis.FailureDetail));

		const FAngelscriptTypedASTJITEmission First =
			EmitTypedASTJITFunction(Function, Shape, {});
		const FAngelscriptTypedASTJITEmission Second =
			EmitTypedASTJITFunction(Function, Shape, {});
		ASSERT_THAT(IsTrue(First.bSuccess, *First.FailureDetail));
		ASSERT_THAT(AreEqual(First.Definition, Second.Definition));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("for ("))));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("while ("))));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("do\n"))));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("switch ("))));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("case int32(0):"))));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("default:"))));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("continue;"))));
		ASSERT_THAT(IsTrue(First.Definition.Contains(TEXT("break;"))));
		ASSERT_THAT(IsFalse(First.Definition.Contains(TEXT("goto "))));
	}

	TEST_METHOD(ExhaustiveEnumSwitchRetainsInvalidValueExceptionEdge)
	{
		asCEnumType EnumType(static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine()));
		EnumType.name = "EControl";
		EnumType.size = 1;
		EnumType.flags = asOBJ_ENUM;
		const asCDataType EnumDataType =
			asCDataType::CreateType(&EnumType, false);
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asCTypedSemanticFunction Function =
			MakeExhaustiveEnumSwitch(EnumDataType, IntType);

		const asSTypedSemanticVerificationResult Verification =
			VerifyTypedSemanticFunction(Function);
		ASSERT_THAT(IsTrue(
			Verification.IsValid(),
			UTF8_TO_TCHAR(Verification.detail.AddressOf())));
		const FAngelscriptTypedASTJITFunctionShape Shape =
			MakeExhaustiveEnumShape(IntType);
		const FAngelscriptTypedASTJITAnalysis Analysis =
			AnalyzeTypedASTJITFunction(Function, Shape);
		ASSERT_THAT(IsTrue(Analysis.bEligible, *Analysis.FailureDetail));

		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, {});
		ASSERT_THAT(IsTrue(Emission.bSuccess, *Emission.FailureDetail));
		ASSERT_THAT(IsTrue(Emission.bRequiresExecutionState));
		ASSERT_THAT(IsTrue(Emission.Declaration.Contains(
			TEXT("FScriptExecution& Execution"))));
		ASSERT_THAT(IsTrue(Emission.Declaration.Contains(
			TEXT("const int32 as_sem_s0"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("const int8 as_sem_s1 = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_sem_s0);"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("case int32(0):"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("case int32(1):"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT("default:"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("FStaticJITFunction::SetSwitchValueInvalidException(Execution);"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("return int32{};"))));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("StaticJIT/StaticJITHeader.h"))));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"))));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT("EControl"))));
	}

	TEST_METHOD(EnumConversionRejectsUnsignedAndNonIntegerSources)
	{
		asCEnumType EnumType(static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine()));
		EnumType.name = "EControl";
		EnumType.size = 1;
		EnumType.flags = asOBJ_ENUM;
		const asCDataType EnumDataType =
			asCDataType::CreateType(&EnumType, false);
		const asCDataType RejectedRawTypes[] = {
			asCDataType::CreatePrimitive(ttUInt, false),
			asCDataType::CreatePrimitive(ttFloat32, false),
		};

		for (const asCDataType& RawType : RejectedRawTypes)
		{
			const FAngelscriptTypedASTJITAnalysis Analysis =
				AnalyzeTypedASTJITFunction(
					MakeExhaustiveEnumSwitch(EnumDataType, RawType),
					MakeExhaustiveEnumShape(RawType));
			ASSERT_THAT(IsFalse(Analysis.bEligible));
			ASSERT_THAT(AreEqual(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
				Analysis.FailureReason));
		}
	}

	TEST_METHOD(ShortCircuitUsesTypedBoolCppWithoutGenericExecutionContext)
	{
		const asCTypedSemanticFunction Function = MakeShortCircuit();
		const FAngelscriptTypedASTJITFunctionShape Shape =
			MakeShortCircuitShape();
		const FAngelscriptTypedASTJITEmission First =
			EmitTypedASTJITFunction(Function, Shape, {});
		const FAngelscriptTypedASTJITEmission Second =
			EmitTypedASTJITFunction(Function, Shape, {});

		ASSERT_THAT(IsTrue(First.bSuccess,
			TEXT("Verified bool short-circuit HIR should emit typed C++")));
		ASSERT_THAT(AreEqual(
			ExpectedShortCircuitDeclaration(), First.Declaration));
		ASSERT_THAT(AreEqual(
			ExpectedShortCircuitDefinition(), First.Definition));
		ASSERT_THAT(AreEqual(First.Definition, Second.Definition,
			TEXT("Short-circuit symbols and text must be deterministic")));
		ASSERT_THAT(IsFalse((First.Declaration + First.Definition).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive),
			TEXT("Pure typed short-circuit code must not name the generic execution context")));
	}

	TEST_METHOD(PrimitiveScalarSignaturesUseFixedWidthCppSpellings)
	{
		struct FTypeCase
		{
			eTokenType Token;
			const TCHAR* CppType;
		};
		const FTypeCase Cases[] = {
			{ttInt8, TEXT("int8")},
			{ttUInt8, TEXT("uint8")},
			{ttInt16, TEXT("int16")},
			{ttUInt16, TEXT("uint16")},
			{ttInt, TEXT("int32")},
			{ttUInt, TEXT("uint32")},
			{ttInt64, TEXT("int64")},
			{ttUInt64, TEXT("uint64")},
			{ttFloat32, TEXT("float")},
			{ttFloat64, TEXT("double")},
			{ttBool, TEXT("bool")},
		};

		for (const FTypeCase& Case : Cases)
		{
			const asCDataType Type =
				asCDataType::CreatePrimitive(Case.Token, false);
			const FAngelscriptTypedASTJITEmission First =
				EmitTypedASTJITFunction(
					MakePrimitiveIdentity(Type),
					MakePrimitiveIdentityShape(Type), {});
			const FAngelscriptTypedASTJITEmission Second =
				EmitTypedASTJITFunction(
					MakePrimitiveIdentity(Type),
					MakePrimitiveIdentityShape(Type), {});
			const FString ExpectedDeclaration = FString::Printf(
				TEXT("static %s AS_Test_Semantic_PrimitiveIdentity(\n")
				TEXT("\tconst %s as_sem_s0);\n"),
				Case.CppType, Case.CppType);
			const FString ExpectedDefinition = FString::Printf(
				TEXT("static %s AS_Test_Semantic_PrimitiveIdentity(\n")
				TEXT("\tconst %s as_sem_s0)\n")
				TEXT("{\n")
				TEXT("\treturn as_sem_s0;\n")
				TEXT("}\n"),
				Case.CppType, Case.CppType);

			ASSERT_THAT(IsTrue(First.bSuccess,
				FString::Printf(TEXT("%s identity should emit"),
					Case.CppType)));
			ASSERT_THAT(AreEqual(ExpectedDeclaration, First.Declaration));
			ASSERT_THAT(AreEqual(ExpectedDefinition, First.Definition));
			ASSERT_THAT(AreEqual(First.Definition, Second.Definition,
				TEXT("Primitive identity output must be deterministic")));
		}
	}

	TEST_METHOD(TypedLocalsEncodeLiteralsAndExplicitConversions)
	{
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				MakeScalarConversionAndLiteral(),
				MakeScalarConversionShape(), {});
		const FString ExpectedDefinition =
			TEXT("static int64 AS_Test_Semantic_ScalarConversion(\n")
			TEXT("\tconst int32 as_sem_s0)\n")
			TEXT("{\n")
			TEXT("\tconst int8 as_sem_s1 = int8(7);\n")
			TEXT("\tconst int64 as_sem_s2 = static_cast<int64>(as_sem_s0);\n")
			TEXT("\treturn as_sem_s2;\n")
			TEXT("}\n");

		ASSERT_THAT(IsTrue(Emission.bSuccess,
			TEXT("Reviewed scalar locals, literals, and conversions should emit")));
		ASSERT_THAT(AreEqual(ExpectedDefinition, Emission.Definition));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(ScalarOperatorsUseContextFreeWrapAndShiftHelpers)
	{
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				MakeScalarOperators(), MakeScalarOperatorsShape(), {});
		const FString ExpectedDefinition =
			TEXT("static int32 AS_Test_Semantic_ScalarOperators(\n")
			TEXT("\tconst int32 as_sem_s0,\n")
			TEXT("\tconst int32 as_sem_s1)\n")
			TEXT("{\n")
			TEXT("\treturn ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e14_left = ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e8_left = ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e4_left = AngelscriptTypedASTJIT::")
			TEXT("WrapNegate<int32>(as_sem_s0);\n")
			TEXT("\t\tconst int32 as_sem_e4_right = AngelscriptTypedASTJIT::")
			TEXT("BitwiseNot<int32>(as_sem_s1);\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::WrapMultiply<int32>(")
			TEXT("as_sem_e4_left, as_sem_e4_right);\n")
			TEXT("\t}());\n")
			TEXT("\t\tconst int32 as_sem_e8_right = ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e7_left = as_sem_s0;\n")
			TEXT("\t\tconst int32 as_sem_e7_right = int32(33);\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::LogicalShiftRight<int32>(")
			TEXT("as_sem_e7_left, as_sem_e7_right);\n")
			TEXT("\t}());\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::WrapSubtract<int32>(")
			TEXT("as_sem_e8_left, as_sem_e8_right);\n")
			TEXT("\t}());\n")
			TEXT("\t\tconst int32 as_sem_e14_right = ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e13_left = ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e11_left = as_sem_s1;\n")
			TEXT("\t\tconst int32 as_sem_e11_right = int32(33);\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::ShiftLeft<int32>(")
			TEXT("as_sem_e11_left, as_sem_e11_right);\n")
			TEXT("\t}());\n")
			TEXT("\t\tconst int32 as_sem_e13_right = int32(34);\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::ArithmeticShiftRight<int32>(")
			TEXT("as_sem_e13_left, as_sem_e13_right);\n")
			TEXT("\t}());\n")
			TEXT("\t\treturn AngelscriptTypedASTJIT::WrapAdd<int32>(")
			TEXT("as_sem_e14_left, as_sem_e14_right);\n")
			TEXT("\t}());\n")
			TEXT("}\n");

		ASSERT_THAT(IsTrue(Emission.bSuccess,
			TEXT("Reviewed wrap and shift operators should emit")));
		ASSERT_THAT(AreEqual(ExpectedDefinition, Emission.Definition));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"))));
		ASSERT_THAT(IsFalse((Emission.Declaration + Emission.Definition).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(IntegerDivisionUsesTheScriptExceptionContract)
	{
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				MakeCheckedIntegerDivision(),
				MakeCheckedIntegerDivisionShape(),
				{});

		ASSERT_THAT(IsTrue(
			Emission.bSuccess,
			TEXT("A verified scalar integer division should emit")));
		ASSERT_THAT(IsTrue(
			Emission.bRequiresExecutionState,
			TEXT("Integer division can raise a script exception and must receive the active execution record")));
		ASSERT_THAT(IsTrue(Emission.Declaration.Contains(
			TEXT("FScriptExecution& Execution"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e2_left, as_sem_e2_right)"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("const int32 as_sem_e2_left = as_sem_s0;"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("const int32 as_sem_e2_right = as_sem_s1;"))));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"))));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("(as_sem_s0 / as_sem_s1)"),
			ESearchCase::CaseSensitive),
			TEXT("Generated C++ must not execute an unchecked integral division")));
		ASSERT_THAT(IsFalse((Emission.Declaration + Emission.Definition).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(ScalarMutationsMaterializeOneTargetAndOneStore)
	{
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(
				MakeScalarMutations(), MakeScalarMutationsShape(), {});
		const FString ExpectedDefinition =
			TEXT("static int32 AS_Test_Semantic_ScalarMutations(\n")
			TEXT("\tconst int32 as_sem_s0)\n")
			TEXT("{\n")
			TEXT("\tint32 as_sem_s1 = int32(1);\n")
			TEXT("\t([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e3_value = as_sem_s0;\n")
			TEXT("\t\tint32& as_sem_e3_target = as_sem_s1;\n")
			TEXT("\t\tas_sem_e3_target = as_sem_e3_value;\n")
			TEXT("\t\treturn as_sem_e3_target;\n")
			TEXT("\t}());\n")
			TEXT("\t([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tconst int32 as_sem_e6_value = int32(3);\n")
			TEXT("\t\tint32& as_sem_e6_target = as_sem_s1;\n")
			TEXT("\t\tas_sem_e6_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e6_target, as_sem_e6_value);\n")
			TEXT("\t\treturn as_sem_e6_target;\n")
			TEXT("\t}());\n")
			TEXT("\tconst int32 as_sem_s2 = ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tint32& as_sem_e8_target = as_sem_s1;\n")
			TEXT("\t\tas_sem_e8_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e8_target, int32(1));\n")
			TEXT("\t\treturn as_sem_e8_target;\n")
			TEXT("\t}());\n")
			TEXT("\treturn ([&]() -> int32\n")
			TEXT("\t{\n")
			TEXT("\t\tint32& as_sem_e10_target = as_sem_s1;\n")
			TEXT("\t\tconst int32 as_sem_e10_old = as_sem_e10_target;\n")
			TEXT("\t\tas_sem_e10_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e10_old, int32(1));\n")
			TEXT("\t\treturn as_sem_e10_old;\n")
			TEXT("\t}());\n")
			TEXT("}\n");

		ASSERT_THAT(IsTrue(Emission.bSuccess,
			TEXT("Reviewed scalar mutations should emit explicit C++ temporaries")));
		ASSERT_THAT(AreEqual(ExpectedDefinition, Emission.Definition));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"))));
		ASSERT_THAT(IsFalse((Emission.Declaration + Emission.Definition).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(PowerEmitsOnlyCompilerProvenFloatingShapes)
	{
		const asCDataType FloatType =
			asCDataType::CreatePrimitive(ttFloat32, false);
		const asCDataType DoubleType =
			asCDataType::CreatePrimitive(ttFloat64, false);
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);

		const FAngelscriptTypedASTJITEmission FloatPower =
			EmitTypedASTJITFunction(
				MakeScalarPower(FloatType, FloatType, FloatType),
				MakeScalarPowerShape(
					TEXT("AS_Test_Semantic_PowerFloat"),
					FloatType,
					FloatType,
					FloatType),
				{});
		ASSERT_THAT(IsTrue(FloatPower.bSuccess,
			TEXT("compiler-selected POWf should emit")));
		ASSERT_THAT(AreEqual(
			FString(
				TEXT("static float AS_Test_Semantic_PowerFloat(\n")
				TEXT("\tFScriptExecution& Execution,\n")
				TEXT("\tconst float as_sem_s0,\n")
				TEXT("\tconst float as_sem_s1)\n")
				TEXT("{\n")
				TEXT("\treturn ([&]() -> float\n")
				TEXT("\t{\n")
				TEXT("\t\tconst float as_sem_e2_left = as_sem_s0;\n")
				TEXT("\t\tconst float as_sem_e2_right = as_sem_s1;\n")
				TEXT("\t\treturn AngelscriptTypedASTJIT::CheckedPower<float>(")
				TEXT("Execution, as_sem_e2_left, as_sem_e2_right);\n")
				TEXT("\t}());\n")
				TEXT("}\n")),
			FloatPower.Definition));

		const FAngelscriptTypedASTJITEmission DoublePower =
			EmitTypedASTJITFunction(
				MakeScalarPower(DoubleType, DoubleType, DoubleType),
				MakeScalarPowerShape(
					TEXT("AS_Test_Semantic_PowerDouble"),
					DoubleType,
					DoubleType,
					DoubleType),
				{});
		ASSERT_THAT(IsTrue(DoublePower.bSuccess,
			TEXT("compiler-selected POWd should emit")));
		ASSERT_THAT(AreEqual(
			FString(
				TEXT("static double AS_Test_Semantic_PowerDouble(\n")
				TEXT("\tFScriptExecution& Execution,\n")
				TEXT("\tconst double as_sem_s0,\n")
				TEXT("\tconst double as_sem_s1)\n")
				TEXT("{\n")
				TEXT("\treturn ([&]() -> double\n")
				TEXT("\t{\n")
				TEXT("\t\tconst double as_sem_e2_left = as_sem_s0;\n")
				TEXT("\t\tconst double as_sem_e2_right = as_sem_s1;\n")
				TEXT("\t\treturn AngelscriptTypedASTJIT::CheckedPower<double>(")
				TEXT("Execution, as_sem_e2_left, as_sem_e2_right);\n")
				TEXT("\t}());\n")
				TEXT("}\n")),
			DoublePower.Definition));

		const FAngelscriptTypedASTJITEmission DoubleIntPower =
			EmitTypedASTJITFunction(
				MakeScalarPower(DoubleType, IntType, DoubleType),
				MakeScalarPowerShape(
					TEXT("AS_Test_Semantic_PowerDoubleInt32"),
					DoubleType,
					IntType,
					DoubleType),
				{});
		ASSERT_THAT(IsTrue(DoubleIntPower.bSuccess,
			TEXT("compiler-selected POWdi should emit")));
		ASSERT_THAT(AreEqual(
			FString(
				TEXT("static double AS_Test_Semantic_PowerDoubleInt32(\n")
				TEXT("\tFScriptExecution& Execution,\n")
				TEXT("\tconst double as_sem_s0,\n")
				TEXT("\tconst int32 as_sem_s1)\n")
				TEXT("{\n")
				TEXT("\treturn ([&]() -> double\n")
				TEXT("\t{\n")
				TEXT("\t\tconst double as_sem_e2_left = as_sem_s0;\n")
				TEXT("\t\tconst int32 as_sem_e2_right = as_sem_s1;\n")
				TEXT("\t\treturn AngelscriptTypedASTJIT::CheckedPower<double>(")
				TEXT("Execution, as_sem_e2_left, as_sem_e2_right);\n")
				TEXT("\t}());\n")
				TEXT("}\n")),
			DoubleIntPower.Definition));

		for (const FAngelscriptTypedASTJITEmission* Emission :
			{&FloatPower, &DoublePower, &DoubleIntPower})
		{
			ASSERT_THAT(IsTrue(Emission->RequiredIncludes.Contains(
				TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"))));
			ASSERT_THAT(IsTrue(Emission->bRequiresExecutionState));
			ASSERT_THAT(IsTrue(Emission->Declaration.Contains(
				TEXT("FScriptExecution& Execution"))));
			ASSERT_THAT(IsFalse(
				(Emission->Declaration + Emission->Definition).Contains(
					TEXT("FAngelscriptJITExecutionContext"),
					ESearchCase::CaseSensitive)));
		}

		const FAngelscriptTypedASTJITEmission IntegerPower =
			EmitTypedASTJITFunction(
				MakeScalarPower(IntType, IntType, IntType),
				MakeScalarPowerShape(
					TEXT("AS_Test_Semantic_UnsupportedIntegerPower"),
					IntType,
					IntType,
					IntType),
				{});
		ASSERT_THAT(IsFalse(IntegerPower.bSuccess,
			TEXT("integer power must remain unsupported")));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
			IntegerPower.FailureReason));
		AssertNoPartialOutput(*TestRunner, IntegerPower);
	}

	TEST_METHOD(NativeCallArgumentsPreserveEvaluationAndFormalOrder)
	{
		const asCTypedSemanticFunction Function = MakeOrderedNativeCall();
		const FAngelscriptTypedASTJITFunctionShape Shape =
			MakeOrderedNativeCallShape();
		const FAngelscriptTypedASTJITEmission MissingPlan =
			EmitTypedASTJITFunction(Function, Shape, {});
		ASSERT_THAT(IsFalse(MissingPlan.bSuccess,
			TEXT("A resolved call without an exact closure plan must fail closed")));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			MissingPlan.FailureReason));
		AssertNoPartialOutput(*TestRunner, MissingPlan);

		FAngelscriptTypedASTJITNativeCallEmissionPlan NativeCall;
		NativeCall.Expression = asTypedSemanticExpressionId(3);
		NativeCall.EngineLocalTargetFunctionId = 42;
		NativeCall.Symbol = TEXT("ObserveSystemCallTarget");
		NativeCall.Include = TEXT("ReviewedNative.h");
		const TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls = {
			NativeCall,
		};
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.NativeCalls = NativeCalls;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, Options);

		ASSERT_THAT(IsTrue(Emission.bSuccess,
			TEXT("An exact validated native-call plan should emit direct C++")));
		ASSERT_THAT(IsFalse(Emission.bRequiresExecutionState,
			TEXT("A direct-only body must keep its ordinary C++ signature")));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("ReviewedNative.h"))));
		const int32 Arg2Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg2 = as_sem_s2;"));
		const int32 Arg1Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg1 = as_sem_s1;"));
		const int32 Arg0Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg0 = as_sem_s0;"));
		const int32 CallPosition = Emission.Definition.Find(
			TEXT("return ObserveSystemCallTarget(as_sem_e3_arg0, ")
			TEXT("as_sem_e3_arg1, as_sem_e3_arg2);"));
		ASSERT_THAT(IsTrue(
			Arg2Position != INDEX_NONE
			&& Arg2Position < Arg1Position
			&& Arg1Position < Arg0Position
			&& Arg0Position < CallPosition,
			TEXT("Operands must materialize in HIR evaluation order before the formal-order call")));
		ASSERT_THAT(IsFalse((Emission.Declaration + Emission.Definition).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(ExportedBindUsesTheCurrentEngineNativeSlot)
	{
		const asCTypedSemanticFunction Function = MakeOrderedNativeCall();
		const FAngelscriptTypedASTJITFunctionShape Shape =
			MakeOrderedNativeCallShape();

		FAngelscriptTypedASTJITNativeCallEmissionPlan NativeCall;
		NativeCall.Expression = asTypedSemanticExpressionId(3);
		NativeCall.EngineLocalTargetFunctionId = 42;
		NativeCall.Route =
			EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding;
		NativeCall.NativeCallSiteRoute =
			EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding;
		NativeCall.CanonicalDeclaration =
			TEXT("int ObserveSystemCallTarget(int, int, int)");
		NativeCall.RegisteredTarget = TEXT("ObserveSystemCallTarget");
		NativeCall.CallSiteSymbol = TEXT("ASJIT_Call_ObserveSystemCallTarget");
		NativeCall.StableTargetKey.Hash = FAngelscriptHash256{
			FBlake3::HashBuffer(
				"ObserveSystemCallTarget",
				sizeof("ObserveSystemCallTarget") - 1)};
		NativeCall.ExpectedABI = FAngelscriptHash256{
			FBlake3::HashBuffer(
				"cdecl:i32(i32,i32,i32)",
				sizeof("cdecl:i32(i32,i32,i32)") - 1)};
		NativeCall.ReferenceSlotIndex = 5;
		const TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls = {
			NativeCall,
		};
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.NativeCalls = NativeCalls;

		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, Options);
		ASSERT_THAT(IsTrue(Emission.bSuccess, Emission.FailureDetail));
		ASSERT_THAT(IsTrue(Emission.bRequiresExecutionState,
			TEXT("A current-Engine slot call needs the owning entry's FScriptExecution")));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(TEXT(
			"StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT(
			"AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32, int32>("))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT(
			"Execution, ASJIT_Call_ObserveSystemCallTarget,"))));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT(
			"Reference Slot      : 5"))));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT(
			"return ObserveSystemCallTarget(")),
			TEXT("An exported Bind must not freeze the descriptor symbol as its executable target")));
		ASSERT_THAT(IsFalse((Emission.Declaration + Emission.Definition).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(VoidExportedBindUsesTheCurrentEngineNativeSlot)
	{
		asCTypedSemanticFunction Function = MakeOrderedNativeCall();
		Function.declaration = "void ObserveSystemCallTarget(int A, int B, int C)";
		Function.expressions[3].type =
			asCDataType::CreatePrimitive(ttVoid, false);
		Function.statements[0].kind =
			asETypedSemanticStatementKind::Expression;
		Function.statements[0].safePointRole =
			asETypedSemanticSafePointRole::None;

		FAngelscriptTypedASTJITFunctionShape Shape =
			MakeOrderedNativeCallShape();
		Shape.SymbolName = TEXT("AS_Test_Semantic_VoidNativeCall");
		Shape.ReturnType = asCDataType::CreatePrimitive(ttVoid, false);

		FAngelscriptTypedASTJITNativeCallEmissionPlan NativeCall;
		NativeCall.Expression = asTypedSemanticExpressionId(3);
		NativeCall.EngineLocalTargetFunctionId = 42;
		NativeCall.Route =
			EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding;
		NativeCall.NativeCallSiteRoute =
			EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding;
		NativeCall.CanonicalDeclaration =
			TEXT("void ObserveSystemCallTarget(int, int, int)");
		NativeCall.RegisteredTarget = TEXT("ObserveSystemCallTarget");
		NativeCall.CallSiteSymbol = TEXT("ASJIT_Call_ObserveSystemCallTarget");
		NativeCall.StableTargetKey.Hash = FAngelscriptHash256{
			FBlake3::HashBuffer(
				"ObserveSystemCallTargetVoid",
				sizeof("ObserveSystemCallTargetVoid") - 1)};
		NativeCall.ExpectedABI = FAngelscriptHash256{
			FBlake3::HashBuffer(
				"cdecl:void(i32,i32,i32)",
				sizeof("cdecl:void(i32,i32,i32)") - 1)};
		NativeCall.ReferenceSlotIndex = 6;
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.NativeCalls = {NativeCall};

		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, Options);
		ASSERT_THAT(IsTrue(Emission.bSuccess, Emission.FailureDetail));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT(
			"AngelscriptTypedASTJIT::InvokeBoundNative<void, int32, int32, int32>("))));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(TEXT("const void")),
			TEXT("A void native Bind must not materialize an impossible result object")));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(TEXT(
			"if (UNLIKELY(Execution.bExceptionThrown))"))));
	}

	TEST_METHOD(DirectScriptHelperCallsThreadExecutionAndCheckFailureImmediately)
	{
		asCTypedSemanticFunction Function = MakeOrderedNativeCall();
		Function.declaration = "int OrderedScriptCall(int A, int B, int C)";
		Function.expressions[3].callTargetKind =
			asETypedSemanticCallTargetKind::ScriptFunction;
		Function.expressions[3].resolvedFunctionId = 84;
		Function.expressions[3].nativeCall =
			asSTypedSemanticNativeCallRequirements();
		Function.expressions[3].targetProvenance.canonicalDeclaration =
			"int OrderedScriptTarget(int,int,int)";
		Function.expressions[3].targetProvenance.declarationModule =
			"SyntheticTypedASTJIT";
		Function.expressions[3].targetProvenance.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Function.expressions[3].targetProvenance.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Function.expressions[3].targetProvenance.bodyOwnerModule =
			"SyntheticTypedASTJIT";

		FAngelscriptTypedASTJITScriptCallEmissionPlan ScriptCall;
		ScriptCall.Expression = asTypedSemanticExpressionId(3);
		ScriptCall.EngineLocalTargetFunctionId = 84;
		ScriptCall.Symbol = TEXT("ASJIT_Helper_Calculate_deadbeef");
		const TArray<FAngelscriptTypedASTJITScriptCallEmissionPlan> ScriptCalls = {
			ScriptCall,
		};
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.ScriptCalls = ScriptCalls;
		FAngelscriptTypedASTJITFunctionShape Shape =
			MakeOrderedNativeCallShape();
		Shape.SymbolName = TEXT("AS_Test_Semantic_OrderedScriptCall");
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, Options);

		ASSERT_THAT(IsTrue(Emission.bSuccess, Emission.FailureDetail));
		ASSERT_THAT(IsTrue(Emission.bRequiresExecutionState,
			TEXT("A direct helper edge must carry the current execution for its helper frame and exception state")));
		ASSERT_THAT(IsTrue(Emission.Declaration.Contains(TEXT(
			"AS_Test_Semantic_OrderedScriptCall(\n\tFScriptExecution& Execution,"))));
		const int32 Arg2Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg2 = as_sem_s2;"));
		const int32 Arg1Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg1 = as_sem_s1;"));
		const int32 Arg0Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg0 = as_sem_s0;"));
		const int32 CallPosition = Emission.Definition.Find(TEXT(
			"const int32 as_sem_e3_result = "
			"ASJIT_Helper_Calculate_deadbeef(Execution, "
			"as_sem_e3_arg0, as_sem_e3_arg1, as_sem_e3_arg2);"));
		const int32 ExceptionCheckPosition = Emission.Definition.Find(
			TEXT("if (UNLIKELY(Execution.bExceptionThrown))"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			CallPosition == INDEX_NONE ? 0 : CallPosition);
		ASSERT_THAT(IsTrue(
			Arg2Position != INDEX_NONE
			&& Arg2Position < Arg1Position
			&& Arg1Position < Arg0Position
			&& Arg0Position < CallPosition
			&& CallPosition < ExceptionCheckPosition,
			TEXT("Direct helper operands must materialize in HIR order, call in formal order, and check failure immediately")));
		ASSERT_THAT(IsFalse((Emission.Declaration + Emission.Definition).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(BridgeCallSiteNamesResolveReadableStableSuffixCollisions)
	{
		auto HashFromHex = [](const TCHAR* Hex)
		{
			return FAngelscriptHash256{
				FBlake3Hash(FWideStringView(Hex))};
		};
		const FAngelscriptHash256 FirstKey = HashFromHex(TEXT(
			"12345678aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
		const FAngelscriptHash256 SecondKey = HashFromHex(TEXT(
			"12345678bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
		const FAngelscriptHash256 ExpectedABI = HashFromHex(TEXT(
			"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
		const FAngelscriptStableFunctionKey OwningRoot{HashFromHex(TEXT(
			"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"))};
		const FString OwningRootHex = OwningRoot.Hash.ToHexString();

		FAngelscriptTypedASTJITNativeCallEmissionPlan First;
		First.Route = EAngelscriptTypedASTJITCallEmissionRoute::Bridge;
		First.CanonicalDeclaration =
			TEXT("int Namespace::Compute-Value(int Value)");
		First.StableTargetKey.Hash = FirstKey;
		First.ExpectedABI = ExpectedABI;
		First.ReferenceSlotIndex = 0;
		FAngelscriptTypedASTJITNativeCallEmissionPlan FirstDuplicate = First;
		FAngelscriptTypedASTJITNativeCallEmissionPlan Second = First;
		Second.CanonicalDeclaration =
			TEXT("float Namespace::Compute-Value(float Value)");
		Second.StableTargetKey.Hash = SecondKey;
		Second.ReferenceSlotIndex = 1;
		TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan> Plans = {
			Second, FirstDuplicate, First,
		};

		FString Error;
		ASSERT_THAT(IsTrue(
			AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
				Plans, OwningRoot, &Error),
			Error));
		ASSERT_THAT(AreEqual(
			FString::Printf(TEXT(
				"ASJIT_Call_Namespace__Compute_Value_12345678bbbb_%s"),
				*OwningRootHex),
			Plans[0].CallSiteSymbol));
		ASSERT_THAT(AreEqual(
			FString::Printf(TEXT(
				"ASJIT_Call_Namespace__Compute_Value_12345678aaaa_%s"),
				*OwningRootHex),
			Plans[1].CallSiteSymbol));
		ASSERT_THAT(AreEqual(Plans[1].CallSiteSymbol,
			Plans[2].CallSiteSymbol));

		TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan> Reversed = {
			First, Second,
		};
		ASSERT_THAT(IsTrue(
			AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
				Reversed, OwningRoot, &Error), Error));
		ASSERT_THAT(AreEqual(Plans[1].CallSiteSymbol,
			Reversed[0].CallSiteSymbol));
		ASSERT_THAT(AreEqual(Plans[0].CallSiteSymbol,
			Reversed[1].CallSiteSymbol));

		const FString FirstOwnerSymbol = Reversed[0].CallSiteSymbol;
		const FAngelscriptStableFunctionKey OtherRoot{HashFromHex(TEXT(
			"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"))};
		ASSERT_THAT(IsTrue(
			AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
				Reversed, OtherRoot, &Error), Error));
		ASSERT_THAT(AreNotEqual(
			FirstOwnerSymbol, Reversed[0].CallSiteSymbol));
		ASSERT_THAT(IsTrue(Reversed[0].CallSiteSymbol.EndsWith(
			OtherRoot.Hash.ToHexString())));
	}

	TEST_METHOD(ProviderPrivateCallNamesItsBridgeTargetAndThreadsExecution)
	{
		const asCTypedSemanticFunction Function =
			MakeRepeatedOrderedNativeCalls();
		const FAngelscriptTypedASTJITFunctionShape Shape =
			MakeOrderedNativeCallShape();
		FAngelscriptTypedASTJITNativeCallEmissionPlan NativeCall;
		NativeCall.Expression = asTypedSemanticExpressionId(3);
		NativeCall.EngineLocalTargetFunctionId = 42;
		NativeCall.Route =
			EAngelscriptTypedASTJITCallEmissionRoute::Bridge;
		NativeCall.CallSiteSymbol = TEXT("ASJIT_Call_ProviderPrivateAdd");
		NativeCall.CanonicalDeclaration =
			TEXT("int ProviderPrivateAdd(int A, int B, int C)");
		NativeCall.RegisteredTarget =
			TEXT("FProviderPrivateBinds::PrivateAdd");
		NativeCall.StableTargetKey.Hash = FAngelscriptHash256{
			FBlake3::HashBuffer(
				"ProviderPrivateAdd", sizeof("ProviderPrivateAdd") - 1)};
		NativeCall.ExpectedABI = FAngelscriptHash256{
			FBlake3::HashBuffer(
				"cdecl:i32(i32,i32,i32)",
				sizeof("cdecl:i32(i32,i32,i32)") - 1)};
		NativeCall.ReferenceSlotIndex = 7;

		FAngelscriptTypedASTJITNativeCallEmissionPlan DuplicateNativeCall =
			NativeCall;
		DuplicateNativeCall.Expression = asTypedSemanticExpressionId(7);
		const TArray<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls = {
			NativeCall,
			DuplicateNativeCall,
		};
		const FAngelscriptTypedASTJITGenerationCapabilityProfile
			InstrumentationProfile = MakeProfile(
				ECapability::FramePosition | ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.NativeCalls = NativeCalls;
		Options.InstrumentationProfile = &InstrumentationProfile;
		Options.bEmitNormalizedComments = true;
		const FAngelscriptTypedASTJITEmission Emission =
			EmitTypedASTJITFunction(Function, Shape, Options);

		ASSERT_THAT(IsTrue(Emission.bSuccess,
			TEXT("A reviewed scalar private target should emit the typed bridge")));
		ASSERT_THAT(IsTrue(Emission.bRequiresExecutionState,
			TEXT("A bridge must receive the current execution/reference state")));
		ASSERT_THAT(IsTrue(Emission.RequiredIncludes.Contains(
			TEXT("StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"))));

		const FString Combined = Emission.Declaration + Emission.Definition;
		const FString CallSiteDefinition = TEXT(
			"static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_ProviderPrivateAdd");
		const int32 FirstCallSiteDefinition = Combined.Find(CallSiteDefinition);
		const int32 DuplicateCallSiteDefinition = FirstCallSiteDefinition == INDEX_NONE
			? INDEX_NONE
			: Combined.Find(
				CallSiteDefinition,
				ESearchCase::CaseSensitive,
				ESearchDir::FromStart,
				FirstCallSiteDefinition + CallSiteDefinition.Len());
		ASSERT_THAT(IsTrue(FirstCallSiteDefinition != INDEX_NONE));
		ASSERT_THAT(IsTrue(
			DuplicateCallSiteDefinition == INDEX_NONE,
			TEXT("Repeated calls to one stable target must share one immutable call-site row")));
		ASSERT_THAT(IsTrue(Combined.Contains(
			TEXT("\"int ProviderPrivateAdd(int A, int B, int C)\""))));
		ASSERT_THAT(IsTrue(Combined.Contains(
			TEXT("\"FProviderPrivateBinds::PrivateAdd\""))));
		ASSERT_THAT(IsTrue(Combined.Contains(
			NativeCall.StableTargetKey.Hash.ToHexString())));
		ASSERT_THAT(IsTrue(Combined.Contains(
			NativeCall.ExpectedABI.ToHexString())));
		ASSERT_THAT(IsTrue(Combined.Contains(TEXT(
			"AS_Test_Semantic_OrderedNativeCall(\n\tFScriptExecution& Execution,"))));

		const int32 Arg2Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg2 = as_sem_s2;"));
		const int32 Arg1Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg1 = as_sem_s1;"));
		const int32 Arg0Position = Emission.Definition.Find(
			TEXT("const int32 as_sem_e3_arg0 = as_sem_s0;"));
		const int32 CallPosition = Emission.Definition.Find(TEXT(
			"AngelscriptTypedASTJIT::InvokeBound<int32, int32, int32, int32>(\n"
			"\t\t\tExecution, ASJIT_Call_ProviderPrivateAdd,"));
		const int32 ExceptionCheckPosition = Emission.Definition.Find(
			TEXT("if (UNLIKELY(Execution.bExceptionThrown))"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			CallPosition == INDEX_NONE ? 0 : CallPosition);
		ASSERT_THAT(IsTrue(
			Arg2Position != INDEX_NONE
			&& Arg2Position < Arg1Position
			&& Arg1Position < Arg0Position
			&& Arg0Position < CallPosition
			&& CallPosition < ExceptionCheckPosition,
			TEXT("Bridge operands must materialize in HIR order, call in formal order, and check failure immediately")));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("return FProviderPrivateBinds::PrivateAdd("),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Combined.Contains(TEXT("FunctionId=42"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("extern "),
			ESearchCase::CaseSensitive),
			TEXT("A private bridge must not guess a linkable extern declaration")));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("reinterpret_cast"),
			ESearchCase::CaseSensitive),
			TEXT("A private bridge must not reconstruct a caller from a raw address")));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("uintptr_t"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Combined.Contains(
			TEXT("0x"),
			ESearchCase::CaseSensitive),
			TEXT("Generated bridge metadata must use stable hashes and a slot, not a raw pointer literal")));

		FAngelscriptTypedASTJITProviderEntryPlan EntryPlan;
		EntryPlan.bHasVMEntry = true;
		EntryPlan.bHasRawEntry = true;
		EntryPlan.bHasParmsEntry = true;
		EntryPlan.Return.BodyCppType = TEXT("int32");
		EntryPlan.Return.EntryCppType = TEXT("asDWORD");
		EntryPlan.CanonicalDeclaration =
			TEXT("int OrderedNativeCall(int A, int B, int C)");
		EntryPlan.SourceSection =
			TEXT("/Angelscript/Game/OrderedNativeCall.as");
		EntryPlan.SourceLine = 1;
		EntryPlan.InstrumentationProfile = InstrumentationProfile;
		for (int32 ParameterIndex = 0; ParameterIndex < 3; ++ParameterIndex)
		{
			FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
				EntryPlan.Parameters.Emplace_GetRef();
			Parameter.BodyCppType = TEXT("int32");
			Parameter.EntryCppType = TEXT("asDWORD");
			Parameter.VMFrameOffsetDWords = ParameterIndex;
		}
		const FAngelscriptTypedASTJITProviderEmission ProviderEmission =
			EmitAngelscriptTypedASTJITProviderFunction(EntryPlan, Emission);
		ASSERT_THAT(IsTrue(ProviderEmission.bSuccess,
			ProviderEmission.FailureDetail));
		ASSERT_THAT(IsTrue(ProviderEmission.ImplementationTemplate.Contains(
			TEXT("{SYMBOL_PREFIX}_TypedBody(Execution,"),
			ESearchCase::CaseSensitive),
			TEXT("Raw, VM and Parms routes must preserve the same execution state through the raw adapter")));
		ASSERT_THAT(IsFalse(ProviderEmission.ImplementationTemplate.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(ReflectedParmsLayoutAccountsForSyntheticStaticWorldContext)
	{
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Profile =
			MakeProfile(ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.InstrumentationProfile = &Profile;
		const FAngelscriptTypedASTJITEmission Body =
			EmitTypedASTJITFunction(MakeScalarBranch(), MakeShape(), Options);
		ASSERT_THAT(IsTrue(Body.bSuccess, Body.FailureDetail));

		FAngelscriptTypedASTJITProviderEntryPlan EntryPlan;
		EntryPlan.bHasVMEntry = true;
		EntryPlan.bHasRawEntry = true;
		EntryPlan.bHasParmsEntry = true;
		EntryPlan.ReflectedParmsLayout =
			EAngelscriptTypedASTJITReflectedParmsLayout::
				AppendGeneratedWorldContext;
		EntryPlan.Return.BodyCppType = TEXT("int32");
		EntryPlan.Return.EntryCppType = TEXT("asDWORD");
		EntryPlan.CanonicalDeclaration = TEXT("int StaticRoot(int Value)");
		EntryPlan.SourceSection = TEXT("/Angelscript/Game/StaticRoot.as");
		EntryPlan.SourceLine = 7;
		EntryPlan.InstrumentationProfile = Profile;
		FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
			EntryPlan.Parameters.Emplace_GetRef();
		Parameter.BodyCppType = TEXT("int32");
		Parameter.EntryCppType = TEXT("asDWORD");
		Parameter.VMFrameOffsetDWords = 0;

		const FAngelscriptTypedASTJITProviderEmission Provider =
			EmitAngelscriptTypedASTJITProviderFunction(EntryPlan, Body);
		ASSERT_THAT(IsTrue(Provider.bSuccess, Provider.FailureDetail));
		const FString& Source = Provider.ImplementationTemplate;
		const int32 ParameterPosition = Source.Find(
			TEXT("const SIZE_T ParmOffset_0 = ParmsOffset;"));
		const int32 WorldContextPosition = Source.Find(
			TEXT("const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;"));
		const int32 ReturnPosition = Source.Find(
			TEXT("const SIZE_T ReturnParmOffset = ParmsOffset;"));
		ASSERT_THAT(IsTrue(
			ParameterPosition != INDEX_NONE
				&& ParameterPosition < WorldContextPosition
				&& WorldContextPosition < ReturnPosition,
			TEXT("Reflected Parms must place the synthetic UObject pointer after declared arguments and before the return property")));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"ParmsOffset = Align(ParmsOffset, alignof(void*));\n"
			"\tconst SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;\n"
			"\tParmsOffset += sizeof(void*);"))));
	}

	TEST_METHOD(FinalInstanceProviderPreservesNativeObjectRawAbi)
	{
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Profile =
			MakeProfile(ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.InstrumentationProfile = &Profile;
		const FAngelscriptTypedASTJITEmission Body =
			EmitTypedASTJITFunction(MakeScalarBranch(), MakeShape(), Options);
		ASSERT_THAT(IsTrue(Body.bSuccess, Body.FailureDetail));

		FAngelscriptTypedASTJITProviderEntryPlan EntryPlan;
		EntryPlan.bHasVMEntry = true;
		EntryPlan.bHasRawEntry = true;
		EntryPlan.bHasParmsEntry = true;
		EntryPlan.bHasNativeObjectSlot = true;
		EntryPlan.Return.BodyCppType = TEXT("int32");
		EntryPlan.Return.EntryCppType = TEXT("asDWORD");
		EntryPlan.CanonicalDeclaration =
			TEXT("int UTypedFinalReceiver::ReadValue(int Value) final");
		EntryPlan.SourceSection =
			TEXT("/Angelscript/Game/TypedFinalReceiver.as");
		EntryPlan.SourceLine = 11;
		EntryPlan.InstrumentationProfile = Profile;
		FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
			EntryPlan.Parameters.Emplace_GetRef();
		Parameter.BodyCppType = TEXT("int32");
		Parameter.EntryCppType = TEXT("asDWORD");
		Parameter.VMFrameOffsetDWords = 2;

		const FAngelscriptTypedASTJITProviderEmission Provider =
			EmitAngelscriptTypedASTJITProviderFunction(EntryPlan, Body);
		ASSERT_THAT(IsTrue(Provider.bSuccess, Provider.FailureDetail));
		const FString Source = Provider.RawEntryDeclaration
			+ Provider.ImplementationTemplate;
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"(FScriptExecution& Execution, void* Object, asDWORD p_0)"))));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"\t\tObject,\n\t\tfalse);")),
			TEXT("The Typed frame must observe the exact instance passed by the optimized wrapper")));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"{RAW_SYMBOL}(Execution,\n\t\t*(void**)l_fp,")),
			TEXT("The VM adapter must forward its authoritative native object slot into Raw")));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"{RAW_SYMBOL}(Execution,\n\t\tObject,")),
			TEXT("The reflected Parms adapter must forward its ProcessEvent object into Raw")));
		ASSERT_THAT(IsFalse(Source.Contains(TEXT(
			"static_cast<void>(Object);")),
			TEXT("An instance adapter must not discard the object supplied by the wrapper")));
	}

	TEST_METHOD(ExplicitInstrumentationProfileEmitsOnlyApprovedPositionHooks)
	{
		asCTypedSemanticFunction Function = MakeScalarBranch();
		Function.statements[0].span.row = 11;
		Function.statements[0].span.column = 2;
		Function.statements[0].safePointRole =
			asETypedSemanticSafePointRole::Statement;
		Function.statements[1].span.row = 13;
		Function.statements[1].span.column = 6;
		Function.statements[3].span.row = 12;
		Function.statements[3].span.column = 4;
		Function.statements[3].safePointRole =
			asETypedSemanticSafePointRole::Statement;
		Function.statements[4].span.row = 17;
		Function.statements[4].span.column = 8;

		const FAngelscriptTypedASTJITGenerationCapabilityProfile Position =
			MakeProfile(ECapability::FramePosition
				| ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions PositionOptions;
		PositionOptions.InstrumentationProfile = &Position;
		const FAngelscriptTypedASTJITEmission PositionEmission =
			EmitTypedASTJITFunction(Function, MakeShape(), PositionOptions);

		ASSERT_THAT(IsTrue(PositionEmission.bSuccess,
			PositionEmission.FailureDetail));
		ASSERT_THAT(IsTrue(PositionEmission.bRequiresInstrumentationFrame));
		ASSERT_THAT(IsTrue(
			PositionEmission.InstrumentationProfileIdentityHash
				== Position.IdentityHash));
		ASSERT_THAT(IsTrue(PositionEmission.Declaration.Contains(TEXT(
			"FScopeTypedASTJITExecutionFrame& TypedFrame"))));
		ASSERT_THAT(IsTrue(PositionEmission.Definition.Contains(
			TEXT("TypedFrame.SetSourcePosition(11, 2);"))));
		ASSERT_THAT(IsTrue(PositionEmission.Definition.Contains(
			TEXT("TypedFrame.SetSourcePosition(12, 4);"))));
		ASSERT_THAT(IsTrue(PositionEmission.Definition.Contains(
			TEXT("TypedFrame.SetSourcePosition(13, 6);"))));
		ASSERT_THAT(IsTrue(PositionEmission.Definition.Contains(
			TEXT("TypedFrame.SetSourcePosition(17, 8);"))));
		for (const TCHAR* ForbiddenObserver : {
			TEXT("AngelscriptLineCallback"),
			TEXT("AngelscriptLoopDetectionCallback"),
			TEXT("HitLine("),
			TEXT("FAngelscriptCodeCoverage"),
			TEXT("DebugServer")})
		{
			ASSERT_THAT(IsFalse(PositionEmission.Definition.Contains(
				ForbiddenObserver, ESearchCase::CaseSensitive),
				TEXT("Position-only output must remain safe on arbitrary worker threads and must not call the game-thread observer pipeline")));
		}

		const FAngelscriptTypedASTJITGenerationCapabilityProfile RecursionOnly =
			MakeProfile(ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions RecursionOptions;
		RecursionOptions.InstrumentationProfile = &RecursionOnly;
		const FAngelscriptTypedASTJITEmission RecursionEmission =
			EmitTypedASTJITFunction(Function, MakeShape(), RecursionOptions);
		ASSERT_THAT(IsTrue(RecursionEmission.bSuccess,
			RecursionEmission.FailureDetail));
		ASSERT_THAT(IsFalse(RecursionEmission.bRequiresInstrumentationFrame));
		ASSERT_THAT(IsFalse(RecursionEmission.Definition.Contains(
			TEXT("SetSourcePosition("))));
		ASSERT_THAT(IsFalse(
			RecursionEmission.InstrumentationProfileIdentityHash
				== PositionEmission.InstrumentationProfileIdentityHash));
		ASSERT_THAT(IsFalse(
			RecursionEmission.Definition == PositionEmission.Definition));

		const FAngelscriptTypedASTJITGenerationCapabilityProfile Unsupported =
			MakeProfile(ECapability::FramePosition
				| ECapability::LineCallback
				| ECapability::Coverage
				| ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions UnsupportedOptions;
		UnsupportedOptions.InstrumentationProfile = &Unsupported;
		const FAngelscriptTypedASTJITEmission UnsupportedEmission =
			EmitTypedASTJITFunction(Function, MakeShape(), UnsupportedOptions);
		ASSERT_THAT(IsFalse(UnsupportedEmission.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::EmitterFailure,
			UnsupportedEmission.FailureReason));
		ASSERT_THAT(IsTrue(UnsupportedEmission.FailureDetail.Contains(
			TEXT("UnsupportedInstrumentationCapabilities"))));
		AssertNoPartialOutput(*TestRunner, UnsupportedEmission);
	}

	TEST_METHOD(ProviderEntryOwnsTypedFrameAndFeedsPositionAwareBody)
	{
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Profile =
			MakeProfile(ECapability::FramePosition
				| ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.InstrumentationProfile = &Profile;
		const FAngelscriptTypedASTJITEmission Body =
			EmitTypedASTJITFunction(
				MakeScalarBranch(),
				MakeShape(),
				Options);
		ASSERT_THAT(IsTrue(Body.bSuccess, Body.FailureDetail));
		ASSERT_THAT(IsFalse(Body.bRequiresExecutionState,
			TEXT("A direct-only Typed body must keep its ordinary C++ signature")));
		ASSERT_THAT(IsTrue(Body.bRequiresInstrumentationFrame));

		FAngelscriptTypedASTJITProviderEntryPlan EntryPlan;
		EntryPlan.bHasVMEntry = true;
		EntryPlan.bHasRawEntry = true;
		EntryPlan.Return.BodyCppType = TEXT("int32");
		EntryPlan.Return.EntryCppType = TEXT("asDWORD");
		EntryPlan.CanonicalDeclaration = TEXT("int FrameRoot(int Value)");
		EntryPlan.SourceSection =
			TEXT("/Angelscript/Game/TypedFrameFixture.as");
		EntryPlan.SourceLine = 37;
		EntryPlan.InstrumentationProfile = Profile;
		FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
			EntryPlan.Parameters.Emplace_GetRef();
		Parameter.BodyCppType = TEXT("int32");
		Parameter.EntryCppType = TEXT("asDWORD");
		Parameter.VMFrameOffsetDWords = 0;

		const FAngelscriptTypedASTJITProviderEmission ProviderEmission =
			EmitAngelscriptTypedASTJITProviderFunction(EntryPlan, Body);
		ASSERT_THAT(IsTrue(
			ProviderEmission.bSuccess,
			ProviderEmission.FailureDetail));

		const FString& Source = ProviderEmission.ImplementationTemplate;
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"FScopeTypedASTJITExecutionFrame TypedFrame(\n"
			"\t\tExecution,\n"
			"\t\tExecution.jitFunction,\n"
			"\t\t\"/Angelscript/Game/TypedFrameFixture.as\",\n"
			"\t\t\"int FrameRoot(int Value)\",\n"
			"\t\t37,\n"
			"\t\tnullptr,\n"
			"\t\ttrue);"))));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"if (UNLIKELY(!TypedFrame.IsActive()))\n"
			"\t{\n"
			"\t\treturn {};\n"
			"\t}")),
			TEXT("Recursion exhaustion must stop before entering the native body")));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("return {SYMBOL_PREFIX}_TypedBody(TypedFrame,"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Body.Declaration.Contains(
			TEXT("FScopeTypedASTJITExecutionFrame& TypedFrame"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Body.Declaration.Contains(
			TEXT("FScriptExecution"),
			ESearchCase::CaseSensitive),
			TEXT("Position instrumentation must pass the narrow frame rather than the generic execution dispatcher")));
	}

	TEST_METHOD(InternalHelperWrapperOwnsCurrentFunctionFrameAndRecursionBudget)
	{
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Profile =
			MakeProfile(ECapability::FramePosition
				| ECapability::RecursionBudget);
		FAngelscriptTypedASTJITEmitOptions Options;
		Options.InstrumentationProfile = &Profile;
		FAngelscriptTypedASTJITFunctionShape Shape = MakeShape();
		Shape.SymbolName = TEXT("ASJIT_Helper_Calculate_deadbeef_Body");
		const FAngelscriptTypedASTJITEmission Body =
			EmitTypedASTJITFunction(MakeScalarBranch(), Shape, Options);
		ASSERT_THAT(IsTrue(Body.bSuccess, Body.FailureDetail));
		ASSERT_THAT(IsFalse(Body.bRequiresExecutionState));

		FAngelscriptTypedASTJITInternalHelperPlan HelperPlan;
		HelperPlan.Symbol = TEXT("ASJIT_Helper_Calculate_deadbeef");
		HelperPlan.BodySymbol = Shape.SymbolName;
		HelperPlan.ReturnBodyCppType = TEXT("int32");
		HelperPlan.CanonicalDeclaration =
			TEXT("int Calculate(int Left, int Right)");
		HelperPlan.SourceSection =
			TEXT("/Angelscript/Game/TypedHelperFixture.as");
		HelperPlan.SourceLine = 19;
		HelperPlan.InstrumentationProfile = Profile;
		HelperPlan.FunctionReferenceSlotIndex = 2;
		for (int32 ParameterIndex = 0; ParameterIndex < 2; ++ParameterIndex)
		{
			FAngelscriptTypedASTJITProviderParameterPlan& Parameter =
				HelperPlan.Parameters.Emplace_GetRef();
			Parameter.BodyCppType = TEXT("int32");
			Parameter.EntryCppType = TEXT("asDWORD");
		}

		const FAngelscriptTypedASTJITInternalHelperEmission Helper =
			EmitAngelscriptTypedASTJITInternalHelper(HelperPlan, Body);
		ASSERT_THAT(IsTrue(Helper.bSuccess, Helper.FailureDetail));
		ASSERT_THAT(IsTrue(Helper.ForwardDeclaration.Contains(TEXT(
			"static int32 ASJIT_Helper_Calculate_deadbeef(\n"
			"\tFScriptExecution& Execution,\n"
			"\tconst int32 as_helper_p0,\n"
			"\tconst int32 as_helper_p1);"))));
		const FString& Source = Helper.Implementation;
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"asIScriptFunction* const TypedFunction =\n"
			"\t\tFAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(\n"
			"\t\t\tExecution, 2u);"))));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"FScopeTypedASTJITExecutionFrame TypedFrame(\n"
			"\t\tExecution,\n"
			"\t\tTypedFunction,\n"
			"\t\t\"/Angelscript/Game/TypedHelperFixture.as\",\n"
			"\t\t\"int Calculate(int Left, int Right)\",\n"
			"\t\t19,\n"
			"\t\tnullptr,\n"
			"\t\ttrue);"))));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"if (UNLIKELY(!TypedFrame.IsActive()))\n"
			"\t{\n"
			"\t\treturn {};\n"
			"\t}"))));
		ASSERT_THAT(IsTrue(Source.Contains(TEXT(
			"return ASJIT_Helper_Calculate_deadbeef_Body(\n"
			"\t\tTypedFrame, as_helper_p0, as_helper_p1);"))));
		ASSERT_THAT(IsTrue(Helper.RequiredIncludes.Contains(
			TEXT("StaticJIT/StaticJITHeader.h"))));
		ASSERT_THAT(IsFalse((Helper.ForwardDeclaration + Source).Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(InvalidAndUnsupportedInputsReturnNoPartialCpp)
	{
		asCTypedSemanticFunction Dangling = MakeScalarBranch();
		Dangling.statements[4].valueExpression = asTypedSemanticExpressionId(99);
		const FAngelscriptTypedASTJITEmission Invalid =
			EmitTypedASTJITFunction(Dangling, MakeShape(), {});
		ASSERT_THAT(IsFalse(Invalid.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR,
			Invalid.FailureReason));
		AssertNoPartialOutput(*TestRunner, Invalid);

		asCTypedSemanticFunction Unsupported = MakeScalarBranch();
		Unsupported.expressions[0].kind = asETypedSemanticExpressionKind::Unsupported;
		Unsupported.expressions[0].unsupportedKind =
			asETypedSemanticUnsupportedKind::Lambda;
		const FAngelscriptTypedASTJITEmission UnsupportedResult =
			EmitTypedASTJITFunction(Unsupported, MakeShape(), {});
		ASSERT_THAT(IsFalse(UnsupportedResult.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
			UnsupportedResult.FailureReason));
		AssertNoPartialOutput(*TestRunner, UnsupportedResult);

		FAngelscriptTypedASTJITFunctionShape UnsupportedShape = MakeShape();
		UnsupportedShape.ReturnType = asCDataType();
		const FAngelscriptTypedASTJITEmission UnsupportedType =
			EmitTypedASTJITFunction(MakeScalarBranch(), UnsupportedShape, {});
		ASSERT_THAT(IsFalse(UnsupportedType.bSuccess));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
			UnsupportedType.FailureReason));
		AssertNoPartialOutput(*TestRunner, UnsupportedType);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
