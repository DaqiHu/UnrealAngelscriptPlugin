#include "frontend/Parser/as_parser.h"
#include "frontend/Parser/as_type_syntax_parser.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_attr.h"
#include "frontend/Basic/as_identifier_table.h"
#include "Containers/StringConv.h"
#include "Misc/ScopeExit.h"

BEGIN_AS_NAMESPACE

FString asCParser::Text(int32 Index) const
	{
		if (!Tokens.IsValidIndex(Index)) return {};
		const asCToken& Token = Tokens[Index];
		if (Token.Identifier)
		{
			const TConstArrayView<uint8> Bytes = Token.Identifier->GetSpelling(); FUTF8ToTCHAR Value(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num()); return FString(Value.Length(), Value.Get());
		}
		const TCHAR* FixedSpelling = asGetTokenSpelling(Token.Kind);
		if (FixedSpelling && FixedSpelling[0] != 0) return FixedSpelling;
		if (SourceSnapshot)
		{
			TConstArrayView<uint8> Bytes;
			if (SourceSnapshot->TryGetBytes(Token.Range.Begin.FileID, Bytes) &&
				Token.Range.End.ByteOffset <= static_cast<asUINT>(Bytes.Num()))
			{
				const int32 Length = static_cast<int32>(Token.Range.Length());
				FUTF8ToTCHAR Value(
					reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Token.Range.Begin.ByteOffset),
					Length);
				return FString(Value.Length(), Value.Get());
			}
		}
		return {};
	}

	FString asCParser::Join(int32 Begin, int32 End) const
	{
		FString Result; for (int32 I=Begin; I<End; ++I) Result += Text(I); return Result;
	}

	asCSourceRange asCParser::Span(int32 Begin, int32 End) const
	{
		if (!Tokens.IsValidIndex(Begin) || !Tokens.IsValidIndex(End)) return {};
		return {Tokens[Begin].Range.Begin, Tokens[End].Range.End};
	}

	int32 asCParser::FindMatching(int32 Open, asETokenKind Left, asETokenKind Right) const
	{
		int32 Depth=0; for(int32 I=Open; I<Tokens.Num(); ++I){if(Tokens[I].Kind==Left)++Depth;else if(Tokens[I].Kind==Right&&--Depth==0)return I;} return INDEX_NONE;
	}

	TArray<asCAttr*> asCParser::ParseAnnotations()
	{
		TArray<asCAttr*> Result;
		while (Tokens.IsValidIndex(Cursor))
		{
			asEAnnotationKind Kind;
			switch (Tokens[Cursor].Kind)
			{
			case asETokenKind::KwUClass: Kind = asEAnnotationKind::UClass; break;
			case asETokenKind::KwUStruct: Kind = asEAnnotationKind::UStruct; break;
			case asETokenKind::KwUEnum: Kind = asEAnnotationKind::UEnum; break;
			case asETokenKind::KwUFunction: Kind = asEAnnotationKind::UFunction; break;
			case asETokenKind::KwUProperty: Kind = asEAnnotationKind::UProperty; break;
			case asETokenKind::KwUMeta: Kind = asEAnnotationKind::UMeta; break;
			default: return Result;
			}
			const int32 Begin = Cursor;
			++Cursor;
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftParen)
			{
				Sema.ActOnRecovery(Span(Begin, Begin), TEXT("annotation-requires-parentheses"), static_cast<asUINT>(asEDeclarationDiagnostic::InvalidAnnotation));
				return Result;
			}
			const int32 Close = FindMatching(Cursor, asETokenKind::LeftParen, asETokenKind::RightParen);
			if (Close == INDEX_NONE)
			{
				Recover(Begin, TEXT("unterminated-annotation"), static_cast<asUINT>(asEDeclarationDiagnostic::InvalidAnnotation));
				return Result;
			}
			TArray<asSAnnotationArgument> Arguments;
			const bool bValid = ParseAnnotationPayload(Cursor + 1, Close, Arguments);
			Cursor = Close + 1;
			if (bValid) Result.Add(Sema.CreateAnnotation(Kind, MoveTemp(Arguments), Span(Begin, Close)));
			else Sema.ActOnRecovery(Span(Begin, Close), TEXT("malformed-annotation-payload"), static_cast<asUINT>(asEDeclarationDiagnostic::InvalidAnnotation));
		}
		return Result;
	}

	bool asCParser::ParseAnnotationPayload(int32 Begin, int32 End, TArray<asSAnnotationArgument>& OutArguments)
	{
		int32 Index = Begin;
		while (Index < End)
		{
			if (Tokens[Index].Kind != asETokenKind::Identifier) return false;
			const int32 NameIndex = Index;
			asSAnnotationArgument Argument;
			Argument.Name = Text(Index++);
			if (Index < End && Tokens[Index].Kind == asETokenKind::Equal)
			{
				const int32 ValueBegin = ++Index;
				if (Index >= End) return false;
				if (Tokens[Index].Kind == asETokenKind::LeftParen)
				{
					const int32 Close = FindMatching(Index, asETokenKind::LeftParen, asETokenKind::RightParen);
					if (Close == INDEX_NONE || Close >= End) return false;
					Argument.Kind = asEAnnotationValueKind::Group;
					if (!ParseAnnotationPayload(Index + 1, Close, Argument.Arguments)) return false;
					Index = Close + 1;
				}
				else
				{
					Argument.Kind = Tokens[Index].Kind == asETokenKind::StringLiteral ? asEAnnotationValueKind::String : asEAnnotationValueKind::Scalar;
					while (Index < End && Tokens[Index].Kind != asETokenKind::Comma)
					{
						if (Tokens[Index].Kind == asETokenKind::LeftParen || Tokens[Index].Kind == asETokenKind::RightParen || Tokens[Index].Kind == asETokenKind::Equal) return false;
						++Index;
					}
					if (Index == ValueBegin) return false;
					if (Argument.Kind == asEAnnotationValueKind::String && Index != ValueBegin + 1) return false;
					Argument.Value = Join(ValueBegin, Index);
					if (Argument.Kind == asEAnnotationValueKind::String)
					{
						Argument.Value = Argument.Value.Mid(1, Argument.Value.Len() - 2);
						Argument.Value.ReplaceInline(TEXT("\\\""), TEXT("\""));
						Argument.Value.ReplaceInline(TEXT("\\'"), TEXT("'"));
						Argument.Value.ReplaceInline(TEXT("\\\\"), TEXT("\\"));
					}
				}
				Argument.ValueRange = Span(ValueBegin, Index - 1);
			}
			Argument.Range = Span(NameIndex, Index - 1);
			OutArguments.Add(MoveTemp(Argument));
			if (Index == End) break;
			if (Tokens[Index++].Kind != asETokenKind::Comma || Index == End) return false;
		}
		return true;
	}

	bool asCParser::Collect(const asSDirectiveProcessResult& Input, asCDeclarationFragment& Fragment)
	{
		Tokens.Reset(); Cursor=0; SourceSnapshot=Input.SourceSnapshot; if(!Input.IsValid())return false;
		for(const asCToken& Token:Input.ActiveTokens)if(!asIsTrivia(Token.Kind)&&Token.Kind!=asETokenKind::EndOfFile)Tokens.Add(Token);
		Sema.BeginFragment(Fragment); while(Cursor<Tokens.Num()) ParseDeclaration(); Sema.EndFragment(); return !Fragment.HasErrors();
	}

	bool asCParser::ParseDeclaration()
	{
		const int32 Before = Cursor;
		TArray<asCAttr*> Attrs = ParseAnnotations();
		if (Cursor >= Tokens.Num())
		{
			if (!Attrs.IsEmpty()) Sema.ActOnRecovery(Span(Before, Cursor - 1), TEXT("annotation-requires-declaration"), static_cast<asUINT>(asEDeclarationDiagnostic::InvalidAnnotation));
			return Attrs.IsEmpty();
		}
		if (ParseRemovedDeclaration()) return false;
		if (Tokens[Cursor].Kind == asETokenKind::KwAccess
			&& (!Tokens.IsValidIndex(Cursor + 1) || Tokens[Cursor + 1].Kind != asETokenKind::Colon))
		{
			if (!Attrs.IsEmpty()) { Recover(Before, TEXT("access-policy-does-not-accept-annotations")); return false; }
			return ParseAccessDeclaration();
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwDefault && Sema.GetCurrentRecord())
		{
			const int32 Begin = Cursor++;
			asCSourceRange StatementRange;
			if (!Attrs.IsEmpty() || !CaptureStatementRange(Cursor, StatementRange))
			{ Recover(Begin, TEXT("malformed-class-default-statement")); return false; }
			asCRecordDecl* Record = Sema.GetCurrentRecord();
			asCFunctionDecl* Function = nullptr;
			for (asCDecl* Member : Record->GetDecls())
				if (asCFunctionDecl* Candidate = dyn_cast<asCFunctionDecl>(Member))
					if (Candidate->GetFunctionKind() == asEFunctionKind::ClassDefault) Function = Candidate;
			if (!Function)
			{
				Function = Sema.ActOnFunction({}, TEXT("void"), Span(Begin, Cursor - 1), {}, {});
				Function->SetFunctionKind(asEFunctionKind::ClassDefault);
			}
			else Function->SetSourceRange({Function->GetSourceRange().Begin, Span(Begin, Cursor - 1).End});
			Function->AddDeferredStatementRange(Span(Begin, Cursor - 1));
			return true;
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwTypedef)
		{
			const int32 Begin = Cursor++;
			asSTypeSyntax Type;
			if (!ParseTypeSyntax(Cursor, Tokens.Num(), Type) || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier)
			{ Recover(Begin, TEXT("malformed-type-alias")); return false; }
			FString Name = Text(Cursor++);
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon)
			{ Recover(Begin, TEXT("type-alias-missing-semicolon")); return false; }
			Sema.ActOnTypeAlias(MoveTemp(Name), MoveTemp(Type), Span(Begin, Cursor++), Attrs);
			return true;
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwDelegate || Tokens[Cursor].Kind == asETokenKind::KwEvent)
		{
			if (!ParseCallableDeclaration(MoveTemp(Attrs))) Recover(Before, TEXT("malformed-callable-declaration"), static_cast<asUINT>(asEDeclarationDiagnostic::InvalidCallable));
			return true;
		}
		if(Tokens[Cursor].Kind==asETokenKind::KwNamespace)return ParseNamespace(MoveTemp(Attrs));
		if(Tokens[Cursor].Kind==asETokenKind::KwClass)return ParseRecord(MoveTemp(Attrs), asETypeDeclKind::Class);
		if(Tokens[Cursor].Kind==asETokenKind::KwStruct)return ParseRecord(MoveTemp(Attrs), asETypeDeclKind::Struct);
		if(Tokens[Cursor].Kind==asETokenKind::KwInterface)return ParseRecord(MoveTemp(Attrs), asETypeDeclKind::Interface);
		if(Tokens[Cursor].Kind==asETokenKind::KwEnum)return ParseRecord(MoveTemp(Attrs), asETypeDeclKind::Enum);
		if (Tokens[Cursor].Kind == asETokenKind::Semicolon)
		{
			if (!Attrs.IsEmpty()) Sema.ActOnRecovery(Span(Before, Cursor), TEXT("annotation-requires-declaration"), static_cast<asUINT>(asEDeclarationDiagnostic::InvalidAnnotation));
			++Cursor; return Attrs.IsEmpty();
		}
		if(!ParseTypedDeclaration(MoveTemp(Attrs)))Recover(Before,TEXT("malformed declaration"));
		return true;
	}

	bool asCParser::ParseNamespace(TArray<asCAttr*> Attrs)
	{
		const int32 Begin=Cursor++; if(!Tokens.IsValidIndex(Cursor)||Tokens[Cursor].Kind!=asETokenKind::Identifier)return false; FString Name=Text(Cursor++);
		if(!Tokens.IsValidIndex(Cursor)||Tokens[Cursor].Kind!=asETokenKind::LeftBrace)return false;
		auto* Decl=Sema.ActOnNamespace(MoveTemp(Name),Span(Begin,Cursor),Attrs); ++Cursor; Sema.PushContext(Decl);
		while(Cursor<Tokens.Num()&&Tokens[Cursor].Kind!=asETokenKind::RightBrace)ParseDeclaration();
		const int32 Close = Cursor < Tokens.Num() ? Cursor : (Cursor > Begin ? Cursor - 1 : Begin);
		Decl->SetSourceRange(Span(Begin, Close));
		if(Cursor<Tokens.Num())++Cursor; Sema.PopContext(); if(Cursor<Tokens.Num()&&Tokens[Cursor].Kind==asETokenKind::Semicolon)++Cursor; return true;
	}

	bool asCParser::ParseRecord(TArray<asCAttr*> Attrs, asETypeDeclKind DeclaredTypeKind)
	{
		const int32 Begin=Cursor++; if(!Tokens.IsValidIndex(Cursor)||Tokens[Cursor].Kind!=asETokenKind::Identifier)return false; FString Name=Text(Cursor++); TArray<FString> Bases;
		if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::Colon)
		{
			++Cursor;
			if (DeclaredTypeKind == asETypeDeclKind::Enum) { Recover(Begin, TEXT("enum-underlying-type-not-supported")); return false; }
			for (;;)
			{
				asSTypeSyntax Base;
				if (!ParseTypeSyntax(Cursor, Tokens.Num(), Base) || Base.bConst || Base.bHandle
					|| Base.Reference != asETypeReferenceDirection::None || !Base.ArrayDimensions.IsEmpty() || !Base.GenericArguments.IsEmpty())
				{ Recover(Begin, TEXT("malformed-base-specifier")); return false; }
				Bases.Add((Base.bGlobal ? TEXT("::") : TEXT("")) + Base.GetQualifiedName());
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Comma) break;
				++Cursor;
			}
		}
		if(!Tokens.IsValidIndex(Cursor)||Tokens[Cursor].Kind!=asETokenKind::LeftBrace)return false;
		if(DeclaredTypeKind==asETypeDeclKind::Enum)
		{
			const int32 Close=FindMatching(Cursor,asETokenKind::LeftBrace,asETokenKind::RightBrace); if(Close==INDEX_NONE)return false;
			auto* Decl=Sema.ActOnRecord(MoveTemp(Name),Span(Begin,Close),DeclaredTypeKind,Bases,Attrs); ++Cursor; Sema.PushContext(Decl);
			ParseEnumBody(*Decl,Close); Sema.PopContext(); Cursor=Close+1;
			if(Cursor<Tokens.Num()&&Tokens[Cursor].Kind==asETokenKind::Semicolon)++Cursor; return true;
		}
		auto* Decl=Sema.ActOnRecord(MoveTemp(Name),Span(Begin,Cursor),DeclaredTypeKind,Bases,Attrs); ++Cursor; Sema.PushContext(Decl);
		while(Cursor<Tokens.Num()&&Tokens[Cursor].Kind!=asETokenKind::RightBrace)ParseDeclaration();
		const int32 Close = Cursor < Tokens.Num() ? Cursor : (Cursor > Begin ? Cursor - 1 : Begin);
		Decl->SetSourceRange(Span(Begin, Close));
		if(Cursor<Tokens.Num())++Cursor; Sema.PopContext(); if(Cursor<Tokens.Num()&&Tokens[Cursor].Kind==asETokenKind::Semicolon)++Cursor; return true;
	}

	bool asCParser::ParseEnumBody(asCRecordDecl& EnumDecl, int32 CloseBrace)
	{
		if (EnumDecl.GetDeclaredTypeKind() != asETypeDeclKind::Enum) return false;
		while (Cursor < CloseBrace)
		{
			const int32 Begin = Cursor;
			if (Tokens[Cursor].Kind != asETokenKind::Identifier)
			{
				Sema.ActOnRecovery(Span(Begin, Begin), TEXT("enum-constant-name-expected"));
				Cursor = CloseBrace; return false;
			}
			FString Name = Text(Cursor++);
			asCSourceRange Initializer;
			const bool bExplicit = Cursor < CloseBrace && Tokens[Cursor].Kind == asETokenKind::Equal;
			if (bExplicit)
			{
				++Cursor;
				if (!CaptureInitializerRange(Cursor, CloseBrace, Initializer))
				{
					Sema.ActOnRecovery(Span(Begin, FMath::Min(Cursor, CloseBrace)), TEXT("enum-initializer-expression-expected"));
					Cursor = CloseBrace; return false;
				}
			}
			TArray<asCAttr*> Attrs = ParseAnnotations();
			asCEnumConstantDecl* Constant = Sema.ActOnEnumConstant(MoveTemp(Name), 0, bExplicit, Span(Begin, Cursor - 1), Attrs);
			Constant->MarkValueUnresolved();
			if (bExplicit) Constant->SetDeferredInitializerRange(Initializer);
			if (Cursor < CloseBrace && Tokens[Cursor].Kind == asETokenKind::Comma) ++Cursor;
			else if (Cursor < CloseBrace)
			{
				Sema.ActOnRecovery(Span(Begin, Cursor), TEXT("enum-constant-separator-expected"));
				Cursor = CloseBrace; return false;
			}
		}
		return true;
	}


	bool asCParser::CaptureInitializerRange(int32& Index, int32 End, asCSourceRange& Range) const
	{
		const int32 Begin = Index;
		int32 Parentheses = 0, Brackets = 0, Braces = 0;
		for (; Index < End; ++Index)
		{
			const asETokenKind Kind = Tokens[Index].Kind;
			if (Parentheses == 0 && Brackets == 0 && Braces == 0
				&& (Kind == asETokenKind::Comma || Kind == asETokenKind::Semicolon || Kind == asETokenKind::RightParen || Kind == asETokenKind::RightBrace || Kind == asETokenKind::KwUMeta)) break;
			if (Kind == asETokenKind::LeftParen) ++Parentheses;
			else if (Kind == asETokenKind::RightParen) { if (--Parentheses < 0) return false; }
			else if (Kind == asETokenKind::LeftBracket) ++Brackets;
			else if (Kind == asETokenKind::RightBracket) { if (--Brackets < 0) return false; }
			else if (Kind == asETokenKind::LeftBrace) ++Braces;
			else if (Kind == asETokenKind::RightBrace) { if (--Braces < 0) return false; }
			else if (Kind == asETokenKind::Identifier)
			{
				int32 TypeEnd = Index;
				asSTypeSyntax Type;
				if (ParseTypeSyntax(TypeEnd, End, Type) && !Type.GenericArguments.IsEmpty()) Index = TypeEnd - 1;
			}
		}
		if (Index == Begin || Parentheses || Brackets || Braces) return false;
		Range = Span(Begin, Index - 1);
		return true;
	}

	bool asCParser::ParseInitializer(const asSDirectiveProcessResult& Input, asCVarDecl& Declaration)
	{
		asCExpr* Value = ParseConstantExpression(Input, Declaration.GetDeferredInitializerRange());
		return Value && Sema.ActOnInitializer(Declaration, Value);
	}

	asCExpr* asCParser::ParseConstantExpression(const asSDirectiveProcessResult& Input, asCSourceRange Range)
	{
		Tokens.Reset(); Cursor = 0; ExpressionDepth = 0; SourceSnapshot = Input.SourceSnapshot;
		if (!Input.IsValid() || !Range.IsOrdered()) return nullptr;
		for (const asCToken& Token : Input.ActiveTokens)
		{
			if (!asIsTrivia(Token.Kind) && Token.Kind != asETokenKind::EndOfFile
				&& Token.Range.Begin.SnapshotID == Range.Begin.SnapshotID && Token.Range.Begin.FileID == Range.Begin.FileID
				&& Token.Range.Begin.ByteOffset >= Range.Begin.ByteOffset && Token.Range.End.ByteOffset <= Range.End.ByteOffset)
				Tokens.Add(Token);
		}
		asCExpr* Value = ParseExpression();
		if (!Value || Cursor != Tokens.Num())
		{
			return Sema.ActOnRecoveryExpression(Range, TEXT("malformed-constant-expression"));
		}
		return Value;
	}

	bool asCParser::CaptureStatementRange(int32& Index, asCSourceRange& Range, int32 Depth) const
	{
		if (Depth > 128 || !Tokens.IsValidIndex(Index)) return false;
		const int32 Begin = Index;
		const asETokenKind Kind = Tokens[Index].Kind;
		if (Kind == asETokenKind::LeftBrace)
		{
			const int32 Close = FindMatching(Index, asETokenKind::LeftBrace, asETokenKind::RightBrace);
			if (Close == INDEX_NONE) return false;
			Index = Close + 1;
		}
		else if (Kind == asETokenKind::KwIf || Kind == asETokenKind::KwWhile || Kind == asETokenKind::KwFor
			|| Kind == asETokenKind::KwForeach || Kind == asETokenKind::KwSwitch)
		{
			if (!Tokens.IsValidIndex(++Index) || Tokens[Index].Kind != asETokenKind::LeftParen) return false;
			const int32 Close = FindMatching(Index, asETokenKind::LeftParen, asETokenKind::RightParen);
			if (Close == INDEX_NONE) return false;
			Index = Close + 1;
			asCSourceRange Child;
			if (!CaptureStatementRange(Index, Child, Depth + 1)) return false;
			if (Kind == asETokenKind::KwIf && Tokens.IsValidIndex(Index) && Tokens[Index].Kind == asETokenKind::KwElse)
			{
				++Index;
				if (!CaptureStatementRange(Index, Child, Depth + 1)) return false;
			}
		}
		else if (Kind == asETokenKind::KwDo)
		{
			++Index; asCSourceRange Child;
			if (!CaptureStatementRange(Index, Child, Depth + 1) || !Tokens.IsValidIndex(Index)
				|| Tokens[Index++].Kind != asETokenKind::KwWhile || !Tokens.IsValidIndex(Index)
				|| Tokens[Index].Kind != asETokenKind::LeftParen) return false;
			const int32 Close = FindMatching(Index, asETokenKind::LeftParen, asETokenKind::RightParen);
			if (Close == INDEX_NONE || !Tokens.IsValidIndex(Close + 1) || Tokens[Close + 1].Kind != asETokenKind::Semicolon) return false;
			Index = Close + 2;
		}
		else
		{
			while (Tokens.IsValidIndex(Index) && Tokens[Index].Kind != asETokenKind::Semicolon)
			{
				const asETokenKind TokenKind = Tokens[Index].Kind;
				if (TokenKind == asETokenKind::RightBrace) return false;
				if (TokenKind == asETokenKind::LeftParen || TokenKind == asETokenKind::LeftBracket || TokenKind == asETokenKind::LeftBrace)
				{
					const asETokenKind Closing = TokenKind == asETokenKind::LeftParen ? asETokenKind::RightParen
						: TokenKind == asETokenKind::LeftBracket ? asETokenKind::RightBracket : asETokenKind::RightBrace;
					const int32 Close = FindMatching(Index, TokenKind, Closing);
					if (Close == INDEX_NONE) return false;
					Index = Close;
				}
				++Index;
			}
			if (!Tokens.IsValidIndex(Index)) return false;
			++Index;
		}
		Range = Span(Begin, Index - 1);
		return Range.IsOrdered();
	}

	asCStmt* asCParser::ParseClassDefaultStatement(const asSDirectiveProcessResult& Input)
	{
		Tokens.Reset(); Cursor = 0; ExpressionDepth = 0; SourceSnapshot = Input.SourceSnapshot;
		if (!Input.IsValid() || !SourceSnapshot.IsValid()) return nullptr;
		for (const asCToken& Token : Input.ActiveTokens)
			if (!asIsTrivia(Token.Kind) && Token.Kind != asETokenKind::EndOfFile) Tokens.Add(Token);
		if (Tokens.IsEmpty() || Tokens[Cursor].Kind != asETokenKind::KwDefault) return nullptr;
		const asCSourceRange KeywordRange = Tokens[Cursor++].Range;
		asCStmt* Statement = ParseBodyStatement();
		if (!Statement || Cursor != Tokens.Num())
			return Sema.ActOnRecoveryStatement(Span(0, Tokens.Num() - 1), TEXT("malformed-class-default-statement"));
		return Sema.GetASTContext().Create<asCClassDefaultStmt>(Statement, KeywordRange, Span(0, Tokens.Num() - 1));
	}

	bool asCParser::TryParseLocalDeclaration(TArray<asCStmt*>& OutDeclarations)
	{
		const int32 Begin = Cursor;
		int32 TypeEnd = Cursor;
		asSTypeSyntax TypeSyntax;
		if (!ParseTypeSyntax(TypeEnd, Tokens.Num(), TypeSyntax) || !Tokens.IsValidIndex(TypeEnd)
			|| Tokens[TypeEnd].Kind != asETokenKind::Identifier
			|| (TypeSyntax.Reference != asETypeReferenceDirection::None && !Sema.IsBodyTypeName(
				(TypeSyntax.bGlobal ? TEXT("::") : TEXT("")) + TypeSyntax.GetQualifiedName()))) return false;
		Cursor = TypeEnd;
		for (;;)
		{
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier)
			{
				OutDeclarations.Add(Sema.ActOnRecoveryStatement(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("local-name-required")));
				return true;
			}
			FString Name = Text(Cursor++);
			asCExpr* Initializer = nullptr;
			if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::Equal)
			{
				++Cursor;
				Initializer = ParseExpression();
				if (!Initializer)
				{
					OutDeclarations.Add(Sema.ActOnRecoveryStatement(Span(Begin, Cursor - 1), TEXT("missing-local-initializer")));
					return true;
				}
			}
			else if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::LeftParen)
			{
				++Cursor; TArray<asSParsedCallArgument> Arguments;
				if (!ParseCallArguments(Arguments))
				{
					OutDeclarations.Add(Sema.ActOnRecoveryStatement(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("malformed-direct-initializer")));
					return true;
				}
				Initializer = Sema.ActOnConstruct(TypeSyntax, MoveTemp(Arguments), Span(Begin, Cursor - 1));
			}
			OutDeclarations.Add(Sema.ActOnLocalVariable(TypeSyntax, MoveTemp(Name), Initializer, Span(Begin, Cursor - 1)));
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Comma) return true;
			++Cursor;
		}
	}

	bool asCParser::ParseTypedDeclaration(TArray<asCAttr*> Attrs)
	{
		const int32 Begin = Cursor;
		asEAccessSpecifier Access = asEAccessSpecifier::Public;
		FString AccessName;
		asCSourceRange AccessKeyword, AccessColon, AccessNameRange;
		asEFunctionModifiers PrefixModifiers = asEFunctionModifiers::None;
		while (Tokens.IsValidIndex(Cursor))
		{
			const asETokenKind Prefix = Tokens[Cursor].Kind;
			if (Prefix == asETokenKind::KwAccess)
			{
				if (!Sema.GetCurrentRecord() || Access != asEAccessSpecifier::Public || !AccessName.IsEmpty()
					|| !Tokens.IsValidIndex(Cursor + 2) || Tokens[Cursor + 1].Kind != asETokenKind::Colon
					|| Tokens[Cursor + 2].Kind != asETokenKind::Identifier) return false;
				AccessKeyword = Tokens[Cursor].Range; AccessColon = Tokens[Cursor + 1].Range;
				AccessNameRange = Tokens[Cursor + 2].Range; AccessName = Text(Cursor + 2); Cursor += 2;
			}
			else if (Prefix == asETokenKind::KwPrivate || Prefix == asETokenKind::KwProtected)
			{
				if (!Sema.GetCurrentRecord() || Access != asEAccessSpecifier::Public || !AccessName.IsEmpty()) return false;
				Access = Prefix == asETokenKind::KwPrivate ? asEAccessSpecifier::Private : asEAccessSpecifier::Protected;
			}
			else if (Prefix == asETokenKind::KwLocal || Prefix == asETokenKind::KwMixin)
			{
				PrefixModifiers = static_cast<asEFunctionModifiers>(static_cast<uint16>(PrefixModifiers)
					| static_cast<uint16>(Prefix == asETokenKind::KwLocal ? asEFunctionModifiers::Local : asEFunctionModifiers::Mixin));
			}
			else break;
			++Cursor;
		}
		if (!Tokens.IsValidIndex(Cursor)) return false;
		asSTypeSyntax Type;
		asEFunctionKind FunctionKind = asEFunctionKind::Ordinary;
		FString Name;
		asCRecordDecl* Record = Sema.GetCurrentRecord();
		const bool bDestructor = Record && Tokens[Cursor].Kind == asETokenKind::Tilde;
		const int32 NameIndex = Cursor + (bDestructor ? 1 : 0);
		if (Record && Tokens.IsValidIndex(NameIndex + 1) && Text(NameIndex) == Record->GetName()
			&& Tokens[NameIndex + 1].Kind == asETokenKind::LeftParen)
		{
			FunctionKind = bDestructor ? asEFunctionKind::Destructor : asEFunctionKind::Constructor;
			Name = (bDestructor ? TEXT("~") : TEXT("")) + Text(NameIndex);
			Type.NameParts.Add(TEXT("void")); Type.ComponentRanges.Add(Tokens[NameIndex].Range); Type.Range = Tokens[NameIndex].Range;
			Cursor = NameIndex + 1;
		}
		else
		{
			if (!ParseTypeSyntax(Cursor, Tokens.Num(), Type) || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier) return false;
			Name = Text(Cursor++);
		}
		if (!Tokens.IsValidIndex(Cursor)) return false;
		if (Tokens[Cursor].Kind != asETokenKind::LeftParen)
		{
			if (PrefixModifiers != asEFunctionModifiers::None) return false;
			if (Tokens[Cursor].Kind == asETokenKind::LeftBrace)
			{
				Sema.DiagnoseRemovedFeature(asERemovedLanguageFeature::VirtualProperty, Tokens[Cursor].Range);
				Recover(Begin, {}, 0);
				return true;
			}
			asCSourceRange InitializerRange;
			if (Tokens[Cursor].Kind == asETokenKind::Equal)
			{
				++Cursor;
				if (!CaptureInitializerRange(Cursor, Tokens.Num(), InitializerRange)) return false;
			}
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon) return false;
			asCVarDecl* Variable = Sema.ActOnVariable(MoveTemp(Name), Type.GetSpelling(), false, Span(Begin, Cursor++), Attrs);
			Variable->SetTypeSyntax(MoveTemp(Type));
			Variable->SetAccess(Access);
			if (!AccessName.IsEmpty()) Variable->SetAuthoredAccessUse(AccessName, AccessKeyword, AccessColon, AccessNameRange);
			if (InitializerRange.IsOrdered()) Variable->SetDeferredInitializerRange(InitializerRange);
			return true;
		}

		const int32 Close = FindMatching(Cursor, asETokenKind::LeftParen, asETokenKind::RightParen);
		if (Close == INDEX_NONE) return false;
		++Cursor;
		struct asSParsedParameter { asSTypeSyntax Type; FString Name; asCSourceRange Range; asCSourceRange Initializer; };
		TArray<asSParsedParameter> Parameters;
		if (Cursor + 1 == Close && Tokens[Cursor].Kind == asETokenKind::KwVoid) ++Cursor;
		bool bSawDefault = false;
		while (Cursor < Close)
		{
			const int32 ParameterBegin = Cursor;
			asSParsedParameter Parameter;
			if (!ParseTypeSyntax(Cursor, Close, Parameter.Type)) return false;
			if (Cursor < Close && Tokens[Cursor].Kind == asETokenKind::Identifier) Parameter.Name = Text(Cursor++);
			Parameter.Range = Span(ParameterBegin, Cursor - 1);
			if (Cursor < Close && Tokens[Cursor].Kind == asETokenKind::Equal)
			{
				++Cursor;
				if (!CaptureInitializerRange(Cursor, Close, Parameter.Initializer)) return false;
				Parameter.Range = Span(ParameterBegin, Cursor - 1);
			}
			if (bSawDefault && !Parameter.Initializer.IsOrdered()) return false;
			bSawDefault = Parameter.Initializer.IsOrdered();
			Parameters.Add(MoveTemp(Parameter));
			if (Cursor == Close) break;
			if (Tokens[Cursor++].Kind != asETokenKind::Comma || Cursor == Close) return false;
		}
		Cursor = Close + 1;
		const bool bConstMethod = Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::KwConst;
		if (bConstMethod) ++Cursor;
		asEFunctionModifiers Modifiers = PrefixModifiers;
		while (Tokens.IsValidIndex(Cursor) && (Tokens[Cursor].Kind == asETokenKind::Identifier || Tokens[Cursor].Kind == asETokenKind::KwMixin))
		{
			const FString Modifier = Text(Cursor);
			if (Modifier == TEXT("property"))
			{
				Sema.DiagnoseRemovedFeature(asERemovedLanguageFeature::VirtualProperty, Tokens[Cursor].Range);
				Recover(Begin, {}, 0);
				return true;
			}
			uint16 Bit = Modifier == TEXT("final") ? static_cast<uint16>(asEFunctionModifiers::Final)
				: Modifier == TEXT("override") ? static_cast<uint16>(asEFunctionModifiers::Override)
				: Modifier == TEXT("mixin") ? static_cast<uint16>(asEFunctionModifiers::Mixin)
				: Modifier == TEXT("accept_temporary_this") ? static_cast<uint16>(asEFunctionModifiers::AcceptTemporary)
				: Modifier == TEXT("external_implicit_this") ? static_cast<uint16>(asEFunctionModifiers::ExternalImplicitThis)
				: Modifier == TEXT("no_discard") ? static_cast<uint16>(asEFunctionModifiers::NoDiscard)
				: Modifier == TEXT("allow_discard") ? static_cast<uint16>(asEFunctionModifiers::AllowDiscard)
				: Modifier == TEXT("__generated") ? static_cast<uint16>(asEFunctionModifiers::Generated)
				: Modifier == TEXT("deprecated") ? static_cast<uint16>(asEFunctionModifiers::Deprecated)
				: Modifier == TEXT("defaults") ? static_cast<uint16>(asEFunctionModifiers::Defaults)
				: Modifier == TEXT("unsafe_during_construction") ? static_cast<uint16>(asEFunctionModifiers::UnsafeDuringConstruction) : 0;
			if (!Bit) break;
			Modifiers = static_cast<asEFunctionModifiers>(static_cast<uint16>(Modifiers) | Bit); ++Cursor;
		}
		asCSourceRange Body;
		if (!Tokens.IsValidIndex(Cursor)) return false;
		int32 End = Cursor;
		if (Tokens[Cursor].Kind == asETokenKind::LeftBrace)
		{
			End = FindMatching(Cursor, asETokenKind::LeftBrace, asETokenKind::RightBrace);
			if (End == INDEX_NONE) return false;
			Body = Span(Cursor, End);
			Cursor = End + 1;
		}
		else if (Tokens[Cursor].Kind == asETokenKind::Semicolon) ++Cursor;
		else return false;
		asCFunctionDecl* Function = Sema.ActOnFunction(MoveTemp(Name), Type.GetSpelling(), Span(Begin, End), Body, Attrs);
		if (Record && FunctionKind == asEFunctionKind::Ordinary
			&& (Function->GetName() == TEXT("opConv") || Function->GetName() == TEXT("opImplConv")
				|| Function->GetName() == TEXT("opCast") || Function->GetName() == TEXT("opImplCast"))) FunctionKind = asEFunctionKind::Conversion;
		Function->SetReturnTypeSyntax(MoveTemp(Type));
		Function->SetConstMethod(bConstMethod);
		Function->SetFunctionKind(FunctionKind);
		Function->SetModifiers(Modifiers);
		Function->SetAccess(Access);
		if (!AccessName.IsEmpty()) Function->SetAuthoredAccessUse(AccessName, AccessKeyword, AccessColon, AccessNameRange);
		Sema.PushContext(Function);
		for (asSParsedParameter& Parameter : Parameters)
		{
			asCVarDecl* Decl = Sema.ActOnVariable(MoveTemp(Parameter.Name), Parameter.Type.GetSpelling(), true, Parameter.Range, {});
			Decl->SetTypeSyntax(MoveTemp(Parameter.Type));
			if (Parameter.Initializer.IsOrdered()) Decl->SetDeferredInitializerRange(Parameter.Initializer);
		}
		Sema.PopContext();
		return true;
	}
	bool asCParser::ParseTypeSyntax(int32& Index, int32 End, asSTypeSyntax& OutType) const
    {
        return asCTypeSyntaxParser(Tokens).ParseTypeSyntax(Index, End, OutType);
    }
    bool asCParser::ParseTypeSyntaxInternal(int32& Index, int32 End, asSTypeSyntax& OutType, int32& PendingClosers, int32 Depth) const
    {
        return asCTypeSyntaxParser(Tokens).ParseTypeSyntaxInternal(Index, End, OutType, PendingClosers, Depth);
    }

	bool asCParser::ParseCallableDeclaration(TArray<asCAttr*> Attrs)
	{
		const int32 Begin = Cursor;
		const bool bMulticast = Tokens[Cursor++].Kind == asETokenKind::KwEvent;
		asSTypeSyntax ReturnType;
		if (!ParseTypeSyntax(Cursor, Tokens.Num(), ReturnType) || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier) return false;
		FString Name = Text(Cursor++);
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftParen) return false;
		const int32 Close = FindMatching(Cursor, asETokenKind::LeftParen, asETokenKind::RightParen);
		if (Close == INDEX_NONE) return false;
		++Cursor;
		struct asSParsedParameter { asSTypeSyntax Type; FString Name; asCSourceRange Range; asCSourceRange Initializer; };
		TArray<asSParsedParameter> Parameters;
		if (Cursor + 1 == Close && Tokens[Cursor].Kind == asETokenKind::KwVoid) ++Cursor;
		bool bSawDefault = false;
		while (Cursor < Close)
		{
			const int32 ParameterBegin = Cursor;
			asSParsedParameter Parameter;
			if (!ParseTypeSyntax(Cursor, Close, Parameter.Type)) return false;
			if (Cursor < Close && Tokens[Cursor].Kind == asETokenKind::Identifier) Parameter.Name = Text(Cursor++);
			Parameter.Range = Span(ParameterBegin, Cursor - 1);
			if (Cursor < Close && Tokens[Cursor].Kind == asETokenKind::Equal)
			{
				++Cursor;
				if (!CaptureInitializerRange(Cursor, Close, Parameter.Initializer)) return false;
				Parameter.Range = Span(ParameterBegin, Cursor - 1);
			}
			if (bSawDefault && !Parameter.Initializer.IsOrdered()) return false;
			bSawDefault = Parameter.Initializer.IsOrdered();
			Parameters.Add(MoveTemp(Parameter));
			if (Cursor == Close) break;
			if (Tokens[Cursor++].Kind != asETokenKind::Comma || Cursor == Close) return false;
		}
		Cursor = Close + 1;
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Semicolon) return false;
		const int32 End = Cursor++;
		asCCallableTypeDecl* Declaration = Sema.ActOnCallableType(bMulticast, MoveTemp(Name), MoveTemp(ReturnType), Span(Begin, End), Attrs);
		Sema.PushContext(Declaration);
		for (asSParsedParameter& Parameter : Parameters)
		{
			asCVarDecl* Decl = Sema.ActOnVariable(MoveTemp(Parameter.Name), Parameter.Type.GetSpelling(), true, Parameter.Range, {});
			Decl->SetTypeSyntax(MoveTemp(Parameter.Type));
			if (Parameter.Initializer.IsOrdered()) Decl->SetDeferredInitializerRange(Parameter.Initializer);
		}
		Sema.PopContext();
		return true;
	}

	bool asCParser::ParseRemovedDeclaration()
	{
		if (Tokens[Cursor].Kind != asETokenKind::Identifier) return false;
		const int32 Begin = Cursor;
		const FString Spelling = Text(Cursor);
		asERemovedLanguageFeature Feature = asERemovedLanguageFeature::ModuleSharing;
		bool bRemovedFeature = false;
		const bool bRecordFollows = Tokens.IsValidIndex(Cursor + 1)
			&& (Tokens[Cursor + 1].Kind == asETokenKind::KwClass || Tokens[Cursor + 1].Kind == asETokenKind::KwStruct
				|| Tokens[Cursor + 1].Kind == asETokenKind::KwInterface || Tokens[Cursor + 1].Kind == asETokenKind::KwEnum);
		// A same-spelling type or function name is still an ordinary identifier.
		// Only a modifier followed by a typed callable declaration is retired.
		int32 TypeEnd = Cursor + 1;
		asSTypeSyntax ReturnType;
		const bool bFunctionFollows = ParseTypeSyntax(TypeEnd, Tokens.Num(), ReturnType)
			&& Tokens.IsValidIndex(TypeEnd + 1) && Tokens[TypeEnd].Kind == asETokenKind::Identifier
			&& Tokens[TypeEnd + 1].Kind == asETokenKind::LeftParen;
		if ((Spelling == TEXT("shared") || Spelling == TEXT("external")) && (bRecordFollows || bFunctionFollows))
			bRemovedFeature = true;
		else if (Spelling == TEXT("funcdef") && bFunctionFollows)
		{ Feature = asERemovedLanguageFeature::FuncdefDeclaration; bRemovedFeature = true; }
		else if (Spelling == TEXT("template") && Tokens.IsValidIndex(Cursor + 1) && Tokens[Cursor + 1].Kind == asETokenKind::Less)
		{ Feature = asERemovedLanguageFeature::UserTemplate; bRemovedFeature = true; }
		else if (Spelling == TEXT("coroutine") && bFunctionFollows)
		{ Feature = asERemovedLanguageFeature::ScriptCoroutine; bRemovedFeature = true; }
		const bool bAsset = Text(Cursor) == TEXT("asset") && Tokens.IsValidIndex(Cursor + 2)
			&& Tokens[Cursor + 1].Kind == asETokenKind::Identifier && Text(Cursor + 2) == TEXT("of");
		bool bImport = false;
		if (Text(Cursor) == TEXT("import"))
		{
			for (int32 I = Cursor + 1; I < Tokens.Num() && Tokens[I].Kind != asETokenKind::Semicolon && Tokens[I].Kind != asETokenKind::LeftBrace; ++I)
			{
				if (Tokens[I].Kind == asETokenKind::Identifier && Text(I) == TEXT("from")
					&& Tokens.IsValidIndex(I + 1) && Tokens[I + 1].Kind == asETokenKind::StringLiteral) { bImport = true; break; }
			}
		}
		if (!bAsset && !bImport && !bRemovedFeature) return false;
		// Consume the complete authored construct, including nested initializer braces.
		// Recovery creates no semantic callable, variable, type or synthetic initializer.
		int32 End = Begin;
		while (Cursor < Tokens.Num() && Tokens[Cursor].Kind != asETokenKind::RightBrace)
		{
			End = Cursor;
			if (Tokens[Cursor].Kind == asETokenKind::LeftBrace)
			{
				const int32 Close = FindMatching(Cursor, asETokenKind::LeftBrace, asETokenKind::RightBrace);
				if (Close == INDEX_NONE) { Cursor = Tokens.Num(); End = Tokens.Num() - 1; break; }
				Cursor = Close + 1; End = Close;
				if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::Semicolon) End = Cursor++;
				break;
			}
			if (Tokens[Cursor++].Kind == asETokenKind::Semicolon) break;
		}
		if (bRemovedFeature)
		{
			Sema.DiagnoseRemovedFeature(Feature, Tokens[Begin].Range);
			Sema.ActOnRecovery(Span(Begin, End), {}, 0);
		}
		else
			Sema.ActOnRecovery(Span(Begin, End), bAsset ? TEXT("asset-syntax-removed") : TEXT("import-syntax-removed"),
				static_cast<asUINT>(bAsset ? asEDeclarationDiagnostic::RemovedAsset : asEDeclarationDiagnostic::RemovedImport));
		return true;
	}

	void asCParser::Recover(int32 Begin, FString Message, asUINT DiagnosticID)
	{
		int32 End = FMath::Clamp(Cursor, 0, Tokens.Num() - 1);
		while (Cursor < Tokens.Num() && Tokens[Cursor].Kind != asETokenKind::Semicolon && Tokens[Cursor].Kind != asETokenKind::RightBrace)
		{
			if (Tokens[Cursor].Kind == asETokenKind::LeftBrace)
			{
				const int32 Close = FindMatching(Cursor, asETokenKind::LeftBrace, asETokenKind::RightBrace);
				if (Close != INDEX_NONE) { End = Close; Cursor = Close + 1; break; }
			}
			End = Cursor++;
		}
		if (Cursor < Tokens.Num() && Tokens[Cursor].Kind == asETokenKind::Semicolon) End = Cursor++;
		Sema.ActOnRecovery(Span(FMath::Clamp(Begin, 0, Tokens.Num() - 1), End), MoveTemp(Message), DiagnosticID);
		if (Cursor == Begin && Cursor < Tokens.Num()) ++Cursor;
	}

	namespace
	{
		int32 BinaryPrecedence(asETokenKind Kind, asEBinaryOperator& OutOperator)
		{
			// Keep AngelScript's language precedence (bitwise before comparison),
			// using Clang's precedence-climbing algorithm, not C++ precedence.
			switch (Kind)
			{
#define AS_BINARY(Token, Operator, Level) case asETokenKind::Token: OutOperator = asEBinaryOperator::Operator; return Level;
			AS_BINARY(Equal, Assign, 1)
			AS_BINARY(PlusEqual, AddAssign, 1) AS_BINARY(MinusEqual, SubtractAssign, 1)
			AS_BINARY(StarEqual, MultiplyAssign, 1) AS_BINARY(SlashEqual, DivideAssign, 1)
			AS_BINARY(PercentEqual, ModuloAssign, 1) AS_BINARY(PowerEqual, PowerAssign, 1)
			AS_BINARY(ShiftLeftEqual, ShiftLeftAssign, 1) AS_BINARY(ShiftRightEqual, ShiftRightAssign, 1)
			AS_BINARY(ShiftRightArithmeticEqual, ShiftRightArithmeticAssign, 1)
			AS_BINARY(AmpEqual, BitwiseAndAssign, 1) AS_BINARY(CaretEqual, BitwiseXorAssign, 1) AS_BINARY(PipeEqual, BitwiseOrAssign, 1)
			AS_BINARY(PipePipe, LogicalOr, 3) AS_BINARY(AmpAmp, LogicalAnd, 4)
			AS_BINARY(CaretCaret, LogicalXor, 5) AS_BINARY(EqualEqual, Equal, 5) AS_BINARY(BangEqual, NotEqual, 5)
			AS_BINARY(Less, Less, 6) AS_BINARY(Greater, Greater, 6)
			AS_BINARY(LessEqual, LessEqual, 6) AS_BINARY(GreaterEqual, GreaterEqual, 6)
			AS_BINARY(Pipe, BitwiseOr, 7) AS_BINARY(Caret, BitwiseXor, 8) AS_BINARY(Amp, BitwiseAnd, 9)
			AS_BINARY(ShiftLeft, ShiftLeft, 10) AS_BINARY(ShiftRight, ShiftRight, 10) AS_BINARY(ShiftRightArithmetic, ShiftRightArithmetic, 10)
			AS_BINARY(Plus, Add, 11) AS_BINARY(Minus, Subtract, 11)
			AS_BINARY(Star, Multiply, 12) AS_BINARY(Slash, Divide, 12) AS_BINARY(Percent, Modulo, 12)
			AS_BINARY(Power, Power, 13)
#undef AS_BINARY
			default: return 0;
			}
		}
	}

	asCExpr* asCParser::ParseExpression(int32 MinimumPrecedence)
	{
		if (!Tokens.IsValidIndex(Cursor)) return nullptr;
		if (++ExpressionDepth > 256)
		{
			--ExpressionDepth;
			return Sema.ActOnRecoveryExpression(Tokens[Cursor++].Range, TEXT("expression-depth-limit"));
		}
		ON_SCOPE_EXIT { --ExpressionDepth; };
		asCExpr* Left = ParseUnaryExpression();
		if (!Left) return nullptr;
		while (Tokens.IsValidIndex(Cursor))
		{
			if (Tokens[Cursor].Kind == asETokenKind::Question && MinimumPrecedence <= 2)
			{
				++Cursor;
				asCExpr* TrueExpr = ParseExpression();
				if (!TrueExpr || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Colon)
					return Sema.ActOnRecoveryExpression(Left->GetSourceRange(), TEXT("conditional-missing-colon"));
				++Cursor;
				asCExpr* FalseExpr = ParseExpression(2);
				if (!FalseExpr) return Sema.ActOnRecoveryExpression(Left->GetSourceRange(), TEXT("conditional-missing-false-expression"));
				const asCSourceRange Range{Left->GetSourceRange().Begin, FalseExpr->GetSourceRange().End};
				Left = Sema.ActOnConditional(Left, TrueExpr, FalseExpr, Range);
				continue;
			}
			asEBinaryOperator Operator = asEBinaryOperator::Add;
			const int32 Precedence = BinaryPrecedence(Tokens[Cursor].Kind, Operator);
			if (Precedence == 0 || Precedence < MinimumPrecedence) break;
			const asCSourceRange OperatorRange = Tokens[Cursor++].Range;
			// Assignment is right associative. Maintained AS power, like its
			// other arithmetic operators, is left associative.
			asCExpr* Right = ParseExpression(Precedence + (Precedence == 1 ? 0 : 1));
			if (!Right) return Sema.ActOnRecoveryExpression(OperatorRange, TEXT("missing-binary-operand"));
			const asCSourceRange Range{Left->GetSourceRange().Begin, Right->GetSourceRange().End};
			Left = Sema.ActOnBinary(Operator, Left, Right, OperatorRange, Range);
		}
		return Left;
	}

	asCExpr* asCParser::ParseUnaryExpression()
	{
		if (!Tokens.IsValidIndex(Cursor)) return nullptr;
		if (++ExpressionDepth > 256)
		{
			--ExpressionDepth;
			return Sema.ActOnRecoveryExpression(Tokens[Cursor++].Range, TEXT("expression-depth-limit"));
		}
		ON_SCOPE_EXIT { --ExpressionDepth; };
		asEUnaryOperator Operator = asEUnaryOperator::Plus;
		bool bPrefix = true;
		switch (Tokens[Cursor].Kind)
		{
		case asETokenKind::Plus: Operator = asEUnaryOperator::Plus; break;
		case asETokenKind::Minus: Operator = asEUnaryOperator::Minus; break;
		case asETokenKind::Bang: Operator = asEUnaryOperator::LogicalNot; break;
		case asETokenKind::Tilde: Operator = asEUnaryOperator::BitwiseNot; break;
		case asETokenKind::PlusPlus: Operator = asEUnaryOperator::PreIncrement; break;
		case asETokenKind::MinusMinus: Operator = asEUnaryOperator::PreDecrement; break;
		default: bPrefix = false; break;
		}
		if (bPrefix)
		{
			const asCSourceRange OperatorRange = Tokens[Cursor++].Range;
			asCExpr* Operand = ParseUnaryExpression();
			if (!Operand) return Sema.ActOnRecoveryExpression(OperatorRange, TEXT("missing-unary-operand"));
			return Sema.ActOnUnary(Operator, Operand, OperatorRange, {OperatorRange.Begin, Operand->GetSourceRange().End});
		}
		asCExpr* Result = ParsePrimaryExpression();
		while (Result && Tokens.IsValidIndex(Cursor))
		{
			if (Tokens[Cursor].Kind == asETokenKind::Dot)
			{
				++Cursor;
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier)
					return Sema.ActOnRecoveryExpression(Result->GetSourceRange(), TEXT("member-name-required"));
				const asCSourceRange NameRange = Tokens[Cursor].Range;
				FString Name = Text(Cursor++);
				if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::LeftParen)
				{
					++Cursor;
					TArray<asSParsedCallArgument> Arguments;
					if (!ParseCallArguments(Arguments)) return Sema.ActOnRecoveryExpression(NameRange, TEXT("malformed-member-call"));
					Result = Sema.ActOnMemberCall(Result, MoveTemp(Name), MoveTemp(Arguments), NameRange, {Result->GetSourceRange().Begin, Tokens[Cursor - 1].Range.End});
				}
				else Result = Sema.ActOnMember(Result, MoveTemp(Name), NameRange, {Result->GetSourceRange().Begin, NameRange.End});
				continue;
			}
			if (Tokens[Cursor].Kind == asETokenKind::LeftParen || Tokens[Cursor].Kind == asETokenKind::LeftBracket)
			{
				const bool bIndex = Tokens[Cursor++].Kind == asETokenKind::LeftBracket;
				TArray<asSParsedCallArgument> Arguments;
				if (!ParseCallArguments(Arguments, bIndex ? asETokenKind::RightBracket : asETokenKind::RightParen))
					return Sema.ActOnRecoveryExpression(Result->GetSourceRange(), TEXT("malformed-postfix-arguments"));
				const asCSourceRange Range{Result->GetSourceRange().Begin, Tokens[Cursor - 1].Range.End};
				if (bIndex)
				{
					TArray<asCExpr*> Indices;
					for (const asSParsedCallArgument& Argument : Arguments)
					{
						if (!Argument.Name.IsEmpty()) return Sema.ActOnRecoveryExpression(Range, TEXT("named-subscript-not-supported"));
						Indices.Add(Argument.Value);
					}
					Result = Sema.ActOnSubscript(Result, MoveTemp(Indices), Range);
				}
				else Result = Sema.ActOnIndirectCall(Result, MoveTemp(Arguments), Range);
				continue;
			}
			if (Tokens[Cursor].Kind == asETokenKind::PlusPlus || Tokens[Cursor].Kind == asETokenKind::MinusMinus)
			{
				const asCSourceRange OperatorRange = Tokens[Cursor].Range;
				Operator = Tokens[Cursor++].Kind == asETokenKind::PlusPlus ? asEUnaryOperator::PostIncrement : asEUnaryOperator::PostDecrement;
				Result = Sema.ActOnUnary(Operator, Result, OperatorRange, {Result->GetSourceRange().Begin, OperatorRange.End});
				continue;
			}
			break;
		}
		return Result;
	}

	bool asCParser::ParseCallArguments(TArray<asSParsedCallArgument>& Arguments, asETokenKind Closing)
	{
		while (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != Closing)
		{
			asSParsedCallArgument Argument;
			if (Tokens.IsValidIndex(Cursor + 1) && Tokens[Cursor].Kind == asETokenKind::Identifier && Tokens[Cursor + 1].Kind == asETokenKind::Colon)
			{
				Argument.Name = Text(Cursor);
				Argument.NameRange = Tokens[Cursor].Range;
				Cursor += 2;
			}
			Argument.Value = ParseExpression();
			if (!Argument.Value) return false;
			Arguments.Add(MoveTemp(Argument));
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Comma) break;
			++Cursor;
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind == Closing)
			{
				Sema.ActOnRecoveryExpression(Tokens[FMath::Min(Cursor, Tokens.Num() - 1)].Range, TEXT("call-trailing-comma"));
				return false;
			}
		}
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != Closing) return false;
		++Cursor;
		return true;
	}

	asCExpr* asCParser::ParsePrimaryExpression()
	{
		if (!Tokens.IsValidIndex(Cursor)) return nullptr;
		const int32 Begin = Cursor;
		if (Tokens[Cursor].Kind == asETokenKind::Identifier && Text(Cursor) == TEXT("this"))
			return Sema.ActOnThis(false, Tokens[Cursor++].Range);
		if ((Tokens[Cursor].Kind == asETokenKind::Identifier && Text(Cursor) == TEXT("function")
			&& Tokens.IsValidIndex(Cursor + 1)
			&& (Tokens[Cursor + 1].Kind == asETokenKind::LeftParen || Tokens[Cursor + 1].Kind == asETokenKind::LeftBracket))
			|| Tokens[Cursor].Kind == asETokenKind::LeftBracket)
		{
			Sema.DiagnoseRemovedFeature(asERemovedLanguageFeature::AnonymousFunction, Tokens[Begin].Range);
			if (Tokens[Cursor].Kind == asETokenKind::Identifier) ++Cursor;
			for (const auto Pair : {TPair<asETokenKind, asETokenKind>(asETokenKind::LeftBracket, asETokenKind::RightBracket),
				TPair<asETokenKind, asETokenKind>(asETokenKind::LeftParen, asETokenKind::RightParen),
				TPair<asETokenKind, asETokenKind>(asETokenKind::LeftBrace, asETokenKind::RightBrace)})
			{
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != Pair.Key) continue;
				const int32 Close = FindMatching(Cursor, Pair.Key, Pair.Value);
				if (Close == INDEX_NONE) { ++Cursor; break; }
				Cursor = Close + 1;
			}
			return Sema.GetASTContext().Create<asCRecoveryExpr>(Span(Begin, FMath::Max(Begin, Cursor - 1)));
		}
		if (Tokens[Cursor].Kind == asETokenKind::LeftBrace)
		{
			++Cursor; TArray<asCExpr*> Elements;
			while (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind != asETokenKind::RightBrace)
			{
				if (Tokens[Cursor].Kind == asETokenKind::Comma) Elements.Add(Sema.ActOnValueInit(Tokens[Cursor].Range));
				else
				{
					asCExpr* Element = ParseExpression();
					if (!Element) return Sema.ActOnRecoveryExpression(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("malformed-list-initializer"));
					Elements.Add(Element);
				}
				if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::RightBrace) break;
				if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor++].Kind != asETokenKind::Comma)
					return Sema.ActOnRecoveryExpression(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("list-initializer-missing-comma"));
				if (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::RightBrace)
					Elements.Add(Sema.ActOnValueInit(Tokens[Cursor].Range));
			}
			if (!Tokens.IsValidIndex(Cursor)) return Sema.ActOnRecoveryExpression(Span(Begin, Tokens.Num() - 1), TEXT("list-initializer-unclosed"));
			return Sema.ActOnInitList(MoveTemp(Elements), Span(Begin, Cursor++));
		}
		if (Tokens[Cursor].Kind == asETokenKind::NumericLiteral)
		{
			const FString Spelling = Text(Cursor);
			return Sema.ActOnNumericLiteral(Spelling, Tokens[Cursor++].Range);
		}
		if (Tokens[Cursor].Kind == asETokenKind::StringLiteral)
		{
			const FString Spelling = Text(Cursor);
			return Sema.ActOnStringLiteral(Spelling, Tokens[Cursor++].Range);
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwNullptr) return Sema.ActOnNullLiteral(Tokens[Cursor++].Range);
		if (Tokens[Cursor].Kind == asETokenKind::KwTrue || Tokens[Cursor].Kind == asETokenKind::KwFalse)
		{
			const bool Value = Tokens[Cursor].Kind == asETokenKind::KwTrue;
			return Sema.ActOnBooleanLiteral(Value, Tokens[Cursor++].Range);
		}
		if (Tokens[Cursor].Kind == asETokenKind::KwCast)
		{
			++Cursor;
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor++].Kind != asETokenKind::Less)
				return Sema.ActOnRecoveryExpression(Span(Begin, Cursor - 1), TEXT("cast-target-required"));
			asSTypeSyntax Target;
			int32 PendingClosers = 0;
			if (!ParseTypeSyntaxInternal(Cursor, Tokens.Num(), Target, PendingClosers, 0))
				return Sema.ActOnRecoveryExpression(Span(Begin, Cursor - 1), TEXT("malformed-cast-target"));
			if (PendingClosers == 1) PendingClosers = 0;
			else if (PendingClosers != 0 || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor++].Kind != asETokenKind::Greater)
				return Sema.ActOnRecoveryExpression(Span(Begin, Cursor - 1), TEXT("cast-missing-angle-close"));
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor++].Kind != asETokenKind::LeftParen)
				return Sema.ActOnRecoveryExpression(Span(Begin, Cursor - 1), TEXT("cast-missing-operand"));
			asCExpr* Operand = ParseExpression();
			if (!Operand || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::RightParen)
				return Sema.ActOnRecoveryExpression(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("cast-missing-parenthesis"));
			return Sema.ActOnExplicitCast(MoveTemp(Target), Operand, Span(Begin, Cursor++));
		}
		if (Tokens[Cursor].Kind == asETokenKind::LeftParen)
		{
			++Cursor;
			asCExpr* Expression = ParseExpression();
			if (!Expression || !Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::RightParen)
				return Sema.ActOnRecoveryExpression(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("parenthesized-expression-missing-close"));
			return Sema.ActOnParen(Expression, Span(Begin, Cursor++));
		}
		int32 TypeEnd = Cursor;
		asSTypeSyntax ConstructionType;
		if (ParseTypeSyntax(TypeEnd, Tokens.Num(), ConstructionType) && Tokens.IsValidIndex(TypeEnd)
			&& Tokens[TypeEnd].Kind == asETokenKind::LeftParen && Sema.ResolveBodyType(ConstructionType).IsValid())
		{
			Cursor = TypeEnd + 1;
			TArray<asSParsedCallArgument> Arguments;
			if (!ParseCallArguments(Arguments)) return Sema.ActOnRecoveryExpression(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("malformed-construction-arguments"));
			return Sema.ActOnConstruct(MoveTemp(ConstructionType), MoveTemp(Arguments), Span(Begin, Cursor - 1));
		}
		if (Tokens[Cursor].Kind != asETokenKind::Identifier && Tokens[Cursor].Kind != asETokenKind::Scope) return nullptr;
		FString Name;
		if (Tokens[Cursor].Kind == asETokenKind::Scope)
		{
			Name = TEXT("::");
			++Cursor;
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier) return nullptr;
		}
		Name += Text(Cursor++);
		while (Tokens.IsValidIndex(Cursor) && Tokens[Cursor].Kind == asETokenKind::Scope)
		{
			++Cursor;
			if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::Identifier)
				return Sema.ActOnRecoveryExpression(Span(Begin, Cursor - 1), TEXT("qualified-name-missing-component"));
			Name += TEXT("::") + Text(Cursor++);
		}
		if (!Tokens.IsValidIndex(Cursor) || Tokens[Cursor].Kind != asETokenKind::LeftParen)
			return Sema.ActOnDeclReference(MoveTemp(Name), Span(Begin, Cursor - 1));
		++Cursor;
		TArray<asSParsedCallArgument> Arguments;
		if (!ParseCallArguments(Arguments))
			return Sema.ActOnRecoveryExpression(Span(Begin, FMath::Min(Cursor, Tokens.Num() - 1)), TEXT("malformed-call-argument"));
		return Sema.ActOnCallWithArguments(MoveTemp(Name), MoveTemp(Arguments), Span(Begin, Cursor - 1));
	}

END_AS_NAMESPACE
