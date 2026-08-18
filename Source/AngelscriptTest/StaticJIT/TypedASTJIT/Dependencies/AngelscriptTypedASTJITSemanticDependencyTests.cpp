#include "CQTest.h"

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITDependencies.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptTypedASTJITSemanticDependencyTests_Private
{
	static FAngelscriptHash256 RepeatedByteHash(const uint8 Value)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (uint8& Byte : Bytes)
		{
			Byte = Value;
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptStableFunctionKey FunctionKey(const uint8 Value)
	{
		return FAngelscriptStableFunctionKey{RepeatedByteHash(Value)};
	}

	static FAngelscriptArtifactReference Reference(
		const EAngelscriptArtifactReferenceKind Kind,
		const uint8 KeyByte,
		const uint8 AbiByte)
	{
		FAngelscriptArtifactReference Result;
		Result.Kind = Kind;
		Result.StableKey = RepeatedByteHash(KeyByte);
		Result.ExpectedAbi = RepeatedByteHash(AbiByte);
		return Result;
	}

	static asSTypedSemanticSourceSpan Span()
	{
		asSTypedSemanticSourceSpan Result;
		Result.section = "/Angelscript/Game/TypedDependencies.as";
		Result.sourceOffset = 71;
		Result.sourceLength = 8;
		Result.row = 19;
		Result.column = 6;
		return Result;
	}

	static void MarkVerifiedEmptyCleanup(
		asCTypedSemanticFunction& Function)
	{
		asSTypedSemanticCleanupPlan Plan;
		Plan.id = asTypedSemanticCleanupPlanId(0);
		Function.cleanupPlans.PushLast(Plan);
		Function.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::VerifiedEmpty;
		Function.header.universalCleanupPlan = Plan.id;
		Function.header.cleanupPlanCoversAllTransfers = true;
		for (asUINT Index = 0; Index < Function.statements.GetLength(); ++Index)
		{
			if (Function.statements[Index].kind
				== asETypedSemanticStatementKind::Return)
			{
				Function.statements[Index].cleanupPlan = Plan.id;
			}
		}
	}

	static asCTypedSemanticFunction MakeScriptCallHIR(
		const int32 TargetFunctionId)
	{
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		asCTypedSemanticFunction Function;
		Function.declaration = "int DependencyRoot(int Value)";
		Function.header.callingModule = "TypedDependencies";
		Function.header.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Function.header.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Function.header.bodyOwnerModule = "TypedDependencies";

		asSTypedSemanticSymbol Parameter;
		Parameter.kind = asETypedSemanticSymbolKind::Parameter;
		Parameter.name = "Value";
		Parameter.type = IntType;
		Parameter.span = Span();
		const asTypedSemanticSymbolId ParameterId =
			Function.AddSymbol(Parameter);

		asSTypedSemanticExpression Read;
		Read.kind = asETypedSemanticExpressionKind::Symbol;
		Read.type = IntType;
		Read.span = Span();
		Read.symbol = ParameterId;
		const asTypedSemanticExpressionId ReadId =
			Function.AddExpression(Read);

		asSTypedSemanticExpression Call;
		Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
		Call.type = IntType;
		Call.span = Span();
		Call.callTargetKind =
			asETypedSemanticCallTargetKind::ScriptFunction;
		Call.resolvedFunctionId = TargetFunctionId;
		Call.targetProvenance.canonicalDeclaration =
			"int DependencyHelper(int)";
		Call.targetProvenance.declarationModule = "TypedDependencies";
		Call.targetProvenance.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Call.targetProvenance.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Call.targetProvenance.bodyOwnerModule = "TypedDependencies";
		Call.operands.PushLast(ReadId);
		Call.formalParameterIndices.PushLast(0);
		Call.sourceArgumentOrdinals.PushLast(0);
		Call.evaluationSequence.PushLast(0);
		Call.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::SourcePositional);
		asSTypedSemanticArgumentProvenance Provenance;
		Provenance.origin =
			asETypedSemanticArgumentOrigin::SourcePositional;
		Provenance.sourceArgumentOrdinal = 0;
		Provenance.processedSpan = Span();
		Call.argumentProvenance.PushLast(Provenance);
		asSTypedSemanticEvaluationStep Evaluation;
		Evaluation.role = asETypedSemanticEvaluationRole::Argument;
		Evaluation.expression = ReadId;
		Evaluation.formalParameterIndex = 0;
		Evaluation.sourceArgumentOrdinal = 0;
		Call.evaluationSteps.PushLast(Evaluation);
		const asTypedSemanticExpressionId CallId =
			Function.AddExpression(Call);

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = Span();
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = CallId;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId =
			Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = Span();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	struct FDependencyFixture
	{
		FAngelscriptStableFunctionKey RootKey = FunctionKey(0x11);
		FAngelscriptArtifactReference Callee = Reference(
			EAngelscriptArtifactReferenceKind::ScriptFunction,
			0x21,
			0x22);
		FAngelscriptArtifactReference Extra = Reference(
			EAngelscriptArtifactReferenceKind::RuntimeHelper,
			0x31,
			0x32);
		TArray<FAngelscriptStaticJITDependencyView> References;
		TArray<FAngelscriptStaticJITSemanticDependencyView> Dependencies;
		FAngelscriptStaticJITCompiledSourceGraph Graph;

		FDependencyFixture()
		{
			References.Add({RootKey, Callee, TEXT("int DependencyHelper(int)")});
			References.Add({RootKey, Extra, TEXT("RuntimeExtra")});
			Dependencies.Add({
				RootKey,
				EAngelscriptArtifactSemanticDependencyKind::Signature,
				Callee,
				{},
				INDEX_NONE,
			});
			Dependencies.Add({
				RootKey,
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				Callee,
				RepeatedByteHash(0x23),
				INDEX_NONE,
			});
			Dependencies.Add({
				RootKey,
				EAngelscriptArtifactSemanticDependencyKind::CompileOption,
				Extra,
				RepeatedByteHash(0x33),
				INDEX_NONE,
			});
			Graph.Dependencies = References;
			Graph.SemanticDependencies = Dependencies;
		}
	};

	static FAngelscriptTypedASTJITScriptCallEmissionPlan MakeScriptPlan(
		const FDependencyFixture& Fixture,
		const int32 TargetFunctionId)
	{
		FAngelscriptTypedASTJITScriptCallEmissionPlan Plan;
		Plan.Expression = asTypedSemanticExpressionId(1);
		Plan.EngineLocalTargetFunctionId = TargetFunctionId;
		Plan.Symbol = TEXT("ASJIT_DependencyHelper");
		Plan.TargetReference = Fixture.Callee;
		Plan.bRequiresFunctionContent = true;
		return Plan;
	}

	static FAngelscriptStaticJITCompiledFunctionView MakeFunctionView(
		const FDependencyFixture& Fixture)
	{
		FAngelscriptStaticJITCompiledFunctionView Function;
		Function.FunctionKey = Fixture.RootKey;
		Function.ModuleKey.Hash = RepeatedByteHash(0x12);
		Function.CanonicalDeclaration = TEXT("int DependencyRoot(int)");
		return Function;
	}

	static FAngelscriptTypedASTJITFunctionShape MakeShape()
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_DependencyRoot");
		Shape.ParameterTypes.Add(
			asCDataType::CreatePrimitive(ttInt, false));
		Shape.ReturnType = asCDataType::CreatePrimitive(ttInt, false);
		return Shape;
	}

	static FDependencyFixture MakeSignatureOnlyFixture(
		const FAngelscriptArtifactReference& Callee)
	{
		FDependencyFixture Fixture;
		Fixture.Callee = Callee;
		Fixture.References.Reset();
		Fixture.Dependencies.Reset();
		Fixture.References.Add({
			Fixture.RootKey,
			Callee,
			TEXT("int DependencyHelper(int)"),
		});
		Fixture.References.Add({
			Fixture.RootKey,
			Fixture.Extra,
			TEXT("RuntimeExtra"),
		});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::Signature,
			Callee,
			{},
			INDEX_NONE,
		});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::CompileOption,
			Fixture.Extra,
			RepeatedByteHash(0x33),
			INDEX_NONE,
		});
		Fixture.Graph.Dependencies = Fixture.References;
		Fixture.Graph.SemanticDependencies = Fixture.Dependencies;
		return Fixture;
	}

	static void FillResolvedCall(
		asSTypedSemanticExpression& Call,
		const asCDataType& IntType,
		const asETypedSemanticCallTargetKind TargetKind,
		const int32 TargetFunctionId,
		const asETypedSemanticBodyOwnership BodyOwnership,
		const asTypedSemanticExpressionId Operand,
		const bool bHostHidden)
	{
		Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
		Call.type = IntType;
		Call.span = Span();
		Call.callTargetKind = TargetKind;
		Call.resolvedFunctionId = TargetFunctionId;
		Call.targetProvenance.canonicalDeclaration =
			"int DependencyHelper(int)";
		Call.targetProvenance.declarationModule = "TypedDependencies";
		Call.targetProvenance.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Call.targetProvenance.bodyOwnership = BodyOwnership;
		Call.targetProvenance.bodyOwnerModule = "TypedDependencies";
		Call.targetProvenance.importedCanonicalSignature =
			"int DependencyHelper(int)";
		Call.operands.PushLast(Operand);
		Call.formalParameterIndices.PushLast(0);
		Call.sourceArgumentOrdinals.PushLast(0);
		Call.evaluationSequence.PushLast(0);
		Call.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::SourcePositional);
		asSTypedSemanticArgumentProvenance Provenance;
		Provenance.origin =
			asETypedSemanticArgumentOrigin::SourcePositional;
		Provenance.sourceArgumentOrdinal = 0;
		Provenance.processedSpan = Span();
		Call.argumentProvenance.PushLast(Provenance);
		asSTypedSemanticEvaluationStep Evaluation;
		Evaluation.role = asETypedSemanticEvaluationRole::Argument;
		Evaluation.expression = Operand;
		Evaluation.formalParameterIndex = 0;
		Evaluation.sourceArgumentOrdinal = 0;
		Call.evaluationSteps.PushLast(Evaluation);
		if (bHostHidden)
		{
			asSTypedSemanticArgumentProvenance Hidden;
			Hidden.origin = asETypedSemanticArgumentOrigin::HostHidden;
			Hidden.sourceArgumentOrdinal = -1;
			Hidden.processedSpan = Span();
			Call.argumentProvenance.PushLast(Hidden);
			Call.argumentOrigins.PushLast(
				asETypedSemanticArgumentOrigin::HostHidden);
			Call.operands.PushLast(Operand);
			Call.formalParameterIndices.PushLast(-1);
			Call.sourceArgumentOrdinals.PushLast(-1);
		}
	}

	static asCTypedSemanticFunction MakeResolvedCallHIR(
		const asETypedSemanticCallTargetKind TargetKind,
		const int32 TargetFunctionId,
		const asETypedSemanticBodyOwnership BodyOwnership,
		const bool bHostHidden = false)
	{
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		asCTypedSemanticFunction Function;
		Function.declaration = "int DependencyRoot(int Value)";
		Function.header.callingModule = "TypedDependencies";
		Function.header.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Function.header.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Function.header.bodyOwnerModule = "TypedDependencies";

		asSTypedSemanticSymbol Parameter;
		Parameter.kind = asETypedSemanticSymbolKind::Parameter;
		Parameter.name = "Value";
		Parameter.type = IntType;
		Parameter.span = Span();
		const asTypedSemanticSymbolId ParameterId =
			Function.AddSymbol(Parameter);

		asSTypedSemanticExpression Read;
		Read.kind = asETypedSemanticExpressionKind::Symbol;
		Read.type = IntType;
		Read.span = Span();
		Read.symbol = ParameterId;
		const asTypedSemanticExpressionId ReadId =
			Function.AddExpression(Read);

		asSTypedSemanticExpression Call;
		FillResolvedCall(
			Call,
			IntType,
			TargetKind,
			TargetFunctionId,
			BodyOwnership,
			ReadId,
			bHostHidden);
		const asTypedSemanticExpressionId CallId =
			Function.AddExpression(Call);

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = Span();
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = CallId;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId =
			Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = Span();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeTwoScriptCallHIR(
		const int32 FirstTargetFunctionId,
		const int32 SecondTargetFunctionId)
	{
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		asCTypedSemanticFunction Function;
		Function.declaration = "int DependencyRoot(int Value)";
		Function.header.callingModule = "TypedDependencies";
		Function.header.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Function.header.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Function.header.bodyOwnerModule = "TypedDependencies";

		asSTypedSemanticSymbol Parameter;
		Parameter.kind = asETypedSemanticSymbolKind::Parameter;
		Parameter.name = "Value";
		Parameter.type = IntType;
		Parameter.span = Span();
		const asTypedSemanticSymbolId ParameterId =
			Function.AddSymbol(Parameter);

		asSTypedSemanticExpression Read;
		Read.kind = asETypedSemanticExpressionKind::Symbol;
		Read.type = IntType;
		Read.span = Span();
		Read.symbol = ParameterId;
		const asTypedSemanticExpressionId ReadId =
			Function.AddExpression(Read);

		asSTypedSemanticExpression FirstCall;
		FillResolvedCall(
			FirstCall,
			IntType,
			asETypedSemanticCallTargetKind::ScriptFunction,
			FirstTargetFunctionId,
			asETypedSemanticBodyOwnership::CallingModule,
			ReadId,
			false);
		const asTypedSemanticExpressionId FirstCallId =
			Function.AddExpression(FirstCall);

		asSTypedSemanticExpression SecondCall;
		FillResolvedCall(
			SecondCall,
			IntType,
			asETypedSemanticCallTargetKind::ScriptFunction,
			SecondTargetFunctionId,
			asETypedSemanticBodyOwnership::CallingModule,
			ReadId,
			false);
		const asTypedSemanticExpressionId SecondCallId =
			Function.AddExpression(SecondCall);

		asSTypedSemanticExpression Add;
		Add.kind = asETypedSemanticExpressionKind::Binary;
		Add.type = IntType;
		Add.span = Span();
		Add.operatorToken = "+";
		Add.operands.PushLast(FirstCallId);
		Add.operands.PushLast(SecondCallId);
		const asTypedSemanticExpressionId AddId =
			Function.AddExpression(Add);

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = Span();
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = AddId;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId =
			Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = Span();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeGlobalHIR(
		const asETypedSemanticExpressionKind Kind,
		const int32 GlobalPropertyId)
	{
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		asCTypedSemanticFunction Function;
		Function.declaration = "int DependencyRoot()";
		Function.header.callingModule = "TypedDependencies";
		Function.header.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Function.header.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Function.header.bodyOwnerModule = "TypedDependencies";

		asSTypedSemanticExpression Global;
		Global.kind = Kind;
		Global.type = IntType;
		Global.span = Span();
		Global.resolvedGlobalPropertyId = GlobalPropertyId;
		const asTypedSemanticExpressionId GlobalId =
			Function.AddExpression(Global);

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = Span();
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = GlobalId;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId =
			Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = Span();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static int32 CountUses(
		const FAngelscriptTypedASTJITSemanticUseManifest& Manifest,
		const EAngelscriptTypedASTJITSemanticUseKind Kind)
	{
		int32 Count = 0;
		for (const FAngelscriptTypedASTJITSemanticUse& Use : Manifest.Uses)
		{
			Count += Use.Kind == Kind ? 1 : 0;
		}
		return Count;
	}

	static int32 CountArtifactKinds(
		const FAngelscriptTypedASTJITSemanticUseManifest& Manifest,
		const EAngelscriptArtifactSemanticDependencyKind Kind)
	{
		int32 Count = 0;
		for (const FAngelscriptArtifactSemanticDependency& Dependency
			: Manifest.ArtifactDependencies)
		{
			Count += Dependency.Kind == Kind ? 1 : 0;
		}
		return Count;
	}

	static bool HasSynthesizedContent(
		const FAngelscriptTypedASTJITSemanticUseManifest& Manifest,
		const FAngelscriptArtifactReference& Target,
		const TOptional<FAngelscriptHash256>& ExpectedContent)
	{
		return Manifest.ArtifactDependencies.ContainsByPredicate(
			[&Target, &ExpectedContent](
				const FAngelscriptArtifactSemanticDependency& Dependency)
			{
				return Dependency.Kind
						== EAngelscriptArtifactSemanticDependencyKind::
							FunctionContent
					&& Dependency.Target == Target
					&& Dependency.ExpectedContentOrValue == ExpectedContent;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITSemanticDependencyTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.Dependencies.SemanticManifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptCallReconcilesSignatureContentAndPreservesExtraDependencies)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 77;
		FDependencyFixture Fixture;
		const asCTypedSemanticFunction HIR =
			MakeScriptCallHIR(TargetFunctionId);
		const FAngelscriptTypedASTJITScriptCallEmissionPlan Plan =
			MakeScriptPlan(Fixture, TargetFunctionId);

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph,
				{},
				MakeArrayView(&Plan, 1));

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(IsTrue(Analysis.FailureReason
			== EAngelscriptTypedASTJITFallbackReason::None));
		ASSERT_THAT(AreEqual(3, Analysis.Manifest.ArtifactDependencies.Num(),
			TEXT("Compiler-only dependencies must remain in the Typed artifact manifest")));
		ASSERT_THAT(AreEqual(2, Analysis.Manifest.RequiredReferences.Num(),
			TEXT("Compiler-only stable references must not be dropped")));
		ASSERT_THAT(IsTrue(Analysis.Manifest.Uses.ContainsByPredicate(
			[](const FAngelscriptTypedASTJITSemanticUse& Use)
			{
				return Use.Kind
					== EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature;
			})));
		ASSERT_THAT(IsTrue(Analysis.Manifest.Uses.ContainsByPredicate(
			[](const FAngelscriptTypedASTJITSemanticUse& Use)
			{
				return Use.Kind
					== EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent;
			})));
	}

	TEST_METHOD(MissingOrWrongKindScriptDependencyFailsAtTheHIRUse)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 77;
		const asCTypedSemanticFunction HIR =
			MakeScriptCallHIR(TargetFunctionId);
		const FAngelscriptTypedASTJITFunctionShape Shape = MakeShape();

		FDependencyFixture Missing;
		Missing.Dependencies.RemoveAt(0);
		Missing.Graph.SemanticDependencies = Missing.Dependencies;
		const FAngelscriptTypedASTJITScriptCallEmissionPlan MissingPlan =
			MakeScriptPlan(Missing, TargetFunctionId);
		const FAngelscriptTypedASTJITDependencyAnalysis MissingAnalysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				Shape,
				MakeFunctionView(Missing),
				Missing.Graph,
				{},
				MakeArrayView(&MissingPlan, 1));
		ASSERT_THAT(IsFalse(MissingAnalysis.bValid));
		ASSERT_THAT(IsTrue(MissingAnalysis.FailureReason
			== EAngelscriptTypedASTJITFallbackReason::
				SemanticDependencyMismatch));
		ASSERT_THAT(AreEqual(19u, MissingAnalysis.FailureSpan.row));
		ASSERT_THAT(IsTrue(MissingAnalysis.FailureDetail.Contains(
			TEXT("ScriptCallSignature"), ESearchCase::CaseSensitive)));

		FDependencyFixture WrongKind;
		WrongKind.Dependencies[0].Kind =
			EAngelscriptArtifactSemanticDependencyKind::Declaration;
		WrongKind.Graph.SemanticDependencies = WrongKind.Dependencies;
		const FAngelscriptTypedASTJITScriptCallEmissionPlan WrongKindPlan =
			MakeScriptPlan(WrongKind, TargetFunctionId);
		const FAngelscriptTypedASTJITDependencyAnalysis WrongKindAnalysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				Shape,
				MakeFunctionView(WrongKind),
				WrongKind.Graph,
				{},
				MakeArrayView(&WrongKindPlan, 1));
		ASSERT_THAT(IsFalse(WrongKindAnalysis.bValid));
		ASSERT_THAT(IsTrue(WrongKindAnalysis.FailureReason
			== EAngelscriptTypedASTJITFallbackReason::
				SemanticDependencyMismatch));
		ASSERT_THAT(IsTrue(WrongKindAnalysis.FailureDetail.Contains(
			TEXT("ActualKinds=Declaration"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(SelfRecursionAcceptsCompilerSignatureWithoutFunctionContent)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 77;
		const FAngelscriptArtifactReference Self =
			Reference(EAngelscriptArtifactReferenceKind::ScriptFunction,
				0x11, 0x12);
		FDependencyFixture Fixture = MakeSignatureOnlyFixture(Self);
		const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
			asETypedSemanticCallTargetKind::ScriptFunction,
			TargetFunctionId,
			asETypedSemanticBodyOwnership::CallingModule);
		FAngelscriptTypedASTJITScriptCallEmissionPlan Plan =
			MakeScriptPlan(Fixture, TargetFunctionId);
		Plan.TargetReference = Self;
		Plan.bRequiresFunctionContent = true;

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph,
				{},
				MakeArrayView(&Plan, 1));

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature)));
		ASSERT_THAT(AreEqual(0, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
		ASSERT_THAT(AreEqual(0, CountArtifactKinds(Analysis.Manifest,
			EAngelscriptArtifactSemanticDependencyKind::FunctionContent)));
		ASSERT_THAT(AreEqual(2, Analysis.Manifest.ArtifactDependencies.Num(),
			TEXT("Self-recursion must preserve the compiler Signature and extra rows")));
		ASSERT_THAT(AreEqual(2, Analysis.Manifest.RequiredReferences.Num()));
	}

	TEST_METHOD(DirectHelperSynthesizesFunctionContentWhenFlagIsFalse)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 77;
		FDependencyFixture Fixture = MakeSignatureOnlyFixture(
			Reference(EAngelscriptArtifactReferenceKind::ScriptFunction,
				0x21, 0x22));
		TArray<FAngelscriptStaticJITCompiledFunctionView> Functions;
		FAngelscriptStaticJITCompiledFunctionView CalleeView;
		CalleeView.FunctionKey.Hash = Fixture.Callee.StableKey;
		CalleeView.ExecutionHash = RepeatedByteHash(0xAB);
		Functions.Add(CalleeView);
		Fixture.Graph.Functions = Functions;

		const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
			asETypedSemanticCallTargetKind::ScriptFunction,
			TargetFunctionId,
			asETypedSemanticBodyOwnership::CallingModule);
		FAngelscriptTypedASTJITScriptCallEmissionPlan Plan =
			MakeScriptPlan(Fixture, TargetFunctionId);
		Plan.bRequiresFunctionContent = false;

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph,
				{},
				MakeArrayView(&Plan, 1));

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature)));
		ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
		ASSERT_THAT(IsTrue(HasSynthesizedContent(
			Analysis.Manifest,
			Fixture.Callee,
			TOptional<FAngelscriptHash256>(RepeatedByteHash(0xAB)))));
		ASSERT_THAT(AreEqual(2, Analysis.Manifest.RequiredReferences.Num(),
			TEXT("Synthesizing content must not drop extra compiler references")));
	}

	TEST_METHOD(ProductionLikeHelperSynthesizesMissingFunctionContent)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 77;
		FDependencyFixture Fixture = MakeSignatureOnlyFixture(
			Reference(EAngelscriptArtifactReferenceKind::ScriptFunction,
				0x21, 0x22));
		const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
			asETypedSemanticCallTargetKind::ScriptFunction,
			TargetFunctionId,
			asETypedSemanticBodyOwnership::CallingModule);
		FAngelscriptTypedASTJITScriptCallEmissionPlan Plan =
			MakeScriptPlan(Fixture, TargetFunctionId);
		Plan.bRequiresFunctionContent = true;

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph,
				{},
				MakeArrayView(&Plan, 1));

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
		ASSERT_THAT(IsTrue(HasSynthesizedContent(
			Analysis.Manifest,
			Fixture.Callee,
			TOptional<FAngelscriptHash256>())));
	}

	TEST_METHOD(SharedAndExternalBorrowedBodiesUseDirectHelperClassification)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 77;
		const asETypedSemanticBodyOwnership Ownerships[] = {
			asETypedSemanticBodyOwnership::SharedBorrowed,
			asETypedSemanticBodyOwnership::ExternalBorrowed,
		};
		for (const asETypedSemanticBodyOwnership Ownership : Ownerships)
		{
			FDependencyFixture Fixture = MakeSignatureOnlyFixture(
				Reference(EAngelscriptArtifactReferenceKind::ScriptFunction,
					0x21, 0x22));
			const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
				asETypedSemanticCallTargetKind::ScriptFunction,
				TargetFunctionId,
				Ownership);
			FAngelscriptTypedASTJITScriptCallEmissionPlan Plan =
				MakeScriptPlan(Fixture, TargetFunctionId);
			Plan.bRequiresFunctionContent = false;

			const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
				AnalyzeAngelscriptTypedASTJITDependencies(
					HIR,
					MakeShape(),
					MakeFunctionView(Fixture),
					Fixture.Graph,
					{},
					MakeArrayView(&Plan, 1));

			ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
			ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
				EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
			ASSERT_THAT(AreEqual(1, CountArtifactKinds(Analysis.Manifest,
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent)));
		}
	}

	TEST_METHOD(SCCMembersEachSynthesizeOneFunctionContentRow)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		const FAngelscriptArtifactReference First =
			Reference(EAngelscriptArtifactReferenceKind::ScriptFunction,
				0x21, 0x22);
		const FAngelscriptArtifactReference Second =
			Reference(EAngelscriptArtifactReferenceKind::ScriptFunction,
				0x41, 0x42);
		FDependencyFixture Fixture;
		Fixture.References.Reset();
		Fixture.Dependencies.Reset();
		Fixture.References.Add({Fixture.RootKey, First, TEXT("int HelperA(int)")});
		Fixture.References.Add({Fixture.RootKey, Second, TEXT("int HelperB(int)")});
		Fixture.References.Add({Fixture.RootKey, Fixture.Extra, TEXT("RuntimeExtra")});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::Signature,
			First,
			{},
			INDEX_NONE,
		});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::Signature,
			Second,
			{},
			INDEX_NONE,
		});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::CompileOption,
			Fixture.Extra,
			RepeatedByteHash(0x33),
			INDEX_NONE,
		});
		Fixture.Graph.Dependencies = Fixture.References;
		Fixture.Graph.SemanticDependencies = Fixture.Dependencies;

		const asCTypedSemanticFunction HIR = MakeTwoScriptCallHIR(11, 22);
		FAngelscriptTypedASTJITScriptCallEmissionPlan Plans[2];
		Plans[0].Expression = asTypedSemanticExpressionId(1);
		Plans[0].EngineLocalTargetFunctionId = 11;
		Plans[0].Symbol = TEXT("ASJIT_HelperA");
		Plans[0].TargetReference = First;
		Plans[0].bRequiresFunctionContent = false;
		Plans[1].Expression = asTypedSemanticExpressionId(2);
		Plans[1].EngineLocalTargetFunctionId = 22;
		Plans[1].Symbol = TEXT("ASJIT_HelperB");
		Plans[1].TargetReference = Second;
		Plans[1].bRequiresFunctionContent = false;

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph,
				{},
				MakeArrayView(Plans, 2));

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(AreEqual(2, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature)));
		ASSERT_THAT(AreEqual(2, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
		ASSERT_THAT(AreEqual(2, CountArtifactKinds(Analysis.Manifest,
			EAngelscriptArtifactSemanticDependencyKind::FunctionContent)));
		ASSERT_THAT(IsTrue(HasSynthesizedContent(
			Analysis.Manifest, First, TOptional<FAngelscriptHash256>())));
		ASSERT_THAT(IsTrue(HasSynthesizedContent(
			Analysis.Manifest, Second, TOptional<FAngelscriptHash256>())));
	}

	TEST_METHOD(PrimitiveHostHiddenArgumentDoesNotReplaceScriptCallClassification)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 77;
		FDependencyFixture Fixture = MakeSignatureOnlyFixture(
			Reference(EAngelscriptArtifactReferenceKind::ScriptFunction,
				0x21, 0x22));
		const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
			asETypedSemanticCallTargetKind::ScriptFunction,
			TargetFunctionId,
			asETypedSemanticBodyOwnership::CallingModule,
			true);
		FAngelscriptTypedASTJITScriptCallEmissionPlan Plan =
			MakeScriptPlan(Fixture, TargetFunctionId);
		Plan.bRequiresFunctionContent = false;

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph,
				{},
				MakeArrayView(&Plan, 1));

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallSignature)));
		ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
		ASSERT_THAT(AreEqual(0, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::HiddenArgumentType)));
	}

	TEST_METHOD(CurrentRouteNativeCallsDoNotRequireFunctionContent)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 88;
		const FAngelscriptArtifactReference Native =
			Reference(EAngelscriptArtifactReferenceKind::EnvironmentSymbol,
				0x61, 0x62);
		FDependencyFixture Fixture;
		Fixture.References.Reset();
		Fixture.Dependencies.Reset();
		Fixture.References.Add({
			Fixture.RootKey,
			Native,
			TEXT("float FMath::Sqrt(float)"),
		});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi,
			Native,
			{},
			INDEX_NONE,
		});
		Fixture.Graph.Dependencies = Fixture.References;
		Fixture.Graph.SemanticDependencies = Fixture.Dependencies;

		const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
			asETypedSemanticCallTargetKind::SystemFunction,
			TargetFunctionId,
			asETypedSemanticBodyOwnership::NativeRuntime);
		const EAngelscriptTypedASTJITCallEmissionRoute Routes[] = {
			EAngelscriptTypedASTJITCallEmissionRoute::CurrentNativeBinding,
			EAngelscriptTypedASTJITCallEmissionRoute::Bridge,
		};
		for (const EAngelscriptTypedASTJITCallEmissionRoute Route : Routes)
		{
			FAngelscriptTypedASTJITNativeCallEmissionPlan Plan;
			Plan.Expression = asTypedSemanticExpressionId(1);
			Plan.EngineLocalTargetFunctionId = TargetFunctionId;
			Plan.Route = Route;
			Plan.TargetReference = Native;
			Plan.RegisteredTarget = TEXT("FMath::Sqrt");

			const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
				AnalyzeAngelscriptTypedASTJITDependencies(
					HIR,
					MakeShape(),
					MakeFunctionView(Fixture),
					Fixture.Graph,
					MakeArrayView(&Plan, 1));

			ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
			ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
				EAngelscriptTypedASTJITSemanticUseKind::NativeCall)));
			ASSERT_THAT(AreEqual(0, CountUses(Analysis.Manifest,
				EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
			ASSERT_THAT(AreEqual(0, CountArtifactKinds(Analysis.Manifest,
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent)));
		}
	}

	TEST_METHOD(DirectNativeCallRecordsIncludeAndEnvironmentAbi)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 88;
		const FAngelscriptArtifactReference Native =
			Reference(EAngelscriptArtifactReferenceKind::EnvironmentSymbol,
				0x61, 0x62);
		FDependencyFixture Fixture;
		Fixture.References.Reset();
		Fixture.Dependencies.Reset();
		Fixture.References.Add({
			Fixture.RootKey,
			Native,
			TEXT("float FMath::Sqrt(float)"),
		});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::EnvironmentAbi,
			Native,
			{},
			INDEX_NONE,
		});
		Fixture.Graph.Dependencies = Fixture.References;
		Fixture.Graph.SemanticDependencies = Fixture.Dependencies;

		const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
			asETypedSemanticCallTargetKind::SystemFunction,
			TargetFunctionId,
			asETypedSemanticBodyOwnership::NativeRuntime);
		FAngelscriptTypedASTJITNativeCallEmissionPlan Plan;
		Plan.Expression = asTypedSemanticExpressionId(1);
		Plan.EngineLocalTargetFunctionId = TargetFunctionId;
		Plan.Route = EAngelscriptTypedASTJITCallEmissionRoute::Direct;
		Plan.TargetReference = Native;
		Plan.Symbol = TEXT("FMath::Sqrt");
		Plan.Include = TEXT("Math/UnrealMathUtility.h");

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph,
				MakeArrayView(&Plan, 1));

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(AreEqual(1, Analysis.Manifest.RequiredIncludes.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Math/UnrealMathUtility.h")),
			Analysis.Manifest.RequiredIncludes[0]));
		ASSERT_THAT(AreEqual(0, CountArtifactKinds(Analysis.Manifest,
			EAngelscriptArtifactSemanticDependencyKind::FunctionContent)));
	}

	TEST_METHOD(ImportedBindingSlotUsesImportKindNotFunctionContent)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		constexpr int32 TargetFunctionId = 99;
		const FAngelscriptArtifactReference Import =
			Reference(EAngelscriptArtifactReferenceKind::ScriptImport,
				0x71, 0x72);
		FDependencyFixture Fixture;
		Fixture.References.Reset();
		Fixture.Dependencies.Reset();
		Fixture.References.Add({
			Fixture.RootKey,
			Import,
			TEXT("int ImportedHelper(int)"),
		});
		Fixture.Dependencies.Add({
			Fixture.RootKey,
			EAngelscriptArtifactSemanticDependencyKind::Import,
			Import,
			{},
			INDEX_NONE,
		});
		Fixture.Graph.Dependencies = Fixture.References;
		Fixture.Graph.SemanticDependencies = Fixture.Dependencies;

		const asCTypedSemanticFunction HIR = MakeResolvedCallHIR(
			asETypedSemanticCallTargetKind::ImportedFunction,
			TargetFunctionId,
			asETypedSemanticBodyOwnership::None);

		const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
			AnalyzeAngelscriptTypedASTJITDependencies(
				HIR,
				MakeShape(),
				MakeFunctionView(Fixture),
				Fixture.Graph);

		ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
		ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ImportedBindingSlot)));
		ASSERT_THAT(AreEqual(0, CountUses(Analysis.Manifest,
			EAngelscriptTypedASTJITSemanticUseKind::ScriptCallContent)));
		ASSERT_THAT(AreEqual(1, CountArtifactKinds(Analysis.Manifest,
			EAngelscriptArtifactSemanticDependencyKind::Import)));
	}

	TEST_METHOD(FoldedHardValueAndMutableStoragePreserveExactKinds)
	{
		using namespace AngelscriptTypedASTJITSemanticDependencyTests_Private;
		const FAngelscriptArtifactReference Folded =
			Reference(EAngelscriptArtifactReferenceKind::ScriptGlobal,
				0x81, 0x82);
		const FAngelscriptArtifactReference Mutable =
			Reference(EAngelscriptArtifactReferenceKind::ScriptGlobal,
				0x83, 0x84);

		{
			FDependencyFixture Fixture;
			Fixture.References.Reset();
			Fixture.Dependencies.Reset();
			Fixture.References.Add({Fixture.RootKey, Folded, TEXT("const int G")});
			Fixture.Dependencies.Add({
				Fixture.RootKey,
				EAngelscriptArtifactSemanticDependencyKind::HardValue,
				Folded,
				RepeatedByteHash(0x85),
				7,
			});
			Fixture.Graph.Dependencies = Fixture.References;
			Fixture.Graph.SemanticDependencies = Fixture.Dependencies;
			FAngelscriptTypedASTJITGlobalDependency Global;
			Global.EngineLocalGlobalPropertyId = 7;
			Global.Kind = EAngelscriptArtifactSemanticDependencyKind::HardValue;
			Global.Reference = Folded;
			Global.ExpectedContentOrValue = RepeatedByteHash(0x85);
			const asCTypedSemanticFunction HIR = MakeGlobalHIR(
				asETypedSemanticExpressionKind::FoldedGlobalConstant, 7);
			const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
				AnalyzeAngelscriptTypedASTJITDependencies(
					HIR,
					MakeShape(),
					MakeFunctionView(Fixture),
					Fixture.Graph,
					{},
					{},
					MakeArrayView(&Global, 1));
			ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
			ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
				EAngelscriptTypedASTJITSemanticUseKind::FoldedGlobalHardValue)));
			ASSERT_THAT(AreEqual(1, CountArtifactKinds(Analysis.Manifest,
				EAngelscriptArtifactSemanticDependencyKind::HardValue)));
		}

		{
			FDependencyFixture Fixture;
			Fixture.References.Reset();
			Fixture.Dependencies.Reset();
			Fixture.References.Add({Fixture.RootKey, Mutable, TEXT("int G")});
			Fixture.Dependencies.Add({
				Fixture.RootKey,
				EAngelscriptArtifactSemanticDependencyKind::GlobalStorage,
				Mutable,
				{},
				8,
			});
			Fixture.Graph.Dependencies = Fixture.References;
			Fixture.Graph.SemanticDependencies = Fixture.Dependencies;
			FAngelscriptTypedASTJITGlobalDependency Global;
			Global.EngineLocalGlobalPropertyId = 8;
			Global.Kind =
				EAngelscriptArtifactSemanticDependencyKind::GlobalStorage;
			Global.Reference = Mutable;
			const asCTypedSemanticFunction HIR = MakeGlobalHIR(
				asETypedSemanticExpressionKind::GlobalStorage, 8);
			const FAngelscriptTypedASTJITDependencyAnalysis Analysis =
				AnalyzeAngelscriptTypedASTJITDependencies(
					HIR,
					MakeShape(),
					MakeFunctionView(Fixture),
					Fixture.Graph,
					{},
					{},
					MakeArrayView(&Global, 1));
			ASSERT_THAT(IsTrue(Analysis.bValid, Analysis.FailureDetail));
			ASSERT_THAT(AreEqual(1, CountUses(Analysis.Manifest,
				EAngelscriptTypedASTJITSemanticUseKind::MutableGlobalStorage)));
			ASSERT_THAT(AreEqual(1, CountArtifactKinds(Analysis.Manifest,
				EAngelscriptArtifactSemanticDependencyKind::GlobalStorage)));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
