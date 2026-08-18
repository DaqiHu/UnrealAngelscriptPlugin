#include "CQTest.h"

#include "Core/AngelscriptEngine.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"

#include "Algo/Reverse.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_typeinfo.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptTypedASTJITEligibilityTests_Private
{
	static asSTypedSemanticSourceSpan MakeSpan(
		const asUINT Row = 12,
		const asUINT Column = 7)
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = "/Angelscript/Game/Eligibility.as";
		Span.sourceOffset = 41;
		Span.sourceLength = 3;
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

	static asCTypedSemanticFunction MakeEligibilityHIR(
		const asCDataType& ParameterType)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int Eligibility(int Value)";

		asSTypedSemanticSymbol Parameter;
		Parameter.kind = asETypedSemanticSymbolKind::Parameter;
		Parameter.name = "Value";
		Parameter.type = ParameterType;
		Parameter.span = MakeSpan();
		const asTypedSemanticSymbolId ParameterId =
			Function.AddSymbol(Parameter);

		asSTypedSemanticExpression Read;
		Read.kind = asETypedSemanticExpressionKind::Symbol;
		Read.type = ParameterType;
		Read.span = MakeSpan();
		Read.symbol = ParameterId;
		const asTypedSemanticExpressionId ReadId =
			Function.AddExpression(Read);

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = MakeSpan();
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = ReadId;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId =
			Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static asCTypedSemanticFunction MakeFinalInstanceEligibilityHIR(
		const asCDataType& ReceiverType,
		const asCDataType& ParameterType,
		const bool bReadReceiver)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int FinalInstanceEligibility(int Value) final";
		Function.header.declaredTraitBits = asTRAIT_FINAL;
		Function.header.traitSummary =
			ClassifyTypedSemanticFunctionTraits(asTRAIT_FINAL);
		Function.header.invocationKind =
			asETypedSemanticInvocationKind::InstanceMethod;

		asSTypedSemanticSymbol Receiver;
		Receiver.kind = asETypedSemanticSymbolKind::Receiver;
		Receiver.name = "this";
		Receiver.type = ReceiverType;
		Receiver.span = MakeSpan();
		const asTypedSemanticSymbolId ReceiverId =
			Function.AddSymbol(Receiver);
		Function.header.receiver.kind =
			asETypedSemanticReceiverKind::NativeObjectThis;
		Function.header.receiver.symbol = ReceiverId;
		Function.header.receiver.parameterIndex = -1;
		Function.header.receiver.type = ReceiverType;

		asSTypedSemanticSymbol Parameter;
		Parameter.kind = asETypedSemanticSymbolKind::Parameter;
		Parameter.name = "Value";
		Parameter.type = ParameterType;
		Parameter.span = MakeSpan();
		const asTypedSemanticSymbolId ParameterId =
			Function.AddSymbol(Parameter);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		if (bReadReceiver)
		{
			asSTypedSemanticExpression ReadReceiver;
			ReadReceiver.kind = asETypedSemanticExpressionKind::Symbol;
			ReadReceiver.type = ReceiverType;
			ReadReceiver.span = MakeSpan(13, 3);
			ReadReceiver.symbol = ReceiverId;

			asSTypedSemanticStatement ObserveReceiver;
			ObserveReceiver.kind =
				asETypedSemanticStatementKind::Expression;
			ObserveReceiver.span = ReadReceiver.span;
			ObserveReceiver.valueExpression =
				Function.AddExpression(ReadReceiver);
			Root.children.PushLast(
				Function.AddStatement(ObserveReceiver));
		}

		asSTypedSemanticExpression ReadParameter;
		ReadParameter.kind = asETypedSemanticExpressionKind::Symbol;
		ReadParameter.type = ParameterType;
		ReadParameter.span = MakeSpan(14, 9);
		ReadParameter.symbol = ParameterId;

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = ReadParameter.span;
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = Function.AddExpression(ReadParameter);
		Return.exitedScopeCount = 1;
		Root.children.PushLast(Function.AddStatement(Return));
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static FAngelscriptTypedASTJITEligibilityInput MakeEligibilityInput(
		const asCTypedSemanticFunction& Function,
		const asCDataType& Type)
	{
		FAngelscriptTypedASTJITEligibilityInput Input;
		Input.bIsUFunctionRoot = true;
		Input.TypedHIR = &Function;
		Input.Shape.SymbolName = TEXT("AS_Typed_Eligibility");
		Input.Shape.ParameterTypes.Add(Type);
		Input.Shape.ReturnType = Type;
		Input.Shape.InvocationKind =
			asETypedSemanticInvocationKind::Global;
		Input.Shape.ReceiverKind =
			asETypedSemanticReceiverKind::None;
		return Input;
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

	static asCTypedSemanticFunction MakeGlobalUseHIR(
		const asETypedSemanticExpressionKind Kind,
		const asETypedSemanticInvocationKind InvocationKind,
		const int32 GlobalPropertyId = 17,
		const asQWORD LiteralBits = 42)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = InvocationKind
			== asETypedSemanticInvocationKind::GlobalInitializer
			? "void __GlobalInit()"
			: "int ReadGlobal()";
		Function.header.invocationKind = InvocationKind;

		asSTypedSemanticExpression GlobalUse;
		GlobalUse.kind = Kind;
		GlobalUse.type = asCDataType::CreatePrimitive(ttInt, true);
		GlobalUse.span = MakeSpan(31, 11);
		GlobalUse.resolvedGlobalPropertyId = GlobalPropertyId;
		GlobalUse.literalBits = LiteralBits;
		const asTypedSemanticExpressionId GlobalUseId =
			Function.AddExpression(GlobalUse);

		asSTypedSemanticStatement BodyStatement;
		BodyStatement.span = GlobalUse.span;
		if (InvocationKind
			== asETypedSemanticInvocationKind::GlobalInitializer)
		{
			BodyStatement.kind = asETypedSemanticStatementKind::Expression;
			BodyStatement.valueExpression = GlobalUseId;
		}
		else
		{
			BodyStatement.kind = asETypedSemanticStatementKind::Return;
			BodyStatement.safePointRole =
				asETypedSemanticSafePointRole::Return;
			BodyStatement.valueExpression = GlobalUseId;
			BodyStatement.exitedScopeCount = 1;
		}
		const asTypedSemanticStatementId BodyStatementId =
			Function.AddStatement(BodyStatement);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan(30, 1);
		Root.children.PushLast(BodyStatementId);
		Function.rootStatement = Function.AddStatement(Root);
		MarkVerifiedEmptyCleanup(Function);
		return Function;
	}

	static FAngelscriptTypedASTJITEligibilityInput MakeGlobalUseInput(
		const asCTypedSemanticFunction& Function)
	{
		FAngelscriptTypedASTJITEligibilityInput Input;
		Input.bIsUFunctionRoot = true;
		Input.TypedHIR = &Function;
		Input.Shape.SymbolName = TEXT("AS_Typed_ReadGlobal");
		Input.Shape.ReturnType = Function.header.invocationKind
			== asETypedSemanticInvocationKind::GlobalInitializer
			? asCDataType::CreatePrimitive(ttVoid, false)
			: asCDataType::CreatePrimitive(ttInt, false);
		Input.Shape.InvocationKind = Function.header.invocationKind;
		Input.Shape.ReceiverKind = asETypedSemanticReceiverKind::None;
		return Input;
	}

	static FAngelscriptTypedASTJITGlobalDependency MakeGlobalDependency(
		const int32 GlobalPropertyId,
		const EAngelscriptArtifactSemanticDependencyKind Kind)
	{
		FAngelscriptTypedASTJITGlobalDependency Dependency;
		Dependency.EngineLocalGlobalPropertyId = GlobalPropertyId;
		Dependency.Kind = Kind;
		Dependency.Reference.Kind =
			EAngelscriptArtifactReferenceKind::ScriptGlobal;
		Dependency.Reference.StableKey = RepeatedByteHash(0x31);
		Dependency.Reference.ExpectedAbi = RepeatedByteHash(0x32);
		Dependency.ExpectedContentOrValue = RepeatedByteHash(0x33);
		return Dependency;
	}

	static asIScriptFunction* FunctionHandle(const UPTRINT Value)
	{
		return reinterpret_cast<asIScriptFunction*>(Value);
	}

	static asIScriptModule* ModuleHandle(const UPTRINT Value)
	{
		return reinterpret_cast<asIScriptModule*>(Value);
	}

	struct FGraphFixture
	{
		TArray<TSharedRef<FAngelscriptModuleDesc>> OwnedModules;
		TArray<TSharedRef<FAngelscriptFunctionDesc>> DetachedDescriptors;
		TArray<asIScriptModule*> ScriptModules;
		TArray<const FAngelscriptModuleDesc*> ResolvedModuleDescriptors;
		TArray<FAngelscriptStaticJITCompiledFunctionView> Functions;
		TArray<FAngelscriptStaticJITDescriptorView> Descriptors;
		FAngelscriptStaticJITCompiledSourceGraph Graph;

		TSharedRef<FAngelscriptModuleDesc> AddModule(
			const TCHAR* Name,
			const UPTRINT Handle)
		{
			TSharedRef<FAngelscriptModuleDesc> Module =
				MakeShared<FAngelscriptModuleDesc>();
			Module->ModuleName = Name;
			OwnedModules.Add(Module);
			ScriptModules.Add(ModuleHandle(Handle));
			ResolvedModuleDescriptors.Add(&Module.Get());
			return Module;
		}

		TSharedRef<FAngelscriptClassDesc> AddClass(
			const TSharedRef<FAngelscriptModuleDesc>& Module,
			const TCHAR* Name,
			const bool bStatics,
			const uint8 DescriptorByte)
		{
			TSharedRef<FAngelscriptClassDesc> Class =
				MakeShared<FAngelscriptClassDesc>();
			Class->ClassName = Name;
			Class->bIsStaticsClass = bStatics;
			Module->Classes.Add(Class);

			FAngelscriptStaticJITDescriptorView& Descriptor =
				Descriptors.Emplace_GetRef();
			Descriptor.EngineLocalDescriptor = &Class.Get();
			Descriptor.StableDescriptorKey =
				RepeatedByteHash(DescriptorByte);
			Descriptor.Kind = EAngelscriptStaticJITDescriptorKind::Class;
			Descriptor.CanonicalName = bStatics
				? FStringView(TEXT("GeneratedStaticsSurface"))
				: FStringView(TEXT("OrdinaryInstanceSurface"));
			Descriptor.CanonicalDeclaration = bStatics
				? FStringView(TEXT("class GeneratedStaticsSurface"))
				: FStringView(TEXT("class OrdinaryInstanceSurface"));
			return Class;
		}

		TSharedRef<FAngelscriptFunctionDesc> AddRoot(
			const TSharedRef<FAngelscriptClassDesc>& Class,
			asIScriptFunction* Function,
			const uint8 FunctionByte,
			const TCHAR* FunctionName,
			const TCHAR* Declaration)
		{
			TSharedRef<FAngelscriptFunctionDesc> FunctionDesc =
				MakeShared<FAngelscriptFunctionDesc>();
			FunctionDesc->FunctionName = FunctionName;
			FunctionDesc->ScriptFunctionName = FunctionName;
			FunctionDesc->ScriptFunction = Function;
			FunctionDesc->bIsStatic = Class->bIsStaticsClass;
			Class->Methods.Add(FunctionDesc);

			FAngelscriptStaticJITCompiledFunctionView& FunctionView =
				Functions.Emplace_GetRef();
			FunctionView.EngineLocalFunction = Function;
			FunctionView.ModuleKey.Hash = RepeatedByteHash(0x70);
			FunctionView.FunctionKey.Hash = RepeatedByteHash(FunctionByte);
			FunctionView.CanonicalDeclaration = Declaration;

			FAngelscriptStaticJITDescriptorView& Descriptor =
				Descriptors.Emplace_GetRef();
			Descriptor.EngineLocalDescriptor = &FunctionDesc.Get();
			Descriptor.StableDescriptorKey = FunctionView.FunctionKey.Hash;
			Descriptor.Kind = EAngelscriptStaticJITDescriptorKind::Function;
			Descriptor.CanonicalName = FunctionName;
			Descriptor.CanonicalDeclaration = Declaration;
			Descriptor.bUFunctionRoot = true;
			return FunctionDesc;
		}

		void AddHelper(
			asIScriptFunction* Function,
			const uint8 FunctionByte,
			const TCHAR* Declaration)
		{
			FAngelscriptStaticJITCompiledFunctionView& FunctionView =
				Functions.Emplace_GetRef();
			FunctionView.EngineLocalFunction = Function;
			FunctionView.ModuleKey.Hash = RepeatedByteHash(0x70);
			FunctionView.FunctionKey.Hash = RepeatedByteHash(FunctionByte);
			FunctionView.CanonicalDeclaration = Declaration;
		}

		void RefreshViews()
		{
			Graph.Modules = ScriptModules;
			Graph.ResolvedModuleDescriptors = ResolvedModuleDescriptors;
			Graph.Functions = Functions;
			Graph.Descriptors = Descriptors;
		}
	};

	static FGraphFixture MakeCompleteGraphFixture()
	{
		FGraphFixture Fixture;
		const TSharedRef<FAngelscriptModuleDesc> Module =
			Fixture.AddModule(TEXT("Eligibility.Roots"), 0x1000);
		const TSharedRef<FAngelscriptClassDesc> Statics = Fixture.AddClass(
			Module, TEXT("EligibilityStatics"), true, 0x61);
		const TSharedRef<FAngelscriptClassDesc> Instance = Fixture.AddClass(
			Module, TEXT("UEligibilityObject"), false, 0x62);

		Fixture.AddRoot(Statics, FunctionHandle(0x2000), 0x30,
			TEXT("SharedTextName"), TEXT("int SharedTextName(int)"));
		Fixture.AddRoot(Instance, FunctionHandle(0x2100), 0x10,
			TEXT("SharedTextName"), TEXT("int SharedTextName(int)"));
		Fixture.AddRoot(Instance, FunctionHandle(0x2200), 0x20,
			TEXT("Overloaded"), TEXT("int Overloaded(int)"));
		Fixture.AddRoot(Instance, FunctionHandle(0x2300), 0x40,
			TEXT("Overloaded"), TEXT("int Overloaded(float)"));
		Fixture.AddHelper(FunctionHandle(0x2400), 0x50,
			TEXT("int SharedTextName(int)"));

		// This descriptor deliberately has the same textual identity as roots but
		// is not present in any resolved ClassDesc::Methods array. Text matching
		// would incorrectly publish the helper as a root.
		TSharedRef<FAngelscriptFunctionDesc> TextDecoy =
			MakeShared<FAngelscriptFunctionDesc>();
		TextDecoy->FunctionName = TEXT("SharedTextName");
		TextDecoy->ScriptFunctionName = TEXT("SharedTextName");
		TextDecoy->ScriptFunction = FunctionHandle(0x2400);
		Fixture.DetachedDescriptors.Add(TextDecoy);
		FAngelscriptStaticJITDescriptorView& Decoy =
			Fixture.Descriptors.Emplace_GetRef();
		Decoy.EngineLocalDescriptor = &TextDecoy.Get();
		Decoy.StableDescriptorKey = RepeatedByteHash(0x50);
		Decoy.Kind = EAngelscriptStaticJITDescriptorKind::Function;
		Decoy.CanonicalName = TEXT("SharedTextName");
		Decoy.CanonicalDeclaration = TEXT("int SharedTextName(int)");
		Decoy.bUFunctionRoot = false;

		Fixture.RefreshViews();
		return Fixture;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITEligibilityTests,
	"Angelscript.TestModule.StaticJIT.Eligibility.TypedASTJIT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SupportedByValueScalarAndEnumSignaturesRequireVerifiedEmptyCleanup)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		asCEnumType EnumType(static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine()));
		EnumType.name = "EEligibility";
		EnumType.size = 4;
		EnumType.flags = asOBJ_ENUM;

		const asCDataType SupportedTypes[] = {
			asCDataType::CreatePrimitive(ttBool, false),
			asCDataType::CreatePrimitive(ttInt8, false),
			asCDataType::CreatePrimitive(ttUInt8, false),
			asCDataType::CreatePrimitive(ttInt16, false),
			asCDataType::CreatePrimitive(ttUInt16, false),
			asCDataType::CreatePrimitive(ttInt, false),
			asCDataType::CreatePrimitive(ttUInt, false),
			asCDataType::CreatePrimitive(ttInt64, false),
			asCDataType::CreatePrimitive(ttUInt64, false),
			asCDataType::CreatePrimitive(ttFloat32, false),
			asCDataType::CreatePrimitive(ttFloat64, false),
			asCDataType::CreateType(&EnumType, false),
		};

		for (const asCDataType& Type : SupportedTypes)
		{
			asCTypedSemanticFunction Function = MakeEligibilityHIR(Type);
			const FAngelscriptTypedASTJITEligibility Eligibility =
				EvaluateAngelscriptTypedASTJITEligibility(
					MakeEligibilityInput(Function, Type));
			ASSERT_THAT(IsTrue(Eligibility.bEligible));
			ASSERT_THAT(AreEqual(
				EAngelscriptTypedASTJITFallbackReason::None,
				Eligibility.Reason));
		}

		asCTypedSemanticFunction Unverified = MakeEligibilityHIR(
			asCDataType::CreatePrimitive(ttInt, false));
		Unverified.cleanupPlans.SetLength(0);
		Unverified.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::Unverified;
		Unverified.header.universalCleanupPlan =
			asTypedSemanticCleanupPlanId();
		Unverified.header.cleanupPlanCoversAllTransfers = false;
		const FAngelscriptTypedASTJITEligibility Rejected =
			EvaluateAngelscriptTypedASTJITEligibility(
				MakeEligibilityInput(Unverified,
					asCDataType::CreatePrimitive(ttInt, false)));
		ASSERT_THAT(IsFalse(Rejected.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan,
			Rejected.Reason));
		ASSERT_THAT(IsTrue(Rejected.ToDiagnosticString().StartsWith(
			TEXT("InvalidCleanupPlan:"))));
	}

	TEST_METHOD(FinalInstanceRootAllowsUnusedNativeReceiverAndRejectsReceiverRead)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		asCObjectType ReceiverObject(static_cast<asCScriptEngine*>(
			FAngelscriptEngine::Get().GetScriptEngine()));
		ReceiverObject.name = "UTypedFinalReceiver";
		ReceiverObject.flags = asOBJ_REF;
		const asCDataType ReceiverType =
			asCDataType::CreateType(&ReceiverObject, false);
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);

		auto MakeInput = [&](const asCTypedSemanticFunction& Function)
		{
			FAngelscriptTypedASTJITEligibilityInput Input;
			Input.bIsUFunctionRoot = true;
			Input.TypedHIR = &Function;
			Input.RawTraitBits = asTRAIT_FINAL;
			Input.Shape.SymbolName = TEXT("AS_Typed_FinalInstance");
			Input.Shape.ParameterTypes.Add(IntType);
			Input.Shape.ReturnType = IntType;
			Input.Shape.InvocationKind =
				asETypedSemanticInvocationKind::InstanceMethod;
			Input.Shape.ReceiverKind =
				asETypedSemanticReceiverKind::NativeObjectThis;
			Input.Shape.ReceiverParameterIndex = INDEX_NONE;
			Input.Shape.bHasNativeObjectSlot = true;
			return Input;
		};

		const asCTypedSemanticFunction UnusedReceiver =
			MakeFinalInstanceEligibilityHIR(
				ReceiverType, IntType, false);
		const FAngelscriptTypedASTJITEligibility Eligible =
			EvaluateAngelscriptTypedASTJITEligibility(
				MakeInput(UnusedReceiver));
		ASSERT_THAT(IsTrue(Eligible.bEligible, Eligible.ToDiagnosticString()));

		const asCTypedSemanticFunction ReadReceiver =
			MakeFinalInstanceEligibilityHIR(
				ReceiverType, IntType, true);
		const FAngelscriptTypedASTJITEligibility Rejected =
			EvaluateAngelscriptTypedASTJITEligibility(
				MakeInput(ReadReceiver));
		ASSERT_THAT(IsFalse(Rejected.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
			Rejected.Reason));
		ASSERT_THAT(IsTrue(Rejected.ToDiagnosticString().Contains(
			TEXT("/Angelscript/Game/Eligibility.as:13:3"))));
	}

	TEST_METHOD(FallbackReasonsExposeFrozenDebugTokens)
	{
		struct FReasonCase
		{
			EAngelscriptTypedASTJITFallbackReason Reason;
			const TCHAR* Token;
		};
		const FReasonCase Cases[] = {
			{EAngelscriptTypedASTJITFallbackReason::NotUFunctionRoot, TEXT("NotUFunctionRoot")},
			{EAngelscriptTypedASTJITFallbackReason::MissingTypedHIR, TEXT("MissingTypedHIR")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind, TEXT("UnsupportedFunctionKind")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait, TEXT("UnsupportedFunctionTrait")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver, TEXT("UnsupportedReceiver")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedUFunctionFlags, TEXT("UnsupportedUFunctionFlags")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature, TEXT("UnsupportedSignature")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedType, TEXT("UnsupportedType")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedStatement, TEXT("UnsupportedStatement")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression, TEXT("UnsupportedExpression")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedCall, TEXT("UnsupportedCall")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedGlobalStorage, TEXT("UnsupportedGlobalStorage")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedGlobalInitializer, TEXT("UnsupportedGlobalInitializer")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedImportedRoute, TEXT("UnsupportedImportedRoute")},
			{EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch, TEXT("SemanticDependencyMismatch")},
			{EAngelscriptTypedASTJITFallbackReason::NonPortableNumericConversion, TEXT("NonPortableNumericConversion")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability, TEXT("UnsupportedExecutionObservability")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl, TEXT("UnsupportedExecutionControl")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime, TEXT("UnsupportedLifetime")},
			{EAngelscriptTypedASTJITFallbackReason::UnsupportedExceptionRegion, TEXT("UnsupportedExceptionRegion")},
			{EAngelscriptTypedASTJITFallbackReason::ExceptionPayloadUnavailable, TEXT("ExceptionPayloadUnavailable")},
			{EAngelscriptTypedASTJITFallbackReason::BridgeExceptionAdoptionUnavailable, TEXT("BridgeExceptionAdoptionUnavailable")},
			{EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState, TEXT("SuspendOrExceptionState")},
			{EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver, TEXT("InvalidEffectiveReceiver")},
			{EAngelscriptTypedASTJITFallbackReason::InvalidCallEvaluationSequence, TEXT("InvalidCallEvaluationSequence")},
			{EAngelscriptTypedASTJITFallbackReason::InvalidControlTarget, TEXT("InvalidControlTarget")},
			{EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan, TEXT("InvalidCleanupPlan")},
			{EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR, TEXT("InvalidTypedHIR")},
			{EAngelscriptTypedASTJITFallbackReason::EmitterFailure, TEXT("EmitterFailure")},
			{EAngelscriptTypedASTJITFallbackReason::BackendUnavailable, TEXT("BackendUnavailable")},
		};
		for (const FReasonCase& Case : Cases)
		{
			ASSERT_THAT(AreEqual(FString(Case.Token),
				FString(LexToString(Case.Reason))));
		}
	}

	TEST_METHOD(FunctionSurfaceTraitReceiverAndSignatureExclusionsAreTyped)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		asCTypedSemanticFunction Function = MakeEligibilityHIR(IntType);

		auto ExpectReason = [this](
			const FAngelscriptTypedASTJITEligibilityInput& Input,
			const EAngelscriptTypedASTJITFallbackReason Expected)
		{
			const FAngelscriptTypedASTJITEligibility Eligibility =
				EvaluateAngelscriptTypedASTJITEligibility(Input);
			ASSERT_THAT(IsFalse(Eligibility.bEligible));
			ASSERT_THAT(AreEqual(Expected, Eligibility.Reason));
			ASSERT_THAT(IsTrue(Eligibility.ToDiagnosticString().StartsWith(
				FString(LexToString(Expected)) + TEXT(":"))));
		};

		FAngelscriptTypedASTJITEligibilityInput Input =
			MakeEligibilityInput(Function, IntType);
		Input.bIsUFunctionRoot = false;
		ExpectReason(Input,
			EAngelscriptTypedASTJITFallbackReason::NotUFunctionRoot);

		Input = MakeEligibilityInput(Function, IntType);
		Input.TypedHIR = nullptr;
		ExpectReason(Input,
			EAngelscriptTypedASTJITFallbackReason::MissingTypedHIR);

		for (const asETypedSemanticInvocationKind Kind : {
			asETypedSemanticInvocationKind::Constructor,
			asETypedSemanticInvocationKind::Destructor,
			asETypedSemanticInvocationKind::Factory,
			asETypedSemanticInvocationKind::Imported,
			asETypedSemanticInvocationKind::System,
			asETypedSemanticInvocationKind::Funcdef,
			asETypedSemanticInvocationKind::Synthesized})
		{
			asCTypedSemanticFunction KindFunction = Function;
			KindFunction.header.invocationKind = Kind;
			Input = MakeEligibilityInput(KindFunction, IntType);
			Input.Shape.InvocationKind = Kind;
			ExpectReason(Input,
				Kind == asETypedSemanticInvocationKind::Constructor
					|| Kind == asETypedSemanticInvocationKind::Destructor
					? EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver
					: EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind);
		}

		Input = MakeEligibilityInput(Function, IntType);
		Input.RawTraitBits = 0x80000000u;
		Function.header.declaredTraitBits = Input.RawTraitBits;
		Function.header.traitSummary =
			ClassifyTypedSemanticFunctionTraits(Input.RawTraitBits);
		Input.TypedHIR = &Function;
		ExpectReason(Input,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait);
		Function.header.declaredTraitBits = 0;
		Function.header.traitSummary =
			ClassifyTypedSemanticFunctionTraits(0);

		Input = MakeEligibilityInput(Function, IntType);
		Input.bVirtualOrNonFinal = true;
		ExpectReason(Input,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait);

		const EAngelscriptTypedASTJITUFunctionFlags UFunctionFlags[] = {
			EAngelscriptTypedASTJITUFunctionFlags::RpcOrNet,
			EAngelscriptTypedASTJITUFunctionFlags::BlueprintEvent,
			EAngelscriptTypedASTJITUFunctionFlags::BlueprintOverride,
			EAngelscriptTypedASTJITUFunctionFlags::ThreadSafeSpecial,
			EAngelscriptTypedASTJITUFunctionFlags::WorldContextGenerated,
		};
		for (const EAngelscriptTypedASTJITUFunctionFlags Flag : UFunctionFlags)
		{
			Input = MakeEligibilityInput(Function, IntType);
			Input.UFunctionFlags = Flag;
			ExpectReason(Input,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedUFunctionFlags);
		}

		for (const asETypedSemanticReceiverKind Receiver : {
			asETypedSemanticReceiverKind::NativeObjectThis,
			asETypedSemanticReceiverKind::ExternalImplicitThis,
			asETypedSemanticReceiverKind::MixinFirstParameter})
		{
			asCTypedSemanticFunction ReceiverFunction = Function;
			ReceiverFunction.header.receiver.kind = Receiver;
			ReceiverFunction.header.receiver.symbol =
				ReceiverFunction.symbols[0].id;
			ReceiverFunction.header.receiver.parameterIndex = 0;
			ReceiverFunction.header.receiver.type = IntType;
			Input = MakeEligibilityInput(ReceiverFunction, IntType);
			Input.Shape.ReceiverKind = Receiver;
			Input.Shape.ReceiverParameterIndex = 0;
			Input.Shape.bHasNativeObjectSlot =
				Receiver == asETypedSemanticReceiverKind::NativeObjectThis;
			ExpectReason(Input,
				EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver);
		}

		struct FSignatureCase
		{
			EAngelscriptTypedASTJITSignatureFlags Flag;
			EAngelscriptTypedASTJITFallbackReason Reason;
		};
		const FSignatureCase SignatureCases[] = {
			{EAngelscriptTypedASTJITSignatureFlags::Reference,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature},
			{EAngelscriptTypedASTJITSignatureFlags::OutParameter,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature},
			{EAngelscriptTypedASTJITSignatureFlags::ReturnOnStack,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature},
			{EAngelscriptTypedASTJITSignatureFlags::Object,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedType},
			{EAngelscriptTypedASTJITSignatureFlags::Struct,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedType},
			{EAngelscriptTypedASTJITSignatureFlags::Container,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedType},
			{EAngelscriptTypedASTJITSignatureFlags::Delegate,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedType},
		};
		for (const FSignatureCase& Case : SignatureCases)
		{
			Input = MakeEligibilityInput(Function, IntType);
			Input.SignatureFlags = Case.Flag;
			if (Case.Flag
				== EAngelscriptTypedASTJITSignatureFlags::ReturnOnStack)
			{
				Input.Shape.bReturnsOnStack = true;
			}
			ExpectReason(Input, Case.Reason);
		}
	}

	TEST_METHOD(UnrealRoutedRootsNeverBecomeTypedASTJITRawDirectEntries)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asCTypedSemanticFunction Function =
			MakeEligibilityHIR(IntType);

		struct FRouteCase
		{
			EAngelscriptTypedASTJITUFunctionFlags Flags;
			const TCHAR* Token;
		};
		const FRouteCase Cases[] = {
			{EAngelscriptTypedASTJITUFunctionFlags::RpcOrNet,
				TEXT("RpcOrNet")},
			{EAngelscriptTypedASTJITUFunctionFlags::BlueprintEvent,
				TEXT("BlueprintEvent")},
			{EAngelscriptTypedASTJITUFunctionFlags::BlueprintOverride,
				TEXT("BlueprintOverride")},
		};

		for (const FRouteCase& Case : Cases)
		{
			FAngelscriptTypedASTJITEligibilityInput Input =
				MakeEligibilityInput(Function, IntType);
			Input.UFunctionFlags = Case.Flags;
			const FAngelscriptTypedASTJITEligibility Eligibility =
				EvaluateAngelscriptTypedASTJITEligibility(Input);
			ASSERT_THAT(IsFalse(Eligibility.bEligible, Case.Token));
			ASSERT_THAT(AreEqual(
				EAngelscriptTypedASTJITFallbackReason::UnsupportedUFunctionFlags,
				Eligibility.Reason));
			ASSERT_THAT(IsTrue(
				Eligibility.Detail.Contains(Case.Token), Eligibility.Detail));
		}

		FAngelscriptTypedASTJITEligibilityInput VirtualInput =
			MakeEligibilityInput(Function, IntType);
		VirtualInput.bVirtualOrNonFinal = true;
		const FAngelscriptTypedASTJITEligibility VirtualEligibility =
			EvaluateAngelscriptTypedASTJITEligibility(VirtualInput);
		ASSERT_THAT(IsFalse(VirtualEligibility.bEligible));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait,
			VirtualEligibility.Reason));
		ASSERT_THAT(IsTrue(VirtualEligibility.Detail.Contains(
			TEXT("VirtualOrNonFinal=true")), VirtualEligibility.Detail));
	}

	TEST_METHOD(HIRControlLifetimeAndVerifierExclusionsRetainSourceLocation)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);
		const asCTypedSemanticFunction Baseline = MakeEligibilityHIR(IntType);

		auto ExpectReason = [this, &IntType](
			const asCTypedSemanticFunction& Function,
			const EAngelscriptTypedASTJITFallbackReason Expected)
		{
			const FAngelscriptTypedASTJITEligibility Eligibility =
				EvaluateAngelscriptTypedASTJITEligibility(
					MakeEligibilityInput(Function, IntType));
			ASSERT_THAT(IsFalse(Eligibility.bEligible));
			ASSERT_THAT(IsTrue(Expected == Eligibility.Reason,
				Eligibility.Detail));
			ASSERT_THAT(AreEqual(12,
				static_cast<int32>(Eligibility.SourceSpan.row)));
			ASSERT_THAT(AreEqual(7,
				static_cast<int32>(Eligibility.SourceSpan.column)));
			ASSERT_THAT(IsTrue(Eligibility.ToDiagnosticString().Contains(
				TEXT("/Angelscript/Game/Eligibility.as:12:7"))));
		};

		asCTypedSemanticFunction UnsupportedExpression = Baseline;
		UnsupportedExpression.expressions[0].kind =
			asETypedSemanticExpressionKind::Unsupported;
		UnsupportedExpression.expressions[0].unsupportedKind =
			asETypedSemanticUnsupportedKind::Lambda;
		ExpectReason(UnsupportedExpression,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression);

		asCTypedSemanticFunction UnsupportedStatement = Baseline;
		UnsupportedStatement.statements[0].kind =
			asETypedSemanticStatementKind::Unsupported;
		UnsupportedStatement.statements[0].unsupportedKind =
			asETypedSemanticUnsupportedKind::ConstructionOrLifetime;
		ExpectReason(UnsupportedStatement,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime);

		asCTypedSemanticFunction UnsupportedCall = Baseline;
		asSTypedSemanticExpression Call;
		Call.kind = asETypedSemanticExpressionKind::ResolvedCall;
		Call.type = IntType;
		Call.span = MakeSpan();
		Call.callTargetKind =
			asETypedSemanticCallTargetKind::ScriptFunction;
		Call.resolvedFunctionId = 17;
		Call.targetProvenance.canonicalDeclaration =
			"int UnsupportedTarget(int)";
		Call.targetProvenance.declarationModule = "SyntheticEligibility";
		Call.targetProvenance.bodyAvailability =
			asETypedSemanticBodyAvailability::ScriptBody;
		Call.targetProvenance.bodyOwnership =
			asETypedSemanticBodyOwnership::CallingModule;
		Call.targetProvenance.bodyOwnerModule = "SyntheticEligibility";
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
		Provenance.processedSpan = MakeSpan();
		Call.argumentProvenance.PushLast(Provenance);
		asSTypedSemanticEvaluationStep EvaluationStep;
		EvaluationStep.role = asETypedSemanticEvaluationRole::Argument;
		EvaluationStep.expression = asTypedSemanticExpressionId(0);
		EvaluationStep.formalParameterIndex = 0;
		EvaluationStep.sourceArgumentOrdinal = 0;
		Call.evaluationSteps.PushLast(EvaluationStep);
		UnsupportedCall.statements[0].valueExpression =
			UnsupportedCall.AddExpression(Call);
		ExpectReason(UnsupportedCall,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedCall);

		asCTypedSemanticFunction Suspend = Baseline;
		Suspend.header.hasSuspendState = true;
		ExpectReason(Suspend,
			EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState);

		asCTypedSemanticFunction InvalidReceiver = Baseline;
		InvalidReceiver.header.receiver.kind =
			asETypedSemanticReceiverKind::NativeObjectThis;
		ExpectReason(InvalidReceiver,
			EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver);

		asCTypedSemanticFunction InvalidHIR = Baseline;
		InvalidHIR.rootStatement = asTypedSemanticStatementId(99);
		ExpectReason(InvalidHIR,
			EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR);

		asCTypedSemanticFunction InvalidCleanup = Baseline;
		InvalidCleanup.header.cleanupPlanCoversAllTransfers = false;
		ExpectReason(InvalidCleanup,
			EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan);

		for (const asETypedSemanticCleanupPlanState State : {
			asETypedSemanticCleanupPlanState::NonEmpty,
			asETypedSemanticCleanupPlanState::PartialConstruction,
			asETypedSemanticCleanupPlanState::ScriptDestructor})
		{
			asCTypedSemanticFunction Lifetime = Baseline;
			Lifetime.header.cleanupPlanState = State;
			Lifetime.header.cleanupPlanCoversAllTransfers = false;
			if (State == asETypedSemanticCleanupPlanState::NonEmpty)
			{
				Lifetime.cleanupPlans[0].reverseLiveLifetimeSlots.PushLast(0);
			}
			ExpectReason(Lifetime,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime);
		}

		asCTypedSemanticFunction ExceptionRegion = Baseline;
		ExceptionRegion.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::CompilerExceptionRegion;
		ExceptionRegion.header.hasExceptionCleanup = true;
		ExceptionRegion.header.cleanupPlanCoversAllTransfers = false;
		asSTypedSemanticExpression ExceptionRegionMarker;
		ExceptionRegionMarker.kind =
			asETypedSemanticExpressionKind::Unsupported;
		ExceptionRegionMarker.unsupportedKind =
			asETypedSemanticUnsupportedKind::ExceptionCleanup;
		ExceptionRegionMarker.type =
			asCDataType::CreatePrimitive(ttVoid, false);
		ExceptionRegionMarker.span = MakeSpan();
		ExceptionRegionMarker.resolvedTarget =
			"compiler-exception-region";
		ExceptionRegion.AddExpression(ExceptionRegionMarker);
		ExpectReason(ExceptionRegion,
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExceptionRegion);
	}

	TEST_METHOD(GlobalInitializerStorageAndFoldedConstantDependencyAreDistinct)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;

		asCTypedSemanticFunction Initializer = MakeGlobalUseHIR(
			asETypedSemanticExpressionKind::GlobalStorage,
			asETypedSemanticInvocationKind::GlobalInitializer);
		const FAngelscriptTypedASTJITEligibility InitializerResult =
			EvaluateAngelscriptTypedASTJITEligibility(
				MakeGlobalUseInput(Initializer));
		ASSERT_THAT(IsFalse(InitializerResult.bEligible));
		ASSERT_THAT(IsTrue(InitializerResult.Reason
			== EAngelscriptTypedASTJITFallbackReason::
				UnsupportedGlobalInitializer));

		asCTypedSemanticFunction Storage = MakeGlobalUseHIR(
			asETypedSemanticExpressionKind::GlobalStorage,
			asETypedSemanticInvocationKind::Global);
		const FAngelscriptTypedASTJITEligibility StorageResult =
			EvaluateAngelscriptTypedASTJITEligibility(
				MakeGlobalUseInput(Storage));
		ASSERT_THAT(IsFalse(StorageResult.bEligible));
		ASSERT_THAT(IsTrue(StorageResult.Reason
			== EAngelscriptTypedASTJITFallbackReason::
				UnsupportedGlobalStorage));
		ASSERT_THAT(AreEqual(31u, StorageResult.SourceSpan.row));
		ASSERT_THAT(AreEqual(11u, StorageResult.SourceSpan.column));

		asCTypedSemanticFunction Folded = MakeGlobalUseHIR(
			asETypedSemanticExpressionKind::FoldedGlobalConstant,
			asETypedSemanticInvocationKind::Global);
		FAngelscriptTypedASTJITEligibilityInput FoldedInput =
			MakeGlobalUseInput(Folded);
		const FAngelscriptTypedASTJITEligibility MissingDependency =
			EvaluateAngelscriptTypedASTJITEligibility(FoldedInput);
		ASSERT_THAT(IsFalse(MissingDependency.bEligible));
		ASSERT_THAT(IsTrue(MissingDependency.Reason
			== EAngelscriptTypedASTJITFallbackReason::
				SemanticDependencyMismatch));
		ASSERT_THAT(AreEqual(31u, MissingDependency.SourceSpan.row));

		FoldedInput.GlobalDependencies.Add(MakeGlobalDependency(
			17, EAngelscriptArtifactSemanticDependencyKind::GlobalStorage));
		const FAngelscriptTypedASTJITEligibility WrongKind =
			EvaluateAngelscriptTypedASTJITEligibility(FoldedInput);
		ASSERT_THAT(IsFalse(WrongKind.bEligible));
		ASSERT_THAT(IsTrue(WrongKind.Reason
			== EAngelscriptTypedASTJITFallbackReason::
				SemanticDependencyMismatch));

		FoldedInput.GlobalDependencies.Reset();
		FoldedInput.GlobalDependencies.Add(MakeGlobalDependency(
			18, EAngelscriptArtifactSemanticDependencyKind::HardValue));
		const FAngelscriptTypedASTJITEligibility WrongCoordinate =
			EvaluateAngelscriptTypedASTJITEligibility(FoldedInput);
		ASSERT_THAT(IsFalse(WrongCoordinate.bEligible));
		ASSERT_THAT(IsTrue(WrongCoordinate.Reason
			== EAngelscriptTypedASTJITFallbackReason::
				SemanticDependencyMismatch));

		FoldedInput.GlobalDependencies.Reset();
		FoldedInput.GlobalDependencies.Add(MakeGlobalDependency(
			17, EAngelscriptArtifactSemanticDependencyKind::HardValue));
		const FAngelscriptTypedASTJITEligibility ExactDependency =
			EvaluateAngelscriptTypedASTJITEligibility(FoldedInput);
		ASSERT_THAT(IsTrue(ExactDependency.bEligible));
		ASSERT_THAT(IsTrue(ExactDependency.Reason
			== EAngelscriptTypedASTJITFallbackReason::None));

		FoldedInput.GlobalDependencies.Add(MakeGlobalDependency(
			17, EAngelscriptArtifactSemanticDependencyKind::HardValue));
		const FAngelscriptTypedASTJITEligibility DuplicateDependency =
			EvaluateAngelscriptTypedASTJITEligibility(FoldedInput);
		ASSERT_THAT(IsFalse(DuplicateDependency.bEligible));
		ASSERT_THAT(IsTrue(DuplicateDependency.Reason
			== EAngelscriptTypedASTJITFallbackReason::
				SemanticDependencyMismatch));
	}

	TEST_METHOD(ResolvedDescriptorPointersSelectGlobalAndInstanceRoots)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		FGraphFixture Fixture = MakeCompleteGraphFixture();
		FAngelscriptTypedASTJITRootIndex RootIndex;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITRootIndex(
			Fixture.Graph, RootIndex, &Error)));
		ASSERT_THAT(AreEqual(FString(), Error));
		ASSERT_THAT(AreEqual(4, RootIndex.Roots.Num()));

		const FAngelscriptTypedASTJITRoot* Global =
			RootIndex.FindByFunction(FunctionHandle(0x2000));
		const FAngelscriptTypedASTJITRoot* Instance =
			RootIndex.FindByFunction(FunctionHandle(0x2100));
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsNotNull(Instance));
		ASSERT_THAT(IsTrue(Global->bGeneratedStaticsClassSurface));
		ASSERT_THAT(IsFalse(Instance->bGeneratedStaticsClassSurface));
		ASSERT_THAT(IsNull(RootIndex.FindByFunction(FunctionHandle(0x2400))));
		ASSERT_THAT(AreEqual(
			Global->FunctionDescriptor->ScriptFunction,
			Global->EngineLocalFunction));
		ASSERT_THAT(AreEqual(
			Instance->FunctionDescriptor->ScriptFunction,
			Instance->EngineLocalFunction));
	}

	TEST_METHOD(OverloadsAndInputOrderUseStableFunctionIdentity)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		FGraphFixture Fixture = MakeCompleteGraphFixture();
		FAngelscriptTypedASTJITRootIndex First;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITRootIndex(
			Fixture.Graph, First)));
		ASSERT_THAT(IsNotNull(First.FindByFunction(FunctionHandle(0x2200))));
		ASSERT_THAT(IsNotNull(First.FindByFunction(FunctionHandle(0x2300))));

		Algo::Reverse(Fixture.Functions);
		Algo::Reverse(Fixture.Descriptors);
		Algo::Reverse(Fixture.ResolvedModuleDescriptors);
		for (const TSharedRef<FAngelscriptModuleDesc>& Module
			: Fixture.OwnedModules)
		{
			Algo::Reverse(Module->Classes);
			for (const TSharedRef<FAngelscriptClassDesc>& Class
				: Module->Classes)
			{
				Algo::Reverse(Class->Methods);
			}
		}
		Fixture.RefreshViews();

		FAngelscriptTypedASTJITRootIndex Reordered;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITRootIndex(
			Fixture.Graph, Reordered)));
		ASSERT_THAT(AreEqual(First.Roots.Num(), Reordered.Roots.Num()));
		for (int32 Index = 0; Index < First.Roots.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(
				First.Roots[Index].FunctionKey.Hash
					== Reordered.Roots[Index].FunctionKey.Hash));
		}
		ASSERT_THAT(IsTrue(RepeatedByteHash(0x10)
			== Reordered.Roots[0].FunctionKey.Hash));
		ASSERT_THAT(IsTrue(RepeatedByteHash(0x20)
			== Reordered.Roots[1].FunctionKey.Hash));
		ASSERT_THAT(IsTrue(RepeatedByteHash(0x30)
			== Reordered.Roots[2].FunctionKey.Hash));
		ASSERT_THAT(IsTrue(RepeatedByteHash(0x40)
			== Reordered.Roots[3].FunctionKey.Hash));
	}

	TEST_METHOD(MalformedDescriptorGraphFailsClosedWithStableDiagnostics)
	{
		using namespace AngelscriptTypedASTJITEligibilityTests_Private;
		FGraphFixture MissingGraph = MakeCompleteGraphFixture();
		MissingGraph.ResolvedModuleDescriptors.Reset();
		MissingGraph.RefreshViews();
		FAngelscriptTypedASTJITRootIndex RootIndex;
		FString Error;
		ASSERT_THAT(IsFalse(BuildAngelscriptTypedASTJITRootIndex(
			MissingGraph.Graph, RootIndex, &Error)));
		ASSERT_THAT(IsTrue(Error.StartsWith(
			TEXT("TypedASTJITDescriptorGraphMissing:"))));
		ASSERT_THAT(IsTrue(RootIndex.Roots.IsEmpty()));

		FGraphFixture IdentityMismatch = MakeCompleteGraphFixture();
		FAngelscriptFunctionDesc& FirstResolvedRoot =
			IdentityMismatch.OwnedModules[0]->Classes[0]->Methods[0].Get();
		FirstResolvedRoot.ScriptFunction = FunctionHandle(0x2400);
		IdentityMismatch.RefreshViews();
		Error.Reset();
		ASSERT_THAT(IsFalse(BuildAngelscriptTypedASTJITRootIndex(
			IdentityMismatch.Graph, RootIndex, &Error)));
		ASSERT_THAT(IsTrue(Error.StartsWith(
			TEXT("TypedASTJITRootIdentityMismatch:"))));
		ASSERT_THAT(IsTrue(RootIndex.Roots.IsEmpty()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
