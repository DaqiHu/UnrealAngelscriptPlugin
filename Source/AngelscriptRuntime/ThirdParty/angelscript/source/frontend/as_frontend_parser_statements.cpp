#include "frontend/as_parser.h"

#include "frontend/as_ast_cast.h"

#include "Misc/ScopeExit.h"

BEGIN_AS_NAMESPACE

bool asCParser::ParseBody(const asSDirectiveProcessResult& Input, asCBodyFragment& Fragment)
	{
		Tokens.Reset();
		Cursor = 0;
		ExpressionDepth = 0;
		SourceSnapshot = Input.SourceSnapshot;
		if (!Input.IsValid() || !Input.SourceSnapshot.IsValid())
		{
			Fragment.MarkError(TEXT("body-input-invalid"));
			return false;
		}
		for (const asCToken& Token : Input.ActiveTokens)
		{
			if (!asIsTrivia(Token.Kind) && Token.Kind != asETokenKind::EndOfFile) Tokens.Add(Token);
		}
		if (Tokens.IsEmpty() || Tokens[0].Kind != asETokenKind::LeftBrace)
		{
			Fragment.MarkError(TEXT("body-missing-opening-brace"));
			return false;
		}
		asCCompoundStmt* Body = ParseCompoundStatement();
		if (Body) Fragment.SetBody(Body);
		return Body && !Fragment.HasErrors();
	}

	asCCompoundStmt* asCParser::ParseCompoundStatement()
	{
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftBrace) return nullptr;
		const int32 Begin = Cursor++;
		Sema.PushBodyScope();
		TArray<asCStmt*> Statements;
		while (Cursor < Tokens.Num() && Tokens[Cursor].Kind != asETokenKind::RightBrace)
		{
			const int32 Before = Cursor;
			if (asCStmt* Statement = ParseBodyStatement()) Statements.Add(Statement);
			if (Cursor <= Before) ++Cursor;
		}
		if (!Tokens.IsValidIndex(Cursor))
		{
			const int32 End = FMath::Max(Begin, Tokens.Num() - 1);
			Statements.Add(Sema.ActOnRecoveryStatement(Span(Begin, End), TEXT("unterminated-compound")));
			asCCompoundStmt* Compound = Sema.ActOnCompound(MoveTemp(Statements), Span(Begin, End));
			Sema.PopBodyScope();
			return Compound;
		}
		const int32 End = Cursor++;
		asCCompoundStmt* Compound = Sema.ActOnCompound(MoveTemp(Statements), Span(Begin, End));
		Sema.PopBodyScope();
		return Compound;
	}

	asCStmt* asCParser::ParseScopedBodyStatement()
	{
		if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::LeftBrace)
			return ParseCompoundStatement();
		Sema.PushBodyScope();
		ON_SCOPE_EXIT { Sema.PopBodyScope(); };
		asCStmt* Statement = ParseBodyStatement();
		if (!Statement) return nullptr;
		// An unbraced controlled statement has its own lexical lifetime scope.
		return Sema.ActOnCompound({Statement}, Statement->GetSourceRange());
	}

	asCExpr* asCParser::ParseParenthesizedCondition()
	{
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftParen) return nullptr;
		++Cursor;
		asCExpr* Condition = ParseExpression();
		if (!Condition || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::RightParen)
			return nullptr;
		++Cursor;
		return Condition;
	}

	asCStmt* asCParser::ParseForStatement()
	{
		const int32 Begin = Cursor++;
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftParen)
			return RecoverBody(Begin, TEXT("for-missing-opening-parenthesis"));
		++Cursor;
		Sema.PushBodyScope();
		ON_SCOPE_EXIT { Sema.PopBodyScope(); };

		TArray<asCStmt*> Initializers;
		if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != asETokenKind::Semicolon)
		{
			const int32 InitializerBegin = Cursor;
			if (!TryParseLocalDeclaration(Initializers))
			{
				asCExpr* Initializer = ParseExpression();
				if (!Initializer)
					return RecoverBody(Begin, TEXT("invalid-for-initializer"));
				Sema.FinishExpressionStatement(Initializer);
				Initializers.Add(Initializer);
			}
			if (Cursor == InitializerBegin)
				return RecoverBody(Begin, TEXT("invalid-for-initializer"));
		}
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
			return RecoverBody(Begin, TEXT("for-missing-initializer-semicolon"));
		++Cursor;

		asCExpr* Condition = nullptr;
		if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != asETokenKind::Semicolon)
			Condition = ParseExpression();
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
			return RecoverBody(Begin, TEXT("for-missing-condition-semicolon"));
		++Cursor;

		TArray<asCExpr*> Increments;
		if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != asETokenKind::RightParen)
		{
			for (;;)
			{
				asCExpr* Increment = ParseExpression();
				if (!Increment) return RecoverBody(Begin, TEXT("invalid-for-increment"));
				Sema.FinishExpressionStatement(Increment);
				Increments.Add(Increment);
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Comma) break;
				++Cursor;
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind == asETokenKind::RightParen)
					return RecoverBody(Begin, TEXT("for-trailing-increment-comma"));
			}
		}
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::RightParen)
			return RecoverBody(Begin, TEXT("for-missing-closing-parenthesis"));
		++Cursor;

		asCForStmt* For = Sema.ActOnForStart(MoveTemp(Initializers), Condition,
			MoveTemp(Increments), Span(Begin, Cursor - 1));
		Sema.PushControlTarget(For);
		asCStmt* Body = ParseScopedBodyStatement();
		Sema.PopControlTarget();
		if (!Body || !For->SetBody(Body)) return RecoverBody(Begin, TEXT("missing-for-body"));
		For->SetSourceRange(Span(Begin, Cursor - 1));
		return For;
	}

	asCStmt* asCParser::ParseSwitchStatement()
	{
		const int32 Begin = Cursor++;
		asCExpr* Selector = ParseParenthesizedCondition();
		if (!Selector) return RecoverBody(Begin, TEXT("malformed-switch-selector"));
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftBrace)
			return RecoverBody(Begin, TEXT("switch-missing-opening-brace"));
		const asCSourceRange HeaderRange = Span(Begin, Cursor - 1);
		++Cursor;

		asCSwitchStmt* Switch = Sema.ActOnSwitchStart(Selector, HeaderRange);
		Sema.PushBodyScope();
		ON_SCOPE_EXIT { Sema.PopBodyScope(); };
		Sema.PushControlTarget(Switch, false);
		ON_SCOPE_EXIT { Sema.PopControlTarget(); };

		asCSwitchClause* CurrentClause = nullptr;
		while (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != asETokenKind::RightBrace)
		{
			if (Tokens[Cursor].Kind == asETokenKind::KwCase || Tokens[Cursor].Kind == asETokenKind::KwDefault)
			{
				if (CurrentClause && Cursor > 0) CurrentClause->SetSourceRange(
					{CurrentClause->GetSourceRange().Begin, Tokens[Cursor - 1].Range.End});
				const bool bCase = Tokens[Cursor].Kind == asETokenKind::KwCase;
				const asCSourceRange KeywordRange = Tokens[Cursor++].Range;
				asCExpr* Label = bCase ? ParseExpression() : nullptr;
				if ((bCase && !Label) || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Colon)
				{
					RecoverBody(Begin, bCase ? TEXT("malformed-case-label") : TEXT("default-missing-colon"));
					continue;
				}
				const asCSourceRange ColonRange = Tokens[Cursor++].Range;
				CurrentClause = bCase
					? static_cast<asCSwitchClause*>(Sema.ActOnCase(*Switch, Label, KeywordRange, ColonRange))
					: static_cast<asCSwitchClause*>(Sema.ActOnDefault(*Switch, KeywordRange, ColonRange));
				if (Switch->GetClauses().Num() > 1)
				{
					asCSwitchClause* Previous = Switch->GetClauses()[Switch->GetClauses().Num() - 2];
					for (asCStmt* Statement : Previous->GetStatements())
						if (asCFallthroughStmt* Fallthrough = dyn_cast<asCFallthroughStmt>(Statement))
							Fallthrough->SetTarget(CurrentClause);
				}
				continue;
			}
			if (!CurrentClause)
			{
				RecoverBody(Cursor, TEXT("switch-statement-requires-clause"));
				continue;
			}
			if (Tokens[Cursor].Kind == asETokenKind::KwFallthrough)
			{
				const int32 FallthroughBegin = Cursor++;
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
				{
					CurrentClause->AddStatement(RecoverBody(FallthroughBegin, TEXT("fallthrough-missing-semicolon")));
					continue;
				}
				CurrentClause->AddStatement(Sema.ActOnFallthrough(Span(FallthroughBegin, Cursor++)));
				continue;
			}
			const int32 StatementBegin = Cursor;
			asCStmt* Statement = ParseBodyStatement();
			if (!Statement)
			{
				Statement = RecoverBody(StatementBegin, TEXT("malformed-switch-statement"));
			}
			if (isa<asCDeclStmt>(Statement) || isa<asCDeclGroupStmt>(Statement))
			{
				Statement->MarkInvalid();
				Sema.ActOnRecoveryStatement(Statement->GetSourceRange(),
					TEXT("switch-clause-declaration-requires-scope"));
			}
			CurrentClause->AddStatement(Statement);
		}

		if (!Tokens.IsValidIndex(Cursor))
		{
			Switch->MarkInvalid();
			Sema.ActOnRecoveryStatement(Span(Begin, FMath::Max(Begin, Tokens.Num() - 1)),
				TEXT("unterminated-switch"));
			return Switch;
		}
		if (CurrentClause && Cursor > 0) CurrentClause->SetSourceRange(
			{CurrentClause->GetSourceRange().Begin, Tokens[Cursor - 1].Range.End});
		const int32 End = Cursor++;
		Switch->SetSourceRange(Span(Begin, End));
		Sema.FinishSwitch(*Switch);
		return Switch;
	}

	bool asCParser::ParseForeachVariable(asSTypeSyntax& OutType, FString& OutName, asCSourceRange& OutRange)
	{
		if (!Tokens.IsValidIndex(Cursor)) return false;
		const int32 Begin = Cursor;
		int32 TypeEnd = Cursor;
		if (Tokens[TypeEnd].Kind == asETokenKind::KwConst) ++TypeEnd;
		if (Tokens.IsValidIndex(TypeEnd) && Tokens[TypeEnd].Kind == asETokenKind::KwAuto)
		{
			OutType.bConst = TypeEnd != Begin;
			OutType.NameParts.Add(Text(TypeEnd));
			OutType.ComponentRanges.Add(Tokens[TypeEnd].Range);
			++TypeEnd;
			if (Tokens.IsValidIndex(TypeEnd) && Tokens[TypeEnd].Kind == asETokenKind::At)
			{
				OutType.bHandle = true;
				++TypeEnd;
			}
			if (Tokens.IsValidIndex(TypeEnd) && Tokens[TypeEnd].Kind == asETokenKind::Amp)
			{
				++TypeEnd;
				OutType.Reference = asETypeReferenceDirection::InOut;
				if (Tokens.IsValidIndex(TypeEnd) && Tokens[TypeEnd].Kind == asETokenKind::KwIn)
				{
					OutType.Reference = asETypeReferenceDirection::In;
					++TypeEnd;
				}
			}
			OutType.Range = Span(Begin, TypeEnd - 1);
		}
		else if (!ParseTypeSyntax(TypeEnd, Tokens.Num(), OutType)) return false;
		if (!Tokens.IsValidIndex(TypeEnd) || Tokens[TypeEnd].Kind != asETokenKind::Identifier) return false;
		OutName = Text(TypeEnd++);
		OutRange = Span(Begin, TypeEnd - 1);
		Cursor = TypeEnd;
		return true;
	}

	asCStmt* asCParser::ParseForeachStatement()
	{
		const int32 Begin = Cursor++;
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftParen)
			return RecoverBody(Begin, TEXT("foreach-missing-opening-parenthesis"));
		++Cursor;
		asSParsedForeachVariable Value;
		if (!ParseForeachVariable(Value.Type, Value.Name, Value.Range))
			return RecoverBody(Begin, TEXT("foreach-invalid-value-variable"));
		asSParsedForeachVariable Key;
		if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::Comma)
		{
			++Cursor;
			if (!ParseForeachVariable(Key.Type, Key.Name, Key.Range))
				return RecoverBody(Begin, TEXT("foreach-invalid-key-variable"));
		}
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Colon)
			return RecoverBody(Begin, TEXT("foreach-missing-colon"));
		const asCSourceRange ColonRange = Tokens[Cursor++].Range;
		asCExpr* RangeExpression = ParseExpression();
		if (!RangeExpression || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::RightParen)
			return RecoverBody(Begin, TEXT("foreach-invalid-range"));
		++Cursor;
		const asCSourceRange HeaderRange = Span(Begin, Cursor - 1);

		Sema.PushBodyScope();
		ON_SCOPE_EXIT { Sema.PopBodyScope(); };
		asCForeachStmt* Foreach = Sema.ActOnForeachStart(MoveTemp(Value), MoveTemp(Key),
			RangeExpression, HeaderRange, ColonRange);
		Sema.PushControlTarget(Foreach);
		asCStmt* Body = ParseScopedBodyStatement();
		Sema.PopControlTarget();
		if (!Body || !Foreach->SetBody(Body)) return RecoverBody(Begin, TEXT("missing-foreach-body"));
		Foreach->SetSourceRange(Span(Begin, Cursor - 1));
		return Foreach;
	}

	asCStmt* asCParser::ParseBodyStatement()
	{
		if (!Tokens.IsValidIndex(Cursor)) return nullptr;
		const int32 Begin = Cursor;
		if (Tokens[Cursor].Kind == asETokenKind::Identifier && Tokens.IsValidIndex(Cursor + 1))
		{
			const FString Name = Text(Cursor);
			const asETokenKind Next = Tokens[Cursor + 1].Kind;
			const bool bBlock = Next == asETokenKind::LeftBrace;
			const bool bStatementValue = Next == asETokenKind::Semicolon || Next == asETokenKind::NumericLiteral
				|| Next == asETokenKind::Identifier || Next == asETokenKind::StringLiteral
				|| Next == asETokenKind::KwNullptr || Next == asETokenKind::KwTrue || Next == asETokenKind::KwFalse
				|| Next == asETokenKind::Plus || Next == asETokenKind::Minus || Next == asETokenKind::Bang
				|| Next == asETokenKind::Tilde || Next == asETokenKind::At;
			const int32 CatchClose = Name == TEXT("catch") && Next == asETokenKind::LeftParen
				? FindMatching(Cursor + 1, asETokenKind::LeftParen, asETokenKind::RightParen) : INDEX_NONE;
			const bool bCatch = Name == TEXT("catch") && (bBlock || (CatchClose != INDEX_NONE
				&& Tokens.IsValidIndex(CatchClose + 1) && Tokens[CatchClose + 1].Kind == asETokenKind::LeftBrace));
			const bool bException = (Name == TEXT("try") && bBlock) || bCatch
				|| (Name == TEXT("throw") && (bStatementValue || Next == asETokenKind::LeftParen));
			const bool bCoroutine = (Name == TEXT("coroutine") && bBlock) || (Name == TEXT("yield") && bStatementValue);
			if (bException || bCoroutine)
			{
				Sema.DiagnoseRemovedFeature(bException ? asERemovedLanguageFeature::ScriptException
					: asERemovedLanguageFeature::ScriptCoroutine, Tokens[Begin].Range);
				// Skip one complete removed statement; a following catch is diagnosed separately.
				// Braces inside a removed construct must not terminate the containing function.
				++Cursor;
				while (Cursor < Tokens.Num() && Tokens[Cursor].Kind != asETokenKind::RightBrace)
				{
					if (Tokens[Cursor].Kind == asETokenKind::LeftBrace)
					{
						const int32 Close = FindMatching(Cursor, asETokenKind::LeftBrace, asETokenKind::RightBrace);
						Cursor = Close != INDEX_NONE ? Close + 1 : Tokens.Num();
						break;
					}
					if (Tokens[Cursor++].Kind == asETokenKind::Semicolon) break;
				}
				return Sema.GetASTContext().Create<asCRecoveryStmt>(Span(Begin, Cursor - 1));
			}
		}
		if (Tokens[Cursor].Kind == asETokenKind::LeftBrace) return ParseCompoundStatement();
		if (Tokens[Cursor].Kind == asETokenKind::Semicolon) return Sema.ActOnNullStatement(Tokens[Cursor++].Range);
		if (Tokens[Cursor].Kind == asETokenKind::KwIf)
		{
			++Cursor;
			asCExpr* Condition = ParseParenthesizedCondition();
			if (!Condition) return RecoverBody(Begin, TEXT("malformed-if-condition"));
			asCStmt* Then = ParseScopedBodyStatement();
			asCStmt* Else = nullptr;
			if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::KwElse)
			{
				++Cursor;
				Else = ParseScopedBodyStatement();
				if (!Else) return RecoverBody(Begin, TEXT("missing-else-statement"));
			}
			if (!Then) return RecoverBody(Begin, TEXT("missing-if-statement"));
			return Sema.ActOnIf(Condition, Then, Else, Span(Begin, Cursor - 1));
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwWhile)
		{
			++Cursor;
			asCExpr* Condition = ParseParenthesizedCondition();
			if (!Condition) return RecoverBody(Begin, TEXT("malformed-while-condition"));
			asCWhileStmt* While = Sema.ActOnWhileStart(Condition, Span(Begin, Cursor - 1));
			Sema.PushControlTarget(While);
			asCStmt* Body = ParseScopedBodyStatement();
			Sema.PopControlTarget();
			if (!Body) return RecoverBody(Begin, TEXT("missing-while-body"));
			While->SetSourceRange(Span(Begin, Cursor - 1));
			return Sema.ActOnWhileFinish(While, Body);
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwFor) return ParseForStatement();
		if (Tokens[Cursor].Kind == asETokenKind::KwSwitch) return ParseSwitchStatement();
		if (Tokens[Cursor].Kind == asETokenKind::KwForeach) return ParseForeachStatement();
		if (Tokens[Cursor].Kind == asETokenKind::KwDo)
		{
			++Cursor;
			asCDoStmt* Do = Sema.ActOnDoStart(Span(Begin, Begin));
			Sema.PushControlTarget(Do);
			asCStmt* Body = ParseScopedBodyStatement();
			Sema.PopControlTarget();
			if (!Body || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::KwWhile)
				return RecoverBody(Begin, TEXT("do-missing-while"));
			++Cursor;
			asCExpr* Condition = ParseParenthesizedCondition();
			if (!Condition || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
				return RecoverBody(Begin, TEXT("malformed-do-condition"));
			++Cursor;
			Do->SetBody(Body);
			if (!Sema.CheckCondition(Condition)) Do->MarkInvalid();
			Do->SetCondition(Condition);
			Do->SetSourceRange(Span(Begin, Cursor - 1));
			return Do;
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwBreak || Tokens[Cursor].Kind == asETokenKind::KwContinue)
		{
			const bool bBreak = Tokens[Cursor++].Kind == asETokenKind::KwBreak;
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
				return RecoverBody(Begin, bBreak ? TEXT("break-missing-semicolon") : TEXT("continue-missing-semicolon"));
			const int32 End = Cursor++;
			return bBreak ? Sema.ActOnBreak(Span(Begin, End)) : Sema.ActOnContinue(Span(Begin, End));
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwReturn)
		{
			++Cursor;
			asCExpr* Value = nullptr;
			if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != asETokenKind::Semicolon)
			{
				Value = ParseExpression();
				if (!Value) return RecoverBody(Begin, TEXT("malformed-return-expression"));
			}
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
				return RecoverBody(Begin, TEXT("return-missing-semicolon"));
			return Sema.ActOnReturn(Value, Span(Begin, Cursor++));
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwCase || Tokens[Cursor].Kind == asETokenKind::KwDefault)
			return RecoverBody(Begin, TEXT("switch-label-outside-switch"));
		if (Tokens[Cursor].Kind == asETokenKind::KwFallthrough)
			return RecoverBody(Begin, TEXT("fallthrough-outside-switch"));

		TArray<asCStmt*> Declarations;
		if (TryParseLocalDeclaration(Declarations))
		{
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
				return RecoverBody(Begin, TEXT("local-declaration-missing-semicolon"));
			const int32 End = Cursor++;
			if (Declarations.Num() == 1)
			{
				Declarations[0]->SetSourceRange(Span(Begin, End));
				return Declarations[0];
			}
			return Sema.GetASTContext().Create<asCDeclGroupStmt>(MoveTemp(Declarations), Span(Begin, End));
		}

		asCExpr* Expression = ParseExpression();
		if (!Expression) return RecoverBody(Begin, TEXT("malformed-statement"));
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
			return RecoverBody(Begin, TEXT("statement-missing-semicolon"));
		++Cursor;
		Sema.FinishExpressionStatement(Expression);
		return Expression;
	}

	asCStmt* asCParser::RecoverBody(int32 Begin, FString Message)
	{
		int32 End = FMath::Clamp(Cursor, 0, Tokens.Num() - 1);
		while (Cursor < Tokens.Num() && Tokens[Cursor].Kind != asETokenKind::Semicolon
			&& Tokens[Cursor].Kind != asETokenKind::RightBrace
			&& Tokens[Cursor].Kind != asETokenKind::KwCase
			&& Tokens[Cursor].Kind != asETokenKind::KwDefault)
		{
			End = Cursor++;
		}
		if (Cursor < Tokens.Num() && Tokens[Cursor].Kind == asETokenKind::Semicolon) End = Cursor++;
		return Sema.ActOnRecoveryStatement(Span(FMath::Clamp(Begin, 0, Tokens.Num() - 1), End), MoveTemp(Message));
	}

END_AS_NAMESPACE
