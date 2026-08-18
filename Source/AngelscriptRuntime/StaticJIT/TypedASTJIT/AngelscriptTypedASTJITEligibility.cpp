#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"

#include "Core/AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#if AS_CAN_GENERATE_JIT

namespace AngelscriptTypedASTJITEligibility_Private
{
	static asSTypedSemanticSourceSpan FunctionSpan(
		const asCTypedSemanticFunction* Function)
	{
		if (Function != nullptr
			&& Function->rootStatement.IsValid()
			&& Function->rootStatement.value < Function->statements.GetLength())
		{
			return Function->statements[Function->rootStatement.value].span;
		}
		if (Function != nullptr && Function->statements.GetLength() != 0)
		{
			return Function->statements[0].span;
		}
		if (Function != nullptr && Function->expressions.GetLength() != 0)
		{
			return Function->expressions[0].span;
		}
		return {};
	}

	static FAngelscriptTypedASTJITEligibility Reject(
		const EAngelscriptTypedASTJITFallbackReason Reason,
		const asSTypedSemanticSourceSpan& Span,
		const FString& Detail)
	{
		FAngelscriptTypedASTJITEligibility Result;
		Result.Reason = Reason;
		Result.SourceSpan = Span;
		Result.Detail = Detail;
		return Result;
	}

	static bool IsByValueScalarOrEnum(
		const asCDataType& Type,
		const bool bAllowVoid)
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

	static bool IsReviewedManagedInput(const asCDataType& Type)
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

	static bool IsReviewedParameterType(const asCDataType& Type)
	{
		return IsByValueScalarOrEnum(Type, false)
			|| IsReviewedManagedInput(Type);
	}

	static int32 FindParameterOrdinal(
		const asCTypedSemanticFunction& Function,
		const asTypedSemanticSymbolId SymbolId)
	{
		int32 Ordinal = 0;
		for (asUINT SymbolIndex = 0;
			SymbolIndex < Function.symbols.GetLength(); ++SymbolIndex)
		{
			const asSTypedSemanticSymbol& Symbol =
				Function.symbols[SymbolIndex];
			if (Symbol.kind != asETypedSemanticSymbolKind::Parameter)
			{
				continue;
			}
			if (Symbol.id == SymbolId)
			{
				return Ordinal;
			}
			++Ordinal;
		}
		return INDEX_NONE;
	}

	static EAngelscriptTypedASTJITFallbackReason UnsupportedKindReason(
		const asETypedSemanticUnsupportedKind Kind,
		const EAngelscriptTypedASTJITFallbackReason DefaultReason)
	{
		switch (Kind)
		{
		case asETypedSemanticUnsupportedKind::ConstructionOrLifetime:
			return EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime;
		case asETypedSemanticUnsupportedKind::ExceptionCleanup:
			return EAngelscriptTypedASTJITFallbackReason::UnsupportedExceptionRegion;
		case asETypedSemanticUnsupportedKind::SuspendPoint:
			return EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState;
		case asETypedSemanticUnsupportedKind::CompilerSynthesizedFunction:
			return EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind;
		default:
			return DefaultReason;
		}
	}

	static FString UFunctionFlagDetail(
		const EAngelscriptTypedASTJITUFunctionFlags Flags)
	{
		TArray<FString> Tokens;
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::RpcOrNet))
			Tokens.Add(TEXT("RpcOrNet"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::BlueprintEvent))
			Tokens.Add(TEXT("BlueprintEvent"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::BlueprintOverride))
			Tokens.Add(TEXT("BlueprintOverride"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::ThreadSafeSpecial))
			Tokens.Add(TEXT("ThreadSafeSpecial"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITUFunctionFlags::WorldContextGenerated))
			Tokens.Add(TEXT("WorldContextGenerated"));
		return FString::Join(Tokens, TEXT("|"));
	}

	static FString SignatureFlagDetail(
		const EAngelscriptTypedASTJITSignatureFlags Flags)
	{
		TArray<FString> Tokens;
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Reference))
			Tokens.Add(TEXT("Reference"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::OutParameter))
			Tokens.Add(TEXT("OutParameter"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::ReturnOnStack))
			Tokens.Add(TEXT("ReturnOnStack"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Object))
			Tokens.Add(TEXT("Object"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Struct))
			Tokens.Add(TEXT("Struct"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Container))
			Tokens.Add(TEXT("Container"));
		if (EnumHasAnyFlags(Flags,
			EAngelscriptTypedASTJITSignatureFlags::Delegate))
			Tokens.Add(TEXT("Delegate"));
		return FString::Join(Tokens, TEXT("|"));
	}

	static void ClassifyType(
		const asCDataType& Type,
		EAngelscriptTypedASTJITSignatureFlags& Flags)
	{
		if (Type.IsReference())
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Reference;
		}
		if (!Type.IsObject() && !Type.IsFuncdef())
		{
			return;
		}
		if (Type.IsFuncdef())
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Delegate;
		}
		else if (Type.IsArrayType()
			|| (Type.GetTypeInfo() != nullptr
				&& (Type.GetTypeInfo()->name == "TArray"
					|| Type.GetTypeInfo()->name == "TSet"
					|| Type.GetTypeInfo()->name == "TMap")))
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Container;
		}
		else if (Type.IsNonPrimitiveValueType())
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Struct;
		}
		else
		{
			Flags |= EAngelscriptTypedASTJITSignatureFlags::Object;
		}
	}

	static bool Fail(
		FAngelscriptTypedASTJITRootIndex& OutIndex,
		FString* OutError,
		const TCHAR* Detail)
	{
		OutIndex = {};
		if (OutError != nullptr)
		{
			*OutError = Detail;
		}
		return false;
	}
}

FString FAngelscriptTypedASTJITEligibility::ToDiagnosticString() const
{
	FString Result = FString::Printf(
		TEXT("%s: %s"), LexToString(Reason), *Detail);
	if (SourceSpan.section.GetLength() != 0)
	{
		Result += FString::Printf(TEXT(" [%s:%u:%u]"),
			UTF8_TO_TCHAR(SourceSpan.section.AddressOf()),
			SourceSpan.row,
			SourceSpan.column);
	}
	return Result;
}

FAngelscriptTypedASTJITEligibility EvaluateAngelscriptTypedASTJITEligibility(
	const FAngelscriptTypedASTJITEligibilityInput& Input)
{
	using namespace AngelscriptTypedASTJITEligibility_Private;
	if (Input.Purpose
			!= EAngelscriptTypedASTJITEligibilityPurpose::InternalHelperCallClosure
		&& !Input.bIsUFunctionRoot)
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::NotUFunctionRoot,
			{}, TEXT("No exact resolved FunctionDesc root exists."));
	}
	if (Input.TypedHIR == nullptr)
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::MissingTypedHIR,
			{}, TEXT("No same-compilation verified typed HIR is available."));
	}

	const asCTypedSemanticFunction& Function = *Input.TypedHIR;
	const asSTypedSemanticVerificationResult Verification =
		VerifyTypedSemanticFunction(Function);
	if (!Verification.IsValid())
	{
		EAngelscriptTypedASTJITFallbackReason Reason =
			EAngelscriptTypedASTJITFallbackReason::InvalidTypedHIR;
		if (Verification.error
			== asETypedSemanticVerificationError::InvalidEffectiveReceiver)
		{
			Reason = EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver;
		}
		else if (Verification.error
			== asETypedSemanticVerificationError::InvalidCleanupPlan)
		{
			Reason = EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan;
		}
		else if (Verification.error
			== asETypedSemanticVerificationError::InvalidCallEvaluationSequence)
		{
			Reason = EAngelscriptTypedASTJITFallbackReason::InvalidCallEvaluationSequence;
		}
		return Reject(Reason,
			Verification.span.section.GetLength() != 0
				? Verification.span : FunctionSpan(&Function),
			UTF8_TO_TCHAR(Verification.detail.AddressOf()));
	}

	const asSTypedSemanticSourceSpan RootSpan = FunctionSpan(&Function);
	if (Function.header.declaredTraitBits != Input.RawTraitBits)
	{
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch,
			RootSpan, TEXT("Resolved trait bits disagree with captured HIR."));
	}
	if (Function.header.invocationKind != Input.Shape.InvocationKind)
	{
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::SemanticDependencyMismatch,
			RootSpan, TEXT("Resolved invocation kind disagrees with captured HIR."));
	}
	if (Function.header.invocationKind
		== asETypedSemanticInvocationKind::GlobalInitializer)
	{
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::
				UnsupportedGlobalInitializer,
			RootSpan,
			TEXT("The initial TypedASTJIT profile does not emit global initializer bodies."));
	}
	const bool bGlobalInvocation = Function.header.invocationKind
		== asETypedSemanticInvocationKind::Global;
	const bool bFinalInstanceInvocation = Function.header.invocationKind
		== asETypedSemanticInvocationKind::InstanceMethod;
	if (!bGlobalInvocation && !bFinalInstanceInvocation)
	{
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionKind,
			RootSpan,
			TEXT("The scalar profile accepts ordinary global functions and final non-virtual instance methods only."));
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
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedFunctionTrait,
			RootSpan,
			FString::Printf(TEXT(
				"RawTraits=0x%08x Unknown=0x%08x Unsupported=0x%08x VirtualOrNonFinal=%s"),
				Input.RawTraitBits, UnknownTraitBits, RejectedTraitBits,
				Input.bVirtualOrNonFinal ? TEXT("true") : TEXT("false")));
	}
	if (Input.UFunctionFlags
		!= EAngelscriptTypedASTJITUFunctionFlags::None)
	{
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedUFunctionFlags,
			RootSpan, UFunctionFlagDetail(Input.UFunctionFlags));
	}
	if (Function.header.receiver.kind != Input.Shape.ReceiverKind)
	{
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::InvalidEffectiveReceiver,
			RootSpan, TEXT("Resolved receiver disagrees with captured HIR."));
	}
	if (bGlobalInvocation
		&& (Input.Shape.ReceiverKind
				!= asETypedSemanticReceiverKind::None
			|| Input.Shape.ReceiverParameterIndex != INDEX_NONE
			|| Input.Shape.bHasNativeObjectSlot))
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
			RootSpan,
			TEXT("A global scalar root must not carry an effective receiver."));
	}
	if (bFinalInstanceInvocation
		&& (Input.Shape.ReceiverKind
				!= asETypedSemanticReceiverKind::NativeObjectThis
			|| Input.Shape.ReceiverParameterIndex != INDEX_NONE
			|| !Input.Shape.bHasNativeObjectSlot))
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
			RootSpan,
			TEXT("A final instance scalar root requires the exact native object-this slot."));
	}
	if (bFinalInstanceInvocation)
	{
		const asTypedSemanticSymbolId ReceiverSymbol =
			Function.header.receiver.symbol;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < Function.expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				Function.expressions[ExpressionIndex];
			if (Expression.symbol.IsValid()
				&& Expression.symbol == ReceiverSymbol)
			{
				return Reject(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver,
					Expression.span,
					TEXT("The reviewed instance slice preserves object identity but does not read native receiver storage."));
			}
		}
	}

	const bool bHasReviewedManagedInput =
		Input.Shape.ParameterTypes.ContainsByPredicate(
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
		|| Function.header.returnsOnStack != Input.Shape.bReturnsOnStack)
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
			RootSpan, SignatureFlagDetail(Input.SignatureFlags));
	}
	if (EnumHasAnyFlags(Input.SignatureFlags,
		EAngelscriptTypedASTJITSignatureFlags::Object
		| EAngelscriptTypedASTJITSignatureFlags::Container
		| EAngelscriptTypedASTJITSignatureFlags::Delegate)
		|| (EnumHasAnyFlags(Input.SignatureFlags,
				EAngelscriptTypedASTJITSignatureFlags::Struct)
			&& !bHasReviewedManagedInput))
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
			RootSpan, SignatureFlagDetail(Input.SignatureFlags));
	}
	if (!IsByValueScalarOrEnum(Input.Shape.ReturnType, true))
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
			RootSpan, TEXT("Return type is not a by-value scalar or enum."));
	}
	for (const asCDataType& ParameterType : Input.Shape.ParameterTypes)
	{
		if (!IsReviewedParameterType(ParameterType))
		{
			return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
				RootSpan, TEXT("Parameter type is neither a by-value scalar/enum nor a reviewed const FString/FLinearColor input."));
		}
	}
	int32 ParameterOrdinal = 0;
	for (asUINT SymbolIndex = 0;
		SymbolIndex < Function.symbols.GetLength(); ++SymbolIndex)
	{
		const asSTypedSemanticSymbol& Symbol = Function.symbols[SymbolIndex];
		if (Symbol.kind != asETypedSemanticSymbolKind::Parameter)
		{
			continue;
		}
		if (ParameterOrdinal >= Input.Shape.ParameterTypes.Num()
			|| !Symbol.type.IsEqualExceptRefAndConst(
				Input.Shape.ParameterTypes[ParameterOrdinal]))
		{
			return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
				Symbol.span, TEXT("HIR parameter order/type disagrees with the resolved signature."));
		}
		++ParameterOrdinal;
	}
	if (ParameterOrdinal != Input.Shape.ParameterTypes.Num())
	{
		return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedSignature,
			RootSpan, TEXT("HIR parameter count disagrees with the resolved signature."));
	}

	if (Function.header.hasSuspendState)
	{
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::SuspendOrExceptionState,
			RootSpan, TEXT("The function carries suspend or resumable state."));
	}
	switch (Function.header.cleanupPlanState)
	{
	case asETypedSemanticCleanupPlanState::VerifiedEmpty:
		break;
	case asETypedSemanticCleanupPlanState::Unverified:
		return Reject(EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan,
			RootSpan, TEXT("The compiler did not verify a cleanup plan."));
	case asETypedSemanticCleanupPlanState::NonEmpty:
	case asETypedSemanticCleanupPlanState::PartialConstruction:
	case asETypedSemanticCleanupPlanState::ScriptDestructor:
		return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedLifetime,
			RootSpan, TEXT("The scalar profile requires an explicitly empty cleanup plan."));
	case asETypedSemanticCleanupPlanState::CompilerExceptionRegion:
		return Reject(
			EAngelscriptTypedASTJITFallbackReason::UnsupportedExceptionRegion,
			RootSpan, TEXT("Compiler-internal exception regions are outside the scalar profile."));
	default:
		return Reject(EAngelscriptTypedASTJITFallbackReason::InvalidCleanupPlan,
			RootSpan, TEXT("Cleanup plan state is outside the supported contract."));
	}

	for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
	{
		const asSTypedSemanticExpression& Expression = Function.expressions[Index];
		switch (Expression.kind)
		{
		case asETypedSemanticExpressionKind::ResolvedCall:
			if (Input.Purpose
				== EAngelscriptTypedASTJITEligibilityPurpose::StandaloneRoot)
			{
				return Reject(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedCall,
					Expression.span,
					TEXT("Resolved calls require whole-root closure planning."));
			}
			break;
		case asETypedSemanticExpressionKind::CallRewrite:
			if (Expression.callRewriteKind
					!= asETypedSemanticCompileOutKind::CompileOutEntirely
				&& !IsByValueScalarOrEnum(Expression.type, false))
			{
				return Reject(
					EAngelscriptTypedASTJITFallbackReason::UnsupportedType,
					Expression.span,
					TEXT("Retained call-rewrite value is outside the reviewed scalar/enum slice."));
			}
			break;
		case asETypedSemanticExpressionKind::FoldedGlobalConstant:
		{
			FString DependencyError;
			if (!ValidateAngelscriptTypedASTJITFoldedGlobalDependency(
					Input.GlobalDependencies,
					Expression.resolvedGlobalPropertyId,
					&DependencyError))
			{
				return Reject(
					EAngelscriptTypedASTJITFallbackReason::
						SemanticDependencyMismatch,
					Expression.span, DependencyError);
			}
			break;
		}
		case asETypedSemanticExpressionKind::GlobalStorage:
			return Reject(
				EAngelscriptTypedASTJITFallbackReason::
					UnsupportedGlobalStorage,
				Expression.span,
				TEXT("The initial TypedASTJIT profile does not read or mutate runtime global storage."));
		case asETypedSemanticExpressionKind::Literal:
		case asETypedSemanticExpressionKind::Symbol:
		case asETypedSemanticExpressionKind::Conversion:
		case asETypedSemanticExpressionKind::Assignment:
		case asETypedSemanticExpressionKind::Unary:
		case asETypedSemanticExpressionKind::Binary:
		case asETypedSemanticExpressionKind::ShortCircuit:
			break;
		case asETypedSemanticExpressionKind::Unsupported:
			if (Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::Reference
				&& Expression.symbol.IsValid()
				&& FindParameterOrdinal(Function, Expression.symbol)
					!= INDEX_NONE
				&& Expression.symbol.value < Function.symbols.GetLength()
				&& Function.symbols[Expression.symbol.value].kind
					== asETypedSemanticSymbolKind::Parameter
				&& IsReviewedManagedInput(Expression.type)
				&& IsReviewedManagedInput(
					Function.symbols[Expression.symbol.value].type)
				&& Expression.type.IsEqualExceptRefAndConst(
					Function.symbols[Expression.symbol.value].type))
			{
				// The maintained HIR intentionally preserves a typed reference
				// marker. This backend separately reviews exact borrowed
				// FString/FLinearColor parameters; every other reference marker
				// remains a deterministic fallback below.
				break;
			}
			return Reject(UnsupportedKindReason(Expression.unsupportedKind,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression),
				Expression.span, TEXT("HIR contains an unsupported expression category."));
		default:
			return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedExpression,
				Expression.span, TEXT("Expression kind is outside the reviewed scalar profile."));
		}
	}
	for (asUINT Index = 0; Index < Function.statements.GetLength(); ++Index)
	{
		const asSTypedSemanticStatement& Statement = Function.statements[Index];
		if (Statement.kind == asETypedSemanticStatementKind::Unsupported)
		{
			return Reject(UnsupportedKindReason(Statement.unsupportedKind,
				EAngelscriptTypedASTJITFallbackReason::UnsupportedStatement),
				Statement.span, TEXT("HIR contains an unsupported statement category."));
		}
		if (Statement.kind != asETypedSemanticStatementKind::Block
			&& Statement.kind != asETypedSemanticStatementKind::LocalDeclaration
			&& Statement.kind != asETypedSemanticStatementKind::Expression
			&& Statement.kind != asETypedSemanticStatementKind::If
			&& Statement.kind != asETypedSemanticStatementKind::Return
			&& Statement.kind != asETypedSemanticStatementKind::For
			&& Statement.kind != asETypedSemanticStatementKind::While
			&& Statement.kind != asETypedSemanticStatementKind::DoWhile
			&& Statement.kind != asETypedSemanticStatementKind::Switch
			&& Statement.kind != asETypedSemanticStatementKind::Case
			&& Statement.kind != asETypedSemanticStatementKind::Break
			&& Statement.kind != asETypedSemanticStatementKind::Continue)
		{
			return Reject(EAngelscriptTypedASTJITFallbackReason::UnsupportedStatement,
				Statement.span, TEXT("Statement kind is outside the reviewed scalar profile."));
		}
	}

	FAngelscriptTypedASTJITEligibility Result;
	Result.bEligible = true;
	Result.SourceSpan = RootSpan;
	Result.Detail = TEXT("Verified scalar return and scalar/enum or reviewed const managed-input signature with an explicit empty cleanup plan.");
	return Result;
}

FAngelscriptTypedASTJITEligibilityInput
BuildAngelscriptTypedASTJITEligibilityInput(
	const FAngelscriptTypedASTJITRoot* Root,
	const FAngelscriptStaticJITCompiledFunctionView& Function)
{
	using namespace AngelscriptTypedASTJITEligibility_Private;
	FAngelscriptTypedASTJITEligibilityInput Input;
	Input.bIsUFunctionRoot = Root != nullptr;
	Input.TypedHIR = Function.VerifiedTypedHIR;
	if (Function.EngineLocalFunction == nullptr)
	{
		return Input;
	}

	const asCScriptFunction* const ScriptFunction =
		static_cast<const asCScriptFunction*>(Function.EngineLocalFunction);
	Input.RawTraitBits = ScriptFunction->traits.traits;
	Input.Shape.SymbolName = Function.FunctionKey.Hash.ToHexString();
	Input.Shape.ReturnType = ScriptFunction->returnType;
	Input.Shape.bReturnsOnStack = ScriptFunction->DoesReturnOnStack();
	for (asUINT Index = 0;
		Index < ScriptFunction->parameterTypes.GetLength(); ++Index)
	{
		Input.Shape.ParameterTypes.Add(ScriptFunction->parameterTypes[Index]);
	}
	if (Input.TypedHIR != nullptr)
	{
		Input.Shape.InvocationKind = Input.TypedHIR->header.invocationKind;
		Input.Shape.ReceiverKind = Input.TypedHIR->header.receiver.kind;
		Input.Shape.ReceiverParameterIndex =
			Input.TypedHIR->header.receiver.parameterIndex;
	}
	Input.Shape.bHasNativeObjectSlot = ScriptFunction->objectType != nullptr;
	Input.bVirtualOrNonFinal =
		ScriptFunction->funcType == asFUNC_VIRTUAL
		|| (ScriptFunction->objectType != nullptr
			&& !ScriptFunction->traits.GetTrait(asTRAIT_FINAL));

	if (Root != nullptr && Root->FunctionDescriptor != nullptr)
	{
		const FAngelscriptFunctionDesc& Desc = *Root->FunctionDescriptor;
		if (Desc.bNetMulticast || Desc.bNetClient || Desc.bNetServer
			|| Desc.bNetValidate || Desc.bUnreliable)
		{
			Input.UFunctionFlags |=
				EAngelscriptTypedASTJITUFunctionFlags::RpcOrNet;
		}
		if (Desc.bBlueprintEvent)
			Input.UFunctionFlags |= EAngelscriptTypedASTJITUFunctionFlags::BlueprintEvent;
		if (Desc.bBlueprintOverride)
			Input.UFunctionFlags |= EAngelscriptTypedASTJITUFunctionFlags::BlueprintOverride;
		if (Desc.bThreadSafe)
			Input.UFunctionFlags |= EAngelscriptTypedASTJITUFunctionFlags::ThreadSafeSpecial;
		if (Desc.Meta.Contains(FName(TEXT("WorldContext")))
			|| ScriptFunction->hiddenArgumentIndex >= 0
			|| ScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT))
		{
			Input.UFunctionFlags |=
				EAngelscriptTypedASTJITUFunctionFlags::WorldContextGenerated;
		}
		for (const FAngelscriptArgumentDesc& Argument : Desc.Arguments)
		{
			if (Argument.bBlueprintOutRef || Argument.bBlueprintInRef)
				Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::Reference;
			if (Argument.bBlueprintOutRef || Argument.bInRefForceCopyOut)
				Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::OutParameter;
		}
	}
	if (ScriptFunction->DoesReturnOnStack())
		Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::ReturnOnStack;
	ClassifyType(ScriptFunction->returnType, Input.SignatureFlags);
	for (asUINT Index = 0;
		Index < ScriptFunction->parameterTypes.GetLength(); ++Index)
	{
		ClassifyType(ScriptFunction->parameterTypes[Index], Input.SignatureFlags);
		if (Index < ScriptFunction->inOutFlags.GetLength()
			&& ScriptFunction->inOutFlags[Index] != asTM_NONE)
		{
			Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::Reference;
			if (ScriptFunction->inOutFlags[Index] == asTM_OUTREF
				|| ScriptFunction->inOutFlags[Index] == asTM_INOUTREF)
			{
				Input.SignatureFlags |= EAngelscriptTypedASTJITSignatureFlags::OutParameter;
			}
		}
	}
	Input.CapabilityDetail = UFunctionFlagDetail(Input.UFunctionFlags);
	return Input;
}

const FAngelscriptTypedASTJITRoot*
FAngelscriptTypedASTJITRootIndex::FindByFunction(
	const asIScriptFunction* Function) const
{
	return Roots.FindByPredicate(
		[Function](const FAngelscriptTypedASTJITRoot& Root)
		{
			return Root.EngineLocalFunction == Function;
		});
}

bool BuildAngelscriptTypedASTJITRootIndex(
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	FAngelscriptTypedASTJITRootIndex& OutIndex,
	FString* OutError)
{
	using namespace AngelscriptTypedASTJITEligibility_Private;
	OutIndex = {};
	if (OutError != nullptr)
	{
		OutError->Reset();
	}

	if (Graph.ResolvedModuleDescriptors.IsEmpty()
		|| Graph.ResolvedModuleDescriptors.Num() != Graph.Modules.Num())
	{
		return Fail(OutIndex, OutError, TEXT(
			"TypedASTJITDescriptorGraphMissing: the complete generation view must carry one resolved ModuleDesc for every compiled module."));
	}

	TMap<const asIScriptFunction*,
		const FAngelscriptStaticJITCompiledFunctionView*> FunctionsByPointer;
	FunctionsByPointer.Reserve(Graph.Functions.Num());
	for (const FAngelscriptStaticJITCompiledFunctionView& Function
		: Graph.Functions)
	{
		if (Function.EngineLocalFunction == nullptr
			|| Function.FunctionKey.Hash.IsZero()
			|| FunctionsByPointer.Contains(Function.EngineLocalFunction))
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITCompiledFunctionGraphInvalid: compiled functions require unique non-null task-local pointers and stable keys."));
		}
		FunctionsByPointer.Add(Function.EngineLocalFunction, &Function);
	}

	TMap<const void*, const FAngelscriptStaticJITDescriptorView*>
		DescriptorsByPointer;
	DescriptorsByPointer.Reserve(Graph.Descriptors.Num());
	for (const FAngelscriptStaticJITDescriptorView& Descriptor
		: Graph.Descriptors)
	{
		if (Descriptor.EngineLocalDescriptor == nullptr
			|| Descriptor.StableDescriptorKey.IsZero()
			|| DescriptorsByPointer.Contains(Descriptor.EngineLocalDescriptor))
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITDescriptorGraphInvalid: descriptors require unique non-null task-local pointers and stable keys."));
		}
		DescriptorsByPointer.Add(Descriptor.EngineLocalDescriptor, &Descriptor);
	}

	TSet<const FAngelscriptModuleDesc*> SeenModules;
	TSet<const FAngelscriptClassDesc*> SeenClasses;
	TSet<const FAngelscriptFunctionDesc*> SeenFunctionDescriptors;
	TSet<const asIScriptFunction*> SeenRootFunctions;
	for (const FAngelscriptModuleDesc* ModuleDesc
		: Graph.ResolvedModuleDescriptors)
	{
		if (ModuleDesc == nullptr || SeenModules.Contains(ModuleDesc))
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITDescriptorGraphInvalid: resolved ModuleDesc pointers must be non-null and unique."));
		}
		SeenModules.Add(ModuleDesc);

		for (const TSharedRef<FAngelscriptClassDesc>& ClassRef
			: ModuleDesc->Classes)
		{
			const FAngelscriptClassDesc* const ClassDesc = &ClassRef.Get();
			if (SeenClasses.Contains(ClassDesc))
			{
				return Fail(OutIndex, OutError, TEXT(
					"TypedASTJITDescriptorGraphInvalid: a resolved class descriptor appears more than once."));
			}
			SeenClasses.Add(ClassDesc);

			const FAngelscriptStaticJITDescriptorView* const* ClassViewPtr =
				DescriptorsByPointer.Find(ClassDesc);
			const EAngelscriptStaticJITDescriptorKind ExpectedClassKind =
				ClassDesc->bIsStruct
				? EAngelscriptStaticJITDescriptorKind::Struct
				: EAngelscriptStaticJITDescriptorKind::Class;
			if (ClassViewPtr == nullptr
				|| (*ClassViewPtr)->Kind != ExpectedClassKind)
			{
				return Fail(OutIndex, OutError, TEXT(
					"TypedASTJITDescriptorGraphMismatch: resolved ClassDesc is absent from the complete descriptor view."));
			}

			for (const TSharedRef<FAngelscriptFunctionDesc>& FunctionRef
				: ClassDesc->Methods)
			{
				const FAngelscriptFunctionDesc* const FunctionDesc =
					&FunctionRef.Get();
				asIScriptFunction* const ScriptFunction =
					FunctionDesc->ScriptFunction;
				if (ScriptFunction == nullptr
					|| SeenFunctionDescriptors.Contains(FunctionDesc)
					|| SeenRootFunctions.Contains(ScriptFunction))
				{
					return Fail(OutIndex, OutError, TEXT(
						"TypedASTJITRootDescriptorInvalid: every UFUNCTION descriptor requires one unique resolved ScriptFunction pointer."));
				}
				SeenFunctionDescriptors.Add(FunctionDesc);
				SeenRootFunctions.Add(ScriptFunction);

				const FAngelscriptStaticJITCompiledFunctionView* const*
					FunctionViewPtr = FunctionsByPointer.Find(ScriptFunction);
				const FAngelscriptStaticJITDescriptorView* const*
					DescriptorViewPtr = DescriptorsByPointer.Find(FunctionDesc);
				if (FunctionViewPtr == nullptr
					|| DescriptorViewPtr == nullptr
					|| (*DescriptorViewPtr)->Kind
						!= EAngelscriptStaticJITDescriptorKind::Function
					|| !(*DescriptorViewPtr)->bUFunctionRoot)
				{
					return Fail(OutIndex, OutError, TEXT(
						"TypedASTJITRootDescriptorMismatch: a resolved ClassDesc::Methods entry must exact-match one compiled function and one UFUNCTION descriptor row."));
				}

				const FAngelscriptStaticJITCompiledFunctionView& FunctionView =
					**FunctionViewPtr;
				const FAngelscriptStaticJITDescriptorView& DescriptorView =
					**DescriptorViewPtr;
				if (DescriptorView.StableDescriptorKey
						!= FunctionView.FunctionKey.Hash)
				{
					return Fail(OutIndex, OutError, TEXT(
						"TypedASTJITRootIdentityMismatch: descriptor and compiled-function stable identities disagree."));
				}

				FAngelscriptTypedASTJITRoot& Root =
					OutIndex.Roots.Emplace_GetRef();
				Root.EngineLocalFunction = ScriptFunction;
				Root.ClassDescriptor = ClassDesc;
				Root.FunctionDescriptor = FunctionDesc;
				Root.ModuleKey = FunctionView.ModuleKey;
				Root.FunctionKey = FunctionView.FunctionKey;
				Root.StableDescriptorKey =
					DescriptorView.StableDescriptorKey;
				Root.EntryPlan = DescriptorView.EntryPlan;
				Root.bGeneratedStaticsClassSurface =
					ClassDesc->bIsStaticsClass;
			}
		}
	}

	OutIndex.Roots.Sort(
		[](const FAngelscriptTypedASTJITRoot& Left,
			const FAngelscriptTypedASTJITRoot& Right)
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
		});
	for (int32 Index = 1; Index < OutIndex.Roots.Num(); ++Index)
	{
		if (OutIndex.Roots[Index - 1].FunctionKey
			== OutIndex.Roots[Index].FunctionKey)
		{
			return Fail(OutIndex, OutError, TEXT(
				"TypedASTJITRootIdentityDuplicate: distinct roots cannot share one stable function key."));
		}
	}
	return true;
}

#endif // AS_CAN_GENERATE_JIT
