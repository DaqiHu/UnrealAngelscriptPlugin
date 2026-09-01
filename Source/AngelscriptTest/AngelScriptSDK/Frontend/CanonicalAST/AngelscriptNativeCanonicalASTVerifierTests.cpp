#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_lifetime.h"
#include "source/as_ast_verifier.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
struct FLocalLifetimeViewFixture
{
	asASTDeclId Destructor;
	asASTDeclId FirstLocal;
	asASTDeclId SecondLocal;
	asASTStmtId Region;
	asASTExprId FirstAssignment;
	asASTExprId SecondAssignment;
	asASTExprId FirstInitializerValue;
	asASTExprId SecondInitializerValue;
};

struct FConstructorLifetimeViewFixture
{
	asASTDeclId PartClass;
	asASTDeclId PartConstructor;
	asASTDeclId PartDestructor;
	asASTDeclId BaseClass;
	asASTDeclId BaseConstructor;
	asASTDeclId BaseDestructor;
	asASTDeclId DerivedClass;
	asASTDeclId DerivedConstructor;
	asASTDeclId DerivedDestructor;
	asASTDeclId FirstField;
	asASTDeclId SecondField;
	asASTStmtId ConstructorBody;
	asASTExprId BaseCall;
	asASTExprId FirstAssignment;
	asASTExprId SecondAssignment;
};

struct FAggregateLifetimeViewFixture
{
	asASTDeclId ElementClass;
	asASTDeclId ElementDestructor;
	asASTDeclId InnerArrayClass;
	asASTDeclId InnerArrayDestructor;
	asASTDeclId InnerListFactory;
	asASTDeclId OuterArrayClass;
	asASTDeclId OuterListFactory;
	asASTDeclId Function;
	asASTStmtId FunctionBody;
	asASTExprId FirstInnerAggregate;
	asASTExprId SecondInnerAggregate;
	asASTExprId OuterAggregate;
	asCArray<asASTExprId> FirstInnerElements;
	asCArray<asASTExprId> SecondInnerElements;
};

static asASTDeclId CreateListFactory(
	asCASTContext& Context,
	asASTDeclId Owner,
	const char* Name,
	const asCQualType& VoidType)
{
	const asASTDeclId Factory = Context.CreateDecl(
		asAST_DECL_CONSTRUCTOR, Owner, asCSourceRange(), Name);
	Context.SetDeclType(Factory, VoidType);
	Context.AddDeclTrait(Factory, asAST_TRAIT_LIST_FACTORY);
	return Factory;
}

static int CreateSwitchCaseDomainFact(
	asCASTContext& Context,
	const asCQualType& DomainType,
	const asCSourceRange& Range,
	asASTExprId AuthoredValue,
	asQWORD LiteralBits,
	asASTExprId& OutFact)
{
	OutFact = Context.CreateExpr(
		asAST_EXPR_CONVERSION, DomainType, asAST_VALUE_PRVALUE, Range);
	int r = Context.SetLiteral(OutFact, "switch-case-domain");
	if( r < 0 )
		return r;
	r = Context.SetLiteralBits(OutFact, LiteralBits);
	if( r < 0 )
		return r;
	return Context.AddExprChild(OutFact, AuthoredValue);
}

static FAggregateLifetimeViewFixture BuildAggregateLifetimeViewFixture(
	asCASTContext& Context)
{
	FAggregateLifetimeViewFixture Fixture;
	const asASTDeclId Tu = Context.CreateTranslationUnit(
		"AggregateLifetimeView");
	const asCQualType VoidType = Context.InternPrimitive(ttVoid);
	const asCQualType ElementType = Context.InternNamedType(
		asAST_TYPE_VALUE_OBJECT, "FElement", 0);
	const asCQualType InnerArrayType = Context.InternNamedType(
		asAST_TYPE_VALUE_OBJECT, "FElementArray", 0);
	const asCQualType OuterArrayType = Context.InternNamedType(
		asAST_TYPE_VALUE_OBJECT, "FElementArrayArray", 0);

	Fixture.ElementClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FElement");
	Context.SetStableKey(Fixture.ElementClass, "FElement");
	Context.SetDeclType(Fixture.ElementClass, ElementType);
	Fixture.ElementDestructor = Context.CreateDecl(
		asAST_DECL_DESTRUCTOR, Fixture.ElementClass,
		asCSourceRange(), "FElement");
	Context.SetDeclType(Fixture.ElementDestructor, VoidType);

	Fixture.InnerArrayClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FElementArray");
	Context.SetStableKey(Fixture.InnerArrayClass, "FElementArray");
	Context.SetDeclType(Fixture.InnerArrayClass, InnerArrayType);
	Fixture.InnerArrayDestructor = Context.CreateDecl(
		asAST_DECL_DESTRUCTOR, Fixture.InnerArrayClass,
		asCSourceRange(), "FElementArray");
	Context.SetDeclType(Fixture.InnerArrayDestructor, VoidType);
	Fixture.InnerListFactory = CreateListFactory(
		Context, Fixture.InnerArrayClass, "FElementArray", VoidType);

	Fixture.OuterArrayClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FElementArrayArray");
	Context.SetStableKey(Fixture.OuterArrayClass, "FElementArrayArray");
	Context.SetDeclType(Fixture.OuterArrayClass, OuterArrayType);
	Fixture.OuterListFactory = CreateListFactory(
		Context, Fixture.OuterArrayClass, "FElementArrayArray", VoidType);

	Fixture.Function = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, asCSourceRange(), "BuildArrays");
	Context.SetDeclType(Fixture.Function, VoidType);
	Fixture.FunctionBody = Context.CreateStmt(
		asAST_STMT_BLOCK, Fixture.Function, asCSourceRange());
	Context.SetBody(Fixture.Function, Fixture.FunctionBody);

	auto CreateInnerAggregate = [&](asUINT ElementCount,
		asCArray<asASTExprId>& OutElements) -> asASTExprId
	{
		const asASTExprId Aggregate = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, InnerArrayType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(Aggregate, "list-pattern");
		Context.SetResolvedDecl(Aggregate, Fixture.InnerListFactory);
		for( asUINT ElementIndex = 0;
			ElementIndex < ElementCount; ++ElementIndex )
		{
			const asASTExprId Element = Context.CreateExpr(
				asAST_EXPR_CONSTRUCT, ElementType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Element, "FElement::FElement()");
			Context.AddExprChild(Aggregate, Element);
			OutElements.PushLast(Element);
		}
		return Aggregate;
	};

	Fixture.FirstInnerAggregate = CreateInnerAggregate(
		3, Fixture.FirstInnerElements);
	Fixture.SecondInnerAggregate = CreateInnerAggregate(
		2, Fixture.SecondInnerElements);
	Fixture.OuterAggregate = Context.CreateExpr(
		asAST_EXPR_CONSTRUCT, OuterArrayType,
		asAST_VALUE_PRVALUE, asCSourceRange());
	Context.SetLiteral(Fixture.OuterAggregate, "list-pattern");
	Context.SetResolvedDecl(
		Fixture.OuterAggregate, Fixture.OuterListFactory);
	Context.AddExprChild(
		Fixture.OuterAggregate, Fixture.FirstInnerAggregate);
	Context.AddExprChild(
		Fixture.OuterAggregate, Fixture.SecondInnerAggregate);

	const asASTStmtId AggregateStatement = Context.CreateStmt(
		asAST_STMT_EXPR, Fixture.Function, asCSourceRange());
	Context.SetStmtExpr(AggregateStatement, Fixture.OuterAggregate);
	Context.AddStmtChild(Fixture.FunctionBody, AggregateStatement);
	return Fixture;
}

static asSASTLifetimeRecord MakeAggregateLifetimeRecord(
	asASTExprId Aggregate,
	asASTDeclId ElementDestructor,
	asASTDeclId SemanticOwner,
	asUINT Order)
{
	asSASTLifetimeRecord Record;
	Record.subjectKind = asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT;
	Record.subject = asSASTLifetimeNodeId::FromExpr(Aggregate);
	Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_ELEMENT;
	Record.actionTarget = asSASTLifetimeNodeId::FromDecl(ElementDestructor);
	Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Aggregate);
	Record.semanticRegion = asSASTLifetimeNodeId::FromDecl(SemanticOwner);
	Record.phase = asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT;
	Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
	Record.constructionStep = 0;
	Record.order = Order;
	Record.completeObjectCommit = false;
	return Record;
}

static FConstructorLifetimeViewFixture BuildConstructorLifetimeViewFixture(
	asCASTContext& Context)
{
	FConstructorLifetimeViewFixture Fixture;
	const asASTDeclId Tu = Context.CreateTranslationUnit("ConstructorLifetimeView");
	const asCQualType VoidType = Context.InternPrimitive(ttVoid);
	const asCQualType PartType = Context.InternNamedType(
		asAST_TYPE_VALUE_OBJECT, "FPart", 0);
	const asCQualType BaseType = Context.InternNamedType(
		asAST_TYPE_REFERENCE_OBJECT, "FBase",
		asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
	const asCQualType DerivedType = Context.InternNamedType(
		asAST_TYPE_REFERENCE_OBJECT, "FDerived",
		asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);

	Fixture.PartClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FPart");
	Context.SetStableKey(Fixture.PartClass, "FPart");
	Context.SetDeclType(Fixture.PartClass, PartType);
	Fixture.PartConstructor = Context.CreateDecl(
		asAST_DECL_CONSTRUCTOR, Fixture.PartClass,
		asCSourceRange(), "FPart");
	Context.SetDeclType(Fixture.PartConstructor, VoidType);
	Fixture.PartDestructor = Context.CreateDecl(
		asAST_DECL_DESTRUCTOR, Fixture.PartClass,
		asCSourceRange(), "FPart");
	Context.SetDeclType(Fixture.PartDestructor, VoidType);

	Fixture.BaseClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FBase");
	Context.SetStableKey(Fixture.BaseClass, "FBase");
	Context.SetDeclType(Fixture.BaseClass, BaseType);
	Fixture.BaseConstructor = Context.CreateDecl(
		asAST_DECL_CONSTRUCTOR, Fixture.BaseClass,
		asCSourceRange(), "FBase");
	Context.SetDeclType(Fixture.BaseConstructor, VoidType);
	Fixture.BaseDestructor = Context.CreateDecl(
		asAST_DECL_DESTRUCTOR, Fixture.BaseClass,
		asCSourceRange(), "FBase");
	Context.SetDeclType(Fixture.BaseDestructor, VoidType);

	Fixture.DerivedClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FDerived");
	Context.SetStableKey(Fixture.DerivedClass, "FDerived");
	Context.SetDeclType(Fixture.DerivedClass, DerivedType);
	Context.AddBase(Fixture.DerivedClass, Fixture.BaseClass);
	Fixture.FirstField = Context.CreateDecl(
		asAST_DECL_VAR, Fixture.DerivedClass,
		asCSourceRange(), "First");
	Context.SetDeclType(Fixture.FirstField, PartType);
	Fixture.SecondField = Context.CreateDecl(
		asAST_DECL_VAR, Fixture.DerivedClass,
		asCSourceRange(), "Second");
	Context.SetDeclType(Fixture.SecondField, PartType);
	Fixture.DerivedConstructor = Context.CreateDecl(
		asAST_DECL_CONSTRUCTOR, Fixture.DerivedClass,
		asCSourceRange(), "FDerived");
	Context.SetDeclType(Fixture.DerivedConstructor, VoidType);
	Fixture.DerivedDestructor = Context.CreateDecl(
		asAST_DECL_DESTRUCTOR, Fixture.DerivedClass,
		asCSourceRange(), "FDerived");
	Context.SetDeclType(Fixture.DerivedDestructor, VoidType);

	const asASTExprId ThisExpr = Context.CreateExpr(
		asAST_EXPR_THIS, DerivedType,
		asAST_VALUE_PRVALUE, asCSourceRange());
	Context.SetLiteral(ThisExpr, "this");
	Fixture.BaseCall = Context.CreateExpr(
		asAST_EXPR_CALL, VoidType,
		asAST_VALUE_PRVALUE, asCSourceRange());
	Context.SetLiteral(Fixture.BaseCall, "FBase:reverse-formal");
	Context.SetResolvedDecl(Fixture.BaseCall, Fixture.BaseConstructor);
	Context.SetExprReceiver(Fixture.BaseCall, ThisExpr);
	Context.AddExprChild(Fixture.BaseCall, ThisExpr);
	Context.AddDeclInit(Fixture.DerivedConstructor, Fixture.BaseCall);

	auto AddMemberInit = [&](asASTDeclId Field) -> asASTExprId
	{
		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, PartType,
			asAST_VALUE_LVALUE, asCSourceRange());
		Context.SetResolvedDecl(Target, Field);
		const asASTExprId Construct = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, PartType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(Construct, "FPart::FPart()");
		Context.SetResolvedDecl(Construct, Fixture.PartConstructor);
		const asASTExprId Assignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, PartType,
			asAST_VALUE_LVALUE, asCSourceRange());
		Context.SetLiteral(Assignment, "=");
		Context.AddExprChild(Assignment, Target);
		Context.AddExprChild(Assignment, Construct);
		Context.AddDeclInit(Fixture.DerivedConstructor, Assignment);
		return Assignment;
	};
	Fixture.FirstAssignment = AddMemberInit(Fixture.FirstField);
	Fixture.SecondAssignment = AddMemberInit(Fixture.SecondField);
	Fixture.ConstructorBody = Context.CreateStmt(
		asAST_STMT_BLOCK, Fixture.DerivedConstructor, asCSourceRange());
	Context.SetBody(Fixture.DerivedConstructor, Fixture.ConstructorBody);
	return Fixture;
}

static asSASTLifetimeRecord MakeConstructorLifetimeRecord(
	const FConstructorLifetimeViewFixture& Fixture,
	asEASTLifetimePhase Phase,
	asUINT Step,
	asUINT Order)
{
	asSASTLifetimeRecord Record;
	Record.subjectKind = asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT;
	Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
	Record.semanticRegion = asSASTLifetimeNodeId::FromDecl(
		Fixture.DerivedConstructor);
	Record.phase = Phase;
	Record.constructionStep = Step;
	Record.order = Order;
	if( Phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE )
	{
		Record.subject = asSASTLifetimeNodeId::FromDecl(Fixture.BaseClass);
		Record.actionTarget = asSASTLifetimeNodeId::FromDecl(
			Fixture.BaseDestructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromExpr(
			Fixture.BaseCall);
		Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
	}
	else if( Phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER )
	{
		const bool First = Step == 1;
		Record.subject = asSASTLifetimeNodeId::FromDecl(
			First ? Fixture.FirstField : Fixture.SecondField);
		Record.actionTarget = asSASTLifetimeNodeId::FromDecl(
			Fixture.PartDestructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromExpr(
			First ? Fixture.FirstAssignment : Fixture.SecondAssignment);
		Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
	}
	else
	{
		Record.subject = asSASTLifetimeNodeId::FromDecl(Fixture.DerivedClass);
		Record.actionTarget = asSASTLifetimeNodeId::FromDecl(
			Fixture.DerivedDestructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromStmt(
			Fixture.ConstructorBody);
		Record.supportedExitMask = asAST_LIFETIME_EXIT_NORMAL;
		Record.completeObjectCommit = true;
	}
	return Record;
}

static void AddCompleteNonDelegatingConstructorLifetimePlan(
	asCASTContext& Context,
	const FConstructorLifetimeViewFixture& Fixture)
{
	Context.AddLifetimeRecord(MakeConstructorLifetimeRecord(
		Fixture, asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE, 0, 0));
	Context.AddLifetimeRecord(MakeConstructorLifetimeRecord(
		Fixture, asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER, 1, 1));
	Context.AddLifetimeRecord(MakeConstructorLifetimeRecord(
		Fixture, asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER, 2, 2));
	Context.AddLifetimeRecord(MakeConstructorLifetimeRecord(
		Fixture, asAST_LIFETIME_PHASE_COMPLETE_OBJECT, 3, 3));
}

static FLocalLifetimeViewFixture BuildLocalLifetimeViewFixture(
	asCASTContext& Context)
{
	FLocalLifetimeViewFixture Fixture;
	const asASTDeclId Tu = Context.CreateTranslationUnit("LifetimeView");
	const asCQualType ValueType = Context.InternNamedType(
		asAST_TYPE_VALUE_OBJECT, "FValue", 0);
	const asASTDeclId ValueClass = Context.CreateDecl(
		asAST_DECL_CLASS, Tu, asCSourceRange(), "FValue");
	Context.SetStableKey(ValueClass, "FValue");
	Fixture.Destructor = Context.CreateDecl(
		asAST_DECL_DESTRUCTOR, ValueClass, asCSourceRange(), "FValue");
	const asASTDeclId Function = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
	Fixture.Region = Context.CreateStmt(
		asAST_STMT_BLOCK, Function, asCSourceRange());
	Context.SetBody(Function, Fixture.Region);

	for( asUINT LocalIndex = 0; LocalIndex < 2; ++LocalIndex )
	{
		const bool IsFirst = LocalIndex == 0;
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(),
			IsFirst ? "First" : "Second");
		Context.SetDeclType(Local, ValueType);
		const asASTStmtId DeclStmt = Context.CreateStmt(
			asAST_STMT_DECL, Function, asCSourceRange());
		Context.SetStmtDecl(DeclStmt, Local);
		Context.AddStmtChild(Fixture.Region, DeclStmt);

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType,
			asAST_VALUE_LVALUE, asCSourceRange());
		Context.SetResolvedDecl(Target, Local);
		const asASTExprId InitializerValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, ValueType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(InitializerValue, IsFirst ? "1" : "2");
		const asASTExprId Assignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ValueType,
			asAST_VALUE_LVALUE, asCSourceRange());
		Context.SetLiteral(Assignment, "=");
		Context.AddExprChild(Assignment, Target);
		Context.AddExprChild(Assignment, InitializerValue);
		Context.AddDeclInit(Local, Assignment);
		const asASTStmtId ExprStmt = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		Context.SetStmtExpr(ExprStmt, Assignment);
		Context.AddStmtChild(Fixture.Region, ExprStmt);

		if( IsFirst )
		{
			Fixture.FirstLocal = Local;
			Fixture.FirstAssignment = Assignment;
			Fixture.FirstInitializerValue = InitializerValue;
		}
		else
		{
			Fixture.SecondLocal = Local;
			Fixture.SecondAssignment = Assignment;
			Fixture.SecondInitializerValue = InitializerValue;
		}
	}
	return Fixture;
}

static asSASTLifetimeRecord MakeLocalLifetimeRecord(
	const FLocalLifetimeViewFixture& Fixture,
	bool First,
	asUINT Order)
{
	asSASTLifetimeRecord Record;
	Record.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
	Record.subject = asSASTLifetimeNodeId::FromDecl(
		First ? Fixture.FirstLocal : Fixture.SecondLocal);
	Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
	Record.actionTarget = asSASTLifetimeNodeId::FromDecl(Fixture.Destructor);
	Record.activationPoint = asSASTLifetimeNodeId::FromExpr(
		First ? Fixture.FirstAssignment : Fixture.SecondAssignment);
	Record.semanticRegion = asSASTLifetimeNodeId::FromStmt(Fixture.Region);
	Record.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
	Record.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
	Record.constructionStep = 0;
	Record.order = Order;
	Record.completeObjectCommit = false;
	return Record;
}

static asASTStmtId CreateScopeExitCompatibilityStatement(
	asCASTContext& Context,
	const asSASTLifetimeRecord& Record)
{
	const asASTDeclId Local(Record.subject.value);
	const asCDecl* const LocalDecl = Context.GetDecl(Local);
	const asASTExprId Target = Context.CreateExpr(
		asAST_EXPR_DECL_REF, LocalDecl->type,
		asAST_VALUE_LVALUE, asCSourceRange());
	Context.SetResolvedDecl(Target, Local);
	const asASTExprId Cleanup = Context.CreateExpr(
		asAST_EXPR_CLEANUP, LocalDecl->type,
		asAST_VALUE_PRVALUE, asCSourceRange());
	Context.SetLiteral(Cleanup, "scope-exit");
	Context.SetResolvedDecl(Cleanup, asASTDeclId(Record.actionTarget.value));
	Context.AddExprChild(Cleanup, Target);
	const asASTStmtId CleanupStmt = Context.CreateStmt(
		asAST_STMT_EXPR, LocalDecl->parent, asCSourceRange());
	Context.SetStmtExpr(CleanupStmt, Cleanup);
	return CleanupStmt;
}

static asASTStmtId AppendNormalScopeExit(
	asCASTContext& Context,
	const asSASTLifetimeRecord& Record)
{
	const asASTStmtId CleanupStmt =
		CreateScopeExitCompatibilityStatement(Context, Record);
	Context.AddStmtChild(
		asASTStmtId(Record.semanticRegion.value), CleanupStmt);
	return CleanupStmt;
}

struct FMethodRelationVerifierFixture
{
	asASTDeclId Tu;
	asASTDeclId Interface;
	asASTDeclId Requirement;
	asASTDeclId BaseClass;
	asASTDeclId BaseImplementation;
	asASTDeclId Owner;
	asCQualType IntType;
	asCQualType BoolType;
};

static asASTDeclId AddRelationMethod(
	asCASTContext& Context,
	asASTDeclId Owner,
	const char* StableKey,
	const asCQualType& ReturnType,
	const asCQualType& ParameterType)
{
	const asASTDeclId Method = Context.CreateDecl(
		asAST_DECL_METHOD, Owner, asCSourceRange(), "Probe");
	Context.SetStableKey(Method, StableKey);
	Context.SetDeclType(Method, ReturnType);
	const asASTDeclId Parameter = Context.CreateDecl(
		asAST_DECL_PARAM, Method, asCSourceRange(), "Value");
	Context.SetDeclType(Parameter, ParameterType);
	return Method;
}

static FMethodRelationVerifierFixture BuildMethodRelationVerifierFixture(
	asCASTContext& Context,
	bool AttachInterface = true)
{
	FMethodRelationVerifierFixture Fixture;
	Fixture.Tu = Context.CreateTranslationUnit("MethodRelationVerifier");
	Fixture.IntType = Context.InternPrimitive(ttInt, 0);
	Fixture.BoolType = Context.InternPrimitive(ttBool, 0);
	Fixture.Interface = Context.CreateDecl(
		asAST_DECL_INTERFACE, Fixture.Tu, asCSourceRange(), "IProbe");
	Context.SetStableKey(Fixture.Interface, "IProbe");
	Fixture.Requirement = AddRelationMethod(
		Context,
		Fixture.Interface,
		"IProbe::Probe(int)",
		Fixture.IntType,
		Fixture.IntType);
	Fixture.BaseClass = Context.CreateDecl(
		asAST_DECL_CLASS, Fixture.Tu, asCSourceRange(), "BaseProbe");
	Context.SetStableKey(Fixture.BaseClass, "BaseProbe");
	Fixture.BaseImplementation = AddRelationMethod(
		Context,
		Fixture.BaseClass,
		"BaseProbe::Probe(int)",
		Fixture.IntType,
		Fixture.IntType);
	Fixture.Owner = Context.CreateDecl(
		asAST_DECL_CLASS, Fixture.Tu, asCSourceRange(), "DerivedProbe");
	Context.SetStableKey(Fixture.Owner, "DerivedProbe");
	Context.AddBase(Fixture.Owner, Fixture.BaseClass);
	if( AttachInterface )
	{
		Context.AddBase(Fixture.Owner, Fixture.Interface);
	}
	return Fixture;
}

static bool BuildIncompleteCallArgumentFixture(
	asCASTContext& Context,
	bool AddExactBRecord)
{
	const asASTDeclId Tu = Context.CreateTranslationUnit(
		AddExactBRecord ? "CallPartialRecords" : "CallMissingRecords");
	const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
	const asASTDeclId Callee = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Pack");
	const asASTDeclId A = Context.CreateDecl(
		asAST_DECL_PARAM, Callee, asCSourceRange(), "A");
	const asASTDeclId B = Context.CreateDecl(
		asAST_DECL_PARAM, Callee, asCSourceRange(), "B");
	const asASTDeclId Entry = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Entry");
	const asASTStmtId Return = Context.CreateStmt(
		asAST_STMT_RETURN, Entry, asCSourceRange());
	const asASTExprId ArgA = Context.CreateExpr(
		asAST_EXPR_INTEGER_LITERAL, IntType,
		asAST_VALUE_PRVALUE, asCSourceRange());
	const asASTExprId ArgB = Context.CreateExpr(
		asAST_EXPR_INTEGER_LITERAL, IntType,
		asAST_VALUE_PRVALUE, asCSourceRange());
	const asASTExprId Call = Context.CreateExpr(
		asAST_EXPR_CALL, IntType,
		asAST_VALUE_PRVALUE, asCSourceRange());
	if( !Tu.IsValid() || !IntType.IsValid() || !Callee.IsValid()
		|| !A.IsValid() || !B.IsValid() || !Entry.IsValid()
		|| !Return.IsValid() || !ArgA.IsValid() || !ArgB.IsValid()
		|| !Call.IsValid()
		|| Context.SetDeclType(Callee, IntType) < 0
		|| Context.SetDeclType(A, IntType) < 0
		|| Context.SetDeclType(B, IntType) < 0
		|| Context.SetDeclType(Entry, IntType) < 0
		|| Context.SetLiteralBits(ArgA, 1) < 0
		|| Context.SetLiteralBits(ArgB, 2) < 0
		|| Context.AddExprChild(Call, ArgB) < 0
		|| Context.AddExprChild(Call, ArgA) < 0
		|| Context.SetResolvedDecl(Call, Callee) < 0
		|| Context.SetExprCallDispatch(
			Call, asAST_CALL_DISPATCH_DIRECT) < 0 )
	{
		return false;
	}

	if( AddExactBRecord )
	{
		asSASTCallArgument ExactB;
		ExactB.expression = ArgB;
		ExactB.formal = B;
		ExactB.formalType = IntType;
		ExactB.formalIndex = 1;
		ExactB.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		ExactB.sourceOrdinal = 1;
		if( Context.AddCallArgument(Call, ExactB) < 0 )
		{
			return false;
		}
	}

	return Context.SetBody(Entry, Return) == 0
		&& Context.SetStmtExpr(Return, Call) == 0;
}

enum ECallArgumentProvenanceMutation
{
	CALL_PROVENANCE_VALID = 0,
	CALL_PROVENANCE_INVALID_ORIGIN,
	CALL_PROVENANCE_NAMED_WITHOUT_NAME,
	CALL_PROVENANCE_POSITIONAL_WITH_NAME,
	CALL_PROVENANCE_SYNTHESIZED_WITH_SOURCE,
	CALL_PROVENANCE_DUPLICATE_SOURCE_ORDINAL,
	CALL_PROVENANCE_FOREIGN_RANGE,
	CALL_PROVENANCE_WRONG_NAMED_FORMAL,
	CALL_PROVENANCE_HIDDEN_NON_HIDDEN_FORMAL,
	CALL_PROVENANCE_DEFAULT_WITHOUT_INITIALIZER,
	CALL_PROVENANCE_IMPLICIT_RECEIVER_WITHOUT_RECEIVER,
};

static bool BuildCallArgumentProvenanceFixture(
	asCASTContext& Context,
	ECallArgumentProvenanceMutation Mutation)
{
	const char Source[] = "Pack(1, B:2)";
	const asASTFileID File = Context.AddSourceSection(
		"CallArgumentProvenance.as",
		asAST_SOURCE_AUTHORED,
		Source,
		sizeof(Source) - 1,
		0);
	if( File == 0 )
	{
		return false;
	}
	const asCSourceManager& Sources = Context.GetSourceManager();
	const asCSourceRange CallRange = Sources.MakeRange(File, 0, sizeof(Source) - 1);
	const asCSourceRange ARange = Sources.MakeRange(File, 5, 6);
	const asCSourceRange BRange = Sources.MakeRange(File, 10, 11);
	const asASTDeclId Tu = Context.CreateTranslationUnit("CallArgumentProvenance");
	const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
	const asASTDeclId Callee = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, CallRange, "Pack");
	const asASTDeclId A = Context.CreateDecl(
		asAST_DECL_PARAM, Callee, ARange, "A");
	const asASTDeclId B = Context.CreateDecl(
		asAST_DECL_PARAM, Callee, BRange, "B");
	const asASTDeclId C = Context.CreateDecl(
		asAST_DECL_PARAM, Callee, CallRange, "C");
	const asASTDeclId Hidden = Context.CreateDecl(
		asAST_DECL_PARAM, Callee, CallRange, "Hidden");
	const asASTDeclId Entry = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, CallRange, "Entry");
	const asASTStmtId Return = Context.CreateStmt(
		asAST_STMT_RETURN, Entry, CallRange);
	const asASTExprId ArgA = Context.CreateExpr(
		asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, ARange);
	const asASTExprId ArgB = Context.CreateExpr(
		asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, BRange);
	const asASTExprId DefaultC = Context.CreateExpr(
		asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, CallRange);
	const asASTExprId HiddenValue = Context.CreateExpr(
		asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, CallRange);
	const asASTExprId Call = Context.CreateExpr(
		asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, CallRange);
	if( !Tu.IsValid() || !IntType.IsValid() || !Callee.IsValid()
		|| !A.IsValid() || !B.IsValid() || !C.IsValid() || !Hidden.IsValid()
		|| !Entry.IsValid() || !Return.IsValid() || !ArgA.IsValid()
		|| !ArgB.IsValid() || !DefaultC.IsValid() || !HiddenValue.IsValid()
		|| !Call.IsValid()
		|| Context.SetDeclType(Callee, IntType) < 0
		|| Context.SetDeclType(A, IntType) < 0
		|| Context.SetDeclType(B, IntType) < 0
		|| Context.SetDeclType(C, IntType) < 0
		|| Context.SetDeclType(Hidden, IntType) < 0
		|| Context.SetDeclType(Entry, IntType) < 0
		|| Context.SetDefaultArg(C, "3") < 0
		|| Context.SetOrigin(Hidden, "hidden") < 0
		|| Context.SetLiteralBits(ArgA, 1) < 0
		|| Context.SetLiteralBits(ArgB, 2) < 0
		|| Context.SetLiteralBits(DefaultC, 3) < 0
		|| Context.SetLiteralBits(HiddenValue, 4) < 0
		|| Context.AddExprChild(Call, HiddenValue) < 0
		|| Context.AddExprChild(Call, DefaultC) < 0
		|| Context.AddExprChild(Call, ArgB) < 0
		|| Context.AddExprChild(Call, ArgA) < 0
		|| Context.SetResolvedDecl(Call, Callee) < 0
		|| Context.SetExprCallDispatch(Call, asAST_CALL_DISPATCH_DIRECT) < 0 )
	{
		return false;
	}

	asSASTCallArgument HiddenRecord;
	HiddenRecord.expression = HiddenValue;
	HiddenRecord.formal = Hidden;
	HiddenRecord.formalType = IntType;
	HiddenRecord.range = CallRange;
	HiddenRecord.formalIndex = 3;
	HiddenRecord.origin = asAST_CALL_ARGUMENT_HIDDEN;
	HiddenRecord.sourceOrdinal = asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL;
	asSASTCallArgument DefaultRecord;
	DefaultRecord.expression = DefaultC;
	DefaultRecord.formal = C;
	DefaultRecord.formalType = IntType;
	DefaultRecord.range = CallRange;
	DefaultRecord.formalIndex = 2;
	DefaultRecord.origin = asAST_CALL_ARGUMENT_DEFAULT;
	DefaultRecord.sourceOrdinal = asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL;
	asSASTCallArgument NamedRecord;
	NamedRecord.expression = ArgB;
	NamedRecord.formal = B;
	NamedRecord.formalType = IntType;
	NamedRecord.range = BRange;
	NamedRecord.authoredName = "B";
	NamedRecord.formalIndex = 1;
	NamedRecord.sourceOrdinal = 1;
	NamedRecord.origin = asAST_CALL_ARGUMENT_NAMED;
	asSASTCallArgument PositionalRecord;
	PositionalRecord.expression = ArgA;
	PositionalRecord.formal = A;
	PositionalRecord.formalType = IntType;
	PositionalRecord.range = ARange;
	PositionalRecord.formalIndex = 0;
	PositionalRecord.sourceOrdinal = 0;
	PositionalRecord.origin = asAST_CALL_ARGUMENT_POSITIONAL;

	switch( Mutation )
	{
	case CALL_PROVENANCE_INVALID_ORIGIN:
		NamedRecord.origin = static_cast<asEASTCallArgumentOrigin>(0xff);
		break;
	case CALL_PROVENANCE_NAMED_WITHOUT_NAME:
		NamedRecord.authoredName = "";
		break;
	case CALL_PROVENANCE_POSITIONAL_WITH_NAME:
		PositionalRecord.authoredName = "A";
		break;
	case CALL_PROVENANCE_SYNTHESIZED_WITH_SOURCE:
		DefaultRecord.sourceOrdinal = 2;
		break;
	case CALL_PROVENANCE_DUPLICATE_SOURCE_ORDINAL:
		NamedRecord.sourceOrdinal = 0;
		break;
	case CALL_PROVENANCE_FOREIGN_RANGE:
		NamedRecord.range = asCSourceRange(
			asCSourceLocation(File + 100, 0),
			asCSourceLocation(File + 100, 1));
		break;
	case CALL_PROVENANCE_WRONG_NAMED_FORMAL:
		NamedRecord.authoredName = "A";
		break;
	case CALL_PROVENANCE_HIDDEN_NON_HIDDEN_FORMAL:
		DefaultRecord.origin = asAST_CALL_ARGUMENT_HIDDEN;
		break;
	case CALL_PROVENANCE_DEFAULT_WITHOUT_INITIALIZER:
		PositionalRecord.origin = asAST_CALL_ARGUMENT_DEFAULT;
		PositionalRecord.sourceOrdinal = asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL;
		break;
	case CALL_PROVENANCE_IMPLICIT_RECEIVER_WITHOUT_RECEIVER:
		PositionalRecord.origin = asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER;
		PositionalRecord.sourceOrdinal = asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL;
		break;
	case CALL_PROVENANCE_VALID:
	default:
		break;
	}

	return Context.AddCallArgument(Call, HiddenRecord) == 0
		&& Context.AddCallArgument(Call, DefaultRecord) == 0
		&& Context.AddCallArgument(Call, NamedRecord) == 0
		&& Context.AddCallArgument(Call, PositionalRecord) == 0
		&& Context.SetBody(Entry, Return) == 0
		&& Context.SetStmtExpr(Return, Call) == 0;
}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTVerifierTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Verifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(VerifierRejectsForeignWrongOwnerWrongSignatureAndDuplicateMethodRelations)
	{
		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			ASSERT_THAT(AreEqual(
				(int)asAST_VERIFY_DANGLING_ID,
				Context.AddMethodRelation(
					Fixture.Owner,
					asSASTMethodRelation(
						asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
						asASTDeclId(Fixture.BaseImplementation.value, 0x51u),
						Fixture.Requirement)),
				TEXT("construction must reject a foreign snapshot identity")));
		}

		auto RequireRelationFailure = [this](
			asCASTContext& Context,
			asEASTVerifyCategory ExpectedCategory,
			const char* ExpectedDetail,
			const TCHAR* Message)
		{
			asSAstVerifyResult Result;
			const int Status = asCASTVerify(Context, Result);
			ASSERT_THAT(AreEqual((int)ExpectedCategory, Status, Message));
			ASSERT_THAT(IsTrue(
				Result.detail.Equals(ExpectedDetail),
				*FString::Printf(
					TEXT("expected stable detail '%hs', got '%hs'"),
					ExpectedDetail,
					Result.detail.AddressOf())));
			ASSERT_THAT(AreEqual(
				(int)ExpectedCategory,
				Context.Seal(),
				TEXT("the publication Seal gate must reject the same forged relation")));
		};

		{
			// Construction rejects foreign IDs early; this white-box corruption
			// proves the final Seal verifier still classifies them independently.
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			asCDecl* const Owner =
				const_cast<asCDecl*>(Context.GetDecl(Fixture.Owner));
			ASSERT_THAT(IsNotNull(Owner));
			Owner->methodRelations.PushLast(asSASTMethodRelation(
				asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
				asASTDeclId(Fixture.BaseImplementation.value, 0x51u),
				Fixture.Requirement));
			RequireRelationFailure(
				Context, asAST_VERIFY_FOREIGN_ID,
				"method-relation-implementation-foreign",
				TEXT("the final verifier must reject a foreign implementation ID"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			const asASTDeclId WrongOwner = Context.CreateDecl(
				asAST_DECL_VAR, Fixture.Tu, asCSourceRange(), "WrongOwner");
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				WrongOwner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_WRONG_KIND,
				"method-relation-owner-kind",
				TEXT("a VarDecl cannot own method relations"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context, false);
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-interface-closure",
				TEXT("an unrelated interface requirement is not in the owner closure"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			const asASTDeclId WrongSignature = AddRelationMethod(
				Context,
				Fixture.Owner,
				"DerivedProbe::Probe(bool)",
				Fixture.IntType,
				Fixture.BoolType);
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					WrongSignature,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_TYPE,
				"method-relation-signature",
				TEXT("relation endpoints must have an exact Canonical signature"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			const asASTDeclId CompetingImplementation = AddRelationMethod(
				Context,
				Fixture.Owner,
				"DerivedProbe::Probe(int)",
				Fixture.IntType,
				Fixture.IntType);
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					CompetingImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-requirement-duplicate",
				TEXT("one requirement cannot have two implementations"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-interface-incomplete",
				TEXT("every concrete interface requirement needs one exact relation"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			Context.SetDeclType(
				Fixture.BaseImplementation,
				asCQualType(asASTTypeRef(999), 0));
			Context.SetDeclType(
				Fixture.Requirement,
				asCQualType(asASTTypeRef(999), 0));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_TYPE,
				"method-relation-signature-type",
				TEXT("relation signatures cannot contain a dangling Canonical type"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Fixture.Requirement, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetBody(Fixture.Requirement, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-interface-method-body",
				TEXT("interface methods are declaration-only Canonical requirements"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			const asASTDeclId OtherInterface = Context.CreateDecl(
				asAST_DECL_INTERFACE, Fixture.Tu, asCSourceRange(), "IOther");
			Context.SetStableKey(OtherInterface, "IOther");
			ASSERT_THAT(AreEqual(0,
				Context.AddBase(Fixture.Interface, OtherInterface)));
			ASSERT_THAT(AreEqual(0,
				Context.AddBase(OtherInterface, Fixture.Interface)));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-interface-ancestry-cycle",
				TEXT("an interface ancestry cycle cannot be silently deduplicated"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			ASSERT_THAT(AreEqual(0,
				Context.AddBase(Fixture.BaseClass, Fixture.Owner)));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-class-ancestry-cycle",
				TEXT("a class ancestry cycle cannot be silently deduplicated"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			const asASTDeclId SecondBase = Context.CreateDecl(
				asAST_DECL_CLASS, Fixture.Tu, asCSourceRange(), "SecondBase");
			ASSERT_THAT(AreEqual(0, Context.AddBase(Fixture.Owner, SecondBase)));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-class-ancestry-multiple-bases",
				TEXT("Canonical class ancestry must preserve AS single inheritance"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			ASSERT_THAT(AreEqual(0,
				Context.AddBase(Fixture.Interface, Fixture.BaseClass)));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_WRONG_KIND,
				"method-relation-interface-base-kind",
				TEXT("an interface cannot inherit a concrete class"));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			ASSERT_THAT(AreEqual(0,
				Context.AddBase(Fixture.Owner, Fixture.Interface)));
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			RequireRelationFailure(
				Context, asAST_VERIFY_INVALID_CHILD,
				"method-relation-interface-ancestry-duplicate",
				TEXT("a direct duplicate interface base cannot claim one closure twice"));
		}

		{
			// A shared transitive interface is a legal diamond, not a duplicate
			// authored base. Keep this positive case beside the duplicate gate.
			asCASTContext Context;
			const asASTDeclId Tu =
				Context.CreateTranslationUnit("MethodRelationDiamond");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Root = Context.CreateDecl(
				asAST_DECL_INTERFACE, Tu, asCSourceRange(), "IRoot");
			Context.SetStableKey(Root, "IRoot");
			const asASTDeclId Requirement = AddRelationMethod(
				Context, Root, "IRoot::Probe(int)", IntType, IntType);
			const asASTDeclId Left = Context.CreateDecl(
				asAST_DECL_INTERFACE, Tu, asCSourceRange(), "ILeft");
			const asASTDeclId Right = Context.CreateDecl(
				asAST_DECL_INTERFACE, Tu, asCSourceRange(), "IRight");
			Context.AddBase(Left, Root);
			Context.AddBase(Right, Root);
			const asASTDeclId Owner = Context.CreateDecl(
				asAST_DECL_CLASS, Tu, asCSourceRange(), "DiamondProbe");
			Context.SetStableKey(Owner, "DiamondProbe");
			Context.AddBase(Owner, Left);
			Context.AddBase(Owner, Right);
			const asASTDeclId Implementation = AddRelationMethod(
				Context, Owner, "DiamondProbe::Probe(int)", IntType, IntType);
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Implementation,
					Requirement))));
			ASSERT_THAT(AreEqual(
				(int)asAST_VERIFY_OK,
				Context.Seal(),
				TEXT("a legal interface diamond must retain one exact requirement")));
		}

		{
			asCASTContext Context;
			const FMethodRelationVerifierFixture Fixture =
				BuildMethodRelationVerifierFixture(Context);
			ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
				Fixture.Owner,
				asSASTMethodRelation(
					asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
					Fixture.BaseImplementation,
					Fixture.Requirement))));
			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(
				(int)asAST_VERIFY_OK,
				Context.Seal(),
				TEXT("one exact complete relation must remain admissible")));
		}
	}

	TEST_METHOD(TraversalNamesMethodRelationImplementationAndRequirementEdges)
	{
		asCASTContext Context;
		const FMethodRelationVerifierFixture Fixture =
			BuildMethodRelationVerifierFixture(Context);
		ASSERT_THAT(AreEqual(0, Context.AddMethodRelation(
			Fixture.Owner,
			asSASTMethodRelation(
				asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
				Fixture.BaseImplementation,
				Fixture.Requirement))));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		const asSASTNodeRef OwnerNode = asSASTNodeRef::FromDecl(Fixture.Owner);
		const asEASTEdgeRole ExpectedImplementationRole =
			asAST_EDGE_DECL_METHOD_RELATION_IMPLEMENTATION;
		const asEASTEdgeRole ExpectedRequirementRole =
			asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT;
		bool FoundImplementation = false;
		bool FoundRequirement = false;
		for( asUINT EdgeIndex = 0;
			EdgeIndex < asCASTGetEdgeCount(Context, OwnerNode); ++EdgeIndex )
		{
			asSASTTraversalEdge Edge;
			ASSERT_THAT(IsTrue(asCASTGetEdge(
				Context, OwnerNode, EdgeIndex, Edge)));
			if( Edge.role == ExpectedImplementationRole
				&& Edge.index == 0
				&& Edge.target == asSASTNodeRef::FromDecl(
					Fixture.BaseImplementation) )
			{
				ASSERT_THAT(AreEqual(
					(int)asAST_EDGE_REFERENCE,
					(int)Edge.edgeClass,
					TEXT("method implementation is a non-owning reference edge")));
				FoundImplementation = true;
			}
			if( Edge.role == ExpectedRequirementRole
				&& Edge.index == 0
				&& Edge.target == asSASTNodeRef::FromDecl(
					Fixture.Requirement) )
			{
				ASSERT_THAT(AreEqual(
					(int)asAST_EDGE_REFERENCE,
					(int)Edge.edgeClass,
					TEXT("method requirement is a non-owning reference edge")));
				FoundRequirement = true;
			}
		}
		ASSERT_THAT(IsTrue(FoundImplementation,
			TEXT("traversal must name implementation[index] as a reference edge")));
		ASSERT_THAT(IsTrue(FoundRequirement,
			TEXT("traversal must name requirement[index] as a reference edge")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString DumpText =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString RelationText = FString::Printf(
			TEXT("interface-implementation:D%u>D%u"),
			Fixture.BaseImplementation.value,
			Fixture.Requirement.value);
		ASSERT_THAT(IsTrue(DumpText.Contains(
			TEXT("method-relations=") + RelationText),
			TEXT("deterministic dump must include exact relation kind and endpoints")));

		asSASTStructuredDumpOptions TreeOptions;
		TreeOptions.format = asAST_STRUCTURED_DUMP_TREE;
		asCString TreeDump;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_OK,
			asCASTStructuredDump(Context, TreeOptions, TreeDump)));
		const FString TreeText =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(TreeDump);
		ASSERT_THAT(IsTrue(TreeText.Contains(
			TEXT("methodRelations=[") + RelationText + TEXT("]"))));
		ASSERT_THAT(IsTrue(TreeText.Contains(
			TEXT("edge role=decl-method-relation-implementation class=reference index=0"))));
		ASSERT_THAT(IsTrue(TreeText.Contains(
			TEXT("edge role=decl-method-relation-requirement class=reference index=0"))));

		asSASTStructuredDumpOptions JsonOptions;
		JsonOptions.format = asAST_STRUCTURED_DUMP_JSON;
		asCString JsonDump;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_OK,
			asCASTStructuredDump(Context, JsonOptions, JsonDump)));
		const FString JsonText =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(JsonDump);
		const FString JsonRelation = FString::Printf(
			TEXT("\"methodRelations\":[{\"kind\":\"interface-implementation\",\"implementation\":\"D%u\",\"requirement\":\"D%u\"}]"),
			Fixture.BaseImplementation.value,
			Fixture.Requirement.value);
		ASSERT_THAT(IsTrue(JsonText.Contains(JsonRelation)));
		ASSERT_THAT(IsTrue(JsonText.Contains(
			TEXT("\"role\":\"decl-method-relation-implementation\",\"edgeClass\":\"reference\",\"index\":0"))));
		ASSERT_THAT(IsTrue(JsonText.Contains(
			TEXT("\"role\":\"decl-method-relation-requirement\",\"edgeClass\":\"reference\",\"index\":0"))));
	}

	TEST_METHOD(ExposesNamedAuthenticatedCompatibilityCleanupActions)
	{
		asCASTContext Context;
		const FLocalLifetimeViewFixture Fixture =
			BuildLocalLifetimeViewFixture(Context);
		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF,
			Context.GetDecl(Fixture.FirstLocal)->type,
			asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Target, Fixture.FirstLocal)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP,
			Context.GetDecl(Fixture.FirstLocal)->type,
			asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-exit")));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Cleanup, Fixture.Destructor)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		const asASTStmtId CleanupStmt = Context.CreateStmt(
			asAST_STMT_EXPR,
			Context.GetDecl(Fixture.FirstLocal)->parent,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(CleanupStmt, Cleanup)));
		ASSERT_THAT(AreEqual(0,
			Context.AddStmtChild(Fixture.Region, CleanupStmt)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
			MakeLocalLifetimeRecord(Fixture, true, 0))));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asCASTLifetimeView View;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, View)));
		const asSASTLifetimeCleanupBinding* const Binding =
			View.FindCleanupBinding(CleanupStmt);
		ASSERT_THAT(IsNotNull(Binding,
			TEXT("a compatibility statement must resolve through a named authenticated binding")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Binding->recordIndex));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_LIFETIME_ACTION_DESTROY_VALUE),
			static_cast<int32>(Binding->actionKind)));
		ASSERT_THAT(IsTrue(Binding->subject ==
			asSASTLifetimeNodeId::FromDecl(Fixture.FirstLocal)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_LIFETIME_COMPAT_NORMAL_EXIT),
			static_cast<int32>(Binding->role)));
	}

	TEST_METHOD(RejectsLifetimeProtocolCompatibilityEncodingDisagreement)
	{
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture =
				BuildLocalLifetimeViewFixture(Context);
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
				MakeLocalLifetimeRecord(Fixture, true, 0))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("a protocol action without its compatibility statement must fail closed")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture =
				BuildLocalLifetimeViewFixture(Context);
			AppendNormalScopeExit(Context,
				MakeLocalLifetimeRecord(Fixture, true, 0));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("a compatibility statement without its protocol action must fail closed")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture =
				BuildLocalLifetimeViewFixture(Context);
			const asSASTLifetimeRecord Record =
				MakeLocalLifetimeRecord(Fixture, true, 0);
			AppendNormalScopeExit(Context, Record);
			AppendNormalScopeExit(Context, Record);
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("duplicate compatibility statements for one protocol action must fail closed")));
		}
	}

	TEST_METHOD(BuildsDeterministicLifetimeViewWithCommittedLiveSets)
	{
		asCASTContext Context;
		const FLocalLifetimeViewFixture Fixture =
			BuildLocalLifetimeViewFixture(Context);
		const asSASTLifetimeRecord First =
			MakeLocalLifetimeRecord(Fixture, true, 0);
		const asSASTLifetimeRecord Second =
			MakeLocalLifetimeRecord(Fixture, false, 1);
		AppendNormalScopeExit(Context, Second);
		AppendNormalScopeExit(Context, First);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(First)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Second)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("the exact local lifetime fixture must reach Frozen publication")));

		asCASTLifetimeView FirstView;
		asCASTLifetimeView SecondView;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, FirstView)));
		asCString DiagnosticDump;
		asCASTDump(Context, DiagnosticDump);
		ASSERT_THAT(IsTrue(DiagnosticDump.GetLength() != 0,
			TEXT("the compatibility dump is only a diagnostic side effect")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, SecondView)));

		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), FirstView.GetRecordCount()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), FirstView.GetScopeEdgeCount()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), FirstView.GetCommitPointCount()));
		ASSERT_THAT(AreEqual(
			FirstView.GetStructuralDigest(),
			SecondView.GetStructuralDigest(),
			TEXT("rebuilding from one Frozen snapshot must be deterministic")));
		ASSERT_THAT(IsTrue(
			FirstView.GetStructuralDigest() != Context.GetLifetimeProtocolHash(),
			TEXT("the derived-view digest must have its own typed domain")));
		for( asUINT Index = 0; Index < FirstView.GetRecordCount(); ++Index )
		{
			ASSERT_THAT(IsTrue(
				*FirstView.GetRecord(Index) == *SecondView.GetRecord(Index),
				TEXT("typed derived records must reconstruct identically")));
		}

		const asSASTLifetimeCommitPoint* FirstCommit =
			FirstView.GetCommitPoint(0);
		const asSASTLifetimeCommitPoint* SecondCommit =
			FirstView.GetCommitPoint(1);
		ASSERT_THAT(IsNotNull(FirstCommit));
		ASSERT_THAT(IsNotNull(SecondCommit));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			FirstCommit->liveBefore.GetLength(),
			TEXT("the first local is not live while its initializer is running")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			FirstCommit->liveAfter.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			SecondCommit->liveBefore.GetLength(),
			TEXT("only the first committed local is live before Second commits")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			SecondCommit->liveBefore[0]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			SecondCommit->liveAfter.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			SecondCommit->reverseLiveCleanup[0],
			TEXT("normal cleanup starts at the newest committed local")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			SecondCommit->reverseLiveCleanup[1]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			SecondCommit->abortCleanup.GetLength(),
			TEXT("initializer failure must exclude the failed current object")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			SecondCommit->abortCleanup[0]));
	}

	TEST_METHOD(BuildsDeterministicConstructorCommittedPrefixPlan)
	{
		asCASTContext Context;
		const FConstructorLifetimeViewFixture Fixture =
			BuildConstructorLifetimeViewFixture(Context);
		AddCompleteNonDelegatingConstructorLifetimePlan(Context, Fixture);
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("a complete base/member constructor plan must authenticate")));

		asCASTLifetimeView FirstView;
		asCASTLifetimeView SecondView;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, FirstView)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, SecondView)));
		ASSERT_THAT(AreEqual(
			FirstView.GetStructuralDigest(),
			SecondView.GetStructuralDigest(),
			TEXT("constructor plans must reconstruct deterministically")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1),
			FirstView.GetConstructionPlanCount()));

		const asSASTLifetimeConstructionPlan* const Plan =
			FirstView.FindConstructionPlan(Fixture.DerivedConstructor);
		ASSERT_THAT(IsNotNull(Plan));
		if( Plan == nullptr )
		{
			return;
		}
		ASSERT_THAT(IsTrue(Plan->constructor == Fixture.DerivedConstructor));
		ASSERT_THAT(IsTrue(Plan->ownerClass == Fixture.DerivedClass));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(4), Plan->recordIndices.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(3),
			Plan->completeRecordIndex));

		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(4), FirstView.GetCommitPointCount()));
		for( asUINT Step = 0; Step < 4; ++Step )
		{
			const asSASTLifetimeCommitPoint* const Commit =
				FirstView.GetCommitPoint(Step);
			ASSERT_THAT(IsNotNull(Commit));
			if( Commit == nullptr )
			{
				return;
			}
			ASSERT_THAT(AreEqual(Step, Commit->recordIndex));
			ASSERT_THAT(AreEqual(Step, Commit->liveBefore.GetLength()));
			ASSERT_THAT(AreEqual(Step + 1, Commit->liveAfter.GetLength()));
			ASSERT_THAT(AreEqual(Step, Commit->abortCleanup.GetLength()));
			ASSERT_THAT(AreEqual(
				Step + 1, Commit->reverseLiveCleanup.GetLength()));
			for( asUINT Prefix = 0; Prefix < Step; ++Prefix )
			{
				ASSERT_THAT(AreEqual(
					Prefix, Commit->liveBefore[Prefix]));
				ASSERT_THAT(AreEqual(
					Step - Prefix - 1, Commit->abortCleanup[Prefix]));
			}
		}
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			FirstView.GetCleanupBindingCount(),
			TEXT("constructor protocol must not manufacture lexical compatibility cleanup")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			FirstView.GetExitPlanCount(),
			TEXT("constructor protocol must not become a normal/transfer exit plan")));
	}

	TEST_METHOD(AcceptsDelegatingTargetSuccessAsCompleteObjectCommit)
	{
		asCASTContext Context;
		const FConstructorLifetimeViewFixture Fixture =
			BuildConstructorLifetimeViewFixture(Context);
		const asCQualType VoidType = Context.InternPrimitive(ttVoid);
		const asCQualType DerivedType = Context.GetDecl(
			Fixture.DerivedClass)->type;
		const asASTDeclId DelegatingConstructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, Fixture.DerivedClass,
			asCSourceRange(), "FDerived");
		Context.SetDeclType(DelegatingConstructor, VoidType);
		const asASTStmtId DelegatingBody = Context.CreateStmt(
			asAST_STMT_BLOCK, DelegatingConstructor, asCSourceRange());
		Context.SetBody(DelegatingConstructor, DelegatingBody);
		const asASTExprId ThisExpr = Context.CreateExpr(
			asAST_EXPR_THIS, DerivedType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(ThisExpr, "this");
		const asASTExprId DelegatingCall = Context.CreateExpr(
			asAST_EXPR_CALL, VoidType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetResolvedDecl(
			DelegatingCall, Fixture.DerivedConstructor);
		Context.SetExprReceiver(DelegatingCall, ThisExpr);
		Context.AddExprChild(DelegatingCall, ThisExpr);
		Context.AddDeclInit(DelegatingConstructor, DelegatingCall);

		asSASTLifetimeRecord Record;
		Record.subjectKind =
			asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT;
		Record.subject = asSASTLifetimeNodeId::FromDecl(
			Fixture.DerivedClass);
		Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
		Record.actionTarget = asSASTLifetimeNodeId::FromDecl(
			Fixture.DerivedDestructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromExpr(
			DelegatingCall);
		Record.semanticRegion = asSASTLifetimeNodeId::FromDecl(
			DelegatingConstructor);
		Record.phase = asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING;
		Record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
		Record.constructionStep = 0;
		Record.order = 0;
		Record.completeObjectCommit = true;
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("delegating target success is the complete-object commit")));

		asCASTLifetimeView View;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, View)));
		const asSASTLifetimeConstructionPlan* const Plan =
			View.FindConstructionPlan(DelegatingConstructor);
		ASSERT_THAT(IsNotNull(Plan));
		if( Plan == nullptr )
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1), Plan->recordIndices.GetLength()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), Plan->completeRecordIndex));
		const asSASTLifetimeCommitPoint* const Commit =
			View.GetCommitPoint(0);
		ASSERT_THAT(IsNotNull(Commit));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Commit->abortCleanup.GetLength(),
			TEXT("delegating target failure cannot destroy an incomplete object")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			Commit->reverseLiveCleanup.GetLength(),
			TEXT("delegating body failure must reach the complete destructor")));
	}

	TEST_METHOD(RejectsForgedConstructorOwnerStepActivationAndCompletion)
	{
		{
			asCASTContext Context;
			const FConstructorLifetimeViewFixture Fixture =
				BuildConstructorLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeConstructorLifetimeRecord(
				Fixture, asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE, 0, 0);
			Record.semanticRegion = asSASTLifetimeNodeId::FromDecl(
				Fixture.BaseConstructor);
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("a base step cannot be attached to a foreign constructor owner")));
		}
		{
			asCASTContext Context;
			const FConstructorLifetimeViewFixture Fixture =
				BuildConstructorLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeConstructorLifetimeRecord(
				Fixture, asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER, 1, 0);
			Record.constructionStep = 0;
			Record.activationPoint = asSASTLifetimeNodeId::FromExpr(
				Fixture.SecondAssignment);
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("a member step must commit at that exact field initializer")));
		}
		{
			asCASTContext Context;
			const FConstructorLifetimeViewFixture Fixture =
				BuildConstructorLifetimeViewFixture(Context);
			Context.AddLifetimeRecord(MakeConstructorLifetimeRecord(
				Fixture, asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE, 0, 0));
			Context.AddLifetimeRecord(MakeConstructorLifetimeRecord(
				Fixture, asAST_LIFETIME_PHASE_COMPLETE_OBJECT, 2, 1));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("construction steps must be gapless and complete must be final")));
		}
		{
			asCASTContext Context;
			const FConstructorLifetimeViewFixture Fixture =
				BuildConstructorLifetimeViewFixture(Context);
			asSASTLifetimeRecord Complete = MakeConstructorLifetimeRecord(
				Fixture, asAST_LIFETIME_PHASE_COMPLETE_OBJECT, 0, 0);
			Complete.completeObjectCommit = false;
			Context.AddLifetimeRecord(Complete);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("a complete-object phase must carry the distinct complete commit")));
		}
	}

	TEST_METHOD(BuildsDeterministicAggregateCommittedPrefixPlan)
	{
		asCASTContext Context;
		const FAggregateLifetimeViewFixture Fixture =
			BuildAggregateLifetimeViewFixture(Context);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
			MakeAggregateLifetimeRecord(
				Fixture.FirstInnerAggregate,
				Fixture.ElementDestructor,
				Fixture.Function,
				0))));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
			MakeAggregateLifetimeRecord(
				Fixture.SecondInnerAggregate,
				Fixture.ElementDestructor,
				Fixture.Function,
				1))));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
			MakeAggregateLifetimeRecord(
				Fixture.OuterAggregate,
				Fixture.InnerArrayDestructor,
				Fixture.Function,
				2))));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("nested aggregate progress must authenticate")));

		asCASTLifetimeView FirstView;
		asCASTLifetimeView SecondView;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, FirstView)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, SecondView)));
		ASSERT_THAT(AreEqual(
			FirstView.GetStructuralDigest(),
			SecondView.GetStructuralDigest(),
			TEXT("aggregate plans must reconstruct deterministically")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(3),
			FirstView.GetAggregatePlanCount()));

		const asSASTLifetimeAggregatePlan* const InnerPlan =
			FirstView.FindAggregatePlan(Fixture.FirstInnerAggregate);
		const asSASTLifetimeAggregatePlan* const OuterPlan =
			FirstView.FindAggregatePlan(Fixture.OuterAggregate);
		ASSERT_THAT(IsNotNull(InnerPlan));
		ASSERT_THAT(IsNotNull(OuterPlan));
		if( InnerPlan == nullptr || OuterPlan == nullptr )
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			InnerPlan->aggregate == Fixture.FirstInnerAggregate));
		ASSERT_THAT(IsTrue(
			InnerPlan->parentAggregate == Fixture.OuterAggregate));
		ASSERT_THAT(IsTrue(
			OuterPlan->parentAggregate == asASTExprId()));
		ASSERT_THAT(IsTrue(InnerPlan->semanticOwner == Fixture.Function));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_LIFETIME_ACTION_DESTROY_ELEMENT),
			static_cast<int32>(InnerPlan->actionKind)));
		ASSERT_THAT(IsTrue(InnerPlan->actionTarget
			== asSASTLifetimeNodeId::FromDecl(Fixture.ElementDestructor)));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(3),
			InnerPlan->elementCommits.GetLength()));

		asCArray<asSASTLifetimeAggregateCleanupAction> Cleanup;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			FirstView.DeriveAggregateAbortCleanup(
				Fixture.FirstInnerAggregate, 0, Cleanup)));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), Cleanup.GetLength(),
			TEXT("zero committed elements require no cleanup")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			FirstView.DeriveAggregateAbortCleanup(
				Fixture.FirstInnerAggregate, 1, Cleanup)));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1), Cleanup.GetLength()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), Cleanup[0].elementOrdinal));
		ASSERT_THAT(IsTrue(Cleanup[0].element
			== Fixture.FirstInnerElements[0]));
		ASSERT_THAT(IsTrue(Cleanup[0].actionTarget
			== asSASTLifetimeNodeId::FromDecl(Fixture.ElementDestructor)));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			FirstView.DeriveAggregateAbortCleanup(
				Fixture.FirstInnerAggregate, 2, Cleanup),
			TEXT("failure in element two cleans only committed elements zero and one")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(2), Cleanup.GetLength()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1), Cleanup[0].elementOrdinal));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), Cleanup[1].elementOrdinal));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			FirstView.DeriveAggregateAbortCleanup(
				Fixture.FirstInnerAggregate, 3, Cleanup)));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(3), Cleanup.GetLength()));
		for( asUINT CleanupIndex = 0; CleanupIndex < 3; ++CleanupIndex )
		{
			ASSERT_THAT(AreEqual(
				2 - CleanupIndex, Cleanup[CleanupIndex].elementOrdinal));
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
			FirstView.DeriveAggregateAbortCleanup(
				Fixture.FirstInnerAggregate, 4, Cleanup),
			TEXT("a cursor beyond the authenticated element count must fail closed")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), Cleanup.GetLength(),
			TEXT("a rejected cursor must not leak stale cleanup actions")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			FirstView.DeriveAggregateAbortCleanup(
				Fixture.OuterAggregate, 1, Cleanup)));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1), Cleanup.GetLength()));
		ASSERT_THAT(IsTrue(Cleanup[0].element
			== Fixture.FirstInnerAggregate));
		ASSERT_THAT(IsTrue(Cleanup[0].actionTarget
			== asSASTLifetimeNodeId::FromDecl(
				Fixture.InnerArrayDestructor),
			TEXT("the parent cursor advances only after the complete inner aggregate commits")));
	}

	TEST_METHOD(RejectsForgedAggregateSubjectActionOwnerAndProgress)
	{
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
				MakeAggregateLifetimeRecord(
					Fixture.FirstInnerAggregate,
					Fixture.ElementDestructor,
					Fixture.Function,
					0))));
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
				MakeAggregateLifetimeRecord(
					Fixture.OuterAggregate,
					Fixture.InnerArrayDestructor,
					Fixture.Function,
					1))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("every nested list-pattern aggregate needs an independent committed-prefix plan")));
		}
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeAggregateLifetimeRecord(
				Fixture.FirstInnerAggregate,
				Fixture.ElementDestructor,
				Fixture.Function,
				0);
			Record.activationPoint = asSASTLifetimeNodeId::FromExpr(
				Fixture.SecondInnerAggregate);
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("aggregate progress identity must be the subject expression")));
		}
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
				MakeAggregateLifetimeRecord(
					Fixture.FirstInnerAggregate,
					Fixture.InnerArrayDestructor,
					Fixture.Function,
					0))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("aggregate cleanup must name the exact element destructor")));
		}
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			const asCDecl* const Function = Context.GetDecl(Fixture.Function);
			ASSERT_THAT(IsNotNull(Function));
			if( Function == nullptr ) return;
			const asASTDeclId ForeignFunction = Context.CreateDecl(
				asAST_DECL_FUNCTION, Function->parent,
				asCSourceRange(), "ForeignBuildArrays");
			ASSERT_THAT(AreEqual(
				0, Context.SetDeclType(ForeignFunction, Function->type)));
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
				MakeAggregateLifetimeRecord(
					Fixture.FirstInnerAggregate,
					Fixture.ElementDestructor,
					ForeignFunction,
					0))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("the semantic owner must own the aggregate expression")));
		}
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			ASSERT_THAT(AreEqual(
				0, Context.SetDeclTraits(
					Fixture.InnerListFactory, asAST_TRAIT_NONE)));
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(
				MakeAggregateLifetimeRecord(
					Fixture.FirstInnerAggregate,
					Fixture.ElementDestructor,
					Fixture.Function,
					0))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("aggregate progress requires an authenticated list factory")));
		}
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeAggregateLifetimeRecord(
				Fixture.FirstInnerAggregate,
				Fixture.ElementDestructor,
				Fixture.Function,
				0);
			Record.supportedExitMask = asAST_LIFETIME_EXIT_NORMAL;
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("aggregate committed-prefix cleanup is an exceptional-abort fact")));
		}
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeAggregateLifetimeRecord(
				Fixture.FirstInnerAggregate,
				Fixture.ElementDestructor,
				Fixture.Function,
				0);
			Record.constructionStep = 1;
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("the dynamic committed cursor must not be persisted as a construction step")));
		}
		{
			asCASTContext Context;
			const FAggregateLifetimeViewFixture Fixture =
				BuildAggregateLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeAggregateLifetimeRecord(
				Fixture.FirstInnerAggregate,
				Fixture.ElementDestructor,
				Fixture.Function,
				0);
			Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
			ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
				Context.Seal(),
				TEXT("an aggregate record must use the exact element cleanup action")));
		}
	}

	TEST_METHOD(DerivesProtocolOnlyNormalExitPlanInReverseCommitOrder)
	{
		asCASTContext Context;
		const FLocalLifetimeViewFixture Fixture =
			BuildLocalLifetimeViewFixture(Context);
		const asSASTLifetimeRecord First =
			MakeLocalLifetimeRecord(Fixture, true, 0);
		const asSASTLifetimeRecord Second =
			MakeLocalLifetimeRecord(Fixture, false, 1);
		AppendNormalScopeExit(Context, Second);
		AppendNormalScopeExit(Context, First);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(First)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Second)));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asCASTLifetimeView FirstView;
		asCASTLifetimeView SecondView;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, FirstView)));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, SecondView)));

		const asSASTLifetimeExitPlan* const FirstPlan =
			FirstView.FindExitPlan(
				Fixture.Region, asAST_LIFETIME_EXIT_PLAN_NORMAL);
		const asSASTLifetimeExitPlan* const SecondPlan =
			SecondView.FindExitPlan(
				Fixture.Region, asAST_LIFETIME_EXIT_PLAN_NORMAL);
		ASSERT_THAT(IsNotNull(FirstPlan,
			TEXT("the shared view must derive normal cleanup from protocol records")));
		ASSERT_THAT(IsNotNull(SecondPlan));
		ASSERT_THAT(IsTrue(*FirstPlan == *SecondPlan,
			TEXT("one Frozen protocol must reconstruct the same exit plan")));
		ASSERT_THAT(IsTrue(FirstPlan->structuralOwner == Fixture.Region));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_LIFETIME_EXIT_PLAN_NORMAL),
			static_cast<int32>(FirstPlan->kind)));
		ASSERT_THAT(AreEqual(
			static_cast<asDWORD>(asAST_LIFETIME_EXIT_NORMAL),
			FirstPlan->exitMask));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(2), FirstPlan->recordIndices.GetLength()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1), FirstPlan->recordIndices[0],
			TEXT("normal cleanup starts with the latest committed record")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), FirstPlan->recordIndices[1]));
	}

	TEST_METHOD(DerivesProtocolOnlyTransferExitPlanForCommittedLocals)
	{
		asCASTContext Context;
		const FLocalLifetimeViewFixture Fixture =
			BuildLocalLifetimeViewFixture(Context);
		const asSASTLifetimeRecord First =
			MakeLocalLifetimeRecord(Fixture, true, 0);
		const asSASTLifetimeRecord Second =
			MakeLocalLifetimeRecord(Fixture, false, 1);
		const asASTStmtId Return = Context.CreateStmt(
			asAST_STMT_RETURN,
			Context.GetDecl(Fixture.FirstLocal)->parent,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Fixture.Region, Return)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Return,
			CreateScopeExitCompatibilityStatement(Context, Second))));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Return,
			CreateScopeExitCompatibilityStatement(Context, First))));
		AppendNormalScopeExit(Context, Second);
		AppendNormalScopeExit(Context, First);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(First)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Second)));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asCASTLifetimeView View;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, View)));
		const asSASTLifetimeExitPlan* const Plan =
			View.FindExitPlan(Return, asAST_LIFETIME_EXIT_PLAN_TRANSFER);
		ASSERT_THAT(IsNotNull(Plan,
			TEXT("return cleanup must be derived from live protocol records")));
		ASSERT_THAT(IsTrue(Plan->structuralOwner == Return));
		ASSERT_THAT(AreEqual(
			static_cast<asDWORD>(asAST_LIFETIME_EXIT_RETURN),
			Plan->exitMask));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(2), Plan->recordIndices.GetLength()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1), Plan->recordIndices[0]));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), Plan->recordIndices[1]));
	}

	TEST_METHOD(DerivesProtocolOnlyForeachIteratorExitPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ForeachExitPlan");
		const asCQualType IteratorType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FIterator", 0);
		const asASTDeclId IteratorClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FIterator");
		ASSERT_THAT(AreEqual(0,
			Context.SetStableKey(IteratorClass, "FIterator")));
		const asASTDeclId Destructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, IteratorClass,
			asCSourceRange(), "FIterator");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId FunctionBody = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, FunctionBody)));

		const asASTDeclId Iterator = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Iterator");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Iterator, IteratorType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Iterator, asAST_TRAIT_GENERATED)));
		const asASTStmtId Initializer = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		const asASTStmtId IteratorDecl = Context.CreateStmt(
			asAST_STMT_DECL, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtDecl(IteratorDecl, Iterator)));
		ASSERT_THAT(AreEqual(0,
			Context.AddStmtChild(Initializer, IteratorDecl)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, IteratorType,
			asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Iterator)));
		const asASTExprId InitializerValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IteratorType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(InitializerValue, "0")));
		const asASTExprId Assignment = Context.CreateExpr(
			asAST_EXPR_ASSIGN, IteratorType,
			asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Assignment, "=")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Assignment, Target)));
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(Assignment, InitializerValue)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Iterator, Assignment)));
		const asASTStmtId Activation = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Activation, Assignment)));
		ASSERT_THAT(AreEqual(0,
			Context.AddStmtChild(Initializer, Activation)));

		const asASTStmtId Body = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		const asASTExprId IncrementValue = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, Context.InternPrimitive(ttInt),
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(IncrementValue, "0")));
		const asASTStmtId Increment = Context.CreateStmt(
			asAST_STMT_EXPR, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetStmtExpr(Increment, IncrementValue)));

		asSASTLifetimeRecord Record;
		Record.subjectKind = asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR;
		Record.subject = asSASTLifetimeNodeId::FromDecl(Iterator);
		Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
		Record.actionTarget = asSASTLifetimeNodeId::FromDecl(Destructor);
		Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Assignment);
		Record.phase = asAST_LIFETIME_PHASE_LOOP_ITERATION;
		Record.supportedExitMask = asAST_LIFETIME_EXIT_NORMAL
			| asAST_LIFETIME_EXIT_RETURN
			| asAST_LIFETIME_EXIT_BREAK
			| asAST_LIFETIME_EXIT_EXCEPTION;
		Record.order = 0;

		const asASTStmtId Cleanup =
			CreateScopeExitCompatibilityStatement(Context, Record);
		const asASTStmtId Foreach = Context.CreateStmt(
			asAST_STMT_FOREACH, Function, asCSourceRange());
		const asASTExprId Condition = Context.CreateExpr(
			asAST_EXPR_BOOL_LITERAL, Context.InternPrimitive(ttBool),
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Condition, "true")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Foreach, Condition)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Foreach, Initializer)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Foreach, Body)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Foreach, Increment)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Foreach, Cleanup)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(FunctionBody, Foreach)));
		Record.semanticRegion = asSASTLifetimeNodeId::FromStmt(Foreach);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(Record)));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asCASTLifetimeView View;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, View)));
		const asSASTLifetimeExitPlan* const Plan =
			View.FindExitPlan(Foreach, asAST_LIFETIME_EXIT_PLAN_FOREACH);
		ASSERT_THAT(IsNotNull(Plan,
			TEXT("foreach cleanup must route through its exact protocol record")));
		ASSERT_THAT(IsTrue(Plan->structuralOwner == Foreach));
		ASSERT_THAT(AreEqual(Record.supportedExitMask, Plan->exitMask));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1), Plan->recordIndices.GetLength()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0), Plan->recordIndices[0]));
	}

	TEST_METHOD(DerivesNestedScopeEdgesAndOuterLiveSet)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("NestedLifetimeView");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		const asASTDeclId ValueClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FValue");
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(ValueClass, "FValue")));
		const asASTDeclId Destructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, ValueClass, asCSourceRange(), "FValue");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId OuterRegion = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Function, OuterRegion)));

		auto AddLocal = [&](const asASTStmtId Region, const char* Name,
			const char* Literal, asUINT Order) -> asSASTLifetimeRecord
		{
			const asASTDeclId Local = Context.CreateDecl(
				asAST_DECL_VAR, Function, asCSourceRange(), Name);
			Context.SetDeclType(Local, ValueType);
			const asASTStmtId DeclStmt = Context.CreateStmt(
				asAST_STMT_DECL, Function, asCSourceRange());
			Context.SetStmtDecl(DeclStmt, Local);
			Context.AddStmtChild(Region, DeclStmt);

			const asASTExprId Target = Context.CreateExpr(
				asAST_EXPR_DECL_REF, ValueType,
				asAST_VALUE_LVALUE, asCSourceRange());
			Context.SetResolvedDecl(Target, Local);
			const asASTExprId Initializer = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, ValueType,
				asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Initializer, Literal);
			const asASTExprId Assignment = Context.CreateExpr(
				asAST_EXPR_ASSIGN, ValueType,
				asAST_VALUE_LVALUE, asCSourceRange());
			Context.SetLiteral(Assignment, "=");
			Context.AddExprChild(Assignment, Target);
			Context.AddExprChild(Assignment, Initializer);
			Context.AddDeclInit(Local, Assignment);
			const asASTStmtId ExprStmt = Context.CreateStmt(
				asAST_STMT_EXPR, Function, asCSourceRange());
			Context.SetStmtExpr(ExprStmt, Assignment);
			Context.AddStmtChild(Region, ExprStmt);

			asSASTLifetimeRecord Record;
			Record.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
			Record.subject = asSASTLifetimeNodeId::FromDecl(Local);
			Record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
			Record.actionTarget = asSASTLifetimeNodeId::FromDecl(Destructor);
			Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Assignment);
			Record.semanticRegion = asSASTLifetimeNodeId::FromStmt(Region);
			Record.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
			Record.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
			Record.order = Order;
			return Record;
		};

		const asSASTLifetimeRecord OuterRecord =
			AddLocal(OuterRegion, "Outer", "1", 0);
		const asASTStmtId InnerRegion = Context.CreateStmt(
			asAST_STMT_BLOCK, Function, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(OuterRegion, InnerRegion)));
		const asSASTLifetimeRecord InnerRecord =
			AddLocal(InnerRegion, "Inner", "2", 1);
		AppendNormalScopeExit(Context, InnerRecord);
		AppendNormalScopeExit(Context, OuterRecord);
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(OuterRecord)));
		ASSERT_THAT(AreEqual(0, Context.AddLifetimeRecord(InnerRecord)));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asCASTLifetimeView View;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			asCASTBuildLifetimeView(Context, View)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), View.GetScopeEdgeCount()));
		const asSASTLifetimeScopeEdge* OuterEdge = nullptr;
		const asSASTLifetimeScopeEdge* InnerEdge = nullptr;
		for( asUINT Index = 0; Index < View.GetScopeEdgeCount(); ++Index )
		{
			const asSASTLifetimeScopeEdge* Edge = View.GetScopeEdge(Index);
			if( Edge->region == asSASTLifetimeNodeId::FromStmt(OuterRegion) )
				OuterEdge = Edge;
			if( Edge->region == asSASTLifetimeNodeId::FromStmt(InnerRegion) )
				InnerEdge = Edge;
		}
		ASSERT_THAT(IsNotNull(OuterEdge));
		ASSERT_THAT(IsNotNull(InnerEdge));
		ASSERT_THAT(IsFalse(OuterEdge->parentRegion.IsValid()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), OuterEdge->depth));
		ASSERT_THAT(IsTrue(
			InnerEdge->parentRegion == asSASTLifetimeNodeId::FromStmt(OuterRegion)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), InnerEdge->depth));

		const asSASTLifetimeCommitPoint* InnerCommit = View.GetCommitPoint(1);
		ASSERT_THAT(IsNotNull(InnerCommit));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			InnerCommit->liveBefore.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), InnerCommit->liveBefore[0],
			TEXT("the committed outer local must be live before Inner initializes")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			InnerCommit->liveAfter.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			InnerCommit->reverseLiveCleanup[0]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			InnerCommit->reverseLiveCleanup[1]));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
			InnerCommit->abortCleanup.GetLength()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			InnerCommit->abortCleanup[0],
			TEXT("failed Inner initialization cleans only the committed outer local")));
	}

	TEST_METHOD(RejectsForgedLifetimeSubjectActionActivationOrderAndPhase)
	{
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.subject = asSASTLifetimeNodeId::FromDecl(Fixture.Destructor);
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a destructor declaration cannot masquerade as a local subject")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.actionKind = asAST_LIFETIME_ACTION_RELEASE_REFERENCE;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a value local cannot replace its exact destructor with release")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.activationPoint = asSASTLifetimeNodeId::FromExpr(Fixture.FirstInitializerValue);
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("cleanup may not activate while the current initializer can still fail")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			Context.AddLifetimeRecord(MakeLocalLifetimeRecord(Fixture, false, 0));
			Context.AddLifetimeRecord(MakeLocalLifetimeRecord(Fixture, true, 1));
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("storage indices cannot forge source commit order inside one region")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.phase = asAST_LIFETIME_PHASE_FULL_EXPRESSION;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a lexical local may not be reclassified as a temporary phase")));
		}
	}

	TEST_METHOD(RejectsMissingDuplicateForeignAndEarlyLifetimeActions)
	{
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.actionTarget = asSASTLifetimeNodeId();
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("a committed subject requires one exact cleanup action")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			Context.AddLifetimeRecord(MakeLocalLifetimeRecord(Fixture, true, 0));
			asSASTLifetimeRecord Duplicate = MakeLocalLifetimeRecord(Fixture, true, 1);
			Context.AddLifetimeRecord(Duplicate);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("one commit cannot publish the same cleanup action twice")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.subject.snapshotOwner = 0xA57u;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("snapshot-local lifetime facts reject public or foreign owner tokens")));
		}
		{
			asCASTContext Context;
			const FLocalLifetimeViewFixture Fixture = BuildLocalLifetimeViewFixture(Context);
			asSASTLifetimeRecord Record = MakeLocalLifetimeRecord(Fixture, true, 0);
			Record.phase = asAST_LIFETIME_PHASE_COMPLETE_OBJECT;
			Record.completeObjectCommit = true;
			Context.AddLifetimeRecord(Record);
			ASSERT_THAT(AreEqual(static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL), Context.Seal(),
				TEXT("complete-object destruction is unreachable before a distinct complete commit")));
		}
	}

	TEST_METHOD(RejectsWrongLifetimeProtocolRevisionBeforePlanning)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("WrongLifetimeRevision");
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.SetLifetimeProtocolRevision(
				asAST_LIFETIME_PROTOCOL_REVISION + 1),
			TEXT("the malformed-protocol fixture must be authored while Building")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_OK),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_BUILDING,
				asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("structural Sema finalization precedes lifetime protocol authentication")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_VERIFY_INVALID_LIFETIME_PROTOCOL),
			Context.AdvanceLifecycle(
				asAST_CONTEXT_SEMA_FINALIZED,
				asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("an unknown lifetime protocol revision must fail closed")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asAST_CONTEXT_SEMA_FINALIZED),
			static_cast<int32>(Context.GetLifecycleState()),
			TEXT("failed authentication must not claim LifetimePlanned")));
		ASSERT_THAT(IsFalse(Context.IsPublishable(),
			TEXT("a wrong-revision protocol must never reach a backend or snapshot reader")));
	}

	TEST_METHOD(RejectsDanglingWrongKindAndInvalidRange)
	{
		asCASTContext Empty;
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerify(Empty, Result),
			TEXT("missing translation unit should be dangling-id")));

		asCASTContext Context;
		Context.CreateTranslationUnit("V");
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Context.GetTranslationUnit(), asCSourceRange(), "F");
		ASSERT_THAT(IsTrue(Fn.IsValid(), TEXT("function decl should exist")));
		ASSERT_THAT(AreEqual(0, Context.AddDeclChild(Fn, asASTDeclId(99)), TEXT("foreign child attach")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("unknown child ID should fail closed")));

		asCASTContext Kinds;
		Kinds.CreateTranslationUnit("K");
		const asASTDeclId Broken = Kinds.CreateDecl(asAST_DECL_FUNCTION, Kinds.GetTranslationUnit(), asCSourceRange(), "G");
		ASSERT_THAT(AreEqual(0, Kinds.SetDeclKind(Broken, asAST_DECL_INVALID), TEXT("kind corruption")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Kinds, Result),
			TEXT("invalid kind should fail closed")));
	}

	TEST_METHOD(RejectsInvalidSourceRange)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("R");
		asCSourceRange Range;
		Range.begin = asCSourceLocation(1, 0);
		Range.end = asCSourceLocation(1, 4);
		Context.CreateDecl(asAST_DECL_FUNCTION, Tu, Range, "F");
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_RANGE, asCASTVerify(Context, Result),
			TEXT("range into a missing FileID should fail")));
	}

	TEST_METHOD(RejectsStmtIndexMismatch)
	{
		asCASTContext Stmts;
		Stmts.CreateTranslationUnit("StmtV");
		const asASTStmtId Block = Stmts.CreateStmt(asAST_STMT_BLOCK, Stmts.GetTranslationUnit(), asCSourceRange());
		ASSERT_THAT(IsTrue(Block.IsValid(), TEXT("block stmt should exist")));
		ASSERT_THAT(AreEqual(0, Stmts.SetStmtId(Block, asASTStmtId(99)), TEXT("stmt id corruption")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_FOREIGN_ID, asCASTVerify(Stmts, Result),
			TEXT("stmt id must match its table index")));
	}

	TEST_METHOD(RejectsExprIndexMismatch)
	{
		asCASTContext Exprs;
		Exprs.CreateTranslationUnit("ExprV");
		const asCQualType IntType = Exprs.InternPrimitive(ttInt, 0);
		const asASTExprId Lit = Exprs.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(IsTrue(Lit.IsValid(), TEXT("literal expr should exist")));
		ASSERT_THAT(AreEqual(0, Exprs.SetExprId(Lit, asASTExprId(99)), TEXT("expr id corruption")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_FOREIGN_ID, asCASTVerify(Exprs, Result),
			TEXT("expr id must match its table index")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-id"), TEXT("detail token must be expr-id")));
	}

	TEST_METHOD(RejectsParentChildNotBidirectional)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ParentV");
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		ASSERT_THAT(IsNotNull(Context.GetDecl(Tu), TEXT("translation unit should exist")));
		ASSERT_THAT(IsTrue(Fn.IsValid(), TEXT("function child should exist")));
		ASSERT_THAT(AreEqual(0, Context.ClearDeclChildren(Tu), TEXT("drop reverse child list")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("child parent must be listed on the parent")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("decl-parent-child"), TEXT("detail token must be decl-parent-child")));
	}

	TEST_METHOD(RejectsBreakTargetNotAncestor)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("BreakV");
		const asASTStmtId LoopA = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId LoopB = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Brk = Context.CreateStmt(asAST_STMT_BREAK, Tu, asCSourceRange());
		ASSERT_THAT(IsNotNull(Context.GetStmt(LoopA), TEXT("loop A should exist")));
		ASSERT_THAT(IsNotNull(Context.GetStmt(Brk), TEXT("break should exist")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(LoopA, Brk), TEXT("break under loop A")));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Brk, LoopB), TEXT("break targets sibling loop B")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("break must target an ancestor loop or switch")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("break-ancestor"), TEXT("detail token must be break-ancestor")));
	}

	TEST_METHOD(RejectsExprMissingExactType)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("TypeV");
		const asASTExprId Lit = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, asCQualType(), asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(IsTrue(Lit.IsValid(), TEXT("untyped literal should still be allocated")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_TYPE, asCASTVerify(Context, Result),
			TEXT("non-error expr must carry an exact interned type")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-type"), TEXT("detail token must be expr-type")));
	}

	TEST_METHOD(RejectsNullConversionToReferenceFormal)
	{
		// Production mutation caught: accepting a reference-qualified nullable
		// target would turn a constant null prvalue into storage for &in/&out.
		asCASTContext Context;
		Context.CreateTranslationUnit("NullReferenceConversionV");
		const asCQualType NullType = Context.InternPrimitive(
			ttVoid, asAST_QUAL_HANDLE);
		const asCQualType ReferenceTarget = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT,
			"FNullableReference",
			asAST_QUAL_HANDLE | asAST_QUAL_REFERENCE | asAST_QUAL_IN);
		const asASTExprId NullLiteral = Context.CreateExpr(
			asAST_EXPR_NULL_LITERAL,
			NullType,
			asAST_VALUE_PRVALUE,
			asCSourceRange());
		const asASTExprId Conversion = Context.CreateExpr(
			asAST_EXPR_CONVERSION,
			ReferenceTarget,
			asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(Conversion, NullLiteral),
			TEXT("forged null reference conversion should be structurally connected")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_TYPE,
			asCASTVerify(Context, Result),
			TEXT("null literal must not bind to a reference-qualified formal")));
		ASSERT_THAT(IsTrue(
			Result.detail.Equals("null-conversion-target"),
			TEXT("null reference rejection must keep the stable detail token")));
	}

	TEST_METHOD(RejectsNullConversionToValueObjectHandleShape)
	{
		// Production mutation caught: HANDLE-shaped metadata on a value object
		// (including asOBJ_ASHANDLE-like forms) is not a real object handle.
		asCASTContext Context;
		Context.CreateTranslationUnit("NullValueHandleConversionV");
		const asCQualType NullType = Context.InternPrimitive(
			ttVoid, asAST_QUAL_HANDLE);
		const asCQualType ValueHandleShape = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT,
			"FAsHandleValue",
			asAST_QUAL_HANDLE);
		const asASTExprId NullLiteral = Context.CreateExpr(
			asAST_EXPR_NULL_LITERAL,
			NullType,
			asAST_VALUE_PRVALUE,
			asCSourceRange());
		const asASTExprId Conversion = Context.CreateExpr(
			asAST_EXPR_CONVERSION,
			ValueHandleShape,
			asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(Conversion, NullLiteral),
			TEXT("forged null value-handle conversion should be structurally connected")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_TYPE,
			asCASTVerify(Context, Result),
			TEXT("null literal must not convert to a value-object handle shape")));
		ASSERT_THAT(IsTrue(
			Result.detail.Equals("null-conversion-target"),
			TEXT("null value-object rejection must keep the stable detail token")));
	}

	TEST_METHOD(RejectsExprOperandWrongKind)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("OperandV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId Bin = Context.CreateExpr(asAST_EXPR_BINARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(IsTrue(Bin.IsValid(), TEXT("binary expr should exist")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Bin, asASTExprId(99)), TEXT("dangling operand")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("expr operands must resolve as expr ids")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-operand"), TEXT("detail token must be expr-operand")));
	}

	TEST_METHOD(RejectsStmtMultiOwner)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("StmtMulti");
		const asASTStmtId LoopA = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId LoopB = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Inner = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(LoopA, Inner), TEXT("inner under loop A")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(LoopB, Inner), TEXT("same inner under loop B")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("a stmt must not have two parents")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-multi-owner"), TEXT("detail token must be stmt-multi-owner")));
	}

	TEST_METHOD(RejectsDuplicateStructuralChildEdgeFromTheSameParent)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"DuplicateBlockChildV");
			const asASTStmtId Block = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Child = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Block, Child)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Block, Child)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("one Block cannot structurally execute the same statement identity twice")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-multi-owner")));
			ASSERT_THAT(IsTrue(Result.offendingNode
				== asSASTNodeRef::FromStmt(Child)));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(Block)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"DuplicateSwitchCaseIdentityV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId Case = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case, Value)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Case, Switch)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, Case)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, Case)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("one Switch cannot list the same Case identity twice")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-multi-owner")));
			ASSERT_THAT(IsTrue(Result.offendingNode
				== asSASTNodeRef::FromStmt(Case)));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(Switch)));
		}
	}

	TEST_METHOD(RejectsStmtCycle)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("StmtCycle");
		const asASTStmtId BlockA = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		const asASTStmtId BlockB = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(BlockA, BlockB), TEXT("A owns B")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(BlockB, BlockA), TEXT("B owns A")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("stmt child lists must be acyclic")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-cycle"), TEXT("detail token must be stmt-cycle")));
	}

	TEST_METHOD(RejectsExprCycle)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("ExprCycle");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId UnaryA = Context.CreateExpr(asAST_EXPR_UNARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId UnaryB = Context.CreateExpr(asAST_EXPR_UNARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(UnaryA, UnaryB), TEXT("A owns B")));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(UnaryB, UnaryA), TEXT("B owns A")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("expr child lists must be acyclic before a backend can traverse them")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-cycle"), TEXT("detail token must be expr-cycle")));
	}

	TEST_METHOD(RejectsExprReceiverCycle)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("ExprReceiverCycle");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId CallA = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId CallB = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetExprReceiver(CallA, CallB), TEXT("A receives B")));
		ASSERT_THAT(AreEqual(0, Context.SetExprReceiver(CallB, CallA), TEXT("B receives A")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Context, Result),
			TEXT("effective receiver traversal must not form a cycle")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-cycle"), TEXT("detail token must be expr-cycle")));
	}

	TEST_METHOD(RejectsGeneratedGetterInitWithoutExactCopyConstructionPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("GeneratedGetterCopyPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierNativeValue", 0);
		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Getter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "GetInner");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Getter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Getter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Getter, asAST_ACCESSOR_GET, Field)));

		// A field reference has the right storage identity and type, but it is not
		// an object-return lifetime plan. Publishing it would let a backend invent
		// raw-copy or copy-constructor semantics after Sema.
		const asASTExprId BareField = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(BareField, Field)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Getter, BareField)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_INVALID_CHILD),
			asCASTVerify(Context, Result),
			TEXT("a generated getter init must be an exact sealed copy-construction plan")));
		ASSERT_THAT(IsTrue(
			Result.detail.Equals("accessor-get-copy-plan"),
			TEXT("the malformed getter plan needs a stable verifier token")));
	}

	TEST_METHOD(AcceptsGeneratedGetterExactCopyConstructionPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("ValidGeneratedGetterCopyPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierNativeValue", 0);
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);

		const asASTDeclId ValueOwner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FVerifierNativeValue");
		const asASTDeclId CopyConstructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, ValueOwner, asCSourceRange(),
			"FVerifierNativeValue");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(CopyConstructor, VoidType)));
		const asASTDeclId CopyParameter = Context.CreateDecl(
			asAST_DECL_PARAM, CopyConstructor, asCSourceRange(), "Other");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(
			CopyParameter,
			asCQualType(
				ValueType.type,
				asAST_QUAL_CONST | asAST_QUAL_REFERENCE | asAST_QUAL_IN))));

		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Getter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "GetInner");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Getter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Getter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Getter, asAST_ACCESSOR_GET, Field)));

		const asASTExprId Source = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Source, Field)));
		const asASTExprId Construct = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ValueType, asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Construct, CopyConstructor)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Construct, Source)));
		const asASTExprId Materialize = Context.CreateExpr(
			asAST_EXPR_MATERIALIZE_TEMPORARY, ValueType,
			asAST_VALUE_XVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(Materialize, Construct)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, ValueType, asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.AddExprChild(Cleanup, Materialize)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Getter, Cleanup)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_OK),
			asCASTVerify(Context, Result),
			TEXT("the exact field-to-copy-constructor plan must remain publishable")));
	}

	TEST_METHOD(RejectsGeneratedSetterInitWithoutExactAssignmentPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("GeneratedSetterAssignmentPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierSetterValue", 0);
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);
		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Setter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "SetInner");
		const asASTDeclId Parameter = Context.CreateDecl(
			asAST_DECL_PARAM, Setter, asCSourceRange(), "value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Setter, VoidType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Parameter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Setter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Setter, asAST_ACCESSOR_SET, Field)));

		const asASTExprId BareParameter = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(BareParameter, Parameter)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Setter, BareParameter)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_INVALID_CHILD),
			asCASTVerify(Context, Result),
			TEXT("a generated setter init must be an exact sealed assignment plan")));
		ASSERT_THAT(IsTrue(
			Result.detail.Equals("accessor-set-assignment-plan"),
			TEXT("the malformed setter plan needs a stable verifier token")));
	}

	TEST_METHOD(AcceptsGeneratedSetterExactAssignmentPlan)
	{
		asCASTContext Context;
		const asASTDeclId Tu =
			Context.CreateTranslationUnit("ValidGeneratedSetterAssignmentPlan");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FVerifierSetterValue", 0);
		const asCQualType ConstRefValueType(
			ValueType.type,
			asAST_QUAL_CONST | asAST_QUAL_REFERENCE | asAST_QUAL_IN);
		const asCQualType VoidType = Context.InternPrimitive(ttVoid, 0);

		const asASTDeclId ValueOwner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FVerifierSetterValue");
		const asASTDeclId AssignOperator = Context.CreateDecl(
			asAST_DECL_METHOD, ValueOwner, asCSourceRange(), "opAssign");
		const asASTDeclId AssignParameter = Context.CreateDecl(
			asAST_DECL_PARAM, AssignOperator, asCSourceRange(), "Other");
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(AssignOperator, ConstRefValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.SetDeclType(AssignParameter, ConstRefValueType)));

		const asASTDeclId Owner = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FOwner");
		const asASTDeclId Field = Context.CreateDecl(
			asAST_DECL_VAR, Owner, asCSourceRange(), "Inner");
		const asASTDeclId Setter = Context.CreateDecl(
			asAST_DECL_METHOD, Owner, asCSourceRange(), "SetInner");
		const asASTDeclId Parameter = Context.CreateDecl(
			asAST_DECL_PARAM, Setter, asCSourceRange(), "value");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Field, ValueType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Setter, VoidType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Parameter, ValueType)));
		ASSERT_THAT(AreEqual(0,
			Context.AddDeclTrait(Setter, asAST_TRAIT_GENERATED)));
		ASSERT_THAT(AreEqual(0,
			Context.SetGeneratedAccessor(Setter, asAST_ACCESSOR_SET, Field)));

		const asASTExprId Lhs = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		const asASTExprId Rhs = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		const asASTExprId Assign = Context.CreateExpr(
			asAST_EXPR_ASSIGN, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Lhs, Field)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Rhs, Parameter)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Assign, Lhs)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Assign, Rhs)));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Assign, AssignOperator)));
		ASSERT_THAT(AreEqual(0, Context.AddDeclInit(Setter, Assign)));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			static_cast<int>(asAST_VERIFY_OK),
			asCASTVerify(Context, Result),
			TEXT("the verifier must accept the exact generated setter assignment plan")));
	}

	TEST_METHOD(RejectsFallthroughOutsideSwitch)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("FallV");
		const asASTStmtId Block = Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange());
		const asASTStmtId Fall = Context.CreateStmt(asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Block, Fall), TEXT("fallthrough under a bare block")));
		asSAstVerifyResult Result;
		ASSERT_THAT(IsTrue(asCASTVerify(Context, Result) != (int)asAST_VERIFY_OK,
			TEXT("fallthrough must sit under a switch")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("fallthrough-switch"), TEXT("detail token must be fallthrough-switch")));
	}

	TEST_METHOD(RejectsUnsealedPublication)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("PubV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId Body = Context.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
		const asASTExprId Lit = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetBody(Fn, Body), TEXT("set body")));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Body, Lit), TEXT("return literal")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Context, Result),
			TEXT("asCASTVerify must still accept an unsealed construction graph so Seal() can run")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Context, Result),
			TEXT("publication of an unsealed graph must fail closed")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("unsealed-publication"), TEXT("detail token must be unsealed-publication")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_BUILDING, asAST_CONTEXT_SEMA_FINALIZED),
			TEXT("fixture must reach SemaFinalized")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Context, Result),
			TEXT("SemaFinalized must not be publishable")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_SEMA_FINALIZED, asAST_CONTEXT_LIFETIME_PLANNED),
			TEXT("fixture must reach LifetimePlanned")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Context, Result),
			TEXT("LifetimePlanned must not be publishable")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, Context.AdvanceLifecycle(
			asAST_CONTEXT_LIFETIME_PLANNED, asAST_CONTEXT_FROZEN_PUBLISHABLE),
			TEXT("fixture must reach Frozen/Publishable")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerifyPublication(Context, Result),
			TEXT("publication of a Frozen/Publishable verified graph must succeed")));
	}

	TEST_METHOD(RejectsCleanupResolvedDeclNotDestructor)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("CleanupV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "NotDtor");
		const asASTExprId Inner = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Cleanup = Context.CreateExpr(asAST_EXPR_CLEANUP, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Inner), TEXT("cleanup arity 1")));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Cleanup, Fn), TEXT("point cleanup at a non-destructor")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("CLEANUP resolvedDecl if set must be a destructor")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("cleanup-dtor"), TEXT("detail token must be cleanup-dtor")));
	}

	TEST_METHOD(RejectsScopeExitCleanupBoundToWrongValueTypeDestructor)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeExitCleanupTypeV");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FExpectedScopeValue", 0);
		const asASTDeclId ExpectedClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FExpectedScopeValue");
		const asASTDeclId WrongClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "FWrongScopeValue");
		const asASTDeclId WrongDestructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, WrongClass, asCSourceRange(), "FWrongScopeValue");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Local");
		ASSERT_THAT(IsTrue(ExpectedClass.IsValid()));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, ValueType)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, ValueType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Cleanup, WrongDestructor)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-exit")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_TYPE,
			asCASTVerify(Context, Result),
			TEXT("scope-exit cleanup must reject a destructor from a different value type")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-exit-cleanup-type"),
			TEXT("detail token must identify the forged scope-exit destructor owner")));
	}

	TEST_METHOD(RejectsScopeExitCleanupWhenOnlyUnqualifiedOwnerNameMatches)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit(
			"ScopeExitCleanupQualifiedOwnerV");
		const asCQualType ValueType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		const asASTDeclId WrongNamespace = Context.CreateDecl(
			asAST_DECL_NAMESPACE, Tu, asCSourceRange(), "Wrong");
		const asASTDeclId WrongClass = Context.CreateDecl(
			asAST_DECL_CLASS, WrongNamespace, asCSourceRange(), "FValue");
		ASSERT_THAT(AreEqual(0,
			Context.SetStableKey(WrongClass, "Wrong::FValue")));
		const asASTDeclId WrongDestructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, WrongClass, asCSourceRange(), "FValue");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Local");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, ValueType)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, ValueType, asAST_VALUE_LVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, ValueType, asAST_VALUE_PRVALUE,
			asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0,
			Context.SetResolvedDecl(Cleanup, WrongDestructor)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-exit")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_TYPE,
			asCASTVerify(Context, Result),
			TEXT("an unqualified type key must not accept a destructor owned by a namespaced same-name class")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-exit-cleanup-type"),
			TEXT("detail token must identify the non-exact destructor owner")));
	}

	TEST_METHOD(RejectsScopeReleaseCleanupForNonOwningReference)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeReleaseNonOwningV");
		const asCQualType NonOwningReference = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "CScopedRef", asAST_QUAL_REFERENCE);
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Borrowed");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, NonOwningReference)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, NonOwningReference, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, NonOwningReference, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-release")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_TYPE,
			asCASTVerify(Context, Result),
			TEXT("scope-release cleanup must reject a borrowed non-owning reference")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-release-cleanup-type"),
			TEXT("detail token must identify the forged non-owning release target")));
	}

	TEST_METHOD(RejectsScopeReleaseCleanupWithDestructorBinding)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeReleaseDestructorV");
		const asCQualType OwningHandle = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT,
			"CScopedRef",
			asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
		const asASTDeclId RefClass = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(), "CScopedRef");
		const asASTDeclId Destructor = Context.CreateDecl(
			asAST_DECL_DESTRUCTOR, RefClass, asCSourceRange(), "CScopedRef");
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "Owned");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, OwningHandle)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, OwningHandle, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, OwningHandle, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Cleanup, Destructor)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-release")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_WRONG_KIND,
			asCASTVerify(Context, Result),
			TEXT("scope-release cleanup must not bind a value-object destructor route")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-release-cleanup-decl"),
			TEXT("detail token must identify the forged release destructor binding")));
	}

	TEST_METHOD(RejectsScopeReleaseCleanupForUninitializedOwningHandle)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ScopeReleaseUninitializedV");
		const asCQualType OwningHandle = Context.InternNamedType(
			asAST_TYPE_REFERENCE_OBJECT, "CScopedRef", asAST_QUAL_HANDLE);
		const asASTDeclId Function = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTDeclId Local = Context.CreateDecl(
			asAST_DECL_VAR, Function, asCSourceRange(), "NeverInitialized");
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Local, OwningHandle)));

		const asASTExprId Target = Context.CreateExpr(
			asAST_EXPR_DECL_REF, OwningHandle, asAST_VALUE_LVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Target, Local)));
		const asASTExprId Cleanup = Context.CreateExpr(
			asAST_EXPR_CLEANUP, OwningHandle, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Cleanup, Target)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteral(Cleanup, "scope-release")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual(
			(int)asAST_VERIFY_INVALID_CHILD,
			asCASTVerify(Context, Result),
			TEXT("scope-release must reject a local with no sealed initialization fact")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("scope-release-cleanup-uninitialized"),
			TEXT("detail token must identify the missing live-state initializer")));
	}

	TEST_METHOD(RejectsBreakSkippedNearerLoop)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("SkipV");
		const asASTStmtId Outer = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Inner = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Brk = Context.CreateStmt(asAST_STMT_BREAK, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Outer, Inner), TEXT("inner under outer")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Inner, Brk), TEXT("break under inner")));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Brk, Outer), TEXT("break skips inner, targets outer")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("break must target the nearest enclosing loop or switch, not a skipped outer loop")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("break-skipped-nearer"),
			TEXT("detail token must be break-skipped-nearer")));

		asCASTContext Dangling;
		const asASTDeclId DTu = Dangling.CreateTranslationUnit("DangleV");
		const asASTStmtId Loop = Dangling.CreateStmt(asAST_STMT_WHILE, DTu, asCSourceRange());
		const asASTStmtId DBrk = Dangling.CreateStmt(asAST_STMT_BREAK, DTu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Dangling.AddStmtChild(Loop, DBrk), TEXT("break under loop")));
		ASSERT_THAT(AreEqual(0, Dangling.SetTarget(DBrk, asASTStmtId(99)), TEXT("dangling target id")));
		asSAstVerifyResult DanglingResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerify(Dangling, DanglingResult),
			TEXT("break target must resolve in the stmt table")));
		ASSERT_THAT(IsTrue(
			DanglingResult.detail.Equals("stmt-target") || DanglingResult.detail.Equals("ctrl-target"),
			TEXT("detail token must stay stmt-target or ctrl-target")));
	}

	TEST_METHOD(RejectsContinueSkippedNearerLoop)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("ContSkipV");
		const asASTStmtId Outer = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Inner = Context.CreateStmt(asAST_STMT_WHILE, Tu, asCSourceRange());
		const asASTStmtId Cont = Context.CreateStmt(asAST_STMT_CONTINUE, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Outer, Inner), TEXT("inner under outer")));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Inner, Cont), TEXT("continue under inner")));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Cont, Outer), TEXT("continue skips inner, targets outer")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result),
			TEXT("continue must target the nearest enclosing loop, not a skipped outer loop")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("continue-skipped-nearer"),
			TEXT("detail token must be continue-skipped-nearer")));
	}

	TEST_METHOD(RejectsBlockWrappedTransferThatSkipsTheNearestLoop)
	{
		struct FCase
		{
			asEASTStmtKind Kind;
			const char* Detail;
		};
		const FCase Cases[] = {
			{ asAST_STMT_BREAK, "break-skipped-nearer" },
			{ asAST_STMT_CONTINUE, "continue-skipped-nearer" },
		};

		for( asUINT CaseIndex = 0;
			CaseIndex < sizeof(Cases) / sizeof(Cases[0]); ++CaseIndex )
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"BlockWrappedSkippedNearestV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId OuterCondition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId InnerCondition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Outer = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Inner = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			// Match Parser/Sema construction order: the contained transfer is
			// interned before the Blocks that later become its parent chain.
			const asASTStmtId Transfer = Context.CreateStmt(
				Cases[CaseIndex].Kind, Tu, asCSourceRange());
			const asASTStmtId InnerBody = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId OuterBody = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());

			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(OuterCondition, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(InnerCondition, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Outer, OuterCondition)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Inner, InnerCondition)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Outer, OuterBody)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(OuterBody, Inner)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Inner, InnerBody)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(InnerBody, Transfer)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Transfer, Outer),
				TEXT("forged transfer skips the inner loop")));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Outer, asAST_SAFEPOINT_LOOP_BACKEDGE)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Inner, asAST_SAFEPOINT_LOOP_BACKEDGE)));
			ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
				Outer, asAST_SAFEPOINT_LOOP_ENTRY)));
			ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
				Inner, asAST_SAFEPOINT_LOOP_ENTRY)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Transfer, asAST_SAFEPOINT_TRANSFER)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("nearest-target authentication must not depend on statement ID order")));
			ASSERT_THAT(IsTrue(Result.detail.Equals(Cases[CaseIndex].Detail),
				TEXT("Block-wrapped skipped-nearer failures keep their transfer-specific token")));
		}
	}

	TEST_METHOD(RejectsContinueTargetWrongKindAndNonAncestor)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ContinueWrongKindV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Continue = Context.CreateStmt(
				asAST_STMT_CONTINUE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, Continue)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Continue, Body),
				TEXT("forged Continue targets an ancestor Block")));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("Continue targets only loops")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("continue-target")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ContinueNonAncestorV");
			const asASTStmtId Root = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId LoopA = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId LoopB = Context.CreateStmt(
				asAST_STMT_FOR, Tu, asCSourceRange());
			const asASTStmtId Continue = Context.CreateStmt(
				asAST_STMT_CONTINUE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Root, LoopA)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Root, LoopB)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(LoopA, Continue)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Continue, LoopB),
				TEXT("forged Continue targets a sibling loop")));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("Continue target must be an enclosing loop")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("continue-ancestor")));
		}
	}

	TEST_METHOD(RejectsDuplicateCaseAndDefaultNotLast)
	{
		asCASTContext Dup;
		const asASTDeclId Tu = Dup.CreateTranslationUnit("DupCaseV");
		const asCQualType IntType = Dup.InternPrimitive(ttInt, 0);
		const asASTStmtId Sw = Dup.CreateStmt(asAST_STMT_SWITCH, Tu, asCSourceRange());
		const asASTStmtId CaseA = Dup.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTStmtId CaseB = Dup.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTExprId LitA = Dup.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId LitB = Dup.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Dup.SetLiteral(LitA, "1")));
		ASSERT_THAT(AreEqual(0, Dup.SetLiteral(LitB, "1")));
		ASSERT_THAT(AreEqual(0, Dup.SetLiteralBits(LitA, 1)));
		ASSERT_THAT(AreEqual(0, Dup.SetLiteralBits(LitB, 1)));
		ASSERT_THAT(AreEqual(0, Dup.SetStmtExpr(CaseA, LitA)));
		ASSERT_THAT(AreEqual(0, Dup.SetStmtExpr(CaseB, LitB)));
		ASSERT_THAT(AreEqual(0, Dup.AddStmtChild(Sw, CaseA)));
		ASSERT_THAT(AreEqual(0, Dup.AddStmtChild(Sw, CaseB)));
		asSAstVerifyResult DupResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Dup, DupResult),
			TEXT("duplicate normalized case constants must fail")));
		ASSERT_THAT(IsTrue(DupResult.detail.Equals("duplicate-case"),
			TEXT("detail token must be duplicate-case")));

		asCASTContext Order;
		const asASTDeclId OTu = Order.CreateTranslationUnit("DefOrderV");
		const asASTStmtId OSw = Order.CreateStmt(asAST_STMT_SWITCH, OTu, asCSourceRange());
		const asASTStmtId DefaultCase = Order.CreateStmt(asAST_STMT_CASE, OTu, asCSourceRange());
		const asASTStmtId LaterCase = Order.CreateStmt(asAST_STMT_CASE, OTu, asCSourceRange());
		const asASTExprId LaterLit = Order.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Order.SetLiteral(LaterLit, "2")));
		ASSERT_THAT(AreEqual(0, Order.SetLiteralBits(LaterLit, 2)));
		ASSERT_THAT(AreEqual(0, Order.SetStmtExpr(LaterCase, LaterLit)));
		ASSERT_THAT(AreEqual(0, Order.AddStmtChild(OSw, DefaultCase), TEXT("default first (no expr)")));
		ASSERT_THAT(AreEqual(0, Order.AddStmtChild(OSw, LaterCase), TEXT("valued case after default")));
		asSAstVerifyResult OrderResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Order, OrderResult),
			TEXT("default must be last in source order (current dialect TXT_DEFAULT_MUST_BE_LAST)")));
		ASSERT_THAT(IsTrue(OrderResult.detail.Equals("default-order"),
			TEXT("detail token must be default-order")));
	}

	TEST_METHOD(RejectsFallthroughTargetNotNextCase)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("FallTargetV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTStmtId Sw = Context.CreateStmt(asAST_STMT_SWITCH, Tu, asCSourceRange());
		const asASTStmtId Case0 = Context.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTStmtId Case1 = Context.CreateStmt(asAST_STMT_CASE, Tu, asCSourceRange());
		const asASTStmtId Fall = Context.CreateStmt(asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
		const asASTExprId Lit0 = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Lit1 = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Lit0, 0)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Lit1, 1)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case0, Lit0)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case1, Lit1)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Sw, Case0)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Sw, Case1)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Case0, Fall)));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Fall, Sw), TEXT("fallthrough targets the switch, not next case")));
		asSAstVerifyResult Result;
		ASSERT_THAT(IsTrue(asCASTVerify(Context, Result) != (int)asAST_VERIFY_OK,
			TEXT("fallthrough target must be the next ordered case")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("fallthrough-target"),
			TEXT("detail token must be fallthrough-target")));
	}

	TEST_METHOD(RejectsMissingBreakAndContinueTargetsBeforePublication)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MissingBreakTargetV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Break = Context.CreateStmt(
				asAST_STMT_BREAK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteral(Condition, "true")));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Condition, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, Break)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_BACKEDGE)));
			ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_ENTRY)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Break, asAST_SAFEPOINT_TRANSFER)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK,
				asCASTVerify(Context, Result),
				TEXT("an unsealed recovery graph may retain a targetless break")));
			ASSERT_THAT(AreEqual(0, Context.Seal(),
				TEXT("ordinary Seal must preserve the forged graph for publication diagnostics")));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID,
				asCASTVerifyPublication(Context, Result),
				TEXT("a publishable break must carry its exact Sema-resolved target")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("break-target-missing"),
				TEXT("missing break target must keep a stable detail token")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MissingContinueTargetV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Continue = Context.CreateStmt(
				asAST_STMT_CONTINUE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteral(Condition, "true")));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Condition, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, Continue)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_BACKEDGE)));
			ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_ENTRY)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Continue, asAST_SAFEPOINT_TRANSFER)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK,
				asCASTVerify(Context, Result),
				TEXT("an unsealed recovery graph may retain a targetless continue")));
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID,
				asCASTVerifyPublication(Context, Result),
				TEXT("a publishable continue must carry its exact Sema-resolved target")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("continue-target-missing"),
				TEXT("missing continue target must keep a stable detail token")));
		}
	}

	TEST_METHOD(RejectsMissingAndMismatchedCaseTargets)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MissingCaseTargetV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			asASTExprId ValueDomain;
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId Case = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Selector, 0)));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Value, 1)));
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), Value, 1, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, Case)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Switch, asAST_SAFEPOINT_STATEMENT)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Case, asAST_SAFEPOINT_STATEMENT)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID,
				asCASTVerifyPublication(Context, Result),
				TEXT("every published Case must reference its exact enclosing Switch")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("case-target-missing"),
				TEXT("missing Case target must keep a stable detail token")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MismatchedCaseTargetV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId SelectorA = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId SelectorB = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			asASTExprId ValueDomain;
			const asASTStmtId SwitchA = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId SwitchB = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId Case = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), Value, 0, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(SwitchA, SelectorA)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(SwitchB, SelectorB)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(SwitchA, Case)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Case, SwitchB)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("a Case target must be the structural parent Switch, not a sibling Switch")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("case-target"),
				TEXT("mismatched Case target must keep a stable detail token")));
		}
	}

	TEST_METHOD(RejectsMissingNonFinalFallthroughAndAcceptsUntargetedFinalFallthrough)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MissingFallthroughTargetV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId FirstValue = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId SecondValue = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			asASTExprId FirstValueDomain;
			asASTExprId SecondValueDomain;
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId FirstCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId SecondCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId Fallthrough = Context.CreateStmt(
				asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(FirstValue, 0)));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(SecondValue, 1)));
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), FirstValue, 0,
				FirstValueDomain)));
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), SecondValue, 1,
				SecondValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(
				FirstCase, FirstValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(
				SecondCase, SecondValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, FirstCase)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, SecondCase)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(FirstCase, Switch)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(SecondCase, Switch)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(
				FirstCase, Fallthrough)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID,
				asCASTVerifyPublication(Context, Result),
				TEXT("a non-final explicit fallthrough must target the exact next Case")));
			ASSERT_THAT(IsTrue(Result.detail.Equals(
				"fallthrough-target-missing"),
				TEXT("missing fallthrough target must keep a stable detail token")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"LastCaseFallthroughV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			asASTExprId ValueDomain;
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId LastCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId Fallthrough = Context.CreateStmt(
				asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), Value, 0, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(
				LastCase, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, LastCase)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(LastCase, Switch)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Switch, asAST_SAFEPOINT_STATEMENT)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				LastCase, asAST_SAFEPOINT_STATEMENT)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(
				LastCase, Fallthrough)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK,
				asCASTVerifyPublication(Context, Result),
				TEXT("LEGACY-compatible final-Case fallthrough has no next target and must remain publishable")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForgedLastCaseFallthroughTargetV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			asASTExprId ValueDomain;
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId LastCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId Fallthrough = Context.CreateStmt(
				asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), Value, 0, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(
				LastCase, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, LastCase)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(LastCase, Switch)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(
				LastCase, Fallthrough)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Fallthrough, Switch)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("a final-Case fallthrough cannot carry a forged valid target")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("fallthrough-target")));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_TARGET,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.offendingNode
				== asSASTNodeRef::FromStmt(Fallthrough)));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(Switch)));
		}
	}

	TEST_METHOD(RejectsFallthroughWithInvalidStructuralPlacement)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"NestedFallthroughPlacementV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			asASTExprId ValueDomain;
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId Case = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId NestedBlock = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Fallthrough = Context.CreateStmt(
				asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), Value, 0, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, Case)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Case, Switch)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Case, NestedBlock)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(
				NestedBlock, Fallthrough)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("explicit fallthrough must be a direct Case child")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("fallthrough-case")));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(NestedBlock)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"NonFinalFallthroughPlacementV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Value = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			asASTExprId ValueDomain;
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId Case = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId Fallthrough = Context.CreateStmt(
				asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
			const asASTStmtId After = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, CreateSwitchCaseDomainFact(
				Context, IntType, asCSourceRange(), Value, 0, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case, ValueDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, Case)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Case, Switch)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Case, Fallthrough)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Case, After)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("explicit fallthrough must be the final Case child")));
			ASSERT_THAT(IsTrue(Result.detail.Equals(
				"fallthrough-position")));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(Case)));
		}
	}

	TEST_METHOD(RejectsSwitchNonCaseChildrenAndNormalizedDuplicateValues)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"SwitchWrongChildV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId NotCase = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, NotCase)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("Switch children are ordered Case nodes, not arbitrary statements")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("switch-child-kind"),
				TEXT("wrong Switch child kind must keep a stable detail token")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"NormalizedDuplicateCaseV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asCQualType UIntType = Context.InternPrimitive(ttUInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, UIntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Decimal = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId HexLiteral = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Hex = Context.CreateExpr(
				asAST_EXPR_CONVERSION, UIntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId DecimalDomain = Context.CreateExpr(
				asAST_EXPR_CONVERSION, UIntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId HexDomain = Context.CreateExpr(
				asAST_EXPR_CONVERSION, UIntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId DecimalCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId HexCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteral(Decimal, "1")));
			ASSERT_THAT(AreEqual(0, Context.SetLiteral(HexLiteral, "0x1")));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Decimal, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(HexLiteral, 1)));
			ASSERT_THAT(AreEqual(0, Context.AddExprChild(Hex, HexLiteral)));
			ASSERT_THAT(AreEqual(0,
				Context.SetLiteral(DecimalDomain, "switch-case-domain")));
			ASSERT_THAT(AreEqual(0,
				Context.SetLiteral(HexDomain, "switch-case-domain")));
			ASSERT_THAT(AreEqual(0,
				Context.SetLiteralBits(DecimalDomain, 1)));
			ASSERT_THAT(AreEqual(0,
				Context.SetLiteralBits(HexDomain, 1)));
			ASSERT_THAT(AreEqual(0,
				Context.AddExprChild(DecimalDomain, Decimal)));
			ASSERT_THAT(AreEqual(0,
				Context.AddExprChild(HexDomain, Hex)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0,
				Context.SetStmtExpr(DecimalCase, DecimalDomain)));
			ASSERT_THAT(AreEqual(0,
				Context.SetStmtExpr(HexCase, HexDomain)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, DecimalCase)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, HexCase)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(DecimalCase, Switch)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(HexCase, Switch)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("duplicate Case values must be compared after conversion to the uint selector domain, not by exact authored AST type")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("duplicate-case"),
				TEXT("normalized duplicate Case must keep a stable detail token")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MissingCaseDomainFactV");
			const asCQualType UIntType = Context.InternPrimitive(ttUInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, UIntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId Authored = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, UIntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId Case = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Authored, 2)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Case, Authored)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, Case)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Case, Switch)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("a primitive Case must carry Sema's explicit normalized-domain fact")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("case-domain-fact"),
				TEXT("missing Case-domain fact must keep a stable detail token")));
		}
	}

	TEST_METHOD(MalformedStructuredControlCannotSealOrPublish)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit(
			"MalformedControlFirewallV");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId Selector = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
			asCSourceRange());
		const asASTStmtId Switch = Context.CreateStmt(
			asAST_STMT_SWITCH, Tu, asCSourceRange());
		const asASTStmtId NotCase = Context.CreateStmt(
			asAST_STMT_BLOCK, Tu, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
		ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, NotCase)));

		asCString Before;
		asCASTDump(Context, Before);
		const asUINT DeclCount = Context.GetDeclCount();
		const asUINT StmtCount = Context.GetStmtCount();
		const asUINT ExprCount = Context.GetExprCount();

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
			asCASTVerify(Context, Result),
			TEXT("a Switch with a non-Case child is structurally malformed")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("switch-child-kind")));
		ASSERT_THAT(IsTrue(
			Result.offendingNode == asSASTNodeRef::FromStmt(Switch)));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD,
			(int)Result.edgeRole));
		ASSERT_THAT(IsTrue(
			Result.relatedNode == asSASTNodeRef::FromStmt(NotCase)));

		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, Context.Seal(),
			TEXT("Seal must fail closed with the same structural category")));
		ASSERT_THAT(IsFalse(Context.IsPublishable(),
			TEXT("malformed structured control must never become publishable")));

		asCString After;
		asCASTDump(Context, After);
		ASSERT_THAT(AreEqual(DeclCount, Context.GetDeclCount()));
		ASSERT_THAT(AreEqual(StmtCount, Context.GetStmtCount()));
		ASSERT_THAT(AreEqual(ExprCount, Context.GetExprCount()));
		ASSERT_THAT(IsTrue(Before == After,
			TEXT("failed control verification and Seal must be mutation free")));

		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION,
			asCASTVerifyPublication(Context, Result),
			TEXT("publication must reject the still-unsealed malformed graph")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("unsealed-publication")));
	}

	TEST_METHOD(RejectsMalformedStructuredControlPhaseCounts)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit("IfPhaseCountV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId If = Context.CreateStmt(
				asAST_STMT_IF, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(If, Condition)));
			for( int ChildIndex = 0; ChildIndex < 3; ++ChildIndex )
			{
				ASSERT_THAT(AreEqual(0, Context.AddStmtChild(If,
					Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange()))));
			}
			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("If has exactly then and optional else phases")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("if-phase-count")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit("WhilePhaseCountV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop,
				Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange()))));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop,
				Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange()))));
			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("While has exactly one body phase")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("while-phase-count")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"DoWhilePhaseCountV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_DO_WHILE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("DoWhile has exactly one body phase before its trailing condition")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("do-while-phase-count")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit("ForPhaseCountV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOR, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop,
				Context.CreateStmt(asAST_STMT_BLOCK, Tu, asCSourceRange()))));
			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("For must retain initializer and body, plus only an optional increment phase")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("for-phase-count")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForIncrementKindV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOR, Tu, asCSourceRange());
			const asASTStmtId Init = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId WrongIncrement = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Init)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(
				Loop, WrongIncrement)));
			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("the optional For increment phase must be an ExprStmt")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("for-increment-kind")));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(WrongIncrement)));
		}
	}

	TEST_METHOD(RejectsMalformedForAndForeachPhaseKinds)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForInitKindV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOR, Tu, asCSourceRange());
			const asASTStmtId WrongInit = Context.CreateStmt(
				asAST_STMT_BREAK, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetTarget(WrongInit, Loop)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, WrongInit)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("For initializer must be a Block or ExprStmt phase")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("for-init-kind")));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(WrongInit)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForeachInitKindV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOREACH, Tu, asCSourceRange());
			const asASTStmtId WrongInit = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Increment = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			const asASTExprId InitExpr = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId IncrementExpr = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(InitExpr, 0)));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(IncrementExpr, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(WrongInit, InitExpr)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Increment, IncrementExpr)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, WrongInit)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Increment)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("Foreach initializer must be its canonical Block phase")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("foreach-init-kind")));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(WrongInit)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForeachBodyKindV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOREACH, Tu, asCSourceRange());
			const asASTStmtId Init = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId WrongBody = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			const asASTStmtId Increment = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			const asASTExprId BodyExpr = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId IncrementExpr = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(BodyExpr, 0)));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(IncrementExpr, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(WrongBody, BodyExpr)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Increment, IncrementExpr)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Init)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, WrongBody)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Increment)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("Foreach authored body must remain its canonical Block phase")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("foreach-body-kind")));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(WrongBody)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForeachIncrementKindV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOREACH, Tu, asCSourceRange());
			const asASTStmtId Init = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId WrongIncrement = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Init)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, WrongIncrement)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND,
				asCASTVerify(Context, Result),
				TEXT("Foreach increment must be its canonical ExprStmt phase")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("foreach-increment-kind")));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(WrongIncrement)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForIncrementExprV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOR, Tu, asCSourceRange());
			const asASTStmtId Init = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Increment = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Init)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Increment)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID,
				asCASTVerify(Context, Result),
				TEXT("a present For increment phase requires its exact Expr")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("for-increment-expr")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForeachIncrementExprV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOREACH, Tu, asCSourceRange());
			const asASTStmtId Init = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Increment = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Init)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Increment)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID,
				asCASTVerify(Context, Result),
				TEXT("the canonical Foreach increment phase requires its exact Expr")));
			ASSERT_THAT(IsTrue(
				Result.detail.Equals("foreach-increment-expr")));
		}
	}

	TEST_METHOD(RejectsStructuredTransferPayloads)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"BreakExprPayloadV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Break = Context.CreateStmt(
				asAST_STMT_BREAK, Tu, asCSourceRange());
			const asASTExprId Payload = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Break)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Break, Loop)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Break, Payload)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("Break must be an expression-free transfer leaf")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("transfer-expr")));
			ASSERT_THAT(IsTrue(Result.offendingNode
				== asSASTNodeRef::FromStmt(Break)));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_EXPR,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromExpr(Payload)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ContinueDeclPayloadV");
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_FOR, Tu, asCSourceRange());
			const asASTStmtId Init = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			const asASTStmtId Continue = Context.CreateStmt(
				asAST_STMT_CONTINUE, Tu, asCSourceRange());
			const asASTDeclId Payload = Context.CreateDecl(
				asAST_DECL_VAR, Tu, asCSourceRange(), "Payload");
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Init)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Continue)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Continue, Loop)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtDecl(Continue, Payload)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("Continue must be a declaration-free transfer leaf")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("transfer-decl")));
			ASSERT_THAT(IsTrue(Result.offendingNode
				== asSASTNodeRef::FromStmt(Continue)));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_DECL,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromDecl(Payload)));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"FallthroughChildPayloadV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId FirstValue = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTExprId SecondValue = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId FirstCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId SecondCase = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			const asASTStmtId Fallthrough = Context.CreateStmt(
				asAST_STMT_FALLTHROUGH, Tu, asCSourceRange());
			const asASTStmtId Payload = Context.CreateStmt(
				asAST_STMT_EXPR, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(FirstValue, 0)));
			ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(SecondValue, 1)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(FirstCase, FirstValue)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(SecondCase, SecondValue)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, FirstCase)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, SecondCase)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(FirstCase, Switch)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(SecondCase, Switch)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(
				FirstCase, Fallthrough)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(
				Fallthrough, Payload)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(
				Fallthrough, SecondCase)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerify(Context, Result),
				TEXT("Fallthrough must not own executable structural children")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("transfer-child")));
			ASSERT_THAT(IsTrue(Result.offendingNode
				== asSASTNodeRef::FromStmt(Fallthrough)));
			ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD,
				(int)Result.edgeRole));
			ASSERT_THAT(IsTrue(Result.relatedNode
				== asSASTNodeRef::FromStmt(Payload)));
		}
	}

	TEST_METHOD(RejectsMissingControlConditionsBeforePublication)
	{
		struct FCase
		{
			asEASTStmtKind Kind;
			const char* Detail;
		};
		const FCase Cases[] = {
			{ asAST_STMT_IF, "if-condition" },
			{ asAST_STMT_WHILE, "while-condition" },
			{ asAST_STMT_DO_WHILE, "do-while-condition" },
			{ asAST_STMT_SWITCH, "switch-condition" },
		};

		for( asUINT CaseIndex = 0;
			CaseIndex < sizeof(Cases) / sizeof(Cases[0]); ++CaseIndex )
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MissingControlConditionV");
			const asASTStmtId Control = Context.CreateStmt(
				Cases[CaseIndex].Kind, Tu, asCSourceRange());
			const asEASTStmtKind BodyKind =
				Cases[CaseIndex].Kind == asAST_STMT_SWITCH
					? asAST_STMT_CASE
					: asAST_STMT_BLOCK;
			const asASTStmtId Body = Context.CreateStmt(
				BodyKind, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Control, Body)));
			if( Cases[CaseIndex].Kind == asAST_STMT_SWITCH )
			{
				ASSERT_THAT(AreEqual(0, Context.SetTarget(Body, Control)));
				ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
					Control, asAST_SAFEPOINT_STATEMENT)));
				ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
					Body, asAST_SAFEPOINT_STATEMENT)));
			}
			else if( Cases[CaseIndex].Kind == asAST_STMT_IF )
			{
				ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
					Control, asAST_SAFEPOINT_STATEMENT)));
			}
			else
			{
				ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
					Control, asAST_SAFEPOINT_LOOP_BACKEDGE)));
				ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
					Control, asAST_SAFEPOINT_LOOP_ENTRY)));
			}

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID,
				asCASTVerifyPublication(Context, Result),
				TEXT("a published conditional control must retain its exact typed condition")));
			ASSERT_THAT(IsTrue(Result.detail.Equals(Cases[CaseIndex].Detail),
				TEXT("missing condition must keep its kind-specific detail token")));
		}
	}

	TEST_METHOD(RejectsForgedStructuredControlSafePointRolesBeforePublication)
	{
		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForgedLoopSafePointV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_ENTRY)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Loop, asAST_SAFEPOINT_STATEMENT)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerifyPublication(Context, Result),
				TEXT("published loops require the exact LoopBackedge role")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("loop-safepoint")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForgedTransferSafePointV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			const asASTStmtId Break = Context.CreateStmt(
				asAST_STMT_BREAK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Body, Break)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Break, Loop)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_BACKEDGE)));
			ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_ENTRY)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Break, asAST_SAFEPOINT_RETURN)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerifyPublication(Context, Result),
				TEXT("published Break/Continue statements require the Transfer role")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("transfer-safepoint")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"MissingLoopBodyEdgeSafePointV");
			const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);
			const asASTExprId Condition = Context.CreateExpr(
				asAST_EXPR_BOOL_LITERAL, BoolType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Loop = Context.CreateStmt(
				asAST_STMT_WHILE, Tu, asCSourceRange());
			const asASTStmtId Body = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Loop, Condition)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Loop, Body)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Loop, asAST_SAFEPOINT_LOOP_BACKEDGE)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerifyPublication(Context, Result),
				TEXT("published loops require LoopEntry on the exact body edge")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("loop-body-safepoint")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForgedNonLoopBodyEdgeSafePointV");
			const asASTStmtId Block = Context.CreateStmt(
				asAST_STMT_BLOCK, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetLoopBodySafePointRole(
				Block, asAST_SAFEPOINT_LOOP_ENTRY)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerifyPublication(Context, Result),
				TEXT("only loop statements may carry a body-edge safe point")));
			ASSERT_THAT(IsTrue(
				Result.detail.Equals("non-loop-body-safepoint")));
		}

		{
			asCASTContext Context;
			const asASTDeclId Tu = Context.CreateTranslationUnit(
				"ForgedSwitchCaseSafePointV");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTExprId Selector = Context.CreateExpr(
				asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE,
				asCSourceRange());
			const asASTStmtId Switch = Context.CreateStmt(
				asAST_STMT_SWITCH, Tu, asCSourceRange());
			const asASTStmtId Case = Context.CreateStmt(
				asAST_STMT_CASE, Tu, asCSourceRange());
			ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Switch, Selector)));
			ASSERT_THAT(AreEqual(0, Context.AddStmtChild(Switch, Case)));
			ASSERT_THAT(AreEqual(0, Context.SetTarget(Case, Switch)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Switch, asAST_SAFEPOINT_STATEMENT)));
			ASSERT_THAT(AreEqual(0, Context.SetStmtSafePointRole(
				Case, asAST_SAFEPOINT_TRANSFER)));

			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(0, Context.Seal()));
			ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
				asCASTVerifyPublication(Context, Result),
				TEXT("published Case nodes require their authored Statement role")));
			ASSERT_THAT(IsTrue(Result.detail.Equals("case-safepoint")));
		}
	}

	TEST_METHOD(RejectsSealedPublicationCallWithoutResolvedDecl)
	{
		asCASTContext Missing;
		const asASTDeclId Tu = Missing.CreateTranslationUnit("PubCallMissing");
		const asCQualType IntType = Missing.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Missing.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
		const asASTStmtId Body = Missing.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
		const asASTExprId Call = Missing.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Missing.SetBody(Fn, Body), TEXT("set body")));
		ASSERT_THAT(AreEqual(0, Missing.SetStmtExpr(Body, Call), TEXT("return CALL without callee")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Missing, Result),
			TEXT("HARD NO: unsealed asCASTVerify must succeed without CALL resolvedDecl")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_UNSEALED_PUBLICATION, asCASTVerifyPublication(Missing, Result),
			TEXT("unsealed publication stays UNSEALED_PUBLICATION, not call-decl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("unsealed-publication"),
			TEXT("detail token must stay unsealed-publication")));
		ASSERT_THAT(AreEqual(0, Missing.Seal(),
			TEXT("Seal() uses asCASTVerify; CALL without callee must still seal for diagnostics")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerifyPublication(Missing, Result),
			TEXT("sealed publication must refuse CALL without resolvedDecl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-decl"), TEXT("detail token must be call-decl")));

		asCASTContext Present;
		const asASTDeclId PresentTu = Present.CreateTranslationUnit("PubCallPresent");
		const asCQualType PresentInt = Present.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee = Present.CreateDecl(asAST_DECL_FUNCTION, PresentTu, asCSourceRange(), "Callee");
		const asASTDeclId PresentFn = Present.CreateDecl(asAST_DECL_FUNCTION, PresentTu, asCSourceRange(), "F");
		const asASTStmtId PresentBody = Present.CreateStmt(asAST_STMT_RETURN, PresentFn, asCSourceRange());
		const asASTExprId PresentCall = Present.CreateExpr(asAST_EXPR_CALL, PresentInt, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Present.SetBody(PresentFn, PresentBody), TEXT("set present body")));
		ASSERT_THAT(AreEqual(0, Present.SetStmtExpr(PresentBody, PresentCall), TEXT("return CALL")));
		ASSERT_THAT(AreEqual(0, Present.SetResolvedDecl(PresentCall, Callee), TEXT("publication target present")));
		ASSERT_THAT(AreEqual(0, Present.SetExprCallDispatch(
			PresentCall, asAST_CALL_DISPATCH_DIRECT), TEXT("publication dispatch present")));
		ASSERT_THAT(AreEqual(0, Present.Seal(), TEXT("CALL with resolvedDecl must seal")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerifyPublication(Present, Result),
			TEXT("sealed CALL with a resolving target must publish")));
	}

	TEST_METHOD(RejectsCallArgumentFormalMismatch)
	{
		// Production mutation caught: a same-typed reverse child can be paired
		// with the wrong ParamDecl while arity, type and callee remain valid. The
		// publication verifier must authenticate the exact formal edge.
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("CallFormalMismatch");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Pack");
		const asASTDeclId A = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, asCSourceRange(), "A");
		const asASTDeclId B = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, asCSourceRange(), "B");
		const asASTDeclId Entry = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Entry");
		const asASTStmtId Return = Context.CreateStmt(
			asAST_STMT_RETURN, Entry, asCSourceRange());
		const asASTExprId ArgA = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId ArgB = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Call = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Callee, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(A, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(B, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Entry, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(ArgA, 1)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(ArgB, 2)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, ArgB)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, ArgA)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Call, Callee)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			Call, asAST_CALL_DISPATCH_DIRECT)));

		asSASTCallArgument WrongB;
		WrongB.expression = ArgB;
		WrongB.formal = A;
		WrongB.formalType = IntType;
		WrongB.formalIndex = 1;
		WrongB.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		WrongB.sourceOrdinal = 1;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, WrongB)));
		asSASTCallArgument ExactA;
		ExactA.expression = ArgA;
		ExactA.formal = A;
		ExactA.formalType = IntType;
		ExactA.formalIndex = 0;
		ExactA.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		ExactA.sourceOrdinal = 0;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, ExactA)));
		ASSERT_THAT(AreEqual(0, Context.SetBody(Entry, Return)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Return, Call)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("ordinary seal keeps malformed semantic facts for publication diagnostics")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
			asCASTVerifyPublication(Context, Result),
			TEXT("a record claiming formal slot B while pointing at A must fail closed")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-argument-formal"),
			TEXT("formal mismatch must keep a stable verifier detail token")));
	}

	TEST_METHOD(RejectsDuplicateCallArgumentFormal)
	{
		// Production mutation caught: two valid children may both claim the same
		// exact formal, leaving another required formal uncovered. This must fail
		// before CodeGen can pair children with Runtime parameter positions.
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("CallDuplicateFormal");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Callee = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Pack");
		const asASTDeclId A = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, asCSourceRange(), "A");
		const asASTDeclId B = Context.CreateDecl(
			asAST_DECL_PARAM, Callee, asCSourceRange(), "B");
		const asASTDeclId Entry = Context.CreateDecl(
			asAST_DECL_FUNCTION, Tu, asCSourceRange(), "Entry");
		const asASTStmtId Return = Context.CreateStmt(
			asAST_STMT_RETURN, Entry, asCSourceRange());
		const asASTExprId ArgA = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId ArgB = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Call = Context.CreateExpr(
			asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Callee, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(A, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(B, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Entry, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(ArgA, 1)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(ArgB, 2)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, ArgB)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Call, ArgA)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(Call, Callee)));
		ASSERT_THAT(AreEqual(0, Context.SetExprCallDispatch(
			Call, asAST_CALL_DISPATCH_DIRECT)));

		asSASTCallArgument ExactB;
		ExactB.expression = ArgB;
		ExactB.formal = B;
		ExactB.formalType = IntType;
		ExactB.formalIndex = 1;
		ExactB.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		ExactB.sourceOrdinal = 1;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, ExactB)));
		asSASTCallArgument DuplicateB;
		DuplicateB.expression = ArgA;
		DuplicateB.formal = B;
		DuplicateB.formalType = IntType;
		DuplicateB.formalIndex = 1;
		DuplicateB.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		DuplicateB.sourceOrdinal = 0;
		ASSERT_THAT(AreEqual(0, Context.AddCallArgument(Call, DuplicateB)));
		ASSERT_THAT(AreEqual(0, Context.SetBody(Entry, Return)));
		ASSERT_THAT(AreEqual(0, Context.SetStmtExpr(Return, Call)));
		ASSERT_THAT(AreEqual(0, Context.Seal()));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
			asCASTVerifyPublication(Context, Result),
			TEXT("duplicate exact formal identities must fail publication")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-argument-duplicate-formal"),
			TEXT("duplicate formal rejection must keep a stable detail token")));
	}

	TEST_METHOD(RejectsMissingCallArgumentCoverage)
	{
		// Production mutation caught: a direct call may keep a structurally
		// valid reverse-formal child list while omitting every provenance record.
		asCASTContext Context;
		ASSERT_THAT(IsTrue(BuildIncompleteCallArgumentFixture(Context, false),
			TEXT("missing-record fixture must build")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("ordinary seal must preserve missing semantic facts for publication diagnostics")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
			asCASTVerifyPublication(Context, Result),
			TEXT("a direct call with no provenance records must fail publication")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-argument-count"),
			TEXT("missing call provenance must keep a stable detail token")));
	}

	TEST_METHOD(RejectsConstructMissingCallArgumentCoverage)
	{
		// Production mutation caught: CONSTRUCT used to retain only its arranged
		// children, so a backend could infer constructor ABI slots from child
		// position after Sema had discarded named/default source provenance.
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit(
			"ConstructMissingRecords");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType ObjectType = Context.InternNamedType(
			asAST_TYPE_VALUE_OBJECT, "FConstructMissingRecords", 0);
		const asASTDeclId Class = Context.CreateDecl(
			asAST_DECL_CLASS, Tu, asCSourceRange(),
			"FConstructMissingRecords");
		const asASTDeclId Constructor = Context.CreateDecl(
			asAST_DECL_CONSTRUCTOR, Class, asCSourceRange(),
			"FConstructMissingRecords");
		const asASTDeclId Formal = Context.CreateDecl(
			asAST_DECL_PARAM, Constructor, asCSourceRange(), "Value");
		const asASTExprId Argument = Context.CreateExpr(
			asAST_EXPR_INTEGER_LITERAL, IntType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId Construct = Context.CreateExpr(
			asAST_EXPR_CONSTRUCT, ObjectType,
			asAST_VALUE_PRVALUE, asCSourceRange());
		ASSERT_THAT(IsTrue(Tu.IsValid() && IntType.IsValid()
			&& ObjectType.IsValid() && Class.IsValid()
			&& Constructor.IsValid() && Formal.IsValid()
			&& Argument.IsValid() && Construct.IsValid(),
			TEXT("missing CONSTRUCT provenance fixture identities must exist")));
		ASSERT_THAT(AreEqual(0, Context.SetStableKey(
			Class, "FConstructMissingRecords")));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Class, ObjectType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Constructor, ObjectType)));
		ASSERT_THAT(AreEqual(0, Context.SetDeclType(Formal, IntType)));
		ASSERT_THAT(AreEqual(0, Context.SetLiteralBits(Argument, 7)));
		ASSERT_THAT(AreEqual(0, Context.AddExprChild(Construct, Argument)));
		ASSERT_THAT(AreEqual(0, Context.SetResolvedDecl(
			Construct, Constructor)));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("ordinary Seal must retain missing CONSTRUCT provenance for publication diagnostics")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
			asCASTVerifyPublication(Context, Result),
			TEXT("a resolved CONSTRUCT with no formal provenance must fail publication")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-argument-count"),
			TEXT("missing CONSTRUCT provenance must reuse the stable call relation token")));
	}

	TEST_METHOD(RejectsPartialCallArgumentCoverage)
	{
		// Production mutation caught: a valid record for formal B must not make
		// publication accept an uncovered formal A and let CodeGen reconstruct it
		// from the remaining reverse-formal child position.
		asCASTContext Context;
		ASSERT_THAT(IsTrue(BuildIncompleteCallArgumentFixture(Context, true),
			TEXT("partial-record fixture must build")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("ordinary seal must preserve partial semantic facts for publication diagnostics")));

		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD,
			asCASTVerifyPublication(Context, Result),
			TEXT("a direct call with partial provenance records must fail publication")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-argument-count"),
			TEXT("partial call provenance must keep a stable detail token")));
	}

	TEST_METHOD(RejectsCallArgumentProvenanceMismatches)
	{
		// One risk-clustered gate pins every relationship that Sidecar V9 and
		// CodeGen are allowed to trust. Structural Seal remains deliberately
		// permissive so each forged graph reaches publication diagnostics.
		struct FCase
		{
			ECallArgumentProvenanceMutation Mutation;
			int Category;
			const char* Detail;
		};
		const FCase Cases[] =
		{
			{ CALL_PROVENANCE_INVALID_ORIGIN,
				asAST_VERIFY_INVALID_CHILD, "call-argument-origin" },
			{ CALL_PROVENANCE_NAMED_WITHOUT_NAME,
				asAST_VERIFY_INVALID_CHILD, "call-argument-provenance" },
			{ CALL_PROVENANCE_POSITIONAL_WITH_NAME,
				asAST_VERIFY_INVALID_CHILD, "call-argument-provenance" },
			{ CALL_PROVENANCE_SYNTHESIZED_WITH_SOURCE,
				asAST_VERIFY_INVALID_CHILD, "call-argument-provenance" },
			{ CALL_PROVENANCE_DUPLICATE_SOURCE_ORDINAL,
				asAST_VERIFY_INVALID_CHILD, "call-argument-source-order" },
			{ CALL_PROVENANCE_FOREIGN_RANGE,
				asAST_VERIFY_INVALID_RANGE, "call-argument-range" },
			{ CALL_PROVENANCE_WRONG_NAMED_FORMAL,
				asAST_VERIFY_INVALID_CHILD, "call-argument-provenance" },
			{ CALL_PROVENANCE_HIDDEN_NON_HIDDEN_FORMAL,
				asAST_VERIFY_INVALID_CHILD, "call-argument-hidden" },
			{ CALL_PROVENANCE_DEFAULT_WITHOUT_INITIALIZER,
				asAST_VERIFY_INVALID_CHILD, "call-argument-default" },
			{ CALL_PROVENANCE_IMPLICIT_RECEIVER_WITHOUT_RECEIVER,
				asAST_VERIFY_INVALID_CHILD, "call-argument-receiver" },
		};

		for( asUINT Index = 0; Index < sizeof(Cases) / sizeof(Cases[0]); ++Index )
		{
			asCASTContext Context;
			ASSERT_THAT(IsTrue(BuildCallArgumentProvenanceFixture(
				Context, Cases[Index].Mutation),
				TEXT("provenance mutation fixture must build")));
			ASSERT_THAT(AreEqual(0, Context.Seal(),
				TEXT("ordinary Seal must retain forged provenance for publication diagnostics")));
			asSAstVerifyResult Result;
			ASSERT_THAT(AreEqual(Cases[Index].Category,
				asCASTVerifyPublication(Context, Result),
				TEXT("publication must reject impossible CallArgument provenance")));
			ASSERT_THAT(IsTrue(Result.detail.Equals(Cases[Index].Detail),
				TEXT("provenance rejection must keep its stable detail token")));
		}

		asCASTContext Valid;
		ASSERT_THAT(IsTrue(BuildCallArgumentProvenanceFixture(
			Valid, CALL_PROVENANCE_VALID),
			TEXT("valid provenance fixture must build")));
		ASSERT_THAT(AreEqual(0, Valid.Seal()));
		asSAstVerifyResult ValidResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK,
			asCASTVerifyPublication(Valid, ValidResult),
			TEXT("complete valid provenance must remain publishable")));
	}

	TEST_METHOD(RejectsSealedPublicationConstructWithoutResolvedDecl)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("PubConstructMissing");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		Context.CreateExpr(asAST_EXPR_CONSTRUCT, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Context, Result),
			TEXT("HARD NO: unsealed CONSTRUCT without resolvedDecl must asCASTVerify")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("CONSTRUCT without callee must still seal for diagnostics")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerifyPublication(Context, Result),
			TEXT("sealed publication must refuse CONSTRUCT without resolvedDecl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("construct-decl"), TEXT("detail token must be construct-decl")));
	}

	TEST_METHOD(RejectsSealedPublicationDeclRefWithoutResolvedDecl)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("PubDeclRefMissing");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		Context.CreateExpr(asAST_EXPR_DECL_REF, IntType, asAST_VALUE_LVALUE, asCSourceRange());
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_OK, asCASTVerify(Context, Result),
			TEXT("HARD NO: unsealed DECL_REF without resolvedDecl must asCASTVerify")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("DECL_REF without target must still seal for diagnostics")));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerifyPublication(Context, Result),
			TEXT("sealed publication must refuse DECL_REF without resolvedDecl")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("declref-decl"), TEXT("detail token must be declref-decl")));
	}
};

#endif
