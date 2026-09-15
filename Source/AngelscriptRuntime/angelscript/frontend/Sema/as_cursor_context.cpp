#include "frontend/Sema/as_cursor_context.h"

#include "as_builder.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_expr.h"
#include "frontend/AST/as_type.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Lexer/as_token.h"
#include "frontend/Parser/as_parser.h"
#include "frontend/Sema/as_tooling_session.h"

BEGIN_AS_NAMESPACE

namespace
{
	asSDirectiveProcessResult MakePrefixInput(
		TConstArrayView<asSDirectiveProcessResult> Sources,
		const asCSourceRange& BodyRange,
		asUINT CursorOffset)
	{
		asSDirectiveProcessResult Prefix;
		for (const asSDirectiveProcessResult& Source : Sources)
		{
			asSDirectiveProcessResult Candidate;
			Candidate.SnapshotID = Source.SnapshotID;
			Candidate.ConfigurationDigest = Source.ConfigurationDigest;
			Candidate.Sources = Source.Sources;
			Candidate.IdentifierOwner = Source.IdentifierOwner;
			for (const asCToken& Token : Source.ActiveTokens)
			{
				if (Token.Kind == asETokenKind::EndOfFile
					|| Token.Range.Begin.FileID != BodyRange.Begin.FileID
					|| Token.Range.Begin.ByteOffset < BodyRange.Begin.ByteOffset
					|| Token.Range.Begin.ByteOffset >= CursorOffset)
				{
					continue;
				}
				Candidate.ActiveTokens.Add(Token);
			}
			if (!Candidate.ActiveTokens.IsEmpty())
			{
				Candidate.bValid = true;
				return Candidate;
			}
		}
		return Prefix;
	}

	FString ReceiverNameFromExpression(asCCompilationSession& Session, const asCExpr* Base)
	{
		if (!Base)
		{
			return {};
		}
		if (const asCDeclRefExpr* Reference = dyn_cast<asCDeclRefExpr>(Base))
		{
			if (const asCVarDecl* Variable = dyn_cast<asCVarDecl>(Reference->GetDecl()))
			{
				const asSTypeSyntax& Syntax = Variable->GetTypeSyntax();
				if (!Syntax.NameParts.IsEmpty())
				{
					return Syntax.NameParts.Last();
				}
			}
		}
		if (const asCNominalType* Nominal = dyn_cast<asCNominalType>(Base->GetExpressionType()))
		{
			if (const asCNamedDecl* Named = Session.FindNominalDeclaration(Nominal->GetDeclarationIdentity()))
			{
				return Named->GetName();
			}
		}
		return {};
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

	bool IsQueryOffsetValid(const FUtf8String& Text, asUINT Offset)
	{
		const asUINT Length = static_cast<asUINT>(Text.Len());
		if (Offset > Length)
		{
			return false;
		}
		if (Offset == Length)
		{
			return true;
		}
		const UTF8CHAR* Data = *Text;
		return (static_cast<uint8>(Data[Offset]) & 0xC0) != 0x80;
	}

	bool IsQueryPositionValid(const asSAnalysisInputs& Inputs, const asSQueryPosition& Position)
	{
		for (const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>& Source : Inputs.Sources)
		{
			if (Source->GetPath() == Position.LogicalSourceKey)
			{
				return IsQueryOffsetValid(Source->GetSourceText(), Position.ByteOffset);
			}
		}
		return false;
	}

	bool HasLogicalSource(const asSAnalysisInputs& Inputs, const FString& Key)
	{
		for (const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>& Source : Inputs.Sources)
		{
			if (Source->GetPath() == Key)
			{
				return true;
			}
		}
		return false;
	}

	void AddVisible(asSCursorContext& Context, const FString& Name, const FString& TypeName)
	{
		if (Name.IsEmpty() || Context.IsNameVisible(Name))
		{
			return;
		}
		asSCursorVisibleSymbol Symbol;
		Symbol.Name = Name;
		Symbol.TypeName = TypeName;
		Context.VisibleSymbols.Add(MoveTemp(Symbol));
		Context.VisibleNames.Add(Name);
	}

	void CollectFileScopeNames(
		const asCDeclContext& Context,
		const asCSourceManager& Sources,
		const asSQueryPosition& Position,
		asSCursorContext& Out)
	{
		for (asCDecl* Decl : Context.GetDecls())
		{
			const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Decl);
			if (!Named || Named->GetName().IsEmpty())
			{
				continue;
			}
			FStringView Key;
			const asCSourceRange& Range = Named->GetSourceRange();
			if (!Sources.TryGetLogicalSourceKey(Range.Begin.FileID, Key)
				|| Key != Position.LogicalSourceKey
				|| Range.Begin.ByteOffset > Position.ByteOffset)
			{
				continue;
			}
			AddVisible(Out, Named->GetName(), Named->GetName());
			if (const asCNamespaceDecl* Namespace = dyn_cast<asCNamespaceDecl>(Named))
			{
				CollectFileScopeNames(*Namespace, Sources, Position, Out);
			}
		}
	}

	FString TypeNameFromExpression(asCCompilationSession& Session, const asCExpr* Value)
	{
		if (!Value)
		{
			return {};
		}
		if (const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Value->GetExpressionType()))
		{
			switch (Builtin->GetPrimitive())
			{
			case asEPrimitiveType::Bool: return TEXT("bool");
			case asEPrimitiveType::Int32: return TEXT("int");
			case asEPrimitiveType::Int64: return TEXT("int64");
			case asEPrimitiveType::Float32: return TEXT("float");
			case asEPrimitiveType::Float64: return TEXT("double");
			default: return {};
			}
		}
		if (const asCNominalType* Nominal = dyn_cast<asCNominalType>(Value->GetExpressionType()))
		{
			if (const asCNamedDecl* Named = Session.FindNominalDeclaration(Nominal->GetDeclarationIdentity()))
			{
				return Named->GetName();
			}
		}
		return {};
	}

	void CollectSessionFileScope(
		asCCompilationSession& Session,
		const asSQueryPosition& Position,
		asSCursorContext& Out)
	{
		if (const asCTranslationUnitDecl* Unit = Session.GetTranslationUnit())
		{
			CollectFileScopeNames(*Unit, Session.GetASTContext().GetSources(), Position, Out);
		}
	}
}

asSCursorContext asCToolingSession::PrepareCursor(asSQueryPosition Position, const asSCancellationToken& Cancel) const
{
	return PrepareCursor(Position, Cancel, nullptr);
}

asSCursorContext asCToolingSession::PrepareCursor(
	asSQueryPosition Position,
	const asSCancellationToken& Cancel,
	asCBuilder* Prepared) const
{
	asSCursorContext Context;
	if (Cancel.IsCancelled())
	{
		Context.Status = asEAnalysisStatus::Cancelled;
		return Context;
	}
	if (!ValidateInputs() || Position.LogicalSourceKey.IsEmpty())
	{
		Context.Status = asEAnalysisStatus::InvalidInput;
		return Context;
	}
	if (!IsQueryPositionValid(Inputs.Get(), Position))
	{
		Context.Status = asEAnalysisStatus::InvalidPosition;
		return Context;
	}
#if WITH_ANGELSCRIPT_TESTS
	if (bDebugFailDeclarations)
	{
		bDebugFailDeclarations = false;
		Context.Status = asEAnalysisStatus::AnalysisUnavailable;
		return Context;
	}
#endif

	TUniquePtr<asCBuilder> Owned;
	asCBuilder* Builder = Prepared;
	if (!Builder)
	{
		asSBuilderOptions Options = Inputs->Options;
		Options.Dependencies.Reset();
		for (const TSharedPtr<asCModuleDefinitionSet, ESPMode::ThreadSafe>& OwnedSet : Inputs->OwnedDefinitionSets)
		{
			Options.Dependencies.Add(OwnedSet.Get());
		}
		Owned = MakeUnique<asCBuilder>(MakeArrayView(Inputs->Sources), Options);
		Builder = Owned.Get();
	}
	if (Builder->GetStage() < asEBuilderStage::DeclarationsResolved)
	{
		Builder->RunThrough(asEBuilderStage::DeclarationsResolved);
	}
#if WITH_ANGELSCRIPT_TESTS
	ConsumeCancelCheckpoint(asEToolingCancelCheckpoint::AfterDeclarations);
#endif
	if (Cancel.IsCancelled())
	{
		Context.Status = asEAnalysisStatus::Cancelled;
		return Context;
	}

	asCCompilationSession* Session = Builder->GetCompilationSession();
	if (!Session)
	{
		Context.Status = asEAnalysisStatus::AnalysisUnavailable;
		return Context;
	}
	const asECompilationPhase Phase = Session->GetPhase();
	if (Phase != asECompilationPhase::DeclarationsResolved
		&& Phase != asECompilationPhase::BodiesAnalyzed
		&& Phase != asECompilationPhase::ASTVerified)
	{
		Context.Status = asEAnalysisStatus::AnalysisUnavailable;
		return Context;
	}

	asCFunctionDecl* Function = FindEnclosingFunction(*Session, Position);
	if (!Function)
	{
		if (!HasLogicalSource(Inputs.Get(), Position.LogicalSourceKey))
		{
			Context.Status = asEAnalysisStatus::InvalidPosition;
			return Context;
		}
		CollectSessionFileScope(*Session, Position, Context);
		Context.Status = asEAnalysisStatus::Success;
		return Context;
	}

	const asSDirectiveProcessResult Prefix = MakePrefixInput(
		Builder->GetPreprocessedSources(),
		Function->GetDeferredBodyRange(),
		Position.ByteOffset);
	if (!Prefix.IsValid())
	{
		Context.Status = asEAnalysisStatus::InvalidPosition;
		return Context;
	}

	asCSema ScratchSema(Session->GetASTContext(), Session->GetDiagnosticsEngine(), Inputs->Options.StableScope);
	asSDeferredBodyWorkItem Work;
	Work.Function = Function;
	Work.StableFunctionKey = Function->GetStableIdentity();
	Work.BodyRange = Function->GetDeferredBodyRange();
	asCBodyFragment Fragment(Work);
	ScratchSema.BeginBody(
		Fragment,
		[Session, Function](const FString& Name) { return Session->LookupInScope(Name, *Function); },
		[Session, Function](const FString& Name)
		{
			asSTypeSyntax Syntax;
			Syntax.NameParts.Add(Name);
			Syntax.Range = Function->GetSourceRange();
			return Session->ResolveTypeSyntax(Syntax, *Function).GetType();
		},
		[Session, Function](const asSTypeSyntax& Syntax) { return Session->ResolveTypeSyntax(Syntax, *Function); },
		[Session](const asSStableKey& Key) { return Session->FindNominalDeclaration(Key); },
		[Prefix](const asCSourceRange& Range)
		{
			asSDirectiveProcessResult Slice = Prefix;
			Slice.ActiveTokens.Reset();
			for (const asCToken& Token : Prefix.ActiveTokens)
			{
				if (Token.Range.Begin.ByteOffset >= Range.Begin.ByteOffset
					&& Token.Range.End.ByteOffset <= Range.End.ByteOffset)
				{
					Slice.ActiveTokens.Add(Token);
				}
			}
			Slice.bValid = !Slice.ActiveTokens.IsEmpty();
			return Slice;
		},
		[Session](asCQualType Type, asSListInitialization& Out) { return Session->ResolveListInitializer(Type, Out); },
		[](const FString&) { return TConstArrayView<asSExternalCallable>{}; });

	asCParser Parser(ScratchSema);
#if WITH_ANGELSCRIPT_TESTS
	ConsumeCancelCheckpoint(asEToolingCancelCheckpoint::DuringCursorParse);
#endif
	if (Cancel.IsCancelled())
	{
		ScratchSema.EndBody();
		Context.Status = asEAnalysisStatus::Cancelled;
		return Context;
	}
	Parser.ParseBody(Prefix, Fragment);
	if (Cancel.IsCancelled())
	{
		ScratchSema.EndBody();
		Context.Status = asEAnalysisStatus::Cancelled;
		return Context;
	}

	if (const asCExpr* Base = ScratchSema.GetIncompleteMemberBase())
	{
		Context.bReceiverReadOnly = Base->IsReadOnly() || Base->GetExpressionQualType().IsConst();
	}
	Context.ReceiverTypeName = ReceiverNameFromExpression(*Session, ScratchSema.GetIncompleteMemberBase());
	Context.InnermostCallName = ScratchSema.GetIncompleteCallName();
	for (const asSParsedCallArgument& Argument : ScratchSema.GetIncompleteCallArguments())
	{
		Context.CallArgumentNames.Add(Argument.Name);
		Context.CallArgumentTypeNames.Add(TypeNameFromExpression(*Session, Argument.Value));
		Context.CallArgumentIsInitList.Add(Argument.Value && isa<asCInitListExpr>(Argument.Value));
	}
	Context.bInnermostCallArgumentIsInitList = ScratchSema.IsIncompleteArgumentInitList()
		|| (!Context.CallArgumentIsInitList.IsEmpty() && Context.CallArgumentIsInitList.Last());
	Context.ActiveParameter = ScratchSema.GetIncompleteCallName().IsEmpty()
		? INDEX_NONE
		: (Context.bInnermostCallArgumentIsInitList
			? FMath::Max(0, ScratchSema.GetIncompleteCallArgumentCount() - 1)
			: ScratchSema.GetIncompleteCallArgumentCount());

	Context.ExpectedTypeName = ScratchSema.GetExpectedTypeName();
	if (Context.ExpectedTypeName.IsEmpty())
	{
		for (int32 TokenIndex = Prefix.ActiveTokens.Num() - 1; TokenIndex >= 0; --TokenIndex)
		{
			const asCToken& Token = Prefix.ActiveTokens[TokenIndex];
			if (asIsTrivia(Token.Kind))
			{
				continue;
			}
			if (Token.Kind == asETokenKind::KwReturn)
			{
				Context.ExpectedTypeName = Function->GetReturnTypeSpelling();
			}
			break;
		}
	}

	TArray<FString> LocalNames;
	TArray<FString> LocalTypes;
	ScratchSema.CollectVisibleLocalSymbols(LocalNames, LocalTypes);
	for (int32 Index = 0; Index < LocalNames.Num(); ++Index)
	{
		AddVisible(Context, LocalNames[Index], LocalTypes.IsValidIndex(Index) ? LocalTypes[Index] : FString());
	}
	for (asCVarDecl* Parameter : Function->GetParameters())
	{
		if (Parameter)
		{
			AddVisible(Context, Parameter->GetName(), Parameter->GetTypeSpelling());
		}
	}
	CollectSessionFileScope(*Session, Position, Context);

	ScratchSema.EndBody();
	Context.Status = asEAnalysisStatus::Success;
	return Context;
}

END_AS_NAMESPACE
