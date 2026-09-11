#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_sidecar.h"
#include "as_builder.h"
#include "frontend/as_decl.h"
#include "as_module.h"
#include "frontend/as_parser.h"
#include "as_scriptengine.h"
#include "frontend/as_sema.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// AST-first interface-dispatch gate. These tests intentionally inspect sealed
// declaration identities before Runtime shells, vtable slots, or VM execution
// are allowed to count as evidence.
TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaInterfaceDispatchTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.InterfaceDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const asCDecl* FindDecl(
		const asCASTContext& Context,
		asEASTDeclKind Kind,
		const char* StableKey)
	{
		const asCDecl* Match = nullptr;
		for (asUINT Index = 1; Index <= Context.GetDeclCount(); ++Index)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(Index));
			if (Decl != nullptr
				&& Decl->kind == Kind
				&& Decl->stableKey.Equals(StableKey))
			{
				if (Match != nullptr)
				{
					return nullptr;
				}
				Match = Decl;
			}
		}
		return Match;
	}

	static bool HasRelation(
		const asCASTContext& Context,
		const asCDecl* Owner,
		asEASTMethodRelationKind Kind,
		const char* ImplementationKey,
		const char* RequirementKey)
	{
		if (Owner == nullptr)
		{
			return false;
		}
		for (asUINT Index = 0; Index < Owner->methodRelations.GetLength(); ++Index)
		{
			const asSASTMethodRelation& Relation = Owner->methodRelations[Index];
			const asCDecl* const Implementation =
				Context.GetDecl(Relation.implementation);
			const asCDecl* const Requirement =
				Context.GetDecl(Relation.requirement);
			if (Relation.kind == Kind
				&& Implementation != nullptr
				&& Requirement != nullptr
				&& Implementation->stableKey.Equals(ImplementationKey)
				&& Requirement->stableKey.Equals(RequirementKey))
			{
				return true;
			}
		}
		return false;
	}

	static bool HasDiagnosticPrefix(
		const asCSema& Sema,
		const char* Prefix)
	{
		const asCArray<asCString>& Diagnostics = Sema.GetDiagnostics();
		for (asUINT Index = 0; Index < Diagnostics.GetLength(); ++Index)
		{
			const char* const Text = Diagnostics[Index].AddressOf();
			if (Text != nullptr
				&& FCStringAnsi::Strncmp(
					Text,
					Prefix,
					FCStringAnsi::Strlen(Prefix)) == 0)
			{
				return true;
			}
		}
		return false;
	}

public:
	TEST_METHOD(SidecarRoundTripPreservesExactMethodRelations)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(
			Engine, "CanonicalInterfaceRelationSidecar");
		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("relation sidecar module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IResolve
			{
				int Resolve(int Value);
			}

			class BaseResolver
			{
				int Resolve(int Value)
				{
					return Value;
				}
			}

			class Resolver : BaseResolver, IResolve
			{
				int Resolve(int Value) override
				{
					return Value + 1;
				}
			}
			)AS");
		Code.SetCode(
			"CanonicalInterfaceRelationSidecar.as",
			ScriptSource.c_str(),
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			Sema.GetDiagnostics().GetLength()));
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.Seal()));

		const asCDecl* const OriginalOwner =
			FindDecl(Context, asAST_DECL_CLASS, "Resolver");
		const asCDecl* const OriginalImplementation =
			FindDecl(Context, asAST_DECL_METHOD, "Resolver::Resolve(int)");
		const asCDecl* const OriginalBaseRequirement =
			FindDecl(Context, asAST_DECL_METHOD, "BaseResolver::Resolve(int)");
		const asCDecl* const OriginalRequirement =
			FindDecl(Context, asAST_DECL_METHOD, "IResolve::Resolve(int)");
		ASSERT_THAT(IsNotNull(OriginalOwner));
		ASSERT_THAT(IsNotNull(OriginalImplementation));
		ASSERT_THAT(IsNotNull(OriginalBaseRequirement));
		ASSERT_THAT(IsNotNull(OriginalRequirement));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			OriginalOwner != nullptr
				? OriginalOwner->methodRelations.GetLength() : 0));

		asSASTSidecarHeader Header;
		Header.functionKey = "CanonicalInterfaceRelationSidecar";
		Header.profile = "Win64";
		Header.version = asAST_SIDECAR_SCHEMA_VERSION;
		asCArray<asBYTE> Bytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, Bytes),
			TEXT("encode sealed method relations")));
		asCArray<asBYTE> RepeatedBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Context, Header, RepeatedBytes),
			TEXT("repeat encoding the same sealed snapshot")));
		ASSERT_THAT(AreEqual(Bytes.GetLength(), RepeatedBytes.GetLength()));
		for( asUINT Index = 0; Index < Bytes.GetLength(); ++Index )
		{
			ASSERT_THAT(AreEqual(
				(int)Bytes[Index],
				(int)RepeatedBytes[Index],
				TEXT("V8 relation encoding must be byte deterministic")));
		}

		asCASTContext Restored;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTDecodeSidecar(
				Restored, Header, Bytes.AddressOf(), Bytes.GetLength()),
			TEXT("decode method relations without re-running Sema")));
		const asCDecl* const RestoredOwner =
			FindDecl(Restored, asAST_DECL_CLASS, "Resolver");
		const asCDecl* const RestoredImplementation =
			FindDecl(Restored, asAST_DECL_METHOD, "Resolver::Resolve(int)");
		const asCDecl* const RestoredBaseRequirement =
			FindDecl(Restored, asAST_DECL_METHOD, "BaseResolver::Resolve(int)");
		const asCDecl* const RestoredRequirement =
			FindDecl(Restored, asAST_DECL_METHOD, "IResolve::Resolve(int)");
		ASSERT_THAT(IsNotNull(RestoredOwner));
		ASSERT_THAT(IsNotNull(RestoredImplementation));
		ASSERT_THAT(IsNotNull(RestoredBaseRequirement));
		ASSERT_THAT(IsNotNull(RestoredRequirement));
		ASSERT_THAT(AreEqual(
			OriginalOwner != nullptr
				? OriginalOwner->methodRelations.GetLength() : 0,
			RestoredOwner != nullptr
				? RestoredOwner->methodRelations.GetLength() : 0,
			TEXT("Sidecar must not erase an irreducible Sema relationship")));
		if( RestoredOwner != nullptr
			&& RestoredImplementation != nullptr
			&& RestoredBaseRequirement != nullptr
			&& RestoredRequirement != nullptr
			&& RestoredOwner->methodRelations.GetLength() == 2 )
		{
			const asSASTMethodRelation& OverrideRelation =
				RestoredOwner->methodRelations[0];
			ASSERT_THAT(AreEqual(
				(int)asAST_METHOD_RELATION_BASE_OVERRIDE,
				(int)OverrideRelation.kind));
			ASSERT_THAT(IsTrue(
				OverrideRelation.implementation == RestoredImplementation->id));
			ASSERT_THAT(IsTrue(
				OverrideRelation.requirement == RestoredBaseRequirement->id));

			const asSASTMethodRelation& InterfaceRelation =
				RestoredOwner->methodRelations[1];
			ASSERT_THAT(AreEqual(
				(int)asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
				(int)InterfaceRelation.kind));
			ASSERT_THAT(IsTrue(
				InterfaceRelation.implementation == RestoredImplementation->id,
				TEXT("restored source must be the exact remapped DeclId")));
			ASSERT_THAT(IsTrue(
				InterfaceRelation.requirement == RestoredRequirement->id,
				TEXT("restored target must be the exact remapped DeclId")));
		}

		asCArray<asBYTE> RestoredBytes;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK,
			asCASTEncodeSidecar(Restored, Header, RestoredBytes),
			TEXT("re-encode restored relations")));
		ASSERT_THAT(AreEqual(Bytes.GetLength(), RestoredBytes.GetLength()));
		for( asUINT Index = 0; Index < Bytes.GetLength(); ++Index )
		{
			ASSERT_THAT(AreEqual(
				(int)Bytes[Index],
				(int)RestoredBytes[Index],
				TEXT("V8 decode/re-encode must preserve exact bytes")));
		}

		asCString OriginalDump;
		asCString RestoredDump;
		asCASTDump(Context, OriginalDump);
		asCASTDump(Restored, RestoredDump);
		ASSERT_THAT(IsTrue(OriginalDump == RestoredDump,
			TEXT("deterministic structural dump must survive V8 round-trip")));
	}

	TEST_METHOD(ParserSealsExactOverrideAndInterfaceImplementationEdges)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(
			Engine, "CanonicalInterfaceRelations");
		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("interface relation module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IBaseProbe
			{
				int Probe(int Value);
			}

			interface IDerivedProbe : IBaseProbe
			{
				int Probe(bool Value);
			}

			class BaseProbe
			{
				int Probe(int Value)
				{
					return Value + 1;
				}
			}

			class DerivedProbe : BaseProbe, IDerivedProbe
			{
				int Probe(bool Value)
				{
					return Value ? 42 : 0;
				}
			}

			class OverrideProbe : BaseProbe
			{
				int Probe(int Value) override
				{
					return Value + 2;
				}
			}
			)AS");
		Code.SetCode("CanonicalInterfaceRelations.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("interface relation fixture must parse")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Sema.GetDiagnostics().GetLength(),
			TEXT("valid interface relation fixture has no Sema diagnostics")));
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.Seal(),
			TEXT("interface relation fixture must seal")));

		const asCDecl* const Derived =
			FindDecl(Context, asAST_DECL_CLASS, "DerivedProbe");
		const asCDecl* const Override =
			FindDecl(Context, asAST_DECL_CLASS, "OverrideProbe");
		ASSERT_THAT(IsNotNull(Derived, TEXT("DerivedProbe sealed declaration")));
		ASSERT_THAT(IsNotNull(Override, TEXT("OverrideProbe sealed declaration")));
		ASSERT_THAT(IsTrue(HasRelation(
			Context,
			Derived,
			asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
			"BaseProbe::Probe(int)",
			"IBaseProbe::Probe(int)"),
			TEXT("inherited BaseProbe::Probe(int) must satisfy IBaseProbe exactly")));
		ASSERT_THAT(IsTrue(HasRelation(
			Context,
			Derived,
			asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
			"DerivedProbe::Probe(bool)",
			"IDerivedProbe::Probe(bool)"),
			TEXT("DerivedProbe::Probe(bool) must satisfy the bool requirement")));
		ASSERT_THAT(IsTrue(HasRelation(
			Context,
			Override,
			asAST_METHOD_RELATION_BASE_OVERRIDE,
			"OverrideProbe::Probe(int)",
			"BaseProbe::Probe(int)"),
			TEXT("explicit override must point at the exact base declaration")));
	}

	TEST_METHOD(InheritedClassMethodSatisfiesDerivedInterfaceRequirement)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(
			Engine, "CanonicalInheritedInterfaceImplementation");
		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("inherited implementation module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IRun
			{
				int Run();
			}

			class BaseRunner
			{
				int Run()
				{
					return 42;
				}
			}

			class DerivedRunner : BaseRunner, IRun
			{
			}
			)AS");
		Code.SetCode(
			"CanonicalInheritedInterfaceImplementation.as",
			ScriptSource.c_str(),
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Sema.GetDiagnostics().GetLength()));
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.Seal()));

		const asCDecl* const Derived =
			FindDecl(Context, asAST_DECL_CLASS, "DerivedRunner");
		ASSERT_THAT(IsTrue(HasRelation(
			Context,
			Derived,
			asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
			"BaseRunner::Run()",
			"IRun::Run()"),
			TEXT("an inherited class method remains the exact implementation")));
	}

	TEST_METHOD(OverloadedInterfaceRequirementsBindByCanonicalSignature)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(
			Engine, "CanonicalOverloadedInterfaceRelations");
		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("overloaded interface module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IOverloaded
			{
				int Resolve(int Value);
				int Resolve(bool Value);
			}

			class Overloaded : IOverloaded
			{
				int Resolve(int Value)
				{
					return 101;
				}

				int Resolve(bool Value)
				{
					return 202;
				}
			}
			)AS");
		Code.SetCode(
			"CanonicalOverloadedInterfaceRelations.as",
			ScriptSource.c_str(),
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Sema.GetDiagnostics().GetLength()));
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.Seal()));

		const asCDecl* const Overloaded =
			FindDecl(Context, asAST_DECL_CLASS, "Overloaded");
		ASSERT_THAT(IsNotNull(Overloaded, TEXT("Overloaded sealed declaration")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(2),
			Overloaded != nullptr ? Overloaded->methodRelations.GetLength() : 0,
			TEXT("each overload must own one distinct interface relation")));
		ASSERT_THAT(IsTrue(HasRelation(
			Context,
			Overloaded,
			asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
			"Overloaded::Resolve(int)",
			"IOverloaded::Resolve(int)")));
		ASSERT_THAT(IsTrue(HasRelation(
			Context,
			Overloaded,
			asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
			"Overloaded::Resolve(bool)",
			"IOverloaded::Resolve(bool)")));
	}

	TEST_METHOD(ReorderedParamChildrenDoNotChangeOverrideAndInterfaceMatching)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		const asASTDeclId TranslationUnit =
			Sema.ActOnTranslationUnit("CanonicalReorderedMethodRelations");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asCQualType BoolType = Context.InternPrimitive(ttBool, 0);

		const asASTDeclId Interface = Sema.ActOnInterfaceDecl(
			TranslationUnit, "IResolve", asCSourceRange());
		const asASTDeclId InterfaceMethod = Sema.ActOnMethodDecl(
			Interface, "Resolve", IntType, asCSourceRange());
		Sema.ActOnParamDecl(
			InterfaceMethod, "First", IntType, asCSourceRange());
		Sema.ActOnParamDecl(
			InterfaceMethod, "Second", BoolType, asCSourceRange());

		const asASTDeclId Base = Sema.ActOnClassDecl(
			TranslationUnit, "BaseResolve", asCSourceRange());
		const asASTDeclId BaseMethod = Sema.ActOnMethodDecl(
			Base, "Resolve", IntType, asCSourceRange());
		Sema.ActOnParamDecl(BaseMethod, "First", IntType, asCSourceRange());
		Sema.ActOnParamDecl(BaseMethod, "Second", BoolType, asCSourceRange());

		const asASTDeclId Derived = Sema.ActOnClassDecl(
			TranslationUnit, "DerivedResolve", asCSourceRange());
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.AddBase(Derived, Base)));
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.AddBase(Derived, Interface)));
		const asASTDeclId DerivedMethod = Sema.ActOnMethodDecl(
			Derived, "Resolve", IntType, asCSourceRange());
		const asASTDeclId First = Sema.ActOnParamDecl(
			DerivedMethod, "First", IntType, asCSourceRange());
		const asASTDeclId Second = Sema.ActOnParamDecl(
			DerivedMethod, "Second", BoolType, asCSourceRange());
		Sema.ActOnFunctionTraitsAction(
			DerivedMethod, asAST_TRAIT_OVERRIDE);

		asCDecl* const MutableMethod =
			const_cast<asCDecl*>(Context.GetDecl(DerivedMethod));
		ASSERT_THAT(IsNotNull(MutableMethod));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2),
			MutableMethod != nullptr ? MutableMethod->children.GetLength() : 0));
		if (MutableMethod == nullptr || MutableMethod->children.GetLength() != 2)
		{
			return;
		}
		MutableMethod->children[0] = Second;
		MutableMethod->children[1] = First;

		ASSERT_THAT(IsTrue(
			Context.GetFormalDecl(DerivedMethod, 0) != nullptr
				&& Context.GetFormalDecl(DerivedMethod, 0)->id == First
				&& Context.GetFormalDecl(DerivedMethod, 1) != nullptr
				&& Context.GetFormalDecl(DerivedMethod, 1)->id == Second,
			TEXT("the adversarial fixture must preserve sealed formal identity")));

		Sema.ActOnRecordFinishAction(Derived);
		const asCDecl* const DerivedDecl = Context.GetDecl(Derived);
		ASSERT_THAT(IsNotNull(DerivedDecl));
		bool HasOverride = false;
		bool HasInterface = false;
		for (asUINT RelationIndex = 0;
			DerivedDecl != nullptr
				&& RelationIndex < DerivedDecl->methodRelations.GetLength();
			++RelationIndex)
		{
			const asSASTMethodRelation& Relation =
				DerivedDecl->methodRelations[RelationIndex];
			HasOverride |=
				Relation.kind == asAST_METHOD_RELATION_BASE_OVERRIDE
				&& Relation.implementation == DerivedMethod
				&& Relation.requirement == BaseMethod;
			HasInterface |=
				Relation.kind == asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION
				&& Relation.implementation == DerivedMethod
				&& Relation.requirement == InterfaceMethod;
		}
		ASSERT_THAT(IsTrue(HasOverride,
			TEXT("override matching must join parameters by formalIndex")));
		ASSERT_THAT(IsTrue(HasInterface,
			TEXT("interface matching must join parameters by formalIndex")));
		ASSERT_THAT(AreEqual(asAST_VERIFY_OK, Context.Seal(),
			TEXT("reordered structural children remain a valid sealed graph")));
	}

	TEST_METHOD(MissingInterfaceImplementationProducesSemaDiagnostic)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(
			Engine, "CanonicalMissingInterfaceImplementation");
		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("missing implementation module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			interface IRequired
			{
				int Resolve(int Value);
			}

			class Missing : IRequired
			{
				int Resolve(bool Value)
				{
					return 0;
				}
			}
			)AS");
		Code.SetCode(
			"CanonicalMissingInterfaceImplementation.as",
			ScriptSource.c_str(),
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("Parser still recognizes the invalid semantic fixture")));
		ASSERT_THAT(IsTrue(HasDiagnosticPrefix(
			Sema, "interface-method-implementation-missing"),
			TEXT("Sema must reject a missing exact interface method")));
	}

	TEST_METHOD(ExplicitOverrideWithoutBaseTargetProducesSemaDiagnostic)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(
			Engine, "CanonicalMissingOverrideTarget");
		asCModule* const Module =
			AngelscriptBuilderTestSupport::CreateBuilderModule(
				ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("missing override target module")));

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class Standalone
			{
				int Resolve(int Value) override
				{
					return Value;
				}
			}
			)AS");
		Code.SetCode(
			"CanonicalMissingOverrideTarget.as",
			ScriptSource.c_str(),
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("Parser still recognizes the invalid override fixture")));
		ASSERT_THAT(IsTrue(HasDiagnosticPrefix(
			Sema, "method-override-target-missing"),
			TEXT("Sema must reject override without an exact base method")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
