#include "frontend/as_sema.h"
#include "frontend/as_parser.h"
#include "frontend/as_identifier_table.h"
#include "frontend/as_type_context.h"
#include "Containers/StringConv.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE
namespace asLambdaSema
	{
		asCQualType ValueType(asCQualType Type)
		{
			asCQualType Result;
			uint8 Bits = static_cast<uint8>(Type.GetQualifiers()) & ~static_cast<uint8>(asETypeQualifiers::Reference);
			if (!Type.IsHandle()) Bits &= ~static_cast<uint8>(asETypeQualifiers::Const);
			asCQualType::TryCreateRaw(Type.GetType(), Bits, Result); return Result;
		}
		bool IsFunctionIntroducer(const asCToken& Token)
		{
			if (Token.Kind != asETokenKind::Identifier || !Token.Identifier) return false;
			const auto Bytes = Token.Identifier->GetSpelling();
			static const ANSICHAR Name[] = "function";
			return Bytes.Num() == 8 && FMemory::Memcmp(Bytes.GetData(), Name, 8) == 0;
		}
		uint32 FindOrdinal(const asSDirectiveProcessResult& Input, asCSourceRange ParentRange, asCSourceRange LambdaRange)
		{
			TArray<asCToken> Tokens;
			for (const asCToken& Token : Input.ActiveTokens)
				if (!asIsTrivia(Token.Kind) && Token.Kind != asETokenKind::EndOfFile) Tokens.Add(Token);
			uint32 Ordinal = 0;
			for (int32 Index = 0; Index < Tokens.Num(); ++Index)
			{
				if (!IsFunctionIntroducer(Tokens[Index]) || !Tokens.IsValidIndex(Index + 1)
					|| Tokens[Index + 1].Kind != asETokenKind::LeftParen) continue;
				if (Tokens[Index].Range.Begin == LambdaRange.Begin) return Ordinal;
				if (Tokens[Index].Range.Begin == ParentRange.Begin) continue;
				int32 Parentheses = 0;
				int32 BodyOpen = Index + 1;
				for (; BodyOpen < Tokens.Num(); ++BodyOpen)
				{
					if (Tokens[BodyOpen].Kind == asETokenKind::LeftParen) ++Parentheses;
					else if (Tokens[BodyOpen].Kind == asETokenKind::RightParen && --Parentheses == 0) { ++BodyOpen; break; }
				}
				if (!Tokens.IsValidIndex(BodyOpen) || Tokens[BodyOpen].Kind != asETokenKind::LeftBrace) continue;
				++Ordinal;
				int32 Braces = 0;
				for (Index = BodyOpen; Index < Tokens.Num(); ++Index)
				{
					if (Tokens[Index].Kind == asETokenKind::LeftBrace) ++Braces;
					else if (Tokens[Index].Kind == asETokenKind::RightBrace && --Braces == 0) break;
				}
			}
			return MAX_uint32;
		}
	}

	asCExpr* asCSema::ActOnLambda(TArray<asSParsedLambdaParameter> Parameters, asCSourceRange BodyRange, asCSourceRange Range)
	{
		asCFunctionDecl* Enclosing = GetCurrentFunction();
		if (!Enclosing || !BodyInputLookup)
			return ActOnRecoveryExpression(Range, TEXT("lambda-requires-explicit-function-origin"));
		const uint32 Ordinal = asLambdaSema::FindOrdinal(BodyInputLookup(Enclosing->GetSourceRange()), Enclosing->GetSourceRange(), Range);
		if (Ordinal == MAX_uint32) return ActOnRecoveryExpression(Range, TEXT("lambda-source-origin-unavailable"));
		auto* Function = Context.Create<asCFunctionDecl>(FString{}, Range);
		Function->SetLambdaOrigin(Enclosing, Ordinal);
		Function->SetDeferredBodyRange(BodyRange);
		TSet<FString> Names;
		for (asSParsedLambdaParameter& Parameter : Parameters)
		{
			if (Parameter.Name.IsEmpty() || Names.Contains(Parameter.Name))
				return ActOnRecoveryExpression(Parameter.Range, TEXT("duplicate-lambda-parameter"));
			Names.Add(Parameter.Name);
			auto* Decl = Context.Create<asCVarDecl>(asEDeclKind::ParmVar, MoveTemp(Parameter.Name), Parameter.Range);
			if (Parameter.Type.IsValid()) Decl->SetTypeSyntax(MoveTemp(Parameter.Type));
			Function->AddParameter(Decl);
		}
		auto& OuterVariables = LambdaOuterVariables.Add(Function);
		for (asCVarDecl* Parameter : Enclosing->GetParameters()) OuterVariables.Add(Parameter->GetName(), Parameter);
		for (const auto& Scope : BodyScopes)
			for (const auto& Pair : Scope) OuterVariables.Add(Pair.Key, Pair.Value);
		return Context.Create<asCLambdaExpr>(Function, Range);
	}

	asCReturnStmt* asCSema::ActOnInferredLambdaReturn(asCExpr* Value, asCSourceRange Range)
	{
		auto* Return = Context.Create<asCReturnStmt>(Value, Range, GetCurrentFunction());
		InferredLambdaReturns.Add(Return);
		if (Value && (!Value->GetExpressionQualType().IsValid() || Value->IsInvalid()))
		{
			Return->MarkInvalid(); ReportBodyError(Range, TEXT("lambda-return-needs-concrete-type"));
		}
		return Return;
	}

	bool asCSema::FinalizeLambdaIdentity(asCFunctionDecl& Function)
	{
		if (!Function.GetStableIdentity().IsZero()) return true;
		asCFunctionDecl* Enclosing = Function.GetEnclosingFunction();
		if (!Enclosing || !Function.GetResolvedSignature()) return false;
		// An enclosing inferred lambda obtains its identity after its one body
		// parse. Descendants are finalized in lexical-parent order at that point.
		if (Enclosing->GetStableIdentity().IsZero()) return true;
		auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
		asSFunctionIdentityDescriptor Descriptor;
		Descriptor.StableScope = StableScope;
		Descriptor.Kind = asEFunctionIdentityKind::Lambda;
		Descriptor.EnclosingFunction = Enclosing->GetStableIdentity();
		asSFunctionIdentityDescriptor EnclosingDescriptor;
		if (Registry.ResolveFunction(Descriptor.EnclosingFunction, EnclosingDescriptor) != asETypeIdentityStatus::Succeeded) return false;
		Descriptor.NamespacePath = EnclosingDescriptor.NamespacePath;
		Descriptor.LambdaOrdinal = Function.GetLambdaOrdinal();
		if (Function.GetResolvedSignature()->GetReturnType().GetStableIdentity(Registry, Descriptor.ReturnType) != asETypeIdentityStatus::Succeeded) return false;
		for (int32 Index = 0; Index < Function.GetParameters().Num(); ++Index)
		{
			asSStableParameter Parameter;
			if (Function.GetParameters()[Index]->GetResolvedQualType().GetStableIdentity(Registry, Parameter.Type) != asETypeIdentityStatus::Succeeded) return false;
			Parameter.PassingMode = Function.GetResolvedSignature()->GetParameterPassingModes()[Index];
			Descriptor.Parameters.Add(Parameter);
		}
		asSStableKey Key;
		if (Registry.InternFunction(Descriptor, Key) != asETypeIdentityStatus::Succeeded || !Function.SetStableIdentity(Key)) return false;
		for (int32 Index = 0; Index < Function.GetParameters().Num(); ++Index)
		{
			asSDeclarationIdentityDescriptor Parameter;
			Parameter.StableScope = StableScope; Parameter.Kind = asEDeclarationIdentityKind::Parameter;
			Parameter.Owner = Key; Parameter.Ordinal = Index;
			asSStableKey ParameterKey;
			if (Registry.InternDeclaration(Parameter, ParameterKey) != asETypeIdentityStatus::Succeeded
				|| !Function.GetParameters()[Index]->SetStableIdentity(ParameterKey)) return false;
		}
		return true;
	}

	bool asCSema::AnalyzeLambda(asCLambdaExpr& Lambda, asCFunctionType* Expected, TConstArrayView<asSParsedCallArgument> Arguments, bool bMayCapture)
	{
		asCFunctionDecl* Function = Lambda.GetFunction();
		if (!Function || Function->IsInvalid()) return false;
		if (Function->GetBody()) return Function->GetResolvedSignature() && (bMayCapture || (Function->GetCaptures().IsEmpty() && !Function->GetCapturedThisRecord()));
		const auto Parameters = Function->GetParameters();
		if (Expected && Expected->GetParameterTypes().Num() != Parameters.Num()) return false;
		if (!Expected && Arguments.Num() != Parameters.Num())
		{
			if (!Arguments.IsEmpty()) return false;
			for (asCVarDecl* Parameter : Parameters) if (!Parameter->GetTypeSyntax().IsValid()) return false;
		}
		TArray<asCQualType> ParameterTypes;
		TArray<asEParameterPassingMode> PassingModes;
		for (int32 Index = 0; Index < Parameters.Num(); ++Index)
		{
			asCQualType Type = Parameters[Index]->GetTypeSyntax().IsValid() ? ResolveBodyType(Parameters[Index]->GetTypeSyntax())
				: Expected ? Expected->GetParameterTypes()[Index] : asLambdaSema::ValueType(Arguments[Index].Value->GetExpressionQualType());
			if (!Type.IsValid() || (Expected && Type != Expected->GetParameterTypes()[Index])) return false;
			if (!Parameters[Index]->SetResolvedQualType(Type)) return false;
			ParameterTypes.Add(Type);
			const auto Direction = Parameters[Index]->GetTypeSyntax().Reference;
			const auto AuthoredMode = Direction == asETypeReferenceDirection::In ? asEParameterPassingMode::In
				: Direction == asETypeReferenceDirection::Out ? asEParameterPassingMode::Out
				: Direction == asETypeReferenceDirection::InOut ? asEParameterPassingMode::InOut : asEParameterPassingMode::Value;
			if (Expected && Parameters[Index]->GetTypeSyntax().IsValid() && AuthoredMode != Expected->GetParameterPassingModes()[Index]) return false;
			PassingModes.Add(Expected ? Expected->GetParameterPassingModes()[Index] : AuthoredMode);
		}
		if (Expected)
		{
			Function->SetResolvedSignature(Expected);
			Function->SetResolvedReturnType(Expected->GetReturnType().GetType());
			if (!FinalizeLambdaIdentity(*Function)) return false;
		}
		asCBodyFragment Fragment({Function, Function->GetStableIdentity(), Function->GetDeferredBodyRange()});
		asCSema Nested(Context, Diagnostics, StableScope);
		TArray<asCDecl*> CapturedLookup;
		const TMap<FString, asCVarDecl*> OuterVariables = LambdaOuterVariables.FindRef(Function);
		Nested.BeginBody(Fragment, [this, Function, OuterVariables, &CapturedLookup](const FString& Name) -> TConstArrayView<asCDecl*>
		{
			if (asCVarDecl* Variable = OuterVariables.FindRef(Name))
			{
				Function->AddCapture(Variable); CapturedLookup = {Variable}; return MakeArrayView(CapturedLookup);
			}
			const auto Found = BodyLookup ? BodyLookup(Name) : TConstArrayView<asCDecl*>{};
			if (Found.Num() == 1)
				if (asCVarDecl* Variable = dyn_cast<asCVarDecl>(Found[0]))
					if (!Variable->GetDeclContext() || isa<asCFunctionDecl>(Variable->GetDeclContext()->GetOwnerDecl())) Function->AddCapture(Variable);
			return Found;
		}, BodyTypeLookup, StructuredTypeLookup, NominalDeclLookup, BodyInputLookup, ListInitializationLookup, ExternalCallableLookup);
		Nested.bInferLambdaReturn = Expected == nullptr;
		Nested.bInheritedEditDefaultsContext = bMayCapture && IsEditDefaultsContext();
		Nested.bInheritedConstructionContext = bMayCapture && IsConstructionContext();
		asCParser Parser(Nested);
		const bool bParsed = Parser.ParseBody(BodyInputLookup(Function->GetDeferredBodyRange()), Fragment);
		Nested.EndBody();
		if (!bParsed || Fragment.HasErrors())
		{ Function->MarkInvalid(); ReportBodyError(Lambda.GetSourceRange(), TEXT("invalid-lambda-body")); return false; }
		if (!Expected)
		{
			asCQualType ReturnType;
			asCQualType::TryCreate(Context.GetBuiltinType(asEPrimitiveType::Void), asETypeQualifiers::None, ReturnType);
			bool bFirst = true;
			for (asCReturnStmt* Return : Nested.InferredLambdaReturns)
			{
				asCQualType Actual = Return->GetValue() ? asLambdaSema::ValueType(Return->GetValue()->GetExpressionQualType()) : asCQualType{};
				if (!Return->GetValue()) asCQualType::TryCreate(Context.GetBuiltinType(asEPrimitiveType::Void), asETypeQualifiers::None, Actual);
				if (!Actual.IsValid() || (!bFirst && Actual != ReturnType))
				{ Function->MarkInvalid(); ReportBodyError(Return->GetSourceRange(), TEXT("inconsistent-lambda-return-types")); return false; }
				ReturnType = Actual; bFirst = false;
			}
			auto& Registry = Context.GetTypeContext()->GetIdentityRegistry();
			asSStableTypeUseDescriptor Signature;
			Signature.Kind = asETypeUseKind::Callable;
			if (ReturnType.GetStableIdentity(Registry, Signature.ReturnType) != asETypeIdentityStatus::Succeeded) return false;
			for (int32 Index = 0; Index < ParameterTypes.Num(); ++Index)
			{
				asSStableParameter Parameter;
				if (ParameterTypes[Index].GetStableIdentity(Registry, Parameter.Type) != asETypeIdentityStatus::Succeeded) return false;
				Parameter.PassingMode = PassingModes[Index]; Signature.Parameters.Add(Parameter);
			}
			asSStableKey Key;
			if (Registry.InternTypeUse(Signature, Key) != asETypeIdentityStatus::Succeeded) return false;
			Function->SetResolvedSignature(Context.GetFunctionType(ReturnType, ParameterTypes, Key, PassingModes));
			Function->SetResolvedReturnType(ReturnType.GetType());
		}
		Function->SetBody(Fragment.GetBody());
		if (!FinalizeLambdaIdentity(*Function)) return false;
		CompletedLambdaFunctions.Add(Function);
		for (asCFunctionDecl* Child : Nested.CompletedLambdaFunctions)
		{
			if (!FinalizeLambdaIdentity(*Child)) return false;
			CompletedLambdaFunctions.Add(Child);
		}
		Lambda.SetExpressionSemantics(Function->GetResolvedSignature(), asEValueCategory::PRValue);
		if (!bMayCapture && (!Function->GetCaptures().IsEmpty() || Function->GetCapturedThisRecord()))
		{ ReportBodyError(Lambda.GetSourceRange(), TEXT("capturing-lambda-cannot-escape")); return false; }
		return true;
	}

	asCExpr* asCSema::ConvertLambda(asCLambdaExpr& Lambda, asCQualType Target)
	{
		asCFunctionType* Signature = dyn_cast<asCFunctionType>(Target.GetType());
		if (!Signature && NominalDeclLookup)
			if (asCNominalType* Nominal = dyn_cast<asCNominalType>(Target.GetType()))
				if (asCCallableTypeDecl* Callable = dyn_cast<asCCallableTypeDecl>(NominalDeclLookup(Nominal->GetDeclarationIdentity())))
					if (!Callable->IsMulticast()) Signature = Callable->GetResolvedSignature();
		if (!Signature || !AnalyzeLambda(Lambda, Signature, {}, false)) return nullptr;
		Lambda.SetExpressionQualType(asLambdaSema::ValueType(Target), asEValueCategory::PRValue);
		return &Lambda;
	}
END_AS_NAMESPACE
