#include "frontend/Sema/as_sema.h"
#include "frontend/AST/as_ast_cast.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE
namespace asConversionSema
	{
		bool SameType(const asCType* Left, const asCType* Right)
		{
			return Left && Right && (Left == Right || Left->GetStableIdentity() == Right->GetStableIdentity());
		}
		int32 DestinationRank(asCQualType Actual, asCQualType Target)
		{
			if (!Actual.IsValid() || Actual.IsHandle() != Target.IsHandle()) return INDEX_NONE;
			if (Actual.IsHandle() && Actual.IsConst() && !Target.IsConst()) return INDEX_NONE;
			if (Target.IsReference() && !Target.IsConst() && (!Actual.IsReference() || Actual.IsConst())) return INDEX_NONE;
			if (SameType(Actual.GetType(), Target.GetType())) return 0;
			if (Target.IsHandle() || Target.IsReference()) return INDEX_NONE;
			const auto* From = dyn_cast<asCBuiltinType>(Actual.GetType());
			const auto* To = dyn_cast<asCBuiltinType>(Target.GetType());
			if (!From || !To) return INDEX_NONE;
			using P = asEPrimitiveType;
			// Maintained AS object-to-primitive preference. Bool is not a numeric fallback.
			static constexpr P Rows[10][10] = {
				{P::Float64,P::Float32,P::Int64,P::UInt64,P::Int32,P::UInt32,P::Int16,P::UInt16,P::Int8,P::UInt8},
				{P::Float32,P::Float64,P::Int64,P::UInt64,P::Int32,P::UInt32,P::Int16,P::UInt16,P::Int8,P::UInt8},
				{P::Int64,P::UInt64,P::Int32,P::UInt32,P::Int16,P::UInt16,P::Int8,P::UInt8,P::Float64,P::Float32},
				{P::UInt64,P::Int64,P::UInt32,P::Int32,P::UInt16,P::Int16,P::UInt8,P::Int8,P::Float64,P::Float32},
				{P::Int32,P::UInt32,P::Int64,P::UInt64,P::Int16,P::UInt16,P::Int8,P::UInt8,P::Float64,P::Float32},
				{P::UInt32,P::Int32,P::UInt64,P::Int64,P::UInt16,P::Int16,P::UInt8,P::Int8,P::Float64,P::Float32},
				{P::Int16,P::UInt16,P::Int32,P::UInt32,P::Int64,P::UInt64,P::Int8,P::UInt8,P::Float64,P::Float32},
				{P::UInt16,P::Int16,P::UInt32,P::Int32,P::UInt64,P::Int64,P::UInt8,P::Int8,P::Float64,P::Float32},
				{P::Int8,P::UInt8,P::Int16,P::UInt16,P::Int32,P::UInt32,P::Int64,P::UInt64,P::Float64,P::Float32},
				{P::UInt8,P::Int8,P::UInt16,P::Int16,P::UInt32,P::Int32,P::UInt64,P::Int64,P::Float64,P::Float32}};
			for (const auto& Row : Rows)
				if (Row[0] == To->GetPrimitive())
					for (int32 Rank = 1; Rank < 10; ++Rank)
						if (Row[Rank] == From->GetPrimitive()) return Rank;
			return INDEX_NONE;
		}
	}

	asCFunctionDecl* asCSema::SelectUserConversion(asCExpr* Value, asCQualType Target, bool bExplicit, bool& bAmbiguous) const
	{
		bAmbiguous = false;
		if (!Value || Value->IsInvalid() || !Target.IsValid()) return nullptr;
		const asCQualType Source = Value->GetExpressionQualType();
		if (!Source.IsValid() || (Target.IsHandle() && Source.IsConst() && !Target.IsConst())) return nullptr;
		asCRecordDecl* Record = ResolveRecord(Source.GetType());
		if (!Record) return nullptr;
		const FString ExplicitName = Target.IsHandle() ? TEXT("opCast") : TEXT("opConv");
		const FString ImplicitName = Target.IsHandle() ? TEXT("opImplCast") : TEXT("opImplConv");
		struct FSearch { asCRecordDecl* Record; int32 Depth; };
		struct FCandidate { asCFunctionDecl* Function; int32 Depth; };
		TArray<FSearch> Pending{{Record, 0}};
		TSet<asCRecordDecl*> Visited;
		TArray<FCandidate> Candidates;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			const FSearch Current = Pending[Index];
			if (!Current.Record || Visited.Contains(Current.Record)) continue;
			Visited.Add(Current.Record);
			for (asCDecl* Member : Current.Record->GetDecls())
			{
				auto* Function = dyn_cast<asCFunctionDecl>(Member);
				if (!Function || Function->IsInvalid() || Function->GetFunctionKind() != asEFunctionKind::Conversion
					|| !Function->GetResolvedSignature() || !Function->GetParameters().IsEmpty()
					|| (Function->GetName() != ImplicitName && (!bExplicit || Function->GetName() != ExplicitName))) continue;
				bool Hidden = false;
				for (const FCandidate& Earlier : Candidates)
					if (Earlier.Depth < Current.Depth && Earlier.Function->GetName() == Function->GetName()
						&& Earlier.Function->IsConstMethod() == Function->IsConstMethod()
						&& Earlier.Function->GetResolvedSignature()->GetReturnType() == Function->GetResolvedSignature()->GetReturnType()) { Hidden = true; break; }
				if (!Hidden) Candidates.Add({Function, Current.Depth});
			}
			for (asCType* Base : Current.Record->GetResolvedBases()) Pending.Add({ResolveRecord(Base), Current.Depth + 1});
		}
		asCFunctionDecl* Best = nullptr;
		int32 BestScore = MAX_int32;
		for (const FCandidate& Candidate : Candidates)
		{
			asCFunctionDecl* Function = Candidate.Function;
			if ((Value->IsReadOnly() && !Function->IsConstMethod()) || !CanAccessMember(Function)) continue;
			const int32 Rank = asConversionSema::DestinationRank(Function->GetResolvedSignature()->GetReturnType(), Target);
			if (Rank == INDEX_NONE) continue;
			const int32 Score = Rank * 4 + (!Value->IsReadOnly() && Function->IsConstMethod() ? 2 : 0)
				+ (bExplicit && Function->GetName() == ImplicitName ? 1 : 0);
			if (Score < BestScore) { Best = Function; BestScore = Score; bAmbiguous = false; }
			else if (Score == BestScore && Best != Function) bAmbiguous = true;
		}
		return bAmbiguous ? nullptr : Best;
	}

	asCExpr* asCSema::ConvertUserDefined(asCExpr* Value, asCQualType Target, bool bExplicit)
	{
		bool Ambiguous = false;
		asCFunctionDecl* Function = SelectUserConversion(Value, Target, bExplicit, Ambiguous);
		if (Ambiguous) ReportBodyError(Value->GetSourceRange(), TEXT("ambiguous-user-conversion"));
		if (!Function || !CheckCallContext(*Function, Value->GetSourceRange())) return nullptr;
		const asCQualType Return = Function->GetResolvedSignature()->GetReturnType();
		auto* Conversion = Context.Create<asCUserConversionExpr>(Value, Function,
			bExplicit ? asEUserConversionMode::Explicit : asEUserConversionMode::Implicit, Value->GetSourceRange());
		Conversion->SetExpressionQualType(Return, Return.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		if (asConversionSema::SameType(Return.GetType(), Target.GetType())) return Conversion;
		// A builtin numeric step is a separate typed node, never a fabricated return.
		auto* Numeric = Context.Create<asCImplicitCastExpr>(Conversion, asEImplicitConversionKind::Numeric, Value->GetSourceRange());
		Numeric->SetExpressionSemantics(Target.GetType(), asEValueCategory::PRValue);
		return Numeric;
	}
END_AS_NAMESPACE
