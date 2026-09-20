#include "CQTest.h"

#include "NativeEngine/NativeEngineTestSupport.h"
#include "Core/AngelscriptDescriptors.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Compile/as_definition_consumer.h"
#include "frontend/Compile/as_descriptor_consumer.h"
#include "frontend/Lexer/as_preprocess_result.h"
#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateRegisterTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	bool ProjectDelegates(FSessionRun& Run, AS_NAMESPACE_QUALIFIER FAngelscriptPreprocessResult& Out)
	{
		if (!Run.ResolveDeclarations()) return false;
		if (!Run.Session->AnalyzeBodies(1)) return false;
		return AS_NAMESPACE_QUALIFIER FAngelscriptDescriptorConsumer().Project(*Run.Session, Run.SourceManager, Out, true);
	}

	asCTypeInfo* FindType(const asCDefinitions& Set, const char* Name)
	{
		for (asCTypeInfo* Type : Set.GetTypes())
		{
			if (Type && Type->name == Name) return Type;
		}
		return nullptr;
	}
}

TEST_CLASS_WITH_FLAGS(DelegateRegister, "Angelscript.UnitTest.NativeEngine.Definitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OrdinaryDescFlavor)
	{
		DelegateRegisterTests::FSessionRun Run({{TEXT("Ordinary.as"), TEXT("DECLARE_DELEGATE_OneParam(FOnDone, int);")}});
		AS_NAMESPACE_QUALIFIER FAngelscriptPreprocessResult Result;
		ASSERT_THAT(IsTrue(DelegateRegisterTests::ProjectDelegates(Run, Result)));
		ASSERT_THAT(AreEqual(1, Result.GetModules().Num()));
		ASSERT_THAT(AreEqual(1, Result.GetModules()[0]->Delegates.Num()));
		const TSharedRef<FAngelscriptDelegateDesc>& Delegate = Result.GetModules()[0]->Delegates[0];
		ASSERT_THAT(IsFalse(Delegate->bIsDynamic));
		ASSERT_THAT(IsFalse(Delegate->bIsMulticast));
		ASSERT_THAT(IsTrue(Delegate->Signature.IsValid()));
		ASSERT_THAT(AreEqual(1, Delegate->Signature->Arguments.Num()));
		ASSERT_THAT(IsNull(Delegate->Function));
	}

	TEST_METHOD(DynamicDescNames)
	{
		DelegateRegisterTests::FSessionRun Run({{TEXT("Dynamic.as"), TEXT("DECLARE_DYNAMIC_DELEGATE_OneParam(FDynOne, int, Value);")}});
		AS_NAMESPACE_QUALIFIER FAngelscriptPreprocessResult Result;
		ASSERT_THAT(IsTrue(DelegateRegisterTests::ProjectDelegates(Run, Result)));
		ASSERT_THAT(AreEqual(1, Result.GetModules()[0]->Delegates.Num()));
		const TSharedRef<FAngelscriptDelegateDesc>& Delegate = Result.GetModules()[0]->Delegates[0];
		ASSERT_THAT(IsTrue(Delegate->bIsDynamic));
		ASSERT_THAT(AreEqual(FString(TEXT("Value")), Delegate->Signature->Arguments[0].ArgumentName));
	}

	TEST_METHOD(CreateCallableTypeExists)
	{
		DelegateRegisterTests::FSessionRun Run({{TEXT("Ordinary.as"), TEXT("DECLARE_DELEGATE_OneParam(FOnDone, int);")}});
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		auto Consumed = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Run.Session);
		ASSERT_THAT(IsTrue(Consumed.Succeeded()));
		auto* Type = static_cast<asCCallableType*>(DelegateRegisterTests::FindType(*Consumed.Definitions, "FOnDone"));
		ASSERT_THAT(IsNotNull(Type));
		auto* Signature = static_cast<asCScriptFunction*>(Type->GetCallableSignature());
		ASSERT_THAT(IsNotNull(Signature));
		ASSERT_THAT(IsTrue(Signature->funcType == asFUNC_CALLABLE_SIGNATURE));
		ASSERT_THAT(AreEqual(1, Signature->parameterTypes.GetLength()));
	}
};

#endif
