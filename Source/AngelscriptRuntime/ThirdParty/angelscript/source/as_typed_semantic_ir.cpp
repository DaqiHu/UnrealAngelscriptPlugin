/*
   AngelCode Scripting Library
   Copyright (c) 2003-2026 Andreas Jonsson

   Private typed semantic sidecar for the maintained Unreal AngelScript fork.
*/

#include "as_typed_semantic_ir.h"
#include "as_module.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

BEGIN_AS_NAMESPACE

namespace
{
	bool IsValidSpan(const asSTypedSemanticSourceSpan& Span)
	{
		return Span.section.GetLength() != 0 && Span.sourceLength != 0;
	}

	bool IsDefaultAnchor(const asSTypedSemanticSourceAnchor& Anchor)
	{
		return Anchor.section.GetLength() == 0
			&& Anchor.sourceOffset == 0
			&& Anchor.sourceLength == 0
			&& Anchor.row == 0
			&& Anchor.column == 0;
	}

	bool IsValidAnchor(const asSTypedSemanticSourceAnchor& Anchor)
	{
		return Anchor.section.GetLength() != 0 && Anchor.sourceLength != 0;
	}

	bool IsDefaultGeneratedOrigin(
		const asSTypedSemanticGeneratedSourceOrigin& Origin)
	{
		return Origin.kind == asETypedSemanticGeneratedSourceKind::None
			&& Origin.generatorName.GetLength() == 0
			&& !Origin.hasAuthoredAnchor
			&& IsDefaultAnchor(Origin.authoredAnchor);
	}

	bool IsValidSourceProvenance(const asSTypedSemanticSourceSpan& Span)
	{
		if( Span.hasAuthoredOrigin )
		{
			if( !IsValidAnchor(Span.authoredOrigin) )
				return false;
		}
		else if( !IsDefaultAnchor(Span.authoredOrigin) )
		{
			return false;
		}

		if( Span.hasGeneratedOrigin )
		{
			const asSTypedSemanticGeneratedSourceOrigin& Origin =
				Span.generatedOrigin;
			if( Origin.kind == asETypedSemanticGeneratedSourceKind::None
				|| Origin.generatorName.GetLength() == 0 )
			{
				return false;
			}
			if( Origin.hasAuthoredAnchor )
			{
				if( !IsValidAnchor(Origin.authoredAnchor) )
					return false;
			}
			else if( !IsDefaultAnchor(Origin.authoredAnchor) )
			{
				return false;
			}
		}
		else if( !IsDefaultGeneratedOrigin(Span.generatedOrigin) )
		{
			return false;
		}
		return true;
	}

	asSTypedSemanticVerificationResult MakeSymbolError(
		asETypedSemanticVerificationError Error,
		const asSTypedSemanticSymbol& Symbol,
		const char* Detail)
	{
		asSTypedSemanticVerificationResult Result;
		Result.error = Error;
		Result.symbol = Symbol.id;
		Result.span = Symbol.span;
		Result.detail = Detail;
		return Result;
	}

	asSTypedSemanticVerificationResult MakeExpressionError(
		asETypedSemanticVerificationError Error,
		const asSTypedSemanticExpression& Expression,
		const char* Detail)
	{
		asSTypedSemanticVerificationResult Result;
		Result.error = Error;
		Result.expression = Expression.id;
		Result.span = Expression.span;
		Result.detail = Detail;
		return Result;
	}

	asSTypedSemanticVerificationResult MakeStatementError(
		asETypedSemanticVerificationError Error,
		const asSTypedSemanticStatement& Statement,
		const char* Detail)
	{
		asSTypedSemanticVerificationResult Result;
		Result.error = Error;
		Result.statement = Statement.id;
		Result.span = Statement.span;
		Result.detail = Detail;
		return Result;
	}

	asSTypedSemanticVerificationResult MakeReceiverError(
		const asSTypedSemanticEffectiveReceiver& Receiver,
		const char* Detail)
	{
		asSTypedSemanticVerificationResult Result;
		Result.error =
			asETypedSemanticVerificationError::InvalidEffectiveReceiver;
		Result.symbol = Receiver.symbol;
		Result.detail = Detail;
		return Result;
	}

	bool IsSymbolInRange(
		const asCTypedSemanticFunction& Function,
		asTypedSemanticSymbolId Id)
	{
		return Id.IsValid() && Id.value < Function.symbols.GetLength();
	}

	bool IsExpressionInRange(
		const asCTypedSemanticFunction& Function,
		asTypedSemanticExpressionId Id)
	{
		return Id.IsValid() && Id.value < Function.expressions.GetLength();
	}

	bool IsStatementInRange(
		const asCTypedSemanticFunction& Function,
		asTypedSemanticStatementId Id)
	{
		return Id.IsValid() && Id.value < Function.statements.GetLength();
	}

	bool TraitSummariesMatch(
		const asSTypedSemanticFunctionTraitSummary& Left,
		const asSTypedSemanticFunctionTraitSummary& Right)
	{
		return Left.knownTraitBits == Right.knownTraitBits
			&& Left.unknownTraitBits == Right.unknownTraitBits
			&& Left.sourcePolicyTraitBits == Right.sourcePolicyTraitBits
			&& Left.receiverTraitBits == Right.receiverTraitBits
			&& Left.invocationTraitBits == Right.invocationTraitBits
			&& Left.dispatchTraitBits == Right.dispatchTraitBits
			&& Left.bodyAvailabilityTraitBits
				== Right.bodyAvailabilityTraitBits
			&& Left.lifetimeTraitBits == Right.lifetimeTraitBits
			&& Left.profileTraitBits == Right.profileTraitBits;
	}

	const char* ToString(asETypedSemanticSymbolKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticSymbolKind::Receiver: return "Receiver";
		case asETypedSemanticSymbolKind::Parameter: return "Parameter";
		case asETypedSemanticSymbolKind::Local: return "Local";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticExpressionKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticExpressionKind::Literal: return "Literal";
		case asETypedSemanticExpressionKind::FoldedGlobalConstant: return "FoldedGlobalConstant";
		case asETypedSemanticExpressionKind::GlobalStorage: return "GlobalStorage";
		case asETypedSemanticExpressionKind::Symbol: return "Symbol";
		case asETypedSemanticExpressionKind::Conversion: return "Conversion";
		case asETypedSemanticExpressionKind::Assignment: return "Assignment";
		case asETypedSemanticExpressionKind::Unary: return "Unary";
		case asETypedSemanticExpressionKind::Binary: return "Binary";
		case asETypedSemanticExpressionKind::ShortCircuit: return "ShortCircuit";
		case asETypedSemanticExpressionKind::ResolvedCall: return "ResolvedCall";
		case asETypedSemanticExpressionKind::CallRewrite: return "CallRewrite";
		case asETypedSemanticExpressionKind::Unsupported: return "Unsupported";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticArgumentOrigin Origin)
	{
		switch (Origin)
		{
		case asETypedSemanticArgumentOrigin::None: return "None";
		case asETypedSemanticArgumentOrigin::SourcePositional:
			return "SourcePositional";
		case asETypedSemanticArgumentOrigin::SourceNamed:
			return "SourceNamed";
		case asETypedSemanticArgumentOrigin::Default: return "Default";
		case asETypedSemanticArgumentOrigin::HostHidden:
			return "HostHidden";
		case asETypedSemanticArgumentOrigin::Receiver:
			return "Receiver";
		default:
			return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticEvaluationRole Role)
	{
		switch (Role)
		{
		case asETypedSemanticEvaluationRole::Operand: return "Operand";
		case asETypedSemanticEvaluationRole::Receiver: return "Receiver";
		case asETypedSemanticEvaluationRole::Argument: return "Argument";
		case asETypedSemanticEvaluationRole::HiddenArgument:
			return "HiddenArgument";
		case asETypedSemanticEvaluationRole::MutationValue:
			return "MutationValue";
		case asETypedSemanticEvaluationRole::MutationTarget:
			return "MutationTarget";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticFirstParamMetadata Metadata)
	{
		switch (Metadata)
		{
		case asETypedSemanticFirstParamMetadata::None: return "None";
		case asETypedSemanticFirstParamMetadata::ScriptFunction:
			return "ScriptFunction";
		case asETypedSemanticFirstParamMetadata::ScriptObjectType:
			return "ScriptObjectType";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticCallTargetKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticCallTargetKind::Unspecified: return "Unspecified";
		case asETypedSemanticCallTargetKind::ScriptFunction: return "ScriptFunction";
		case asETypedSemanticCallTargetKind::SystemFunction: return "SystemFunction";
		case asETypedSemanticCallTargetKind::ImportedFunction: return "ImportedFunction";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticBodyAvailability Availability)
	{
		switch (Availability)
		{
		case asETypedSemanticBodyAvailability::Unspecified:
			return "Unspecified";
		case asETypedSemanticBodyAvailability::ScriptBody:
			return "ScriptBody";
		case asETypedSemanticBodyAvailability::NoScriptBody:
			return "NoScriptBody";
		case asETypedSemanticBodyAvailability::RegisteredNative:
			return "RegisteredNative";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticBodyOwnership Ownership)
	{
		switch (Ownership)
		{
		case asETypedSemanticBodyOwnership::Unspecified:
			return "Unspecified";
		case asETypedSemanticBodyOwnership::CallingModule:
			return "CallingModule";
		case asETypedSemanticBodyOwnership::OtherModule:
			return "OtherModule";
		case asETypedSemanticBodyOwnership::SharedOwner:
			return "SharedOwner";
		case asETypedSemanticBodyOwnership::SharedBorrowed:
			return "SharedBorrowed";
		case asETypedSemanticBodyOwnership::ExternalBorrowed:
			return "ExternalBorrowed";
		case asETypedSemanticBodyOwnership::NativeRuntime:
			return "NativeRuntime";
		case asETypedSemanticBodyOwnership::None:
			return "None";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticUnsupportedKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticUnsupportedKind::None: return "None";
		case asETypedSemanticUnsupportedKind::Ternary: return "Ternary";
		case asETypedSemanticUnsupportedKind::ObjectAccess: return "ObjectAccess";
		case asETypedSemanticUnsupportedKind::PropertyAccess: return "PropertyAccess";
		case asETypedSemanticUnsupportedKind::Reference: return "Reference";
		case asETypedSemanticUnsupportedKind::Handle: return "Handle";
		case asETypedSemanticUnsupportedKind::Container: return "Container";
		case asETypedSemanticUnsupportedKind::ConstructionOrLifetime: return "ConstructionOrLifetime";
		case asETypedSemanticUnsupportedKind::Lambda: return "Lambda";
		case asETypedSemanticUnsupportedKind::ExceptionCleanup: return "ExceptionCleanup";
		case asETypedSemanticUnsupportedKind::SuspendPoint: return "SuspendPoint";
		case asETypedSemanticUnsupportedKind::CompilerSynthesizedFunction: return "CompilerSynthesizedFunction";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticStatementKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticStatementKind::Block: return "Block";
		case asETypedSemanticStatementKind::LocalDeclaration: return "LocalDeclaration";
		case asETypedSemanticStatementKind::Expression: return "Expression";
		case asETypedSemanticStatementKind::If: return "If";
		case asETypedSemanticStatementKind::For: return "For";
		case asETypedSemanticStatementKind::While: return "While";
		case asETypedSemanticStatementKind::DoWhile: return "DoWhile";
		case asETypedSemanticStatementKind::Switch: return "Switch";
		case asETypedSemanticStatementKind::Case: return "Case";
		case asETypedSemanticStatementKind::Break: return "Break";
		case asETypedSemanticStatementKind::Continue: return "Continue";
		case asETypedSemanticStatementKind::Return: return "Return";
		case asETypedSemanticStatementKind::Unsupported: return "Unsupported";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticSafePointRole Role)
	{
		switch (Role)
		{
		case asETypedSemanticSafePointRole::None: return "None";
		case asETypedSemanticSafePointRole::Position: return "Position";
		case asETypedSemanticSafePointRole::FunctionEntry: return "FunctionEntry";
		case asETypedSemanticSafePointRole::Statement: return "Statement";
		case asETypedSemanticSafePointRole::Call: return "Call";
		case asETypedSemanticSafePointRole::LoopEntry: return "LoopEntry";
		case asETypedSemanticSafePointRole::LoopBackedge: return "LoopBackedge";
		case asETypedSemanticSafePointRole::Transfer: return "Transfer";
		case asETypedSemanticSafePointRole::SwitchInvalidValue: return "SwitchInvalidValue";
		case asETypedSemanticSafePointRole::Return: return "Return";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticLoopPhase Phase)
	{
		switch (Phase)
		{
		case asETypedSemanticLoopPhase::None: return "None";
		case asETypedSemanticLoopPhase::Initializer: return "Initializer";
		case asETypedSemanticLoopPhase::Condition: return "Condition";
		case asETypedSemanticLoopPhase::Body: return "Body";
		case asETypedSemanticLoopPhase::Increment: return "Increment";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticSwitchNormalization Normalization)
	{
		switch (Normalization)
		{
		case asETypedSemanticSwitchNormalization::None: return "None";
		case asETypedSemanticSwitchNormalization::SignedInt32: return "SignedInt32";
		case asETypedSemanticSwitchNormalization::UnsignedInt32: return "UnsignedInt32";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticReceiverKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticReceiverKind::None: return "None";
		case asETypedSemanticReceiverKind::NativeObjectThis: return "NativeObjectThis";
		case asETypedSemanticReceiverKind::ExternalImplicitThis: return "ExternalImplicitThis";
		case asETypedSemanticReceiverKind::MixinFirstParameter: return "MixinFirstParameter";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticInvocationKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticInvocationKind::Global: return "Global";
		case asETypedSemanticInvocationKind::GlobalInitializer: return "GlobalInitializer";
		case asETypedSemanticInvocationKind::InstanceMethod: return "InstanceMethod";
		case asETypedSemanticInvocationKind::Constructor: return "Constructor";
		case asETypedSemanticInvocationKind::Destructor: return "Destructor";
		case asETypedSemanticInvocationKind::Factory: return "Factory";
		case asETypedSemanticInvocationKind::Imported: return "Imported";
		case asETypedSemanticInvocationKind::System: return "System";
		case asETypedSemanticInvocationKind::Funcdef: return "Funcdef";
		case asETypedSemanticInvocationKind::Synthesized: return "Synthesized";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticCompileOutKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticCompileOutKind::CompileCalls: return "CompileCalls";
		case asETypedSemanticCompileOutKind::CompileOutEntirely: return "CompileOutEntirely";
		case asETypedSemanticCompileOutKind::ReplaceWithFirstParam: return "ReplaceWithFirstParam";
		case asETypedSemanticCompileOutKind::CompileOutAsMethodChain: return "CompileOutAsMethodChain";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticCleanupPlanState State)
	{
		switch (State)
		{
		case asETypedSemanticCleanupPlanState::Unverified: return "Unverified";
		case asETypedSemanticCleanupPlanState::VerifiedEmpty: return "VerifiedEmpty";
		case asETypedSemanticCleanupPlanState::NonEmpty: return "NonEmpty";
		case asETypedSemanticCleanupPlanState::PartialConstruction: return "PartialConstruction";
		case asETypedSemanticCleanupPlanState::ScriptDestructor: return "ScriptDestructor";
		case asETypedSemanticCleanupPlanState::CompilerExceptionRegion: return "CompilerExceptionRegion";
		default: return "Unknown";
		}
	}

	bool IsTypedSemanticContainerType(const asCDataType& Type)
	{
		const asCTypeInfo* const TypeInfo = Type.GetTypeInfo();
		return Type.IsArrayType()
			|| (TypeInfo != 0
				&& (TypeInfo->name == "TArray"
					|| TypeInfo->name == "TSet"
					|| TypeInfo->name == "TMap"));
	}

	const char* BoolString(bool Value)
	{
		return Value ? "true" : "false";
	}

	const char* ToString(asETypedSemanticVerificationError Error)
	{
		switch (Error)
		{
		case asETypedSemanticVerificationError::None: return "None";
		case asETypedSemanticVerificationError::MissingRoot: return "MissingRoot";
		case asETypedSemanticVerificationError::RootIsNotBlock: return "RootIsNotBlock";
		case asETypedSemanticVerificationError::NonContiguousSymbolId: return "NonContiguousSymbolId";
		case asETypedSemanticVerificationError::NonContiguousExpressionId: return "NonContiguousExpressionId";
		case asETypedSemanticVerificationError::NonContiguousStatementId: return "NonContiguousStatementId";
		case asETypedSemanticVerificationError::DanglingSymbolId: return "DanglingSymbolId";
		case asETypedSemanticVerificationError::DanglingExpressionId: return "DanglingExpressionId";
		case asETypedSemanticVerificationError::DanglingStatementId: return "DanglingStatementId";
		case asETypedSemanticVerificationError::StatementHasMultipleOwners: return "StatementHasMultipleOwners";
		case asETypedSemanticVerificationError::StatementOwnershipCycle: return "StatementOwnershipCycle";
		case asETypedSemanticVerificationError::InvalidNodeShape: return "InvalidNodeShape";
		case asETypedSemanticVerificationError::InvalidSourceSpan: return "InvalidSourceSpan";
		case asETypedSemanticVerificationError::InvalidSourceProvenance: return "InvalidSourceProvenance";
		case asETypedSemanticVerificationError::MissingUnsupportedCategory: return "MissingUnsupportedCategory";
		case asETypedSemanticVerificationError::InvalidEffectiveReceiver: return "InvalidEffectiveReceiver";
		case asETypedSemanticVerificationError::InvalidCleanupPlan: return "InvalidCleanupPlan";
		case asETypedSemanticVerificationError::InvalidControlTarget: return "InvalidControlTarget";
		case asETypedSemanticVerificationError::InvalidCallEvaluationSequence: return "InvalidCallEvaluationSequence";
		case asETypedSemanticVerificationError::InvalidArgumentProvenance: return "InvalidArgumentProvenance";
		case asETypedSemanticVerificationError::InvalidFunctionTraitSummary: return "InvalidFunctionTraitSummary";
		case asETypedSemanticVerificationError::InvalidFunctionBodyProvenance: return "InvalidFunctionBodyProvenance";
		case asETypedSemanticVerificationError::InvalidCallTargetProvenance: return "InvalidCallTargetProvenance";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticGeneratedSourceKind Kind)
	{
		switch (Kind)
		{
		case asETypedSemanticGeneratedSourceKind::None: return "None";
		case asETypedSemanticGeneratedSourceKind::LiteralAsset: return "LiteralAsset";
		case asETypedSemanticGeneratedSourceKind::SubsystemAccessor: return "SubsystemAccessor";
		case asETypedSemanticGeneratedSourceKind::ClassHelper: return "ClassHelper";
		case asETypedSemanticGeneratedSourceKind::StaticClassHelper: return "StaticClassHelper";
		case asETypedSemanticGeneratedSourceKind::DelegateHelper: return "DelegateHelper";
		case asETypedSemanticGeneratedSourceKind::RangeBasedFor: return "RangeBasedFor";
		default: return "Unknown";
		}
	}

	const char* ToString(asETypedSemanticSnapshotState State)
	{
		switch (State)
		{
		case asETypedSemanticSnapshotState::Verified: return "Verified";
		case asETypedSemanticSnapshotState::MissingFunction: return "MissingFunction";
		case asETypedSemanticSnapshotState::VerificationFailed: return "VerificationFailed";
		default: return "Unknown";
		}
	}

	void AppendJsonString(asCString& Output, const asCString& Value)
	{
		static const char HexDigits[] = "0123456789abcdef";
		Output += "\"";
		for (asUINT Index = 0; Index < Value.GetLength(); ++Index)
		{
			const unsigned char Character =
				static_cast<unsigned char>(Value[Index]);
			switch (Character)
			{
			case '\"': Output += "\\\""; break;
			case '\\': Output += "\\\\"; break;
			case '\b': Output += "\\b"; break;
			case '\f': Output += "\\f"; break;
			case '\n': Output += "\\n"; break;
			case '\r': Output += "\\r"; break;
			case '\t': Output += "\\t"; break;
			default:
				if (Character < 0x20)
				{
					char Escape[7] = {
						'\\', 'u', '0', '0',
						HexDigits[(Character >> 4) & 0x0f],
						HexDigits[Character & 0x0f],
						0 };
					Output += Escape;
				}
				else
				{
					const char Byte = static_cast<char>(Character);
					Output.Concatenate(&Byte, 1);
				}
				break;
			}
		}
		Output += "\"";
	}

	void AppendTextValue(asCString& Output, const asCString& Value)
	{
		for (asUINT Index = 0; Index < Value.GetLength(); ++Index)
		{
			switch (Value[Index])
			{
			case '\\': Output += "\\\\"; break;
			case '\n': Output += "\\n"; break;
			case '\r': Output += "\\r"; break;
			case '\t': Output += "\\t"; break;
			default:
				Output.Concatenate(Value.AddressOf() + Index, 1);
				break;
			}
		}
	}

	void AppendAnchor(
		asCString& Output,
		const asSTypedSemanticSourceAnchor& Anchor)
	{
		asCString Text;
		Text.Format(
			"%s@%u+%u:%u:%u",
			Anchor.section.AddressOf(),
			Anchor.sourceOffset,
			Anchor.sourceLength,
			Anchor.row,
			Anchor.column);
		Output += Text;
	}

	void AppendSpan(asCString& Output, const asSTypedSemanticSourceSpan& Span)
	{
		asCString Text;
		if( !Span.hasAuthoredOrigin && !Span.hasGeneratedOrigin )
		{
			// Preserve the compact v1 spelling for ordinary processed-only
			// nodes. The field itself is always the processed compiler span.
			Text.Format(
				"%s@%u+%u",
				Span.section.AddressOf(),
				Span.sourceOffset,
				Span.sourceLength);
		}
		else
		{
			Text.Format(
				"processed=%s@%u+%u:%u:%u",
				Span.section.AddressOf(),
				Span.sourceOffset,
				Span.sourceLength,
				Span.row,
				Span.column);
		}
		Output += Text;
		if( Span.hasAuthoredOrigin )
		{
			Output += " authored=";
			AppendAnchor(Output, Span.authoredOrigin);
		}
		if( Span.hasGeneratedOrigin )
		{
			Output += " generated=";
			Output += ToString(Span.generatedOrigin.kind);
			Output += " generator=";
			AppendTextValue(Output, Span.generatedOrigin.generatorName);
			if( Span.generatedOrigin.hasAuthoredAnchor )
			{
				Output += " anchor=";
				AppendAnchor(Output, Span.generatedOrigin.authoredAnchor);
			}
		}
	}

	void AppendExpressionList(
		asCString& Output,
		const asCArray<asTypedSemanticExpressionId>& Ids)
	{
		Output += "[";
		for (asUINT Index = 0; Index < Ids.GetLength(); ++Index)
		{
			if (Index != 0)
				Output += ",";
			asCString Id;
			Id.Format("E%u", Ids[Index].value);
			Output += Id;
		}
		Output += "]";
	}

	void AppendStatementList(
		asCString& Output,
		const asCArray<asTypedSemanticStatementId>& Ids)
	{
		Output += "[";
		for (asUINT Index = 0; Index < Ids.GetLength(); ++Index)
		{
			if (Index != 0)
				Output += ",";
			asCString Id;
			Id.Format("T%u", Ids[Index].value);
			Output += Id;
		}
		Output += "]";
	}

	void AppendIntList(
		asCString& Output,
		const asCArray<int>& Values)
	{
		Output += "[";
		for (asUINT Index = 0; Index < Values.GetLength(); ++Index)
		{
			if (Index != 0)
				Output += ",";
			asCString Value;
			Value.Format("%d", Values[Index]);
			Output += Value;
		}
		Output += "]";
	}

	void AppendUIntList(
		asCString& Output,
		const asCArray<asUINT>& Values)
	{
		Output += "[";
		for (asUINT Index = 0; Index < Values.GetLength(); ++Index)
		{
			if (Index != 0)
				Output += ",";
			asCString Value;
			Value.Format("%u", Values[Index]);
			Output += Value;
		}
		Output += "]";
	}

	void AppendArgumentOrigins(
		asCString& Output,
		const asCArray<asETypedSemanticArgumentOrigin>& Origins)
	{
		Output += "[";
		for (asUINT Index = 0; Index < Origins.GetLength(); ++Index)
		{
			if (Index != 0)
				Output += ",";
			Output += ToString(Origins[Index]);
		}
		Output += "]";
	}

	void AppendArgumentProvenance(
		asCString& Output,
		const asCArray<asSTypedSemanticArgumentProvenance>& Provenance)
	{
		Output += "[";
		for (asUINT Index = 0; Index < Provenance.GetLength(); ++Index)
		{
			if (Index != 0)
				Output += ",";
			const asSTypedSemanticArgumentProvenance& Origin =
				Provenance[Index];
			asCString Prefix;
			Prefix.Format(
				"{origin=%s,source=%d,name=",
				ToString(Origin.origin),
				Origin.sourceArgumentOrdinal);
			Output += Prefix;
			AppendTextValue(Output, Origin.sourceArgumentName);
			Output += ",span=";
			AppendSpan(Output, Origin.processedSpan);
			asCString Callee;
			Callee.Format(
				",callee=%d,decl=",
				Origin.calleeFunctionId);
			Output += Callee;
			AppendTextValue(Output, Origin.calleeDeclaration);
			asCString Formal;
			Formal.Format(
				",formal=%d,param=",
				Origin.calleeParameterIndex);
			Output += Formal;
			AppendTextValue(Output, Origin.calleeParameterName);
			Output += ",canonical=";
			AppendTextValue(Output, Origin.canonicalExpression);
			Output += "}";
		}
		Output += "]";
	}

	void AppendEvaluationSteps(
		asCString& Output,
		const asCArray<asSTypedSemanticEvaluationStep>& Steps)
	{
		Output += "[";
		for (asUINT Index = 0; Index < Steps.GetLength(); ++Index)
		{
			if (Index != 0)
				Output += ",";
			const asSTypedSemanticEvaluationStep& Step = Steps[Index];
			asCString Value;
			Value.Format(
				"%s(E%u,formal=%d,source=%d)",
				ToString(Step.role),
				Step.expression.value,
				Step.formalParameterIndex,
				Step.sourceArgumentOrdinal);
			Output += Value;
		}
		Output += "]";
	}

	void AppendFunctionTargetProvenance(
		asCString& Output,
		const asSTypedSemanticFunctionTargetProvenance& Provenance)
	{
		Output += " declaration=";
		AppendTextValue(Output, Provenance.canonicalDeclaration);
		Output += " declarationModule=";
		AppendTextValue(Output, Provenance.declarationModule);
		asCString TraitsAndBody;
		TraitsAndBody.Format(
			" targetTraits=%u bodyAvailability=%s bodyOwnership=%s bodyOwner=",
			Provenance.declaredTraitBits,
			ToString(Provenance.bodyAvailability),
			ToString(Provenance.bodyOwnership));
		Output += TraitsAndBody;
		AppendTextValue(Output, Provenance.bodyOwnerModule);
		Output += " importSource=";
		AppendTextValue(Output, Provenance.importedSourceModule);
		Output += " importSignature=";
		AppendTextValue(Output, Provenance.importedCanonicalSignature);
	}

	void CollectOwnedStatements(
		const asSTypedSemanticStatement& Statement,
		asCArray<asTypedSemanticStatementId>& OutOwned)
	{
		for (asUINT Index = 0; Index < Statement.children.GetLength(); ++Index)
			OutOwned.PushLast(Statement.children[Index]);
		if (Statement.initializerStatement.IsValid()) OutOwned.PushLast(Statement.initializerStatement);
		if (Statement.incrementStatement.IsValid()) OutOwned.PushLast(Statement.incrementStatement);
		if (Statement.thenStatement.IsValid()) OutOwned.PushLast(Statement.thenStatement);
		if (Statement.elseStatement.IsValid()) OutOwned.PushLast(Statement.elseStatement);
		if (Statement.bodyStatement.IsValid()) OutOwned.PushLast(Statement.bodyStatement);
	}

	bool FindStatementCycle(
		const asCTypedSemanticFunction& Function,
		asUINT StatementIndex,
		asCArray<asBYTE>& States,
		asTypedSemanticStatementId& OutCycle)
	{
		if (States[StatementIndex] == 1)
		{
			OutCycle = asTypedSemanticStatementId(StatementIndex);
			return true;
		}
		if (States[StatementIndex] == 2)
			return false;

		States[StatementIndex] = 1;
		asCArray<asTypedSemanticStatementId> Owned;
		CollectOwnedStatements(Function.statements[StatementIndex], Owned);
		for (asUINT Index = 0; Index < Owned.GetLength(); ++Index)
		{
			if (Owned[Index].value < Function.statements.GetLength()
				&& FindStatementCycle(Function, Owned[Index].value, States, OutCycle))
			{
				return true;
			}
		}
		States[StatementIndex] = 2;
		return false;
	}

	const char* ValidateFunctionBodyProvenance(
		const asSTypedSemanticFunctionHeader& Header)
	{
		const bool HasAnyProvenance = Header.callingModule.GetLength() != 0
			|| Header.bodyAvailability
				!= asETypedSemanticBodyAvailability::Unspecified
			|| Header.bodyOwnership
				!= asETypedSemanticBodyOwnership::Unspecified
			|| Header.bodyOwnerModule.GetLength() != 0;
		// Hand-authored private test HIR predating body provenance remains usable.
		// Every compiler-captured function takes the populated branch below.
		if (!HasAnyProvenance)
			return 0;
		if (Header.callingModule.GetLength() == 0)
			return "Function body provenance requires the compiler transaction module";

		switch (Header.bodyAvailability)
		{
		case asETypedSemanticBodyAvailability::ScriptBody:
			if (Header.bodyOwnerModule.GetLength() == 0
				|| Header.bodyOwnerModule != Header.callingModule)
			{
				return "Captured script body must be owned by its compiler transaction module";
			}
			if (Header.bodyOwnership
					!= asETypedSemanticBodyOwnership::CallingModule
				&& Header.bodyOwnership
					!= asETypedSemanticBodyOwnership::SharedOwner)
			{
				return "Captured script body requires local or shared-owner provenance";
			}
			if ((Header.declaredTraitBits & asTRAIT_SHARED) != 0
				&& Header.bodyOwnership
					!= asETypedSemanticBodyOwnership::SharedOwner)
			{
				return "Declared shared body must retain shared-owner provenance";
			}
			break;
		case asETypedSemanticBodyAvailability::NoScriptBody:
			if (Header.bodyOwnership != asETypedSemanticBodyOwnership::None
				|| Header.bodyOwnerModule.GetLength() != 0)
			{
				return "Bodyless function header must not claim a body owner";
			}
			break;
		case asETypedSemanticBodyAvailability::RegisteredNative:
			return "Compiler-owned script HIR cannot claim a registered native body";
		case asETypedSemanticBodyAvailability::Unspecified:
		default:
			return "Partially populated function body provenance is invalid";
		}
		return 0;
	}

	const char* ValidateCallTargetProvenance(
		const asCTypedSemanticFunction& Function,
		const asSTypedSemanticExpression& Expression)
	{
		const asSTypedSemanticFunctionTargetProvenance& Target =
			Expression.targetProvenance;
		if (Target.canonicalDeclaration.GetLength() == 0)
			return "Resolved call target requires a canonical declaration";

		const bool IsImportedCoordinate =
			(Expression.resolvedFunctionId & FUNC_IMPORTED) != 0;
		switch (Expression.callTargetKind)
		{
		case asETypedSemanticCallTargetKind::ScriptFunction:
			if (IsImportedCoordinate)
				return "Concrete script target cannot use an imported-slot coordinate";
			if (Target.declarationModule.GetLength() == 0)
				return "Script target requires its declaration module";
			if (Target.importedSourceModule.GetLength() != 0
				|| Target.importedCanonicalSignature.GetLength() != 0)
			{
				return "Concrete script target must not claim imported-slot provenance";
			}
			if (Target.bodyAvailability
				== asETypedSemanticBodyAvailability::NoScriptBody)
			{
				if (Target.bodyOwnership
						!= asETypedSemanticBodyOwnership::None
					|| Target.bodyOwnerModule.GetLength() != 0)
				{
					return "Bodyless script declaration must not claim an owner";
				}
				return 0;
			}
			if (Target.bodyAvailability
				!= asETypedSemanticBodyAvailability::ScriptBody
				|| Target.bodyOwnerModule.GetLength() == 0)
			{
				return "Concrete script target requires explicit body availability and owner";
			}
			switch (Target.bodyOwnership)
			{
			case asETypedSemanticBodyOwnership::CallingModule:
			case asETypedSemanticBodyOwnership::SharedOwner:
				if (Target.declarationModule != Target.bodyOwnerModule
					|| (Function.header.callingModule.GetLength() != 0
						&& Target.bodyOwnerModule
							!= Function.header.callingModule))
				{
					return "Locally owned call target must agree with the calling module";
				}
				break;
			case asETypedSemanticBodyOwnership::OtherModule:
				if (Target.declarationModule != Target.bodyOwnerModule
					|| (Function.header.callingModule.GetLength() != 0
						&& Target.bodyOwnerModule
							== Function.header.callingModule))
				{
					return "Other-module call target must retain its distinct declaration/body owner";
				}
				break;
			case asETypedSemanticBodyOwnership::SharedBorrowed:
				if ((Target.declaredTraitBits & asTRAIT_SHARED) == 0)
					return "Shared borrower requires a shared target trait";
				// Fall through: shared and external declarations have the same
				// caller/body-owner separation, but remain distinct enum values.
			case asETypedSemanticBodyOwnership::ExternalBorrowed:
				if (Function.header.callingModule.GetLength() == 0
					|| Target.declarationModule
						!= Function.header.callingModule
					|| Target.bodyOwnerModule
						== Function.header.callingModule)
				{
					return "Borrowed declaration must keep caller and body owner distinct";
				}
				break;
			default:
				return "Script target body ownership is outside the supported contract";
			}
			return 0;
		case asETypedSemanticCallTargetKind::SystemFunction:
			if (IsImportedCoordinate
				|| Target.bodyAvailability
					!= asETypedSemanticBodyAvailability::RegisteredNative
				|| Target.bodyOwnership
					!= asETypedSemanticBodyOwnership::NativeRuntime
				|| Target.declarationModule.GetLength() != 0
				|| Target.bodyOwnerModule.GetLength() != 0
				|| Target.importedSourceModule.GetLength() != 0
				|| Target.importedCanonicalSignature.GetLength() != 0)
			{
				return "System target requires registered-native provenance without module ownership";
			}
			return 0;
		case asETypedSemanticCallTargetKind::ImportedFunction:
			if (!IsImportedCoordinate
				|| Target.declarationModule.GetLength() == 0
				|| Target.bodyAvailability
					!= asETypedSemanticBodyAvailability::NoScriptBody
				|| Target.bodyOwnership != asETypedSemanticBodyOwnership::None
				|| Target.bodyOwnerModule.GetLength() != 0
				|| Target.importedSourceModule.GetLength() == 0
				|| Target.importedCanonicalSignature.GetLength() == 0
				|| Target.canonicalDeclaration
					!= Target.importedCanonicalSignature)
			{
				return "Imported call must retain source module, canonical signature, and the mutable slot coordinate only";
			}
			return 0;
		case asETypedSemanticCallTargetKind::Unspecified:
		default:
			return "Resolved call target kind is outside the supported contract";
		}
	}
}

asTypedSemanticSymbolId asCTypedSemanticFunction::AddSymbol(
	const asSTypedSemanticSymbol& Symbol)
{
	asSTypedSemanticSymbol Stored = Symbol;
	Stored.id = asTypedSemanticSymbolId(symbols.GetLength());
	symbols.PushLast(Stored);
	return Stored.id;
}

asTypedSemanticExpressionId asCTypedSemanticFunction::AddExpression(
	const asSTypedSemanticExpression& Expression)
{
	asSTypedSemanticExpression Stored = Expression;
	Stored.id = asTypedSemanticExpressionId(expressions.GetLength());
	expressions.PushLast(Stored);
	return Stored.id;
}

asTypedSemanticStatementId asCTypedSemanticFunction::AddStatement(
	const asSTypedSemanticStatement& Statement)
{
	asSTypedSemanticStatement Stored = Statement;
	Stored.id = asTypedSemanticStatementId(statements.GetLength());
	statements.PushLast(Stored);
	return Stored.id;
}

asTypedSemanticCleanupPlanId asCTypedSemanticFunction::AddCleanupPlan(
	const asSTypedSemanticCleanupPlan& Plan)
{
	asSTypedSemanticCleanupPlan Stored = Plan;
	Stored.id = asTypedSemanticCleanupPlanId(cleanupPlans.GetLength());
	cleanupPlans.PushLast(Stored);
	return Stored.id;
}

asSTypedSemanticFunctionTraitSummary ClassifyTypedSemanticFunctionTraits(
	const asDWORD DeclaredTraitBits)
{
	static_assert(
		asTYPED_SEMANTIC_KNOWN_FUNC_TRAIT_BITS
			== static_cast<asDWORD>(
				asTRAIT_CONSTRUCTOR
				| asTRAIT_DESTRUCTOR
				| asTRAIT_CONST
				| asTRAIT_PRIVATE
				| asTRAIT_PROTECTED
				| asTRAIT_FINAL
				| asTRAIT_OVERRIDE
				| asTRAIT_SHARED
				| asTRAIT_EXTERNAL
				| asTRAIT_PROPERTY
				| asTRAIT_IMPLICITCONSTRUCTOR
				| asTRAIT_MIXIN
				| asTRAIT_LOCAL
				| asTRAIT_NODISCARD
				| asTRAIT_DEPRECATED
				| asTRAIT_GENERIC_TEMPLATE_FUNCTION
				| asTRAIT_USES_WORLDCONTEXT
				| asTRAIT_ACCEPT_TEMPORARY_OBJECT
				| asTRAIT_GENERATED_FUNCTION
				| asTRAIT_NOT_CALLABLE
				| asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS
				| asTRAIT_EXTERNAL_IMPLICIT_THIS
				| asTRAIT_ALLOWDISCARD
				| asTRAIT_EDITOR_ONLY
				| asTRAIT_EXPLICIT
				| asTRAIT_UNSAFE_DURING_CONSTRUCTION
				| asTRAIT_DEFAULTS_ONLY),
		"Update the typed semantic trait summary when asEFuncTrait changes");

	asSTypedSemanticFunctionTraitSummary Summary;
	Summary.knownTraitBits =
		DeclaredTraitBits & asTYPED_SEMANTIC_KNOWN_FUNC_TRAIT_BITS;
	Summary.unknownTraitBits =
		DeclaredTraitBits & ~asTYPED_SEMANTIC_KNOWN_FUNC_TRAIT_BITS;
	Summary.sourcePolicyTraitBits = Summary.knownTraitBits & (
		asTRAIT_PRIVATE
		| asTRAIT_PROTECTED
		| asTRAIT_PROPERTY
		| asTRAIT_NODISCARD
		| asTRAIT_DEPRECATED
		| asTRAIT_NOT_CALLABLE
		| asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS
		| asTRAIT_ALLOWDISCARD
		| asTRAIT_EXPLICIT);
	Summary.receiverTraitBits = Summary.knownTraitBits & (
		asTRAIT_CONST
		| asTRAIT_MIXIN
		| asTRAIT_ACCEPT_TEMPORARY_OBJECT
		| asTRAIT_EXTERNAL_IMPLICIT_THIS);
	Summary.invocationTraitBits = Summary.knownTraitBits & (
		asTRAIT_CONSTRUCTOR
		| asTRAIT_DESTRUCTOR
		| asTRAIT_IMPLICITCONSTRUCTOR
		| asTRAIT_MIXIN
		| asTRAIT_EXTERNAL_IMPLICIT_THIS);
	Summary.dispatchTraitBits = Summary.knownTraitBits & (
		asTRAIT_FINAL
		| asTRAIT_OVERRIDE
		| asTRAIT_SHARED
		| asTRAIT_EXTERNAL
		| asTRAIT_PROPERTY
		| asTRAIT_MIXIN
		| asTRAIT_LOCAL
		| asTRAIT_GENERIC_TEMPLATE_FUNCTION
		| asTRAIT_USES_WORLDCONTEXT
		| asTRAIT_NOT_CALLABLE);
	Summary.bodyAvailabilityTraitBits = Summary.knownTraitBits & (
		asTRAIT_SHARED
		| asTRAIT_EXTERNAL
		| asTRAIT_LOCAL
		| asTRAIT_GENERIC_TEMPLATE_FUNCTION
		| asTRAIT_GENERATED_FUNCTION
		| asTRAIT_EDITOR_ONLY);
	Summary.lifetimeTraitBits = Summary.knownTraitBits & (
		asTRAIT_CONSTRUCTOR
		| asTRAIT_DESTRUCTOR
		| asTRAIT_IMPLICITCONSTRUCTOR
		| asTRAIT_ACCEPT_TEMPORARY_OBJECT
		| asTRAIT_UNSAFE_DURING_CONSTRUCTION
		| asTRAIT_DEFAULTS_ONLY);
	Summary.profileTraitBits = Summary.knownTraitBits & (
		asTRAIT_USES_WORLDCONTEXT
		| asTRAIT_GENERATED_FUNCTION
		| asTRAIT_EDITOR_ONLY
		| asTRAIT_UNSAFE_DURING_CONSTRUCTION
		| asTRAIT_DEFAULTS_ONLY);
	return Summary;
}

asSTypedSemanticVerificationResult VerifyTypedSemanticFunction(
	const asCTypedSemanticFunction& Function)
{
	if (!Function.rootStatement.IsValid())
	{
		asSTypedSemanticVerificationResult Result;
		Result.error = asETypedSemanticVerificationError::MissingRoot;
		Result.detail = "Function root is missing";
		return Result;
	}
	if (!IsStatementInRange(Function, Function.rootStatement))
	{
		asSTypedSemanticVerificationResult Result;
		Result.error = asETypedSemanticVerificationError::DanglingStatementId;
		Result.statement = Function.rootStatement;
		Result.detail = "Function root is outside the statement arena";
		return Result;
	}
	if (Function.statements[Function.rootStatement.value].kind
		!= asETypedSemanticStatementKind::Block)
	{
		return MakeStatementError(
			asETypedSemanticVerificationError::RootIsNotBlock,
			Function.statements[Function.rootStatement.value],
			"Function root must be a block");
	}

	for (asUINT Index = 0; Index < Function.cleanupPlans.GetLength(); ++Index)
	{
		if (!Function.cleanupPlans[Index].id.IsValid()
			|| Function.cleanupPlans[Index].id.value != Index)
		{
			asSTypedSemanticVerificationResult Result;
			Result.error = asETypedSemanticVerificationError::InvalidCleanupPlan;
			Result.statement = Function.rootStatement;
			Result.span = Function.statements[Function.rootStatement.value].span;
			Result.detail = "Cleanup plan ID does not match its arena index";
			return Result;
		}
	}
	const asSTypedSemanticFunctionHeader& Header = Function.header;
	const asSTypedSemanticFunctionTraitSummary ExpectedTraitSummary =
		ClassifyTypedSemanticFunctionTraits(Header.declaredTraitBits);
	if (!TraitSummariesMatch(Header.traitSummary, ExpectedTraitSummary))
	{
		asSTypedSemanticVerificationResult Result;
		Result.error =
			asETypedSemanticVerificationError::InvalidFunctionTraitSummary;
		Result.statement = Function.rootStatement;
		Result.span = Function.statements[Function.rootStatement.value].span;
		Result.detail =
			"Function trait summary does not match the declared trait snapshot";
		return Result;
	}
	if (const char* const BodyProvenanceError =
		ValidateFunctionBodyProvenance(Header))
	{
		asSTypedSemanticVerificationResult Result;
		Result.error =
			asETypedSemanticVerificationError::InvalidFunctionBodyProvenance;
		Result.statement = Function.rootStatement;
		Result.span = Function.statements[Function.rootStatement.value].span;
		Result.detail = BodyProvenanceError;
		return Result;
	}
	const bool HasUniversalCleanupPlan =
		Header.universalCleanupPlan.IsValid()
		&& Header.universalCleanupPlan.value < Function.cleanupPlans.GetLength();
	auto MakeCleanupError = [&Function](const char* Detail)
	{
		asSTypedSemanticVerificationResult Result;
		Result.error = asETypedSemanticVerificationError::InvalidCleanupPlan;
		Result.statement = Function.rootStatement;
		Result.span = Function.statements[Function.rootStatement.value].span;
		Result.detail = Detail;
		return Result;
	};
	switch (Header.cleanupPlanState)
	{
	case asETypedSemanticCleanupPlanState::Unverified:
		if (Header.universalCleanupPlan.IsValid()
			|| Header.cleanupPlanCoversAllTransfers
			|| Function.cleanupPlans.GetLength() != 0)
		{
			return MakeCleanupError(
				"Unverified cleanup state must not claim a cleanup plan");
		}
		break;
	case asETypedSemanticCleanupPlanState::VerifiedEmpty:
		if (!HasUniversalCleanupPlan
			|| Function.cleanupPlans.GetLength() != 1
			|| !Function.cleanupPlans[
				Header.universalCleanupPlan.value].reverseLiveLifetimeSlots.GetLength() == 0
			|| !Header.cleanupPlanCoversAllTransfers
			|| Header.hasExceptionCleanup)
		{
			return MakeCleanupError(
				"Verified-empty cleanup requires one empty universal plan covering every transfer");
		}
		break;
	case asETypedSemanticCleanupPlanState::NonEmpty:
		if (!HasUniversalCleanupPlan
			|| Function.cleanupPlans[
				Header.universalCleanupPlan.value].reverseLiveLifetimeSlots.GetLength() == 0)
		{
			return MakeCleanupError(
				"Non-empty cleanup state requires a plan with live lifetime slots");
		}
		break;
	case asETypedSemanticCleanupPlanState::PartialConstruction:
	case asETypedSemanticCleanupPlanState::ScriptDestructor:
		if (!HasUniversalCleanupPlan)
			return MakeCleanupError("Lifetime cleanup state requires a cleanup plan");
		break;
	case asETypedSemanticCleanupPlanState::CompilerExceptionRegion:
		if (!HasUniversalCleanupPlan || !Header.hasExceptionCleanup)
			return MakeCleanupError("Compiler exception region requires an explicit cleanup plan");
		break;
	default:
		return MakeCleanupError("Cleanup plan state is outside the supported contract");
	}

	for (asUINT Index = 0; Index < Function.symbols.GetLength(); ++Index)
	{
		const asSTypedSemanticSymbol& Symbol = Function.symbols[Index];
		if (!Symbol.id.IsValid() || Symbol.id.value != Index)
			return MakeSymbolError(asETypedSemanticVerificationError::NonContiguousSymbolId, Symbol, "Symbol ID does not match its arena index");
		if (!Symbol.type.IsValid() || Symbol.name.GetLength() == 0)
			return MakeSymbolError(asETypedSemanticVerificationError::InvalidNodeShape, Symbol, "Symbol requires a name and exact type");
		if (!IsValidSpan(Symbol.span))
			return MakeSymbolError(asETypedSemanticVerificationError::InvalidSourceSpan, Symbol, "Symbol source span is invalid");
		if (!IsValidSourceProvenance(Symbol.span))
			return MakeSymbolError(asETypedSemanticVerificationError::InvalidSourceProvenance, Symbol, "Symbol source provenance is malformed");
	}

	bool HasCompilerExceptionRegionMarker = false;
	for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
	{
		const asSTypedSemanticExpression& Expression = Function.expressions[Index];
		if (!Expression.id.IsValid() || Expression.id.value != Index)
			return MakeExpressionError(asETypedSemanticVerificationError::NonContiguousExpressionId, Expression, "Expression ID does not match its arena index");
		if (!Expression.type.IsValid())
			return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Expression requires an exact result type");
		if (!IsValidSpan(Expression.span))
			return MakeExpressionError(asETypedSemanticVerificationError::InvalidSourceSpan, Expression, "Expression source span is invalid");
		if (!IsValidSourceProvenance(Expression.span))
			return MakeExpressionError(asETypedSemanticVerificationError::InvalidSourceProvenance, Expression, "Expression source provenance is malformed");
		for (asUINT OperandIndex = 0; OperandIndex < Expression.operands.GetLength(); ++OperandIndex)
		{
			if (!IsExpressionInRange(Function, Expression.operands[OperandIndex]))
				return MakeExpressionError(asETypedSemanticVerificationError::DanglingExpressionId, Expression, "Expression operand is outside the arena");
		}
		if (Expression.receiverExpression.IsValid()
			&& (!IsExpressionInRange(Function, Expression.receiverExpression)
				|| Expression.receiverExpression.value >= Index))
		{
			return MakeExpressionError(
				asETypedSemanticVerificationError::DanglingExpressionId,
				Expression,
				"Call receiver must reference a previously captured expression");
		}
		if (Expression.kind != asETypedSemanticExpressionKind::ResolvedCall
			&& Expression.kind != asETypedSemanticExpressionKind::CallRewrite
			&& Expression.receiverExpression.IsValid())
		{
			return MakeExpressionError(
				asETypedSemanticVerificationError::InvalidNodeShape,
				Expression,
				"Only a resolved call or final call rewrite may retain a dedicated receiver expression");
		}
		for (asUINT StepIndex = 0;
			StepIndex < Expression.evaluationSteps.GetLength();
			++StepIndex)
		{
			const asSTypedSemanticEvaluationStep& Step =
				Expression.evaluationSteps[StepIndex];
			if (!IsExpressionInRange(Function, Step.expression)
				|| Step.expression.value >= Index)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::DanglingExpressionId,
					Expression,
					"Evaluation step must reference one previously captured expression");
			}
			if (Step.formalParameterIndex < -1
				|| Step.sourceArgumentOrdinal < -1)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
					Expression,
					"Evaluation step indices must be non-negative or the explicit -1 sentinel");
			}
			switch (Step.role)
			{
			case asETypedSemanticEvaluationRole::Operand:
			case asETypedSemanticEvaluationRole::Receiver:
			case asETypedSemanticEvaluationRole::Argument:
			case asETypedSemanticEvaluationRole::HiddenArgument:
			case asETypedSemanticEvaluationRole::MutationValue:
			case asETypedSemanticEvaluationRole::MutationTarget:
				break;
			default:
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
					Expression,
					"Evaluation step role is outside the stable contract");
			}
		}
		switch (Expression.kind)
		{
		case asETypedSemanticExpressionKind::Literal:
			if (Expression.literal.GetLength() == 0)
				return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Literal expression has no normalized value");
			break;
		case asETypedSemanticExpressionKind::FoldedGlobalConstant:
			if (Expression.resolvedGlobalPropertyId < 0
				|| Expression.operands.GetLength() != 0)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Folded global constant requires one engine-local property coordinate and no operands");
			}
			break;
		case asETypedSemanticExpressionKind::GlobalStorage:
			if (Expression.resolvedGlobalPropertyId < 0
				|| Expression.operands.GetLength() != 0)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Global storage requires one engine-local property coordinate and no operands");
			}
			break;
		case asETypedSemanticExpressionKind::Symbol:
			if (!IsSymbolInRange(Function, Expression.symbol))
				return MakeExpressionError(asETypedSemanticVerificationError::DanglingSymbolId, Expression, "Symbol expression references an unknown symbol");
			break;
		case asETypedSemanticExpressionKind::Conversion:
			if (Expression.operands.GetLength() != 1
				|| Expression.operands[0].value >= Index
				|| Expression.evaluationSteps.GetLength() != 1
				|| Expression.evaluationSteps[0].role
					!= asETypedSemanticEvaluationRole::Operand
				|| Expression.evaluationSteps[0].expression
					!= Expression.operands[0])
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Conversion expression requires one previously captured operand");
			}
			break;
		case asETypedSemanticExpressionKind::Unary:
			if (Expression.operands.GetLength() != 1
				|| Expression.operatorToken.GetLength() == 0
				|| Expression.evaluationSteps.GetLength() != 1
				|| Expression.evaluationSteps[0].role
					!= asETypedSemanticEvaluationRole::Operand
				|| Expression.evaluationSteps[0].expression
					!= Expression.operands[0])
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Unary expression requires one operand and an operator");
			}
			break;
		case asETypedSemanticExpressionKind::Binary:
			if (Expression.operands.GetLength() != 2
				|| Expression.operatorToken.GetLength() == 0
				|| Expression.evaluationSteps.GetLength() != 2
				|| Expression.evaluationSteps[0].role
					!= asETypedSemanticEvaluationRole::Operand
				|| Expression.evaluationSteps[1].role
					!= asETypedSemanticEvaluationRole::Operand
				|| Expression.evaluationSteps[0].expression
					!= Expression.operands[0]
				|| Expression.evaluationSteps[1].expression
					!= Expression.operands[1])
				return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Binary expression requires two ordered operands and an operator");
			break;
		case asETypedSemanticExpressionKind::ShortCircuit:
			if (Expression.operands.GetLength() != 2
				|| Expression.operatorToken.GetLength() == 0
				|| Expression.evaluationSteps.GetLength() != 0)
				return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Short-circuit expression retains conditional operands instead of an eager sequence");
			break;
		case asETypedSemanticExpressionKind::Assignment:
		{
			const bool IsIncrementOrDecrement =
				Expression.operatorToken == "++"
				|| Expression.operatorToken == "--";
			if (Expression.operands.GetLength() != 0
				|| Expression.operatorToken.GetLength() == 0)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Assignment expression requires a dedicated target, value shape, and operator");
			}
			if (!IsExpressionInRange(Function, Expression.mutationTarget))
				return MakeExpressionError(asETypedSemanticVerificationError::DanglingExpressionId, Expression, "Assignment target is outside the expression arena");
			if (Expression.mutationTarget.value >= Index)
				return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Assignment target must be captured before the mutation node");
			if (!Function.expressions[Expression.mutationTarget.value].type
					.IsEqualExceptRefAndConst(Expression.type))
			{
				return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Assignment result type must match its target type");
			}
			if (IsIncrementOrDecrement)
			{
				if (Expression.mutationValue.IsValid())
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Increment and decrement mutations do not carry a separate value");
				if (Expression.evaluationSteps.GetLength() != 1
					|| Expression.evaluationSteps[0].role
						!= asETypedSemanticEvaluationRole::MutationTarget
					|| Expression.evaluationSteps[0].expression
						!= Expression.mutationTarget)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"Increment/decrement must evaluate its mutable target exactly once");
				}
			}
			else
			{
				if (!IsExpressionInRange(Function, Expression.mutationValue))
					return MakeExpressionError(asETypedSemanticVerificationError::DanglingExpressionId, Expression, "Assignment value is outside the expression arena");
				if (Expression.mutationValue.value
						>= Expression.mutationTarget.value)
				{
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Assignment value must precede its target to preserve RHS-before-LHS evaluation");
				}
				if (Expression.mutationReturnsOldValue)
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Only postfix increment or decrement may return the old value");
				if (Expression.evaluationSteps.GetLength() != 2
					|| Expression.evaluationSteps[0].role
						!= asETypedSemanticEvaluationRole::MutationValue
					|| Expression.evaluationSteps[0].expression
						!= Expression.mutationValue
					|| Expression.evaluationSteps[1].role
						!= asETypedSemanticEvaluationRole::MutationTarget
					|| Expression.evaluationSteps[1].expression
						!= Expression.mutationTarget)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"Assignment must explicitly evaluate RHS before its single mutable target");
				}
			}
			break;
		}
		case asETypedSemanticExpressionKind::ResolvedCall:
		{
			if (Expression.callTargetKind
				== asETypedSemanticCallTargetKind::Unspecified)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Resolved call requires an explicit target kind");
			}
			if (Expression.resolvedFunctionId < 0)
				return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Resolved call requires an engine-local function ID");
			if (const char* const TargetProvenanceError =
				ValidateCallTargetProvenance(Function, Expression))
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidCallTargetProvenance,
					Expression,
					TargetProvenanceError);
			}
			if (Expression.callRewriteKind
				!= asETypedSemanticCompileOutKind::CompileCalls)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Executable resolved calls must retain the CompileCalls disposition");
			}
			if (Expression.formalParameterIndices.GetLength()
					!= Expression.operands.GetLength()
				|| Expression.sourceArgumentOrdinals.GetLength()
					!= Expression.operands.GetLength()
				|| Expression.evaluationSequence.GetLength()
					!= Expression.operands.GetLength()
				|| Expression.argumentOrigins.GetLength()
					!= Expression.operands.GetLength()
				|| Expression.argumentProvenance.GetLength()
					!= Expression.operands.GetLength())
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidArgumentProvenance,
					Expression,
					"Resolved call requires one formal binding, provenance record, and evaluation step per operand");
			}
			if ((Expression.hiddenArgumentIndex < -1)
				|| (Expression.hiddenArgumentIndex >= 0
					&& asUINT(Expression.hiddenArgumentIndex)
						>= Expression.operands.GetLength())
				|| (Expression.determinesOutputTypeArgumentIndex < -1)
				|| (Expression.determinesOutputTypeArgumentIndex >= 0
					&& asUINT(Expression.determinesOutputTypeArgumentIndex)
						>= Expression.operands.GetLength()))
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
					Expression,
					"Resolved call hidden/determines-output indices must address exact final formals");
			}
			if (Expression.callTargetKind
					== asETypedSemanticCallTargetKind::SystemFunction)
			{
				if (!Expression.nativeCall.isSystemFunction
					|| Expression.nativeCall.callConvention < 0
					|| Expression.nativeCall.scriptReturnSize < 0
					|| Expression.nativeCall.parameterSize < 0)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"System call requires a pointer-free native ABI requirement record");
				}
				switch (Expression.nativeCall.firstParamMetadata)
				{
				case asETypedSemanticFirstParamMetadata::None:
				case asETypedSemanticFirstParamMetadata::ScriptFunction:
				case asETypedSemanticFirstParamMetadata::ScriptObjectType:
					break;
				default:
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"System call first-parameter metadata is outside the stable contract");
				}
			}
			else if (Expression.nativeCall.isSystemFunction)
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Non-system call must not claim native ABI-only requirements");
			}
			{
				asCArray<asBYTE> SeenFormals;
				asCArray<asBYTE> SeenEvaluationIndices;
				asCArray<asBYTE> SeenSourceOrdinals;
				asUINT ReceiverOperandCount = 0;
				asUINT HiddenOperandCount = 0;
				int ReceiverFormal = -1;
				SeenFormals.SetLength(Expression.operands.GetLength());
				SeenEvaluationIndices.SetLength(
					Expression.operands.GetLength());
				SeenSourceOrdinals.SetLength(
					Expression.operands.GetLength());
				for (asUINT SlotIndex = 0;
					SlotIndex < Expression.operands.GetLength(); ++SlotIndex)
				{
					SeenFormals[SlotIndex] = 0;
					SeenEvaluationIndices[SlotIndex] = 0;
					SeenSourceOrdinals[SlotIndex] = 0;
				}
				for (asUINT BindingIndex = 0;
					BindingIndex < Expression.operands.GetLength();
					++BindingIndex)
				{
					const int Formal =
						Expression.formalParameterIndices[BindingIndex];
					const asUINT EvaluationIndex =
						Expression.evaluationSequence[BindingIndex];
					const asETypedSemanticArgumentOrigin Origin =
						Expression.argumentOrigins[BindingIndex];
					const asSTypedSemanticArgumentProvenance& Provenance =
						Expression.argumentProvenance[BindingIndex];
					const int SourceOrdinal =
						Expression.sourceArgumentOrdinals[BindingIndex];
					if (Formal < 0
						|| asUINT(Formal) >= SeenFormals.GetLength()
						|| SeenFormals[Formal] != 0
						|| EvaluationIndex
							>= SeenEvaluationIndices.GetLength()
						|| SeenEvaluationIndices[EvaluationIndex] != 0
						|| SourceOrdinal < -1
						|| (SourceOrdinal >= 0
							&& (asUINT(SourceOrdinal)
									>= SeenSourceOrdinals.GetLength()
								|| SeenSourceOrdinals[SourceOrdinal] != 0)))
					{
						return MakeExpressionError(
							asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
							Expression,
							"Resolved call formal bindings and evaluation steps must be complete unique permutations");
					}
					SeenFormals[Formal] = 1;
					SeenEvaluationIndices[EvaluationIndex] = 1;
					if (SourceOrdinal >= 0)
						SeenSourceOrdinals[SourceOrdinal] = 1;
					if (Provenance.origin != Origin
						|| Provenance.sourceArgumentOrdinal != SourceOrdinal
						|| !IsValidSpan(Provenance.processedSpan)
						|| !IsValidSourceProvenance(Provenance.processedSpan))
					{
						return MakeExpressionError(
							asETypedSemanticVerificationError::InvalidArgumentProvenance,
							Expression,
							"Argument provenance must agree with compatibility projections and retain a processed compiler span");
					}
					switch (Origin)
					{
					case asETypedSemanticArgumentOrigin::SourcePositional:
						if (Formal == Expression.hiddenArgumentIndex
							|| SourceOrdinal < 0
							|| Provenance.sourceArgumentName.GetLength() != 0
							|| Provenance.calleeFunctionId != -1
							|| Provenance.calleeDeclaration.GetLength() != 0
							|| Provenance.calleeParameterIndex != -1
							|| Provenance.calleeParameterName.GetLength() != 0
							|| Provenance.canonicalExpression.GetLength() != 0)
						{
							return MakeExpressionError(
								asETypedSemanticVerificationError::InvalidArgumentProvenance,
								Expression,
								"A positional source argument requires a written ordinal and no callee-default metadata");
						}
						break;
					case asETypedSemanticArgumentOrigin::SourceNamed:
						if (Formal == Expression.hiddenArgumentIndex
							|| SourceOrdinal < 0
							|| Provenance.sourceArgumentName.GetLength() == 0
							|| Provenance.calleeFunctionId != -1
							|| Provenance.calleeDeclaration.GetLength() != 0
							|| Provenance.calleeParameterIndex != -1
							|| Provenance.calleeParameterName.GetLength() != 0
							|| Provenance.canonicalExpression.GetLength() != 0)
						{
							return MakeExpressionError(
								asETypedSemanticVerificationError::InvalidArgumentProvenance,
								Expression,
								"A named source argument requires its written name/ordinal and no callee-default metadata");
						}
						break;
					case asETypedSemanticArgumentOrigin::Default:
						if (Formal == Expression.hiddenArgumentIndex
							|| SourceOrdinal != -1
							|| Provenance.sourceArgumentName.GetLength() != 0
							|| Provenance.calleeFunctionId
								!= Expression.resolvedFunctionId
							|| Provenance.calleeDeclaration.GetLength() == 0
							|| Provenance.calleeParameterIndex != Formal
							|| Provenance.canonicalExpression.GetLength() == 0)
						{
							return MakeExpressionError(
								asETypedSemanticVerificationError::InvalidArgumentProvenance,
								Expression,
								"A default argument requires exact callee/formal/canonical-expression provenance and no source ordinal");
						}
						break;
					case asETypedSemanticArgumentOrigin::HostHidden:
						if (Formal != Expression.hiddenArgumentIndex
							|| SourceOrdinal != -1
							|| Provenance.sourceArgumentName.GetLength() != 0
							|| Provenance.calleeFunctionId
								!= Expression.resolvedFunctionId
							|| Provenance.calleeDeclaration.GetLength() == 0
							|| Provenance.calleeParameterIndex != Formal
							|| Provenance.canonicalExpression.GetLength() == 0)
						{
							return MakeExpressionError(
								asETypedSemanticVerificationError::InvalidArgumentProvenance,
								Expression,
								"HostHidden origin must retain exact target/formal/canonical-expression provenance");
						}
						++HiddenOperandCount;
						break;
					case asETypedSemanticArgumentOrigin::Receiver:
						if (!Expression.receiverExpression.IsValid()
							|| Expression.operands[BindingIndex]
								!= Expression.receiverExpression
							|| SourceOrdinal != -1
							|| Provenance.sourceArgumentName.GetLength() != 0
							|| Provenance.calleeFunctionId != -1
							|| Provenance.calleeDeclaration.GetLength() != 0
							|| Provenance.calleeParameterIndex != -1
							|| Provenance.calleeParameterName.GetLength() != 0
							|| Provenance.canonicalExpression.GetLength() != 0)
						{
							return MakeExpressionError(
								asETypedSemanticVerificationError::InvalidArgumentProvenance,
								Expression,
								"Receiver origin must reference the effective receiver without source/default metadata");
						}
						break;
					default:
						return MakeExpressionError(
							asETypedSemanticVerificationError::InvalidArgumentProvenance,
							Expression,
							"Call argument origin is outside the stable contract");
					}
					if (Expression.receiverExpression.IsValid()
						&& Expression.operands[BindingIndex]
							== Expression.receiverExpression)
					{
						++ReceiverOperandCount;
						ReceiverFormal = Formal;
					}
				}
				if (ReceiverOperandCount != 0
					&& (ReceiverOperandCount != 1 || ReceiverFormal != 0))
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
						Expression,
						"A call receiver that aliases an effective argument must uniquely map to formal zero");
				}
				if ((Expression.hiddenArgumentIndex >= 0
						&& HiddenOperandCount != 1)
					|| (Expression.hiddenArgumentIndex < 0
						&& HiddenOperandCount != 0))
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
						Expression,
						"Resolved call must retain exactly one origin for its hidden formal");
				}

				const asUINT ExpectedStepCount =
					Expression.operands.GetLength()
					+ (Expression.receiverExpression.IsValid()
						&& ReceiverOperandCount == 0 ? 1u : 0u);
				if (Expression.evaluationSteps.GetLength()
					!= ExpectedStepCount)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
						Expression,
						"Resolved call evaluation steps must cover every formal and its independent receiver exactly once");
				}
				for (asUINT EvaluationOrdinal = 0;
					EvaluationOrdinal < Expression.operands.GetLength();
					++EvaluationOrdinal)
				{
					asUINT BindingIndex = Expression.operands.GetLength();
					for (asUINT CandidateIndex = 0;
						CandidateIndex < Expression.operands.GetLength();
						++CandidateIndex)
					{
						if (Expression.evaluationSequence[CandidateIndex]
							== EvaluationOrdinal)
						{
							BindingIndex = CandidateIndex;
							break;
						}
					}
					if (BindingIndex >= Expression.operands.GetLength())
					{
						return MakeExpressionError(
							asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
							Expression,
							"Resolved call evaluation projection is incomplete");
					}
					const asETypedSemanticArgumentOrigin Origin =
						Expression.argumentOrigins[BindingIndex];
					const asETypedSemanticEvaluationRole ExpectedRole =
						Origin == asETypedSemanticArgumentOrigin::Receiver
							? asETypedSemanticEvaluationRole::Receiver
							: Origin == asETypedSemanticArgumentOrigin::HostHidden
								? asETypedSemanticEvaluationRole::HiddenArgument
								: asETypedSemanticEvaluationRole::Argument;
					const asSTypedSemanticEvaluationStep& Step =
						Expression.evaluationSteps[EvaluationOrdinal];
					if (Step.role != ExpectedRole
						|| Step.expression != Expression.operands[BindingIndex]
						|| Step.formalParameterIndex
							!= Expression.formalParameterIndices[BindingIndex]
						|| Step.sourceArgumentOrdinal
							!= Expression.sourceArgumentOrdinals[BindingIndex])
					{
						return MakeExpressionError(
							asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
							Expression,
							"Resolved call evaluation step disagrees with its source role or formal binding");
					}
				}
				if (Expression.receiverExpression.IsValid()
					&& ReceiverOperandCount == 0)
				{
					const asSTypedSemanticEvaluationStep& ReceiverStep =
						Expression.evaluationSteps[
							Expression.evaluationSteps.GetLength() - 1];
					if (ReceiverStep.role
							!= asETypedSemanticEvaluationRole::Receiver
						|| ReceiverStep.expression
							!= Expression.receiverExpression
						|| ReceiverStep.formalParameterIndex != -1
						|| ReceiverStep.sourceArgumentOrdinal != -1)
					{
						return MakeExpressionError(
							asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
							Expression,
							"Independent receiver must be the final explicit receiver evaluation step");
					}
				}
			}
			break;
		}
		case asETypedSemanticExpressionKind::CallRewrite:
		{
			if (Expression.callTargetKind
					== asETypedSemanticCallTargetKind::Unspecified
				|| Expression.resolvedFunctionId < 0
				|| Expression.callRewriteKind
					== asETypedSemanticCompileOutKind::CompileCalls
				|| Expression.formalParameterIndices.GetLength() != 0
				|| Expression.nativeCall.isSystemFunction
				|| Expression.argumentOrigins.GetLength()
					!= Expression.operands.GetLength()
				|| Expression.argumentProvenance.GetLength() != 0
				|| Expression.sourceArgumentOrdinals.GetLength()
					!= Expression.operands.GetLength()
				|| Expression.evaluationSequence.GetLength()
					!= Expression.operands.GetLength()
				|| Expression.evaluationSteps.GetLength()
					!= Expression.operands.GetLength())
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Final call rewrite requires non-executable target provenance and one origin/evaluation entry per retained value");
			}
			if (const char* const TargetProvenanceError =
				ValidateCallTargetProvenance(Function, Expression))
			{
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidCallTargetProvenance,
					Expression,
					TargetProvenanceError);
			}
			for (asUINT OperandIndex = 0;
				OperandIndex < Expression.operands.GetLength(); ++OperandIndex)
			{
				if (Expression.operands[OperandIndex].value >= Index
					|| Expression.evaluationSequence[OperandIndex]
						!= OperandIndex
					|| Expression.sourceArgumentOrdinals[OperandIndex]
						!= -1
					|| Expression.evaluationSteps[OperandIndex].expression
						!= Expression.operands[OperandIndex]
					|| Expression.evaluationSteps[OperandIndex]
						.formalParameterIndex != -1
					|| Expression.evaluationSteps[OperandIndex]
						.sourceArgumentOrdinal != -1
					|| Expression.evaluationSteps[OperandIndex].role
						!= (Expression.argumentOrigins[OperandIndex]
								== asETypedSemanticArgumentOrigin::Receiver
							? asETypedSemanticEvaluationRole::Receiver
							: asETypedSemanticEvaluationRole::Argument))
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
						Expression,
						"Call rewrite retained values must be captured once in exact final order");
				}
			}
			switch (Expression.callRewriteKind)
			{
			case asETypedSemanticCompileOutKind::CompileOutEntirely:
				if (Expression.type.GetTokenType() != ttVoid
					|| Expression.operands.GetLength() != 0
					|| Expression.receiverExpression.IsValid())
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"CompileOutEntirely must retain only a final void marker");
				}
				break;
			case asETypedSemanticCompileOutKind::ReplaceWithFirstParam:
				if (Expression.operands.GetLength() != 1
					|| Expression.receiverExpression.IsValid()
					|| Expression.argumentOrigins[0]
						!= asETypedSemanticArgumentOrigin::SourcePositional)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"ReplaceWithFirstParam must retain exactly the selected source value");
				}
				break;
			case asETypedSemanticCompileOutKind::CompileOutAsMethodChain:
				if (Expression.operands.GetLength() > 1
					|| (Expression.operands.GetLength() == 0
						&& Expression.receiverExpression.IsValid())
					|| (Expression.operands.GetLength() == 1
						&& (!Expression.receiverExpression.IsValid()
							|| Expression.receiverExpression
								!= Expression.operands[0]
							|| Expression.argumentOrigins[0]
								!= asETypedSemanticArgumentOrigin::Receiver)))
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"CompileOutAsMethodChain must retain at most its exact receiver value");
				}
				break;
			default:
				return MakeExpressionError(
					asETypedSemanticVerificationError::InvalidNodeShape,
					Expression,
					"Call rewrite disposition is outside the stable contract");
			}
			break;
		}
		case asETypedSemanticExpressionKind::Unsupported:
			if (Expression.unsupportedKind == asETypedSemanticUnsupportedKind::None)
				return MakeExpressionError(asETypedSemanticVerificationError::MissingUnsupportedCategory, Expression, "Unsupported expression requires a stable category");
			if (Expression.unsupportedKind == asETypedSemanticUnsupportedKind::Ternary)
			{
				if (Expression.operands.GetLength() != 3)
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Ternary unsupported expression requires condition, true, and false operands");
				for (asUINT OperandIndex = 0;
					OperandIndex < Expression.operands.GetLength();
					++OperandIndex)
				{
					if (Expression.operands[OperandIndex].value >= Index)
						return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Ternary operands must be captured before the unsupported marker");
				}
			}
			else if (Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::ObjectAccess
				|| Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::PropertyAccess)
			{
				if (Expression.operands.GetLength() != 1
					|| Expression.operands[0].value >= Index)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"Object and property access markers require one previously captured receiver operand");
				}
				if (Expression.resolvedTarget.GetLength() == 0)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"Object and property access markers require pointer-free resolved target detail");
				}
			}
			else if (Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::Reference
				|| Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::Handle
				|| Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::Container)
			{
				if (Expression.operands.GetLength() != 0)
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Reference, handle, and container type markers must not carry executable operands");
				if (!IsSymbolInRange(Function, Expression.symbol))
					return MakeExpressionError(asETypedSemanticVerificationError::DanglingSymbolId, Expression, "Reference, handle, and container type markers require their source symbol");
				const asSTypedSemanticSymbol& SourceSymbol =
					Function.symbols[Expression.symbol.value];
				if (Expression.type != SourceSymbol.type)
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Reference, handle, and container type marker must preserve the exact source-symbol type");
				if (Expression.unsupportedKind
						== asETypedSemanticUnsupportedKind::Reference
					&& !Expression.type.IsReference())
				{
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Reference marker requires a reference type");
				}
				if (Expression.unsupportedKind
						== asETypedSemanticUnsupportedKind::Handle
					&& !Expression.type.IsObjectHandle())
				{
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Handle marker requires an object-handle type");
				}
				if (Expression.unsupportedKind
						== asETypedSemanticUnsupportedKind::Container
					&& !IsTypedSemanticContainerType(Expression.type))
				{
					return MakeExpressionError(asETypedSemanticVerificationError::InvalidNodeShape, Expression, "Container marker requires an array, TArray, TSet, or TMap type");
				}
			}
			else if (Expression.unsupportedKind
				== asETypedSemanticUnsupportedKind::ConstructionOrLifetime)
			{
				if (Expression.operands.GetLength() > 1
					|| Expression.symbol.IsValid()
					|| Expression.type.GetTypeInfo() == 0
					|| Expression.type.IsFuncdef()
					|| Expression.resolvedTarget.GetLength() == 0
					|| Expression.evaluationSteps.GetLength()
						!= Expression.operands.GetLength()
					|| (Expression.operands.GetLength() == 1
						&& (Function.expressions[
								Expression.operands[0].value].kind
								!= asETypedSemanticExpressionKind::ResolvedCall
							|| Expression.evaluationSteps[0].role
								!= asETypedSemanticEvaluationRole::Operand
							|| Expression.evaluationSteps[0].expression
								!= Expression.operands[0])))
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"Construction/lifetime marker requires one pointer-free object type and at most its selected constructor call");
				}
			}
			else if (Expression.unsupportedKind
				== asETypedSemanticUnsupportedKind::ExceptionCleanup)
			{
				const bool IsManagedCleanup =
					Expression.resolvedTarget == "managed-cleanup";
				const bool IsCompilerExceptionRegion =
					Expression.resolvedTarget
						== "compiler-exception-region";
				if (Expression.operands.GetLength() != 0
					|| Expression.symbol.IsValid()
					|| Expression.type.GetTokenType() != ttVoid
					|| (!IsManagedCleanup && !IsCompilerExceptionRegion)
					|| !Function.header.hasExceptionCleanup
					|| (IsCompilerExceptionRegion
						&& Function.header.cleanupPlanState
							!= asETypedSemanticCleanupPlanState::CompilerExceptionRegion))
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"Exception-cleanup marker requires a stable cleanup target, matching function state, and no executable payload");
				}
				HasCompilerExceptionRegionMarker |=
					IsCompilerExceptionRegion;
			}
			else if (Expression.unsupportedKind
				== asETypedSemanticUnsupportedKind::CompilerSynthesizedFunction)
			{
				if (Expression.operands.GetLength() != 0
					|| Expression.symbol.IsValid()
					|| Expression.type.GetTokenType() != ttVoid
					|| Expression.resolvedTarget != "__generated"
					|| (Function.header.declaredTraitBits
						& asTRAIT_GENERATED_FUNCTION) == 0)
				{
					return MakeExpressionError(
						asETypedSemanticVerificationError::InvalidNodeShape,
						Expression,
						"Compiler-synthesized marker requires the generated-function trait and no executable payload");
				}
			}
			break;
		default:
			break;
		}
	}
	if (Header.cleanupPlanState
			== asETypedSemanticCleanupPlanState::CompilerExceptionRegion
		&& !HasCompilerExceptionRegionMarker)
	{
		return MakeStatementError(
			asETypedSemanticVerificationError::MissingUnsupportedCategory,
			Function.statements[Function.rootStatement.value],
			"Compiler exception region requires a stable unsupported marker");
	}

	asCArray<asUINT> OwnerCounts;
	OwnerCounts.SetLength(Function.statements.GetLength());
	asCArray<asTypedSemanticStatementId> ParentStatements;
	ParentStatements.SetLength(Function.statements.GetLength());
	for (asUINT Index = 0; Index < OwnerCounts.GetLength(); ++Index)
	{
		OwnerCounts[Index] = 0;
		ParentStatements[Index] = asTypedSemanticStatementId();
	}

	for (asUINT Index = 0; Index < Function.statements.GetLength(); ++Index)
	{
		const asSTypedSemanticStatement& Statement = Function.statements[Index];
		if (!Statement.id.IsValid() || Statement.id.value != Index)
			return MakeStatementError(asETypedSemanticVerificationError::NonContiguousStatementId, Statement, "Statement ID does not match its arena index");
		if (!IsValidSpan(Statement.span))
			return MakeStatementError(asETypedSemanticVerificationError::InvalidSourceSpan, Statement, "Statement source span is invalid");
		if (!IsValidSourceProvenance(Statement.span))
			return MakeStatementError(asETypedSemanticVerificationError::InvalidSourceProvenance, Statement, "Statement source provenance is malformed");

		asCArray<asTypedSemanticStatementId> Owned;
		CollectOwnedStatements(Statement, Owned);
		for (asUINT OwnedIndex = 0; OwnedIndex < Owned.GetLength(); ++OwnedIndex)
		{
			if (!IsStatementInRange(Function, Owned[OwnedIndex]))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "Owned statement is outside the arena");
			if (OwnerCounts[Owned[OwnedIndex].value] == 0)
				ParentStatements[Owned[OwnedIndex].value] = Statement.id;
			if (++OwnerCounts[Owned[OwnedIndex].value] > 1)
				return MakeStatementError(asETypedSemanticVerificationError::StatementHasMultipleOwners, Statement, "Statement has more than one lexical owner");
		}

		switch (Statement.kind)
		{
		case asETypedSemanticStatementKind::LocalDeclaration:
			if (!IsSymbolInRange(Function, Statement.symbol))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingSymbolId, Statement, "Local declaration references an unknown symbol");
			if (!IsExpressionInRange(Function, Statement.initializerExpression))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "Local initializer is outside the expression arena");
			break;
		case asETypedSemanticStatementKind::Expression:
			if (Statement.valueExpression.IsValid()
				&& !IsExpressionInRange(Function, Statement.valueExpression))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "Statement value is outside the expression arena");
			break;
		case asETypedSemanticStatementKind::Return:
			if (Statement.valueExpression.IsValid()
				&& !IsExpressionInRange(Function, Statement.valueExpression))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "Return value is outside the expression arena");
			if (Statement.safePointRole
				!= asETypedSemanticSafePointRole::Return)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Return must retain its explicit return safe-point role");
			if (Header.cleanupPlanState
				== asETypedSemanticCleanupPlanState::Unverified)
			{
				if (Statement.cleanupPlan.IsValid())
					return MakeStatementError(asETypedSemanticVerificationError::InvalidCleanupPlan, Statement, "Unverified return cleanup cannot claim a compiler-authored plan");
			}
			else
			{
				if (!Statement.cleanupPlan.IsValid()
					|| Statement.cleanupPlan.value
						>= Function.cleanupPlans.GetLength())
					return MakeStatementError(asETypedSemanticVerificationError::InvalidCleanupPlan, Statement, "Return requires a compiler-authored cleanup plan");
				if (Header.cleanupPlanCoversAllTransfers
					&& Statement.cleanupPlan != Header.universalCleanupPlan)
					return MakeStatementError(asETypedSemanticVerificationError::InvalidCleanupPlan, Statement, "Return must reference the claimed universal cleanup plan");
			}
			if (Statement.exitedScopeCount == 0)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidCleanupPlan, Statement, "Return must record every exited compiler variable scope");
			break;
		case asETypedSemanticStatementKind::If:
			if (!IsExpressionInRange(Function, Statement.conditionExpression))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "If condition is outside the expression arena");
			if (!IsStatementInRange(Function, Statement.thenStatement))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "If then branch is outside the statement arena");
			break;
		case asETypedSemanticStatementKind::While:
			if (!IsExpressionInRange(Function, Statement.conditionExpression))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "While condition is outside the expression arena");
			if (!Function.expressions[Statement.conditionExpression.value].type.IsBooleanType())
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "While condition must retain its compiler-resolved bool type");
			if (!IsStatementInRange(Function, Statement.bodyStatement))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "While body is outside the statement arena");
			if (Function.statements[Statement.bodyStatement.value].kind
				!= asETypedSemanticStatementKind::Block)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "While body must be normalized to an owned block");
			if (Statement.loopPhase != asETypedSemanticLoopPhase::Condition
				|| Statement.safePointRole
					!= asETypedSemanticSafePointRole::LoopBackedge)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "While must identify its condition-targeting loop backedge");
			if (Function.statements[Statement.bodyStatement.value].loopPhase
					!= asETypedSemanticLoopPhase::Body
				|| Function.statements[Statement.bodyStatement.value].safePointRole
					!= asETypedSemanticSafePointRole::LoopEntry)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "While body must identify its loop-entry phase");
			break;
		case asETypedSemanticStatementKind::DoWhile:
			if (!IsStatementInRange(Function, Statement.bodyStatement))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "Do-while body is outside the statement arena");
			if (Function.statements[Statement.bodyStatement.value].kind
				!= asETypedSemanticStatementKind::Block)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Do-while body must be normalized to an owned block");
			if (!IsExpressionInRange(Function, Statement.conditionExpression))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "Do-while condition is outside the expression arena");
			if (!Function.expressions[Statement.conditionExpression.value].type.IsBooleanType())
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Do-while condition must retain its compiler-resolved bool type");
			if (Statement.loopPhase != asETypedSemanticLoopPhase::Condition
				|| Statement.safePointRole
					!= asETypedSemanticSafePointRole::LoopBackedge)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Do-while must identify its trailing-condition backedge");
			if (Function.statements[Statement.bodyStatement.value].loopPhase
					!= asETypedSemanticLoopPhase::Body
				|| Function.statements[Statement.bodyStatement.value].safePointRole
					!= asETypedSemanticSafePointRole::LoopEntry)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Do-while body must identify its body-first loop-entry phase");
			break;
		case asETypedSemanticStatementKind::For:
			if (Statement.initializerStatement.IsValid())
			{
				if (!IsStatementInRange(Function, Statement.initializerStatement))
					return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "For initializer is outside the statement arena");
				if (Function.statements[Statement.initializerStatement.value].loopPhase
					!= asETypedSemanticLoopPhase::Initializer)
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For initializer must identify its initializer phase");
			}
			if (Statement.conditionExpression.IsValid())
			{
				if (!IsExpressionInRange(Function, Statement.conditionExpression))
					return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "For condition is outside the expression arena");
				if (!Function.expressions[Statement.conditionExpression.value].type.IsBooleanType())
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For condition must retain its compiler-resolved bool type");
			}
			if (Statement.incrementStatement.IsValid())
			{
				if (!IsStatementInRange(Function, Statement.incrementStatement))
					return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "For increment phase is outside the statement arena");
				const asSTypedSemanticStatement& Increment =
					Function.statements[Statement.incrementStatement.value];
				if (Increment.kind != asETypedSemanticStatementKind::Block
					|| Increment.loopPhase != asETypedSemanticLoopPhase::Increment)
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For increment phase must be normalized to an owned increment block");
				if (Increment.children.GetLength()
					!= Statement.incrementExpressions.GetLength())
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For increment block and ordered expression list must have identical lengths");
				for (asUINT IncrementIndex = 0;
					IncrementIndex < Increment.children.GetLength();
					++IncrementIndex)
				{
					if (!IsStatementInRange(Function, Increment.children[IncrementIndex]))
						return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "For increment child is outside the statement arena");
					if (!IsExpressionInRange(Function, Statement.incrementExpressions[IncrementIndex]))
						return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "For increment expression is outside the expression arena");
					const asSTypedSemanticStatement& IncrementChild =
						Function.statements[Increment.children[IncrementIndex].value];
					if (IncrementChild.kind
							!= asETypedSemanticStatementKind::Expression
						|| IncrementChild.valueExpression
							!= Statement.incrementExpressions[IncrementIndex])
						return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For ordered increment list must exactly mirror its expression statements");
				}
			}
			else if (Statement.incrementExpressions.GetLength() != 0)
			{
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For without an increment statement cannot retain increment expressions");
			}
			if (!IsStatementInRange(Function, Statement.bodyStatement))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "For body is outside the statement arena");
			if (Function.statements[Statement.bodyStatement.value].kind
				!= asETypedSemanticStatementKind::Block)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For body must be normalized to an owned block");
			if (Statement.loopPhase != asETypedSemanticLoopPhase::Condition
				|| Statement.safePointRole
					!= asETypedSemanticSafePointRole::LoopBackedge)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For must identify its condition-targeting loop backedge");
			if (Function.statements[Statement.bodyStatement.value].loopPhase
					!= asETypedSemanticLoopPhase::Body
				|| Function.statements[Statement.bodyStatement.value].safePointRole
					!= asETypedSemanticSafePointRole::LoopEntry)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "For body must identify its loop-entry phase");
			break;
		case asETypedSemanticStatementKind::Switch:
		{
			if (!IsExpressionInRange(Function, Statement.conditionExpression))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "Switch selector is outside the expression arena");
			const asCDataType& SelectorType =
				Function.expressions[Statement.conditionExpression.value].type;
			if (!SelectorType.IsIntegerType()
				&& !SelectorType.IsUnsignedType())
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch selector must retain its compiler-resolved integral or enum type");
			if (Statement.switchNormalization
					== asETypedSemanticSwitchNormalization::SignedInt32
				&& !SelectorType.IsIntegerType())
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Signed switch normalization requires a signed integral or enum selector");
			if (Statement.switchNormalization
					== asETypedSemanticSwitchNormalization::UnsignedInt32
				&& !SelectorType.IsUnsignedType())
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Unsigned switch normalization requires an unsigned selector");
			if (Statement.switchNormalization
				== asETypedSemanticSwitchNormalization::None)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch must record its compiler-selected 32-bit normalization");
			if (Statement.children.GetLength() == 0)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch requires at least one ordered case");

			asUINT DefaultCount = 0;
			for (asUINT CaseIndex = 0;
				CaseIndex < Statement.children.GetLength();
				++CaseIndex)
			{
				const asTypedSemanticStatementId CaseId =
					Statement.children[CaseIndex];
				if (!IsStatementInRange(Function, CaseId))
					return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "Switch case is outside the statement arena");
				const asSTypedSemanticStatement& Case =
					Function.statements[CaseId.value];
				if (Case.kind != asETypedSemanticStatementKind::Case)
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch children must be ordered case statements");
				if (Case.isDefaultCase)
				{
					++DefaultCount;
					if (CaseIndex + 1 != Statement.children.GetLength())
						return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Default case must be last in source order");
				}
				else
				{
					for (asUINT PreviousIndex = 0;
						PreviousIndex < CaseIndex;
						++PreviousIndex)
					{
						const asSTypedSemanticStatement& Previous =
							Function.statements[
								Statement.children[PreviousIndex].value];
						if (!Previous.isDefaultCase
							&& Previous.caseConstantValue
								== Case.caseConstantValue)
							return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch case constants must be unique after normalization");
					}
				}
				if (Case.fallsThrough
					&& CaseIndex + 1 == Statement.children.GetLength())
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Final switch case cannot fall through past the switch");
			}

			if ((DefaultCount != 0) != Statement.switchHasDefault
				|| DefaultCount > 1)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch default metadata must match its ordered cases");
			if (Statement.switchHasDefault && !Statement.switchIsExhaustive)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch with a default case is exhaustive");
			const bool ExpectedInvalidEnumEdge =
				!Statement.switchHasDefault
				&& Statement.switchIsExhaustive
				&& SelectorType.IsEnumType();
			if (Statement.switchHasInvalidEnumValueEdge
				!= ExpectedInvalidEnumEdge)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Exhaustive enum switch without default must retain its invalid-value edge");
			if (Statement.safePointRole
				!= (ExpectedInvalidEnumEdge
					? asETypedSemanticSafePointRole::SwitchInvalidValue
					: asETypedSemanticSafePointRole::Statement))
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Switch safe-point role must match its invalid-value edge");
			break;
		}
		case asETypedSemanticStatementKind::Case:
			if (!IsStatementInRange(Function, Statement.bodyStatement))
				return MakeStatementError(asETypedSemanticVerificationError::DanglingStatementId, Statement, "Case body is outside the statement arena");
			if (Function.statements[Statement.bodyStatement.value].kind
				!= asETypedSemanticStatementKind::Block)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Case body must be normalized to an owned block");
			if (Statement.isDefaultCase)
			{
				if (Statement.valueExpression.IsValid())
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Default case cannot retain a value expression");
			}
			else if (!IsExpressionInRange(Function, Statement.valueExpression))
			{
				return MakeStatementError(asETypedSemanticVerificationError::DanglingExpressionId, Statement, "Non-default case value is outside the expression arena");
			}
			else
			{
				const asCDataType& CaseValueType =
					Function.expressions[Statement.valueExpression.value].type;
				if (!CaseValueType.IsIntegerType()
					&& !CaseValueType.IsUnsignedType())
					return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Non-default case value must retain its normalized integral or enum type");
			}
			if (Statement.hasExplicitFallthrough && !Statement.fallsThrough)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Explicit fallthrough must reach the next ordered case");
			if (Statement.safePointRole
				!= asETypedSemanticSafePointRole::Statement)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Case must retain its statement safe-point role");
			break;
		case asETypedSemanticStatementKind::Break:
		case asETypedSemanticStatementKind::Continue:
			if (Statement.safePointRole
				!= asETypedSemanticSafePointRole::Transfer)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Statement, "Break and continue must retain the transfer safe-point role");
			if (!Statement.cleanupPlan.IsValid()
				|| Statement.cleanupPlan.value
					>= Function.cleanupPlans.GetLength())
				return MakeStatementError(asETypedSemanticVerificationError::InvalidCleanupPlan, Statement, "Control transfer requires a compiler-authored cleanup plan");
			if (Header.cleanupPlanCoversAllTransfers
				&& Statement.cleanupPlan != Header.universalCleanupPlan)
				return MakeStatementError(asETypedSemanticVerificationError::InvalidCleanupPlan, Statement, "Control transfer must reference the claimed universal cleanup plan");
			break;
		case asETypedSemanticStatementKind::Unsupported:
			if (Statement.unsupportedKind == asETypedSemanticUnsupportedKind::None)
				return MakeStatementError(asETypedSemanticVerificationError::MissingUnsupportedCategory, Statement, "Unsupported statement requires a stable category");
			break;
		default:
			break;
		}
	}

	if (OwnerCounts[Function.rootStatement.value] != 0)
		return MakeStatementError(asETypedSemanticVerificationError::StatementOwnershipCycle, Function.statements[Function.rootStatement.value], "Root statement is owned by another statement");
	for (asUINT Index = 0; Index < OwnerCounts.GetLength(); ++Index)
	{
		if (Index != Function.rootStatement.value && OwnerCounts[Index] == 0)
			return MakeStatementError(asETypedSemanticVerificationError::InvalidNodeShape, Function.statements[Index], "Statement is not owned by the function root tree");
	}

	asCArray<asBYTE> States;
	States.SetLength(Function.statements.GetLength());
	for (asUINT Index = 0; Index < States.GetLength(); ++Index)
		States[Index] = 0;
	for (asUINT Index = 0; Index < States.GetLength(); ++Index)
	{
		asTypedSemanticStatementId Cycle;
		if (FindStatementCycle(Function, Index, States, Cycle))
			return MakeStatementError(asETypedSemanticVerificationError::StatementOwnershipCycle, Function.statements[Cycle.value], "Statement ownership contains a cycle");
	}

	for (asUINT Index = 0; Index < Function.statements.GetLength(); ++Index)
	{
		const asSTypedSemanticStatement& Transfer = Function.statements[Index];
		if (Transfer.kind != asETypedSemanticStatementKind::Break
			&& Transfer.kind != asETypedSemanticStatementKind::Continue)
		{
			continue;
		}

		if (!IsStatementInRange(Function, Transfer.targetStatement))
			return MakeStatementError(asETypedSemanticVerificationError::InvalidControlTarget, Transfer, "Control transfer target is outside the statement arena");

		asTypedSemanticStatementId NearestTarget;
		asTypedSemanticStatementId Ancestor = ParentStatements[Index];
		while (Ancestor.IsValid())
		{
			const asETypedSemanticStatementKind AncestorKind =
				Function.statements[Ancestor.value].kind;
			const bool IsLoop =
				AncestorKind == asETypedSemanticStatementKind::For
				|| AncestorKind == asETypedSemanticStatementKind::While
				|| AncestorKind == asETypedSemanticStatementKind::DoWhile;
			if (IsLoop
				|| (Transfer.kind == asETypedSemanticStatementKind::Break
					&& AncestorKind
						== asETypedSemanticStatementKind::Switch))
			{
				NearestTarget = Ancestor;
				break;
			}
			Ancestor = ParentStatements[Ancestor.value];
		}

		if (!NearestTarget.IsValid()
			|| Transfer.targetStatement != NearestTarget)
		{
			return MakeStatementError(
				asETypedSemanticVerificationError::InvalidControlTarget,
				Transfer,
				Transfer.kind == asETypedSemanticStatementKind::Continue
					? "Continue must target the nearest enclosing loop"
					: "Break must target the nearest enclosing loop or switch");
		}
	}

	const asSTypedSemanticEffectiveReceiver& Receiver = Function.header.receiver;
	const bool HasExternalImplicitThisTrait =
		(Function.header.declaredTraitBits & asTRAIT_EXTERNAL_IMPLICIT_THIS) != 0;
	const bool HasMixinTrait =
		(Function.header.declaredTraitBits & asTRAIT_MIXIN) != 0;
	const bool IsNativeReceiverInvocation =
		Function.header.invocationKind
			== asETypedSemanticInvocationKind::InstanceMethod
		|| Function.header.invocationKind
			== asETypedSemanticInvocationKind::Constructor
		|| Function.header.invocationKind
			== asETypedSemanticInvocationKind::Destructor;
	if (Receiver.kind == asETypedSemanticReceiverKind::None)
	{
		if (Receiver.symbol.IsValid() || Receiver.parameterIndex != -1 || Receiver.type.IsValid())
			return MakeReceiverError(
				Receiver,
				"None receiver must not retain a symbol, parameter, or type");
		if (HasExternalImplicitThisTrait || HasMixinTrait)
			return MakeReceiverError(
				Receiver,
				"Receiver-bearing function trait requires its normalized receiver");
		if (IsNativeReceiverInvocation)
			return MakeReceiverError(
				Receiver,
				"Instance, constructor, and destructor invocations require native object this");
	}
	else if (!IsSymbolInRange(Function, Receiver.symbol) || !Receiver.type.IsValid())
	{
		return MakeReceiverError(
			Receiver,
			"Effective receiver must reference an owned symbol and exact type");
	}
	else
	{
		const asSTypedSemanticSymbol& ReceiverSymbol =
			Function.symbols[Receiver.symbol.value];
		if (!Receiver.type.IsObject() || !ReceiverSymbol.type.IsObject())
			return MakeReceiverError(
				Receiver,
				"Effective receiver and its symbol must have object type");
		if (Receiver.type != ReceiverSymbol.type)
			return MakeReceiverError(
				Receiver,
				"Effective receiver type must exactly match its owned symbol");

		switch (Receiver.kind)
		{
		case asETypedSemanticReceiverKind::NativeObjectThis:
			if (ReceiverSymbol.kind != asETypedSemanticSymbolKind::Receiver
				|| Receiver.parameterIndex != -1)
				return MakeReceiverError(
					Receiver,
					"Native object this must be a synthetic non-parameter receiver symbol");
			if (HasExternalImplicitThisTrait || HasMixinTrait)
				return MakeReceiverError(
					Receiver,
					"Native object this conflicts with parameter-receiver traits");
			if (!IsNativeReceiverInvocation)
				return MakeReceiverError(
					Receiver,
					"Native object this requires an instance, constructor, or destructor invocation");
			break;

		case asETypedSemanticReceiverKind::ExternalImplicitThis:
			if (ReceiverSymbol.kind != asETypedSemanticSymbolKind::Parameter
				|| Receiver.parameterIndex != 0
				|| Receiver.symbol.value != 0)
				return MakeReceiverError(
					Receiver,
					"External implicit this must alias declared object parameter zero");
			if (!HasExternalImplicitThisTrait || HasMixinTrait)
				return MakeReceiverError(
					Receiver,
					"External implicit receiver requires only its authoritative trait");
			if (Function.header.invocationKind
				!= asETypedSemanticInvocationKind::Global)
				return MakeReceiverError(
					Receiver,
					"External implicit this remains a global invocation");
			break;

		case asETypedSemanticReceiverKind::MixinFirstParameter:
			if (ReceiverSymbol.kind != asETypedSemanticSymbolKind::Parameter
				|| Receiver.parameterIndex != 0
				|| Receiver.symbol.value != 0)
				return MakeReceiverError(
					Receiver,
					"Mixin receiver must alias declared object parameter zero");
			if (!HasMixinTrait || HasExternalImplicitThisTrait)
				return MakeReceiverError(
					Receiver,
					"Mixin receiver requires only its authoritative trait");
			if (Function.header.invocationKind
				!= asETypedSemanticInvocationKind::Global)
				return MakeReceiverError(
					Receiver,
					"Mixin function body remains a global invocation");
			break;

		default:
			return MakeReceiverError(
				Receiver,
				"Effective receiver kind is unknown");
		}
	}

	return asSTypedSemanticVerificationResult();
}

asSTypedSemanticDiagnosticSource SelectTypedSemanticDiagnosticSource(
	const asSTypedSemanticSourceSpan& Span)
{
	asSTypedSemanticDiagnosticSource Result;
	if( Span.hasAuthoredOrigin && IsValidAnchor(Span.authoredOrigin) )
	{
		Result.kind = asETypedSemanticDiagnosticSourceKind::Authored;
		Result.source = Span.authoredOrigin;
		return Result;
	}

	if( Span.hasGeneratedOrigin )
	{
		Result.generatedKind = Span.generatedOrigin.kind;
		Result.generatorName = Span.generatedOrigin.generatorName;
		if( Span.generatedOrigin.hasAuthoredAnchor
			&& IsValidAnchor(Span.generatedOrigin.authoredAnchor) )
		{
			Result.kind =
				asETypedSemanticDiagnosticSourceKind::GeneratedAnchor;
			Result.source = Span.generatedOrigin.authoredAnchor;
			return Result;
		}
	}

	Result.kind = asETypedSemanticDiagnosticSourceKind::Processed;
	Result.source.section = Span.section;
	Result.source.sourceOffset = Span.sourceOffset;
	Result.source.sourceLength = Span.sourceLength;
	Result.source.row = Span.row;
	Result.source.column = Span.column;
	return Result;
}

asCString DumpTypedSemanticFunction(const asCTypedSemanticFunction& Function)
{
	asCString Output;
	Output += "function ";
	Output += Function.declaration;
	Output += "\n";

	const asSTypedSemanticFunctionHeader& Header = Function.header;
	asCString HeaderLine;
	HeaderLine.Format(
		"header traits=%u traitKnown=%u traitUnknown=%u traitPolicy=%u traitReceiver=%u traitInvocation=%u traitDispatch=%u traitBody=%u traitLifetime=%u traitProfile=%u invocation=%s receiver=%s receiverSymbol=",
		Header.declaredTraitBits,
		Header.traitSummary.knownTraitBits,
		Header.traitSummary.unknownTraitBits,
		Header.traitSummary.sourcePolicyTraitBits,
		Header.traitSummary.receiverTraitBits,
		Header.traitSummary.invocationTraitBits,
		Header.traitSummary.dispatchTraitBits,
		Header.traitSummary.bodyAvailabilityTraitBits,
		Header.traitSummary.lifetimeTraitBits,
		Header.traitSummary.profileTraitBits,
		ToString(Header.invocationKind),
		ToString(Header.receiver.kind));
	Output += HeaderLine;
	if (Header.receiver.symbol.IsValid())
	{
		asCString Symbol;
		Symbol.Format("S%u", Header.receiver.symbol.value);
		Output += Symbol;
	}
	else
	{
		Output += "none";
	}
	asCString BodyLine;
	BodyLine.Format(
		" callingModule=%s bodyAvailability=%s bodyOwnership=%s bodyOwner=%s",
		Header.callingModule.GetLength() != 0
			? Header.callingModule.AddressOf() : "none",
		ToString(Header.bodyAvailability),
		ToString(Header.bodyOwnership),
		Header.bodyOwnerModule.GetLength() != 0
			? Header.bodyOwnerModule.AddressOf() : "none");
	Output += BodyLine;
	asCString ReceiverTail;
	ReceiverTail.Format(
		" receiverParameter=%d receiverType=%s compileOut=%s hiddenArgument=%d determinesOutputType=%d returnsOnStack=%s suspend=%s cleanup=%s cleanupState=%s cleanupPlan=",
		Header.receiver.parameterIndex,
		Header.receiver.type.IsValid() ? Header.receiver.type.Format(0, true).AddressOf() : "none",
		ToString(Header.compileOutKind),
		Header.hiddenArgumentIndex,
		Header.determinesOutputTypeArgumentIndex,
		BoolString(Header.returnsOnStack),
		BoolString(Header.hasSuspendState),
		BoolString(Header.hasExceptionCleanup),
		ToString(Header.cleanupPlanState));
	Output += ReceiverTail;
	if (Header.universalCleanupPlan.IsValid())
	{
		asCString Plan;
		Plan.Format("C%u", Header.universalCleanupPlan.value);
		Output += Plan;
	}
	else
	{
		Output += "none";
	}
	Output += " allTransfers=";
	Output += BoolString(Header.cleanupPlanCoversAllTransfers);
	Output += "\n";

	asCString Root;
	Root.Format("root=T%u\n", Function.rootStatement.value);
	Output += Root;
	Output += "symbols:\n";
	for (asUINT Index = 0; Index < Function.symbols.GetLength(); ++Index)
	{
		const asSTypedSemanticSymbol& Symbol = Function.symbols[Index];
		asCString Line;
		Line.Format(
			"  S%u kind=%s name=%s type=%s span=",
			Symbol.id.value,
			ToString(Symbol.kind),
			Symbol.name.AddressOf(),
			Symbol.type.Format(0, true).AddressOf());
		Output += Line;
		AppendSpan(Output, Symbol.span);
		Output += "\n";
	}

	Output += "expressions:\n";
	for (asUINT Index = 0; Index < Function.expressions.GetLength(); ++Index)
	{
		const asSTypedSemanticExpression& Expression = Function.expressions[Index];
		asCString Line;
		Line.Format(
			"  E%u kind=%s type=%s",
			Expression.id.value,
			ToString(Expression.kind),
			Expression.type.Format(0, true).AddressOf());
		Output += Line;
		if (Expression.kind == asETypedSemanticExpressionKind::Symbol)
		{
			asCString Symbol;
			Symbol.Format(" symbol=S%u", Expression.symbol.value);
			Output += Symbol;
		}
		else if (Expression.kind == asETypedSemanticExpressionKind::Literal)
		{
			Output += " value=";
			Output += Expression.literal;
			asCString Bits;
			Bits.Format(
				" bits=%llu",
				static_cast<unsigned long long>(Expression.literalBits));
			Output += Bits;
		}
		else if (Expression.kind
			== asETypedSemanticExpressionKind::FoldedGlobalConstant)
		{
			asCString Global;
			Global.Format(
				" globalPropertyId=%d bits=%llu",
				Expression.resolvedGlobalPropertyId,
				static_cast<unsigned long long>(Expression.literalBits));
			Output += Global;
		}
		else if (Expression.kind
			== asETypedSemanticExpressionKind::GlobalStorage)
		{
			asCString Global;
			Global.Format(
				" globalPropertyId=%d",
				Expression.resolvedGlobalPropertyId);
			Output += Global;
		}
		else if (Expression.kind == asETypedSemanticExpressionKind::Binary
			|| Expression.kind == asETypedSemanticExpressionKind::ShortCircuit)
		{
			Output += " operator=";
			Output += Expression.operatorToken;
			Output += " operands=";
			AppendExpressionList(Output, Expression.operands);
		}
		else if (Expression.kind == asETypedSemanticExpressionKind::Assignment)
		{
			Output += " operator=";
			Output += Expression.operatorToken;
			asCString Target;
			Target.Format(" target=E%u value=", Expression.mutationTarget.value);
			Output += Target;
			if (Expression.mutationValue.IsValid())
			{
				asCString Value;
				Value.Format("E%u", Expression.mutationValue.value);
				Output += Value;
				Output += " evaluation=[value,target]";
			}
			else
			{
				Output += "none evaluation=[target]";
			}
			Output += Expression.mutationReturnsOldValue
				? " returns=old"
				: " returns=updated";
		}
		else if (Expression.kind == asETypedSemanticExpressionKind::ResolvedCall)
		{
			asCString Target;
			Target.Format(
				" target=%s functionId=%d rewrite=%s",
				ToString(Expression.callTargetKind),
				Expression.resolvedFunctionId,
				ToString(Expression.callRewriteKind));
			Output += Target;
			AppendFunctionTargetProvenance(
				Output,
				Expression.targetProvenance);
			if (Expression.receiverExpression.IsValid())
			{
				asCString Receiver;
				Receiver.Format(
					" receiver=E%u",
					Expression.receiverExpression.value);
				Output += Receiver;
			}
			Output += " operands=";
			AppendExpressionList(Output, Expression.operands);
			Output += " formal=";
			AppendIntList(Output, Expression.formalParameterIndices);
			Output += " sourceOrdinals=";
			AppendIntList(Output, Expression.sourceArgumentOrdinals);
			Output += " evaluation=";
			AppendUIntList(Output, Expression.evaluationSequence);
			Output += " origins=";
			AppendArgumentOrigins(Output, Expression.argumentOrigins);
			Output += " provenance=";
			AppendArgumentProvenance(
				Output,
				Expression.argumentProvenance);
			Output += " steps=";
			AppendEvaluationSteps(Output, Expression.evaluationSteps);
			asCString CallMetadata;
			CallMetadata.Format(
				" hiddenArgument=%d determinesOutputType=%d nativeSystem=%s callConvention=%d baseOffset=%d scriptReturnSize=%d parameterSize=%d firstParam=%s genericContext=%s userData=%s functionCaller=%s nativeReturnsOnStack=%s cleanupArgs=%u",
				Expression.hiddenArgumentIndex,
				Expression.determinesOutputTypeArgumentIndex,
				BoolString(Expression.nativeCall.isSystemFunction),
				Expression.nativeCall.callConvention,
				Expression.nativeCall.baseOffset,
				Expression.nativeCall.scriptReturnSize,
				Expression.nativeCall.parameterSize,
				ToString(Expression.nativeCall.firstParamMetadata),
				BoolString(Expression.nativeCall.usesGenericContext),
				BoolString(Expression.nativeCall.hasFunctionUserData),
				BoolString(Expression.nativeCall.hasFunctionCaller),
				BoolString(Expression.nativeCall.returnsOnStack),
				Expression.nativeCall.cleanupArgumentCount);
			Output += CallMetadata;
		}
		else if (Expression.kind == asETypedSemanticExpressionKind::CallRewrite)
		{
			asCString Rewrite;
			Rewrite.Format(
				" rewrite=%s target=%s functionId=%d",
				ToString(Expression.callRewriteKind),
				ToString(Expression.callTargetKind),
				Expression.resolvedFunctionId);
			Output += Rewrite;
			AppendFunctionTargetProvenance(
				Output,
				Expression.targetProvenance);
			if (Expression.receiverExpression.IsValid())
			{
				asCString Receiver;
				Receiver.Format(
					" receiver=E%u",
					Expression.receiverExpression.value);
				Output += Receiver;
			}
			Output += " operands=";
			AppendExpressionList(Output, Expression.operands);
			Output += " origins=";
			AppendArgumentOrigins(Output, Expression.argumentOrigins);
			Output += " sourceOrdinals=";
			AppendIntList(Output, Expression.sourceArgumentOrdinals);
			Output += " evaluation=";
			AppendUIntList(Output, Expression.evaluationSequence);
			Output += " steps=";
			AppendEvaluationSteps(Output, Expression.evaluationSteps);
			asCString RewriteMetadata;
			RewriteMetadata.Format(
				" hiddenArgument=%d determinesOutputType=%d",
				Expression.hiddenArgumentIndex,
				Expression.determinesOutputTypeArgumentIndex);
			Output += RewriteMetadata;
		}
		else if (Expression.kind == asETypedSemanticExpressionKind::Unsupported)
		{
			Output += " unsupported=";
			Output += ToString(Expression.unsupportedKind);
			if (Expression.symbol.IsValid())
			{
				asCString Symbol;
				Symbol.Format(" symbol=S%u", Expression.symbol.value);
				Output += Symbol;
			}
			if (Expression.resolvedTarget.GetLength() != 0)
			{
				Output += " target=";
				Output += Expression.resolvedTarget;
			}
			Output += " operands=";
			AppendExpressionList(Output, Expression.operands);
		}
		if (Expression.kind != asETypedSemanticExpressionKind::ResolvedCall
			&& Expression.kind != asETypedSemanticExpressionKind::CallRewrite
			&& Expression.evaluationSteps.GetLength() != 0)
		{
			Output += " steps=";
			AppendEvaluationSteps(Output, Expression.evaluationSteps);
		}
		if (Expression.safePointRole
			!= asETypedSemanticSafePointRole::None)
		{
			Output += " safePoint=";
			Output += ToString(Expression.safePointRole);
		}
		Output += " span=";
		AppendSpan(Output, Expression.span);
		Output += "\n";
	}

	Output += "statements:\n";
	for (asUINT Index = 0; Index < Function.statements.GetLength(); ++Index)
	{
		const asSTypedSemanticStatement& Statement = Function.statements[Index];
		asCString Line;
		Line.Format("  T%u kind=%s", Statement.id.value, ToString(Statement.kind));
		Output += Line;
		switch (Statement.kind)
		{
		case asETypedSemanticStatementKind::Block:
			Output += " children=";
			AppendStatementList(Output, Statement.children);
			break;
		case asETypedSemanticStatementKind::LocalDeclaration:
		{
			asCString Detail;
			Detail.Format(" symbol=S%u initializer=E%u", Statement.symbol.value, Statement.initializerExpression.value);
			Output += Detail;
			break;
		}
		case asETypedSemanticStatementKind::If:
		{
			asCString Detail;
			Detail.Format(" condition=E%u then=T%u else=", Statement.conditionExpression.value, Statement.thenStatement.value);
			Output += Detail;
			if (Statement.elseStatement.IsValid())
			{
				asCString Else;
				Else.Format("T%u", Statement.elseStatement.value);
				Output += Else;
			}
			else
			{
				Output += "none";
			}
			break;
		}
		case asETypedSemanticStatementKind::While:
		{
			asCString Detail;
			Detail.Format(
				" condition=E%u body=T%u",
				Statement.conditionExpression.value,
				Statement.bodyStatement.value);
			Output += Detail;
			break;
		}
		case asETypedSemanticStatementKind::DoWhile:
		{
			asCString Detail;
			Detail.Format(
				" body=T%u condition=E%u",
				Statement.bodyStatement.value,
				Statement.conditionExpression.value);
			Output += Detail;
			break;
		}
		case asETypedSemanticStatementKind::For:
		{
			Output += " initializer=";
			if (Statement.initializerStatement.IsValid())
			{
				asCString Initializer;
				Initializer.Format("T%u", Statement.initializerStatement.value);
				Output += Initializer;
			}
			else
			{
				Output += "none";
			}
			Output += " condition=";
			if (Statement.conditionExpression.IsValid())
			{
				asCString Condition;
				Condition.Format("E%u", Statement.conditionExpression.value);
				Output += Condition;
			}
			else
			{
				Output += "none";
			}
			Output += " increment=";
			if (Statement.incrementStatement.IsValid())
			{
				asCString Increment;
				Increment.Format("T%u", Statement.incrementStatement.value);
				Output += Increment;
			}
			else
			{
				Output += "none";
			}
			Output += " increments=";
			AppendExpressionList(Output, Statement.incrementExpressions);
			asCString Body;
			Body.Format(" body=T%u", Statement.bodyStatement.value);
			Output += Body;
			break;
		}
		case asETypedSemanticStatementKind::Switch:
		{
			asCString Detail;
			Detail.Format(
				" selector=E%u normalization=%s cases=",
				Statement.conditionExpression.value,
				ToString(Statement.switchNormalization));
			Output += Detail;
			AppendStatementList(Output, Statement.children);
			Output += " default=";
			Output += BoolString(Statement.switchHasDefault);
			Output += " exhaustive=";
			Output += BoolString(Statement.switchIsExhaustive);
			Output += " invalidEnumEdge=";
			Output += BoolString(Statement.switchHasInvalidEnumValueEdge);
			break;
		}
		case asETypedSemanticStatementKind::Case:
		{
			if (Statement.isDefaultCase)
			{
				Output += " default=true";
			}
			else
			{
				asCString Value;
				Value.Format(
					" value=E%u constant=%d",
					Statement.valueExpression.value,
					Statement.caseConstantValue);
				Output += Value;
			}
			asCString Body;
			Body.Format(" body=T%u", Statement.bodyStatement.value);
			Output += Body;
			Output += " fallsThrough=";
			Output += BoolString(Statement.fallsThrough);
			Output += " explicitFallthrough=";
			Output += BoolString(Statement.hasExplicitFallthrough);
			break;
		}
		case asETypedSemanticStatementKind::Expression:
			if (Statement.valueExpression.IsValid())
			{
				asCString Value;
				Value.Format(" value=E%u", Statement.valueExpression.value);
				Output += Value;
			}
			break;
		case asETypedSemanticStatementKind::Return:
		{
			if (Statement.valueExpression.IsValid())
			{
				asCString Value;
				Value.Format(" value=E%u", Statement.valueExpression.value);
				Output += Value;
			}
			Output += " cleanup=";
			if (Statement.cleanupPlan.IsValid())
			{
				asCString Cleanup;
				Cleanup.Format("C%u", Statement.cleanupPlan.value);
				Output += Cleanup;
			}
			else
			{
				Output += "none";
			}
			asCString ScopeCount;
			ScopeCount.Format(
				" exitedScopes=%u",
				Statement.exitedScopeCount);
			Output += ScopeCount;
			break;
		}
		case asETypedSemanticStatementKind::Break:
		case asETypedSemanticStatementKind::Continue:
		{
			Output += " target=";
			if (Statement.targetStatement.IsValid())
			{
				asCString Target;
				Target.Format("T%u", Statement.targetStatement.value);
				Output += Target;
			}
			else
			{
				Output += "none";
			}
			Output += " cleanup=";
			if (Statement.cleanupPlan.IsValid())
			{
				asCString Cleanup;
				Cleanup.Format("C%u", Statement.cleanupPlan.value);
				Output += Cleanup;
			}
			else
			{
				Output += "none";
			}
			asCString ScopeCount;
			ScopeCount.Format(
				" exitedScopes=%u",
				Statement.exitedScopeCount);
			Output += ScopeCount;
			break;
		}
		case asETypedSemanticStatementKind::Unsupported:
			Output += " unsupported=";
			Output += ToString(Statement.unsupportedKind);
			break;
		default:
			break;
		}
		if (Statement.loopPhase != asETypedSemanticLoopPhase::None)
		{
			Output += " phase=";
			Output += ToString(Statement.loopPhase);
		}
		if (Statement.safePointRole
			!= asETypedSemanticSafePointRole::None)
		{
			Output += " safePoint=";
			Output += ToString(Statement.safePointRole);
		}
		Output += " span=";
		AppendSpan(Output, Statement.span);
		Output += "\n";
	}
	return Output;
}

asSTypedSemanticSnapshotResult BuildTypedSemanticSnapshot(
	const asCTypedSemanticFunction* Function)
{
	static const asUINT SchemaRevision = 1;
	asSTypedSemanticSnapshotResult Result;
	if (Function != 0)
	{
		Result.verification = VerifyTypedSemanticFunction(*Function);
		Result.state = Result.verification.IsValid()
			? asETypedSemanticSnapshotState::Verified
			: asETypedSemanticSnapshotState::VerificationFailed;
	}

	Result.text += "SchemaRevision=1\n";
	Result.text += "RequestKind=TestHIRSnapshot\n";
	Result.text += "State=";
	Result.text += ToString(Result.state);
	Result.text += "\nVerificationError=";
	Result.text += ToString(Result.verification.error);
	Result.text += "\nVerificationDetail=";
	AppendTextValue(Result.text, Result.verification.detail);
	Result.text += "\nFailureSymbol=";
	if (Result.verification.symbol.IsValid())
	{
		asCString Id;
		Id.Format("S%u", Result.verification.symbol.value);
		Result.text += Id;
	}
	else Result.text += "none";
	Result.text += "\nFailureExpression=";
	if (Result.verification.expression.IsValid())
	{
		asCString Id;
		Id.Format("E%u", Result.verification.expression.value);
		Result.text += Id;
	}
	else Result.text += "none";
	Result.text += "\nFailureStatement=";
	if (Result.verification.statement.IsValid())
	{
		asCString Id;
		Id.Format("T%u", Result.verification.statement.value);
		Result.text += Id;
	}
	else Result.text += "none";
	Result.text += "\nFailureSpan=";
	if (Result.verification.span.section.GetLength() != 0)
		AppendSpan(Result.text, Result.verification.span);
	else Result.text += "none";
	Result.text += "\n";

	asCString NormalizedHIR;
	if (Result.IsValid())
	{
		NormalizedHIR = DumpTypedSemanticFunction(*Function);
		Result.text += "NormalizedHIRBegin\n";
		Result.text += NormalizedHIR;
		if (NormalizedHIR.GetLength() == 0
			|| NormalizedHIR[NormalizedHIR.GetLength() - 1] != '\n')
			Result.text += "\n";
		Result.text += "NormalizedHIREnd\n";
	}
	else
	{
		Result.text += "NormalizedHIR=none\n";
	}

	Result.json += "{\"schemaRevision\":";
	asCString Revision;
	Revision.Format("%u", SchemaRevision);
	Result.json += Revision;
	Result.json += ",\"requestKind\":\"TestHIRSnapshot\",\"state\":";
	AppendJsonString(Result.json, asCString(ToString(Result.state)));
	Result.json += ",\"verification\":{\"error\":";
	AppendJsonString(
		Result.json,
		asCString(ToString(Result.verification.error)));
	Result.json += ",\"detail\":";
	AppendJsonString(Result.json, Result.verification.detail);
	Result.json += ",\"symbol\":";
	if (Result.verification.symbol.IsValid())
	{
		asCString Id;
		Id.Format("\"S%u\"", Result.verification.symbol.value);
		Result.json += Id;
	}
	else Result.json += "null";
	Result.json += ",\"expression\":";
	if (Result.verification.expression.IsValid())
	{
		asCString Id;
		Id.Format("\"E%u\"", Result.verification.expression.value);
		Result.json += Id;
	}
	else Result.json += "null";
	Result.json += ",\"statement\":";
	if (Result.verification.statement.IsValid())
	{
		asCString Id;
		Id.Format("\"T%u\"", Result.verification.statement.value);
		Result.json += Id;
	}
	else Result.json += "null";
	Result.json += ",\"span\":";
	if (Result.verification.span.section.GetLength() != 0)
	{
		Result.json += "{\"section\":";
		AppendJsonString(Result.json, Result.verification.span.section);
		asCString SpanTail;
		SpanTail.Format(
			",\"sourceOffset\":%u,\"sourceLength\":%u,\"row\":%u,\"column\":%u}",
			Result.verification.span.sourceOffset,
			Result.verification.span.sourceLength,
			Result.verification.span.row,
			Result.verification.span.column);
		Result.json += SpanTail;
	}
	else Result.json += "null";
	Result.json += "},\"normalizedHIR\":";
	if (Result.IsValid())
		AppendJsonString(Result.json, NormalizedHIR);
	else
		Result.json += "null";
	Result.json += "}";
	return Result;
}

END_AS_NAMESPACE
