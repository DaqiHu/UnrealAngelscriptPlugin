/* Maintained Unreal AngelScript fork — declaration parser driving typed Sema actions. */
#pragma once

#include "frontend/Lexer/as_preprocessor.h"
#include "frontend/Sema/as_sema.h"

BEGIN_AS_NAMESPACE

enum class asEDeclarationDiagnostic : asUINT
	{
		RemovedAsset = 3004,
		RemovedImport = 3005,
		InvalidCallable = 3006,
		InvalidAnnotation = 3007,
	};

	class ANGELSCRIPTRUNTIME_API asCParser
	{
	public:
		explicit asCParser(asCSema& InSema) : Sema(InSema) {}
		asCDiagnostic Diag(const asCSourceRange& Range, asUINT ID);
		bool Collect(const asSDirectiveProcessResult& Input, asCDeclarationFragment& Fragment);
		bool ParseBody(const asSDirectiveProcessResult& Input, asCBodyFragment& Fragment);
		bool ParseInitializer(const asSDirectiveProcessResult& Input, asCVarDecl& Declaration);
		asCExpr* ParseConstantExpression(const asSDirectiveProcessResult& Input, asCSourceRange Range);
		asCStmt* ParseClassDefaultStatement(const asSDirectiveProcessResult& Input);

	private:
		bool ParseDeclaration();
		bool ParseAccessDeclaration();
		bool ParseNamespace(TArray<asCAttr*> Attrs);
		bool ParseRecord(TArray<asCAttr*> Attrs, asETypeDeclKind DeclaredTypeKind);
		bool ParseEnumBody(asCRecordDecl& EnumDecl, int32 CloseBrace);
		bool ParseTypedDeclaration(TArray<asCAttr*> Attrs);
		bool ParseCallableDeclaration(TArray<asCAttr*> Attrs);
		bool ParseTypeSyntax(int32& Index, int32 End, asSTypeSyntax& OutType) const;
		bool ParseTypeSyntaxInternal(int32& Index, int32 End, asSTypeSyntax& OutType, int32& PendingClosers, int32 Depth) const;
		bool ParseRemovedDeclaration();
		TArray<asCAttr*> ParseAnnotations();
		bool ParseAnnotationPayload(int32 Begin, int32 End, TArray<asSAnnotationArgument>& OutArguments);
		FString Text(int32 Index) const;
		FString Join(int32 Begin, int32 End) const;
		int32 FindMatching(int32 Open, asETokenKind Left, asETokenKind Right) const;
		asCSourceRange Span(int32 Begin, int32 EndInclusive) const;
		void Recover(int32 Begin, FString Message, asUINT DiagnosticID = 3001);
		asCCompoundStmt* ParseCompoundStatement();
		asCStmt* ParseBodyStatement();
		asCStmt* ParseForStatement();
		asCStmt* ParseSwitchStatement();
		asCStmt* ParseForeachStatement();
		bool ParseForeachVariable(asSTypeSyntax& OutType, FString& OutName, asCSourceRange& OutRange);
		asCExpr* ParseExpression(int32 MinimumPrecedence = 1);
		asCExpr* ParseUnaryExpression();
		asCExpr* ParsePrimaryExpression();
		bool ParseCallArguments(TArray<asSParsedCallArgument>& Arguments, asETokenKind Closing = asETokenKind::RightParen);
		bool CaptureInitializerRange(int32& Index, int32 End, asCSourceRange& Range) const;
		bool TryParseLocalDeclaration(TArray<asCStmt*>& OutDeclarations);
		bool CaptureStatementRange(int32& Index, asCSourceRange& Range, int32 Depth = 0) const;
		asCStmt* ParseScopedBodyStatement();
		asCExpr* ParseParenthesizedCondition();
		int32 ExpressionDepth = 0;
		asCStmt* RecoverBody(int32 Begin, FString Message);

		asCSema& Sema;
		TArray<asCToken> Tokens;
		asCSourceManager Sources;
		int32 Cursor = 0;
	};

END_AS_NAMESPACE
