#include "as_typed_semantic_ir.h"
#include "as_builder.h"
#include "as_objecttype.h"
#include "as_parser.h"
#include "as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	void MessageCallback(const asSMessageInfo* Message, void*)
	{
		std::cerr
			<< (Message->section != nullptr ? Message->section : "<unknown>")
			<< ':' << Message->row << ':' << Message->col
			<< ": "
			<< (Message->message != nullptr ? Message->message : "<no message>")
			<< '\n';
	}

	bool Require(bool Condition, std::string_view Message)
	{
		if (!Condition)
		{
			std::cerr << Message << '\n';
		}
		return Condition;
	}

	std::vector<void*> GParserArenaFreedAllocations;

	void* TrackParserArenaAllocation(
		const std::size_t Size,
		const std::size_t Alignment)
	{
		return FMemory::DefaultMalloc(Size, Alignment);
	}

	void TrackParserArenaFree(void* Address)
	{
		GParserArenaFreedAllocations.push_back(Address);
		FMemory::DefaultFree(Address);
	}

	class FScopedParserArenaTracking
	{
	public:
		FScopedParserArenaTracking()
		{
			GParserArenaFreedAllocations.clear();
			FMemory::SetAllocationFunctions(
				&TrackParserArenaAllocation,
				&TrackParserArenaFree);
		}

		~FScopedParserArenaTracking()
		{
			FMemory::ResetAllocationFunctions();
		}
	};

	asSTypedSemanticSourceSpan MakeSpan()
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = "standalone.as";
		Span.sourceLength = 1;
		return Span;
	}

	void SetTraitBits(asCTypedSemanticFunction& Function, const asDWORD TraitBits)
	{
		Function.header.declaredTraitBits = TraitBits;
		Function.header.traitSummary =
			ClassifyTypedSemanticFunctionTraits(TraitBits);
	}

	asCTypedSemanticFunction MakeFunction()
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int StandaloneTypedHIR(int Value)";

		asSTypedSemanticSymbol Parameter;
		Parameter.kind = asETypedSemanticSymbolKind::Parameter;
		Parameter.name = "Value";
		Parameter.type = asCDataType::CreatePrimitive(ttInt, false);
		Parameter.span = MakeSpan();
		const asTypedSemanticSymbolId ParameterId = Function.AddSymbol(Parameter);

		asSTypedSemanticExpression Read;
		Read.kind = asETypedSemanticExpressionKind::Symbol;
		Read.type = asCDataType::CreatePrimitive(ttInt, false);
		Read.span = MakeSpan();
		Read.symbol = ParameterId;
		const asTypedSemanticExpressionId ReadId = Function.AddExpression(Read);

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = MakeSpan();
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = ReadId;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId = Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = MakeSpan();
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		asSTypedSemanticCleanupPlan EmptyCleanup;
		Function.header.universalCleanupPlan =
			Function.AddCleanupPlan(EmptyCleanup);
		Function.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::VerifiedEmpty;
		Function.header.cleanupPlanCoversAllTransfers = true;
		Function.statements[ReturnId.value].cleanupPlan =
			Function.header.universalCleanupPlan;
		return Function;
	}

	asCTypedSemanticFunction MakeParameterReceiverFunction(
		const asCDataType& ReceiverType,
		const asETypedSemanticReceiverKind ReceiverKind,
		const asDWORD TraitBits)
	{
		asCTypedSemanticFunction Function = MakeFunction();
		Function.symbols[0].type = ReceiverType;
		SetTraitBits(Function, TraitBits);
		Function.header.receiver.kind = ReceiverKind;
		Function.header.receiver.symbol = asTypedSemanticSymbolId(0);
		Function.header.receiver.parameterIndex = 0;
		Function.header.receiver.type = ReceiverType;
		return Function;
	}

	asCTypedSemanticFunction MakeNativeReceiverFunction(
		const asCDataType& ReceiverType)
	{
		asCTypedSemanticFunction Function = MakeFunction();
		asSTypedSemanticSymbol Receiver;
		Receiver.kind = asETypedSemanticSymbolKind::Receiver;
		Receiver.name = "this";
		Receiver.type = ReceiverType;
		Receiver.span = MakeSpan();
		const asTypedSemanticSymbolId ReceiverId = Function.AddSymbol(Receiver);
		Function.header.invocationKind =
			asETypedSemanticInvocationKind::InstanceMethod;
		Function.header.receiver.kind =
			asETypedSemanticReceiverKind::NativeObjectThis;
		Function.header.receiver.symbol = ReceiverId;
		Function.header.receiver.parameterIndex = -1;
		Function.header.receiver.type = ReceiverType;
		return Function;
	}

	bool HasInvalidReceiver(const asCTypedSemanticFunction& Function)
	{
		return VerifyTypedSemanticFunction(Function).error
			== asETypedSemanticVerificationError::InvalidEffectiveReceiver;
	}

	struct FTraitClassificationCase
	{
		const char* Name;
		asEFuncTrait Trait;
		asDWORD SourcePolicyBits;
		asDWORD ReceiverBits;
		asDWORD InvocationBits;
		asDWORD DispatchBits;
		asDWORD BodyAvailabilityBits;
		asDWORD LifetimeBits;
		asDWORD ProfileBits;
	};

	bool RequireCapturedReceiver(
		asIScriptFunction* PublicFunction,
		asITypeInfo* ExpectedObjectType,
		const asETypedSemanticReceiverKind ExpectedReceiverKind,
		const asETypedSemanticInvocationKind ExpectedInvocationKind,
		const asETypedSemanticSymbolKind ExpectedSymbolKind,
		const int ExpectedParameterIndex,
		std::string_view MissingMessage,
		std::string_view ShapeMessage)
	{
		if (!Require(PublicFunction != nullptr, MissingMessage))
		{
			return false;
		}

		asCScriptFunction* const Function =
			static_cast<asCScriptFunction*>(PublicFunction);
		const asCTypedSemanticFunction* const HIR =
			Function->GetTypedSemanticFunction();
		if (!Require(HIR != nullptr, MissingMessage))
		{
			return false;
		}

		const asSTypedSemanticEffectiveReceiver& Receiver =
			HIR->header.receiver;
		const bool bReceiverSymbolValid =
			Receiver.symbol.IsValid()
			&& Receiver.symbol.value < HIR->symbols.GetLength();
		const bool bDeclaredParameterShapeMatches =
			ExpectedParameterIndex < 0
				? Function->GetParamCount() == 0
				: Function->GetParamCount()
					> static_cast<asUINT>(ExpectedParameterIndex)
					&& Function->parameterTypes[ExpectedParameterIndex]
						== Receiver.type;
		const bool bShapeMatches =
			HIR->header.invocationKind == ExpectedInvocationKind
			&& Receiver.kind == ExpectedReceiverKind
			&& Receiver.parameterIndex == ExpectedParameterIndex
			&& Receiver.symbol.value == 0
			&& Receiver.type.IsObject()
			&& Receiver.type.GetTypeInfo() == ExpectedObjectType
			&& bReceiverSymbolValid
			&& bDeclaredParameterShapeMatches
			&& HIR->symbols[Receiver.symbol.value].kind == ExpectedSymbolKind
			&& HIR->symbols[Receiver.symbol.value].type == Receiver.type
			&& VerifyTypedSemanticFunction(*HIR).IsValid();
		return Require(bShapeMatches, ShapeMessage);
	}

	const asSTypedSemanticSymbol* FindParameterSymbol(
		const asCTypedSemanticFunction& Function,
		const char* Name)
	{
		for (asUINT SymbolIndex = 0;
			SymbolIndex < Function.symbols.GetLength();
			++SymbolIndex)
		{
			const asSTypedSemanticSymbol& Candidate =
				Function.symbols[SymbolIndex];
			if (Candidate.kind == asETypedSemanticSymbolKind::Parameter
				&& Candidate.name == Name)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	const asSTypedSemanticExpression* FindUnsupportedExpression(
		const asCTypedSemanticFunction& Function,
		const asETypedSemanticUnsupportedKind Kind)
	{
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < Function.expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Candidate =
				Function.expressions[ExpressionIndex];
			if (Candidate.kind
					== asETypedSemanticExpressionKind::Unsupported
				&& Candidate.unsupportedKind == Kind)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	constexpr std::string_view ExpectedDump = R"HIR(function int StandaloneTypedHIR(int Value)
header traits=0 traitKnown=0 traitUnknown=0 traitPolicy=0 traitReceiver=0 traitInvocation=0 traitDispatch=0 traitBody=0 traitLifetime=0 traitProfile=0 invocation=Global receiver=None receiverSymbol=none callingModule=none bodyAvailability=Unspecified bodyOwnership=Unspecified bodyOwner=none receiverParameter=-1 receiverType=none compileOut=CompileCalls hiddenArgument=-1 determinesOutputType=-1 returnsOnStack=false suspend=false cleanup=false cleanupState=VerifiedEmpty cleanupPlan=C0 allTransfers=true
root=T1
symbols:
  S0 kind=Parameter name=Value type=int span=standalone.as@0+1
expressions:
  E0 kind=Symbol type=int symbol=S0 span=standalone.as@0+1
statements:
  T0 kind=Return value=E0 cleanup=C0 exitedScopes=1 safePoint=Return span=standalone.as@0+1
  T1 kind=Block children=[T0] span=standalone.as@0+1
)HIR";
}

int main()
{
	bool bPassed = true;
	bPassed &= Require(!asTypedSemanticSymbolId().IsValid(),
		"default symbol ID should be invalid");
	bPassed &= Require(!asTypedSemanticExpressionId().IsValid(),
		"default expression ID should be invalid");
	bPassed &= Require(!asTypedSemanticStatementId().IsValid(),
		"default statement ID should be invalid");

	const asCTypedSemanticFunction First = MakeFunction();
	const asCTypedSemanticFunction Second = MakeFunction();
	bPassed &= Require(VerifyTypedSemanticFunction(First).IsValid(),
		"standalone should verify the same private typed HIR model");
	const asCString FirstDump = DumpTypedSemanticFunction(First);
	const asCString SecondDump = DumpTypedSemanticFunction(Second);
	bPassed &= Require(FirstDump == SecondDump,
		"standalone typed HIR dumps should be deterministic");
	bPassed &= Require(std::string_view(FirstDump.AddressOf()) == ExpectedDump,
		"standalone typed HIR dump should match its exact normalized oracle");
	bPassed &= Require(std::string(FirstDump.AddressOf()).find("0x") == std::string::npos,
		"standalone typed HIR dump should be pointer-free");

	asIScriptEngine* const ParserEngineInterface = asCreateScriptEngine();
	bPassed &= Require(ParserEngineInterface != nullptr,
		"parser lifetime fixture should create its test-owned Engine");
	if (ParserEngineInterface != nullptr)
	{
		asCBuilder ParserBuilder(
			static_cast<asCScriptEngine*>(ParserEngineInterface),
			nullptr);
		asCScriptCode ParserSource;
		const char* const ParserDeclaration =
			"int ParserLifetime(int Value)";
		bPassed &= Require(
			ParserSource.SetCode(
				"typed_parser_lifetime.as",
				ParserDeclaration,
				false) >= 0,
			"parser lifetime fixture should own a valid declaration source");

		asCScriptNode* ParserNode = nullptr;
		const asCTypedSemanticFunction IndependentHIR = MakeFunction();
		const asCString IndependentDumpBeforeParserDestruction =
			DumpTypedSemanticFunction(IndependentHIR);
		{
			FScopedParserArenaTracking Tracking;
			{
				asCParser Parser(&ParserBuilder);
				bPassed &= Require(
					Parser.ParseFunctionDefinition(&ParserSource, false) >= 0,
					"parser lifetime fixture should parse its declaration");
				ParserNode = Parser.GetScriptNode();
				bPassed &= Require(ParserNode != nullptr,
					"parser should expose its transient raw syntax root while alive");
				bPassed &= Require(
					std::find(
						GParserArenaFreedAllocations.begin(),
						GParserArenaFreedAllocations.end(),
						ParserNode) == GParserArenaFreedAllocations.end(),
					"parser syntax root should remain live until parser destruction");
			}

			bPassed &= Require(
				std::find(
					GParserArenaFreedAllocations.begin(),
					GParserArenaFreedAllocations.end(),
					ParserNode) != GParserArenaFreedAllocations.end(),
				"parser destruction should invalidate its FMemStack-owned syntax root");
		}

		bPassed &= Require(
			VerifyTypedSemanticFunction(IndependentHIR).IsValid()
				&& DumpTypedSemanticFunction(IndependentHIR)
					== IndependentDumpBeforeParserDestruction,
			"independently owned verified HIR should survive parser syntax destruction");
		ParserEngineInterface->ShutDownAndRelease();
	}

	const FTraitClassificationCase TraitCases[] = {
		{ "CONSTRUCTOR", asTRAIT_CONSTRUCTOR,
			0, 0, asTRAIT_CONSTRUCTOR, 0, 0,
			asTRAIT_CONSTRUCTOR, 0 },
		{ "DESTRUCTOR", asTRAIT_DESTRUCTOR,
			0, 0, asTRAIT_DESTRUCTOR, 0, 0,
			asTRAIT_DESTRUCTOR, 0 },
		{ "CONST", asTRAIT_CONST,
			0, asTRAIT_CONST, 0, 0, 0, 0, 0 },
		{ "PRIVATE", asTRAIT_PRIVATE,
			asTRAIT_PRIVATE, 0, 0, 0, 0, 0, 0 },
		{ "PROTECTED", asTRAIT_PROTECTED,
			asTRAIT_PROTECTED, 0, 0, 0, 0, 0, 0 },
		{ "FINAL", asTRAIT_FINAL,
			0, 0, 0, asTRAIT_FINAL, 0, 0, 0 },
		{ "OVERRIDE", asTRAIT_OVERRIDE,
			0, 0, 0, asTRAIT_OVERRIDE, 0, 0, 0 },
		{ "SHARED", asTRAIT_SHARED,
			0, 0, 0, asTRAIT_SHARED, asTRAIT_SHARED, 0, 0 },
		{ "EXTERNAL", asTRAIT_EXTERNAL,
			0, 0, 0, asTRAIT_EXTERNAL, asTRAIT_EXTERNAL, 0, 0 },
		{ "PROPERTY", asTRAIT_PROPERTY,
			asTRAIT_PROPERTY, 0, 0, asTRAIT_PROPERTY, 0, 0, 0 },
		{ "IMPLICITCONSTRUCTOR", asTRAIT_IMPLICITCONSTRUCTOR,
			0, 0, asTRAIT_IMPLICITCONSTRUCTOR, 0, 0,
			asTRAIT_IMPLICITCONSTRUCTOR, 0 },
		{ "MIXIN", asTRAIT_MIXIN,
			0, asTRAIT_MIXIN, asTRAIT_MIXIN, asTRAIT_MIXIN, 0, 0, 0 },
		{ "LOCAL", asTRAIT_LOCAL,
			0, 0, 0, asTRAIT_LOCAL, asTRAIT_LOCAL, 0, 0 },
		{ "NODISCARD", asTRAIT_NODISCARD,
			asTRAIT_NODISCARD, 0, 0, 0, 0, 0, 0 },
		{ "DEPRECATED", asTRAIT_DEPRECATED,
			asTRAIT_DEPRECATED, 0, 0, 0, 0, 0, 0 },
		{ "GENERIC_TEMPLATE_FUNCTION", asTRAIT_GENERIC_TEMPLATE_FUNCTION,
			0, 0, 0, asTRAIT_GENERIC_TEMPLATE_FUNCTION,
			asTRAIT_GENERIC_TEMPLATE_FUNCTION, 0, 0 },
		{ "USES_WORLDCONTEXT", asTRAIT_USES_WORLDCONTEXT,
			0, 0, 0, asTRAIT_USES_WORLDCONTEXT, 0, 0,
			asTRAIT_USES_WORLDCONTEXT },
		{ "ACCEPT_TEMPORARY_OBJECT", asTRAIT_ACCEPT_TEMPORARY_OBJECT,
			0, asTRAIT_ACCEPT_TEMPORARY_OBJECT, 0, 0, 0,
			asTRAIT_ACCEPT_TEMPORARY_OBJECT, 0 },
		{ "GENERATED_FUNCTION", asTRAIT_GENERATED_FUNCTION,
			0, 0, 0, 0, asTRAIT_GENERATED_FUNCTION, 0,
			asTRAIT_GENERATED_FUNCTION },
		{ "NOT_CALLABLE", asTRAIT_NOT_CALLABLE,
			asTRAIT_NOT_CALLABLE, 0, 0, asTRAIT_NOT_CALLABLE, 0, 0, 0 },
		{ "FORCE_CONST_ARGUMENT_EXPRESSIONS",
			asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS,
			asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, 0, 0, 0, 0, 0, 0 },
		{ "EXTERNAL_IMPLICIT_THIS", asTRAIT_EXTERNAL_IMPLICIT_THIS,
			0, asTRAIT_EXTERNAL_IMPLICIT_THIS,
			asTRAIT_EXTERNAL_IMPLICIT_THIS, 0, 0, 0, 0 },
		{ "ALLOWDISCARD", asTRAIT_ALLOWDISCARD,
			asTRAIT_ALLOWDISCARD, 0, 0, 0, 0, 0, 0 },
		{ "EDITOR_ONLY", asTRAIT_EDITOR_ONLY,
			0, 0, 0, 0, asTRAIT_EDITOR_ONLY, 0, asTRAIT_EDITOR_ONLY },
		{ "EXPLICIT", asTRAIT_EXPLICIT,
			asTRAIT_EXPLICIT, 0, 0, 0, 0, 0, 0 },
		{ "UNSAFE_DURING_CONSTRUCTION", asTRAIT_UNSAFE_DURING_CONSTRUCTION,
			0, 0, 0, 0, 0, asTRAIT_UNSAFE_DURING_CONSTRUCTION,
			asTRAIT_UNSAFE_DURING_CONSTRUCTION },
		{ "DEFAULTS_ONLY", asTRAIT_DEFAULTS_ONLY,
			0, 0, 0, 0, 0, asTRAIT_DEFAULTS_ONLY,
			asTRAIT_DEFAULTS_ONLY },
	};
	asDWORD VisitedTraitBits = 0;
	for (const FTraitClassificationCase& TraitCase : TraitCases)
	{
		const asDWORD TraitBit = static_cast<asDWORD>(TraitCase.Trait);
		const asSTypedSemanticFunctionTraitSummary Summary =
			ClassifyTypedSemanticFunctionTraits(TraitBit);
		const bool bMatches =
			Summary.knownTraitBits == TraitBit
			&& Summary.unknownTraitBits == 0
			&& Summary.sourcePolicyTraitBits == TraitCase.SourcePolicyBits
			&& Summary.receiverTraitBits == TraitCase.ReceiverBits
			&& Summary.invocationTraitBits == TraitCase.InvocationBits
			&& Summary.dispatchTraitBits == TraitCase.DispatchBits
			&& Summary.bodyAvailabilityTraitBits
				== TraitCase.BodyAvailabilityBits
			&& Summary.lifetimeTraitBits == TraitCase.LifetimeBits
			&& Summary.profileTraitBits == TraitCase.ProfileBits;
		const std::string Message = std::string("trait classification mismatch: ")
			+ TraitCase.Name;
		bPassed &= Require(bMatches, Message);
		VisitedTraitBits |= TraitBit;
	}
	bPassed &= Require(
		VisitedTraitBits == asTYPED_SEMANTIC_KNOWN_FUNC_TRAIT_BITS,
		"trait table should cover every and only current asEFuncTrait bit");
	constexpr asDWORD UnknownTraitBit = 0x80000000u;
	const asSTypedSemanticFunctionTraitSummary UnknownTraitSummary =
		ClassifyTypedSemanticFunctionTraits(UnknownTraitBit);
	bPassed &= Require(
		UnknownTraitSummary.knownTraitBits == 0
			&& UnknownTraitSummary.unknownTraitBits == UnknownTraitBit
			&& UnknownTraitSummary.sourcePolicyTraitBits == 0
			&& UnknownTraitSummary.receiverTraitBits == 0
			&& UnknownTraitSummary.invocationTraitBits == 0
			&& UnknownTraitSummary.dispatchTraitBits == 0
			&& UnknownTraitSummary.bodyAvailabilityTraitBits == 0
			&& UnknownTraitSummary.lifetimeTraitBits == 0
			&& UnknownTraitSummary.profileTraitBits == 0,
		"unknown trait bits should remain visible without entering a known category");
	asCTypedSemanticFunction UnknownTraitFunction = MakeFunction();
	SetTraitBits(UnknownTraitFunction, UnknownTraitBit);
	bPassed &= Require(
		VerifyTypedSemanticFunction(UnknownTraitFunction).IsValid(),
		"unknown traits should remain structurally valid HIR for later eligibility fail-closed handling");
	asCTypedSemanticFunction InvalidTraitSummary = MakeFunction();
	InvalidTraitSummary.header.traitSummary.knownTraitBits = asTRAIT_CONST;
	bPassed &= Require(
		VerifyTypedSemanticFunction(InvalidTraitSummary).error
			== asETypedSemanticVerificationError::InvalidFunctionTraitSummary,
		"verifier should reject a trait summary inconsistent with raw traits");

	asCTypedSemanticFunction Invalid = MakeFunction();
	Invalid.statements[0].valueExpression = asTypedSemanticExpressionId(7);
	bPassed &= Require(
		VerifyTypedSemanticFunction(Invalid).error
			== asETypedSemanticVerificationError::DanglingExpressionId,
		"standalone verifier should reject a dangling expression ID");

	asCTypedSemanticFunction NonBlockRoot = MakeFunction();
	NonBlockRoot.rootStatement = asTypedSemanticStatementId(0);
	bPassed &= Require(
		VerifyTypedSemanticFunction(NonBlockRoot).error
			== asETypedSemanticVerificationError::RootIsNotBlock,
		"standalone verifier should reject a non-block root");

	asCTypedSemanticFunction MissingUnsupportedKind = MakeFunction();
	MissingUnsupportedKind.expressions[0].kind = asETypedSemanticExpressionKind::Unsupported;
	bPassed &= Require(
		VerifyTypedSemanticFunction(MissingUnsupportedKind).error
			== asETypedSemanticVerificationError::MissingUnsupportedCategory,
		"standalone verifier should require an unsupported category");

	asIScriptEngine* ReceiverEngineInterface = asCreateScriptEngine();
	bPassed &= Require(ReceiverEngineInterface != nullptr,
		"receiver verifier fixture should create its test-owned Engine");
	if (ReceiverEngineInterface == nullptr)
	{
		return 1;
	}
	asCScriptEngine* const ReceiverEngine =
		static_cast<asCScriptEngine*>(ReceiverEngineInterface);
	{
		asCObjectType ReceiverObjectType(ReceiverEngine);
		ReceiverObjectType.name = "FReceiver";
		ReceiverObjectType.flags = asOBJ_REF;
		asCObjectType OtherObjectType(ReceiverEngine);
		OtherObjectType.name = "FOtherReceiver";
		OtherObjectType.flags = asOBJ_REF;
		const asCDataType ReceiverType =
			asCDataType::CreateType(&ReceiverObjectType, false);
		const asCDataType OtherType =
			asCDataType::CreateType(&OtherObjectType, false);
		const asCDataType IntType =
			asCDataType::CreatePrimitive(ttInt, false);

		const asCTypedSemanticFunction NativeReceiver =
			MakeNativeReceiverFunction(ReceiverType);
		bPassed &= Require(
			VerifyTypedSemanticFunction(NativeReceiver).IsValid(),
			"native-object this should be a synthetic non-parameter receiver");
		const asCTypedSemanticFunction ExternalReceiver =
			MakeParameterReceiverFunction(
				ReceiverType,
				asETypedSemanticReceiverKind::ExternalImplicitThis,
				asTRAIT_EXTERNAL_IMPLICIT_THIS);
		bPassed &= Require(
			VerifyTypedSemanticFunction(ExternalReceiver).IsValid(),
			"external implicit this should alias object parameter zero");
		const asCTypedSemanticFunction MixinReceiver =
			MakeParameterReceiverFunction(
				ReceiverType,
				asETypedSemanticReceiverKind::MixinFirstParameter,
				asTRAIT_MIXIN);
		bPassed &= Require(
			VerifyTypedSemanticFunction(MixinReceiver).IsValid(),
			"mixin receiver should alias its real object parameter zero");

		asCTypedSemanticFunction MissingParameterZero = ExternalReceiver;
		MissingParameterZero.header.receiver.parameterIndex = -1;
		bPassed &= Require(HasInvalidReceiver(MissingParameterZero),
			"external implicit this without parameter zero should fail closed");

		asCTypedSemanticFunction NonObjectParameterZero =
			MakeParameterReceiverFunction(
				IntType,
				asETypedSemanticReceiverKind::ExternalImplicitThis,
				asTRAIT_EXTERNAL_IMPLICIT_THIS);
		bPassed &= Require(HasInvalidReceiver(NonObjectParameterZero),
			"external implicit this with primitive parameter zero should fail closed");

		asCTypedSemanticFunction WrongParameterIndex = ExternalReceiver;
		WrongParameterIndex.header.receiver.parameterIndex = 1;
		bPassed &= Require(HasInvalidReceiver(WrongParameterIndex),
			"parameter receiver with a nonzero parameter index should fail closed");

		asCTypedSemanticFunction DanglingReceiverSymbol = ExternalReceiver;
		DanglingReceiverSymbol.header.receiver.symbol =
			asTypedSemanticSymbolId(41);
		bPassed &= Require(HasInvalidReceiver(DanglingReceiverSymbol),
			"receiver symbol outside the owned arena should fail closed");

		asCTypedSemanticFunction MismatchedReceiverType = ExternalReceiver;
		MismatchedReceiverType.header.receiver.type = OtherType;
		bPassed &= Require(HasInvalidReceiver(MismatchedReceiverType),
			"receiver type inconsistent with its parameter symbol should fail closed");

		asCTypedSemanticFunction NativeAsDeclaredParameter = NativeReceiver;
		NativeAsDeclaredParameter.header.receiver.symbol = asTypedSemanticSymbolId(0);
		NativeAsDeclaredParameter.header.receiver.parameterIndex = 0;
		bPassed &= Require(HasInvalidReceiver(NativeAsDeclaredParameter),
			"native object this must not be inserted as a declared parameter");

		asCTypedSemanticFunction MissingExternalTrait = ExternalReceiver;
		SetTraitBits(MissingExternalTrait, 0);
		bPassed &= Require(HasInvalidReceiver(MissingExternalTrait),
			"external receiver without its authoritative trait should fail closed");

		asCTypedSemanticFunction ExternalTraitWithoutReceiver = MakeFunction();
		SetTraitBits(ExternalTraitWithoutReceiver,
			asTRAIT_EXTERNAL_IMPLICIT_THIS);
		bPassed &= Require(HasInvalidReceiver(ExternalTraitWithoutReceiver),
			"external-this trait without its normalized receiver should fail closed");

		asCTypedSemanticFunction MissingMixinTrait = MixinReceiver;
		SetTraitBits(MissingMixinTrait, 0);
		bPassed &= Require(HasInvalidReceiver(MissingMixinTrait),
			"mixin receiver without its authoritative trait should fail closed");

		asCTypedSemanticFunction MixinTraitWithoutReceiver = MakeFunction();
		SetTraitBits(MixinTraitWithoutReceiver, asTRAIT_MIXIN);
		bPassed &= Require(HasInvalidReceiver(MixinTraitWithoutReceiver),
			"mixin trait without its normalized parameter receiver should fail closed");

		asCTypedSemanticFunction ConflictingParameterReceiverTraits =
			ExternalReceiver;
		SetTraitBits(ConflictingParameterReceiverTraits,
			asTRAIT_EXTERNAL_IMPLICIT_THIS | asTRAIT_MIXIN);
		bPassed &= Require(HasInvalidReceiver(ConflictingParameterReceiverTraits),
			"external-this and mixin receiver traits must remain distinct");

		asCTypedSemanticFunction NativeGlobalInvocation = NativeReceiver;
		NativeGlobalInvocation.header.invocationKind =
			asETypedSemanticInvocationKind::Global;
		bPassed &= Require(HasInvalidReceiver(NativeGlobalInvocation),
			"native object this on a global invocation should fail closed");

		asCTypedSemanticFunction InstanceWithoutReceiver = MakeFunction();
		InstanceWithoutReceiver.header.invocationKind =
			asETypedSemanticInvocationKind::InstanceMethod;
		bPassed &= Require(HasInvalidReceiver(InstanceWithoutReceiver),
			"instance invocation without a native receiver should fail closed");
	}
	ReceiverEngineInterface->ShutDownAndRelease();

	asIScriptEngine* CaptureEngineInterface = asCreateScriptEngine();
	bPassed &= Require(CaptureEngineInterface != nullptr,
		"receiver capture fixture should create its test-owned Engine");
	if (CaptureEngineInterface != nullptr)
	{
		CaptureEngineInterface->SetMessageCallback(
			asFUNCTION(MessageCallback),
			nullptr,
			asCALL_CDECL);
		asCScriptEngine* const CaptureEngine =
			static_cast<asCScriptEngine*>(CaptureEngineInterface);
		bPassed &= Require(
			!CaptureEngine->IsTypedSemanticIRCaptureEnabled(),
			"typed semantic capture should default off on a new Engine");
		bPassed &= Require(
			CaptureEngine->SetTypedSemanticIRCapture(true),
			"typed semantic capture should be configurable before the first source build");
		asIScriptModule* const Module = CaptureEngineInterface->GetModule(
			"TypedSemanticReceiverCapture",
			asGM_ALWAYS_CREATE);
		const char* const Source = R"AS(
class Receiver
{
	int NativeMethod()
	{
		return 7;
	}
}

int ExternalReceiver(Receiver&in Self) external_implicit_this
{
	return 7;
}

mixin int MixinReceiver(Receiver&in Self)
{
	return 7;
}

int UnsupportedTernary(bool Condition)
{
	return Condition ? 1 : 2;
}

class UnsupportedTarget
{
}

int UnsupportedReference(int&in Value)
{
	return 7;
}

int UnsupportedHandle(UnsupportedTarget Target)
{
	return 9;
}
)AS";
		const bool bSectionAdded = Module != nullptr
			&& Module->AddScriptSection(
				"typed_receiver_capture.as",
				Source) >= 0;
		bPassed &= Require(bSectionAdded,
			"receiver capture fixture should add its source section");
		const bool bBuilt = bSectionAdded && Module->Build() >= 0;
		bPassed &= Require(bBuilt,
			"receiver capture fixture should compile valid source");
		bPassed &= Require(
			!CaptureEngine->SetTypedSemanticIRCapture(false),
			"typed semantic capture should reject changes after the first build starts");
		bPassed &= Require(
			CaptureEngine->IsTypedSemanticIRCaptureEnabled(),
			"a rejected post-build change should preserve the frozen capture profile");
		if (bBuilt)
		{
			asITypeInfo* const ReceiverType =
				Module->GetTypeInfoByName("Receiver");
			bPassed &= Require(ReceiverType != nullptr,
				"receiver capture fixture should publish its script object type");
			if (ReceiverType != nullptr)
			{
				bPassed &= RequireCapturedReceiver(
					ReceiverType->GetMethodByName("NativeMethod"),
					ReceiverType,
					asETypedSemanticReceiverKind::NativeObjectThis,
					asETypedSemanticInvocationKind::InstanceMethod,
					asETypedSemanticSymbolKind::Receiver,
					-1,
					"native method should publish captured typed HIR",
					"native method should capture a synthetic non-parameter receiver");
				bPassed &= RequireCapturedReceiver(
					Module->GetFunctionByName("ExternalReceiver"),
					ReceiverType,
					asETypedSemanticReceiverKind::ExternalImplicitThis,
					asETypedSemanticInvocationKind::Global,
					asETypedSemanticSymbolKind::Parameter,
					0,
					"external implicit-this function should publish captured typed HIR",
					"external implicit this should alias the exact declared parameter zero");
				bPassed &= RequireCapturedReceiver(
					Module->GetFunctionByName("MixinReceiver"),
					ReceiverType,
					asETypedSemanticReceiverKind::MixinFirstParameter,
					asETypedSemanticInvocationKind::Global,
					asETypedSemanticSymbolKind::Parameter,
					0,
					"mixin function should publish captured typed HIR",
					"mixin receiver should alias its distinct declared parameter zero");
			}

			asIScriptFunction* const ReferencePublicFunction =
				Module->GetFunctionByName("UnsupportedReference");
			bPassed &= Require(ReferencePublicFunction != nullptr,
				"reference marker fixture should publish its valid script function");
			if (ReferencePublicFunction != nullptr)
			{
				asCScriptFunction* const ReferenceFunction =
					static_cast<asCScriptFunction*>(ReferencePublicFunction);
				const asCTypedSemanticFunction* const ReferenceHIR =
					ReferenceFunction->GetTypedSemanticFunction();
				bPassed &= Require(ReferenceHIR != nullptr,
					"valid reference source should retain unsupported-but-valid HIR");
				if (ReferenceHIR != nullptr)
				{
					const asSTypedSemanticSymbol* const Parameter =
						FindParameterSymbol(*ReferenceHIR, "Value");
					const asSTypedSemanticExpression* const Marker =
						FindUnsupportedExpression(
							*ReferenceHIR,
							asETypedSemanticUnsupportedKind::Reference);
					const bool bReferenceMarkerMatches =
						Parameter != nullptr
							&& Marker != nullptr
							&& Parameter->type.IsReference()
							&& Marker->symbol == Parameter->id
							&& Marker->type == Parameter->type
							&& Marker->operands.GetLength() == 0
							&& Marker->span.section == Parameter->span.section
							&& Marker->span.sourceOffset
								== Parameter->span.sourceOffset
							&& Marker->span.sourceLength
								== Parameter->span.sourceLength
							&& VerifyTypedSemanticFunction(*ReferenceHIR).IsValid();
					const std::string ReferenceDump(
						DumpTypedSemanticFunction(*ReferenceHIR).AddressOf());
					if (!bReferenceMarkerMatches)
					{
						std::cerr << "reference marker diagnostic:\n"
							<< ReferenceDump;
					}
					bPassed &= Require(
						bReferenceMarkerMatches,
						"reference marker should retain the exact typed parameter span without operands");
					if (Marker != nullptr)
					{
						asCTypedSemanticFunction InvalidReference = *ReferenceHIR;
						InvalidReference.expressions[Marker->id.value].operands.PushLast(
							Marker->id);
						bPassed &= Require(
							VerifyTypedSemanticFunction(InvalidReference).error
								== asETypedSemanticVerificationError::InvalidNodeShape,
							"verifier should reject a reference marker carrying an executable operand");

						asCTypedSemanticFunction DanglingReference = *ReferenceHIR;
						DanglingReference.expressions[Marker->id.value].symbol =
							asTypedSemanticSymbolId();
						bPassed &= Require(
							VerifyTypedSemanticFunction(DanglingReference).error
								== asETypedSemanticVerificationError::DanglingSymbolId,
							"verifier should reject a reference marker without its source symbol");
					}
					bPassed &= Require(
						ReferenceDump.find(
							"kind=Unsupported type=int& unsupported=Reference symbol=S0 operands=[]")
							!= std::string::npos,
						"reference dump should expose the stable typed unsupported category");
				}
			}

			asIScriptFunction* const HandlePublicFunction =
				Module->GetFunctionByName("UnsupportedHandle");
			bPassed &= Require(HandlePublicFunction != nullptr,
				"handle marker fixture should publish its valid script function");
			if (HandlePublicFunction != nullptr)
			{
				asCScriptFunction* const HandleFunction =
					static_cast<asCScriptFunction*>(HandlePublicFunction);
				const asCTypedSemanticFunction* const HandleHIR =
					HandleFunction->GetTypedSemanticFunction();
				bPassed &= Require(HandleHIR != nullptr,
					"valid handle source should retain unsupported-but-valid HIR");
				if (HandleHIR != nullptr)
				{
					const asSTypedSemanticSymbol* const Parameter =
						FindParameterSymbol(*HandleHIR, "Target");
					const asSTypedSemanticExpression* const Marker =
						FindUnsupportedExpression(
							*HandleHIR,
							asETypedSemanticUnsupportedKind::Handle);
					bPassed &= Require(
						Parameter != nullptr
							&& Marker != nullptr
							&& Parameter->type.IsObjectHandle()
							&& Marker->symbol == Parameter->id
							&& Marker->type == Parameter->type
							&& Marker->operands.GetLength() == 0
							&& Marker->span.section == Parameter->span.section
							&& Marker->span.sourceOffset
								== Parameter->span.sourceOffset
							&& Marker->span.sourceLength
								== Parameter->span.sourceLength
							&& VerifyTypedSemanticFunction(*HandleHIR).IsValid(),
						"handle marker should retain the exact typed parameter span without operands");
					if (Marker != nullptr)
					{
						asCTypedSemanticFunction InvalidHandle = *HandleHIR;
						InvalidHandle.expressions[Marker->id.value].type =
							asCDataType::CreatePrimitive(ttInt, false);
						bPassed &= Require(
							VerifyTypedSemanticFunction(InvalidHandle).error
								== asETypedSemanticVerificationError::InvalidNodeShape,
							"verifier should reject a Handle marker whose type is not an object handle");
					}
					bPassed &= Require(
						std::string(DumpTypedSemanticFunction(*HandleHIR).AddressOf()).find(
							"unsupported=Handle symbol=S0 operands=[]")
							!= std::string::npos,
						"handle dump should expose the stable typed unsupported category");
				}
			}

			asIScriptFunction* const TernaryPublicFunction =
				Module->GetFunctionByName("UnsupportedTernary");
			bPassed &= Require(TernaryPublicFunction != nullptr,
				"ternary capture fixture should publish its valid script function");
			if (TernaryPublicFunction != nullptr)
			{
				asCScriptFunction* const TernaryFunction =
					static_cast<asCScriptFunction*>(TernaryPublicFunction);
				const asCTypedSemanticFunction* const TernaryHIR =
					TernaryFunction->GetTypedSemanticFunction();
				bPassed &= Require(TernaryHIR != nullptr,
					"valid ternary source should retain unsupported-but-valid HIR");
				if (TernaryHIR != nullptr)
				{
					bPassed &= Require(
						VerifyTypedSemanticFunction(*TernaryHIR).IsValid(),
						"ternary unsupported marker should remain verifier-valid");

					const asSTypedSemanticExpression* TernaryExpression = nullptr;
					for (asUINT ExpressionIndex = 0;
						ExpressionIndex < TernaryHIR->expressions.GetLength();
						++ExpressionIndex)
					{
						const asSTypedSemanticExpression& Candidate =
							TernaryHIR->expressions[ExpressionIndex];
						if (Candidate.kind
								== asETypedSemanticExpressionKind::Unsupported
							&& Candidate.unsupportedKind
								== asETypedSemanticUnsupportedKind::Ternary)
						{
							TernaryExpression = &Candidate;
							break;
						}
					}
					bPassed &= Require(TernaryExpression != nullptr,
						"compiler should capture a typed Ternary unsupported marker");
					if (TernaryExpression != nullptr)
					{
						const std::string_view SourceView(Source);
						const std::string_view TernaryText = "Condition ? 1 : 2";
						const size_t ExpectedOffset = SourceView.find(TernaryText);
						bPassed &= Require(
							TernaryExpression->type
								== asCDataType::CreatePrimitive(ttInt, false)
							&& TernaryExpression->operands.GetLength() == 3
							&& ExpectedOffset != std::string_view::npos
							&& TernaryExpression->span.section
								== "typed_receiver_capture.as"
							&& TernaryExpression->span.sourceOffset
								== ExpectedOffset
							&& TernaryExpression->span.sourceLength
								== TernaryText.size(),
							"ternary marker should retain exact type, operands, and source span");

						asCTypedSemanticFunction MissingTernaryOperand =
							*TernaryHIR;
						MissingTernaryOperand.expressions[
							TernaryExpression->id.value].operands.SetLength(2);
						bPassed &= Require(
							VerifyTypedSemanticFunction(MissingTernaryOperand).error
								== asETypedSemanticVerificationError::InvalidNodeShape,
							"verifier should reject a ternary marker missing one branch operand");

						asCTypedSemanticFunction ForwardTernaryOperand =
							*TernaryHIR;
						ForwardTernaryOperand.expressions[
							TernaryExpression->id.value].operands[0] =
								TernaryExpression->id;
						bPassed &= Require(
							VerifyTypedSemanticFunction(ForwardTernaryOperand).error
								== asETypedSemanticVerificationError::InvalidNodeShape,
							"verifier should reject a ternary marker with a forward or self operand");
					}

					const std::string TernaryDump(
						DumpTypedSemanticFunction(*TernaryHIR).AddressOf());
					bPassed &= Require(
						TernaryDump.find(
							"kind=Unsupported type=int unsupported=Ternary operands=")
							!= std::string::npos,
						"ternary dump should expose its stable unsupported category and operands");

					const asSTypedSemanticSnapshotResult TernarySnapshot =
						BuildTypedSemanticSnapshot(TernaryHIR);
					const asSTypedSemanticSnapshotResult RepeatedSnapshot =
						BuildTypedSemanticSnapshot(TernaryHIR);
					bPassed &= Require(
						TernarySnapshot.IsValid()
						&& TernarySnapshot.state
							== asETypedSemanticSnapshotState::Verified
						&& TernarySnapshot.text == RepeatedSnapshot.text
						&& TernarySnapshot.json == RepeatedSnapshot.json,
						"verified HIR snapshots should be deterministic in text and JSON");
					const std::string SnapshotText(TernarySnapshot.text.AddressOf());
					const std::string SnapshotJson(TernarySnapshot.json.AddressOf());
					bPassed &= Require(
						SnapshotText.find("RequestKind=TestHIRSnapshot")
							!= std::string::npos
						&& SnapshotText.find("unsupported=Ternary")
							!= std::string::npos
						&& SnapshotJson.find("\"requestKind\":\"TestHIRSnapshot\"")
							!= std::string::npos
						&& SnapshotJson.find("unsupported=Ternary")
							!= std::string::npos
						&& SnapshotText.find("0x") == std::string::npos
						&& SnapshotJson.find("0x") == std::string::npos,
						"snapshot formats should retain unsupported markers without process pointers");

					if (TernaryExpression != nullptr)
					{
						asCTypedSemanticFunction InvalidSnapshotFunction =
							*TernaryHIR;
						InvalidSnapshotFunction.expressions[
							TernaryExpression->id.value].operands.SetLength(2);
						const asSTypedSemanticSnapshotResult InvalidSnapshot =
							BuildTypedSemanticSnapshot(&InvalidSnapshotFunction);
						bPassed &= Require(
							!InvalidSnapshot.IsValid()
							&& InvalidSnapshot.state
								== asETypedSemanticSnapshotState::VerificationFailed
							&& InvalidSnapshot.verification.error
								== asETypedSemanticVerificationError::InvalidNodeShape
							&& std::string(InvalidSnapshot.text.AddressOf()).find(
								"VerificationError=InvalidNodeShape")
								!= std::string::npos
							&& std::string(InvalidSnapshot.json.AddressOf()).find(
								"\"normalizedHIR\":null")
								!= std::string::npos,
							"invalid HIR snapshots should report diagnostics without normalized success output");
					}

					const asSTypedSemanticSnapshotResult MissingSnapshot =
						BuildTypedSemanticSnapshot(nullptr);
					bPassed &= Require(
						!MissingSnapshot.IsValid()
						&& MissingSnapshot.state
							== asETypedSemanticSnapshotState::MissingFunction,
						"snapshot helper should distinguish missing HIR from verifier failure");
				}

				asIScriptContext* const Context =
					CaptureEngineInterface->CreateContext();
				bPassed &= Require(Context != nullptr,
					"ternary fixture should create a VM context");
				if (Context != nullptr)
				{
					for (int ConditionValue = 0; ConditionValue <= 1;
						++ConditionValue)
					{
						const int PrepareResult =
							Context->Prepare(TernaryPublicFunction);
						const int SetArgResult = PrepareResult >= 0
							? Context->SetArgByte(
								0, static_cast<asBYTE>(ConditionValue))
							: PrepareResult;
						const int ExecuteResult = SetArgResult >= 0
							? Context->Execute()
							: SetArgResult;
						const asDWORD ExpectedResult = ConditionValue != 0 ? 1u : 2u;
						bPassed &= Require(
							ExecuteResult == asEXECUTION_FINISHED
							&& Context->GetReturnDWord() == ExpectedResult,
							"capturing unsupported ternary HIR must not change VM behavior");
					}
					Context->Release();
				}
			}
		}
		CaptureEngineInterface->ShutDownAndRelease();
	}

	return bPassed ? 0 : 1;
}
