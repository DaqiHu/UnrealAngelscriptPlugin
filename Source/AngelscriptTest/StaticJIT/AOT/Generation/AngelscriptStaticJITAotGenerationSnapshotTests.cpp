#include "CQTest.h"

#include "AngelscriptEngine.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotGenerationFactsTests,
	"Angelscript.TestModule.StaticJIT.AOT.GenerationFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LargeFixtureBuildsVerifiedFactsWithoutCacheRecords)
	{
		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
				AngelscriptStaticJITAotGeneration::
					CreateInterpreterFixtureEngineSession();
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(IsTrue(Session->IsValid()));
		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
				Session->Engine->GetStaticJITGenerationSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(IsTrue(Snapshot->IsComplete()));
		ASSERT_THAT(AreEqual(0u, Snapshot->InputCacheRecordCount));
		const FAngelscriptStaticJITGenerationModule* const FixtureModule =
			Snapshot->Modules.FindByPredicate([](const auto& Module)
			{
				return Module.CanonicalModuleName == TEXT("ASStaticJITAotFixture");
			});
		ASSERT_THAT(IsNotNull(FixtureModule));
		int32 FixtureFunctionCount = 0;
		for (const FAngelscriptStaticJITGenerationFunction& Function
			: Snapshot->Functions)
		{
			if (Function.ModuleKey == FixtureModule->ModuleKey)
			{
				++FixtureFunctionCount;
			}
		}
		const TSharedPtr<FAngelscriptModuleDesc> LiveFixtureModule =
			Session->Engine->GetModuleByModuleName(
				FixtureModule->CanonicalModuleName);
		ASSERT_THAT(IsTrue(LiveFixtureModule.IsValid()));
		ASSERT_THAT(IsNotNull(LiveFixtureModule->ScriptModule));
		// The isolated generation Engine intentionally does not publish the
		// process-facing FunctionRoute snapshot. Compare against its complete VM
		// module function table instead, so this remains independent of Cache V2
		// records and automatically follows authored/generated fixture growth.
		int32 ExpectedFixtureFunctionCount = 0;
		for (asUINT FunctionIndex = 0;
			FunctionIndex
				< LiveFixtureModule->ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			const asCScriptFunction* const Function =
				LiveFixtureModule->ScriptModule->scriptFunctions[FunctionIndex];
			if (Function != nullptr
				&& Function->module == LiveFixtureModule->ScriptModule)
			{
				++ExpectedFixtureFunctionCount;
			}
		}
		ASSERT_THAT(AreEqual(
			ExpectedFixtureFunctionCount, FixtureFunctionCount));
		const FAngelscriptStaticJITGenerationFunction* const SemanticFunction =
			Snapshot->Functions.FindByPredicate(
				[FixtureModule](const auto& Function)
				{
					return Function.ModuleKey == FixtureModule->ModuleKey
						&& Function.CanonicalDeclaration.Contains(
							TEXT("SemanticScalarBranch"));
				});
		ASSERT_THAT(IsNotNull(SemanticFunction));
		ASSERT_THAT(IsTrue(
			SemanticFunction->bHasVerifiedArtifactIdentity));
		ASSERT_THAT(IsTrue(
			SemanticFunction->bHasVerifiedArtifactReferences));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
