#include "CQTest.h"

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallClosure.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallDescriptor.h"

#include "Algo/Reverse.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITCallClosureTests,
	"Angelscript.TestModule.StaticJIT.Eligibility.TypedASTJIT.CallClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
		FAngelscriptStableFunctionKey Key;
		Key.Hash = RepeatedByteHash(Value);
		return Key;
	}

	static asSTypedSemanticSourceSpan MakeSpan(
		const asUINT Row,
		const asUINT Column)
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = "/Angelscript/Game/CallClosure.as";
		Span.sourceOffset = Row * 10 + Column;
		Span.sourceLength = 4;
		Span.row = Row;
		Span.column = Column;
		return Span;
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
		for (asUINT StatementIndex = 0;
			StatementIndex < Function.statements.GetLength();
			++StatementIndex)
		{
			asSTypedSemanticStatement& Statement =
				Function.statements[StatementIndex];
			if (Statement.kind == asETypedSemanticStatementKind::Return)
			{
				Statement.cleanupPlan = Plan.id;
			}
		}
	}

	static asCTypedSemanticFunction MakeLeafHIR(
		const asUINT Row = 5,
		const asUINT Column = 3)
	{
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		asCTypedSemanticFunction Function;
		Function.declaration = "int Leaf(int Value)";

		asSTypedSemanticSymbol Parameter;
		Parameter.kind = asETypedSemanticSymbolKind::Parameter;
		Parameter.name = "Value";
		Parameter.type = IntType;
		Parameter.span = MakeSpan(Row, Column);
		const asTypedSemanticSymbolId ParameterId =
			Function.AddSymbol(Parameter);

		asSTypedSemanticExpression Read;
		Read.kind = asETypedSemanticExpressionKind::Symbol;
		Read.type = IntType;
		Read.span = MakeSpan(Row, Column);
		Read.symbol = ParameterId;
		const asTypedSemanticExpressionId ReadId =
			Function.AddExpression(Read);

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = MakeSpan(Row, Column);
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = ReadId;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId =
			Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan(Row, Column);
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeCallHIR(
		const int32 TargetFunctionId,
		const asUINT Row,
		const asUINT Column,
		const asETypedSemanticCallTargetKind TargetKind =
			asETypedSemanticCallTargetKind::ScriptFunction)
	{
		asCTypedSemanticFunction Function = MakeLeafHIR(Row, Column);
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);

		asSTypedSemanticExpression Call;
		Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
		Call.type = IntType;
		Call.span = MakeSpan(Row, Column);
		Call.callTargetKind = TargetKind;
		Call.resolvedFunctionId = TargetFunctionId;
		Call.targetProvenance.canonicalDeclaration =
			"int SyntheticTarget(int)";
		if (TargetKind == asETypedSemanticCallTargetKind::SystemFunction)
		{
			Call.targetProvenance.bodyAvailability =
				asETypedSemanticBodyAvailability::RegisteredNative;
			Call.targetProvenance.bodyOwnership =
				asETypedSemanticBodyOwnership::NativeRuntime;
		}
		else if (TargetKind
			== asETypedSemanticCallTargetKind::ImportedFunction)
		{
			Call.targetProvenance.declarationModule =
				"SyntheticCallClosure";
			Call.targetProvenance.bodyAvailability =
				asETypedSemanticBodyAvailability::NoScriptBody;
			Call.targetProvenance.bodyOwnership =
				asETypedSemanticBodyOwnership::None;
			Call.targetProvenance.importedSourceModule =
				"SyntheticProvider";
			Call.targetProvenance.importedCanonicalSignature =
				Call.targetProvenance.canonicalDeclaration;
		}
		else
		{
			Call.targetProvenance.declarationModule =
				"SyntheticCallClosure";
			Call.targetProvenance.bodyAvailability =
				asETypedSemanticBodyAvailability::ScriptBody;
			Call.targetProvenance.bodyOwnership =
				asETypedSemanticBodyOwnership::CallingModule;
			Call.targetProvenance.bodyOwnerModule =
				"SyntheticCallClosure";
		}
		Call.operands.PushLast(asTypedSemanticExpressionId(0));
		Call.formalParameterIndices.PushLast(0);
		Call.sourceArgumentOrdinals.PushLast(0);
		Call.evaluationSequence.PushLast(0);
		Call.argumentOrigins.PushLast(
			asETypedSemanticArgumentOrigin::SourcePositional);
		asSTypedSemanticArgumentProvenance Provenance;
		Provenance.origin =
			asETypedSemanticArgumentOrigin::SourcePositional;
		Provenance.sourceArgumentOrdinal = 0;
		Provenance.processedSpan = MakeSpan(Row, Column);
		Call.argumentProvenance.PushLast(Provenance);
		asSTypedSemanticEvaluationStep EvaluationStep;
		EvaluationStep.role = asETypedSemanticEvaluationRole::Argument;
		EvaluationStep.expression = asTypedSemanticExpressionId(0);
		EvaluationStep.formalParameterIndex = 0;
		EvaluationStep.sourceArgumentOrdinal = 0;
		Call.evaluationSteps.PushLast(EvaluationStep);
		if (TargetKind == asETypedSemanticCallTargetKind::SystemFunction)
		{
			Call.nativeCall.isSystemFunction = true;
			Call.nativeCall.callConvention = 2;
		}
		const asTypedSemanticExpressionId CallId =
			Function.AddExpression(Call);
		Function.statements[0].valueExpression = CallId;
		return Function;
	}

	static FAngelscriptTypedASTJITCallClosureFunctionInput MakeFunction(
		const int32 FunctionId,
		const uint8 StableByte,
		const asCTypedSemanticFunction& HIR,
		const bool bRoot,
		const EAngelscriptTypedASTJITDirectCapability Capabilities =
			EAngelscriptTypedASTJITDirectCapability::FramePosition)
	{
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		FAngelscriptTypedASTJITCallClosureFunctionInput Input;
		Input.EngineLocalFunctionId = FunctionId;
		Input.FunctionKey = FunctionKey(StableByte);
		Input.bIsUFunctionRoot = bRoot;
		FString CapabilityError;
		check(BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
			Capabilities, Input.CapabilityProfile, &CapabilityError));
		check(CapabilityError.IsEmpty());
		Input.Eligibility.bIsUFunctionRoot = bRoot;
		Input.Eligibility.TypedHIR = &HIR;
		Input.Eligibility.Shape.SymbolName =
			FString::Printf(TEXT("AS_Typed_Closure_%02x"), StableByte);
		Input.Eligibility.Shape.ParameterTypes.Add(IntType);
		Input.Eligibility.Shape.ReturnType = IntType;
		Input.Eligibility.Shape.InvocationKind =
			HIR.header.invocationKind;
		Input.Eligibility.Shape.ReceiverKind =
			HIR.header.receiver.kind;
		Input.Eligibility.Shape.ReceiverParameterIndex =
			HIR.header.receiver.parameterIndex;
		Input.Eligibility.RawTraitBits = HIR.header.declaredTraitBits;
		return Input;
	}

	static FAngelscriptTypedASTJITCallClosureOptions MakeOptions(
		const EAngelscriptTypedASTJITDirectCapability Capabilities =
			EAngelscriptTypedASTJITDirectCapability::None)
	{
		FAngelscriptTypedASTJITCallClosureOptions Options;
		FString RequirementsError;
		check(BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
			Capabilities, Options.ExecutionRequirements,
			&RequirementsError));
		check(RequirementsError.IsEmpty());
		return Options;
	}

	static FAngelscriptStaticJITNativeCallDescriptor MakeDirectDescriptor(
		const EAngelscriptStaticJITNativeCallLinkage Linkage,
		const FAngelscriptStaticJITScalarABIIdentity& ABI)
	{
		FAngelscriptStaticJITNativeCallDescriptor Descriptor;
		Descriptor.Linkage = Linkage;
		Descriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::External;
		Descriptor.Symbol = TEXT("FPlatformMath::CountBits");
		Descriptor.Include = TEXT("HAL/PlatformMath.h");
		Descriptor.OwningModule = FName(TEXT("Core"));
		Descriptor.ApiMacro = TEXT("CORE_API");
		Descriptor.NativeABI =
			FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
				TEXT("callconv=0;return=int;params=(int:inout=0)"));
		Descriptor.CppCallableSignature = TEXT("int32(int32)");
		Descriptor.ABIDomain =
			EAngelscriptStaticJITNativeCallABIDomain::Scalar;
		Descriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::DoesNotSetScriptException;
		Descriptor.ScalarABI = ABI;
		Descriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::DirectCall;
		Descriptor.LifetimeFlags =
			EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture;
		if (Linkage
			== EAngelscriptStaticJITNativeCallLinkage::HeaderInline)
		{
			Descriptor.Visibility =
				EAngelscriptStaticJITNativeCallVisibility::HeaderInline;
			Descriptor.ApiMacro.Reset();
		}
		else if (Linkage
				== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk
			|| Linkage
				== EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable)
		{
			Descriptor.Symbol = TEXT("AngelscriptStaticJIT_CountBitsThunk");
			Descriptor.Include =
				TEXT("StaticJIT/AngelscriptStaticJITNativeCallables.h");
			Descriptor.OwningModule = FName(TEXT("AngelscriptRuntime"));
			Descriptor.ApiMacro = TEXT("ANGELSCRIPTRUNTIME_API");
		}
		return Descriptor;
	}

	static const FAngelscriptTypedASTJITCallClosureComponent* FindRecursive(
		const FAngelscriptTypedASTJITCallClosurePlan& Plan)
	{
		return Plan.Components.FindByPredicate(
			[](const FAngelscriptTypedASTJITCallClosureComponent& Component)
			{
				return Component.bRecursive;
			});
	}

public:
	TEST_METHOD(ReachableSafeHelpersArePlannedTransitivelyAndDeterministically)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(202, 11, 7);
		asCTypedSemanticFunction HelperAHIR = MakeCallHIR(303, 21, 9);
		asCTypedSemanticFunction HelperBHIR = MakeLeafHIR(31, 4);
		const EAngelscriptTypedASTJITDirectCapability Capabilities =
			EAngelscriptTypedASTJITDirectCapability::FramePosition
			| EAngelscriptTypedASTJITDirectCapability::Coverage;

		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x30, RootHIR, true, Capabilities),
			MakeFunction(202, 0x10, HelperAHIR, false, Capabilities),
			MakeFunction(303, 0x20, HelperBHIR, false, Capabilities),
		};
		FAngelscriptTypedASTJITCallClosureOptions Options =
			MakeOptions(Capabilities);

		FAngelscriptTypedASTJITCallClosurePlan First;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x30), Options, First, &Error)));
		ASSERT_THAT(AreEqual(FString(), Error));
		ASSERT_THAT(IsTrue(First.bEligible));
		ASSERT_THAT(AreEqual(3, First.Functions.Num()));
		ASSERT_THAT(AreEqual(2, First.Calls.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITClosureFunctionDisposition::Root,
			First.FindFunction(FunctionKey(0x30))->Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITClosureFunctionDisposition::InternalSemanticHelper,
			First.FindFunction(FunctionKey(0x10))->Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITClosureFunctionDisposition::InternalSemanticHelper,
			First.FindFunction(FunctionKey(0x20))->Disposition));
		ASSERT_THAT(AreEqual(Capabilities,
			First.AvailableDirectCapabilities));

		Algo::Reverse(Functions);
		FAngelscriptTypedASTJITCallClosurePlan Reordered;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x30), Options, Reordered, &Error)));
		ASSERT_THAT(AreEqual(First.DumpNormalized(),
			Reordered.DumpNormalized()));
		ASSERT_THAT(IsFalse(First.DumpNormalized().Contains(TEXT("0x"))));
		ASSERT_THAT(IsFalse(First.DumpNormalized().Contains(TEXT("functionId"))));
	}

	TEST_METHOD(GeneratedAndReceiverHelpersBridgeOrFailAtTheExactCallSpan)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(202, 42, 13);
		asCTypedSemanticFunction LifecycleHIR = MakeLeafHIR(50, 2);
		LifecycleHIR.header.declaredTraitBits =
			asTRAIT_GENERATED_FUNCTION | asTRAIT_EXTERNAL_IMPLICIT_THIS;
		LifecycleHIR.header.traitSummary = ClassifyTypedSemanticFunctionTraits(
			LifecycleHIR.header.declaredTraitBits);
		LifecycleHIR.header.receiver.kind =
			asETypedSemanticReceiverKind::ExternalImplicitThis;
		LifecycleHIR.header.receiver.symbol = LifecycleHIR.symbols[0].id;
		LifecycleHIR.header.receiver.parameterIndex = 0;
		LifecycleHIR.header.receiver.type = LifecycleHIR.symbols[0].type;

		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x31, RootHIR, true),
			MakeFunction(202, 0x11, LifecycleHIR, false),
		};
		Functions[1].Eligibility.Shape.ReceiverKind =
			asETypedSemanticReceiverKind::ExternalImplicitThis;
		Functions[1].Eligibility.Shape.ReceiverParameterIndex = 0;
		Functions[1].bScalarBridgeAvailable = true;

		FAngelscriptTypedASTJITCallClosurePlan Bridged;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x31), MakeOptions(), Bridged, &Error)));
		ASSERT_THAT(IsTrue(Bridged.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Bridge,
			Bridged.Calls[0].Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITClosureFunctionDisposition::Bridge,
			Bridged.FindFunction(FunctionKey(0x11))->Disposition));

		Functions[1].bScalarBridgeAvailable = false;
		FAngelscriptTypedASTJITCallClosurePlan Rejected;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x31), MakeOptions(), Rejected, &Error)));
		ASSERT_THAT(IsFalse(Rejected.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			Rejected.RootEligibility.Reason));
		ASSERT_THAT(AreEqual(42,
			static_cast<int32>(Rejected.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(13,
			static_cast<int32>(Rejected.RootEligibility.SourceSpan.column)));
		ASSERT_THAT(IsTrue(Rejected.RootEligibility.Detail.Contains(
			TEXT("InvalidEffectiveReceiver")),
			Rejected.RootEligibility.Detail));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Unsupported,
			Rejected.Calls[0].Disposition));

		LifecycleHIR.header.declaredTraitBits = asTRAIT_MIXIN;
		LifecycleHIR.header.traitSummary =
			ClassifyTypedSemanticFunctionTraits(asTRAIT_MIXIN);
		LifecycleHIR.header.receiver.kind =
			asETypedSemanticReceiverKind::MixinFirstParameter;
		Functions[1].Eligibility.RawTraitBits = asTRAIT_MIXIN;
		Functions[1].Eligibility.Shape.ReceiverKind =
			asETypedSemanticReceiverKind::MixinFirstParameter;
		FAngelscriptTypedASTJITCallClosurePlan MixinRejected;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x31), MakeOptions(), MixinRejected,
			&Error)));
		ASSERT_THAT(IsFalse(MixinRejected.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			MixinRejected.RootEligibility.Reason));
		ASSERT_THAT(IsTrue(MixinRejected.RootEligibility.Detail.Contains(
			TEXT("InvalidEffectiveReceiver")),
			MixinRejected.RootEligibility.Detail));
	}

	TEST_METHOD(RecursiveStrongComponentsRequireARecursionCapability)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(202, 60, 4);
		asCTypedSemanticFunction HelperAHIR = MakeCallHIR(303, 61, 6);
		asCTypedSemanticFunction HelperBHIR = MakeCallHIR(202, 62, 8);
		const EAngelscriptTypedASTJITDirectCapability FrameOnly =
			EAngelscriptTypedASTJITDirectCapability::FramePosition;
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x32, RootHIR, true, FrameOnly),
			MakeFunction(202, 0x12, HelperAHIR, false, FrameOnly),
			MakeFunction(303, 0x22, HelperBHIR, false, FrameOnly),
		};

		FAngelscriptTypedASTJITCallClosureOptions Options =
			MakeOptions(FrameOnly);
		FAngelscriptTypedASTJITCallClosurePlan Rejected;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x32), Options, Rejected, &Error)));
		ASSERT_THAT(IsFalse(Rejected.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl,
			Rejected.RootEligibility.Reason));
		ASSERT_THAT(IsTrue(Rejected.RootEligibility.Detail.Contains(
			TEXT("RecursionGuardUnavailable"))));

		const EAngelscriptTypedASTJITDirectCapability WithRecursion =
			FrameOnly
			| EAngelscriptTypedASTJITDirectCapability::RecursionBudget;
		for (FAngelscriptTypedASTJITCallClosureFunctionInput& Function : Functions)
		{
			FString CapabilityError;
			ASSERT_THAT(IsTrue(
				BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
					WithRecursion, Function.CapabilityProfile,
					&CapabilityError), CapabilityError));
		}
		FAngelscriptTypedASTJITCallClosurePlan Accepted;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x32), Options, Accepted, &Error)));
		ASSERT_THAT(IsTrue(Accepted.bEligible));
		const FAngelscriptTypedASTJITCallClosureComponent* Recursive =
			FindRecursive(Accepted);
		ASSERT_THAT(IsNotNull(Recursive));
		ASSERT_THAT(AreEqual(2, Recursive->Members.Num()));
		ASSERT_THAT(IsTrue(
			Recursive->Members[0] == FunctionKey(0x12)));
		ASSERT_THAT(IsTrue(
			Recursive->Members[1] == FunctionKey(0x22)));
		ASSERT_THAT(IsTrue(
			Accepted.FindFunction(FunctionKey(0x12))->bRecursiveComponent));
		ASSERT_THAT(IsTrue(
			Accepted.FindFunction(FunctionKey(0x22))->bRecursiveComponent));
	}

	TEST_METHOD(DirectCapabilityMismatchAndMissingTargetsFailClosed)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(202, 70, 17);
		asCTypedSemanticFunction HelperHIR = MakeLeafHIR(71, 2);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x33, RootHIR, true,
				EAngelscriptTypedASTJITDirectCapability::Coverage),
			MakeFunction(202, 0x13, HelperHIR, false,
				EAngelscriptTypedASTJITDirectCapability::FramePosition),
		};
		FAngelscriptTypedASTJITCallClosureOptions Options = MakeOptions(
			EAngelscriptTypedASTJITDirectCapability::Coverage);

		FAngelscriptTypedASTJITCallClosurePlan Mismatch;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x33), Options, Mismatch, &Error)));
		ASSERT_THAT(IsFalse(Mismatch.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability,
			Mismatch.RootEligibility.Reason));
		ASSERT_THAT(IsTrue(Mismatch.RootEligibility.Detail.Contains(
			TEXT("DirectCalleeProfileMismatch"))));
		ASSERT_THAT(IsTrue(Mismatch.RootEligibility.Detail.Contains(
			TEXT("Required=Coverage"))));
		ASSERT_THAT(IsTrue(Mismatch.RootEligibility.Detail.Contains(
			TEXT("Available=FramePosition"))));
		ASSERT_THAT(AreEqual(70,
			static_cast<int32>(Mismatch.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Unsupported,
			Mismatch.Calls[0].Disposition));

		Functions.Pop();
		FAngelscriptTypedASTJITCallClosurePlan Missing;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x33), Options, Missing, &Error)));
		ASSERT_THAT(IsFalse(Missing.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			Missing.RootEligibility.Reason));
		ASSERT_THAT(IsTrue(Missing.RootEligibility.Detail.Contains(
			TEXT("ResolvedTargetMissing"))));
		ASSERT_THAT(AreEqual(70,
			static_cast<int32>(Missing.RootEligibility.SourceSpan.row)));
	}

	TEST_METHOD(SystemTargetsAwaitExplicitExternalDescriptorsWithoutScriptGraphLookup)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(
			404,
			91,
			12,
			asETypedSemanticCallTargetKind::SystemFunction);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x39, RootHIR, true),
		};

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x39), MakeOptions(), Plan, &Error), Error));
		ASSERT_THAT(IsFalse(Plan.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			Plan.RootEligibility.Reason));
		ASSERT_THAT(AreEqual(91,
			static_cast<int32>(Plan.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(12,
			static_cast<int32>(Plan.RootEligibility.SourceSpan.column)));
		ASSERT_THAT(AreEqual(1, Plan.Calls.Num()));
		ASSERT_THAT(AreEqual(
			asETypedSemanticCallTargetKind::SystemFunction,
			Plan.Calls[0].TargetKind));
		ASSERT_THAT(AreEqual(404,
			Plan.Calls[0].EngineLocalTargetFunctionId));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Unsupported,
			Plan.Calls[0].Disposition));
		ASSERT_THAT(IsTrue(Plan.RootEligibility.Detail.Contains(
			TEXT("ExternalCallDescriptorMissing"))));
		ASSERT_THAT(IsTrue(Plan.RootEligibility.Detail.Contains(
			TEXT("SystemFunctionId=404"))));
		ASSERT_THAT(IsFalse(Plan.RootEligibility.Detail.Contains(
			TEXT("ResolvedTargetMissing"))));
		const FString Dump = Plan.DumpNormalized();
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT(
			"target=SystemFunction targetId=404"))));
	}

	TEST_METHOD(ImportedTargetsRetainMutableSlotAndFailWithTypedRouteReason)
	{
		constexpr int32 ImportedSlotFunctionId = 0x40000194;
		asCTypedSemanticFunction RootHIR = MakeCallHIR(
			ImportedSlotFunctionId,
			94,
			17,
			asETypedSemanticCallTargetKind::ImportedFunction);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x3a, RootHIR, true),
		};

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x3a), MakeOptions(), Plan, &Error), Error));
		ASSERT_THAT(IsFalse(Plan.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedImportedRoute,
			Plan.RootEligibility.Reason));
		ASSERT_THAT(AreEqual(94,
			static_cast<int32>(Plan.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(17,
			static_cast<int32>(Plan.RootEligibility.SourceSpan.column)));
		ASSERT_THAT(AreEqual(1, Plan.Calls.Num()));
		ASSERT_THAT(AreEqual(
			asETypedSemanticCallTargetKind::ImportedFunction,
			Plan.Calls[0].TargetKind));
		ASSERT_THAT(AreEqual(
			ImportedSlotFunctionId,
			Plan.Calls[0].EngineLocalTargetFunctionId));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Unsupported,
			Plan.Calls[0].Disposition));
		ASSERT_THAT(IsTrue(Plan.RootEligibility.Detail.Contains(
			TEXT("ImportedBindingRouteUnavailable"))));
		ASSERT_THAT(IsTrue(Plan.RootEligibility.Detail.Contains(
			TEXT("SlotFunctionId=1073742228"))));
		ASSERT_THAT(IsFalse(Plan.RootEligibility.Detail.Contains(
			TEXT("ResolvedTargetMissing"))));
		const FString Dump = Plan.DumpNormalized();
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT(
			"target=ImportedFunction targetId=1073742228"))));
	}

	TEST_METHOD(DescriptorAbsentScalarTargetUsesSeparateVMBridgeABI)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(
			404,
			92,
			13,
			asETypedSemanticCallTargetKind::SystemFunction);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x47, RootHIR, true),
		};
		FAngelscriptStaticJITNativeCallTarget Target;
		Target.EngineLocalFunctionId = 404;
		Target.CanonicalDeclaration = TEXT("float32 Sqrt(float32 Value)");
		Target.RegisteredTargetDisplay = TEXT("FMath::Sqrt");
		Target.ExpectedNativeABI =
			FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
				TEXT("callconv=0;return=float32;params=(float32:inout=0)"));
		Target.ExpectedVMBridgeABI =
			FAngelscriptTypedASTJITVMBridgeABIIdentity::FromCanonical(
				TEXT("return=f32;params=(f32:inout=0);receiver=none;returnOnStack=0;firstMetadata=0;hidden=-1;determinesOutput=-1;worldContext=0;compileOut=0"));
		Target.bScalarBridgeCallable = true;
		TArray<FAngelscriptStaticJITNativeCallTarget> Targets = {Target};
		FAngelscriptTypedASTJITCallClosureOptions Options = MakeOptions();
		Options.NativeCallTargets = Targets;
		Options.bNativeScalarBridgeAvailable = true;

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x47), Options, Plan, &Error), Error));
		ASSERT_THAT(IsTrue(Plan.bEligible));
		ASSERT_THAT(AreEqual(1, Plan.Calls.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Bridge,
			Plan.Calls[0].Disposition));
		ASSERT_THAT(IsTrue(Plan.Calls[0].ExternalCallDescriptor.IsAbsent()));
		ASSERT_THAT(IsTrue(
			Plan.Calls[0].ExternalCallValidation.bScalarBridgeCallable));
		ASSERT_THAT(IsTrue(Plan.Calls[0].ExternalCallValidation.Detail.Contains(
			TEXT("AutomaticVMScalarBridge"))));
	}

	TEST_METHOD(DescriptorAbsentScalarTargetRejectsMissingVMBridgeABI)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(
			404,
			92,
			14,
			asETypedSemanticCallTargetKind::SystemFunction);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x48, RootHIR, true),
		};
		FAngelscriptStaticJITNativeCallTarget Target;
		Target.EngineLocalFunctionId = 404;
		Target.CanonicalDeclaration = TEXT("float32 Sqrt(float32 Value)");
		Target.RegisteredTargetDisplay = TEXT("FMath::Sqrt");
		Target.ExpectedNativeABI =
			FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
				TEXT("callconv=0;return=float32;params=(float32:inout=0)"));
		Target.bScalarBridgeCallable = true;
		TArray<FAngelscriptStaticJITNativeCallTarget> Targets = {Target};
		FAngelscriptTypedASTJITCallClosureOptions Options = MakeOptions();
		Options.NativeCallTargets = Targets;
		Options.bNativeScalarBridgeAvailable = true;

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsFalse(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x48), Options, Plan, &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT(
			"TypedASTJITCallGraphInvalidNativeCallTarget"))));
	}

	TEST_METHOD(DescriptorAbsentScalarTargetRejectsUnavailableBridgeRuntime)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(
			404,
			92,
			15,
			asETypedSemanticCallTargetKind::SystemFunction);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x4a, RootHIR, true),
		};
		FAngelscriptStaticJITNativeCallTarget Target;
		Target.EngineLocalFunctionId = 404;
		Target.CanonicalDeclaration = TEXT("float32 Sqrt(float32 Value)");
		Target.RegisteredTargetDisplay = TEXT("FMath::Sqrt");
		Target.ExpectedNativeABI =
			FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
				TEXT("callconv=0;return=float32;params=(float32:inout=0)"));
		Target.ExpectedVMBridgeABI =
			FAngelscriptTypedASTJITVMBridgeABIIdentity::FromCanonical(
				TEXT("return=f32;params=(f32:inout=0);receiver=none;returnOnStack=0;firstMetadata=0;hidden=-1;determinesOutput=-1;worldContext=0;compileOut=0"));
		Target.bScalarBridgeCallable = true;
		TArray<FAngelscriptStaticJITNativeCallTarget> Targets = {Target};
		FAngelscriptTypedASTJITCallClosureOptions Options = MakeOptions();
		Options.NativeCallTargets = Targets;
		Options.bNativeScalarBridgeAvailable = false;

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x4a), Options, Plan, &Error), Error));
		ASSERT_THAT(IsFalse(Plan.bEligible));
		ASSERT_THAT(AreEqual(1, Plan.Calls.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Unsupported,
			Plan.Calls[0].Disposition));
		ASSERT_THAT(IsTrue(Plan.RootEligibility.Detail.Contains(TEXT(
			"ExternalCallRouteUnavailable"))));
	}

	TEST_METHOD(ValidatedExternalDescriptorsSelectExactDirectDisposition)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(
			404,
			92,
			14,
			asETypedSemanticCallTargetKind::SystemFunction);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x49, RootHIR, true),
		};
		const FAngelscriptStaticJITScalarABIIdentity ABI =
			FAngelscriptStaticJITScalarABIIdentity::FromCanonical(
				TEXT("cdecl:i32(i32)"));
		FAngelscriptStaticJITNativeCallTarget Target;
		Target.EngineLocalFunctionId = 404;
		Target.CanonicalDeclaration = TEXT("int CountBits(int Value)");
		Target.ExpectedNativeABI =
			FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
				TEXT("callconv=0;return=int;params=(int:inout=0)"));
		Target.ExpectedScalarABI = ABI;
		TArray<FAngelscriptStaticJITNativeCallTarget> Targets = {Target};

		struct FCase
		{
			EAngelscriptStaticJITNativeCallLinkage Linkage;
			EAngelscriptTypedASTJITCallDisposition Expected;
		};
		const FCase Cases[] = {
			{EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol,
				EAngelscriptTypedASTJITCallDisposition::DirectExported},
			{EAngelscriptStaticJITNativeCallLinkage::HeaderInline,
				EAngelscriptTypedASTJITCallDisposition::DirectInline},
			{EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeThunk,
				EAngelscriptTypedASTJITCallDisposition::RuntimeThunk},
			{EAngelscriptStaticJITNativeCallLinkage::ExportedRuntimeCallable,
				EAngelscriptTypedASTJITCallDisposition::DirectExported},
		};

		for (const FCase& Case : Cases)
		{
			Targets[0].Descriptor = MakeDirectDescriptor(Case.Linkage, ABI);
			FAngelscriptTypedASTJITCallClosureOptions Options = MakeOptions();
			Options.NativeCallTargets = Targets;
			Options.KnownNativeCallModules = {
				FName(TEXT("Core")), FName(TEXT("AngelscriptRuntime"))};
			Options.PublicNativeCallDependencyModules =
				Options.KnownNativeCallModules;

			FAngelscriptTypedASTJITCallClosurePlan Plan;
			FString Error;
			ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
				Functions, FunctionKey(0x49), Options, Plan, &Error), Error));
			ASSERT_THAT(IsTrue(Plan.bEligible));
			ASSERT_THAT(AreEqual(1, Plan.Calls.Num()));
			ASSERT_THAT(AreEqual(Case.Expected, Plan.Calls[0].Disposition));
			ASSERT_THAT(AreEqual(
				Targets[0].Descriptor.DumpNormalized(),
				Plan.Calls[0].ExternalCallDescriptor.DumpNormalized()));
			ASSERT_THAT(AreEqual(
				EAngelscriptStaticJITNativeCallValidationCode::Valid,
				Plan.Calls[0].ExternalCallValidation.Code));
			ASSERT_THAT(IsTrue(Plan.DumpNormalized().Contains(
				Targets[0].Descriptor.Symbol)));
		}

		Targets[0].Descriptor = MakeDirectDescriptor(
			EAngelscriptStaticJITNativeCallLinkage::ExportedSymbol,
			FAngelscriptStaticJITScalarABIIdentity::FromCanonical(
				TEXT("cdecl:i64(i32)")));
		FAngelscriptTypedASTJITCallClosureOptions MismatchOptions =
			MakeOptions();
		MismatchOptions.NativeCallTargets = Targets;
		MismatchOptions.KnownNativeCallModules = {FName(TEXT("Core"))};
		MismatchOptions.PublicNativeCallDependencyModules =
			MismatchOptions.KnownNativeCallModules;
		FAngelscriptTypedASTJITCallClosurePlan Mismatch;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x49), MismatchOptions, Mismatch, &Error),
			Error));
		ASSERT_THAT(IsFalse(Mismatch.bEligible));
		ASSERT_THAT(AreEqual(1, Mismatch.Calls.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Unsupported,
			Mismatch.Calls[0].Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITNativeCallValidationCode::ScalarABIMismatch,
			Mismatch.Calls[0].ExternalCallValidation.Code));
		ASSERT_THAT(AreEqual(92,
			static_cast<int32>(Mismatch.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(14,
			static_cast<int32>(Mismatch.RootEligibility.SourceSpan.column)));
		ASSERT_THAT(IsTrue(Mismatch.RootEligibility.Detail.Contains(
			TEXT("ScalarABIMismatch"))));
	}

	TEST_METHOD(ReviewedProviderPrivateScalarUsesCurrentNativeBinding)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(
			404,
			93,
			17,
			asETypedSemanticCallTargetKind::SystemFunction);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x4b, RootHIR, true),
		};
		const FAngelscriptStaticJITScalarABIIdentity ScalarIdentity =
			FAngelscriptStaticJITScalarABIIdentity::FromCanonical(
				TEXT("cdecl:i32(i32)"));
		const FAngelscriptStaticJITNativeABIIdentity NativeIdentity =
			FAngelscriptStaticJITNativeABIIdentity::FromCanonical(
				TEXT("callconv=0;return=int;params=(int:inout=0)"));

		FAngelscriptStaticJITNativeCallTarget Target;
		Target.EngineLocalFunctionId = 404;
		Target.CanonicalDeclaration = TEXT("int PrivateAdd(int Value)");
		Target.RegisteredTargetDisplay = TEXT("FPrivateBinds::PrivateAdd");
		Target.ExpectedNativeABI = NativeIdentity;
		Target.ExpectedScalarABI = ScalarIdentity;
		Target.Reference.Kind =
			EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
		Target.Reference.StableKey = RepeatedByteHash(0xa1);
		Target.Reference.ExpectedAbi = RepeatedByteHash(0xa2);
		Target.bCurrentNativeCallable = true;
		Target.Descriptor.Linkage =
			EAngelscriptStaticJITNativeCallLinkage::ProviderPrivate;
		Target.Descriptor.Visibility =
			EAngelscriptStaticJITNativeCallVisibility::ProviderPrivate;
		Target.Descriptor.Symbol = TEXT("FPrivateBinds::PrivateAdd");
		Target.Descriptor.OwningModule = FName(TEXT("AngelscriptTest"));
		Target.Descriptor.CppCallableSignature = TEXT("int32(int32)");
		Target.Descriptor.NativeABI = NativeIdentity;
		Target.Descriptor.ABIDomain =
			EAngelscriptStaticJITNativeCallABIDomain::Scalar;
		Target.Descriptor.ExceptionPolicy =
			EAngelscriptStaticJITNativeCallExceptionPolicy::
				DoesNotSetScriptException;
		Target.Descriptor.ScalarABI = ScalarIdentity;
		Target.Descriptor.RouteFlags =
			EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall;
		Target.Descriptor.LifetimeFlags =
			EAngelscriptStaticJITNativeCallLifetimeFlags::ByValueScalarOnly
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoRetainedReferences
			| EAngelscriptStaticJITNativeCallLifetimeFlags::NoEngineLocalCapture;

		TArray<FAngelscriptStaticJITNativeCallTarget> Targets = {Target};
		FAngelscriptTypedASTJITCallClosureOptions Options = MakeOptions();
		Options.NativeCallTargets = Targets;
		Options.KnownNativeCallModules = {FName(TEXT("AngelscriptTest"))};

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x4b), Options, Plan, &Error), Error));
		ASSERT_THAT(IsTrue(Plan.bEligible));
		ASSERT_THAT(AreEqual(1, Plan.Calls.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::CurrentNativeBinding,
			Plan.Calls[0].Disposition));
		ASSERT_THAT(IsTrue(
			Plan.Calls[0].ExternalCallValidation.bCurrentNativeCallable));
		ASSERT_THAT(IsFalse(
			Plan.Calls[0].ExternalCallValidation.bScalarBridgeCallable));
	}

	TEST_METHOD(PositionOnlyClosurePassesButInstrumentedChildMismatchSelectsVm)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(202, 76, 19);
		asCTypedSemanticFunction HelperHIR = MakeLeafHIR(77, 3);
		const EAngelscriptTypedASTJITDirectCapability PositionOnly =
			EAngelscriptTypedASTJITDirectCapability::FramePosition;
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x38, RootHIR, true, PositionOnly),
			MakeFunction(202, 0x18, HelperHIR, false, PositionOnly),
		};
		FAngelscriptTypedASTJITCallClosureOptions Options =
			MakeOptions(PositionOnly);

		FAngelscriptTypedASTJITCallClosurePlan PositionPlan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x38), Options, PositionPlan, &Error)));
		ASSERT_THAT(IsTrue(PositionPlan.bEligible));

		const EAngelscriptTypedASTJITDirectCapability Instrumented =
			PositionOnly
			| EAngelscriptTypedASTJITDirectCapability::LineCallback
			| EAngelscriptTypedASTJITDirectCapability::Coverage;
		FString CapabilityError;
		ASSERT_THAT(IsTrue(
			BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
				Instrumented, Functions[0].CapabilityProfile,
				&CapabilityError), CapabilityError));
		Options = MakeOptions(Instrumented);
		FAngelscriptTypedASTJITCallClosurePlan InstrumentedPlan;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x38), Options, InstrumentedPlan, &Error)));
		ASSERT_THAT(IsFalse(InstrumentedPlan.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability,
			InstrumentedPlan.RootEligibility.Reason));
		ASSERT_THAT(AreEqual(76,
			static_cast<int32>(
				InstrumentedPlan.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(19,
			static_cast<int32>(
				InstrumentedPlan.RootEligibility.SourceSpan.column)));
		ASSERT_THAT(IsTrue(InstrumentedPlan.RootEligibility.Detail.Contains(
			TEXT("DirectCalleeProfileMismatch"))));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITCallDisposition::Unsupported,
			InstrumentedPlan.Calls[0].Disposition));
	}

	TEST_METHOD(IneligibleRequiredCalleeDoesNotPoisonIndependentRoot)
	{
		asCTypedSemanticFunction EligibleRootHIR = MakeLeafHIR(72, 2);
		asCTypedSemanticFunction FallbackRootHIR = MakeCallHIR(303, 73, 11);
		asCTypedSemanticFunction UnsupportedCalleeHIR = MakeLeafHIR(74, 4);
		UnsupportedCalleeHIR.expressions[0].kind =
			asETypedSemanticExpressionKind::Unsupported;
		UnsupportedCalleeHIR.expressions[0].unsupportedKind =
			asETypedSemanticUnsupportedKind::Lambda;

		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x36, EligibleRootHIR, true),
			MakeFunction(202, 0x46, FallbackRootHIR, true),
			MakeFunction(303, 0x56, UnsupportedCalleeHIR, false),
		};

		FAngelscriptTypedASTJITCallClosurePlan Eligible;
		FAngelscriptTypedASTJITCallClosurePlan Fallback;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x36), MakeOptions(), Eligible, &Error)));
		ASSERT_THAT(IsTrue(Eligible.bEligible));
		ASSERT_THAT(AreEqual(1, Eligible.Functions.Num()));
		ASSERT_THAT(AreEqual(0, Eligible.Calls.Num()));

		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x46), MakeOptions(), Fallback, &Error)));
		ASSERT_THAT(IsFalse(Fallback.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
			Fallback.RootEligibility.Reason));
		ASSERT_THAT(AreEqual(73,
			static_cast<int32>(Fallback.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(11,
			static_cast<int32>(Fallback.RootEligibility.SourceSpan.column)));
		ASSERT_THAT(AreEqual(1, Fallback.Calls.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
			Fallback.Calls[0].CalleeEligibility.Reason));
		ASSERT_THAT(IsTrue(Fallback.RootEligibility.Detail.Contains(
			TEXT("CalleeDisposition=Unsupported"))));
		ASSERT_THAT(IsTrue(Eligible.bEligible));
	}

	TEST_METHOD(DuplicateTaskCoordinatesProduceNoPartialClosure)
	{
		asCTypedSemanticFunction RootHIR = MakeLeafHIR(80, 1);
		asCTypedSemanticFunction DuplicateHIR = MakeLeafHIR(81, 1);
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x34, RootHIR, true),
			MakeFunction(101, 0x14, DuplicateHIR, false),
		};

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsFalse(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x34), MakeOptions(), Plan, &Error)));
		ASSERT_THAT(IsTrue(Error.StartsWith(
			TEXT("TypedASTJITCallGraphDuplicateFunctionId:"))));
		ASSERT_THAT(IsFalse(Plan.bEligible));
		ASSERT_THAT(AreEqual(0, Plan.Functions.Num()));
		ASSERT_THAT(AreEqual(0, Plan.Calls.Num()));
	}

	TEST_METHOD(InvalidEvaluationSequenceIsSourceLocatedAndNeverPlanned)
	{
		asCTypedSemanticFunction RootHIR = MakeCallHIR(202, 90, 19);
		asCTypedSemanticFunction HelperHIR = MakeLeafHIR(91, 1);
		RootHIR.expressions[1].evaluationSequence[0] = 1;
		TArray<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions = {
			MakeFunction(101, 0x35, RootHIR, true),
			MakeFunction(202, 0x15, HelperHIR, false),
		};

		FAngelscriptTypedASTJITCallClosurePlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITCallClosure(
			Functions, FunctionKey(0x35), MakeOptions(), Plan, &Error)));
		ASSERT_THAT(IsFalse(Plan.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::InvalidCallEvaluationSequence,
			Plan.RootEligibility.Reason));
		ASSERT_THAT(AreEqual(90,
			static_cast<int32>(Plan.RootEligibility.SourceSpan.row)));
		ASSERT_THAT(AreEqual(19,
			static_cast<int32>(Plan.RootEligibility.SourceSpan.column)));
		ASSERT_THAT(AreEqual(0, Plan.Calls.Num()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
