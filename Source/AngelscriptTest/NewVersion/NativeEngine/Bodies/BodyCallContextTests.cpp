#include "CQTest.h"
#include "../NativeEngineTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace BodyCallContextTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;
	TEST_CLASS_WITH_FLAGS(BodiesCallContext, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ImmediateAnonymousFunctionInDefaultsIsRejected)
		{
			FSessionRun Run({{TEXT("ImmediateDefaults.as"),
				TEXT("void Edit() defaults; void F() defaults { function() { Edit(); }(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(EscapingLambdaDoesNotKeepDefaultsPrivilege)
		{
			FSessionRun Run({{TEXT("EscapingDefaults.as"),
				TEXT("delegate void Work(); void Edit() defaults; void F() defaults { Work Op = function() { Edit(); }; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(ConstructorImmediateLambdaRejectsUnsafeDuringConstruction)
		{
			FSessionRun Run({{TEXT("CtorImmediateUnsafe.as"),
				TEXT("void Unsafe() unsafe_during_construction; class Item { Item() { function() { Unsafe(); }(); } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(OrdinaryFunctionMayCallUnsafeDuringConstruction)
		{
			FSessionRun Run({{TEXT("OrdinaryUnsafe.as"),
				TEXT("void Unsafe() unsafe_during_construction; void F() { Unsafe(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(NestedImmediateLambdasPreserveConstructionContext)
		{
			FSessionRun Run({{TEXT("NestedImmediateUnsafe.as"),
				TEXT("void Unsafe() unsafe_during_construction; class Item { Item() { function() { function() { Unsafe(); }(); }(); } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}
	};
}
#endif
