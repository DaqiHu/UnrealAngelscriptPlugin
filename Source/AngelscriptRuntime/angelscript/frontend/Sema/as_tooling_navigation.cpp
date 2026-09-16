#include "frontend/Sema/as_tooling_session.h"

#include "as_scriptfunction.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_visitor.h"
#include "frontend/AST/as_expr.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Lexer/as_token.h"

BEGIN_AS_NAMESPACE

namespace
{
	bool SameAnalysisInputs(const asSAnalysisInputs& Left, const asSAnalysisInputs& Right)
	{
		if (Left.Options.StableScope != Right.Options.StableScope
			|| Left.Options.TypeContext.Get() != Right.Options.TypeContext.Get()
			|| Left.OwnedDefinitions.Num() != Right.OwnedDefinitions.Num()
			|| Left.Sources.Num() != Right.Sources.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.OwnedDefinitions.Num(); ++Index)
		{
			if (Left.OwnedDefinitions[Index].Get() != Right.OwnedDefinitions[Index].Get())
			{
				return false;
			}
		}
		for (int32 Index = 0; Index < Left.Sources.Num(); ++Index)
		{
			const FAngelscriptSource& LeftSource = *Left.Sources[Index];
			const FAngelscriptSource& RightSource = *Right.Sources[Index];
			if (LeftSource.GetPath() != RightSource.GetPath()
				|| LeftSource.GetSourceText() != RightSource.GetSourceText())
			{
				return false;
			}
		}
		return true;
	}

	const asCToken* SelectToken(
		TConstArrayView<asSDirectiveProcessResult> Sources,
		const asSQueryPosition& Position)
	{
		const asCToken* Containing = nullptr;
		const asCToken* TouchingEnd = nullptr;
		for (const asSDirectiveProcessResult& Source : Sources)
		{
			for (const asCToken& Token : Source.ActiveTokens)
			{
				if (Token.Kind == asETokenKind::EndOfFile)
				{
					continue;
				}
				FStringView Key;
				if (!Source.Sources.TryGetLogicalSourceKey(Token.Range.Begin.FileID, Key)
					|| Key != Position.LogicalSourceKey)
				{
					continue;
				}
				const asUINT Begin = Token.Range.Begin.ByteOffset;
				const asUINT End = Token.Range.End.ByteOffset;
				if (Begin <= Position.ByteOffset && Position.ByteOffset < End)
				{
					Containing = &Token;
					break;
				}
				if (Token.Kind == asETokenKind::Identifier && Position.ByteOffset == End)
				{
					TouchingEnd = &Token;
				}
			}
			if (Containing)
			{
				break;
			}
		}
		return Containing ? Containing : TouchingEnd;
	}

	bool RangeTouches(const asCSourceRange& Range, const asCSourceRange& Token)
	{
		return Range.HasConsistentOwner()
			&& Token.HasConsistentOwner()
			&& Range.Begin.FileID == Token.Begin.FileID
			&& Range.Begin.ByteOffset < Token.End.ByteOffset
			&& Token.Begin.ByteOffset < Range.End.ByteOffset;
	}

	FString BuiltinTypeName(asEPrimitiveType Primitive)
	{
		switch (Primitive)
		{
		case asEPrimitiveType::Int32: return TEXT("int");
		case asEPrimitiveType::UInt32: return TEXT("uint");
		case asEPrimitiveType::Int64: return TEXT("int64");
		case asEPrimitiveType::UInt64: return TEXT("uint64");
		case asEPrimitiveType::Float32: return TEXT("float");
		case asEPrimitiveType::Float64: return TEXT("double");
		case asEPrimitiveType::Bool: return TEXT("bool");
		case asEPrimitiveType::Void: return TEXT("void");
		default: return {};
		}
	}

	FString TypeDisplayName(const asCType* Type, const asCNamedDecl* Decl)
	{
		if (const asCVarDecl* Variable = dyn_cast<asCVarDecl>(Decl))
		{
			const FString Syntax = Variable->GetTypeSyntax().GetQualifiedName();
			if (!Syntax.IsEmpty())
			{
				return Syntax;
			}
		}
		if (const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type))
		{
			return BuiltinTypeName(Builtin->GetPrimitive());
		}
		if (const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Decl))
		{
			return Named->GetName();
		}
		return {};
	}

	struct asSSelectedSymbol
	{
		asCDecl* Decl = nullptr;
		asCScriptFunction* External = nullptr;
		FString Display;
	};

	asSSelectedSymbol SelectSymbol(const asCAnalysisResult& Result, const asCToken& Token)
	{
		asSSelectedSymbol Selected;
		int32 BestLength = MAX_int32;
		const asCCompilationSession* Session = Result.GetSession();
		if (!Session)
		{
			return Selected;
		}
		asCASTVisitor::Walk(Session->GetTranslationUnit(), [&](const asCASTNode& Node)
		{
			asCDecl* Decl = nullptr;
			asCScriptFunction* External = nullptr;
			asCSourceRange Range;
			if (const asCDeclRefExpr* Reference = dyn_cast<asCDeclRefExpr>(&Node))
			{
				Range = Reference->GetSourceRange();
				Decl = Reference->GetDecl();
				External = Reference->GetExternalFunction();
			}
			else if (const asCMemberExpr* Member = dyn_cast<asCMemberExpr>(&Node))
			{
				Range = Member->GetMemberRange();
				Decl = Member->GetMember();
			}
			else
			{
				return;
			}
			if (!RangeTouches(Range, Token.Range))
			{
				return;
			}
			const int32 Length = static_cast<int32>(Range.End.ByteOffset - Range.Begin.ByteOffset);
			if (Length >= BestLength)
			{
				return;
			}
			BestLength = Length;
			Selected.Decl = Decl;
			Selected.External = External;
			if (External)
			{
				Selected.Display = UTF8_TO_TCHAR(External->GetName());
			}
			else if (const asCVarDecl* Variable = dyn_cast<asCVarDecl>(Decl))
			{
				Selected.Display = TypeDisplayName(Variable->GetResolvedType(), Variable);
				if (!Selected.Display.IsEmpty())
				{
					Selected.Display += TEXT(" ");
				}
				Selected.Display += Variable->GetName();
			}
			else if (const asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Decl))
			{
				Selected.Display = Function->GetName();
			}
			else if (const asCNamedDecl* Named = dyn_cast<asCNamedDecl>(Decl))
			{
				Selected.Display = Named->GetName();
			}
		});
		return Selected;
	}

	asCFunctionDecl* PreferDefinition(asCFunctionDecl* Function, const asCCompilationSession& Session)
	{
		if (!Function || Function->GetBody())
		{
			return Function;
		}
		asCFunctionDecl* WithBody = nullptr;
		for (asCDecl* Candidate : Session.Lookup(Function->GetName()))
		{
			if (asCFunctionDecl* Other = dyn_cast<asCFunctionDecl>(Candidate); Other && Other->GetBody())
			{
				WithBody = Other;
				break;
			}
		}
		return WithBody ? WithBody : Function;
	}

	asEAnalysisStatus QueryPrelude(
		const asCToolingSession& Session,
		const asCAnalysisResult& Result,
		const asSQueryPosition& Position,
		const asSCancellationToken& Cancel)
	{
		if (Cancel.IsCancelled())
		{
			return asEAnalysisStatus::Cancelled;
		}
		if (!Result.GetSession() || Position.LogicalSourceKey.IsEmpty())
		{
			return asEAnalysisStatus::InvalidInput;
		}
		if (!SameAnalysisInputs(Session.GetInputs(), Result.GetInputs()))
		{
			return asEAnalysisStatus::SnapshotMismatch;
		}
		return asEAnalysisStatus::Success;
	}
}

asSHoverResult asCToolingSession::GetHover(
	const asCAnalysisResult& Result,
	asSQueryPosition Position,
	const asSCancellationToken& Cancel)
{
	asSHoverResult Hover;
	Hover.Status = QueryPrelude(*this, Result, Position, Cancel);
	if (Hover.Status != asEAnalysisStatus::Success)
	{
		return Hover;
	}
	const asCToken* Token = SelectToken(Result.GetPreprocessedSources(), Position);
	if (!Token || Token->Kind != asETokenKind::Identifier)
	{
		Hover.Status = asEAnalysisStatus::Success;
		return Hover;
	}
	Hover.Display = SelectSymbol(Result, *Token).Display;
	return Hover;
}

asSDefinitionResult asCToolingSession::FindDefinition(
	const asCAnalysisResult& Result,
	asSQueryPosition Position,
	const asSCancellationToken& Cancel)
{
	asSDefinitionResult Definition;
	Definition.Status = QueryPrelude(*this, Result, Position, Cancel);
	if (Definition.Status != asEAnalysisStatus::Success)
	{
		return Definition;
	}
	const asCToken* Token = SelectToken(Result.GetPreprocessedSources(), Position);
	if (!Token || Token->Kind != asETokenKind::Identifier)
	{
		Definition.Status = asEAnalysisStatus::Success;
		Definition.Target = asEDefinitionTargetKind::None;
		return Definition;
	}
	const asSSelectedSymbol Selected = SelectSymbol(Result, *Token);
	if (Selected.External)
	{
		Definition.Target = asEDefinitionTargetKind::NoSourceTarget;
		return Definition;
	}
	asCDecl* Target = Selected.Decl;
	if (asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Target))
	{
		Target = PreferDefinition(Function, *Result.GetSession());
	}
	if (!Target || !Target->GetSourceRange().IsOrdered())
	{
		Definition.Target = asEDefinitionTargetKind::None;
		return Definition;
	}
	Definition.Target = asEDefinitionTargetKind::SourceTarget;
	Definition.Range = Target->GetSourceRange();
	return Definition;
}

END_AS_NAMESPACE
