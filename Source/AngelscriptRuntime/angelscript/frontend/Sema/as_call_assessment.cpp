/* Maintained Unreal AngelScript fork — non-mutating call candidate assessment. */

#include "frontend/Sema/as_call_assessment.h"
#include "frontend/Sema/as_sema.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_type_context.h"
#include "as_scriptfunction.h"
#include "as_datatype.h"
#include "as_typeinfo.h"

BEGIN_AS_NAMESPACE

namespace asCallAssessment
{
	bool SameType(const asCType* Left, const asCType* Right)
	{
		return Left && Right && (Left == Right
			|| (Left->HasStableIdentity() && Left->GetStableIdentity() == Right->GetStableIdentity()));
	}

	bool Enum(const asCType* Type)
	{
		const asCNominalType* Nominal = dyn_cast<asCNominalType>(Type);
		asSNominalTypeDescriptor Descriptor;
		return Nominal && Nominal->GetOwnerContext()
			&& Nominal->GetOwnerContext()->GetTypeContext()->GetIdentityRegistry().ResolveNominalType(
				Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
			&& Descriptor.Kind == asETypeDeclKind::Enum;
	}

	bool Numeric(const asCType* Type)
	{
		const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type);
		return Enum(Type) || (Builtin && Builtin->GetPrimitive() != asEPrimitiveType::Void
			&& Builtin->GetPrimitive() != asEPrimitiveType::Bool);
	}

	asCQualType ValueType(asCQualType Type)
	{
		asCQualType Value;
		asCQualType::TryCreateRaw(
			Type.GetType(),
			static_cast<uint8>(Type.GetQualifiers()) & ~static_cast<uint8>(asETypeQualifiers::Reference),
			Value);
		return Value;
	}

	asEPrimitiveType PrimitiveFromToken(eTokenType Token)
	{
		switch (Token)
		{
		case ttBool: return asEPrimitiveType::Bool;
		case ttInt8: return asEPrimitiveType::Int8;
		case ttInt16: return asEPrimitiveType::Int16;
		case ttInt: return asEPrimitiveType::Int32;
		case ttInt64: return asEPrimitiveType::Int64;
		case ttUInt8: return asEPrimitiveType::UInt8;
		case ttUInt16: return asEPrimitiveType::UInt16;
		case ttUInt: return asEPrimitiveType::UInt32;
		case ttUInt64: return asEPrimitiveType::UInt64;
		case ttFloat32: return asEPrimitiveType::Float32;
		case ttFloat64:
		case ttDouble: return asEPrimitiveType::Float64;
		case ttFloat: return asEPrimitiveType::Float32;
		case ttVoid: return asEPrimitiveType::Void;
		default: return asEPrimitiveType::Invalid;
		}
	}

	asETypeQualifiers QualifiersFromHost(const asCDataType& Type)
	{
		asETypeQualifiers Qualifiers = asETypeQualifiers::None;
		if (Type.IsReadOnly()) Qualifiers = Qualifiers | asETypeQualifiers::Const;
		if (Type.IsReference()) Qualifiers = Qualifiers | asETypeQualifiers::Reference;
		if (Type.IsObjectHandle()) Qualifiers = Qualifiers | asETypeQualifiers::Handle;
		return Qualifiers;
	}

	asCQualType QualTypeFromHost(asCASTContext& Context, const asCDataType& Type)
	{
		asCQualType Result;
		const asETypeQualifiers Qualifiers = QualifiersFromHost(Type);
		if (asCTypeInfo* Info = Type.GetTypeInfo())
		{
			asSStableTypeUseDescriptor Use;
			Use.Kind = asETypeUseKind::Nominal;
			Use.Declaration = Info->GetStableKey();
			asSStableKey Key;
			if (Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(Use, Key) != asETypeIdentityStatus::Succeeded)
			{
				return {};
			}
			asCQualType::TryCreate(Context.GetNominalType(Use.Declaration, Key), Qualifiers, Result);
			return Result;
		}
		const asEPrimitiveType Primitive = PrimitiveFromToken(Type.GetTokenType());
		if (Primitive == asEPrimitiveType::Invalid)
		{
			return {};
		}
		asCQualType::TryCreate(Context.GetBuiltinType(Primitive), Qualifiers, Result);
		return Result;
	}

	asCQualType ActualType(const asSCallArgumentFact& Fact)
	{
		return Fact.Value ? Fact.Value->GetExpressionQualType() : Fact.QualType;
	}

	asEValueCategory ActualCategory(const asSCallArgumentFact& Fact)
	{
		return Fact.Value ? Fact.Value->GetValueCategory() : Fact.ValueCategory;
	}

	bool ActualReadOnly(const asSCallArgumentFact& Fact)
	{
		return Fact.Value ? Fact.Value->IsReadOnly() : Fact.bReadOnly;
	}

	bool IsNull(const asSCallArgumentFact& Fact)
	{
		return Fact.bNull || (Fact.Value && dyn_cast<asCNullPtrLiteral>(Fact.Value));
	}

	bool IsInitList(const asSCallArgumentFact& Fact)
	{
		return Fact.bInitList || (Fact.Value && isa<asCInitListExpr>(Fact.Value));
	}

	void Reject(asSCandidateAssessment& Out, asECandidateRejection Reason)
	{
		Out.bViable = false;
		Out.Rejection = Reason;
	}
}

asSCandidateAssessment asCSema::AssessScriptCandidate(
	asCFunctionDecl* Candidate,
	const asSCallAssessmentRequest& Request) const
{
	asSCandidateAssessment Out;
	Out.ScriptFunction = Candidate;
	if (!Candidate || Candidate->IsInvalid() || !Candidate->GetResolvedSignature())
	{
		asCallAssessment::Reject(Out, asECandidateRejection::InvalidCandidate);
		return Out;
	}
	if (!CanAccessMember(Candidate))
	{
		asCallAssessment::Reject(Out, asECandidateRejection::AccessDenied);
		return Out;
	}
	if (((static_cast<uint16>(Candidate->GetModifiers()) & static_cast<uint16>(asEFunctionModifiers::Defaults)) != 0)
		&& !IsEditDefaultsContext())
	{
		asCallAssessment::Reject(Out, asECandidateRejection::ContextRejected);
		return Out;
	}
	if (IsConstructionContext()
		&& (static_cast<uint16>(Candidate->GetModifiers())
			& static_cast<uint16>(asEFunctionModifiers::UnsafeDuringConstruction)) != 0)
	{
		asCallAssessment::Reject(Out, asECandidateRejection::ContextRejected);
		return Out;
	}

	const auto Parameters = Candidate->GetParameters();
	Out.FormalToSource.Init(INDEX_NONE, Parameters.Num());
	Out.FormalSlots.Init(asEArgumentSlotKind::Unbound, Parameters.Num());
	Out.AuthoredToFormal.Init(INDEX_NONE, Request.Arguments.Num());
	int32 NextPositional = 0;
	int32 Score = 0;
	bool bSawNamed = false;
	for (int32 SourceIndex = 0; SourceIndex < Request.Arguments.Num(); ++SourceIndex)
	{
		const asSCallArgumentFact& Fact = Request.Arguments[SourceIndex];
		if (Fact.bUnknownFuture)
		{
			if (NextPositional < Parameters.Num()
				&& Out.FormalSlots[NextPositional] == asEArgumentSlotKind::Unbound)
			{
				Out.FormalSlots[NextPositional] = asEArgumentSlotKind::UnknownFuture;
				Out.AuthoredToFormal[SourceIndex] = NextPositional;
				++NextPositional;
			}
			continue;
		}
		int32 ParameterIndex = INDEX_NONE;
		if (!Fact.Name.IsEmpty())
		{
			bSawNamed = true;
			for (int32 Index = 0; Index < Parameters.Num(); ++Index)
			{
				if (Parameters[Index]->GetName() == Fact.Name)
				{
					ParameterIndex = Index;
					break;
				}
			}
			if (ParameterIndex == INDEX_NONE)
			{
				asCallAssessment::Reject(Out, asECandidateRejection::UnknownNamedArgument);
				return Out;
			}
		}
		else if (!bSawNamed)
		{
			ParameterIndex = NextPositional++;
		}
		else
		{
			asCallAssessment::Reject(Out, asECandidateRejection::PositionalAfterNamed);
			return Out;
		}
		if (!Out.FormalToSource.IsValidIndex(ParameterIndex))
		{
			asCallAssessment::Reject(Out, asECandidateRejection::TooManyArguments);
			return Out;
		}
		if (Out.FormalToSource[ParameterIndex] != INDEX_NONE)
		{
			asCallAssessment::Reject(Out, asECandidateRejection::DuplicateNamedArgument);
			return Out;
		}
		Out.FormalToSource[ParameterIndex] = SourceIndex;
		Out.FormalSlots[ParameterIndex] = asEArgumentSlotKind::Supplied;
		Out.AuthoredToFormal[SourceIndex] = ParameterIndex;

		const asCQualType Formal = Parameters[ParameterIndex]->GetResolvedQualType();
		const asETypeReferenceDirection Direction = Parameters[ParameterIndex]->GetTypeSyntax().Reference;
		if (asCallAssessment::IsInitList(Fact))
		{
			asSListInitialization Contract;
			if (!ListInitializationLookup || !ListInitializationLookup(Formal, Contract))
			{
				asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
				return Out;
			}
			++Score;
			continue;
		}
		if (asCallAssessment::IsNull(Fact))
		{
			if (!Formal.IsHandle()
				|| Direction == asETypeReferenceDirection::Out
				|| Direction == asETypeReferenceDirection::InOut)
			{
				asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
				return Out;
			}
			++Score;
			continue;
		}
		const asCQualType Actual = asCallAssessment::ActualType(Fact);
		if (!Actual.IsValid())
		{
			asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
			return Out;
		}
		if ((Direction == asETypeReferenceDirection::None || Direction == asETypeReferenceDirection::In)
			&& (!asCallAssessment::SameType(Actual.GetType(), Formal.GetType())
				|| Formal.IsHandle() != Actual.IsHandle()))
		{
			bool bAmbiguousConversion = false;
			if (Fact.Value && SelectUserConversion(Fact.Value, asCallAssessment::ValueType(Formal), false, bAmbiguousConversion))
			{
				Score += 2;
				continue;
			}
			if (bAmbiguousConversion)
			{
				asCallAssessment::Reject(Out, asECandidateRejection::AmbiguousConversion);
				return Out;
			}
		}
		if (Formal.IsHandle() != Actual.IsHandle())
		{
			asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
			return Out;
		}
		if ((Direction == asETypeReferenceDirection::Out || Direction == asETypeReferenceDirection::InOut)
			&& (asCallAssessment::ActualCategory(Fact) != asEValueCategory::LValue
				|| asCallAssessment::ActualReadOnly(Fact)
				|| !asCallAssessment::SameType(Actual.GetType(), Formal.GetType())))
		{
			asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
			return Out;
		}
		if (Actual.IsHandle() && Actual.IsConst() && !Formal.IsConst())
		{
			asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
			return Out;
		}
		if (asCallAssessment::SameType(Actual.GetType(), Formal.GetType()))
		{
			continue;
		}
		if (!Actual.IsHandle() && asCallAssessment::Numeric(Actual.GetType()) && asCallAssessment::Numeric(Formal.GetType()))
		{
			++Score;
			continue;
		}
		asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
		return Out;
	}

	for (int32 Index = 0; Index < Parameters.Num(); ++Index)
	{
		if (Out.FormalSlots[Index] != asEArgumentSlotKind::Unbound)
		{
			continue;
		}
		if (Request.Mode == asECallAssessmentMode::Incomplete)
		{
			Out.FormalSlots[Index] = asEArgumentSlotKind::UnknownFuture;
			continue;
		}
		if (Parameters[Index]->HasDeferredInitializer() && !Parameters[Index]->IsInvalid())
		{
			Out.FormalSlots[Index] = asEArgumentSlotKind::Defaulted;
			continue;
		}
		asCallAssessment::Reject(Out, asECandidateRejection::MissingRequiredArgument);
		return Out;
	}

	Out.bViable = true;
	Out.Score = Score;
	return Out;
}

asSCandidateAssessment asCSema::AssessHostCandidate(
	const asSExternalCallable& Candidate,
	const asSCallAssessmentRequest& Request) const
{
	asSCandidateAssessment Out;
	Out.HostCallable = Candidate;
	asCScriptFunction* Function = Candidate.Function;
	if (!Function || !Function->GetDefinitions() || Function->GetStableKey() != Candidate.Key)
	{
		asCallAssessment::Reject(Out, asECandidateRejection::InvalidCandidate);
		return Out;
	}
	const int32 ParameterCount = static_cast<int32>(Function->parameterTypes.GetLength());
	Out.FormalToSource.Init(INDEX_NONE, ParameterCount);
	Out.FormalSlots.Init(asEArgumentSlotKind::Unbound, ParameterCount);
	Out.AuthoredToFormal.Init(INDEX_NONE, Request.Arguments.Num());
	int32 NextPositional = 0;
	int32 Score = 0;
	bool bSawNamed = false;
	for (int32 SourceIndex = 0; SourceIndex < Request.Arguments.Num(); ++SourceIndex)
	{
		const asSCallArgumentFact& Fact = Request.Arguments[SourceIndex];
		if (Fact.bUnknownFuture)
		{
			if (NextPositional < ParameterCount
				&& Out.FormalSlots[NextPositional] == asEArgumentSlotKind::Unbound)
			{
				Out.FormalSlots[NextPositional] = asEArgumentSlotKind::UnknownFuture;
				Out.AuthoredToFormal[SourceIndex] = NextPositional;
				++NextPositional;
			}
			continue;
		}
		int32 ParameterIndex = INDEX_NONE;
		if (!Fact.Name.IsEmpty())
		{
			bSawNamed = true;
			for (int32 Index = 0; Index < ParameterCount; ++Index)
			{
				if (Function->parameterNames[Index] == TCHAR_TO_UTF8(*Fact.Name))
				{
					ParameterIndex = Index;
					break;
				}
			}
			if (ParameterIndex == INDEX_NONE)
			{
				asCallAssessment::Reject(Out, asECandidateRejection::UnknownNamedArgument);
				return Out;
			}
		}
		else if (!bSawNamed)
		{
			ParameterIndex = NextPositional++;
		}
		else
		{
			asCallAssessment::Reject(Out, asECandidateRejection::PositionalAfterNamed);
			return Out;
		}
		if (!Out.FormalToSource.IsValidIndex(ParameterIndex))
		{
			asCallAssessment::Reject(Out, asECandidateRejection::TooManyArguments);
			return Out;
		}
		if (Out.FormalToSource[ParameterIndex] != INDEX_NONE)
		{
			asCallAssessment::Reject(Out, asECandidateRejection::DuplicateNamedArgument);
			return Out;
		}
		Out.FormalToSource[ParameterIndex] = SourceIndex;
		Out.FormalSlots[ParameterIndex] = asEArgumentSlotKind::Supplied;
		Out.AuthoredToFormal[SourceIndex] = ParameterIndex;

		const asETypeModifiers Flags = ParameterIndex < static_cast<int32>(Function->inOutFlags.GetLength())
			? Function->inOutFlags[ParameterIndex] : asTM_NONE;
		if ((Flags == asTM_OUTREF || Flags == asTM_INOUTREF)
			&& (asCallAssessment::ActualCategory(Fact) != asEValueCategory::LValue
				|| asCallAssessment::ActualReadOnly(Fact)))
		{
			asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
			return Out;
		}

		bool bMatches = false;
		if (Fact.Value)
		{
			const asCType* Actual = Fact.Value->GetExpressionType();
			if (asCTypeInfo* Info = Function->parameterTypes[ParameterIndex].GetTypeInfo())
			{
				const auto* Nominal = dyn_cast<asCNominalType>(Actual);
				bMatches = Nominal && Nominal->GetDeclarationIdentity() == Info->GetStableKey();
			}
			else
			{
				const auto* Builtin = dyn_cast<asCBuiltinType>(Actual);
				const asEPrimitiveType Primitive = asCallAssessment::PrimitiveFromToken(
					Function->parameterTypes[ParameterIndex].GetTokenType());
				bMatches = Builtin && Primitive != asEPrimitiveType::Invalid && Builtin->GetPrimitive() == Primitive;
			}
			if (!bMatches)
			{
				asCQualType Formal = asCallAssessment::QualTypeFromHost(Context, Function->parameterTypes[ParameterIndex]);
				bool bAmbiguousConversion = false;
				if (Formal.IsValid()
					&& SelectUserConversion(Fact.Value, asCallAssessment::ValueType(Formal), false, bAmbiguousConversion))
				{
					Score += 2;
					continue;
				}
				if (bAmbiguousConversion)
				{
					asCallAssessment::Reject(Out, asECandidateRejection::AmbiguousConversion);
					return Out;
				}
				asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
				return Out;
			}
		}
		else
		{
			asCQualType Formal = asCallAssessment::QualTypeFromHost(Context, Function->parameterTypes[ParameterIndex]);
			const asCQualType Actual = Fact.QualType;
			if (!Formal.IsValid() || !Actual.IsValid()
				|| !asCallAssessment::SameType(Actual.GetType(), Formal.GetType()))
			{
				asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
				return Out;
			}
		}
	}

	for (int32 Index = 0; Index < ParameterCount; ++Index)
	{
		if (Out.FormalSlots[Index] != asEArgumentSlotKind::Unbound)
		{
			continue;
		}
		if (Index < static_cast<int32>(Function->defaultArgs.GetLength())
			&& Function->defaultArgs[Index]
			&& Function->defaultArgs[Index]->GetLength() > 0)
		{
			asCallAssessment::Reject(Out, asECandidateRejection::UnsupportedDefault);
			return Out;
		}
		if (Request.Mode == asECallAssessmentMode::Incomplete)
		{
			Out.FormalSlots[Index] = asEArgumentSlotKind::UnknownFuture;
			continue;
		}
		asCallAssessment::Reject(Out, asECandidateRejection::MissingRequiredArgument);
		return Out;
	}

	Out.bViable = true;
	Out.Score = Score;
	return Out;
}

asSCandidateAssessment asCSema::AssessIndirectCandidate(
	asCFunctionType* Signature,
	const asSCallAssessmentRequest& Request) const
{
	asSCandidateAssessment Out;
	Out.IndirectSignature = Signature;
	if (!Signature)
	{
		asCallAssessment::Reject(Out, asECandidateRejection::InvalidCandidate);
		return Out;
	}
	const int32 ParameterCount = Signature->GetParameterTypes().Num();
	Out.FormalToSource.Init(INDEX_NONE, ParameterCount);
	Out.FormalSlots.Init(asEArgumentSlotKind::Unbound, ParameterCount);
	Out.AuthoredToFormal.Init(INDEX_NONE, Request.Arguments.Num());
	int32 NextPositional = 0;
	for (int32 SourceIndex = 0; SourceIndex < Request.Arguments.Num(); ++SourceIndex)
	{
		const asSCallArgumentFact& Fact = Request.Arguments[SourceIndex];
		if (!Fact.Name.IsEmpty())
		{
			asCallAssessment::Reject(Out, asECandidateRejection::UnknownNamedArgument);
			return Out;
		}
		if (Fact.bUnknownFuture)
		{
			if (NextPositional < ParameterCount
				&& Out.FormalSlots[NextPositional] == asEArgumentSlotKind::Unbound)
			{
				Out.FormalSlots[NextPositional] = asEArgumentSlotKind::UnknownFuture;
				Out.AuthoredToFormal[SourceIndex] = NextPositional;
				++NextPositional;
			}
			continue;
		}
		const int32 ParameterIndex = NextPositional++;
		if (!Out.FormalToSource.IsValidIndex(ParameterIndex))
		{
			asCallAssessment::Reject(Out, asECandidateRejection::TooManyArguments);
			return Out;
		}
		Out.FormalToSource[ParameterIndex] = SourceIndex;
		Out.FormalSlots[ParameterIndex] = asEArgumentSlotKind::Supplied;
		Out.AuthoredToFormal[SourceIndex] = ParameterIndex;
		const asCQualType Formal = Signature->GetParameterTypes()[ParameterIndex];
		const asCQualType Actual = asCallAssessment::ActualType(Fact);
		asCQualType Target = Formal;
		if (Signature->GetParameterPassingModes().IsValidIndex(ParameterIndex)
			&& Signature->GetParameterPassingModes()[ParameterIndex] == asEParameterPassingMode::In)
		{
			Target = asCallAssessment::ValueType(Formal);
		}
		if (!Actual.IsValid() || !asCallAssessment::SameType(Actual.GetType(), Target.GetType()))
		{
			asCallAssessment::Reject(Out, asECandidateRejection::IncompatibleArgument);
			return Out;
		}
	}
	for (int32 Index = 0; Index < ParameterCount; ++Index)
	{
		if (Out.FormalSlots[Index] != asEArgumentSlotKind::Unbound)
		{
			continue;
		}
		if (Request.Mode == asECallAssessmentMode::Incomplete)
		{
			Out.FormalSlots[Index] = asEArgumentSlotKind::UnknownFuture;
			continue;
		}
		asCallAssessment::Reject(Out, asECandidateRejection::MissingRequiredArgument);
		return Out;
	}
	Out.bViable = true;
	return Out;
}

asSCallAssessment asCSema::AssessCall(const asSCallAssessmentRequest& Request) const
{
	asSCallAssessment Result;
	for (asCDecl* Candidate : Request.ScriptCandidates)
	{
		Result.Candidates.Add(AssessScriptCandidate(dyn_cast<asCFunctionDecl>(Candidate), Request));
	}
	for (const asSExternalCallable& Candidate : Request.HostCandidates)
	{
		Result.Candidates.Add(AssessHostCandidate(Candidate, Request));
	}
	if (Request.IndirectSignature)
	{
		Result.Candidates.Add(AssessIndirectCandidate(Request.IndirectSignature, Request));
	}

	int32 BestScore = MAX_int32;
	bool bAmbiguous = false;
	for (int32 Index = 0; Index < Result.Candidates.Num(); ++Index)
	{
		const asSCandidateAssessment& Candidate = Result.Candidates[Index];
		if (!Candidate.bViable)
		{
			continue;
		}
		if (Candidate.Score < BestScore)
		{
			BestScore = Candidate.Score;
			Result.SelectedIndex = Index;
			bAmbiguous = false;
		}
		else if (Candidate.Score == BestScore && Result.SelectedIndex != INDEX_NONE)
		{
			asCFunctionDecl* Current = Result.Candidates[Result.SelectedIndex].ScriptFunction;
			asCFunctionDecl* Other = Candidate.ScriptFunction;
			if (Request.bPreferNonConstOnTie && Current && Other
				&& Current->IsConstMethod() != Other->IsConstMethod())
			{
				if (!Other->IsConstMethod())
				{
					Result.SelectedIndex = Index;
				}
			}
			else
			{
				bAmbiguous = true;
			}
		}
	}
	if (bAmbiguous)
	{
		Result.SelectedIndex = INDEX_NONE;
		Result.bAmbiguous = true;
	}
	return Result;
}

END_AS_NAMESPACE
