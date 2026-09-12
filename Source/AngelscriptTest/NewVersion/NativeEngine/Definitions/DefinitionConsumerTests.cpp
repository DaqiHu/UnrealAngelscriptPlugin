#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "../Compiler/NativeSourceExecutionTestSupport.h"
#include "frontend/Compile/as_definition_consumer.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "frontend/Basic/as_identifier_table.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_scriptengine.h"
#include <thread>
#include <atomic>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(DefinitionConsumer, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FInput
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asCSourceManager SourceManager{Snapshot};
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{SourceManager};
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCCompilationSession> Session;
		bool Collected = false;
		explicit FInput(const FString& Text, TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>())
		{
const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(TEXT("Definitions.as"), Text);
			const auto File = Snapshot->AddFile(TEXT("Definitions.as"), Input.GetUtf8Bytes());
			Snapshot->Freeze();
			Session = MakeUnique<asCCompilationSession>(Snapshot, Diagnostics, TEXT("definition-tests"), Context);
			AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(SourceManager, SourceManager.MakeRange(File, 0, Input.GetUtf8Bytes().Num()),
				asSLexOptions(asEUnicodeIdentifierPolicy::AsciiOnly, asETriviaMode::RetainTrivia), Identifiers, Diagnostics);
			TArray<asCToken> Tokens;
			for (;;) { asCToken Token; if (!Tokenizer.Lex(Token)) break; Tokens.Add(Token); if (Token.Kind == asETokenKind::EndOfFile) break; }
			Tokenizer.FlushDiagnostics();
			asCPreprocessorConfiguration Configuration;
			asCPreprocessor Preprocessor(SourceManager, Configuration, Diagnostics);
			const auto Preprocessed = Preprocessor.Process(Tokens);
			Collected = Session->CollectSource(Preprocessor, Preprocessed, TEXT("Definitions.as"));
		}
	};
	struct FEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
	};
	static asCTypeInfo* FindType(const asCModuleDefinitionSet& Set, const char* Name)
	{
		for (auto* Type : Set.GetTypes()) if (Type->name == Name) return Type;
		return nullptr;
	}

public:
	TEST_METHOD(SameNamedNestedTypesKeepDistinctOwnersThroughEngineRegistration)
	{
		FInput Input(TEXT("namespace N { class Left { class Item {}; }; class Right { class Item {}; }; }"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(4, Result.Definitions->GetTypes().Num()));
		asCTypeInfo* LeftItem = nullptr;
		asCTypeInfo* RightItem = nullptr;
		for (auto* Type : Result.Definitions->GetTypes())
		{
			if (!Type->GetMetadataDeclarationOwner()) continue;
			if (Type->GetMetadataDeclarationOwner()->name == "Left") LeftItem = Type;
			if (Type->GetMetadataDeclarationOwner()->name == "Right") RightItem = Type;
		}
		ASSERT_THAT(IsNotNull(LeftItem));
		ASSERT_THAT(IsNotNull(RightItem));
		ASSERT_THAT(IsFalse(LeftItem->GetStableKey() == RightItem->GetStableKey()));
		ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Result.Definitions->Freeze() == asEMetadataResult::Succeeded));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Result.Definitions)) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(AreEqual(LeftItem, Engine.Ptr->FindMetadataType("N::Left", "Item")));
		ASSERT_THAT(AreEqual(RightItem, Engine.Ptr->FindMetadataType("N::Right", "Item")));
		ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataType("N", "Item")));
		ASSERT_THAT(AreEqual(FString(TEXT("N")), FString(UTF8_TO_TCHAR(LeftItem->GetNamespace()))));
	}

	TEST_METHOD(DuplicateNestedDeclarationInTheSameOwnerIsStillRejected)
	{
		FInput Input(TEXT("class Owner { class Item {}; class Item {}; };"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Input.Session->ResolveDeclarations(1)));
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsFalse(Result.Definitions.IsValid()));
	}

	TEST_METHOD(RequiresTheExplicitVerifiedASTBarrier)
	{
		FInput Input(TEXT("struct Value { int Number; };"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Status == AS_NAMESPACE_QUALIFIER asEDefinitionConsumerStatus::InvalidPhase));
		ASSERT_THAT(IsNull(Result.Definitions.Get()));
		ASSERT_THAT(AreEqual(1, Result.Diagnostics.Num()));
	}

	TEST_METHOD(ActualForwardRecursiveTypesAndMethodsRemainEngineFree)
	{
		FInput Input(TEXT("namespace Model { class First { Second@ Next; int Read(int Value) { return Value; } }; class Second { First@ Previous; }; }"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(2)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(2)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		auto* First = static_cast<asCObjectType*>(FindType(*Result.Definitions, "First"));
		auto* Second = static_cast<asCObjectType*>(FindType(*Result.Definitions, "Second"));
		ASSERT_THAT(IsNotNull(First));
		ASSERT_THAT(IsNotNull(Second));
		ASSERT_THAT(IsTrue(First->localProperties[0]->type.GetTypeInfo() == Second));
		ASSERT_THAT(IsTrue(Second->localProperties[0]->type.GetTypeInfo() == First));
		ASSERT_THAT(IsTrue(First->localProperties[0]->type.IsObjectHandle()));
		ASSERT_THAT(AreEqual(1, static_cast<int>(First->GetMethodCount())));
		auto* Method = static_cast<asCScriptFunction*>(First->GetMethodByIndex(0));
		ASSERT_THAT(IsTrue(Method->objectType == First));
		ASSERT_THAT(IsTrue(Method->GetId() < 0));
		ASSERT_THAT(IsNull(First->GetEngine()));
		ASSERT_THAT(IsTrue(Result.Definitions->GetState() == asEDefinitionSetState::Building));
		ASSERT_THAT(IsTrue(Result.Definitions->Freeze() == asEMetadataResult::IncompleteDefinition));
		ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Result.Definitions->Freeze() == asEMetadataResult::Succeeded));
		ASSERT_THAT(AreEqual(FString(TEXT("Model")), FString(UTF8_TO_TCHAR(First->GetNamespace()))));
	}

	TEST_METHOD(RecordLayoutUsesRealBaseInterfaceAndEnumEdges)
	{
		FInput Input(TEXT("interface Reader { int Read(); } class Base { int Head; }; class Derived : Base, Reader { bool Tail; int Read() { return 1; } }; enum Mode { Off = 2, On = 9 }"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(2)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(2)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		auto* Base = static_cast<asCObjectType*>(FindType(*Result.Definitions, "Base"));
		auto* Derived = static_cast<asCObjectType*>(FindType(*Result.Definitions, "Derived"));
		auto* Reader = static_cast<asCObjectType*>(FindType(*Result.Definitions, "Reader"));
		auto* Mode = static_cast<asCEnumType*>(FindType(*Result.Definitions, "Mode"));
		ASSERT_THAT(IsNotNull(Derived));
		ASSERT_THAT(IsTrue(Derived->derivedFrom == Base));
		ASSERT_THAT(IsTrue(Derived->GetInterface(0) == Reader));
		ASSERT_THAT(IsTrue(Reader->IsInterface()));
		ASSERT_THAT(IsNotNull(Mode));
		int EnumValue = 0;
		ASSERT_THAT(AreEqual(FString(TEXT("On")), FString(UTF8_TO_TCHAR(Mode->GetEnumValueByIndex(1, &EnumValue)))));
		ASSERT_THAT(AreEqual(9, EnumValue));
		ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(AreEqual(2, static_cast<int>(Derived->GetPropertyCount())));
		ASSERT_THAT(AreEqual(4, Derived->localProperties[0]->byteOffset));
		ASSERT_THAT(AreEqual(8, Derived->size));
	}

	TEST_METHOD(ValueCyclesFailLayoutWithoutFreezingOrPartialOffsets)
	{
		FInput Input(TEXT("struct Left { Right Next; }; struct Right { Left Previous; };"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		auto* Left = static_cast<asCObjectType*>(FindType(*Result.Definitions, "Left"));
		ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::RecursiveValueType));
		ASSERT_THAT(IsTrue(Result.Definitions->Freeze() == asEMetadataResult::IncompleteDefinition));
		ASSERT_THAT(AreEqual(-1, Left->size));
		ASSERT_THAT(IsTrue(Result.Definitions->GetState() == asEDefinitionSetState::Building));
	}

	TEST_METHOD(DelegateAndEventShareActualStructuralSignatureNotFunctionIdentity)
	{
		FInput Input(TEXT("delegate void Callback(int Value); event void Changed(int NewValue);"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		auto* Callback = static_cast<asCCallableType*>(FindType(*Result.Definitions, "Callback"));
		auto* Changed = static_cast<asCCallableType*>(FindType(*Result.Definitions, "Changed"));
		ASSERT_THAT(IsNotNull(Callback));
		ASSERT_THAT(IsNotNull(Changed));
		auto* Signature = static_cast<asCScriptFunction*>(Callback->GetCallableSignature());
		ASSERT_THAT(IsTrue(Signature == Changed->GetCallableSignature()));
		ASSERT_THAT(IsTrue(Signature->funcType == asFUNC_CALLABLE_SIGNATURE));
		ASSERT_THAT(IsTrue(Signature->IsMetadataCallableSignature()));
		ASSERT_THAT(IsTrue(Callback->GetStableKey() != Changed->GetStableKey()));
		ASSERT_THAT(IsTrue(Callback->GetMetadataDeclarationKind() == AS_NAMESPACE_QUALIFIER asETypeDeclKind::Delegate));
		ASSERT_THAT(IsTrue(Changed->GetMetadataDeclarationKind() == AS_NAMESPACE_QUALIFIER asETypeDeclKind::Event));
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
		ASSERT_THAT(IsTrue(Result.Definitions->GetIdentityContext()->GetIdentityRegistry().ResolveTypeUse(Signature->GetStableKey(), Descriptor) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded));
		ASSERT_THAT(IsTrue(Descriptor.Kind == AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable));
		ASSERT_THAT(AreEqual(FString(TEXT("arg0")), FString(UTF8_TO_TCHAR(Signature->parameterNames[0].AddressOf()))));
		ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Result.Definitions->Freeze() == asEMetadataResult::Succeeded));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Result.Definitions)) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetTypeInfoById(Callback->GetTypeId()) == Callback));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(Signature->GetId()) == Signature));
		ASSERT_THAT(IsNull(Engine.Ptr->FindMetadataFunction(Signature->GetStableKey())));
		ASSERT_THAT(IsTrue(Signature->GetTypeId() < 0)); // No arbitrary nominal type chosen from two declarations.
	}

	TEST_METHOD(SeparateImagesCanRegisterEqualStructuralSignatures)
	{
		FInput FirstInput(TEXT("delegate int First(int Value);"));
		FInput SecondInput(TEXT("delegate int Second(int Other);"));
		for (auto* Input : {&FirstInput, &SecondInput})
		{
			ASSERT_THAT(IsTrue(Input->Collected));
			ASSERT_THAT(IsTrue(Input->Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsTrue(Input->Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsTrue(Input->Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Input->Session->VerifyAST()));
		}
		auto First = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*FirstInput.Session);
		auto Second = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*SecondInput.Session);
		ASSERT_THAT(IsTrue(First.Succeeded()));
		ASSERT_THAT(IsTrue(Second.Succeeded()));
		ASSERT_THAT(IsTrue(First.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(First.Definitions->Freeze() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Second.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Second.Definitions->Freeze() == asEMetadataResult::Succeeded));
		auto* FirstSignature = First.Definitions->GetFunctions()[0];
		auto* SecondSignature = Second.Definitions->GetFunctions()[0];
		ASSERT_THAT(IsTrue(FirstSignature != SecondSignature));
		ASSERT_THAT(IsTrue(FirstSignature->GetStableKey() == SecondSignature->GetStableKey()));
		FEngineOwner Engine;
		ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(First.Definitions)) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Second.Definitions)) == asEMetadataRegistrationResult::Succeeded));
		ASSERT_THAT(IsTrue(FirstSignature->GetId() != SecondSignature->GetId()));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(FirstSignature->GetId()) == FirstSignature));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetFunctionById(SecondSignature->GetId()) == SecondSignature));
	}

	TEST_METHOD(CallableSignatureCorruptionFailsBeforeEnginePublication)
	{
		FInput Input(TEXT("delegate int Callback(int Value);"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Result.Definitions->Freeze() == asEMetadataResult::Succeeded));
		Result.Definitions->GetFunctions()[0]->returnType = Result.Definitions->Primitive(ttBool);
		FEngineOwner Engine;
		const auto Before = Engine.Ptr->GetMetadataRegistrationCounts();
		ASSERT_THAT(IsTrue(RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Result.Definitions)) == asEMetadataRegistrationResult::InvalidIdentity));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetMetadataRegistrationCounts() == Before));
	}

	TEST_METHOD(DefinitionsOutliveSessionAndSourceWithoutBorrowedAST)
	{
		TUniquePtr<asCModuleDefinitionSet> Kept;
		asCTypeInfo* Retained = nullptr;
		{
			FInput Input(TEXT("namespace Kept { class Node { Node@ Next; }; }"));
			ASSERT_THAT(IsTrue(Input.Collected));
			ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
			auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
			ASSERT_THAT(IsTrue(Result.Succeeded()));
			ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
			ASSERT_THAT(IsTrue(Result.Definitions->Freeze() == asEMetadataResult::Succeeded));
			Retained = FindType(*Result.Definitions, "Node");
			Kept = MoveTemp(Result.Definitions);
		}
		ASSERT_THAT(IsNotNull(Kept.Get()));
		ASSERT_THAT(AreEqual(FString(TEXT("Kept")), FString(UTF8_TO_TCHAR(Retained->GetNamespace()))));
		ASSERT_THAT(IsTrue(static_cast<asCObjectType*>(Retained)->localProperties[0]->type.GetTypeInfo() == Retained));
	}

	TEST_METHOD(ImmutableFloatOptionsReachActualPropertyLayout)
	{
		AS_NAMESPACE_QUALIFIER asSTypeContextOptions Options;
		Options.FloatIsFloat64 = false;
		auto Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(Options);
		FInput Input(TEXT("struct Samples { float Small; double Large; };"), Context);
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Definitions->GetIdentityContext().Get() == &Context.Get()));
		ASSERT_THAT(IsTrue(Result.Definitions->FinalizeLayouts() == asEMetadataResult::Succeeded));
		auto* Samples = static_cast<asCObjectType*>(FindType(*Result.Definitions, "Samples"));
		ASSERT_THAT(AreEqual(4, Samples->localProperties[0]->type.GetSizeInMemoryBytes()));
		ASSERT_THAT(AreEqual(8, Samples->localProperties[1]->type.GetSizeInMemoryBytes()));
		ASSERT_THAT(AreEqual(8, Samples->localProperties[1]->byteOffset));
		ASSERT_THAT(AreEqual(16, Samples->size));
	}

	TEST_METHOD(UnfrozenDependenciesCannotEscapeThroughConsumer)
	{
		FInput Input(TEXT("class Empty {};"));
		ASSERT_THAT(IsTrue(Input.Collected));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsTrue(Input.Session->ResolveDeclarations(1)));
		ASSERT_THAT(IsTrue(Input.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsTrue(Input.Session->VerifyAST()));
		TUniquePtr<asCModuleDefinitionSet> Dependency = MakeUnique<asCModuleDefinitionSet>();
		TArray<asCModuleDefinitionSet*> Dependencies{Dependency.Get()};
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session, Dependencies);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Status == AS_NAMESPACE_QUALIFIER asEDefinitionConsumerStatus::InvalidDependency));
		ASSERT_THAT(IsNull(Result.Definitions.Get()));
	}

	TEST_METHOD(RecoveryInputCannotPublishAnEmptySuccessImage)
	{
		FInput Input(TEXT("class Broken { Missing Value; };"));
		ASSERT_THAT(IsTrue(Input.Session->CloseDeclarationBarrier()));
		ASSERT_THAT(IsFalse(Input.Session->ResolveDeclarations(1)));
		const auto Result = AS_NAMESPACE_QUALIFIER asCDefinitionConsumer().Consume(*Input.Session);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsNull(Result.Definitions.Get()));
	}
};
#endif
