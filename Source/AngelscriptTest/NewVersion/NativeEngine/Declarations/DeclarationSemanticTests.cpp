#include "CQTest.h"

#include "../NativeEngineTestSupport.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Parser/as_parser.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "frontend/AST/as_ast_verifier.h"
#include "frontend/AST/as_ast_visitor.h"
#include "frontend/Compile/as_descriptor_consumer.h"
#include "Containers/StringConv.h"
#include "Containers/Set.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DeclarationSemanticTests
{
	struct FSessionRun
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> SharedSources;
		AS_NAMESPACE_QUALIFIER asCSourceManager SourceManager;
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics;
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCCompilationSession> Session;

		explicit FSessionRun(const TArray<TPair<FString, FString>>& Sources, TMap<FString, bool> Flags = {},
			TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>())
			: SharedSources(AngelscriptNativeEngineTest::FFrontendSessionRun::MakeSharedSources(Sources))
			, SourceManager(SharedSources)
			, Diagnostics(SourceManager)
		{
			TArray<AS_NAMESPACE_QUALIFIER asCSourceFileID> Files;
			TArray<TArray<uint8>> Encoded;
			for (const auto& Source : Sources)
			{
				const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(Source.Key, Source.Value);
				TArray<uint8> Bytes;
				Bytes.Append(Input.GetUtf8Bytes().GetData(), Input.GetUtf8Bytes().Num());
				Encoded.Add(MoveTemp(Bytes));
				Files.Add(AS_NAMESPACE_QUALIFIER asCSourceFileID(static_cast<asUINT>(Encoded.Num())));
			}
			Session = MakeUnique<AS_NAMESPACE_QUALIFIER asCCompilationSession>(SourceManager, Diagnostics, TEXT("game"), MoveTemp(TypeContext));
			for (int32 Index = 0; Index < Sources.Num(); ++Index)
			{
				AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(SourceManager, SourceManager.MakeRange(Files[Index], 0, Encoded[Index].Num()), AS_NAMESPACE_QUALIFIER asSLexOptions(AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly, AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia), Identifiers, Diagnostics);
				TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
				for (;;) { AS_NAMESPACE_QUALIFIER asCToken Token; if (!Tokenizer.Lex(Token)) break; Tokens.Add(Token); if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile) break; }
				Tokenizer.FlushDiagnostics();
				AS_NAMESPACE_QUALIFIER asCPreprocessorConfiguration Configuration(Flags);
				AS_NAMESPACE_QUALIFIER asCPreprocessor Preprocessor(SourceManager, Configuration, Diagnostics);
				const AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult Result = Preprocessor.Process(Tokens);
				Session->CollectSource(Preprocessor, Result, Sources[Index].Key);
			}
		}

		FString SourceText(const AS_NAMESPACE_QUALIFIER asCSourceRange& Range) const
		{
			TConstArrayView<uint8> Bytes;
			if (!SourceManager.TryGetBytes(Range, Bytes)) return TEXT("<invalid-range>");
			FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
			return FString(Text.Length(), Text.Get());
		}

		TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> GetDiagnostics()
		{
			AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
			Diagnostics.EmitTo(Consumer);
			TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Records;
			Records.Append(Consumer.GetRecords().GetData(), Consumer.GetRecords().Num());
			return Records;
		}
	};

	TEST_CLASS_WITH_FLAGS(DeclarationsCollection, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ParserBoundaryUsesOnlySema)
		{
			static_assert(std::is_constructible_v<AS_NAMESPACE_QUALIFIER asCParser, AS_NAMESPACE_QUALIFIER asCSema&>);
			static_assert(!std::is_default_constructible_v<AS_NAMESPACE_QUALIFIER asCParser>);
			ASSERT_THAT(IsTrue(true));
		}

		TEST_METHOD(AllSourcesCollectBeforeTheBarrier)
		{
			FSessionRun Run({{TEXT("A.as"), TEXT("class A {};\n")}, {TEXT("B.as"), TEXT("class B {};\n")}});
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asECompilationPhase::Collecting, Run.Session->GetPhase()));
			ASSERT_THAT(AreEqual(2, Run.Session->GetFragments().Num()));
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asECompilationPhase::DeclarationsCollected, Run.Session->GetPhase()));
			ASSERT_THAT(AreEqual(2, Run.Session->GetTranslationUnit()->GetDecls().Num()));
		}

		TEST_METHOD(ParserCreatesConcreteDeclarationsThroughSema)
		{
			FSessionRun Run({{TEXT("Types.as"), TEXT("namespace Game { class Actor { int Health; }; }\n")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			const auto* Namespace = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCNamespaceDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			ASSERT_THAT(IsTrue(Namespace != nullptr));
			const auto* Record = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Namespace->GetDecls()[0]);
			ASSERT_THAT(IsTrue(Record != nullptr));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Record->GetDecls()[0]) != nullptr));
		}

		TEST_METHOD(FunctionBodiesRemainDeferredRanges)
		{
			FSessionRun Run({{TEXT("Functions.as"), TEXT("int Add(int A, int B) { return A + B; }\n")}});
			Run.Session->CloseDeclarationBarrier();
			const auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			ASSERT_THAT(IsTrue(Function != nullptr));
			ASSERT_THAT(AreEqual(2, Function->GetParameters().Num()));
			ASSERT_THAT(IsTrue(Function->HasDeferredBody()));
			ASSERT_THAT(IsTrue(Function->GetDeferredBodyRange().Length() > 2));
			ASSERT_THAT(IsTrue(Function->GetBody() == nullptr));
		}

		TEST_METHOD(TypedAnnotationsArePreservedWithoutReflectionExecution)
		{
			FSessionRun Run({{TEXT("Attrs.as"), TEXT("UFUNCTION() int Tick();\n")}});
			Run.Session->CloseDeclarationBarrier();
			const auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			ASSERT_THAT(IsTrue(Function != nullptr));
			ASSERT_THAT(AreEqual(1, Function->GetAttrs().Num()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCAnnotationAttr>(Function->GetAttrs()[0])));
		}

		TEST_METHOD(CollectionBarrierIsOneWay)
		{
			FSessionRun Run({{TEXT("A.as"), TEXT("class A {};\n")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsFalse(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsFalse(Run.Session->CollectSourceAfterBarrierForTesting()));
		}
	};

	TEST_CLASS_WITH_FLAGS(DeclarationsMaintainedSyntax, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(NestedGenericClosersAndArrayDimensionsRetainTypedSourceTrees)
		{
			FSessionRun Run({{TEXT("GenericSyntax.as"), TEXT("class Holder { const Map<int, array<array<int>>>[]@ Values; }")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			auto* Field = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Record->GetDecls()[0]);
			const auto& Syntax = Field->GetTypeSyntax();
			ASSERT_THAT(IsTrue(Syntax.bConst && Syntax.bHandle));
			ASSERT_THAT(AreEqual(2, Syntax.GenericArguments.Num()));
			ASSERT_THAT(AreEqual(1, Syntax.ArrayDimensions.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("[]")), Run.SourceText(Syntax.ArrayDimensions[0])));
			ASSERT_THAT(AreEqual(FString(TEXT("array<array<int>>")), Run.SourceText(Syntax.GenericArguments[1].Range)));
			ASSERT_THAT(AreEqual(FString(TEXT("array<int>")), Run.SourceText(Syntax.GenericArguments[1].GenericArguments[0].Range)));
			ASSERT_THAT(AreEqual(FString(TEXT("int")), Run.SourceText(Syntax.GenericArguments[1].GenericArguments[0].GenericArguments[0].Range)));
		}

		TEST_METHOD(DefaultInitializerRangeExcludesDelimitersAndKeepsNestedCommas)
		{
			FSessionRun Run({{TEXT("DefaultRange.as"), TEXT("int Pair(int A, int B); int Use(int Value = Pair(1, 2)); const int Answer = (6 * 7);")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			auto* Function = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->GetTranslationUnit()->Lookup(TEXT("Use"))[0]);
			auto* Parameter = Function->GetParameters()[0];
			ASSERT_THAT(IsTrue(Parameter->HasDeferredInitializer()));
			ASSERT_THAT(IsNull(Parameter->GetInitializer()));
			ASSERT_THAT(AreEqual(FString(TEXT("Pair(1, 2)")), Run.SourceText(Parameter->GetDeferredInitializerRange())));
			auto* Global = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Run.Session->GetTranslationUnit()->Lookup(TEXT("Answer"))[0]);
			ASSERT_THAT(AreEqual(FString(TEXT("(6 * 7)")), Run.SourceText(Global->GetDeferredInitializerRange())));
		}

		TEST_METHOD(VoidParameterSpellingMeansAnEmptySignature)
		{
			FSessionRun Run({{TEXT("VoidParameters.as"), TEXT("void Run(void); delegate void Callback(void); event void Changed(void);")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			for (auto* Declaration : Run.Session->GetTranslationUnit()->GetDecls())
			{
				if (auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Declaration))
				{
					ASSERT_THAT(AreEqual(0, Function->GetParameters().Num()));
				}
				else
				{
					ASSERT_THAT(AreEqual(0, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallableTypeDecl>(Declaration)->GetParameters().Num()));
				}
			}
		}

		TEST_METHOD(CallableDefaultOwnsAnAnalyzedExpression)
		{
			FSessionRun Run({{TEXT("CallableDefault.as"), TEXT("delegate int Callback(int Value = 6 * 7);")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Callable = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallableTypeDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Callable->GetParameters()[0]->GetInitializer())));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(RequiredParameterAfterDefaultIsRejectedAndLaterDeclarationSurvives)
		{
			for (const TCHAR* Prefix : {TEXT("int"), TEXT("delegate int"), TEXT("event void")})
			{
				FSessionRun Run({{TEXT("InvalidDefaults.as"), FString::Printf(TEXT("%s Bad(int First = 1, int Second); class Later {}"), Prefix)}});
				ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
				ASSERT_THAT(IsTrue(Run.Session->GetFragments()[0].HasErrors()));
				ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
				ASSERT_THAT(AreEqual(1, Run.Session->GetTranslationUnit()->Lookup(TEXT("Later")).Num()));
				ASSERT_THAT(IsTrue(Run.Session->GetTranslationUnit()->Lookup(TEXT("Bad")).IsEmpty()));
			}
		}

		TEST_METHOD(ConstructorDestructorAndMethodModifiersRemainDistinctTypedFacts)
		{
			FSessionRun Run({{TEXT("SpecialMembers.as"), TEXT("class Item { Item(int Value); ~Item(); int Read() const final; }")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			auto* Constructor = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Record->Lookup(TEXT("Item"))[0]);
			auto* Destructor = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Record->Lookup(TEXT("~Item"))[0]);
			auto* Read = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Record->Lookup(TEXT("Read"))[0]);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFunctionKind::Constructor, Constructor->GetFunctionKind()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFunctionKind::Destructor, Destructor->GetFunctionKind()));
			ASSERT_THAT(IsTrue(Read->IsConstMethod()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFunctionModifiers::Final, Read->GetModifiers()));
			ASSERT_THAT(IsTrue(Constructor->GetStableIdentity() != Destructor->GetStableIdentity()));
		}

		TEST_METHOD(PrimitiveAliasRetainsAuthoredTargetAndCanonicalAliasedType)
		{
			FSessionRun Run({{TEXT("Alias.as"), TEXT("typedef int Count; Count Identity(Count Value);")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			auto* Alias = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCTypeAliasDecl>(Run.Session->GetTranslationUnit()->Lookup(TEXT("Count"))[0]);
			auto* Function = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->GetTranslationUnit()->Lookup(TEXT("Identity"))[0]);
			ASSERT_THAT(AreEqual(FString(TEXT("int")), Run.SourceText(Alias->GetTypeSyntax().Range)));
			ASSERT_THAT(IsTrue(Alias->GetResolvedAliasedType() == Function->GetResolvedReturnType()));
			ASSERT_THAT(IsFalse(Alias->GetStableIdentity().IsZero()));
		}
	};

	TEST_CLASS_WITH_FLAGS(DeclarationsLanguageForms, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(DelegateAndEventAreCallableTypesNotFunctionsOrGeneratedRecords)
		{
			FSessionRun Run({{TEXT("Callable.as"), TEXT("delegate int Compute(int Value); event void Changed();")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			const auto Decls = Run.Session->GetTranslationUnit()->GetDecls();
			ASSERT_THAT(AreEqual(2, Decls.Num()));
			const auto* Delegate = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Decls[0]);
			const auto* Event = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCEventDecl>(Decls[1]);
			ASSERT_THAT(IsNotNull(Delegate)); ASSERT_THAT(IsNotNull(Event));
			ASSERT_THAT(IsFalse(Delegate->IsMulticast())); ASSERT_THAT(IsTrue(Event->IsMulticast()));
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Delegate)));
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Event)));
			ASSERT_THAT(IsNull(Delegate->GetResolvedSignature()));
			ASSERT_THAT(IsNull(Event->GetResolvedSignature()));
			ASSERT_THAT(AreEqual(1, Delegate->GetDecls().Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDeclKind::ParmVar, Delegate->GetDecls()[0]->GetKind()));
			ASSERT_THAT(AreEqual(0, Event->GetDecls().Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("delegate int Compute(int Value);")), Run.SourceText(Delegate->GetSourceRange())));
		}

		TEST_METHOD(CallableTypeSyntaxRetainsQualificationModifiersAndExactLocations)
		{
			FSessionRun Run({{TEXT("Syntax.as"), TEXT("delegate const ::Game::Payload@ Convert(const Game::Payload&in Input, int&out Count);")}});
			Run.Session->CloseDeclarationBarrier();
			const auto* Decl = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			ASSERT_THAT(IsNotNull(Decl));
			const auto& Return = Decl->GetReturnTypeSyntax();
			ASSERT_THAT(AreEqual(2, Return.NameParts.Num()));
			ASSERT_THAT(IsTrue(Return.bGlobal && Return.bConst && Return.bHandle));
			ASSERT_THAT(AreEqual(FString(TEXT("Game")), Return.NameParts[0]));
			ASSERT_THAT(AreEqual(FString(TEXT("Payload")), Run.SourceText(Return.ComponentRanges[1])));
			ASSERT_THAT(AreEqual(FString(TEXT("const ::Game::Payload@")), Run.SourceText(Return.Range)));
			ASSERT_THAT(AreEqual(2, Decl->GetParameters().Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeReferenceDirection::In, Decl->GetParameters()[0]->GetTypeSyntax().Reference));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeReferenceDirection::Out, Decl->GetParameters()[1]->GetTypeSyntax().Reference));
			ASSERT_THAT(AreEqual(FString(TEXT("int&out")), Run.SourceText(Decl->GetParameters()[1]->GetTypeSyntax().Range)));
		}

		TEST_METHOD(CallableForwardTypesResolveToQualifiedSignatureWithoutEngine)
		{
			FSessionRun Run({
				{TEXT("A.as"), TEXT("namespace Game { delegate const Payload@ Make(const Payload&in Source, int&out Count); }")},
				{TEXT("Z.as"), TEXT("namespace Game { class Payload {}; }")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto Found = Run.Session->Lookup(TEXT("Make"));
			ASSERT_THAT(AreEqual(1, Found.Num()));
			const auto* Decl = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Found[0]);
			ASSERT_THAT(IsNotNull(Decl));
			const auto* Signature = Decl->GetResolvedSignature();
			ASSERT_THAT(IsNotNull(Signature));
			ASSERT_THAT(IsTrue(Signature->GetReturnType().IsHandle() && Signature->GetReturnType().IsConst()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Nominal, Signature->GetReturnType().GetType()->GetKind()));
			ASSERT_THAT(AreEqual(2, Signature->GetParameterTypes().Num()));
			ASSERT_THAT(IsTrue(Signature->GetParameterTypes()[0].IsConst() && Signature->GetParameterTypes()[0].IsReference()));
			ASSERT_THAT(IsTrue(Signature->GetParameterTypes()[0].GetType() == Signature->GetReturnType().GetType()));
			ASSERT_THAT(IsTrue(Signature->GetParameterTypes()[1].IsReference()));
		}

		TEST_METHOD(CallableGlobalQualificationBypassesInnerShadow)
		{
			FSessionRun Run({{TEXT("Scopes.as"), TEXT("class Payload {}; namespace Local { class Payload {}; delegate ::Payload@ Global(); delegate Payload@ Inner(); }")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto* Global = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->Lookup(TEXT("Global"))[0]);
			const auto* Inner = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->Lookup(TEXT("Inner"))[0]);
			ASSERT_THAT(IsTrue(Global->GetResolvedSignature()->GetReturnType().GetType() != Inner->GetResolvedSignature()->GetReturnType().GetType()));
		}

		TEST_METHOD(ReopenedNamespaceRetainsDistinctOccurrencesAndSharedMemberScope)
		{
			FSessionRun Run({
				{TEXT("A.as"), TEXT("namespace Game { delegate Payload@ Create(); }")},
				{TEXT("B.as"), TEXT("namespace Game { class Payload {}; }")},
				{TEXT("C.as"), TEXT("namespace Game { event void Changed(Payload@ Value); }")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1), *Run.Session->SerializeStableDiagnostics()));
			const auto Namespaces = Run.Session->GetTranslationUnit()->GetDecls();
			ASSERT_THAT(AreEqual(3, Namespaces.Num()));
			for (const auto* Namespace : Namespaces)
			{
				ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCNamespaceDecl>(Namespace)));
				ASSERT_THAT(IsFalse(Namespace->IsInvalid()));
			}
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Create")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Changed")).Num()));
			const auto* Create = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->Lookup(TEXT("Create"))[0]);
			const auto* Changed = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEventDecl>(Run.Session->Lookup(TEXT("Changed"))[0]);
			ASSERT_THAT(IsTrue(Create->GetResolvedSignature()->GetReturnType().GetType() == Changed->GetParameters()[0]->GetResolvedType()));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableDiagnostics().IsEmpty()));
			FSessionRun Conflict({{TEXT("Conflict.as"), TEXT("namespace Game {} int Game;")}});
			Conflict.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsFalse(Conflict.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsTrue(Conflict.Session->SerializeStableDiagnostics().Contains(TEXT("duplicate:Game"))));
		}

		TEST_METHOD(CallableLookupDoesNotLeakAnUnrelatedNamespaceType)
		{
			FSessionRun Run({{TEXT("Scopes.as"), TEXT("namespace Other { class Payload {}; } delegate Payload@ Invalid(); class Good {};")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("Invalid")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableDiagnostics().Contains(TEXT("unresolved-callable-return:Invalid"))));
		}

		TEST_METHOD(CallableDeclarationsCanReferenceLaterCallableTypes)
		{
			FSessionRun Run({{TEXT("Recursive.as"), TEXT("delegate void First(Second@ Callback); event void Second(First@ Callback); First@ GetCallback();")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto* First = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->Lookup(TEXT("First"))[0]);
			const auto* Second = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEventDecl>(Run.Session->Lookup(TEXT("Second"))[0]);
			ASSERT_THAT(IsNotNull(First->GetResolvedSignature())); ASSERT_THAT(IsNotNull(Second->GetResolvedSignature()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Nominal, First->GetParameters()[0]->GetResolvedType()->GetKind()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeDeclKind::Delegate, First->GetDeclaredTypeKind()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeDeclKind::Event, Second->GetDeclaredTypeKind()));
			const auto* Function = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->Lookup(TEXT("GetCallback"))[0]);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Nominal, Function->GetResolvedReturnType()->GetKind()));
		}

		TEST_METHOD(UnnamedCallableParametersPreserveOrderedTypes)
		{
			FSessionRun Run({{TEXT("Unnamed.as"), TEXT("delegate float64 Convert(int, float32, bool);")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto* Decl = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->Lookup(TEXT("Convert"))[0]);
			ASSERT_THAT(AreEqual(3, Decl->GetParameters().Num()));
			ASSERT_THAT(IsTrue(Decl->GetParameters()[0]->GetName().IsEmpty()));
			const auto* Signature = Decl->GetResolvedSignature();
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Signature->GetReturnType().GetType())->GetPrimitive()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Signature->GetParameterTypes()[1].GetType())->GetPrimitive()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(Signature->GetParameterTypes()[2].GetType())->GetPrimitive()));
		}

		TEST_METHOD(InvalidCallableParametersNeverPublishAPartialSignature)
		{
			const TCHAR* Sources[] = { TEXT("delegate void Bad(Missing Value);"), TEXT("event void Bad(void Value);"), TEXT("delegate void Bad(int Same, bool Same);") };
			for (const TCHAR* Source : Sources)
			{
				FSessionRun Run({{TEXT("Bad.as"), Source}}); Run.Session->CloseDeclarationBarrier();
				const auto* Decl = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCallableTypeDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
				ASSERT_THAT(IsNotNull(Decl)); ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
				ASSERT_THAT(IsTrue(Decl->IsInvalid())); ASSERT_THAT(IsNull(Decl->GetResolvedSignature()));
				ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("Bad")).Num()));
				ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			}
		}

		TEST_METHOD(CallableAndRecordShareOneNominalNameDomain)
		{
			FSessionRun Run({{TEXT("A.as"), TEXT("delegate void Same();")}, {TEXT("Z.as"), TEXT("class Same {};")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Same")).Num()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->Lookup(TEXT("Same"))[0])));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableDiagnostics().Contains(TEXT("duplicate:Same:A.as:Z.as"))));
		}

		TEST_METHOD(MalformedCallableRecoversAfterTrailingCommaAndIllegalBody)
		{
			const TCHAR* Sources[] = { TEXT("delegate void Bad(int Value,); class Good {};"), TEXT("event void Bad() { { int Hidden; } } class Good {};") };
			for (const TCHAR* Source : Sources)
			{
				FSessionRun Run({{TEXT("Recover.as"), Source}}); Run.Session->CloseDeclarationBarrier();
				ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
				ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("Bad")).Num()));
				ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
				const auto Diagnostics = Run.GetDiagnostics(); ASSERT_THAT(AreEqual(1, Diagnostics.Num()));
				ASSERT_THAT(AreEqual(static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::InvalidCallable), Diagnostics[0].DiagnosticID));
			}
		}

		TEST_METHOD(NestedAnnotationPayloadRemainsATreeWithAuthoredRanges)
		{
			FSessionRun Run({{TEXT("Attrs.as"), TEXT("UCLASS(Blueprintable, Meta=(Category=\"Game\", Nested=(Enabled, Count=2))) class Actor {};")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Actor"))[0]);
			ASSERT_THAT(AreEqual(1, Record->GetAttrs().Num()));
			const auto* Attr = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCAnnotationAttr>(Record->GetAttrs()[0]); ASSERT_THAT(IsNotNull(Attr));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAnnotationKind::UClass, Attr->GetAnnotationKind()));
			ASSERT_THAT(AreEqual(2, Attr->GetArguments().Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAnnotationValueKind::Flag, Attr->GetArguments()[0].Kind));
			const auto& Meta = Attr->GetArguments()[1];
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAnnotationValueKind::Group, Meta.Kind));
			ASSERT_THAT(AreEqual(FString(TEXT("Meta")), Meta.Name));
			ASSERT_THAT(AreEqual(2, Meta.Arguments.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("Game")), Meta.Arguments[0].Value));
			ASSERT_THAT(AreEqual(FString(TEXT("\"Game\"")), Run.SourceText(Meta.Arguments[0].ValueRange)));
			const auto& Nested = Meta.Arguments[1];
			ASSERT_THAT(AreEqual(2, Nested.Arguments.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEAnnotationValueKind::Scalar, Nested.Arguments[1].Kind));
			ASSERT_THAT(AreEqual(FString(TEXT("2")), Nested.Arguments[1].Value));
			ASSERT_THAT(AreEqual(FString(TEXT("Nested=(Enabled, Count=2)")), Run.SourceText(Nested.Range)));
		}

		TEST_METHOD(AllSixAnnotationKindsAttachToTheirDeclaredTargets)
		{
			FSessionRun Run({{TEXT("Six.as"), TEXT("UCLASS() class Actor { UPROPERTY() int Count; UFUNCTION() void Tick(); }; USTRUCT() struct Data {}; UENUM() enum Mode { Idle UMETA(DisplayName=\"Waiting\") };")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			TArray<AS_NAMESPACE_QUALIFIER asEAnnotationKind> Kinds;
			AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Run.Session->GetTranslationUnit(), [&](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
			{
				if (const auto* Attr = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCAnnotationAttr>(&Node)) Kinds.AddUnique(Attr->GetAnnotationKind());
			});
			ASSERT_THAT(AreEqual(6, Kinds.Num()));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVerifier::Verify(Run.Session->GetASTContext(), Run.Session->GetTranslationUnit(), Verification)));
		}

		TEST_METHOD(AnnotationTargetValidationDistinguishesClassStructEnumAndCallable)
		{
			const TCHAR* Sources[] = { TEXT("UCLASS() struct Bad {};"), TEXT("USTRUCT() class Bad {};"), TEXT("UENUM() class Bad {};"), TEXT("UMETA() int Bad;"), TEXT("UFUNCTION() delegate void Bad();") };
			for (const TCHAR* Source : Sources)
			{
				FSessionRun Run({{TEXT("Targets.as"), Source}}); Run.Session->CloseDeclarationBarrier();
				ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
				const auto Diagnostics = Run.GetDiagnostics(); ASSERT_THAT(AreEqual(1, Diagnostics.Num()));
				ASSERT_THAT(AreEqual(static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidAttributeTarget), Diagnostics[0].DiagnosticID));
				ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("Bad")).Num()));
			}
		}

		TEST_METHOD(MalformedAnnotationPayloadAndDanglingAnnotationAreErrors)
		{
			struct FCase
			{
				const TCHAR* Source;
				asUINT DiagnosticID;
			};
			const FCase Cases[] = {
				{TEXT("UCLASS(Meta=(Category=)) class Good {};"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedAnnotationPayload)},
				{TEXT("UCLASS(,Bad) class Good {};"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedAnnotationPayload)},
				{TEXT("UCLASS()"), static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::AnnotationRequiresDeclaration)},
			};
			for (const FCase& Case : Cases)
			{
				FSessionRun Run({{TEXT("Attrs.as"), Case.Source}}); Run.Session->CloseDeclarationBarrier();
				ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
				const auto Diagnostics = Run.GetDiagnostics(); ASSERT_THAT(AreEqual(1, Diagnostics.Num(), Case.Source));
				ASSERT_THAT(AreEqual(Case.DiagnosticID, Diagnostics[0].DiagnosticID, Case.Source));
				ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			}
		}

		TEST_METHOD(RemovedAssetConsumesNestedInitializerWithoutCreatingSemanticEntities)
		{
			const FString Removed = TEXT("asset Old of Payload { Values = { 1, { 2 } }; }");
			FSessionRun Run({{TEXT("Asset.as"), Removed + TEXT(" class Good {};")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			const auto Decls = Run.Session->GetTranslationUnit()->GetDecls(); ASSERT_THAT(AreEqual(2, Decls.Num()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCRecoveryDecl>(Decls[0])));
			ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("Old")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
			for (const AS_NAMESPACE_QUALIFIER asCASTNode* Node : Run.Session->GetASTContext().GetNodes())
			{
				ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Node)));
				ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCVarDecl>(Node)));
			}
			const auto Diagnostics = Run.GetDiagnostics(); ASSERT_THAT(AreEqual(1, Diagnostics.Num()));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedAsset), Diagnostics[0].DiagnosticID));
			ASSERT_THAT(AreEqual(Removed, Run.SourceText(Diagnostics[0].PrimaryRange)));
		}

		TEST_METHOD(RemovedImportDiagnosesItsAuthoredRangeAndRecovers)
		{
			const FString Removed = TEXT("import void Old(int Value) from \"Module\";");
			FSessionRun Run({{TEXT("Import.as"), Removed + TEXT(" delegate void Good();")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("Old")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
			const auto Diagnostics = Run.GetDiagnostics(); ASSERT_THAT(AreEqual(1, Diagnostics.Num()));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDeclarationDiagnostic::RemovedImport), Diagnostics[0].DiagnosticID));
			ASSERT_THAT(AreEqual(Removed, Run.SourceText(Diagnostics[0].PrimaryRange)));
		}

		TEST_METHOD(RemovedSpellingsRemainLegalAsOrdinaryIdentifiers)
		{
			FSessionRun Run({{TEXT("Identifiers.as"), TEXT("class asset {}; class import {}; asset of; import from; int assetValue; int importValue;")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("asset")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("import")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("of")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("from")).Num()));
			ASSERT_THAT(AreEqual(0, Run.GetDiagnostics().Num()));
		}

		TEST_METHOD(RemovedSyntaxInsideTriviaStringsAndInactiveBranchesIsNotDiagnosed)
		{
			FSessionRun Run({{TEXT("Inactive.as"), TEXT(
				"// asset Old of Type {}\n/* import void F() from X; */\n"
				"#if OLD\nasset Hidden of Type {};\nimport void Hidden() from \"Old\";\n#endif\n"
				"UCLASS(Meta=(ToolTip=\"asset Old of Type; import F from Old\")) class Good {};\n")}}, {{TEXT("OLD"), false}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(AreEqual(1, Run.Session->GetTranslationUnit()->GetDecls().Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
			ASSERT_THAT(AreEqual(0, Run.GetDiagnostics().Num()));
		}

		TEST_METHOD(UDelegateIsNotAnAnnotationAlias)
		{
			FSessionRun Run({{TEXT("NotAnAlias.as"), TEXT("UDELEGATE() void Old(); class Good {};")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			for (const AS_NAMESPACE_QUALIFIER asCASTNode* Node : Run.Session->GetASTContext().GetNodes())
			{
				ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCCallableTypeDecl>(Node)));
				ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCAnnotationAttr>(Node)));
			}
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
		}

		TEST_METHOD(CollectedHostOutputPrecedesResolvedSignaturesAndOwnsSyntax)
		{
			TArray<AS_NAMESPACE_QUALIFIER asSCollectedDeclarationDescription> Collected;
			{
				FSessionRun Run({{TEXT("Host.as"), TEXT("delegate Payload@ Build(const Payload&in Input); class Payload {};")}});
				AS_NAMESPACE_QUALIFIER FAngelscriptDescriptorConsumer Consumer;
				ASSERT_THAT(IsFalse(Consumer.Collect(*Run.Session, Run.SourceManager, Collected)));
				Run.Session->CloseDeclarationBarrier();
				ASSERT_THAT(IsTrue(Consumer.Collect(*Run.Session, Run.SourceManager, Collected)));
				ASSERT_THAT(AreEqual(2, Collected.Num()));
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDeclKind::Delegate, Collected[0].Kind));
				const auto* Decl = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
				ASSERT_THAT(IsNull(Decl->GetResolvedSignature()));
				ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
				ASSERT_THAT(IsNotNull(Decl->GetResolvedSignature()));
				ASSERT_THAT(IsFalse(Run.Session->GetASTContext().IsSealed()));
			}
			ASSERT_THAT(AreEqual(FString(TEXT("Build")), Collected[0].QualifiedName));
			ASSERT_THAT(IsTrue(Collected[0].Source.IsValid()));
			ASSERT_THAT(AreEqual(FString(TEXT("Payload")), Collected[0].ReturnType.NameParts[0]));
			ASSERT_THAT(IsTrue(Collected[0].ReturnType.bHandle));
			ASSERT_THAT(AreEqual(FString(TEXT("Input")), Collected[0].Parameters[0].Name));
			ASSERT_THAT(IsTrue(Collected[0].Parameters[0].Type.bConst));
		}

		TEST_METHOD(FailedCollectedHostOutputIsEmpty)
		{
			FSessionRun Run({{TEXT("Failed.as"), TEXT("class First {}; asset Old of Type {}; class Last {};")}});
			Run.Session->CloseDeclarationBarrier();
			TArray<AS_NAMESPACE_QUALIFIER asSCollectedDeclarationDescription> Collected;
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER FAngelscriptDescriptorConsumer().Collect(*Run.Session, Run.SourceManager, Collected)));
			ASSERT_THAT(AreEqual(0, Collected.Num()));
		}

		TEST_METHOD(CallableTraversalIncludesParametersWithoutGeneratedChildren)
		{
			FSessionRun Run({{TEXT("Traversal.as"), TEXT("delegate int F(int Value); event void E(bool Value);")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			int32 CallableCount = 0, ParameterCount = 0, FunctionCount = 0;
			AS_NAMESPACE_QUALIFIER asCASTVisitor::Walk(Run.Session->GetTranslationUnit(), [&](const AS_NAMESPACE_QUALIFIER asCASTNode& Node)
			{
				CallableCount += AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCCallableTypeDecl>(&Node) ? 1 : 0;
				ParameterCount += AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCVarDecl>(&Node) ? 1 : 0;
				FunctionCount += AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(&Node) ? 1 : 0;
			});
			ASSERT_THAT(AreEqual(2, CallableCount)); ASSERT_THAT(AreEqual(2, ParameterCount)); ASSERT_THAT(AreEqual(0, FunctionCount));
			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Verification;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVerifier::Verify(Run.Session->GetASTContext(), Run.Session->GetTranslationUnit(), Verification)));
		}
	};


	TEST_CLASS_WITH_FLAGS(DeclarationsStableIdentity, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
	private:
		struct FCollisionDigest final : AS_NAMESPACE_QUALIFIER asIStableKeyDigestProvider
		{
			AS_NAMESPACE_QUALIFIER asSStableKey ComputeDigest(TConstArrayView<uint8>) const override
			{
				uint8 Bytes[AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount] = {};
				Bytes[0] = 1;
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Key);
				return Key;
			}
		};

	public:
		TEST_METHOD(EquivalentNamesAndSourceMovesReuseFunctionIdentity)
		{
			auto Owner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
			FSessionRun First({{TEXT("Old.as"), TEXT("namespace Game { class Payload {}; int Read(const Payload&in Value) { return 1; } }")}}, {}, Owner);
			FSessionRun Second({{TEXT("Moved.as"), TEXT("namespace Game { int Read(const ::Game::Payload&in Renamed) { return 99; } class Payload {}; }")}}, {}, Owner);
			First.Session->CloseDeclarationBarrier(); Second.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(First.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsTrue(Second.Session->ResolveDeclarations(4)));
			const auto* A = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(First.Session->Lookup(TEXT("Read"))[0]);
			const auto* B = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Second.Session->Lookup(TEXT("Read"))[0]);
			ASSERT_THAT(IsTrue(A != B));
			ASSERT_THAT(IsTrue(A->GetStableIdentity() == B->GetStableIdentity()));
			ASSERT_THAT(IsTrue(A->GetResolvedSignature()->GetStableIdentity() == B->GetResolvedSignature()->GetStableIdentity()));
			ASSERT_THAT(IsTrue(A->GetParameters()[0]->GetStableIdentity() == B->GetParameters()[0]->GetStableIdentity()));
			ASSERT_THAT(IsTrue(First.Session->GetASTContext().GetTypeContext() == Owner));
			ASSERT_THAT(IsTrue(Second.Session->GetASTContext().GetTypeContext() == Owner));
		}

		TEST_METHOD(OrdinaryDeclarationsRetainStructuredQualifiedTypeLocations)
		{
			FSessionRun Run({{TEXT("Qualified.as"), TEXT("class Payload {}; const ::Payload@ Fetch(const Payload&in Input, int&out Count); const Payload@ Cached;")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto* Function = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->Lookup(TEXT("Fetch"))[0]);
			ASSERT_THAT(AreEqual(FString(TEXT("const ::Payload@")), Run.SourceText(Function->GetReturnTypeSyntax().Range)));
			ASSERT_THAT(IsTrue(Function->GetResolvedSignature()->GetReturnType().IsConst()));
			ASSERT_THAT(IsTrue(Function->GetResolvedSignature()->GetReturnType().IsHandle()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeReferenceDirection::Out, Function->GetParameters()[1]->GetTypeSyntax().Reference));
			const auto* Variable = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Run.Session->Lookup(TEXT("Cached"))[0]);
			ASSERT_THAT(IsTrue(Variable->GetResolvedQualType().IsConst() && Variable->GetResolvedQualType().IsHandle()));
		}

		TEST_METHOD(PassingModesCreateDistinctOverloadsAndCallableTypeUses)
		{
			FSessionRun Run({{TEXT("Modes.as"), TEXT("void F(int Value); void F(int&in Input); void F(int&out Output); void F(int&inout Both);")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto Found = Run.Session->Lookup(TEXT("F"));
			ASSERT_THAT(AreEqual(4, Found.Num()));
			TSet<AS_NAMESPACE_QUALIFIER asSStableKey> Functions, Signatures;
			TSet<uint8> Modes;
			auto& Registry = Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry();
			for (const auto* Decl : Found)
			{
				const auto* Function = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Decl);
				AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.ResolveFunction(Function->GetStableIdentity(), Descriptor)));
				ASSERT_THAT(AreEqual(1, Descriptor.Parameters.Num()));
				Functions.Add(Function->GetStableIdentity()); Signatures.Add(Function->GetResolvedSignature()->GetStableIdentity());
				Modes.Add(static_cast<uint8>(Descriptor.Parameters[0].PassingMode));
			}
			ASSERT_THAT(AreEqual(4, Functions.Num())); ASSERT_THAT(AreEqual(4, Signatures.Num())); ASSERT_THAT(AreEqual(4, Modes.Num()));
		}

		TEST_METHOD(ConstAndHandleQualifiersRemainCanonicalTypeEdges)
		{
			FSessionRun Run({{TEXT("Qualifiers.as"), TEXT("class Payload {}; void F(Payload Value); void F(const Payload Value); void F(Payload@ Value);")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto Found = Run.Session->Lookup(TEXT("F"));
			ASSERT_THAT(AreEqual(3, Found.Num()));
			TSet<AS_NAMESPACE_QUALIFIER asSStableKey> ParameterKeys;
			auto& Registry = Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry();
			for (const auto* Decl : Found)
			{
				AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Function;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.ResolveFunction(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Decl)->GetStableIdentity(), Function)));
				ParameterKeys.Add(Function.Parameters[0].Type);
			}
			ASSERT_THAT(AreEqual(3, ParameterKeys.Num()));
		}

		TEST_METHOD(ReturnTypeOnlyOverloadIsRejected)
		{
			FSessionRun Run({{TEXT("Return.as"), TEXT("int Value(int Input); bool Value(int Renamed); class Good {};")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableDiagnostics().Contains(TEXT("return-type-conflict:Value"))));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Value")).Num()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
		}

		TEST_METHOD(ParameterNamesAndBuiltinAliasesDoNotInventOverloads)
		{
			FSessionRun Run({{TEXT("Aliases.as"), TEXT("int F(uint Value); int F(uint32 Renamed);")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableDiagnostics().Contains(TEXT("duplicate:F"))));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("F")).Num()));
		}

		TEST_METHOD(NestedNominalOwnersDistinguishSameMethodNames)
		{
			FSessionRun Run({{TEXT("Owners.as"), TEXT("namespace Game { class A { class Inner {}; int F(int X); }; class B { class Inner {}; int F(int X); }; }")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto ResolvedMethods = Run.Session->Lookup(TEXT("F")), Inners = Run.Session->Lookup(TEXT("Inner"));
			ASSERT_THAT(AreEqual(2, ResolvedMethods.Num())); ASSERT_THAT(AreEqual(2, Inners.Num()));
			const auto* A = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(ResolvedMethods[0]);
			const auto* B = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(ResolvedMethods[1]);
			ASSERT_THAT(IsTrue(A->GetStableIdentity() != B->GetStableIdentity()));
			ASSERT_THAT(IsTrue(A->GetResolvedSignature()->GetStableIdentity() == B->GetResolvedSignature()->GetStableIdentity()));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Inners[0])->GetStableIdentity() != AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Inners[1])->GetStableIdentity()));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry().ResolveFunction(A->GetStableIdentity(), Descriptor)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method, Descriptor.Kind));
			ASSERT_THAT(AreEqual(FString(TEXT("Game")), Descriptor.NamespacePath[0]));
			ASSERT_THAT(IsFalse(Descriptor.Owner.IsZero()));
		}

		TEST_METHOD(ConstMethodQualificationChangesFunctionNotSignatureType)
		{
			FSessionRun Run({{TEXT("ConstMethod.as"), TEXT("class Actor { int Read(); int Read() const; };")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto Found = Run.Session->Lookup(TEXT("Read")); ASSERT_THAT(AreEqual(2, Found.Num()));
			const auto* A = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Found[0]);
			const auto* B = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Found[1]);
			ASSERT_THAT(IsTrue(A->IsConstMethod() != B->IsConstMethod()));
			ASSERT_THAT(IsTrue(A->GetStableIdentity() != B->GetStableIdentity()));
			ASSERT_THAT(IsTrue(A->GetResolvedSignature()->GetStableIdentity() == B->GetResolvedSignature()->GetStableIdentity()));
		}

		TEST_METHOD(CallableNominalAndSignatureHaveDistinctRegistryRoles)
		{
			FSessionRun Run({{TEXT("CallableIdentity.as"), TEXT("delegate int First(int Input); event int Second(int Output);")}});
			Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto* First = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDelegateDecl>(Run.Session->Lookup(TEXT("First"))[0]);
			const auto* Second = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEventDecl>(Run.Session->Lookup(TEXT("Second"))[0]);
			ASSERT_THAT(IsTrue(First->GetStableIdentity() != Second->GetStableIdentity()));
			ASSERT_THAT(IsTrue(First->GetResolvedSignature()->GetStableIdentity() == Second->GetResolvedSignature()->GetStableIdentity()));
			ASSERT_THAT(IsTrue(First->GetStableIdentity() != First->GetResolvedSignature()->GetStableIdentity()));
			auto& Registry = Run.Session->GetASTContext().GetTypeContext()->GetIdentityRegistry();
			AS_NAMESPACE_QUALIFIER asEStableIdentityRole Role = AS_NAMESPACE_QUALIFIER asEStableIdentityRole::Invalid;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.ResolveIdentityRole(First->GetStableIdentity(), Role)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStableIdentityRole::NominalType, Role));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.ResolveIdentityRole(First->GetResolvedSignature()->GetStableIdentity(), Role)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStableIdentityRole::TypeUse, Role));
		}

		TEST_METHOD(HostDeclarationCoordinatesSurviveSourceMoves)
		{
			auto Owner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
			FSessionRun First({{TEXT("First.as"), TEXT("namespace Game { int Counter; class Actor { int Health; }; }")}}, {}, Owner);
			FSessionRun Second({{TEXT("Moved.as"), TEXT("namespace Game { class Actor { int Health; }; int Counter; }")}}, {}, Owner);
			First.Session->CloseDeclarationBarrier(); Second.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(First.Session->ResolveDeclarations(1))); ASSERT_THAT(IsTrue(Second.Session->ResolveDeclarations(1)));
			for (const TCHAR* Name : { TEXT("Game"), TEXT("Counter"), TEXT("Health") })
			{
				const auto* A = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCNamedDecl>(First.Session->Lookup(Name)[0]);
				const auto* B = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCNamedDecl>(Second.Session->Lookup(Name)[0]);
				ASSERT_THAT(IsFalse(A->GetStableIdentity().IsZero())); ASSERT_THAT(IsTrue(A->GetStableIdentity() == B->GetStableIdentity()));
			}
			ASSERT_THAT(IsTrue(First.Session->GetModuleIdentity(TEXT("First.as")) != Second.Session->GetModuleIdentity(TEXT("Moved.as"))));
		}

		TEST_METHOD(RegistryCollisionPreventsFrontendPublication)
		{
			FCollisionDigest Digest;
			auto Registry = MakeShared<AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry, ESPMode::ThreadSafe>(Digest);
			auto Owner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(AS_NAMESPACE_QUALIFIER asSTypeContextOptions{}, Registry);
			FSessionRun Run({{TEXT("Collision.as"), TEXT("class Payload {}; int F();")}}, {}, Owner);
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableDiagnostics().Contains(TEXT("invalid-nominal-identity:Payload"))));
			ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("F")).Num()));
		}

		TEST_METHOD(FloatAliasUsesContextOptionsWhileSizedSpellingsStayFixed)
		{
			AS_NAMESPACE_QUALIFIER asSTypeContextOptions NarrowOptions;
			NarrowOptions.FloatIsFloat64 = false;
			AS_NAMESPACE_QUALIFIER asSTypeContextOptions WideOptions;
			WideOptions.FloatIsFloat64 = true;
			auto NarrowOwner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(NarrowOptions);
			auto WideOwner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(WideOptions);
			const TArray<TPair<FString, FString>> Sources = {{TEXT("Float.as"), TEXT("float F(float Input); float32 F32(float32 Input); float64 F64(float64 Input);")}};
			FSessionRun Narrow(Sources, {}, NarrowOwner), Wide(Sources, {}, WideOwner);
			Narrow.Session->CloseDeclarationBarrier(); Wide.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(Narrow.Session->ResolveDeclarations(1)));
			ASSERT_THAT(IsTrue(Wide.Session->ResolveDeclarations(1)));
			const auto* NarrowF = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Narrow.Session->Lookup(TEXT("F"))[0]);
			const auto* WideF = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Wide.Session->Lookup(TEXT("F"))[0]);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(NarrowF->GetResolvedReturnType())->GetPrimitive()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(WideF->GetResolvedReturnType())->GetPrimitive()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(NarrowF->GetParameters()[0]->GetResolvedType())->GetPrimitive()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBuiltinType>(WideF->GetParameters()[0]->GetResolvedType())->GetPrimitive()));
			ASSERT_THAT(IsTrue(NarrowF->GetStableIdentity() != WideF->GetStableIdentity()));
			ASSERT_THAT(IsTrue(NarrowF->GetResolvedSignature()->GetStableIdentity() != WideF->GetResolvedSignature()->GetStableIdentity()));
			for (const TCHAR* Name : { TEXT("F32"), TEXT("F64") })
			{
				const auto* NarrowFixed = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Narrow.Session->Lookup(Name)[0]);
				const auto* WideFixed = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Wide.Session->Lookup(Name)[0]);
				ASSERT_THAT(IsTrue(NarrowFixed->GetStableIdentity() == WideFixed->GetStableIdentity()));
				ASSERT_THAT(IsTrue(NarrowFixed->GetResolvedSignature()->GetStableIdentity() == WideFixed->GetResolvedSignature()->GetStableIdentity()));
			}
			ASSERT_THAT(IsTrue(NarrowF->GetResolvedSignature()->GetStableIdentity() == AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Narrow.Session->Lookup(TEXT("F32"))[0])->GetResolvedSignature()->GetStableIdentity()));
			ASSERT_THAT(IsTrue(WideF->GetResolvedSignature()->GetStableIdentity() == AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Wide.Session->Lookup(TEXT("F64"))[0])->GetResolvedSignature()->GetStableIdentity()));
		}

		TEST_METHOD(SharedTypeContextRetainsIdentityAfterSessionDestruction)
		{
			auto Owner = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			{
				FSessionRun Run({{TEXT("Lifetime.as"), TEXT("int Read(bool Enabled);")}}, {}, Owner);
				Run.Session->CloseDeclarationBarrier(); ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
				Key = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->Lookup(TEXT("Read"))[0])->GetStableIdentity();
			}
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Owner->GetIdentityRegistry().ResolveFunction(Key, Descriptor)));
			ASSERT_THAT(AreEqual(FString(TEXT("Read")), Descriptor.Name));
			ASSERT_THAT(AreEqual(1, Descriptor.Parameters.Num()));
		}
	};

	TEST_CLASS_WITH_FLAGS(DeclarationsResolution, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(LaterFileTypesResolveAfterTheBarrier)
		{
			FSessionRun Run({{TEXT("A.as"), TEXT("B@ MakeB();\n")}, {TEXT("B.as"), TEXT("class B {};\n")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(1)));
			const auto* Function = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Run.Session->Lookup(TEXT("MakeB"))[0]);
			ASSERT_THAT(IsTrue(Function->GetResolvedReturnType() != nullptr));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Nominal, Function->GetResolvedReturnType()->GetKind()));
		}

		TEST_METHOD(ReversedSourceInputProducesTheSameStableProjection)
		{
			FSessionRun First({{TEXT("A.as"), TEXT("B@ MakeB();\n")}, {TEXT("B.as"), TEXT("class B {};\n")}});
			FSessionRun Second({{TEXT("B.as"), TEXT("class B {};\n")}, {TEXT("A.as"), TEXT("B@ MakeB();\n")}});
			First.Session->CloseDeclarationBarrier(); Second.Session->CloseDeclarationBarrier();
			First.Session->ResolveDeclarations(1); Second.Session->ResolveDeclarations(1);
			ASSERT_THAT(AreEqual(First.Session->SerializeStableProjection(), Second.Session->SerializeStableProjection()));
			ASSERT_THAT(AreEqual(First.Session->SerializeStableDiagnostics(), Second.Session->SerializeStableDiagnostics()));
		}

		TEST_METHOD(DuplicatePrimaryIsChosenByStableSourceOrder)
		{
			FSessionRun Run({{TEXT("Z.as"), TEXT("class Same {};\n")}, {TEXT("A.as"), TEXT("class Same {};\n")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(4)));
			ASSERT_THAT(IsFalse(Run.Session->IsPublishable()));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Same")).Num()));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableDiagnostics().Contains(TEXT("duplicate:Same:A.as:Z.as"))));
		}

		TEST_METHOD(MalformedDeclarationRecoversToLaterValidDeclaration)
		{
			FSessionRun Run({{TEXT("Recover.as"), TEXT("class Broken { int ; }; class Good {};\n")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
			ASSERT_THAT(AreEqual(1, Run.Session->Lookup(TEXT("Good")).Num()));
			ASSERT_THAT(AreEqual(0, Run.Session->Lookup(TEXT("BrokenMember")).Num()));
			ASSERT_THAT(IsTrue(Run.Session->HasRecoveryNodes()));
		}

		TEST_METHOD(WorkerCountDoesNotChangeResolution)
		{
			const TArray<TPair<FString,FString>> Sources={{TEXT("B.as"),TEXT("class B {}; int F(B@ X);\n")},{TEXT("A.as"),TEXT("class A : B {};\n")}};
			FSessionRun One(Sources); FSessionRun Four(Sources);
			One.Session->CloseDeclarationBarrier(); Four.Session->CloseDeclarationBarrier();
			One.Session->ResolveDeclarations(1); Four.Session->ResolveDeclarations(4);
			ASSERT_THAT(AreEqual(One.Session->SerializeStableProjection(), Four.Session->SerializeStableProjection()));
			ASSERT_THAT(AreEqual(One.Session->SerializeStableDiagnostics(), Four.Session->SerializeStableDiagnostics()));
		}

		TEST_METHOD(OverloadSetsUseStableSignatureOrder)
		{
			FSessionRun Run({{TEXT("B.as"), TEXT("int F(bool X);\n")}, {TEXT("A.as"), TEXT("int F(int X);\n")}});
			Run.Session->CloseDeclarationBarrier();
			ASSERT_THAT(IsTrue(Run.Session->ResolveDeclarations(4)));
			const auto Overloads = Run.Session->Lookup(TEXT("F"));
			ASSERT_THAT(AreEqual(2, Overloads.Num()));
			ASSERT_THAT(IsTrue(static_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl*>(Overloads[0])->GetSemanticKey() < static_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl*>(Overloads[1])->GetSemanticKey()));
		}
	};
}

#endif
