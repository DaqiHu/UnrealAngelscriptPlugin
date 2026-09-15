#include "frontend/Sema/as_tooling_session.h"

#include "as_builder.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_type.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Lexer/as_token.h"
#include "frontend/Sema/as_call_assessment.h"

BEGIN_AS_NAMESPACE

namespace
{
	TUniquePtr<asCBuilder> MakeDeclarationBuilder(const asSAnalysisInputs& Inputs)
	{
		asSBuilderOptions Options = Inputs.Options;
		Options.Dependencies.Reset();
		for (const TSharedPtr<asCModuleDefinitionSet, ESPMode::ThreadSafe>& Owned : Inputs.OwnedDefinitionSets)
		{
			Options.Dependencies.Add(Owned.Get());
		}
		TUniquePtr<asCBuilder> Builder = MakeUnique<asCBuilder>(
			MakeArrayView(Inputs.Sources),
			Options);
		Builder->RunThrough(asEBuilderStage::DeclarationsResolved);
		return Builder;
	}

	bool PositionInNonCode(
		TConstArrayView<asSDirectiveProcessResult> Sources,
		const asSQueryPosition& Position)
	{
		for (const asSDirectiveProcessResult& Source : Sources)
		{
			for (const asCToken& Token : Source.RawTokens)
			{
				FStringView Key;
				if (!Source.Sources.TryGetLogicalSourceKey(Token.Range.Begin.FileID, Key)
					|| Key != Position.LogicalSourceKey)
				{
					continue;
				}
				if (Token.Range.Begin.ByteOffset <= Position.ByteOffset
					&& Position.ByteOffset < Token.Range.End.ByteOffset
					&& (Token.Kind == asETokenKind::StringLiteral
						|| Token.Kind == asETokenKind::LineComment
						|| Token.Kind == asETokenKind::BlockComment))
				{
					return true;
				}
			}
		}
		return false;
	}

	asCSourceRange ReplacementFor(
		TConstArrayView<asSDirectiveProcessResult> Sources,
		const asSQueryPosition& Position)
	{
		for (const asSDirectiveProcessResult& Source : Sources)
		{
			for (const asCToken& Token : Source.ActiveTokens)
			{
				if (Token.Kind != asETokenKind::Identifier)
				{
					continue;
				}
				FStringView Key;
				if (!Source.Sources.TryGetLogicalSourceKey(Token.Range.Begin.FileID, Key)
					|| Key != Position.LogicalSourceKey)
				{
					continue;
				}
				if (Token.Range.Begin.ByteOffset <= Position.ByteOffset
					&& Position.ByteOffset <= Token.Range.End.ByteOffset)
				{
					return Token.Range;
				}
			}
		}
		return {};
	}

	FString UnqualifiedName(const FString& Name)
	{
		int32 Separator = INDEX_NONE;
		if (Name.FindLastChar(TEXT(':'), Separator))
		{
			return Name.Mid(Separator + 1);
		}
		return Name;
	}

	void AddItem(
		asSCompletionResult& Result,
		const FString& Name,
		const FString& TypeName,
		const FString& ExpectedTypeName,
		int32 SequentialRank,
		const asCSourceRange& Replacement)
	{
		if (Name.IsEmpty() || Name == TEXT("function") || Name == TEXT("lambda"))
		{
			return;
		}
		for (const asSCompletionItem& Existing : Result.Items)
		{
			if (Existing.Name == Name)
			{
				return;
			}
		}
		asSCompletionItem Item;
		Item.Name = Name;
		Item.Display = Name;
		Item.Replacement = Replacement;
		if (ExpectedTypeName.IsEmpty())
		{
			Item.Rank = SequentialRank;
		}
		else if (TypeName.IsEmpty())
		{
			Item.Rank = 1;
		}
		else if (TypeName == ExpectedTypeName)
		{
			Item.Rank = 0;
		}
		else
		{
			Item.Rank = 2;
		}
		Result.Items.Add(MoveTemp(Item));
	}

	asCFunctionDecl* FindEnclosingFunction(asCCompilationSession& Session, const asSQueryPosition& Position)
	{
		const asCSourceManager& Sources = Session.GetASTContext().GetSources();
		for (asCASTNode* Node : Session.GetASTContext().GetNodes())
		{
			asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Node);
			if (!Function || !Function->HasDeferredBody())
			{
				continue;
			}
			const asCSourceRange& Body = Function->GetDeferredBodyRange();
			FStringView Key;
			if (!Sources.TryGetLogicalSourceKey(Body.Begin.FileID, Key)
				|| Key != Position.LogicalSourceKey
				|| Position.ByteOffset < Body.Begin.ByteOffset
				|| Position.ByteOffset > Body.End.ByteOffset)
			{
				continue;
			}
			return Function;
		}
		return nullptr;
	}

	void AppendRecordMembers(
		asCCompilationSession& Session,
		asCRecordDecl* Record,
		TArray<asCDecl*>& Out,
		int32 Depth)
	{
		if (!Record || Depth > 64)
		{
			return;
		}
		for (asCDecl* Decl : Record->GetDecls())
		{
			Out.Add(Decl);
		}
		for (asCType* Base : Record->GetResolvedBases())
		{
			const asCType* Type = Base;
			if (const asCGenericType* Generic = dyn_cast<asCGenericType>(Type))
			{
				Type = Generic->GetTemplateType();
			}
			const asCNominalType* Nominal = dyn_cast<asCNominalType>(Type);
			if (!Nominal)
			{
				continue;
			}
			AppendRecordMembers(
				Session,
				dyn_cast<asCRecordDecl>(Session.FindNominalDeclaration(Nominal->GetDeclarationIdentity())),
				Out,
				Depth + 1);
		}
	}

	void AddGrammarKeywords(
		asSCompletionResult& Result,
		const FString& ExpectedTypeName,
		int32& SequentialRank,
		const asCSourceRange& Replacement)
	{
		for (asUINT KindValue = 0; KindValue < static_cast<asUINT>(asETokenKind::Count); ++KindValue)
		{
			const asETokenKind Kind = static_cast<asETokenKind>(KindValue);
			if (!asIsKeyword(Kind))
			{
				continue;
			}
			const FString Spelling(asGetTokenSpelling(Kind));
			AddItem(Result, Spelling, Spelling, ExpectedTypeName, SequentialRank++, Replacement);
		}
	}
}

asSCompletionResult asCToolingSession::Complete(asSQueryPosition Position, const asSCancellationToken& Cancel)
{
	asSCompletionResult Result;
	if (Cancel.IsCancelled())
	{
		Result.Status = asEAnalysisStatus::Cancelled;
		return Result;
	}
	if (!ValidateInputs() || Position.LogicalSourceKey.IsEmpty())
	{
		Result.Status = asEAnalysisStatus::InvalidInput;
		return Result;
	}

	TUniquePtr<asCBuilder> Builder = MakeDeclarationBuilder(Inputs.Get());
	if (PositionInNonCode(Builder->GetPreprocessedSources(), Position))
	{
		Result.Status = asEAnalysisStatus::Success;
		return Result;
	}

	const asSCursorContext Cursor = PrepareCursor(Position, Cancel, Builder.Get());
	if (Cursor.Status != asEAnalysisStatus::Success)
	{
		Result.Status = Cursor.Status;
		return Result;
	}

	const asCSourceRange Replacement = ReplacementFor(Builder->GetPreprocessedSources(), Position);
	asCCompilationSession* Session = Builder->GetCompilationSession();
	Result.Status = asEAnalysisStatus::Success;
	if (!Cursor.ReceiverTypeName.IsEmpty() && Session)
	{
		const TConstArrayView<asCDecl*> Found = Session->Lookup(Cursor.ReceiverTypeName);
		asCRecordDecl* Record = Found.Num() > 0 ? dyn_cast<asCRecordDecl>(Found[0]) : nullptr;
		if (Record)
		{
			Session->GetSema().SetAccessCaller(
				FindEnclosingFunction(*Session, Position),
				[Session](const asSStableKey& Key) { return Session->FindNominalDeclaration(Key); });
			TArray<asCDecl*> Members;
			AppendRecordMembers(*Session, Record, Members, 0);
			for (asCDecl* Decl : Members)
			{
				const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Decl);
				if (!Named || !Session->GetSema().CanAccessMember(Named))
				{
					continue;
				}
				if (Cursor.bReceiverReadOnly)
				{
					if (const asCFunctionDecl* Method = dyn_cast<asCFunctionDecl>(Named))
					{
						if (!Method->IsConstMethod())
						{
							continue;
						}
					}
				}
				AddItem(Result, Named->GetName(), {}, {}, 0, Replacement);
			}
			Session->GetSema().SetAccessCaller(nullptr);
		}
		return Result;
	}

	int32 Rank = 0;
	if (!Cursor.VisibleSymbols.IsEmpty())
	{
		for (const asSCursorVisibleSymbol& Symbol : Cursor.VisibleSymbols)
		{
			AddItem(Result, Symbol.Name, Symbol.TypeName, Cursor.ExpectedTypeName, Rank++, Replacement);
		}
	}
	else
	{
		for (const FString& Name : Cursor.VisibleNames)
		{
			AddItem(Result, Name, {}, Cursor.ExpectedTypeName, Rank++, Replacement);
		}
	}
	AddGrammarKeywords(Result, Cursor.ExpectedTypeName, Rank, Replacement);
	return Result;
}

asSSignatureHelpResult asCToolingSession::GetSignatureHelp(
	asSQueryPosition Position,
	const asSCancellationToken& Cancel)
{
	asSSignatureHelpResult Result;
	if (Cancel.IsCancelled())
	{
		Result.Status = asEAnalysisStatus::Cancelled;
		return Result;
	}
	if (!ValidateInputs() || Position.LogicalSourceKey.IsEmpty())
	{
		Result.Status = asEAnalysisStatus::InvalidInput;
		return Result;
	}

	TUniquePtr<asCBuilder> Builder = MakeDeclarationBuilder(Inputs.Get());
	if (PositionInNonCode(Builder->GetPreprocessedSources(), Position))
	{
		Result.Status = asEAnalysisStatus::Success;
		return Result;
	}

	const asSCursorContext Cursor = PrepareCursor(Position, Cancel, Builder.Get());
	if (Cursor.Status != asEAnalysisStatus::Success)
	{
		Result.Status = Cursor.Status;
		return Result;
	}
	Result.Status = asEAnalysisStatus::Success;
	Result.CallName = Cursor.InnermostCallName;
	if (Cursor.InnermostCallName.IsEmpty())
	{
		return Result;
	}

	asCCompilationSession* Session = Builder->GetCompilationSession();
	if (!Session)
	{
		Result.ActiveParameter = Cursor.CallArgumentNames.Num();
		return Result;
	}

	asSCallAssessmentRequest Request;
	Request.Mode = asECallAssessmentMode::Incomplete;
	TConstArrayView<asCDecl*> Found = Session->Lookup(Cursor.InnermostCallName);
	if (Found.IsEmpty())
	{
		Found = Session->Lookup(UnqualifiedName(Cursor.InnermostCallName));
	}
	for (asCDecl* Decl : Found)
	{
		Request.ScriptCandidates.Add(Decl);
	}
	auto AddHost = [&Request, Session](const FString& Name)
	{
		for (const asSExternalCallable& Callable : Session->FindExternalCallables(Name))
		{
			bool bExists = false;
			for (const asSExternalCallable& Existing : Request.HostCandidates)
			{
				bExists |= Existing.Function == Callable.Function;
			}
			if (!bExists)
			{
				Request.HostCandidates.Add(Callable);
			}
		}
	};
	AddHost(Cursor.InnermostCallName);
	AddHost(UnqualifiedName(Cursor.InnermostCallName));
	for (int32 Index = 0; Index < Cursor.CallArgumentNames.Num(); ++Index)
	{
		asSCallArgumentFact Fact;
		Fact.Name = Cursor.CallArgumentNames[Index];
		Fact.bInitList = Cursor.CallArgumentIsInitList.IsValidIndex(Index) && Cursor.CallArgumentIsInitList[Index];
		const FString TypeName = Cursor.CallArgumentTypeNames.IsValidIndex(Index)
			? Cursor.CallArgumentTypeNames[Index] : FString();
		asEPrimitiveType Primitive = asEPrimitiveType::Invalid;
		if (TypeName == TEXT("bool"))
		{
			Primitive = asEPrimitiveType::Bool;
		}
		else if (TypeName == TEXT("int"))
		{
			Primitive = asEPrimitiveType::Int32;
		}
		else if (TypeName == TEXT("float"))
		{
			Primitive = asEPrimitiveType::Float32;
		}
		else if (TypeName == TEXT("double"))
		{
			Primitive = asEPrimitiveType::Float64;
		}
		if (Primitive != asEPrimitiveType::Invalid)
		{
			asCQualType::TryCreate(
				Session->GetASTContext().GetBuiltinType(Primitive),
				asETypeQualifiers::None,
				Fact.QualType);
		}
		Request.Arguments.Add(MoveTemp(Fact));
	}
	const asSCallAssessment Assessment = Session->GetSema().AssessCall(Request);
	Result.SelectedIndex = Assessment.SelectedIndex;
	for (const asSCandidateAssessment& Candidate : Assessment.Candidates)
	{
		asSSignatureCandidate Signature;
		Signature.bViable = Candidate.bViable;
		Signature.Rejection = Candidate.Rejection;
		Signature.AuthoredToFormal = Candidate.AuthoredToFormal;
		Signature.FormalToSource = Candidate.FormalToSource;
		Signature.FormalSlots = Candidate.FormalSlots;
		Signature.Score = Candidate.Score;
		if (asCFunctionDecl* Function = Candidate.ScriptFunction)
		{
			Signature.ReturnTypeName = Function->GetReturnTypeSpelling();
			Signature.Display = Function->GetName();
			for (asCVarDecl* Parameter : Function->GetParameters())
			{
				asSSignatureParameter Owned;
				Owned.Name = Parameter->GetName();
				Owned.TypeName = Parameter->GetTypeSpelling();
				Owned.bHasDefault = Parameter && (Parameter->HasDeferredInitializer() || Parameter->GetInitializer());
				Signature.Parameters.Add(MoveTemp(Owned));
			}
		}
		else if (!Candidate.HostCallable.QualifiedName.IsEmpty())
		{
			Signature.Display = Candidate.HostCallable.QualifiedName;
			if (asCFunctionType* Type = Candidate.HostCallable.Signature)
			{
				for (int32 Index = 0; Index < Type->GetParameterTypes().Num(); ++Index)
				{
					asSSignatureParameter Owned;
					Owned.bHasDefault = false;
					Signature.Parameters.Add(MoveTemp(Owned));
				}
			}
		}
		Result.Signatures.Add(MoveTemp(Signature));
	}
	Result.ActiveParameter = Cursor.ActiveParameter;
	if (!Cursor.bInnermostCallArgumentIsInitList)
	{
		if (const asSCandidateAssessment* Selected = Assessment.Selected())
		{
			if (!Cursor.CallArgumentNames.IsEmpty() && !Cursor.CallArgumentNames.Last().IsEmpty()
				&& Selected->AuthoredToFormal.Num() == Cursor.CallArgumentNames.Num())
			{
				Result.ActiveParameter = Selected->AuthoredToFormal.Last();
			}
		}
	}
	else
	{
		Result.ActiveParameter = FMath::Max(0, Cursor.CallArgumentNames.Num() - 1);
	}
	if (Result.ActiveParameter == INDEX_NONE)
	{
		Result.ActiveParameter = Cursor.CallArgumentNames.Num();
	}
	return Result;
}

END_AS_NAMESPACE
