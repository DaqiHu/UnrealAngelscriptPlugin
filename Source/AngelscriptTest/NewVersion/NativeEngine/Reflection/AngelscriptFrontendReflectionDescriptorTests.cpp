#include "CQTest.h"
#include "as_builder.h"

#include "../NativeEngineTestSupport.h"
#include "Core/AngelscriptDescriptors.h"
#include "Core/AngelscriptEngine.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_identifier_table.h"
#include "frontend/as_preprocess_result.h"
#include "frontend/as_tokenizer.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptFrontendReflectionDescriptorTests
{
	struct FSemanticRun
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asCSourceManager SourceManager{Snapshot};
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
		AS_NAMESPACE_QUALIFIER asCPreprocessorConfiguration Configuration;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{SourceManager};
		AS_NAMESPACE_QUALIFIER asCPreprocessor Preprocessor{SourceManager, Configuration, Diagnostics};
		AS_NAMESPACE_QUALIFIER FAngelscriptPreprocessResult Result;

		FSemanticRun(
			const TArray<TPair<FString, FString>>& Sources,
			TMap<FString, bool> Flags = {})
			: Configuration(MoveTemp(Flags))
		{
			TArray<AS_NAMESPACE_QUALIFIER asCSourceFileID> FileIDs;
			TArray<TArray<uint8>> Encoded;
			for (const TPair<FString, FString>& Source : Sources)
			{
				const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(Source.Key, Source.Value);
				TArray<uint8>& Bytes = Encoded.AddDefaulted_GetRef();
				Bytes.Append(Input.GetUtf8Bytes().GetData(), Input.GetUtf8Bytes().Num());
				FileIDs.Add(Snapshot->AddFile(Source.Key, Bytes));
			}
			Snapshot->Freeze();

			TArray<AS_NAMESPACE_QUALIFIER asSSemanticSourceInput> Inputs;
			for (int32 Index = 0; Index < Sources.Num(); ++Index)
			{
				AS_NAMESPACE_QUALIFIER asSSemanticSourceInput& SemanticInput = Inputs.AddDefaulted_GetRef();
				SemanticInput.LogicalSourceKey = Sources[Index].Key;
				AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
					SourceManager,
					SourceManager.MakeRange(FileIDs[Index], 0, Encoded[Index].Num()),
					AS_NAMESPACE_QUALIFIER asSLexOptions(
						AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
						AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia),
					Identifiers,
					Diagnostics);
				for (;;)
				{
					AS_NAMESPACE_QUALIFIER asCToken Token;
					if (!Tokenizer.Lex(Token)) break;
					SemanticInput.RawTokens.Add(Token);
					if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile) break;
				}
				Tokenizer.FlushDiagnostics();
			}
			Result = asCBuilder::CompileDeclarations(SourceManager, Configuration, Diagnostics, Inputs, TEXT("game"), 4);
		}
	};

	TEST_CLASS_WITH_FLAGS(ReflectionDescriptors, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{

		TEST_METHOD(HostDescriptorsPreserveTheFrontendCanonicalKeyRoles)
		{
			FSemanticRun Run({{TEXT("Keys.as"), TEXT("class Payload {}; UCLASS() class Actor { UPROPERTY() const Payload@ Cached; UFUNCTION() int Read(const Payload&in Input); };")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			auto& Session = *Run.Result.SemanticSession;
			auto& Registry = Session.GetASTContext().GetTypeContext()->GetIdentityRegistry();
			const auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Session.Lookup(TEXT("Actor"))[0]);
			const auto* Function = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Session.Lookup(TEXT("Read"))[0]);
			const auto& Module = Run.Result.GetModules()[0].Get();
			const auto& Class = Module.Classes[0].Get();
			ASSERT_THAT(IsTrue(Module.Semantic.StableDeclarationKey == Session.GetModuleIdentity(TEXT("Keys.as"))));
			ASSERT_THAT(IsTrue(Class.Semantic.StableDeclarationKey == Record->GetStableIdentity()));
			ASSERT_THAT(IsTrue(Class.Semantic.StableTypeUseKey == Session.GetDeclaredType(Record->GetStableIdentity())->GetStableIdentity()));
			ASSERT_THAT(IsTrue(Class.Methods[0]->Semantic.StableDeclarationKey == Function->GetStableIdentity()));
			ASSERT_THAT(IsTrue(Class.Methods[0]->Semantic.StableTypeUseKey == Function->GetResolvedSignature()->GetStableIdentity()));
			const auto& Argument = Class.Methods[0]->Arguments[0].Semantic;
			ASSERT_THAT(IsTrue(Argument.StableDeclarationKey == Function->GetParameters()[0]->GetStableIdentity()));
			ASSERT_THAT(IsTrue(Argument.StableOwnerKey == Function->GetStableIdentity()));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Type;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.ResolveTypeUse(Argument.StableTypeUseKey, Type)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const | AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Reference, Type.Qualifiers));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.ResolveTypeUse(Class.Properties[0]->Semantic.StableTypeUseKey, Type)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const | AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Handle, Type.Qualifiers));
		}

		TEST_METHOD(DelegateSignatureUsesCallableKeyAndNominalParameterOwner)
		{
			FSemanticRun Run({{TEXT("CallableKeys.as"), TEXT("delegate int Compute(int&out Value);")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			auto& Session = *Run.Result.SemanticSession;
			const auto* Decl = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Session.Lookup(TEXT("Compute"))[0]);
			const auto& Delegate = Run.Result.GetModules()[0]->Delegates[0].Get();
			ASSERT_THAT(IsTrue(Delegate.Semantic.StableDeclarationKey == Decl->GetStableIdentity()));
			ASSERT_THAT(IsTrue(Delegate.Signature->Semantic.StableTypeUseKey == Decl->GetResolvedSignature()->GetStableIdentity()));
			ASSERT_THAT(IsTrue(Delegate.Signature->Semantic.StableTypeUseKey != Delegate.Semantic.StableTypeUseKey));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Parameter;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Session.GetASTContext().GetTypeContext()->GetIdentityRegistry().ResolveDeclaration(Delegate.Signature->Arguments[0].Semantic.StableDeclarationKey, Parameter)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Parameter, Parameter.Kind));
			ASSERT_THAT(IsTrue(Parameter.Owner == Decl->GetStableIdentity()));
			ASSERT_THAT(AreEqual(asUINT(0), Parameter.Ordinal));
			ASSERT_THAT(IsTrue(Parameter.Name.IsEmpty()));
		}

		TEST_METHOD(ConcreteResultOwnsResolvedDescriptorCollection)
		{
			FSemanticRun Run({{TEXT("Game.Actor.as"), TEXT("UCLASS() class Actor {};\n")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			ASSERT_THAT(AreEqual(1, Run.Result.PreprocessingResults.Num()));
			ASSERT_THAT(IsTrue(Run.Result.PreprocessingResults[0].IsValid()));
			ASSERT_THAT(IsTrue(Run.Result.PreprocessingResults[0]->PreprocessingRecord.IsValid()));
			ASSERT_THAT(IsTrue(Run.Result.SemanticSession.IsValid()));
			ASSERT_THAT(IsFalse(Run.Result.SemanticSession->GetASTContext().IsSealed()));
			ASSERT_THAT(AreEqual(1, Run.Result.GetModules().Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Game.Actor.as")), Run.Result.GetModules()[0]->ModuleName));
			ASSERT_THAT(AreEqual(1, Run.Result.GetModules()[0]->Classes.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Actor")), Run.Result.GetModules()[0]->Classes[0]->ClassName));
		}

		TEST_METHOD(ResolvedProjectionAllowsBodyResume)
		{
			FSemanticRun Run({{TEXT("Game.Resume.as"), TEXT(
				"UCLASS() class Actor {};\n"
				"int Value() { return 42; }\n")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			auto& Session = *Run.Result.SemanticSession;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asECompilationPhase::DeclarationsResolved, Session.GetPhase()));
			const TConstArrayView<AS_NAMESPACE_QUALIFIER asCDecl*> Found = Session.Lookup(TEXT("Value"));
			ASSERT_THAT(AreEqual(1, Found.Num()));
			auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Found[0]);
			ASSERT_THAT(IsNotNull(Function));
			ASSERT_THAT(IsNull(Function->GetBody()));
			ASSERT_THAT(IsTrue(Session.AnalyzeBodies(1), TEXT("Descriptor projection must leave the deferred body phase available.")));
			ASSERT_THAT(IsTrue(Session.AreBodiesAnalyzed()));
			ASSERT_THAT(IsFalse(Session.GetASTContext().IsSealed()));
			ASSERT_THAT(IsFalse(Session.IsASTVerified()));
			ASSERT_THAT(IsTrue(Session.VerifyAST(), TEXT("Only successful explicit AST verification may seal the resumed body.")));
			ASSERT_THAT(IsTrue(Session.IsASTVerified()));
			ASSERT_THAT(IsTrue(Session.GetASTContext().IsSealed()));
			ASSERT_THAT(IsTrue(Function == Session.Lookup(TEXT("Value"))[0]));
			auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Function->GetBody());
			ASSERT_THAT(IsNotNull(Body));
			ASSERT_THAT(AreEqual(1, Body->GetStatements().Num()));
			auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Return));
			auto* Value = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(Return->GetValue());
			ASSERT_THAT(IsNotNull(Value));
			ASSERT_THAT(AreEqual(42ll, Value->GetValue()));
			ASSERT_THAT(AreEqual(1, Run.Result.GetModules()[0]->Classes.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Actor")), Run.Result.GetModules()[0]->Classes[0]->ClassName));
		}

		TEST_METHOD(ResolvedDescriptorsRetainSemanticKeysAndNullMaterializationPointers)
		{
			FSemanticRun Run({{TEXT("Game.Actor.as"), TEXT("UCLASS() class Actor {};\n")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const FAngelscriptModuleDesc& Module = Run.Result.GetModules()[0].Get();
			const FAngelscriptClassDesc& Class = Module.Classes[0].Get();
			ASSERT_THAT(AreEqual(EAngelscriptDescriptorLifecycle::Resolved, Module.Semantic.Lifecycle));
			ASSERT_THAT(AreEqual(EAngelscriptDescriptorLifecycle::Resolved, Class.Semantic.Lifecycle));
			ASSERT_THAT(IsTrue(Class.Semantic.SourceAnchor.IsValid()));
			ASSERT_THAT(IsFalse(Class.Semantic.StableDeclarationKey.IsZero()));
			ASSERT_THAT(IsTrue(Module.ScriptModule == nullptr));
			ASSERT_THAT(IsTrue(Class.ScriptType == nullptr));
			ASSERT_THAT(IsTrue(Class.Class == nullptr && Class.Struct == nullptr && Class.CodeSuperClass == nullptr));
		}

		TEST_METHOD(MemberDescriptorsComeFromTypedDeclarationsAndAttributes)
		{
			FSemanticRun Run({{TEXT("Game.Actor.as"), TEXT(
				"UCLASS() class Actor { UPROPERTY() int Health; UFUNCTION() int Tick(int Delta); };\n")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const FAngelscriptClassDesc& Class = Run.Result.GetModules()[0]->Classes[0].Get();
			ASSERT_THAT(AreEqual(1, Class.Properties.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Health")), Class.Properties[0]->PropertyName));
			ASSERT_THAT(IsFalse(Class.Properties[0]->Semantic.StableTypeUseKey.IsZero()));
			ASSERT_THAT(AreEqual(1, Class.Methods.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Tick")), Class.Methods[0]->FunctionName));
			ASSERT_THAT(AreEqual(1, Class.Methods[0]->Arguments.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Delta")), Class.Methods[0]->Arguments[0].ArgumentName));
			ASSERT_THAT(IsTrue(Class.Methods[0]->bBlueprintCallable));
		}

		TEST_METHOD(ActiveConfigurationSelectsOneDescriptorSurface)
		{
			const TArray<TPair<FString, FString>> Sources = {{TEXT("Conditional.as"), TEXT(
				"#if FEATURE\nUCLASS() class Enabled {};\n#else\nUCLASS() class Disabled {};\n#endif\n")}};
			FSemanticRun Enabled(Sources, {{TEXT("FEATURE"), true}});
			FSemanticRun Disabled(Sources, {{TEXT("FEATURE"), false}});
			ASSERT_THAT(IsTrue(Enabled.Result.IsSuccess() && Disabled.Result.IsSuccess()));
			ASSERT_THAT(AreEqual(FString(TEXT("Enabled")), Enabled.Result.GetModules()[0]->Classes[0]->ClassName));
			ASSERT_THAT(AreEqual(FString(TEXT("Disabled")), Disabled.Result.GetModules()[0]->Classes[0]->ClassName));
			ASSERT_THAT(IsFalse(Enabled.Result.GetConfigurationDigest() == Disabled.Result.GetConfigurationDigest()));
		}

		TEST_METHOD(DeclarationFailureReturnsDiagnosticsWithoutPublishableDescriptors)
		{
			FSemanticRun Run({{TEXT("Invalid.as"), TEXT("UCLASS() class Broken { UPROPERTY() Missing Value; };\n")}});
			ASSERT_THAT(IsFalse(Run.Result.IsSuccess()));
			ASSERT_THAT(IsFalse(Run.Result.HasPublishableDescriptors()));
			ASSERT_THAT(AreEqual(0, Run.Result.GetModules().Num()));
			ASSERT_THAT(IsTrue(Run.Result.GetDiagnostics().Num() > 0));
		}

		TEST_METHOD(EnumAndDelegateDescriptorsRemainResolvedAndUnmaterialized)
		{
			FSemanticRun Run({{TEXT("Game.Signatures.as"), TEXT(
				"UENUM() enum EMode { Idle, Run };\n"
				"delegate void OnDone(int Code);\n")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const FAngelscriptModuleDesc& Module = Run.Result.GetModules()[0].Get();
			ASSERT_THAT(AreEqual(1, Module.Enums.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("EMode")), Module.Enums[0]->EnumName));
			ASSERT_THAT(AreEqual(EAngelscriptDescriptorLifecycle::Resolved, Module.Enums[0]->Semantic.Lifecycle));
			ASSERT_THAT(IsTrue(Module.Enums[0]->Enum == nullptr && Module.Enums[0]->ScriptType == nullptr));
			ASSERT_THAT(AreEqual(1, Module.Delegates.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("OnDone")), Module.Delegates[0]->DelegateName));
			ASSERT_THAT(AreEqual(EAngelscriptDescriptorLifecycle::Resolved, Module.Delegates[0]->Semantic.Lifecycle));
			ASSERT_THAT(IsTrue(Module.Delegates[0]->Function == nullptr && Module.Delegates[0]->ScriptType == nullptr));
		}

		TEST_METHOD(MetadataArgumentsProjectIntoConcreteDescriptors)
		{
			FSemanticRun Run({{TEXT("Game.Metadata.as"), TEXT(
				"UCLASS(DisplayName=\"Actor Label\", Meta=(Category=\"Gameplay\", ToolTip=\"Actor docs\")) "
				"class Actor { UPROPERTY(Category=\"Stats\") int Health; };\n")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const FAngelscriptClassDesc& Class = Run.Result.GetModules()[0]->Classes[0].Get();
			ASSERT_THAT(AreEqual(FString(TEXT("Actor Label")), Class.Meta.FindRef(TEXT("DisplayName"))));
			ASSERT_THAT(AreEqual(FString(TEXT("Gameplay")), Class.Meta.FindRef(TEXT("Category"))));
			ASSERT_THAT(AreEqual(FString(TEXT("Actor docs")), Class.Meta.FindRef(TEXT("ToolTip"))));
			ASSERT_THAT(AreEqual(1, Class.Properties.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Stats")), Class.Properties[0]->Meta.FindRef(TEXT("Category"))));
		}

		TEST_METHOD(EnumConstantsProjectNamesAndValues)
		{
			FSemanticRun Run({{TEXT("Game.Enum.as"), TEXT(
				"UENUM() enum EMode { Idle, Run = 4, Jump };\n")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const FAngelscriptEnumDesc& Enum = Run.Result.GetModules()[0]->Enums[0].Get();
			ASSERT_THAT(AreEqual(3, Enum.ValueNames.Num()));
			ASSERT_THAT(AreEqual(FName(TEXT("Idle")), Enum.ValueNames[0]));
			ASSERT_THAT(AreEqual(FName(TEXT("Run")), Enum.ValueNames[1]));
			ASSERT_THAT(AreEqual(FName(TEXT("Jump")), Enum.ValueNames[2]));
			ASSERT_THAT(AreEqual(0, Enum.EnumValues[0]));
			ASSERT_THAT(AreEqual(4, Enum.EnumValues[1]));
			ASSERT_THAT(AreEqual(5, Enum.EnumValues[2]));
		}

		TEST_METHOD(DelegateAndEventHostDescriptionsKeepSingleAndMulticastSignatures)
		{
			FSemanticRun Run({{TEXT("Game.Callables.as"), TEXT("delegate int Calculate(int Value); event void Updated(bool Enabled);")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const FAngelscriptModuleDesc& Module = Run.Result.GetModules()[0].Get();
			ASSERT_THAT(AreEqual(0, Module.Classes.Num()));
			ASSERT_THAT(AreEqual(2, Module.Delegates.Num()));
			ASSERT_THAT(IsFalse(Module.Delegates[0]->bIsMulticast));
			ASSERT_THAT(IsTrue(Module.Delegates[1]->bIsMulticast));
			ASSERT_THAT(AreEqual(FString(TEXT("Calculate")), Module.Delegates[0]->DelegateName));
			ASSERT_THAT(AreEqual(FString(TEXT("Updated")), Module.Delegates[1]->DelegateName));
			for (const auto& Delegate : Module.Delegates)
			{
				ASSERT_THAT(IsTrue(Delegate->Signature.IsValid()));
				ASSERT_THAT(AreEqual(EAngelscriptDescriptorLifecycle::Resolved, Delegate->Signature->Semantic.Lifecycle));
				ASSERT_THAT(AreEqual(1, Delegate->Signature->Arguments.Num()));
				ASSERT_THAT(IsFalse(Delegate->Signature->Arguments[0].Semantic.StableTypeUseKey.IsZero()));
				ASSERT_THAT(IsTrue(Delegate->Function == nullptr && Delegate->ScriptType == nullptr));
			}
			ASSERT_THAT(AreEqual(FString(TEXT("Enabled")), Module.Delegates[1]->Signature->Arguments[0].ArgumentName));
			ASSERT_THAT(IsFalse(Run.Result.SemanticSession->GetASTContext().IsSealed()));
		}

		TEST_METHOD(CallableForwardSignaturesProduceSourceLocatedDependencies)
		{
			FSemanticRun Run({
				{TEXT("A.Consumer.as"), TEXT("delegate Payload@ Make(); event void Updated(const Payload&in Value);")},
				{TEXT("Z.Provider.as"), TEXT("USTRUCT() struct Payload {};")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const auto Edges = Run.Result.DeclarationDependencies.GetEdges();
			ASSERT_THAT(AreEqual(2, Edges.Num()));
			for (const auto& Edge : Edges)
			{
				ASSERT_THAT(AreEqual(FString(TEXT("A.Consumer.as")), Edge.SourceModuleKey));
				ASSERT_THAT(AreEqual(FString(TEXT("Z.Provider.as")), Edge.TargetModuleKey));
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::DelegateSignature, Edge.Reason));
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::Signature, Edge.Completeness));
				ASSERT_THAT(IsTrue(Edge.StableSource.IsValid()));
			}
			ASSERT_THAT(AreEqual(9u, Edges[0].StableSource.BeginByteOffset));
			ASSERT_THAT(AreEqual(17u, Edges[0].StableSource.EndByteOffset));
		}

		TEST_METHOD(EnumMetaAnnotationProjectsValueMetadata)
		{
			FSemanticRun Run({{TEXT("Game.EnumMeta.as"), TEXT("UENUM(Meta=(ToolTip=\"Mode docs\")) enum Mode { Idle UMETA(DisplayName=\"Waiting\", Hidden), Run };")}});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			const auto& Enum = Run.Result.GetModules()[0]->Enums[0];
			ASSERT_THAT(AreEqual(FString(TEXT("Mode docs")), Enum->Meta.FindRef(TPair<FName, int32>(FName(TEXT("ToolTip")), INDEX_NONE))));
			ASSERT_THAT(AreEqual(FString(TEXT("Waiting")), Enum->Meta.FindRef(TPair<FName, int32>(FName(TEXT("DisplayName")), 0))));
			ASSERT_THAT(IsTrue(Enum->Meta.Contains(TPair<FName, int32>(FName(TEXT("Hidden")), 0))));
			ASSERT_THAT(IsFalse(Enum->Meta.Contains(TPair<FName, int32>(FName(TEXT("DisplayName")), 1))));
		}

		TEST_METHOD(RemovedDeclarationsNeverProducePublishableHostDescriptions)
		{
			const TCHAR* Sources[] = { TEXT("asset Old of Payload {}; UCLASS() class Good {};"), TEXT("import void Old() from \"Other\"; delegate void Good();") };
			for (const TCHAR* Source : Sources)
			{
				FSemanticRun Run({{TEXT("Removed.as"), Source}});
				ASSERT_THAT(IsFalse(Run.Result.IsSuccess()));
				ASSERT_THAT(IsFalse(Run.Result.HasPublishableDescriptors()));
				ASSERT_THAT(AreEqual(0, Run.Result.GetModules().Num()));
				ASSERT_THAT(IsTrue(Run.Result.GetDiagnostics().Num() > 0));
			}
		}

		TEST_METHOD(ResolvedTypeUseProducesDeclarationDependencyWithoutSourceImports)
		{
			FSemanticRun Run({
				{TEXT("Game.Provider.as"), TEXT("USTRUCT() struct Payload {};\n")},
				{TEXT("Game.Consumer.as"), TEXT("UCLASS() class Consumer { UPROPERTY() Payload Value; };\n")},
			});
			ASSERT_THAT(IsTrue(Run.Result.IsSuccess()));
			ASSERT_THAT(AreEqual(1, Run.Result.DeclarationDependencies.GetEdges().Num()));
			const AS_NAMESPACE_QUALIFIER FAngelscriptDependencyEdge& Edge = Run.Result.DeclarationDependencies.GetEdges()[0];
			ASSERT_THAT(AreEqual(FString(TEXT("Game.Consumer.as")), Edge.SourceModuleKey));
			ASSERT_THAT(AreEqual(FString(TEXT("Game.Provider.as")), Edge.TargetModuleKey));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER EAngelscriptDependencyReason::PropertyType, Edge.Reason));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER EAngelscriptCompletenessRequirement::CompleteDefinition, Edge.Completeness));
			ASSERT_THAT(IsTrue(Edge.StableSource.IsValid()));
		}

		TEST_METHOD(EngineAndDescriptorHeadersExposeTheSameConcreteTypes)
		{
			FAngelscriptModuleDesc Module;
			FAngelscriptClassDesc Class;
			Module.Classes.Add(MakeShared<FAngelscriptClassDesc>(Class));
			ASSERT_THAT(AreEqual(1, Module.Classes.Num()));
			ASSERT_THAT(AreEqual(EAngelscriptDescriptorLifecycle::Parsed, Module.Semantic.Lifecycle));
		}
	};
}

#endif
