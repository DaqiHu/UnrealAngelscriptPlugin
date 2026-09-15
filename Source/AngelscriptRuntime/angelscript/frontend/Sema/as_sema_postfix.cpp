#include "frontend/Sema/as_sema.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_type_context.h"
#include "as_scriptfunction.h"
#include "as_datatype.h"
#include "as_typeinfo.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE

namespace asPostfixSema
	{
		bool SameType(const asCType* Left, const asCType* Right)
		{
			return Left && Right && (Left == Right || (Left->HasStableIdentity() && Left->GetStableIdentity() == Right->GetStableIdentity()));
		}
		bool Enum(const asCType* Type)
		{
			const asCNominalType* Nominal = dyn_cast<asCNominalType>(Type);
			asSNominalTypeDescriptor Descriptor;
			return Nominal && Nominal->GetOwnerContext()
				&& Nominal->GetOwnerContext()->GetTypeContext()->GetIdentityRegistry().ResolveNominalType(Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
				&& Descriptor.Kind == asETypeDeclKind::Enum;
		}
		bool Numeric(const asCType* Type)
		{
			const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type);
			return Enum(Type) || (Builtin && Builtin->GetPrimitive() != asEPrimitiveType::Void && Builtin->GetPrimitive() != asEPrimitiveType::Bool);
		}
		bool Integral(const asCType* Type)
		{
			const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type);
			return Enum(Type) || (Numeric(Type) && Builtin->GetPrimitive() != asEPrimitiveType::Float32 && Builtin->GetPrimitive() != asEPrimitiveType::Float64);
		}
		asCQualType ValueType(asCQualType Type)
		{
			asCQualType Value;
			asCQualType::TryCreateRaw(Type.GetType(), static_cast<uint8>(Type.GetQualifiers()) & ~static_cast<uint8>(asETypeQualifiers::Reference), Value);
			return Value;
		}
	}

	asCExpr* asCSema::ConvertTo(asCExpr* Value, asCQualType Target)
	{
		if (!Value || !Target.IsValid()) return nullptr;
		if (asCInitListExpr* List = dyn_cast<asCInitListExpr>(Value)) return ConvertInitList(*List, Target);
		if (asCNullPtrLiteral* Null = dyn_cast<asCNullPtrLiteral>(Value))
		{
			if (!Target.IsHandle()) return nullptr;
			const asCQualType Type = asPostfixSema::ValueType(Target);
			Null->SetExpressionQualType(Type, asEValueCategory::PRValue);
			auto* Conversion = Context.Create<asCImplicitCastExpr>(Null, asEImplicitConversionKind::NullToHandle, Null->GetSourceRange());
			Conversion->SetExpressionQualType(Type, asEValueCategory::PRValue);
			return Conversion;
		}
		const asCQualType Actual = Value->GetExpressionQualType();
		if (!Actual.IsValid()) return nullptr;
		if (!asPostfixSema::SameType(Actual.GetType(), Target.GetType()) || Actual.IsHandle() != Target.IsHandle())
			if (asCExpr* UserConversion = ConvertUserDefined(Value, Target, false)) return UserConversion;
		if (Actual.IsHandle() != Target.IsHandle()) return nullptr;
		if (Target.IsReference() && !Target.IsConst() && (Value->GetValueCategory() != asEValueCategory::LValue || Value->IsReadOnly())) return nullptr;
		if (asPostfixSema::SameType(Actual.GetType(), Target.GetType()))
		{
			if (Actual.IsHandle() && Actual.IsConst() && !Target.IsConst()) return nullptr;
			return Value;
		}
		if (Target.IsHandle() || Target.IsReference()) return nullptr;
		return ConvertTo(Value, Target.GetType());
	}

	asCRecordDecl* asCSema::ResolveRecord(const asCType* Type) const
	{
		if (const asCGenericType* Generic = dyn_cast<asCGenericType>(Type)) Type = Generic->GetTemplateType();
		const asCNominalType* Nominal = dyn_cast<asCNominalType>(Type);
		return Nominal && NominalDeclLookup ? dyn_cast<asCRecordDecl>(NominalDeclLookup(Nominal->GetDeclarationIdentity())) : nullptr;
	}

	asCExpr* asCSema::ActOnThis(bool bImplicit, asCSourceRange Range)
	{
		asCRecordDecl* Record = GetCurrentRecord();
		if (!Record || Record->GetDeclaredTypeKind() == asETypeDeclKind::Enum)
			return ActOnRecoveryExpression(Range, TEXT("this-outside-instance-context"));
		asSStableTypeUseDescriptor Use;
		Use.Kind = asETypeUseKind::Nominal; Use.Declaration = Record->GetStableIdentity();
		asSStableKey Key;
		if (Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(Use, Key) != asETypeIdentityStatus::Succeeded)
			return ActOnRecoveryExpression(Range, TEXT("invalid-this-type"));
		asCFunctionDecl* Function = GetCurrentFunction();
		asETypeQualifiers Qualifiers = asETypeQualifiers::Handle;
		if (Function && Function->IsConstMethod()) Qualifiers = Qualifiers | asETypeQualifiers::Const;
		asCQualType Type;
		asCQualType::TryCreate(Context.GetNominalType(Use.Declaration, Key), Qualifiers, Type);
		auto* This = Context.Create<asCThisExpr>(Record, bImplicit, Range);
		This->SetExpressionQualType(Type, asEValueCategory::PRValue);
		return This;
	}

	void asCSema::LookupMembers(asCRecordDecl* Record, const FString& Name, TArray<asCDecl*>& Out, int32 Depth) const
	{
		if (!Record || Depth > 64) return;
		const TConstArrayView<asCDecl*> Direct = Record->Lookup(Name);
		if (!Direct.IsEmpty()) { Out.Append(Direct.GetData(), Direct.Num()); return; }
		for (asCType* Base : Record->GetResolvedBases()) LookupMembers(ResolveRecord(Base), Name, Out, Depth + 1);
	}

	asCExpr* asCSema::ActOnMember(asCExpr* Base, FString Name, asCSourceRange NameRange, asCSourceRange Range)
	{
		TArray<asCDecl*> Members;
		if (Base) LookupMembers(ResolveRecord(Base->GetExpressionType()), Name, Members);
		if (Members.Num() != 1)
			return ActOnRecoveryExpression(Range, FString::Printf(TEXT("unresolved-member:%s"), *Name));
		asCNamedDecl* Member = dyn_cast<asCNamedDecl>(Members[0]);
		if (!CanAccessMember(Member)) return ActOnRecoveryExpression(Range, TEXT("inaccessible-member"));
		asCVarDecl* Variable = dyn_cast<asCVarDecl>(Member);
		asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Member);
		if (!Variable && !Function) return ActOnRecoveryExpression(Range, TEXT("member-is-not-a-value"));
		auto* Result = Context.Create<asCMemberExpr>(Base, Member, NameRange, Range);
		asCQualType Type;
		if (Variable) Type = Variable->GetResolvedQualType();
		else asCQualType::TryCreate(Function->GetResolvedSignature(), asETypeQualifiers::None, Type);
		Result->SetExpressionQualType(Type, Variable ? asEValueCategory::LValue : asEValueCategory::PRValue);
		const asSMemberAccess Access = GetMemberAccess(Member);
		if (Variable && (Base->IsReadOnly() || (!Access.Write && !(Access.Edit && IsEditDefaultsContext())))) Result->SetReadOnly(true);
		return Result;
	}

	asCFunctionDecl* asCSema::ResolveCall(TConstArrayView<asCDecl*> Candidates, TConstArrayView<asSParsedCallArgument> Arguments,
		TArray<asCExpr*>& OutArguments, TArray<int32>& OutOrdinals, asCSourceRange Range, bool bPreferNonConstOnTie)
	{
		asSCallAssessmentRequest Request;
		Request.ScriptCandidates.Append(Candidates.GetData(), Candidates.Num());
		Request.Range = Range;
		Request.Mode = asECallAssessmentMode::Complete;
		Request.bPreferNonConstOnTie = bPreferNonConstOnTie;
		for (const asSParsedCallArgument& Argument : Arguments)
		{
			asSCallArgumentFact Fact;
			Fact.Name = Argument.Name;
			Fact.Value = Argument.Value;
			Request.Arguments.Add(Fact);
		}
		const asSCallAssessment Assessment = AssessCall(Request);
		asCFunctionDecl* Best = Assessment.SelectedScript();
		if (!Best || Assessment.bAmbiguous) return nullptr;
		if (!CheckCallContext(*Best, Range)) return nullptr;
		OutOrdinals = Assessment.Selected()->FormalToSource;
		for (int32 Index = 0; Index < OutOrdinals.Num(); ++Index)
		{
			asCVarDecl* Parameter = Best->GetParameters()[Index];
			asCExpr* Value = nullptr;
			if (OutOrdinals[Index] == INDEX_NONE)
			{
				auto* Default = Context.Create<asCDefaultArgumentExpr>(Parameter, Range);
				Default->SetExpressionQualType(asPostfixSema::ValueType(Parameter->GetResolvedQualType()), asEValueCategory::PRValue);
				Value = Default;
			}
			else
			{
				asCQualType Target = Parameter->GetResolvedQualType();
				if (Parameter->GetTypeSyntax().Reference == asETypeReferenceDirection::In) Target = asPostfixSema::ValueType(Target);
				Value = ConvertTo(Arguments[OutOrdinals[Index]].Value, Target);
			}
			if (!Value) return nullptr;
			OutArguments.Add(Value);
		}
		if ((static_cast<uint16>(Best->GetModifiers()) & static_cast<uint16>(asEFunctionModifiers::Deprecated)) != 0)
			ReportBodyWarning(Range, TEXT("deprecated-function-call"));
		return Best;
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
		case ttFloat64: case ttDouble: return asEPrimitiveType::Float64;
		case ttFloat: return asEPrimitiveType::Float32;
		case ttVoid: return asEPrimitiveType::Void;
		default: return asEPrimitiveType::Invalid;
		}
	}

	bool FormalMatchesExpr(const asCDataType& Formal, const asCExpr& Value)
	{
		const asCType* Actual = Value.GetExpressionType();
		if (!Actual) return false;
		if (asCTypeInfo* Info = Formal.GetTypeInfo())
		{
			const auto* Nominal = dyn_cast<asCNominalType>(Actual);
			return Nominal && Nominal->GetDeclarationIdentity() == Info->GetStableKey();
		}
		const auto* Builtin = dyn_cast<asCBuiltinType>(Actual);
		const asEPrimitiveType Primitive = PrimitiveFromToken(Formal.GetTokenType());
		return Builtin && Primitive != asEPrimitiveType::Invalid && Builtin->GetPrimitive() == Primitive;
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
			if (Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(Use, Key) != asETypeIdentityStatus::Succeeded) return {};
			asCQualType::TryCreate(Context.GetNominalType(Use.Declaration, Key), Qualifiers, Result);
			return Result;
		}
		const asEPrimitiveType Primitive = PrimitiveFromToken(Type.GetTokenType());
		if (Primitive == asEPrimitiveType::Invalid) return {};
		asCQualType::TryCreate(Context.GetBuiltinType(Primitive), Qualifiers, Result);
		return Result;
	}

	const asSExternalCallable* asCSema::ResolveExternalCall(TConstArrayView<asSExternalCallable> Candidates,
		TConstArrayView<asSParsedCallArgument> Arguments, TArray<asCExpr*>& OutArguments,
		TArray<int32>& OutOrdinals, asCSourceRange Range)
	{
		asSCallAssessmentRequest Request;
		Request.HostCandidates.Append(Candidates.GetData(), Candidates.Num());
		Request.Range = Range;
		Request.Mode = asECallAssessmentMode::Complete;
		for (const asSParsedCallArgument& Argument : Arguments)
		{
			asSCallArgumentFact Fact;
			Fact.Name = Argument.Name;
			Fact.Value = Argument.Value;
			Request.Arguments.Add(Fact);
		}
		const asSCallAssessment Assessment = AssessCall(Request);
		if (!Assessment.Selected() || Assessment.bAmbiguous)
		{
			for (const asSCandidateAssessment& Candidate : Assessment.Candidates)
			{
				if (Candidate.Rejection == asECandidateRejection::UnsupportedDefault)
				{
					ReportBodyError(Range, TEXT("unsupported-default"));
					break;
				}
			}
			return nullptr;
		}
		const asSExternalCallable* Best = nullptr;
		for (const asSExternalCallable& Candidate : Candidates)
		{
			if (Candidate.Function == Assessment.Selected()->HostCallable.Function
				&& Candidate.Key == Assessment.Selected()->HostCallable.Key)
			{
				Best = &Candidate;
				break;
			}
		}
		if (!Best) return nullptr;
		OutOrdinals = Assessment.Selected()->FormalToSource;
		for (int32 Index = 0; Index < OutOrdinals.Num(); ++Index)
		{
			asCExpr* Argument = Arguments[OutOrdinals[Index]].Value;
			const asETypeModifiers Flags = Index < static_cast<int32>(Best->Function->inOutFlags.GetLength())
				? Best->Function->inOutFlags[Index] : asTM_NONE;
			if ((Flags == asTM_OUTREF || Flags == asTM_INOUTREF)
				&& (Argument->GetValueCategory() != asEValueCategory::LValue || Argument->IsReadOnly()))
			{
				return nullptr;
			}
			asCExpr* Value = ConvertTo(Argument, QualTypeFromHost(Context, Best->Function->parameterTypes[Index]));
			if (!Value) return nullptr;
			OutArguments.Add(Value);
		}
		return Best;
	}

	asCExpr* asCSema::ActOnExternalCall(const asSExternalCallable& Callable, TArray<asCExpr*> Converted,
		TArray<int32> Ordinals, asCSourceRange Range)
	{
		asCQualType Return = QualTypeFromHost(Context, Callable.Function->returnType);
		if (!Return.IsValid()) return ActOnRecoveryExpression(Range, TEXT("unresolved-host-return"));
		asSFunctionIdentityDescriptor Descriptor;
		asCFunctionType* SignatureType = Callable.Signature;
		if (!SignatureType)
		{
			auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
			if (Registry.ResolveFunction(Callable.Key, Descriptor) != asETypeIdentityStatus::Succeeded)
				return ActOnRecoveryExpression(Range, TEXT("unresolved-host-identity"));
			asSStableTypeUseDescriptor Signature;
			Signature.Kind = asETypeUseKind::Callable;
			Signature.ReturnType = Descriptor.ReturnType;
			Signature.Parameters = Descriptor.Parameters;
			asSStableKey SignatureKey;
			if (Registry.InternTypeUse(Signature, SignatureKey) != asETypeIdentityStatus::Succeeded)
				return ActOnRecoveryExpression(Range, TEXT("unresolved-host-signature"));
			TArray<asCQualType> ParameterTypes;
			TArray<asEParameterPassingMode> Modes;
			for (const auto& Parameter : Descriptor.Parameters)
			{
				asSStableTypeUseDescriptor Use;
				if (Registry.ResolveTypeUse(Parameter.Type, Use) != asETypeIdentityStatus::Succeeded
					|| Use.Kind != asETypeUseKind::Primitive)
					return ActOnRecoveryExpression(Range, TEXT("unresolved-host-parameter"));
				asETypeQualifiers Qualifiers = asETypeQualifiers::None;
				if ((static_cast<uint16>(Use.Qualifiers) & static_cast<uint16>(asETypeUseQualifiers::Const)) != 0)
					Qualifiers = Qualifiers | asETypeQualifiers::Const;
				if ((static_cast<uint16>(Use.Qualifiers) & static_cast<uint16>(asETypeUseQualifiers::Reference)) != 0
					|| Parameter.PassingMode != asEParameterPassingMode::Value)
					Qualifiers = Qualifiers | asETypeQualifiers::Reference;
				if ((static_cast<uint16>(Use.Qualifiers) & static_cast<uint16>(asETypeUseQualifiers::Handle)) != 0)
					Qualifiers = Qualifiers | asETypeQualifiers::Handle;
				asCQualType ParameterType;
				asCQualType::TryCreate(Context.GetBuiltinType(Use.Primitive), Qualifiers, ParameterType);
				ParameterTypes.Add(ParameterType);
				Modes.Add(Parameter.PassingMode);
			}
			SignatureType = Context.GetFunctionType(Return, MoveTemp(ParameterTypes), SignatureKey, MoveTemp(Modes));
		}
		if (!SignatureType) return ActOnRecoveryExpression(Range, TEXT("unresolved-host-signature"));
		auto* Callee = Context.Create<asCDeclRefExpr>(Callable.Function, Callable.Key, Range);
		Callee->SetExpressionSemantics(SignatureType, asEValueCategory::PRValue);
		auto* Call = Context.Create<asCCallExpr>(Callee, Range);
		for (asCExpr* Value : Converted) Call->AddArgument(Value);
		Call->SetArgumentSourceOrdinals(MoveTemp(Ordinals));
		Call->SetExpressionQualType(Return, Return.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Call;
	}

	asCExpr* asCSema::ActOnCallWithArguments(FString Name, TArray<asSParsedCallArgument> Arguments, asCSourceRange Range)
	{
		asCVarDecl* Variable = nullptr;
		for (int32 Index = BodyScopes.Num() - 1; !Variable && Index >= 0; --Index) Variable = BodyScopes[Index].FindRef(Name);
		if (!Variable && CurrentBody && CurrentBody->GetWorkItem().Function)
			for (asCVarDecl* Parameter : CurrentBody->GetWorkItem().Function->GetParameters())
				if (Parameter->GetName() == Name) { Variable = Parameter; break; }
		if (!Variable && BodyLookup)
		{
			const auto Found = BodyLookup(Name);
			if (Found.Num() == 1) Variable = dyn_cast<asCVarDecl>(Found[0]);
		}
		if (Variable) return ActOnIndirectCall(ActOnDeclReference(Name, Range), MoveTemp(Arguments), Range);
		if (GetCurrentRecord() && !Name.Contains(TEXT("::")))
		{
			TArray<asCDecl*> Members;
			LookupMembers(GetCurrentRecord(), Name, Members);
			if (!Members.IsEmpty()) return ActOnMemberCall(ActOnThis(true, Range), Name, MoveTemp(Arguments), Range, Range);
		}
		TArray<asCExpr*> Converted;
		TArray<int32> Ordinals;
		TArray<asCDecl*> GlobalCandidates;
		if (BodyLookup)
			for (asCDecl* Candidate : BodyLookup(Name))
				if (auto* FunctionCandidate = dyn_cast<asCFunctionDecl>(Candidate))
					if ((static_cast<uint16>(FunctionCandidate->GetModifiers()) & static_cast<uint16>(asEFunctionModifiers::Mixin)) == 0) GlobalCandidates.Add(Candidate);
		asCFunctionDecl* Function = ResolveCall(GlobalCandidates, Arguments, Converted, Ordinals, Range);
		if (!Function && GetCurrentRecord())
			if (asCExpr* MixinCall = TryActOnMixinCall(nullptr, Name, Arguments, Range)) return MixinCall;
		if (!Function && ExternalCallableLookup)
		{
			TArray<asCExpr*> HostConverted;
			TArray<int32> HostOrdinals;
			if (const asSExternalCallable* Host = ResolveExternalCall(ExternalCallableLookup(Name), Arguments, HostConverted, HostOrdinals, Range))
				return ActOnExternalCall(*Host, MoveTemp(HostConverted), MoveTemp(HostOrdinals), Range);
		}
		if (!Function) return ActOnRecoveryExpression(Range, FString::Printf(TEXT("unresolved-call:%s"), *Name));
		auto* Callee = Context.Create<asCDeclRefExpr>(Function, Range);
		Callee->SetExpressionSemantics(Function->GetResolvedSignature(), asEValueCategory::PRValue);
		auto* Call = Context.Create<asCCallExpr>(Callee, Range);
		for (asCExpr* Value : Converted) Call->AddArgument(Value);
		Call->SetArgumentSourceOrdinals(MoveTemp(Ordinals));
		const asCQualType Return = Function->GetResolvedSignature()->GetReturnType();
		Call->SetExpressionQualType(Return, Return.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Call;
	}

	asCExpr* asCSema::ActOnMemberCall(asCExpr* Base, FString Name, TArray<asSParsedCallArgument> Arguments, asCSourceRange NameRange, asCSourceRange Range)
	{
		TArray<asCDecl*> Candidates;
		if (Base) LookupMembers(ResolveRecord(Base->GetExpressionType()), Name, Candidates);
		Candidates.RemoveAll([this](asCDecl* Decl) { return !CanAccessMember(Decl); });
		if (Base && Base->IsReadOnly()) Candidates.RemoveAll([](asCDecl* Decl) { const auto* Function = dyn_cast<asCFunctionDecl>(Decl); return Function && !Function->IsConstMethod(); });
		TArray<asCExpr*> Converted;
		TArray<int32> Ordinals;
		asCFunctionDecl* Function = ResolveCall(Candidates, Arguments, Converted, Ordinals, Range, Base && !Base->IsReadOnly());
		if (!Function)
			if (asCExpr* MixinCall = TryActOnMixinCall(Base, Name, Arguments, Range)) return MixinCall;
		if (!Function && Base && ExternalCallableLookup)
		{
			if (const auto* Nominal = dyn_cast<asCNominalType>(Base->GetExpressionType()))
			{
				asSNominalTypeDescriptor Descriptor;
				FString Qualified = Name;
				if (Context.GetTypeContext()->GetIdentityRegistry().ResolveNominalType(Nominal->GetDeclarationIdentity(), Descriptor)
					== asETypeIdentityStatus::Succeeded)
					Qualified = Descriptor.Name + TEXT("::") + Name;
				TArray<asSExternalCallable> HostCandidates;
				for (const asSExternalCallable& Candidate : ExternalCallableLookup(Qualified))
				{
					if (Candidate.OwnerKey != Nominal->GetDeclarationIdentity()) continue;
					if (Base->IsReadOnly() && !Candidate.bConstMethod) continue;
					HostCandidates.Add(Candidate);
				}
				TArray<asCExpr*> HostConverted;
				TArray<int32> HostOrdinals;
				if (const asSExternalCallable* Host = ResolveExternalCall(HostCandidates, Arguments, HostConverted, HostOrdinals, Range))
					return ActOnExternalCall(*Host, MoveTemp(HostConverted), MoveTemp(HostOrdinals), Range);
			}
		}
		if (!Function) return ActOnRecoveryExpression(Range, FString::Printf(TEXT("unresolved-member-call:%s"), *Name));
		auto* Callee = Context.Create<asCMemberExpr>(Base, Function, NameRange, Range);
		Callee->SetExpressionSemantics(Function->GetResolvedSignature(), asEValueCategory::PRValue);
		auto* Call = Context.Create<asCCallExpr>(Callee, Range);
		for (asCExpr* Value : Converted) Call->AddArgument(Value);
		Call->SetArgumentSourceOrdinals(MoveTemp(Ordinals));
		const asCQualType Return = Function->GetResolvedSignature()->GetReturnType();
		Call->SetExpressionQualType(Return, Return.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Call;
	}

	asCExpr* asCSema::TryActOnMixinCall(asCExpr* Base, const FString& Name, TConstArrayView<asSParsedCallArgument> Arguments, asCSourceRange Range)
	{
		if (!BodyLookup) return nullptr;
		TArray<asCDecl*> Candidates;
		for (asCDecl* Candidate : BodyLookup(Name))
			if (auto* Function = dyn_cast<asCFunctionDecl>(Candidate))
				if ((static_cast<uint16>(Function->GetModifiers()) & static_cast<uint16>(asEFunctionModifiers::Mixin)) != 0) Candidates.Add(Candidate);
		if (Candidates.IsEmpty()) return nullptr;
		if (!Base) Base = ActOnThis(true, Range);
		if (!Base || Base->IsInvalid()) return nullptr;
		asCExpr* Receiver = Base;
		if (isa<asCThisExpr>(Base))
		{
			asCQualType ObjectReference;
			asETypeQualifiers Qualifiers = asETypeQualifiers::Reference;
			if (Base->IsReadOnly()) Qualifiers = Qualifiers | asETypeQualifiers::Const;
			asCQualType::TryCreate(Base->GetExpressionType(), Qualifiers, ObjectReference);
			auto* Binding = Context.Create<asCImplicitCastExpr>(Base, asEImplicitConversionKind::ReferenceBinding, Base->GetSourceRange());
			Binding->SetExpressionQualType(ObjectReference, asEValueCategory::LValue); Receiver = Binding;
		}
		TArray<asSParsedCallArgument> WithReceiver;
		WithReceiver.Add({Receiver, {}, {}});
		WithReceiver.Append(Arguments.GetData(), Arguments.Num());
		TArray<asCExpr*> Converted; TArray<int32> Ordinals;
		asCFunctionDecl* Function = ResolveCall(Candidates, WithReceiver, Converted, Ordinals, Range);
		if (!Function) return nullptr;
		auto* Callee = Context.Create<asCDeclRefExpr>(Function, Range);
		Callee->SetExpressionSemantics(Function->GetResolvedSignature(), asEValueCategory::PRValue);
		auto* Call = Context.Create<asCCallExpr>(Callee, Range);
		for (asCExpr* Argument : Converted) Call->AddArgument(Argument);
		Call->SetArgumentSourceOrdinals(MoveTemp(Ordinals));
		const asCQualType Return = Function->GetResolvedSignature()->GetReturnType();
		Call->SetExpressionQualType(Return, Return.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Call;
	}

	asCExpr* asCSema::ActOnIndirectCall(asCExpr* Callee, TArray<asSParsedCallArgument> Arguments, asCSourceRange Range)
	{
		asCFunctionType* Signature = Callee ? dyn_cast<asCFunctionType>(Callee->GetExpressionType()) : nullptr;
		if (!Signature && Callee && NominalDeclLookup)
		{
			if (asCNominalType* Nominal = dyn_cast<asCNominalType>(Callee->GetExpressionType()))
				if (auto* Callable = dyn_cast<asCCallableTypeDecl>(NominalDeclLookup(Nominal->GetDeclarationIdentity()))) Signature = Callable->GetResolvedSignature();
		}
		if (!Signature || Signature->GetParameterTypes().Num() != Arguments.Num())
			return ActOnRecoveryExpression(Range, TEXT("invalid-indirect-call-signature"));
		auto* Call = Context.Create<asCCallExpr>(Callee, Range);
		for (int32 Index = 0; Index < Arguments.Num(); ++Index)
		{
			if (!Arguments[Index].Name.IsEmpty()) return ActOnRecoveryExpression(Range, TEXT("named-argument-requires-declaration"));
			asCQualType Target = Signature->GetParameterTypes()[Index];
			if (Signature->GetParameterPassingModes()[Index] == asEParameterPassingMode::In) Target = asPostfixSema::ValueType(Target);
			asCExpr* Converted = ConvertTo(Arguments[Index].Value, Target);
			if (!Converted) return ActOnRecoveryExpression(Range, TEXT("incompatible-indirect-call-argument"));
			Call->AddArgument(Converted);
		}
		TArray<int32> Ordinals;
		for (int32 Index = 0; Index < Arguments.Num(); ++Index) Ordinals.Add(Index);
		Call->SetArgumentSourceOrdinals(MoveTemp(Ordinals));
		const asCQualType Return = Signature->GetReturnType();
		Call->SetExpressionQualType(Return, Return.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Call;
	}

	asCExpr* asCSema::ActOnSubscript(asCExpr* Base, TArray<asCExpr*> Arguments, asCSourceRange Range)
	{
		if (!Base || Arguments.IsEmpty()) return ActOnRecoveryExpression(Range, TEXT("invalid-subscript"));
		asCQualType Element;
		asCFunctionDecl* Operator = nullptr;
		if (asCGenericType* Generic = dyn_cast<asCGenericType>(Base->GetExpressionType()); Generic && Generic->GetArguments().Num() == 1)
		{
			asCNominalType* Nominal = dyn_cast<asCNominalType>(Generic->GetTemplateType());
			asSNominalTypeDescriptor Descriptor;
			if (Nominal && Context.GetTypeContext()->GetIdentityRegistry().ResolveNominalType(Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
				&& Descriptor.Name == TEXT("array") && Arguments.Num() == 1 && asPostfixSema::Integral(Arguments[0]->GetExpressionType()))
				Element = Generic->GetArguments()[0];
		}
		if (!Element.IsValid())
		{
			TArray<asCDecl*> Candidates;
			LookupMembers(ResolveRecord(Base->GetExpressionType()), TEXT("opIndex"), Candidates);
			if (Base->IsReadOnly()) Candidates.RemoveAll([](asCDecl* Decl) { const auto* Function = dyn_cast<asCFunctionDecl>(Decl); return Function && !Function->IsConstMethod(); });
			TArray<asSParsedCallArgument> Parsed;
			for (asCExpr* Value : Arguments) Parsed.Add({Value, {}, {}});
			TArray<asCExpr*> Converted;
			TArray<int32> Ordinals;
			Operator = ResolveCall(Candidates, Parsed, Converted, Ordinals, Range, !Base->IsReadOnly());
			if (!Operator) return ActOnRecoveryExpression(Range, TEXT("unresolved-index-operator"));
			Element = Operator->GetResolvedSignature()->GetReturnType();
			Arguments = MoveTemp(Converted);
		}
		auto* Index = Context.Create<asCArraySubscriptExpr>(Base, MoveTemp(Arguments), Operator, Range);
		Index->SetExpressionQualType(Element, !Operator || Element.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		if (Base->IsReadOnly()) Index->SetReadOnly(true);
		return Index;
	}

	asCExpr* asCSema::ActOnExplicitCast(asSTypeSyntax TypeSyntax, asCExpr* Operand, asCSourceRange Range)
	{
		const asCQualType Target = ResolveBodyType(TypeSyntax);
		if (!Target.IsValid() || !Operand) return ActOnRecoveryExpression(Range, TEXT("invalid-cast-target"));
		const asCQualType Actual = Operand->GetExpressionQualType();
		if (!asPostfixSema::SameType(Target.GetType(), Actual.GetType()) || Target.IsHandle() != Actual.IsHandle())
		{
			if (asCExpr* Converted = ConvertUserDefined(Operand, Target, true))
			{
				auto* Cast = Context.Create<asCExplicitCastExpr>(Converted, MoveTemp(TypeSyntax), Range);
				Cast->SetExpressionQualType(Target, Target.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
				return Cast;
			}
		}
		bool bCompatible = asPostfixSema::SameType(Target.GetType(), Actual.GetType())
			|| (asPostfixSema::Numeric(Target.GetType()) && asPostfixSema::Numeric(Actual.GetType()));
		if (dyn_cast<asCNullPtrLiteral>(Operand) && Target.IsHandle())
		{
			Operand->SetExpressionQualType(asPostfixSema::ValueType(Target), asEValueCategory::PRValue);
			bCompatible = true;
		}
		if (!bCompatible || (!dyn_cast<asCNullPtrLiteral>(Operand) && Target.IsHandle() != Actual.IsHandle())
			|| (Actual.IsHandle() && Actual.IsConst() && !Target.IsConst())
			|| (Target.IsReference() && (Operand->GetValueCategory() != asEValueCategory::LValue || (!Target.IsConst() && Operand->IsReadOnly()))))
			return ActOnRecoveryExpression(Range, TEXT("incompatible-explicit-cast"));
		auto* Cast = Context.Create<asCExplicitCastExpr>(Operand, MoveTemp(TypeSyntax), Range);
		Cast->SetExpressionQualType(Target, Target.IsReference() ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Cast;
	}

	asCExpr* asCSema::ActOnConstruct(asSTypeSyntax TypeSyntax, TArray<asSParsedCallArgument> Arguments, asCSourceRange Range)
	{
		const asCQualType Type = ResolveBodyType(TypeSyntax);
		if (!Type.IsValid() || Type.IsHandle() || Type.IsReference()) return ActOnRecoveryExpression(Range, TEXT("invalid-construction-type"));
		TArray<asCExpr*> Converted;
		TArray<int32> Ordinals;
		asCFunctionDecl* Constructor = nullptr;
		if (asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type.GetType()))
		{
			if (Builtin->GetPrimitive() == asEPrimitiveType::Void || Arguments.Num() != 1
				|| !Arguments[0].Name.IsEmpty()) return ActOnRecoveryExpression(Range, TEXT("invalid-primitive-construction"));
			asCExpr* Value = ConvertUserDefined(Arguments[0].Value, Type, true);
			if (!Value) Value = ConvertTo(Arguments[0].Value, Type);
			if (!Value) return ActOnRecoveryExpression(Range, TEXT("incompatible-primitive-construction"));
			Converted.Add(Value);
		}
		else
		{
			asCRecordDecl* Record = ResolveRecord(Type.GetType());
			if (!Record) return ActOnRecoveryExpression(Range, TEXT("missing-construction-definition"));
			TArray<asCDecl*> Candidates;
			LookupMembers(Record, Record->GetName(), Candidates);
			if (!Candidates.IsEmpty())
			{
				Constructor = ResolveCall(Candidates, Arguments, Converted, Ordinals, Range);
				if (!Constructor) return ActOnRecoveryExpression(Range, TEXT("unresolved-constructor"));
			}
			else if (Arguments.Num() == 1 && Arguments[0].Name.IsEmpty() && asPostfixSema::SameType(Arguments[0].Value->GetExpressionType(), Type.GetType()))
				Converted.Add(Arguments[0].Value);
			else if (Arguments.Num() == 1 && Arguments[0].Name.IsEmpty())
			{
				asCExpr* Value = ConvertUserDefined(Arguments[0].Value, Type, true);
				if (!Value) return ActOnRecoveryExpression(Range, TEXT("no-matching-constructor"));
				Converted.Add(Value);
			}
			else if (!Arguments.IsEmpty()) return ActOnRecoveryExpression(Range, TEXT("no-matching-constructor"));
		}
		auto* Construction = Context.Create<asCConstructExpr>(MoveTemp(TypeSyntax), MoveTemp(Converted), Constructor, Range);
		if (Ordinals.Num() != Construction->GetArguments().Num())
		{
			Ordinals.Reset();
			for (int32 Index = 0; Index < Construction->GetArguments().Num(); ++Index) Ordinals.Add(Index);
		}
		Construction->SetArgumentSourceOrdinals(MoveTemp(Ordinals));
		Construction->SetExpressionQualType(Type, asEValueCategory::PRValue);
		return Construction;
	}

	asCNullPtrLiteral* asCSema::ActOnNullLiteral(asCSourceRange Range)
	{
		return Context.Create<asCNullPtrLiteral>(MoveTemp(Range));
	}

	bool asCSema::FinishExpressionStatement(asCExpr* Expression)
	{
		if (!Expression || !Expression->GetExpressionQualType().IsValid())
		{
			ReportBodyError(Expression ? Expression->GetSourceRange() : asCSourceRange{}, TEXT("expression-requires-contextual-type"));
			return false;
		}
		asCExpr* RootExpression = Expression;
		while (auto* Paren = dyn_cast<asCParenExpr>(RootExpression)) RootExpression = Paren->GetSubExpr();
		if (auto* Call = dyn_cast<asCCallExpr>(RootExpression))
		{
			asCFunctionDecl* Function = nullptr;
			if (auto* Reference = dyn_cast<asCDeclRefExpr>(Call->GetCallee())) Function = dyn_cast<asCFunctionDecl>(Reference->GetDecl());
			else if (auto* Member = dyn_cast<asCMemberExpr>(Call->GetCallee())) Function = dyn_cast<asCFunctionDecl>(Member->GetMember());
			const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Call->GetExpressionType());
			if (Function && !(Builtin && Builtin->GetPrimitive() == asEPrimitiveType::Void)
				&& (static_cast<uint16>(Function->GetModifiers()) & static_cast<uint16>(asEFunctionModifiers::NoDiscard)) != 0)
				ReportBodyWarning(Expression->GetSourceRange(), TEXT("nodiscard-call-result-unused"));
		}
		return !Expression->IsInvalid();
	}

	asCExpr* asCSema::ActOnStringLiteral(FString Spelling, asCSourceRange Range)
	{
		const bool bTriple = Spelling.Len() >= 6 && Spelling[0] == Spelling[1] && Spelling[1] == Spelling[2];
		const int32 Delimiter = bTriple ? 3 : 1;
		if (Spelling.Len() < Delimiter * 2) return ActOnRecoveryExpression(Range, TEXT("unterminated-string-literal"));
		FString Value;
		auto AppendCodePoint = [&Value](uint32 Code)
		{
			if (Code > 0x10FFFF || (Code >= 0xD800 && Code <= 0xDFFF)) return false;
			if (Code <= 0xFFFF) Value.AppendChar(static_cast<TCHAR>(Code));
			else
			{
				Code -= 0x10000;
				Value.AppendChar(static_cast<TCHAR>(0xD800 + (Code >> 10)));
				Value.AppendChar(static_cast<TCHAR>(0xDC00 + (Code & 0x3FF)));
			}
			return true;
		};
		for (int32 Index = Delimiter; Index < Spelling.Len() - Delimiter; ++Index)
		{
			TCHAR Character = Spelling[Index];
			if (bTriple || Character != TEXT('\\')) { Value.AppendChar(Character); continue; }
			if (++Index >= Spelling.Len() - Delimiter) return ActOnRecoveryExpression(Range, TEXT("incomplete-string-escape"));
			Character = Spelling[Index];
			switch (Character)
			{
			case TEXT('n'): Value.AppendChar(TEXT('\n')); break;
			case TEXT('r'): Value.AppendChar(TEXT('\r')); break;
			case TEXT('t'): Value.AppendChar(TEXT('\t')); break;
			case TEXT('0'): Value.AppendChar(TCHAR(0)); break;
			case TEXT('b'): Value.AppendChar(TEXT('\b')); break;
			case TEXT('f'): Value.AppendChar(TEXT('\f')); break;
			case TEXT('v'): Value.AppendChar(TEXT('\v')); break;
			case TEXT('a'): Value.AppendChar(TCHAR(7)); break;
			case TEXT('\\'): case TEXT('"'): case TEXT('\''): Value.AppendChar(Character); break;
			case TEXT('x'): case TEXT('u'): case TEXT('U'):
			{
				const int32 Digits = Character == TEXT('x') ? 2 : Character == TEXT('u') ? 4 : 8;
				uint32 Code = 0;
				for (int32 DigitIndex = 0; DigitIndex < Digits; ++DigitIndex)
				{
					if (++Index >= Spelling.Len() - Delimiter) return ActOnRecoveryExpression(Range, TEXT("incomplete-string-escape"));
					const TCHAR Hex = Spelling[Index];
					const int32 Digit = Hex >= TEXT('0') && Hex <= TEXT('9') ? Hex - TEXT('0')
						: Hex >= TEXT('a') && Hex <= TEXT('f') ? Hex - TEXT('a') + 10
						: Hex >= TEXT('A') && Hex <= TEXT('F') ? Hex - TEXT('A') + 10 : -1;
					if (Digit < 0) return ActOnRecoveryExpression(Range, TEXT("invalid-string-escape"));
					Code = (Code << 4) | static_cast<uint32>(Digit);
				}
				if (!AppendCodePoint(Code)) return ActOnRecoveryExpression(Range, TEXT("invalid-string-codepoint"));
				break;
			}
			default: return ActOnRecoveryExpression(Range, TEXT("unknown-string-escape"));
			}
		}
		const FString& TypeName = Context.GetTypeContext()->GetOptions().StringLiteralTypeName;
		asCType* Type = BodyTypeLookup ? BodyTypeLookup(TypeName) : nullptr;
		if (!Type) return ActOnRecoveryExpression(Range, TEXT("missing-string-literal-provider"));
		auto* Literal = Context.Create<asCStringLiteral>(MoveTemp(Value), Range);
		Literal->SetExpressionSemantics(Type, asEValueCategory::PRValue);
		return Literal;
	}

END_AS_NAMESPACE
