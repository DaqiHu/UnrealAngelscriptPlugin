/* Maintained Unreal AngelScript fork — typed declaration semantic actions. */
#pragma once

#include "frontend/as_ast_context.h"
#include "frontend/as_attr.h"
#include "frontend/as_body_fragment.h"
#include "frontend/as_declaration_fragment.h"
#include "frontend/as_diagnostics.h"
#include "frontend/as_preprocessor.h"
#include "frontend/as_list_initializer.h"
#include "frontend/as_external_semantics.h"

#include "Templates/Function.h"

BEGIN_AS_NAMESPACE

using asFBodyNameLookup = TFunction<TConstArrayView<asCDecl*>(const FString&)>;
	using asFBodyTypeLookup = TFunction<asCType*(const FString&)>;
	using asFStructuredTypeLookup = TFunction<asCQualType(const asSTypeSyntax&)>;
	using asFNominalDeclLookup = TFunction<asCNamedDecl*(const asSStableKey&)>;
	struct asSDirectiveProcessResult;
	using asFBodyInputLookup = TFunction<asSDirectiveProcessResult(const asCSourceRange&)>;
	using asFExternalCallableLookup = TFunction<TConstArrayView<asSExternalCallable>(const FString&)>;

	struct asSParsedCallArgument
	{
		asCExpr* Value = nullptr;
		FString Name;
		asCSourceRange NameRange;
	};
	struct asSParsedLambdaParameter
	{
		asSTypeSyntax Type;
		FString Name;
		asCSourceRange Range;
	};

	struct asSParsedForeachVariable
	{
		asSTypeSyntax Type;
		FString Name;
		asCSourceRange Range;
		bool IsValid() const { return Type.IsValid() && !Name.IsEmpty() && Range.IsOrdered(); }
	};

	class ANGELSCRIPTRUNTIME_API asCSema
	{
	public:
		asCSema(asCASTContext& InContext, asCDiagnosticsEngine& InDiagnostics, FString InStableScope);
		void BeginFragment(asCDeclarationFragment& Fragment);
		void EndFragment();
		asCNamespaceDecl* ActOnNamespace(FString Name, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs);
		asCAccessSpecifierDecl* ActOnAccessSpecifier(FString Name, asEAccessSpecifier BaseAccess,
			TArray<asSAuthoredAccessPermission> Permissions, asSAccessSpecifierSourceInfo Source, asCSourceRange Range);
		asCTypeAliasDecl* ActOnTypeAlias(FString Name, asSTypeSyntax Type, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs);
		asCRecordDecl* ActOnRecord(FString Name, asCSourceRange Range, asETypeDeclKind DeclaredTypeKind, TConstArrayView<FString> Bases, TConstArrayView<asCAttr*> Attrs);
		asCEnumConstantDecl* ActOnEnumConstant(FString Name, int64 Value, bool bExplicitValue, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs);
		asCFunctionDecl* ActOnFunction(FString Name, FString ReturnType, asCSourceRange Range, asCSourceRange DeferredBody, TConstArrayView<asCAttr*> Attrs);
		asCCallableTypeDecl* ActOnCallableType(bool bMulticast, FString Name, asSTypeSyntax ReturnType, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs);
		asCVarDecl* ActOnVariable(FString Name, FString Type, bool bParameter, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs);
		asCRecoveryDecl* ActOnRecovery(asCSourceRange Range, FString Message, asUINT DiagnosticID = 3001);
		asCAnnotationAttr* CreateAnnotation(asEAnnotationKind Kind, TArray<asSAnnotationArgument> Arguments, asCSourceRange Range);
		asCFlagAttr* CreateAnnotation(FString Name, asCSourceRange Range);
		asCStringAttr* CreateStringAnnotation(FString Name, FString Value, asCSourceRange Range);
		void BeginBody(
			asCBodyFragment& Fragment,
			asFBodyNameLookup InLookup,
			asFBodyTypeLookup InTypeLookup,
			asFStructuredTypeLookup InStructuredLookup = {},
			asFNominalDeclLookup InNominalLookup = {}, asFBodyInputLookup InBodyInputLookup = {}, asFListInitializationLookup InListLookup = {},
			asFExternalCallableLookup InExternalLookup = {});
		void EndBody();
		void BeginInitializer(asCVarDecl& Declaration, asFBodyNameLookup InLookup, asFBodyTypeLookup InTypeLookup,
			asFStructuredTypeLookup InStructuredLookup = {}, asFNominalDeclLookup InNominalLookup = {}, asFBodyInputLookup InBodyInputLookup = {}, asFListInitializationLookup InListLookup = {});
		void BeginConstantInitializer(asCEnumConstantDecl& Declaration, asFBodyNameLookup InLookup, asFBodyTypeLookup InTypeLookup,
			asFStructuredTypeLookup InStructuredLookup = {}, asFNominalDeclLookup InNominalLookup = {});
		bool EndInitializer();
		bool ActOnInitializer(asCVarDecl& Declaration, asCExpr* Value);
		asCQualType ResolveAutoVariableType(const asSTypeSyntax& Syntax, asCExpr* Initializer);
		asCQualType ResolveBodyType(const asSTypeSyntax& Syntax) const;
		asCRecordDecl* GetCurrentRecord() const;
		asCFunctionDecl* GetCurrentFunction() const;
		asCExpr* ActOnThis(bool bImplicit, asCSourceRange Range);
		asCExpr* ActOnLambda(TArray<asSParsedLambdaParameter> Parameters, asCSourceRange BodyRange, asCSourceRange Range);
		bool IsInferringLambdaReturn() const { return bInferLambdaReturn; }
		asCReturnStmt* ActOnInferredLambdaReturn(asCExpr* Value, asCSourceRange Range);
		asCInitListExpr* ActOnInitList(TArray<asCExpr*> Elements, asCSourceRange Range);
		asCValueInitExpr* ActOnValueInit(asCSourceRange Range);
		asCExpr* ActOnStringLiteral(FString Spelling, asCSourceRange Range);
		asCNullPtrLiteral* ActOnNullLiteral(asCSourceRange Range);
		bool FinishExpressionStatement(asCExpr* Expression);
		asCExpr* ActOnMember(asCExpr* Base, FString Name, asCSourceRange NameRange, asCSourceRange Range);
		asCExpr* ActOnMemberCall(asCExpr* Base, FString Name, TArray<asSParsedCallArgument> Arguments, asCSourceRange NameRange, asCSourceRange Range);
		asCExpr* ActOnSubscript(asCExpr* Base, TArray<asCExpr*> Arguments, asCSourceRange Range);
		asCExpr* ActOnExplicitCast(asSTypeSyntax TypeSyntax, asCExpr* Operand, asCSourceRange Range);
		asCExpr* ActOnConstruct(asSTypeSyntax TypeSyntax, TArray<asSParsedCallArgument> Arguments, asCSourceRange Range);
		asCExpr* ActOnCallWithArguments(FString Name, TArray<asSParsedCallArgument> Arguments, asCSourceRange Range);
		asCExpr* ActOnIndirectCall(asCExpr* Callee, TArray<asSParsedCallArgument> Arguments, asCSourceRange Range);
		asCIntegerLiteral* ActOnIntegerLiteral(int64 Value, asCSourceRange Range);
		asCExpr* ActOnNumericLiteral(FString Spelling, asCSourceRange Range);
		asCBooleanLiteral* ActOnBooleanLiteral(bool Value, asCSourceRange Range);
		asCExpr* ActOnParen(asCExpr* Operand, asCSourceRange Range);
		asCExpr* ActOnUnary(asEUnaryOperator Operator, asCExpr* Operand, asCSourceRange OperatorRange, asCSourceRange Range);
		asCExpr* ActOnBinary(asEBinaryOperator Operator, asCExpr* Left, asCExpr* Right, asCSourceRange OperatorRange, asCSourceRange Range);
		asCExpr* ActOnConditional(asCExpr* Condition, asCExpr* TrueExpr, asCExpr* FalseExpr, asCSourceRange Range);
		bool CheckCondition(asCExpr*& Condition);
		bool IsBodyTypeName(const FString& Name) const { return BodyTypeLookup && BodyTypeLookup(Name) != nullptr; }
		asCIfStmt* ActOnIf(asCExpr* Condition, asCStmt* Then, asCStmt* Else, asCSourceRange Range);
		asCForStmt* ActOnForStart(TArray<asCStmt*> Initializers, asCExpr* Condition,
			TArray<asCExpr*> Increments, asCSourceRange HeaderRange);
		asCSwitchStmt* ActOnSwitchStart(asCExpr* Selector, asCSourceRange HeaderRange);
		asCCaseStmt* ActOnCase(asCSwitchStmt& Switch, asCExpr* Label, asCSourceRange KeywordRange,
			asCSourceRange ColonRange);
		asCDefaultStmt* ActOnDefault(asCSwitchStmt& Switch, asCSourceRange KeywordRange,
			asCSourceRange ColonRange);
		asCFallthroughStmt* ActOnFallthrough(asCSourceRange Range);
		bool FinishSwitch(asCSwitchStmt& Switch);
		asCForeachStmt* ActOnForeachStart(asSParsedForeachVariable Value,
			asSParsedForeachVariable Key, asCExpr* RangeExpression,
			asCSourceRange HeaderRange, asCSourceRange ColonRange);
		asCDoStmt* ActOnDoStart(asCSourceRange Range);
		asCNullStmt* ActOnNullStatement(asCSourceRange Range);
		asCExpr* ActOnDeclReference(FString Name, asCSourceRange Range);
		asCExpr* ActOnCall(FString Name, TArray<asCExpr*> Arguments, asCSourceRange Range);
		asCReturnStmt* ActOnReturn(asCExpr* Value, asCSourceRange Range);
		asCCompoundStmt* ActOnCompound(TArray<asCStmt*> Statements, asCSourceRange Range);
		asCDeclStmt* ActOnLocalVariable(FString TypeName, FString Name, asCExpr* Initializer, asCSourceRange Range);
		asCDeclStmt* ActOnLocalVariable(asSTypeSyntax TypeSyntax, FString Name, asCExpr* Initializer, asCSourceRange Range);
		asCWhileStmt* ActOnWhileStart(asCExpr* Condition, asCSourceRange Range);
		asCWhileStmt* ActOnWhileFinish(asCWhileStmt* While, asCStmt* Body);
		asCStmt* ActOnBreak(asCSourceRange Range);
		asCStmt* ActOnContinue(asCSourceRange Range);
		void PushBodyScope();
		void PopBodyScope();
		void PushControlTarget(asCStmt* Target, bool bContinuable = true);
		void PopControlTarget();
		asCRecoveryExpr* ActOnRecoveryExpression(asCSourceRange Range, FString Message);
		asCRecoveryStmt* ActOnRecoveryStatement(asCSourceRange Range, FString Message);
		void PushContext(asCDeclContext* InContext) { ContextStack.Add(InContext); }
		void PopContext() { if (!ContextStack.IsEmpty()) ContextStack.Pop(); }
		asCASTContext& GetASTContext() const { return Context; }

	private:
		bool Attach(asCDecl* Decl, TConstArrayView<asCAttr*> Attrs);
		void BeginInitializerContext(asCNamedDecl& Declaration, asFBodyNameLookup InLookup, asFBodyTypeLookup InTypeLookup,
			asFStructuredTypeLookup InStructuredLookup, asFNominalDeclLookup InNominalLookup, asFBodyInputLookup InBodyInputLookup = {}, asFListInitializationLookup InListLookup = {});
		bool AnalyzeLambda(asCLambdaExpr& Lambda, asCFunctionType* Expected, TConstArrayView<asSParsedCallArgument> Arguments, bool bMayCapture);
		bool FinalizeLambdaIdentity(asCFunctionDecl& Function);
		asCExpr* ConvertLambda(asCLambdaExpr& Lambda, asCQualType Target);
		asCExpr* ConvertInitList(asCInitListExpr& List, asCQualType Target);
		void ReportBodyError(const asCSourceRange& Range, FString Message);
		void ReportBodyWarning(const asCSourceRange& Range, FString Message);
		asCExpr* ConvertTo(asCExpr* Value, asCType* Type);
		asCExpr* ConvertTo(asCExpr* Value, asCQualType Type);
		asCRecordDecl* ResolveRecord(const asCType* Type) const;
		struct asSMemberAccess { bool Read = false; bool Write = false; bool Edit = false; };
		asSMemberAccess GetMemberAccess(const asCDecl* Member) const;
		bool IsEditDefaultsContext() const;
		bool IsConstructionContext() const;
		bool CheckCallContext(const asCFunctionDecl& Function, const asCSourceRange& Range);
		bool CanAccessMember(const asCDecl* Member) const;
		asCFunctionDecl* SelectUserConversion(asCExpr* Value, asCQualType Target, bool bExplicit, bool& bAmbiguous) const;
		asCExpr* ConvertUserDefined(asCExpr* Value, asCQualType Target, bool bExplicit);
		asCExpr* TryActOnMixinCall(asCExpr* Base, const FString& Name, TConstArrayView<asSParsedCallArgument> Arguments, asCSourceRange Range);
		void LookupMembers(asCRecordDecl* Record, const FString& Name, TArray<asCDecl*>& Out, int32 Depth = 0) const;
		asCFunctionDecl* ResolveCall(TConstArrayView<asCDecl*> Candidates, TConstArrayView<asSParsedCallArgument> Arguments,
			TArray<asCExpr*>& OutArguments, TArray<int32>& OutOrdinals, asCSourceRange Range,
			bool bPreferNonConstOnTie = false);
		const asSExternalCallable* ResolveExternalCall(TConstArrayView<asSExternalCallable> Candidates,
			TConstArrayView<asSParsedCallArgument> Arguments, TArray<asCExpr*>& OutArguments,
			TArray<int32>& OutOrdinals, asCSourceRange Range);
		asCExpr* ActOnExternalCall(const asSExternalCallable& Callable, TArray<asCExpr*> Converted,
			TArray<int32> Ordinals, asCSourceRange Range);
		asCASTContext& Context;
		asCDiagnosticsEngine& Diagnostics;
		FString StableScope;
		asCDeclarationFragment* CurrentFragment = nullptr;
		TArray<asCDeclContext*> ContextStack;
		TUniquePtr<asCDiagnosticFragment> DiagnosticFragment;
		asCBodyFragment* CurrentBody = nullptr;
		asCNamedDecl* CurrentInitializer = nullptr;
		bool bExpressionErrors = false;
		asFBodyNameLookup BodyLookup;
		asFBodyTypeLookup BodyTypeLookup;
		asFStructuredTypeLookup StructuredTypeLookup;
		asFNominalDeclLookup NominalDeclLookup;
		asFBodyInputLookup BodyInputLookup;
		asFListInitializationLookup ListInitializationLookup;
		asFExternalCallableLookup ExternalCallableLookup;
		TMap<asCFunctionDecl*, TMap<FString, asCVarDecl*>> LambdaOuterVariables;
		TArray<asCFunctionDecl*> CompletedLambdaFunctions;
		TArray<asCReturnStmt*> InferredLambdaReturns;
		bool bInferLambdaReturn = false;
		bool bInheritedEditDefaultsContext = false;
		bool bInheritedConstructionContext = false;
		TArray<TMap<FString, asCVarDecl*>> BodyScopes;
		struct asSControlTarget
		{
			asCStmt* Target = nullptr;
			bool bBreakable = false;
			bool bContinuable = false;
		};
		TArray<asSControlTarget> ControlTargets;
		int32 NextLocalOrdinal = 0;
	};

END_AS_NAMESPACE
